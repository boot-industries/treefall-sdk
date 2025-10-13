// SPDX-License-Identifier: MIT
#include "transport_controller.h"

#include "session/session_graph.h" // For SessionGraph
#include <algorithm>
#include <cmath>
#include <cstring>

namespace orpheus {

TransportController::TransportController(core::SessionGraph* sessionGraph, uint32_t sampleRate)
    : m_sessionGraph(sessionGraph), m_sampleRate(sampleRate), m_callback(nullptr) {
  // Calculate fade-out samples
  m_fadeOutSamples =
      static_cast<size_t>((FADE_OUT_DURATION_MS / 1000.0f) * static_cast<float>(sampleRate));

  // Pre-allocate audio read buffer (avoid allocations in audio thread)
  m_audioReadBuffer.resize(MAX_READ_BUFFER_SIZE);

  // Create and initialize routing matrix
  m_routingMatrix = createRoutingMatrix();

  RoutingConfig routingConfig;
  routingConfig.num_channels = MAX_ACTIVE_CLIPS; // One channel per possible active clip
  routingConfig.num_groups = 4; // 4 Clip Groups (as per ORP070)
  routingConfig.num_outputs = 2; // Stereo output
  routingConfig.solo_mode = SoloMode::SIP;
  routingConfig.metering_mode = MeteringMode::Peak;
  routingConfig.gain_smoothing_ms = 10.0f;
  routingConfig.enable_metering = true;
  routingConfig.enable_clipping_protection = true;

  m_routingMatrix->initialize(routingConfig);

  // Pre-allocate per-clip channel buffers (audio thread uses these)
  m_clipChannelBuffers.resize(MAX_ACTIVE_CLIPS);
  for (auto& buffer : m_clipChannelBuffers) {
    buffer.resize(MAX_BUFFER_FRAMES, 0.0f);
  }

  // Pre-allocate pointer array for processRouting()
  m_clipChannelPointers.resize(MAX_ACTIVE_CLIPS);
  for (size_t i = 0; i < MAX_ACTIVE_CLIPS; ++i) {
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

  // Check if already playing
  if (isClipPlaying(handle)) {
    return SessionGraphError::OK; // Already playing, no-op
  }

  // Post command to audio thread
  size_t writeIndex = m_commandWriteIndex.load(std::memory_order_relaxed);
  size_t nextIndex = (writeIndex + 1) % MAX_COMMANDS;

  // Check if queue is full
  if (nextIndex == m_commandReadIndex.load(std::memory_order_acquire)) {
    return SessionGraphError::InternalError; // Queue full
  }

  m_commands[writeIndex] = {TransportCommand::Type::Start, handle, 0};
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

  m_commands[writeIndex] = {TransportCommand::Type::Stop, handle, 0};
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

  m_commands[writeIndex] = {TransportCommand::Type::StopAll, 0, 0};
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

  m_commands[writeIndex] = {TransportCommand::Type::StopGroup, 0, groupIndex};
  m_commandWriteIndex.store(nextIndex, std::memory_order_release);

  return SessionGraphError::OK;
}

PlaybackState TransportController::getClipState(ClipHandle handle) const {
  // This is called from UI thread, but we're reading audio thread state
  // We rely on the fact that checking a clip in the array is atomic enough
  // for our purposes (worst case: we're one frame behind)

  for (size_t i = 0; i < m_activeClipCount; ++i) {
    if (m_activeClips[i].handle == handle) {
      if (m_activeClips[i].isStopping) {
        return PlaybackState::Stopping;
      }
      return PlaybackState::Playing;
    }
  }

  return PlaybackState::Stopped;
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

  // Clear all clip channel buffers
  for (size_t i = 0; i < MAX_ACTIVE_CLIPS; ++i) {
    std::memset(m_clipChannelBuffers[i].data(), 0, numFrames * sizeof(float));
  }

  // Render each active clip to its own channel buffer
  for (size_t i = 0; i < m_activeClipCount; ++i) {
    ActiveClip& clip = m_activeClips[i];

    // Skip if no audio file registered
    if (!clip.audioReader || !clip.audioReader->isOpen()) {
      continue;
    }

    // Calculate how many frames to read (respecting trim OUT point)
    int64_t framesUntilEnd = clip.trimOutSamples - clip.currentSample;
    if (framesUntilEnd <= 0) {
      continue; // Already past end
    }

    size_t framesToRead = static_cast<size_t>(std::min(static_cast<int64_t>(numFrames), framesUntilEnd));

    // Seek to current position (respecting trim IN point)
    int64_t readPosition = clip.trimInSamples + clip.currentSample;
    clip.audioReader->seek(readPosition);

    // Read audio from file
    size_t numFileChannels = clip.numChannels;

    // Check if read fits in pre-allocated buffer
    size_t samplesNeeded = framesToRead * numFileChannels;
    if (samplesNeeded > m_audioReadBuffer.size()) {
      // Clip too large for buffer - skip (should not happen with reasonable buffer size)
      continue;
    }

    // Read samples from audio file
    auto readResult = clip.audioReader->readSamples(m_audioReadBuffer.data(), framesToRead);

    if (!readResult.isOk()) {
      // Read error - skip this clip
      continue;
    }

    size_t framesRead = readResult.value;

    // Output to this clip's channel buffer (mono sum for routing)
    float* clipChannelBuffer = m_clipChannelBuffers[i].data();

    for (size_t frame = 0; frame < framesRead; ++frame) {
      // Calculate fade-out gain for this frame
      float gain = clip.fadeOutGain;
      if (clip.isStopping) {
        // Linear fade-out
        float fadeStep = 1.0f / static_cast<float>(m_fadeOutSamples);
        gain = clip.fadeOutGain - (fadeStep * static_cast<float>(frame));
        gain = std::max(0.0f, gain); // Clamp to 0
      }

      // Mix all file channels to mono for routing
      float monoSample = 0.0f;
      for (size_t ch = 0; ch < numFileChannels; ++ch) {
        size_t srcIndex = frame * numFileChannels + ch;
        monoSample += m_audioReadBuffer[srcIndex];
      }
      monoSample /= static_cast<float>(numFileChannels); // Average channels

      clipChannelBuffer[frame] = monoSample * gain;
    }
  }

  // Process routing matrix: clips → groups → master output
  m_routingMatrix->processRouting(
      const_cast<const float**>(m_clipChannelPointers.data()),
      outputBuffers,
      static_cast<uint32_t>(numFrames)
  );

  // Update clips
  size_t i = 0;
  while (i < m_activeClipCount) {
    ActiveClip& clip = m_activeClips[i];

    // Advance playback position
    clip.currentSample += static_cast<int64_t>(numFrames);

    // Apply fade-out if stopping
    if (clip.isStopping) {
      float fadeStep = 1.0f / static_cast<float>(m_fadeOutSamples);
      clip.fadeOutGain -= fadeStep * static_cast<float>(numFrames);

      if (clip.fadeOutGain <= 0.0f) {
        // Fade complete, remove clip
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
    if (clip.currentSample >= clip.trimOutSamples) {
      // TODO: Check if clip should loop
      // For now, just stop the clip
      postCallback([this, handle = clip.handle, pos = getCurrentPosition()]() {
        if (m_callback) {
          m_callback->onClipStopped(handle, pos);
        }
      });

      removeActiveClip(clip.handle);
      continue; // Don't increment i
    }

    ++i;
  }

  // Update transport position
  int64_t newSample =
      m_currentSample.load(std::memory_order_relaxed) + static_cast<int64_t>(numFrames);
  m_currentSample.store(newSample, std::memory_order_relaxed);
}

void TransportController::processCommands() {
  size_t readIndex = m_commandReadIndex.load(std::memory_order_relaxed);
  size_t writeIndex = m_commandWriteIndex.load(std::memory_order_acquire);

  while (readIndex != writeIndex) {
    const TransportCommand& cmd = m_commands[readIndex];

    switch (cmd.type) {
    case TransportCommand::Type::Start:
      addActiveClip(cmd.handle);
      postCallback([this, handle = cmd.handle, pos = getCurrentPosition()]() {
        if (m_callback) {
          m_callback->onClipStarted(handle, pos);
        }
      });
      break;

    case TransportCommand::Type::Stop: {
      ActiveClip* clip = findActiveClip(cmd.handle);
      if (clip) {
        clip->isStopping = true;
        clip->fadeOutGain = 1.0f;
      }
    } break;

    case TransportCommand::Type::StopAll:
      for (size_t i = 0; i < m_activeClipCount; ++i) {
        m_activeClips[i].isStopping = true;
        m_activeClips[i].fadeOutGain = 1.0f;
      }
      break;

    case TransportCommand::Type::StopGroup:
      // TODO: Get clip group assignments from SessionGraph
      // For now, this is a no-op
      break;
    }

    readIndex = (readIndex + 1) % MAX_COMMANDS;
    m_commandReadIndex.store(readIndex, std::memory_order_release);
  }
}

ActiveClip* TransportController::findActiveClip(ClipHandle handle) {
  for (size_t i = 0; i < m_activeClipCount; ++i) {
    if (m_activeClips[i].handle == handle) {
      return &m_activeClips[i];
    }
  }
  return nullptr;
}

void TransportController::addActiveClip(ClipHandle handle) {
  if (m_activeClipCount >= MAX_ACTIVE_CLIPS) {
    // TODO: Report error (too many active clips)
    return;
  }

  // Look up audio file reader and metadata for this clip
  // NOTE: Brief mutex lock in audio thread - only happens when starting clip, not during playback
  // TODO: Optimize to lock-free structure for production
  IAudioFileReader* reader = nullptr;
  uint16_t numChannels = 2; // Default stereo
  int64_t totalFrames = 48000 * 10; // Default 10 seconds

  {
    std::lock_guard<std::mutex> lock(m_audioFilesMutex);
    auto it = m_audioFiles.find(handle);
    if (it != m_audioFiles.end()) {
      reader = it->second.reader.get();
      numChannels = it->second.metadata.num_channels;
      totalFrames = it->second.metadata.duration_samples;
    }
  }

  // If no audio file registered, we'll play silence (reader will be nullptr)

  // TODO: Get clip metadata from SessionGraph (trim points, etc.)
  // For now, use placeholder values or query from audio file
  ActiveClip& clip = m_activeClips[m_activeClipCount++];
  clip.handle = handle;
  clip.startSample = m_currentSample.load(std::memory_order_relaxed);
  clip.currentSample = 0;
  clip.trimInSamples = 0;
  clip.trimOutSamples = totalFrames;
  clip.audioReader = reader;
  clip.numChannels = numChannels;
  clip.fadeOutGain = 1.0f;
  clip.isStopping = false;
}

void TransportController::removeActiveClip(ClipHandle handle) {
  for (size_t i = 0; i < m_activeClipCount; ++i) {
    if (m_activeClips[i].handle == handle) {
      // Remove by moving last clip into this slot
      if (i < m_activeClipCount - 1) {
        m_activeClips[i] = m_activeClips[m_activeClipCount - 1];
      }
      --m_activeClipCount;
      return;
    }
  }
}

void TransportController::postCallback(std::function<void()> callback) {
  std::lock_guard<std::mutex> lock(m_callbackMutex);
  m_callbackQueue.push(std::move(callback));
}

void TransportController::processCallbacks() {
  std::lock_guard<std::mutex> lock(m_callbackMutex);
  while (!m_callbackQueue.empty()) {
    auto callback = std::move(m_callbackQueue.front());
    m_callbackQueue.pop();
    callback();
  }
}

SessionGraphError TransportController::registerClipAudio(ClipHandle handle, const std::string& file_path) {
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  if (file_path.empty()) {
    return SessionGraphError::InvalidParameter;
  }

  std::lock_guard<std::mutex> lock(m_audioFilesMutex);

  // Create audio file reader
  auto reader = createAudioFileReader();
  auto result = reader->open(file_path);

  if (!result.isOk()) {
    return result.error;
  }

  // Store reader and metadata for this clip
  AudioFileEntry entry;
  entry.reader = std::move(reader);
  entry.metadata = result.value;
  m_audioFiles[handle] = std::move(entry);

  return SessionGraphError::OK;
}

// Factory function
std::unique_ptr<ITransportController> createTransportController(core::SessionGraph* sessionGraph,
                                                                uint32_t sampleRate) {
  return std::make_unique<TransportController>(sessionGraph, sampleRate);
}

} // namespace orpheus
