// SPDX-License-Identifier: MIT
#include "coreaudio/coreaudio_endpoint_catalog.h"

#include <orpheus/audio_driver.h>
#include <orpheus/audio_driver_manager.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cwchar>
#include <mutex>
#include <sstream>
#include <thread>
#include <utility>

#if defined(__APPLE__) && defined(ORPHEUS_ENABLE_COREAUDIO)
#include <CoreAudio/CoreAudio.h>
#endif

#if defined(_WIN32) && defined(ORPHEUS_ENABLE_WASAPI)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <windows.h>
#endif

namespace orpheus {

namespace {

bool isSampleRateSupported(const std::vector<uint32_t>& supported, uint32_t rate) {
  return std::find(supported.begin(), supported.end(), rate) != supported.end();
}

bool isBufferSizeSupported(const std::vector<uint32_t>& supported, uint32_t size) {
  return std::find(supported.begin(), supported.end(), size) != supported.end();
}

#if defined(_WIN32) && defined(ORPHEUS_ENABLE_WASAPI)
std::string wideToUtf8(const wchar_t* value) {
  if (value == nullptr || *value == L'\0') {
    return {};
  }
  const int size =
      WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, nullptr, 0, nullptr, nullptr);
  if (size <= 1) {
    return {};
  }
  std::string result(static_cast<size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, result.data(), size, nullptr,
                      nullptr);
  result.pop_back();
  return result;
}

template <typename T> void releaseCom(T*& object) {
  if (object != nullptr) {
    object->Release();
    object = nullptr;
  }
}

void appendUnique(std::vector<uint32_t>& values, uint32_t value) {
  if (value > 0 && std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}
#endif

} // anonymous namespace

class AudioDriverManager : public IAudioDriverManager {
public:
  using DriverFactory = std::function<std::unique_ptr<IAudioDriver>(const std::string&)>;

  AudioDriverManager();
  explicit AudioDriverManager(DriverFactory factory);
  ~AudioDriverManager() override;

  std::vector<AudioDeviceInfo> enumerateDevices() override;
  std::optional<AudioDeviceInfo> getDeviceInfo(const std::string& deviceId) override;
  SessionGraphError setActiveDevice(const std::string& deviceId, uint32_t sampleRate,
                                    uint32_t bufferSize) override;
  std::optional<std::string> getCurrentDevice() const override;
  uint32_t getCurrentSampleRate() const override;
  uint32_t getCurrentBufferSize() const override;
  void setDeviceChangeCallback(std::function<void()> callback) override;
  IAudioDriver* getActiveDriver() override;
  std::vector<AudioEndpointCapabilities> enumerateEndpointCapabilities() override;
  std::optional<AudioEndpointCapabilities> getEndpointCapabilities(
      const std::string& deviceId) override;
  void setEndpointChangeCallback(std::function<void()> callback) override;

private:
  std::vector<AudioDeviceInfo> enumerateCoreAudioDevices();
  std::vector<AudioDeviceInfo> enumerateWindowsDevices();
  std::vector<AudioDeviceInfo> enumerateLinuxDevices();
  std::vector<AudioEndpointCapabilities> enumerateCoreAudioEndpointCapabilities();
  AudioDeviceInfo getDummyDeviceInfo();
  std::unique_ptr<IAudioDriver> createDriverForDevice(const std::string& deviceId);
#if defined(__APPLE__) && defined(ORPHEUS_ENABLE_COREAUDIO)
  static OSStatus endpointPropertyChanged(AudioObjectID, UInt32,
                                          const AudioObjectPropertyAddress*, void*) noexcept;
  void startEndpointMonitor();
  void stopEndpointMonitor();
  void endpointMonitorLoop();
#endif

