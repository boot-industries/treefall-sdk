// SPDX-License-Identifier: MIT
// Clip replacement metadata policy tests (broadcast operator intent preservation)

#include "../Source/Core/ClipReplacementPolicy.h"
#include <gtest/gtest.h>
#include <juce_core/juce_core.h>

namespace {

SessionManager::ClipData makePreviousClip() {
  SessionManager::ClipData clip;
  clip.filePath = "/show/original/stinger.wav";
  clip.displayName = "Custom Stinger";
  clip.color = juce::Colours::orange;
  clip.clipGroup = 2;
  clip.tabIndex = 3;
  clip.sampleRate = 48000;
  clip.durationSamples = 48000 * 10;
  clip.trimInSamples = 24000;
  clip.trimOutSamples = 48000 * 8;
  clip.fadeInSeconds = 2.0;
  clip.fadeOutSeconds = 3.0;
  clip.fadeInCurve = "EqualPower";
  clip.fadeOutCurve = "Exponential";
  clip.gainDb = -4.5;
  clip.loopEnabled = true;
  clip.stopOthersEnabled = true;
  return clip;
}

SessionManager::ClipData makeReplacementClip() {
  SessionManager::ClipData clip;
  clip.filePath = "/show/replacement/hit.wav";
  clip.displayName = "hit";
  clip.color = juce::Colours::blue;
  clip.clipGroup = 0;
  clip.tabIndex = 3;
  clip.sampleRate = 48000;
  clip.durationSamples = 48000 * 4;
  clip.trimInSamples = 0;
  clip.trimOutSamples = clip.durationSamples;
  clip.fadeInSeconds = 0.0;
  clip.fadeOutSeconds = 0.0;
  clip.fadeInCurve = "Linear";
  clip.fadeOutCurve = "Linear";
  clip.gainDb = 0.0;
  clip.loopEnabled = false;
  clip.stopOthersEnabled = false;
  return clip;
}

} // namespace

TEST(ClipReplacementPolicyTest, PreservesOperatorRoutingVisualAndPlaybackIntent) {
  const auto merged = occ::applyReplacementPolicy(makePreviousClip(), makeReplacementClip());

  EXPECT_EQ(merged.filePath, "/show/replacement/hit.wav");
  EXPECT_EQ(merged.displayName, "Custom Stinger");
  EXPECT_EQ(merged.color, juce::Colours::orange);
  EXPECT_EQ(merged.clipGroup, 2);
  EXPECT_EQ(merged.gainDb, -4.5);
  EXPECT_TRUE(merged.loopEnabled);
  EXPECT_TRUE(merged.stopOthersEnabled);
}

TEST(ClipReplacementPolicyTest, KeepsReplacementNameWhenPreviousNameWasOnlyFileStem) {
  auto previous = makePreviousClip();
  previous.displayName = "stinger";

  const auto merged = occ::applyReplacementPolicy(previous, makeReplacementClip());

  EXPECT_EQ(merged.displayName, "hit");
}

TEST(ClipReplacementPolicyTest, ResetsTrimToFullReplacementAndClampsFadesToClipDuration) {
  auto previous = makePreviousClip();
  previous.fadeInSeconds = 8.0;
  previous.fadeOutSeconds = 8.0;
  auto replacement = makeReplacementClip();
  replacement.durationSamples = 48000 * 2;
  replacement.trimOutSamples = replacement.durationSamples;

  const auto merged = occ::applyReplacementPolicy(previous, replacement);

  EXPECT_EQ(merged.trimInSamples, 0);
  EXPECT_EQ(merged.trimOutSamples, replacement.durationSamples);
  EXPECT_EQ(merged.fadeInSeconds, 1.0);
  EXPECT_EQ(merged.fadeOutSeconds, 1.0);
  EXPECT_EQ(merged.fadeInCurve, "EqualPower");
  EXPECT_EQ(merged.fadeOutCurve, "Exponential");
}
