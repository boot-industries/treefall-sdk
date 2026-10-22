// SPDX-License-Identifier: MIT
#include "clip_source_registry.h"

#include "audio_io/resampling_audio_file_reader.h" // ORP127 G6: SRC decorator

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <optional>

namespace orpheus {

namespace {

std::optional<ChannelFormat> standardChannelFormat(ChannelLayout layout) {
  switch (layout) {
  case ChannelLayout::Mono:
    return ChannelFormat::Mono();
  case ChannelLayout::Stereo:
    return ChannelFormat::Stereo();
  case ChannelLayout::LCR:
    return ChannelFormat::LCR();
  case ChannelLayout::Quad:
    return ChannelFormat::Quad();
  case ChannelLayout::Surround_5_0:
    return ChannelFormat::Surround50();
  case ChannelLayout::Surround_5_1:
    return ChannelFormat::Surround51();
  case ChannelLayout::Surround_7_1:
    return ChannelFormat::Surround71();
  case ChannelLayout::SMPTE_51_ST:
    return ChannelFormat::SMPTE51Stereo();
  case ChannelLayout::SMPTE_51_LTRT:
    return ChannelFormat::SMPTE51MatrixStereo();
  default:
    return std::nullopt;
  }
}

std::optional<ChannelFormat> inferUnambiguousChannelFormat(uint16_t channels) {
  switch (channels) {
  case 1:
    return ChannelFormat::Mono();
  case 2:
    return ChannelFormat::Stereo();
  case 3:
    return ChannelFormat::LCR();
  case 4:
    return ChannelFormat::Quad();
  case 5:
    return ChannelFormat::Surround50();
  case 6:
    return ChannelFormat::Surround51();
  default:
    return std::nullopt;
  }
}

} // namespace

bool isValidSpeakerPatch(ChannelLayout layout, uint8_t patchSize,
                         const std::array<Speaker, 8>& patch, uint16_t fileChannels) {
  if (layout == ChannelLayout::Unspecified) {
    return patchSize == 0;
  }
  if (fileChannels == 0 || fileChannels > patch.size() || patchSize != fileChannels) {
    return false;
  }
  for (size_t channel = 0; channel < patchSize; ++channel) {
    if (patch[channel] == Speaker::None) {
      return false;
    }
    for (size_t earlier = 0; earlier < channel; ++earlier) {
      if (patch[channel] == patch[earlier]) {
        return false;
      }
    }
  }

  if (layout == ChannelLayout::Custom) {
    return true;
  }
  const auto standard = standardChannelFormat(layout);
  if (!standard.has_value() || standard->num_channels != fileChannels) {
    return false;
  }
  return std::equal(patch.begin(), patch.begin() + patchSize, standard->channel_map.begin());
}

ClipSourceRegistry::ClipSourceRegistry(uint32_t sampleRate, const TransportConfig& config,
                                       const std::atomic<uint16_t>* groupOutputWidths) noexcept
    : m_sampleRate(sampleRate), m_numGroups(config.numGroups),
      m_maxSourceChannels(config.maxSourceChannels), m_outputChannels(config.outputChannels),
      m_sourceChannelPolicy(config.sourceChannelPolicy), m_groupOutputWidths(groupOutputWidths) {}

void ClipSourceRegistry::setPreparedSourceMaxFrames(int64_t maxFrames) noexcept {
  preparedSourceMaxFrames_ = maxFrames;
}

int64_t ClipSourceRegistry::preparedSourceMaxFrames() const noexcept {
  return preparedSourceMaxFrames_;
}

void ClipSourceRegistry::setSessionDefaults(const SessionDefaults& defaults) {
  std::lock_guard<std::mutex> lock(mutex_);
  m_sessionDefaults = defaults;
}

SessionDefaults ClipSourceRegistry::getSessionDefaults() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return m_sessionDefaults;
}

void ClipSourceRegistry::shutdown() noexcept {
  // Stop and join the worker before any registered source is released: the
  // worker is the only other thread that touches those sources.
  streamWorker_.reset();
}

void ClipSourceRegistry::retainSourceCommand(SourceCommandLifetime* lifetime) noexcept {
  if (lifetime) {
    lifetime->unread.fetch_add(1, std::memory_order_relaxed);
  }
}

void ClipSourceRegistry::releaseSourceCommand(SourceCommandLifetime* lifetime) noexcept {
  if (lifetime) {
    const uint32_t previous = lifetime->unread.fetch_sub(1, std::memory_order_release);
    assert(previous > 0);
    static_cast<void>(previous);
  }
}