  std::unique_ptr<IAudioDriver> m_activeDriver;
  DriverFactory m_driverFactory;
  std::string m_currentDeviceId;
  uint32_t m_currentSampleRate{48000};
  uint32_t m_currentBufferSize{512};
  std::function<void()> m_deviceChangeCallback;
  std::function<void()> m_endpointChangeCallback;
  std::atomic<bool> m_endpointMonitorActive{false};
  std::atomic<bool> m_endpointChangePending{false};
  std::condition_variable m_endpointChangeCondition;
  std::thread m_endpointMonitorThread;
  mutable std::mutex m_mutex;
};

AudioDriverManager::AudioDriverManager()
    : m_driverFactory(
          [this](const std::string& deviceId) { return createDriverForDevice(deviceId); }) {}

AudioDriverManager::AudioDriverManager(DriverFactory factory)
    : m_driverFactory(std::move(factory)) {}

AudioDriverManager::~AudioDriverManager() {
#if defined(__APPLE__) && defined(ORPHEUS_ENABLE_COREAUDIO)
  stopEndpointMonitor();
#endif
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_activeDriver) {
    m_activeDriver->stop();
  }
}

std::vector<AudioDeviceInfo> AudioDriverManager::enumerateDevices() {
  std::vector<AudioDeviceInfo> devices;
  devices.push_back(getDummyDeviceInfo());

#if defined(__APPLE__) && defined(ORPHEUS_ENABLE_COREAUDIO)
  auto core_audio_devices = enumerateCoreAudioDevices();
  devices.insert(devices.end(), core_audio_devices.begin(), core_audio_devices.end());
#elif defined(_WIN32) && defined(ORPHEUS_ENABLE_WASAPI)
  auto windows_devices = enumerateWindowsDevices();
  devices.insert(devices.end(), windows_devices.begin(), windows_devices.end());
#elif defined(__linux__)
  auto linux_devices = enumerateLinuxDevices();
  devices.insert(devices.end(), linux_devices.begin(), linux_devices.end());
#endif

  return devices;
}

std::optional<AudioDeviceInfo> AudioDriverManager::getDeviceInfo(const std::string& device_id) {
  if (device_id == "dummy") {
    return getDummyDeviceInfo();
  }

#if defined(__APPLE__) && defined(ORPHEUS_ENABLE_COREAUDIO)
  for (const auto& device : enumerateCoreAudioDevices()) {
    if (device.deviceId == device_id) {
      return device;
    }
  }
#elif defined(_WIN32) && defined(ORPHEUS_ENABLE_WASAPI)
  for (const auto& device : enumerateWindowsDevices()) {
    if (device.deviceId == device_id) {
      return device;
    }
  }
#elif defined(__linux__)
  for (const auto& device : enumerateLinuxDevices()) {
    if (device.deviceId == device_id) {
      return device;
    }
  }
#endif

  return std::nullopt;
}

