// SPDX-License-Identifier: MIT
#include <orpheus/audio_driver.h>
#include <orpheus/audio_driver_manager.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <sstream>

#ifdef __APPLE__
#include <CoreAudio/CoreAudio.h>
#endif

namespace orpheus {

namespace {

/// Helper function to check if sample rate is supported
bool isSampleRateSupported(const std::vector<uint32_t>& supported, uint32_t rate) {
  return std::find(supported.begin(), supported.end(), rate) != supported.end();
}

/// Helper function to check if buffer size is supported
bool isBufferSizeSupported(const std::vector<uint32_t>& supported, uint32_t size) {
  return std::find(supported.begin(), supported.end(), size) != supported.end();
}

} // anonymous namespace

/// Audio driver manager implementation
class AudioDriverManager : public IAudioDriverManager {
public:
  AudioDriverManager();
  ~AudioDriverManager() override;

  // IAudioDriverManager interface
  std::vector<AudioDeviceInfo> enumerateDevices() override;
  std::optional<AudioDeviceInfo> getDeviceInfo(const std::string& deviceId) override;
  SessionGraphError setActiveDevice(const std::string& deviceId, uint32_t sampleRate,
                                    uint32_t bufferSize) override;
  std::optional<std::string> getCurrentDevice() const override;
  uint32_t getCurrentSampleRate() const override;
  uint32_t getCurrentBufferSize() const override;
  void setDeviceChangeCallback(std::function<void()> callback) override;
  IAudioDriver* getActiveDriver() override;

private:
  /// Enumerate CoreAudio devices (macOS)
  std::vector<AudioDeviceInfo> enumerateCoreAudioDevices();

  /// Enumerate Windows devices (WASAPI/ASIO) - stub for Phase 1
  std::vector<AudioDeviceInfo> enumerateWindowsDevices();

  /// Enumerate Linux devices (ALSA) - stub for Phase 1
  std::vector<AudioDeviceInfo> enumerateLinuxDevices();

  /// Get dummy driver device info (always available)
  AudioDeviceInfo getDummyDeviceInfo();

  /// Create driver instance for device ID
  std::unique_ptr<IAudioDriver> createDriverForDevice(const std::string& deviceId);

  // State
  std::unique_ptr<IAudioDriver> m_activeDriver;
  std::string m_currentDeviceId;
  uint32_t m_currentSampleRate{48000};
  uint32_t m_currentBufferSize{512};

  // Callback
  std::function<void()> m_deviceChangeCallback;

  // Thread safety
  mutable std::mutex m_mutex;
};

AudioDriverManager::AudioDriverManager() = default;

AudioDriverManager::~AudioDriverManager() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_activeDriver) {
    m_activeDriver->stop();
  }
}

std::vector<AudioDeviceInfo> AudioDriverManager::enumerateDevices() {
  std::vector<AudioDeviceInfo> devices;

  // Dummy driver is always first (for testing)
  devices.push_back(getDummyDeviceInfo());

#ifdef __APPLE__
  // Enumerate CoreAudio devices (macOS)
  auto coreAudioDevices = enumerateCoreAudioDevices();
  devices.insert(devices.end(), coreAudioDevices.begin(), coreAudioDevices.end());
#elif defined(_WIN32)
  // Enumerate Windows devices (WASAPI/ASIO) - stub for Phase 1
  auto windowsDevices = enumerateWindowsDevices();
  devices.insert(devices.end(), windowsDevices.begin(), windowsDevices.end());
#elif defined(__linux__)
  // Enumerate Linux devices (ALSA) - stub for Phase 1
  auto linuxDevices = enumerateLinuxDevices();
  devices.insert(devices.end(), linuxDevices.begin(), linuxDevices.end());
#endif

  return devices;
}

std::optional<AudioDeviceInfo> AudioDriverManager::getDeviceInfo(const std::string& deviceId) {
  // Check if dummy driver
  if (deviceId == "dummy") {
    return getDummyDeviceInfo();
  }

#ifdef __APPLE__
  // Check CoreAudio devices
  auto devices = enumerateCoreAudioDevices();
  for (const auto& device : devices) {
    if (device.deviceId == deviceId) {
      return device;
    }
  }
#elif defined(_WIN32)
  // Check Windows devices (stub)
  auto devices = enumerateWindowsDevices();
  for (const auto& device : devices) {
    if (device.deviceId == deviceId) {
      return device;
    }
  }
#elif defined(__linux__)
  // Check Linux devices (stub)
  auto devices = enumerateLinuxDevices();
  for (const auto& device : devices) {
    if (device.deviceId == deviceId) {
      return device;
    }
  }
#endif

  return std::nullopt;
}

