// SPDX-License-Identifier: MIT
// ORP168: rendered first-block streaming seek and command-prime transactions.

#define ORPHEUS_TEST_DEFINE_RT_ALLOC_HOOKS
#include "../support/rt_guard.hpp"

#include "audio_io/resampling_audio_file_reader.h"
#include "transport/transport_controller.h"

#include <orpheus/clip_source.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <string>
#include <thread>

#include <tuple>
#include <vector>

using namespace orpheus;
using orpheus::tests::support::RtGuardState;
using orpheus::tests::support::RtSection;

namespace {

constexpr std::array<float, 4> kMarkers = {-0.60f, -0.20f, 0.20f, 0.60f};

std::string writePositionCodedWav(const std::filesystem::path& directory, const std::string& name,
                                  uint32_t sampleRate, uint16_t channels) {
  const int64_t frames = static_cast<int64_t>(sampleRate) * 9;
  const std::filesystem::path path = directory / name;
  std::ofstream file(path, std::ios::binary);
  const uint32_t dataSize = static_cast<uint32_t>(frames * channels * sizeof(int16_t));
  const uint32_t fileSize = 36 + dataSize;
  const uint32_t fmtSize = 16;
  const uint16_t audioFormat = 1;
  const uint16_t blockAlign = channels * sizeof(int16_t);
  const uint32_t byteRate = sampleRate * blockAlign;
  const uint16_t bitsPerSample = 16;
  file.write("RIFF", 4);
  file.write(reinterpret_cast<const char*>(&fileSize), 4);
  file.write("WAVE", 4);
  file.write("fmt ", 4);
  file.write(reinterpret_cast<const char*>(&fmtSize), 4);
  file.write(reinterpret_cast<const char*>(&audioFormat), 2);
  file.write(reinterpret_cast<const char*>(&channels), 2);
  file.write(reinterpret_cast<const char*>(&sampleRate), 4);
  file.write(reinterpret_cast<const char*>(&byteRate), 4);
  file.write(reinterpret_cast<const char*>(&blockAlign), 2);
  file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
  file.write("data", 4);
  file.write(reinterpret_cast<const char*>(&dataSize), 4);

  const int64_t halfSecondFrames = sampleRate / 2;
  for (int64_t frame = 0; frame < frames; ++frame) {
    const float marker = kMarkers[static_cast<size_t>((frame / halfSecondFrames) % 4)];
    for (uint16_t channel = 0; channel < channels; ++channel) {
      const float value = marker * static_cast<float>(channel + 1) / static_cast<float>(channels);
      const int16_t sample = static_cast<int16_t>(std::lround(value * 32767.0f));
      file.write(reinterpret_cast<const char*>(&sample), sizeof(sample));
    }
  }
  return path.string();
}

class SeekCallback final : public ITransportCallback {
public:
  std::atomic<int> underruns{0};
  std::atomic<int> seeks{0};

  void onClipStarted(ClipHandle, orpheus::StartRequestTag, uint32_t, TransportPosition) override {}
  void onClipStopped(ClipHandle, orpheus::StartRequestTag, uint32_t, TransportPosition) override {}
  void onClipLooped(ClipHandle, orpheus::StartRequestTag, uint32_t, TransportPosition) override {}
  void onBufferUnderrun(TransportPosition) override {
    underruns.fetch_add(1, std::memory_order_relaxed);
  }
  void onClipSeeked(ClipHandle, TransportPosition) override {
    seeks.fetch_add(1, std::memory_order_relaxed);
  }
};

float trailingMean(const std::vector<float>& samples) {
  const size_t begin = samples.size() / 2;
  float sum = 0.0f;
  for (size_t index = begin; index < samples.size(); ++index) {
    sum += samples[index];
  }
  return sum / static_cast<float>(samples.size() - begin);
}

class FaultReader final : public IAudioFileReader {
public:
  explicit FaultReader(int64_t frames, uint16_t channels = 1, uint32_t rate = 48000)
      : m_frames(frames), m_channels(channels), m_rate(rate) {}

  Result<AudioFileMetadata> open(const std::string&) override {
    m_open = true;
    Result<AudioFileMetadata> result{};
    result.value.format = AudioFileFormat::WAV;
    result.value.sample_rate = m_rate;
    result.value.num_channels = m_channels;
    result.value.duration_samples = m_frames;
    result.value.bit_depth = 16;
    result.error = SessionGraphError::OK;
    return result;
  }

  Result<size_t> readSamples(float* buffer, size_t samples) override {
    if (std::this_thread::get_id() == m_callingThread) {
      readOnCallingThread.store(true, std::memory_order_relaxed);
    }
    if (!m_open || failRead.load(std::memory_order_relaxed)) {
      return {0, SessionGraphError::NotReady, {}};
    }
    const size_t available = static_cast<size_t>(std::max<int64_t>(0, m_frames - m_position));
    size_t give = std::min(samples, available);
    if (earlyEof.load(std::memory_order_relaxed) && give != 0) {
      give -= 1;
    }
    for (size_t frame = 0; frame < give; ++frame) {
      for (uint16_t channel = 0; channel < m_channels; ++channel) {
        buffer[frame * m_channels + channel] =
            static_cast<float>(m_position + static_cast<int64_t>(frame));
      }
    }
    m_position += static_cast<int64_t>(give);
    return {give, SessionGraphError::OK, {}};
  }

