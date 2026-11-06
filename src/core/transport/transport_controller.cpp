// SPDX-License-Identifier: MIT
#include "transport_controller.h"
#include "../common/pinned_math.h"
#include "../common/realtime_counter.h"
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

uint64_t incrementSaturated(uint64_t value) noexcept {
  return value == std::numeric_limits<uint64_t>::max() ? value : value + 1;
}

bool isLaterStartOrdinal(uint64_t candidate, uint64_t reference) noexcept {
  const uint64_t delta = candidate - reference;
  return delta != 0 && delta < (uint64_t{1} << 63);
}
void beginOperatorStop(ActiveClip& clip) noexcept {
  const bool wasWaitingForDelay = clip.playDelayFramesRemaining > 0;
  clip.isStopping = true;
  clip.isNaturalEnding = false;
  clip.playDelayFramesRemaining = 0;
  clip.stopFadeFramesElapsed =
      wasWaitingForDelay ? std::max<int64_t>(clip.stopFadeOutSamples, 0) : 0;
}

int64_t playbackWindowStart(const ActiveClip& clip) noexcept {
  return clip.segmentCount == 0 ? clip.trimInSamples : clip.segments[clip.segmentIndex].startSample;
}

int64_t playbackWindowEnd(const ActiveClip& clip) noexcept {
  return clip.segmentCount == 0 ? clip.trimOutSamples : clip.segments[clip.segmentIndex].endSample;
}

void resetSegmentCursor(ActiveClip& clip) noexcept {
  clip.segmentIndex = 0;
  clip.segmentRepeatsRemaining = clip.segmentCount == 0 ? 0 : clip.segments[0].repeatCount;
}

PlaybackBoundaryAction advancePlaybackWindow(ActiveClip& clip, bool loopEnabled) noexcept {
  if (clip.segmentCount != 0) {
    if (clip.segmentRepeatsRemaining > 1) {
      --clip.segmentRepeatsRemaining;
      return PlaybackBoundaryAction::Advanced;
    }
    if (clip.segmentIndex + 1 < clip.segmentCount) {
      ++clip.segmentIndex;
      clip.segmentRepeatsRemaining = clip.segments[clip.segmentIndex].repeatCount;
      return PlaybackBoundaryAction::Advanced;
    }
    if (!loopEnabled)
      return PlaybackBoundaryAction::Ended;
    resetSegmentCursor(clip);
    return PlaybackBoundaryAction::Looped;
  }
  return loopEnabled ? PlaybackBoundaryAction::Looped : PlaybackBoundaryAction::Ended;
}

// Every disabled stage of ClipDspProcessor::processFrame is an exact identity,
// so a voice whose program enables none of them can skip the per-frame call.
bool clipDspProgramActive(const ClipDspProgram& program) noexcept {
  if (program.gate.enabled || program.compressor.enabled || program.width.enabled ||
      program.limiter.enabled) {
    return true;
  }
  for (const ClipEqBandSettings& band : program.eq) {
    if (band.enabled) {
      return true;
    }
  }
  return false;
}

} // namespace

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

