// SPDX-License-Identifier: MIT
#include "../common/pinned_math.h"
#include "../common/realtime_counter.h"
#include "transport_controller.h"
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

std::pair<float, float> panGains(float pan, uint16_t numChannels) noexcept {
  const float normalized = std::clamp(pan, -1.0f, 1.0f);
  if (numChannels == 1) {
    const double angle =
        (static_cast<double>(normalized) + 1.0) * (static_cast<double>(M_PI_2) * 0.5);
    return {static_cast<float>(orpheus::pinned::cos(angle)),
            static_cast<float>(orpheus::pinned::sin(angle))};
  }

  const double leftAngle =
      static_cast<double>(std::max(normalized, 0.0f)) * static_cast<double>(M_PI_2);
  const double rightAngle =
      static_cast<double>(std::max(-normalized, 0.0f)) * static_cast<double>(M_PI_2);
  return {static_cast<float>(orpheus::pinned::cos(leftAngle)),
          static_cast<float>(orpheus::pinned::cos(rightAngle))};
}

} // namespace

TransportController::TransportController(core::SessionGraph* sessionGraph,
                                         const TransportConfig& config)
    : m_sessionGraph(sessionGraph), m_config(config), m_sampleRate(config.sampleRate),
      m_callback(nullptr), m_sourceRegistry(config.sampleRate, config, m_groupOutputWidths.data()) {
  m_fadeOutSamples =
      static_cast<size_t>((FADE_OUT_DURATION_MS / 1000.0f) * static_cast<float>(m_sampleRate));
  m_restartCrossfadeSamples = static_cast<size_t>((RESTART_CROSSFADE_DURATION_MS / 1000.0f) *
                                                  static_cast<float>(m_sampleRate));

  const float smoothingSamples =
      (CLIP_GAIN_SMOOTHING_MS / 1000.0f) * static_cast<float>(m_sampleRate);
  m_clipGainRampIncrement = smoothingSamples > 0.0f ? 1.0f / smoothingSamples : 1.0f;

  m_routingMatrix = std::make_unique<RoutingMatrix>();
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
    m_groupOutputWidths[group].store(0, std::memory_order_relaxed);
  }
  for (RoutingGroupIndex group = 0; group < m_config.numGroups; ++group) {
    m_groupOutputWidths[group].store(static_cast<uint16_t>(m_config.outputChannels),
                                     std::memory_order_relaxed);
    (void)m_routingMatrix->setGroupOutputRoute(group, 0,
                                               static_cast<uint16_t>(m_config.outputChannels));
  }
  // From here on the audio thread is the only writer of channel routes: host
  // channel-route edits through getRoutingMatrix() are rejected, so the
  // render-path route publication never contends with a control thread.
  m_routingMatrix->claimChannelRoutesForTransport();

  m_clipReadBuffers.resize(m_config.maxActiveVoices);
  for (auto& buffer : m_clipReadBuffers) {
    buffer.resize(firstRenderFrames(m_config.maxBlockFrames) * m_config.maxSourceChannels, 0.0f);
  }

  m_clipChannelBuffers.resize(routingLaneCount);
  for (auto& buffer : m_clipChannelBuffers) {
    buffer.resize(m_config.maxBlockFrames, 0.0f);
  }

  m_clipChannelPointers.resize(routingLaneCount, nullptr);

  if (m_sessionGraph) {
    m_tempoBpm.store(m_sessionGraph->tempo(), std::memory_order_relaxed);
  }
}

TransportController::~TransportController() {
  // Destruction is externally serialized against processAudio(). Stop and join
  // the worker before releasing any source held only by unread raw commands.
  m_sourceRegistry.shutdown();
  for (size_t index = 0; index < m_activeClipCount; ++index) {
    m_sourceRegistry.releaseActiveSource(m_activeClips[index].sourceLifetime);
    m_activeClips[index].sourceLifetime = nullptr;
    m_activeClips[index].source.reset();
  }
  m_activeClipCount = 0;

  uint32_t index = m_pendingCommandHead.exchange(UINT32_MAX, std::memory_order_acquire);
  std::array<uint32_t, MAX_COMMANDS> unreadIndices{};
  size_t unreadCount = 0;
  while (index != UINT32_MAX && unreadCount < MAX_COMMANDS) {
    unreadIndices[unreadCount++] = index;
    index = m_commandNodes[index].next;
  }
  // Unread commands still own their preparation side effects (loop anchors,
  // streaming primes, source lifetimes); cancellation releases them all.
  for (size_t position = 0; position < unreadCount; ++position) {
    cancelCommandNode(unreadIndices[position]);
  }
  releasePendingStartReservations();
  releasePendingSeekReservations();
}

