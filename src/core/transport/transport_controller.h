// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/clip_source.h> // ORP134 G1: prepared/streaming realtime sources

#include <orpheus/audio_file_reader.h>
#include <orpheus/routing_matrix.h>
#include <orpheus/transport_controller.h>

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <unordered_map>

namespace orpheus {

// Forward declarations
namespace core {
class SessionGraph;
} // namespace core

/// Control-thread-owned token which pins a registered source while a Start,
/// Seek, UpdateTrim, or UpdateMetadata command still carries a raw source
/// pointer through the SPSC ring.
struct SourceCommandLifetime {
  std::atomic<uint32_t> unread{0};
  std::atomic<uint32_t> active_voices{0};
};

/// Playback context (thread-safe state transfer from UI to Audio thread)
/// Contains all immutable state required to start a clip
struct ClipPlaybackContext {
  ClipHandle handle;
  // ORP134 G1: the audio thread reads decoded PCM through an IClipSource
  // (prepared memory or streaming pages) — never through an IAudioFileReader.
  std::shared_ptr<IClipSource> source;
  // Pins the registry entry while the admitted voice remains in the audio set.
  SourceCommandLifetime* sourceLifetime{nullptr};

  // Metadata snapshot at start time
  int64_t trimInSamples;
  int64_t trimOutSamples;
  int64_t fileLengthSamples; // ORP127 G3: true file length, bounds OUT-tail reads
  double fadeInSeconds;
  double fadeOutSeconds;
  FadeCurve fadeInCurve;
  FadeCurve fadeOutCurve;
  double stopFadeOutSeconds;
  FadeCurve stopFadeOutCurve;
  float gainDb;
  float gainLinear;
  bool muted;
  float pan;
  float panLeftGain;
  float panRightGain;
  double playbackRate;
  int64_t playDelayFrames;
  bool loopEnabled;
  uint32_t segmentCount{0};
  std::array<ClipPlaybackSegment, kMaxClipPlaybackSegments> segments{};
  ClipDspProcessor dspProcessor;
  uint16_t numChannels;
  RoutingGroupIndex routingGroup{0};
  VoiceMode voiceMode; // ORP127 G5: voice policy captured at fire time
};

/// Command for audio thread (lock-free queue)
struct TransportCommand {
  enum class Type : uint8_t {
    Start,
    Stop,
    StopAll,
    Panic, // OCC155 Ask #5: hard-cut all voices, no fade

    // ORP133 G2: StopGroup removed — the transport has no clip→group mapping;
    // hosts implement scoped group-stop with stopOtherClips() + host grouping.
    UpdateTrim,
    UpdateFade,
    UpdateGain,
    UpdateLoop,
    UpdateStopOthers,
    // ORP127 G1: UI-thread voice mutations routed onto the audio thread so no
    // ActiveClip field is written from two threads.
    Restart,            // Restart all voices for a handle from trim IN (with fade-in)
    Seek,               // Seek all voices for a handle to an absolute position
    UpdateMetadata,     // Apply a full metadata batch to active voices
    SetVoiceMode,       // ORP127 G5: change a clip's voice policy (audio-thread state)
    StopOthers,         // ORP127 G7: stop every voice except cmd.handle (choke primitive)
    StartWithGroupChoke, // Atomically admit start, then fade registered same-group peers
    StartWithStopOthers // Atomically admit start, then fade every other voice
  };

  Type type;
  ClipHandle handle;

  // Context for Start command (carries reader + metadata safely)
  // This is separate from the union to ensure proper shared_ptr management
  std::shared_ptr<ClipPlaybackContext> startContext;
  ClipDspProcessor dspProcessor;

  // Raw source lifetimes are retained by the registry before publication and
  // released exactly once by the audio command consumer.
  SourceCommandLifetime* sourceLifetime{nullptr};
  StreamingClipSource* startSource{nullptr};
  StreamingClipSource::PrimeReservation startPrime{};
  StreamingClipSource* seekSource{nullptr};
  StreamingClipSource::PrimeReservation seekPrime{};
  // Metadata/trim commands may reposition a live voice before its first
  // render. A control-thread prime keeps that reposition failure-atomic and
  // pins the source page through the consuming render block.
  StreamingClipSource* metadataSource{nullptr};
  StreamingClipSource::PrimeReservation metadataPrime{};

  union {
    struct {
      int64_t in;
      int64_t out;
    } trim; // For UpdateTrim

    struct {
      double inSeconds;
      double outSeconds;
      FadeCurve inCurve;
      FadeCurve outCurve;
    } fade; // For UpdateFade

    float gainDb;      // For UpdateGain
    bool booleanValue; // For UpdateLoop / UpdateStopOthers

