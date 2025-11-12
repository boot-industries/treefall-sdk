// SPDX-License-Identifier: MIT

#pragma once

#include "../Audio/AudioEngine.h"
#include <atomic>
#include <functional>
#include <juce_audio_basics/juce_audio_basics.h>

// Forward declaration
class AudioEngine;

/**
 * PreviewPlayer - View controller for Edit Dialog (controls main grid clip)
 *
 * ARCHITECTURE (v0.2.1):
 * - Edit Dialog is a "zoomed view" of the main grid clip
 * - PreviewPlayer is NOT a separate playback instance
 * - Controls the main grid clip via buttonIndex (ONE clip, TWO views)
 * - NO Cue Buss allocation (deferred to future routing requirements)
 * - Edits apply to the main grid clip in real-time
 *
 * INTEGRATION WITH SDK:
 * - OUT point enforcement is SDK-managed (automatic stop/loop at OUT)
 * - Gap-free seeking via seekClip() API (no stop/start cycle)
 * - UI layer just tracks position for playhead visualization
 * - SDK handles all edit law enforcement sample-accurately
 *
 * Features:
 * - Play/Stop controls for main grid clip (via AudioEngine)
 * - Loop mode (SDK automatically restarts at IN when reaching OUT)
 * - Position tracking with callback for waveform playhead
 * - Gap-free waveform scrubbing (seekClip API)
 * - Real-time metadata updates (trim/fade/loop)
 */
class PreviewPlayer : private juce::Timer {
public:
  PreviewPlayer(AudioEngine* audioEngine, int buttonIndex);
  ~PreviewPlayer();

  //==============================================================================
  // Setup

  /// Set trim points (IN/OUT) in samples
  /// @param trimInSamples Trim IN point (sample position)
  /// @param trimOutSamples Trim OUT point (sample position)
  void setTrimPoints(int64_t trimInSamples, int64_t trimOutSamples);

  /// Set loop mode
  /// @param shouldLoop True to loop between IN/OUT points
  void setLoopEnabled(bool shouldLoop);

  /// Set fade times and curves
  /// @param fadeInSeconds Fade-in duration in seconds
  /// @param fadeOutSeconds Fade-out duration in seconds
  /// @param fadeInCurve Fade-in curve type ("Linear", "EqualPower", "Exponential")
  /// @param fadeOutCurve Fade-out curve type ("Linear", "EqualPower", "Exponential")
  void setFades(float fadeInSeconds, float fadeOutSeconds, const juce::String& fadeInCurve,
                const juce::String& fadeOutCurve);

  //==============================================================================
  // Playback Control

  /// Start playback from trim IN point
  void play();

  /// Stop playback
  void stop();

  /// Jump to specific sample position
  /// @param samplePosition Sample position to jump to (clamped to IN/OUT range)
  void jumpTo(int64_t samplePosition);

  /// Check if currently playing
  bool isPlaying() const;

  /// Get current playback position in samples
  int64_t getCurrentPosition() const;

  /// Start position timer (for UI playhead updates at 75 FPS)
  /// NOTE: Automatically started by play(), but can be called manually
  /// if clip is already playing when Edit Dialog opens
  void startPositionTimer();

  /// Stop position timer (stops UI playhead updates)
  void stopPositionTimer();

  //==============================================================================
  // Callbacks

  /// Called when playback position changes (for waveform playhead visualization)
  std::function<void(int64_t samplePosition)> onPositionChanged;

  /// Called when playback stops (reached end or manual stop)
  std::function<void()> onPlaybackStopped;

private:
  //==============================================================================
  // Main grid clip control (view controller pattern)
  AudioEngine* m_audioEngine = nullptr; // Non-owning reference
  int m_buttonIndex = -1;               // Button index (0-47) of main grid clip

  // Playback state (synchronized with main grid clip)
  bool m_loopEnabled = false;

  // Trim points (atomic for thread-safe access from timer callback)
  std::atomic<int64_t> m_trimInSamples{0};
  std::atomic<int64_t> m_trimOutSamples{0};

  // Fade settings (for UI state tracking)
  float m_fadeInSeconds = 0.0f;
  float m_fadeOutSeconds = 0.0f;
  juce::String m_fadeInCurve = "Linear";
  juce::String m_fadeOutCurve = "Linear";

  // File metadata (read from AudioEngine)
  int m_sampleRate = 48000;
  int m_numChannels = 2;
  int64_t m_totalSamples = 0;

  // Position tracking timer (75 FPS for UI playhead updates)
  // NOTE: Edit law enforcement is now SDK-managed (ORP089)
  // - This timer only updates UI playhead position
  // - SDK handles OUT point enforcement automatically
  void timerCallback() override; // Override from juce::Timer

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PreviewPlayer)
};
