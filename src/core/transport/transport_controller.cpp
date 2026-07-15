// SPDX-License-Identifier: MIT
#include "transport_controller.h"

#include "audio_io/resampling_audio_file_reader.h" // ORP127 G6: SRC decorator
#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <orpheus/session_graph.h> // For SessionGraph

// MSVC and some platforms don't define M_PI_2 by default
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

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

uint64_t incrementSaturated(uint64_t value) noexcept {
  return value == std::numeric_limits<uint64_t>::max() ? value : value + 1;
}

bool isLaterStartOrdinal(uint64_t candidate, uint64_t reference) noexcept {
  const uint64_t delta = candidate - reference;
  return delta != 0 && delta < (uint64_t{1} << 63);
}

} // namespace

TransportController::TransportController(core::SessionGraph* sessionGraph,
                                         const TransportConfig& config)
    : m_sessionGraph(sessionGraph), m_config(config), m_sampleRate(config.sampleRate),
      m_callback(nullptr) {
  m_fadeOutSamples =
      static_cast<size_t>((FADE_OUT_DURATION_MS / 1000.0f) * static_cast<float>(m_sampleRate));
  m_restartCrossfadeSamples = static_cast<size_t>((RESTART_CROSSFADE_DURATION_MS / 1000.0f) *
                                                  static_cast<float>(m_sampleRate));

  const float smoothingSamples =
      (CLIP_GAIN_SMOOTHING_MS / 1000.0f) * static_cast<float>(m_sampleRate);
  m_clipGainRampIncrement = smoothingSamples > 0.0f ? 1.0f / smoothingSamples : 1.0f;

  m_routingMatrix = createRoutingMatrix();
  const size_t routingLaneCount =
      static_cast<size_t>(m_config.maxActiveVoices) * m_config.maxSourceChannels;

  RoutingConfig routingConfig;
  routingConfig.num_channels = static_cast<RoutingChannelIndex>(routingLaneCount);
  routingConfig.num_groups = static_cast<RoutingGroupIndex>(m_config.numGroups);
  routingConfig.num_outputs = static_cast<RoutingOutputIndex>(m_config.outputChannels);
  routingConfig.sample_rate = m_sampleRate;
  routingConfig.solo_mode = SoloMode::SIP;
  routingConfig.metering_mode = MeteringMode::Peak;
  routingConfig.gain_smoothing_ms = 0.0f;
  routingConfig.enable_metering = true;
  routingConfig.enable_clipping_protection = true;
  routingConfig.source_channel_policy = m_config.sourceChannelPolicy;
  routingConfig.downmix_policy = m_config.sourceChannelPolicy == SourceChannelPolicy::Discrete
                                     ? DownmixPolicy::None
                                     : DownmixPolicy::ITU_BS775_3;
  m_routingMatrix->initialize(routingConfig);

  for (size_t voice = 0; voice < m_config.maxActiveVoices; ++voice) {
    for (size_t lane = 0; lane < m_config.maxSourceChannels; ++lane) {
      const auto channel =
          static_cast<RoutingChannelIndex>(voice * m_config.maxSourceChannels + lane);
      ChannelConfig channelConfig;
      channelConfig.gain_db = 0.0f;
      channelConfig.pan = lane % 2 == 0 ? -1.0f : 1.0f;
      channelConfig.output_channel =
          static_cast<RoutingOutputIndex>(lane % m_config.outputChannels);
      m_routingMatrix->configureChannel(channel, channelConfig);
      m_routingMatrix->setChannelGroup(channel, UNASSIGNED_GROUP);
    }
  }
  for (size_t group = 0; group < MAX_LOGICAL_GROUPS; ++group) {
    m_groupOutputStarts[group].store(0, std::memory_order_relaxed);
    m_groupOutputWidths[group].store(0, std::memory_order_relaxed);
  }
  for (RoutingGroupIndex group = 0; group < m_config.numGroups; ++group) {
    m_groupOutputWidths[group].store(static_cast<uint16_t>(m_config.outputChannels),
                                     std::memory_order_relaxed);
    (void)m_routingMatrix->setGroupOutputRoute(group, 0,
                                               static_cast<uint16_t>(m_config.outputChannels));
  }

  m_clipReadBuffers.resize(m_config.maxActiveVoices);
  for (auto& buffer : m_clipReadBuffers) {
    buffer.resize(static_cast<size_t>(m_config.maxBlockFrames) * m_config.maxSourceChannels, 0.0f);
  }

  m_clipChannelBuffers.resize(routingLaneCount);
  for (auto& buffer : m_clipChannelBuffers) {
    buffer.resize(m_config.maxBlockFrames, 0.0f);
  }

  m_clipChannelPointers.resize(routingLaneCount);
  for (size_t lane = 0; lane < routingLaneCount; ++lane) {
    m_clipChannelPointers[lane] = m_clipChannelBuffers[lane].data();
  }

  if (m_sessionGraph) {
    m_tempoBpm.store(m_sessionGraph->tempo(), std::memory_order_relaxed);
  }
}

TransportController::~TransportController() = default;

SessionGraphError
TransportController::makeStartContext(ClipHandle handle, bool requireRegisteredSource,
                                      std::shared_ptr<ClipPlaybackContext>& context) {
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  try {
    context = std::make_shared<ClipPlaybackContext>();
    context->handle = handle;

    std::lock_guard<std::mutex> lock(m_audioFilesMutex);
    auto it = m_audioFiles.find(handle);
    if (it != m_audioFiles.end()) {
      auto& entry = it->second;
      if (entry.sourceLayout == ChannelLayout::Unspecified ||
          !isValidSpeakerPatch(entry.sourceLayout, entry.speakerPatchSize, entry.speakerPatch,
                               entry.metadata.num_channels)) {
        return SessionGraphError::InvalidParameter;
      }

      // Source construction is control-thread work. Group-choke starts must
      // reject unavailable media before posting the one atomic realtime command.
      const SessionGraphError prepareResult = ensurePreparedSourceLocked(entry);
      if (requireRegisteredSource && prepareResult != SessionGraphError::OK) {
        return prepareResult;
      }
      context->source = entry.source;
      context->numChannels = entry.metadata.num_channels;
      context->trimInSamples = entry.trimInSamples;
      context->fileLengthSamples = entry.metadata.duration_samples;
      context->trimOutSamples =
          entry.trimOutSamples == 0 ? entry.metadata.duration_samples : entry.trimOutSamples;
      context->fadeInSeconds = entry.fadeInSeconds;
      context->fadeOutSeconds = entry.fadeOutSeconds;
      context->fadeInCurve = entry.fadeInCurve;
      context->fadeOutCurve = entry.fadeOutCurve;
      context->gainDb = entry.gainDb;
      context->gainLinear = std::pow(10.0f, entry.gainDb / 20.0f);
      context->loopEnabled = entry.loopEnabled;
      context->routingGroup = entry.routingGroup;
      context->voiceMode = entry.voiceMode;
      return SessionGraphError::OK;
    }

    if (requireRegisteredSource) {
      context.reset();
      return SessionGraphError::ClipNotRegistered;
    }

    // Preserve the historical source-less test/default start contract.
    context->source = nullptr;
    context->numChannels = 2;
    context->trimInSamples = 0;
    context->trimOutSamples = 48000 * 60;
    context->fileLengthSamples = 48000 * 60;
    context->fadeInSeconds = 0.0;
    context->fadeOutSeconds = 0.0;
    context->fadeInCurve = FadeCurve::Linear;
    context->fadeOutCurve = FadeCurve::Linear;
    context->gainDb = 0.0f;
    context->gainLinear = 1.0f;
    context->loopEnabled = false;
    context->routingGroup = 0;
    context->voiceMode = VoiceMode::Polyphonic;
    return SessionGraphError::OK;
  } catch (const std::bad_alloc&) {
    context.reset();
    return SessionGraphError::InternalError;
  }
}

SessionGraphError TransportController::startClip(ClipHandle handle) {
  std::shared_ptr<ClipPlaybackContext> context;
  const SessionGraphError contextResult = makeStartContext(handle, false, context);
  if (contextResult != SessionGraphError::OK) {
    return contextResult;
  }

  bool stopOthers = false;
  {
    std::lock_guard<std::mutex> lock(m_audioFilesMutex);
    auto it = m_audioFiles.find(handle);
    if (it != m_audioFiles.end()) {
      stopOthers = it->second.stopOthersOnPlay;
    }
  }

  if (stopOthers) {
    stopOtherClips(handle);
  }

  TransportCommand cmd{};
  cmd.type = TransportCommand::Type::Start;
  cmd.handle = handle;
  cmd.startContext = context;
  return postCommand(cmd);
}

SessionGraphError TransportController::startClipWithGroupChoke(ClipHandle handle) {
  std::shared_ptr<ClipPlaybackContext> context;
  const SessionGraphError contextResult = makeStartContext(handle, true, context);
  if (contextResult != SessionGraphError::OK) {
    return contextResult;
  }

  TransportCommand cmd{};
  cmd.type = TransportCommand::Type::StartWithGroupChoke;
  cmd.handle = handle;
  cmd.startContext = context;
  return postCommand(cmd);
}

SessionGraphError TransportController::stopClip(ClipHandle handle) {
  // Validate handle
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  TransportCommand cmd{};
  cmd.type = TransportCommand::Type::Stop;
  cmd.handle = handle;
  return postCommand(cmd);
}

SessionGraphError TransportController::stopAllClips() {
  TransportCommand cmd{};
  cmd.type = TransportCommand::Type::StopAll;
  cmd.handle = 0;
  return postCommand(cmd);
}

SessionGraphError TransportController::panic() {
  // OCC155 Ask #5: immediate hard-cut. Unlike StopAll (which starts a fade-out
  // on every voice), Panic evicts all voices on the audio thread with no fade,
  // so output goes silent on the next block. Routed through the SPSC command
  // queue so it stays single-producer and RT-safe like the other stops.
  TransportCommand cmd{};
  cmd.type = TransportCommand::Type::Panic;
  cmd.handle = 0;
  return postCommand(cmd);
}

SessionGraphError TransportController::stopAllInGroup(uint8_t groupIndex) {
  // ORP133 G2: Deprecated — group-stop is a host concern, not a transport one.
  //
  // The transport has never had a clip→group mapping (the former StopGroup
  // command arm was a silent no-op blocked on "get clip group assignments from
  // SessionGraph"). Grouping lives with the host: Clip Composer models its own
  // playgroups and scopes chokes with stopOtherClips() (the ORP127 G7
  // host-neutral primitive), and the routing matrix's channel groups are a
  // mixing topology, not a playback-control one. Rather than keep a public
  // method that silently does nothing, this now reports the truth.
  (void)groupIndex;
  return SessionGraphError::NotSupported;
}

SessionGraphError TransportController::stopOtherClips(ClipHandle exceptHandle) {
  // ORP127 G7: host-neutral choke primitive. Stops every voice except those of
  // exceptHandle (pass 0 to stop everything). Routed through the command queue.
  TransportCommand cmd{};
  cmd.type = TransportCommand::Type::StopOthers;
  cmd.handle = exceptHandle;
  return postCommand(cmd);
}

SessionGraphError TransportController::setMaxVoicesPerClip(uint32_t maxVoices) {
  const uint32_t configuredLimit = std::min(m_config.maxActiveVoices, VOICE_CAP_HARD_MAX);
  const uint32_t clamped = std::clamp(maxVoices, 1u, configuredLimit);
  m_maxVoicesPerClip.store(clamped, std::memory_order_relaxed);
  return SessionGraphError::OK;
}

uint32_t TransportController::getMaxVoicesPerClip() const {
  return m_maxVoicesPerClip.load(std::memory_order_relaxed);
}

PlaybackState TransportController::getClipState(ClipHandle handle) const {
  const ActiveVoiceSnapshot snapshot = getActiveVoiceSnapshot();
  for (uint32_t index = 0; index < snapshot.entryCount; ++index) {
    if (snapshot.entries[index].handle == handle) {
      return snapshot.entries[index].state;
    }
  }
  return PlaybackState::Stopped;
}

bool TransportController::isClipPlaying(ClipHandle handle) const {
  PlaybackState state = getClipState(handle);
  return state == PlaybackState::Playing || state == PlaybackState::Stopping;
}

TransportPosition TransportController::positionAtSamples(int64_t samples) const {
  TransportPosition position{};
  position.samples = samples;
  position.seconds = static_cast<double>(samples) / static_cast<double>(m_sampleRate);
  const double tempo = m_tempoBpm.load(std::memory_order_relaxed);
  position.beats = position.seconds * tempo / 60.0;
  return position;
}

TransportPosition TransportController::getCurrentPosition() const {
  return positionAtSamples(m_currentSample.load(std::memory_order_relaxed));
}

void TransportController::setCallback(ITransportCallback* callback) {
  m_callback = callback;
}

TransportConfig TransportController::getRenderConfig() const noexcept {
  return m_config;
}