    int64_t seekPosition; // For Seek (absolute sample position, pre-clamped)

    uint8_t voiceMode; // For SetVoiceMode (VoiceMode enum value)

    // For UpdateMetadata: full metadata batch (fade sample counts precomputed
    // on the UI thread so no pow()/float-math surprises on the audio thread).
    struct {
      int64_t trimIn;
      int64_t trimOut;
      int64_t fadeInSamples;
      int64_t fadeOutSamples;
      double fadeInSeconds;
      double fadeOutSeconds;
      FadeCurve fadeInCurve;
      FadeCurve fadeOutCurve;
      int64_t stopFadeOutSamples;
      double stopFadeOutSeconds;
      FadeCurve stopFadeOutCurve;
      bool loopEnabled;
      float gainDb;
      float gainLinear;
      bool muted;
      float pan;
      float panLeftGain;
      float panRightGain;
      double playbackRate;
      int64_t playDelayFrames;
      RoutingGroupIndex routingGroup;
      uint32_t segmentCount;
      std::array<ClipPlaybackSegment, kMaxClipPlaybackSegments> segments;
    } metadata;
  } data;

  TransportCommand() noexcept : type(Type::Start), handle(0), data{} {}
};

/// Active clip state (in audio thread)
struct ActiveClip {
  ClipHandle handle;
  uint32_t voiceId;      // Unique voice instance ID (for multi-voice layering)
  uint64_t startOrdinal; // Chronological start generation (wrap-aware)
  int64_t startSample;   // When clip started playing (transport time)
  int64_t currentSample; // Current position within clip audio
  double sourcePosition; // Fractional source cursor for varispeed interpolation

  // Trim points (atomic for thread safety)
  std::atomic<int64_t> trimInSamples{0};  // Trim IN point (from metadata)
  std::atomic<int64_t> trimOutSamples{0}; // Trim OUT point (from metadata)

  // ORP127 G3: true file length in samples. Non-atomic (set once at start,
  // audio-thread-read only). Bounds how far past trimOut the OUT-boundary stop
  // fade may keep reading so the fade renders real audio instead of a hard cut.
  int64_t fileLengthSamples{0};

  // Fade settings (atomic for thread safety)
  std::atomic<double> fadeInSeconds{0.0};
  std::atomic<double> fadeOutSeconds{0.0};
  std::atomic<FadeCurve> fadeInCurve{FadeCurve::Linear};
  std::atomic<FadeCurve> fadeOutCurve{FadeCurve::Linear};

  // Cached fade sample counts (computed when fades are updated)
  std::atomic<int64_t> fadeInSamples{0};
  std::atomic<int64_t> fadeOutSamples{0};
  std::atomic<double> stopFadeOutSeconds{0.01};
  std::atomic<FadeCurve> stopFadeOutCurve{FadeCurve::Linear};
  std::atomic<int64_t> stopFadeOutSamples{0};
  int64_t stopFadeFramesElapsed{0};

  // Gain control (atomic for thread safety)
  std::atomic<float> gainDb{0.0f};     // Gain in decibels (0.0 = unity)
  std::atomic<float> gainLinear{1.0f}; // Cached linear gain target (precomputed from gainDb)
  std::atomic<bool> muted{false};
  std::atomic<float> pan{0.0f};
  std::atomic<float> panLeftGain{1.0f};
  std::atomic<float> panRightGain{1.0f};
  std::atomic<double> playbackRate{1.0};
  std::atomic<int64_t> playDelayFrames{0};
  int64_t playDelayFramesRemaining{0};

  // ORP127 G4: per-voice gain smoothing state (audio-thread only). gainCurrent
  // ramps toward gainLinear by gainRampIncrement per sample so Set-Gain changes
  // (fader drags) apply as a short ramp instead of a step — no zipper noise.
  float gainCurrent{1.0f};       // Current smoothed linear gain
  float gainRampIncrement{0.0f}; // Linear gain change per sample (0 = snap)

  // Loop mode (atomic for thread safety)
  std::atomic<bool> loopEnabled{false}; // true = loop indefinitely
  uint32_t segmentCount{0};
  uint32_t segmentIndex{0};
  uint32_t segmentRepeatsRemaining{0};
  std::array<ClipPlaybackSegment, kMaxClipPlaybackSegments> segments{};
  ClipDspProcessor dspProcessor;

  float fadeOutGain;       // 1.0 = normal, 0.0 = fully faded (for stop fade-out)
  bool isStopping;         // true if fade-out in progress
  int64_t fadeOutStartPos; // Sample position when fade-out started (for additive time)
  bool isNaturalEnding;    // true when OUT, false for operator/choke stop

