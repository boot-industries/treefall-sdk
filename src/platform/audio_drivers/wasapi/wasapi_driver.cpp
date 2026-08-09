// SPDX-License-Identifier: MIT
#ifdef _WIN32

#include "wasapi_driver.h"

#include <orpheus/performance_monitor.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <avrt.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
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
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                          static_cast<int>(value.size()), result.data(), size) <= 0) {
    return {};
  }
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

detail::WASAPIRenderStatus statusFromHRESULT(HRESULT result) noexcept {
  if (SUCCEEDED(result)) {
    return detail::WASAPIRenderStatus::Ready;
  }
  if (result == AUDCLNT_E_DEVICE_INVALIDATED || result == AUDCLNT_E_RESOURCES_INVALIDATED) {
    return detail::WASAPIRenderStatus::DeviceInvalidated;
  }
  return detail::WASAPIRenderStatus::BackendFailure;
}

class WASAPIComRenderRuntime final : public detail::IWASAPIRenderRuntime {
public:
  ~WASAPIComRenderRuntime() override {
    cleanup();
  }

  SessionGraphError initialize(const AudioDriverConfig& requested,
                               detail::WASAPIResolvedFormat& resolved) override {
    cleanup();

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(comResult)) {
      com_initialized_ = true;
    } else if (comResult != RPC_E_CHANGED_MODE) {
      return SessionGraphError::InternalError;
    }

