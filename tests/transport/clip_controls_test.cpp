// SPDX-License-Identifier: MIT

#include "../../src/core/transport/transport_controller.h"

#include <orpheus/audio_file_writer.h>
#include <orpheus/clip_dsp.h>

#include "../support/rt_guard.hpp"
#include "../support/unique_temp_dir.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <string>
#include <utility>

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
    path = orpheus::tests::support::UniqueTempPath("orpheus_clip_audio_controls", ".wav").string();
    writeStereoFixture(path);
    monoPath = orpheus::tests::support::UniqueTempPath("orpheus_clip_audio_controls_mono", ".wav")
                   .string();
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

TEST_F(ClipAudioControlsTest, InitialFadeInAppliesWithAndWithoutLooping) {
  const std::array<std::pair<FadeCurve, float>, 3> curves = {{
      {FadeCurve::Linear, 0.5f},
      {FadeCurve::EqualPower, 0.70710678f},
      {FadeCurve::Exponential, 0.25f},
  }};
  std::array<float, 256> left{};
  std::array<float, 256> right{};
  for (const bool looping : {false, true}) {
    for (const auto& [curve, midpointGain] : curves) {
      SCOPED_TRACE(looping ? "loop enabled" : "non-looped");
      SCOPED_TRACE(static_cast<int>(curve));
      ASSERT_EQ(transport->panic(), SessionGraphError::OK);
      render(left, right);
      auto value = metadata();
      value.trimInSamples = 128;
      value.trimOutSamples = 640;
      value.fadeInSeconds = 0.005; // 240 source samples
      value.fadeInCurve = curve;
      value.fadeOutSeconds = 0.005;
      value.loopEnabled = looping;
      ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
      ASSERT_EQ(transport->startClip(handle), SessionGraphError::OK);
      render(left, right);
      // The fade starts from silence. The EqualPower curve is now evaluated
      // with the pinned platform-independent elementary function, whose
      // absolute error near zero is below the float32 noise floor, so the
      // first sample is silent rather than bit-exactly zero.
      EXPECT_NEAR(left.front(), 0.0f, 1.0e-15f);
      EXPECT_NEAR(right.front(), 0.0f, 1.0e-15f);
      EXPECT_NEAR(left[120], 0.25f * midpointGain, 1.0e-5f);
      EXPECT_NEAR(right[120], 0.5f * midpointGain, 1.0e-5f);
      EXPECT_NEAR(left[240], 0.25f, 1.0e-5f);
      EXPECT_NEAR(right[240], 0.5f, 1.0e-5f);
      for (size_t frame = 1; frame <= 240; ++frame)
        EXPECT_GE(left[frame], left[frame - 1] - 1.0e-6f);
    }
  }
}

TEST_F(ClipAudioControlsTest, LoopFadeInSkipsWrapButResetsOnRefireAndFreshStart) {
  auto value = metadata();
  value.trimInSamples = 128;
  value.trimOutSamples = 640;
  value.fadeInSeconds = 0.005;
  value.fadeInCurve = FadeCurve::Linear;
  value.fadeOutSeconds = 0.005;
  value.stopFadeOutSeconds = 0.005;
  value.stopFadeOutCurve = FadeCurve::Linear;
  value.loopEnabled = true;
  value.voiceMode = VoiceMode::MonoWithFadeOverlap;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(handle), SessionGraphError::OK);
  std::array<float, 256> left{};
  std::array<float, 256> right{};
  render(left, right);
  EXPECT_FLOAT_EQ(left.front(), 0.0f);
  EXPECT_NEAR(left[120], 0.125f, 1.0e-5f);
  // The rest of the first window and repeated automatic wraps remain at unity
  // despite configured natural IN and OUT fades.
  for (int block = 0; block < 5; ++block) {
    render(left, right);
    for (size_t frame = 0; frame < left.size(); ++frame) {
      EXPECT_NEAR(left[frame], 0.25f, 1.0e-5f);
      EXPECT_NEAR(right[frame], 0.5f, 1.0e-5f);
    }
  }
  ASSERT_EQ(transport->startClip(handle), SessionGraphError::OK);
  render(left, right);
  EXPECT_FLOAT_EQ(left.front(), 0.0f);
  EXPECT_LT(left[120], 0.1f)
      << "Refire applies the initial envelope alongside the restart crossfade";
  EXPECT_GT(left[120], 0.04f);
  EXPECT_NEAR(left[240], 0.25f, 1.0e-5f);
  EXPECT_EQ(transport->getActiveVoiceCount(handle), 1u);
  ASSERT_EQ(transport->stopClip(handle), SessionGraphError::OK);
  render(left, right);
  EXPECT_NEAR(left.front(), 0.25f, 1.0e-5f);
  EXPECT_NEAR(left[120], 0.125f, 1.0e-5f);
  EXPECT_FLOAT_EQ(left[240], 0.0f);
  EXPECT_EQ(transport->getClipState(handle), PlaybackState::Stopped);
  ASSERT_EQ(transport->startClip(handle), SessionGraphError::OK);
  render(left, right);
  EXPECT_FLOAT_EQ(left.front(), 0.0f);
  EXPECT_NEAR(left[120], 0.125f, 1.0e-5f)
      << "A fresh voice after Stop receives the enabled fade-in";
}

