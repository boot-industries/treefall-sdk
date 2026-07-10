// SPDX-License-Identifier: MIT
// ORP134 G5 / FTR007: IAudioFileWriter round-trip tests.
//
// The acceptance gate: the writer produces WAV/AIFF/FLAC files that the SDK's
// own reader opens and decodes back to the written samples — bit-exact for
// float encodings, within one quantization step for integer encodings.

#include <orpheus/audio_file_reader.h>
#include <orpheus/audio_file_writer.h>

#include "../support/fnv1a64.hpp"
#include "../support/synth.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace orpheus;
using orpheus::tests::support::Fnv1a64;
using orpheus::tests::support::GenerateSine;

namespace {

constexpr uint32_t kSampleRate = 48000;
constexpr size_t kFrames = 24000; // 0.5s

std::vector<float> makeStereoTestSignal(size_t frames) {
  auto left = GenerateSine(frames, kSampleRate, 440, 0.5f);
  auto right = GenerateSine(frames, kSampleRate, 554, 0.4f);
  std::vector<float> interleaved(frames * 2, 0.0f);
  for (size_t i = 0; i < frames; ++i) {
    interleaved[i * 2] = left[i];
    interleaved[i * 2 + 1] = right[i];
  }
  return interleaved;
}

class AudioFileWriterTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_writer = createAudioFileWriter();
    if (!m_writer) {
      GTEST_SKIP() << "libsndfile not available - writer disabled";
    }
    m_tempDir = std::filesystem::temp_directory_path() / "orp134_writer";
    std::filesystem::create_directories(m_tempDir);
    m_signal = makeStereoTestSignal(kFrames);
  }

  void TearDown() override {
    m_writer.reset();
    std::error_code ec;
    std::filesystem::remove_all(m_tempDir, ec);
  }

  // Write the standard stereo signal with the given config, read it back
  // with the SDK reader, and return the decoded samples + metadata.
  void roundTrip(const AudioFileWriterConfig& config, const std::string& name,
                 std::vector<float>& decoded, AudioFileMetadata& readMeta) {
    const std::string path = (m_tempDir / name).string();

    ASSERT_EQ(m_writer->open(path, config), SessionGraphError::OK);
    EXPECT_TRUE(m_writer->isOpen());

    // Write in uneven chunks to exercise streaming appends.
    size_t written = 0;
    const size_t chunks[] = {1000, 3000, 7000, 13000};
    for (size_t chunk : chunks) {
      auto result = m_writer->writeSamples(m_signal.data() + written * 2, chunk);
      ASSERT_TRUE(result.isOk()) << result.errorMessage;
      EXPECT_EQ(result.value, chunk);
      written += chunk;
    }
    ASSERT_EQ(written, kFrames);
    EXPECT_EQ(m_writer->getFramesWritten(), static_cast<int64_t>(kFrames));

    AudioFileMetadata writeMeta = m_writer->metadata();
    EXPECT_EQ(writeMeta.format, config.format);
    EXPECT_EQ(writeMeta.sample_rate, config.sample_rate);
    EXPECT_EQ(writeMeta.num_channels, config.num_channels);
    EXPECT_EQ(writeMeta.duration_samples, static_cast<int64_t>(kFrames));

    ASSERT_EQ(m_writer->close(), SessionGraphError::OK);
    EXPECT_FALSE(m_writer->isOpen());
    EXPECT_EQ(m_writer->close(), SessionGraphError::OK) << "close must be idempotent";

    // Read back with the SDK's own reader.
    auto reader = createAudioFileReader();
    ASSERT_NE(reader, nullptr);
    auto openResult = reader->open(path);
    ASSERT_TRUE(openResult.isOk()) << openResult.errorMessage;
    readMeta = openResult.value;

    decoded.assign(kFrames * 2, 0.0f);
    auto readResult = reader->readSamples(decoded.data(), kFrames);
    ASSERT_TRUE(readResult.isOk());
    ASSERT_EQ(readResult.value, kFrames);
    reader->close();
  }

  static float maxAbsDiff(const std::vector<float>& a, const std::vector<float>& b) {
    float maxDiff = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
      maxDiff = std::max(maxDiff, std::abs(a[i] - b[i]));
    }
    return maxDiff;
  }

  std::unique_ptr<IAudioFileWriter> m_writer;
  std::filesystem::path m_tempDir;
  std::vector<float> m_signal;
};

} // namespace

