// SPDX-License-Identifier: MIT

#include "AudioEngine.h"
#define private public
#include "../../../src/core/transport/transport_controller.h" // Concrete class for extended API
#undef private
#include <algorithm> // For std::find
#include <chrono>
#include <cmath>
#include <dlfcn.h>
#include <limits>
#include <orpheus/audio_driver.h>
#include <orpheus/audio_driver_manager.h>

namespace {

constexpr const char* kDefaultDeviceName = "Default Device";

std::unique_ptr<orpheus::IAudioDriverManager> tryCreateAudioDriverManager() {
  using CreateAudioDriverManagerFn = std::unique_ptr<orpheus::IAudioDriverManager> (*)();
  auto* symbol = dlsym(RTLD_DEFAULT, "__ZN7orpheus24createAudioDriverManagerEv");
  if (symbol == nullptr) {
    return nullptr;
  }

  auto* createManager = reinterpret_cast<CreateAudioDriverManagerFn>(symbol);
  return createManager();
}

float dbToLinear(float db) {
  if (!std::isfinite(db) || db <= -90.0f) {
    return 0.0f;
  }

  return std::pow(10.0f, db / 20.0f);
}

orpheus::FadeCurve toFadeCurve(const juce::String& curveName) {
  if (curveName == "EqualPower")
    return orpheus::FadeCurve::EqualPower;
  if (curveName == "Exponential")
    return orpheus::FadeCurve::Exponential;
  return orpheus::FadeCurve::Linear;
}

std::string describeGraphError(orpheus::SessionGraphError error) {
  switch (error) {
  case orpheus::SessionGraphError::OK:
    return "OK";
  case orpheus::SessionGraphError::InvalidHandle:
    return "Invalid handle";
  case orpheus::SessionGraphError::InvalidParameter:
    return "Invalid parameter";
  case orpheus::SessionGraphError::NotReady:
    return "Not ready";
  case orpheus::SessionGraphError::NotSupported:
    return "Not supported";
  case orpheus::SessionGraphError::NotInitialized:
    return "Not initialized";
  case orpheus::SessionGraphError::InvalidClipTrimPoints:
    return "Invalid trim points";
  case orpheus::SessionGraphError::InvalidFadeDuration:
    return "Invalid fade duration";
  case orpheus::SessionGraphError::ClipNotRegistered:
    return "Clip not registered";
  case orpheus::SessionGraphError::InternalError:
    return "Internal error";
  }

  return "Unknown error";
}

} // namespace

//==============================================================================
AudioEngine::AudioEngine() {
  // Initialize clip handles to invalid
  m_clipHandles.fill(0);
  m_audioAnalyzer = std::make_unique<shmui::AudioAnalyzer>();
  updateDeviceStatus(kDefaultDeviceName, {});
}

AudioEngine::~AudioEngine() {
  stop();
}

//==============================================================================
bool AudioEngine::createConfiguredTransport(
    uint32_t sampleRate, std::unique_ptr<orpheus::TransportController>& transport,
    std::string& errorMessage) const {
  transport =
      std::unique_ptr<orpheus::TransportController>(static_cast<orpheus::TransportController*>(
          orpheus::createTransportController(nullptr, sampleRate).release()));

  if (!transport) {
    errorMessage = "Failed to create transport controller";
    return false;
  }

  // OCC151 T11: cap voices per clip to 2 for OCC's model — one primary voice
  // plus, at most, one fading-out tail during a fade-overlap. The SDK default is
  // 8; OCC never needs polyphonic layering. Applied here so the cap holds for
  // both the initial transport and any transport rebuilt on a device change.
  transport->setMaxVoicesPerClip(2);

  transport->setCallback(const_cast<AudioEngine*>(this));
  return true;
}

bool AudioEngine::createConfiguredDriver(const std::string& deviceName, uint32_t sampleRate,
                                         uint32_t bufferSize,
                                         std::unique_ptr<orpheus::IAudioDriver>& driver,
                                         std::string& errorMessage,
                                         bool& usingFallbackDriver) const {
  usingFallbackDriver = false;

  driver = orpheus::createCoreAudioDriver();
  if (!driver) {
    driver = orpheus::createDummyAudioDriver();
    usingFallbackDriver = true;
  }

  if (!driver) {
    errorMessage = "Failed to create audio driver";
    return false;
  }

  orpheus::AudioDriverConfig config;
  config.sample_rate = sampleRate;
  config.buffer_size = static_cast<uint16_t>(bufferSize);
  config.num_inputs = 0;
  config.num_outputs = 2;

  if (!deviceName.empty() && deviceName != kDefaultDeviceName) {
    config.device_name = deviceName;
  }

  const auto result = driver->initialize(config);
  if (result != orpheus::SessionGraphError::OK) {
    errorMessage = "Failed to initialize audio driver: " + describeGraphError(result);
    return false;
  }

  return true;
}

void AudioEngine::updateCachedClipMetadata(int buttonIndex) {
  if (buttonIndex < 0 || buttonIndex >= MAX_CLIP_BUTTONS || !m_transportController) {
    return;
  }

  const auto handle = m_clipHandles[buttonIndex];
  if (handle == 0 || !m_clipRegistrations[buttonIndex].has_value()) {
    return;
  }

  if (auto metadata = m_transportController->getClipMetadata(handle)) {
    m_clipRegistrations[buttonIndex]->metadata = *metadata;
  }
}

void AudioEngine::updateCachedCueMetadata(int cueSlot) {
  if (cueSlot < 0 || cueSlot >= MAX_CUE_BUSSES || !m_transportController) {
    return;
  }

  const auto handle = m_cueBussPool[cueSlot].handle;
  if (handle == 0) {
    return;
  }

  if (auto metadata = m_transportController->getClipMetadata(handle)) {
    m_cueBussPool[cueSlot].transportMetadata = *metadata;
  }
}

