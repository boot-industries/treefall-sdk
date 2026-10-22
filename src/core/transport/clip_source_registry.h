// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/audio_file_reader.h>
#include <orpheus/clip_source.h>
#include <orpheus/routing_matrix.h>
#include <orpheus/transport_controller.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace orpheus {

/// Control-thread-owned token which pins a registered source while a Start or
/// Seek command still carries a raw source pointer through the bounded MPSC pool.
struct SourceCommandLifetime {
  std::atomic<uint32_t> unread{0};
  std::atomic<uint32_t> active_voices{0};
};

/// Control-side registry of the transport's registered clip sources.
///
/// Owns every registered clip's reader, prepared/streaming source, persistent
/// metadata and cue points, the mutex that serializes control-side registry
/// access, and the lazily created streaming worker that services long sources.
/// All of it is control-thread state: the audio thread only ever reaches a
/// source through the immutable view published with an admitted command.
class ClipSourceRegistry {
public:
  // Audio file registry (UI thread access, mutex protected)
  struct Entry {
    static constexpr size_t MAX_FILE_CHANNELS = 8;

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

  /// The registry needs the transport's immutable rate/channel policy and the
  /// live routing-group output widths to validate a registration or a metadata
  /// batch exactly like the controller did before the extraction. The width
  /// array must stay valid for the registry's lifetime.
  ClipSourceRegistry(uint32_t sampleRate, const TransportConfig& config,
                     const std::atomic<uint16_t>* groupOutputWidths) noexcept;

  ClipSourceRegistry(const ClipSourceRegistry&) = delete;
  ClipSourceRegistry& operator=(const ClipSourceRegistry&) = delete;

  /// The registry mutex. Every control-side registry access is serialized by
  /// it. The const accessor hands out the same lock to const queries.
  std::mutex& mutex() noexcept {
    return mutex_;
  }
  std::mutex& mutex() const noexcept {
    return mutex_;
  }

  /// The registered sources, keyed by clip handle.
  std::unordered_map<ClipHandle, Entry>& entries() noexcept {
    return entries_;
  }
  const std::unordered_map<ClipHandle, Entry>& entries() const noexcept {
    return entries_;
  }

  SessionGraphError registerClipAudioImpl(ClipHandle handle, const std::string& file_path,
                                          const ClipMetadata* initialMetadata);
  SessionGraphError validateClipMetadataFields(const Entry& entry, const ClipMetadata& metadata,
                                               int64_t& trimOut) const;
  static void applyClipMetadata(Entry& entry, const ClipMetadata& metadata, int64_t trimOut);
  SessionGraphError
  ensurePreparedSourceLocked(Entry& entry,
                             StreamingClipSource::PrimeReservation* reservation = nullptr);
  void retainSourceCommand(SourceCommandLifetime* lifetime) noexcept;
  void releaseSourceCommand(SourceCommandLifetime* lifetime) noexcept;
  void retainActiveSource(SourceCommandLifetime* lifetime) noexcept;
  void releaseActiveSource(SourceCommandLifetime* lifetime) noexcept;
  void setPreparedSourceMaxFrames(int64_t maxFrames) noexcept;
  int64_t preparedSourceMaxFrames() const noexcept;

  /// Session defaults applied to newly registered clips. Guarded by the
  /// registry mutex, exactly like the controller's session-default storage.
  void setSessionDefaults(const SessionDefaults& defaults);
  SessionDefaults getSessionDefaults() const;

  /// Stop and join the streaming worker before releasing any registered source.
  void shutdown() noexcept;

private:
  uint32_t m_sampleRate;
  uint32_t m_numGroups;
  uint32_t m_maxSourceChannels;
  uint32_t m_outputChannels;
  SourceChannelPolicy m_sourceChannelPolicy;
  // Routing-group output widths published by the transport. Read-only here:
  // a metadata batch is validated against the group's current width.
  const std::atomic<uint16_t>* m_groupOutputWidths{nullptr};
  SessionDefaults m_sessionDefaults;

  mutable std::mutex mutex_;
  std::unordered_map<ClipHandle, Entry> entries_;

  // ORP134 G1: streaming-source machinery. The worker thread is created
  // lazily when the first streaming source is prepared (short-clip-only hosts
  // never spawn it). kDefaultPreparedSourceMaxFrames ≈ 30s @ 48k: below
  // it, clips decode fully to memory (soundboard case, ~11.5 MB stereo max);
  // above it, they stream through a fixed page ring (long beds, songs).
  static constexpr int64_t kDefaultPreparedSourceMaxFrames = 48000ll * 30;
  int64_t preparedSourceMaxFrames_{kDefaultPreparedSourceMaxFrames};
  std::unique_ptr<MediaStreamWorker> streamWorker_; // guarded by mutex_
};

/// True when a speaker patch matches the clip's channel layout and the file's
/// channel count.
bool isValidSpeakerPatch(ChannelLayout layout, uint8_t patchSize,
                         const std::array<Speaker, 8>& patch, uint16_t fileChannels);

} // namespace orpheus
