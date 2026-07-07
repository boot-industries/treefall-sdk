// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/audio_file_reader.h>
#include <orpheus/routing_matrix.h>
#include <orpheus/transport_controller.h>

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace orpheus {

// Forward declarations
namespace core {
class SessionGraph;
} // namespace core

/// Playback context (thread-safe state transfer from UI to Audio thread)
/// Contains all immutable state required to start a clip
struct ClipPlaybackContext {
  ClipHandle handle;
  std::shared_ptr<IAudioFileReader> reader;

  // Metadata snapshot at start time
  int64_t trimInSamples;
  int64_t trimOutSamples;
  int64_t fileLengthSamples; // ORP127 G3: true file length, bounds OUT-tail reads
  double fadeInSeconds;
  double fadeOutSeconds;
  FadeCurve fadeInCurve;
  FadeCurve fadeOutCurve;
  float gainDb;
  float gainLinear;
  bool loopEnabled;
  uint16_t numChannels;
  VoiceMode voiceMode; // ORP127 G5: voice policy captured at fire time
};

/// Command for audio thread (lock-free queue)
struct TransportCommand {
  enum class Type : uint8_t {
    Start,
    Stop,
    StopAll,
    StopGroup,
    UpdateTrim,
    UpdateFade,
    UpdateGain,
    UpdateLoop,
    UpdateStopOthers,
    // ORP127 G1: UI-thread voice mutations routed onto the audio thread so no
    // ActiveClip field is written from two threads.
    Restart,        // Restart all voices for a handle from trim IN (with fade-in)
    Seek,           // Seek all voices for a handle to an absolute position
    UpdateMetadata, // Apply a full metadata batch to active voices
    SetVoiceMode    // ORP127 G5: change a clip's voice policy (audio-thread state)
  };

  Type type;
  ClipHandle handle;

  // Context for Start command (carries reader + metadata safely)
  // This is separate from the union to ensure proper shared_ptr management
  std::shared_ptr<ClipPlaybackContext> startContext;

  union {
    uint8_t groupIndex; // For StopGroup command

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
      bool loopEnabled;
      float gainDb;
      float gainLinear;
    } metadata;
  } data;
};

/// Active clip state (in audio thread)
struct ActiveClip {
  ClipHandle handle;
  uint32_t voiceId;      // Unique voice instance ID (for multi-voice layering)
  int64_t startSample;   // When clip started playing (transport time)
  int64_t currentSample; // Current position within clip audio

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

  // Gain control (atomic for thread safety)
  std::atomic<float> gainDb{0.0f};     // Gain in decibels (0.0 = unity)
  std::atomic<float> gainLinear{1.0f}; // Cached linear gain target (precomputed from gainDb)

  // ORP127 G4: per-voice gain smoothing state (audio-thread only). gainCurrent
  // ramps toward gainLinear by gainRampIncrement per sample so Set-Gain changes
  // (fader drags) apply as a short ramp instead of a step — no zipper noise.
  float gainCurrent{1.0f};       // Current smoothed linear gain
  float gainRampIncrement{0.0f}; // Linear gain change per sample (0 = snap)

  // Loop mode (atomic for thread safety)
  std::atomic<bool> loopEnabled{false}; // true = loop indefinitely

  float fadeOutGain;       // 1.0 = normal, 0.0 = fully faded (for stop fade-out)
  bool isStopping;         // true if fade-out in progress
  int64_t fadeOutStartPos; // Sample position when fade-out started (for additive time)

  // Restart crossfade state (broadcast-safe restart mechanism)
  bool isRestarting; // true if restart crossfade in progress
  int64_t
      restartFadeFramesRemaining; // Frames remaining in restart fade-in (5ms @ 48kHz = 240 frames)

  // ORP097 Bug 7 Fix: Track whether clip has looped to prevent fade-in/out at loop boundaries
  bool
      hasLoopedOnce; // true if clip has looped at least once (prevents re-applying start/end fades)

  uint16_t numChannels; // Number of channels in audio file

  // Thread-safe reader reference (captured from AudioFileEntry when clip starts)
  // Using shared_ptr provides reference-counted lifetime management:
  // - Audio thread holds reference until clip stops
  // - Reader can't be destroyed while audio thread is still using it
  // - Atomic refcount increment/decrement (lock-free, broadcast-safe)
  std::shared_ptr<IAudioFileReader> reader;