TEST_F(ClipAudioControlsTest, ZeroFadeInBypassesInitialAndLoopEnvelopes) {
  std::array<float, 256> left{};
  std::array<float, 256> right{};
  for (const bool looping : {false, true}) {
    SCOPED_TRACE(looping ? "loop enabled" : "non-looped");
    ASSERT_EQ(transport->panic(), SessionGraphError::OK);
    render(left, right);
    auto value = metadata();
    value.trimInSamples = 128;
    value.trimOutSamples = 640;
    value.fadeInSeconds = 0.0;
    value.fadeInCurve = FadeCurve::Exponential;
    value.fadeOutSeconds = 0.0;
    value.loopEnabled = looping;
    ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
    ASSERT_EQ(transport->startClip(handle), SessionGraphError::OK);
    for (int block = 0; block < (looping ? 5 : 1); ++block) {
      render(left, right);
      for (size_t frame = 0; frame < left.size(); ++frame) {
        EXPECT_NEAR(left[frame], 0.25f, 1.0e-5f);
        EXPECT_NEAR(right[frame], 0.5f, 1.0e-5f);
      }
    }
  }
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

TEST_F(ClipAudioControlsTest, InactiveLaneStaysSilentAfterHostRouteEdit) {
  auto value = metadata();
  value.fadeInSeconds = 0.0;
  value.fadeOutSeconds = 0.0;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(handle), SessionGraphError::OK);

  std::array<float, 16> left{};
  std::array<float, 16> right{};
  render(left, right);
  ASSERT_GT(std::abs(left.front()), 0.1f);

  ASSERT_EQ(transport->panic(), SessionGraphError::OK);
  render(left, right);
  // The transport owns its channel routes: every public channel-route writer
  // is rejected, so the inactive lane cannot be routed back into the mix.
  auto* routing = transport->getRoutingMatrix();
  ASSERT_NE(routing, nullptr);
  EXPECT_EQ(routing->setChannelRoute(0, 0, 0), SessionGraphError::NotSupported);
  EXPECT_EQ(routing->setChannelGroup(0, 0), SessionGraphError::NotSupported);
  EXPECT_EQ(routing->configureChannel(0, ChannelConfig{}), SessionGraphError::NotSupported);
  render(left, right);

  for (float sample : left) {
    EXPECT_NEAR(sample, 0.0f, 1.0e-6f);
  }
  for (float sample : right) {
    EXPECT_NEAR(sample, 0.0f, 1.0e-6f);
  }
}