  // Restart crossfade state (broadcast-safe restart mechanism)
  bool isRestarting; // true if restart crossfade in progress
  int64_t
      restartFadeFramesRemaining; // Frames remaining in restart fade-in (5ms @ 48kHz = 240 frames)

  // ORP097 Bug 7 Fix: Track whether clip has looped to prevent fade-in/out at loop boundaries
  bool
      hasLoopedOnce; // true if clip has looped at least once (prevents re-applying start/end fades)

  uint16_t numChannels; // Number of channels in audio file
  RoutingGroupIndex routingGroup{0};

  // ORP134 G1: thread-safe clip-source reference (captured from AudioFileEntry
  // when the clip starts). shared_ptr gives reference-counted lifetime:
  // - Audio thread holds a reference until the voice is removed
  // - The source can't be destroyed while the audio thread still reads it
  // - Atomic refcount increment/decrement (lock-free, broadcast-safe)
  // Reads are position-explicit (source->read(currentSample, ...)), so
  std::shared_ptr<IClipSource> source;
  // Pins the registry entry while this voice remains in the active set.
  SourceCommandLifetime* sourceLifetime{nullptr};

  // ORP127 G5: per-voice policy (copied from the clip's configured VoiceMode at
  // Start). Governs how a fresh fire interacts with this voice.
  VoiceMode voiceMode{VoiceMode::Polyphonic};
};

/// ORP133 G1: Transport event kinds posted from the audio thread.
///
/// The audio→UI callback ring used to carry std::function<void()> payloads,
/// which violates the callback rule in docs/REALTIME_AUDIT.md ("no
/// std::function ownership changes on realtime callbacks") — small-object
/// optimization may hide the allocation today, but it is not a portable
/// guarantee. The ring now carries these fixed POD events; processCallbacks()
/// translates them into the host's ITransportCallback virtuals on the UI
/// thread. Event emission points map 1:1 to the old postCallback sites, so
/// callback ordering and timing are unchanged.
enum class TransportEventType : uint8_t {
  ClipStarted = 0,
  ClipStopped,
  ClipLooped,
  ClipRestarted,
  ClipSeeked,
  BufferUnderrun,
  ActiveClipLimitReached
};

/// ORP133 G1: Trivially-copyable event payload for the audio→UI SPSC ring.
struct TransportEvent {
  TransportEventType type;
  uint64_t sequence;          ///< Monotonic attempted-delivery sequence
  ClipHandle handle;          ///< Subject clip (0 for transport-wide events)
  uint32_t voiceId;           ///< Voice instance when known, 0 otherwise (diagnostic)
  TransportPosition position; ///< Position payload delivered to the callback
};

static_assert(std::is_trivially_copyable_v<TransportEvent>,
              "TransportEvent must stay POD: it is copied through the audio->UI ring "
              "with no construction/destruction on the audio thread");

static_assert(std::atomic<uint8_t>::is_always_lock_free);
static_assert(std::atomic<uint32_t>::is_always_lock_free);
static_assert(std::atomic<uint64_t>::is_always_lock_free);
static_assert(std::atomic<int64_t>::is_always_lock_free);

/// Transport controller implementation
class TransportController : public ITransportController {
public:
  TransportController(core::SessionGraph* sessionGraph, const TransportConfig& config);
  ~TransportController() override;

  TransportConfig getRenderConfig() const noexcept override;
  void processAudio(float* const* outputBuffers, size_t numChannels,
                    size_t numFrames) noexcept override;
  void processCallbacks() override;

  // ITransportController interface
  SessionGraphError startClip(ClipHandle handle) override;
  SessionGraphError stopClip(ClipHandle handle) override;
  SessionGraphError stopAllClips() override;
  SessionGraphError panic() override; // OCC155 Ask #5: hard-cut, no fade
  SessionGraphError stopAllInGroup(uint8_t groupIndex) override;
  SessionGraphError stopOtherClips(ClipHandle exceptHandle) override;
  SessionGraphError startClipWithGroupChoke(ClipHandle handle) override;
  SessionGraphError setMaxVoicesPerClip(uint32_t maxVoices) override;
  uint32_t getMaxVoicesPerClip() const override;
  PlaybackState getClipState(ClipHandle handle) const override;
  bool isClipPlaying(ClipHandle handle) const override;
  TransportPosition getCurrentPosition() const override;
  void setCallback(ITransportCallback* callback) override;

