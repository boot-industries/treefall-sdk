// SPDX-License-Identifier: MIT

#include "AudioEngine.h"
#include "../../../src/core/transport/transport_controller.h" // Concrete class for extended API
#include <orpheus/audio_driver.h>

//==============================================================================
AudioEngine::AudioEngine() {
  // Initialize clip handles to invalid
  m_clipHandles.fill(0);
}

AudioEngine::~AudioEngine() {
  stop();
}

//==============================================================================
bool AudioEngine::initialize(uint32_t sampleRate) {
  if (m_initialized)
    return true;

  m_sampleRate = sampleRate;

  // Create transport controller (concrete class for extended API)
  // TODO: Pass SessionGraph once integrated
  m_transportController =
      std::unique_ptr<orpheus::TransportController>(static_cast<orpheus::TransportController*>(
          orpheus::createTransportController(nullptr, sampleRate).release()));
  if (!m_transportController) {
    DBG("AudioEngine: Failed to create transport controller");
    return false;
  }

  // Register callback for transport events
  m_transportController->setCallback(this);

  // Create CoreAudio driver (macOS) for real audio output
  m_audioDriver = orpheus::createCoreAudioDriver();
  if (!m_audioDriver) {
    DBG("AudioEngine: Failed to create CoreAudio driver, falling back to dummy");
    m_audioDriver = orpheus::createDummyAudioDriver();
    if (!m_audioDriver) {
      DBG("AudioEngine: Failed to create audio driver");
      return false;
    }
  }

  // Configure driver
  orpheus::AudioDriverConfig config;
  config.sample_rate = sampleRate;
  config.buffer_size = 1024; // Larger buffer to prevent distortion
  config.num_inputs = 0;     // No input for now
  config.num_outputs = 2;    // Stereo output

  auto result = m_audioDriver->initialize(config);
  if (result != orpheus::SessionGraphError::OK) {
    DBG("AudioEngine: Failed to initialize audio driver");
    return false;
  }

  // Log which driver we're actually using
  DBG("AudioEngine: Using audio driver: " << m_audioDriver->getDriverName());

  m_initialized = true;
  DBG("AudioEngine: Initialized successfully (" << static_cast<int>(sampleRate) << " Hz)");
  return true;
}

bool AudioEngine::start() {
  if (!m_initialized) {
    DBG("AudioEngine: Cannot start - not initialized");
    return false;
  }

  if (m_audioDriver->isRunning())
    return true;

  auto result = m_audioDriver->start(this);
  if (result != orpheus::SessionGraphError::OK) {
    DBG("AudioEngine: Failed to start audio driver");
    return false;
  }

  DBG("AudioEngine: Started audio processing");
  return true;
}

void AudioEngine::stop() {
  if (m_audioDriver && m_audioDriver->isRunning()) {
    m_audioDriver->stop();
    DBG("AudioEngine: Stopped audio processing");
  }
}

bool AudioEngine::isRunning() const {
  return m_audioDriver && m_audioDriver->isRunning();
}

//==============================================================================
bool AudioEngine::loadClip(int buttonIndex, const juce::String& filePath) {
  if (buttonIndex < 0 || buttonIndex >= 48 || !m_transportController)
    return false;

  // Generate clip handle (buttonIndex + 1, since 0 is invalid)
  auto handle = static_cast<orpheus::ClipHandle>(buttonIndex + 1);

  // Register audio file with transport controller
  auto result = m_transportController->registerClipAudio(handle, filePath.toStdString());

  if (result != orpheus::SessionGraphError::OK) {
    DBG("AudioEngine: Failed to load clip: " << filePath);
    return false;
  }

  // Store handle and metadata
  m_clipHandles[buttonIndex] = handle;

  // Read metadata for UI
  auto reader = orpheus::createAudioFileReader();
  auto metadataResult = reader->open(filePath.toStdString());
  if (metadataResult.isOk()) {
    m_clipMetadata[buttonIndex] = metadataResult.value;
    DBG("AudioEngine: Clip " << buttonIndex
                             << " metadata: " << static_cast<int>(metadataResult.value.sample_rate)
                             << " Hz, " << static_cast<int>(metadataResult.value.num_channels)
                             << " ch, " << static_cast<int>(metadataResult.value.duration_samples)
                             << " samples");

    // Warn about sample rate mismatch
    if (metadataResult.value.sample_rate != m_sampleRate) {
      DBG("AudioEngine: WARNING - Sample rate mismatch! File is "
          << static_cast<int>(metadataResult.value.sample_rate) << " Hz, engine is running at "
          << static_cast<int>(m_sampleRate)
          << " Hz. Audio will sound distorted. Please convert file to "
          << static_cast<int>(m_sampleRate) << " Hz.");
    }
  }

  DBG("AudioEngine: Loaded clip to button " << buttonIndex << ": " << filePath);
  return true;
}

std::optional<orpheus::AudioFileMetadata> AudioEngine::getClipMetadata(int buttonIndex) const {
  if (buttonIndex >= 0 && buttonIndex < 48)
    return m_clipMetadata[buttonIndex];
  return std::nullopt;
}

void AudioEngine::unloadClip(int buttonIndex) {
  if (buttonIndex < 0 || buttonIndex >= 48)
    return;

  // Stop if playing
  if (isClipPlaying(buttonIndex))
    stopClip(buttonIndex);

  m_clipHandles[buttonIndex] = 0;
  m_clipMetadata[buttonIndex] = std::nullopt;

  // TODO: Unregister from transport controller (needs SDK API)

  DBG("AudioEngine: Unloaded clip from button " << buttonIndex);
}