  SessionGraphError seek(int64_t position) override {
    if (std::this_thread::get_id() == m_callingThread) {
      readOnCallingThread.store(true, std::memory_order_relaxed);
    }
    if (!m_open || failSeek.load(std::memory_order_relaxed)) {
      return SessionGraphError::NotReady;
    }
    m_position = std::clamp(position, int64_t{0}, m_frames);
    return SessionGraphError::OK;
  }

  void close() override {
    m_open = false;
  }
  int64_t getCurrentPosition() const override {
    return m_position;
  }
  bool isOpen() const override {
    return m_open;
  }

  std::atomic<bool> failRead{false};
  std::atomic<bool> failSeek{false};
  std::atomic<bool> earlyEof{false};
  std::atomic<bool> readOnCallingThread{false};

  void setCallingThread(std::thread::id id) {
    m_callingThread = id;
  }

private:
  int64_t m_frames;
  uint16_t m_channels;
  uint32_t m_rate;
  int64_t m_position{0};
  bool m_open{false};
  std::thread::id m_callingThread{};
};

class StreamingSeekMatrixTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() {
    s_directory = std::filesystem::temp_directory_path() / "orp168_streaming_seek";
    std::filesystem::create_directories(s_directory);
  }

  static void TearDownTestSuite() {
    std::error_code error;
    std::filesystem::remove_all(s_directory, error);
  }

  static const std::string& sourcePath(uint32_t engineRate, uint16_t channels, uint32_t fileRate) {
    const auto key = std::make_tuple(engineRate, channels, fileRate);
    auto [it, inserted] = s_sources.try_emplace(key);
    if (inserted) {
      it->second = writePositionCodedWav(s_directory,
                                         "coded_" + std::to_string(engineRate) + "_" +
                                             std::to_string(channels) + "_" +
                                             std::to_string(fileRate) + ".wav",
                                         fileRate, channels);
    }
    return it->second;
  }

  static std::filesystem::path s_directory;
  static std::map<std::tuple<uint32_t, uint16_t, uint32_t>, std::string> s_sources;
};

std::filesystem::path StreamingSeekMatrixTest::s_directory;
std::map<std::tuple<uint32_t, uint16_t, uint32_t>, std::string> StreamingSeekMatrixTest::s_sources;

TEST_F(StreamingSeekMatrixTest, FirstPostSeekBlockIsTargetAudioWithoutUnderrun) {
  for (const uint32_t engineRate : {44100u, 48000u, 96000u, 192000u}) {
    for (const uint32_t blockSize : {64u, 127u, 512u, 1024u}) {
      for (const uint16_t channels : {uint16_t{1}, uint16_t{4}}) {
        for (const double playbackRate : {0.5, 2.0}) {
          for (const bool mismatched : {false, true}) {
            const uint32_t fileRate =
                mismatched ? (engineRate == 44100 ? 48000 : 44100) : engineRate;
            SCOPED_TRACE(
                "engine=" + std::to_string(engineRate) + " block=" + std::to_string(blockSize) +
                " channels=" + std::to_string(channels) + " rate=" + std::to_string(playbackRate) +
                " file=" + std::to_string(fileRate));

            TransportConfig config{};
            config.sampleRate = engineRate;
            config.maxBlockFrames = blockSize;
            config.outputChannels = channels;
            config.maxSourceChannels = channels;
            config.numGroups = 1;
            config.sourceChannelPolicy = SourceChannelPolicy::Discrete;
            auto transport = std::make_unique<TransportController>(nullptr, config);
            transport->setPreparedSourceMaxFrames(engineRate);
            SeekCallback callback;
            transport->setCallback(&callback);
            ASSERT_EQ(transport->registerClipAudio(1, sourcePath(engineRate, channels, fileRate)),
                      SessionGraphError::OK);
            ASSERT_EQ(transport->updateClipTrimPoints(
                          1, 0, static_cast<int64_t>(8.75 * static_cast<double>(engineRate))),
                      SessionGraphError::OK);
            auto metadata = transport->getClipMetadata(1);
            ASSERT_TRUE(metadata.has_value());
            metadata->playbackRate = playbackRate;
            ASSERT_EQ(transport->updateClipMetadata(1, *metadata), SessionGraphError::OK);
            ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);

            std::vector<std::vector<float>> output(channels, std::vector<float>(blockSize, 0.0f));
            std::vector<float*> pointers(channels);
            for (uint16_t channel = 0; channel < channels; ++channel) {
              pointers[channel] = output[channel].data();
            }
            transport->processAudio(pointers.data(), channels, blockSize);
            const int64_t target = static_cast<int64_t>(6.75 * static_cast<double>(engineRate));
            ASSERT_EQ(transport->seekClip(1, target), SessionGraphError::OK);
            RtGuardState::reset();
            {
              RtSection section;
              transport->processAudio(pointers.data(), channels, blockSize);
            }
            transport->processCallbacks();

            for (uint16_t channel = 0; channel < channels; ++channel) {
              const float scale = static_cast<float>(channel + 1) / static_cast<float>(channels);
              EXPECT_NEAR(trailingMean(output[channel]), -0.20f * scale, 0.12f * scale);
            }
            EXPECT_EQ(callback.seeks.load(), 1);
            EXPECT_EQ(callback.underruns.load(), 0);
            EXPECT_EQ(RtGuardState::allocViolations(), 0u);
            EXPECT_EQ(RtGuardState::deallocViolations(), 0u);
          }
        }
      }
    }
  }
}