  // Clip metadata management
  SessionGraphError updateClipTrimPoints(ClipHandle handle, int64_t trimInSamples,
                                         int64_t trimOutSamples) override;
  SessionGraphError updateClipFades(ClipHandle handle, double fadeInSeconds, double fadeOutSeconds,
                                    FadeCurve fadeInCurve, FadeCurve fadeOutCurve) override;
  SessionGraphError getClipTrimPoints(ClipHandle handle, int64_t& trimInSamples,
                                      int64_t& trimOutSamples) const override;
  SessionGraphError updateClipGain(ClipHandle handle, float gainDb) override;
  SessionGraphError setClipLoopMode(ClipHandle handle, bool shouldLoop) override;
  int64_t getClipPosition(ClipHandle handle) const override;
  SessionGraphError setClipStopOthersMode(ClipHandle handle, bool enabled) override;
  bool getClipStopOthersMode(ClipHandle handle) const override;
  SessionGraphError updateClipMetadata(ClipHandle handle, const ClipMetadata& metadata) override;
  std::optional<ClipMetadata> getClipMetadata(ClipHandle handle) const override;
  SessionGraphError setGroupOutputBus(RoutingGroupIndex group,
                                      const OutputBusRoute& route) override;
  std::optional<OutputBusRoute> getGroupOutputBus(RoutingGroupIndex group) const override;
  void setSessionDefaults(const SessionDefaults& defaults) override;
  SessionDefaults getSessionDefaults() const override;
  bool isClipLooping(ClipHandle handle) const override;
  SessionGraphError setClipVoiceMode(ClipHandle handle, VoiceMode mode) override;
  VoiceMode getClipVoiceMode(ClipHandle handle) const override;
  size_t getActiveVoiceCount(ClipHandle handle) const override;
  size_t getTotalActiveVoiceCount() const override;
  TransportCallbackTelemetry getCallbackDeliveryTelemetry() const noexcept override;
  ActiveVoiceSnapshot getActiveVoiceSnapshot() const noexcept override;
  SessionGraphError restartClip(ClipHandle handle) override;
  SessionGraphError seekClip(ClipHandle handle, int64_t position) override;

  // Cue point management
  int addCuePoint(ClipHandle handle, int64_t position, const std::string& name,
                  uint32_t color) override;
  std::vector<CuePoint> getCuePoints(ClipHandle handle) const override;
  SessionGraphError seekToCuePoint(ClipHandle handle, uint32_t cueIndex) override;
  SessionGraphError removeCuePoint(ClipHandle handle, uint32_t cueIndex) override;

  /// Register audio file for a clip (UI thread)
  /// @param handle Clip handle
  /// @param file_path Path to audio file
  /// @return Error code
  SessionGraphError registerClipAudio(ClipHandle handle, const std::string& file_path) override;

  /// ORP134 G1: Build the clip's realtime playback source outside the audio
  /// callback — whole-file PCM in memory for short clips, a worker-fed page
  /// ring for long ones. Hosts should call this after registration/metadata
  /// changes and before latency-critical playback; startClip() prepares
  /// lazily (on the control thread) when it wasn't called.
  SessionGraphError prepareClipAudio(ClipHandle handle) override;

  /// OCC155 Ask #4: release a registered clip's reader + prepared source.
  /// No-op (returns NotReady) while any voice for the handle is still active.
  SessionGraphError unregisterClipAudio(ClipHandle handle) override;

  /// OCC155 Ask #3: expose the transport's routing matrix through the public API
  /// so hosts no longer reach into this header with `#define private public`.
  IRoutingMatrix* getRoutingMatrix() const override {
    return m_routingMatrix.get();
  }

  RealtimeTelemetry* getRealtimeTelemetry() noexcept override {
    return &m_realtimeTelemetry;
  }

  /// ORP134 G1 test hook: clips whose engine-rate length exceeds this many
  /// frames stream from a page ring instead of being fully decoded to memory.
  /// Control thread only; affects sources prepared after the call.
  void setPreparedSourceMaxFrames(int64_t maxFrames) {
    m_preparedSourceMaxFrames = maxFrames;
  }

  /// Test-only control of the next RT voice-ID candidate.
  void setNextVoiceIdForTesting(uint32_t nextVoiceId) noexcept {
    m_nextVoiceId = nextVoiceId;
  }

  /// Test-only control of the next chronological start ordinal.
  void setNextVoiceStartOrdinalForTesting(uint64_t nextOrdinal) noexcept {
    m_nextVoiceStartOrdinal = nextOrdinal;
  }

  /// Test-only mutation while the audio callback is stopped.
  bool setVoiceSnapshotFieldsForTesting(uint32_t voiceId, bool stopping, bool looping,
                                        int64_t trimIn, int64_t trimOut,
                                        int64_t currentSample) noexcept;

private:
  /// Derive seconds/beats from the sample-canonical coordinate and current
  /// block-boundary tempo snapshot.
  TransportPosition positionAtSamples(int64_t samples) const;

