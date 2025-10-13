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

  // Create dummy audio driver (for visual feedback without real audio interface)
  m_audioDriver = orpheus::createDummyAudioDriver();
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

  DBG("AudioEngine: Started (driver: " + juce::String(m_audioDriver->getDriverName()) + ")");
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
  // TODO (Week 7-8): Implement clip loading with SessionGraph
  // For now, just log that we received the request
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
