// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/errors.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace orpheus {

// Forward declarations
namespace core {
class SessionGraph;
} // namespace core

using ClipHandle = uint64_t;

/// Fade curve types for clip fades
enum class FadeCurve : uint8_t {
  Linear = 0,     ///< f(x) = x
  EqualPower = 1, ///< f(x) = sin(x * π/2) - constant power crossfades
  Exponential = 2 ///< f(x) = x² - dramatic effect
};

/// Playback state for clips
enum class PlaybackState : uint8_t {
  Stopped = 0, ///< Clip is not playing
  Playing = 1, ///< Clip is actively playing
  Paused = 2,  ///< Clip is paused (reserved for future use)
  Stopping = 3 ///< Clip is fading out before stop
};

/// Voice allocation policy for a clip (ORP127 G5).
///
/// Governs what happens when a clip is fired while one or more of its voices
/// are already active. Host-neutral: broadcast/soundboard hosts typically
/// select MonoWithFadeOverlap; per-track multitrack hosts typically use
/// MonoStrict; layering hosts use Polyphonic.
enum class VoiceMode : uint8_t {
  /// One primary voice per clip identity. Firing while a voice is *playing*
  /// restarts it in place. Firing while a voice is *fading out* leaves that
  /// tail to complete naturally and starts a fresh voice alongside it
  /// (voices == 2 only during the fade-overlap window). Typical for
  /// broadcast/soundboard hosts.
  MonoWithFadeOverlap = 0,

  /// Full polyphony: every fire allocates a new voice, up to
  /// MAX_VOICES_PER_CLIP, with oldest-voice eviction on overflow. This is the
  /// SDK's historical behavior and the default for backward compatibility.
  Polyphonic = 1,

  /// Strict single voice. Firing while playing restarts from zero with no fade
  /// tail — the previous voice is cut and replaced sample-accurately. Typical
  /// for per-track multitrack hosts, and for stingers / hard cues.
  MonoStrict = 2
};

/// Sample-accurate transport position
/// Sample counts are authoritative; seconds and beats are derived
struct TransportPosition {
  int64_t samples; ///< Absolute position in samples (authoritative)
  double seconds;  ///< Derived: samples / sample_rate
  double beats;    ///< Derived: seconds * session tempo (SessionGraph::tempo) / 60.0
};

/// Clip metadata for batch updates
/// Contains all configurable playback parameters for a clip
struct ClipMetadata {
  int64_t trimInSamples = 0;                  ///< Trim IN point in samples (0 = start of file)
  int64_t trimOutSamples = 0;                 ///< Trim OUT point in samples (0 = use file duration)
  double fadeInSeconds = 0.0;                 ///< Fade-in duration in seconds
  double fadeOutSeconds = 0.0;                ///< Fade-out duration in seconds
  FadeCurve fadeInCurve = FadeCurve::Linear;  ///< Fade-in curve type
  FadeCurve fadeOutCurve = FadeCurve::Linear; ///< Fade-out curve type
  bool loopEnabled = false;                   ///< true = loop indefinitely
  bool stopOthersOnPlay = false;              ///< true = stop other clips on play
  float gainDb = 0.0f;                        ///< Gain in decibels (0 = unity)
  VoiceMode voiceMode = VoiceMode::Polyphonic; ///< Voice allocation policy (ORP127 G5)
};

/// Session-level default metadata for new clips.
/// These defaults are applied when registerClipAudio() is called.
struct SessionDefaults {
  double fadeInSeconds = 0.0;                 ///< Default fade-in time (0.0 = no fade)
  double fadeOutSeconds = 0.0;                ///< Default fade-out time (0.0 = no fade)
  FadeCurve fadeInCurve = FadeCurve::Linear;  ///< Default fade-in curve
  FadeCurve fadeOutCurve = FadeCurve::Linear; ///< Default fade-out curve
  bool loopEnabled = false;                   ///< Default loop mode
  bool stopOthersOnPlay = false;              ///< Default "stop others" mode
  float gainDb = 0.0f;                        ///< Default gain in dB (0.0 = unity)