void TransportController::processAudio(float* const* outputBuffers, size_t numChannels,
                                       size_t numFrames) noexcept {
  if (outputBuffers == nullptr || numChannels != m_config.outputChannels ||
      numFrames > m_config.maxBlockFrames) {
    if (outputBuffers != nullptr) {
      for (size_t channel = 0; channel < numChannels; ++channel) {
        if (outputBuffers[channel] != nullptr) {
          std::memset(outputBuffers[channel], 0, numFrames * sizeof(float));
        }
      }
    }
    return;
  }

  processCommands();

  const bool publishTelemetry =
      m_realtimeTelemetry.beginRealtimeBlock(static_cast<uint32_t>(numFrames), m_sampleRate);

  for (auto& lane : m_clipChannelBuffers) {
    std::memset(lane.data(), 0, numFrames * sizeof(float));
  }

  // ORP127 G2: The stop fade-out is now computed per sample inside the render
  // loop (see below), so no per-buffer fadeOutGain pre-pass is needed. This
  // both removes the F-SDK-2 staircase and drops a redundant loop.

  // Render each active clip to its own channel buffer
  for (size_t i = 0; i < m_activeClipCount; ++i) {
    ActiveClip& clip = m_activeClips[i];

    // Skip if no audio source prepared (unregistered/test clips)
    if (!clip.source) {
      continue;
    }

    // Load trim and fade settings (atomic read for thread safety)
    int64_t trimIn = clip.trimInSamples.load(std::memory_order_acquire);
    int64_t trimOut = clip.trimOutSamples.load(std::memory_order_acquire);
    int64_t fadeInSampleCount = clip.fadeInSamples.load(std::memory_order_acquire);
    int64_t fadeOutSampleCount = clip.fadeOutSamples.load(std::memory_order_acquire);
    FadeCurve fadeInCurveType = clip.fadeInCurve.load(std::memory_order_acquire);
    FadeCurve fadeOutCurveType = clip.fadeOutCurve.load(std::memory_order_acquire);

    // ORP093: Enforce trim boundaries BEFORE rendering (prevents position escape bug)
    // CRITICAL: Clamp position to [trimIn, trimOut) range to maintain edit laws
    // This ensures getClipPosition() never returns values outside user-defined boundaries
    if (clip.currentSample < trimIn) {
      // Position below IN point - clamp to IN (enforce Edit Law #1).
      // ORP134 G1: sources are position-explicit — no reader seek; just hint
      // the streaming prefetcher.
      clip.currentSample = trimIn;
      clip.source->setDemand(trimIn);
    } else if (clip.currentSample >= trimOut) {
      // Position at or past OUT point - handle loop or stop
      const bool shouldLoop = clip.loopEnabled.load(std::memory_order_acquire) && !clip.isStopping;
      if (shouldLoop) {
        // Loop mode: restart from IN point
        clip.currentSample = trimIn;
        clip.source->setDemand(trimIn);

        // ORP097 Bug 7 Fix: Mark that clip has looped
        clip.hasLoopedOnce = true;
      } else {
        // Non-loop mode: trigger stop fade-out when reaching OUT point.
        // ORP127 G3: Keep the reader alive and let the per-sample stop fade
        // (T4) render REAL audio through the fade tail, instead of nulling the
        // reader and fading silence — which produced a click at OUT. Reading
        // continues past trimOut only for the fade duration, bounded by the
        // true file length (below), so libsndfile never wraps past EOF.
        if (!clip.isStopping) {
          clip.isStopping = true;
          clip.fadeOutGain = 1.0f;
          clip.fadeOutStartPos = clip.currentSample;
        }
      }
    } else if (!clip.loopEnabled.load(std::memory_order_acquire) && !clip.isStopping &&
               (clip.currentSample + static_cast<int64_t>(numFrames)) > trimOut) {
      // ORP127 G3: This buffer will CROSS trimOut mid-way. Start the stop fade
      // now, anchored exactly at trimOut, so the fade renders continuously from
      // the OUT sample within this same buffer — previously the read was capped
      // at trimOut and the rest of the buffer rendered as (cleared) silence,
      // producing a hard cut. The reader stays alive; the read horizon below is
      // extended to cover the fade tail.
      clip.isStopping = true;
      clip.fadeOutGain = 1.0f;
      clip.fadeOutStartPos = trimOut;
    }

    // ORP127 G3: When stopping (e.g. after OUT), extend the read horizon past
    // trimOut for the remaining fade tail so the fade applies to real audio.
    // Otherwise the read is bounded by trimOut as before. Everything is clamped
    // to the true file length so we never read past EOF.
    int64_t readHorizon = trimOut;
    if (clip.isStopping) {
      int64_t stopFadeSamples = fadeOutSampleCount;
      if (stopFadeSamples == 0) {
        stopFadeSamples = static_cast<int64_t>(m_fadeOutSamples);
      }
      int64_t fadeEnd = clip.fadeOutStartPos + stopFadeSamples;
      readHorizon = std::max(readHorizon, fadeEnd);
    }
    if (clip.fileLengthSamples > 0) {
      readHorizon = std::min(readHorizon, clip.fileLengthSamples);
    }

    // Calculate how many frames to read (respecting the read horizon)
    int64_t framesUntilEnd = readHorizon - clip.currentSample;
    if (framesUntilEnd <= 0) {
      // ORP127 G3: Past the read horizon (e.g. OUT tail reached EOF) but the
      // reader is still held. Advance position by the buffer so the stop fade
      // can still complete — the reader-less advance loop below only handles
      // clips whose reader is already null.
      if (clip.isStopping) {
        clip.currentSample += static_cast<int64_t>(numFrames);
      }
      continue;
    }

    size_t framesToRead =
        static_cast<size_t>(std::min(static_cast<int64_t>(numFrames), framesUntilEnd));

    // ORP134 G1: reads are position-explicit against the prepared/streamed
    // source — the audio thread never touches a file reader or a shared
    // cursor. (This also makes multi-voice playback of one clip correct:
    // every voice reads at its own currentSample.)
    size_t numFileChannels = clip.numChannels;

    // Use this clip's dedicated read buffer (no shared buffer conflicts!)
    float* clipReadBuffer = m_clipReadBuffers[i].data();
    size_t clipReadBufferSize = m_clipReadBuffers[i].size();

    // Check if read fits in pre-allocated buffer
    size_t samplesNeeded = framesToRead * numFileChannels;
    if (samplesNeeded > clipReadBufferSize) {
      // Clip too large for buffer - skip (should not happen with reasonable buffer size)
      continue;
    }

    // Copy decoded PCM from this clip's source into THIS clip's buffer.
    // A streaming cache miss NEVER blocks: the clip renders silence for this
    // buffer, the transport reports a BufferUnderrun, and the position still
    // advances so the timeline keeps moving while the worker catches up.
    size_t framesRead = 0;
    if (!clip.source->read(clip.currentSample, clipReadBuffer, framesToRead, framesRead)) {
      TransportEvent event{};
      event.type = TransportEventType::BufferUnderrun;
      event.handle = clip.handle;
      event.voiceId = clip.voiceId;
      event.position = getCurrentPosition();
      postTransportEvent(event);
      m_realtimeTelemetry.reportUnderrunFromRealtime();

      clip.source->setDemand(clip.currentSample);
      clip.currentSample += static_cast<int64_t>(framesToRead);
      continue;
    }

    if (framesRead == 0) {
      // Past EOF (e.g. OUT-tail horizon beyond the file). Advance so stop
      // fades can complete, mirroring the source-less advance path.
      if (clip.isStopping) {
        clip.currentSample += static_cast<int64_t>(numFrames);
      }
      continue;
    }

    // ORP121 A-01: Stereo output to L/R channel buffers (indices i*2 and i*2+1)
    // No mono sum - preserves source channel separation per ST2110-30

    // Load the clip gain TARGET (atomic read, no pow() in the audio thread).
    // ORP127 G4: gainCurrent ramps toward this target per sample below.
    float clipGainTarget = clip.gainLinear.load(std::memory_order_acquire);

    for (size_t frame = 0; frame < framesRead; ++frame) {
      // Calculate base gain (starts at 1.0)
      float gain = 1.0f;

      // ORP127 G4: ramp the smoothed clip gain toward the target by at most
      // gainRampIncrement per sample, so fader drags apply as a short ramp
      // instead of a per-buffer step (no zipper noise).
      if (clip.gainCurrent < clipGainTarget) {
        clip.gainCurrent = std::min(clipGainTarget, clip.gainCurrent + clip.gainRampIncrement);
      } else if (clip.gainCurrent > clipGainTarget) {
        clip.gainCurrent = std::max(clipGainTarget, clip.gainCurrent - clip.gainRampIncrement);
      }

      // Apply the smoothed clip gain (from gainDb setting)
      gain *= clip.gainCurrent;

      // Apply broadcast-safe restart crossfade (5ms linear fade-in)
      if (clip.isRestarting && clip.restartFadeFramesRemaining > 0) {
        // Calculate fade-in gain (0.0 → 1.0 over restartCrossfadeSamples)
        int64_t fadeProgress =
            static_cast<int64_t>(m_restartCrossfadeSamples) - clip.restartFadeFramesRemaining;
        float restartFadeGain =
            static_cast<float>(fadeProgress) / static_cast<float>(m_restartCrossfadeSamples);
        gain *= restartFadeGain; // Linear fade-in

        // Decrement remaining frames (will be disabled when reaches 0)
        clip.restartFadeFramesRemaining--;
        if (clip.restartFadeFramesRemaining == 0) {
          clip.isRestarting = false; // Crossfade complete
        }
      }

      // ORP127 G2: Apply the stop fade-out PER SAMPLE. Previously a single
      // fadeOutGain scalar was computed once per buffer (F-SDK-2), which turned
      // short fades (e.g. 10ms @ 512-sample buffers) into a 1-2 step staircase —
      // audible as bitcrush when many clips stop at once. Now each sample's fade
      // position is computed from its own offset into the fade, matching the
      // per-sample fade-in / clip-fade-out envelopes below.
      if (clip.isStopping) {
        int64_t stopFadeSamples = fadeOutSampleCount;
        if (stopFadeSamples == 0) {
          stopFadeSamples = static_cast<int64_t>(m_fadeOutSamples); // default 10ms
        }
        int64_t stopFadeProgress =
            (clip.currentSample + static_cast<int64_t>(frame)) - clip.fadeOutStartPos;
        float stopFadePos =
            static_cast<float>(stopFadeProgress) / static_cast<float>(stopFadeSamples);
        stopFadePos = std::clamp(stopFadePos, 0.0f, 1.0f);
        float stopFadeGain = 1.0f - calculateFadeGain(stopFadePos, fadeOutCurveType);
        gain *= std::max(0.0f, stopFadeGain);
      }

      // ORP097 Fix: Only apply clip fade-in/out for NON-LOOPED clips
      // Looped clips should have seamless loops with no fades at boundaries
      // Note: STOP fades (when clip.isStopping) are applied separately and work for all clips
      bool isLooped = clip.loopEnabled.load(std::memory_order_acquire);
      if (!isLooped) {
        // Apply clip fade-in (first N samples from trim IN)
        int64_t relativePos = clip.currentSample + static_cast<int64_t>(frame) - trimIn;
        if (fadeInSampleCount > 0 && relativePos >= 0 && relativePos < fadeInSampleCount) {
          float fadeInPos = static_cast<float>(relativePos) / static_cast<float>(fadeInSampleCount);
          gain *= calculateFadeGain(fadeInPos, fadeInCurveType);
        }

        // Apply clip fade-out (last N samples before trim OUT)
        int64_t trimmedDuration = trimOut - trimIn;
        if (fadeOutSampleCount > 0 && relativePos >= (trimmedDuration - fadeOutSampleCount)) {
          int64_t fadeOutRelativePos = relativePos - (trimmedDuration - fadeOutSampleCount);
          float fadeOutPos =
              static_cast<float>(fadeOutRelativePos) / static_cast<float>(fadeOutSampleCount);
          // ORP127 G3: clamp to [0,1]. With the OUT-tail read horizon, relativePos
          // can exceed trimmedDuration; without clamping calculateFadeGain(>1)
          // would drive the multiplier negative (phase flip).
          fadeOutPos = std::clamp(fadeOutPos, 0.0f, 1.0f);
          gain *= (1.0f - calculateFadeGain(fadeOutPos, fadeOutCurveType));
        }
      }

      const size_t laneBase = i * m_config.maxSourceChannels;
      if (m_config.sourceChannelPolicy == SourceChannelPolicy::Discrete) {
        for (size_t channel = 0; channel < numFileChannels; ++channel) {
          m_clipChannelBuffers[laneBase + channel][frame] =
              clipReadBuffer[frame * numFileChannels + channel] * gain;
        }
      } else {
        float left = 0.0f;
        float right = 0.0f;
        if (numFileChannels == 1) {
          left = clipReadBuffer[frame];
          right = left;
        } else if (numFileChannels == 2) {
          left = clipReadBuffer[frame * 2];
          right = clipReadBuffer[frame * 2 + 1];
        } else {
          left = applyDownmixLeft(clipReadBuffer, frame, numFileChannels);
          right = applyDownmixRight(clipReadBuffer, frame, numFileChannels);
        }
        m_clipChannelBuffers[laneBase][frame] = left * gain;
        if (m_config.maxSourceChannels > 1) {
          m_clipChannelBuffers[laneBase + 1][frame] = right * gain;
        }
      }
    }

    // Advance clip position by actual frames read (not buffer size!)
    // CRITICAL (Copilot feedback): This must happen AFTER fade processing, not before
    // Previously this was at line 341 (before fade loop), causing fade timing to be off by one
    // buffer
    clip.currentSample += static_cast<int64_t>(framesRead);
  }

  // Multi-voice fix: Advance position for clips WITHOUT sources (test clips)
  // This ensures fade-outs complete properly even when no audio is being rendered
  for (size_t i = 0; i < m_activeClipCount; ++i) {
    ActiveClip& clip = m_activeClips[i];
    if (!clip.source) {
      // Clip has no source - advance position by buffer size so fades can complete
      clip.currentSample += static_cast<int64_t>(numFrames);
    }
  }

  // Process routing matrix: clips → groups → master output
  m_routingMatrix->processRouting(const_cast<const float**>(m_clipChannelPointers.data()),
                                  outputBuffers, static_cast<uint32_t>(numFrames));

  // Update clips
  size_t i = 0;
  while (i < m_activeClipCount) {
    ActiveClip& clip = m_activeClips[i];

    // Check if fade-out is complete (fadeOutGain was pre-computed in pre-render loop)
    if (clip.isStopping) {
      int64_t fadeOutSampleCount = clip.fadeOutSamples.load(std::memory_order_acquire);

      // If no fade-out configured, use default 10ms fade
      if (fadeOutSampleCount == 0) {
        fadeOutSampleCount = static_cast<int64_t>(m_fadeOutSamples);
      }

      int64_t fadeProgress = clip.currentSample - clip.fadeOutStartPos;

      if (fadeProgress >= fadeOutSampleCount) {
        const ClipHandle stoppedHandle = clip.handle;
        const uint32_t stoppedVoiceId = clip.voiceId;
        removeActiveVoice(stoppedVoiceId);

        // Callbacks are handle-level state transitions. A fading tail ending
        // must not report the clip stopped while a newly fired voice for the
        // same handle is still live.
        if (countActiveVoices(stoppedHandle) == 0) {
          TransportEvent event{};
          event.type = TransportEventType::ClipStopped;
          event.handle = stoppedHandle;
          event.voiceId = stoppedVoiceId;
          event.position = getCurrentPosition();
          postTransportEvent(event);
        }
        continue;
      }
    }

    // Check if clip reached trim OUT point
    int64_t clipTrimOut = clip.trimOutSamples.load(std::memory_order_acquire);
    if (clip.currentSample >= clipTrimOut) {
      // Check if clip should loop
      const bool shouldLoop = clip.loopEnabled.load(std::memory_order_acquire) && !clip.isStopping;

      if (shouldLoop) {
        // Loop: jump back to trim IN point (works even without a source)
        int64_t trimIn = clip.trimInSamples.load(std::memory_order_acquire);
        if (clip.source) {
          clip.source->setDemand(trimIn);
        }
        clip.currentSample = trimIn;

        // ORP097 Bug 7 Fix: Mark that clip has looped (prevents fade-in/out on subsequent loops)
        clip.hasLoopedOnce = true;

        // Post loop event
        TransportEvent event{};
        event.type = TransportEventType::ClipLooped;
        event.handle = clip.handle;
        event.voiceId = clip.voiceId;
        event.position = getCurrentPosition();
        postTransportEvent(event);

        // Continue playback (don't remove clip, don't increment i)
        ++i;
      } else {
        // Non-loop mode: Clip reached OUT point.
        // ORP127 G3: Trigger the stop fade but KEEP the reader so the in-render
        // OUT-tail path (bounded by file length) can render real audio through
        // the fade — no hard cut, no click. The reader is released naturally
        // when the fade completes (removeActiveClip) or when the read horizon
        // reaches EOF (the reader-less advance loop then finishes the fade).
        if (!clip.isStopping) {
          clip.isStopping = true;
          clip.fadeOutGain = 1.0f;
          clip.fadeOutStartPos = clip.currentSample;
        }
        // Continue rendering with fade-out (normal fade-out completion logic will remove clip)
        ++i;
      }
    } else {
      ++i;
    }
  }

  // Update transport position
  int64_t newSample =
      m_currentSample.load(std::memory_order_relaxed) + static_cast<int64_t>(numFrames);
  m_currentSample.store(newSample, std::memory_order_relaxed);

  // ORP127 G1: Publish the post-render voice state for lock-free UI queries.
  publishVoiceSnapshot();
  // Callback loss becomes visible only after the voice state caused by every
  // attempted event in this block is published for reconciliation.
  publishCallbackTelemetry();

  if (publishTelemetry) {
    RealtimeTelemetrySnapshot snapshot;
    snapshot.position = TimePoint::fromSamples(newSample);
    snapshot.active_voice_count = static_cast<uint32_t>(m_activeClipCount);

    const RoutingConfig routingConfig = m_routingMatrix->getConfig();
    snapshot.group_count = static_cast<uint8_t>(
        std::min<size_t>(routingConfig.num_groups, kRealtimeTelemetryMaxGroups));
    for (uint8_t group = 0; group < snapshot.group_count; ++group) {
      snapshot.group_meters[group] = m_routingMatrix->getGroupMeter(group);
    }
    snapshot.master_meter = m_routingMatrix->getMasterMeter();

    (void)m_realtimeTelemetry.publishFromRealtime(snapshot);
  }
}