TEST_F(StreamingSeekMatrixTest, PageBoundaryAtFourTimesRatePrimesBothPages) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 1024;
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  transport->setPreparedSourceMaxFrames(rate);
  SeekCallback callback;
  transport->setCallback(&callback);
  ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  auto metadata = transport->getClipMetadata(1);
  ASSERT_TRUE(metadata.has_value());
  metadata->playbackRate = 4.0;
  ASSERT_EQ(transport->updateClipMetadata(1, *metadata), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);
  const int64_t target = 2 * static_cast<int64_t>(StreamingClipSource::kPageFrames) - 32;
  ASSERT_EQ(transport->seekClip(1, target), SessionGraphError::OK);
  RtGuardState::reset();
  {
    RtSection section;
    transport->processAudio(buffers, 1, block);
  }
  transport->processCallbacks();
  EXPECT_GT(std::abs(trailingMean(output)), 0.05f);
  EXPECT_EQ(callback.underruns.load(), 0);
  EXPECT_EQ(RtGuardState::totalViolations(), 0u);
}

TEST_F(StreamingSeekMatrixTest, TwoQueuedDistantSeeksRenderFinalAcceptedTarget) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 1024;
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  transport->setPreparedSourceMaxFrames(rate);
  SeekCallback callback;
  transport->setCallback(&callback);
  ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  auto metadata = transport->getClipMetadata(1);
  ASSERT_TRUE(metadata.has_value());
  metadata->playbackRate = 4.0;
  ASSERT_EQ(transport->updateClipMetadata(1, *metadata), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);

  const int64_t firstTarget = 4 * static_cast<int64_t>(StreamingClipSource::kPageFrames) - 32;
  const int64_t finalTarget = static_cast<int64_t>(6.75 * static_cast<double>(rate));
  ASSERT_EQ(transport->seekClip(1, firstTarget), SessionGraphError::OK);
  ASSERT_EQ(transport->seekClip(1, finalTarget), SessionGraphError::OK);
  RtGuardState::reset();
  {
    RtSection section;
    transport->processAudio(buffers, 1, block);
  }
  transport->processCallbacks();
  EXPECT_NEAR(trailingMean(output), -0.20f, 0.12f);
  EXPECT_EQ(callback.seeks.load(), 2);
  EXPECT_EQ(callback.underruns.load(), 0);
  EXPECT_EQ(RtGuardState::totalViolations(), 0u);
}

TEST_F(StreamingSeekMatrixTest, CommandQueueSaturationRejectsBeforePriming) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 512;
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  transport->setPreparedSourceMaxFrames(rate);
  SeekCallback callback;
  transport->setCallback(&callback);
  ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);
  const int64_t priorPosition = transport->getClipPosition(1);
  for (size_t index = 0; index < 255; ++index) {
    ASSERT_EQ(transport->updateClipGain(1, 0.0f), SessionGraphError::OK);
  }
  EXPECT_EQ(transport->seekClip(1, static_cast<int64_t>(6.75 * static_cast<double>(rate))),
            SessionGraphError::InternalError);
  transport->processAudio(buffers, 1, block);
  transport->processCallbacks();
  EXPECT_GT(transport->getClipPosition(1), priorPosition);
  EXPECT_EQ(callback.seeks.load(), 0);
  EXPECT_EQ(callback.underruns.load(), 0);
}

TEST_F(StreamingSeekMatrixTest, PendingSeekBlocksUnregisterAndReplacementUntilConsumed) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 512;
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  transport->setPreparedSourceMaxFrames(rate);
  ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);

  ASSERT_EQ(transport->seekClip(1, static_cast<int64_t>(6.75 * static_cast<double>(rate))),
            SessionGraphError::OK);
  EXPECT_EQ(transport->unregisterClipAudio(1), SessionGraphError::NotReady);
  EXPECT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)),
            SessionGraphError::NotReady);
  transport->processAudio(buffers, 1, block);
  EXPECT_EQ(transport->unregisterClipAudio(1), SessionGraphError::NotReady);
  ASSERT_EQ(transport->panic(), SessionGraphError::OK);
  transport->processAudio(buffers, 1, block);
  ASSERT_EQ(transport->unregisterClipAudio(1), SessionGraphError::OK);
  EXPECT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
}