  /// Resolve and prepare immutable start state on the control thread.
  ///
  /// Group-choke starts require a registered, available source so every
  /// pre-admission failure is reported before the atomic command is posted.
  SessionGraphError makeStartContext(
      ClipHandle handle, bool requireRegisteredSource,
      std::shared_ptr<ClipPlaybackContext>& context, SourceCommandLifetime*& sourceLifetime,
      StreamingClipSource*& startSource,
      StreamingClipSource::PrimeReservation& startPrime);

  /// Process pending commands from UI thread
  void processCommands();

  /// Find active clip by handle (returns first instance found)
  /// @return Pointer to active clip, or nullptr if not found
  /// @note For multi-voice: returns first matching instance, not necessarily oldest
  ActiveClip* findActiveClip(ClipHandle handle);

  /// Count active voices for a given clip handle
  /// @return Number of instances currently playing (0-MAX_VOICES_PER_CLIP)
  size_t countActiveVoices(ClipHandle handle) const;

  /// ORP127 G1: Count active voices for a handle from the published snapshot.
  /// Safe to call from the UI thread (does not touch the live voice array).
  size_t countActiveVoicesSnapshot(ClipHandle handle) const;

  /// Find oldest active voice for a given clip handle
  /// @return Pointer to oldest voice, or nullptr if none found
  ActiveClip* findOldestVoice(ClipHandle handle);

  /// Allocate the chronological start ordinal, rebasing the bounded live set
  /// before serial-number comparisons could become ambiguous.
  uint64_t allocateVoiceStartOrdinal() noexcept;

  /// Add a clip to the active list. Returns false when the global voice pool
  /// cannot accept the start.
  bool addActiveClip(const std::shared_ptr<ClipPlaybackContext>& context);

  /// Allocate a nonzero ID distinct from every active voice. Audio-thread only;
  /// bounded by the fixed active-voice capacity. ignoredVoiceId is reserved
  /// for same-handle replacement preflight.
  uint32_t allocateVoiceId(uint32_t ignoredVoiceId = 0) noexcept;
  void configureVoiceRouting(size_t voiceIndex, RoutingGroupIndex group,
                             uint16_t numChannels) noexcept;

  /// ORP127 G5: Fire a voice honoring the context's VoiceMode (audio thread).
  /// Returns the accepted nonzero voice identity, including for an in-place
  /// restart, or zero when the start is rejected.
  /// - Polyphonic: always allocate a new voice (historical behavior).
  /// - MonoWithFadeOverlap: restart a live (non-stopping) voice in place; if
  ///   only fading tails exist, add a fresh voice alongside them.
  /// - MonoStrict: restart in place with no fade tail; if a voice is fading,
  ///   cut it and start fresh (single voice, sample-accurate replace).
  uint32_t startVoiceWithMode(const std::shared_ptr<ClipPlaybackContext>& context);

  /// ORP127 G5: Reset an existing voice back to its trim IN for an in-place
  /// restart (used by the mono voice modes). Applies the broadcast-safe restart
  /// crossfade so the reset does not click. Audio thread only.
  void restartVoiceInPlace(ActiveClip& clip);

  /// Remove a specific voice instance from active list (audio thread only)
  /// @param voiceId Specific voice instance to remove
  void removeActiveVoice(uint32_t voiceId);

  /// Remove a clip from active list (audio thread only)
  /// @note Deprecated: Use removeActiveVoice() for multi-voice
  void removeActiveClip(ClipHandle handle);

  /// Post a POD transport event to the control-thread ring (audio thread only).
  /// Every attempt advances cumulative telemetry before the capacity check.
  void postTransportEvent(const TransportEvent& event) noexcept;

  /// Publish callback delivery counters through a coherent atomic seqlock.
  void publishCallbackTelemetry() noexcept;

  /// Calculate fade gain based on curve type
  /// @param normalizedPosition Position in fade (0.0 to 1.0)
  /// @param curve Fade curve type
  /// @return Gain value (0.0 to 1.0)
  float calculateFadeGain(float normalizedPosition, FadeCurve curve) const;

  /// Publish a coherent per-handle aggregate of all surviving voices.
  /// Audio-thread only; fixed work bounded by the configured 32-voice ceiling.
  void publishVoiceSnapshot() noexcept;

  /// Assert the documented single-control-thread producer contract.
  void assertCommandProducer() const noexcept;

  /// True when the SPSC ring has an available publisher slot.
  bool commandQueueHasCapacity() const noexcept;