    HRESULT result =
        CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                         __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator_));
    if (FAILED(result)) {
      cleanup();
      return SessionGraphError::InternalError;
    }

    if (requested.output_device_id.empty()) {
      result = enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
    } else {
      const std::wstring id = utf8ToWide(requested.output_device_id.substr(7));
      result = id.empty() ? E_INVALIDARG : enumerator_->GetDevice(id.c_str(), &device_);
    }
    if (FAILED(result) || device_ == nullptr) {
      cleanup();
      return requested.output_device_id.empty() ? SessionGraphError::NotReady
                                                : SessionGraphError::InvalidParameter;
    }

    result = device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                               reinterpret_cast<void**>(&client_));
    if (FAILED(result)) {
      cleanup();
      return SessionGraphError::NotReady;
    }

    WAVEFORMATEX* mix_format = nullptr;
    result = client_->GetMixFormat(&mix_format);
    if (FAILED(result) || mix_format == nullptr) {
      cleanup();
      return SessionGraphError::InternalError;
    }

    const size_t mix_bytes = sizeof(WAVEFORMATEX) + mix_format->cbSize;
    std::vector<std::byte> requested_bytes(mix_bytes);
    std::memcpy(requested_bytes.data(), mix_format, mix_bytes);
    auto* requested_format = reinterpret_cast<WAVEFORMATEX*>(requested_bytes.data());
    requested_format->nChannels = requested.num_outputs;
    requested_format->nSamplesPerSec = requested.sample_rate;
    const uint32_t bytes_per_sample = requested_format->wBitsPerSample / 8u;
    if (bytes_per_sample == 0 ||
        static_cast<uint32_t>(requested_format->nChannels) * bytes_per_sample >
            std::numeric_limits<uint16_t>::max()) {
      CoTaskMemFree(mix_format);
      cleanup();
      return SessionGraphError::InvalidParameter;
    }
    requested_format->nBlockAlign =
        static_cast<uint16_t>(requested_format->nChannels * bytes_per_sample);
    requested_format->nAvgBytesPerSec =
        requested_format->nSamplesPerSec * requested_format->nBlockAlign;

    WAVEFORMATEX* closest = nullptr;
    const HRESULT support =
        client_->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, requested_format, &closest);
    std::vector<std::byte> selected_bytes;
    if (support == S_OK) {
      selected_bytes = std::move(requested_bytes);
    } else if (support == S_FALSE && closest != nullptr) {
      const size_t closest_bytes = sizeof(WAVEFORMATEX) + closest->cbSize;
      if (closest->nChannels != requested.num_outputs) {
        CoTaskMemFree(closest);
        CoTaskMemFree(mix_format);
        cleanup();
        return SessionGraphError::InvalidParameter;
      }
      selected_bytes.resize(closest_bytes);
      std::memcpy(selected_bytes.data(), closest, closest_bytes);
    } else {
      CoTaskMemFree(closest);
      CoTaskMemFree(mix_format);
      cleanup();
      return FAILED(support) ? SessionGraphError::NotReady : SessionGraphError::InvalidParameter;
    }
    CoTaskMemFree(closest);
    CoTaskMemFree(mix_format);

    auto* selected_format = reinterpret_cast<WAVEFORMATEX*>(selected_bytes.data());
    if (selected_format->nChannels != requested.num_outputs ||
        (!isFloatFormat(selected_format) && !isPcm16Format(selected_format))) {
      cleanup();
      return SessionGraphError::InvalidParameter;
    }

    const REFERENCE_TIME requested_duration = static_cast<REFERENCE_TIME>(
        (10'000'000ULL * requested.buffer_size) / selected_format->nSamplesPerSec);
    result = client_->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                 AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
                                 requested_duration, 0, selected_format, nullptr);
    if (FAILED(result) || FAILED(client_->GetBufferSize(&buffer_frames_)) || buffer_frames_ == 0 ||
        FAILED(client_->GetService(__uuidof(IAudioRenderClient),
                                   reinterpret_cast<void**>(&render_client_)))) {
      cleanup();
      return SessionGraphError::NotReady;
    }

    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (event_ == nullptr || FAILED(client_->SetEventHandle(event_))) {
      cleanup();
      return SessionGraphError::InternalError;
    }

    format_bytes_ = std::move(selected_bytes);
    format_ = reinterpret_cast<WAVEFORMATEX*>(format_bytes_.data());
    resolved.sample_rate = format_->nSamplesPerSec;
    resolved.buffer_size = static_cast<uint16_t>(
        std::min<UINT32>(buffer_frames_, std::numeric_limits<uint16_t>::max()));
    resolved.num_outputs = format_->nChannels;
    resolved.float32 = isFloatFormat(format_);
    return SessionGraphError::OK;
  }

  detail::WASAPIRenderStatus start() noexcept override {
    return client_ == nullptr ? detail::WASAPIRenderStatus::BackendFailure
                              : statusFromHRESULT(client_->Start());
  }

  void signalStop() noexcept override {
    if (event_ != nullptr) {
      SetEvent(event_);
    }
  }

  detail::WASAPIRenderStatus wait() noexcept override {
    if (event_ == nullptr) {
      return detail::WASAPIRenderStatus::BackendFailure;
    }
    const DWORD result = WaitForSingleObject(event_, 2000);
    if (result == WAIT_OBJECT_0) {
      return detail::WASAPIRenderStatus::Ready;
    }
    if (result == WAIT_TIMEOUT) {
      return detail::WASAPIRenderStatus::Timeout;
    }
    return detail::WASAPIRenderStatus::BackendFailure;
  }

  detail::WASAPIRenderStatus getPadding(uint32_t& padding) noexcept override {
    padding = 0;
    if (client_ == nullptr) {
      return detail::WASAPIRenderStatus::BackendFailure;
    }
    const HRESULT result = client_->GetCurrentPadding(&padding);
    if (FAILED(result)) {
      return statusFromHRESULT(result);
    }
    return padding >= buffer_frames_ ? detail::WASAPIRenderStatus::NoFrames
                                     : detail::WASAPIRenderStatus::Ready;
  }

  detail::WASAPIRenderStatus acquire(uint32_t frames, BYTE*& device_buffer) noexcept override {
    device_buffer = nullptr;
    if (render_client_ == nullptr || frames == 0) {
      return detail::WASAPIRenderStatus::BackendFailure;
    }
    return statusFromHRESULT(render_client_->GetBuffer(frames, &device_buffer));
  }

  detail::WASAPIRenderStatus release(uint32_t frames) noexcept override {
    if (render_client_ == nullptr) {
      return detail::WASAPIRenderStatus::BackendFailure;
    }
    return statusFromHRESULT(render_client_->ReleaseBuffer(frames, 0));
  }

  detail::WASAPIRenderStatus stop() noexcept override {
    return client_ == nullptr ? detail::WASAPIRenderStatus::Ready
                              : statusFromHRESULT(client_->Stop());
  }

  void cleanup() noexcept override {
    format_ = nullptr;
    format_bytes_.clear();
    buffer_frames_ = 0;
    if (event_ != nullptr) {
      CloseHandle(event_);
      event_ = nullptr;
    }
    if (render_client_ != nullptr) {
      render_client_->Release();
      render_client_ = nullptr;
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
    if (com_initialized_) {
      CoUninitialize();
      com_initialized_ = false;
    }
  }

  uint32_t bufferFrames() const noexcept override {
    return buffer_frames_;
  }
  bool float32() const noexcept override {
    return format_ != nullptr && isFloatFormat(format_);
  }

  uint32_t latencySamples(uint32_t sample_rate) const noexcept override {
    if (client_ == nullptr || sample_rate == 0) {
      return 0;
    }
    REFERENCE_TIME latency = 0;
    if (FAILED(client_->GetStreamLatency(&latency))) {
      return 0;
    }
    return static_cast<uint32_t>((static_cast<uint64_t>(latency) * sample_rate) / 10'000'000ULL);
  }

private:
  IMMDeviceEnumerator* enumerator_{nullptr};
  IMMDevice* device_{nullptr};
  IAudioClient* client_{nullptr};
  IAudioRenderClient* render_client_{nullptr};
  HANDLE event_{nullptr};
  std::vector<std::byte> format_bytes_;
  WAVEFORMATEX* format_{nullptr};
  UINT32 buffer_frames_{0};
  bool com_initialized_{false};
};

} // namespace

