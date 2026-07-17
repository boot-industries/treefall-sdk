// SPDX-License-Identifier: MIT
#define ORPHEUS_TEST_DEFINE_RT_ALLOC_HOOKS
#include "../support/rt_guard.hpp"

#include <orpheus/trigger_voice.h>

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <vector>

namespace {

using orpheus::TriggerVoice;
using orpheus::VoicePolicy;
using orpheus::tests::support::RtGuardState;
using orpheus::tests::support::RtSection;

TEST(TriggerVoiceTest, CopiesInterleavedSampleAndStartsAtExactBufferOffset) {
  TriggerVoice voice;
  std::array<float, 6> sample{1.0F, 10.0F, 2.0F, 20.0F, 3.0F, 30.0F};
  voice.loadSample(sample.data(), 3, 2, VoicePolicy::RetriggerCut, 4);
  sample.fill(99.0F);

  std::array<float, 10> output{};
  voice.trigger(2, 0.5F);
  voice.render(output.data(), 5);

  const std::array<float, 10> expected{0.0F, 0.0F, 0.0F,  0.0F, 0.5F,
                                       5.0F, 1.0F, 10.0F, 1.5F, 15.0F};
  EXPECT_EQ(output, expected);
  EXPECT_EQ(voice.loadedFrames(), 3u);
  EXPECT_EQ(voice.loadedChannels(), 2);
  EXPECT_EQ(voice.voiceCapacity(), 1u);
}

TEST(TriggerVoiceTest, RetriggerCutReplacesTheSoundingInstance) {
  TriggerVoice voice;
  const std::array<float, 4> sample{1.0F, 1.0F, 1.0F, 1.0F};
  voice.loadSample(sample.data(), sample.size(), 1, VoicePolicy::RetriggerCut, 4);

  std::array<float, 2> first{};
  voice.trigger(0, 1.0F);
  voice.render(first.data(), first.size());
  EXPECT_EQ(first, (std::array<float, 2>{1.0F, 1.0F}));

  std::array<float, 4> second{};
  voice.trigger(1, 2.0F);
  voice.render(second.data(), second.size());
  EXPECT_EQ(second, (std::array<float, 4>{1.0F, 2.0F, 2.0F, 2.0F}));
}

TEST(TriggerVoiceTest, RendersMultipleScheduledTriggersAtTheirOwnOffsets) {
  TriggerVoice voice;
  const std::array<float, 4> sample{1.0F, 1.0F, 1.0F, 1.0F};
  voice.loadSample(sample.data(), sample.size(), 1, VoicePolicy::RetriggerCut, 1);

  std::array<float, 5> output{};
  voice.trigger(3, 2.0F);
  voice.trigger(1, 1.0F);
  voice.render(output.data(), output.size());

  EXPECT_EQ(output, (std::array<float, 5>{0.0F, 1.0F, 1.0F, 2.0F, 2.0F}));
}

TEST(TriggerVoiceTest, PolyphonicPoolLayersAndStealsOldest) {
  TriggerVoice voice;
  const std::array<float, 4> sample{1.0F, 1.0F, 1.0F, 1.0F};
  voice.loadSample(sample.data(), sample.size(), 1, VoicePolicy::Polyphonic, 2);

  std::array<float, 1> output{};
  voice.trigger(0, 1.0F);
  voice.render(output.data(), 1);
  EXPECT_FLOAT_EQ(output[0], 1.0F);

  output[0] = 0.0F;
  voice.trigger(0, 2.0F);
  voice.render(output.data(), 1);
  EXPECT_FLOAT_EQ(output[0], 3.0F);

  output[0] = 0.0F;
  voice.trigger(0, 4.0F);
  voice.render(output.data(), 1);
  EXPECT_FLOAT_EQ(output[0], 6.0F);
  EXPECT_EQ(voice.voiceCapacity(), 2u);
}

TEST(TriggerVoiceTest, PitchRatioUsesLinearInterpolationAcrossCallbacks) {
  TriggerVoice voice;
  const std::array<float, 3> sample{0.0F, 2.0F, 4.0F};
  voice.loadSample(sample.data(), sample.size(), 1, VoicePolicy::RetriggerCut, 1);
  voice.trigger(0, 1.0F, 0.5F);

  std::array<float, 2> first{};
  std::array<float, 3> second{};
  voice.render(first.data(), first.size());
  voice.render(second.data(), second.size());

  EXPECT_EQ(first, (std::array<float, 2>{0.0F, 1.0F}));
  EXPECT_EQ(second, (std::array<float, 3>{2.0F, 3.0F, 4.0F}));
}

TEST(TriggerVoiceTest, InvalidLoadsUnloadAndInvalidTriggersAreIgnored) {
  TriggerVoice voice;
  const std::array<float, 1> sample{1.0F};
  const auto loadValid = [&] {
    voice.loadSample(sample.data(), sample.size(), 1, VoicePolicy::RetriggerCut, 1);
    ASSERT_EQ(voice.loadedFrames(), 1u);
  };

  loadValid();
  voice.loadSample(nullptr, 1, 1, VoicePolicy::RetriggerCut, 1);
  EXPECT_EQ(voice.loadedFrames(), 0u);

  loadValid();
  voice.loadSample(sample.data(), 0, 1, VoicePolicy::RetriggerCut, 1);
  EXPECT_EQ(voice.loadedFrames(), 0u);

  loadValid();
  voice.loadSample(sample.data(), 1, 0, VoicePolicy::RetriggerCut, 1);
  EXPECT_EQ(voice.loadedFrames(), 0u);

  loadValid();
  voice.loadSample(sample.data(), std::numeric_limits<size_t>::max(), 2, VoicePolicy::RetriggerCut,
                   1);
  EXPECT_EQ(voice.loadedFrames(), 0u);

  loadValid();
  voice.trigger(0, std::numeric_limits<float>::quiet_NaN());
  voice.trigger(0, 1.0F, std::numeric_limits<float>::infinity());
  voice.trigger(0, 1.0F, 0.0F);
  std::array<float, 1> output{};
  voice.render(output.data(), output.size());
  EXPECT_EQ(output[0], 0.0F);
}

TEST(TriggerVoiceTest, DropsTriggersBeyondTheFixedEventCapacity) {
  TriggerVoice voice;
  const std::array<float, 1> sample{1.0F};
  voice.loadSample(sample.data(), sample.size(), 1, VoicePolicy::RetriggerCut, 1);

  for (size_t trigger = 1; trigger <= TriggerVoice::kMaxPendingTriggers + 1; ++trigger) {
    voice.trigger(0, static_cast<float>(trigger));
  }

  std::array<float, 1> output{};
  voice.render(output.data(), output.size());
  EXPECT_EQ(output[0], static_cast<float>(TriggerVoice::kMaxPendingTriggers));
}

TEST(TriggerVoiceTest, TriggerAndRenderDoNotAllocateOrDeallocate) {
  TriggerVoice voice;
  std::vector<float> sample(480000, 0.25F);
  voice.loadSample(sample.data(), sample.size(), 1, VoicePolicy::Polyphonic, 4);
  std::array<float, 256> output{};

  RtGuardState::reset();
  {
    RtSection section;
    for (size_t block = 0; block < 100; ++block) {
      voice.trigger(block % output.size(), 0.5F, 1.01F);
      voice.render(output.data(), output.size());
    }
  }

  EXPECT_EQ(RtGuardState::allocViolations(), 0u);
  EXPECT_EQ(RtGuardState::deallocViolations(), 0u);
}

} // namespace