  /// ORP127 G1: Post a command to the audio thread. Returns OK, or InternalError
  /// if the SPSC command queue is full. Centralizes the write-index/full-check
  /// dance that every UI-thread mutation entry point previously duplicated.
  SessionGraphError postCommand(const TransportCommand& command);

  // Configuration
  core::SessionGraph* m_sessionGraph;
  TransportConfig m_config;
  uint32_t m_sampleRate;
  ITransportCallback* m_callback; // User-provided callback

  // Session defaults (UI thread access, mutex protected)
  SessionDefaults m_sessionDefaults;

  // Lock-free command queue (UI → Audio thread)
  static constexpr size_t MAX_COMMANDS = 256;
  std::array<TransportCommand, MAX_COMMANDS> m_commands;
  std::atomic<size_t> m_commandWriteIndex{0};
  std::atomic<size_t> m_commandReadIndex{0};

  // Active clips (audio thread only, no locks needed)
  static constexpr size_t MAX_ACTIVE_CLIPS = 32;
  std::array<ActiveClip, MAX_ACTIVE_CLIPS> m_activeClips;
  size_t m_activeClipCount{0};

  struct PendingStartReservation {
    StreamingClipSource* source{nullptr};
    StreamingClipSource::PrimeReservation prime{};
  };
  std::array<PendingStartReservation, MAX_ACTIVE_CLIPS> m_pendingStartReservations{};
  size_t m_pendingStartReservationCount{0};

  struct PendingSeekReservation {
    ClipHandle handle{0};
    StreamingClipSource* source{nullptr};
    StreamingClipSource::PrimeReservation prime{};
  };
  std::array<PendingSeekReservation, MAX_ACTIVE_CLIPS> m_pendingSeekReservations{};
  size_t m_pendingSeekReservationCount{0};

  struct PendingMetadataReservation {
    StreamingClipSource* source{nullptr};
    StreamingClipSource::PrimeReservation prime{};
  };
  std::array<PendingMetadataReservation, MAX_COMMANDS> m_pendingMetadataReservations{};
  size_t m_pendingMetadataReservationCount{0};

  // Atomic seqlock publication. Every payload field is atomic, so a reader that
  // overlaps publication can retry without a C++ data race; the RT writer never
  // waits, locks, or allocates.
  struct AtomicActiveVoiceSnapshotEntry {
    std::atomic<ClipHandle> handle{0};
    std::atomic<uint32_t> activeVoiceCount{0};
    std::atomic<uint32_t> newestVoiceId{0};
    std::atomic<uint8_t> state{static_cast<uint8_t>(PlaybackState::Stopped)};
    std::atomic<uint8_t> newestVoiceStopping{0};
    std::atomic<uint8_t> newestVoiceLoopEnabled{0};
    std::atomic<int64_t> newestStartSample{0};
    std::atomic<int64_t> newestTrimInSamples{0};
    std::atomic<int64_t> newestTrimOutSamples{0};
    std::atomic<int64_t> newestPositionSamples{0};
    std::atomic<uint64_t> newestPositionSecondsBits{0};
    std::atomic<uint64_t> newestPositionBeatsBits{0};
  };
  struct AtomicActiveVoiceSnapshot {
    std::atomic<uint64_t> revision{0};
    std::atomic<uint64_t> publicationSequence{0};
    std::atomic<uint32_t> entryCount{0};
    std::atomic<uint32_t> totalActiveVoiceCount{0};
    std::array<AtomicActiveVoiceSnapshotEntry, kActiveVoiceSnapshotCapacity> entries{};
  };
  AtomicActiveVoiceSnapshot m_publishedVoiceSnapshot{};
  ActiveVoiceSnapshot m_voiceSnapshotScratch{};
  std::array<bool, kActiveVoiceSnapshotCapacity> m_voiceSnapshotHasNewest{};
  std::array<uint64_t, kActiveVoiceSnapshotCapacity> m_voiceSnapshotNewestStartOrdinal{};
  uint64_t m_voiceSnapshotRevision{0};
  uint64_t m_voiceSnapshotSequence{0};

  // Multi-voice management (ORP127 G7: configurable voice cap for resource
  // protection). Default 8, hard ceiling 32; hosts typically set 2/4/8/16.
  static constexpr uint32_t VOICE_CAP_HARD_MAX = 32;
  static constexpr uint32_t DEFAULT_MAX_VOICES_PER_CLIP = 8;
  std::atomic<uint32_t> m_maxVoicesPerClip{DEFAULT_MAX_VOICES_PER_CLIP};
  uint32_t m_nextVoiceId{1}; // Incrementing voice ID counter (0 = invalid)
  // Chronological serial-number arithmetic. Unsigned wrap is intentional.
  // The bounded live set is rebased before any pair can be separated by 2^63,
  // keeping comparisons unambiguous for the controller lifetime.
  uint64_t m_nextVoiceStartOrdinal{0};
  std::array<uint64_t, MAX_ACTIVE_CLIPS> m_voiceStartOrdinalScratch{};

