// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/channel_format.h>
#include <orpheus/clip_dsp.h>
#include <orpheus/errors.h>
#include <orpheus/export.h>
#include <orpheus/realtime_telemetry.h>
#include <orpheus/routing_matrix.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace orpheus {

// Forward declarations
namespace core {
class SessionGraph;
} // namespace core

using ClipHandle = uint64_t;

using StartRequestTag = uint64_t;

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

/// Stable schema version for TransportCallbackTelemetry.
inline constexpr uint32_t kTransportCallbackTelemetrySchemaVersion = 1;

/// Stable schema version for ActiveVoiceSnapshot.
inline constexpr uint32_t kActiveVoiceSnapshotSchemaVersion = 2;

/// Audio-thread settlement for one nonzero host start-request identity.
enum class StartSettlementOutcome : uint8_t { Started = 0, ActiveVoiceLimitRejected = 1 };

/// Retained result of one tagged start at its command-processing position.
struct StartSettlementRecord {
  uint64_t sequence = 0;
  StartRequestTag requestTag = 0;
  ClipHandle handle = 0;
  uint32_t voiceId = 0;
  TransportPosition position{};
  StartSettlementOutcome outcome = StartSettlementOutcome::Started;
};

/// Stable schema and bounded retention for tagged start settlements.
inline constexpr uint32_t kStartSettlementSnapshotSchemaVersion = 1;
inline constexpr size_t kStartSettlementSnapshotCapacity = 64;

/// Coherent value snapshot ordered by ascending sequence.
///
/// Tag-zero starts are absent. overwrittenCount reports records evicted from
/// the 64-entry window. sequenceExhausted is terminal for this controller:
/// later tagged commands cannot mutate voices and publish no record or callback.
struct StartSettlementSnapshot {
  uint32_t schemaVersion = kStartSettlementSnapshotSchemaVersion;
  uint32_t entryCount = 0;
  uint8_t sequenceExhausted = 0;
  uint8_t reserved[7]{};
  uint64_t oldestSequence = 0;
  uint64_t latestSequence = 0;
  uint64_t overwrittenCount = 0;
  std::array<StartSettlementRecord, kStartSettlementSnapshotCapacity> entries{};
};

/// Maximum number of distinct clip handles represented in one active snapshot.
///
/// The transport admits at most 32 simultaneous voices, so the same fixed
/// capacity is sufficient even when every surviving voice has a different
/// ClipHandle.
inline constexpr size_t kActiveVoiceSnapshotCapacity = 32;

/// Cumulative delivery health for the audio-to-control callback ring.
///
/// Every attempted transport event receives the next sequence before the ring
/// capacity check. lastPostedSequence advances only when that event is retained;
/// lastDroppedSequence identifies the most recent rejected attempt. Therefore a
/// host can detect loss by polling even when no later callback is posted.
/// activeVoiceSnapshotSequence is the already-published reconciliation snapshot
/// watermark for this telemetry observation.
///
/// All counters start at zero when a controller is constructed, never reset when
/// processCallbacks() drains the ring, and remain valid until that controller is
/// destroyed. Counters saturate at UINT64_MAX rather than wrapping.
struct TransportCallbackTelemetry {
  uint32_t schemaVersion = kTransportCallbackTelemetrySchemaVersion;
  uint32_t reserved = 0;
  uint64_t lastAttemptedSequence = 0;
  uint64_t lastPostedSequence = 0;
  uint64_t cumulativeDroppedCount = 0;
  uint64_t lastDroppedSequence = 0;
  uint64_t activeVoiceSnapshotSequence = 0;
};

