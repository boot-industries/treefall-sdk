// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>
#include <memory>
#include <optional>
#include <orpheus/audio_driver.h>
#include <orpheus/audio_file_reader.h>
#include <orpheus/transport_controller.h>
#include <unordered_map>

// Forward declare concrete class for extended API
namespace orpheus {
class TransportController;
}

/**
 * AudioEngine - Integrates Orpheus SDK with Clip Composer
 *
 * This class serves as the bridge between JUCE UI and Orpheus SDK's
 * real-time audio infrastructure.
 *
 * Architecture:
 * - Owns ITransportController, IAudioDriver
 * - Manages clip loading via IAudioFileReader
 * - Posts callbacks to UI thread via juce::MessageManager
 * - Thread-safe command posting from UI
 *
 * Threading Model:
 * - Construction/destruction: UI thread
 * - startClip(), stopClip(), etc.: UI thread (lock-free)
 * - Audio callback: Audio thread (real-time safe)
 * - SDK callbacks: Posted to UI thread via MessageManager
 */
class AudioEngine : public orpheus::ITransportCallback, public orpheus::IAudioCallback {
public:
  //==============================================================================
  AudioEngine();
  ~AudioEngine() override;

  //==============================================================================
  // Initialization

  /// Initialize audio engine with sample rate
  /// @param sampleRate Sample rate in Hz (e.g., 48000)
  /// @return true on success
  bool initialize(uint32_t sampleRate = 48000);

  /// Start audio processing
  /// @return true on success
  bool start();

  /// Stop audio processing
  void stop();

  /// Check if engine is running
  bool isRunning() const;

  //==============================================================================
  // Clip Management (UI Thread)

  /// Load audio file to clip slot (registers with transport controller)
  /// @param buttonIndex Button index (0-47 for MVP)
  /// @param filePath Absolute path to audio file
  /// @return true on success
  bool loadClip(int buttonIndex, const juce::String& filePath);

  /// Get audio file metadata for loaded clip
  /// @param buttonIndex Button index
  /// @return Metadata if clip loaded, empty optional otherwise
  std::optional<orpheus::AudioFileMetadata> getClipMetadata(int buttonIndex) const;

  /// Unload clip from slot
  /// @param buttonIndex Button index
  void unloadClip(int buttonIndex);

  /// Update clip metadata (trim points, fades, etc.)
  /// @param buttonIndex Button index (0-47 for MVP)
  /// @param trimInSamples Trim IN point in samples
  /// @param trimOutSamples Trim OUT point in samples
  /// @param fadeInSeconds Fade-in duration in seconds
  /// @param fadeOutSeconds Fade-out duration in seconds
  /// @param fadeInCurve Fade-in curve type ("Linear", "EqualPower", "Exponential")
  /// @param fadeOutCurve Fade-out curve type ("Linear", "EqualPower", "Exponential")
  /// @return true if metadata was updated successfully
  bool updateClipMetadata(int buttonIndex, int64_t trimInSamples, int64_t trimOutSamples,
                          double fadeInSeconds, double fadeOutSeconds,
                          const juce::String& fadeInCurve, const juce::String& fadeOutCurve);

  //==============================================================================
  // Playback Control (UI Thread, Lock-Free)

  /// Start playing a clip
  /// @param buttonIndex Button index
  /// @return true if command posted successfully
  bool startClip(int buttonIndex);

  /// Stop playing a clip (with fade-out)
  /// @param buttonIndex Button index
  /// @return true if command posted successfully
  bool stopClip(int buttonIndex);

  /// Stop all playing clips
  void stopAllClips();

  /// PANIC - immediate mute (no fade-out)
  void panicStop();

  //==============================================================================
  // State Queries (Any Thread)

  /// Check if clip is playing
  /// @param buttonIndex Button index
  /// @return true if playing
  bool isClipPlaying(int buttonIndex) const;

  /// Get clip playback state
  /// @param buttonIndex Button index
  /// @return Playback state
  orpheus::PlaybackState getClipState(int buttonIndex) const;

  /// Get current transport position
  /// @return Transport position (samples, seconds, beats)
  orpheus::TransportPosition getCurrentPosition() const;

