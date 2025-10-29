// SPDX-License-Identifier: MIT

#include "AudioEngine.h"
#include <orpheus/audio_driver.h>
#include <orpheus/transport_controller.h>

//==============================================================================
// Audio callback adapter: SDK's IAudioCallback implemented for TransportController
class AudioEngineCallback : public orpheus::IAudioCallback {
public:
  AudioEngineCallback(orpheus::ITransportController* transport) : m_transport(transport) {}

  void processAudio(const float** /*input_buffers*/, float** output_buffers, size_t num_channels,
                    size_t num_frames) override {
    // Clear output buffers first
    for (size_t ch = 0; ch < num_channels; ++ch) {
      for (size_t i = 0; i < num_frames; ++i) {
        output_buffers[ch][i] = 0.0f;
      }
    }

    // TODO (Week 5-6): Call m_transport->processAudio() to fill buffers with clip audio
    // For now, just silence (visual feedback will work, no audio yet)
  }

private:
  orpheus::ITransportController* m_transport;
};

//==============================================================================
AudioEngine::AudioEngine() {}

AudioEngine::~AudioEngine() {
  stop();
}

//==============================================================================
bool AudioEngine::initialize(uint32_t sampleRate, uint16_t bufferSize) {
  if (m_initialized) {
    DBG("AudioEngine: Already initialized");
    return false;
  }

  m_sampleRate = sampleRate;
  m_bufferSize = bufferSize;

  // Create transport controller (no session graph yet, we'll use dummy for now)
  m_transportController = orpheus::createTransportController(nullptr, sampleRate);
  if (!m_transportController) {
    DBG("AudioEngine: Failed to create transport controller");
    return false;
  }

  // Create platform-specific audio driver (CoreAudio on macOS, dummy otherwise)
#ifdef __APPLE__
  m_audioDriver = orpheus::createCoreAudioDriver();
  DBG("AudioEngine: Using CoreAudio driver (system default output)");
#else
  m_audioDriver = orpheus::createDummyAudioDriver();
  DBG("AudioEngine: Using dummy driver (no real audio on this platform yet)");
#endif

  if (!m_audioDriver) {
    DBG("AudioEngine: Failed to create audio driver");
    return false;
  }

  // Initialize audio driver
  orpheus::AudioDriverConfig config;
  config.sample_rate = sampleRate;
  config.buffer_size = bufferSize;
  config.num_inputs = 0;  // No inputs for soundboard MVP
  config.num_outputs = 2; // Stereo output

  auto result = m_audioDriver->initialize(config);
  if (result != orpheus::SessionGraphError::OK) {
    DBG("AudioEngine: Failed to initialize audio driver: " << static_cast<int>(result));
    return false;
  }

  m_initialized = true;
  DBG("AudioEngine: Initialized (" + juce::String(sampleRate) + " Hz, " + juce::String(bufferSize) +
      " samples)");
  return true;
}

bool AudioEngine::start() {
  if (!m_initialized) {
    DBG("AudioEngine: Not initialized, cannot start");
    return false;
  }

  if (m_audioDriver->isRunning()) {
    DBG("AudioEngine: Already running");
    return true;
  }

  // Create audio callback adapter
  m_audioCallback = std::make_unique<AudioEngineCallback>(m_transportController.get());

  auto result = m_audioDriver->start(m_audioCallback.get());
  if (result != orpheus::SessionGraphError::OK) {
    DBG("AudioEngine: Failed to start audio driver: " << static_cast<int>(result));
    return false;
  }

  DBG("AudioEngine: Started successfully");
  DBG("  Driver: " + juce::String(m_audioDriver->getDriverName()));
  DBG("  Sample Rate: " + juce::String(m_audioDriver->getConfig().sample_rate) + " Hz");
  DBG("  Buffer Size: " + juce::String(m_audioDriver->getConfig().buffer_size) + " samples");
  DBG("  Latency: " + juce::String(m_audioDriver->getLatencySamples()) + " samples (~" +
      juce::String(
          m_audioDriver->getLatencySamples() * 1000.0 / m_audioDriver->getConfig().sample_rate, 1) +
      " ms)");
  return true;
}

void AudioEngine::stop() {
  if (m_audioDriver && m_audioDriver->isRunning()) {
    m_audioDriver->stop();
    DBG("AudioEngine: Stopped");
  }
}

bool AudioEngine::isRunning() const {
  return m_audioDriver && m_audioDriver->isRunning();
}

//==============================================================================
bool AudioEngine::loadClip(const juce::String& filePath, int buttonIndex) {
  DBG("AudioEngine: Load clip requested: " << filePath << " → button " << buttonIndex);

  // Store clip metadata for later updates
  ClipMetadata metadata;
  metadata.filePath = filePath;
  m_clipMetadata[buttonIndex] = metadata;

  // TODO (Week 7-8): Implement clip loading with SessionGraph
  // For now, just store the metadata
  return true;
}

bool AudioEngine::triggerClip(int buttonIndex) {
  DBG("AudioEngine: Trigger clip on button " << buttonIndex);
  // TODO (Week 5-6): Call m_transportController->startClip(clipHandle)
  // For now, just log
  return true;
}

bool AudioEngine::stopClip(int buttonIndex) {
  DBG("AudioEngine: Stop clip on button " << buttonIndex);
  // TODO (Week 5-6): Call m_transportController->stopClip(clipHandle)
  return true;
}

bool AudioEngine::stopAllClips() {
  if (!m_transportController)
    return false;

  DBG("AudioEngine: Stop all clips");
  auto result = m_transportController->stopAllClips();
  return result == orpheus::SessionGraphError::OK;
}