  // Note: Color is not part of SDK metadata; hosts store presentation state
  // (color, labels, etc.) separately.
};

/// Cue point marker within a clip
/// Used for navigation and precise positioning within audio files
struct CuePoint {
  int64_t position; ///< Position in samples (file offset, 0-based)
  std::string name; ///< User label (e.g., "Verse 1", "Chorus")
  uint32_t color;   ///< RGBA color for UI rendering (0xRRGGBBAA format)
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

  /// Called when a clip restarts playback from its IN point
  /// @param handle The clip that restarted
  /// @param position New position after restart (trim IN point)
  ///
  /// @note This fires when restartClip() is called, not on natural loop restart
  /// @see onClipLooped() for loop-triggered restarts
  virtual void onClipRestarted(ClipHandle /*handle*/, TransportPosition /*position*/) {}

  /// Called when a clip position is seeked to arbitrary position
  /// @param handle The clip that was seeked
  /// @param position New position after seek (samples)
  ///
  /// @note This fires when seekClip() is called
  /// @see onClipRestarted() for restart to IN point
  virtual void onClipSeeked(ClipHandle /*handle*/, TransportPosition /*position*/) {}

  /// Called when a buffer underrun occurs (audio dropout)
  /// @param position Position where underrun occurred
  virtual void onBufferUnderrun(TransportPosition position) = 0;
};