  /// Check if clip is currently looping
  /// @param buttonIndex Button index
  /// @return true if clip is looping
  bool isClipLooping(int buttonIndex) const;

  /// Get current playback position for a main grid clip
  /// @param buttonIndex Button index
  /// @return Current position in samples (relative to file start), or -1 if not playing
  int64_t getClipPosition(int buttonIndex) const;

  /// Set loop mode for a main grid clip
  /// @param buttonIndex Button index (0-47)
  /// @param shouldLoop true = loop indefinitely, false = play once
  /// @return true if loop mode was set successfully
  bool setClipLoopMode(int buttonIndex, bool shouldLoop);

  /// Seek main grid clip to arbitrary position (gap-free, sample-accurate)
  /// @param buttonIndex Button index (0-47)
  /// @param position Target position in samples (0-based file offset)
  /// @return true if seek succeeded, false if clip not loaded or not playing
  ///
  /// Use case: OCC Edit Dialog waveform click-to-jog (seamless seek while playing)
  bool seekClip(int buttonIndex, int64_t position);

  /// Get audio latency in samples (device + buffer)
  /// @return Total latency in samples
  uint32_t getLatencySamples() const;

  /// Get configured buffer size
  /// @return Buffer size in samples
  uint32_t getBufferSize() const;

  /// Get configured sample rate
  /// @return Sample rate in Hz
  uint32_t getSampleRate() const;

  //==============================================================================
  // Audio Device Management (for Audio Settings Dialog)

  /// Get list of available audio device names
  /// @return Vector of device names (empty if driver doesn't support enumeration)
  std::vector<std::string> getAvailableDevices() const;

  /// Get current audio device name
  /// @return Device name, or "Default Device" if not set
  std::string getCurrentDeviceName() const;

  /// Set audio device and restart audio with new settings
  /// @param deviceName Device name (from getAvailableDevices()), or empty for default
  /// @param sampleRate Sample rate in Hz (44100, 48000, 96000)
  /// @param bufferSize Buffer size in samples (64, 128, 256, 512, 1024, 2048)
  /// @return true on success, false if device initialization failed
  bool setAudioDevice(const std::string& deviceName, uint32_t sampleRate, uint32_t bufferSize);

  //==============================================================================
  // Cue Buss Management (for Edit Dialog preview)
  // ClipHandles 10001+ are Cue Busses (dynamically allocated)

  /// Allocate a Cue Buss for preview playback
  /// @param filePath Audio file path to load into Cue Buss
  /// @return Cue Buss ClipHandle (10001+), or 0 if allocation failed
  orpheus::ClipHandle allocateCueBuss(const juce::String& filePath);

  /// Release a Cue Buss (stops playback, unloads clip, frees handle)
  /// @param cueBussHandle Cue Buss ClipHandle to release
  void releaseCueBuss(orpheus::ClipHandle cueBussHandle);

  /// Start Cue Buss playback
  /// @param cueBussHandle Cue Buss ClipHandle
  /// @return true if started successfully
  bool startCueBuss(orpheus::ClipHandle cueBussHandle);

  /// Stop Cue Buss playback
  /// @param cueBussHandle Cue Buss ClipHandle
  /// @return true if stopped successfully
  bool stopCueBuss(orpheus::ClipHandle cueBussHandle);

  /// Restart Cue Buss playback from current IN point (seamless, no gap)
  /// @param cueBussHandle Cue Buss ClipHandle (10001+)
  /// @return true if restart succeeded, false if handle invalid
  ///
  /// Use case: OCC Edit Dialog < > trim buttons
  bool restartCueBuss(orpheus::ClipHandle cueBussHandle);

  /// Update Cue Buss metadata (trim points, fades)
  /// @param cueBussHandle Cue Buss ClipHandle
  /// @param trimInSamples Trim IN point
  /// @param trimOutSamples Trim OUT point
  /// @param fadeInSeconds Fade-in duration
  /// @param fadeOutSeconds Fade-out duration
  /// @param fadeInCurve Fade-in curve type
  /// @param fadeOutCurve Fade-out curve type
  /// @return true if updated successfully
  bool updateCueBussMetadata(orpheus::ClipHandle cueBussHandle, int64_t trimInSamples,
                             int64_t trimOutSamples, double fadeInSeconds, double fadeOutSeconds,
                             const juce::String& fadeInCurve, const juce::String& fadeOutCurve);

