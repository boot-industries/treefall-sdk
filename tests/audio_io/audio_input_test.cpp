// SPDX-License-Identifier: MIT
#include "../../src/core/common/spsc_observation.h"
//
// Covers single-threaded ring semantics (wrap, all-or-nothing overflow,
// partial drains), a threaded producer/consumer stress with sequence
// verification (run under TSAN in the sanitizer builds), and the G5+G7
// pairing that makes "capture → disk" an SDK-supported path.

#include <orpheus/audio_file_reader.h>
#include <orpheus/audio_file_writer.h>
#include <orpheus/audio_input.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace orpheus;

// ---------------------------------------------------------------------------
// Ring semantics (single-threaded)
// ---------------------------------------------------------------------------

TEST(AudioInputRingTest, CapacityRoundsUpToPowerOfTwo) {
  AudioInputRing ring(2, 1000);
  EXPECT_EQ(ring.capacityFrames(), 1024u);
  EXPECT_EQ(ring.numChannels(), 2u);
  EXPECT_EQ(ring.framesAvailable(), 0u);
}

TEST(AudioInputRingTest, RejectsZeroChannels) {
  EXPECT_THROW(AudioInputRing(0, 256), std::invalid_argument);
}

TEST(AudioInputRingTest, RejectsUnrepresentableCapacity) {
  EXPECT_THROW(AudioInputRing(1, std::numeric_limits<size_t>::max()), std::length_error);
}


TEST(AudioInputRingTest, ConcurrentAvailabilityObservationNeverUnderflows) {
  std::atomic<size_t> read{0};
  std::atomic<size_t> write{1};
  bool advanced = false;
  const size_t pending = detail::observeBoundedPending(
      read, write, 8,
      [&]() noexcept {
        if (!advanced) {
          read.store(2, std::memory_order_release);
          write.store(2, std::memory_order_release);
          advanced = true;
        }
      });
  EXPECT_EQ(pending, 2u);
  EXPECT_LE(pending, 8u);
}
TEST(AudioInputRingTest, WriteReadRoundTrip) {
  AudioInputRing ring(2, 256);
  std::vector<float> input(64 * 2);
  for (size_t i = 0; i < input.size(); ++i) {
    input[i] = static_cast<float>(i);
  }

  EXPECT_EQ(ring.write(input.data(), 64), 64u);
  EXPECT_EQ(ring.framesAvailable(), 64u);

  std::vector<float> output(64 * 2, 0.0f);
  EXPECT_EQ(ring.read(output.data(), 64), 64u);
  EXPECT_EQ(ring.framesAvailable(), 0u);
  EXPECT_EQ(input, output);
}

TEST(AudioInputRingTest, WrapAroundPreservesFrameOrder) {
  AudioInputRing ring(1, 8); // tiny ring to force wrapping
  float chunk[4];
  float out[4];

  float next = 0.0f;
  float expected = 0.0f;
  for (int round = 0; round < 10; ++round) {
    for (float& sample : chunk) {
      sample = next++;
    }
    ASSERT_EQ(ring.write(chunk, 4), 4u);
    ASSERT_EQ(ring.read(out, 4), 4u);
    for (float sample : out) {
      EXPECT_EQ(sample, expected);
      expected += 1.0f;
    }
  }
}

TEST(AudioInputRingTest, OverflowDropsWholeBufferAndCounts) {
  AudioInputRing ring(1, 8);
  float data[8] = {1, 2, 3, 4, 5, 6, 7, 8};

  EXPECT_EQ(ring.write(data, 6), 6u);
  // 2 frames free; a 4-frame write must drop ENTIRELY (no partial frames).
  EXPECT_EQ(ring.write(data, 4), 0u);
  EXPECT_EQ(ring.overflowCount(), 1u);
  EXPECT_EQ(ring.framesAvailable(), 6u);

  // A write that exactly fits still succeeds.
  EXPECT_EQ(ring.write(data, 2), 2u);
  EXPECT_EQ(ring.framesAvailable(), 8u);
}