/// Per-ClipHandle aggregate in an ActiveVoiceSnapshot.
///
/// "Newest" is the surviving voice with the greatest transport start sample;
/// when starts share that sample, the later accepted start wins even when its
/// voice ID wrapped. newestVoiceId and newestStartRequestTag always describe
/// that same accepted start. state is Playing when any voice for the handle is
/// playing and Stopping only when all of them are stopping.
struct ActiveVoiceSnapshotEntry {
  ClipHandle handle = 0;
  uint32_t activeVoiceCount = 0;
  uint32_t newestVoiceId = 0;
  StartRequestTag newestStartRequestTag = 0;
  PlaybackState state = PlaybackState::Stopped;
  uint8_t newestVoiceStopping = 0;
  uint8_t newestVoiceLoopEnabled = 0;
  std::array<uint8_t, 5> reserved{};
  int64_t newestStartSample = 0;
  int64_t newestTrimInSamples = 0;
  int64_t newestTrimOutSamples = 0;
  TransportPosition newestPosition{};
};

/// Coherent fixed-capacity view of all currently surviving transport voices.
///
/// Entries [0, entryCount) are keyed by distinct ClipHandle values.
/// totalActiveVoiceCount includes playing voices and fading tails. The
/// publication sequence starts at zero and advances once after every completed
/// processAudio() block. A value copy is coherent: it never combines fields from
/// two audio blocks.
struct ActiveVoiceSnapshot {
  uint32_t schemaVersion = kActiveVoiceSnapshotSchemaVersion;
  uint32_t entryCount = 0;
  uint32_t totalActiveVoiceCount = 0;
  uint32_t reserved = 0;
  uint64_t publicationSequence = 0;
  std::array<ActiveVoiceSnapshotEntry, kActiveVoiceSnapshotCapacity> entries{};
};

static_assert(std::is_trivially_copyable_v<StartSettlementRecord>);
static_assert(std::is_standard_layout_v<StartSettlementRecord>);
static_assert(std::is_trivially_copyable_v<StartSettlementSnapshot>);
static_assert(std::is_standard_layout_v<StartSettlementSnapshot>);
static_assert(std::is_trivially_copyable_v<TransportCallbackTelemetry>);
static_assert(std::is_standard_layout_v<TransportCallbackTelemetry>);
static_assert(std::is_trivially_copyable_v<ActiveVoiceSnapshotEntry>);
static_assert(std::is_standard_layout_v<ActiveVoiceSnapshotEntry>);
static_assert(std::is_trivially_copyable_v<ActiveVoiceSnapshot>);
static_assert(std::is_standard_layout_v<ActiveVoiceSnapshot>);
/// Immutable construction contract for the transport renderer.
///
/// All capacities are validated by createTransportController() and remain fixed
/// for the controller lifetime. outputChannels is the exact number of writable
/// planar buffers required by processAudio(). maxSourceChannels is the largest
/// registered file width accepted by the renderer.
struct TransportConfig {
  uint32_t sampleRate = 48000;
  uint32_t outputChannels = 2;
  uint32_t maxBlockFrames = 2048;
  uint32_t maxActiveVoices = 32;
  uint32_t numGroups = 4;
  uint32_t maxSourceChannels = 8;
  SourceChannelPolicy sourceChannelPolicy = SourceChannelPolicy::Discrete;
};

struct OutputBusRoute {
  RoutingOutputIndex outputStart = 0;
  uint16_t channelCount = 2;

  bool operator==(const OutputBusRoute&) const = default;
};

inline constexpr uint32_t kMaxClipPlaybackSegments = 64;
inline constexpr uint32_t kMaxClipSegmentRepeatCount = 9999;

/// One source-range entry in a clip's bounded realtime segment program.
///
/// Segment programs are host-authored control metadata. Stable segment IDs and
/// source-identity reconciliation remain host/session concerns; the transport
/// consumes only validated sample windows and repeat counts.
struct ClipPlaybackSegment {
  int64_t startSample = 0;
  int64_t endSample = 0;
  uint32_t repeatCount = 1;

  bool operator==(const ClipPlaybackSegment&) const = default;
};