void TransportController::processCommands() {
  size_t readIndex = m_commandReadIndex.load(std::memory_order_relaxed);
  size_t writeIndex = m_commandWriteIndex.load(std::memory_order_acquire);

  while (readIndex != writeIndex) {
    const TransportCommand& cmd = m_commands[readIndex];

    switch (cmd.type) {
    case TransportCommand::Type::Start: {
      if (cmd.startContext && startVoiceWithMode(cmd.startContext)) {
        TransportEvent event{};
        event.type = TransportEventType::ClipStarted;
        event.handle = cmd.handle;
        event.position = getCurrentPosition();
        postTransportEvent(event);
      }
    } break;

    case TransportCommand::Type::StartWithGroupChoke: {
      // Admission is deliberately first. A voice-pool refusal may publish the
      // existing typed rejection event, but it cannot mutate any peer.
      if (cmd.startContext && startVoiceWithMode(cmd.startContext)) {
        TransportEvent event{};
        event.type = TransportEventType::ClipStarted;
        event.handle = cmd.handle;
        event.position = getCurrentPosition();
        postTransportEvent(event);

        // Only after the firing voice is live do same-group peers enter their
        // normal configured stop fades. The firing handle is always spared so
        // MonoWithFadeOverlap and other per-handle policies remain authoritative.
        const RoutingGroupIndex group = cmd.startContext->routingGroup;
        for (size_t i = 0; i < m_activeClipCount; ++i) {
          ActiveClip& peer = m_activeClips[i];
          if (peer.handle != cmd.handle && peer.routingGroup == group && !peer.isStopping) {
            peer.isStopping = true;
            peer.fadeOutGain = 1.0f;
            peer.fadeOutStartPos = peer.currentSample;
          }
        }
      }
    } break;

    case TransportCommand::Type::Stop: {
      // Multi-voice: Stop ALL voice instances for this handle
      for (size_t i = 0; i < m_activeClipCount; ++i) {
        if (m_activeClips[i].handle == cmd.handle && !m_activeClips[i].isStopping) {
          m_activeClips[i].isStopping = true;
          m_activeClips[i].fadeOutGain = 1.0f;
          m_activeClips[i].fadeOutStartPos =
              m_activeClips[i].currentSample; // Record position when fade-out started
        }
      }
    } break;

    case TransportCommand::Type::StopAll:
      for (size_t i = 0; i < m_activeClipCount; ++i) {
        m_activeClips[i].isStopping = true;
        m_activeClips[i].fadeOutGain = 1.0f;
        m_activeClips[i].fadeOutStartPos =
            m_activeClips[i].currentSample; // Record position when fade-out started
      }
      break;

    case TransportCommand::Type::Panic:
      // Emit one handle-level stop transition even if a handle currently has
      // both a fading tail and a freshly fired voice.
      for (size_t i = 0; i < m_activeClipCount; ++i) {
        bool firstVoiceForHandle = true;
        for (size_t earlier = 0; earlier < i; ++earlier) {
          if (m_activeClips[earlier].handle == m_activeClips[i].handle) {
            firstVoiceForHandle = false;
            break;
          }
        }
        if (firstVoiceForHandle) {
          TransportEvent event{};
          event.type = TransportEventType::ClipStopped;
          event.handle = m_activeClips[i].handle;
          event.voiceId = m_activeClips[i].voiceId;
          event.position = getCurrentPosition();
          postTransportEvent(event);
        }
        m_activeClips[i].source.reset();
      }
      m_activeClipCount = 0;
      break;

    case TransportCommand::Type::StopOthers:
      // ORP127 G7: choke primitive — stop every voice except cmd.handle.
      for (size_t i = 0; i < m_activeClipCount; ++i) {
        if (m_activeClips[i].handle != cmd.handle && !m_activeClips[i].isStopping) {
          m_activeClips[i].isStopping = true;
          m_activeClips[i].fadeOutGain = 1.0f;
          m_activeClips[i].fadeOutStartPos = m_activeClips[i].currentSample;
        }
      }
      break;

    case TransportCommand::Type::UpdateTrim:
      for (size_t i = 0; i < m_activeClipCount; ++i) {
        if (m_activeClips[i].handle == cmd.handle) {
          // Update trim points
          int64_t trimIn = cmd.data.trim.in;
          int64_t trimOut = cmd.data.trim.out;

          m_activeClips[i].trimInSamples.store(trimIn, std::memory_order_release);
          m_activeClips[i].trimOutSamples.store(trimOut, std::memory_order_release);

          // ORP114 Fix: Clamp position to new trim range immediately
          // This prevents fade calculation overflow in processAudio()
          if (m_activeClips[i].currentSample < trimIn) {
            m_activeClips[i].currentSample = trimIn;
            if (m_activeClips[i].source) {
              m_activeClips[i].source->setDemand(trimIn);
            }
          } else if (m_activeClips[i].currentSample >= trimOut) {
            m_activeClips[i].currentSample = trimOut;
            // Position clamps to OUT; playback stops naturally in processAudio.
          }
        }
      }
      break;

    case TransportCommand::Type::UpdateFade:
      for (size_t i = 0; i < m_activeClipCount; ++i) {
        if (m_activeClips[i].handle == cmd.handle) {
          m_activeClips[i].fadeInSeconds.store(cmd.data.fade.inSeconds, std::memory_order_release);
          m_activeClips[i].fadeOutSeconds.store(cmd.data.fade.outSeconds,
                                                std::memory_order_release);
          m_activeClips[i].fadeInCurve.store(cmd.data.fade.inCurve, std::memory_order_release);
          m_activeClips[i].fadeOutCurve.store(cmd.data.fade.outCurve, std::memory_order_release);

          // Recalculate sample counts (safe to do in audio thread)
          int64_t inSamples =
              static_cast<int64_t>(cmd.data.fade.inSeconds * static_cast<double>(m_sampleRate));
          int64_t outSamples =
              static_cast<int64_t>(cmd.data.fade.outSeconds * static_cast<double>(m_sampleRate));

          m_activeClips[i].fadeInSamples.store(inSamples, std::memory_order_release);
          m_activeClips[i].fadeOutSamples.store(outSamples, std::memory_order_release);
        }
      }
      break;

    case TransportCommand::Type::UpdateGain:
      for (size_t i = 0; i < m_activeClipCount; ++i) {
        if (m_activeClips[i].handle == cmd.handle) {
          float gainDb = cmd.data.gainDb;
          m_activeClips[i].gainDb.store(gainDb, std::memory_order_release);
          // Calculate linear gain here to avoid pow() in the inner audio loop
          // Note: std::pow is expensive but safe-ish in command processor (run once per update)
          // Ideally we'd use a fast approximation or lookup table, but this is infrequent.
          float linear = std::pow(10.0f, gainDb / 20.0f);
          m_activeClips[i].gainLinear.store(linear, std::memory_order_release);
        }
      }
      break;

    case TransportCommand::Type::UpdateLoop:
      for (size_t i = 0; i < m_activeClipCount; ++i) {
        if (m_activeClips[i].handle == cmd.handle) {
          m_activeClips[i].loopEnabled.store(cmd.data.booleanValue, std::memory_order_release);
        }
      }
      break;

    case TransportCommand::Type::Restart: {
      // ORP127 G1: Restart moved onto the audio thread (was a UI-thread mutation
      // of currentSample/reader/isStopping/hasLoopedOnce). Restart ALL voices
      // for the handle back to trim IN, applying the broadcast-safe crossfade.
      int64_t trimIn = 0;
      bool foundAnyVoice = false;
      for (size_t i = 0; i < m_activeClipCount; ++i) {
        if (m_activeClips[i].handle == cmd.handle) {
          foundAnyVoice = true;
          ActiveClip& clip = m_activeClips[i];

          trimIn = clip.trimInSamples.load(std::memory_order_relaxed);
          clip.currentSample = trimIn;
          if (clip.source) {
            clip.source->setDemand(trimIn);
          }

          // Cancel any fade-out in progress.
          clip.isStopping = false;
          clip.fadeOutGain = 1.0f;

          // Manual restart re-applies the clip fade-in (user action), unlike
          // auto-loop which suppresses it.
          clip.hasLoopedOnce = false;

          // Broadcast-safe restart crossfade (5ms) to avoid a click at reset.
          clip.isRestarting = true;
          clip.restartFadeFramesRemaining = static_cast<int64_t>(m_restartCrossfadeSamples);
        }
      }

      if (foundAnyVoice) {
        TransportEvent event{};
        event.type = TransportEventType::ClipRestarted;
        event.handle = cmd.handle;
        event.position = positionAtSamples(trimIn);
        postTransportEvent(event);
      }
    } break;

    case TransportCommand::Type::Seek: {
      // ORP127 G1: Seek moved onto the audio thread. Position was pre-clamped to
      // file bounds on the UI thread. Seek ALL voices for the handle.
      int64_t position = cmd.data.seekPosition;
      bool foundAnyVoice = false;
      for (size_t i = 0; i < m_activeClipCount; ++i) {
        if (m_activeClips[i].handle == cmd.handle) {
          foundAnyVoice = true;
          ActiveClip& clip = m_activeClips[i];
          clip.currentSample = position;
          if (clip.source) {
            clip.source->setDemand(position);
          }
        }
      }

      if (foundAnyVoice) {
        TransportEvent event{};
        event.type = TransportEventType::ClipSeeked;
        event.handle = cmd.handle;
        event.position = positionAtSamples(position);
        postTransportEvent(event);
      }
    } break;

    case TransportCommand::Type::UpdateMetadata:
      // ORP127 G1: Full metadata batch applied on the audio thread. Fade sample
      // counts and linear gain were precomputed on the UI thread.
      for (size_t i = 0; i < m_activeClipCount; ++i) {
        if (m_activeClips[i].handle == cmd.handle) {
          ActiveClip& clip = m_activeClips[i];
          clip.trimInSamples.store(cmd.data.metadata.trimIn, std::memory_order_release);
          clip.trimOutSamples.store(cmd.data.metadata.trimOut, std::memory_order_release);
          clip.fadeInSeconds.store(cmd.data.metadata.fadeInSeconds, std::memory_order_release);
          clip.fadeOutSeconds.store(cmd.data.metadata.fadeOutSeconds, std::memory_order_release);
          clip.fadeInCurve.store(cmd.data.metadata.fadeInCurve, std::memory_order_release);
          clip.fadeOutCurve.store(cmd.data.metadata.fadeOutCurve, std::memory_order_release);
          clip.fadeInSamples.store(cmd.data.metadata.fadeInSamples, std::memory_order_release);
          clip.fadeOutSamples.store(cmd.data.metadata.fadeOutSamples, std::memory_order_release);
          clip.loopEnabled.store(cmd.data.metadata.loopEnabled, std::memory_order_release);
          clip.gainDb.store(cmd.data.metadata.gainDb, std::memory_order_release);
          clip.gainLinear.store(cmd.data.metadata.gainLinear, std::memory_order_release);
          clip.routingGroup = cmd.data.metadata.routingGroup;
          configureVoiceRouting(i, clip.routingGroup, clip.numChannels);
        }
      }
      break;

    default:
      break;
    }

    readIndex = (readIndex + 1) % MAX_COMMANDS;
    m_commandReadIndex.store(readIndex, std::memory_order_release);
  }
}