TEST_F(StreamingSeekMatrixTest, QueuedStartAndExactEofSeekBlockUnregisterUntilConsumed) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 512;
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  transport->setPreparedSourceMaxFrames(rate);
  ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  EXPECT_EQ(transport->unregisterClipAudio(1), SessionGraphError::NotReady);
  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);
  ASSERT_EQ(transport->seekClip(1, 9 * static_cast<int64_t>(rate)), SessionGraphError::OK);
  EXPECT_EQ(transport->unregisterClipAudio(1), SessionGraphError::NotReady);
  transport->processAudio(buffers, 1, block);
  ASSERT_EQ(transport->panic(), SessionGraphError::OK);
  transport->processAudio(buffers, 1, block);
  ASSERT_EQ(transport->unregisterClipAudio(1), SessionGraphError::OK);
}

TEST_F(StreamingSeekMatrixTest, LoopSeekAtOrPastTrimOutPrimesEvictedTrimIn) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 1024;
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const int64_t trimIn = 3 * page + 100;
  const int64_t trimOut = 4 * page + 2000;
  const int64_t fileLength = 9 * static_cast<int64_t>(rate);
  const int64_t evictionTarget = 5 * page + 1000;

  for (const int64_t target : {trimOut, fileLength}) {
    SCOPED_TRACE("target=" + std::to_string(target));
    TransportConfig config{.sampleRate = rate,
                           .outputChannels = 1,
                           .maxBlockFrames = block,
                           .maxActiveVoices = 32,
                           .numGroups = 1,
                           .maxSourceChannels = 1,
                           .sourceChannelPolicy = SourceChannelPolicy::Discrete};
    auto transport = std::make_unique<TransportController>(nullptr, config);
    transport->setPreparedSourceMaxFrames(rate);
    SeekCallback callback;
    transport->setCallback(&callback);
    ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
    ASSERT_EQ(transport->updateClipTrimPoints(1, trimIn, fileLength), SessionGraphError::OK);
    ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);

    std::vector<float> output(block);
    float* buffers[] = {output.data()};
    transport->processAudio(buffers, 1, block);

    // Render far from trim-IN so read() retires its initial worker page.
    ASSERT_EQ(transport->seekClip(1, evictionTarget), SessionGraphError::OK);
    transport->processAudio(buffers, 1, block);

    ASSERT_EQ(transport->updateClipTrimPoints(1, trimIn, trimOut), SessionGraphError::OK);
    ASSERT_EQ(transport->setClipLoopMode(1, true), SessionGraphError::OK);
    ASSERT_EQ(transport->seekClip(1, target), SessionGraphError::OK);
    RtGuardState::reset();
    {
      RtSection section;
      transport->processAudio(buffers, 1, block);
    }
    transport->processCallbacks();

    EXPECT_NEAR(trailingMean(output), -0.60f, 0.12f);
    EXPECT_EQ(callback.seeks.load(), 2);
    EXPECT_EQ(callback.underruns.load(), 0);
    EXPECT_EQ(RtGuardState::totalViolations(), 0u);
  }
}

TEST_F(StreamingSeekMatrixTest, StartUsesCommandPrimeWhenSteadyWindowIsFull) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 1024;
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const int64_t trimIn = 3 * page + 100;
  const int64_t fileLength = 9 * static_cast<int64_t>(rate);
  const int64_t evictionTarget = 5 * page + 1000;

  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  transport->setPreparedSourceMaxFrames(rate);
  SeekCallback callback;
  transport->setCallback(&callback);
  ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  ASSERT_EQ(transport->updateClipTrimPoints(1, trimIn, fileLength), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);

  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // The target page is already resident, so this advances demand without
  // consuming command-prime capacity. The worker then fills the six steady
  // slots around the target while trim-IN remains evicted.
  ASSERT_EQ(transport->seekClip(1, evictionTarget), SessionGraphError::OK);
  transport->processAudio(buffers, 1, block);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // With every steady slot occupied, start preparation must use the remaining
  // command-prime capacity rather than returning NotReady.
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  EXPECT_EQ(transport->unregisterClipAudio(1), SessionGraphError::NotReady);

  RtGuardState::reset();
  {
    RtSection section;
    transport->processAudio(buffers, 1, block);
  }
  transport->processCallbacks();

  EXPECT_NEAR(trailingMean(output), -0.80f, 0.12f);
  EXPECT_EQ(callback.underruns.load(), 0);
  EXPECT_EQ(RtGuardState::totalViolations(), 0u);
  EXPECT_EQ(transport->unregisterClipAudio(1), SessionGraphError::NotReady);

  // Remove the voice and prove the Start reservation was released after its
  // first render rather than leaking into the registry lifetime.
  ASSERT_EQ(transport->panic(), SessionGraphError::OK);
  transport->processAudio(buffers, 1, block);
  ASSERT_EQ(transport->unregisterClipAudio(1), SessionGraphError::OK);
}