void TransportController::processAudio(float* const* outputBuffers, size_t numChannels,
                                       size_t numFrames) noexcept {
  if (outputBuffers == nullptr || numChannels != m_config.outputChannels ||
      numFrames > m_config.maxBlockFrames) {
    return;
  }
  for (size_t channel = 0; channel < m_config.outputChannels; ++channel) {
    if (outputBuffers[channel] == nullptr) {
      return;
    }
  }

  processCommands();
  static_assert(MAX_ACTIVE_CLIPS <= StreamingClipSource::kMaxVoiceDemands);
  std::array<StreamingClipSource*, MAX_ACTIVE_CLIPS> streamingSources{};
  size_t streamingSourceCount = 0;
  // Publish every active cursor before any read can retire a sibling's
  // current/look-ahead pages. Source snapshots are bounded and nonblocking.
  for (size_t index = 0; index < m_activeClipCount; ++index) {
    ActiveClip& clip = m_activeClips[index];
    StreamingClipSource* source = clip.streamingSource;
    if (source == nullptr) {
      continue;
    }
    if (std::find(streamingSources.begin(), streamingSources.begin() + streamingSourceCount,
                  source) == streamingSources.begin() + streamingSourceCount) {
      streamingSources[streamingSourceCount++] = source;
      source->beginVoiceDemands();
    }
    clip.streamingDemandIndex =
        source->addVoiceDemand(static_cast<int64_t>(std::floor(clip.sourcePosition)));
  }
  for (size_t index = 0; index < streamingSourceCount; ++index) {
    streamingSources[index]->publishVoiceDemands();
  }

  const bool publishTelemetry =
      m_realtimeTelemetry.beginRealtimeBlock(static_cast<uint32_t>(numFrames), m_sampleRate);

  // Only active lanes expose sample storage to routing. Clearing them here
  // keeps reused slots clean; inactive pointers stay null even if a host edits
  // their routes directly through the exposed routing matrix. A voice renders
  // at most max(numChannels, 2) lanes (a mono voice pans into two), and
  // configureVoiceRouting() leaves every lane beyond its channel count
  // unassigned, so only the lanes the voice can write need clearing.
  for (size_t voiceIndex = 0; voiceIndex < m_activeClipCount; ++voiceIndex) {
    const size_t laneBase = voiceIndex * m_config.maxSourceChannels;
    const uint16_t voiceChannels = m_activeClips[voiceIndex].numChannels;
    const size_t laneCount = std::min<size_t>(m_config.maxSourceChannels,
                                              voiceChannels == 1 ? size_t{2} : voiceChannels);
    for (size_t lane = 0; lane < laneCount; ++lane) {
      std::memset(m_clipChannelBuffers[laneBase + lane].data(), 0, numFrames * sizeof(float));
    }
  }

  // Render each active voice into its preallocated routing lanes. Playback
  // delay is measured in output frames; playback rate advances the fractional
  // source cursor and uses linear interpolation without allocating or blocking.
  for (size_t voiceIndex = 0; voiceIndex < m_activeClipCount; ++voiceIndex) {
    ActiveClip& clip = m_activeClips[voiceIndex];
    if (!clip.source) {
      continue;
    }

    const int64_t fadeInSampleCount = clip.fadeInSamples;
    const int64_t fadeOutSampleCount = clip.fadeOutSamples;
    const FadeCurve fadeInCurveType = clip.fadeInCurve;
    const FadeCurve fadeOutCurveType = clip.fadeOutCurve;
    const int64_t operatorStopFadeSamples = clip.stopFadeOutSamples;
    const FadeCurve operatorStopFadeCurve = clip.stopFadeOutCurve;
    const bool loopEnabled = clip.loopEnabled;
    const double playbackRate = clip.playbackRate;
    const bool muted = clip.muted;
    const float panLeft = clip.panLeftGain;
    const float panRight = clip.panRightGain;
    const float clipGainTarget = muted ? 0.0f : clip.gainLinear;
    const size_t numFileChannels = clip.numChannels;
    const bool dspActive = clipDspProgramActive(clip.dspProcessor.program());
    const size_t laneBase = voiceIndex * m_config.maxSourceChannels;
    const bool panMonoToStereo =
        numFileChannels == 1 && m_config.maxSourceChannels > 1 &&
        clip.routingGroup < m_config.numGroups &&
        m_groupOutputWidths[clip.routingGroup].load(std::memory_order_relaxed) >= 2;
    float* const clipReadBuffer = m_clipReadBuffers[voiceIndex].data();
    const size_t clipReadBufferSize = m_clipReadBuffers[voiceIndex].size();

    size_t outputFrame = 0;
    if (clip.playDelayFramesRemaining > 0) {
      const size_t delayedFrames = static_cast<size_t>(
          std::min<int64_t>(clip.playDelayFramesRemaining, static_cast<int64_t>(numFrames)));
      clip.playDelayFramesRemaining -= static_cast<int64_t>(delayedFrames);
      outputFrame += delayedFrames;
    }

    while (outputFrame < numFrames) {
      int64_t activeWindowStart = playbackWindowStart(clip);
      int64_t activeWindowEnd = playbackWindowEnd(clip);
      if (clip.sourcePosition < static_cast<double>(activeWindowStart)) {
        clip.sourcePosition = static_cast<double>(activeWindowStart);
        clip.currentSample = activeWindowStart;
        clip.source->setDemand(activeWindowStart);
      }

      if (!clip.isStopping && clip.sourcePosition >= static_cast<double>(activeWindowEnd)) {
        if (handlePlaybackBoundary(clip, loopEnabled) != PlaybackBoundaryAction::Ended) {
          activeWindowStart = playbackWindowStart(clip);
          activeWindowEnd = playbackWindowEnd(clip);
        }
      }

      const bool atProgramStart =
          clip.segmentCount == 0 ||
          (clip.segmentIndex == 0 && clip.segmentRepeatsRemaining == clip.segments[0].repeatCount);
      const bool atProgramEnd =
          clip.segmentCount == 0 ||
          (clip.segmentIndex + 1 == clip.segmentCount && clip.segmentRepeatsRemaining == 1);

      int64_t activeStopFadeSamples = 0;
      FadeCurve activeStopFadeCurve = operatorStopFadeCurve;
      if (clip.isStopping) {
        if (clip.isNaturalEnding) {
          activeStopFadeSamples = fadeOutSampleCount;
          if (activeStopFadeSamples == 0) {
            activeStopFadeSamples = static_cast<int64_t>(m_fadeOutSamples);
          }
          activeStopFadeCurve = fadeOutCurveType;
        } else {
          activeStopFadeSamples = operatorStopFadeSamples;
        }

        if (activeStopFadeSamples <= 0 || clip.stopFadeFramesElapsed >= activeStopFadeSamples) {
          clip.stopFadeFramesElapsed = std::max<int64_t>(activeStopFadeSamples, 0);
          break;
        }
      }

      size_t segmentFrames = numFrames - outputFrame;
      if (clip.isStopping) {
        segmentFrames = std::min(
            segmentFrames, static_cast<size_t>(activeStopFadeSamples - clip.stopFadeFramesElapsed));
      } else {
        const double sourceFramesToOut = static_cast<double>(activeWindowEnd) - clip.sourcePosition;
        const size_t outputFramesToOut =
            static_cast<size_t>(std::max(1.0, std::ceil(sourceFramesToOut / playbackRate)));
        segmentFrames = std::min(segmentFrames, outputFramesToOut);
      }

      const int64_t readStart = static_cast<int64_t>(std::floor(clip.sourcePosition));
      const double lastSourcePosition =
          clip.sourcePosition + playbackRate * static_cast<double>(segmentFrames - 1);
      const size_t requestedInputFrames =
          static_cast<size_t>(std::floor(lastSourcePosition - static_cast<double>(readStart))) + 2;
      const size_t samplesNeeded = requestedInputFrames * numFileChannels;

      size_t framesRead = 0;
      bool readSucceeded = true;
      if (readStart < clip.fileLengthSamples && samplesNeeded <= clipReadBufferSize) {
        readSucceeded =
            clip.streamingSource
                ? clip.streamingSource->readForVoice(clip.streamingDemandIndex, readStart,
                                                     clipReadBuffer, requestedInputFrames,
                                                     framesRead)
                : clip.source->read(readStart, clipReadBuffer, requestedInputFrames, framesRead);
      }

      if (!readSucceeded) {
        postVoiceEvent(TransportEventType::BufferUnderrun, clip.handle, 0, clip.voiceId,
                       getCurrentPosition());
        m_realtimeTelemetry.reportUnderrunFromRealtime();
        clip.source->setDemand(readStart);
        framesRead = 0;
      }
      std::array<float, 8> loopStartFrame{};
      bool hasLoopStartFrame = false;
      if (clip.segmentCount == 0 && loopEnabled &&
          readStart + static_cast<int64_t>(requestedInputFrames) > activeWindowEnd) {
        size_t loopStartFramesRead = 0;
        hasLoopStartFrame =
            (clip.streamingSource
                 ? clip.streamingSource->readForVoice(clip.streamingDemandIndex, activeWindowStart,
                                                      loopStartFrame.data(), 1, loopStartFramesRead)
                 : clip.source->read(activeWindowStart, loopStartFrame.data(), 1,
                                     loopStartFramesRead)) &&
            loopStartFramesRead == 1;
        if (!hasLoopStartFrame) {
          clip.source->setDemand(activeWindowStart);
        }
      }

      for (size_t frame = 0; frame < segmentFrames; ++frame) {
        if (clip.gainCurrent < clipGainTarget) {
          clip.gainCurrent = std::min(clipGainTarget, clip.gainCurrent + clip.gainRampIncrement);
        } else if (clip.gainCurrent > clipGainTarget) {
          clip.gainCurrent = std::max(clipGainTarget, clip.gainCurrent - clip.gainRampIncrement);
        }

        float gain = clip.gainCurrent;
        if (clip.isRestarting && clip.restartFadeFramesRemaining > 0) {
          const int64_t fadeProgress =
              static_cast<int64_t>(m_restartCrossfadeSamples) - clip.restartFadeFramesRemaining;
          gain *= static_cast<float>(fadeProgress) / static_cast<float>(m_restartCrossfadeSamples);
          --clip.restartFadeFramesRemaining;
          if (clip.restartFadeFramesRemaining == 0) {
            clip.isRestarting = false;
          }
        }

        if (clip.isStopping) {
          const float stopFadePosition = std::clamp(
              static_cast<float>(clip.stopFadeFramesElapsed + static_cast<int64_t>(frame)) /
                  static_cast<float>(activeStopFadeSamples),
              0.0f, 1.0f);
          gain *= std::max(0.0f, 1.0f - calculateFadeGain(stopFadePosition, activeStopFadeCurve));
        }

        const double frameSourcePosition =
            clip.sourcePosition + playbackRate * static_cast<double>(frame);
        const double localPosition = frameSourcePosition - static_cast<double>(readStart);
        const size_t firstInputFrame = static_cast<size_t>(std::floor(localPosition));
        if (firstInputFrame >= framesRead) {
          continue;
        }
        const size_t secondInputFrame = std::min(firstInputFrame + 1, framesRead - 1);
        const float fraction =
            static_cast<float>(localPosition - static_cast<double>(firstInputFrame));

        const int64_t relativePosition =
            static_cast<int64_t>(std::floor(frameSourcePosition)) - activeWindowStart;
        // Looping bypasses boundary fades, not the initial start envelope.
        // Explicit starts/refires reset hasLoopedOnce; automatic wraps set it.
        if (atProgramStart && (!loopEnabled || !clip.hasLoopedOnce) && fadeInSampleCount > 0 &&
            relativePosition >= 0 && relativePosition < fadeInSampleCount) {
          const float fadePosition =
              static_cast<float>(relativePosition) / static_cast<float>(fadeInSampleCount);
          gain *= calculateFadeGain(fadePosition, fadeInCurveType);
        }

        if (!loopEnabled) {
          const int64_t activeWindowDuration = activeWindowEnd - activeWindowStart;
          if (atProgramEnd && fadeOutSampleCount > 0 &&
              relativePosition >= activeWindowDuration - fadeOutSampleCount) {
            const float fadePosition = std::clamp(
                static_cast<float>(relativePosition - (activeWindowDuration - fadeOutSampleCount)) /
                    static_cast<float>(fadeOutSampleCount),
                0.0f, 1.0f);
            gain *= 1.0f - calculateFadeGain(fadePosition, fadeOutCurveType);
          }
        }

        std::array<float, 8> sourceFrame{};
        for (size_t channel = 0; channel < numFileChannels; ++channel) {
          const float first = clipReadBuffer[firstInputFrame * numFileChannels + channel];
          const bool secondFrameCrossesWindow =
              readStart + static_cast<int64_t>(secondInputFrame) >= activeWindowEnd;
          const float second =
              secondFrameCrossesWindow && clip.segmentCount != 0
                  ? first
                  : (secondFrameCrossesWindow && loopEnabled
                         ? (hasLoopStartFrame ? loopStartFrame[channel] : first)
                         : clipReadBuffer[secondInputFrame * numFileChannels + channel]);
          sourceFrame[channel] = first + (second - first) * fraction;
        }
        if (dspActive) {
          clip.dspProcessor.processFrame(sourceFrame.data(), numFileChannels);
        }

        const size_t destinationFrame = outputFrame + frame;
        if (m_config.sourceChannelPolicy == SourceChannelPolicy::Discrete) {
          if (panMonoToStereo) {
            m_clipChannelBuffers[laneBase][destinationFrame] = sourceFrame[0] * gain * panLeft;
            m_clipChannelBuffers[laneBase + 1][destinationFrame] = sourceFrame[0] * gain * panRight;
          } else {
            for (size_t channel = 0; channel < numFileChannels; ++channel) {
              const float panGain =
                  channel == 0 && numFileChannels > 1 ? panLeft : (channel == 1 ? panRight : 1.0f);
              m_clipChannelBuffers[laneBase + channel][destinationFrame] =
                  sourceFrame[channel] * gain * panGain;
            }
          }
        } else {
          float left = 0.0f;
          float right = 0.0f;
          if (numFileChannels == 1) {
            left = sourceFrame[0];
            right = left;
          } else if (numFileChannels == 2) {
            left = sourceFrame[0];
            right = sourceFrame[1];
          } else {
            left = applyDownmixLeft(sourceFrame.data(), 0, numFileChannels);
            right = applyDownmixRight(sourceFrame.data(), 0, numFileChannels);
          }
          m_clipChannelBuffers[laneBase][destinationFrame] = left * gain * panLeft;
          if (m_config.maxSourceChannels > 1) {
            m_clipChannelBuffers[laneBase + 1][destinationFrame] = right * gain * panRight;
          }
        }
      }

      clip.sourcePosition += playbackRate * static_cast<double>(segmentFrames);
      clip.currentSample = static_cast<int64_t>(std::floor(clip.sourcePosition));
      if (clip.isStopping) {
        clip.stopFadeFramesElapsed += static_cast<int64_t>(segmentFrames);
      }
      outputFrame += segmentFrames;
    }
  }

  // Source-less voices exist only in transport tests. Keep their clocks and
  // stop envelopes moving without introducing a fake render source.
  for (size_t voiceIndex = 0; voiceIndex < m_activeClipCount; ++voiceIndex) {
    ActiveClip& clip = m_activeClips[voiceIndex];
    if (clip.source) {
      continue;
    }
    size_t activeFrames = numFrames;
    if (clip.playDelayFramesRemaining > 0) {
      const size_t delayedFrames = static_cast<size_t>(
          std::min<int64_t>(clip.playDelayFramesRemaining, static_cast<int64_t>(numFrames)));
      clip.playDelayFramesRemaining -= static_cast<int64_t>(delayedFrames);
      activeFrames -= delayedFrames;
    }
    const double playbackRate = clip.playbackRate;
    clip.sourcePosition += playbackRate * static_cast<double>(activeFrames);
    clip.currentSample = static_cast<int64_t>(std::floor(clip.sourcePosition));
    if (clip.isStopping) {
      clip.stopFadeFramesElapsed += static_cast<int64_t>(activeFrames);
    }
  }

  // Process routing matrix: clips → groups → master output.
  const SessionGraphError routingResult =
      m_routingMatrix->processRouting(const_cast<const float**>(m_clipChannelPointers.data()),
                                      outputBuffers, static_cast<uint32_t>(numFrames));
  const RoutingConfig routingConfig = m_routingMatrix->getConfig();
  if (routingResult != SessionGraphError::OK || !routingConfig.enable_metering) {
    markRoutingMetersUnmeasured();
  } else {
    collectRoutingMeters(static_cast<uint32_t>(numFrames));
  }
  // Update clips
  size_t i = 0;
  while (i < m_activeClipCount) {
    ActiveClip& clip = m_activeClips[i];

    if (clip.isStopping) {
      int64_t stopFadeSamples =
          clip.isNaturalEnding ? clip.fadeOutSamples : clip.stopFadeOutSamples;
      if (clip.isNaturalEnding && stopFadeSamples == 0) {
        stopFadeSamples = static_cast<int64_t>(m_fadeOutSamples);
      }

      if (clip.stopFadeFramesElapsed >= stopFadeSamples) {
        const ClipHandle stoppedHandle = clip.handle;
        const StartRequestTag stoppedRequestTag = clip.startRequestTag;
        const uint32_t stoppedVoiceId = clip.voiceId;
        removeActiveVoice(stoppedVoiceId);

        // Publish every retired voice. Hosts that present handle-level state
        // can reconcile against getClipState(); durable consumers need the
        // voice identity even when a freshly fired sibling remains live.
        postVoiceEvent(TransportEventType::ClipStopped, stoppedHandle, stoppedRequestTag,
                       stoppedVoiceId, getCurrentPosition());
        continue;
      }

      ++i;
      continue;
    }

    if (clip.currentSample >= playbackWindowEnd(clip)) {
      (void)handlePlaybackBoundary(clip, clip.loopEnabled);
    }
    ++i;
  }

  // Start and seek command primes remain pinned through every source read in
  // this callback, including loop interpolation and stopping overlap tails.
  releasePendingStartReservations();
  releasePendingSeekReservations();

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
    // The snapshot owns the pending routing window (routing_meters), so only
    // the per-block fields are written here; tails a larger earlier count left
    // behind are reset instead of value-initializing the whole snapshot.
    RealtimeTelemetrySnapshot& snapshot = m_realtimeTelemetrySnapshot;
    snapshot.position = TimePoint::fromSamples(newSample);
    snapshot.active_voice_count = static_cast<uint32_t>(m_activeClipCount);

    const auto groupCount = static_cast<uint8_t>(
        std::min<size_t>(routingConfig.num_groups, kRealtimeTelemetryMaxGroups));
    snapshot.group_count = groupCount;
    for (uint8_t group = 0; group < groupCount; ++group) {
      snapshot.group_meters[group] = m_routingMatrix->getGroupMeter(group);
    }
    for (uint8_t group = groupCount; group < m_snapshotGroupHigh; ++group) {
      snapshot.group_meters[group] = AudioMeter{};
    }
    m_snapshotGroupHigh = groupCount;

    const auto outputCount = static_cast<uint8_t>(
        std::min<size_t>(routingConfig.num_outputs, kRealtimeTelemetryMaxOutputs));
    snapshot.output_count = outputCount;
    for (uint8_t output = 0; output < outputCount; ++output) {
      snapshot.output_meters[output] = m_routingMatrix->getOutputMeter(output);
    }
    for (uint8_t output = outputCount; output < m_snapshotOutputHigh; ++output) {
      snapshot.output_meters[output] = AudioMeter{};
    }
    m_snapshotOutputHigh = outputCount;
    snapshot.master_meter = m_routingMatrix->getMasterMeter();

    if (m_realtimeTelemetry.publishFromRealtime(snapshot)) {
      // A successful enqueue transfers the pending canonical window to the
      // ring; the next successful render starts a fresh window.
      rearmRoutingMeterWindow();
    }
  }
}