// Under StereoPairs the transport pans each voice lane hard left or right in
// its routing matrix. Snapshot recall (SceneManager builds snapshots with
// default channel configs) and reset() must keep those lane pans, so a
// playing stereo voice keeps its imaging bit for bit.
TEST_F(ClipAudioControlsTest, SnapshotRecallAndResetKeepTransportLanePans) {
  transport = std::make_unique<TransportController>(
      nullptr, TransportConfig{.sampleRate = kSampleRate,
                               .sourceChannelPolicy = SourceChannelPolicy::StereoPairs});
  ASSERT_EQ(transport->registerClipAudio(handle, path.c_str()), SessionGraphError::OK);
  auto value = metadata();
  value.fadeInSeconds = 0.0;
  value.fadeOutSeconds = 0.0;
  value.loopEnabled = true;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(handle), SessionGraphError::OK);

  std::array<float, 16> referenceLeft{};
  std::array<float, 16> referenceRight{};
  render(referenceLeft, referenceRight);
  render(referenceLeft, referenceRight);
  // The fixture holds 0.25 on its left channel and 0.5 on its right.
  ASSERT_GT(referenceLeft.front(), 0.1f);
  ASSERT_GT(referenceRight.front(), referenceLeft.front() + 0.1f);

  auto* routing = transport->getRoutingMatrix();
  ASSERT_NE(routing, nullptr);
  EXPECT_EQ(routing->setChannelPan(0, 0.0f), SessionGraphError::NotSupported);

  const auto expectReferenceImaging = [&](const char* step) {
    std::array<float, 16> left{};
    std::array<float, 16> right{};
    render(left, right);
    for (size_t frame = 0; frame < left.size(); ++frame) {
      EXPECT_EQ(left[frame], referenceLeft[frame]) << step << " frame " << frame;
      EXPECT_EQ(right[frame], referenceRight[frame]) << step << " frame " << frame;
    }
  };

  RoutingSnapshot recalled = routing->saveSnapshot("recall");
  for (auto& channel : recalled.channels) {
    channel = ChannelConfig{};
  }
  ASSERT_EQ(routing->loadSnapshot(recalled), SessionGraphError::OK);
  expectReferenceImaging("loadSnapshot");

  ASSERT_EQ(routing->reset(), SessionGraphError::OK);
  expectReferenceImaging("reset");
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
  value.dsp.eq.enabled = true;
  value.dsp.eq.bands[0].enabled = true;
  value.dsp.eq.bands[0].frequencyHz = 2500.0f;
  value.dsp.eq.bands[0].gainDb = 4.5f;
  value.dsp.compressor.enabled = true;
  value.dsp.compressor.ratio = 3.0f;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  EXPECT_EQ(metadata().dsp, value.dsp);

  auto invalid = value;
  invalid.dsp.width.amount = 2.5f;
  EXPECT_EQ(transport->updateClipMetadata(handle, invalid), SessionGraphError::InvalidParameter);
  EXPECT_EQ(metadata().dsp, value.dsp);
}

TEST_F(ClipAudioControlsTest, DspPayloadAllocationFailureIsAtomic) {
  auto value = metadata();
  value.gainDb = -6.0f;
  value.dsp.eq.enabled = true;
  value.dsp.eq.bands[0].enabled = true;
  value.dsp.eq.bands[0].frequencyHz = 2500.0f;
  value.dsp.eq.bands[0].gainDb = 4.5f;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);

  auto updated = value;
  updated.gainDb = -3.0f;
  updated.pan = 0.5f;
  updated.dsp.eq.bands[0].gainDb = -4.5f;
  updated.dsp.width.enabled = true;
  updated.dsp.width.amount = 0.0f;

  // make_shared puts the prepared processor and its reference counts in one
  // block: sizeof(ClipDspProcessor) plus a small control-block header. Nothing
  // earlier in updateClipMetadata allocates, so this window selects exactly
  // the DSP payload allocation.
  constexpr size_t kPayloadBytes = sizeof(ClipDspProcessor);
  constexpr size_t kControlBlockSlack = 64;
  SessionGraphError result = SessionGraphError::OK;
  uint32_t injectedFailures = 0;
  size_t failedBytes = 0;
  {
    orpheus::tests::support::FailNextAllocation failPayload(kPayloadBytes,
                                                            kPayloadBytes + kControlBlockSlack);
    result = transport->updateClipMetadata(handle, updated);
    injectedFailures = failPayload.failures();
    failedBytes = failPayload.failedBytes();
  }
  ASSERT_EQ(injectedFailures, 1u);
  EXPECT_GE(failedBytes, kPayloadBytes);
  EXPECT_LE(failedBytes, kPayloadBytes + kControlBlockSlack);
  EXPECT_EQ(result, SessionGraphError::InternalError);

  const auto retained = metadata();
  EXPECT_FLOAT_EQ(retained.gainDb, value.gainDb);
  EXPECT_FLOAT_EQ(retained.pan, value.pan);
  EXPECT_EQ(retained.dsp, value.dsp);

  // The failed update published nothing and released the registry lock, so
  // the same update succeeds once allocation does.
  ASSERT_EQ(transport->updateClipMetadata(handle, updated), SessionGraphError::OK);
  const auto applied = metadata();
  EXPECT_FLOAT_EQ(applied.gainDb, updated.gainDb);
  EXPECT_FLOAT_EQ(applied.pan, updated.pan);
  EXPECT_EQ(applied.dsp, updated.dsp);
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
  // Width glides over kClipDspTransitionSeconds (480 frames at 48 kHz); the
  // limiter applies at once.
  std::array<float, 512> dspLeft{};
  std::array<float, 512> dspRight{};
  render(dspLeft, dspRight);

  for (size_t frame = 0; frame < dspLeft.size(); ++frame) {
    if (frame >= 479)
      EXPECT_NEAR(dspLeft[frame], dspRight[frame], 1.0e-6f) << "frame " << frame;
    EXPECT_LT(std::abs(dspLeft[frame]), 0.251f) << "frame " << frame;
  }
}