SessionGraphError TransportController::makeStartContextLocked(
    ClipHandle handle, bool requireRegisteredSource, std::shared_ptr<ClipPlaybackContext>& context,
    SourceCommandLifetime*& sourceLifetime, StreamingClipSource*& startSource,
    StreamingClipSource::PrimeReservation& startPrime, bool& stopOthersOnPlay) {
  sourceLifetime = nullptr;
  startSource = nullptr;
  startPrime = {};
  stopOthersOnPlay = false;
  const auto releaseStartPrime = [&]() noexcept {
    if (startSource && startPrime.pageMask != 0) {
      startSource->releaseCommandPrime(startPrime);
      startSource = nullptr;
      startPrime = {};
    }
  };
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  try {
    context = std::make_shared<ClipPlaybackContext>();
    context->handle = handle;
    auto it = m_sourceRegistry.entries().find(handle);
    if (it != m_sourceRegistry.entries().end()) {
      auto& entry = it->second;
      stopOthersOnPlay = entry.stopOthersOnPlay;
      if (entry.sourceLayout == ChannelLayout::Unspecified ||
          !isValidSpeakerPatch(entry.sourceLayout, entry.speakerPatchSize, entry.speakerPatch,
                               entry.metadata.num_channels)) {
        context.reset();
        return SessionGraphError::InvalidParameter;
      }

      // Registered sources never fall back to the historical source-less test
      // default. Decode/cache preparation must succeed before this Start's
      // already-reserved command can be published.
      const SessionGraphError prepareResult =
          m_sourceRegistry.ensurePreparedSourceLocked(entry, &startPrime);
      if (prepareResult != SessionGraphError::OK) {
        releaseStartPrime();
        context.reset();
        return prepareResult;
      }
      context->source = entry.source;
      context->streamingSource = dynamic_cast<StreamingClipSource*>(entry.source.get());
      if (startPrime.pageMask != 0) {
        startSource = context->streamingSource;
      }
      context->numChannels = entry.metadata.num_channels;
      context->trimInSamples = entry.trimInSamples;
      context->stopFadeOutSeconds = entry.stopFadeOutSeconds;
      context->stopFadeOutCurve = entry.stopFadeOutCurve;
      context->fileLengthSamples = entry.metadata.duration_samples;
      context->trimOutSamples =
          entry.trimOutSamples == 0 ? entry.metadata.duration_samples : entry.trimOutSamples;
      context->muted = entry.muted;
      context->pan = entry.pan;
      const auto [leftGain, rightGain] = panGains(entry.pan, entry.metadata.num_channels);
      context->panLeftGain = leftGain;
      context->panRightGain = rightGain;
      context->playbackRate = entry.playbackRate;
      context->playDelayFrames =
          static_cast<int64_t>(entry.playDelaySeconds * static_cast<double>(m_sampleRate));
      context->fadeInSeconds = entry.fadeInSeconds;
      context->fadeOutSeconds = entry.fadeOutSeconds;
      context->fadeInCurve = entry.fadeInCurve;
      context->fadeOutCurve = entry.fadeOutCurve;
      context->gainDb = entry.gainDb;
      context->gainLinear = orpheus::pinned::pow10(entry.gainDb / 20.0f);
      context->loopEnabled = entry.loopEnabled;
      context->segmentCount = entry.segmentCount;
      context->segments = entry.segments;
      if (context->dspProcessor.prepare(entry.dsp, m_sampleRate, entry.metadata.num_channels) !=
          ClipDspValidationError::OK) {
        releaseStartPrime();
        context.reset();
        return SessionGraphError::InvalidParameter;
      }
      context->routingGroup = entry.routingGroup;
      context->voiceMode = entry.voiceMode;
      if (!entry.commandLifetime) {
        releaseStartPrime();
        context.reset();
        return SessionGraphError::InternalError;
      }
      context->sourceLifetime = entry.commandLifetime.get();
      m_sourceRegistry.retainSourceCommand(entry.commandLifetime.get());
      sourceLifetime = entry.commandLifetime.get();
      return SessionGraphError::OK;
    }

    if (requireRegisteredSource) {
      context.reset();
      return SessionGraphError::ClipNotRegistered;
    }

    // Preserve the historical source-less test/default start contract.
    context->source = nullptr;
    context->numChannels = 2;
    context->stopFadeOutSeconds = 0.01;
    context->stopFadeOutCurve = FadeCurve::Linear;
    context->trimInSamples = 0;
    context->trimOutSamples = 48000 * 60;
    context->muted = false;
    context->pan = 0.0f;
    const auto [leftGain, rightGain] = panGains(0.0f, 2);
    context->panLeftGain = leftGain;
    context->panRightGain = rightGain;
    context->playbackRate = 1.0;
    context->playDelayFrames = 0;
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
    context->dspProcessor.prepare({}, m_sampleRate, context->numChannels);
    return SessionGraphError::OK;
  } catch (const std::bad_alloc&) {
    releaseStartPrime();
    context.reset();
    sourceLifetime = nullptr;
    return SessionGraphError::InternalError;
  }
}