PlaybackBoundaryAction TransportController::handlePlaybackBoundary(ActiveClip& clip,
                                                                   bool loopEnabled) noexcept {
  const auto boundary = advancePlaybackWindow(clip, loopEnabled);
  if (boundary == PlaybackBoundaryAction::Ended) {
    clip.isStopping = true;
    clip.isNaturalEnding = true;
    clip.stopFadeFramesElapsed = 0;
    return boundary;
  }

  const int64_t nextStart = playbackWindowStart(clip);
  clip.sourcePosition = static_cast<double>(nextStart);
  clip.currentSample = nextStart;
  if (clip.source) {
    clip.source->setDemand(nextStart);
  }
  clip.hasLoopedOnce = true;
  if (boundary == PlaybackBoundaryAction::Looped) {
    postVoiceEvent(TransportEventType::ClipLooped, clip.handle, clip.startRequestTag, clip.voiceId,
                   getCurrentPosition());
  }
  return boundary;
}

void TransportController::postVoiceEvent(TransportEventType type, ClipHandle handle,
                                         StartRequestTag requestTag, uint32_t voiceId,
                                         TransportPosition position) noexcept {
  TransportEvent event{};
  event.type = type;
  event.handle = handle;
  event.requestTag = requestTag;
  event.voiceId = voiceId;
  event.position = position;
  postTransportEvent(event);
}

void TransportController::mergeAudioMeter(AudioMeter& accumulated,
                                          const AudioMeter& current) noexcept {
  if (std::isfinite(current.peak_db)) {
    if (!std::isfinite(accumulated.peak_db) || current.peak_db > accumulated.peak_db) {
      accumulated.peak_db = current.peak_db;
    }
  }
  accumulated.rms_db = std::isfinite(current.rms_db) ? current.rms_db : kAudioMeterSilenceDb;
  accumulated.clip_count = std::max(accumulated.clip_count, current.clip_count);
  accumulated.clipping = accumulated.clipping || current.clipping || accumulated.clip_count != 0;
}

void TransportController::markRoutingMetersUnmeasured() noexcept {
  RoutingMeterTelemetry& accumulator = m_realtimeTelemetrySnapshot.routing_meters;
  accumulator = {};
  accumulator.schema_version = kRoutingMeterTelemetrySchemaVersion;
  accumulator.availability = MeterAvailability::Unmeasured;
  accumulator.peak_window_frames = 0;
  accumulator.rms_window_frames = 0;
  accumulator.group_aggregate_availability.fill(MeterAvailability::Unmeasured);
  accumulator.master_aggregate_availability = MeterAvailability::Unmeasured;
  accumulator.post_master_output_availability.fill(MeterAvailability::Unmeasured);
  accumulator.group_output_meters.schema_version = kGroupOutputMeterSnapshotSchemaVersion;
  accumulator.group_output_meters.availability = MeterAvailability::Unmeasured;
  accumulator.group_output_meters.coherent = 0;
  m_routingMeterAccumulatorValid = false;
  m_meterGroupHigh = 0;
  m_meterLaneHigh.fill(0);
}

void TransportController::rearmRoutingMeterWindow() noexcept {
  // Every payload field is rewritten by the next collectRoutingMeters() that
  // finds the window invalid, so only the window and availability marks are
  // reset here. A zero-frame block, which collects nothing, completes the
  // reset there before anything could publish the retained payload.
  RoutingMeterTelemetry& accumulator = m_realtimeTelemetrySnapshot.routing_meters;
  accumulator.availability = MeterAvailability::Unmeasured;
  accumulator.peak_window_frames = 0;
  accumulator.rms_window_frames = 0;
  accumulator.group_aggregate_availability.fill(MeterAvailability::Unmeasured);
  accumulator.master_aggregate_availability = MeterAvailability::Unmeasured;
  accumulator.post_master_output_availability.fill(MeterAvailability::Unmeasured);
  accumulator.group_output_meters.availability = MeterAvailability::Unmeasured;
  accumulator.group_output_meters.coherent = 0;
  m_routingMeterAccumulatorValid = false;
}