namespace detail {

std::unique_ptr<IWASAPIRenderRuntime> createWASAPIRenderRuntime() {
  return std::make_unique<WASAPIComRenderRuntime>();
}

} // namespace detail

WASAPIAudioDriver::WASAPIAudioDriver()
    : WASAPIAudioDriver(detail::createWASAPIRenderRuntime(), {}) {}

WASAPIAudioDriver::WASAPIAudioDriver(std::unique_ptr<detail::IWASAPIRenderRuntime> runtime,
                                     detail::WASAPIWorkerLauncher worker_launcher)
    : runtime_(std::move(runtime)), worker_launcher_(std::move(worker_launcher)) {
  if (!runtime_) {
    runtime_ = detail::createWASAPIRenderRuntime();
  }
  if (!worker_launcher_) {
    worker_launcher_ = [](std::function<void()> worker) { return std::thread(std::move(worker)); };
  }
}

WASAPIAudioDriver::~WASAPIAudioDriver() {
  (void)stop();
  cleanup();
}

SessionGraphError WASAPIAudioDriver::initialize(const AudioDriverConfig& requested) {
  if (running_.load(std::memory_order_acquire)) {
    return SessionGraphError::NotReady;
  }
  if (requested.sample_rate == 0 || requested.buffer_size == 0 || requested.num_outputs == 0 ||
      requested.num_inputs != 0 || !requested.input_device_id.empty() ||
      (!requested.output_device_id.empty() &&
       (requested.output_device_id.rfind("wasapi:", 0) != 0 ||
        requested.output_device_id.size() == 7))) {
    return SessionGraphError::InvalidParameter;
  }

  // Reinitialize is the explicit recovery boundary after any terminal worker or
  // backend failure. Join and release all previous COM/runtime state first.
  if (hasResources()) {
    (void)stop();
  }
  clearCallback();
  runtime_->cleanup();
  planar_storage_.clear();
  planar_pointers_.clear();
  resolved_ = {};
  initialized_.store(false, std::memory_order_release);

  detail::WASAPIResolvedFormat resolved;
  try {
    const SessionGraphError result = runtime_->initialize(requested, resolved);
    if (result != SessionGraphError::OK) {
      runtime_->cleanup();
      return result;
    }
    if (resolved.num_outputs != requested.num_outputs || resolved.sample_rate == 0 ||
        resolved.buffer_size == 0 || runtime_->bufferFrames() == 0) {
      runtime_->cleanup();
      return SessionGraphError::InvalidParameter;
    }

    const size_t channels = resolved.num_outputs;
    const size_t frames = runtime_->bufferFrames();
    planar_storage_.assign(channels * frames, 0.0f);
    planar_pointers_.resize(channels);
    for (size_t channel = 0; channel < channels; ++channel) {
      planar_pointers_[channel] = planar_storage_.data() + channel * frames;
    }
  } catch (...) {
    runtime_->cleanup();
    planar_storage_.clear();
    planar_pointers_.clear();
    return SessionGraphError::InternalError;
  }

  config_ = requested;
  config_.sample_rate = resolved.sample_rate;
  config_.buffer_size = resolved.buffer_size;
  config_.num_inputs = 0;
  config_.num_outputs = resolved.num_outputs;
  resolved_ = resolved;
  initialized_.store(true, std::memory_order_release);
  reinitialize_required_.store(false, std::memory_order_release);
  route_outcome_.store(AudioRouteRuntimeOutcome::Healthy, std::memory_order_release);
  return SessionGraphError::OK;
}