TEST_F(StreamingSeekMatrixTest, SeekBeforeEvictedTrimInPrimesEffectiveFirstRender) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 1024;
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const int64_t trimIn = 3 * page + 100;
  const int64_t fileLength = 9 * static_cast<int64_t>(rate);
  const int64_t evictionTarget = 5 * page + 1000;
  const int64_t preTrimTarget = 2 * page + 1000;

  for (const bool looping : {false, true}) {
    SCOPED_TRACE(looping ? "looping" : "non-looping");
    TransportConfig config{.sampleRate = rate,
                           .outputChannels = 1,
                           .maxBlockFrames = block,
                           .maxActiveVoices = 32,
                           .numGroups = 1,
                           .maxSourceChannels = 1,
                           .sourceChannelPolicy = SourceChannelPolicy::Discrete};
    auto transport = std::make_unique<TransportController>(nullptr, config);
    transport->setPreparedSourceMaxFrames(rate);
    SeekCallback callback;
    transport->setCallback(&callback);
    ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
    ASSERT_EQ(transport->updateClipTrimPoints(1, trimIn, fileLength), SessionGraphError::OK);
    ASSERT_EQ(transport->setClipLoopMode(1, looping), SessionGraphError::OK);
    ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);

    std::vector<float> output(block);
    float* buffers[] = {output.data()};
    transport->processAudio(buffers, 1, block);

    // Evict the trim-IN page from the steady window before the pre-trim seek.
    ASSERT_EQ(transport->seekClip(1, evictionTarget), SessionGraphError::OK);
    transport->processAudio(buffers, 1, block);

    ASSERT_EQ(transport->seekClip(1, preTrimTarget), SessionGraphError::OK);
    RtGuardState::reset();
    {
      RtSection section;
      transport->processAudio(buffers, 1, block);
    }
    transport->processCallbacks();

    EXPECT_NEAR(trailingMean(output), -0.60f, 0.12f);
    EXPECT_EQ(callback.seeks.load(), 2);
    EXPECT_EQ(callback.underruns.load(), 0);
    EXPECT_EQ(RtGuardState::totalViolations(), 0u);
  }
}

TEST_F(StreamingSeekMatrixTest, SegmentProgramSeekPrimesEveryPossibleFirstBlockStart) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 1024;
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  transport->setPreparedSourceMaxFrames(rate);
  SeekCallback callback;
  transport->setCallback(&callback);
  ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);

  auto metadata = transport->getClipMetadata(1);
  ASSERT_TRUE(metadata.has_value());
  metadata->playbackRate = 4.0;
  metadata->loopEnabled = true;
  metadata->segmentCount = 2;
  metadata->segments[0] = {3 * page + 100, 3 * page + 2000, 1};
  metadata->segments[1] = {5 * page + 100, 5 * page + 5000, 1};
  ASSERT_EQ(transport->updateClipMetadata(1, *metadata), SessionGraphError::OK);
  ASSERT_EQ(transport->seekClip(1, 3 * page + 1500), SessionGraphError::OK);
  RtGuardState::reset();
  {
    RtSection section;
    transport->processAudio(buffers, 1, block);
  }
  transport->processCallbacks();
  EXPECT_EQ(callback.seeks.load(), 1);
  EXPECT_EQ(callback.underruns.load(), 0);
  EXPECT_EQ(RtGuardState::totalViolations(), 0u);
}

TEST_F(StreamingSeekMatrixTest, FadeOverlapSeekMovesBothVoicesWithoutUnderrun) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 512;
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  transport->setPreparedSourceMaxFrames(rate);
  SeekCallback callback;
  transport->setCallback(&callback);
  ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  ASSERT_EQ(transport->setClipVoiceMode(1, VoiceMode::MonoWithFadeOverlap), SessionGraphError::OK);
  auto metadata = transport->getClipMetadata(1);
  ASSERT_TRUE(metadata.has_value());
  metadata->stopFadeOutSeconds = 0.1;
  ASSERT_EQ(transport->updateClipMetadata(1, *metadata), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);
  ASSERT_EQ(transport->stopClip(1), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  transport->processAudio(buffers, 1, block);
  ASSERT_EQ(transport->getTotalActiveVoiceCount(), 2u);
  ASSERT_EQ(transport->seekClip(1, static_cast<int64_t>(6.75 * static_cast<double>(rate))),
            SessionGraphError::OK);
  RtGuardState::reset();
  {
    RtSection section;
    transport->processAudio(buffers, 1, block);
  }
  transport->processCallbacks();
  EXPECT_EQ(callback.underruns.load(), 0);
  EXPECT_EQ(RtGuardState::totalViolations(), 0u);
}

