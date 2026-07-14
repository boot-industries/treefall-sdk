// SPDX-License-Identifier: MIT
#ifdef _WIN32

#include <orpheus/audio_driver.h>
#include <orpheus/performance_monitor.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <audioclient.h>
#include <avrt.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace orpheus {
namespace {

std::wstring utf8ToWide(const std::string& value) {
  if (value.empty()) {
    return {};
  }
  const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                       static_cast<int>(value.size()), nullptr, 0);
  if (size <= 0) {
    return {};
  }
  std::wstring result(static_cast<size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                      result.data(), size);
  return result;
}

bool isFloatFormat(const WAVEFORMATEX* format) {
  if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
    return format->wBitsPerSample == 32;
  }
  if (format->wFormatTag != WAVE_FORMAT_EXTENSIBLE ||
      format->cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
    return false;
  }
  const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
  return extensible->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT && format->wBitsPerSample == 32;
}

bool isPcm16Format(const WAVEFORMATEX* format) {
  if (format->wFormatTag == WAVE_FORMAT_PCM) {
    return format->wBitsPerSample == 16;
  }
  if (format->wFormatTag != WAVE_FORMAT_EXTENSIBLE ||
      format->cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
    return false;
  }
  const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
  return extensible->SubFormat == KSDATAFORMAT_SUBTYPE_PCM && format->wBitsPerSample == 16;
}

class WASAPIAudioDriver final : public IAudioDriver {
public:
  ~WASAPIAudioDriver() override {
    stop();
    cleanup();
  }