SessionGraphError WASAPIAudioDriver::start(IAudioCallback* callback) {
  if (callback == nullptr) {
    return SessionGraphError::InvalidParameter;
  }
  if (!initialized_.load(std::memory_order_acquire) ||
      reinitialize_required_.load(std::memory_order_acquire) ||
      running_.load(std::memory_order_acquire) || thread_.joinable()) {
    return SessionGraphError::NotReady;
  }

  callback_target_.replaceAndDrain(callback);
  running_.store(true, std::memory_order_release);

  if (runtime_->start() != detail::WASAPIRenderStatus::Ready) {
    running_.store(false, std::memory_order_release);
    clearCallback();
    const auto rollback = runtime_->stop();
    if (rollback != detail::WASAPIRenderStatus::Ready) {
      markTerminalFailure();
    }
    return SessionGraphError::InternalError;
  }

  try {
    thread_ = worker_launcher_([this] { audioLoop(); });
    if (!thread_.joinable()) {
      throw std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again));
    }
  } catch (...) {
    running_.store(false, std::memory_order_release);
    clearCallback();
    runtime_->signalStop();
    const auto rollback = runtime_->stop();
    if (rollback != detail::WASAPIRenderStatus::Ready) {
      markTerminalFailure();
    }
    return SessionGraphError::InternalError;
  }
  return SessionGraphError::OK;
}

SessionGraphError WASAPIAudioDriver::stop() {
  if (!hasResources()) {
    return SessionGraphError::OK;
  }

  const bool worker_joinable = thread_.joinable();
  const bool was_running = running_.exchange(false, std::memory_order_acq_rel);
  if (!was_running && !worker_joinable) {
    clearCallback();
    return SessionGraphError::OK;
  }

  clearCallback();
  runtime_->signalStop();
  if (thread_.joinable()) {
    thread_.join();
  }

  const auto result = runtime_->stop();
  if (result != detail::WASAPIRenderStatus::Ready) {
    markTerminalFailure();
    return SessionGraphError::InternalError;
  }
  return SessionGraphError::OK;
}

bool WASAPIAudioDriver::isRunning() const {
  return running_.load(std::memory_order_acquire);
}

const AudioDriverConfig& WASAPIAudioDriver::getConfig() const {
  return config_;
}

std::string WASAPIAudioDriver::getDriverName() const {
  return "WASAPI";
}

uint32_t WASAPIAudioDriver::getLatencySamples() const {
  return runtime_->latencySamples(config_.sample_rate);
}

AudioIoTelemetry WASAPIAudioDriver::getTelemetry() const noexcept {
  return {0, route_outcome_.load(std::memory_order_acquire)};
}

AudioDriverCapabilities WASAPIAudioDriver::getCapabilities() const {
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
  if (initialized_.load(std::memory_order_acquire)) {
    capabilities.native_sample_rates.push_back(config_.sample_rate);
    capabilities.native_buffer_sizes.push_back(config_.buffer_size);
    const std::string endpoint =
        config_.output_device_id.empty() ? "wasapi:default" : config_.output_device_id;
    for (uint16_t channel = 0; channel < config_.num_outputs; ++channel) {
      capabilities.output_channel_ids.push_back(endpoint + ":output:" + std::to_string(channel));
    }
  }
  return capabilities;
}

void WASAPIAudioDriver::setPerformanceMonitor(IPerformanceMonitor* monitor) {
  performance_monitor_target_.replaceAndDrain(monitor);
}

void WASAPIAudioDriver::clearCallback() noexcept {
  callback_target_.replaceAndDrain(nullptr);
}

void WASAPIAudioDriver::markTerminalFailure() noexcept {
  running_.store(false, std::memory_order_release);
  reinitialize_required_.store(true, std::memory_order_release);
  route_outcome_.store(AudioRouteRuntimeOutcome::BackendFailure, std::memory_order_release);
  clearCallback();
}