SessionGraphError TransportController::startClip(ClipHandle handle, StartRequestTag requestTag) {
  if (handle == 0)
    return SessionGraphError::InvalidHandle;
  std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
  const auto entry = m_sourceRegistry.entries().find(handle);
  if (entry != m_sourceRegistry.entries().end() &&
      (entry->second.sourceLayout == ChannelLayout::Unspecified ||
       !isValidSpeakerPatch(entry->second.sourceLayout, entry->second.speakerPatchSize,
                            entry->second.speakerPatch, entry->second.metadata.num_channels)))
    return SessionGraphError::InvalidParameter;
  const uint32_t index = acquireCommandNode();
  if (index == UINT32_MAX) {
    return SessionGraphError::NotReady;
  }
  CommandReservation reservation(*this, index);
  TransportCommand& cmd = m_commandNodes[index].command;
  cmd = TransportCommand{};
  cmd.type = TransportCommand::Type::Start;
  cmd.handle = handle;
  cmd.requestTag = requestTag;
  std::shared_ptr<ClipPlaybackContext> context;
  SourceCommandLifetime* lifetime = nullptr;
  StreamingClipSource* source = nullptr;
  StreamingClipSource::PrimeReservation prime{};
  bool stopOthers = false;
  const SessionGraphError result =
      makeStartContextLocked(handle, false, context, lifetime, source, prime, stopOthers);
  if (result != SessionGraphError::OK) {
    if (source != nullptr && prime.pageMask != 0)
      source->releaseCommandPrime(prime);
    if (lifetime != nullptr)
      m_sourceRegistry.releaseSourceCommand(lifetime);
    return result;
  }
  cmd.type = stopOthers ? TransportCommand::Type::StartWithStopOthers : cmd.type;
  cmd.startContext = std::move(context);
  cmd.sourceLifetime = lifetime;
  cmd.startSource = source;
  cmd.startPrime = prime;
  const SessionGraphError published = reservation.publish();
  if (published == SessionGraphError::OK)
    reservation.disarm();
  return published;
}

SessionGraphError TransportController::startClipWithGroupChoke(ClipHandle handle,
                                                               StartRequestTag requestTag) {
  if (handle == 0)
    return SessionGraphError::InvalidHandle;
  std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
  const auto entry = m_sourceRegistry.entries().find(handle);
  if (entry == m_sourceRegistry.entries().end())
    return SessionGraphError::ClipNotRegistered;
  if (entry->second.sourceLayout == ChannelLayout::Unspecified ||
      !isValidSpeakerPatch(entry->second.sourceLayout, entry->second.speakerPatchSize,
                           entry->second.speakerPatch, entry->second.metadata.num_channels))
    return SessionGraphError::InvalidParameter;
  const uint32_t index = acquireCommandNode();
  if (index == UINT32_MAX) {
    return SessionGraphError::NotReady;
  }
  CommandReservation reservation(*this, index);
  TransportCommand& cmd = m_commandNodes[index].command;
  cmd = TransportCommand{};
  cmd.type = TransportCommand::Type::StartWithGroupChoke;
  cmd.handle = handle;
  cmd.requestTag = requestTag;
  std::shared_ptr<ClipPlaybackContext> context;
  SourceCommandLifetime* lifetime = nullptr;
  StreamingClipSource* source = nullptr;
  StreamingClipSource::PrimeReservation prime{};
  bool stopOthers = false;
  const SessionGraphError result =
      makeStartContextLocked(handle, true, context, lifetime, source, prime, stopOthers);
  if (result != SessionGraphError::OK) {
    if (source != nullptr && prime.pageMask != 0)
      source->releaseCommandPrime(prime);
    if (lifetime != nullptr)
      m_sourceRegistry.releaseSourceCommand(lifetime);
    return result;
  }
  cmd.startContext = std::move(context);
  cmd.sourceLifetime = lifetime;
  cmd.startSource = source;
  cmd.startPrime = prime;
  const SessionGraphError published = reservation.publish();
  if (published == SessionGraphError::OK)
    reservation.disarm();
  return published;
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
  // so output goes silent on the next block. Routed through bounded MPSC
  // admission; the audio-side eviction remains realtime-safe.
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

void TransportController::setCallback(ITransportCallback* callback) {
  m_callback = callback;
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
    voice.loopEnabled = looping;
    voice.trimInSamples = trimIn;
    voice.trimOutSamples = trimOut;
    voice.currentSample = currentSample;
    voice.sourcePosition = static_cast<double>(currentSample);
    return true;
  }
  return false;
}

uint32_t TransportController::acquireCommandNode() noexcept {
  incrementIngressCounter(m_ingressAttempted);
  for (uint32_t index = 0; index < MAX_COMMANDS; ++index) {
    uint8_t expected = 0;
    if (m_commandNodes[index].ownership.compare_exchange_strong(
            expected, uint8_t{1}, std::memory_order_acquire, std::memory_order_relaxed)) {
      // Exclusive producer reuse may reclaim old payloads off the audio thread.
      m_commandNodes[index].command = TransportCommand{};
      return index;
    }
  }
  incrementIngressCounter(m_ingressSlotUnavailable);
  return UINT32_MAX;
}

SessionGraphError TransportController::publishCommandNode(uint32_t index) noexcept {
  CommandNode& node = m_commandNodes[index];
  uint32_t head = m_pendingCommandHead.load(std::memory_order_relaxed);
  for (uint32_t attempt = 0; attempt < 32; ++attempt) {
    node.next = head;
    if (m_commandPublicationTestHook != nullptr)
      m_commandPublicationTestHook(m_commandPublicationTestState, index, attempt, false);
    if (m_pendingCommandHead.compare_exchange_strong(head, index, std::memory_order_release,
                                                     std::memory_order_relaxed)) {
      if (m_commandPublicationTestHook != nullptr)
        m_commandPublicationTestHook(m_commandPublicationTestState, index, attempt, true);
      incrementIngressCounter(m_ingressAdmitted);
      return SessionGraphError::OK;
    }
  }
  incrementIngressCounter(m_ingressPublicationContention);
  return SessionGraphError::NotReady;
}