  SessionGraphError initialize(const AudioDriverConfig& requested) override {
    if (running_.load(std::memory_order_acquire) || requested.sample_rate == 0 ||
        requested.buffer_size == 0 || requested.num_outputs == 0) {
      return SessionGraphError::InvalidParameter;
    }
    cleanup();

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(comResult)) {
      comInitialized_ = true;
    } else if (comResult != RPC_E_CHANGED_MODE) {
      return SessionGraphError::InternalError;
    }

    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator),
                                reinterpret_cast<void**>(&enumerator_)))) {
      cleanup();
      return SessionGraphError::InternalError;
    }

    HRESULT result = E_FAIL;
    if (requested.device_id.rfind("wasapi:", 0) == 0) {
      const std::wstring id = utf8ToWide(requested.device_id.substr(7));
      result = id.empty() ? E_INVALIDARG : enumerator_->GetDevice(id.c_str(), &device_);
    } else {
      result = enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
    }
    if (FAILED(result) || device_ == nullptr ||
        FAILED(device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                 reinterpret_cast<void**>(&client_)))) {
      cleanup();
      return SessionGraphError::NotReady;
    }

    WAVEFORMATEX* mixFormat = nullptr;
    if (FAILED(client_->GetMixFormat(&mixFormat)) || mixFormat == nullptr) {
      cleanup();
      return SessionGraphError::InternalError;
    }

    const size_t mixFormatBytes = sizeof(WAVEFORMATEX) + mixFormat->cbSize;
    std::vector<std::byte> requestedBytes(reinterpret_cast<const std::byte*>(mixFormat),
                                          reinterpret_cast<const std::byte*>(mixFormat) +
                                              mixFormatBytes);
    auto* requestedFormat = reinterpret_cast<WAVEFORMATEX*>(requestedBytes.data());
    requestedFormat->nSamplesPerSec = requested.sample_rate;
    requestedFormat->nAvgBytesPerSec =
        requestedFormat->nSamplesPerSec * requestedFormat->nBlockAlign;
    WAVEFORMATEX* closest = nullptr;
    const HRESULT support =
        client_->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, requestedFormat, &closest);
    if (support == S_OK) {
      formatBytes_ = std::move(requestedBytes);
    } else if (closest != nullptr) {
      const size_t bytes = sizeof(WAVEFORMATEX) + closest->cbSize;
      formatBytes_.assign(reinterpret_cast<const std::byte*>(closest),
                          reinterpret_cast<const std::byte*>(closest) + bytes);
    } else {
      const size_t bytes = sizeof(WAVEFORMATEX) + mixFormat->cbSize;
      formatBytes_.assign(reinterpret_cast<const std::byte*>(mixFormat),
                          reinterpret_cast<const std::byte*>(mixFormat) + bytes);
    }
    CoTaskMemFree(closest);
    CoTaskMemFree(mixFormat);
    format_ = reinterpret_cast<WAVEFORMATEX*>(formatBytes_.data());

    if (!isFloatFormat(format_) && !isPcm16Format(format_)) {
      cleanup();
      return SessionGraphError::InvalidParameter;
    }

    const REFERENCE_TIME requestedDuration = static_cast<REFERENCE_TIME>(
        (10'000'000ULL * requested.buffer_size) / format_->nSamplesPerSec);
    if (FAILED(
            client_->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
                                requestedDuration, 0, format_, nullptr)) ||
        FAILED(client_->GetBufferSize(&bufferFrames_)) || bufferFrames_ == 0 ||
        FAILED(client_->GetService(__uuidof(IAudioRenderClient),
                                   reinterpret_cast<void**>(&renderClient_)))) {
      cleanup();
      return SessionGraphError::NotReady;
    }

    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (event_ == nullptr || FAILED(client_->SetEventHandle(event_))) {
      cleanup();
      return SessionGraphError::InternalError;
    }

    config_ = requested;
    config_.sample_rate = format_->nSamplesPerSec;
    config_.buffer_size = static_cast<uint16_t>(
        std::min<UINT32>(bufferFrames_, std::numeric_limits<uint16_t>::max()));
    config_.num_inputs = 0;
    config_.num_outputs = format_->nChannels;
    planarStorage_.assign(static_cast<size_t>(format_->nChannels) * bufferFrames_, 0.0f);
    planarPointers_.resize(format_->nChannels);
    for (size_t channel = 0; channel < planarPointers_.size(); ++channel) {
      planarPointers_[channel] = planarStorage_.data() + channel * bufferFrames_;
    }
    initialized_ = true;
    return SessionGraphError::OK;
  }

  SessionGraphError start(IAudioCallback* callback) override {
    if (!initialized_ || callback == nullptr || running_.exchange(true)) {
      return SessionGraphError::NotReady;
    }
    callback_.store(callback, std::memory_order_release);
    if (FAILED(client_->Start())) {
      callback_.store(nullptr, std::memory_order_release);
      running_.store(false, std::memory_order_release);
      return SessionGraphError::InternalError;
    }
    thread_ = std::thread([this] { audioLoop(); });
    return SessionGraphError::OK;
  }

  SessionGraphError stop() override {
    if (!running_.exchange(false, std::memory_order_acq_rel)) {
      return SessionGraphError::OK;
    }
    if (event_ != nullptr) {
      SetEvent(event_);
    }
    if (thread_.joinable()) {
      thread_.join();
    }
    callback_.store(nullptr, std::memory_order_release);
    return client_ != nullptr && FAILED(client_->Stop()) ? SessionGraphError::InternalError
                                                         : SessionGraphError::OK;
  }

  bool isRunning() const override {
    return running_.load(std::memory_order_acquire);
  }
  const AudioDriverConfig& getConfig() const override {
    return config_;
  }
  std::string getDriverName() const override {
    return "WASAPI";
  }

  uint32_t getLatencySamples() const override {
    if (client_ == nullptr || config_.sample_rate == 0) {
      return 0;
    }
    REFERENCE_TIME latency = 0;
    if (FAILED(client_->GetStreamLatency(&latency))) {
      return 0;
    }
    return static_cast<uint32_t>((static_cast<uint64_t>(latency) * config_.sample_rate) /
                                 10'000'000ULL);
  }

  AudioDriverCapabilities getCapabilities() const override {
    AudioDriverCapabilities capabilities;
    capabilities.backend = AudioBackend::WASAPI;
    capabilities.platform = AudioPlatform::Windows;
    capabilities.supports_exclusive_mode = false;
    capabilities.supports_shared_mode = true;
    capabilities.supports_device_hot_swap = false;
    capabilities.supports_input = false;
    capabilities.supports_multichannel_output = config_.num_outputs > 2;
    capabilities.reports_hardware_latency = getLatencySamples() > 0;
    capabilities.max_input_channels = 0;
    capabilities.max_output_channels = config_.num_outputs;
    if (initialized_) {
      capabilities.native_sample_rates.push_back(config_.sample_rate);
      capabilities.native_buffer_sizes.push_back(config_.buffer_size);
    }
    return capabilities;
  }

  void setPerformanceMonitor(IPerformanceMonitor* monitor) override {
    monitor_.store(monitor, std::memory_order_release);
  }