  // ORP127 G5: per-voice policy (copied from the clip's configured VoiceMode at
  // Start). Governs how a fresh fire interacts with this voice.
  VoiceMode voiceMode{VoiceMode::Polyphonic};
};

/// ORP127 G1: Immutable per-voice snapshot published by the audio thread for
/// lock-free UI-thread queries. Plain-old-data so it can be copied wholesale
/// without touching the live (audio-thread-owned) ActiveClip array.
struct VoiceSnapshot {
  ClipHandle handle;
  uint32_t voiceId;
  int64_t startSample;
  int64_t currentSample;
  int64_t trimInSamples;
  int64_t trimOutSamples;
  bool isStopping;
  bool loopEnabled;
};

/// Transport controller implementation
class TransportController : public ITransportController {
public:
  TransportController(core::SessionGraph* sessionGraph, uint32_t sampleRate);
  ~TransportController() override;

  // ITransportController interface
  SessionGraphError startClip(ClipHandle handle) override;
  SessionGraphError stopClip(ClipHandle handle) override;
  SessionGraphError stopAllClips() override;
  SessionGraphError stopAllInGroup(uint8_t groupIndex) override;
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
  void setSessionDefaults(const SessionDefaults& defaults) override;
  SessionDefaults getSessionDefaults() const override;
  bool isClipLooping(ClipHandle handle) const override;
  SessionGraphError setClipVoiceMode(ClipHandle handle, VoiceMode mode) override;
  VoiceMode getClipVoiceMode(ClipHandle handle) const override;
  size_t getActiveVoiceCount(ClipHandle handle) const override;
  SessionGraphError restartClip(ClipHandle handle) override;
  SessionGraphError seekClip(ClipHandle handle, int64_t position) override;

  // Cue point management
  int addCuePoint(ClipHandle handle, int64_t position, const std::string& name,
                  uint32_t color) override;
  std::vector<CuePoint> getCuePoints(ClipHandle handle) const override;
  SessionGraphError seekToCuePoint(ClipHandle handle, uint32_t cueIndex) override;
  SessionGraphError removeCuePoint(ClipHandle handle, uint32_t cueIndex) override;

  /// Process audio (called from audio thread)
  /// @param outputBuffers Output buffers (one per channel)
  /// @param numChannels Number of output channels
  /// @param numFrames Number of frames to process
  void processAudio(float** outputBuffers, size_t numChannels, size_t numFrames);

  /// Process callbacks on UI thread
  /// Must be called periodically from UI thread to dispatch transport events
  void processCallbacks();

  /// Register audio file for a clip (UI thread)
  /// @param handle Clip handle
  /// @param file_path Path to audio file
  /// @return Error code
  SessionGraphError registerClipAudio(ClipHandle handle, const std::string& file_path);

private:
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

  /// Add a clip to active list (audio thread only)
  /// @param context Playback context with immutable state
  void addActiveClip(const std::shared_ptr<ClipPlaybackContext>& context);

  /// ORP127 G5: Fire a voice honoring the context's VoiceMode (audio thread).
  /// - Polyphonic: always allocate a new voice (historical behavior).
  /// - MonoWithFadeOverlap: restart a live (non-stopping) voice in place; if
  ///   only fading tails exist, add a fresh voice alongside them.
  /// - MonoStrict: restart in place with no fade tail; if a voice is fading,
  ///   cut it and start fresh (single voice, sample-accurate replace).
  void startVoiceWithMode(const std::shared_ptr<ClipPlaybackContext>& context);

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

  /// Post callback to UI thread
  void postCallback(std::function<void()> callback);

  /// Calculate fade gain based on curve type
  /// @param normalizedPosition Position in fade (0.0 to 1.0)
  /// @param curve Fade curve type
  /// @return Gain value (0.0 to 1.0)
  float calculateFadeGain(float normalizedPosition, FadeCurve curve) const;

  /// ORP127 G1: Publish a snapshot of all active voices for UI-thread queries.
  /// Called from the audio thread at the end of processAudio(). Writes into the
  /// back buffer, then flips m_snapshotIndex with release ordering so UI readers
  /// (acquire) always see a fully-consistent set of voices — never a torn view.
  void publishVoiceSnapshot();