  // Transport position (audio thread writes, UI thread reads)
  std::atomic<int64_t> m_currentSample{0};

  // FTR027 §1: session tempo cache. SessionGraph::tempo() is a plain double
  // mutated on the control thread, but getCurrentPosition() derives beats on
  // the audio thread (event position stamps) and on arbitrary query threads —
  // so the live value is published through this atomic instead of read from
  // the graph directly. Seeded in the constructor, refreshed on every
  // processCallbacks() pump (control thread).
  std::atomic<double> m_tempoBpm{120.0};

  // ORP121 C-03 / ORP133 G1: Lock-free event queue (Audio → UI thread)
  // SPSC ring buffer: Audio thread writes, UI thread reads - no contention
  // Power of 2 size for efficient modulo via bitwise AND. Payload is the POD
  // TransportEvent (no std::function on the audio thread).
  static constexpr size_t CALLBACK_QUEUE_SIZE = 256;
  std::array<TransportEvent, CALLBACK_QUEUE_SIZE> m_eventRing{};
  std::atomic<size_t> m_callbackWriteIndex{0};
  std::atomic<size_t> m_callbackReadIndex{0};

  // Lifetime-cumulative callback-delivery telemetry. The audio thread is the
  // only writer; arbitrary non-RT readers obtain a coherent value copy through
  // the atomic revision.
  struct AtomicCallbackTelemetry {
    std::atomic<uint64_t> revision{0};
    std::atomic<uint64_t> lastAttemptedSequence{0};
    std::atomic<uint64_t> lastPostedSequence{0};
    std::atomic<uint64_t> cumulativeDroppedCount{0};
    std::atomic<uint64_t> lastDroppedSequence{0};
    std::atomic<uint64_t> activeVoiceSnapshotSequence{0};
  };
  AtomicCallbackTelemetry m_publishedCallbackTelemetry{};
  uint64_t m_callbackTelemetryRevision{0};
  uint64_t m_callbackAttemptedSequence{0};
  uint64_t m_callbackPostedSequence{0};
  uint64_t m_callbackDroppedCount{0};
  uint64_t m_callbackLastDroppedSequence{0};

#ifndef NDEBUG
  // ORP133 G3: Debug-only enforcement of the command queue's single-producer
  // contract. The first thread to post a command is captured; any command
  // posted from a different thread afterwards trips an assert. Compiled out in
  // release builds (zero cost on the fast path).
  mutable std::atomic<std::thread::id> m_commandProducerThread{};
#endif

  // ORP127 G4: default clip-gain smoothing time. 5ms is a broadcast-console
  // norm (Yamaha CL/QL fader smoothing sits ~10ms); short enough to feel
  // immediate, long enough to kill zipper noise on drags.
  static constexpr float CLIP_GAIN_SMOOTHING_MS = 5.0f;
  float m_clipGainRampIncrement{0.0f}; // Per-sample linear ramp step (from the above)

  // Fade parameters
  static constexpr float FADE_OUT_DURATION_MS = 10.0f;
  static constexpr float RESTART_CROSSFADE_DURATION_MS =
      5.0f;                         // Broadcast-safe restart crossfade (5ms)
  size_t m_fadeOutSamples;          // Calculated from sample rate
  size_t m_restartCrossfadeSamples; // Calculated from sample rate

  static constexpr size_t MAX_FILE_CHANNELS = 8;

  // Audio file registry (UI thread access, mutex protected)
  struct AudioFileEntry {
    std::shared_ptr<orpheus::IAudioFileReader> reader;
    AudioFileMetadata metadata;

    // Persistent clip metadata (stored with audio file registration)
    int64_t trimInSamples = 0;
    int64_t trimOutSamples = 0; // 0 means use file duration
    double fadeInSeconds = 0.0;
    double fadeOutSeconds = 0.0;
    FadeCurve fadeInCurve = FadeCurve::Linear;
    FadeCurve fadeOutCurve = FadeCurve::Linear;
    double stopFadeOutSeconds = 0.01;
    FadeCurve stopFadeOutCurve = FadeCurve::Linear;
    float gainDb = 0.0f;           // Gain in decibels (0.0 = unity)
    bool loopEnabled = false;      // true = loop indefinitely
    bool stopOthersOnPlay = false; // true = stop all other clips when this one starts
    bool muted = false;
    float pan = 0.0f;
    double playbackRate = 1.0;
    double playDelaySeconds = 0.0;