ActiveClip* TransportController::findActiveClip(ClipHandle handle) {
  // Multi-voice: Returns first matching instance (not necessarily oldest)
  for (size_t i = 0; i < m_activeClipCount; ++i) {
    if (m_activeClips[i].handle == handle) {
      return &m_activeClips[i];
    }
  }
  return nullptr;
}

size_t TransportController::countActiveVoices(ClipHandle handle) const {
  size_t count = 0;
  for (size_t i = 0; i < m_activeClipCount; ++i) {
    if (m_activeClips[i].handle == handle) {
      ++count;
    }
  }
  return count;
}

size_t TransportController::countActiveVoicesSnapshot(ClipHandle handle) const {
  const ActiveVoiceSnapshot snapshot = getActiveVoiceSnapshot();
  for (uint32_t index = 0; index < snapshot.entryCount; ++index) {
    if (snapshot.entries[index].handle == handle) {
      return snapshot.entries[index].activeVoiceCount;
    }
  }
  return 0;
}

ActiveClip* TransportController::findOldestVoice(ClipHandle handle) {
  ActiveClip* oldest = nullptr;
  for (size_t i = 0; i < m_activeClipCount; ++i) {
    ActiveClip& candidate = m_activeClips[i];
    if (candidate.handle != handle) {
      continue;
    }
    if (oldest == nullptr || candidate.startSample < oldest->startSample ||
        (candidate.startSample == oldest->startSample &&
         isLaterStartOrdinal(oldest->startOrdinal, candidate.startOrdinal))) {
      oldest = &candidate;
    }
  }
  return oldest;
}

uint64_t TransportController::allocateVoiceStartOrdinal() noexcept {
  constexpr uint64_t kSerialHalfRange = uint64_t{1} << 63;
  if (m_activeClipCount != 0) {
    uint64_t oldestOrdinal = m_activeClips[0].startOrdinal;
    for (size_t index = 1; index < m_activeClipCount; ++index) {
      const uint64_t candidate = m_activeClips[index].startOrdinal;
      if (isLaterStartOrdinal(oldestOrdinal, candidate)) {
        oldestOrdinal = candidate;
      }
    }

    if (m_nextVoiceStartOrdinal - oldestOrdinal >= kSerialHalfRange) {
      for (size_t index = 0; index < m_activeClipCount; ++index) {
        m_voiceStartOrdinalScratch[index] = m_activeClips[index].startOrdinal;
      }
      for (size_t index = 0; index < m_activeClipCount; ++index) {
        uint64_t rank = 0;
        for (size_t other = 0; other < m_activeClipCount; ++other) {
          if (isLaterStartOrdinal(m_voiceStartOrdinalScratch[index],
                                  m_voiceStartOrdinalScratch[other])) {
            ++rank;
          }
        }
        m_activeClips[index].startOrdinal = rank;
      }
      m_nextVoiceStartOrdinal = static_cast<uint64_t>(m_activeClipCount);
    }
  }

  const uint64_t ordinal = m_nextVoiceStartOrdinal;
  ++m_nextVoiceStartOrdinal;
  return ordinal;
}

void TransportController::restartVoiceInPlace(ActiveClip& clip) {
  // ORP127 G5: reset a live voice to its trim IN for an in-place restart.
  int64_t trimIn = clip.trimInSamples.load(std::memory_order_relaxed);
  clip.currentSample = trimIn;
  if (clip.source) {
    clip.source->setDemand(trimIn);
  }
  clip.isStopping = false;
  clip.fadeOutGain = 1.0f;
  clip.hasLoopedOnce = false;
  // Broadcast-safe restart crossfade to avoid a click at the reset point.
  clip.isRestarting = true;
  clip.restartFadeFramesRemaining = static_cast<int64_t>(m_restartCrossfadeSamples);
}

bool TransportController::startVoiceWithMode(const std::shared_ptr<ClipPlaybackContext>& context) {
  if (!context)
    return false;

  const ClipHandle handle = context->handle;
  const VoiceMode mode = context->voiceMode;

  switch (mode) {
  case VoiceMode::Polyphonic:
    return addActiveClip(context);

  case VoiceMode::MonoStrict: {
    ActiveClip* primary = nullptr;
    for (size_t i = 0; i < m_activeClipCount; ++i) {
      if (m_activeClips[i].handle == handle && !primary)
        primary = &m_activeClips[i];
    }
    if (!primary)
      return addActiveClip(context);

    for (size_t i = m_activeClipCount; i-- > 0;) {
      if (m_activeClips[i].handle == handle && &m_activeClips[i] != primary) {
        removeActiveVoice(m_activeClips[i].voiceId);
        primary = nullptr;
        for (size_t j = 0; j < m_activeClipCount; ++j) {
          if (m_activeClips[j].handle == handle) {
            primary = &m_activeClips[j];
            break;
          }
        }
        if (!primary)
          break;
      }
    }
    if (!primary)
      return addActiveClip(context);

    const int64_t trimIn = primary->trimInSamples.load(std::memory_order_relaxed);
    primary->currentSample = trimIn;
    if (primary->source)
      primary->source->setDemand(trimIn);
    primary->isStopping = false;
    primary->fadeOutGain = 1.0f;
    primary->hasLoopedOnce = false;
    primary->isRestarting = false;
    primary->restartFadeFramesRemaining = 0;
    primary->voiceMode = mode;
    return true;
  }

  case VoiceMode::MonoWithFadeOverlap: {
    ActiveClip* live = nullptr;
    for (size_t i = 0; i < m_activeClipCount; ++i) {
      if (m_activeClips[i].handle == handle && !m_activeClips[i].isStopping) {
        live = &m_activeClips[i];
        break;
      }
    }
    if (!live)
      return addActiveClip(context);

    restartVoiceInPlace(*live);
    live->voiceMode = mode;
    return true;
  }
  }

  return addActiveClip(context);
}

void TransportController::configureVoiceRouting(size_t voiceIndex, RoutingGroupIndex group,
                                                uint16_t numChannels) noexcept {
  const size_t laneBase = voiceIndex * m_config.maxSourceChannels;
  for (uint32_t lane = 0; lane < m_config.maxSourceChannels; ++lane) {
    const bool activeLane = lane < numChannels;
    const auto routeGroup = activeLane ? group : UNASSIGNED_GROUP;
    const auto output =
        activeLane ? static_cast<RoutingOutputIndex>(lane) : static_cast<RoutingOutputIndex>(0);
    (void)m_routingMatrix->setChannelRoute(static_cast<RoutingChannelIndex>(laneBase + lane),
                                           routeGroup, output);
  }
}

uint32_t TransportController::allocateVoiceId() noexcept {
  for (size_t attempt = 0; attempt <= MAX_ACTIVE_CLIPS; ++attempt) {
    uint32_t candidate = m_nextVoiceId;
    if (candidate == 0) {
      candidate = 1;
    }
    m_nextVoiceId = candidate == std::numeric_limits<uint32_t>::max() ? 1 : candidate + 1;

    bool inUse = false;
    for (size_t active = 0; active < m_activeClipCount; ++active) {
      if (m_activeClips[active].voiceId == candidate) {
        inUse = true;
        break;
      }
    }
    if (!inUse) {
      return candidate;
    }
  }
  return 0;
}

bool TransportController::setVoiceSnapshotFieldsForTesting(uint32_t voiceId, bool stopping,
                                                           bool looping, int64_t trimIn,
                                                           int64_t trimOut,
                                                           int64_t currentSample) noexcept {
  for (size_t index = 0; index < m_activeClipCount; ++index) {
    ActiveClip& voice = m_activeClips[index];
    if (voice.voiceId != voiceId) {
      continue;
    }
    voice.isStopping = stopping;
    voice.fadeOutGain = 1.0f;
    voice.fadeOutStartPos = currentSample;
    voice.loopEnabled.store(looping, std::memory_order_relaxed);
    voice.trimInSamples.store(trimIn, std::memory_order_relaxed);
    voice.trimOutSamples.store(trimOut, std::memory_order_relaxed);
    voice.currentSample = currentSample;
    return true;
  }
  return false;
}
bool TransportController::addActiveClip(const std::shared_ptr<ClipPlaybackContext>& context) {
  if (!context)
    return false;

  ClipHandle handle = context->handle;

  // Multi-voice: Check if we need to remove oldest voice to make room.
  // ORP127 G7: the cap is now host-configurable (default 8, hard max 32).
  size_t currentVoiceCount = countActiveVoices(handle);
  size_t maxVoices = m_maxVoicesPerClip.load(std::memory_order_relaxed);
  if (currentVoiceCount >= maxVoices) {
    // At max capacity - remove oldest voice instance for this clip
    ActiveClip* oldest = findOldestVoice(handle);
    if (oldest) {
      const uint32_t oldestVoiceId = oldest->voiceId;
      // Replacing one voice for a handle is not a handle-level stop.
      removeActiveVoice(oldestVoiceId);
    }
  }

  if (m_activeClipCount >= m_config.maxActiveVoices) {
    TransportEvent event{};
    event.type = TransportEventType::ActiveClipLimitReached;
    event.handle = handle;
    event.position = getCurrentPosition();
    postTransportEvent(event);
    return false;
  }

  const uint32_t voiceId = allocateVoiceId();
  if (voiceId == 0) {
    return false;
  }

  const uint64_t startOrdinal = allocateVoiceStartOrdinal();

  // Initialize clip with immutable context - NO MUTEX NEEDED HERE
  const size_t voiceIndex = m_activeClipCount++;
  ActiveClip& clip = m_activeClips[voiceIndex];
  clip.handle = handle;
  clip.voiceId = voiceId;
  clip.startOrdinal = startOrdinal;
  clip.startSample = m_currentSample.load(std::memory_order_relaxed);
  clip.currentSample = context->trimInSamples; // CRITICAL: Start from IN point

  // Initialize trim points
  clip.trimInSamples.store(context->trimInSamples, std::memory_order_release);
  clip.trimOutSamples.store(context->trimOutSamples, std::memory_order_release);

  // Initialize fade settings
  clip.fadeInSeconds.store(context->fadeInSeconds, std::memory_order_release);
  clip.fadeOutSeconds.store(context->fadeOutSeconds, std::memory_order_release);
  clip.fadeInCurve.store(context->fadeInCurve, std::memory_order_release);
  clip.fadeOutCurve.store(context->fadeOutCurve, std::memory_order_release);

  // Calculate and store fade sample counts
  int64_t fadeInSampleCount =
      static_cast<int64_t>(context->fadeInSeconds * static_cast<double>(m_sampleRate));
  int64_t fadeOutSampleCount =
      static_cast<int64_t>(context->fadeOutSeconds * static_cast<double>(m_sampleRate));
  clip.fadeInSamples.store(fadeInSampleCount, std::memory_order_release);
  clip.fadeOutSamples.store(fadeOutSampleCount, std::memory_order_release);

  // Initialize gain (start the smoother AT the target so the clip opens at its
  // configured gain with no initial ramp — ORP127 G4).
  clip.gainDb.store(context->gainDb, std::memory_order_release);
  clip.gainLinear.store(context->gainLinear, std::memory_order_release);
  clip.gainCurrent = context->gainLinear;
  clip.gainRampIncrement = m_clipGainRampIncrement;

  // Initialize loop mode
  clip.loopEnabled.store(context->loopEnabled, std::memory_order_release);

  clip.source = context->source; // Store shared_ptr (maintains reference count)
  clip.numChannels = context->numChannels;
  clip.fileLengthSamples = context->fileLengthSamples; // ORP127 G3
  clip.voiceMode = context->voiceMode;                 // ORP127 G5
  clip.routingGroup = context->routingGroup;
  configureVoiceRouting(voiceIndex, clip.routingGroup, clip.numChannels);
  clip.fadeOutGain = 1.0f;
  clip.isStopping = false;
  clip.fadeOutStartPos = 0; // Will be set when stopClip() is called

  // Initialize restart crossfade state
  clip.isRestarting = false;
  clip.restartFadeFramesRemaining = 0;

  // ORP097 Bug 7 Fix: Initialize loop state
  clip.hasLoopedOnce = false;

  // Prime the streaming prefetcher at the trim IN point (no-op for prepared
  // sources; sources are position-explicit so there is nothing to seek).
  if (clip.source) {
    clip.source->setDemand(context->trimInSamples);
  }
  return true;
}

