// SPDX-License-Identifier: MIT

#pragma once

#include "Session/SessionManager.h"
#include <algorithm>
#include <juce_core/juce_core.h>

namespace occ {

inline bool displayNameLooksLikeFileStem(const SessionManager::ClipData& clip) {
  if (clip.displayName.empty())
    return true;

  const auto fileStem = juce::File(juce::String(clip.filePath)).getFileNameWithoutExtension();
  return juce::String(clip.displayName).trim() == fileStem.trim();
}

inline double durationSecondsFor(const SessionManager::ClipData& clip) {
  if (clip.sampleRate <= 0 || clip.durationSamples <= 0)
    return 0.0;

  return static_cast<double>(clip.durationSamples) / static_cast<double>(clip.sampleRate);
}

inline SessionManager::ClipData
applyReplacementPolicy(const SessionManager::ClipData& previous,
                       const SessionManager::ClipData& replacement) {
  auto merged = replacement;

  // Preserve operator-authored identity/routing decisions that are independent
  // of the underlying media file. These map to live-show muscle memory: colour,
  // group routing, level, loop, and stop-others behaviour should not surprise
  // an operator just because the audio asset was swapped.
  if (!displayNameLooksLikeFileStem(previous))
    merged.displayName = previous.displayName;
  merged.color = previous.color;
  merged.clipGroup = previous.clipGroup;
  merged.gainDb = previous.gainDb;
  merged.loopEnabled = previous.loopEnabled;
  merged.stopOthersEnabled = previous.stopOthersEnabled;

  // Trim points are media-specific. Reset replacement clips to their full file
  // range so stale IN/OUT points from a longer/shorter file cannot create silent
  // pads or illegal boundaries.
  merged.trimInSamples = 0;
  merged.trimOutSamples = std::max<int64_t>(0, replacement.durationSamples);

  // Fades are operator intent, but clamp them to half the new clip duration so
  // a short replacement cannot spend its entire playback inside overlapping fade
  // ramps. Curves remain preserved because they describe the operator's desired
  // feel, not old-media coordinates.
  const double halfDurationSeconds = durationSecondsFor(replacement) * 0.5;
  if (halfDurationSeconds > 0.0) {
    merged.fadeInSeconds = std::clamp(previous.fadeInSeconds, 0.0, halfDurationSeconds);
    merged.fadeOutSeconds = std::clamp(previous.fadeOutSeconds, 0.0, halfDurationSeconds);
  } else {
    merged.fadeInSeconds = 0.0;
    merged.fadeOutSeconds = 0.0;
  }
  merged.fadeInCurve = previous.fadeInCurve;
  merged.fadeOutCurve = previous.fadeOutCurve;

  return merged;
}

} // namespace occ