void TransportController::cancelCommandNode(uint32_t index) noexcept {
  CommandNode& node = m_commandNodes[index];
  TransportCommand& command = node.command;
  // Cancellation owns all preparation side effects.  In particular, merely
  // assigning a default command would leak streaming pins and loop anchors.
  if (command.loopAnchorSource != nullptr) {
    command.loopAnchorSource->rollbackLoopAnchorTransition(command.loopAnchorTransition);
    command.loopAnchorSource = nullptr;
    command.loopAnchorTransition = {};
  }
  if (command.seekSource != nullptr && command.seekPrime.pageMask != 0) {
    command.seekSource->releaseCommandPrime(command.seekPrime);
    command.seekSource = nullptr;
    command.seekPrime = {};
  }
  if (command.startSource != nullptr && command.startPrime.pageMask != 0) {
    command.startSource->releaseCommandPrime(command.startPrime);
    command.startSource = nullptr;
    command.startPrime = {};
  }
  if (command.sourceLifetime != nullptr) {
    m_sourceRegistry.releaseSourceCommand(command.sourceLifetime);
    command.sourceLifetime = nullptr;
  }
  node.next = UINT32_MAX;
  // Keep shared_ptr payload retained until a later producer can destroy it.
  node.ownership.store(0, std::memory_order_release);
}

SessionGraphError TransportController::postCommand(const TransportCommand& command) {
  const uint32_t index = acquireCommandNode();
  if (index == UINT32_MAX)
    return SessionGraphError::NotReady;
  CommandReservation reservation(*this, index);
  m_commandNodes[index].command = command;
  return reservation.publish();
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
        m_callback->onClipStarted(event.handle, event.requestTag, event.voiceId, event.position);
        break;
      case TransportEventType::ClipStopped:
        m_callback->onClipStopped(event.handle, event.requestTag, event.voiceId, event.position);
        break;
      case TransportEventType::ClipLooped:
        m_callback->onClipLooped(event.handle, event.requestTag, event.voiceId, event.position);
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
        m_callback->onActiveClipLimitReached(event.handle, event.requestTag, event.position);
        break;
      }
    }
    readIdx = (readIdx + 1) & (CALLBACK_QUEUE_SIZE - 1);
  }

  m_callbackReadIndex.store(readIdx, std::memory_order_release);
}

SessionGraphError TransportController::registerClipAudio(ClipHandle handle,
                                                         const std::string& file_path) {
  return m_sourceRegistry.registerClipAudioImpl(handle, file_path, nullptr);
}

SessionGraphError TransportController::registerClipAudioWithMetadata(ClipHandle handle,
                                                                     const std::string& file_path,
                                                                     const ClipMetadata& metadata) {
  return m_sourceRegistry.registerClipAudioImpl(handle, file_path, &metadata);
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

  std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
  auto it = m_sourceRegistry.entries().find(handle);
  if (it == m_sourceRegistry.entries().end()) {
    return SessionGraphError::ClipNotRegistered;
  }
  return m_sourceRegistry.ensurePreparedSourceLocked(it->second);
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

  std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
  auto it = m_sourceRegistry.entries().find(handle);
  if (it == m_sourceRegistry.entries().end()) {
    // Idempotent: releasing an unregistered handle is a no-op success so hosts
    // can call it unconditionally on slot teardown.
    return SessionGraphError::OK;
  }
  if (it->second.commandLifetime &&
      (it->second.commandLifetime->unread.load(std::memory_order_acquire) != 0 ||
       it->second.commandLifetime->active_voices.load(std::memory_order_acquire) != 0)) {
    return SessionGraphError::NotReady;
  }
  if (auto streaming = std::dynamic_pointer_cast<StreamingClipSource>(it->second.source);
      streaming && streaming->hasPendingCommandPrimes()) {
    return SessionGraphError::NotReady;
  }
  m_sourceRegistry.entries().erase(it);
  return SessionGraphError::OK;
}