void TransportController::removeActiveVoice(uint32_t voiceId) {
  for (size_t i = 0; i < m_activeClipCount; ++i) {
    if (m_activeClips[i].voiceId == voiceId) {
      // Remove by moving last clip into this slot (manual field-by-field copy since atomic fields
      // can't be copied)
      if (i < m_activeClipCount - 1) {
        ActiveClip& dest = m_activeClips[i];
        ActiveClip& src = m_activeClips[m_activeClipCount - 1];

        dest.handle = src.handle;
        dest.voiceId = src.voiceId; // Multi-voice: copy voice ID
        dest.startOrdinal = src.startOrdinal;
        dest.startSample = src.startSample;
        dest.currentSample = src.currentSample;
        dest.trimInSamples.store(src.trimInSamples.load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
        dest.trimOutSamples.store(src.trimOutSamples.load(std::memory_order_relaxed),
                                  std::memory_order_relaxed);
        dest.fadeInSeconds.store(src.fadeInSeconds.load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
        dest.fadeOutSeconds.store(src.fadeOutSeconds.load(std::memory_order_relaxed),
                                  std::memory_order_relaxed);
        dest.fadeInCurve.store(src.fadeInCurve.load(std::memory_order_relaxed),
                               std::memory_order_relaxed);
        dest.fadeOutCurve.store(src.fadeOutCurve.load(std::memory_order_relaxed),
                                std::memory_order_relaxed);
        dest.fadeInSamples.store(src.fadeInSamples.load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
        dest.fadeOutSamples.store(src.fadeOutSamples.load(std::memory_order_relaxed),
                                  std::memory_order_relaxed);
        dest.gainDb.store(src.gainDb.load(std::memory_order_relaxed), std::memory_order_relaxed);
        // ORP127 G4: carry the linear gain target + smoothing ramp state across
        // the slot move (gainLinear was previously not copied here).
        dest.gainLinear.store(src.gainLinear.load(std::memory_order_relaxed),
                              std::memory_order_relaxed);
        dest.gainCurrent = src.gainCurrent;
        dest.gainRampIncrement = src.gainRampIncrement;
        dest.loopEnabled.store(src.loopEnabled.load(std::memory_order_relaxed),
                               std::memory_order_relaxed);
        dest.fadeOutGain = src.fadeOutGain;
        dest.isStopping = src.isStopping;
        dest.fadeOutStartPos = src.fadeOutStartPos;
        dest.isRestarting = src.isRestarting;
        dest.restartFadeFramesRemaining = src.restartFadeFramesRemaining;
        dest.hasLoopedOnce = src.hasLoopedOnce;
        dest.source = src.source;
        dest.numChannels = src.numChannels;
        dest.fileLengthSamples = src.fileLengthSamples; // ORP127 G3
        dest.voiceMode = src.voiceMode;                 // ORP127 G5
        dest.routingGroup = src.routingGroup;
        configureVoiceRouting(i, dest.routingGroup, dest.numChannels);
      }
      configureVoiceRouting(m_activeClipCount - 1, UNASSIGNED_GROUP, 0);
      --m_activeClipCount;
      return;
    }
  }
}

void TransportController::removeActiveClip(ClipHandle handle) {
  // Multi-voice: This removes FIRST instance found (deprecated - use removeActiveVoice)
  for (size_t i = 0; i < m_activeClipCount; ++i) {
    if (m_activeClips[i].handle == handle) {
      // Remove by moving last clip into this slot (manual field-by-field copy since atomic fields
      // can't be copied)
      if (i < m_activeClipCount - 1) {
        ActiveClip& dest = m_activeClips[i];
        ActiveClip& src = m_activeClips[m_activeClipCount - 1];

        dest.handle = src.handle;
        dest.voiceId = src.voiceId; // Multi-voice: copy voice ID
        dest.startOrdinal = src.startOrdinal;
        dest.startSample = src.startSample;
        dest.currentSample = src.currentSample;
        dest.trimInSamples.store(src.trimInSamples.load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
        dest.trimOutSamples.store(src.trimOutSamples.load(std::memory_order_relaxed),
                                  std::memory_order_relaxed);
        dest.fadeInSeconds.store(src.fadeInSeconds.load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
        dest.fadeOutSeconds.store(src.fadeOutSeconds.load(std::memory_order_relaxed),
                                  std::memory_order_relaxed);
        dest.fadeInCurve.store(src.fadeInCurve.load(std::memory_order_relaxed),
                               std::memory_order_relaxed);
        dest.fadeOutCurve.store(src.fadeOutCurve.load(std::memory_order_relaxed),
                                std::memory_order_relaxed);
        dest.fadeInSamples.store(src.fadeInSamples.load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
        dest.fadeOutSamples.store(src.fadeOutSamples.load(std::memory_order_relaxed),
                                  std::memory_order_relaxed);
        dest.gainDb.store(src.gainDb.load(std::memory_order_relaxed), std::memory_order_relaxed);
        // ORP127 G4: carry the linear gain target + smoothing ramp state across
        // the slot move (gainLinear was previously not copied here).
        dest.gainLinear.store(src.gainLinear.load(std::memory_order_relaxed),
                              std::memory_order_relaxed);
        dest.gainCurrent = src.gainCurrent;
        dest.gainRampIncrement = src.gainRampIncrement;
        dest.loopEnabled.store(src.loopEnabled.load(std::memory_order_relaxed),
                               std::memory_order_relaxed);
        dest.fadeOutGain = src.fadeOutGain;
        dest.isStopping = src.isStopping;
        dest.fadeOutStartPos = src.fadeOutStartPos;
        dest.isRestarting = src.isRestarting;
        dest.restartFadeFramesRemaining = src.restartFadeFramesRemaining;
        dest.hasLoopedOnce = src.hasLoopedOnce; // ORP097 Bug 7 Fix
        dest.source = src.source;               // Copy shared_ptr (atomic refcount increment)
        dest.numChannels = src.numChannels;
        dest.fileLengthSamples = src.fileLengthSamples;
        dest.voiceMode = src.voiceMode;
        dest.routingGroup = src.routingGroup;
        configureVoiceRouting(i, dest.routingGroup, dest.numChannels);
      }
      configureVoiceRouting(m_activeClipCount - 1, UNASSIGNED_GROUP, 0);
      --m_activeClipCount;
      return;
    }
  }
}

SessionGraphError TransportController::postCommand(const TransportCommand& command) {
  // ORP127 G1: Single choke point for UI → audio-thread commands. SPSC ring;
  // ONE control thread is the sole producer, the audio thread the sole
  // consumer. Every control-mutating entry point funnels through here (ORP133
  // G3), so the debug producer check below covers the whole mutation surface.
#ifndef NDEBUG
  {
    // ORP133 G3: enforce the single-producer contract in debug builds. The
    // first producer thread claims the queue; any later post from a different
    // thread is a contract violation (hosts with multiple control sources must
    // funnel through a single dispatcher — see ITransportController docs).
    std::thread::id expected{};
    const std::thread::id self = std::this_thread::get_id();
    if (!m_commandProducerThread.compare_exchange_strong(expected, self,
                                                         std::memory_order_relaxed) &&
        expected != self) {
      assert(false &&
             "TransportController: control-mutating methods must be called from a single "
             "control thread (SPSC command queue). Funnel UI/MIDI/OSC through one dispatcher.");
    }
  }
#endif

  size_t writeIndex = m_commandWriteIndex.load(std::memory_order_relaxed);
  size_t nextIndex = (writeIndex + 1) % MAX_COMMANDS;

  if (nextIndex == m_commandReadIndex.load(std::memory_order_acquire)) {
    return SessionGraphError::InternalError; // Queue full
  }

  m_commands[writeIndex] = command;
  m_commandWriteIndex.store(nextIndex, std::memory_order_release);
  return SessionGraphError::OK;
}

void TransportController::publishVoiceSnapshot() noexcept {
  ActiveVoiceSnapshot& snapshot = m_voiceSnapshotScratch;
  snapshot.entryCount = 0;
  snapshot.totalActiveVoiceCount = static_cast<uint32_t>(m_activeClipCount);
  snapshot.publicationSequence = incrementSaturated(m_voiceSnapshotSequence);
  m_voiceSnapshotSequence = snapshot.publicationSequence;

  for (size_t voiceIndex = 0; voiceIndex < m_activeClipCount; ++voiceIndex) {
    const ActiveClip& voice = m_activeClips[voiceIndex];
    uint32_t entryIndex = 0;
    while (entryIndex < snapshot.entryCount &&
           snapshot.entries[entryIndex].handle != voice.handle) {
      ++entryIndex;
    }

    if (entryIndex == snapshot.entryCount) {
      if (snapshot.entryCount >= kActiveVoiceSnapshotCapacity) {
        break;
      }
      ++snapshot.entryCount;
      ActiveVoiceSnapshotEntry& entry = snapshot.entries[entryIndex];
      entry = {};
      entry.handle = voice.handle;
      entry.state = PlaybackState::Stopping;
      m_voiceSnapshotHasNewest[entryIndex] = false;
      m_voiceSnapshotNewestStartOrdinal[entryIndex] = 0;
    }

    ActiveVoiceSnapshotEntry& entry = snapshot.entries[entryIndex];
    ++entry.activeVoiceCount;
    if (!voice.isStopping) {
      entry.state = PlaybackState::Playing;
    }

    if (!m_voiceSnapshotHasNewest[entryIndex] || voice.startSample > entry.newestStartSample ||
        (voice.startSample == entry.newestStartSample &&
         isLaterStartOrdinal(voice.startOrdinal, m_voiceSnapshotNewestStartOrdinal[entryIndex]))) {
      entry.newestVoiceId = voice.voiceId;
      entry.newestVoiceStopping = voice.isStopping ? 1 : 0;
      entry.newestVoiceLoopEnabled = voice.loopEnabled.load(std::memory_order_relaxed) ? 1 : 0;
      entry.newestStartSample = voice.startSample;
      entry.newestTrimInSamples = voice.trimInSamples.load(std::memory_order_relaxed);
      entry.newestTrimOutSamples = voice.trimOutSamples.load(std::memory_order_relaxed);
      entry.newestPosition = positionAtSamples(voice.currentSample);
      m_voiceSnapshotHasNewest[entryIndex] = true;
      m_voiceSnapshotNewestStartOrdinal[entryIndex] = voice.startOrdinal;
    }
  }

  const uint64_t oddRevision = m_voiceSnapshotRevision + 1;
  m_voiceSnapshotRevision = oddRevision;
  m_publishedVoiceSnapshot.revision.store(oddRevision, std::memory_order_release);
  m_publishedVoiceSnapshot.publicationSequence.store(snapshot.publicationSequence,
                                                     std::memory_order_release);
  m_publishedVoiceSnapshot.entryCount.store(snapshot.entryCount, std::memory_order_release);
  m_publishedVoiceSnapshot.totalActiveVoiceCount.store(snapshot.totalActiveVoiceCount,
                                                       std::memory_order_release);

  for (uint32_t index = 0; index < snapshot.entryCount; ++index) {
    const ActiveVoiceSnapshotEntry& source = snapshot.entries[index];
    AtomicActiveVoiceSnapshotEntry& destination = m_publishedVoiceSnapshot.entries[index];
    destination.handle.store(source.handle, std::memory_order_release);
    destination.activeVoiceCount.store(source.activeVoiceCount, std::memory_order_release);
    destination.newestVoiceId.store(source.newestVoiceId, std::memory_order_release);
    destination.state.store(static_cast<uint8_t>(source.state), std::memory_order_release);
    destination.newestVoiceStopping.store(source.newestVoiceStopping, std::memory_order_release);
    destination.newestVoiceLoopEnabled.store(source.newestVoiceLoopEnabled,
                                             std::memory_order_release);
    destination.newestStartSample.store(source.newestStartSample, std::memory_order_release);
    destination.newestTrimInSamples.store(source.newestTrimInSamples, std::memory_order_release);
    destination.newestTrimOutSamples.store(source.newestTrimOutSamples, std::memory_order_release);
    destination.newestPositionSamples.store(source.newestPosition.samples,
                                            std::memory_order_release);
    destination.newestPositionSecondsBits.store(
        std::bit_cast<uint64_t>(source.newestPosition.seconds), std::memory_order_release);
    destination.newestPositionBeatsBits.store(std::bit_cast<uint64_t>(source.newestPosition.beats),
                                              std::memory_order_release);
  }

  const uint64_t evenRevision = m_voiceSnapshotRevision + 1;
  m_voiceSnapshotRevision = evenRevision;
  m_publishedVoiceSnapshot.revision.store(evenRevision, std::memory_order_release);
}

void TransportController::postTransportEvent(const TransportEvent& sourceEvent) noexcept {
  TransportEvent event = sourceEvent;
  m_callbackAttemptedSequence = incrementSaturated(m_callbackAttemptedSequence);
  event.sequence = m_callbackAttemptedSequence;

  const size_t writeIdx = m_callbackWriteIndex.load(std::memory_order_relaxed);
  const size_t nextIdx = (writeIdx + 1) & (CALLBACK_QUEUE_SIZE - 1);

  if (nextIdx == m_callbackReadIndex.load(std::memory_order_acquire)) {
    m_callbackDroppedCount = incrementSaturated(m_callbackDroppedCount);
    m_callbackLastDroppedSequence = event.sequence;
    return;
  }

  m_eventRing[writeIdx] = event;
  m_callbackWriteIndex.store(nextIdx, std::memory_order_release);
  m_callbackPostedSequence = event.sequence;
}

void TransportController::publishCallbackTelemetry() noexcept {
  const uint64_t oddRevision = m_callbackTelemetryRevision + 1;
  m_callbackTelemetryRevision = oddRevision;
  m_publishedCallbackTelemetry.revision.store(oddRevision, std::memory_order_release);
  m_publishedCallbackTelemetry.lastAttemptedSequence.store(m_callbackAttemptedSequence,
                                                           std::memory_order_release);
  m_publishedCallbackTelemetry.lastPostedSequence.store(m_callbackPostedSequence,
                                                        std::memory_order_release);
  m_publishedCallbackTelemetry.cumulativeDroppedCount.store(m_callbackDroppedCount,
                                                            std::memory_order_release);
  m_publishedCallbackTelemetry.lastDroppedSequence.store(m_callbackLastDroppedSequence,
                                                         std::memory_order_release);
  m_publishedCallbackTelemetry.activeVoiceSnapshotSequence.store(m_voiceSnapshotSequence,
                                                                 std::memory_order_release);
  const uint64_t evenRevision = m_callbackTelemetryRevision + 1;
  m_callbackTelemetryRevision = evenRevision;
  m_publishedCallbackTelemetry.revision.store(evenRevision, std::memory_order_release);
}

void TransportController::processCallbacks() {
  // FTR027 §1: pick up session tempo changes on the control-thread pump.
  // SessionGraph::set_tempo() mutates a plain field on the (single) control
  // thread; this same-thread read republishes it through the atomic cache for
  // getCurrentPosition() readers on any thread.
  if (m_sessionGraph) {
    m_tempoBpm.store(m_sessionGraph->tempo(), std::memory_order_relaxed);
  }

  // ORP121 C-03 / ORP133 G1: Lock-free SPSC event queue
  // UI thread reads POD events from the ring buffer (no mutex, no blocking)
  // and translates them into the host's ITransportCallback virtuals. Events
  // are dispatched strictly in ring (emission) order.
  //
  // Memory ordering rationale:
  // - relaxed for same-thread reads (readIdx in processCallbacks)
  // - acquire for cross-thread reads (writeIdx to see all pending events)
  // - release for the read-index update (frees the slots for the producer)

  size_t readIdx = m_callbackReadIndex.load(std::memory_order_relaxed);
  size_t writeIdx = m_callbackWriteIndex.load(std::memory_order_acquire);

  while (readIdx != writeIdx) {
    const TransportEvent& event = m_eventRing[readIdx];
    if (m_callback) {
      switch (event.type) {
      case TransportEventType::ClipStarted:
        m_callback->onClipStarted(event.handle, event.position);
        break;
      case TransportEventType::ClipStopped:
        m_callback->onClipStopped(event.handle, event.position);
        break;
      case TransportEventType::ClipLooped:
        m_callback->onClipLooped(event.handle, event.position);
        break;
      case TransportEventType::ClipRestarted:
        m_callback->onClipRestarted(event.handle, event.position);
        break;
      case TransportEventType::ClipSeeked:
        m_callback->onClipSeeked(event.handle, event.position);
        break;
      case TransportEventType::BufferUnderrun:
        m_callback->onBufferUnderrun(event.position);
        break;
      case TransportEventType::ActiveClipLimitReached:
        m_callback->onActiveClipLimitReached(event.handle, event.position);
        break;
      }
    }
    readIdx = (readIdx + 1) & (CALLBACK_QUEUE_SIZE - 1);
  }

  m_callbackReadIndex.store(readIdx, std::memory_order_release);
}

SessionGraphError TransportController::registerClipAudio(ClipHandle handle,
                                                         const std::string& file_path) {
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  if (file_path.empty()) {
    return SessionGraphError::InvalidParameter;
  }

  std::lock_guard<std::mutex> lock(m_audioFilesMutex);

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
  AudioFileEntry entry;

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

  if (entry.metadata.num_channels == 0 ||
      entry.metadata.num_channels > m_config.maxSourceChannels ||
      (m_config.sourceChannelPolicy == SourceChannelPolicy::Discrete &&
       entry.metadata.num_channels > m_config.outputChannels)) {
    return SessionGraphError::InvalidParameter;
  }

  if (const auto inferred = inferUnambiguousChannelFormat(entry.metadata.num_channels)) {
    entry.sourceLayout = inferred->layout;
    entry.speakerPatchSize = inferred->num_channels;
    std::copy_n(inferred->channel_map.begin(), inferred->num_channels, entry.speakerPatch.begin());
  }

  // Apply session defaults to new clip
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

  m_audioFiles[handle] = std::move(entry);

  return SessionGraphError::OK;
}

SessionGraphError TransportController::prepareClipAudio(ClipHandle handle) {
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  // Preserved contract: preparation is a pre-playback operation. (With
  // ORP134 G1 sources a re-preparation while playing would actually be safe -
  // active voices hold their own reference - but the historical NotReady
  // keeps host behavior unchanged.)
  if (isClipPlaying(handle)) {
    return SessionGraphError::NotReady;
  }

  std::lock_guard<std::mutex> lock(m_audioFilesMutex);
  auto it = m_audioFiles.find(handle);
  if (it == m_audioFiles.end()) {
    return SessionGraphError::ClipNotRegistered;
  }
  return ensurePreparedSourceLocked(it->second);
}

SessionGraphError TransportController::unregisterClipAudio(ClipHandle handle) {
  // OCC155 Ask #4: inverse of registerClipAudio(). Non-realtime control-thread
  // call. Frees the reader + prepared/streaming source held for the handle.
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  // Refuse while voices are live: an active voice holds its own shared_ptr to
  // the source, so erasing the registry entry would not free it immediately and
  // — more importantly — the host may then re-register the same handle and race
  // its still-playing tail. Require the caller to stop/panic first. Reads the
  // published snapshot (safe from the control thread; no audio-array access).
  if (isClipPlaying(handle)) {
    return SessionGraphError::NotReady;
  }

  std::lock_guard<std::mutex> lock(m_audioFilesMutex);
  // Idempotent: releasing an unregistered handle is a no-op success so hosts can
  // call it unconditionally on slot teardown.
  m_audioFiles.erase(handle);
  return SessionGraphError::OK;
}

SessionGraphError TransportController::ensurePreparedSourceLocked(AudioFileEntry& entry) {
  // ORP134 G1: build the realtime playback source on the CONTROL thread. All
  // decode/resample/file I/O happens here (or on the stream worker) - the
  // audio thread only ever memcpy-reads the published source.
  if (entry.source) {
    return SessionGraphError::OK;
  }

  const uint16_t numChannels = entry.metadata.num_channels;
  if (!entry.reader || !entry.reader->isOpen() || numChannels == 0 ||
      numChannels > m_config.maxSourceChannels) {
    return SessionGraphError::NotReady;
  }

  const int64_t lengthFrames = entry.metadata.duration_samples;
  if (lengthFrames <= m_preparedSourceMaxFrames) {
    // Short clip: decode the whole file (engine-rate; entry.reader is already
    // wrapped in the ORP127 G6 resampler when rates differ).
    auto prepared = PreparedClipSource::decode(*entry.reader, numChannels, lengthFrames);
    if (!prepared) {
      return SessionGraphError::InternalError;
    }
    entry.source = prepared;
    return SessionGraphError::OK;
  }

  // Long file: stream through a fixed page ring. Prefill the window at the
  // trim IN synchronously so playback starts without an initial underrun,
  // then hand the source to the background worker for steady-state refills.
  auto streaming = std::make_shared<StreamingClipSource>(entry.reader, numChannels, lengthFrames);
  streaming->prefill(entry.trimInSamples);
  if (!m_streamWorker) {
    m_streamWorker = std::make_unique<MediaStreamWorker>();
  }
  m_streamWorker->attach(streaming);
  entry.source = streaming;
  return SessionGraphError::OK;
}

SessionGraphError TransportController::updateClipTrimPoints(ClipHandle handle,
                                                            int64_t trimInSamples,
                                                            int64_t trimOutSamples) {
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  // Find clip in registered audio files (need to check file duration)
  int64_t fileDurationSamples = 0;
  {
    std::lock_guard<std::mutex> lock(m_audioFilesMutex);
    auto it = m_audioFiles.find(handle);
    if (it == m_audioFiles.end()) {
      return SessionGraphError::ClipNotRegistered;
    }
    fileDurationSamples = it->second.metadata.duration_samples;
  }

  // Validate trim points
  if (trimInSamples < 0 || trimInSamples >= fileDurationSamples) {
    return SessionGraphError::InvalidClipTrimPoints;
  }

  if (trimOutSamples <= trimInSamples || trimOutSamples > fileDurationSamples) {
    return SessionGraphError::InvalidClipTrimPoints;
  }

  // Store trim points persistently in AudioFileEntry
  {
    std::lock_guard<std::mutex> lock(m_audioFilesMutex);
    auto it = m_audioFiles.find(handle);
    if (it != m_audioFiles.end()) {
      it->second.trimInSamples = trimInSamples;
      it->second.trimOutSamples = trimOutSamples;
    }
  }

  // Post command to audio thread for thread-safe update (ORP115)
  TransportCommand cmd{};
  cmd.type = TransportCommand::Type::UpdateTrim;
  cmd.handle = handle;
  cmd.data.trim.in = trimInSamples;
  cmd.data.trim.out = trimOutSamples;
  return postCommand(cmd);
}

SessionGraphError TransportController::updateClipFades(ClipHandle handle, double fadeInSeconds,
                                                       double fadeOutSeconds, FadeCurve fadeInCurve,
                                                       FadeCurve fadeOutCurve) {
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  // Find clip in registered audio files
  int64_t fileDurationSamples = 0;
  int64_t currentTrimIn = 0;
  int64_t currentTrimOut = 0;
  {
    std::lock_guard<std::mutex> lock(m_audioFilesMutex);
    auto it = m_audioFiles.find(handle);
    if (it == m_audioFiles.end()) {
      return SessionGraphError::ClipNotRegistered;
    }
    fileDurationSamples = it->second.metadata.duration_samples;
  }

  // Get current trim points (or use defaults). ORP127 G1: read from the
  // published snapshot for active voices instead of the live audio-thread array.
  bool foundActiveClip = false;
  const ActiveVoiceSnapshot activeSnapshot = getActiveVoiceSnapshot();
  for (uint32_t index = 0; index < activeSnapshot.entryCount; ++index) {
    if (activeSnapshot.entries[index].handle == handle) {
      currentTrimIn = activeSnapshot.entries[index].newestTrimInSamples;
      currentTrimOut = activeSnapshot.entries[index].newestTrimOutSamples;
      foundActiveClip = true;
      break;
    }
  }

  if (!foundActiveClip) {
    currentTrimIn = 0;
    currentTrimOut = fileDurationSamples;
  }

  // Validate fade durations
  int64_t clipDuration = currentTrimOut - currentTrimIn;
  double clipDurationSeconds =
      static_cast<double>(clipDuration) / static_cast<double>(m_sampleRate);

  if (fadeInSeconds < 0.0 || fadeInSeconds > clipDurationSeconds) {
    return SessionGraphError::InvalidFadeDuration;
  }

  if (fadeOutSeconds < 0.0 || fadeOutSeconds > clipDurationSeconds) {
    return SessionGraphError::InvalidFadeDuration;
  }

  // Store fade settings persistently in AudioFileEntry
  {
    std::lock_guard<std::mutex> lock(m_audioFilesMutex);
    auto it = m_audioFiles.find(handle);
    if (it != m_audioFiles.end()) {
      it->second.fadeInSeconds = fadeInSeconds;
      it->second.fadeOutSeconds = fadeOutSeconds;
      it->second.fadeInCurve = fadeInCurve;
      it->second.fadeOutCurve = fadeOutCurve;
    }
  }

  // Post command to audio thread for thread-safe update (ORP115)
  TransportCommand cmd{};
  cmd.type = TransportCommand::Type::UpdateFade;
  cmd.handle = handle;
  cmd.data.fade.inSeconds = fadeInSeconds;
  cmd.data.fade.outSeconds = fadeOutSeconds;
  cmd.data.fade.inCurve = fadeInCurve;
  cmd.data.fade.outCurve = fadeOutCurve;
  return postCommand(cmd);
}

SessionGraphError TransportController::getClipTrimPoints(ClipHandle handle, int64_t& trimInSamples,
                                                         int64_t& trimOutSamples) const {
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  // ORP127 G1: Try the published snapshot first (most recent active values).
  const ActiveVoiceSnapshot activeSnapshot = getActiveVoiceSnapshot();
  for (uint32_t index = 0; index < activeSnapshot.entryCount; ++index) {
    if (activeSnapshot.entries[index].handle == handle) {
      trimInSamples = activeSnapshot.entries[index].newestTrimInSamples;
      trimOutSamples = activeSnapshot.entries[index].newestTrimOutSamples;
      return SessionGraphError::OK;
    }
  }

  // If not active, check persistent storage
  // NOTE: const_cast needed because this is a query method (read-only, but mutex requires
  // non-const)
  {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_audioFilesMutex));
    auto it = m_audioFiles.find(handle);
    if (it == m_audioFiles.end()) {
      return SessionGraphError::ClipNotRegistered;
    }

    // Return persistent metadata (or defaults if not set)
    trimInSamples = it->second.trimInSamples;
    trimOutSamples = it->second.trimOutSamples;

    // If trim OUT is not set (0), use file duration
    if (trimOutSamples == 0) {
      trimOutSamples = it->second.metadata.duration_samples;
    }
  }

  return SessionGraphError::OK;
}

SessionGraphError TransportController::updateClipGain(ClipHandle handle, float gainDb) {
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  // Validate gain (must be finite)
  if (!std::isfinite(gainDb)) {
    return SessionGraphError::InvalidParameter;
  }

  // Store gain persistently in AudioFileEntry
  {
    std::lock_guard<std::mutex> lock(m_audioFilesMutex);
    auto it = m_audioFiles.find(handle);
    if (it == m_audioFiles.end()) {
      return SessionGraphError::ClipNotRegistered;
    }
    it->second.gainDb = gainDb;
  }

  // Post command to audio thread for thread-safe update (ORP115)
  TransportCommand cmd{};
  cmd.type = TransportCommand::Type::UpdateGain;
  cmd.handle = handle;
  cmd.data.gainDb = gainDb;
  return postCommand(cmd);
}

SessionGraphError TransportController::setClipLoopMode(ClipHandle handle, bool shouldLoop) {
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  // Store loop mode persistently in AudioFileEntry
  {
    std::lock_guard<std::mutex> lock(m_audioFilesMutex);
    auto it = m_audioFiles.find(handle);
    if (it == m_audioFiles.end()) {
      return SessionGraphError::ClipNotRegistered;
    }
    it->second.loopEnabled = shouldLoop;
  }

  // Post command to audio thread for thread-safe update (ORP115)
  TransportCommand cmd{};
  cmd.type = TransportCommand::Type::UpdateLoop;
  cmd.handle = handle;
  cmd.data.booleanValue = shouldLoop;
  return postCommand(cmd);
}

int64_t TransportController::getClipPosition(ClipHandle handle) const {
  const ActiveVoiceSnapshot snapshot = getActiveVoiceSnapshot();
  for (uint32_t index = 0; index < snapshot.entryCount; ++index) {
    const ActiveVoiceSnapshotEntry& entry = snapshot.entries[index];
    if (entry.handle != handle) {
      continue;
    }

    int64_t position = entry.newestPosition.samples;
    if (entry.newestVoiceStopping != 0) {
      const int64_t maxValidPosition = entry.newestTrimOutSamples - 1;
      position = std::min(position, maxValidPosition);
    }
    return position;
  }
  return -1;
}

SessionGraphError TransportController::setClipStopOthersMode(ClipHandle handle, bool enabled) {
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  // Store mode persistently in AudioFileEntry
  std::lock_guard<std::mutex> lock(m_audioFilesMutex);
  auto it = m_audioFiles.find(handle);
  if (it == m_audioFiles.end()) {
    return SessionGraphError::ClipNotRegistered;
  }

  it->second.stopOthersOnPlay = enabled;
  return SessionGraphError::OK;
}

bool TransportController::getClipStopOthersMode(ClipHandle handle) const {
  std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_audioFilesMutex));
  auto it = m_audioFiles.find(handle);
  if (it == m_audioFiles.end()) {
    return false;
  }

  return it->second.stopOthersOnPlay;
}

SessionGraphError TransportController::updateClipMetadata(ClipHandle handle,
                                                          const ClipMetadata& metadata) {
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  // Validate metadata before applying changes (atomic operation)
  // Get file duration for validation
  int64_t fileDurationSamples = 0;
  uint16_t fileChannels = 0;
  {
    std::lock_guard<std::mutex> lock(m_audioFilesMutex);
    auto it = m_audioFiles.find(handle);
    if (it == m_audioFiles.end()) {
      return SessionGraphError::ClipNotRegistered;
    }
    fileDurationSamples = it->second.metadata.duration_samples;
    fileChannels = it->second.metadata.num_channels;
  }

  // Validate trim points
  int64_t trimOut = metadata.trimOutSamples;
  if (trimOut == 0) {
    trimOut = fileDurationSamples; // Use file duration if not specified
  }

  if (metadata.trimInSamples < 0 || metadata.trimInSamples >= fileDurationSamples) {
    return SessionGraphError::InvalidClipTrimPoints;
  }

  if (trimOut <= metadata.trimInSamples || trimOut > fileDurationSamples) {
    return SessionGraphError::InvalidClipTrimPoints;
  }

  // Validate fade durations
  int64_t clipDuration = trimOut - metadata.trimInSamples;
  double clipDurationSeconds =
      static_cast<double>(clipDuration) / static_cast<double>(m_sampleRate);

  if (metadata.fadeInSeconds < 0.0 || metadata.fadeInSeconds > clipDurationSeconds) {
    return SessionGraphError::InvalidFadeDuration;
  }

  if (metadata.fadeOutSeconds < 0.0 || metadata.fadeOutSeconds > clipDurationSeconds) {
    return SessionGraphError::InvalidFadeDuration;
  }

  // Validate gain
  if (!std::isfinite(metadata.gainDb)) {
    return SessionGraphError::InvalidParameter;
  }

  if (metadata.routingGroup >= m_config.numGroups ||
      fileChannels > m_groupOutputWidths[metadata.routingGroup].load(std::memory_order_acquire)) {
    return SessionGraphError::InvalidParameter;
  }
  if (!isValidSpeakerPatch(metadata.sourceLayout, metadata.speakerPatchSize, metadata.speakerPatch,
                           fileChannels)) {
    return SessionGraphError::InvalidParameter;
  }

  // All validation passed - apply changes atomically
  // Calculate fade sample counts
  int64_t fadeInSampleCount =
      static_cast<int64_t>(metadata.fadeInSeconds * static_cast<double>(m_sampleRate));
  int64_t fadeOutSampleCount =
      static_cast<int64_t>(metadata.fadeOutSeconds * static_cast<double>(m_sampleRate));

  // ORP127 G1: Apply to active voices via a command on the audio thread — no
  // direct writes to the live voice array from the UI thread. Precompute linear
  // gain here so the audio thread does no pow().
  TransportCommand cmd{};
  cmd.type = TransportCommand::Type::UpdateMetadata;
  cmd.handle = handle;
  cmd.data.metadata.trimIn = metadata.trimInSamples;
  cmd.data.metadata.trimOut = trimOut;
  cmd.data.metadata.fadeInSamples = fadeInSampleCount;
  cmd.data.metadata.fadeOutSamples = fadeOutSampleCount;
  cmd.data.metadata.fadeInSeconds = metadata.fadeInSeconds;
  cmd.data.metadata.fadeOutSeconds = metadata.fadeOutSeconds;
  cmd.data.metadata.fadeInCurve = metadata.fadeInCurve;
  cmd.data.metadata.fadeOutCurve = metadata.fadeOutCurve;
  cmd.data.metadata.loopEnabled = metadata.loopEnabled;
  cmd.data.metadata.gainDb = metadata.gainDb;
  cmd.data.metadata.gainLinear = std::pow(10.0f, metadata.gainDb / 20.0f);
  cmd.data.metadata.routingGroup = metadata.routingGroup;

  // Queue admission precedes the persistent commit. A full ring therefore
  // cannot publish metadata that active voices did not receive, which is
  // required when group choke compares registered and active routing groups.
  {
    std::lock_guard<std::mutex> lock(m_audioFilesMutex);
    auto it = m_audioFiles.find(handle);
    if (it == m_audioFiles.end()) {
      return SessionGraphError::ClipNotRegistered;
    }
    const SessionGraphError postResult = postCommand(cmd);
    if (postResult != SessionGraphError::OK) {
      return postResult;
    }

    it->second.trimInSamples = metadata.trimInSamples;
    it->second.trimOutSamples = trimOut;
    it->second.fadeInSeconds = metadata.fadeInSeconds;
    it->second.fadeOutSeconds = metadata.fadeOutSeconds;
    it->second.fadeInCurve = metadata.fadeInCurve;
    it->second.fadeOutCurve = metadata.fadeOutCurve;
    it->second.loopEnabled = metadata.loopEnabled;
    it->second.stopOthersOnPlay = metadata.stopOthersOnPlay;
    it->second.voiceMode = metadata.voiceMode;
    it->second.gainDb = metadata.gainDb;
    it->second.routingGroup = metadata.routingGroup;
    it->second.sourceLayout = metadata.sourceLayout;
    it->second.speakerPatchSize = metadata.speakerPatchSize;
    it->second.speakerPatch = metadata.speakerPatch;
  }

  return SessionGraphError::OK;
}

std::optional<ClipMetadata> TransportController::getClipMetadata(ClipHandle handle) const {
  if (handle == 0) {
    return std::nullopt;
  }

  std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_audioFilesMutex));
  auto it = m_audioFiles.find(handle);
  if (it == m_audioFiles.end()) {
    return std::nullopt;
  }

  // Build metadata from persistent storage
  ClipMetadata metadata;
  metadata.trimInSamples = it->second.trimInSamples;
  metadata.trimOutSamples = it->second.trimOutSamples;
  metadata.fadeInSeconds = it->second.fadeInSeconds;
  metadata.fadeOutSeconds = it->second.fadeOutSeconds;
  metadata.fadeInCurve = it->second.fadeInCurve;
  metadata.fadeOutCurve = it->second.fadeOutCurve;
  metadata.loopEnabled = it->second.loopEnabled;
  metadata.stopOthersOnPlay = it->second.stopOthersOnPlay;
  metadata.voiceMode = it->second.voiceMode; // ORP127 G5
  metadata.gainDb = it->second.gainDb;
  metadata.routingGroup = it->second.routingGroup;
  metadata.sourceLayout = it->second.sourceLayout;
  metadata.speakerPatchSize = it->second.speakerPatchSize;
  metadata.speakerPatch = it->second.speakerPatch;

  // If trim OUT is not set (0), use file duration
  if (metadata.trimOutSamples == 0) {
    metadata.trimOutSamples = it->second.metadata.duration_samples;
  }

  return metadata;
}