bool AudioEngine::rehydrateTransportState(orpheus::TransportController& transport,
                                          const orpheus::TransportController* previousTransport,
                                          std::vector<orpheus::ClipHandle>& handlesToRestart,
                                          std::string& errorMessage) const {
  for (int buttonIndex = 0; buttonIndex < MAX_CLIP_BUTTONS; ++buttonIndex) {
    const auto handle = m_clipHandles[buttonIndex];
    if (handle == 0 || !m_clipRegistrations[buttonIndex].has_value()) {
      continue;
    }

    const auto& registration = *m_clipRegistrations[buttonIndex];
    auto result = transport.registerClipAudio(handle, registration.filePath.toStdString());
    if (result != orpheus::SessionGraphError::OK) {
      errorMessage = "Failed to restore clip " + std::to_string(buttonIndex) + ": " +
                     describeGraphError(result);
      return false;
    }

    // OCC151 T10: preserve the MonoWithFadeOverlap policy across device swaps —
    // the new transport starts fresh, so the grid clip's voice mode must be
    // re-applied after re-registration (matches loadClip()).
    transport.setClipVoiceMode(handle, orpheus::VoiceMode::MonoWithFadeOverlap);

    result = transport.updateClipMetadata(handle, registration.metadata);
    if (result != orpheus::SessionGraphError::OK) {
      errorMessage = "Failed to restore clip metadata for slot " + std::to_string(buttonIndex) +
                     ": " + describeGraphError(result);
      return false;
    }

    if (previousTransport && previousTransport->isClipPlaying(handle)) {
      handlesToRestart.push_back(handle);
    }
  }

  for (int cueSlot = 0; cueSlot < MAX_CUE_BUSSES; ++cueSlot) {
    const auto handle = m_cueBussPool[cueSlot].handle;
    if (handle == 0 || m_cueBussPool[cueSlot].filePath.isEmpty()) {
      continue;
    }

    auto result =
        transport.registerClipAudio(handle, m_cueBussPool[cueSlot].filePath.toStdString());
    if (result != orpheus::SessionGraphError::OK) {
      errorMessage = "Failed to restore preview bus " + std::to_string(cueSlot) + ": " +
                     describeGraphError(result);
      return false;
    }

    result = transport.updateClipMetadata(handle, m_cueBussPool[cueSlot].transportMetadata);
    if (result != orpheus::SessionGraphError::OK) {
      errorMessage = "Failed to restore preview metadata for slot " + std::to_string(cueSlot) +
                     ": " + describeGraphError(result);
      return false;
    }

    if (previousTransport && previousTransport->isClipPlaying(handle)) {
      handlesToRestart.push_back(handle);
    }
  }

  return true;
}

std::optional<orpheus::AudioDeviceInfo>
AudioEngine::findDeviceDetails(const std::string& deviceName) const {
  auto manager = tryCreateAudioDriverManager();
  if (!manager) {
    return std::nullopt;
  }

  const auto devices = manager->enumerateDevices();
  if (deviceName.empty() || deviceName == kDefaultDeviceName) {
    const auto defaultIt = std::find_if(devices.begin(), devices.end(), [](const auto& device) {
      return device.driverType == "CoreAudio" && device.isDefaultDevice;
    });
    if (defaultIt != devices.end()) {
      return *defaultIt;
    }

    return std::nullopt;
  }

  const auto it = std::find_if(devices.begin(), devices.end(), [&deviceName](const auto& device) {
    return device.driverType == "CoreAudio" && device.name == deviceName;
  });
  if (it != devices.end()) {
    return *it;
  }

  return std::nullopt;
}

void AudioEngine::updateDeviceStatus(const std::string& requestedDeviceName,
                                     const std::string& errorMessage) {
  m_deviceStatus.initialized = m_initialized;
  m_deviceStatus.running = isRunning();
  m_deviceStatus.requestedDeviceName =
      requestedDeviceName.empty() ? std::string(kDefaultDeviceName) : requestedDeviceName;
  m_deviceStatus.requestedDefaultDevice = m_deviceStatus.requestedDeviceName == kDefaultDeviceName;
  m_deviceStatus.activeDeviceName =
      m_currentDeviceName.empty() ? std::string(kDefaultDeviceName) : m_currentDeviceName;
  m_deviceStatus.driverName = m_audioDriver ? m_audioDriver->getDriverName() : "Unavailable";
  m_deviceStatus.usingFallbackDriver = (m_deviceStatus.driverName == "Dummy");
  m_deviceStatus.sampleRate = m_sampleRate;
  m_deviceStatus.bufferSize = m_bufferSize;
  m_deviceStatus.latencySamples = getLatencySamples();
  m_deviceStatus.lastError = errorMessage;

  if (!errorMessage.empty()) {
    m_deviceStatus.summary = errorMessage;
  } else if (!m_initialized) {
    m_deviceStatus.summary = "Audio engine not initialized";
  } else {
    const double latencyMs =
        (static_cast<double>(m_deviceStatus.latencySamples) / std::max(1u, m_sampleRate)) * 1000.0;
    m_deviceStatus.summary = m_deviceStatus.activeDeviceName + " via " + m_deviceStatus.driverName +
                             " @ " + std::to_string(m_sampleRate) + " Hz / " +
                             std::to_string(m_bufferSize) + " samples (" +
                             juce::String(latencyMs, 2).toStdString() + " ms)";
  }
}

bool AudioEngine::initialize(uint32_t sampleRate) {
  if (m_initialized)
    return true;

  m_sampleRate = sampleRate;
  m_bufferSize = 512;

  m_rmsLevels.resize(2, 0.0f);
  m_peakLevels.resize(2, 0.0f);

  std::string errorMessage;
  if (!createConfiguredTransport(sampleRate, m_transportController, errorMessage)) {
    DBG("AudioEngine: " << errorMessage);
    updateDeviceStatus(kDefaultDeviceName, errorMessage);
    return false;
  }

  bool usingFallbackDriver = false;
  if (!createConfiguredDriver(kDefaultDeviceName, sampleRate, m_bufferSize, m_audioDriver,
                              errorMessage, usingFallbackDriver)) {
    DBG("AudioEngine: " << errorMessage);
    updateDeviceStatus(kDefaultDeviceName, errorMessage);
    return false;
  }

  m_performanceMonitor = orpheus::createPerformanceMonitor(nullptr);
  m_initialized = true;

  updateDeviceStatus(kDefaultDeviceName, {});
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
    updateDeviceStatus(m_currentDeviceName,
                       "Failed to start audio driver: " + describeGraphError(result));
    return false;
  }

  updateDeviceStatus(m_currentDeviceName, {});
  DBG("AudioEngine: Started audio processing");
  return true;
}