/// Transport controller interface for sample-accurate clip playback
///
/// This interface provides real-time control over clip playback with
/// sample-accurate timing and thread-safe operation.
///
/// Threading contract (ORP133 G3 — this is the real, enforced contract):
///
/// - **Control-mutating methods are single-producer.** startClip(), stopClip(),
///   stopAllClips(), stopOtherClips(), restartClip(), seekClip(), and every
///   updateClip*/setClip* method post commands onto a lock-free
///   single-producer/single-consumer (SPSC) queue drained by the audio thread.
///   Exactly ONE control thread may call them — typically the host's UI/message
///   thread. They are NOT safe to call concurrently from multiple threads: the
///   SPSC producer side is unsynchronized by design. Hosts with multiple
///   control sources (UI + MIDI + OSC + network remote) must funnel them
///   through a single dispatcher thread before they reach this interface.
///   (A first-class SDK multi-producer dispatcher is future work — ORP135.)
///   Debug builds assert if commands are posted from more than one thread.
/// - **Queries are lock-free readers of a published snapshot.** getClipState(),
///   isClipPlaying(), getCurrentPosition(), getClipPosition(),
///   getActiveVoiceCount(), isClipLooping(), and the getClip* metadata queries
///   are safe from any thread, including concurrently with the control thread.
/// - **setCallback(): control thread only**, and not concurrently with
///   callback dispatch. Callbacks are invoked on the host's UI thread (from
///   processCallbacks()), never the audio thread.
///
/// Audio Thread Guarantees:
/// - No allocations in audio callback
/// - Lock-free command processing
/// - POD event queue for audio→UI notifications (no std::function on the
///   audio thread — ORP133 G1)
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

  /// Stop all clips in a specific routing group
  ///
  /// @deprecated ORP133 G2: NOT SUPPORTED — always returns
  /// SessionGraphError::NotSupported. This method was a silent no-op in every
  /// SDK release (the transport has no clip→group mapping; grouping is a host
  /// concern), and it now reports that truthfully instead. Hosts that need
  /// scoped group-stop should implement it with their own group model on top
  /// of stopOtherClips() (host-neutral choke, ORP127 G7) and/or stopClip().
  /// The method is retained only for source compatibility and may be removed
  /// in a future major version.
  ///
  /// @param groupIndex Ignored
  /// @return SessionGraphError::NotSupported, always
  virtual SessionGraphError stopAllInGroup(uint8_t groupIndex) = 0;

  /// Stop every playing clip EXCEPT the given one (ORP127 G7 choke primitive)
  ///
  /// Host-neutral choke: fades out all voices whose handle differs from
  /// exceptHandle, leaving the exempt clip untouched. Hosts implement scoped
  /// choke (e.g. host-defined exclusive groupings, "stop others on play") by
  /// calling this with the firing clip's handle — the SDK has no notion of any
  /// host-side grouping.
  ///
  /// @param exceptHandle The clip whose voices are spared (0 = stop all)
  /// @return SessionGraphError::OK on success, or error code on failure
  ///
  /// Thread-safe: Can be called from the UI thread.
  virtual SessionGraphError stopOtherClips(ClipHandle exceptHandle) = 0;

  /// Set the maximum simultaneous voices per clip (ORP127 G7 resource cap)
  ///
  /// Bounds how many voices a single clip may layer in Polyphonic mode (and the
  /// fade-overlap headroom in the mono modes). A resource-protection guard so a
  /// runaway re-fire cannot exhaust the voice pool.
  ///
  /// @param maxVoices Desired cap. Clamped to [1, 32]. Typical host values:
  ///        2 / 4 / 8 / 16. A cap of 2 is effectively "stop-all-on-play" (one
  ///        primary + at most one fade-overlap tail). SDK default is 8.
  /// @return SessionGraphError::OK (always succeeds after clamping)
  ///
  /// Thread-safe: Can be called from the UI thread. Takes effect on the next
  /// fire; voices already playing are unaffected.
  virtual SessionGraphError setMaxVoicesPerClip(uint32_t maxVoices) = 0;

  /// Query the current maximum voices per clip (ORP127 G7)
  /// @return The active per-clip voice cap.
  virtual uint32_t getMaxVoicesPerClip() const = 0;

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
  /// Beats are derived from the session tempo (SessionGraph::tempo). The
  /// tempo is sampled at construction and republished on each
  /// processCallbacks() pump, so a set_tempo() change is reflected in beats
  /// after the next pump (FTR027 §1).
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

  /// Register an audio file for a clip handle.
  ///
  /// Opens the file, reads metadata, applies session defaults, and stores the
  /// reader for future playback. This is a non-realtime preparation call and
  /// must not be invoked from the audio callback.
  ///
  /// @param handle Clip handle
  /// @param file_path Path to an audio file
  /// @return SessionGraphError::OK on success, or error code on failure
  virtual SessionGraphError registerClipAudio(ClipHandle handle, const std::string& file_path) = 0;

  /// Prewarm a registered clip reader outside the audio callback.
  ///
  /// Seeks the clip's reader to its trim-in point and performs a minimal read,
  /// then restores the trim-in position. Hosts should call this after
  /// registration/metadata changes and before latency-critical playback.
  ///
  /// @param handle Clip handle registered via registerClipAudio()
  /// @return SessionGraphError::OK on success, NotReady if the clip is active
  virtual SessionGraphError prepareClipAudio(ClipHandle handle) = 0;

  /// Update trim points for a registered clip
  ///
  /// @param handle Clip handle (must be registered via registerClipAudio)
  /// @param trimInSamples Trim IN point in samples (0 = start of file)
  /// @param trimOutSamples Trim OUT point in samples (file duration = end of file)
  /// @return SessionGraphError::OK on success, error code on failure
  ///
  /// Thread-safe: Can be called from UI thread
  /// Takes effect: On next clip start (does not affect currently playing clips)
  ///
  /// Validation:
  /// - trimInSamples must be >= 0 and < file duration
  /// - trimOutSamples must be > trimInSamples and <= file duration
  /// - If invalid, returns SessionGraphError::InvalidClipTrimPoints
  virtual SessionGraphError updateClipTrimPoints(ClipHandle handle, int64_t trimInSamples,
                                                 int64_t trimOutSamples) = 0;

  /// Update fade settings for a registered clip
  ///
  /// @param handle Clip handle (must be registered via registerClipAudio)
  /// @param fadeInSeconds Fade-in duration in seconds (0.0 = no fade)
  /// @param fadeOutSeconds Fade-out duration in seconds (0.0 = no fade)
  /// @param fadeInCurve Fade-in curve type (Linear, EqualPower, Exponential)
  /// @param fadeOutCurve Fade-out curve type (Linear, EqualPower, Exponential)
  /// @return SessionGraphError::OK on success, error code on failure
  ///
  /// Thread-safe: Can be called from UI thread
  /// Takes effect: On next clip start (does not affect currently playing clips)
  ///
  /// Fade behavior:
  /// - Fade-in: Applied from trimInSamples (0.0 → 1.0 gain over N seconds)
  /// - Fade-out: Applied before trimOutSamples (1.0 → 0.0 gain over N seconds)
  /// - Fade curves:
  ///   - Linear: y = x
  ///   - EqualPower: y = sin(x * π/2)  [smooth crossfades]
  ///   - Exponential: y = x²  [dramatic effect]
  ///
  /// Validation:
  /// - fadeInSeconds must be >= 0.0 and <= (trimOutSamples - trimInSamples) / sampleRate
  /// - fadeOutSeconds must be >= 0.0 and <= (trimOutSamples - trimInSamples) / sampleRate
  /// - If fades overlap, fade-out takes precedence
  virtual SessionGraphError updateClipFades(ClipHandle handle, double fadeInSeconds,
                                            double fadeOutSeconds, FadeCurve fadeInCurve,
                                            FadeCurve fadeOutCurve) = 0;

  /// Get current trim points for a clip (query only)
  ///
  /// @param handle Clip handle
  /// @param[out] trimInSamples Current trim IN point
  /// @param[out] trimOutSamples Current trim OUT point
  /// @return SessionGraphError::OK on success, error code if clip not found
  ///
  /// Thread-safe: Can be called from any thread
  virtual SessionGraphError getClipTrimPoints(ClipHandle handle, int64_t& trimInSamples,
                                              int64_t& trimOutSamples) const = 0;

  /// Update gain for a registered clip
  ///
  /// @param handle Clip handle (must be registered via registerClipAudio)
  /// @param gainDb Gain in decibels (-∞ to +12 dB typical, 0 dB = unity gain)
  /// @return SessionGraphError::OK on success, error code on failure
  ///
  /// Thread-safe: Can be called from UI thread
  /// Takes effect: Immediately for active clips, on next start for stopped clips
  ///
  /// Gain conversion:
  /// - Linear gain = 10^(gainDb / 20)
  /// - Examples: -6 dB = 0.5, 0 dB = 1.0, +6 dB = 2.0
  ///
  /// Validation:
  /// - gainDb must be finite (not NaN or Inf)
  /// - Typical range: -60 dB to +12 dB
  virtual SessionGraphError updateClipGain(ClipHandle handle, float gainDb) = 0;

  /// Set loop mode for a registered clip
  ///
  /// @param handle Clip handle (must be registered via registerClipAudio)
  /// @param shouldLoop true = loop indefinitely, false = play once
  /// @return SessionGraphError::OK on success, error code on failure
  ///
  /// Thread-safe: Can be called from UI thread
  /// Takes effect: On next clip start (does not affect currently playing clips)
  ///
  /// Loop behavior:
  /// - When clip reaches trim OUT point, it seeks back to trim IN point
  /// - Fade-out is NOT applied on loop (fades only apply at manual stop)
  /// - Useful for music beds, ambience, and looping effects
  virtual SessionGraphError setClipLoopMode(ClipHandle handle, bool shouldLoop) = 0;

  /// Get current playback position of a clip
  ///
  /// @param handle Clip handle
  /// @return Current position in samples (relative to file start), or -1 if clip not playing
  ///
  /// Thread-safe: Can be called from any thread.
  /// Performance: <100 CPU cycles (atomic read).
  ///
  /// Resolution: 75 fps "ticks" for broadcast workflows (1/75 second = ~13.33ms @ 48kHz = 640
  /// samples)
  ///
  /// @code
  /// int64_t position = transport->getClipPosition(handle);
  /// if (position >= 0) {
  ///   // Clip is playing, update UI playhead
  ///   int64_t totalFrames = (position * 75) / 48000;
  ///   int frames = totalFrames % 75;
  ///   std::cout << "Position: " << frames << " frames" << std::endl;
  /// }
  /// @endcode
  virtual int64_t getClipPosition(ClipHandle handle) const = 0;

  /// Set "Stop Others On Play" mode for a clip
  ///
  /// When enabled, starting this clip will trigger fade-out of all other playing clips.
  /// This is useful for "exclusive" clips that should play alone (e.g., voiceovers, alarms).
  ///
  /// @param handle Clip handle (must be registered via registerClipAudio)
  /// @param enabled true = stop others when this plays, false = normal behavior
  /// @return SessionGraphError::OK on success, error code on failure
  ///
  /// Thread-safe: Can be called from UI thread
  /// Takes effect: On next clip start
  ///
  /// Crossfade behavior:
  /// - Other clips fade out using their configured fade-out settings
  /// - This clip fades in using its configured fade-in settings
  /// - Creates smooth transitions for exclusive playback scenarios
  virtual SessionGraphError setClipStopOthersMode(ClipHandle handle, bool enabled) = 0;

  /// Query "Stop Others On Play" mode for a clip
  ///
  /// @param handle Clip handle
  /// @return true if enabled, false if disabled or clip not found
  ///
  /// Thread-safe: Can be called from any thread
  virtual bool getClipStopOthersMode(ClipHandle handle) const = 0;

  /// Update all clip metadata in a single operation
  ///
  /// This is more efficient than calling individual update methods when changing
  /// multiple parameters at once.
  ///
  /// @param handle Clip handle (must be registered via registerClipAudio)
  /// @param metadata Clip metadata to apply
  /// @return SessionGraphError::OK on success, error code on failure
  ///
  /// Thread-safe: Can be called from UI thread
  /// Takes effect: Immediately for active clips (where applicable), on next start for stopped clips
  ///
  /// Validation:
  /// - All validation rules from individual update methods apply
  /// - If any validation fails, NO changes are applied (atomic operation)
  virtual SessionGraphError updateClipMetadata(ClipHandle handle, const ClipMetadata& metadata) = 0;

  /// Get all clip metadata in a single query
  ///
  /// @param handle Clip handle
  /// @return Clip metadata if found, std::nullopt if clip not registered
  ///
  /// Thread-safe: Can be called from any thread
  virtual std::optional<ClipMetadata> getClipMetadata(ClipHandle handle) const = 0;

  /// Set session-level default metadata for new clips
  ///
  /// @param defaults Default metadata structure
  ///
  /// Thread-safe: Can be called from UI thread
  ///
  /// Effect: All future registerClipAudio() calls will use these defaults
  /// unless overridden by updateClipMetadata()
  virtual void setSessionDefaults(const SessionDefaults& defaults) = 0;

  /// Get current session-level default metadata
  ///
  /// @return Current defaults structure
  ///
  /// Thread-safe: Can be called from any thread
  virtual SessionDefaults getSessionDefaults() const = 0;

  /// Query if a clip is currently in loop mode and playing
  ///
  /// @param handle Clip handle
  /// @return true if clip is playing AND loop enabled, false otherwise
  ///
  /// Thread-safe: Can be called from any thread
  ///
  /// Use case: UI can query this to show loop indicator icon on clip buttons
  virtual bool isClipLooping(ClipHandle handle) const = 0;

  /// Set the voice allocation policy for a clip (ORP127 G5)
  ///
  /// Governs what happens when the clip is fired while one or more of its
  /// voices are already active. See VoiceMode for the three policies.
  ///
  /// @param handle Clip handle (must be registered via registerClipAudio)
  /// @param mode Voice policy (MonoWithFadeOverlap, Polyphonic, MonoStrict)
  /// @return SessionGraphError::OK on success, error code on failure
  ///
  /// Thread-safe: Can be called from the UI thread.
  /// Takes effect: on the next fire (startClip) for this clip. Any voices
  /// already playing keep the policy they were started under.
  ///
  /// Host-neutral: broadcast/soundboard hosts typically select
  /// MonoWithFadeOverlap; per-track multitrack hosts typically use MonoStrict.
  /// The SDK default is Polyphonic for backward compatibility.
  virtual SessionGraphError setClipVoiceMode(ClipHandle handle, VoiceMode mode) = 0;

  /// Query the voice allocation policy for a clip (ORP127 G5)
  ///
  /// @param handle Clip handle
  /// @return The clip's VoiceMode, or VoiceMode::Polyphonic if not registered
  ///
  /// Thread-safe: Can be called from any thread.
  virtual VoiceMode getClipVoiceMode(ClipHandle handle) const = 0;

  /// Count the currently active voices for a clip (ORP127 G5)
  ///
  /// Includes voices that are still fading out. Useful for UI voice-count
  /// indicators and for hosts implementing their own voice policies.
  ///
  /// @param handle Clip handle
  /// @return Number of active voice instances (0 if none)
  ///
  /// Thread-safe: Can be called from any thread (reads a published snapshot).
  virtual size_t getActiveVoiceCount(ClipHandle handle) const = 0;

  /// Restart clip playback from current IN point (seamless, no gap)
  ///
  /// Unlike startClip(), this method ALWAYS restarts playback even if already playing.
  /// The restart is sample-accurate and gap-free, using audio thread-level position reset.
  ///
  /// @param handle Clip handle
  /// @return SessionGraphError::OK if restart succeeded, error code otherwise
  ///
  /// @note Thread-safe: Can be called from UI thread
  /// @note Real-time safe: Restart happens in audio thread (no allocations, no blocking)
  /// @note Sample accuracy: Position reset is sample-accurate (±0 samples)
  /// @note Fade-in: Applies fade-in from clip metadata (if configured)
  ///
  /// @code
  /// // Example: a trim-IN nudge control in a clip editor.
  /// void onTrimInNudge() {
  ///   // Update trim IN point
  ///   transport->updateClipTrimPoints(handle, newTrimIn, trimOut);
  ///
  ///   // Restart preview from new IN point (seamless, no gap)
  ///   auto result = transport->restartClip(handle);
  ///   if (result != SessionGraphError::OK) {
  ///     showError("Failed to restart playback");
  ///   }
  /// }
  /// @endcode
  ///
  /// @see startClip(), stopClip(), getClipPosition()
  virtual SessionGraphError restartClip(ClipHandle handle) = 0;

  /// Seek clip to arbitrary position (sample-accurate, gap-free)
  ///
  /// Unlike restartClip(), this method allows seeking to any position within
  /// the audio file, not just the IN point. The position is clamped to [0, fileLength].
  ///
  /// @param handle Clip handle
  /// @param position Target position in samples (0-based file offset)
  /// @return SessionGraphError::OK on success, error code otherwise
  ///
  /// @note Thread-safe: Can be called from UI thread
  /// @note Real-time safe: Seek happens in audio thread (no allocations, no blocking)
  /// @note Sample accuracy: Position update is sample-accurate (±0 samples)
  /// @note Use cases: Waveform scrubbing, cue point navigation, timeline jumping
  ///
  /// @code
  /// // Waveform click-to-jog (SpotOn/Pyramix UX):
  /// void WaveformDisplay::mouseDown(const MouseEvent& e) {
  ///   int64_t clickPosition = pixelToSample(e.x);
  ///   if (m_previewPlayer->isPlaying()) {
  ///     m_audioEngine->seekClip(m_handle, clickPosition);  // Seamless seek
  ///   } else {
  ///     // Start from clicked position
  ///     m_metadata.trimInSamples = clickPosition;
  ///     m_audioEngine->updateClipMetadata(m_handle, m_metadata);
  ///     m_audioEngine->startClip(m_handle);
  ///     m_metadata.trimInSamples = originalIn;  // Restore
  ///   }
  /// }
  /// @endcode
  ///
  /// @see restartClip(), startClip(), getClipPosition()
  virtual SessionGraphError seekClip(ClipHandle handle, int64_t position) = 0;

  /// Add cue point to clip
  ///
  /// Cue points are markers within a clip for precise navigation (e.g., "Verse 1", "Chorus").
  /// Cue points are stored sorted by position for efficient seeking.
  ///
  /// @param handle Clip handle (must be registered via registerClipAudio)
  /// @param position Position in samples (0-based file offset)
  /// @param name User label (e.g., "Verse 1", "Intro", "Vocal Entry")
  /// @param color RGBA color for UI rendering (0xRRGGBBAA format)
  /// @return Index of added cue point (0-based), or -1 on error
  ///
  /// @note Thread-safe: Can be called from UI thread
  /// @note Cue points persist across stop/start cycles
  /// @note Position is validated against file duration (clamped to [0, fileDuration])
  /// @note Duplicate positions are allowed (multiple markers at same position)
  ///
  /// @code
  /// // Add cue point at 5 seconds with blue color
  /// int64_t position = 5 * sampleRate;
  /// int cueIndex = transport->addCuePoint(handle, position, "Verse 1", 0x0000FFFF);
  /// @endcode
  ///
  /// @see getCuePoints(), seekToCuePoint(), removeCuePoint()
  virtual int addCuePoint(ClipHandle handle, int64_t position, const std::string& name,
                          uint32_t color) = 0;

  /// Get all cue points for clip
  ///
  /// Returns cue points sorted by position (ascending order).
  ///
  /// @param handle Clip handle
  /// @return Vector of cue points (ordered by position), or empty vector if clip not found
  ///
  /// @note Thread-safe: Can be called from any thread
  /// @note Returns copy of cue points (not references)
  ///
  /// @code
  /// auto cuePoints = transport->getCuePoints(handle);
  /// for (const auto& cue : cuePoints) {
  ///   std::cout << cue.name << " at " << cue.position << " samples" << std::endl;
  /// }
  /// @endcode
  ///
  /// @see addCuePoint(), seekToCuePoint()
  virtual std::vector<CuePoint> getCuePoints(ClipHandle handle) const = 0;

  /// Seek to specific cue point
  ///
  /// Seeks clip to the position of the specified cue point (by index).
  /// Uses seekClip() internally for sample-accurate seeking.
  ///
  /// @param handle Clip handle
  /// @param cueIndex Index in cue points array (0-based)
  /// @return SessionGraphError::OK on success, error code otherwise
  ///
  /// @note Thread-safe: Can be called from UI thread
  /// @note Clip must be playing to seek (returns NotReady if stopped)
  /// @note Returns InvalidParameter if cueIndex is out of range
  ///
  /// @code
  /// // Seek to first cue point (e.g., keyboard shortcut Cmd+1)
  /// auto result = transport->seekToCuePoint(handle, 0);
  /// if (result != SessionGraphError::OK) {
  ///   showError("Cue point not found");
  /// }
  /// @endcode
  ///
  /// @see seekClip(), getCuePoints(), addCuePoint()
  virtual SessionGraphError seekToCuePoint(ClipHandle handle, uint32_t cueIndex) = 0;

  /// Remove cue point
  ///
  /// Removes cue point at specified index. Subsequent indices are shifted down.
  ///
  /// @param handle Clip handle
  /// @param cueIndex Index to remove (0-based)
  /// @return SessionGraphError::OK on success, error code otherwise
  ///
  /// @note Thread-safe: Can be called from UI thread
  /// @note Returns InvalidParameter if cueIndex is out of range
  /// @note Indices shift: removing cue 1 makes cue 2 become cue 1
  ///
  /// @code
  /// // Remove second cue point (index 1)
  /// auto result = transport->removeCuePoint(handle, 1);
  /// @endcode
  ///
  /// @see addCuePoint(), getCuePoints()
  virtual SessionGraphError removeCuePoint(ClipHandle handle, uint32_t cueIndex) = 0;
};

/// Create a transport controller instance
///
/// @param sessionGraph The session graph containing clip metadata
/// @param sampleRate Audio sample rate (e.g., 48000)
/// @return Unique pointer to transport controller
std::unique_ptr<ITransportController> createTransportController(core::SessionGraph* sessionGraph,
                                                                uint32_t sampleRate);

} // namespace orpheus