TEST(StreamingClipSourcePrimeTest, PrimeFailuresRollBackAndRecover) {
  auto reader =
      std::make_shared<FaultReader>(4 * static_cast<int64_t>(StreamingClipSource::kPageFrames));
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 4 * static_cast<int64_t>(StreamingClipSource::kPageFrames));
  StreamingClipSource::PrimeReservation reservation{};
  reader->failRead.store(true, std::memory_order_relaxed);
  EXPECT_EQ(source.primeForCommand(2 * static_cast<int64_t>(StreamingClipSource::kPageFrames), 32,
                                   reservation),
            SessionGraphError::NotReady);
  EXPECT_EQ(reservation.pageMask, 0);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
  reader->failRead.store(false, std::memory_order_relaxed);
  ASSERT_EQ(source.primeForCommand(2 * static_cast<int64_t>(StreamingClipSource::kPageFrames), 32,
                                   reservation),
            SessionGraphError::OK);
  EXPECT_NE(reservation.pageMask, 0);
  source.releaseCommandPrime(reservation);
  EXPECT_FALSE(source.hasPendingCommandPrimes());

  reservation = {};
  reader->failSeek.store(true, std::memory_order_relaxed);
  EXPECT_EQ(source.primeForCommand(3 * static_cast<int64_t>(StreamingClipSource::kPageFrames), 32,
                                   reservation),
            SessionGraphError::NotReady);
  EXPECT_EQ(reservation.pageMask, 0);
  reader->failSeek.store(false, std::memory_order_relaxed);
  ASSERT_EQ(source.primeForCommand(3 * static_cast<int64_t>(StreamingClipSource::kPageFrames), 32,
                                   reservation),
            SessionGraphError::OK);
  source.releaseCommandPrime(reservation);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
}

TEST(StreamingClipSourcePrimeTest, CommandPrefillUsesPrimeCapacityAfterSteadyExhaustion) {
  // 12 pages: after the seek-eviction refill the widened 6-page steady window
  // must be FULL (pages 4..9) so the 3*page demand page can only be served
  // from command-prime capacity.
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(12 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 12 * page);
  StreamingClipSource::PrimeReservation reservation{};

  ASSERT_EQ(source.prefill(3 * page + 100), SessionGraphError::OK);
  std::vector<float> scratch(32, 0.0f);
  size_t framesRead = 0;
  ASSERT_TRUE(source.read(5 * page + 100, scratch.data(), 32, framesRead));
  source.service();

  EXPECT_EQ(source.prefill(3 * page + 100, 1), SessionGraphError::NotReady);
  ASSERT_EQ(source.prefill(3 * page + 100, 1, &reservation), SessionGraphError::OK);
  const uint16_t commandMask = static_cast<uint16_t>(0xFFFFu << StreamingClipSource::kWindowPages);
  EXPECT_NE(reservation.pageMask & commandMask, 0);
  EXPECT_TRUE(source.read(3 * page + 100, scratch.data(), 32, framesRead));
  EXPECT_EQ(framesRead, 32u);

  source.releaseCommandPrime(reservation);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
}

TEST(StreamingClipSourcePrimeTest, EarlyEofDoesNotPublishSilence) {
  auto reader =
      std::make_shared<FaultReader>(2 * static_cast<int64_t>(StreamingClipSource::kPageFrames));
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 2 * static_cast<int64_t>(StreamingClipSource::kPageFrames));
  StreamingClipSource::PrimeReservation reservation{};
  reader->earlyEof.store(true, std::memory_order_relaxed);
  EXPECT_EQ(source.primeForCommand(0, 32, reservation), SessionGraphError::InternalError);
  EXPECT_EQ(reservation.pageMask, 0);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
  reader->earlyEof.store(false, std::memory_order_relaxed);
  ASSERT_EQ(source.primeForCommand(0, 32, reservation), SessionGraphError::OK);
  source.releaseCommandPrime(reservation);
}

TEST(StreamingClipSourcePrimeTest, PrimeCapacityRejectsAtomicallyAndRecovers) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(8 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 8 * page);
  StreamingClipSource::PrimeReservation first{};
  StreamingClipSource::PrimeReservation second{};
  StreamingClipSource::PrimeReservation rejected{};
  ASSERT_EQ(source.primeForCommand(page - 32, 64, first), SessionGraphError::OK);
  ASSERT_EQ(source.primeForCommand(3 * page - 32, 64, second), SessionGraphError::OK);
  EXPECT_TRUE(source.hasPendingCommandPrimes());
  EXPECT_EQ(source.primeForCommand(5 * page - 32, 64, rejected), SessionGraphError::NotReady);
  EXPECT_EQ(rejected.pageMask, 0);
  source.releaseCommandPrime(first);
  source.releaseCommandPrime(second);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
  std::vector<float> scratch(64, 0.0f);
  size_t framesRead = 0;
  EXPECT_FALSE(source.read(5 * page, scratch.data(), 64, framesRead));
  EXPECT_EQ(framesRead, 0u);
  ASSERT_EQ(source.primeForCommand(5 * page - 32, 64, rejected), SessionGraphError::OK);
  source.releaseCommandPrime(rejected);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
}

TEST(StreamingClipSourcePrimeTest,
     ConcurrentWorkerServiceAndCommandPrimeDoesNotDuplicateOwnership) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(7 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 7 * page);
  std::atomic<bool> stop{false};
  std::thread worker([&]() {
    while (!stop.load(std::memory_order_acquire)) {
      source.service();
    }
  });

  for (size_t index = 0; index < 32; ++index) {
    StreamingClipSource::PrimeReservation reservation{};
    ASSERT_EQ(source.primeForCommand(5 * page, 32, reservation), SessionGraphError::OK);
    ASSERT_NE(reservation.pageMask, 0);
    source.releaseCommandPrime(reservation);
  }
  stop.store(true, std::memory_order_release);
  worker.join();
  EXPECT_FALSE(source.hasPendingCommandPrimes());
}