SessionGraphError AudioDriverManager::setActiveDevice(const std::string& device_id,
                                                      uint32_t sample_rate, uint32_t buffer_size) {
  std::function<void()> notification;
  std::unique_ptr<IAudioDriver> candidate;
  std::string candidate_device_id;

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto device_info = getDeviceInfo(device_id);
    if (!device_info.has_value() ||
        !isSampleRateSupported(device_info->supportedSampleRates, sample_rate) ||
        !isBufferSizeSupported(device_info->supportedBufferSizes, buffer_size)) {
      return SessionGraphError::InvalidParameter;
    }

    try {
      candidate_device_id = device_id;
      notification = m_deviceChangeCallback;

      AudioDriverConfig config;
      config.sample_rate = sample_rate;
      config.buffer_size = static_cast<uint16_t>(buffer_size);
      config.num_inputs = 0;
      config.num_outputs = 2;
      config.input_device_id = "";
      config.output_device_id = (device_id == "dummy") ? "" : device_id;
      config.device_name = (device_id == "dummy") ? "" : device_info->name;

      if (!m_driverFactory) {
        return SessionGraphError::InternalError;
      }
      candidate = m_driverFactory(device_id);
      if (!candidate) {
        return SessionGraphError::InternalError;
      }
      const SessionGraphError init_result = candidate->initialize(config);
      if (init_result != SessionGraphError::OK) {
        return init_result;
      }

      if (m_activeDriver) {
        const SessionGraphError stop_result = m_activeDriver->stop();
        if (stop_result != SessionGraphError::OK) {
          return stop_result;
        }
      }

      m_activeDriver = std::move(candidate);
      m_currentDeviceId.swap(candidate_device_id);
      m_currentSampleRate = m_activeDriver->getConfig().sample_rate;
      m_currentBufferSize = m_activeDriver->getConfig().buffer_size;
    } catch (...) {
      return SessionGraphError::InternalError;
    }
  }

  if (notification) {
    notification();
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

std::vector<AudioEndpointCapabilities> AudioDriverManager::enumerateEndpointCapabilities() {
  AudioEndpointCapabilities dummy;
  dummy.device_id = "dummy";
  dummy.display_name = "Dummy Audio Driver";
  dummy.availability = AudioEndpointAvailability::Available;
  dummy.supports_output = true;
  dummy.output_channels.reserve(32);
  for (uint16_t channel = 0; channel < 32; ++channel) {
    dummy.output_channels.push_back(
        {channel, "dummy:output:" + std::to_string(channel),
         "Dummy Output " + std::to_string(channel + 1)});
  }
  dummy.supported_sample_rates = {44100, 48000, 88200, 96000};
  dummy.supported_buffer_sizes = {128, 256, 512, 1024, 2048};
  dummy.nominal_sample_rate = 48000;
  dummy.current_buffer_size = 512;

  std::vector<AudioEndpointCapabilities> endpoints;
  endpoints.push_back(std::move(dummy));
#if defined(__APPLE__) && defined(ORPHEUS_ENABLE_COREAUDIO)
  auto core_audio_endpoints = enumerateCoreAudioEndpointCapabilities();
  endpoints.insert(endpoints.end(), core_audio_endpoints.begin(), core_audio_endpoints.end());
#endif
  return endpoints;
}

std::optional<AudioEndpointCapabilities> AudioDriverManager::getEndpointCapabilities(
    const std::string& deviceId) {
  for (auto& endpoint : enumerateEndpointCapabilities()) {
    if (endpoint.device_id == deviceId) {
      return endpoint;
    }
  }
  return std::nullopt;
}

void AudioDriverManager::setEndpointChangeCallback(std::function<void()> callback) {
  bool should_start = false;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_endpointChangeCallback = std::move(callback);
    should_start = static_cast<bool>(m_endpointChangeCallback);
  }
#if defined(__APPLE__) && defined(ORPHEUS_ENABLE_COREAUDIO)
  if (should_start) {
    startEndpointMonitor();
  } else {
    stopEndpointMonitor();
  }
#else
  (void)should_start;
#endif
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

#if defined(__APPLE__) && defined(ORPHEUS_ENABLE_COREAUDIO)
std::vector<AudioDeviceInfo> AudioDriverManager::enumerateCoreAudioDevices() {
  std::vector<AudioDeviceInfo> devices;
  AudioObjectPropertyAddress property_address = {kAudioHardwarePropertyDevices,
                                                 kAudioObjectPropertyScopeGlobal,
                                                 kAudioObjectPropertyElementMain};
  UInt32 data_size = 0;
  OSStatus status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &property_address, 0,
                                                   nullptr, &data_size);
  if (status != noErr || data_size == 0) {
    return devices;
  }

  const UInt32 device_count = data_size / sizeof(AudioDeviceID);
  std::vector<AudioDeviceID> device_ids(device_count);
  status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &property_address, 0, nullptr,
                                      &data_size, device_ids.data());
  if (status != noErr) {
    return devices;
  }

  AudioObjectPropertyAddress default_device_address = {kAudioHardwarePropertyDefaultOutputDevice,
                                                       kAudioObjectPropertyScopeGlobal,
                                                       kAudioObjectPropertyElementMain};
  AudioDeviceID default_device_id = 0;
  UInt32 default_device_size = sizeof(AudioDeviceID);
  AudioObjectGetPropertyData(kAudioObjectSystemObject, &default_device_address, 0, nullptr,
                             &default_device_size, &default_device_id);

  for (AudioDeviceID device_id : device_ids) {
    AudioObjectPropertyAddress name_address = {kAudioDevicePropertyDeviceNameCFString,
                                               kAudioObjectPropertyScopeGlobal,
                                               kAudioObjectPropertyElementMain};
    CFStringRef cf_name = nullptr;
    UInt32 name_size = sizeof(CFStringRef);
    status = AudioObjectGetPropertyData(device_id, &name_address, 0, nullptr, &name_size, &cf_name);
    if (status != noErr || !cf_name) {
      continue;
    }

    char name_buffer[256];
    const Boolean name_success =
        CFStringGetCString(cf_name, name_buffer, sizeof(name_buffer), kCFStringEncodingUTF8);
    CFRelease(cf_name);
    if (!name_success) {
      continue;
    }

    AudioObjectPropertyAddress uid_address = {kAudioDevicePropertyDeviceUID,
                                              kAudioObjectPropertyScopeGlobal,
                                              kAudioObjectPropertyElementMain};
    CFStringRef cf_uid = nullptr;
    UInt32 uid_size = sizeof(CFStringRef);
    status = AudioObjectGetPropertyData(device_id, &uid_address, 0, nullptr, &uid_size, &cf_uid);
    char uid_buffer[1024];
    const Boolean has_uid =
        status == noErr && cf_uid != nullptr &&
        CFStringGetCString(cf_uid, uid_buffer, sizeof(uid_buffer), kCFStringEncodingUTF8);
    if (cf_uid) {
      CFRelease(cf_uid);
    }
    if (!has_uid) {
      continue;
    }

    AudioObjectPropertyAddress stream_config_address = {kAudioDevicePropertyStreamConfiguration,
                                                        kAudioObjectPropertyScopeOutput,
                                                        kAudioObjectPropertyElementMain};
    UInt32 stream_config_size = 0;
    status = AudioObjectGetPropertyDataSize(device_id, &stream_config_address, 0, nullptr,
                                            &stream_config_size);
    if (status != noErr || stream_config_size < sizeof(AudioBufferList)) {
      continue;
    }
    std::vector<uint8_t> stream_config_storage(stream_config_size);
    auto* buffer_list = reinterpret_cast<AudioBufferList*>(stream_config_storage.data());
    status = AudioObjectGetPropertyData(device_id, &stream_config_address, 0, nullptr,
                                        &stream_config_size, buffer_list);
    uint32_t total_channels = 0;
    if (status == noErr) {
      for (UInt32 index = 0; index < buffer_list->mNumberBuffers; ++index) {
        total_channels += buffer_list->mBuffers[index].mNumberChannels;
      }
    }
    if (total_channels == 0) {
      continue;
    }

    AudioObjectPropertyAddress sample_rate_address = {
        kAudioDevicePropertyAvailableNominalSampleRates, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain};
    UInt32 sample_rate_size = 0;
    status = AudioObjectGetPropertyDataSize(device_id, &sample_rate_address, 0, nullptr,
                                            &sample_rate_size);
    std::vector<uint32_t> supported_sample_rates;
    if (status == noErr && sample_rate_size > 0) {
      const UInt32 range_count = sample_rate_size / sizeof(AudioValueRange);
      std::vector<AudioValueRange> ranges(range_count);
      status = AudioObjectGetPropertyData(device_id, &sample_rate_address, 0, nullptr,
                                          &sample_rate_size, ranges.data());
      if (status == noErr) {
        const std::vector<uint32_t> common_rates = {44100, 48000, 88200, 96000, 176400, 192000};
        for (uint32_t rate : common_rates) {
          for (const auto& range : ranges) {
            if (rate >= range.mMinimum && rate <= range.mMaximum) {
              supported_sample_rates.push_back(rate);
              break;
            }
          }
        }
      }
    }
    if (supported_sample_rates.empty()) {
      supported_sample_rates = {44100, 48000, 96000};
    }

    AudioDeviceInfo device_info;
    device_info.deviceId = uid_buffer;
    device_info.name = std::string(name_buffer);
    device_info.driverType = "CoreAudio";
    device_info.minChannels = 2;
    device_info.maxChannels = total_channels;
    device_info.supportedSampleRates = supported_sample_rates;
    device_info.supportedBufferSizes = {128, 256, 512, 1024, 2048};
    device_info.isDefaultDevice = device_id == default_device_id;
    devices.push_back(std::move(device_info));
  }

  return devices;
}

