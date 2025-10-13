// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace orpheus {

// Forward declarations
namespace core {
class SessionGraph;
} // namespace core

using ClipHandle = uint64_t;

/// Error codes for session graph operations
enum class SessionGraphError : uint8_t {
  OK = 0,
  InvalidHandle = 1,
  InvalidParameter = 2,
  NotReady = 3,
  NotSupported = 4,
  NotInitialized = 5, ///< Added for routing matrix
  InternalError = 255
};

/// Playback state for clips
enum class PlaybackState : uint8_t {
  Stopped = 0, ///< Clip is not playing
  Playing = 1, ///< Clip is actively playing
  Paused = 2,  ///< Clip is paused (reserved for future use)
  Stopping = 3 ///< Clip is fading out before stop
};

/// Sample-accurate transport position
/// Sample counts are authoritative; seconds and beats are derived
struct TransportPosition {
  int64_t samples; ///< Absolute position in samples (authoritative)
  double seconds;  ///< Derived: samples / sample_rate
  double beats;    ///< Derived: seconds * tempo / 60.0
};

/// Callback interface for transport events
/// All callbacks are invoked on the UI thread (NOT audio thread)
class ITransportCallback {
public:
  virtual ~ITransportCallback() = default;

  /// Called when a clip starts playing
  /// @param handle The clip that started
  /// @param position Current transport position
  virtual void onClipStarted(ClipHandle handle, TransportPosition position) = 0;

  /// Called when a clip stops playing
  /// @param handle The clip that stopped
  /// @param position Current transport position
  virtual void onClipStopped(ClipHandle handle, TransportPosition position) = 0;

  /// Called when a clip loops back to start
  /// @param handle The clip that looped
  /// @param position Current transport position
  virtual void onClipLooped(ClipHandle handle, TransportPosition position) = 0;

  /// Called when a buffer underrun occurs (audio dropout)
  /// @param position Position where underrun occurred
  virtual void onBufferUnderrun(TransportPosition position) = 0;
};

/// Transport controller interface for sample-accurate clip playback
///
/// This interface provides real-time control over clip playback with
/// sample-accurate timing and thread-safe operation.
///
/// Thread Safety:
/// - startClip(), stopClip(), stopAllClips(), stopAllInGroup(): Thread-safe, callable from UI
/// thread
/// - getClipState(), isClipPlaying(), getCurrentPosition(): Thread-safe, callable from any thread
/// - setCallback(): UI thread only
///
/// Audio Thread Guarantees:
/// - No allocations in audio callback
/// - Lock-free command processing
/// - Sample-accurate timing (±1 sample tolerance)
class ITransportController {
public:
  virtual ~ITransportController() = default;

  /// Start playback of a specific clip
  ///
  /// This function is thread-safe and can be called from the UI thread.
  /// The clip will begin playing on the next audio buffer callback.
  ///
  /// @param handle The clip to start (must be a valid handle from SessionGraph)
  /// @return SessionGraphError::OK on success, or error code on failure
  ///
  /// @note If the clip is already playing, this function has no effect
  /// @note Playback will honor trim points and fade-in settings from clip metadata
  virtual SessionGraphError startClip(ClipHandle handle) = 0;

  /// Stop playback of a specific clip
  ///
  /// This function is thread-safe and can be called from the UI thread.
  /// The clip will fade out over 10ms (default) before stopping.
  ///
  /// @param handle The clip to stop
  /// @return SessionGraphError::OK on success, or error code on failure
  ///
  /// @note If the clip is not playing, this function has no effect
  /// @note Fade-out duration is configurable via clip metadata
  virtual SessionGraphError stopClip(ClipHandle handle) = 0;

  /// Stop all currently playing clips
  ///
  /// All active clips will fade out simultaneously over 10ms (default).
  ///
  /// @return SessionGraphError::OK on success, or error code on failure
  virtual SessionGraphError stopAllClips() = 0;

  /// Stop all clips in a specific Clip Group
  ///
  /// This is useful for "FIFO choke" behavior where only one clip
  /// in a group can play at a time.
  ///
  /// @param groupIndex Clip Group index (0-3)
  /// @return SessionGraphError::OK on success, or error code on failure
  virtual SessionGraphError stopAllInGroup(uint8_t groupIndex) = 0;

  /// Query the playback state of a specific clip
  ///
  /// This function is thread-safe and can be called from any thread.
  ///
  /// @param handle The clip to query
  /// @return Current playback state
  virtual PlaybackState getClipState(ClipHandle handle) const = 0;

  /// Check if a clip is currently playing
  ///
  /// This is a convenience function equivalent to:
  /// `getClipState(handle) == PlaybackState::Playing`
  ///
  /// @param handle The clip to query
  /// @return true if playing, false otherwise
  virtual bool isClipPlaying(ClipHandle handle) const = 0;

  /// Get the current transport position
  ///
  /// This function is thread-safe and can be called from any thread.
  /// Position is sample-accurate (±1 sample tolerance).
  ///
  /// @return Current transport position (samples, seconds, beats)
  virtual TransportPosition getCurrentPosition() const = 0;

  /// Register a callback for transport events
  ///
  /// Only one callback can be registered at a time. Calling this function
  /// replaces any previously registered callback.
  ///
  /// @param callback Callback interface (pass nullptr to unregister)
  ///
  /// @note This function must be called from the UI thread only
  /// @note Callbacks are invoked on the UI thread, not the audio thread
  virtual void setCallback(ITransportCallback* callback) = 0;
};

/// Create a transport controller instance
///
/// @param sessionGraph The session graph containing clip metadata
/// @param sampleRate Audio sample rate (e.g., 48000)
/// @return Unique pointer to transport controller
std::unique_ptr<ITransportController> createTransportController(core::SessionGraph* sessionGraph,
                                                                uint32_t sampleRate);

} // namespace orpheus