bool WASAPIAudioDriver::hasResources() const noexcept {
  return initialized_.load(std::memory_order_acquire) || thread_.joinable() ||
         resolved_.buffer_size != 0;
}

void WASAPIAudioDriver::audioLoop() noexcept {
  bool discontinuity = true;
  bool terminal_failure = false;

  while (running_.load(std::memory_order_acquire)) {
    const auto wait_result = runtime_->wait();
    if (wait_result == detail::WASAPIRenderStatus::NoFrames) {
      discontinuity = true;
      continue;
    }
    if (wait_result != detail::WASAPIRenderStatus::Ready) {
      terminal_failure = true;
      break;
    }
    if (!running_.load(std::memory_order_acquire)) {
      break;
    }

    uint32_t padding = 0;
    const auto padding_result = runtime_->getPadding(padding);
    if (padding_result == detail::WASAPIRenderStatus::NoFrames) {
      discontinuity = true;
      continue;
    }
    if (padding_result != detail::WASAPIRenderStatus::Ready ||
        padding >= runtime_->bufferFrames()) {
      terminal_failure = true;
      break;
    }

    const uint32_t frames = runtime_->bufferFrames() - padding;
    BYTE* device_buffer = nullptr;
    const auto acquire_result = runtime_->acquire(frames, device_buffer);
    if (acquire_result != detail::WASAPIRenderStatus::Ready || device_buffer == nullptr) {
      terminal_failure = true;
      break;
    }

    for (float* channel : planar_pointers_) {
      std::fill_n(channel, frames, 0.0f);
    }

    uint32_t active_clips = 0;
#if defined(ORPHEUS_ENABLE_AUDIO_CALLBACK_TIMING)
    auto monitor_lease = performance_monitor_target_.tryAcquire();
    const auto callback_start =
        monitor_lease ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
#endif
    {
      auto callback_lease = callback_target_.tryAcquire();
      if (callback_lease) {
        AudioProcessBlock block;
        block.output_buffers = planar_pointers_.data();
        block.num_output_channels = static_cast<uint16_t>(planar_pointers_.size());
        block.num_frames = frames;
        // WASAPI shared-mode padding has no device-clock correlation here.
        block.device_sample_position = 0;
        block.host_time_nanoseconds = 0;
        block.discontinuity = discontinuity;
        callback_lease.get()->processAudio(block);
        active_clips = callback_lease.get()->activeClipCount();
      }
    }

    if (resolved_.float32) {
      auto* output = reinterpret_cast<float*>(device_buffer);
      for (uint32_t frame = 0; frame < frames; ++frame) {
        for (size_t channel = 0; channel < planar_pointers_.size(); ++channel) {
          output[static_cast<size_t>(frame) * planar_pointers_.size() + channel] =
              planar_pointers_[channel][frame];
        }
      }
    } else {
      auto* output = reinterpret_cast<int16_t*>(device_buffer);
      for (uint32_t frame = 0; frame < frames; ++frame) {
        for (size_t channel = 0; channel < planar_pointers_.size(); ++channel) {
          const float sample = std::clamp(planar_pointers_[channel][frame], -1.0f, 1.0f);
          output[static_cast<size_t>(frame) * planar_pointers_.size() + channel] =
              static_cast<int16_t>(std::lrint(sample * 32767.0f));
        }
      }
    }

    const auto release_result = runtime_->release(frames);
    if (release_result != detail::WASAPIRenderStatus::Ready) {
      terminal_failure = true;
      break;
    }

#if defined(ORPHEUS_ENABLE_AUDIO_CALLBACK_TIMING)
    if (auto* monitor = monitor_lease.get()) {
      const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - callback_start);
      const uint64_t budget = (static_cast<uint64_t>(frames) * 1'000'000ULL) / config_.sample_rate;
      monitor->recordAudioCallback(static_cast<uint64_t>(elapsed.count()), budget, active_clips,
                                   config_.sample_rate, frames);
    }
#endif
    discontinuity = false;
  }

  if (terminal_failure) {
    markTerminalFailure();
  }
}

void WASAPIAudioDriver::cleanup() noexcept {
  clearCallback();
  performance_monitor_target_.replaceAndDrain(nullptr);
  runtime_->cleanup();
  planar_pointers_.clear();
  planar_storage_.clear();
  resolved_ = {};
  initialized_.store(false, std::memory_order_release);
}

} // namespace orpheus

#endif