std::vector<AudioEndpointCapabilities>
AudioDriverManager::enumerateCoreAudioEndpointCapabilities() {
  std::vector<AudioEndpointCapabilities> endpoints;
  const AudioObjectPropertyAddress devices_address = {
      kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};
  UInt32 devices_size = 0;
  if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &devices_address, 0, nullptr,
                                     &devices_size) != noErr ||
      devices_size == 0) {
    return endpoints;
  }
  std::vector<AudioDeviceID> device_ids(devices_size / sizeof(AudioDeviceID));
  if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &devices_address, 0, nullptr,
                                 &devices_size, device_ids.data()) != noErr) {
    return endpoints;
  }

  const auto read_string = [](AudioDeviceID device_id,
                              AudioObjectPropertySelector selector) -> std::string {
    const AudioObjectPropertyAddress address = {selector, kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};
    CFStringRef value = nullptr;
    UInt32 size = sizeof(value);
    if (AudioObjectGetPropertyData(device_id, &address, 0, nullptr, &size, &value) != noErr ||
        value == nullptr) {
      return {};
    }
    char buffer[1024] = {};
    const Boolean converted =
        CFStringGetCString(value, buffer, sizeof(buffer), kCFStringEncodingUTF8);
    CFRelease(value);
    return converted ? std::string(buffer) : std::string{};
  };
  const auto read_channel_count = [](AudioDeviceID device_id,
                                     AudioObjectPropertyScope scope) -> uint32_t {
    const AudioObjectPropertyAddress address = {kAudioDevicePropertyStreamConfiguration, scope,
                                                kAudioObjectPropertyElementMain};
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(device_id, &address, 0, nullptr, &size) != noErr ||
        size < sizeof(AudioBufferList)) {
      return 0;
    }
    std::vector<uint8_t> storage(size);
    auto* buffers = reinterpret_cast<AudioBufferList*>(storage.data());
    if (AudioObjectGetPropertyData(device_id, &address, 0, nullptr, &size, buffers) != noErr) {
      return 0;
    }
    uint32_t channels = 0;
    for (UInt32 index = 0; index < buffers->mNumberBuffers; ++index) {
      channels += buffers->mBuffers[index].mNumberChannels;
    }
    return channels;
  };
  const auto read_uint32 = [](AudioDeviceID device_id, AudioObjectPropertySelector selector,
                              AudioObjectPropertyScope scope) -> uint32_t {
    const AudioObjectPropertyAddress address = {selector, scope,
                                                kAudioObjectPropertyElementMain};
    UInt32 value = 0;
    UInt32 size = sizeof(value);
    return AudioObjectGetPropertyData(device_id, &address, 0, nullptr, &size, &value) == noErr
               ? value
               : 0;
  };
  const auto read_nominal_rate = [](AudioDeviceID device_id) -> uint32_t {
    const AudioObjectPropertyAddress address = {kAudioDevicePropertyNominalSampleRate,
                                                kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};
    Float64 value = 0.0;
    UInt32 size = sizeof(value);
    if (AudioObjectGetPropertyData(device_id, &address, 0, nullptr, &size, &value) != noErr ||
        value <= 0.0) {
      return 0;
    }
    return static_cast<uint32_t>(value);
  };

  AudioDeviceID default_input = 0;
  AudioDeviceID default_output = 0;
  const AudioObjectPropertyAddress default_input_address = {
      kAudioHardwarePropertyDefaultInputDevice, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};
  const AudioObjectPropertyAddress default_output_address = {
      kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};
  UInt32 default_size = sizeof(default_input);
  AudioObjectGetPropertyData(kAudioObjectSystemObject, &default_input_address, 0, nullptr,
                             &default_size, &default_input);
  default_size = sizeof(default_output);
  AudioObjectGetPropertyData(kAudioObjectSystemObject, &default_output_address, 0, nullptr,
                             &default_size, &default_output);

  for (const AudioDeviceID device_id : device_ids) {
    const std::string uid = read_string(device_id, kAudioDevicePropertyDeviceUID);
    if (uid.empty()) {
      continue;
    }

    detail::CoreAudioEndpointFacts facts;
    facts.device_id = uid;
    facts.display_name = read_string(device_id, kAudioDevicePropertyDeviceNameCFString);
    facts.is_default_input = device_id == default_input;
    facts.is_default_output = device_id == default_output;
    facts.input_channel_count =
        read_channel_count(device_id, kAudioObjectPropertyScopeInput);
    facts.output_channel_count =
        read_channel_count(device_id, kAudioObjectPropertyScopeOutput);

    const AudioObjectPropertyAddress rates_address = {
        kAudioDevicePropertyAvailableNominalSampleRates, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain};
    UInt32 rates_size = 0;
    if (AudioObjectGetPropertyDataSize(device_id, &rates_address, 0, nullptr, &rates_size) ==
            noErr &&
        rates_size >= sizeof(AudioValueRange)) {
      std::vector<AudioValueRange> ranges(rates_size / sizeof(AudioValueRange));
      if (AudioObjectGetPropertyData(device_id, &rates_address, 0, nullptr, &rates_size,
                                     ranges.data()) == noErr) {
        facts.sample_rate_ranges.reserve(ranges.size());
        for (const auto& range : ranges) {
          facts.sample_rate_ranges.push_back(
              {static_cast<double>(range.mMinimum), static_cast<double>(range.mMaximum)});
        }
      }
    }
    facts.nominal_sample_rate = read_nominal_rate(device_id);

    const AudioObjectPropertyAddress buffer_range_address = {
        kAudioDevicePropertyBufferFrameSizeRange, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain};
    AudioValueRange buffer_range{};
    UInt32 buffer_range_size = sizeof(buffer_range);
    if (AudioObjectGetPropertyData(device_id, &buffer_range_address, 0, nullptr,
                                   &buffer_range_size, &buffer_range) == noErr) {
      facts.buffer_size_ranges.push_back(
          {static_cast<double>(buffer_range.mMinimum),
           static_cast<double>(buffer_range.mMaximum)});
    }
    facts.current_buffer_size =
        read_uint32(device_id, kAudioDevicePropertyBufferFrameSize,
                    kAudioObjectPropertyScopeGlobal);

    endpoints.push_back(detail::makeCoreAudioEndpointCapabilities(facts));
  }
  return endpoints;
}
#endif