TEST(StreamingClipSourcePrimeTest, AttachedPrimeDecodesOnWorkerThreadOnly) {
  // OCC191: while attached to a MediaStreamWorker the worker is the SOLE
  // decoder. primeForCommand must request + wait, never decode on the
  // command-priming (control) thread.
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(7 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 7 * page);
  // Keep the attached source alive for the worker's weak references; the
  // no-op deleter prevents freeing the stack-allocated source.
  std::shared_ptr<StreamingClipSource> keepAlive(&source, [](StreamingClipSource*) {});
  MediaStreamWorker worker;
  worker.attach(keepAlive);
  reader->setCallingThread(std::this_thread::get_id());

  StreamingClipSource::PrimeReservation reservation{};
  ASSERT_EQ(source.primeForCommand(2 * page, 32, reservation), SessionGraphError::OK);
  // Any decode/seek on this thread (including the timeout fallback) is a
  // regression: it would stall the worker that must keep the window ahead.
  EXPECT_FALSE(reader->readOnCallingThread.load());
  source.releaseCommandPrime(reservation);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
}

TEST(StreamingClipSourcePrimeTest, AttachedPrimeFailureRollsBackWithoutLeak) {
  // OCC191 failure path: a worker decode failure must surface as NotReady with
  // no pins, no pending-prime accounting, and recover cleanly on retry.
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(7 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 7 * page);
  std::shared_ptr<StreamingClipSource> keepAlive(&source, [](StreamingClipSource*) {});
  // Fail decodes BEFORE attaching so the worker can never make the demanded
  // page resident from its steady window: the prime must take the
  // worker-decode path, which fails deterministically.
  reader->failRead.store(true, std::memory_order_relaxed);
  MediaStreamWorker worker;
  worker.attach(keepAlive);

  StreamingClipSource::PrimeReservation reservation{};
  EXPECT_EQ(source.primeForCommand(2 * page, 32, reservation), SessionGraphError::NotReady);
  EXPECT_EQ(reservation.pageMask, 0);
  EXPECT_FALSE(source.hasPendingCommandPrimes());

  reader->failRead.store(false, std::memory_order_relaxed);
  ASSERT_EQ(source.primeForCommand(2 * page, 32, reservation), SessionGraphError::OK);
  EXPECT_NE(reservation.pageMask, 0);
  source.releaseCommandPrime(reservation);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
}

TEST(StreamingClipSourcePrimeTest, PrefillWidenedWindowCoversFourPagesAhead) {
  // OCC191: the resident window grew from 1 behind + demand + 2 ahead to
  // 1 behind + demand + 4 ahead, so a demand at page 0 must serve page 4.
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(9 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 9 * page);

  ASSERT_EQ(source.prefill(0), SessionGraphError::OK);
  std::vector<float> scratch(32, 0.0f);
  size_t framesRead = 0;
  ASSERT_TRUE(source.read(4 * page + 100, scratch.data(), 32, framesRead));
  EXPECT_EQ(framesRead, 32u);
}

TEST_F(StreamingSeekMatrixTest, LoopViaMetadataPinsAnchorForLoopRestart) {
  // OCC191: the app toggles loop through updateClipMetadata (not
  // setClipLoopMode). The loop-restart page (trim-IN) must be pinned while
  // loop is enabled so the audio-thread loop boundary — which cannot prime —
  // never misses the first read of the loop restart.
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 512;
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const int64_t trimIn = 2 * page;          // page 2
  const int64_t trimOut = 3 * page + 4000;  // inside page 3
  const int64_t evictionTarget = 6 * page;  // far from trim-IN
  const int64_t fileLength = 9 * static_cast<int64_t>(rate);

  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  transport->setPreparedSourceMaxFrames(rate);
  SeekCallback callback;
  transport->setCallback(&callback);
  ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  ASSERT_EQ(transport->updateClipTrimPoints(1, trimIn, fileLength), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);

  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);

  // Evict the trim-IN page: render far away (still inside the file) so read()
  // retires page 2 without natural-ending the voice.
  ASSERT_EQ(transport->seekClip(1, evictionTarget), SessionGraphError::OK);
  transport->processAudio(buffers, 1, block);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  transport->processAudio(buffers, 1, block);

  // Enable loop AND narrow the trim window through updateClipMetadata (the
  // app's actual path for both).
  auto metadata = transport->getClipMetadata(1);
  ASSERT_TRUE(metadata.has_value());
  metadata->trimInSamples = trimIn;
  metadata->trimOutSamples = trimOut;
  metadata->loopEnabled = true;
  ASSERT_EQ(transport->updateClipMetadata(1, *metadata), SessionGraphError::OK);

  // Seek near trim-OUT; the next render crosses the loop boundary and
  // restarts at trim-IN. The loop-restart read must not miss.
  ASSERT_EQ(transport->seekClip(1, trimOut - 2000), SessionGraphError::OK);
  RtGuardState::reset();
  for (int i = 0; i < 12; ++i) {
    {
      RtSection section;
      transport->processAudio(buffers, 1, block);
    }
    transport->processCallbacks();
  }

  EXPECT_EQ(callback.underruns.load(), 0)
      << "loop-restart read at trim-IN missed with loop anchor pinned";
  EXPECT_EQ(RtGuardState::totalViolations(), 0u);
  transport->setCallback(nullptr);
}