private:
  void audioLoop() {
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = SUCCEEDED(comResult);
    DWORD taskIndex = 0;
    HANDLE task = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

    while (running_.load(std::memory_order_acquire)) {
      if (WaitForSingleObject(event_, 2000) != WAIT_OBJECT_0 ||
          !running_.load(std::memory_order_acquire)) {
        continue;
      }
      UINT32 padding = 0;
      if (FAILED(client_->GetCurrentPadding(&padding)) || padding >= bufferFrames_) {
        continue;
      }
      const UINT32 frames = bufferFrames_ - padding;
      BYTE* deviceBuffer = nullptr;
      if (FAILED(renderClient_->GetBuffer(frames, &deviceBuffer))) {
        continue;
      }

      for (float* channel : planarPointers_) {
        std::fill_n(channel, frames, 0.0f);
      }
      IAudioCallback* callback = callback_.load(std::memory_order_acquire);
      const auto started = std::chrono::steady_clock::now();
      uint32_t activeClips = 0;
      if (callback != nullptr) {
        callback->processAudio(nullptr, planarPointers_.data(), planarPointers_.size(), frames);
        activeClips = callback->activeClipCount();
      }

      if (isFloatFormat(format_)) {
        auto* output = reinterpret_cast<float*>(deviceBuffer);
        for (UINT32 frame = 0; frame < frames; ++frame) {
          for (size_t channel = 0; channel < planarPointers_.size(); ++channel) {
            output[static_cast<size_t>(frame) * planarPointers_.size() + channel] =
                planarPointers_[channel][frame];
          }
        }
      } else {
        auto* output = reinterpret_cast<int16_t*>(deviceBuffer);
        for (UINT32 frame = 0; frame < frames; ++frame) {
          for (size_t channel = 0; channel < planarPointers_.size(); ++channel) {
            const float sample = std::clamp(planarPointers_[channel][frame], -1.0f, 1.0f);
            output[static_cast<size_t>(frame) * planarPointers_.size() + channel] =
                static_cast<int16_t>(std::lrint(sample * 32767.0f));
          }
        }
      }
      renderClient_->ReleaseBuffer(frames, 0);

      if (IPerformanceMonitor* monitor = monitor_.load(std::memory_order_acquire)) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started);
        const uint64_t budget =
            (static_cast<uint64_t>(frames) * 1'000'000ULL) / config_.sample_rate;
        monitor->recordAudioCallback(static_cast<uint64_t>(elapsed.count()), budget, activeClips,
                                     config_.sample_rate, frames);
      }
    }

    if (task != nullptr) {
      AvRevertMmThreadCharacteristics(task);
    }
    if (uninitialize) {
      CoUninitialize();
    }
  }

  void cleanup() {
    initialized_ = false;
    format_ = nullptr;
    formatBytes_.clear();
    planarPointers_.clear();
    planarStorage_.clear();
    if (event_ != nullptr) {
      CloseHandle(event_);
      event_ = nullptr;
    }
    if (renderClient_ != nullptr) {
      renderClient_->Release();
      renderClient_ = nullptr;
    }
    if (client_ != nullptr) {
      client_->Release();
      client_ = nullptr;
    }
    if (device_ != nullptr) {
      device_->Release();
      device_ = nullptr;
    }
    if (enumerator_ != nullptr) {
      enumerator_->Release();
      enumerator_ = nullptr;
    }
    if (comInitialized_) {
      CoUninitialize();
      comInitialized_ = false;
    }
  }

  AudioDriverConfig config_{};
  IMMDeviceEnumerator* enumerator_ = nullptr;
  IMMDevice* device_ = nullptr;
  IAudioClient* client_ = nullptr;
  IAudioRenderClient* renderClient_ = nullptr;
  HANDLE event_ = nullptr;
  std::vector<std::byte> formatBytes_;
  WAVEFORMATEX* format_ = nullptr;
  UINT32 bufferFrames_ = 0;
  std::vector<float> planarStorage_;
  std::vector<float*> planarPointers_;
  std::thread thread_;
  std::atomic<IAudioCallback*> callback_{nullptr};
  std::atomic<IPerformanceMonitor*> monitor_{nullptr};
  std::atomic<bool> running_{false};
  bool initialized_ = false;
  bool comInitialized_ = false;
};

} // namespace

std::unique_ptr<IAudioDriver> createWASAPIAudioDriver() {
  return std::make_unique<WASAPIAudioDriver>();
}

} // namespace orpheus

#endif