#if defined(__APPLE__) && defined(ORPHEUS_ENABLE_COREAUDIO)
OSStatus AudioDriverManager::endpointPropertyChanged(
    AudioObjectID, UInt32, const AudioObjectPropertyAddress*, void* context) noexcept {
  auto* manager = static_cast<AudioDriverManager*>(context);
  if (manager == nullptr) {
    return noErr;
  }
  manager->m_endpointChangePending.store(true, std::memory_order_release);
  manager->m_endpointChangeCondition.notify_one();
  return noErr;
}

void AudioDriverManager::startEndpointMonitor() {
  if (m_endpointMonitorActive.exchange(true, std::memory_order_acq_rel)) {
    return;
  }
  const AudioObjectPropertyAddress address = {kAudioHardwarePropertyDevices,
                                              kAudioObjectPropertyScopeGlobal,
                                              kAudioObjectPropertyElementMain};
  if (AudioObjectAddPropertyListener(kAudioObjectSystemObject, &address, &endpointPropertyChanged,
                                     this) != noErr) {
    m_endpointMonitorActive.store(false, std::memory_order_release);
    return;
  }
  try {
    m_endpointMonitorThread = std::thread(&AudioDriverManager::endpointMonitorLoop, this);
  } catch (...) {
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &address,
                                      &endpointPropertyChanged, this);
    m_endpointMonitorActive.store(false, std::memory_order_release);
  }
}