TEST_F(AudioFileWriterTest, WavFloat32RoundTripIsBitExact) {
  AudioFileWriterConfig config;
  config.format = AudioFileFormat::WAV;
  config.sample_format = AudioSampleFormat::Float32;

  std::vector<float> decoded;
  AudioFileMetadata meta;
  roundTrip(config, "rt_float32.wav", decoded, meta);

  EXPECT_EQ(meta.format, AudioFileFormat::WAV);
  EXPECT_EQ(meta.sample_rate, kSampleRate);
  EXPECT_EQ(meta.num_channels, 2u);
  EXPECT_EQ(meta.duration_samples, static_cast<int64_t>(kFrames));
  EXPECT_EQ(meta.bit_depth, 32u);

  // Float in, float container, float out: bit-exact.
  EXPECT_EQ(Fnv1a64(decoded), Fnv1a64(m_signal)) << "float32 WAV round-trip must be bit-exact";
}

TEST_F(AudioFileWriterTest, WavInt16RoundTripWithinOneLsb) {
  AudioFileWriterConfig config;
  config.format = AudioFileFormat::WAV;
  config.sample_format = AudioSampleFormat::Int16;

  std::vector<float> decoded;
  AudioFileMetadata meta;
  roundTrip(config, "rt_int16.wav", decoded, meta);

  EXPECT_EQ(meta.bit_depth, 16u);
  EXPECT_LT(maxAbsDiff(decoded, m_signal), 1.5f / 32768.0f)
      << "int16 quantization error exceeded one LSB";
}

TEST_F(AudioFileWriterTest, AiffInt24RoundTripWithinOneLsb) {
  AudioFileWriterConfig config;
  config.format = AudioFileFormat::AIFF;
  config.sample_format = AudioSampleFormat::Int24;

  std::vector<float> decoded;
  AudioFileMetadata meta;
  roundTrip(config, "rt_int24.aiff", decoded, meta);

  EXPECT_EQ(meta.format, AudioFileFormat::AIFF);
  EXPECT_EQ(meta.bit_depth, 24u);
  EXPECT_LT(maxAbsDiff(decoded, m_signal), 1.5f / 8388608.0f)
      << "int24 quantization error exceeded one LSB";
}

TEST_F(AudioFileWriterTest, FlacInt16RoundTripIsLossless) {
  AudioFileWriterConfig config;
  config.format = AudioFileFormat::FLAC;
  config.sample_format = AudioSampleFormat::Int16;

  std::vector<float> decoded;
  AudioFileMetadata meta;
  roundTrip(config, "rt_flac16.flac", decoded, meta);

  EXPECT_EQ(meta.format, AudioFileFormat::FLAC);
  EXPECT_EQ(meta.duration_samples, static_cast<int64_t>(kFrames));
  // FLAC is lossless over the quantized integers: same tolerance as int16 PCM.
  EXPECT_LT(maxAbsDiff(decoded, m_signal), 1.5f / 32768.0f);
}

TEST_F(AudioFileWriterTest, FlacRejectsFloat32) {
  AudioFileWriterConfig config;
  config.format = AudioFileFormat::FLAC;
  config.sample_format = AudioSampleFormat::Float32;

  EXPECT_EQ(m_writer->open((m_tempDir / "bad.flac").string(), config),
            SessionGraphError::InvalidParameter)
      << "FLAC is integer-only; Float32 must be rejected at open";
  EXPECT_FALSE(m_writer->isOpen());
}

TEST_F(AudioFileWriterTest, UnsupportedContainerIsRejected) {
  AudioFileWriterConfig config;
  config.format = AudioFileFormat::MP3;
  EXPECT_EQ(m_writer->open((m_tempDir / "bad.mp3").string(), config),
            SessionGraphError::NotSupported);

  config.format = AudioFileFormat::OGG;
  EXPECT_EQ(m_writer->open((m_tempDir / "bad.ogg").string(), config),
            SessionGraphError::NotSupported);
}