SessionGraphError AudioDriverManager::setActiveDevice(const std::string& deviceId,
                                                      uint32_t sampleRate, uint32_t bufferSize) {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Validate device exists
  auto deviceInfo = getDeviceInfo(deviceId);
  if (!deviceInfo.has_value()) {
    return SessionGraphError::InvalidParameter;
  }

  // Validate sample rate
  if (!isSampleRateSupported(deviceInfo->supportedSampleRates, sampleRate)) {
    return SessionGraphError::InvalidParameter;
  }

  // Validate buffer size
  if (!isBufferSizeSupported(deviceInfo->supportedBufferSizes, bufferSize)) {
    return SessionGraphError::InvalidParameter;
  }

  // Step 1-2: Stop current driver (if any) - this fades out clips
  if (m_activeDriver) {
    SessionGraphError stopResult = m_activeDriver->stop();
    if (stopResult != SessionGraphError::OK) {
      return stopResult;
    }
  }

  // Step 3: Close current driver
  m_activeDriver.reset();

  // Step 4: Create and initialize new driver
  auto newDriver = createDriverForDevice(deviceId);
  if (!newDriver) {
    return SessionGraphError::InternalError;
  }

  AudioDriverConfig config;
  config.sample_rate = sampleRate;
  config.buffer_size = static_cast<uint16_t>(bufferSize);
  config.num_inputs = 0;  // Output only for now
  config.num_outputs = 2; // Stereo output
  config.device_name = (deviceId == "dummy") ? "" : deviceInfo->name;

  SessionGraphError initResult = newDriver->initialize(config);
  if (initResult != SessionGraphError::OK) {
    return initResult;
  }

  // Step 5: Store new driver (audio callback restart happens in RealTimeEngine)
  m_activeDriver = std::move(newDriver);
  m_currentDeviceId = deviceId;
  m_currentSampleRate = sampleRate;
  m_currentBufferSize = bufferSize;

  // Step 6: Notify via callback
  if (m_deviceChangeCallback) {
    m_deviceChangeCallback();
  }

  return SessionGraphError::OK;
}

std::optional<std::string> AudioDriverManager::getCurrentDevice() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_currentDeviceId.empty()) {
    return std::nullopt;
  }
  return m_currentDeviceId;
}

uint32_t AudioDriverManager::getCurrentSampleRate() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_currentSampleRate;
}

uint32_t AudioDriverManager::getCurrentBufferSize() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_currentBufferSize;
}

void AudioDriverManager::setDeviceChangeCallback(std::function<void()> callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_deviceChangeCallback = std::move(callback);
}

IAudioDriver* AudioDriverManager::getActiveDriver() {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_activeDriver.get();
}

AudioDeviceInfo AudioDriverManager::getDummyDeviceInfo() {
  AudioDeviceInfo info;
  info.deviceId = "dummy";
  info.name = "Dummy Audio Driver";
  info.driverType = "Dummy";
  info.minChannels = 2;
  info.maxChannels = 2;
  info.supportedSampleRates = {44100, 48000, 88200, 96000};
  info.supportedBufferSizes = {128, 256, 512, 1024, 2048};
  info.isDefaultDevice = false;
  return info;
}