//==============================================================================
bool AudioEngine::startClip(int buttonIndex) {
  if (buttonIndex < 0 || buttonIndex >= 48)
    return false;

  auto handle = m_clipHandles[buttonIndex];
  if (handle == 0) {
    DBG("AudioEngine: No clip loaded on button " << buttonIndex);
    return false;
  }

  if (!m_transportController)
    return false;

  auto result = m_transportController->startClip(handle);
  if (result != orpheus::SessionGraphError::OK) {
    DBG("AudioEngine: Failed to start clip " << handle);
    return false;
  }

  DBG("AudioEngine: Started clip on button " << buttonIndex);
  return true;
}

bool AudioEngine::stopClip(int buttonIndex) {
  if (buttonIndex < 0 || buttonIndex >= 48)
    return false;

  auto handle = m_clipHandles[buttonIndex];
  if (handle == 0)
    return false;

  if (!m_transportController)
    return false;

  auto result = m_transportController->stopClip(handle);
  if (result != orpheus::SessionGraphError::OK) {
    DBG("AudioEngine: Failed to stop clip " << handle);
    return false;
  }

  DBG("AudioEngine: Stopped clip on button " << buttonIndex);
  return true;
}

void AudioEngine::stopAllClips() {
  if (!m_transportController)
    return;

  m_transportController->stopAllClips();
  DBG("AudioEngine: Stopped all clips");
}

void AudioEngine::panicStop() {
  // TODO: Implement immediate mute (no fade-out)
  stopAllClips();
  DBG("AudioEngine: PANIC STOP");
}

//==============================================================================
bool AudioEngine::isClipPlaying(int buttonIndex) const {
  if (buttonIndex < 0 || buttonIndex >= 48 || !m_transportController)
    return false;

  auto handle = m_clipHandles[buttonIndex];
  if (handle == 0)
    return false;

  return m_transportController->isClipPlaying(handle);
}

orpheus::PlaybackState AudioEngine::getClipState(int buttonIndex) const {
  if (buttonIndex < 0 || buttonIndex >= 48 || !m_transportController)
    return orpheus::PlaybackState::Stopped;

  auto handle = m_clipHandles[buttonIndex];
  if (handle == 0)
    return orpheus::PlaybackState::Stopped;

  return m_transportController->getClipState(handle);
}

orpheus::TransportPosition AudioEngine::getCurrentPosition() const {
  if (!m_transportController)
    return {0, 0.0, 0.0};

  return m_transportController->getCurrentPosition();
}

//==============================================================================
void AudioEngine::onClipStarted(orpheus::ClipHandle handle, orpheus::TransportPosition position) {
  // Post to UI thread
  juce::MessageManager::callAsync([this, handle]() {
    int buttonIndex = getButtonIndexFromHandle(handle);
    if (buttonIndex >= 0 && onClipStateChanged)
      onClipStateChanged(buttonIndex, orpheus::PlaybackState::Playing);
  });
}

void AudioEngine::onClipStopped(orpheus::ClipHandle handle, orpheus::TransportPosition position) {
  // Post to UI thread
  juce::MessageManager::callAsync([this, handle]() {
    int buttonIndex = getButtonIndexFromHandle(handle);
    if (buttonIndex >= 0 && onClipStateChanged)
      onClipStateChanged(buttonIndex, orpheus::PlaybackState::Stopped);
  });
}

void AudioEngine::onClipLooped(orpheus::ClipHandle handle, orpheus::TransportPosition position) {
  DBG("AudioEngine: Clip " << handle << " looped at " << position.samples << " samples");
}

void AudioEngine::onBufferUnderrun(orpheus::TransportPosition position) {
  juce::MessageManager::callAsync([this]() {
    DBG("AudioEngine: Buffer underrun!");
    if (onBufferUnderrunDetected)
      onBufferUnderrunDetected();
  });
}

//==============================================================================
void AudioEngine::processAudio(const float** input_buffers, float** output_buffers,
                               size_t num_channels, size_t num_frames) {
  static int audioEngineCallCount = 0;
  if (audioEngineCallCount < 3) {
    // Write to file since stdout might not work
    FILE* f = fopen("/tmp/audio_callback.txt", "a");
    if (f) {
      fprintf(f, "AudioEngine::processAudio() CALLED #%d: frames=%zu, transport=%p\n",
              audioEngineCallCount, num_frames, (void*)m_transportController.get());
      fclose(f);
    }
    audioEngineCallCount++;
  }

  if (!m_transportController) {
    FILE* f = fopen("/tmp/audio_callback.txt", "a");
    if (f) {
      fprintf(f, "ERROR: No transport controller!\n");
      fclose(f);
    }
    // No transport - output silence
    for (size_t ch = 0; ch < num_channels; ++ch) {
      if (output_buffers[ch])
        std::memset(output_buffers[ch], 0, num_frames * sizeof(float));
    }
    return;
  }

  // Call SDK transport controller for real audio processing!
  m_transportController->processAudio(output_buffers, num_channels, num_frames);

  // Process any pending callbacks (posts to UI thread)
  m_transportController->processCallbacks();
}

//==============================================================================
orpheus::ClipHandle AudioEngine::getClipHandle(int buttonIndex) const {
  if (buttonIndex >= 0 && buttonIndex < 48)
    return m_clipHandles[buttonIndex];
  return 0;
}

int AudioEngine::getButtonIndexFromHandle(orpheus::ClipHandle handle) const {
  for (int i = 0; i < 48; ++i) {
    if (m_clipHandles[i] == handle)
      return i;
  }
  return -1;
}