TEST_F(ClipAudioControlsTest, DspEnableStartsOnNextBlockAndSettlesWithinTransition) {
  auto value = metadata();
  value.fadeInSeconds = 0.0;
  value.fadeOutSeconds = 0.0;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(handle), SessionGraphError::OK);

  std::array<float, 16> identityLeft{};
  std::array<float, 16> identityRight{};
  render(identityLeft, identityRight);
  ASSERT_NE(identityLeft.front(), identityRight.front());

  // The program update reaches the voice through the command queue, which the
  // next render drains before it renders: that block starts collapsing at its
  // first frame and is fully collapsed once the 480-frame transition ends.
  value.dsp.width.enabled = true;
  value.dsp.width.amount = 0.0f;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  std::array<float, 512> left{};
  std::array<float, 512> right{};
  render(left, right);
  EXPECT_LT(std::abs(left[0] - right[0]), std::abs(identityLeft[0] - identityRight[0]));
  for (size_t frame = 479; frame < left.size(); ++frame) {
    EXPECT_EQ(left[frame], right[frame]) << "frame " << frame;
    EXPECT_GT(std::abs(left[frame]), 0.1f) << "frame " << frame;
  }

  value.dsp.width.enabled = false;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  render(left, right);
  for (size_t frame = 480; frame < left.size(); ++frame) {
    EXPECT_EQ(left[frame], identityLeft[0]) << "frame " << frame;
    EXPECT_EQ(right[frame], identityRight[0]) << "frame " << frame;
  }

  std::array<float, 16> afterLeft{};
  std::array<float, 16> afterRight{};
  render(afterLeft, afterRight);
  for (size_t frame = 0; frame < afterLeft.size(); ++frame) {
    EXPECT_EQ(afterLeft[frame], identityLeft[frame]) << "frame " << frame;
    EXPECT_EQ(afterRight[frame], identityRight[frame]) << "frame " << frame;
  }
}

TEST_F(ClipAudioControlsTest, EqUpdateOnPlayingVoiceIsContinuous) {
  auto value = metadata();
  value.fadeInSeconds = 0.0;
  value.fadeOutSeconds = 0.0;
  // The +12 dB shelf lifts the DC fixture near full scale; -12 dB of clip gain
  // keeps the mix below the master clipping-protection knee so the measured
  // ratio is the EQ's alone.
  value.gainDb = -12.0f;
  value.dsp.eq.enabled = true;
  value.dsp.eq.bands[0] = {true,  EqBandType::LowShelf, EqBandOrder::Second, 160.0f,
                           12.0f, 0.70710678f};
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(handle), SessionGraphError::OK);

  std::array<float, 1024> settleLeft{};
  std::array<float, 1024> settleRight{};
  render(settleLeft, settleRight);

  // A 0.5 dB nudge on a playing voice must glide, not reset the filter state.
  value.dsp.eq.bands[0].gainDb = 11.5f;
  ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
  std::array<float, 1024> left{};
  std::array<float, 1024> right{};
  render(left, right);

  float previous = settleLeft.back();
  float largestStep = 0.0f;
  for (const float sample : left) {
    largestStep = std::max(largestStep, std::abs(sample - previous));
    previous = sample;
  }
  EXPECT_LT(largestStep, 1.0e-3f);
  EXPECT_NEAR(left.back() / settleLeft.back(), std::pow(10.0f, -0.5f / 20.0f), 1.0e-4f);
}