void AudioEngine::stop() {
  if (m_audioDriver && m_audioDriver->isRunning()) {
    m_audioDriver->stop();
    updateDeviceStatus(m_currentDeviceName, {});
    DBG("AudioEngine: Stopped audio processing");
  }
}

bool AudioEngine::isRunning() const {
  return m_audioDriver && m_audioDriver->isRunning();
}

//==============================================================================
bool AudioEngine::loadClip(int buttonIndex, const juce::String& filePath) {
  if (buttonIndex < 0 || buttonIndex >= AudioEngine::MAX_CLIP_BUTTONS || !m_transportController)
    return false;

  // Generate clip handle (buttonIndex + 1, since 0 is invalid)
  auto handle = static_cast<orpheus::ClipHandle>(buttonIndex + 1);

  // Register audio file with transport controller
  auto result = m_transportController->registerClipAudio(handle, filePath.toStdString());

  if (result != orpheus::SessionGraphError::OK) {
    DBG("AudioEngine: Failed to load clip: " << filePath);
    return false;
  }

  // OCC151 T10: adopt the ORP127 MonoWithFadeOverlap voice policy — OCC's
  // canonical model. One primary voice per clip identity: firing while playing
  // restarts in place; firing while a voice is fading out leaves that tail to
  // complete and starts a fresh voice alongside it (voices == 2 only during the
  // fade-overlap window). The SDK now enforces this, superseding the local
  // restart-if-playing dedup wrapper in startClip().
  m_transportController->setClipVoiceMode(handle, orpheus::VoiceMode::MonoWithFadeOverlap);

  // Store handle and metadata
  m_clipHandles[buttonIndex] = handle;
  m_clipRegistrations[buttonIndex] = ClipRegistrationState{filePath, {}};

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

    // Pre-seek to start of file (warm up OS page cache, reduce first-play latency)
    reader->seek(0);
  }

  updateCachedClipMetadata(buttonIndex);

  DBG("AudioEngine: Loaded clip to button " << buttonIndex << ": " << filePath);
  return true;
}

std::optional<orpheus::AudioFileMetadata> AudioEngine::getClipMetadata(int buttonIndex) const {
  if (buttonIndex >= 0 && buttonIndex < AudioEngine::MAX_CLIP_BUTTONS)
    return m_clipMetadata[buttonIndex];
  return std::nullopt;
}

void AudioEngine::unloadClip(int buttonIndex) {
  if (buttonIndex < 0 || buttonIndex >= AudioEngine::MAX_CLIP_BUTTONS)
    return;

  // Stop if playing
  if (isClipPlaying(buttonIndex))
    stopClip(buttonIndex);

  m_clipHandles[buttonIndex] = 0;
  m_clipMetadata[buttonIndex] = std::nullopt;
  m_clipRegistrations[buttonIndex] = std::nullopt;

  // TODO: Unregister from transport controller (needs SDK API)

  DBG("AudioEngine: Unloaded clip from button " << buttonIndex);
}

bool AudioEngine::updateClipMetadata(int buttonIndex, int64_t trimInSamples, int64_t trimOutSamples,
                                     double fadeInSeconds, double fadeOutSeconds,
                                     const juce::String& fadeInCurve,
                                     const juce::String& fadeOutCurve) {
  if (buttonIndex < 0 || buttonIndex >= AudioEngine::MAX_CLIP_BUTTONS)
    return false;

  auto handle = m_clipHandles[buttonIndex];
  if (handle == 0) {
    DBG("AudioEngine: Cannot update metadata - no clip loaded at button " << buttonIndex);
    return false;
  }

  if (!m_transportController) {
    DBG("AudioEngine: No transport controller");
    return false;
  }

  const auto fadeInCurveEnum = toFadeCurve(fadeInCurve);
  const auto fadeOutCurveEnum = toFadeCurve(fadeOutCurve);

  // Call SDK methods to update trim points
  auto trimResult =
      m_transportController->updateClipTrimPoints(handle, trimInSamples, trimOutSamples);
  if (trimResult != orpheus::SessionGraphError::OK) {
    DBG("AudioEngine: Failed to update trim points: " << static_cast<int>(trimResult));
    return false;
  }

  // CRITICAL: Validate fade times don't exceed trim duration
  int64_t trimDurationSamples = trimOutSamples - trimInSamples;
  double trimDurationSeconds = static_cast<double>(trimDurationSamples) / m_sampleRate;

  // Clamp fade times to fit within trim duration
  double clampedFadeInSeconds = fadeInSeconds;
  double clampedFadeOutSeconds = fadeOutSeconds;

  if (fadeInSeconds + fadeOutSeconds > trimDurationSeconds) {
    // Scale down proportionally to fit within trim duration
    double ratio = trimDurationSeconds / (fadeInSeconds + fadeOutSeconds);
    clampedFadeInSeconds = fadeInSeconds * ratio;
    clampedFadeOutSeconds = fadeOutSeconds * ratio;

    DBG("AudioEngine: Clamped fade times for button "
        << buttonIndex << " - Requested: IN " << fadeInSeconds << "s, OUT " << fadeOutSeconds << "s"
        << " | Clamped: IN " << clampedFadeInSeconds << "s, OUT " << clampedFadeOutSeconds << "s"
        << " (trim duration: " << trimDurationSeconds << "s)");
  }

  // Call SDK method to update fades with validated values
  auto fadeResult = m_transportController->updateClipFades(
      handle, clampedFadeInSeconds, clampedFadeOutSeconds, fadeInCurveEnum, fadeOutCurveEnum);
  if (fadeResult != orpheus::SessionGraphError::OK) {
    DBG("AudioEngine: Failed to update fades: " << static_cast<int>(fadeResult));
    return false;
  }

  DBG("AudioEngine: Successfully updated clip metadata for button "
      << buttonIndex << " - Trim: [" << trimInSamples << ", " << trimOutSamples << "]"
      << ", Fade IN: " << fadeInSeconds << "s (" << fadeInCurve << ")"
      << ", Fade OUT: " << fadeOutSeconds << "s (" << fadeOutCurve << ")");

  updateCachedClipMetadata(buttonIndex);

  return true;
}