SessionGraphError TransportController::updateClipTrimPoints(ClipHandle handle,
                                                            int64_t trimInSamples,
                                                            int64_t trimOutSamples) {
  if (handle == 0)
    return SessionGraphError::InvalidHandle;
  std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
  auto it = m_sourceRegistry.entries().find(handle);
  if (it == m_sourceRegistry.entries().end())
    return SessionGraphError::ClipNotRegistered;
  const int64_t duration = it->second.metadata.duration_samples;
  for (uint32_t i = 0; i < it->second.segmentCount; ++i)
    if (it->second.segments[i].startSample < trimInSamples ||
        it->second.segments[i].endSample > trimOutSamples)
      return SessionGraphError::InvalidClipTrimPoints;
  if (trimInSamples < 0 || trimInSamples >= duration || trimOutSamples <= trimInSamples ||
      trimOutSamples > duration)
    return SessionGraphError::InvalidClipTrimPoints;

  const uint32_t index = acquireCommandNode();
  if (index == UINT32_MAX) {
    return SessionGraphError::NotReady;
  }
  CommandReservation reservation(*this, index);
  TransportCommand& cmd = m_commandNodes[index].command;
  cmd = TransportCommand{};
  cmd.type = TransportCommand::Type::UpdateTrim;
  cmd.handle = handle;
  cmd.data.trim = {trimInSamples, trimOutSamples};
  auto* streaming =
      it->second.source ? dynamic_cast<StreamingClipSource*>(it->second.source.get()) : nullptr;
  if (streaming != nullptr) {
    cmd.sourceLifetime = it->second.commandLifetime.get();
    if (cmd.sourceLifetime == nullptr) {
      return SessionGraphError::InternalError;
    }
    m_sourceRegistry.retainSourceCommand(cmd.sourceLifetime);
    const auto result = streaming->prepareLoopAnchorTransition(
        programStartSample(trimInSamples, it->second.segmentCount, it->second.segments),
        firstRenderFrames(m_config.maxBlockFrames), it->second.loopEnabled,
        cmd.loopAnchorTransition);
    if (result != SessionGraphError::OK) {
      cmd.loopAnchorSource = nullptr;
      return result;
    }
    cmd.loopAnchorSource = streaming;
  }
  const auto result = reservation.publish();
  if (result != SessionGraphError::OK)
    return result;
  reservation.disarm();
  it->second.trimInSamples = trimInSamples;
  it->second.trimOutSamples = trimOutSamples;
  return SessionGraphError::OK;
}

SessionGraphError TransportController::updateClipFades(ClipHandle handle, double fadeInSeconds,
                                                       double fadeOutSeconds, FadeCurve fadeInCurve,
                                                       FadeCurve fadeOutCurve) {
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  // Registry state includes every previously admitted trim, even if the
  // consumer has not rendered it yet. Validate and publish in one transaction.
  std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
  auto it = m_sourceRegistry.entries().find(handle);
  if (it == m_sourceRegistry.entries().end())
    return SessionGraphError::ClipNotRegistered;
  const auto& entry = it->second;
  const int64_t trimOut =
      entry.trimOutSamples == 0 ? entry.metadata.duration_samples : entry.trimOutSamples;
  const double clipDurationSeconds =
      static_cast<double>(trimOut - entry.trimInSamples) / static_cast<double>(m_sampleRate);
  if (!std::isfinite(fadeInSeconds) || !std::isfinite(fadeOutSeconds) || fadeInSeconds < 0.0 ||
      fadeInSeconds > clipDurationSeconds || fadeOutSeconds < 0.0 ||
      fadeOutSeconds > clipDurationSeconds)
    return SessionGraphError::InvalidFadeDuration;
  const uint32_t index = acquireCommandNode();
  if (index == UINT32_MAX) {
    return SessionGraphError::NotReady;
  }
  CommandReservation reservation(*this, index);
  TransportCommand& cmd = m_commandNodes[index].command;
  cmd = TransportCommand{};
  cmd.type = TransportCommand::Type::UpdateFade;
  cmd.handle = handle;
  cmd.data.fade.inSeconds = fadeInSeconds;
  cmd.data.fade.outSeconds = fadeOutSeconds;
  cmd.data.fade.inCurve = fadeInCurve;
  cmd.data.fade.outCurve = fadeOutCurve;
  const SessionGraphError result = reservation.publish();
  if (result != SessionGraphError::OK)
    return result;
  reservation.disarm();
  it->second.fadeInSeconds = fadeInSeconds;
  it->second.fadeOutSeconds = fadeOutSeconds;
  it->second.fadeInCurve = fadeInCurve;
  it->second.fadeOutCurve = fadeOutCurve;
  it->second.stopFadeOutSeconds = fadeOutSeconds;
  it->second.stopFadeOutCurve = fadeOutCurve;
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

  std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
  auto it = m_sourceRegistry.entries().find(handle);
  if (it == m_sourceRegistry.entries().end())
    return SessionGraphError::ClipNotRegistered;
  const uint32_t index = acquireCommandNode();
  if (index == UINT32_MAX) {
    return SessionGraphError::NotReady;
  }
  CommandReservation reservation(*this, index);
  TransportCommand& cmd = m_commandNodes[index].command;
  cmd = TransportCommand{};
  cmd.type = TransportCommand::Type::UpdateGain;
  cmd.handle = handle;
  cmd.data.gain.db = gainDb;
  cmd.data.gain.linear = orpheus::pinned::pow10(gainDb / 20.0f);
  const SessionGraphError result = reservation.publish();
  if (result != SessionGraphError::OK)
    return result;
  reservation.disarm();
  it->second.gainDb = gainDb;
  return SessionGraphError::OK;
}