    // ORP127 G5: voice allocation policy for this clip (default preserves the
    // SDK's historical polyphonic behavior).
    VoiceMode voiceMode = VoiceMode::Polyphonic;
    RoutingGroupIndex routingGroup = 0;
    ChannelLayout sourceLayout = ChannelLayout::Unspecified;
    uint8_t speakerPatchSize = 0;
    std::array<Speaker, MAX_FILE_CHANNELS> speakerPatch = {
        Speaker::None, Speaker::None, Speaker::None, Speaker::None,
        Speaker::None, Speaker::None, Speaker::None, Speaker::None};
    uint32_t segmentCount = 0;
    std::array<ClipPlaybackSegment, kMaxClipPlaybackSegments> segments{};
    ClipDspProgram dsp;

    // ORP134 G1: the realtime playback source built by prepareClipAudio()
    // (or lazily by startClip). The reader above remains the background
    // decode/metadata handle; the audio thread only ever touches `source`.
    std::shared_ptr<IClipSource> source;

    // Stable across unordered-map moves. The registry owns this token until
    // every queued registered-source Start/Seek command has been consumed.
    std::unique_ptr<SourceCommandLifetime> commandLifetime;

    // Cue points (stored sorted by position)
    std::vector<CuePoint> cuePoints;
  };
  std::mutex m_audioFilesMutex;
  std::unordered_map<ClipHandle, AudioFileEntry> m_audioFiles;

  /// ORP134 G1: Ensure entry.source exists (decode-to-memory or streaming
  /// ring). Control thread only; caller holds m_audioFilesMutex. A non-null
  /// reservation receives a command-owned pin for an existing source's
  /// first-render page.
  SessionGraphError
  ensurePreparedSourceLocked(AudioFileEntry& entry,
                             StreamingClipSource::PrimeReservation* reservation = nullptr);
  SessionGraphError prepareMetadataCommandPrimeLocked(AudioFileEntry& entry, int64_t position,
                                                      TransportCommand& command);
  void retainSourceCommand(SourceCommandLifetime* lifetime) noexcept;
  void releaseSourceCommand(SourceCommandLifetime* lifetime) noexcept;
  void retainActiveSource(SourceCommandLifetime* lifetime) noexcept;
  void releaseActiveSource(SourceCommandLifetime* lifetime) noexcept;
  void releasePendingStartReservations() noexcept;
  void releasePendingSeekReservations() noexcept;
  void releasePendingMetadataReservations() noexcept;

  // Routing matrix for final mix (audio thread processes, UI thread configures)
  std::unique_ptr<IRoutingMatrix> m_routingMatrix;
  static constexpr size_t MAX_LOGICAL_GROUPS = 32;
  std::array<std::atomic<RoutingOutputIndex>, MAX_LOGICAL_GROUPS> m_groupOutputStarts{};
  std::array<std::atomic<uint16_t>, MAX_LOGICAL_GROUPS> m_groupOutputWidths{};

  // Fixed-capacity audio-thread → message-thread telemetry bridge.
  RealtimeTelemetry m_realtimeTelemetry{};

  // ORP134 G1: streaming-source machinery. The worker thread is created
  // lazily when the first streaming source is prepared (short-clip-only hosts
  // never spawn it). DEFAULT_PREPARED_SOURCE_MAX_FRAMES ≈ 30s @ 48k: below
  // it, clips decode fully to memory (soundboard case, ~11.5 MB stereo max);
  // above it, they stream through a fixed page ring (long beds, songs).
  static constexpr int64_t DEFAULT_PREPARED_SOURCE_MAX_FRAMES = 48000ll * 30;
  int64_t m_preparedSourceMaxFrames{DEFAULT_PREPARED_SOURCE_MAX_FRAMES};
  std::unique_ptr<MediaStreamWorker> m_streamWorker; // guarded by m_audioFilesMutex

  // Compile-time ceilings; each instance allocates only its configured shape.
  static constexpr size_t MAX_BUFFER_FRAMES = 2048;

  // Per-voice interleaved decode buffers.
  std::vector<std::vector<float>> m_clipReadBuffers;

  // Preallocated planar routing lanes:
  // [voice slot * maxSourceChannels + source channel][frame].
  std::vector<std::vector<float>> m_clipChannelBuffers;
  std::vector<float*> m_clipChannelPointers;

  // StereoPairs compatibility downmix.
  float applyDownmixLeft(const float* src, size_t frame, size_t numCh) const;
  float applyDownmixRight(const float* src, size_t frame, size_t numCh) const;
};

} // namespace orpheus
