// SPDX-License-Identifier: MIT
#include "transport_controller.h"

#include "audio_io/resampling_audio_file_reader.h" // ORP127 G6: SRC decorator
#include "session/session_graph.h"                 // For SessionGraph
#include <algorithm>
#include <cmath>
#include <cstring>

// MSVC and some platforms don't define M_PI_2 by default
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

namespace orpheus {

TransportController::TransportController(core::SessionGraph* sessionGraph, uint32_t sampleRate)
    : m_sessionGraph(sessionGraph), m_sampleRate(sampleRate), m_callback(nullptr) {
  // Calculate fade-out samples
  m_fadeOutSamples =
      static_cast<size_t>((FADE_OUT_DURATION_MS / 1000.0f) * static_cast<float>(sampleRate));

  // Calculate restart crossfade samples (broadcast-safe restart mechanism)
  m_restartCrossfadeSamples = static_cast<size_t>((RESTART_CROSSFADE_DURATION_MS / 1000.0f) *
                                                  static_cast<float>(sampleRate));

  // ORP127 G4: per-sample clip-gain ramp increment for the default smoothing
  // time. A full 0→1 change takes CLIP_GAIN_SMOOTHING_MS; each sample moves by
  // 1 / (ms/1000 * sampleRate).
  {
    float smoothingSamples = (CLIP_GAIN_SMOOTHING_MS / 1000.0f) * static_cast<float>(sampleRate);
    m_clipGainRampIncrement = (smoothingSamples > 0.0f) ? (1.0f / smoothingSamples) : 1.0f;
  }

  // Create and initialize routing matrix
  m_routingMatrix = createRoutingMatrix();

  // ORP121 A-01: ST2110-aligned channel architecture
  // Each clip can use up to 2 routing channels (L/R for stereo sources)
  // Mono sources use 1 channel, stereo sources use 2 independent channels
  // This follows ST2110-30 principle: audio as discrete independent channels
  static constexpr size_t MAX_ROUTING_CHANNELS = MAX_ACTIVE_CLIPS * 2;

  RoutingConfig routingConfig;
  // num_channels is structural: 2 routing channels per active clip (L/R), so it
  // scales with MAX_ACTIVE_CLIPS. num_groups is a typical soundboard default;
  // hosts that need a different grouping topology can reconfigure the matrix.
  routingConfig.num_channels = MAX_ROUTING_CHANNELS; // 2 channels per clip (stereo)
  routingConfig.num_groups = 4;                      // typical soundboard default
  routingConfig.num_outputs = 2;                     // stereo output
  routingConfig.solo_mode = SoloMode::SIP;
  routingConfig.metering_mode = MeteringMode::Peak;
  routingConfig.gain_smoothing_ms =
      0.0f; // DISABLED: Fades handled at clip level, smoothing causes zigzag artifacts
  routingConfig.enable_metering = true;
  routingConfig.enable_clipping_protection =
      true; // Enabled by default: prevents distortion when many voices fade out
            // simultaneously (soft-knee tanh limiter, no audible quality loss).

  m_routingMatrix->initialize(routingConfig);

  // ORP121 A-01: Configure channel pairs for stereo routing
  // Each clip's L channel: pan = -1.0 (hard left)
  // Each clip's R channel: pan = +1.0 (hard right)
  // Both channels route to the same group (group 0 by default)
  for (size_t clip = 0; clip < MAX_ACTIVE_CLIPS; ++clip) {
    size_t ch_L = clip * 2;
    size_t ch_R = clip * 2 + 1;

    // Configure L channel (hard left pan)
    ChannelConfig configL;
    configL.gain_db = 0.0f;
    configL.pan = -1.0f; // Hard left for stereo L channel
    configL.mute = false;
    configL.solo = false;
    m_routingMatrix->configureChannel(static_cast<uint8_t>(ch_L), configL);
    m_routingMatrix->setChannelGroup(static_cast<uint8_t>(ch_L), 0);

    // Configure R channel (hard right pan)
    ChannelConfig configR;
    configR.gain_db = 0.0f;
    configR.pan = 1.0f; // Hard right for stereo R channel
    configR.mute = false;
    configR.solo = false;
    m_routingMatrix->configureChannel(static_cast<uint8_t>(ch_R), configR);
    m_routingMatrix->setChannelGroup(static_cast<uint8_t>(ch_R), 0);
  }

  // Pre-allocate per-clip read buffers (interleaved audio from files)
  m_clipReadBuffers.resize(MAX_ACTIVE_CLIPS);
  for (auto& buffer : m_clipReadBuffers) {
    buffer.resize(MAX_BUFFER_FRAMES * MAX_FILE_CHANNELS, 0.0f);
  }

  // ORP121 A-01: Pre-allocate stereo clip channel buffers
  // Each clip has L and R buffers: index = clip * 2 + (0 for L, 1 for R)
  m_clipChannelBuffers.resize(MAX_ROUTING_CHANNELS);
  for (auto& buffer : m_clipChannelBuffers) {
    buffer.resize(MAX_BUFFER_FRAMES, 0.0f);
  }

  // Pre-allocate pointer array for processRouting()
  m_clipChannelPointers.resize(MAX_ROUTING_CHANNELS);
  for (size_t i = 0; i < MAX_ROUTING_CHANNELS; ++i) {
    m_clipChannelPointers[i] = m_clipChannelBuffers[i].data();
  }

  // TODO: m_sessionGraph will be used for querying clip metadata (trim points, routing, etc.)
  (void)m_sessionGraph; // Suppress unused warning for now
}

TransportController::~TransportController() = default;

SessionGraphError TransportController::startClip(ClipHandle handle) {
  // Validate handle
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  // Resolve playback context (UI thread - safe to lock mutex)
  auto context = std::make_shared<ClipPlaybackContext>();
  context->handle = handle;

  {
    std::lock_guard<std::mutex> lock(m_audioFilesMutex);
    auto it = m_audioFiles.find(handle);
    if (it != m_audioFiles.end()) {
      const auto& entry = it->second;
      context->reader = entry.reader;
      context->numChannels = entry.metadata.num_channels;
      context->trimInSamples = entry.trimInSamples;
      context->fileLengthSamples = entry.metadata.duration_samples; // ORP127 G3

      // If trim OUT is not set (0), use file duration
      context->trimOutSamples =
          (entry.trimOutSamples == 0) ? entry.metadata.duration_samples : entry.trimOutSamples;

      context->fadeInSeconds = entry.fadeInSeconds;
      context->fadeOutSeconds = entry.fadeOutSeconds;
      context->fadeInCurve = entry.fadeInCurve;
      context->fadeOutCurve = entry.fadeOutCurve;
      context->gainDb = entry.gainDb;
      // Precompute linear gain
      context->gainLinear = std::pow(10.0f, entry.gainDb / 20.0f);
      context->loopEnabled = entry.loopEnabled;
      context->voiceMode = entry.voiceMode; // ORP127 G5
    } else {
      // Clip not registered - use defaults for testing
      context->reader = nullptr;
      context->numChannels = 2;
      context->trimInSamples = 0;
      context->trimOutSamples = 48000 * 60;    // Default 60s
      context->fileLengthSamples = 48000 * 60; // ORP127 G3: match default OUT
      context->fadeInSeconds = 0.0;
      context->fadeOutSeconds = 0.0;
      context->fadeInCurve = FadeCurve::Linear;
      context->fadeOutCurve = FadeCurve::Linear;
      context->gainDb = 0.0f;
      context->gainLinear = 1.0f;
      context->loopEnabled = false;
      context->voiceMode = VoiceMode::Polyphonic; // ORP127 G5: default
    }
  }

  // Check "Stop Others" mode (UI thread check is fine for initiating stop command)
  bool stopOthers = false;
  {
    std::lock_guard<std::mutex> lock(m_audioFilesMutex);
    auto it = m_audioFiles.find(handle);
    if (it != m_audioFiles.end()) {
      stopOthers = it->second.stopOthersOnPlay;
    }
  }

  if (stopOthers) {
    // ORP127 G7: choke OTHER clips (not this one) before the Start command that
    // follows in queue order. Using stopOtherClips(handle) instead of the old
    // global stopAllClips() means existing voices of THIS clip are preserved and
    // the choke is correctly scoped to everything else.
    stopOtherClips(handle);
  }

  // Post Start command to audio thread
  size_t writeIndex = m_commandWriteIndex.load(std::memory_order_relaxed);
  size_t nextIndex = (writeIndex + 1) % MAX_COMMANDS;

  // Check if queue is full
  if (nextIndex == m_commandReadIndex.load(std::memory_order_acquire)) {
    return SessionGraphError::InternalError; // Queue full
  }

  TransportCommand& cmd = m_commands[writeIndex];
  cmd.type = TransportCommand::Type::Start;
  cmd.handle = handle;
  cmd.startContext = context; // Move shared_ptr into command

  m_commandWriteIndex.store(nextIndex, std::memory_order_release);

  return SessionGraphError::OK;
}

SessionGraphError TransportController::stopClip(ClipHandle handle) {
  // Validate handle
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  // Post command to audio thread
  size_t writeIndex = m_commandWriteIndex.load(std::memory_order_relaxed);
  size_t nextIndex = (writeIndex + 1) % MAX_COMMANDS;

  // Check if queue is full
  if (nextIndex == m_commandReadIndex.load(std::memory_order_acquire)) {
    return SessionGraphError::InternalError; // Queue full
  }

  TransportCommand& cmd = m_commands[writeIndex];
  cmd.type = TransportCommand::Type::Stop;
  cmd.handle = handle;
  cmd.startContext = nullptr;
  cmd.data.groupIndex = 0;
  m_commandWriteIndex.store(nextIndex, std::memory_order_release);

  return SessionGraphError::OK;
}

SessionGraphError TransportController::stopAllClips() {
  // Post command to audio thread
  size_t writeIndex = m_commandWriteIndex.load(std::memory_order_relaxed);
  size_t nextIndex = (writeIndex + 1) % MAX_COMMANDS;

  // Check if queue is full
  if (nextIndex == m_commandReadIndex.load(std::memory_order_acquire)) {
    return SessionGraphError::InternalError; // Queue full
  }

  TransportCommand& cmd = m_commands[writeIndex];
  cmd.type = TransportCommand::Type::StopAll;
  cmd.handle = 0;
  cmd.startContext = nullptr;
  cmd.data.groupIndex = 0;
  m_commandWriteIndex.store(nextIndex, std::memory_order_release);

  return SessionGraphError::OK;
}

SessionGraphError TransportController::stopAllInGroup(uint8_t groupIndex) {
  // Validate group index (0-3 for 4 Clip Groups)
  if (groupIndex >= 4) {
    return SessionGraphError::InvalidParameter;
  }

  // Post command to audio thread
  size_t writeIndex = m_commandWriteIndex.load(std::memory_order_relaxed);
  size_t nextIndex = (writeIndex + 1) % MAX_COMMANDS;

  // Check if queue is full
  if (nextIndex == m_commandReadIndex.load(std::memory_order_acquire)) {
    return SessionGraphError::InternalError; // Queue full
  }

  TransportCommand& cmd = m_commands[writeIndex];
  cmd.type = TransportCommand::Type::StopGroup;
  cmd.handle = 0;
  cmd.startContext = nullptr;
  cmd.data.groupIndex = groupIndex;
  m_commandWriteIndex.store(nextIndex, std::memory_order_release);

  return SessionGraphError::OK;
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
  // ORP127 G7: clamp to [1, hard max]. Atomic so the audio thread reads a
  // consistent value in addActiveClip.
  uint32_t clamped = std::clamp(maxVoices, 1u, VOICE_CAP_HARD_MAX);
  m_maxVoicesPerClip.store(clamped, std::memory_order_relaxed);
  return SessionGraphError::OK;
}

uint32_t TransportController::getMaxVoicesPerClip() const {
  return m_maxVoicesPerClip.load(std::memory_order_relaxed);
}

PlaybackState TransportController::getClipState(ClipHandle handle) const {
  // ORP127 G1: Read from the published voice snapshot — no access to the live
  // audio-thread voice array. Multi-voice: Playing if ANY voice is playing,
  // Stopping if voices exist but all are stopping, Stopped if none.
  size_t front = m_snapshotIndex.load(std::memory_order_acquire);
  const VoiceSnapshotBuffer& buf = m_snapshotBuffers[front];

  bool hasAnyVoice = false;
  for (size_t i = 0; i < buf.count; ++i) {
    if (buf.voices[i].handle == handle) {
      hasAnyVoice = true;
      if (!buf.voices[i].isStopping) {
        return PlaybackState::Playing;
      }
    }
  }

  if (hasAnyVoice) {
    return PlaybackState::Stopping; // All voices are stopping
  }

  return PlaybackState::Stopped; // No voices found
}

bool TransportController::isClipPlaying(ClipHandle handle) const {
  PlaybackState state = getClipState(handle);
  return state == PlaybackState::Playing || state == PlaybackState::Stopping;
}

TransportPosition TransportController::getCurrentPosition() const {
  int64_t samples = m_currentSample.load(std::memory_order_relaxed);

  TransportPosition position;
  position.samples = samples;
  position.seconds = static_cast<double>(samples) / static_cast<double>(m_sampleRate);

  // TODO: Get tempo from SessionGraph
  double tempo = 120.0; // Default tempo
  position.beats = position.seconds * tempo / 60.0;

  return position;
}

void TransportController::setCallback(ITransportCallback* callback) {
  m_callback = callback;
}

void TransportController::processAudio(float** outputBuffers, size_t numChannels,
                                       size_t numFrames) {
  // Process pending commands from UI thread
  processCommands();

  // Routing matrix controls output channel count
  (void)numChannels;

  // Clamp frames to max buffer size
  numFrames = std::min(numFrames, MAX_BUFFER_FRAMES);

  // ORP121 A-01: Clear all clip channel buffers (stereo - 2 per clip)
  static constexpr size_t MAX_ROUTING_CHANNELS = MAX_ACTIVE_CLIPS * 2;
  for (size_t i = 0; i < MAX_ROUTING_CHANNELS; ++i) {
    std::memset(m_clipChannelBuffers[i].data(), 0, numFrames * sizeof(float));
  }

  // ORP127 G2: The stop fade-out is now computed per sample inside the render
  // loop (see below), so no per-buffer fadeOutGain pre-pass is needed. This
  // both removes the F-SDK-2 staircase and drops a redundant loop.

  // Render each active clip to its own channel buffer
  for (size_t i = 0; i < m_activeClipCount; ++i) {
    ActiveClip& clip = m_activeClips[i];

    // Skip if no audio file registered
    if (!clip.reader || !clip.reader->isOpen()) {
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
      // Position below IN point - clamp to IN (enforce Edit Law #1)
      clip.currentSample = trimIn;
      if (clip.reader) {
        clip.reader->seek(trimIn);
      }
    } else if (clip.currentSample >= trimOut) {
      // Position at or past OUT point - handle loop or stop
      bool shouldLoop = clip.loopEnabled.load(std::memory_order_acquire);
      if (shouldLoop) {
        // Loop mode: restart from IN point
        clip.currentSample = trimIn;
        if (clip.reader) {
          clip.reader->seek(trimIn);
        }

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

    // Note: We don't seek on every callback - the reader maintains its position
    // The initial seek to trimInSamples happens in addActiveClip()

    // Read audio from file
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

    // Read samples from audio file into THIS clip's buffer
    // Use captured shared_ptr reader (thread-safe, no map lookup needed)
    auto readResult = clip.reader->readSamples(clipReadBuffer, framesToRead);

    if (!readResult.isOk()) {
      continue;
    }

    size_t framesRead = readResult.value;

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

      // ORP121 A-01: Preserve stereo from source (ST2110-aligned)
      // Each file channel maps to discrete routing channels
      // - Mono: Duplicate to L/R (phantom center image)
      // - Stereo: Direct L→L, R→R mapping
      // - Multi-channel (>2): ITU-R BS.775-3 downmix to stereo
      float sample_L, sample_R;

      if (numFileChannels == 1) {
        // Mono source: Duplicate to both L/R (centered phantom image)
        float mono = clipReadBuffer[frame];
        sample_L = mono;
        sample_R = mono;
      } else if (numFileChannels == 2) {
        // Stereo source: Preserve L/R separation
        sample_L = clipReadBuffer[frame * 2 + 0];
        sample_R = clipReadBuffer[frame * 2 + 1];
      } else {
        // Multi-channel (>2): Apply ITU-R BS.775-3 downmix to stereo
        sample_L = applyDownmixLeft(clipReadBuffer, frame, numFileChannels);
        sample_R = applyDownmixRight(clipReadBuffer, frame, numFileChannels);
      }

      // Apply gain and write to stereo clip buffers
      // L channel at index i*2, R channel at index i*2+1
      m_clipChannelBuffers[i * 2][frame] = sample_L * gain;
      m_clipChannelBuffers[i * 2 + 1][frame] = sample_R * gain;
    }

    // Advance clip position by actual frames read (not buffer size!)
    // CRITICAL (Copilot feedback): This must happen AFTER fade processing, not before
    // Previously this was at line 341 (before fade loop), causing fade timing to be off by one
    // buffer
    clip.currentSample += static_cast<int64_t>(framesRead);
  }

  // Multi-voice fix: Advance position for clips WITHOUT readers (test clips, stopped clips)
  // This ensures fade-outs complete properly even when no audio is being rendered
  for (size_t i = 0; i < m_activeClipCount; ++i) {
    ActiveClip& clip = m_activeClips[i];
    if (!clip.reader || !clip.reader->isOpen()) {
      // Clip has no reader - advance position by buffer size so fades can complete
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
        // Fade-out complete, remove clip
        postCallback([this, handle = clip.handle, pos = getCurrentPosition()]() {
          if (m_callback) {
            m_callback->onClipStopped(handle, pos);
          }
        });

        removeActiveClip(clip.handle);
        continue; // Don't increment i, we just removed this clip
      }
    }

    // Check if clip reached trim OUT point
    int64_t clipTrimOut = clip.trimOutSamples.load(std::memory_order_acquire);
    if (clip.currentSample >= clipTrimOut) {
      // Check if clip should loop
      bool shouldLoop = clip.loopEnabled.load(std::memory_order_acquire);

      if (shouldLoop) {
        // Loop: seek back to trim IN point (works even without reader)
        int64_t trimIn = clip.trimInSamples.load(std::memory_order_acquire);
        if (clip.reader) {
          clip.reader->seek(trimIn);
        }
        clip.currentSample = trimIn;

        // ORP097 Bug 7 Fix: Mark that clip has looped (prevents fade-in/out on subsequent loops)
        clip.hasLoopedOnce = true;

        // Post loop callback
        postCallback([this, handle = clip.handle, pos = getCurrentPosition()]() {
          if (m_callback) {
            m_callback->onClipLooped(handle, pos);
          }
        });

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
}

void TransportController::processCommands() {
  size_t readIndex = m_commandReadIndex.load(std::memory_order_relaxed);
  size_t writeIndex = m_commandWriteIndex.load(std::memory_order_acquire);

  while (readIndex != writeIndex) {
    const TransportCommand& cmd = m_commands[readIndex];

    switch (cmd.type) {
    case TransportCommand::Type::Start: {
      // ORP127 G5: honor the clip's voice policy when firing.
      if (cmd.startContext) {
        startVoiceWithMode(cmd.startContext);
        postCallback([this, handle = cmd.handle, pos = getCurrentPosition()]() {
          if (m_callback) {
            m_callback->onClipStarted(handle, pos);
          }
        });
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

    case TransportCommand::Type::StopGroup:
      // TODO: Get clip group assignments from SessionGraph
      // For now, this is a no-op
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
            if (m_activeClips[i].reader) {
              m_activeClips[i].reader->seek(trimIn);
            }
          } else if (m_activeClips[i].currentSample >= trimOut) {
            m_activeClips[i].currentSample = trimOut;
            // Don't seek to trimOut (EOF), just let it stop naturally in processAudio logic
            // But ensure reader is consistent if we needed to read
            if (m_activeClips[i].reader) {
              m_activeClips[i].reader->seek(trimOut);
            }
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
          if (clip.reader) {
            clip.reader->seek(trimIn);
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
        postCallback([this, handle = cmd.handle, trimIn]() {
          if (m_callback) {
            TransportPosition pos;
            pos.samples = trimIn;
            pos.seconds = static_cast<double>(trimIn) / static_cast<double>(m_sampleRate);
            pos.beats = 0.0;
            m_callback->onClipRestarted(handle, pos);
          }
        });
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
          if (clip.reader) {
            clip.reader->seek(position);
          }
        }
      }

      if (foundAnyVoice) {
        postCallback([this, handle = cmd.handle, position]() {
          if (m_callback) {
            TransportPosition pos;
            pos.samples = position;
            pos.seconds = static_cast<double>(position) / static_cast<double>(m_sampleRate);
            pos.beats = 0.0;
            m_callback->onClipSeeked(handle, pos);
          }
        });
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
  // ORP127 G1: UI-thread-safe voice count via the published snapshot.
  size_t front = m_snapshotIndex.load(std::memory_order_acquire);
  const VoiceSnapshotBuffer& buf = m_snapshotBuffers[front];
  size_t count = 0;
  for (size_t i = 0; i < buf.count; ++i) {
    if (buf.voices[i].handle == handle) {
      ++count;
    }
  }
  return count;
}

ActiveClip* TransportController::findOldestVoice(ClipHandle handle) {
  ActiveClip* oldest = nullptr;
  int64_t oldestStartSample = INT64_MAX;

  for (size_t i = 0; i < m_activeClipCount; ++i) {
    if (m_activeClips[i].handle == handle) {
      // Find voice with earliest start time (oldest)
      if (m_activeClips[i].startSample < oldestStartSample) {
        oldestStartSample = m_activeClips[i].startSample;
        oldest = &m_activeClips[i];
      }
    }
  }

  return oldest;
}

void TransportController::restartVoiceInPlace(ActiveClip& clip) {
  // ORP127 G5: reset a live voice to its trim IN for an in-place restart.
  int64_t trimIn = clip.trimInSamples.load(std::memory_order_relaxed);
  clip.currentSample = trimIn;
  if (clip.reader) {
    clip.reader->seek(trimIn);
  }
  clip.isStopping = false;
  clip.fadeOutGain = 1.0f;
  clip.hasLoopedOnce = false;
  // Broadcast-safe restart crossfade to avoid a click at the reset point.
  clip.isRestarting = true;
  clip.restartFadeFramesRemaining = static_cast<int64_t>(m_restartCrossfadeSamples);
}

void TransportController::startVoiceWithMode(const std::shared_ptr<ClipPlaybackContext>& context) {
  if (!context)
    return;

  const ClipHandle handle = context->handle;
  const VoiceMode mode = context->voiceMode;

  switch (mode) {
  case VoiceMode::Polyphonic:
    // Historical behavior: always allocate a new voice (addActiveClip evicts
    // the oldest voice for this handle if already at MAX_VOICES_PER_CLIP).
    addActiveClip(context);
    return;

  case VoiceMode::MonoStrict: {
    // Strict single voice: fire-while-anything replaces it from zero with no
    // fade tail. Cut every existing voice for this handle (including fading
    // tails) and restart the first in place; if none exist, add fresh.
    ActiveClip* primary = nullptr;
    for (size_t i = 0; i < m_activeClipCount; ++i) {
      if (m_activeClips[i].handle == handle) {
        if (!primary) {
          primary = &m_activeClips[i];
        }
      }
    }
    if (!primary) {
      addActiveClip(context);
      return;
    }
    // Remove any *other* voices for this handle (no fade — strict cut). Iterate
    // from the end so removeActiveVoice's slot compaction is safe.
    for (size_t i = m_activeClipCount; i-- > 0;) {
      if (m_activeClips[i].handle == handle && &m_activeClips[i] != primary) {
        removeActiveVoice(m_activeClips[i].voiceId);
        // primary pointer may have moved if the last slot was compacted into
        // its position; re-find it.
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
    if (primary) {
      // MonoStrict restarts from zero with NO crossfade tail (hard, sample-
      // accurate replace) — just reset position; the clip fade-in (if any)
      // still applies via hasLoopedOnce=false.
      int64_t trimIn = primary->trimInSamples.load(std::memory_order_relaxed);
      primary->currentSample = trimIn;
      if (primary->reader) {
        primary->reader->seek(trimIn);
      }
      primary->isStopping = false;
      primary->fadeOutGain = 1.0f;
      primary->hasLoopedOnce = false;
      primary->isRestarting = false;
      primary->restartFadeFramesRemaining = 0;
      primary->voiceMode = mode;
    } else {
      addActiveClip(context);
    }
    return;
  }

  case VoiceMode::MonoWithFadeOverlap: {
    // One primary voice. If a live (non-stopping) voice exists, restart it in
    // place (with a short crossfade). If only fading tails exist, add a fresh
    // voice alongside them so the tail completes naturally (voices == 2 during
    // the overlap window).
    ActiveClip* live = nullptr;
    for (size_t i = 0; i < m_activeClipCount; ++i) {
      if (m_activeClips[i].handle == handle && !m_activeClips[i].isStopping) {
        live = &m_activeClips[i];
        break;
      }
    }
    if (live) {
      restartVoiceInPlace(*live);
      live->voiceMode = mode;
    } else {
      addActiveClip(context);
    }
    return;
  }
  }

  // Defensive default (unknown enum) — behave polyphonically.
  addActiveClip(context);
}

void TransportController::addActiveClip(const std::shared_ptr<ClipPlaybackContext>& context) {
  if (!context)
    return;

  ClipHandle handle = context->handle;

  // Multi-voice: Check if we need to remove oldest voice to make room.
  // ORP127 G7: the cap is now host-configurable (default 8, hard max 32).
  size_t currentVoiceCount = countActiveVoices(handle);
  size_t maxVoices = m_maxVoicesPerClip.load(std::memory_order_relaxed);
  if (currentVoiceCount >= maxVoices) {
    // At max capacity - remove oldest voice instance for this clip
    ActiveClip* oldest = findOldestVoice(handle);
    if (oldest) {
      uint32_t oldestVoiceId = oldest->voiceId;

      // Post callback that voice was stopped (for UI tracking)
      // Note: Callback reports handle, not specific voiceId (UI tracks per-handle, not per-voice)
      postCallback([this, handle, pos = getCurrentPosition()]() {
        if (m_callback) {
          m_callback->onClipStopped(handle, pos);
        }
      });

      removeActiveVoice(oldestVoiceId);
    }
  }

  if (m_activeClipCount >= MAX_ACTIVE_CLIPS) {
    // TODO: Report error (too many active clips globally)
    return;
  }

  // Initialize clip with immutable context - NO MUTEX NEEDED HERE
  ActiveClip& clip = m_activeClips[m_activeClipCount++];
  clip.handle = handle;
  clip.voiceId = m_nextVoiceId++; // Multi-voice: Assign unique voice ID
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

  clip.reader = context->reader; // Store shared_ptr (maintains reference count)
  clip.numChannels = context->numChannels;
  clip.fileLengthSamples = context->fileLengthSamples; // ORP127 G3
  clip.voiceMode = context->voiceMode;                 // ORP127 G5
  clip.fadeOutGain = 1.0f;
  clip.isStopping = false;
  clip.fadeOutStartPos = 0; // Will be set when stopClip() is called

  // Initialize restart crossfade state
  clip.isRestarting = false;
  clip.restartFadeFramesRemaining = 0;

  // ORP097 Bug 7 Fix: Initialize loop state
  clip.hasLoopedOnce = false;

  // Seek to trim IN point once when starting
  if (clip.reader) {
    clip.reader->seek(context->trimInSamples);
  }
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
        dest.reader = src.reader;
        dest.numChannels = src.numChannels;
        dest.fileLengthSamples = src.fileLengthSamples; // ORP127 G3
        dest.voiceMode = src.voiceMode;                 // ORP127 G5
      }
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
        dest.reader = src.reader;               // Copy shared_ptr (atomic refcount increment)
        dest.numChannels = src.numChannels;
      }
      --m_activeClipCount;
      return;
    }
  }
}

SessionGraphError TransportController::postCommand(const TransportCommand& command) {
  // ORP127 G1: Single choke point for UI → audio-thread commands. SPSC ring;
  // UI thread is the sole producer, audio thread the sole consumer.
  size_t writeIndex = m_commandWriteIndex.load(std::memory_order_relaxed);
  size_t nextIndex = (writeIndex + 1) % MAX_COMMANDS;

  if (nextIndex == m_commandReadIndex.load(std::memory_order_acquire)) {
    return SessionGraphError::InternalError; // Queue full
  }

  m_commands[writeIndex] = command;
  m_commandWriteIndex.store(nextIndex, std::memory_order_release);
  return SessionGraphError::OK;
}

void TransportController::publishVoiceSnapshot() {
  // ORP127 G1: Audio-thread-only. Copy the live voice array into the back
  // buffer, then flip the published index with release ordering. UI-thread
  // readers acquire the index and see a complete, self-consistent snapshot.
  size_t front = m_snapshotIndex.load(std::memory_order_relaxed);
  size_t back = front ^ 1;
  VoiceSnapshotBuffer& buf = m_snapshotBuffers[back];

  size_t count = m_activeClipCount;
  buf.count = count;
  for (size_t i = 0; i < count; ++i) {
    const ActiveClip& clip = m_activeClips[i];
    VoiceSnapshot& snap = buf.voices[i];
    snap.handle = clip.handle;
    snap.voiceId = clip.voiceId;
    snap.startSample = clip.startSample;
    snap.currentSample = clip.currentSample;
    snap.trimInSamples = clip.trimInSamples.load(std::memory_order_relaxed);
    snap.trimOutSamples = clip.trimOutSamples.load(std::memory_order_relaxed);
    snap.isStopping = clip.isStopping;
    snap.loopEnabled = clip.loopEnabled.load(std::memory_order_relaxed);
  }

  m_snapshotIndex.store(back, std::memory_order_release);
}

void TransportController::postCallback(std::function<void()> callback) {
  // ORP121 C-03: Lock-free SPSC callback queue
  // Audio thread writes to ring buffer (no mutex, no blocking)
  //
  // Memory ordering rationale:
  // - relaxed for same-thread reads (writeIdx in postCallback)
  // - acquire for cross-thread reads (readIdx check for full detection)
  // - release for writes (ensures callback data visible before index update)

  size_t writeIdx = m_callbackWriteIndex.load(std::memory_order_relaxed);
  size_t nextIdx = (writeIdx + 1) & (CALLBACK_QUEUE_SIZE - 1); // Mask for wrap (power of 2)

  // Check if queue full (read index caught up)
  if (nextIdx == m_callbackReadIndex.load(std::memory_order_acquire)) {
    // Queue full - drop callback (better than blocking audio thread)
    // Increment dropped callback counter for diagnostics
    m_droppedCallbackCount.fetch_add(1, std::memory_order_relaxed);
    return;
  }

  m_callbackRing[writeIdx] = std::move(callback);
  m_callbackWriteIndex.store(nextIdx, std::memory_order_release);
}

void TransportController::processCallbacks() {
  // ORP121 C-03: Lock-free SPSC callback queue
  // UI thread reads from ring buffer (no mutex, no blocking)
  //
  // Memory ordering rationale:
  // - relaxed for same-thread reads (readIdx in processCallbacks)
  // - acquire for cross-thread reads (writeIdx to see all pending callbacks)
  // - release for writes (ensures slot cleared before index update)

  size_t readIdx = m_callbackReadIndex.load(std::memory_order_relaxed);
  size_t writeIdx = m_callbackWriteIndex.load(std::memory_order_acquire);

  while (readIdx != writeIdx) {
    auto& callback = m_callbackRing[readIdx];
    if (callback) {
      callback();
      callback = nullptr; // Clear slot to release captured resources
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

  // Do not seek/read a shared reader while an active voice may be using it on
  // the audio thread. Future streaming readers should remove this limitation.
  if (isClipPlaying(handle)) {
    return SessionGraphError::NotReady;
  }

  std::shared_ptr<IAudioFileReader> reader;
  uint16_t numChannels = 0;
  int64_t trimIn = 0;
  {
    std::lock_guard<std::mutex> lock(m_audioFilesMutex);
    auto it = m_audioFiles.find(handle);
    if (it == m_audioFiles.end()) {
      return SessionGraphError::ClipNotRegistered;
    }
    reader = it->second.reader;
    numChannels = it->second.metadata.num_channels;
    trimIn = it->second.trimInSamples;
  }

  if (!reader || !reader->isOpen() || numChannels == 0 || numChannels > MAX_FILE_CHANNELS) {
    return SessionGraphError::NotReady;
  }

  SessionGraphError seekResult = reader->seek(trimIn);
  if (seekResult != SessionGraphError::OK) {
    return seekResult;
  }

  float scratch[MAX_FILE_CHANNELS] = {};
  auto readResult = reader->readSamples(scratch, 1);
  if (!readResult.isOk()) {
    return readResult.error;
  }

  return reader->seek(trimIn);
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
  size_t writeIndex = m_commandWriteIndex.load(std::memory_order_relaxed);
  size_t nextIndex = (writeIndex + 1) % MAX_COMMANDS;

  if (nextIndex != m_commandReadIndex.load(std::memory_order_acquire)) {
    TransportCommand& cmd = m_commands[writeIndex];
    cmd.type = TransportCommand::Type::UpdateTrim;
    cmd.handle = handle;
    cmd.data.trim.in = trimInSamples;
    cmd.data.trim.out = trimOutSamples;
    m_commandWriteIndex.store(nextIndex, std::memory_order_release);
  } else {
    return SessionGraphError::InternalError; // Queue full
  }

  return SessionGraphError::OK;
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
  {
    size_t front = m_snapshotIndex.load(std::memory_order_acquire);
    const VoiceSnapshotBuffer& buf = m_snapshotBuffers[front];
    for (size_t i = 0; i < buf.count; ++i) {
      if (buf.voices[i].handle == handle) {
        currentTrimIn = buf.voices[i].trimInSamples;
        currentTrimOut = buf.voices[i].trimOutSamples;
        foundActiveClip = true;
        break;
      }
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
  size_t writeIndex = m_commandWriteIndex.load(std::memory_order_relaxed);
  size_t nextIndex = (writeIndex + 1) % MAX_COMMANDS;

  if (nextIndex != m_commandReadIndex.load(std::memory_order_acquire)) {
    TransportCommand& cmd = m_commands[writeIndex];
    cmd.type = TransportCommand::Type::UpdateFade;
    cmd.handle = handle;
    cmd.data.fade.inSeconds = fadeInSeconds;
    cmd.data.fade.outSeconds = fadeOutSeconds;
    cmd.data.fade.inCurve = fadeInCurve;
    cmd.data.fade.outCurve = fadeOutCurve;
    m_commandWriteIndex.store(nextIndex, std::memory_order_release);
  } else {
    return SessionGraphError::InternalError; // Queue full
  }

  return SessionGraphError::OK;
}

SessionGraphError TransportController::getClipTrimPoints(ClipHandle handle, int64_t& trimInSamples,
                                                         int64_t& trimOutSamples) const {
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  // ORP127 G1: Try the published snapshot first (most recent active values).
  {
    size_t front = m_snapshotIndex.load(std::memory_order_acquire);
    const VoiceSnapshotBuffer& buf = m_snapshotBuffers[front];
    for (size_t i = 0; i < buf.count; ++i) {
      if (buf.voices[i].handle == handle) {
        trimInSamples = buf.voices[i].trimInSamples;
        trimOutSamples = buf.voices[i].trimOutSamples;
        return SessionGraphError::OK;
      }
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
  size_t writeIndex = m_commandWriteIndex.load(std::memory_order_relaxed);
  size_t nextIndex = (writeIndex + 1) % MAX_COMMANDS;

  if (nextIndex != m_commandReadIndex.load(std::memory_order_acquire)) {
    TransportCommand& cmd = m_commands[writeIndex];
    cmd.type = TransportCommand::Type::UpdateGain;
    cmd.handle = handle;
    cmd.data.gainDb = gainDb;
    m_commandWriteIndex.store(nextIndex, std::memory_order_release);
  } else {
    return SessionGraphError::InternalError; // Queue full
  }

  return SessionGraphError::OK;
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
  size_t writeIndex = m_commandWriteIndex.load(std::memory_order_relaxed);
  size_t nextIndex = (writeIndex + 1) % MAX_COMMANDS;

  if (nextIndex != m_commandReadIndex.load(std::memory_order_acquire)) {
    TransportCommand& cmd = m_commands[writeIndex];
    cmd.type = TransportCommand::Type::UpdateLoop;
    cmd.handle = handle;
    cmd.data.booleanValue = shouldLoop;
    m_commandWriteIndex.store(nextIndex, std::memory_order_release);
  } else {
    return SessionGraphError::InternalError; // Queue full
  }

  return SessionGraphError::OK;
}

int64_t TransportController::getClipPosition(ClipHandle handle) const {
  // ORP127 G1: Multi-voice position from the published snapshot (UI-safe).
  // Return the position of the newest voice (most recently started).
  size_t front = m_snapshotIndex.load(std::memory_order_acquire);
  const VoiceSnapshotBuffer& buf = m_snapshotBuffers[front];

  int64_t newestPosition = -1;
  int64_t newestStartSample = INT64_MIN;
  const VoiceSnapshot* newestVoice = nullptr;

  for (size_t i = 0; i < buf.count; ++i) {
    if (buf.voices[i].handle == handle) {
      if (buf.voices[i].startSample > newestStartSample) {
        newestStartSample = buf.voices[i].startSample;
        newestPosition = buf.voices[i].currentSample;
        newestVoice = &buf.voices[i];
      }
    }
  }

  // Clamp reported position to last valid playback position (trimOut - 1) for stopping clips
  // (internally position advances past OUT to complete fade, but UI shouldn't see this)
  // Edit Law #2: "Playhead < OUT" - position must be strictly less than OUT
  if (newestVoice && newestVoice->isStopping) {
    int64_t maxValidPosition = newestVoice->trimOutSamples - 1; // Last valid playback sample
    newestPosition = std::min(newestPosition, maxValidPosition);
  }

  return newestPosition; // Returns -1 if no voices found
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
  {
    std::lock_guard<std::mutex> lock(m_audioFilesMutex);
    auto it = m_audioFiles.find(handle);
    if (it == m_audioFiles.end()) {
      return SessionGraphError::ClipNotRegistered;
    }
    fileDurationSamples = it->second.metadata.duration_samples;
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

  // All validation passed - apply changes atomically
  // Calculate fade sample counts
  int64_t fadeInSampleCount =
      static_cast<int64_t>(metadata.fadeInSeconds * static_cast<double>(m_sampleRate));
  int64_t fadeOutSampleCount =
      static_cast<int64_t>(metadata.fadeOutSeconds * static_cast<double>(m_sampleRate));

  // Update persistent storage
  {
    std::lock_guard<std::mutex> lock(m_audioFilesMutex);
    auto it = m_audioFiles.find(handle);
    if (it != m_audioFiles.end()) {
      it->second.trimInSamples = metadata.trimInSamples;
      it->second.trimOutSamples = trimOut;
      it->second.fadeInSeconds = metadata.fadeInSeconds;
      it->second.fadeOutSeconds = metadata.fadeOutSeconds;
      it->second.fadeInCurve = metadata.fadeInCurve;
      it->second.fadeOutCurve = metadata.fadeOutCurve;
      it->second.loopEnabled = metadata.loopEnabled;
      it->second.stopOthersOnPlay = metadata.stopOthersOnPlay;
      it->second.voiceMode = metadata.voiceMode; // ORP127 G5
      it->second.gainDb = metadata.gainDb;
    }
  }

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

  // Post best-effort: if the queue is full the persistent store already holds
  // the new metadata and it will be picked up on next start. Return OK to match
  // the historical contract (validation success == OK).
  postCommand(cmd);

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

  // If trim OUT is not set (0), use file duration
  if (metadata.trimOutSamples == 0) {
    metadata.trimOutSamples = it->second.metadata.duration_samples;
  }

  return metadata;
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
  // ORP127 G1: Query the published snapshot (UI-safe, no live-array access).
  size_t front = m_snapshotIndex.load(std::memory_order_acquire);
  const VoiceSnapshotBuffer& buf = m_snapshotBuffers[front];
  for (size_t i = 0; i < buf.count; ++i) {
    if (buf.voices[i].handle == handle) {
      return buf.voices[i].loopEnabled;
    }
  }
  return false; // Clip not playing
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
                                                                uint32_t sampleRate) {
  return std::make_unique<TransportController>(sessionGraph, sampleRate);
}

} // namespace orpheus