TEST_F(StreamingSeekMatrixTest, LoopNudgeChurnKeepsRestartPageResident) {
  // OCC191 app repro: trim-IN nudges (< / >) move trim-IN while loop is
  // enabled. The loop-restart page must follow every nudge so the audio-thread
  // loop boundary never reads a non-resident page at the new trim-IN.
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 512;
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const int64_t baseIn = 2 * page;          // page 2 (small, like app's page-0 nudges)
  const int64_t trimOut = 3 * page + 4000;  // inside page 3
  const int64_t evictionTarget = 6 * page;
  const int64_t fileLength = 9 * static_cast<int64_t>(rate);

  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  transport->setPreparedSourceMaxFrames(rate);
  SeekCallback callback;
  transport->setCallback(&callback);
  ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  ASSERT_EQ(transport->updateClipTrimPoints(1, baseIn, fileLength), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);

  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);

  // Evict the trim-IN page by rendering far away.
  ASSERT_EQ(transport->seekClip(1, evictionTarget), SessionGraphError::OK);
  transport->processAudio(buffers, 1, block);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  transport->processAudio(buffers, 1, block);

  // Enable loop AND narrow the trim window via updateClipMetadata.
  auto metadata = transport->getClipMetadata(1);
  ASSERT_TRUE(metadata.has_value());
  metadata->trimInSamples = baseIn;
  metadata->trimOutSamples = trimOut;
  metadata->loopEnabled = true;
  ASSERT_EQ(transport->updateClipMetadata(1, *metadata), SessionGraphError::OK);

  // Nudge trim-IN left/right a few ticks while loop is enabled — each nudge
  // must re-anchor the loop-restart page (app's < / > keys).
  int64_t trimIn = baseIn;
  for (int tick = 0; tick < 4; ++tick) {
    trimIn -= 640;  // nudge left one tick
    auto m = transport->getClipMetadata(1);
    ASSERT_TRUE(m.has_value());
    m->trimInSamples = trimIn;
    m->loopEnabled = true;
    ASSERT_EQ(transport->updateClipMetadata(1, *m), SessionGraphError::OK);
    transport->processAudio(buffers, 1, block);
  }
  for (int tick = 0; tick < 4; ++tick) {
    trimIn += 640;  // nudge right
    auto m = transport->getClipMetadata(1);
    ASSERT_TRUE(m.has_value());
    m->trimInSamples = trimIn;
    m->loopEnabled = true;
    ASSERT_EQ(transport->updateClipMetadata(1, *m), SessionGraphError::OK);
    transport->processAudio(buffers, 1, block);
  }

  // Seek near trim-OUT and render across the loop boundary repeatedly.
  ASSERT_EQ(transport->seekClip(1, trimOut - 2000), SessionGraphError::OK);
  RtGuardState::reset();
  for (int i = 0; i < 16; ++i) {
    {
      RtSection section;
      transport->processAudio(buffers, 1, block);
    }
    transport->processCallbacks();
  }

  EXPECT_EQ(callback.underruns.load(), 0)
      << "loop restart missed after trim-IN nudges with loop enabled";
  EXPECT_EQ(RtGuardState::totalViolations(), 0u);
  transport->setCallback(nullptr);
}

TEST(ResamplingSeekPrimeTest, WrappedReaderErrorIsNotConvertedToEof) {
  auto reader = std::make_shared<FaultReader>(48000);
  ResamplingAudioFileReader resampling(reader, 44100);
  ASSERT_TRUE(resampling.open("").isOk());
  std::vector<float> samples(512, 0.0f);
  reader->failRead.store(true, std::memory_order_relaxed);
  auto failedRead = resampling.readSamples(samples.data(), samples.size());
  EXPECT_FALSE(failedRead.isOk());
  EXPECT_EQ(failedRead.error, SessionGraphError::NotReady);
  EXPECT_EQ(failedRead.value, 0u);
  const int64_t position = resampling.getCurrentPosition();
  reader->failSeek.store(true, std::memory_order_relaxed);
  EXPECT_EQ(resampling.seek(100), SessionGraphError::NotReady);
  EXPECT_EQ(resampling.getCurrentPosition(), position);
  reader->failSeek.store(false, std::memory_order_relaxed);
  reader->failRead.store(false, std::memory_order_relaxed);
  auto recovered = resampling.readSamples(samples.data(), samples.size());
  EXPECT_TRUE(recovered.isOk());
  EXPECT_GT(recovered.value, 0u);
}

} // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
