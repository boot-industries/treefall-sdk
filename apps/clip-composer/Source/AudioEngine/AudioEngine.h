// SPDX-License-Identifier: MIT

#pragma once

#include <juce_core/juce_core.h>
#include <unordered_map>

// Forward declare SDK types (will be included in .cpp)
namespace orpheus {
class ITransportController;
class IAudioFileReader;
class IAudioDriver;
class ITransportCallback;
class IAudioCallback;
using ClipHandle = uint32_t; // Handle type for clips and Cue Busses
} // namespace orpheus

// Forward declare internal callback adapter
class AudioEngineCallback;

//==============================================================================
/**
 * AudioEngine - Integration layer between JUCE and Orpheus SDK
 *
 * Responsibilities:
 * - Manage SDK module lifecycles (ITransportController, IAudioFileReader, IAudioDriver)
 * - Adapt JUCE's AudioIODeviceCallback to SDK's IAudioCallback
 * - Process SDK callbacks and post to JUCE message thread
 * - Provide simplified API for UI components
 *
 * Threading:
 * - Created and controlled from JUCE message thread
 * - Owns audio thread via IAudioDriver
 * - Uses lock-free communication (SDK provides this)
 *
 * Usage:
 * @code
 * auto engine = std::make_unique<AudioEngine>();
 * engine->initialize(48000, 512);  // Sample rate, buffer size
 * engine->start();
 * engine->loadClip("/path/to/audio.wav", buttonIndex);
 * engine->triggerClip(buttonIndex);
 * @endcode
 */
class AudioEngine {
public:
  //==============================================================================
  AudioEngine();
  ~AudioEngine();

  //==============================================================================
  /**
   * Initialize audio engine with specified configuration
   *
   * @param sampleRate Sample rate in Hz (typically 48000)
   * @param bufferSize Buffer size in samples (typically 512)
   * @return true if initialization succeeded
   *
   * Call this before start(). Creates SDK components but doesn't start audio yet.
   */
  bool initialize(uint32_t sampleRate, uint16_t bufferSize);

  /**
   * Start audio processing
   *
   * @return true if audio started successfully
   *
   * Starts the audio driver's callback thread. Must call initialize() first.
   */
  bool start();

  /**
   * Stop audio processing
   *
   * Stops the audio driver and waits for audio thread to exit.
   * Safe to call from UI thread.
   */
  void stop();

  /**
   * Check if audio is currently running
   *
   * @return true if audio callback is active
   */
  bool isRunning() const;

  //==============================================================================
  // Cue Buss Management (Preview Audio Architecture)

  /**
   * Allocate a Cue Buss for preview playback
   *
   * @param filePath Absolute path to audio file
   * @return Cue Buss handle (0 = allocation failed)
   *
   * Cue Busses are temporary preview streams that sum to main output.
   */
  orpheus::ClipHandle allocateCueBuss(const juce::String& filePath);

  /**
   * Release a Cue Buss and free resources
   *
   * @param handle Cue Buss handle to release
   * @return true if released successfully
   */
  bool releaseCueBuss(orpheus::ClipHandle handle);

  /**
   * Start Cue Buss playback (idempotent)
   *
   * @param handle Cue Buss handle
   * @return true if started (or already playing)
   *
   * IMPORTANT: This method is idempotent - calling multiple times
   * should always start/restart playback, never toggle.
   */
  bool startCueBuss(orpheus::ClipHandle handle);

  /**
   * Stop Cue Buss playback
   *
   * @param handle Cue Buss handle
   * @return true if stopped successfully
   */
  bool stopCueBuss(orpheus::ClipHandle handle);

  /**
   * Update Cue Buss metadata (trim points, fades)
   *
   * @param handle Cue Buss handle
   * @param trimInSamples Trim IN point
   * @param trimOutSamples Trim OUT point
   * @param fadeInSeconds Fade-in duration
   * @param fadeOutSeconds Fade-out duration
   * @param fadeInCurve Fade-in curve type
   * @param fadeOutCurve Fade-out curve type
   * @return true if updated successfully
   */
  bool updateCueBussMetadata(orpheus::ClipHandle handle, int64_t trimInSamples,
                             int64_t trimOutSamples, float fadeInSeconds, float fadeOutSeconds,
                             const juce::String& fadeInCurve, const juce::String& fadeOutCurve);