  /// ORP127 G1: Post a command to the audio thread. Returns OK, or InternalError
  /// if the SPSC command queue is full. Centralizes the write-index/full-check
  /// dance that every UI-thread mutation entry point previously duplicated.
  SessionGraphError postCommand(const TransportCommand& command);

  // Configuration
  core::SessionGraph* m_sessionGraph;
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

  // ORP127 G1: Double-buffered voice snapshot for lock-free UI-thread queries.
  // Audio thread writes the back buffer in publishVoiceSnapshot() and flips
  // m_snapshotIndex (release); UI-thread queries read the front buffer via
  // m_snapshotIndex (acquire). No UI thread ever touches m_activeClips.
  struct VoiceSnapshotBuffer {
    std::array<VoiceSnapshot, MAX_ACTIVE_CLIPS> voices;
    size_t count{0};
  };
  std::array<VoiceSnapshotBuffer, 2> m_snapshotBuffers;
  std::atomic<size_t> m_snapshotIndex{0}; // Index of the front (published) buffer

  // Multi-voice management
  static constexpr size_t MAX_VOICES_PER_CLIP = 4; // Provision for 4 voices (OCC uses 2)
  uint32_t m_nextVoiceId{1};                       // Incrementing voice ID counter (0 = invalid)

  // Transport position (audio thread writes, UI thread reads)
  std::atomic<int64_t> m_currentSample{0};

  // ORP121 C-03: Lock-free callback queue (Audio → UI thread)
  // SPSC ring buffer: Audio thread writes, UI thread reads - no contention
  // Power of 2 size for efficient modulo via bitwise AND
  static constexpr size_t CALLBACK_QUEUE_SIZE = 256;
  std::array<std::function<void()>, CALLBACK_QUEUE_SIZE> m_callbackRing;
  std::atomic<size_t> m_callbackWriteIndex{0};
  std::atomic<size_t> m_callbackReadIndex{0};

  // Diagnostic counter for dropped callbacks (queue overflow)
  std::atomic<uint32_t> m_droppedCallbackCount{0};

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
    float gainDb = 0.0f;           // Gain in decibels (0.0 = unity)
    bool loopEnabled = false;      // true = loop indefinitely
    bool stopOthersOnPlay = false; // true = stop all other clips when this one starts

    // ORP127 G5: voice allocation policy for this clip (default preserves the
    // SDK's historical polyphonic behavior).
    VoiceMode voiceMode = VoiceMode::Polyphonic;

    // Cue points (stored sorted by position)
    std::vector<CuePoint> cuePoints;
  };
  std::mutex m_audioFilesMutex;
  std::unordered_map<ClipHandle, AudioFileEntry> m_audioFiles;

  // Routing matrix for final mix (audio thread processes, UI thread configures)
  std::unique_ptr<IRoutingMatrix> m_routingMatrix;

  // Per-clip buffers (audio thread only, pre-allocated)
  static constexpr size_t MAX_BUFFER_FRAMES = 2048;
  static constexpr size_t MAX_FILE_CHANNELS = 8;

  // Each clip gets its own read buffer (for interleaved audio from file)
  std::vector<std::vector<float>>
      m_clipReadBuffers; // [MAX_ACTIVE_CLIPS][MAX_BUFFER_FRAMES * MAX_FILE_CHANNELS]

  // ORP121 A-01: Stereo clip buffers for routing (preserves source L/R)
  // Each clip has L and R buffers: [clip_index * 2 + 0] = L, [clip_index * 2 + 1] = R
  // Total channels = MAX_ACTIVE_CLIPS * 2 for stereo preservation
  std::vector<std::vector<float>> m_clipChannelBuffers; // [MAX_ACTIVE_CLIPS * 2][MAX_BUFFER_FRAMES]
  std::vector<float*> m_clipChannelPointers;            // Pointers for processRouting()

  // ORP121 A-01: ITU-R BS.775-3 downmix helpers for multi-channel sources
  float applyDownmixLeft(const float* src, size_t frame, size_t numCh) const;
  float applyDownmixRight(const float* src, size_t frame, size_t numCh) const;
};

} // namespace orpheus