//==============================================================================
bool AudioEngine::startClip(int buttonIndex) {
  if (buttonIndex < 0 || buttonIndex >= AudioEngine::MAX_CLIP_BUTTONS)
    return false;

  auto handle = m_clipHandles[buttonIndex];
  if (handle == 0) {
    DBG("AudioEngine: No clip loaded on button " << buttonIndex);
    return false;
  }

  if (!m_transportController)
    return false;

  // OCC151 T10: thin passthrough to the SDK. The local "restart if already
  // playing" dedup wrapper is retired — clips are registered with
  // VoiceMode::MonoWithFadeOverlap (see loadClip), so the SDK's startClip now
  // enforces the one-voice-per-clip model itself: firing a live voice restarts
  // it in place (short crossfade), and firing while only a fade-out tail exists
  // starts a fresh voice alongside the tail (voices == 2 only during the fade
  // overlap). This keeps a single source of truth in the SDK and eliminates the
  // OCC-side isClipPlaying()/restartClip() branch that duplicated it.
  auto result = m_transportController->startClip(handle);
  if (result != orpheus::SessionGraphError::OK) {
    DBG("AudioEngine: Failed to start clip " << handle);
    return false;
  }

  DBG("AudioEngine: Started clip on button " << buttonIndex);
  return true;
}