void TransportController::markLogicalGroupOutputUnmeasured() noexcept {
  GroupOutputMeterSnapshot& groupOutputs =
      m_realtimeTelemetrySnapshot.routing_meters.group_output_meters;
  groupOutputs.availability = MeterAvailability::Unmeasured;
  groupOutputs.coherent = 0;
  const RoutingGroupIndex groupCount =
      std::min<RoutingGroupIndex>(groupOutputs.group_count, kRoutingControlMaxGroups);
  for (RoutingGroupIndex group = 0; group < groupCount; ++group) {
    groupOutputs.groups[group].availability = MeterAvailability::Unmeasured;
  }
}

void TransportController::copyGroupOutputMeterRegion(const GroupOutputMeterSnapshot& source,
                                                     bool mergeMeasuredLanes) noexcept {
  GroupOutputMeterSnapshot& destination =
      m_realtimeTelemetrySnapshot.routing_meters.group_output_meters;
  if (source.availability == MeterAvailability::Unsupported ||
      source.schema_version != destination.schema_version) {
    // No region contract to rely on: take the whole value and assume every
    // frame may now be populated.
    destination = source;
    m_meterGroupHigh = kRoutingControlMaxGroups;
    m_meterLaneHigh.fill(static_cast<uint16_t>(kRoutingMaxOutputs));
    return;
  }

  // A routing snapshot leaves every frame beyond group_count and every lane
  // beyond logical_lane_count default-initialized, so copying the populated
  // region and resetting only what a larger earlier region left behind yields
  // the same value as copying the whole snapshot.
  destination.schema_version = source.schema_version;
  destination.coherent = source.coherent;
  destination.availability = source.availability;
  destination.group_count = source.group_count;
  destination.render_sequence = source.render_sequence;
  destination.routing_topology_revision = source.routing_topology_revision;

  const RoutingGroupIndex groupCount =
      std::min<RoutingGroupIndex>(source.group_count, kRoutingControlMaxGroups);
  for (RoutingGroupIndex group = 0; group < groupCount; ++group) {
    GroupOutputMeterFrame& frame = destination.groups[group];
    const GroupOutputMeterFrame& current = source.groups[group];
    const bool mergeFrame = mergeMeasuredLanes &&
                            frame.availability == MeterAvailability::Measured &&
                            current.availability == MeterAvailability::Measured;
    frame.routing_output_start = current.routing_output_start;
    frame.logical_lane_count = current.logical_lane_count;
    frame.availability = current.availability;
    frame.peak_definition = current.peak_definition;
    frame.raw_block_frames = current.raw_block_frames;
    const uint16_t laneCount = std::min<uint16_t>(current.logical_lane_count, kRoutingMaxOutputs);
    if (mergeFrame) {
      for (uint16_t lane = 0; lane < laneCount; ++lane) {
        mergeAudioMeter(frame.lane_meters[lane], current.lane_meters[lane]);
      }
    } else {
      std::copy_n(current.lane_meters.begin(), laneCount, frame.lane_meters.begin());
    }
    for (uint16_t lane = laneCount; lane < m_meterLaneHigh[group]; ++lane) {
      frame.lane_meters[lane] = AudioMeter{};
    }
    m_meterLaneHigh[group] = laneCount;
  }
  for (RoutingGroupIndex group = groupCount; group < m_meterGroupHigh; ++group) {
    GroupOutputMeterFrame& frame = destination.groups[group];
    const uint16_t laneHigh = m_meterLaneHigh[group];
    frame.routing_output_start = 0;
    frame.logical_lane_count = 0;
    frame.availability = MeterAvailability::Unconfigured;
    frame.peak_definition = MeterPeakDefinition::SamplePeak;
    frame.raw_block_frames = 0;
    for (uint16_t lane = 0; lane < laneHigh; ++lane) {
      frame.lane_meters[lane] = AudioMeter{};
    }
    m_meterLaneHigh[group] = 0;
  }
  m_meterGroupHigh = groupCount;
}

void TransportController::collectRoutingMeters(uint32_t numFrames) noexcept {
  if (numFrames == 0) {
    // A zero-frame block extends no window. After a publish only the window
    // marks were re-armed, so finish the reset before the retained payload of
    // the published window could be captured again.
    if (!m_routingMeterAccumulatorValid) {
      markRoutingMetersUnmeasured();
    }
    return;
  }

  const RoutingConfig routingConfig = m_routingMatrix->getConfig();
  if (!routingConfig.enable_metering) {
    markRoutingMetersUnmeasured();
    return;
  }

  // ORP273 F-05: the block's readings are collected into a small local that
  // carries only what the block measures. The published payload lives in
  // m_realtimeTelemetrySnapshot.routing_meters and is written field by field,
  // so no 17,936-byte value is constructed and copied on the audio thread.
  struct BlockReadings {
    std::array<AudioMeter, kRoutingControlMaxGroups> group_meters{};
    std::array<MeterAvailability, kRoutingControlMaxGroups> group_availability{};
    std::array<AudioMeter, kRoutingMaxOutputs> output_meters{};
    std::array<MeterAvailability, kRoutingMaxOutputs> output_availability{};
    AudioMeter master_meter{};
    MeterAvailability master_availability{MeterAvailability::Unmeasured};
    MeterPeakDefinition aggregate_peak_definition{MeterPeakDefinition::SamplePeak};
    uint16_t output_count{0};
  };
  BlockReadings readings;

  readings.group_availability.fill(MeterAvailability::Unconfigured);
  readings.output_availability.fill(MeterAvailability::Unconfigured);
  readings.aggregate_peak_definition = routingConfig.metering_mode == MeteringMode::TruePeak
                                           ? MeterPeakDefinition::TruePeak4x
                                           : (routingConfig.metering_mode == MeteringMode::LUFS
                                                  ? MeterPeakDefinition::LegacyLufsProxy
                                                  : MeterPeakDefinition::SamplePeak);
  readings.output_count = routingConfig.num_outputs;
  for (RoutingGroupIndex group = 0; group < routingConfig.num_groups; ++group) {
    readings.group_availability[group] = MeterAvailability::Measured;
    readings.group_meters[group] = m_routingMatrix->getGroupMeter(group);
  }
  readings.master_availability = MeterAvailability::Measured;
  readings.master_meter = m_routingMatrix->getMasterMeter();
  for (RoutingOutputIndex output = 0; output < routingConfig.num_outputs; ++output) {
    readings.output_availability[output] = MeterAvailability::Measured;
    readings.output_meters[output] = m_routingMatrix->getOutputMeter(output);
  }
  bool logicalExtensionImplemented = false;
  for (uint32_t attempt = 0; attempt < 3; ++attempt) {
    m_routingMatrix->copyGroupOutputMeterSnapshot(m_groupOutputMeterScratch);
    logicalExtensionImplemented =
        m_groupOutputMeterScratch.availability != MeterAvailability::Unsupported;
    if (!logicalExtensionImplemented || m_groupOutputMeterScratch.coherent != 0) {
      break;
    }
  }

  // The group-output payload has a single holder: the scratch buffer.
  m_groupOutputMeterScratch.schema_version = kGroupOutputMeterSnapshotSchemaVersion;
  const bool logicalMeasured =
      logicalExtensionImplemented && m_groupOutputMeterScratch.coherent != 0 &&
      m_groupOutputMeterScratch.availability == MeterAvailability::Measured;
  const bool logicalRetryFailed = logicalExtensionImplemented && !logicalMeasured;
  if (logicalRetryFailed) {
    m_groupOutputMeterScratch.availability = MeterAvailability::Unmeasured;
    m_groupOutputMeterScratch.coherent = 0;
    const RoutingGroupIndex groupCount = std::min<RoutingGroupIndex>(
        m_groupOutputMeterScratch.group_count, kRoutingControlMaxGroups);
    for (RoutingGroupIndex group = 0; group < groupCount; ++group) {
      m_groupOutputMeterScratch.groups[group].availability = MeterAvailability::Unmeasured;
    }
  }

  RoutingMeterTelemetry& accumulator = m_realtimeTelemetrySnapshot.routing_meters;
  const bool topologyChanged = m_routingMeterAccumulatorValid && logicalExtensionImplemented &&
                               accumulator.group_output_meters.routing_topology_revision !=
                                   m_groupOutputMeterScratch.routing_topology_revision;
  if (!m_routingMeterAccumulatorValid || topologyChanged) {
    accumulator.group_aggregate_availability = readings.group_availability;
    accumulator.group_aggregate_meters = readings.group_meters;
    accumulator.master_aggregate_availability = readings.master_availability;
    accumulator.master_aggregate_meter = readings.master_meter;
    accumulator.post_master_output_availability = readings.output_availability;
    accumulator.post_master_output_lane_meters = readings.output_meters;
    copyGroupOutputMeterRegion(m_groupOutputMeterScratch, false);
    m_routingMeterAccumulatorValid = true;
    accumulator.peak_window_frames = 0;
    accumulator.rms_window_frames = 0;
  } else {
    for (RoutingGroupIndex group = 0; group < kRoutingControlMaxGroups; ++group) {
      if (readings.group_availability[group] == MeterAvailability::Measured) {
        if (accumulator.group_aggregate_availability[group] != MeterAvailability::Measured) {
          accumulator.group_aggregate_meters[group] = readings.group_meters[group];
        } else {
          mergeAudioMeter(accumulator.group_aggregate_meters[group], readings.group_meters[group]);
        }
        accumulator.group_aggregate_availability[group] = MeterAvailability::Measured;
      } else {
        accumulator.group_aggregate_availability[group] = readings.group_availability[group];
      }
    }
    if (readings.master_availability == MeterAvailability::Measured) {
      if (accumulator.master_aggregate_availability != MeterAvailability::Measured) {
        accumulator.master_aggregate_meter = readings.master_meter;
      } else {
        mergeAudioMeter(accumulator.master_aggregate_meter, readings.master_meter);
      }
      accumulator.master_aggregate_availability = MeterAvailability::Measured;
    }
    for (RoutingOutputIndex output = 0; output < kRoutingMaxOutputs; ++output) {
      if (readings.output_availability[output] == MeterAvailability::Measured) {
        if (accumulator.post_master_output_availability[output] != MeterAvailability::Measured) {
          accumulator.post_master_output_lane_meters[output] = readings.output_meters[output];
        } else {
          mergeAudioMeter(accumulator.post_master_output_lane_meters[output],
                          readings.output_meters[output]);
        }
        accumulator.post_master_output_availability[output] = MeterAvailability::Measured;
      } else {
        accumulator.post_master_output_availability[output] = readings.output_availability[output];
      }
    }

    // Lanes measured in both the pending window and this block merge in place;
    // every other populated lane takes this block's reading.
    const GroupOutputMeterSnapshot& pending = accumulator.group_output_meters;
    const bool mergeMeasuredLanes =
        logicalMeasured && pending.availability == MeterAvailability::Measured &&
        pending.routing_topology_revision == m_groupOutputMeterScratch.routing_topology_revision;
    copyGroupOutputMeterRegion(m_groupOutputMeterScratch, mergeMeasuredLanes);
  }

  accumulator.schema_version = kRoutingMeterTelemetrySchemaVersion;
  accumulator.availability = MeterAvailability::Measured;
  accumulator.aggregate_peak_definition = readings.aggregate_peak_definition;
  accumulator.post_master_output_peak_definition = MeterPeakDefinition::TruePeak4x;
  accumulator.post_master_output_count = readings.output_count;
  accumulator.peak_window_frames = detail::saturatingAdd(accumulator.peak_window_frames, numFrames);
  accumulator.rms_window_frames = numFrames;
  if (logicalRetryFailed) {
    markLogicalGroupOutputUnmeasured();
  }
}