SessionGraphError TransportController::setGroupOutputBus(RoutingGroupIndex group,
                                                         const OutputBusRoute& route) {
  if (group >= m_config.numGroups || route.channelCount == 0 ||
      static_cast<uint32_t>(route.outputStart) + route.channelCount > m_config.outputChannels) {
    return SessionGraphError::InvalidParameter;
  }

  std::lock_guard<std::mutex> lock(m_audioFilesMutex);
  for (const auto& [handle, entry] : m_audioFiles) {
    (void)handle;
    if (entry.routingGroup == group && entry.metadata.num_channels > route.channelCount) {
      return SessionGraphError::InvalidParameter;
    }
  }
  const auto result =
      m_routingMatrix->setGroupOutputRoute(group, route.outputStart, route.channelCount);
  if (result != SessionGraphError::OK) {
    return result;
  }
  m_groupOutputStarts[group].store(route.outputStart, std::memory_order_release);
  m_groupOutputWidths[group].store(route.channelCount, std::memory_order_release);
  return SessionGraphError::OK;
}

std::optional<OutputBusRoute>
TransportController::getGroupOutputBus(RoutingGroupIndex group) const {
  if (group >= m_config.numGroups) {
    return std::nullopt;
  }
  return OutputBusRoute{m_groupOutputStarts[group].load(std::memory_order_acquire),
                        m_groupOutputWidths[group].load(std::memory_order_acquire)};
}