SessionGraphError TransportController::setClipLoopMode(ClipHandle handle, bool shouldLoop) {
  if (handle == 0)
    return SessionGraphError::InvalidHandle;
  std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
  auto it = m_sourceRegistry.entries().find(handle);
  if (it == m_sourceRegistry.entries().end())
    return SessionGraphError::ClipNotRegistered;
  const uint32_t index = acquireCommandNode();
  if (index == UINT32_MAX) {
    return SessionGraphError::NotReady;
  }
  CommandReservation reservation(*this, index);
  TransportCommand& cmd = m_commandNodes[index].command;
  cmd = TransportCommand{};
  cmd.type = TransportCommand::Type::UpdateLoop;
  cmd.handle = handle;
  cmd.data.booleanValue = shouldLoop;
  auto* streaming =
      it->second.source ? dynamic_cast<StreamingClipSource*>(it->second.source.get()) : nullptr;
  if (streaming != nullptr) {
    cmd.sourceLifetime = it->second.commandLifetime.get();
    if (!cmd.sourceLifetime) {
      return SessionGraphError::InternalError;
    }
    m_sourceRegistry.retainSourceCommand(cmd.sourceLifetime);
    const auto result = streaming->prepareLoopAnchorTransition(
        programStartSample(it->second.trimInSamples, it->second.segmentCount, it->second.segments),
        firstRenderFrames(m_config.maxBlockFrames), shouldLoop, cmd.loopAnchorTransition);
    if (result != SessionGraphError::OK) {
      cmd.loopAnchorSource = nullptr;
      return result;
    }
    cmd.loopAnchorSource = streaming;
  }
  const auto result = reservation.publish();
  if (result != SessionGraphError::OK)
    return result;
  reservation.disarm();
  it->second.loopEnabled = shouldLoop;
  return SessionGraphError::OK;
}

SessionGraphError TransportController::setClipStopOthersMode(ClipHandle handle, bool enabled) {
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  // Store mode persistently in the registry entry
  std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
  auto it = m_sourceRegistry.entries().find(handle);
  if (it == m_sourceRegistry.entries().end()) {
    return SessionGraphError::ClipNotRegistered;
  }

  it->second.stopOthersOnPlay = enabled;
  return SessionGraphError::OK;
}

SessionGraphError TransportController::updateClipMetadata(ClipHandle handle,
                                                          const ClipMetadata& metadata) {
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
  auto it = m_sourceRegistry.entries().find(handle);
  if (it == m_sourceRegistry.entries().end())
    return SessionGraphError::ClipNotRegistered;
  const uint16_t fileChannels = it->second.metadata.num_channels;
  int64_t trimOut = 0;
  const auto validation =
      m_sourceRegistry.validateClipMetadataFields(it->second, metadata, trimOut);
  if (validation != SessionGraphError::OK)
    return validation;

  ClipDspProcessor preparedDsp;

  const int64_t fadeInSampleCount =
      static_cast<int64_t>(metadata.fadeInSeconds * static_cast<double>(m_sampleRate));
  const int64_t fadeOutSampleCount =
      static_cast<int64_t>(metadata.fadeOutSeconds * static_cast<double>(m_sampleRate));
  const int64_t stopFadeOutSampleCount =
      static_cast<int64_t>(metadata.stopFadeOutSeconds * static_cast<double>(m_sampleRate));
  const int64_t playDelayFrames =
      static_cast<int64_t>(metadata.playDelaySeconds * static_cast<double>(m_sampleRate));
  const auto [panLeftGain, panRightGain] = panGains(metadata.pan, fileChannels);

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
  cmd.data.metadata.stopFadeOutSamples = stopFadeOutSampleCount;
  cmd.data.metadata.stopFadeOutSeconds = metadata.stopFadeOutSeconds;
  cmd.data.metadata.stopFadeOutCurve = metadata.stopFadeOutCurve;
  cmd.data.metadata.loopEnabled = metadata.loopEnabled;
  cmd.data.metadata.gainDb = metadata.gainDb;
  cmd.data.metadata.gainLinear = orpheus::pinned::pow10(metadata.gainDb / 20.0f);
  cmd.data.metadata.muted = metadata.muted;
  cmd.data.metadata.pan = metadata.pan;
  cmd.data.metadata.panLeftGain = panLeftGain;
  cmd.data.metadata.panRightGain = panRightGain;
  cmd.data.metadata.playbackRate = metadata.playbackRate;
  cmd.data.metadata.playDelayFrames = playDelayFrames;
  cmd.data.metadata.routingGroup = metadata.routingGroup;
  cmd.data.metadata.segmentCount = metadata.segmentCount;
  cmd.data.metadata.segments = metadata.segments;

  const uint32_t nodeIndex = acquireCommandNode();
  if (nodeIndex == UINT32_MAX) {
    return SessionGraphError::NotReady;
  }
  CommandReservation reservation(*this, nodeIndex);
  TransportCommand& admitted = m_commandNodes[nodeIndex].command;
  admitted = cmd;
  if (preparedDsp.prepare(metadata.dsp, m_sampleRate, fileChannels) != ClipDspValidationError::OK) {
    return SessionGraphError::InvalidParameter;
  }
  admitted.dspProcessor = preparedDsp;
  StreamingClipSource* streaming = nullptr;
  SourceCommandLifetime* lifetime = nullptr;
  StreamingClipSource::LoopAnchorTransition transition{};
  if (it->second.source)
    streaming = dynamic_cast<StreamingClipSource*>(it->second.source.get());
  if (streaming != nullptr) {
    const SessionGraphError prepareResult = streaming->prepareLoopAnchorTransition(
        programStartSample(metadata.trimInSamples, metadata.segmentCount, metadata.segments),
        firstRenderFrames(m_config.maxBlockFrames), metadata.loopEnabled, transition);
    if (prepareResult != SessionGraphError::OK) {
      return prepareResult;
    }
    lifetime = it->second.commandLifetime.get();
    if (lifetime == nullptr) {
      streaming->rollbackLoopAnchorTransition(transition);
      return SessionGraphError::InternalError;
    }
    admitted.loopAnchorSource = streaming;
    admitted.loopAnchorTransition = transition;
    admitted.sourceLifetime = lifetime;
    m_sourceRegistry.retainSourceCommand(lifetime);
  }

  const SessionGraphError postResult = reservation.publish();
  if (postResult != SessionGraphError::OK)
    return postResult;
  reservation.disarm();

  ClipSourceRegistry::applyClipMetadata(it->second, metadata, trimOut);
  return SessionGraphError::OK;
}