void TransportController::processCommands() {
  detachPendingCommands();
  size_t commandPosition = m_detachedCommandCount;
  const auto settleStartPrime = [&](const TransportCommand& command, bool retain) noexcept {
    if (command.startSource == nullptr || command.startPrime.pageMask == 0) {
      return;
    }
    if (!retain || m_pendingStartReservationCount >= MAX_ACTIVE_CLIPS) {
      command.startSource->releaseCommandPrime(command.startPrime);
      return;
    }
    const int64_t primedStart = command.startContext ? command.startContext->trimInSamples : 0;
    m_pendingStartReservations[m_pendingStartReservationCount++] = {
        command.startSource, command.startPrime, primedStart};
  };
  const auto processStart = [&](const TransportCommand& command) -> uint32_t {
    if (command.requestTag != 0 && m_startSettlementSequenceExhausted) {
      settleStartPrime(command, false);
      return 0;
    }

    const TransportPosition position = getCurrentPosition();
    const uint32_t voiceId =
        command.startContext ? startVoiceWithMode(command.startContext, command.requestTag) : 0;
    settleStartPrime(command, voiceId != 0);

    if (command.requestTag != 0) {
      publishStartSettlement(command.requestTag, command.handle, voiceId, position,
                             voiceId != 0 ? StartSettlementOutcome::Started
                                          : StartSettlementOutcome::ActiveVoiceLimitRejected);
    }

    if (voiceId != 0) {
      postVoiceEvent(TransportEventType::ClipStarted, command.handle, command.requestTag, voiceId,
                     position);
    }
    return voiceId;
  };

  // Every operator-stop command applies the same stop fade to the voices its
  // predicate selects; only StopAll also restarts fades already in progress.
  const auto stopVoicesWhere = [this](const auto& shouldStop) noexcept {
    for (size_t i = 0; i < m_activeClipCount; ++i) {
      ActiveClip& voice = m_activeClips[i];
      if (shouldStop(voice)) {
        beginOperatorStop(voice);
      }
    }
  };

  while (commandPosition != 0) {
    const uint32_t nodeIndex = m_commandScratch[--commandPosition];
    CommandNode& node = m_commandNodes[nodeIndex];
    TransportCommand& cmd = node.command;

    switch (cmd.type) {
    case TransportCommand::Type::Start:
      (void)processStart(cmd);
      break;

    case TransportCommand::Type::StartWithGroupChoke: {
      // Admission is deliberately first. A voice-pool refusal may publish the
      // existing typed rejection event, but it cannot mutate any peer.
      const uint32_t voiceId = processStart(cmd);
      if (voiceId != 0) {
        // Only after the firing voice is live do same-group peers enter their
        // normal configured stop fades. The firing handle is always spared so
        // MonoWithFadeOverlap and other per-handle policies remain authoritative.
        const RoutingGroupIndex group = cmd.startContext->routingGroup;
        stopVoicesWhere([&](const ActiveClip& peer) {
          return peer.handle != cmd.handle && peer.routingGroup == group && !peer.isStopping;
        });
      }
    } break;

    case TransportCommand::Type::StartWithStopOthers: {
      // Admission is deliberately first. The firing voice and the peer choke
      // share one queue slot, so a full queue cannot partially mutate peers.
      const uint32_t voiceId = processStart(cmd);
      if (voiceId != 0) {
        stopVoicesWhere(
            [&](const ActiveClip& peer) { return peer.handle != cmd.handle && !peer.isStopping; });
      }
    } break;

    case TransportCommand::Type::Stop:
      // Multi-voice: Stop ALL voice instances for this handle
      stopVoicesWhere(
          [&](const ActiveClip& voice) { return voice.handle == cmd.handle && !voice.isStopping; });
      break;

    case TransportCommand::Type::StopAll:
      stopVoicesWhere([](const ActiveClip&) { return true; });
      break;

    case TransportCommand::Type::Panic:
      // Panic evicts each voice through the same compaction/lifetime path used
      // by natural completion, operator stops, and same-handle replacement.
      while (m_activeClipCount != 0) {
        const ActiveClip& voice = m_activeClips[0];
        const ClipHandle stoppedHandle = voice.handle;
        const StartRequestTag stoppedRequestTag = voice.startRequestTag;
        const uint32_t stoppedVoiceId = voice.voiceId;
        removeActiveVoice(stoppedVoiceId);
        postVoiceEvent(TransportEventType::ClipStopped, stoppedHandle, stoppedRequestTag,
                       stoppedVoiceId, getCurrentPosition());
      }
      break;

    case TransportCommand::Type::StopOthers:
      // ORP127 G7: choke primitive — stop every voice except cmd.handle.
      stopVoicesWhere(
          [&](const ActiveClip& voice) { return voice.handle != cmd.handle && !voice.isStopping; });
      break;

    case TransportCommand::Type::UpdateTrim:
      for (size_t i = 0; i < m_activeClipCount; ++i) {
        if (m_activeClips[i].handle == cmd.handle) {
          // Update trim points
          int64_t trimIn = cmd.data.trim.in;
          int64_t trimOut = cmd.data.trim.out;

          m_activeClips[i].trimInSamples = trimIn;
          m_activeClips[i].trimOutSamples = trimOut;

          // ORP114 Fix: Clamp position to new trim range immediately
          // This prevents fade calculation overflow in processAudio()
          if (m_activeClips[i].currentSample < trimIn) {
            m_activeClips[i].currentSample = trimIn;
            m_activeClips[i].sourcePosition = static_cast<double>(trimIn);
            if (m_activeClips[i].source) {
              m_activeClips[i].source->setDemand(trimIn);
            }
          } else if (m_activeClips[i].currentSample >= trimOut) {
            m_activeClips[i].currentSample = trimOut;
            m_activeClips[i].sourcePosition = static_cast<double>(trimOut);
            // Position clamps to OUT; playback stops naturally in processAudio.
          }
        }
      }
      break;

    case TransportCommand::Type::UpdateFade:
      for (size_t i = 0; i < m_activeClipCount; ++i) {
        if (m_activeClips[i].handle == cmd.handle) {
          m_activeClips[i].fadeInCurve = cmd.data.fade.inCurve;
          m_activeClips[i].fadeOutCurve = cmd.data.fade.outCurve;

          // Recalculate sample counts (safe to do in audio thread)
          int64_t inSamples =
              static_cast<int64_t>(cmd.data.fade.inSeconds * static_cast<double>(m_sampleRate));
          int64_t outSamples =
              static_cast<int64_t>(cmd.data.fade.outSeconds * static_cast<double>(m_sampleRate));

          m_activeClips[i].fadeInSamples = inSamples;
          m_activeClips[i].fadeOutSamples = outSamples;
          // Preserve updateClipFades()'s historical meaning for callers that
          // predate the independently configurable operator-stop fields.
          m_activeClips[i].stopFadeOutCurve = cmd.data.fade.outCurve;
          m_activeClips[i].stopFadeOutSamples = outSamples;
        }
      }
      break;

    case TransportCommand::Type::UpdateGain:
      for (size_t i = 0; i < m_activeClipCount; ++i) {
        if (m_activeClips[i].handle == cmd.handle) {
          m_activeClips[i].gainLinear = cmd.data.gain.linear;
        }
      }
      break;

    case TransportCommand::Type::UpdateLoop:
      for (size_t i = 0; i < m_activeClipCount; ++i) {
        if (m_activeClips[i].handle == cmd.handle) {
          m_activeClips[i].loopEnabled = cmd.data.booleanValue;
        }
      }
      break;

    case TransportCommand::Type::Restart: {
      // ORP127 G1: Restart moved onto the audio thread (was a UI-thread mutation
      // of currentSample/reader/isStopping/hasLoopedOnce). Restart ALL voices
      // for the handle back to trim IN, applying the broadcast-safe crossfade.
      int64_t startPosition = 0;
      bool foundAnyVoice = false;
      for (size_t i = 0; i < m_activeClipCount; ++i) {
        if (m_activeClips[i].handle == cmd.handle) {
          foundAnyVoice = true;
          // Manual restart re-applies the clip fade-in (user action), unlike
          // auto-loop which suppresses it.
          startPosition = restartVoiceInPlace(m_activeClips[i]);
        }
      }

      if (foundAnyVoice) {
        postVoiceEvent(TransportEventType::ClipRestarted, cmd.handle, 0, 0,
                       positionAtSamples(startPosition));
      }
    } break;

    case TransportCommand::Type::Seek: {
      // Position was clamped and the source's first-render pages were pinned
      // on the control thread. Seek every live voice for this handle.
      const int64_t position = cmd.data.seekPosition;
      bool foundAnyVoice = false;
      for (size_t i = 0; i < m_activeClipCount; ++i) {
        if (m_activeClips[i].handle == cmd.handle) {
          foundAnyVoice = true;
          ActiveClip& clip = m_activeClips[i];
          clip.currentSample = position;
          clip.sourcePosition = static_cast<double>(position);
          if (clip.source) {
            clip.source->setDemand(position);
          }
        }
      }

      if (!foundAnyVoice) {
        if (cmd.seekSource && cmd.seekPrime.pageMask != 0) {
          cmd.seekSource->releaseCommandPrime(cmd.seekPrime);
        }
        break;
      }

      if (cmd.seekSource && cmd.seekPrime.pageMask != 0) {
        bool replaced = false;
        for (size_t index = 0; index < m_pendingSeekReservationCount; ++index) {
          PendingSeekReservation& pending = m_pendingSeekReservations[index];
          if (pending.source == cmd.seekSource) {
            pending.source->releaseCommandPrime(pending.prime);
            pending = {cmd.handle, cmd.seekSource, cmd.seekPrime, cmd.data.seekPosition};
            replaced = true;
            break;
          }
        }
        if (!replaced) {
          assert(m_pendingSeekReservationCount < MAX_ACTIVE_CLIPS);
          if (m_pendingSeekReservationCount < MAX_ACTIVE_CLIPS) {
            m_pendingSeekReservations[m_pendingSeekReservationCount++] = {
                cmd.handle, cmd.seekSource, cmd.seekPrime, cmd.data.seekPosition};
          } else {
            // The bound is implied by the active-voice ceiling. Retain
            // failure-atomic lifetime behavior even in a corrupted state.
            cmd.seekSource->releaseCommandPrime(cmd.seekPrime);
          }
        }
      }

      postVoiceEvent(TransportEventType::ClipSeeked, cmd.handle, 0, 0, positionAtSamples(position));
    } break;

    case TransportCommand::Type::UpdateMetadata:
      // ORP127 G1: Full metadata batch applied on the audio thread. Fade sample
      // counts and linear gain were precomputed on the UI thread.
      for (size_t i = 0; i < m_activeClipCount; ++i) {
        if (m_activeClips[i].handle == cmd.handle) {
          ActiveClip& clip = m_activeClips[i];
          const bool segmentProgramChanged =
              clip.segmentCount != cmd.data.metadata.segmentCount ||
              !std::equal(clip.segments.begin(),
                          clip.segments.begin() +
                              std::min(clip.segmentCount, cmd.data.metadata.segmentCount),
                          cmd.data.metadata.segments.begin());
          if (segmentProgramChanged) {
            clip.segmentCount = cmd.data.metadata.segmentCount;
            clip.segments = cmd.data.metadata.segments;
            resetSegmentCursor(clip);
            const int64_t startPosition =
                clip.segmentCount == 0 ? cmd.data.metadata.trimIn : clip.segments[0].startSample;
            clip.currentSample = startPosition;
            clip.sourcePosition = static_cast<double>(startPosition);
            clip.isStopping = false;
            clip.isNaturalEnding = false;
            if (clip.source) {
              clip.source->setDemand(startPosition);
            }
          }
          clip.trimInSamples = cmd.data.metadata.trimIn;
          clip.trimOutSamples = cmd.data.metadata.trimOut;
          clip.fadeInCurve = cmd.data.metadata.fadeInCurve;
          clip.fadeOutCurve = cmd.data.metadata.fadeOutCurve;
          clip.fadeInSamples = cmd.data.metadata.fadeInSamples;
          clip.fadeOutSamples = cmd.data.metadata.fadeOutSamples;
          clip.stopFadeOutSamples = cmd.data.metadata.stopFadeOutSamples;
          clip.stopFadeOutCurve = cmd.data.metadata.stopFadeOutCurve;
          clip.loopEnabled = cmd.data.metadata.loopEnabled;
          clip.gainLinear = cmd.data.metadata.gainLinear;
          clip.muted = cmd.data.metadata.muted;
          clip.panLeftGain = cmd.data.metadata.panLeftGain;
          clip.panRightGain = cmd.data.metadata.panRightGain;
          clip.playbackRate = cmd.data.metadata.playbackRate;
          clip.playDelayFrames = cmd.data.metadata.playDelayFrames;
          if (clip.playDelayFramesRemaining > 0) {
            clip.playDelayFramesRemaining = cmd.data.metadata.playDelayFrames;
          }
          clip.routingGroup = cmd.data.metadata.routingGroup;
          clip.dspProcessor = cmd.dspProcessor;
          configureVoiceRouting(i, clip.routingGroup, clip.numChannels);
        }
      }
      break;

    default:
      break;
    }

    if (cmd.loopAnchorSource != nullptr) {
      cmd.loopAnchorSource->commitLoopAnchorTransition(cmd.loopAnchorTransition);
      cmd.loopAnchorSource = nullptr;
    }
    if (cmd.sourceLifetime) {
      m_sourceRegistry.releaseSourceCommand(cmd.sourceLifetime);
      cmd.sourceLifetime = nullptr;
    }
    // Detach the link before making this slot available.  The retained
    // shared_ptr payload is intentionally left for the next producer to
    // destroy, never on the audio thread.
    node.next = UINT32_MAX;
    incrementIngressCounter(m_ingressProcessed);
    node.ownership.store(0, std::memory_order_release);
  }
}

