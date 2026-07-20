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
void writeMonoFixture(const std::string& path) {
  auto writer = createAudioFileWriter();
  ASSERT_NE(writer, nullptr);

  AudioFileWriterConfig config;
  config.format = AudioFileFormat::WAV;
  config.sample_rate = kSampleRate;
  config.num_channels = 1;
  config.sample_format = AudioSampleFormat::Float32;
  ASSERT_EQ(writer->open(path, config), SessionGraphError::OK);

  std::array<float, kFileFrames> samples{};
  samples.fill(0.5f);
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
    monoPath =
        (std::filesystem::temp_directory_path() / "orpheus_clip_audio_controls_mono.wav").string();
    writeMonoFixture(monoPath);
    transport =
        std::make_unique<TransportController>(nullptr, TransportConfig{.sampleRate = kSampleRate});
    ASSERT_EQ(transport->registerClipAudio(handle, path.c_str()), SessionGraphError::OK);
  }

  void TearDown() override {
    transport.reset();
    std::error_code error;
    std::filesystem::remove(path, error);
    std::filesystem::remove(monoPath, error);
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
  std::string monoPath;
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

TEST_F(ClipAudioControlsTest, LegacyFadeUpdateRespectsStoppedClipTrimWindow) {
  auto value = metadata();
  value.trimInSamples = 0;
  value.trimOutSamples = 100;
  value.fadeInSeconds = 0.0;
  value.fadeOutSeconds = 0.0;
  value.stopFadeOutSeconds = 0.0;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);

  EXPECT_EQ(transport->updateClipFades(handle, 0.0, 0.01, FadeCurve::Linear, FadeCurve::Linear),
            SessionGraphError::InvalidFadeDuration);
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
TEST_F(ClipAudioControlsTest, StopCancelsVoiceWaitingForDelay) {
  auto value = metadata();
  value.fadeInSeconds = 0.0;
  value.fadeOutSeconds = 0.0;
  value.stopFadeOutSeconds = 0.02;
  value.playDelaySeconds = 0.05;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(handle), SessionGraphError::OK);
  ASSERT_EQ(transport->stopClip(handle), SessionGraphError::OK);

  std::array<float, 32> left{};
  std::array<float, 32> right{};
  render(left, right);

  EXPECT_EQ(transport->getClipState(handle), PlaybackState::Stopped);
  for (float sample : left) {
    EXPECT_FLOAT_EQ(sample, 0.0f);
  }
  for (float sample : right) {
    EXPECT_FLOAT_EQ(sample, 0.0f);
  }
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
TEST_F(ClipAudioControlsTest, MonoCenterPanDuplicatesWithConstantPowerGain) {
  constexpr ClipHandle monoHandle = 2;
  ASSERT_EQ(transport->registerClipAudio(monoHandle, monoPath.c_str()), SessionGraphError::OK);
  auto value = transport->getClipMetadata(monoHandle);
  ASSERT_TRUE(value.has_value());
  value->fadeInSeconds = 0.0;
  value->fadeOutSeconds = 0.0;
  value->pan = 0.0f;
  ASSERT_EQ(transport->updateClipMetadata(monoHandle, *value), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(monoHandle), SessionGraphError::OK);

  std::array<float, 16> left{};
  std::array<float, 16> right{};
  render(left, right);

  constexpr float expected = 0.3535533906f;
  EXPECT_NEAR(left.front(), expected, 1.0e-5f);
  EXPECT_NEAR(right.front(), expected, 1.0e-5f);
}

TEST_F(ClipAudioControlsTest, MonoRoutePreservesLevelRegardlessOfPan) {
  TransportConfig config;
  config.sampleRate = kSampleRate;
  config.outputChannels = 1;
  auto monoTransport = std::make_unique<TransportController>(nullptr, config);
  constexpr ClipHandle monoHandle = 3;
  ASSERT_EQ(monoTransport->registerClipAudio(monoHandle, monoPath.c_str()), SessionGraphError::OK);

  auto value = monoTransport->getClipMetadata(monoHandle);
  ASSERT_TRUE(value.has_value());
  value->fadeInSeconds = 0.0;
  value->fadeOutSeconds = 0.0;
  value->pan = 1.0f;
  ASSERT_EQ(monoTransport->updateClipMetadata(monoHandle, *value), SessionGraphError::OK);
  ASSERT_EQ(monoTransport->startClip(monoHandle), SessionGraphError::OK);

  std::array<float, 16> output{};
  float* outputs[] = {output.data()};
  monoTransport->processAudio(outputs, 1, output.size());
  EXPECT_NEAR(output.front(), 0.5f, 1.0e-5f);
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

TEST_F(ClipAudioControlsTest, SegmentProgramRepeatsAndAdvancesWithinOneRenderBlock) {
  auto value = metadata();
  value.fadeInSeconds = 0.0;
  value.fadeOutSeconds = 0.0;
  value.segmentCount = 2;
  value.segments[0] = {100, 116, 2};
  value.segments[1] = {200, 216, 1};
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(handle), SessionGraphError::OK);

  std::array<float, 40> left{};
  std::array<float, 40> right{};
  render(left, right);

  EXPECT_EQ(transport->getClipPosition(handle), 208);
  for (float sample : left)
    EXPECT_GT(std::abs(sample), 0.1f);
}

TEST_F(ClipAudioControlsTest, SegmentProgramValidationIsAtomicAndUnrelatedUpdatesDoNotRestart) {
  auto value = metadata();
  value.fadeInSeconds = 0.0;
  value.fadeOutSeconds = 0.0;
  value.segmentCount = 1;
  value.segments[0] = {100, 200, 3};
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(handle), SessionGraphError::OK);

  std::array<float, 16> left{};
  std::array<float, 16> right{};
  render(left, right);
  EXPECT_EQ(transport->getClipPosition(handle), 116);

  value.gainDb = -3.0f;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  render(left, right);
  EXPECT_EQ(transport->getClipPosition(handle), 132);

  auto invalid = value;
  invalid.segments[0].repeatCount = 0;
  EXPECT_EQ(transport->updateClipMetadata(handle, invalid), SessionGraphError::InvalidParameter);
  invalid = value;
  invalid.segments[0].endSample = kFileFrames + 1;
  EXPECT_EQ(transport->updateClipMetadata(handle, invalid), SessionGraphError::InvalidParameter);

  const auto restored = metadata();
  EXPECT_EQ(restored.segmentCount, 1u);
  EXPECT_EQ(restored.segments[0], value.segments[0]);
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

TEST_F(ClipAudioControlsTest, DspProgramRoundTripsAndInvalidUpdateIsAtomic) {
  auto value = metadata();
  value.dsp.eq[0].enabled = true;
  value.dsp.eq[0].frequencyHz = 2500.0f;
  value.dsp.eq[0].gainDb = 4.5f;
  value.dsp.compressor.enabled = true;
  value.dsp.compressor.ratio = 3.0f;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  EXPECT_EQ(metadata().dsp, value.dsp);

  auto invalid = value;
  invalid.dsp.width.amount = 2.5f;
  EXPECT_EQ(transport->updateClipMetadata(handle, invalid), SessionGraphError::InvalidParameter);
  EXPECT_EQ(metadata().dsp, value.dsp);
}

TEST_F(ClipAudioControlsTest, PerVoiceDspRunsBeforeClipGainAndPan) {
  auto value = metadata();
  value.fadeInSeconds = 0.0;
  value.fadeOutSeconds = 0.0;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(handle), SessionGraphError::OK);

  std::array<float, 16> left{};
  std::array<float, 16> right{};
  render(left, right);
  EXPECT_NE(left.front(), right.front());

  value.dsp.width.enabled = true;
  value.dsp.width.amount = 0.0f;
  value.dsp.limiter.enabled = true;
  value.dsp.limiter.ceilingDb = -12.0411998f;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  render(left, right);

  for (size_t frame = 0; frame < left.size(); ++frame) {
    EXPECT_NEAR(left[frame], right[frame], 1.0e-6f);
    EXPECT_LT(std::abs(left[frame]), 0.251f);
  }
}

} // namespace
