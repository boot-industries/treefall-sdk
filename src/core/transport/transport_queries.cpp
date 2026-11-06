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

TransportConfig TransportController::getRenderConfig() const noexcept {
  return m_config;
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

TransportCommandIngressTelemetry TransportController::getCommandIngressTelemetry() const noexcept {
  TransportCommandIngressTelemetry telemetry{};
  telemetry.supported = 1;
  telemetry.capacity = kTransportCommandIngressCapacity;
  telemetry.maxConcurrentCalls = kTransportCommandIngressMaxConcurrentCalls;
  const auto clamp = [](uint64_t value) noexcept {
    return static_cast<uint32_t>(std::min<uint64_t>(value, UINT32_MAX));
  };
  telemetry.attemptedCount = clamp(m_ingressAttempted.load(std::memory_order_relaxed));
  telemetry.admittedCount = clamp(m_ingressAdmitted.load(std::memory_order_relaxed));
  telemetry.slotUnavailableCount = clamp(m_ingressSlotUnavailable.load(std::memory_order_relaxed));
  telemetry.publicationContentionCount =
      clamp(m_ingressPublicationContention.load(std::memory_order_relaxed));
  telemetry.preparationRejectedCount =
      clamp(m_ingressPreparationRejected.load(std::memory_order_relaxed));
  telemetry.processedCount = clamp(m_ingressProcessed.load(std::memory_order_relaxed));
  return telemetry;
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
  {
    std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
    auto it = m_sourceRegistry.entries().find(handle);
    if (it == m_sourceRegistry.entries().end()) {
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

bool TransportController::getClipStopOthersMode(ClipHandle handle) const {
  std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
  auto it = m_sourceRegistry.entries().find(handle);
  if (it == m_sourceRegistry.entries().end()) {
    return false;
  }

  return it->second.stopOthersOnPlay;
}

std::optional<ClipMetadata> TransportController::getClipMetadata(ClipHandle handle) const {
  if (handle == 0) {
    return std::nullopt;
  }

  std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
  auto it = m_sourceRegistry.entries().find(handle);
  if (it == m_sourceRegistry.entries().end()) {
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
  metadata.stopFadeOutSeconds = it->second.stopFadeOutSeconds;
  metadata.stopFadeOutCurve = it->second.stopFadeOutCurve;
  metadata.loopEnabled = it->second.loopEnabled;
  metadata.segmentCount = it->second.segmentCount;
  metadata.segments = it->second.segments;
  metadata.stopOthersOnPlay = it->second.stopOthersOnPlay;
  metadata.voiceMode = it->second.voiceMode; // ORP127 G5
  metadata.gainDb = it->second.gainDb;
  metadata.muted = it->second.muted;
  metadata.pan = it->second.pan;
  metadata.playbackRate = it->second.playbackRate;
  metadata.playDelaySeconds = it->second.playDelaySeconds;
  metadata.routingGroup = it->second.routingGroup;
  metadata.sourceLayout = it->second.sourceLayout;
  metadata.speakerPatchSize = it->second.speakerPatchSize;
  metadata.speakerPatch = it->second.speakerPatch;
  metadata.dsp = it->second.dsp;

  // If trim OUT is not set (0), use file duration
  if (metadata.trimOutSamples == 0) {
    metadata.trimOutSamples = it->second.metadata.duration_samples;
  }

  return metadata;
}

std::optional<OutputBusRoute>
TransportController::getGroupOutputBus(RoutingGroupIndex group) const {
  if (group >= m_config.numGroups) {
    return std::nullopt;
  }
  // The matrix's committed geometry, so a route the host set through
  // getRoutingMatrix() reads back as faithfully as one set by setGroupOutputBus().
  const RoutingControlSnapshot controls = m_routingMatrix->getRoutingControlSnapshot();
  return OutputBusRoute{controls.groups[group].output_start, controls.groups[group].output_width};
}

SessionDefaults TransportController::getSessionDefaults() const {
  return m_sourceRegistry.getSessionDefaults();
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

VoiceMode TransportController::getClipVoiceMode(ClipHandle handle) const {
  std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
  auto it = m_sourceRegistry.entries().find(handle);
  if (it == m_sourceRegistry.entries().end()) {
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

StartSettlementSnapshot TransportController::getStartSettlementSnapshot() const noexcept {
  for (;;) {
    const uint64_t revisionBefore =
        m_publishedStartSettlements.revision.load(std::memory_order_acquire);
    if ((revisionBefore & 1u) != 0) {
      continue;
    }

    StartSettlementSnapshot snapshot{};
    snapshot.schemaVersion =
        m_publishedStartSettlements.schemaVersion.load(std::memory_order_acquire);
    snapshot.entryCount = m_publishedStartSettlements.entryCount.load(std::memory_order_acquire);
    snapshot.sequenceExhausted =
        m_publishedStartSettlements.sequenceExhausted.load(std::memory_order_acquire);
    for (size_t index = 0; index < std::size(snapshot.reserved); ++index) {
      snapshot.reserved[index] =
          m_publishedStartSettlements.reserved[index].load(std::memory_order_acquire);
    }
    snapshot.oldestSequence =
        m_publishedStartSettlements.oldestSequence.load(std::memory_order_acquire);
    snapshot.latestSequence =
        m_publishedStartSettlements.latestSequence.load(std::memory_order_acquire);
    snapshot.overwrittenCount =
        m_publishedStartSettlements.overwrittenCount.load(std::memory_order_acquire);

    for (size_t index = 0; index < kStartSettlementSnapshotCapacity; ++index) {
      const AtomicStartSettlementRecord& source = m_publishedStartSettlements.entries[index];
      StartSettlementRecord& destination = snapshot.entries[index];
      destination.sequence = source.sequence.load(std::memory_order_acquire);
      destination.requestTag = source.requestTag.load(std::memory_order_acquire);
      destination.handle = source.handle.load(std::memory_order_acquire);
      destination.voiceId = source.voiceId.load(std::memory_order_acquire);
      destination.position.samples = source.positionSamples.load(std::memory_order_acquire);
      destination.position.seconds =
          std::bit_cast<double>(source.positionSecondsBits.load(std::memory_order_acquire));
      destination.position.beats =
          std::bit_cast<double>(source.positionBeatsBits.load(std::memory_order_acquire));
      destination.outcome =
          static_cast<StartSettlementOutcome>(source.outcome.load(std::memory_order_acquire));
    }

    const uint64_t revisionAfter =
        m_publishedStartSettlements.revision.load(std::memory_order_acquire);
    if (revisionBefore == revisionAfter) {
      return snapshot;
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
      destination.newestStartRequestTag =
          source.newestStartRequestTag.load(std::memory_order_acquire);
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

std::vector<CuePoint> TransportController::getCuePoints(ClipHandle handle) const {
  std::lock_guard<std::mutex> lock(m_sourceRegistry.mutex());
  auto it = m_sourceRegistry.entries().find(handle);
  if (it == m_sourceRegistry.entries().end()) {
    return {}; // Return empty vector if clip not found
  }

  return it->second.cuePoints; // Return copy of cue points
}

} // namespace orpheus