void TransportController::releasePendingStartReservations() noexcept {
  // OCC191: a Start prime must stay pinned until the consuming voice has
  // actually read past the primed page. Releasing one block too early lets a
  // competing command (seek/restart) retire the page before the first render
  // consumed it, producing a silent-buffer underrun on the restart.
  size_t kept = 0;
  for (size_t index = 0; index < m_pendingStartReservationCount; ++index) {
    PendingStartReservation& pending = m_pendingStartReservations[index];
    if (pending.source == nullptr || pending.prime.pageMask == 0) {
      continue;
    }
    const int64_t primeEnd =
        pending.primedStart + static_cast<int64_t>(StreamingClipSource::kPageFrames);
    bool consumed = true;
    for (size_t voice = 0; voice < m_activeClipCount; ++voice) {
      const ActiveClip& clip = m_activeClips[voice];
      if (clip.handle == 0 || clip.source.get() != pending.source) {
        continue;
      }
      // The primed page is consumed once every voice on this source has read
      // past its end (sourcePosition is the fractional source cursor).
      if (clip.sourcePosition < static_cast<double>(primeEnd)) {
        consumed = false;
        break;
      }
    }
    if (consumed) {
      pending.source->releaseCommandPrime(pending.prime);
      pending = {};
    } else {
      m_pendingStartReservations[kept++] = pending;
    }
  }
  m_pendingStartReservationCount = kept;
}