void ClipSourceRegistry::retainActiveSource(SourceCommandLifetime* lifetime) noexcept {
  if (lifetime) {
    lifetime->active_voices.fetch_add(1, std::memory_order_relaxed);
  }
}

void ClipSourceRegistry::releaseActiveSource(SourceCommandLifetime* lifetime) noexcept {
  if (lifetime) {
    const uint32_t previous = lifetime->active_voices.fetch_sub(1, std::memory_order_release);
    assert(previous > 0);
    static_cast<void>(previous);
  }
}

SessionGraphError ClipSourceRegistry::registerClipAudioImpl(ClipHandle handle,
                                                            const std::string& file_path,
                                                            const ClipMetadata* initialMetadata) {
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  if (file_path.empty()) {
    return SessionGraphError::InvalidParameter;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (entries_.find(handle) != entries_.end()) {
    // Registration is a clean-cutover operation: callers must stop, drain,
    // and unregister before supplying replacement media for a handle.
    return SessionGraphError::NotReady;
  }

  // Create audio file reader (convert unique_ptr to shared_ptr for thread-safe lifetime)
  auto uniqueReader = createAudioFileReader();

  // Check if audio file reader is available (may be nullptr if libsndfile not installed)
  if (!uniqueReader) {
    fprintf(stderr, "TransportController: createAudioFileReader() returned nullptr\n");
    return SessionGraphError::NotReady; // Audio file reading not available
  }

  auto result = uniqueReader->open(file_path);

  if (!result.isOk()) {
    fprintf(stderr, "TransportController: AudioFileReader::open('%s') failed with error %d\n",
            file_path.c_str(), (int)result.error);
    return result.error;
  }

  // Store reader and metadata for this clip.
  Entry entry;
  try {
    entry.commandLifetime = std::make_unique<SourceCommandLifetime>();
  } catch (const std::bad_alloc&) {
    return SessionGraphError::InternalError;
  }

  // ORP127 G6: if the file's sample rate differs from the engine rate, wrap the
  // reader in a deterministic polyphase resampler so it plays at correct pitch.
  // The decorator presents metadata/positions in the ENGINE-rate timeline, so
  // trims/fades/cues stay sample-accurate against the transport.
  if (result.value.sample_rate != m_sampleRate) {
    auto inner = std::shared_ptr<IAudioFileReader>(std::move(uniqueReader));
    auto resampling = std::make_shared<ResamplingAudioFileReader>(inner, m_sampleRate);
    // Re-open through the decorator to capture engine-rate metadata.
    auto rres = resampling->open(file_path);
    if (!rres.isOk()) {
      return rres.error;
    }
    entry.reader = resampling;
    entry.metadata = rres.value; // sample_rate == m_sampleRate, duration in engine frames
  } else {
    entry.reader = std::shared_ptr<IAudioFileReader>(std::move(uniqueReader));
    entry.metadata = result.value;
  }

  if (entry.metadata.num_channels == 0 || entry.metadata.num_channels > m_maxSourceChannels ||
      (m_sourceChannelPolicy == SourceChannelPolicy::Discrete &&
       entry.metadata.num_channels > m_outputChannels)) {
    return SessionGraphError::InvalidParameter;
  }

  if (const auto inferred = inferUnambiguousChannelFormat(entry.metadata.num_channels)) {
    entry.sourceLayout = inferred->layout;
    entry.speakerPatchSize = inferred->num_channels;
    std::copy_n(inferred->channel_map.begin(), inferred->num_channels, entry.speakerPatch.begin());
  }
  const double clipDurationSeconds =
      static_cast<double>(entry.metadata.duration_samples) / static_cast<double>(m_sampleRate);
  entry.stopFadeOutSeconds = std::min(m_sessionDefaults.stopFadeOutSeconds, clipDurationSeconds);
  entry.stopFadeOutCurve = m_sessionDefaults.stopFadeOutCurve;

  // Apply session defaults to new clip
  entry.muted = m_sessionDefaults.muted;
  entry.pan = m_sessionDefaults.pan;
  entry.playbackRate = m_sessionDefaults.playbackRate;
  entry.playDelaySeconds = m_sessionDefaults.playDelaySeconds;
  entry.fadeInSeconds = m_sessionDefaults.fadeInSeconds;
  entry.fadeOutSeconds = m_sessionDefaults.fadeOutSeconds;
  entry.fadeInCurve = m_sessionDefaults.fadeInCurve;
  entry.fadeOutCurve = m_sessionDefaults.fadeOutCurve;
  entry.loopEnabled = m_sessionDefaults.loopEnabled;
  entry.stopOthersOnPlay = m_sessionDefaults.stopOthersOnPlay;
  entry.gainDb = m_sessionDefaults.gainDb;

  // Trim points default to full file duration. Use entry.metadata (engine-rate
  // for resampled files) so trims are in the same timeline as the transport.
  entry.trimInSamples = 0;
  entry.trimOutSamples = entry.metadata.duration_samples;

  if (initialMetadata != nullptr) {
    int64_t trimOut = 0;
    const auto validation = validateClipMetadataFields(entry, *initialMetadata, trimOut);
    if (validation != SessionGraphError::OK)
      return validation;
    ClipDspProcessor preparedDsp;
    if (preparedDsp.prepare(initialMetadata->dsp, m_sampleRate, entry.metadata.num_channels) !=
        ClipDspValidationError::OK) {
      return SessionGraphError::InvalidParameter;
    }
    applyClipMetadata(entry, *initialMetadata, trimOut);
  }

  entries_.emplace(handle, std::move(entry));

  return SessionGraphError::OK;
}

SessionGraphError ClipSourceRegistry::validateClipMetadataFields(const Entry& entry,
                                                                 const ClipMetadata& metadata,
                                                                 int64_t& trimOut) const {
  const int64_t fileDurationSamples = entry.metadata.duration_samples;
  const uint16_t fileChannels = entry.metadata.num_channels;
  trimOut = metadata.trimOutSamples == 0 ? fileDurationSamples : metadata.trimOutSamples;
  if (metadata.trimInSamples < 0 || metadata.trimInSamples >= fileDurationSamples ||
      trimOut <= metadata.trimInSamples || trimOut > fileDurationSamples) {
    return SessionGraphError::InvalidClipTrimPoints;
  }
  if (metadata.segmentCount > kMaxClipPlaybackSegments)
    return SessionGraphError::InvalidParameter;
  for (uint32_t index = 0; index < metadata.segmentCount; ++index) {
    const auto& segment = metadata.segments[index];
    if (segment.startSample < metadata.trimInSamples || segment.endSample <= segment.startSample ||
        segment.endSample > trimOut || segment.repeatCount == 0 ||
        segment.repeatCount > kMaxClipSegmentRepeatCount) {
      return SessionGraphError::InvalidParameter;
    }
  }

  const int64_t clipDuration = trimOut - metadata.trimInSamples;
  const double clipDurationSeconds =
      static_cast<double>(clipDuration) / static_cast<double>(m_sampleRate);
  if (metadata.fadeInSeconds < 0.0 || metadata.fadeInSeconds > clipDurationSeconds ||
      metadata.fadeOutSeconds < 0.0 || metadata.fadeOutSeconds > clipDurationSeconds ||
      !std::isfinite(metadata.stopFadeOutSeconds) || metadata.stopFadeOutSeconds < 0.0 ||
      metadata.stopFadeOutSeconds > clipDurationSeconds) {
    return SessionGraphError::InvalidFadeDuration;
  }

  if (!std::isfinite(metadata.gainDb) || !std::isfinite(metadata.pan) || metadata.pan < -1.0f ||
      metadata.pan > 1.0f || !std::isfinite(metadata.playbackRate) ||
      metadata.playbackRate < 0.25 || metadata.playbackRate > 4.0 ||
      !std::isfinite(metadata.playDelaySeconds) || metadata.playDelaySeconds < 0.0 ||
      metadata.playDelaySeconds > 99.9) {
    return SessionGraphError::InvalidParameter;
  }
  if (metadata.routingGroup >= m_numGroups ||
      fileChannels > m_groupOutputWidths[metadata.routingGroup].load(std::memory_order_acquire)) {
    return SessionGraphError::InvalidParameter;
  }
  if (!isValidSpeakerPatch(metadata.sourceLayout, metadata.speakerPatchSize, metadata.speakerPatch,
                           fileChannels)) {
    return SessionGraphError::InvalidParameter;
  }
  return SessionGraphError::OK;
}

void ClipSourceRegistry::applyClipMetadata(Entry& entry, const ClipMetadata& metadata,
                                           int64_t trimOut) {
  entry.trimInSamples = metadata.trimInSamples;
  entry.trimOutSamples = trimOut;
  entry.fadeInSeconds = metadata.fadeInSeconds;
  entry.fadeOutSeconds = metadata.fadeOutSeconds;
  entry.fadeInCurve = metadata.fadeInCurve;
  entry.fadeOutCurve = metadata.fadeOutCurve;
  entry.stopFadeOutSeconds = metadata.stopFadeOutSeconds;
  entry.stopFadeOutCurve = metadata.stopFadeOutCurve;
  entry.loopEnabled = metadata.loopEnabled;
  entry.stopOthersOnPlay = metadata.stopOthersOnPlay;
  entry.voiceMode = metadata.voiceMode;
  entry.gainDb = metadata.gainDb;
  entry.muted = metadata.muted;
  entry.pan = metadata.pan;
  entry.playbackRate = metadata.playbackRate;
  entry.playDelaySeconds = metadata.playDelaySeconds;
  entry.routingGroup = metadata.routingGroup;
  entry.sourceLayout = metadata.sourceLayout;
  entry.speakerPatchSize = metadata.speakerPatchSize;
  entry.speakerPatch = metadata.speakerPatch;
  entry.segmentCount = metadata.segmentCount;
  entry.segments = metadata.segments;
  entry.dsp = metadata.dsp;
}

SessionGraphError
ClipSourceRegistry::ensurePreparedSourceLocked(Entry& entry,
                                               StreamingClipSource::PrimeReservation* reservation) {
  // ORP134 G1: build the realtime playback source on the CONTROL thread. All
  // decode/resample/file I/O happens here (or on the stream worker) - the
  // audio thread only ever memcpy-reads the published source.
  const auto prepareStreaming = [&](const std::shared_ptr<StreamingClipSource>& streaming) {
    StreamingClipSource::LoopAnchorTransition transition{};
    SessionGraphError result =
        streaming->prepareLoopAnchorTransition(entry.trimInSamples, entry.loopEnabled, transition);
    if (result != SessionGraphError::OK) {
      return result;
    }
    result = reservation != nullptr
                 ? streaming->primeForCommand(entry.trimInSamples, 1, *reservation)
                 : streaming->prefill(entry.trimInSamples);
    if (result != SessionGraphError::OK) {
      streaming->rollbackLoopAnchorTransition(transition);
      return result;
    }
    streaming->commitLoopAnchorTransition(transition);
    return SessionGraphError::OK;
  };

  if (entry.source) {
    if (auto streaming = std::dynamic_pointer_cast<StreamingClipSource>(entry.source)) {
      return prepareStreaming(streaming);
    }
    return SessionGraphError::OK;
  }

  const uint16_t numChannels = entry.metadata.num_channels;
  if (!entry.reader || !entry.reader->isOpen() || numChannels == 0 ||
      numChannels > m_maxSourceChannels) {
    return SessionGraphError::NotReady;
  }

  const int64_t lengthFrames = entry.metadata.duration_samples;
  if (lengthFrames <= preparedSourceMaxFrames_) {
    // Short clip: decode the whole file (engine-rate; entry.reader is already
    // wrapped in the ORP127 G6 resampler when rates differ).
    auto prepared = PreparedClipSource::decode(*entry.reader, numChannels, lengthFrames);
    if (!prepared) {
      return SessionGraphError::InternalError;
    }
    entry.source = prepared;
    return SessionGraphError::OK;
  }

  // Attach before publishing the source into the registry. If any preparation
  // step fails, only the worker's weak reference remains and the source is
  // destroyed without leaving a partially prepared registry entry.
  auto streaming = std::make_shared<StreamingClipSource>(entry.reader, numChannels, lengthFrames);
  if (!streamWorker_) {
    streamWorker_ = std::make_unique<MediaStreamWorker>();
  }
  streamWorker_->attach(streaming);
  streaming->setDemand(entry.trimInSamples);

  StreamingClipSource::LoopAnchorTransition transition{};
  SessionGraphError result =
      streaming->prepareLoopAnchorTransition(entry.trimInSamples, entry.loopEnabled, transition);
  if (result != SessionGraphError::OK) {
    return result;
  }
  result = reservation != nullptr ? streaming->primeForCommand(entry.trimInSamples, 1, *reservation)
                                  : streaming->prefill(entry.trimInSamples);
  if (result != SessionGraphError::OK) {
    streaming->rollbackLoopAnchorTransition(transition);
    return result;
  }
  streaming->commitLoopAnchorTransition(transition);
  entry.source = streaming;
  return SessionGraphError::OK;
}

} // namespace orpheus