float TransportController::calculateFadeGain(float normalizedPosition, FadeCurve curve) const {
  switch (curve) {
  case FadeCurve::Linear:
    return normalizedPosition; // y = x

  case FadeCurve::EqualPower:
    return std::sin(normalizedPosition * static_cast<float>(M_PI_2)); // y = sin(x * π/2)

  case FadeCurve::Exponential:
    return normalizedPosition * normalizedPosition; // y = x²

  default:
    return normalizedPosition; // Fallback to linear
  }
}

void TransportController::setSessionDefaults(const SessionDefaults& defaults) {
  std::lock_guard<std::mutex> lock(m_audioFilesMutex);
  m_sessionDefaults = defaults;
}

SessionDefaults TransportController::getSessionDefaults() const {
  std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_audioFilesMutex));
  return m_sessionDefaults;
}

bool TransportController::isClipLooping(ClipHandle handle) const {
  const ActiveVoiceSnapshot snapshot = getActiveVoiceSnapshot();
  for (uint32_t index = 0; index < snapshot.entryCount; ++index) {
    if (snapshot.entries[index].handle == handle) {
      return snapshot.entries[index].newestVoiceLoopEnabled != 0;
    }
  }
  return false;
}

SessionGraphError TransportController::setClipVoiceMode(ClipHandle handle, VoiceMode mode) {
  // ORP127 G5: Store the policy persistently. It is read into the playback
  // context at fire time (startClip), so it takes effect on the next fire —
  // voices already playing keep the policy they were started under. This keeps
  // the operation host-neutral and lock-free on the audio thread.
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  std::lock_guard<std::mutex> lock(m_audioFilesMutex);
  auto it = m_audioFiles.find(handle);
  if (it == m_audioFiles.end()) {
    return SessionGraphError::ClipNotRegistered;
  }
  it->second.voiceMode = mode;
  return SessionGraphError::OK;
}