SessionGraphError TransportController::setGroupOutputBus(RoutingGroupIndex group,
                                                         const OutputBusRoute& route) {
  if (group >= m_config.numGroups || route.channelCount == 0 ||
      static_cast<uint32_t>(route.outputStart) + route.channelCount > m_config.outputChannels) {
    return SessionGraphError::InvalidParameter;
  }

  std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
  for (const auto& [handle, entry] : m_sourceRegistry.entries()) {
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
  m_groupOutputWidths[group].store(route.channelCount, std::memory_order_release);
  return SessionGraphError::OK;
}

void TransportController::setSessionDefaults(const SessionDefaults& defaults) {
  SessionDefaults sanitized = defaults;
  if (!std::isfinite(sanitized.stopFadeOutSeconds) || sanitized.stopFadeOutSeconds < 0.0) {
    sanitized.stopFadeOutSeconds = 0.01;
  }
  sanitized.pan = std::isfinite(sanitized.pan) ? std::clamp(sanitized.pan, -1.0f, 1.0f) : 0.0f;
  sanitized.playbackRate =
      std::isfinite(sanitized.playbackRate) ? std::clamp(sanitized.playbackRate, 0.25, 4.0) : 1.0;
  sanitized.playDelaySeconds = std::isfinite(sanitized.playDelaySeconds)
                                   ? std::clamp(sanitized.playDelaySeconds, 0.0, 99.9)
                                   : 0.0;

  m_sourceRegistry.setSessionDefaults(sanitized);
}

SessionGraphError TransportController::setClipVoiceMode(ClipHandle handle, VoiceMode mode) {
  // ORP127 G5: Store the policy persistently. It is read into the playback
  // context at fire time (startClip), so it takes effect on the next fire —
  // voices already playing keep the policy they were started under. This keeps
  // the operation host-neutral and lock-free on the audio thread.
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
  auto it = m_sourceRegistry.entries().find(handle);
  if (it == m_sourceRegistry.entries().end()) {
    return SessionGraphError::ClipNotRegistered;
  }
  it->second.voiceMode = mode;
  return SessionGraphError::OK;
}

SessionGraphError TransportController::restartClip(ClipHandle handle) {
  if (handle == 0)
    return SessionGraphError::InvalidHandle;
  std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
  const auto entry = m_sourceRegistry.entries().find(handle);
  if (entry == m_sourceRegistry.entries().end())
    return SessionGraphError::ClipNotRegistered;
  const bool needsStart = countActiveVoicesSnapshot(handle) == 0;
  if (needsStart &&
      (entry->second.sourceLayout == ChannelLayout::Unspecified ||
       !isValidSpeakerPatch(entry->second.sourceLayout, entry->second.speakerPatchSize,
                            entry->second.speakerPatch, entry->second.metadata.num_channels)))
    return SessionGraphError::InvalidParameter;
  const uint32_t index = acquireCommandNode();
  if (index == UINT32_MAX)
    return SessionGraphError::NotReady;
  CommandReservation reservation(*this, index);
  auto& command = m_commandNodes[index].command;
  command.handle = handle;
  command.type = TransportCommand::Type::Restart;
  if (needsStart) {
    bool stopOthers = false;
    const auto result =
        makeStartContextLocked(handle, true, command.startContext, command.sourceLifetime,
                               command.startSource, command.startPrime, stopOthers);
    if (result != SessionGraphError::OK)
      return result;
    command.type =
        stopOthers ? TransportCommand::Type::StartWithStopOthers : TransportCommand::Type::Start;
  } else {
    // A live restart renders from the program start on its next block; prime that window so the
    // restart never depends on the stream worker. A failed prime publishes the Restart unprimed.
    auto& registered = entry->second;
    if (auto* streaming = dynamic_cast<StreamingClipSource*>(registered.source.get());
        streaming != nullptr && registered.commandLifetime) {
      StreamingClipSource::PrimeReservation prime{};
      if (m_sourceRegistry.ensurePreparedSourceLocked(registered, &prime) ==
          SessionGraphError::OK) {
        command.sourceLifetime = registered.commandLifetime.get();
        m_sourceRegistry.retainSourceCommand(command.sourceLifetime);
        if (prime.pageMask != 0) {
          command.startSource = streaming;
          command.startPrime = prime;
        }
      }
    }
  }
  return reservation.publish();
}

SessionGraphError TransportController::seekClip(ClipHandle handle, int64_t position) {
  // Multi-voice seek remains one FIFO command applied to every active voice.
  // Only its control-thread cache/lifetime preparation changes.
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
  auto it = m_sourceRegistry.entries().find(handle);
  if (it == m_sourceRegistry.entries().end()) {
    return SessionGraphError::ClipNotRegistered;
  }
  ClipSourceRegistry::Entry& entry = it->second;
  const int64_t fileLength = entry.metadata.duration_samples;

  // A published active voice is the authoritative control-thread admission
  // signal; an unread Start has not materialized a seekable voice yet.
  if (countActiveVoicesSnapshot(handle) == 0) {
    return SessionGraphError::NotReady;
  }
  const int64_t clampedPosition = std::clamp(position, int64_t{0}, fileLength);

  // Publication is a bounded MPSC admission operation; preparation above
  // remains serialized by the registry transaction.

  const uint32_t nodeIndex = acquireCommandNode();
  if (nodeIndex == UINT32_MAX) {
    return SessionGraphError::NotReady;
  }
  CommandReservation reservation(*this, nodeIndex);
  TransportCommand& cmd = m_commandNodes[nodeIndex].command;
  cmd.type = TransportCommand::Type::Seek;
  cmd.handle = handle;
  cmd.data.seekPosition = clampedPosition;
  if (!entry.commandLifetime)
    return SessionGraphError::InternalError;
  m_sourceRegistry.retainSourceCommand(entry.commandLifetime.get());
  cmd.sourceLifetime = entry.commandLifetime.get();
  if (auto* streaming = dynamic_cast<StreamingClipSource*>(entry.source.get())) {
    cmd.seekSource = streaming;
    const size_t firstRender = firstRenderFrames(m_config.maxBlockFrames);

    const auto prime = [&](int64_t start) {
      return streaming->primeForCommand(start, firstRender, cmd.seekPrime);
    };
    // The renderer clamps a non-segment seek below trim-IN before its first
    // source read. Prime that effective position, not the discarded request.
    const int64_t effectiveFirstRenderPosition =
        entry.segmentCount == 0 ? std::max(clampedPosition, entry.trimInSamples) : clampedPosition;
    SessionGraphError primeResult = prime(effectiveFirstRenderPosition);
    if (primeResult == SessionGraphError::OK && entry.segmentCount != 0) {
      for (uint32_t index = 0; index < entry.segmentCount; ++index) {
        primeResult = prime(entry.segments[index].startSample);
        if (primeResult != SessionGraphError::OK) {
          break;
        }
      }
    } else if (primeResult == SessionGraphError::OK && entry.loopEnabled) {
      const int64_t trimIn = entry.trimInSamples;
      const int64_t trimOut = entry.trimOutSamples == 0 ? fileLength : entry.trimOutSamples;
      const int64_t rangeFrames = static_cast<int64_t>(firstRender);
      // A position at/past OUT wraps before its first read; otherwise this
      // accepted command could enter trim-IN on a non-resident page.
      if (trimOut > trimIn &&
          (clampedPosition >= trimOut || trimOut - clampedPosition <= rangeFrames)) {
        primeResult = prime(trimIn);
      }
    }

    if (primeResult != SessionGraphError::OK) {
      return primeResult;
    }
  } else if (!entry.source) {
    return SessionGraphError::NotReady;
  }

  return reservation.publish();
}

int TransportController::addCuePoint(ClipHandle handle, int64_t position, const std::string& name,
                                     uint32_t color) {
  if (handle == 0) {
    return -1; // Invalid handle
  }

  std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
  auto it = m_sourceRegistry.entries().find(handle);
  if (it == m_sourceRegistry.entries().end()) {
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

SessionGraphError TransportController::seekToCuePoint(ClipHandle handle, uint32_t cueIndex) {
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  // Get cue point position
  int64_t cuePosition = 0;
  {
    std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
    auto it = m_sourceRegistry.entries().find(handle);
    if (it == m_sourceRegistry.entries().end()) {
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

  std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
  auto it = m_sourceRegistry.entries().find(handle);
  if (it == m_sourceRegistry.entries().end()) {
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

// Factory function
std::unique_ptr<ITransportController> createTransportController(core::SessionGraph* sessionGraph,
                                                                const TransportConfig& config) {
  constexpr uint32_t maxBlockFrames = 2048;
  constexpr uint32_t maxActiveVoices = 32;
  constexpr uint32_t maxSourceChannels = 8;
  const uint64_t routingLanes =
      static_cast<uint64_t>(config.maxActiveVoices) * config.maxSourceChannels;
  if (config.sampleRate == 0 || config.outputChannels == 0 ||
      config.outputChannels > kRoutingMaxOutputs || config.maxBlockFrames == 0 ||
      config.maxBlockFrames > maxBlockFrames || config.maxActiveVoices == 0 ||
      config.maxActiveVoices > maxActiveVoices || config.numGroups == 0 || config.numGroups > 32 ||
      config.maxSourceChannels == 0 || config.maxSourceChannels > maxSourceChannels ||
      routingLanes > 256) {
    return nullptr;
  }
  return std::make_unique<TransportController>(sessionGraph, config);
}

} // namespace orpheus