  /**
   * Get clip metadata by button index
   *
   * @param buttonIndex Button index (0-959)
   * @return Optional clip metadata
   */
  std::optional<ClipMetadata> getClipMetadata(int buttonIndex) const;

  //==============================================================================
  // Clip Management (to be implemented in Week 7-8)

  /**
   * Load an audio clip from file
   *
   * @param filePath Absolute path to audio file (WAV/AIFF/FLAC)
   * @param buttonIndex Button index (0-959) to assign clip to
   * @return true if clip loaded successfully
   *
   * Loads clip metadata and stores reference. File is read on-demand in audio thread.
   */
  bool loadClip(const juce::String& filePath, int buttonIndex);

  /**
   * Trigger a clip to start playing
   *
   * @param buttonIndex Button index (0-959) of clip to play
   * @return true if command was queued successfully
   *
   * Sends lock-free command to audio thread. Returns immediately.
   */
  bool triggerClip(int buttonIndex);

  /**
   * Stop a currently playing clip
   *
   * @param buttonIndex Button index (0-959) of clip to stop
   * @return true if command was queued successfully
   *
   * Initiates 10ms fade-out. Clip removed from active list after fade completes.
   */
  bool stopClip(int buttonIndex);

  /**
   * Stop all currently playing clips (panic button)
   *
   * @return true if command was queued successfully
   */
  bool stopAllClips();

  /**
   * Update clip metadata (trim points, fades, etc.)
   *
   * @param buttonIndex Button index (0-959) of clip to update
   * @param trimInSamples Trim IN point in samples
   * @param trimOutSamples Trim OUT point in samples
   * @param fadeInSeconds Fade-in duration in seconds
   * @param fadeOutSeconds Fade-out duration in seconds
   * @param fadeInCurve Fade-in curve type ("Linear", "EqualPower", "Exponential")
   * @param fadeOutCurve Fade-out curve type ("Linear", "EqualPower", "Exponential")
   * @return true if metadata was updated successfully
   *
   * Updates trim/fade settings for an already-loaded clip.
   * Call after clip is loaded with loadClip().
   */
  bool updateClipMetadata(int buttonIndex, int64_t trimInSamples, int64_t trimOutSamples,
                          double fadeInSeconds, double fadeOutSeconds,
                          const juce::String& fadeInCurve, const juce::String& fadeOutCurve);

  //==============================================================================
  // Status Queries

  /**
   * Get current transport position
   *
   * @return Sample count since audio started
   *
   * Thread-safe atomic read. Can be called from UI thread.
   */
  int64_t getCurrentPosition() const;

  /**
   * Check if a clip is currently playing
   *
   * @param buttonIndex Button index (0-959) to query
   * @return true if clip is in Playing or Stopping state
   *
   * Note: May be 1 audio buffer stale (~10ms @ 48kHz/512)
   */
  bool isClipPlaying(int buttonIndex) const;

  /**
   * Get current CPU usage percentage
   *
   * @return CPU usage 0.0-100.0 (requires IPerformanceMonitor in Month 4-5)
   *
   * Returns 0.0 if performance monitor not available.
   */
  float getCpuUsage() const;

private:
  //==============================================================================
  // SDK Components (created in initialize())
  std::unique_ptr<orpheus::ITransportController> m_transportController;
  std::unique_ptr<orpheus::IAudioDriver> m_audioDriver;
  std::unique_ptr<AudioEngineCallback> m_audioCallback;
  // std::unique_ptr<orpheus::IRoutingMatrix> m_routingMatrix;  // Month 3-4
  // std::unique_ptr<orpheus::IPerformanceMonitor> m_perfMonitor;  // Month 4-5

  // Configuration
  uint32_t m_sampleRate = 0;
  uint16_t m_bufferSize = 0;
  bool m_initialized = false;

  // Clip metadata storage (buttonIndex → ClipMetadata mapping)
  struct ClipMetadata {
    juce::String filePath;
    int64_t trimInSamples = 0;
    int64_t trimOutSamples = 0;
    double fadeInSeconds = 0.0;
    double fadeOutSeconds = 0.0;
    juce::String fadeInCurve = "Linear";
    juce::String fadeOutCurve = "Linear";
  };
  std::unordered_map<int, ClipMetadata> m_clipMetadata;

  //==============================================================================
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