void AudioDriverManager::stopEndpointMonitor() {
  if (!m_endpointMonitorActive.exchange(false, std::memory_order_acq_rel) &&
      !m_endpointMonitorThread.joinable()) {
    return;
  }
  const AudioObjectPropertyAddress address = {kAudioHardwarePropertyDevices,
                                              kAudioObjectPropertyScopeGlobal,
                                              kAudioObjectPropertyElementMain};
  AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &address, &endpointPropertyChanged,
                                    this);
  m_endpointChangePending.store(true, std::memory_order_release);
  m_endpointChangeCondition.notify_one();
  if (m_endpointMonitorThread.joinable()) {
    m_endpointMonitorThread.join();
  }
}

void AudioDriverManager::endpointMonitorLoop() {
  while (m_endpointMonitorActive.load(std::memory_order_acquire)) {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_endpointChangeCondition.wait(lock, [this] {
      return m_endpointChangePending.load(std::memory_order_acquire) ||
             !m_endpointMonitorActive.load(std::memory_order_acquire);
    });
    m_endpointChangePending.store(false, std::memory_order_release);
    if (!m_endpointMonitorActive.load(std::memory_order_acquire)) {
      break;
    }
    const auto endpoint_callback = m_endpointChangeCallback;
    lock.unlock();
    if (endpoint_callback) {
      endpoint_callback();
    }
  }
}
#endif

