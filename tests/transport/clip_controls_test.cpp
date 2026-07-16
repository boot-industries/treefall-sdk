// SPDX-License-Identifier: MIT

#include "../../src/core/transport/transport_controller.h"

#include <orpheus/audio_file_writer.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <string>

using namespace orpheus;

namespace {

constexpr uint32_t kSampleRate = 48000;
constexpr int kFileFrames = 4096;

void writeStereoFixture(const std::string& path) {
  auto writer = createAudioFileWriter();
  ASSERT_NE(writer, nullptr);

  AudioFileWriterConfig config;
  config.format = AudioFileFormat::WAV;
  config.sample_rate = kSampleRate;
  config.num_channels = 2;
  config.sample_format = AudioSampleFormat::Float32;
  ASSERT_EQ(writer->open(path, config), SessionGraphError::OK);

  std::array<float, kFileFrames * 2> samples{};
  for (size_t frame = 0; frame < kFileFrames; ++frame) {
    samples[frame * 2] = 0.25f;
    samples[frame * 2 + 1] = 0.5f;
  }
  const Result<size_t> written = writer->writeSamples(samples.data(), kFileFrames);
  ASSERT_TRUE(written.isOk()) << written.errorMessage;
  ASSERT_EQ(written.value, kFileFrames);
  ASSERT_EQ(writer->close(), SessionGraphError::OK);
}

class ClipAudioControlsTest : public ::testing::Test {
protected:
  void SetUp() override {
    path = (std::filesystem::temp_directory_path() / "orpheus_clip_audio_controls.wav").string();
    writeStereoFixture(path);
    transport =
        std::make_unique<TransportController>(nullptr, TransportConfig{.sampleRate = kSampleRate});
    ASSERT_EQ(transport->registerClipAudio(handle, path.c_str()), SessionGraphError::OK);
  }

  void TearDown() override {
    transport.reset();
    std::error_code error;
    std::filesystem::remove(path, error);
  }

  ClipMetadata metadata() const {
    const auto value = transport->getClipMetadata(handle);
    EXPECT_TRUE(value.has_value());
    return value.value_or(ClipMetadata{});
  }

  template <size_t Frames>
  void render(std::array<float, Frames>& left, std::array<float, Frames>& right) {
    left.fill(0.0f);
    right.fill(0.0f);
    float* outputs[] = {left.data(), right.data()};
    transport->processAudio(outputs, 2, Frames);
  }

  std::unique_ptr<TransportController> transport;
  std::string path;
  const ClipHandle handle = 1;
};

TEST_F(ClipAudioControlsTest, MetadataRoundTripsAndRejectsUnsafeBounds) {
  auto value = metadata();
  value.stopFadeOutSeconds = 0.02;
  value.stopFadeOutCurve = FadeCurve::Exponential;
  value.muted = true;
  value.pan = -0.4f;
  value.playbackRate = 1.25;
  value.playDelaySeconds = 0.05;

  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  const auto restored = metadata();
  EXPECT_DOUBLE_EQ(restored.stopFadeOutSeconds, 0.02);
  EXPECT_EQ(restored.stopFadeOutCurve, FadeCurve::Exponential);
  EXPECT_TRUE(restored.muted);
  EXPECT_FLOAT_EQ(restored.pan, -0.4f);
  EXPECT_DOUBLE_EQ(restored.playbackRate, 1.25);
  EXPECT_DOUBLE_EQ(restored.playDelaySeconds, 0.05);

  value.pan = std::numeric_limits<float>::quiet_NaN();
  EXPECT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::InvalidParameter);
  value.pan = 0.0f;
  value.playbackRate = 4.01;
  EXPECT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::InvalidParameter);
  value.playbackRate = 1.0;
  value.playDelaySeconds = 100.0;
  EXPECT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::InvalidParameter);
  value.playDelaySeconds = 0.0;
  value.stopFadeOutSeconds = 1.0;
  EXPECT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::InvalidFadeDuration);
}

TEST_F(ClipAudioControlsTest, DelayDefersAudioWithoutAdvancingSource) {
  auto value = metadata();
  value.fadeInSeconds = 0.0;
  value.fadeOutSeconds = 0.0;
  value.playDelaySeconds = 0.001;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(handle), SessionGraphError::OK);

  std::array<float, 64> left{};
  std::array<float, 64> right{};
  render(left, right);

  for (size_t frame = 0; frame < 48; ++frame) {
    EXPECT_FLOAT_EQ(left[frame], 0.0f);
    EXPECT_FLOAT_EQ(right[frame], 0.0f);
  }
  EXPECT_GT(std::abs(left[48]), 0.1f);
  EXPECT_GT(std::abs(right[48]), 0.2f);
  EXPECT_EQ(transport->getClipPosition(handle), 16);
}