  /// Check if Cue Buss is playing
  /// @param cueBussHandle Cue Buss ClipHandle
  /// @return true if playing
  bool isCueBussPlaying(orpheus::ClipHandle cueBussHandle) const;

  /// Get audio file metadata for a Cue Buss
  /// @param cueBussHandle Cue Buss ClipHandle
  /// @return Metadata if Cue Buss exists, empty optional otherwise
  std::optional<orpheus::AudioFileMetadata>
  getCueBussMetadata(orpheus::ClipHandle cueBussHandle) const;

  /// Set loop mode for a Cue Buss
  /// @param cueBussHandle Cue Buss ClipHandle
  /// @param enabled true = loop indefinitely, false = play once
  /// @return true if loop mode was set successfully
  bool setCueBussLoop(orpheus::ClipHandle cueBussHandle, bool enabled);

  /// Get current playback position for a Cue Buss
  /// @param cueBussHandle Cue Buss ClipHandle
  /// @return Current position in samples, or 0 if not playing/invalid
  int64_t getCueBussPosition(orpheus::ClipHandle cueBussHandle) const;

  /// Check if Cue Buss is currently looping
  /// @param cueBussHandle Cue Buss ClipHandle
  /// @return true if Cue Buss is looping
  bool isCueBussLooping(orpheus::ClipHandle cueBussHandle) const;

  /// Seek Cue Buss to arbitrary position (gap-free, sample-accurate)
  /// @param cueBussHandle Cue Buss ClipHandle
  /// @param position Target position in samples (0-based file offset)
  /// @return true if seek succeeded, false if handle invalid or not playing
  ///
  /// Use case: OCC waveform click-to-jog (seamless seek while playing)
  bool seekCueBuss(orpheus::ClipHandle cueBussHandle, int64_t position);

  //==============================================================================
  // Callbacks from UI

  /// Set callback for clip events
  std::function<void(int buttonIndex, orpheus::PlaybackState state)> onClipStateChanged;

  /// Set callback for buffer underruns (audio dropouts)
  std::function<void()> onBufferUnderrunDetected;

  //==============================================================================
  // ITransportCallback overrides (Posted to UI Thread)
  void onClipStarted(orpheus::ClipHandle handle, orpheus::TransportPosition position) override;
  void onClipStopped(orpheus::ClipHandle handle, orpheus::TransportPosition position) override;
  void onClipLooped(orpheus::ClipHandle handle, orpheus::TransportPosition position) override;
  void onBufferUnderrun(orpheus::TransportPosition position) override;

  //==============================================================================
  // IAudioCallback override (Audio Thread, Real-Time Safe)
  void processAudio(const float** input_buffers, float** output_buffers, size_t num_channels,
                    size_t num_frames) override;

private:
  //==============================================================================
  // Helper methods
  orpheus::ClipHandle getClipHandle(int buttonIndex) const;
  int getButtonIndexFromHandle(orpheus::ClipHandle handle) const;

  //==============================================================================
  // SDK Components
  // Note: Using concrete class for extended API (registerClipAudio, processAudio, etc.)
  std::unique_ptr<orpheus::TransportController> m_transportController;
  std::unique_ptr<orpheus::IAudioDriver> m_audioDriver;

  // Clip handle mapping (buttonIndex → ClipHandle)
  std::array<orpheus::ClipHandle, 48> m_clipHandles; // MVP: 48 buttons

  // Clip metadata cache (for UI queries)
  std::array<std::optional<orpheus::AudioFileMetadata>, 48> m_clipMetadata;

  // Cue Buss management (ClipHandles 10001+)
  std::vector<orpheus::ClipHandle> m_cueBussHandles; // Active Cue Busses
  orpheus::ClipHandle m_nextCueBussHandle = 10001;   // Next available Cue Buss handle
  std::unordered_map<orpheus::ClipHandle, orpheus::AudioFileMetadata>
      m_cueBussMetadata; // Cue Buss metadata cache

  // Engine state
  uint32_t m_sampleRate = 48000;
  uint32_t m_bufferSize = 512;
  bool m_initialized = false;
  std::string m_currentDeviceName = "Default Device"; // Current audio device name

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