/// Clip metadata for batch updates.
///
/// `fadeOut*` defines the envelope at a natural trim-OUT boundary. The
/// `stopFadeOut*` fields independently define the envelope for an operator
/// stop: stopClip(), stopAllClips(), stopOtherClips(), or group choke.
struct ClipMetadata {
  int64_t trimInSamples = 0;                  ///< Trim IN point in samples (0 = start of file)
  int64_t trimOutSamples = 0;                 ///< Trim OUT point in samples (0 = use file duration)
  double fadeInSeconds = 0.0;                 ///< Fade-in duration in seconds
  double fadeOutSeconds = 0.0;                ///< Natural-END fade duration in seconds
  FadeCurve fadeInCurve = FadeCurve::Linear;  ///< Fade-in curve type
  FadeCurve fadeOutCurve = FadeCurve::Linear; ///< Natural-END fade curve type
  double stopFadeOutSeconds = 0.01;           ///< Operator-stop fade duration in seconds
  FadeCurve stopFadeOutCurve = FadeCurve::Linear; ///< Operator-stop fade curve type
  bool loopEnabled = false;                       ///< true = loop indefinitely
  bool stopOthersOnPlay = false;                  ///< true = stop other clips on play
  float gainDb = 0.0f;                            ///< Gain in decibels (0 = unity)
  bool muted = false;            ///< Silence the clip while preserving gainDb for later unmute
  float pan = 0.0f;              ///< Normalized mono pan / stereo balance [-1, +1]
  double playbackRate = 1.0;     ///< Forward varispeed multiplier [0.25, 4.0], pitch-coupled
  double playDelaySeconds = 0.0; ///< Trigger-to-audible delay [0, 99.9] seconds
  VoiceMode voiceMode = VoiceMode::Polyphonic;             ///< Voice allocation policy (ORP127 G5)
  RoutingGroupIndex routingGroup = 0;                      ///< Logical bus assignment
  ChannelLayout sourceLayout = ChannelLayout::Unspecified; ///< Explicit source channel meaning
  uint8_t speakerPatchSize = 0; ///< Number of valid entries in speakerPatch
  std::array<Speaker, 8> speakerPatch = {Speaker::None, Speaker::None, Speaker::None,
                                         Speaker::None, Speaker::None, Speaker::None,
                                         Speaker::None, Speaker::None};
  uint32_t segmentCount = 0; ///< Valid entries in segments; zero uses trim IN/OUT
  std::array<ClipPlaybackSegment, kMaxClipPlaybackSegments> segments{};
  ClipDspProgram dsp;
};
/// Session-level default metadata for new clips.
///
/// These values are copied when registerClipAudio() creates a registry entry;
/// later default changes do not mutate existing clips. The setter normalizes
/// pan, rate, and delay to their ClipMetadata bounds; an operator-stop default
/// longer than a new clip is clamped to that clip's duration.
struct SessionDefaults {
  double fadeInSeconds = 0.0;                     ///< Default fade-in time (0.0 = no fade)
  double fadeOutSeconds = 0.0;                    ///< Default natural-END fade time
  FadeCurve fadeInCurve = FadeCurve::Linear;      ///< Default fade-in curve
  FadeCurve fadeOutCurve = FadeCurve::Linear;     ///< Default natural-END fade curve
  double stopFadeOutSeconds = 0.01;               ///< Default operator-stop fade duration
  FadeCurve stopFadeOutCurve = FadeCurve::Linear; ///< Default operator-stop fade curve
  bool loopEnabled = false;                       ///< Default loop mode
  bool stopOthersOnPlay = false;                  ///< Default "stop others" mode
  float gainDb = 0.0f;                            ///< Default gain in dB (0.0 = unity)
  bool muted = false;                             ///< Default mute state
  float pan = 0.0f;                               ///< Default stereo balance
  double playbackRate = 1.0;                      ///< Default forward varispeed multiplier
  double playDelaySeconds = 0.0;                  ///< Default trigger-to-audible delay

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
  /// @param requestTag Host request identity, zero for an untagged start
  /// @param voiceId Nonzero SDK voice instance identity
  /// @param position Current transport position
  virtual void onClipStarted(ClipHandle handle, StartRequestTag requestTag, uint32_t voiceId,
                             TransportPosition position) = 0;

  /// Called when a clip stops playing
  /// @param handle The clip that stopped
  /// @param requestTag Identity copied from the exact retiring voice
  /// @param voiceId SDK identity of the retired voice
  /// @param position Current transport position
  virtual void onClipStopped(ClipHandle handle, StartRequestTag requestTag, uint32_t voiceId,
                             TransportPosition position) = 0;