#if defined(_WIN32) && defined(ORPHEUS_ENABLE_WASAPI)
std::vector<AudioDeviceInfo> AudioDriverManager::enumerateWindowsDevices() {
  std::vector<AudioDeviceInfo> devices;
  const HRESULT com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const bool uninitialize = SUCCEEDED(com_result);
  if (FAILED(com_result) && com_result != RPC_E_CHANGED_MODE) {
    return devices;
  }

  IMMDeviceEnumerator* enumerator = nullptr;
  IMMDeviceCollection* collection = nullptr;
  IMMDevice* default_device = nullptr;
  LPWSTR default_id = nullptr;
  if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                              __uuidof(IMMDeviceEnumerator),
                              reinterpret_cast<void**>(&enumerator))) ||
      FAILED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection))) {
    releaseCom(collection);
    releaseCom(enumerator);
    if (uninitialize) {
      CoUninitialize();
    }
    return devices;
  }
  if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &default_device))) {
    default_device->GetId(&default_id);
  }

  UINT count = 0;
  collection->GetCount(&count);
  for (UINT index = 0; index < count; ++index) {
    IMMDevice* device = nullptr;
    IPropertyStore* properties = nullptr;
    IAudioClient* client = nullptr;
    LPWSTR id = nullptr;
    WAVEFORMATEX* mix_format = nullptr;
    PROPVARIANT name;
    PropVariantInit(&name);

    if (FAILED(collection->Item(index, &device)) || FAILED(device->GetId(&id)) ||
        FAILED(device->OpenPropertyStore(STGM_READ, &properties)) ||
        FAILED(properties->GetValue(PKEY_Device_FriendlyName, &name)) ||
        FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                reinterpret_cast<void**>(&client))) ||
        FAILED(client->GetMixFormat(&mix_format)) || mix_format == nullptr) {
      PropVariantClear(&name);
      CoTaskMemFree(id);
      CoTaskMemFree(mix_format);
      releaseCom(client);
      releaseCom(properties);
      releaseCom(device);
      continue;
    }

    AudioDeviceInfo info{};
    const std::string raw_id = wideToUtf8(id);
    info.deviceId = "wasapi:" + raw_id;
    info.name = name.vt == VT_LPWSTR ? wideToUtf8(name.pwszVal) : raw_id;
    info.driverType = "WASAPI";
    info.minChannels = mix_format->nChannels;
    info.maxChannels = mix_format->nChannels;
    info.isDefaultDevice = default_id != nullptr && std::wcscmp(id, default_id) == 0;

    constexpr std::array<uint32_t, 6> sample_rates = {44100, 48000, 88200, 96000, 176400, 192000};
    const size_t format_bytes = sizeof(WAVEFORMATEX) + mix_format->cbSize;
    for (const uint32_t sample_rate : sample_rates) {
      std::vector<unsigned char> candidate_bytes(
          reinterpret_cast<const unsigned char*>(mix_format),
          reinterpret_cast<const unsigned char*>(mix_format) + format_bytes);
      auto* candidate_format = reinterpret_cast<WAVEFORMATEX*>(candidate_bytes.data());
      candidate_format->nSamplesPerSec = sample_rate;
      candidate_format->nAvgBytesPerSec = sample_rate * candidate_format->nBlockAlign;
      WAVEFORMATEX* closest = nullptr;
      if (client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, candidate_format, &closest) == S_OK) {
        appendUnique(info.supportedSampleRates, sample_rate);
      }
      CoTaskMemFree(closest);
    }
    appendUnique(info.supportedSampleRates, mix_format->nSamplesPerSec);
    std::sort(info.supportedSampleRates.begin(), info.supportedSampleRates.end());

    IAudioClient3* client3 = nullptr;
    if (SUCCEEDED(
            client->QueryInterface(__uuidof(IAudioClient3), reinterpret_cast<void**>(&client3)))) {
      UINT32 default_frames = 0;
      UINT32 fundamental_frames = 0;
      UINT32 minimum_frames = 0;
      UINT32 maximum_frames = 0;
      if (SUCCEEDED(client3->GetSharedModeEnginePeriod(mix_format, &default_frames,
                                                       &fundamental_frames, &minimum_frames,
                                                       &maximum_frames))) {
        appendUnique(info.supportedBufferSizes, minimum_frames);
        appendUnique(info.supportedBufferSizes, default_frames);
        appendUnique(info.supportedBufferSizes, maximum_frames);
      }
      releaseCom(client3);
    }
    if (info.supportedBufferSizes.empty()) {
      REFERENCE_TIME default_period = 0;
      REFERENCE_TIME minimum_period = 0;
      if (SUCCEEDED(client->GetDevicePeriod(&default_period, &minimum_period))) {
        appendUnique(info.supportedBufferSizes,
                     static_cast<uint32_t>(
                         (static_cast<uint64_t>(minimum_period) * mix_format->nSamplesPerSec) /
                         10'000'000ULL));
        appendUnique(info.supportedBufferSizes,
                     static_cast<uint32_t>(
                         (static_cast<uint64_t>(default_period) * mix_format->nSamplesPerSec) /
                         10'000'000ULL));
      }
    }
    std::sort(info.supportedBufferSizes.begin(), info.supportedBufferSizes.end());
    devices.push_back(std::move(info));

    PropVariantClear(&name);
    CoTaskMemFree(id);
    CoTaskMemFree(mix_format);
    releaseCom(client);
    releaseCom(properties);
    releaseCom(device);
  }

  CoTaskMemFree(default_id);
  releaseCom(default_device);
  releaseCom(collection);
  releaseCom(enumerator);
  if (uninitialize) {
    CoUninitialize();
  }
  return devices;
}
#endif

#ifdef __linux__
std::vector<AudioDeviceInfo> AudioDriverManager::enumerateLinuxDevices() {
  return {};
}
#endif

std::unique_ptr<IAudioDriver>
AudioDriverManager::createDriverForDevice(const std::string& device_id) {
  if (device_id == "dummy") {
    return createDummyAudioDriver();
  }

#if defined(__APPLE__) && defined(ORPHEUS_ENABLE_COREAUDIO)
  return createCoreAudioDriver();
#endif

#if defined(_WIN32) && defined(ORPHEUS_ENABLE_WASAPI)
  if (device_id.rfind("wasapi:", 0) == 0) {
    return createWASAPIAudioDriver();
  }
#endif

  return nullptr;
}

namespace detail {

std::unique_ptr<IAudioDriverManager> createAudioDriverManagerForTesting(
    std::function<std::unique_ptr<IAudioDriver>(const std::string&)> factory) {
  return std::make_unique<AudioDriverManager>(std::move(factory));
}

} // namespace detail

std::unique_ptr<IAudioDriverManager> createAudioDriverManager() {
  return std::make_unique<AudioDriverManager>();
}

} // namespace orpheus