bool AudioEngine::stopClip(int buttonIndex) {
  if (buttonIndex < 0 || buttonIndex >= AudioEngine::MAX_CLIP_BUTTONS)
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

void AudioEngine::drainTransportCallbacks() {
  // OCC151 T5 / F-APP-3: message-thread-only consumer of the SPSC callback ring.
  // Assert (Debug) that we are NOT on the audio thread. m_audioThreadId is stamped
  // by processAudio(); if it is unset (audio never ran) the guard is a no-op.
  const auto audioThreadId = m_audioThreadId.load(std::memory_order_relaxed);
  jassert(audioThreadId == std::thread::id{} || std::this_thread::get_id() != audioThreadId);

  if (m_transportController) {
    m_transportController->processCallbacks();
  }
}

//==============================================================================
bool AudioEngine::isClipPlaying(int buttonIndex) const {
  if (buttonIndex < 0 || buttonIndex >= AudioEngine::MAX_CLIP_BUTTONS || !m_transportController)
    return false;

  auto handle = m_clipHandles[buttonIndex];
  if (handle == 0)
    return false;

  return m_transportController->isClipPlaying(handle);
}

bool AudioEngine::isClipLooping(int buttonIndex) const {
  if (buttonIndex < 0 || buttonIndex >= AudioEngine::MAX_CLIP_BUTTONS || !m_transportController)
    return false;

  auto handle = m_clipHandles[buttonIndex];
  if (handle == 0)
    return false;

  // Use SDK's isClipLooping() API (Phase 7 of ORP085)
  return m_transportController->isClipLooping(handle);
}

bool AudioEngine::setClipLoopMode(int buttonIndex, bool shouldLoop) {
  if (buttonIndex < 0 || buttonIndex >= AudioEngine::MAX_CLIP_BUTTONS || !m_transportController)
    return false;

  auto handle = m_clipHandles[buttonIndex];
  if (handle == 0) {
    DBG("AudioEngine: Cannot set loop mode - no clip loaded at button " << buttonIndex);
    return false;
  }

  auto result = m_transportController->setClipLoopMode(handle, shouldLoop);
  if (result != orpheus::SessionGraphError::OK) {
    DBG("AudioEngine: Failed to set loop mode for button " << buttonIndex);
    return false;
  }

  DBG("AudioEngine: Set button " << buttonIndex << " loop mode to "
                                 << (shouldLoop ? "enabled" : "disabled"));
  updateCachedClipMetadata(buttonIndex);
  return true;
}

int64_t AudioEngine::getClipPosition(int buttonIndex) const {
  if (buttonIndex < 0 || buttonIndex >= AudioEngine::MAX_CLIP_BUTTONS || !m_transportController)
    return -1;

  auto handle = m_clipHandles[buttonIndex];
  if (handle == 0)
    return -1;

  // Use SDK's getClipPosition() API
  return m_transportController->getClipPosition(handle);
}

bool AudioEngine::seekClip(int buttonIndex, int64_t position) {
  if (buttonIndex < 0 || buttonIndex >= AudioEngine::MAX_CLIP_BUTTONS || !m_transportController)
    return false;

  auto handle = m_clipHandles[buttonIndex];
  if (handle == 0) {
    DBG("AudioEngine: Cannot seek - no clip loaded at button " << buttonIndex);
    return false;
  }

  // Use SDK's seekClip() API (ORP089 - gap-free, sample-accurate seek)
  auto result = m_transportController->seekClip(handle, position);
  if (result != orpheus::SessionGraphError::OK) {
    DBG("AudioEngine: Failed to seek clip on button " << buttonIndex << " to position "
                                                      << position);
    return false;
  }

  DBG("AudioEngine: Seeked button " << buttonIndex << " to position " << position
                                    << " (gap-free, sample-accurate)");
  return true;
}

orpheus::PlaybackState AudioEngine::getClipState(int buttonIndex) const {
  if (buttonIndex < 0 || buttonIndex >= AudioEngine::MAX_CLIP_BUTTONS || !m_transportController)
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

uint32_t AudioEngine::getLatencySamples() const {
  if (!m_audioDriver)
    return m_bufferSize; // Conservative estimate if driver not initialized

  return m_audioDriver->getLatencySamples();
}

uint32_t AudioEngine::getBufferSize() const {
  return m_bufferSize;
}

orpheus::PerformanceMetrics AudioEngine::getPerformanceMetrics() const {
  if (m_performanceMonitor)
    return m_performanceMonitor->getMetrics();

  return orpheus::PerformanceMetrics{0.0f, 0.0f, 0, 0, 0, 0.0};
}

uint32_t AudioEngine::getSampleRate() const {
  return m_sampleRate;
}

const std::vector<float>& AudioEngine::getRmsLevels() const {
  return m_rmsLevels;
}

const std::vector<float>& AudioEngine::getPeakLevels() const {
  return m_peakLevels;
}

//==============================================================================
// OCC144: Level Metering

float AudioEngine::getMasterRmsLevel() const {
  if (m_audioAnalyzer) {
    return m_audioAnalyzer->getRMSLevel();
  }
  return 0.0f;
}

float AudioEngine::getMasterPeakLevel() const {
  if (m_audioAnalyzer) {
    return m_audioAnalyzer->getPeakLevel();
  }
  return 0.0f;
}

void AudioEngine::getGroupLevels(std::array<float, 4>& groupLevels) const {
  groupLevels.fill(0.0f);

  if (!m_transportController || !m_transportController->m_routingMatrix) {
    return;
  }

  for (uint8_t groupIndex = 0; groupIndex < 4; ++groupIndex) {
    const auto meter = m_transportController->m_routingMatrix->getGroupMeter(groupIndex);
    groupLevels[groupIndex] =
        juce::jlimit(0.0f, 1.5f, std::max(dbToLinear(meter.peak_db), dbToLinear(meter.rms_db)));
  }
}

//==============================================================================
// OCC149c: Group routing control
//
// The SDK's IRoutingMatrix exposes setters for mute/solo/gain but only one
// getter (isGroupMuted). We shadow solo and gain locally so the inspector
// snapshot can render M·S pills and gain readouts without round-tripping
// through saveSnapshot() each poll.

bool AudioEngine::setGroupMute(uint8_t groupIndex, bool mute) {
  if (groupIndex >= kGroupCount || !m_transportController ||
      !m_transportController->m_routingMatrix)
    return false;
  return m_transportController->m_routingMatrix->setGroupMute(groupIndex, mute) ==
         orpheus::SessionGraphError::OK;
}

bool AudioEngine::setGroupSolo(uint8_t groupIndex, bool solo) {
  if (groupIndex >= kGroupCount || !m_transportController ||
      !m_transportController->m_routingMatrix)
    return false;
  const bool ok = m_transportController->m_routingMatrix->setGroupSolo(groupIndex, solo) ==
                  orpheus::SessionGraphError::OK;
  if (ok)
    m_groupSoloCache[groupIndex] = solo;
  return ok;
}

bool AudioEngine::setGroupGain(uint8_t groupIndex, float gainDb) {
  if (groupIndex >= kGroupCount || !m_transportController ||
      !m_transportController->m_routingMatrix)
    return false;
  const bool ok = m_transportController->m_routingMatrix->setGroupGain(groupIndex, gainDb) ==
                  orpheus::SessionGraphError::OK;
  if (ok)
    m_groupGainDbCache[groupIndex] = gainDb;
  return ok;
}

bool AudioEngine::isGroupMuted(uint8_t groupIndex) const {
  if (groupIndex >= kGroupCount || !m_transportController ||
      !m_transportController->m_routingMatrix)
    return false;
  return m_transportController->m_routingMatrix->isGroupMuted(groupIndex);
}

bool AudioEngine::isGroupSoloed(uint8_t groupIndex) const {
  if (groupIndex >= kGroupCount)
    return false;
  return m_groupSoloCache[groupIndex];
}

float AudioEngine::getGroupGainDb(uint8_t groupIndex) const {
  if (groupIndex >= kGroupCount)
    return 0.0f;
  return m_groupGainDbCache[groupIndex];
}

juce::String AudioEngine::getGroupOutputLabel(uint8_t groupIndex) const {
  // TODO(occ149c-routing-outputs): when per-group output bus assignment is
  // exposed in the UI, swap this for the real bus label. Today
  // transport_controller.cpp routes all four groups to the master bus, so the
  // operator-true answer is "Main L/R" for every row.
  juce::ignoreUnused(groupIndex);
  return "Main L/R";
}

//==============================================================================
// Audio Device Management (for Audio Settings Dialog)

std::vector<std::string> AudioEngine::getAvailableDevices() const {
  auto manager = tryCreateAudioDriverManager();
  if (!manager) {
    return {"Default Device"};
  }

  std::vector<std::string> names;
  names.push_back("Default Device"); // Always first — lets user follow system default

  for (const auto& device : manager->enumerateDevices()) {
    if (device.driverType == "CoreAudio") {
      names.push_back(device.name);
    }
  }
  return names;
}

std::string AudioEngine::getCurrentDeviceName() const {
  return m_currentDeviceName;
}

bool AudioEngine::setAudioDevice(const std::string& deviceName, uint32_t sampleRate,
                                 uint32_t bufferSize) {
  if (!m_initialized) {
    updateDeviceStatus(deviceName, "Audio engine must be initialized before changing devices");
    return false;
  }

  const std::string requestedDeviceName =
      deviceName.empty() ? std::string(kDefaultDeviceName) : deviceName;

  if (requestedDeviceName != kDefaultDeviceName) {
    const auto requestedInfo = findDeviceDetails(requestedDeviceName);
    if (!requestedInfo.has_value()) {
      updateDeviceStatus(requestedDeviceName, "Requested device is no longer available");
      return false;
    }

    if (!requestedInfo->supportedSampleRates.empty() &&
        std::find(requestedInfo->supportedSampleRates.begin(),
                  requestedInfo->supportedSampleRates.end(),
                  sampleRate) == requestedInfo->supportedSampleRates.end()) {
      updateDeviceStatus(requestedDeviceName, "Requested sample rate is not supported by device");
      return false;
    }

    if (!requestedInfo->supportedBufferSizes.empty() &&
        std::find(requestedInfo->supportedBufferSizes.begin(),
                  requestedInfo->supportedBufferSizes.end(),
                  bufferSize) == requestedInfo->supportedBufferSizes.end()) {
      updateDeviceStatus(requestedDeviceName, "Requested buffer size is not supported by device");
      return false;
    }
  }

  DBG("AudioEngine: Changing audio settings - Device: "
      << requestedDeviceName << ", Sample Rate: " << static_cast<int>(sampleRate)
      << " Hz, Buffer Size: " << static_cast<int>(bufferSize));

  auto oldTransport = std::move(m_transportController);
  auto oldDriver = std::move(m_audioDriver);
  const auto* previousTransport = oldTransport.get();
  const auto oldSampleRate = m_sampleRate;
  const auto oldBufferSize = m_bufferSize;
  const auto oldDeviceName = m_currentDeviceName;
  const auto oldDeviceStatus = m_deviceStatus;

  std::string errorMessage;
  std::unique_ptr<orpheus::TransportController> newTransport;
  if (!createConfiguredTransport(sampleRate, newTransport, errorMessage)) {
    m_transportController = std::move(oldTransport);
    m_audioDriver = std::move(oldDriver);
    updateDeviceStatus(requestedDeviceName, errorMessage);
    return false;
  }

  std::vector<orpheus::ClipHandle> handlesToRestart;
  if (!rehydrateTransportState(*newTransport, previousTransport, handlesToRestart, errorMessage)) {
    m_transportController = std::move(oldTransport);
    m_audioDriver = std::move(oldDriver);
    updateDeviceStatus(requestedDeviceName, errorMessage);
    return false;
  }

  std::unique_ptr<orpheus::IAudioDriver> newDriver;
  bool usingFallbackDriver = false;
  if (!createConfiguredDriver(requestedDeviceName, sampleRate, bufferSize, newDriver, errorMessage,
                              usingFallbackDriver)) {
    m_transportController = std::move(oldTransport);
    m_audioDriver = std::move(oldDriver);
    updateDeviceStatus(requestedDeviceName, errorMessage);
    return false;
  }

  const bool wasRunning = oldDriver && oldDriver->isRunning();
  if (wasRunning) {
    oldDriver->stop();
  }

  m_transportController = std::move(newTransport);
  m_audioDriver = std::move(newDriver);
  m_sampleRate = sampleRate;
  m_bufferSize = bufferSize;
  m_currentDeviceName = requestedDeviceName;
  updateDeviceStatus(requestedDeviceName, usingFallbackDriver ? "Using dummy audio driver" : "");

  if (wasRunning) {
    if (!start()) {
      m_transportController = std::move(oldTransport);
      m_audioDriver = std::move(oldDriver);
      m_sampleRate = oldSampleRate;
      m_bufferSize = oldBufferSize;
      m_currentDeviceName = oldDeviceName;
      m_deviceStatus = oldDeviceStatus;
      if (m_audioDriver) {
        m_audioDriver->start(this);
      }
      updateDeviceStatus(requestedDeviceName, "Failed to restart audio after changing device");
      return false;
    }
  }

  for (const auto handle : handlesToRestart) {
    m_transportController->startClip(handle);
  }

  DBG("AudioEngine: Successfully changed audio settings");
  return true;
}

AudioEngine::AudioDeviceStatus AudioEngine::getAudioDeviceStatus() const {
  return m_deviceStatus;
}

std::optional<orpheus::AudioDeviceInfo>
AudioEngine::getDeviceDetails(const std::string& deviceName) const {
  return findDeviceDetails(deviceName);
}

//==============================================================================
// Cue Buss Management (pool-based, broadcast-safe - no runtime allocations)

orpheus::ClipHandle AudioEngine::allocateCueBuss(const juce::String& filePath) {
  if (!m_transportController)
    return 0;

  // Find a free slot in the pre-allocated pool (no dynamic allocation)
  int freeSlot = -1;
  for (int i = 0; i < MAX_CUE_BUSSES; ++i) {
    if (m_cueBussPool[i].handle == 0) {
      freeSlot = i;
      break;
    }
  }

  if (freeSlot < 0) {
    DBG("AudioEngine: Cue Buss pool exhausted (max " << MAX_CUE_BUSSES << ")");
    return 0;
  }

  // Calculate handle from pool slot (deterministic, no counter needed)
  orpheus::ClipHandle cueBussHandle =
      CUE_BUSS_BASE_HANDLE + static_cast<orpheus::ClipHandle>(freeSlot);

  // Register audio file with transport controller
  auto result = m_transportController->registerClipAudio(cueBussHandle, filePath.toStdString());

  if (result != orpheus::SessionGraphError::OK) {
    DBG("AudioEngine: Failed to allocate Cue Buss for: " << filePath);
    return 0;
  }

  // Mark slot as allocated
  m_cueBussPool[freeSlot].handle = cueBussHandle;
  m_cueBussPool[freeSlot].filePath = filePath;

  // Read metadata for UI (same as loadClip does for buttons)
  auto reader = orpheus::createAudioFileReader();
  auto metadataResult = reader->open(filePath.toStdString());
  if (metadataResult.isOk()) {
    m_cueBussPool[freeSlot].metadata = metadataResult.value;
    DBG("AudioEngine: Cue Buss " << cueBussHandle << " metadata: "
                                 << static_cast<int>(metadataResult.value.sample_rate) << " Hz, "
                                 << static_cast<int>(metadataResult.value.num_channels) << " ch, "
                                 << static_cast<int>(metadataResult.value.duration_samples)
                                 << " samples");
  } else {
    m_cueBussPool[freeSlot].metadata = std::nullopt;
    DBG("AudioEngine: WARNING - Failed to read metadata for Cue Buss " << cueBussHandle);
  }

  // CRITICAL: Set default loop state to DISABLED (SDK defaults to loop=true)
  // This will be overridden by ClipEditDialog::setClipMetadata() if user has loop enabled
  m_transportController->setClipLoopMode(cueBussHandle, false);
  updateCachedCueMetadata(freeSlot);

  // Count active cue busses for logging
  int activeCues = 0;
  for (int i = 0; i < MAX_CUE_BUSSES; ++i) {
    if (m_cueBussPool[i].handle != 0)
      ++activeCues;
  }

  DBG("AudioEngine: Allocated Cue " << activeCues << " (handle " << cueBussHandle << ", slot "
                                    << freeSlot << "): " << filePath
                                    << " (loop=disabled by default)");
  return cueBussHandle;
}

void AudioEngine::releaseCueBuss(orpheus::ClipHandle cueBussHandle) {
  if (cueBussHandle < CUE_BUSS_BASE_HANDLE || !m_transportController)
    return;

  // Find slot by handle
  int slot = static_cast<int>(cueBussHandle - CUE_BUSS_BASE_HANDLE);
  if (slot < 0 || slot >= MAX_CUE_BUSSES || m_cueBussPool[slot].handle != cueBussHandle)
    return;

  // Stop if playing
  if (m_transportController->isClipPlaying(cueBussHandle)) {
    m_transportController->stopClip(cueBussHandle);
  }

  // Clear slot (no deallocation, just mark as free)
  m_cueBussPool[slot].handle = 0;
  m_cueBussPool[slot].filePath.clear();
  m_cueBussPool[slot].transportMetadata = {};
  m_cueBussPool[slot].metadata = std::nullopt;

  // TODO: Unregister from transport controller (needs SDK API)

  DBG("AudioEngine: Released Cue Buss (handle " << cueBussHandle << ", slot " << slot << ")");
}

bool AudioEngine::startCueBuss(orpheus::ClipHandle cueBussHandle) {
  if (cueBussHandle < CUE_BUSS_BASE_HANDLE || !m_transportController)
    return false;

  auto result = m_transportController->startClip(cueBussHandle);
  if (result != orpheus::SessionGraphError::OK) {
    DBG("AudioEngine: Failed to start Cue Buss " << cueBussHandle);
    return false;
  }

  DBG("AudioEngine: Started Cue Buss " << cueBussHandle);
  return true;
}

bool AudioEngine::stopCueBuss(orpheus::ClipHandle cueBussHandle) {
  if (cueBussHandle < CUE_BUSS_BASE_HANDLE || !m_transportController)
    return false;

  auto result = m_transportController->stopClip(cueBussHandle);
  if (result != orpheus::SessionGraphError::OK) {
    DBG("AudioEngine: Failed to stop Cue Buss " << cueBussHandle);
    return false;
  }

  DBG("AudioEngine: Stopped Cue Buss " << cueBussHandle);
  return true;
}

bool AudioEngine::restartCueBuss(orpheus::ClipHandle cueBussHandle) {
  if (cueBussHandle < CUE_BUSS_BASE_HANDLE || !m_transportController)
    return false;

  // Use SDK's restartClip() - works for both main grid and Cue Buss
  auto result = m_transportController->restartClip(cueBussHandle);
  if (result != orpheus::SessionGraphError::OK) {
    DBG("AudioEngine: Failed to restart Cue Buss " << cueBussHandle);
    return false;
  }

  DBG("AudioEngine: Restarted Cue Buss " << cueBussHandle << " (seamless, no gap)");
  return true;
}

bool AudioEngine::updateCueBussMetadata(orpheus::ClipHandle cueBussHandle, int64_t trimInSamples,
                                        int64_t trimOutSamples, double fadeInSeconds,
                                        double fadeOutSeconds, const juce::String& fadeInCurve,
                                        const juce::String& fadeOutCurve) {
  if (cueBussHandle < CUE_BUSS_BASE_HANDLE || !m_transportController)
    return false;

  const auto fadeInCurveEnum = toFadeCurve(fadeInCurve);
  const auto fadeOutCurveEnum = toFadeCurve(fadeOutCurve);

  // Update trim points
  auto trimResult =
      m_transportController->updateClipTrimPoints(cueBussHandle, trimInSamples, trimOutSamples);
  if (trimResult != orpheus::SessionGraphError::OK) {
    DBG("AudioEngine: Failed to update Cue Buss trim points: " << static_cast<int>(trimResult));
    return false;
  }

  // CRITICAL: Validate fade times don't exceed trim duration
  int64_t trimDurationSamples = trimOutSamples - trimInSamples;
  double trimDurationSeconds = static_cast<double>(trimDurationSamples) / m_sampleRate;

  // Clamp fade times to fit within trim duration
  double clampedFadeInSeconds = fadeInSeconds;
  double clampedFadeOutSeconds = fadeOutSeconds;

  if (fadeInSeconds + fadeOutSeconds > trimDurationSeconds) {
    // Scale down proportionally to fit within trim duration
    double ratio = trimDurationSeconds / (fadeInSeconds + fadeOutSeconds);
    clampedFadeInSeconds = fadeInSeconds * ratio;
    clampedFadeOutSeconds = fadeOutSeconds * ratio;

    DBG("AudioEngine: Clamped Cue Buss fade times for handle "
        << cueBussHandle << " - Requested: IN " << fadeInSeconds << "s, OUT " << fadeOutSeconds
        << "s"
        << " | Clamped: IN " << clampedFadeInSeconds << "s, OUT " << clampedFadeOutSeconds << "s"
        << " (trim duration: " << trimDurationSeconds << "s)");
  }

  // Update fades with validated values
  auto fadeResult = m_transportController->updateClipFades(cueBussHandle, clampedFadeInSeconds,
                                                           clampedFadeOutSeconds, fadeInCurveEnum,
                                                           fadeOutCurveEnum);
  if (fadeResult != orpheus::SessionGraphError::OK) {
    DBG("AudioEngine: Failed to update Cue Buss fades: " << static_cast<int>(fadeResult));
    return false;
  }

  DBG("AudioEngine: Updated Cue Buss "
      << cueBussHandle << " - Trim: [" << trimInSamples << ", " << trimOutSamples << "]"
      << ", Fade IN: " << fadeInSeconds << "s (" << fadeInCurve << ")"
      << ", Fade OUT: " << fadeOutSeconds << "s (" << fadeOutCurve << ")");

  updateCachedCueMetadata(static_cast<int>(cueBussHandle - CUE_BUSS_BASE_HANDLE));

  return true;
}

bool AudioEngine::isCueBussPlaying(orpheus::ClipHandle cueBussHandle) const {
  if (cueBussHandle < CUE_BUSS_BASE_HANDLE || !m_transportController)
    return false;

  return m_transportController->isClipPlaying(cueBussHandle);
}

std::optional<orpheus::AudioFileMetadata>
AudioEngine::getCueBussMetadata(orpheus::ClipHandle cueBussHandle) const {
  if (cueBussHandle < CUE_BUSS_BASE_HANDLE)
    return std::nullopt;

  int slot = static_cast<int>(cueBussHandle - CUE_BUSS_BASE_HANDLE);
  if (slot < 0 || slot >= MAX_CUE_BUSSES || m_cueBussPool[slot].handle != cueBussHandle)
    return std::nullopt;

  return m_cueBussPool[slot].metadata;
}

bool AudioEngine::setCueBussLoop(orpheus::ClipHandle cueBussHandle, bool enabled) {
  if (cueBussHandle < CUE_BUSS_BASE_HANDLE || !m_transportController)
    return false;

  auto result = m_transportController->setClipLoopMode(cueBussHandle, enabled);
  if (result != orpheus::SessionGraphError::OK) {
    DBG("AudioEngine: Failed to set loop mode for Cue Buss " << cueBussHandle);
    return false;
  }

  DBG("AudioEngine: Set Cue Buss " << cueBussHandle << " loop mode to "
                                   << (enabled ? "enabled" : "disabled"));
  updateCachedCueMetadata(static_cast<int>(cueBussHandle - CUE_BUSS_BASE_HANDLE));
  return true;
}

int64_t AudioEngine::getCueBussPosition(orpheus::ClipHandle cueBussHandle) const {
  if (cueBussHandle < CUE_BUSS_BASE_HANDLE || !m_transportController)
    return 0;

  // Use SDK's getClipPosition() API (Phase 2 of ORP085)
  return m_transportController->getClipPosition(cueBussHandle);
}

bool AudioEngine::isCueBussLooping(orpheus::ClipHandle cueBussHandle) const {
  if (cueBussHandle < CUE_BUSS_BASE_HANDLE || !m_transportController)
    return false;

  // Use SDK's isClipLooping() API (Phase 7 of ORP085)
  return m_transportController->isClipLooping(cueBussHandle);
}

bool AudioEngine::seekCueBuss(orpheus::ClipHandle cueBussHandle, int64_t position) {
  if (cueBussHandle < CUE_BUSS_BASE_HANDLE || !m_transportController)
    return false;

  // Use SDK's seekClip() API (ORP089)
  auto result = m_transportController->seekClip(cueBussHandle, position);
  if (result != orpheus::SessionGraphError::OK) {
    DBG("AudioEngine: Failed to seek Cue Buss " << cueBussHandle << " to position " << position);
    return false;
  }

  DBG("AudioEngine: Seeked Cue Buss " << cueBussHandle << " to position " << position
                                      << " (gap-free, sample-accurate)");
  return true;
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
  // Record underrun in SDK PerformanceMonitor (audio-thread-safe, atomic)
  if (m_performanceMonitor)
    m_performanceMonitor->reportUnderrun();

  juce::MessageManager::callAsync([this]() {
    DBG("AudioEngine: Buffer underrun!");
    if (onBufferUnderrunDetected)
      onBufferUnderrunDetected();
  });
}

//==============================================================================
void AudioEngine::processAudio(const float** input_buffers, float** output_buffers,
                               size_t num_channels, size_t num_frames) {
  // BROADCAST-SAFE: No allocations, no locks, no I/O in audio thread
  auto callbackStart = std::chrono::high_resolution_clock::now();

  // OCC151 T5 / F-APP-3: record the audio-thread id so drainTransportCallbacks()
  // can assert it never runs here. relaxed is fine — this is a Debug-only guard.
  m_audioThreadId.store(std::this_thread::get_id(), std::memory_order_relaxed);

  if (!m_transportController) {
    // No transport - output silence
    for (size_t ch = 0; ch < num_channels; ++ch) {
      if (output_buffers[ch])
        std::memset(output_buffers[ch], 0, num_frames * sizeof(float));
    }
    return;
  }

  // Call SDK transport controller for real audio processing!
  m_transportController->processAudio(output_buffers, num_channels, num_frames);

  // Process audio for VU meter (analyze output for visualization)
  if (m_audioAnalyzer && num_channels > 0 && num_frames > 0) {
    // Create a temporary JUCE AudioBuffer wrapper around output buffers
    // AudioAnalyzer::processBlock() will average all channels for mono analysis
    juce::AudioBuffer<float> tempBuffer(output_buffers, static_cast<int>(num_channels),
                                        static_cast<int>(num_frames));
    m_audioAnalyzer->processBlock(tempBuffer);

    // Read mono analysis results and duplicate to all channels
    float rmsLevel = m_audioAnalyzer->getRMSLevel();
    float peakLevel = m_audioAnalyzer->getPeakLevel();

    for (size_t ch = 0; ch < m_rmsLevels.size() && ch < num_channels; ++ch) {
      m_rmsLevels[ch] = rmsLevel;
      m_peakLevels[ch] = peakLevel;
    }
  }

  // OCC151 T5 / F-APP-3: DO NOT drain transport callbacks here. The SDK callback
  // ring is SPSC and the message thread is the sole consumer. Draining on the
  // audio thread destroyed std::function objects (and their captured
  // shared_ptrs) on the RT thread. The drain now happens on the message thread
  // via drainTransportCallbacks(), called from MainComponent's UI timer.

  // Record performance metrics (atomic, no allocations)
  if (m_performanceMonitor) {
    auto callbackEnd = std::chrono::high_resolution_clock::now();
    auto durationUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(callbackEnd - callbackStart).count());
    uint64_t bufferUs = (num_frames * 1'000'000ULL) / m_sampleRate;
    m_performanceMonitor->recordAudioCallback(durationUs, bufferUs, 0, m_sampleRate, m_bufferSize);
  }
}

//==============================================================================
orpheus::ClipHandle AudioEngine::getClipHandle(int buttonIndex) const {
  if (buttonIndex >= 0 && buttonIndex < AudioEngine::MAX_CLIP_BUTTONS)
    return m_clipHandles[buttonIndex];
  return 0;
}

int AudioEngine::getButtonIndexFromHandle(orpheus::ClipHandle handle) const {
  for (int i = 0; i < AudioEngine::MAX_CLIP_BUTTONS; ++i) {
    if (m_clipHandles[i] == handle)
      return i;
  }
  return -1;
}