TEST(AudioInputRingTest, PartialReadsDrainInOrder) {
  AudioInputRing ring(1, 16);
  float data[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  ASSERT_EQ(ring.write(data, 10), 10u);

  float out[4] = {};
  EXPECT_EQ(ring.read(out, 4), 4u);
  EXPECT_EQ(out[0], 0.0f);
  EXPECT_EQ(out[3], 3.0f);
  EXPECT_EQ(ring.read(out, 4), 4u);
  EXPECT_EQ(out[0], 4.0f);
  EXPECT_EQ(ring.read(out, 4), 2u); // only 2 left
  EXPECT_EQ(out[0], 8.0f);
  EXPECT_EQ(out[1], 9.0f);
  EXPECT_EQ(ring.read(out, 4), 0u); // empty
}

// ---------------------------------------------------------------------------
// Threaded stress (SPSC contract; meaningful under TSAN)
// ---------------------------------------------------------------------------

TEST(AudioInputRingTest, ThreadedProducerConsumerPreservesSequence) {
  AudioInputRing ring(1, 4096);
  constexpr int64_t kTotalFrames = 480000; // 10s @ 48k
  constexpr size_t kChunk = 480;

  std::atomic<bool> failed{false};

  std::thread producer([&]() {
    float chunk[kChunk];
    int64_t produced = 0;
    while (produced < kTotalFrames) {
      for (size_t i = 0; i < kChunk; ++i) {
        chunk[i] = static_cast<float>(produced + static_cast<int64_t>(i));
      }
      if (ring.write(chunk, kChunk) == kChunk) {
        produced += static_cast<int64_t>(kChunk);
      } else {
        std::this_thread::yield(); // ring full — consumer will catch up
      }
    }
  });

  float out[kChunk];
  int64_t consumed = 0;
  while (consumed < kTotalFrames && !failed.load()) {
    const size_t got = ring.read(out, kChunk);
    for (size_t i = 0; i < got; ++i) {
      if (out[i] != static_cast<float>(consumed + static_cast<int64_t>(i))) {
        failed.store(true);
        break;
      }
    }
    if (got == 0) {
      std::this_thread::yield();
    }
    consumed += static_cast<int64_t>(got);
  }

  producer.join();
  EXPECT_FALSE(failed.load()) << "frame sequence corrupted across threads";
  EXPECT_EQ(consumed, kTotalFrames);
}

// ---------------------------------------------------------------------------
// IAudioInputStream contract + the G5 pairing (capture → disk)
// ---------------------------------------------------------------------------

TEST(AudioInputStreamTest, StreamWrapsRingContract) {
  AudioInputStreamConfig config;
  config.num_channels = 2;
  config.sample_rate = 48000;
  config.ring_capacity_frames = 1024;

  auto stream = createAudioInputStream(config);
  ASSERT_NE(stream, nullptr);
  EXPECT_EQ(stream->numChannels(), 2u);
  EXPECT_EQ(stream->sampleRate(), 48000u);
  EXPECT_EQ(stream->framesPending(), 0u);

  std::vector<float> frames(256 * 2, 0.25f);
  EXPECT_EQ(stream->capture(frames.data(), 256), 256u);
  EXPECT_EQ(stream->framesPending(), 256u);

  std::vector<float> drained(256 * 2, 0.0f);
  EXPECT_EQ(stream->drain(drained.data(), 256), 256u);
  EXPECT_EQ(drained, frames);
  EXPECT_EQ(stream->overflowCount(), 0u);
}

TEST(AudioInputStreamTest, CaptureToDiskPairsWithAudioFileWriter) {
  auto writer = createAudioFileWriter();
  if (!writer) {
    GTEST_SKIP() << "libsndfile not available";
  }

  const auto dir = std::filesystem::temp_directory_path() / "orp134_capture";
  std::filesystem::create_directories(dir);
  const std::string path = (dir / "capture.wav").string();

  AudioInputStreamConfig config;
  config.num_channels = 1;
  auto stream = createAudioInputStream(config);

  AudioFileWriterConfig writerConfig;
  writerConfig.format = AudioFileFormat::WAV;
  writerConfig.sample_format = AudioSampleFormat::Float32;
  writerConfig.num_channels = 1;
  ASSERT_EQ(writer->open(path, writerConfig), SessionGraphError::OK);

  // "Audio thread" pushes 1s of a ramp; "writer thread" drains to disk.
  constexpr size_t kTotal = 48000;
  std::thread audio([&]() {
    float chunk[480];
    for (size_t produced = 0; produced < kTotal;) {
      for (size_t i = 0; i < 480; ++i) {
        chunk[i] = static_cast<float>(produced + i) / static_cast<float>(kTotal);
      }
      if (stream->capture(chunk, 480) == 480) {
        produced += 480;
      } else {
        std::this_thread::yield();
      }
    }
  });

  size_t written = 0;
  float buffer[1024];
  while (written < kTotal) {
    const size_t got = stream->drain(buffer, 1024);
    if (got == 0) {
      std::this_thread::yield();
      continue;
    }
    auto result = writer->writeSamples(buffer, got);
    ASSERT_TRUE(result.isOk());
    written += got;
  }
  audio.join();
  ASSERT_EQ(writer->close(), SessionGraphError::OK);

  // Read back and verify the captured ramp survived intact.
  auto reader = createAudioFileReader();
  auto opened = reader->open(path);
  ASSERT_TRUE(opened.isOk());
  EXPECT_EQ(opened.value.duration_samples, static_cast<int64_t>(kTotal));

  std::vector<float> decoded(kTotal, 0.0f);
  auto read = reader->readSamples(decoded.data(), kTotal);
  ASSERT_TRUE(read.isOk());
  ASSERT_EQ(read.value, kTotal);
  for (size_t i = 0; i < kTotal; i += 4801) {
    EXPECT_FLOAT_EQ(decoded[i], static_cast<float>(i) / static_cast<float>(kTotal));
  }

  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
}