  /// Called when a clip loops back to start
  /// @param handle The clip that looped
  /// @param requestTag Identity copied from the exact looping voice
  /// @param voiceId SDK identity of the looping voice
  /// @param position Current transport position
  virtual void onClipLooped(ClipHandle handle, StartRequestTag requestTag, uint32_t voiceId,
                            TransportPosition position) = 0;

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

  /// Called when the fixed global active-voice pool refuses a start.
  /// @param requestTag Identity of the refused start
  virtual void onActiveClipLimitReached(ClipHandle /*handle*/, StartRequestTag /*requestTag*/,
                                        TransportPosition /*position*/) {}
};

/// Transport controller interface for sample-accurate clip playback
///
/// This interface provides real-time control over clip playback with
/// sample-accurate timing and thread-safe operation.
///
/// Threading contract (ORP133 G3 — this is the real, enforced contract):
///
/// - **Control-mutating methods are single-producer.** startClip(),
///   startClipWithGroupChoke(), stopClip(), stopAllClips(), stopOtherClips(),
///   restartClip(), seekClip(), and every updateClip*/setClip* method post
///   commands onto a lock-free
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

  /// Start playback of a specific clip.
  ///
  /// The one control thread resolves registered-source preparation
  /// synchronously before publishing the next-render-boundary command. A
  /// registered streaming source whose trim-IN page cannot be prepared returns
  /// that error; it never falls back to the source-less historical default.
  /// The audio thread consumes only prepared PCM and remains allocation-, lock-,
  /// and I/O-free.
  ///
  /// @param handle The clip to start (must be a valid handle from SessionGraph)
  /// @param requestTag Host identity retained through settlement and lifecycle
  /// @return SessionGraphError::OK on success, or preparation/queue error
  ///
  /// @note If the clip is already playing, this function follows its configured
  ///       VoiceMode. Playback honors trim points and fade-in settings.
  virtual SessionGraphError startClip(ClipHandle handle, StartRequestTag requestTag = 0) = 0;

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

  /// Immediately silence all playback — hard cut, no fade (OCC155 Ask #5).
  ///
  /// Unlike stopAllClips(), which applies each voice's configured fade-out
  /// envelope, panic() drops every active voice at once and zeroes output on the
  /// next audio block. Intended for emergency "immediate mute" controls
  /// (feedback loop, wrong clip on air) where a fade tail — potentially seconds
  /// long — is unacceptable and the operator expects instant silence.
  ///
  /// @return SessionGraphError::OK on success, or error code on failure
  ///
  /// Thread-safe: callable from the control thread (posts onto the SPSC command
  /// queue like the other stop methods). RT-safe on the audio side: no
  /// allocations, no blocking — voices are evicted in place.
  virtual SessionGraphError panic() = 0;

  /// Stop all clips in a specific routing group
  ///
  /// @deprecated ORP133 G2: NOT SUPPORTED — always returns
  /// SessionGraphError::NotSupported. This legacy method was a silent no-op in
  /// earlier releases and remains unavailable for arbitrary group-only stops.
  /// Registered ClipMetadata::routingGroup is now consumed only by the atomic
  /// startClipWithGroupChoke() contract below. Hosts needing a standalone
  /// group stop must enqueue explicit stopClip() operations under their own
  /// policy. The method remains only for source compatibility.
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

  /// Access the session's routing matrix (group gains/mutes/solos/meters).
  ///
  /// The transport owns an IRoutingMatrix that mixes active clip voices into
  /// output groups. Hosts that expose group faders/meters (e.g. per-playgroup
  /// level UI) reach the matrix through this accessor rather than reimplementing
  /// grouping. The returned pointer is owned by the transport and stays valid
  /// for the controller's lifetime; do not delete it.
  ///
  /// @return The routing matrix, or nullptr if none is configured.
  ///
  /// Thread-safety: the pointer itself is stable; IRoutingMatrix defines its own
  /// per-method threading contract (UI-thread config, any-thread lock-free
  /// reads). See routing_matrix.h.
  virtual IRoutingMatrix* getRoutingMatrix() const = 0;

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
  /// must not be invoked from the audio callback. An existing registration is
  /// never replaced in place: it returns SessionGraphError::NotReady until the
  /// caller stops/drains, unregisters, and performs a clean re-registration.
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

  /// Release a clip previously registered via registerClipAudio() (OCC155 Ask #4).
  ///
  /// The inverse of registerClipAudio(): drops the reader and prepared/streaming
  /// source held for the handle. Active voices, unread registered-source Start
  /// or Seek commands, and pending streaming command-page primes return
  /// SessionGraphError::NotReady. The registry entry remains intact until those
  /// lifetimes have drained, so replacement registration cannot race a raw
  /// command source pointer. Calling on an unregistered handle is an idempotent
  /// SessionGraphError::OK.
  ///
  /// This is a non-realtime call and must not be invoked from the audio callback.
  ///
  /// @param handle Clip handle to release
  /// @return SessionGraphError::OK on success (or unregistered handle),
  ///         InvalidHandle if handle == 0, NotReady while ownership remains
  virtual SessionGraphError unregisterClipAudio(ClipHandle handle) = 0;

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
  /// Thread-safe: Can be called from UI thread.
  /// Takes effect: on command consumption for active clips and at the next
  /// start for stopped clips.
  ///
  /// Compatibility: this legacy convenience method sets both the natural-END
  /// fade-out and the operator-stop fade-out to its `fadeOut*` arguments. Use
  /// updateClipMetadata() when those envelopes must differ.
  /// Fade curve shape:
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
  /// - All validation rules from individual update methods apply.
  /// - stopFadeOutSeconds is finite, non-negative, and no longer than the
  ///   trimmed clip duration.
  /// - pan is finite and in [-1, +1]; playbackRate is finite and in
  ///   [0.25, 4.0]; playDelaySeconds is finite and in [0, 99.9].
  /// - If any validation fails, NO changes are applied (atomic operation).
  virtual SessionGraphError updateClipMetadata(ClipHandle handle, const ClipMetadata& metadata) = 0;

  /// Get all clip metadata in a single query
  ///
  /// @param handle Clip handle
  /// @return Clip metadata if found, std::nullopt if clip not registered
  ///
  /// Thread-safe: Can be called from any thread
  virtual std::optional<ClipMetadata> getClipMetadata(ClipHandle handle) const = 0;

  /// Route one logical clip group to a contiguous physical output bus.
  virtual SessionGraphError setGroupOutputBus(RoutingGroupIndex group,
                                              const OutputBusRoute& route) = 0;

  /// Query the current physical destination of a logical clip group.
  virtual std::optional<OutputBusRoute> getGroupOutputBus(RoutingGroupIndex group) const = 0;

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

  /// Total active voices, including fading tails, from the published snapshot.
  virtual size_t getTotalActiveVoiceCount() const = 0;

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

  /// Seek every active voice for a clip to an arbitrary file position.
  ///
  /// This is a one-control-thread operation. The position is clamped to
  /// [0, fileLength], and one FIFO command applies it to every active voice at
  /// the next valid render boundary. For a registered streaming source the
  /// control thread synchronously validates and pins the complete first-render
  /// working set before publishing that command; page copies, command
  /// application, callbacks, and genuine unexpected-miss underrun reporting
  /// remain bounded and real-time safe on the render thread.
  ///
  /// Failure is atomic: InternalError means the command ring was full; NotReady
  /// means no active voice or unavailable command-prime capacity; reader/cache
  /// preparation errors propagate. In every failure case no command, cursor
  /// change, ClipSeeked callback, or synthetic BufferUnderrun is produced.
  ///
  /// @param handle Clip handle
  /// @param position Target position in samples (0-based file offset)
  /// @return SessionGraphError::OK on acceptance, error code otherwise
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

  /// Access the fixed-capacity realtime telemetry bridge.
  ///
  /// The transport owns the returned object for its lifetime. Exactly one
  /// message-thread consumer may configure its decimation and drain snapshots
  /// with RealtimeTelemetry::tryRead(). The audio callback is the sole producer.
  /// Hosts must not retain the pointer after destroying the controller.
  virtual RealtimeTelemetry* getRealtimeTelemetry() noexcept = 0;

  /// Query the immutable audio-render contract.
  ///
  /// Control-thread query. The returned value is a copy; no SDK-owned storage
  /// escapes. The transport's capacities and routing topology are fixed after
  /// construction: hosts must not reinitialize the routing matrix while this
  /// controller is in use.
  virtual TransportConfig getRenderConfig() const noexcept = 0;

  /// Render one transport block into planar output buffers.
  ///
  /// This is the audio-thread entry point and the sole consumer of the
  /// control-to-audio SPSC command ring. It is non-reentrant and must be called
  /// by exactly one audio thread. The host must supply exactly
  /// getRenderConfig().outputChannels writable buffers and no more than
  /// getRenderConfig().maxBlockFrames frames per call.
  ///
  /// Real-time contract: no allocation, locks, blocking, I/O, or host callbacks.
  virtual void processAudio(float* const* outputBuffers, size_t numChannels,
                            size_t numFrames) noexcept = 0;

  /// Drain pending transport events on the control/message thread.
  ///
  /// This is the sole consumer of the audio-to-control SPSC event ring. Call
  /// from exactly one control thread, never concurrently with setCallback().
  /// The pump also republishes SessionGraph tempo changes for lock-free
  /// TransportPosition::beats queries, so hosts must call it even when no
  /// callback object is installed.
  virtual void processCallbacks() = 0;

  /// Atomically start a clip and choke active peers in its registered group.
  ///
  /// The firing clip's registered source is fully prepared before peer mutation
  /// or command publication. A preparation failure is returned directly; it
  /// never falls back to a source-less default. The firing clip and peer choke
  /// are then admitted as one bounded SPSC command. If the command ring is
  /// full, the clip is unregistered/unavailable, or the realtime voice pool
  /// later refuses the start, no peer voice is changed.
  ///
  /// After successful voice admission, every active voice with a different
  /// handle and the same ClipMetadata::routingGroup begins its normal configured
  /// stop fade. Other groups and the firing handle are untouched.
  ///
  /// Control thread only; this method shares the single-producer contract of
  /// startClip() and the other control-mutating methods. The default preserves
  /// source compatibility for external interface implementations; concrete
  /// controllers that do not override it report an unavailable capability.
  virtual SessionGraphError startClipWithGroupChoke(ClipHandle /*handle*/,
                                                    StartRequestTag /*requestTag*/ = 0) {
    return SessionGraphError::NotSupported;
  }

  /// Poll cumulative audio-to-control callback delivery health.
  ///
  /// Lock-free any-thread query. Draining callbacks does not reset counters.
  /// The default preserves source compatibility for recompiled custom
  /// implementations; it is not a C++ binary-compatibility guarantee.
  virtual TransportCallbackTelemetry getCallbackDeliveryTelemetry() const noexcept {
    return {};
  }

  /// Poll the retained outcomes for tagged start requests.
  ///
  /// Lock-free any-thread query. Tag-zero starts are deliberately absent.
  virtual StartSettlementSnapshot getStartSettlementSnapshot() const noexcept {
    return {};
  }

  /// Poll a coherent fixed-capacity aggregate of all active voices.
  ///
  /// Lock-free non-realtime query. The default preserves source compatibility
  /// for recompiled custom implementations; it is not a C++ ABI guarantee.
  virtual ActiveVoiceSnapshot getActiveVoiceSnapshot() const noexcept {
    return {};
  }
};

/// Create a transport controller with an immutable render contract.
///
/// @param sessionGraph The session graph containing clip metadata
/// @param config Validated renderer capacities and output shape
/// @return Controller, or nullptr when any capacity is unsupported
ORPHEUS_API std::unique_ptr<ITransportController>
createTransportController(core::SessionGraph* sessionGraph, const TransportConfig& config);

} // namespace orpheus