// Renders the survivors of a voice-pool compaction and compares them with a
// run in which the removed voice never existed. Each survivor carries its own
// gain, pan, rate, loop and trim state, so a field the compaction move drops
// or mixes up changes the output.
class TransportCompactionTest : public ::testing::Test {
protected:
  static constexpr size_t kBlock = 256;

  void SetUp() override {
    path = orpheus::tests::support::UniqueTempPath("orpheus_transport_compaction", ".wav").string();
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
      samples[frame * 2] = static_cast<float>(frame % 97) / 194.0f;
      samples[frame * 2 + 1] = -static_cast<float>(frame % 61) / 122.0f;
    }
    const Result<size_t> written = writer->writeSamples(samples.data(), kFileFrames);
    ASSERT_TRUE(written.isOk()) << written.errorMessage;
    ASSERT_EQ(writer->close(), SessionGraphError::OK);
  }

  void TearDown() override {
    std::error_code error;
    std::filesystem::remove(path, error);
  }

  // Registers handles 1-3 with distinct controls; handle 2 starts muted so it
  // contributes exact silence while it is live.
  std::unique_ptr<TransportController> makeTransport() const {
    auto transport =
        std::make_unique<TransportController>(nullptr, TransportConfig{.sampleRate = kSampleRate});
    for (ClipHandle clip = 1; clip <= 3; ++clip) {
      EXPECT_EQ(transport->registerClipAudio(clip, path.c_str()), SessionGraphError::OK);
      auto value = transport->getClipMetadata(clip);
      EXPECT_TRUE(value.has_value());
      if (!value) {
        continue;
      }
      value->fadeInSeconds = 0.0;
      value->fadeOutSeconds = 0.0;
      value->loopEnabled = clip != 2;
      value->muted = clip == 2;
      value->gainDb = clip == 1 ? -3.0f : -9.0f;
      value->pan = clip == 1 ? 0.25f : -0.5f;
      value->playbackRate = clip == 3 ? 0.75 : 1.0;
      value->trimInSamples = clip == 3 ? 100 : 0;
      EXPECT_EQ(transport->updateClipMetadata(clip, *value), SessionGraphError::OK);
    }
    return transport;
  }

  std::string path;
};

TEST_F(TransportCompactionTest, RemovingMiddleVoicePreservesSurvivors) {
  auto compacted = makeTransport();
  auto reference = makeTransport();
  ASSERT_EQ(compacted->startClip(1), SessionGraphError::OK);
  ASSERT_EQ(compacted->startClip(2), SessionGraphError::OK);
  ASSERT_EQ(compacted->startClip(3), SessionGraphError::OK);
  ASSERT_EQ(reference->startClip(1), SessionGraphError::OK);
  ASSERT_EQ(reference->startClip(3), SessionGraphError::OK);

  std::array<float, kBlock> left{};
  std::array<float, kBlock> right{};
  std::array<float, kBlock> referenceLeft{};
  std::array<float, kBlock> referenceRight{};
  float* outputs[] = {left.data(), right.data()};
  float* referenceOutputs[] = {referenceLeft.data(), referenceRight.data()};
  for (int block = 0; block < 24; ++block) {
    if (block == 3) {
      // The middle voice fades out over silence, then the last voice is
      // compacted into its slot.
      ASSERT_EQ(compacted->stopClip(2), SessionGraphError::OK);
    }
    compacted->processAudio(outputs, 2, kBlock);
    reference->processAudio(referenceOutputs, 2, kBlock);
    for (size_t frame = 0; frame < kBlock; ++frame) {
      ASSERT_EQ(left[frame], referenceLeft[frame]) << "block " << block << " frame " << frame;
      ASSERT_EQ(right[frame], referenceRight[frame]) << "block " << block << " frame " << frame;
    }
  }
  EXPECT_EQ(compacted->getTotalActiveVoiceCount(), 2u);
  EXPECT_GT(std::abs(left[kBlock - 1]) + std::abs(right[kBlock - 1]), 0.0f);
}

} // namespace