TEST_F(ClipAudioControlsTest, LiveMuteRampsToSilenceWhileTransportAdvances) {
  auto value = metadata();
  value.fadeInSeconds = 0.0;
  value.fadeOutSeconds = 0.0;
  value.muted = false;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(handle), SessionGraphError::OK);

  std::array<float, 32> left{};
  std::array<float, 32> right{};
  render(left, right);
  ASSERT_GT(std::abs(left.front()), 0.1f);

  value.muted = true;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  render(left, right);
  EXPECT_GT(std::abs(left.front()), std::abs(left.back()));

  for (int block = 0; block < 8; ++block) {
    render(left, right);
  }
  for (float sample : left) {
    EXPECT_NEAR(sample, 0.0f, 1.0e-6f);
  }
  for (float sample : right) {
    EXPECT_NEAR(sample, 0.0f, 1.0e-6f);
  }
  EXPECT_EQ(transport->getClipPosition(handle), 320);
  EXPECT_FLOAT_EQ(metadata().gainDb, value.gainDb);
}

TEST_F(ClipAudioControlsTest, HardLeftPanSuppressesOnlyRightOutput) {
  auto value = metadata();
  value.fadeInSeconds = 0.0;
  value.fadeOutSeconds = 0.0;
  value.pan = -1.0f;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(handle), SessionGraphError::OK);

  std::array<float, 16> left{};
  std::array<float, 16> right{};
  render(left, right);

  EXPECT_GT(std::abs(left[0]), 0.1f);
  for (float sample : right) {
    EXPECT_NEAR(sample, 0.0f, 1.0e-6f);
  }
}

TEST_F(ClipAudioControlsTest, PlaybackRateControlsSourcePosition) {
  auto value = metadata();
  value.fadeInSeconds = 0.0;
  value.fadeOutSeconds = 0.0;
  value.playbackRate = 2.0;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(handle), SessionGraphError::OK);

  std::array<float, 32> left{};
  std::array<float, 32> right{};
  render(left, right);
  EXPECT_EQ(transport->getClipPosition(handle), 64);

  value.playbackRate = 0.5;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  ASSERT_EQ(transport->seekClip(handle, 0), SessionGraphError::OK);
  render(left, right);
  EXPECT_EQ(transport->getClipPosition(handle), 16);
}

TEST_F(ClipAudioControlsTest, OperatorStopFadeIsIndependentFromTailFade) {
  auto value = metadata();
  value.fadeInSeconds = 0.0;
  value.fadeOutSeconds = 0.05;
  value.stopFadeOutSeconds = 0.0;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(handle), SessionGraphError::OK);

  std::array<float, 16> left{};
  std::array<float, 16> right{};
  render(left, right);
  ASSERT_EQ(transport->stopClip(handle), SessionGraphError::OK);
  render(left, right);
  EXPECT_EQ(transport->getClipState(handle), PlaybackState::Stopped);
}

TEST_F(ClipAudioControlsTest, OperatorStopFadeUsesOutputFrameDuration) {
  auto value = metadata();
  value.fadeInSeconds = 0.0;
  value.fadeOutSeconds = 0.0;
  value.stopFadeOutSeconds = 0.001;
  value.playbackRate = 4.0;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(handle), SessionGraphError::OK);

  std::array<float, 16> left{};
  std::array<float, 16> right{};
  render(left, right);
  ASSERT_EQ(transport->stopClip(handle), SessionGraphError::OK);
  render(left, right);
  EXPECT_EQ(transport->getClipState(handle), PlaybackState::Stopping);
  render(left, right);
  EXPECT_EQ(transport->getClipState(handle), PlaybackState::Stopping);
  render(left, right);
  EXPECT_EQ(transport->getClipState(handle), PlaybackState::Stopped);
}

TEST_F(ClipAudioControlsTest, SessionDefaultsNormalizeClipControlBounds) {
  SessionDefaults defaults;
  defaults.stopFadeOutSeconds = -1.0;
  defaults.pan = 2.0f;
  defaults.playbackRate = 0.1;
  defaults.playDelaySeconds = 100.0;
  transport->setSessionDefaults(defaults);

  const SessionDefaults normalized = transport->getSessionDefaults();
  EXPECT_DOUBLE_EQ(normalized.stopFadeOutSeconds, 0.01);
  EXPECT_FLOAT_EQ(normalized.pan, 1.0f);
  EXPECT_DOUBLE_EQ(normalized.playbackRate, 0.25);
  EXPECT_DOUBLE_EQ(normalized.playDelaySeconds, 99.9);

  constexpr ClipHandle secondHandle = 2;
  ASSERT_EQ(transport->registerClipAudio(secondHandle, path), SessionGraphError::OK);
  const auto secondMetadata = transport->getClipMetadata(secondHandle);
  ASSERT_TRUE(secondMetadata.has_value());
  EXPECT_DOUBLE_EQ(secondMetadata->stopFadeOutSeconds, 0.01);
  EXPECT_FLOAT_EQ(secondMetadata->pan, 1.0f);
  EXPECT_DOUBLE_EQ(secondMetadata->playbackRate, 0.25);
  EXPECT_DOUBLE_EQ(secondMetadata->playDelaySeconds, 99.9);
}

} // namespace