bool AudioEngine::updateClipMetadata(int buttonIndex, int64_t trimInSamples, int64_t trimOutSamples,
                                     double fadeInSeconds, double fadeOutSeconds,
                                     const juce::String& fadeInCurve,
                                     const juce::String& fadeOutCurve) {
  // Find existing clip metadata
  auto it = m_clipMetadata.find(buttonIndex);
  if (it == m_clipMetadata.end()) {
    DBG("AudioEngine: Cannot update metadata - no clip loaded at button " << buttonIndex);
    return false;
  }

  // Update stored metadata
  it->second.trimInSamples = trimInSamples;
  it->second.trimOutSamples = trimOutSamples;
  it->second.fadeInSeconds = fadeInSeconds;
  it->second.fadeOutSeconds = fadeOutSeconds;
  it->second.fadeInCurve = fadeInCurve;
  it->second.fadeOutCurve = fadeOutCurve;

  DBG("AudioEngine: Updated clip metadata for button "
      << buttonIndex << " - Trim: [" << trimInSamples << ", " << trimOutSamples << "]"
      << ", Fade IN: " << fadeInSeconds << "s (" << fadeInCurve << ")"
      << ", Fade OUT: " << fadeOutSeconds << "s (" << fadeOutCurve << ")");

  // TODO (Week 7-8): Apply metadata to SDK's TransportController
  // This requires extending the SDK API to accept trim/fade parameters
  // For now, we store the metadata and it will be applied when SDK API is ready
  //
  // Proposed SDK API:
  //   m_transportController->updateClipTrimPoints(clipHandle, trimInSamples, trimOutSamples);
  //   m_transportController->updateClipFades(clipHandle, fadeInSec, fadeOutSec, inCurve, outCurve);

  return true;
}

//==============================================================================
int64_t AudioEngine::getCurrentPosition() const {
  if (!m_transportController)
    return 0;

  auto pos = m_transportController->getCurrentPosition();
  return pos.samples;
}

bool AudioEngine::isClipPlaying(int buttonIndex) const {
  // TODO (Week 5-6): Query m_transportController->getClipState(handle)
  return false;
}

float AudioEngine::getCpuUsage() const {
  // TODO (Month 4-5): Get from IPerformanceMonitor
  return 0.0f;
}

//==============================================================================
// Cue Buss Management (Preview Audio)

orpheus::ClipHandle AudioEngine::allocateCueBuss(const juce::String& filePath) {
  // Generate unique handle (starting from 10001 to distinguish from clip buttons 0-959)
  static orpheus::ClipHandle nextCueBussHandle = 10001;
  orpheus::ClipHandle handle = nextCueBussHandle++;

  // Store metadata
  ClipMetadata metadata;
  metadata.filePath = filePath;
  m_clipMetadata[handle] = metadata;

  DBG("AudioEngine: Allocated Cue Buss " << handle << " for " << filePath);
  return handle;
}

bool AudioEngine::releaseCueBuss(orpheus::ClipHandle handle) {
  auto it = m_clipMetadata.find(handle);
  if (it == m_clipMetadata.end()) {
    DBG("AudioEngine: Cannot release Cue Buss " << handle << " - not found");
    return false;
  }

  // Remove from metadata map
  m_clipMetadata.erase(it);

  DBG("AudioEngine: Released Cue Buss " << handle);
  return true;
}

bool AudioEngine::startCueBuss(orpheus::ClipHandle handle) {
  auto it = m_clipMetadata.find(handle);
  if (it == m_clipMetadata.end()) {
    DBG("AudioEngine: Cannot start Cue Buss " << handle << " - not found");
    return false;
  }

  // TODO (Week 5-6): Actual audio playback via TransportController
  // For now, just log (fixes Bug #3 & #4 by being idempotent)
  DBG("AudioEngine: Started Cue Buss " << handle);
  return true; // ALWAYS returns true (idempotent)
}

bool AudioEngine::stopCueBuss(orpheus::ClipHandle handle) {
  auto it = m_clipMetadata.find(handle);
  if (it == m_clipMetadata.end()) {
    DBG("AudioEngine: Cannot stop Cue Buss " << handle << " - not found");
    return false;
  }

  // TODO (Week 5-6): Actual audio stop via TransportController
  DBG("AudioEngine: Stopped Cue Buss " << handle);
  return true;
}

bool AudioEngine::updateCueBussMetadata(orpheus::ClipHandle handle, int64_t trimInSamples,
                                        int64_t trimOutSamples, float fadeInSeconds,
                                        float fadeOutSeconds, const juce::String& fadeInCurve,
                                        const juce::String& fadeOutCurve) {
  auto it = m_clipMetadata.find(handle);
  if (it == m_clipMetadata.end()) {
    DBG("AudioEngine: Cannot update Cue Buss metadata " << handle << " - not found");
    return false;
  }

  // Update stored metadata
  it->second.trimInSamples = trimInSamples;
  it->second.trimOutSamples = trimOutSamples;
  it->second.fadeInSeconds = fadeInSeconds;
  it->second.fadeOutSeconds = fadeOutSeconds;
  it->second.fadeInCurve = fadeInCurve;
  it->second.fadeOutCurve = fadeOutCurve;

  DBG("AudioEngine: Updated Cue Buss " << handle << " - Trim: [" << trimInSamples << ", "
                                       << trimOutSamples << "]");
  return true;
}

std::optional<AudioEngine::ClipMetadata> AudioEngine::getClipMetadata(int buttonIndex) const {
  auto it = m_clipMetadata.find(buttonIndex);
  if (it == m_clipMetadata.end()) {
    return std::nullopt;
  }
  return it->second;
}