TEST_F(AudioFileWriterTest, InvalidConfigsAreRejected) {
  AudioFileWriterConfig config;
  config.sample_rate = 0;
  EXPECT_EQ(m_writer->open((m_tempDir / "bad_rate.wav").string(), config),
            SessionGraphError::InvalidParameter);

  config = AudioFileWriterConfig{};
  config.num_channels = 0;
  EXPECT_EQ(m_writer->open((m_tempDir / "bad_ch.wav").string(), config),
            SessionGraphError::InvalidParameter);

  EXPECT_EQ(m_writer->open("", AudioFileWriterConfig{}), SessionGraphError::InvalidParameter);
}

TEST_F(AudioFileWriterTest, WriteBeforeOpenReturnsNotReady) {
  float dummy[2] = {0.0f, 0.0f};
  auto result = m_writer->writeSamples(dummy, 1);
  EXPECT_EQ(result.error, SessionGraphError::NotReady);
  EXPECT_EQ(m_writer->getFramesWritten(), 0);
}

TEST_F(AudioFileWriterTest, RepeatedWritesAreByteIdentical) {
  // Determinism is a contract: the same samples must produce the same file
  // bytes on every run. libsndfile's default PEAK chunk for float files embeds
  // a wall-clock timestamp, which broke this — the writer disables it.
  auto readBytes = [](const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  };

  const struct {
    AudioFileFormat format;
    AudioSampleFormat sample_format;
    const char* ext;
  } cases[] = {
      {AudioFileFormat::WAV, AudioSampleFormat::Float32, "wav"},
      {AudioFileFormat::AIFF, AudioSampleFormat::Float32, "aiff"},
      {AudioFileFormat::FLAC, AudioSampleFormat::Int24, "flac"},
  };
  for (const auto& c : cases) {
    AudioFileWriterConfig config;
    config.format = c.format;
    config.sample_format = c.sample_format;

    std::filesystem::path paths[2];
    for (int run = 0; run < 2; ++run) {
      paths[run] = m_tempDir / ("det_" + std::to_string(run) + "." + c.ext);
      ASSERT_EQ(m_writer->open(paths[run].string(), config), SessionGraphError::OK);
      auto result = m_writer->writeSamples(m_signal.data(), kFrames);
      ASSERT_TRUE(result.isOk());
      ASSERT_EQ(m_writer->close(), SessionGraphError::OK);
      // A wall-clock-stamped chunk only differs across seconds; make sure the
      // two runs cannot land in the same second and mask the regression.
      if (run == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
      }
    }
    EXPECT_EQ(readBytes(paths[0]), readBytes(paths[1]))
        << "." << c.ext << " output must be byte-identical across runs";
  }
}

TEST_F(AudioFileWriterTest, MonoAndHighRateConfigs) {
  AudioFileWriterConfig config;
  config.format = AudioFileFormat::WAV;
  config.sample_format = AudioSampleFormat::Float32;
  config.num_channels = 1;
  config.sample_rate = 96000;

  auto mono = GenerateSine(4800, 96000, 1000, 0.5f);
  const std::string path = (m_tempDir / "mono96k.wav").string();

  ASSERT_EQ(m_writer->open(path, config), SessionGraphError::OK);
  auto writeResult = m_writer->writeSamples(mono.data(), mono.size());
  ASSERT_TRUE(writeResult.isOk());
  ASSERT_EQ(m_writer->close(), SessionGraphError::OK);

  auto reader = createAudioFileReader();
  auto openResult = reader->open(path);
  ASSERT_TRUE(openResult.isOk());
  EXPECT_EQ(openResult.value.num_channels, 1u);
  EXPECT_EQ(openResult.value.sample_rate, 96000u);
  EXPECT_EQ(openResult.value.duration_samples, static_cast<int64_t>(mono.size()));

  std::vector<float> decoded(mono.size(), 0.0f);
  auto readResult = reader->readSamples(decoded.data(), mono.size());
  ASSERT_TRUE(readResult.isOk());
  EXPECT_EQ(Fnv1a64(decoded), Fnv1a64(mono));
}