#ifdef __APPLE__
std::vector<AudioDeviceInfo> AudioDriverManager::enumerateCoreAudioDevices() {
  std::vector<AudioDeviceInfo> devices;

  // Query all audio devices
  AudioObjectPropertyAddress propertyAddress = {kAudioHardwarePropertyDevices,
                                                kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};

  UInt32 dataSize = 0;
  OSStatus status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0,
                                                   nullptr, &dataSize);

  if (status != noErr || dataSize == 0) {
    return devices;
  }

  UInt32 deviceCount = dataSize / sizeof(AudioDeviceID);
  std::vector<AudioDeviceID> deviceIDs(deviceCount);

  status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, nullptr,
                                      &dataSize, deviceIDs.data());

  if (status != noErr) {
    return devices;
  }

  // Get default output device ID
  AudioObjectPropertyAddress defaultDeviceAddress = {kAudioHardwarePropertyDefaultOutputDevice,
                                                     kAudioObjectPropertyScopeGlobal,
                                                     kAudioObjectPropertyElementMain};

  AudioDeviceID defaultDeviceID = 0;
  UInt32 defaultDeviceSize = sizeof(AudioDeviceID);
  AudioObjectGetPropertyData(kAudioObjectSystemObject, &defaultDeviceAddress, 0, nullptr,
                             &defaultDeviceSize, &defaultDeviceID);

  // Enumerate each device
  for (AudioDeviceID deviceID : deviceIDs) {
    // Get device name
    AudioObjectPropertyAddress nameAddress = {kAudioDevicePropertyDeviceNameCFString,
                                              kAudioObjectPropertyScopeGlobal,
                                              kAudioObjectPropertyElementMain};

    CFStringRef cfName = nullptr;
    UInt32 nameSize = sizeof(CFStringRef);
    status = AudioObjectGetPropertyData(deviceID, &nameAddress, 0, nullptr, &nameSize, &cfName);

    if (status != noErr || !cfName) {
      continue;
    }

    // Convert CFString to std::string
    char nameBuffer[256];
    Boolean success =
        CFStringGetCString(cfName, nameBuffer, sizeof(nameBuffer), kCFStringEncodingUTF8);
    CFRelease(cfName);

    if (!success) {
      continue;
    }

    // Check if device has output channels
    AudioObjectPropertyAddress streamConfigAddress = {kAudioDevicePropertyStreamConfiguration,
                                                      kAudioDevicePropertyScopeOutput,
                                                      kAudioObjectPropertyElementMain};

    AudioBufferList bufferList;
    UInt32 bufferListSize = sizeof(bufferList);
    status = AudioObjectGetPropertyData(deviceID, &streamConfigAddress, 0, nullptr, &bufferListSize,
                                        &bufferList);

    uint32_t totalChannels = 0;
    if (status == noErr) {
      for (UInt32 i = 0; i < bufferList.mNumberBuffers; ++i) {
        totalChannels += bufferList.mBuffers[i].mNumberChannels;
      }
    }

    // Skip devices with no output channels
    if (totalChannels == 0) {
      continue;
    }

    // Query supported sample rates
    AudioObjectPropertyAddress sampleRateAddress = {kAudioDevicePropertyAvailableNominalSampleRates,
                                                    kAudioObjectPropertyScopeGlobal,
                                                    kAudioObjectPropertyElementMain};

    UInt32 sampleRateSize = 0;
    status =
        AudioObjectGetPropertyDataSize(deviceID, &sampleRateAddress, 0, nullptr, &sampleRateSize);

    std::vector<uint32_t> supportedSampleRates;
    if (status == noErr && sampleRateSize > 0) {
      UInt32 rangeCount = sampleRateSize / sizeof(AudioValueRange);
      std::vector<AudioValueRange> ranges(rangeCount);
      status = AudioObjectGetPropertyData(deviceID, &sampleRateAddress, 0, nullptr, &sampleRateSize,
                                          ranges.data());

      if (status == noErr) {
        // Common sample rates to check
        std::vector<uint32_t> commonRates = {44100, 48000, 88200, 96000, 176400, 192000};
        for (uint32_t rate : commonRates) {
          for (const auto& range : ranges) {
            if (rate >= range.mMinimum && rate <= range.mMaximum) {
              supportedSampleRates.push_back(rate);
              break;
            }
          }
        }
      }
    }

    // Fallback: If no sample rates found, assume common rates
    if (supportedSampleRates.empty()) {
      supportedSampleRates = {44100, 48000, 96000};
    }

    // Build device info
    AudioDeviceInfo deviceInfo;
    deviceInfo.deviceId = "coreaudio:" + std::to_string(deviceID);
    deviceInfo.name = std::string(nameBuffer);
    deviceInfo.driverType = "CoreAudio";
    deviceInfo.minChannels = 2; // Stereo minimum
    deviceInfo.maxChannels = totalChannels;
    deviceInfo.supportedSampleRates = supportedSampleRates;
    deviceInfo.supportedBufferSizes = {128, 256, 512, 1024, 2048}; // Common buffer sizes
    deviceInfo.isDefaultDevice = (deviceID == defaultDeviceID);

    devices.push_back(deviceInfo);
  }

  return devices;
}
#endif

#ifdef _WIN32
std::vector<AudioDeviceInfo> AudioDriverManager::enumerateWindowsDevices() {
  // Phase 1: Stub implementation
  // Phase 2: Implement WASAPI device enumeration using IMMDeviceEnumerator
  return {};
}
#endif

#ifdef __linux__
std::vector<AudioDeviceInfo> AudioDriverManager::enumerateLinuxDevices() {
  // Phase 1: Stub implementation
  // Phase 2: Implement ALSA device enumeration using snd_device_name_hint
  return {};
}
#endif

std::unique_ptr<IAudioDriver>
AudioDriverManager::createDriverForDevice(const std::string& deviceId) {
  if (deviceId == "dummy") {
    return createDummyAudioDriver();
  }

#ifdef __APPLE__
  if (deviceId.rfind("coreaudio:", 0) == 0) {
    // Extract device ID from string (e.g., "coreaudio:123" -> 123)
    // For now, use the default CoreAudio driver with device_name set
    return createCoreAudioDriver();
  }
#endif

  // Unsupported device type
  return nullptr;
}

// Factory function
std::unique_ptr<IAudioDriverManager> createAudioDriverManager() {
  return std::make_unique<AudioDriverManager>();
}

} // namespace orpheus