void TransportController::releasePendingSeekReservations() noexcept {
  // OCC191: same consumption rule for accepted seeks — the seek prime pins the
  // first-render page and must survive until the sought voice reads past it.
  size_t kept = 0;
  for (size_t index = 0; index < m_pendingSeekReservationCount; ++index) {
    PendingSeekReservation& pending = m_pendingSeekReservations[index];
    if (pending.source == nullptr || pending.prime.pageMask == 0) {
      continue;
    }
    const int64_t primeEnd =
        pending.primedStart + static_cast<int64_t>(StreamingClipSource::kPageFrames);
    bool consumed = true;
    for (size_t voice = 0; voice < m_activeClipCount; ++voice) {
      const ActiveClip& clip = m_activeClips[voice];
      if (clip.source.get() != pending.source || clip.handle != pending.handle ||
          clip.sourcePosition >= static_cast<double>(primeEnd)) {
        continue;
      }
      consumed = false;
      break;
    }
    if (consumed) {
      pending.source->releaseCommandPrime(pending.prime);
      pending = {};
    } else {
      m_pendingSeekReservations[kept++] = pending;
    }
  }
  m_pendingSeekReservationCount = kept;
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
int64_t TransportController::restartVoiceInPlace(ActiveClip& clip) {
  // ORP127 G5: reset a live voice to its first programmed source window and
  // cancel any fade-out in progress.
  resetSegmentCursor(clip);
  const int64_t startPosition = playbackWindowStart(clip);
  clip.currentSample = startPosition;
  clip.sourcePosition = static_cast<double>(startPosition);
  clip.playDelayFramesRemaining = clip.playDelayFrames;
  clip.isNaturalEnding = false;
  if (clip.source) {
    clip.source->setDemand(startPosition);
  }
  clip.isStopping = false;
  clip.hasLoopedOnce = false;
  // Broadcast-safe restart crossfade to avoid a click at the reset point.
  clip.isRestarting = true;
  clip.restartFadeFramesRemaining = static_cast<int64_t>(m_restartCrossfadeSamples);
  return startPosition;
}

uint32_t
TransportController::startVoiceWithMode(const std::shared_ptr<ClipPlaybackContext>& context,
                                        StartRequestTag requestTag) {
  if (!context)
    return 0;

  const auto addVoice = [this, &context, requestTag]() -> uint32_t {
    if (!addActiveClip(context, requestTag))
      return 0;
    return m_activeClips[m_activeClipCount - 1].voiceId;
  };

  const ClipHandle handle = context->handle;
  const VoiceMode mode = context->voiceMode;

  switch (mode) {
  case VoiceMode::Polyphonic:
    return addVoice();

  case VoiceMode::MonoStrict: {
    ActiveClip* primary = nullptr;
    for (size_t i = 0; i < m_activeClipCount; ++i) {
      if (m_activeClips[i].handle == handle && !primary)
        primary = &m_activeClips[i];
    }
    if (!primary)
      return addVoice();

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
      return addVoice();
    primary->startRequestTag = requestTag;
    primary->startOrdinal = allocateVoiceStartOrdinal();
    primary->startSample = m_currentSample.load(std::memory_order_relaxed);

    resetSegmentCursor(*primary);
    const int64_t startPosition = playbackWindowStart(*primary);
    primary->currentSample = startPosition;
    primary->sourcePosition = static_cast<double>(startPosition);
    primary->playDelayFramesRemaining = primary->playDelayFrames;
    primary->isNaturalEnding = false;
    if (primary->source)
      primary->source->setDemand(startPosition);
    primary->isStopping = false;
    primary->hasLoopedOnce = false;
    primary->isRestarting = false;
    primary->restartFadeFramesRemaining = 0;
    primary->voiceMode = mode;
    return primary->voiceId;
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
      return addVoice();

    restartVoiceInPlace(*live);
    live->startRequestTag = requestTag;
    live->startOrdinal = allocateVoiceStartOrdinal();
    live->startSample = m_currentSample.load(std::memory_order_relaxed);
    live->voiceMode = mode;
    return live->voiceId;
  }
  }

  return addVoice();
}

void TransportController::configureVoiceRouting(size_t voiceIndex, RoutingGroupIndex group,
                                                uint16_t numChannels) noexcept {
  const size_t laneBase = voiceIndex * m_config.maxSourceChannels;
  const bool panMonoToStereo = numChannels == 1 && m_config.maxSourceChannels > 1 &&
                               group < m_config.numGroups &&
                               m_groupOutputWidths[group].load(std::memory_order_relaxed) >= 2;
  const uint16_t renderedChannels = panMonoToStereo ? 2 : numChannels;
  for (uint32_t lane = 0; lane < m_config.maxSourceChannels; ++lane) {
    const bool activeLane = lane < renderedChannels;
    const auto routeGroup = activeLane ? group : UNASSIGNED_GROUP;
    const auto output =
        activeLane ? static_cast<RoutingOutputIndex>(lane) : static_cast<RoutingOutputIndex>(0);
    (void)m_routingMatrix->setChannelRoute(static_cast<RoutingChannelIndex>(laneBase + lane),
                                           routeGroup, output);
  }
}

uint32_t TransportController::allocateVoiceId(uint32_t ignoredVoiceId) noexcept {
  uint32_t nextVoiceId = m_nextVoiceId;
  for (size_t attempt = 0; attempt <= MAX_ACTIVE_CLIPS; ++attempt) {
    uint32_t candidate = nextVoiceId;
    if (candidate == 0) {
      candidate = 1;
    }
    nextVoiceId = candidate == std::numeric_limits<uint32_t>::max() ? 1 : candidate + 1;

    bool inUse = ignoredVoiceId != 0 && candidate == ignoredVoiceId;
    for (size_t active = 0; !inUse && active < m_activeClipCount; ++active) {
      inUse = m_activeClips[active].voiceId == candidate;
    }
    if (!inUse) {
      m_nextVoiceId = nextVoiceId;
      return candidate;
    }
  }
  return 0;
}
bool TransportController::addActiveClip(const std::shared_ptr<ClipPlaybackContext>& context,
                                        StartRequestTag requestTag) {
  if (!context) {
    return false;
  }

  const ClipHandle handle = context->handle;
  const size_t currentVoiceCount = countActiveVoices(handle);
  const size_t maxVoices = m_maxVoicesPerClip.load(std::memory_order_relaxed);
  uint32_t replacementVoiceId = 0;
  if (currentVoiceCount >= maxVoices) {
    ActiveClip* oldest = findOldestVoice(handle);
    if (oldest == nullptr) {
      return false;
    }
    replacementVoiceId = oldest->voiceId;
  }

  if (m_activeClipCount >= m_config.maxActiveVoices && replacementVoiceId == 0) {
    postVoiceEvent(TransportEventType::ActiveClipLimitReached, handle, requestTag, 0,
                   getCurrentPosition());
    return false;
  }

  // Preflight every refusal condition before changing the live voice set.
  const uint32_t voiceId = allocateVoiceId(replacementVoiceId);
  if (voiceId == 0) {
    return false;
  }

  // The incoming source pin is acquired before same-handle replacement drops
  // the old pin, so unregister cannot observe a transient zero.
  m_sourceRegistry.retainActiveSource(context->sourceLifetime);
  if (replacementVoiceId != 0) {
    removeActiveVoice(replacementVoiceId);
  }

  const uint64_t startOrdinal = allocateVoiceStartOrdinal();

  // Initialize clip with immutable context - NO MUTEX NEEDED HERE
  const size_t voiceIndex = m_activeClipCount++;
  ActiveClip& clip = m_activeClips[voiceIndex];
  const size_t laneBase = voiceIndex * m_config.maxSourceChannels;
  for (size_t lane = 0; lane < m_config.maxSourceChannels; ++lane) {
    m_clipChannelPointers[laneBase + lane] = m_clipChannelBuffers[laneBase + lane].data();
  }
  clip.segmentCount = context->segmentCount;
  clip.segments = context->segments;
  resetSegmentCursor(clip);
  const int64_t startPosition =
      clip.segmentCount == 0 ? context->trimInSamples : clip.segments[0].startSample;
  clip.sourcePosition = static_cast<double>(startPosition);
  clip.handle = handle;
  clip.voiceId = voiceId;
  clip.startRequestTag = requestTag;
  clip.startOrdinal = startOrdinal;
  clip.startSample = m_currentSample.load(std::memory_order_relaxed);
  clip.currentSample = startPosition;

  // Initialize trim points
  clip.trimInSamples = context->trimInSamples;
  clip.trimOutSamples = context->trimOutSamples;

  // Initialize fade settings
  clip.fadeInCurve = context->fadeInCurve;
  clip.fadeOutCurve = context->fadeOutCurve;

  // Calculate and store fade sample counts.
  const int64_t fadeInSampleCount =
      static_cast<int64_t>(context->fadeInSeconds * static_cast<double>(m_sampleRate));
  const int64_t fadeOutSampleCount =
      static_cast<int64_t>(context->fadeOutSeconds * static_cast<double>(m_sampleRate));
  const int64_t stopFadeOutSampleCount =
      static_cast<int64_t>(context->stopFadeOutSeconds * static_cast<double>(m_sampleRate));
  clip.fadeInSamples = fadeInSampleCount;
  clip.fadeOutSamples = fadeOutSampleCount;
  clip.stopFadeOutCurve = context->stopFadeOutCurve;
  clip.stopFadeOutSamples = stopFadeOutSampleCount;

  // A muted start must remain silent from its first frame. Unmuting later
  // ramps from zero through the normal gain-smoothing path.
  clip.gainLinear = context->gainLinear;
  clip.gainCurrent = context->muted ? 0.0f : context->gainLinear;
  clip.gainRampIncrement = m_clipGainRampIncrement;

  // Initialize loop mode
  clip.muted = context->muted;
  clip.panLeftGain = context->panLeftGain;
  clip.panRightGain = context->panRightGain;
  clip.playbackRate = context->playbackRate;
  clip.playDelayFrames = context->playDelayFrames;
  clip.playDelayFramesRemaining = context->playDelayFrames;
  clip.loopEnabled = context->loopEnabled;

  clip.source = context->source; // Store shared_ptr (maintains reference count)
  clip.streamingSource = context->streamingSource;
  clip.streamingDemandIndex = 0;
  clip.sourceLifetime = context->sourceLifetime;
  clip.numChannels = context->numChannels;
  clip.fileLengthSamples = context->fileLengthSamples; // ORP127 G3
  clip.voiceMode = context->voiceMode;                 // ORP127 G5
  clip.routingGroup = context->routingGroup;
  clip.dspProcessor = context->dspProcessor;
  configureVoiceRouting(voiceIndex, clip.routingGroup, clip.numChannels);
  clip.isStopping = false;

  // Initialize restart crossfade state
  clip.isRestarting = false;
  clip.isNaturalEnding = false;
  clip.restartFadeFramesRemaining = 0;

  // ORP097 Bug 7 Fix: Initialize loop state
  clip.hasLoopedOnce = false;

  // Prime the streaming prefetcher at the first programmed source window
  // (no-op for prepared sources; reads remain position-explicit).
  if (clip.source) {
    clip.source->setDemand(startPosition);
  }
  return true;
}

void TransportController::removeActiveVoice(uint32_t voiceId) {
  for (size_t i = 0; i < m_activeClipCount; ++i) {
    if (m_activeClips[i].voiceId == voiceId) {
      SourceCommandLifetime* removedLifetime = m_activeClips[i].sourceLifetime;
      StreamingClipSource* removedSource = m_activeClips[i].streamingSource;
      if (removedSource != nullptr) {
        bool stillActive = false;
        for (size_t other = 0; other < m_activeClipCount; ++other) {
          if (other != i && m_activeClips[other].streamingSource == removedSource) {
            stillActive = true;
            break;
          }
        }
        if (!stillActive) {
          removedSource->beginVoiceDemands();
          removedSource->publishVoiceDemands();
        }
      }
      // Remove by moving the last voice into this slot. Every ActiveClip field
      // is audio-thread-only plain state, so the defaulted move carries the
      // whole voice, including its gain ramp and source references.
      if (i < m_activeClipCount - 1) {
        ActiveClip& dest = m_activeClips[i];
        dest = std::move(m_activeClips[m_activeClipCount - 1]);
        configureVoiceRouting(i, dest.routingGroup, dest.numChannels);
      }
      const size_t vacatedLaneBase = (m_activeClipCount - 1) * m_config.maxSourceChannels;
      for (size_t lane = 0; lane < m_config.maxSourceChannels; ++lane) {
        m_clipChannelPointers[vacatedLaneBase + lane] = nullptr;
      }
      configureVoiceRouting(m_activeClipCount - 1, UNASSIGNED_GROUP, 0);
      m_activeClips[m_activeClipCount - 1].source.reset();
      m_activeClips[m_activeClipCount - 1].streamingSource = nullptr;
      m_activeClips[m_activeClipCount - 1].streamingDemandIndex = 0;
      m_activeClips[m_activeClipCount - 1].sourceLifetime = nullptr;
      --m_activeClipCount;
      m_sourceRegistry.releaseActiveSource(removedLifetime);
      return;
    }
  }
}

void TransportController::incrementIngressCounter(std::atomic<uint64_t>& counter) noexcept {
  const uint64_t previous = counter.fetch_add(1, std::memory_order_relaxed);
  if (previous >= UINT32_MAX)
    counter.fetch_sub(1, std::memory_order_relaxed);
}

void TransportController::detachPendingCommands() noexcept {
  const uint32_t head = m_pendingCommandHead.exchange(UINT32_MAX, std::memory_order_acquire);
  size_t count = 0;
  for (uint32_t index = head; index != UINT32_MAX && count < MAX_COMMANDS;
       index = m_commandNodes[index].next)
    m_commandScratch[count++] = index;
  m_detachedCommandCount = count;
}

void TransportController::releaseCommandNode(uint32_t index) noexcept {
  m_commandNodes[index].next = UINT32_MAX;
  m_commandNodes[index].ownership.store(0, std::memory_order_release);
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
      entry.newestStartRequestTag = voice.startRequestTag;
      entry.newestVoiceStopping = voice.isStopping ? 1 : 0;
      entry.newestVoiceLoopEnabled = voice.loopEnabled ? 1 : 0;
      entry.newestStartSample = voice.startSample;
      entry.newestTrimInSamples = voice.trimInSamples;
      entry.newestTrimOutSamples = voice.trimOutSamples;
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
    destination.newestStartRequestTag.store(source.newestStartRequestTag,
                                            std::memory_order_release);
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

void TransportController::publishStartSettlement(StartRequestTag requestTag, ClipHandle handle,
                                                 uint32_t voiceId, TransportPosition position,
                                                 StartSettlementOutcome outcome) noexcept {
  if (requestTag == 0 || m_startSettlementSequenceExhausted) {
    return;
  }

  StartSettlementSnapshot& snapshot = m_startSettlementScratch;
  const uint64_t sequence = m_startSettlementSequence + 1;
  m_startSettlementSequence = sequence;

  uint32_t destinationIndex = snapshot.entryCount;
  if (snapshot.entryCount == kStartSettlementSnapshotCapacity) {
    for (size_t index = 1; index < kStartSettlementSnapshotCapacity; ++index) {
      snapshot.entries[index - 1] = snapshot.entries[index];
    }
    destinationIndex = static_cast<uint32_t>(kStartSettlementSnapshotCapacity - 1);
    snapshot.overwrittenCount = incrementSaturated(snapshot.overwrittenCount);
  } else {
    ++snapshot.entryCount;
  }

  StartSettlementRecord& record = snapshot.entries[destinationIndex];
  record = {};
  record.sequence = sequence;
  record.requestTag = requestTag;
  record.handle = handle;
  record.voiceId = voiceId;
  record.position = position;
  record.outcome = outcome;
  snapshot.oldestSequence = snapshot.entries[0].sequence;
  snapshot.latestSequence = sequence;
  if (sequence == std::numeric_limits<uint64_t>::max()) {
    snapshot.sequenceExhausted = 1;
    m_startSettlementSequenceExhausted = true;
  }

  const uint64_t oddRevision = m_startSettlementRevision + 1;
  m_startSettlementRevision = oddRevision;
  m_publishedStartSettlements.revision.store(oddRevision, std::memory_order_release);
  m_publishedStartSettlements.schemaVersion.store(snapshot.schemaVersion,
                                                  std::memory_order_release);
  m_publishedStartSettlements.entryCount.store(snapshot.entryCount, std::memory_order_release);
  m_publishedStartSettlements.sequenceExhausted.store(snapshot.sequenceExhausted,
                                                      std::memory_order_release);
  for (size_t index = 0; index < std::size(snapshot.reserved); ++index) {
    m_publishedStartSettlements.reserved[index].store(snapshot.reserved[index],
                                                      std::memory_order_release);
  }
  m_publishedStartSettlements.oldestSequence.store(snapshot.oldestSequence,
                                                   std::memory_order_release);
  m_publishedStartSettlements.latestSequence.store(snapshot.latestSequence,
                                                   std::memory_order_release);
  m_publishedStartSettlements.overwrittenCount.store(snapshot.overwrittenCount,
                                                     std::memory_order_release);

  for (size_t index = 0; index < kStartSettlementSnapshotCapacity; ++index) {
    const StartSettlementRecord& source = snapshot.entries[index];
    AtomicStartSettlementRecord& destination = m_publishedStartSettlements.entries[index];
    destination.sequence.store(source.sequence, std::memory_order_release);
    destination.requestTag.store(source.requestTag, std::memory_order_release);
    destination.handle.store(source.handle, std::memory_order_release);
    destination.voiceId.store(source.voiceId, std::memory_order_release);
    destination.positionSamples.store(source.position.samples, std::memory_order_release);
    destination.positionSecondsBits.store(std::bit_cast<uint64_t>(source.position.seconds),
                                          std::memory_order_release);
    destination.positionBeatsBits.store(std::bit_cast<uint64_t>(source.position.beats),
                                        std::memory_order_release);
    destination.outcome.store(static_cast<uint8_t>(source.outcome), std::memory_order_release);
  }

  const uint64_t evenRevision = m_startSettlementRevision + 1;
  m_startSettlementRevision = evenRevision;
  m_publishedStartSettlements.revision.store(evenRevision, std::memory_order_release);
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

float TransportController::calculateFadeGain(float normalizedPosition, FadeCurve curve) const {
  switch (curve) {
  case FadeCurve::Linear:
    return normalizedPosition; // y = x

  case FadeCurve::EqualPower: {
    const float argument = normalizedPosition * static_cast<float>(M_PI_2);
    return static_cast<float>(orpheus::pinned::sin(static_cast<double>(argument)));
  }

  case FadeCurve::Exponential:
    return normalizedPosition * normalizedPosition; // y = x²

  default:
    return normalizedPosition; // Fallback to linear
  }
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

} // namespace orpheus