VoiceMode TransportController::getClipVoiceMode(ClipHandle handle) const {
  std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_audioFilesMutex));
  auto it = m_audioFiles.find(handle);
  if (it == m_audioFiles.end()) {
    return VoiceMode::Polyphonic;
  }
  return it->second.voiceMode;
}

size_t TransportController::getActiveVoiceCount(ClipHandle handle) const {
  // ORP127 G5: UI-safe voice count via the published snapshot (includes tails).
  return countActiveVoicesSnapshot(handle);
}

size_t TransportController::getTotalActiveVoiceCount() const {
  return getActiveVoiceSnapshot().totalActiveVoiceCount;
}

TransportCallbackTelemetry TransportController::getCallbackDeliveryTelemetry() const noexcept {
  for (;;) {
    const uint64_t revisionBefore =
        m_publishedCallbackTelemetry.revision.load(std::memory_order_acquire);
    if ((revisionBefore & 1u) != 0) {
      continue;
    }

    TransportCallbackTelemetry telemetry{};
    telemetry.lastAttemptedSequence =
        m_publishedCallbackTelemetry.lastAttemptedSequence.load(std::memory_order_acquire);
    telemetry.lastPostedSequence =
        m_publishedCallbackTelemetry.lastPostedSequence.load(std::memory_order_acquire);
    telemetry.cumulativeDroppedCount =
        m_publishedCallbackTelemetry.cumulativeDroppedCount.load(std::memory_order_acquire);
    telemetry.lastDroppedSequence =
        m_publishedCallbackTelemetry.lastDroppedSequence.load(std::memory_order_acquire);
    telemetry.activeVoiceSnapshotSequence =
        m_publishedCallbackTelemetry.activeVoiceSnapshotSequence.load(std::memory_order_acquire);

    const uint64_t revisionAfter =
        m_publishedCallbackTelemetry.revision.load(std::memory_order_acquire);
    if (revisionBefore == revisionAfter) {
      return telemetry;
    }
  }
}

ActiveVoiceSnapshot TransportController::getActiveVoiceSnapshot() const noexcept {
  for (;;) {
    const uint64_t revisionBefore =
        m_publishedVoiceSnapshot.revision.load(std::memory_order_acquire);
    if ((revisionBefore & 1u) != 0) {
      continue;
    }

    ActiveVoiceSnapshot snapshot{};
    snapshot.publicationSequence =
        m_publishedVoiceSnapshot.publicationSequence.load(std::memory_order_acquire);
    snapshot.entryCount =
        std::min<uint32_t>(m_publishedVoiceSnapshot.entryCount.load(std::memory_order_acquire),
                           static_cast<uint32_t>(kActiveVoiceSnapshotCapacity));
    snapshot.totalActiveVoiceCount =
        m_publishedVoiceSnapshot.totalActiveVoiceCount.load(std::memory_order_acquire);

    for (uint32_t index = 0; index < snapshot.entryCount; ++index) {
      const AtomicActiveVoiceSnapshotEntry& source = m_publishedVoiceSnapshot.entries[index];
      ActiveVoiceSnapshotEntry& destination = snapshot.entries[index];
      destination.handle = source.handle.load(std::memory_order_acquire);
      destination.activeVoiceCount = source.activeVoiceCount.load(std::memory_order_acquire);
      destination.newestVoiceId = source.newestVoiceId.load(std::memory_order_acquire);
      destination.state = static_cast<PlaybackState>(source.state.load(std::memory_order_acquire));
      destination.newestVoiceStopping = source.newestVoiceStopping.load(std::memory_order_acquire);
      destination.newestVoiceLoopEnabled =
          source.newestVoiceLoopEnabled.load(std::memory_order_acquire);
      destination.newestStartSample = source.newestStartSample.load(std::memory_order_acquire);
      destination.newestTrimInSamples = source.newestTrimInSamples.load(std::memory_order_acquire);
      destination.newestTrimOutSamples =
          source.newestTrimOutSamples.load(std::memory_order_acquire);
      destination.newestPosition.samples =
          source.newestPositionSamples.load(std::memory_order_acquire);
      destination.newestPosition.seconds =
          std::bit_cast<double>(source.newestPositionSecondsBits.load(std::memory_order_acquire));
      destination.newestPosition.beats =
          std::bit_cast<double>(source.newestPositionBeatsBits.load(std::memory_order_acquire));
    }

    const uint64_t revisionAfter =
        m_publishedVoiceSnapshot.revision.load(std::memory_order_acquire);
    if (revisionBefore == revisionAfter) {
      return snapshot;
    }
  }
}

SessionGraphError TransportController::restartClip(ClipHandle handle) {
  // ORP127 G1: Restart is now a command processed on the audio thread — the UI
  // thread no longer touches ActiveClip fields directly. Multi-voice: restarts
  // ALL voices for this handle back to trim IN.

  // Validate handle
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  // Check if clip is registered in audio files
  {
    std::lock_guard<std::mutex> lock(m_audioFilesMutex);
    auto it = m_audioFiles.find(handle);
    if (it == m_audioFiles.end()) {
      return SessionGraphError::ClipNotRegistered;
    }
  }

  // If no voice is currently active (per the published snapshot), fall back to
  // a normal start — this preserves the historical restart-or-start semantics
  // while keeping playback-context construction on the UI thread.
  if (countActiveVoicesSnapshot(handle) == 0) {
    return startClip(handle);
  }

  TransportCommand cmd{};
  cmd.type = TransportCommand::Type::Restart;
  cmd.handle = handle;
  return postCommand(cmd);
}

SessionGraphError TransportController::seekClip(ClipHandle handle, int64_t position) {
  // ORP127 G1: Seek is now a command processed on the audio thread. Multi-voice:
  // seeks ALL voices for this handle to the same (pre-clamped) position.

  // Validate handle
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  // Check if clip is registered (need to get file length for clamping)
  int64_t fileLength = 0;
  {
    std::lock_guard<std::mutex> lock(m_audioFilesMutex);
    auto it = m_audioFiles.find(handle);
    if (it == m_audioFiles.end()) {
      return SessionGraphError::ClipNotRegistered;
    }
    fileLength = it->second.metadata.duration_samples;
  }

  // No active voice → nothing to seek (matches historical NotReady contract).
  if (countActiveVoicesSnapshot(handle) == 0) {
    return SessionGraphError::NotReady;
  }

  // Clamp position to file bounds [0, fileLength] on the UI thread (audio thread
  // receives a validated absolute position).
  int64_t clampedPosition = std::clamp(position, int64_t(0), fileLength);

  TransportCommand cmd{};
  cmd.type = TransportCommand::Type::Seek;
  cmd.handle = handle;
  cmd.data.seekPosition = clampedPosition;
  return postCommand(cmd);
}

int TransportController::addCuePoint(ClipHandle handle, int64_t position, const std::string& name,
                                     uint32_t color) {
  if (handle == 0) {
    return -1; // Invalid handle
  }

  std::lock_guard<std::mutex> lock(m_audioFilesMutex);
  auto it = m_audioFiles.find(handle);
  if (it == m_audioFiles.end()) {
    return -1; // Clip not registered
  }

  // Clamp position to valid range [0, fileDuration]
  int64_t fileDuration = it->second.metadata.duration_samples;
  int64_t clampedPosition = std::clamp(position, int64_t(0), fileDuration);

  // Create cue point
  CuePoint cue;
  cue.position = clampedPosition;
  cue.name = name;
  cue.color = color;

  // Add to vector and keep sorted by position
  auto& cuePoints = it->second.cuePoints;
  auto insertPos = std::lower_bound(
      cuePoints.begin(), cuePoints.end(), cue,
      [](const CuePoint& a, const CuePoint& b) { return a.position < b.position; });

  auto insertedIt = cuePoints.insert(insertPos, cue);

  int index = static_cast<int>(std::distance(cuePoints.begin(), insertedIt));
  return index;
}

std::vector<CuePoint> TransportController::getCuePoints(ClipHandle handle) const {
  std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_audioFilesMutex));
  auto it = m_audioFiles.find(handle);
  if (it == m_audioFiles.end()) {
    return {}; // Return empty vector if clip not found
  }

  return it->second.cuePoints; // Return copy of cue points
}

SessionGraphError TransportController::seekToCuePoint(ClipHandle handle, uint32_t cueIndex) {
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  // Get cue point position
  int64_t cuePosition = 0;
  {
    std::lock_guard<std::mutex> lock(m_audioFilesMutex);
    auto it = m_audioFiles.find(handle);
    if (it == m_audioFiles.end()) {
      return SessionGraphError::ClipNotRegistered;
    }

    const auto& cuePoints = it->second.cuePoints;
    if (cueIndex >= cuePoints.size()) {
      return SessionGraphError::InvalidParameter; // Cue index out of range
    }

    cuePosition = cuePoints[cueIndex].position;
  }

  // Use existing seekClip() to perform the seek
  return seekClip(handle, cuePosition);
}

SessionGraphError TransportController::removeCuePoint(ClipHandle handle, uint32_t cueIndex) {
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  std::lock_guard<std::mutex> lock(m_audioFilesMutex);
  auto it = m_audioFiles.find(handle);
  if (it == m_audioFiles.end()) {
    return SessionGraphError::ClipNotRegistered;
  }

  auto& cuePoints = it->second.cuePoints;
  if (cueIndex >= cuePoints.size()) {
    return SessionGraphError::InvalidParameter; // Cue index out of range
  }

  // Remove cue point at index (subsequent indices shift down)
  cuePoints.erase(cuePoints.begin() + cueIndex);

  return SessionGraphError::OK;
}

// ============================================================================
// ORP121 A-01: ITU-R BS.775-3 Downmix Helper Functions
// Standard coefficients for multi-channel to stereo conversion
// ============================================================================

/// ITU-R BS.775-3 standard downmix coefficients
/// These values ensure energy-preserving downmix from surround to stereo
static constexpr float DOWNMIX_CENTER = 0.7071067811865476f;   // 1/sqrt(2) = -3dB
static constexpr float DOWNMIX_SURROUND = 0.7071067811865476f; // 1/sqrt(2) = -3dB

float TransportController::applyDownmixLeft(const float* src, size_t frame, size_t numCh) const {
  // ITU-R BS.775-3 downmix to left channel
  // Standard 5.1 layout: L=0, R=1, C=2, LFE=3, Ls=4, Rs=5
  // Formula: L_out = L + 0.707*C + 0.707*Ls

  if (numCh < 3) {
    // Mono or stereo - just return left channel (or mono)
    return src[frame * numCh];
  }

  if (numCh < 6) {
    // 3-5 channels (unusual layouts) - simple left-weighted average
    float sum = 0.0f;
    for (size_t ch = 0; ch < numCh; ch += 2) {
      sum += src[frame * numCh + ch];
    }
    return sum / static_cast<float>((numCh + 1) / 2);
  }

  // 5.1 or higher: Full ITU-R BS.775-3 downmix
  float L = src[frame * numCh + 0];  // Left
  float C = src[frame * numCh + 2];  // Center
  float Ls = src[frame * numCh + 4]; // Left Surround

  // Note: LFE (channel 3) typically excluded from stereo downmix per spec
  return L + DOWNMIX_CENTER * C + DOWNMIX_SURROUND * Ls;
}

float TransportController::applyDownmixRight(const float* src, size_t frame, size_t numCh) const {
  // ITU-R BS.775-3 downmix to right channel
  // Standard 5.1 layout: L=0, R=1, C=2, LFE=3, Ls=4, Rs=5
  // Formula: R_out = R + 0.707*C + 0.707*Rs

  if (numCh < 2) {
    // Mono - return the single channel
    return src[frame * numCh];
  }

  if (numCh < 3) {
    // Stereo - return right channel
    return src[frame * numCh + 1];
  }

  if (numCh < 6) {
    // 3-5 channels (unusual layouts) - simple right-weighted average
    float sum = 0.0f;
    for (size_t ch = 1; ch < numCh; ch += 2) {
      sum += src[frame * numCh + ch];
    }
    return sum / static_cast<float>(numCh / 2);
  }

  // 5.1 or higher: Full ITU-R BS.775-3 downmix
  float R = src[frame * numCh + 1];  // Right
  float C = src[frame * numCh + 2];  // Center
  float Rs = src[frame * numCh + 5]; // Right Surround

  // Note: LFE (channel 3) typically excluded from stereo downmix per spec
  return R + DOWNMIX_CENTER * C + DOWNMIX_SURROUND * Rs;
}

// Factory function
std::unique_ptr<ITransportController> createTransportController(core::SessionGraph* sessionGraph,
                                                                const TransportConfig& config) {
  constexpr uint32_t maxBlockFrames = 2048;
  constexpr uint32_t maxActiveVoices = 32;
  constexpr uint32_t maxSourceChannels = 8;
  const uint64_t routingLanes =
      static_cast<uint64_t>(config.maxActiveVoices) * config.maxSourceChannels;
  if (config.sampleRate == 0 || config.outputChannels == 0 || config.outputChannels > 32 ||
      config.maxBlockFrames == 0 || config.maxBlockFrames > maxBlockFrames ||
      config.maxActiveVoices == 0 || config.maxActiveVoices > maxActiveVoices ||
      config.numGroups == 0 || config.numGroups > 32 || config.maxSourceChannels == 0 ||
      config.maxSourceChannels > maxSourceChannels || routingLanes > 256) {
    return nullptr;
  }
  return std::make_unique<TransportController>(sessionGraph, config);
}

} // namespace orpheus
