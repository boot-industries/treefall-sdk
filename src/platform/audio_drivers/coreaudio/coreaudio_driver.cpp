// SPDX-License-Identifier: MIT
#include "coreaudio_driver.h"
#include "../../../core/common/realtime_counter.h"
#include <orpheus/performance_monitor.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>

namespace orpheus {

namespace {

std::optional<UInt32> getOptionalUInt32Property(AudioDeviceID device_id,
                                                AudioObjectPropertySelector selector,
                                                AudioObjectPropertyScope scope);
std::optional<Float64> getOptionalFloat64Property(AudioDeviceID device_id,
                                                  AudioObjectPropertySelector selector,
                                                  AudioObjectPropertyScope scope);
std::optional<UInt32> getOptionalStreamLatency(AudioDeviceID device_id,
                                               AudioObjectPropertyScope scope);

CFStringRef copyDeviceUID(AudioDeviceID device_id) {
  AudioObjectPropertyAddress property_address = {kAudioDevicePropertyDeviceUID,
                                                 kAudioObjectPropertyScopeGlobal,
                                                 kAudioObjectPropertyElementMain};
  CFStringRef uid = nullptr;
  UInt32 data_size = sizeof(CFStringRef);
  const OSStatus status =
      AudioObjectGetPropertyData(device_id, &property_address, 0, nullptr, &data_size, &uid);
  return status == noErr ? uid : nullptr;
}

UInt32 getClockDomain(AudioDeviceID device_id) {
  AudioObjectPropertyAddress property_address = {kAudioDevicePropertyClockDomain,
                                                 kAudioObjectPropertyScopeGlobal,
                                                 kAudioObjectPropertyElementMain};
  UInt32 domain = 0;
  UInt32 data_size = sizeof(domain);
  AudioObjectGetPropertyData(device_id, &property_address, 0, nullptr, &data_size, &domain);
  return domain;
}

UInt32 getUInt32Property(AudioDeviceID device_id, AudioObjectPropertySelector selector,
                         AudioObjectPropertyScope scope) {
  if (device_id == 0) {
    return 0;
  }
  AudioObjectPropertyAddress address = {selector, scope, kAudioObjectPropertyElementMain};
  UInt32 value = 0;
  UInt32 size = sizeof(value);
  return AudioObjectGetPropertyData(device_id, &address, 0, nullptr, &size, &value) == noErr ? value
                                                                                             : 0;
}

std::optional<UInt32> getOptionalUInt32Property(AudioDeviceID device_id,
                                                AudioObjectPropertySelector selector,
                                                AudioObjectPropertyScope scope) {
  if (device_id == 0) {
    return std::nullopt;
  }
  AudioObjectPropertyAddress address = {selector, scope, kAudioObjectPropertyElementMain};
  UInt32 value = 0;
  UInt32 size = sizeof(value);
  if (AudioObjectGetPropertyData(device_id, &address, 0, nullptr, &size, &value) != noErr ||
      size != sizeof(value)) {
    return std::nullopt;
  }
  return value;
}

std::optional<Float64> getOptionalFloat64Property(AudioDeviceID device_id,
                                                  AudioObjectPropertySelector selector,
                                                  AudioObjectPropertyScope scope) {
  if (device_id == 0) {
    return std::nullopt;
  }
  AudioObjectPropertyAddress address = {selector, scope, kAudioObjectPropertyElementMain};
  Float64 value = 0.0;
  UInt32 size = sizeof(value);
  if (AudioObjectGetPropertyData(device_id, &address, 0, nullptr, &size, &value) != noErr ||
      size != sizeof(value)) {
    return std::nullopt;
  }
  return value;
}

std::optional<UInt32> getOptionalStreamLatency(AudioDeviceID device_id,
                                               AudioObjectPropertyScope scope) {
  AudioObjectPropertyAddress streams_address = {kAudioDevicePropertyStreams, scope,
                                                kAudioObjectPropertyElementMain};
  UInt32 size = 0;
  if (AudioObjectGetPropertyDataSize(device_id, &streams_address, 0, nullptr, &size) != noErr ||
      size == 0) {
    return std::nullopt;
  }

  std::vector<AudioStreamID> streams(size / sizeof(AudioStreamID));
  if (streams.empty() || AudioObjectGetPropertyData(device_id, &streams_address, 0, nullptr, &size,
                                                    streams.data()) != noErr) {
    return std::nullopt;
  }

  UInt32 latency = 0;
  bool observed = false;
  for (const AudioStreamID stream : streams) {
    AudioObjectPropertyAddress address = {kAudioStreamPropertyLatency,
                                          kAudioObjectPropertyScopeGlobal,
                                          kAudioObjectPropertyElementMain};
    UInt32 stream_latency = 0;
    UInt32 data_size = sizeof(stream_latency);
    if (AudioObjectGetPropertyData(stream, &address, 0, nullptr, &data_size, &stream_latency) ==
            noErr &&
        data_size == sizeof(stream_latency)) {
      latency = std::max(latency, stream_latency);
      observed = true;
    }
  }
  return observed ? std::optional<UInt32>(latency) : std::nullopt;
}

bool addLatencyTerm(const std::optional<UInt32>& term, uint32_t& total) {
  if (!term.has_value()) {
    return false;
  }
  const uint64_t sum = static_cast<uint64_t>(total) + *term;
  total = static_cast<uint32_t>(std::min<uint64_t>(sum, std::numeric_limits<uint32_t>::max()));
  return true;
}

} // namespace

CoreAudioDriver::CoreAudioDriver() : route_resolver_(detail::makeCoreAudioRouteQuery()) {}

CoreAudioDriver::CoreAudioDriver(std::shared_ptr<const detail::ICoreAudioRouteQuery> query)
    : route_resolver_(query ? std::move(query) : detail::makeCoreAudioRouteQuery()) {}

CoreAudioDriver::~CoreAudioDriver() {
  stop();
  stopRouteMonitor();
  cleanupAudioUnit();
}

SessionGraphError CoreAudioDriver::initialize(const AudioDriverConfig& config) {
  if (config.sample_rate == 0 || config.buffer_size == 0 || config.num_outputs == 0) {
    return SessionGraphError::InvalidParameter;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_running_.load(std::memory_order_acquire)) {
      return SessionGraphError::NotReady;
    }
  }

  stopRouteMonitor();

  std::lock_guard<std::mutex> lock(mutex_);
  if (is_running_.load(std::memory_order_acquire)) {
    return SessionGraphError::NotReady;
  }

  cleanupAudioUnit();
  config_ = config;
  active_route_ = {};
  expected_stream_sample_ = 0;
  stream_timeline_initialized_ = false;
  runtime_outcome_.store(AudioDriverRuntimeOutcome::Healthy, std::memory_order_release);
  route_outcome_.store(AudioRouteRuntimeOutcome::Healthy, std::memory_order_release);
  unavailable_route_state_.store(AudioRouteState::Failed, std::memory_order_release);

  auto resolved_route = route_resolver_.resolve(config);
  const auto compatibility_status = resolved_route.compatibility.status;
  const bool activation_eligible =
      resolved_route.resolved &&
      (compatibility_status == AudioRouteCompatibilityStatus::Compatible ||
       compatibility_status == AudioRouteCompatibilityStatus::RequiresSampleRateChange);
  if (!activation_eligible) {
    switch (compatibility_status) {
    case AudioRouteCompatibilityStatus::InputUnavailable:
      unavailable_route_state_.store(AudioRouteState::InputUnavailable, std::memory_order_release);
      route_outcome_.store(AudioRouteRuntimeOutcome::RouteUnavailable, std::memory_order_release);
      break;
    case AudioRouteCompatibilityStatus::OutputUnavailable:
      unavailable_route_state_.store(AudioRouteState::OutputUnavailable, std::memory_order_release);
      route_outcome_.store(AudioRouteRuntimeOutcome::RouteUnavailable, std::memory_order_release);
      break;
    case AudioRouteCompatibilityStatus::PermissionDenied:
      unavailable_route_state_.store(AudioRouteState::PermissionDenied, std::memory_order_release);
      route_outcome_.store(AudioRouteRuntimeOutcome::RouteUnavailable, std::memory_order_release);
      break;
    case AudioRouteCompatibilityStatus::BackendFailure:
      route_outcome_.store(AudioRouteRuntimeOutcome::BackendFailure, std::memory_order_release);
      break;
    case AudioRouteCompatibilityStatus::SampleRateUnsupported:
    case AudioRouteCompatibilityStatus::InvalidChannelMap:
    case AudioRouteCompatibilityStatus::Compatible:
    case AudioRouteCompatibilityStatus::RequiresSampleRateChange:
      route_outcome_.store(AudioRouteRuntimeOutcome::ReinitializationRequired,
                           std::memory_order_release);
      break;
    }
    cleanupAudioUnit();
    return SessionGraphError::InvalidParameter;
  }

  input_device_id_ = resolved_route.input_device_id;
  output_device_id_ = resolved_route.output_device_id;
  available_input_channels_ = resolved_route.input_channel_count;
  available_output_channels_ = resolved_route.output_channel_count;
  input_channel_map_ = std::move(resolved_route.input_channel_map);
  output_channel_map_ = std::move(resolved_route.output_channel_map);
  device_id_ = output_device_id_;

  if (resolved_route.requires_private_aggregate) {
    input_channel_offset_ = getChannelCount(output_device_id_, kAudioObjectPropertyScopeInput);
    aggregate_device_id_ = createAggregateDevice(input_device_id_, output_device_id_);
    if (aggregate_device_id_ == 0) {
      route_outcome_.store(AudioRouteRuntimeOutcome::BackendFailure, std::memory_order_release);
      cleanupAudioUnit();
      return SessionGraphError::InvalidParameter;
    }
    device_id_ = aggregate_device_id_;
  }

  try {
    input_storage_.assign(static_cast<size_t>(config_.num_inputs) * config_.buffer_size, 0.0f);
    output_storage_.assign(static_cast<size_t>(config_.num_outputs) * config_.buffer_size, 0.0f);
    input_buffers_.resize(config_.num_inputs);
    output_buffers_.resize(config_.num_outputs);
    for (uint16_t channel = 0; channel < config_.num_inputs; ++channel) {
      input_buffers_[channel] =
          input_storage_.data() + static_cast<size_t>(channel) * config_.buffer_size;
    }
    for (uint16_t channel = 0; channel < config_.num_outputs; ++channel) {
      output_buffers_[channel] =
          output_storage_.data() + static_cast<size_t>(channel) * config_.buffer_size;
    }
    if (config_.num_inputs > 0) {
      const size_t buffer_list_size =
          sizeof(AudioBufferList) +
          static_cast<size_t>(config_.num_inputs - 1) * sizeof(AudioBuffer);
      input_abl_storage_.resize(buffer_list_size);
    }
  } catch (...) {
    cleanupAudioUnit();
    return SessionGraphError::InternalError;
  }

  const SessionGraphError setup_result = setupAudioUnit(device_id_);
  if (setup_result != SessionGraphError::OK) {
    route_outcome_.store(AudioRouteRuntimeOutcome::BackendFailure, std::memory_order_release);
    cleanupAudioUnit();
    return setup_result;
  }

  if (!createRouteMonitor()) {
    route_outcome_.store(AudioRouteRuntimeOutcome::BackendFailure, std::memory_order_release);
    cleanupAudioUnit();
    return SessionGraphError::InternalError;
  }

  refreshActiveRouteLocked();
  if (active_route_.output_device_id.empty() || active_route_.actual_sample_rate == 0 ||
      active_route_.actual_buffer_frames == 0 ||
      (config_.num_inputs > 0 && active_route_.input_device_id.empty())) {
    route_outcome_.store(AudioRouteRuntimeOutcome::BackendFailure, std::memory_order_release);
    cleanupAudioUnit();
    active_route_ = {};
    return SessionGraphError::InternalError;
  }

  if (active_route_.actual_sample_rate != config_.sample_rate ||
      active_route_.actual_buffer_frames != config_.buffer_size) {
    route_outcome_.store(AudioRouteRuntimeOutcome::ReinitializationRequired,
                         std::memory_order_release);
    cleanupAudioUnit();
    active_route_ = {};
    return SessionGraphError::InvalidParameter;
  }

  input_render_failures_.store(0, std::memory_order_release);
  runtime_outcome_.store(AudioDriverRuntimeOutcome::Healthy, std::memory_order_release);
  route_outcome_.store(AudioRouteRuntimeOutcome::Healthy, std::memory_order_release);
  return SessionGraphError::OK;
}

SessionGraphError CoreAudioDriver::initializeAudioOutput(const AudioOutputRouteRequest& request) {
  if (request.output_channel_map.empty() || request.requested_sample_rate == 0 ||
      request.requested_buffer_size == 0 || request.requested_buffer_size > 0xffffu ||
      request.output_channel_map.size() > 0xffffu) {
    return SessionGraphError::InvalidParameter;
  }
  for (size_t index = 0; index < request.output_channel_map.size(); ++index) {
    for (size_t previous = 0; previous < index; ++previous) {
      if (request.output_channel_map[index] == request.output_channel_map[previous]) {
        return SessionGraphError::InvalidParameter;
      }
    }
  }

  AudioDriverConfig config;
  config.sample_rate = request.requested_sample_rate;
  config.buffer_size = static_cast<uint16_t>(request.requested_buffer_size);
  config.num_inputs = 0;
  config.num_outputs = static_cast<uint16_t>(request.output_channel_map.size());
  config.output_device_id = request.output_device_id;
  config.channel_map.output_channels = request.output_channel_map;
  return initialize(config);
}

SessionGraphError CoreAudioDriver::start(IAudioCallback* callback) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!audio_unit_ || !route_monitor_) {
    return SessionGraphError::NotReady;
  }
  if (is_running_.load(std::memory_order_acquire)) {
    return SessionGraphError::NotReady;
  }
  if (!callback) {
    return SessionGraphError::InvalidParameter;
  }
  if (route_outcome_.load(std::memory_order_acquire) != AudioRouteRuntimeOutcome::Healthy) {
    return SessionGraphError::NotReady;
  }

  if (route_monitor_thread_.joinable()) {
    route_monitor_thread_.join();
  }

  if (!startRouteMonitorLocked()) {
    route_monitor_->stop();
    return SessionGraphError::InternalError;
  }

  callback_target_.replaceAndDrain(callback);
  expected_stream_sample_ = 0;
  stream_timeline_initialized_ = false;
  is_running_.store(true, std::memory_order_release);

  const OSStatus status = AudioOutputUnitStart(audio_unit_);
  if (status != noErr) {
    is_running_.store(false, std::memory_order_release);
    callback_target_.replaceAndDrain(nullptr);
    route_monitor_->stop();
    route_outcome_.store(AudioRouteRuntimeOutcome::BackendFailure, std::memory_order_release);
    runtime_outcome_.store(AudioDriverRuntimeOutcome::BackendFailure, std::memory_order_release);
    return SessionGraphError::InternalError;
  }

  route_monitor_active_.store(true, std::memory_order_release);
  try {
    route_monitor_thread_ = std::thread(&CoreAudioDriver::routeMonitorLoop, this);
  } catch (...) {
    route_monitor_active_.store(false, std::memory_order_release);
    AudioOutputUnitStop(audio_unit_);
    callback_target_.replaceAndDrain(nullptr);
    is_running_.store(false, std::memory_order_release);
    route_monitor_->stop();
    route_outcome_.store(AudioRouteRuntimeOutcome::BackendFailure, std::memory_order_release);
    runtime_outcome_.store(AudioDriverRuntimeOutcome::BackendFailure, std::memory_order_release);
    return SessionGraphError::InternalError;
  }

  route_outcome_.store(AudioRouteRuntimeOutcome::Healthy, std::memory_order_release);
  return SessionGraphError::OK;
}

SessionGraphError CoreAudioDriver::stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopRenderingLocked();
  }
  stopRouteMonitor();
  return SessionGraphError::OK;
}

bool CoreAudioDriver::isRunning() const {
  return is_running_.load(std::memory_order_acquire);
}

const AudioDriverConfig& CoreAudioDriver::getConfig() const {
  return config_;
}

std::string CoreAudioDriver::getDriverName() const {
  return "CoreAudio";
}

uint32_t CoreAudioDriver::getLatencySamples() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return queryDeviceLatency();
}

AudioIoTelemetry CoreAudioDriver::getTelemetry() const noexcept {
  return {input_render_failures_.load(std::memory_order_acquire),
          runtime_outcome_.load(std::memory_order_acquire),
          route_outcome_.load(std::memory_order_acquire)};
}

AudioRouteCompatibility CoreAudioDriver::probeRoute(const AudioDriverConfig& config) const {
  return route_resolver_.resolve(config).compatibility;
}

ActiveAudioRoute CoreAudioDriver::getActiveRoute() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return active_route_;
}

AudioIoRouteState CoreAudioDriver::getAudioIoRouteState() const {
  std::lock_guard<std::mutex> lock(mutex_);
  AudioIoRouteState state;
  const AudioRouteRuntimeOutcome route_outcome = route_outcome_.load(std::memory_order_acquire);
  if (is_running_.load(std::memory_order_acquire) &&
      route_outcome == AudioRouteRuntimeOutcome::Healthy) {
    state.state = AudioRouteState::Running;
  } else if (route_outcome == AudioRouteRuntimeOutcome::RouteUnavailable) {
    state.state = unavailable_route_state_.load(std::memory_order_acquire);
    switch (state.state) {
    case AudioRouteState::InputUnavailable:
      state.detail = "The selected CoreAudio input endpoint is unavailable.";
      break;
    case AudioRouteState::OutputUnavailable:
      state.detail = "The selected CoreAudio output endpoint is unavailable.";
      break;
    default:
      state.detail = "The selected CoreAudio route is unavailable.";
      break;
    }
  } else if (route_outcome == AudioRouteRuntimeOutcome::FormatChanged ||
             route_outcome == AudioRouteRuntimeOutcome::ReinitializationRequired) {
    state.state = AudioRouteState::ReconfigurationRequired;
    state.detail = "The active CoreAudio format or route changed; reinitialize explicitly.";
  } else if (route_outcome == AudioRouteRuntimeOutcome::BackendFailure) {
    state.state = AudioRouteState::Failed;
    state.detail = "CoreAudio reported a terminal route failure.";
  } else {
    state.state = AudioRouteState::Inactive;
  }

  state.selected_input_device_id = config_.input_device_id;
  state.selected_output_device_id = config_.output_device_id;
  state.active_input_device_id = active_route_.input_device_id;
  state.active_output_device_id = active_route_.output_device_id;
  state.active_input_channel_map = active_route_.input_channels;
  state.active_output_channel_map = active_route_.output_channels;
  state.requested_sample_rate = active_route_.requested_sample_rate;
  state.actual_sample_rate = active_route_.actual_sample_rate;
  state.requested_buffer_size = config_.buffer_size;
  state.actual_buffer_size = active_route_.actual_buffer_frames;
  state.latency = queryDetailedLatencyBreakdown();
  return state;
}
void CoreAudioDriver::setInputRenderFailuresForTesting(uint64_t count) noexcept {
  input_render_failures_.store(count, std::memory_order_release);
}

void CoreAudioDriver::incrementInputRenderFailuresForTesting() noexcept {
  recordInputRenderFailure();
}

void CoreAudioDriver::recordInputRenderFailure() noexcept {
  detail::publishSaturatingIncrement(input_render_failures_);
}

AudioDriverCapabilities CoreAudioDriver::getCapabilities() const {
  AudioDriverCapabilities caps;
  caps.backend = AudioBackend::CoreAudio;
  caps.platform = AudioPlatform::macOS;
  caps.min_output_channels = config_.num_outputs == 0 ? 0 : 1;
  caps.max_output_channels = config_.num_outputs;
  caps.min_input_channels = 0;
  caps.max_input_channels = config_.num_inputs;
  caps.native_sample_rates.push_back(config_.sample_rate);
  caps.native_buffer_sizes.push_back(config_.buffer_size);
  const std::string input_endpoint =
      config_.input_device_id.empty() ? "coreaudio:default-input" : config_.input_device_id;
  const std::string output_endpoint =
      config_.output_device_id.empty() ? "coreaudio:default-output" : config_.output_device_id;
  for (uint16_t channel = 0; channel < config_.num_inputs; ++channel) {
    caps.input_channel_ids.push_back(input_endpoint + ":input:" + std::to_string(channel));
  }
  for (uint16_t channel = 0; channel < config_.num_outputs; ++channel) {
    caps.output_channel_ids.push_back(output_endpoint + ":output:" + std::to_string(channel));
  }
  caps.supports_exclusive_mode = false;
  caps.supports_shared_mode = true;
  caps.supports_device_hot_swap = true;
  caps.supports_input = config_.num_inputs > 0;
  caps.supports_multichannel_output = config_.num_outputs > 2;
  caps.reports_hardware_latency = getLatencySamples() > 0;
  return caps;
}

void CoreAudioDriver::setPerformanceMonitor(IPerformanceMonitor* monitor) {
  performance_monitor_target_.replaceAndDrain(monitor);
}

OSStatus CoreAudioDriver::renderCallback(void* inRefCon, AudioUnitRenderActionFlags* ioActionFlags,
                                         const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber,
                                         UInt32 inNumberFrames, AudioBufferList* ioData) {
  (void)inBusNumber;

  auto* driver = static_cast<CoreAudioDriver*>(inRefCon);
  assert(driver != nullptr);

  for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i) {
    std::memset(ioData->mBuffers[i].mData, 0, ioData->mBuffers[i].mDataByteSize);
  }

  auto callback_lease = driver->callback_target_.tryAcquire();
  if (!callback_lease) {
    return noErr;
  }

  const uint32_t frames_to_process = std::min(static_cast<uint32_t>(inNumberFrames),
                                              static_cast<uint32_t>(driver->config_.buffer_size));
  const uint32_t num_output_channels = driver->config_.num_outputs;
  for (uint32_t channel = 0; channel < num_output_channels; ++channel) {
    std::memset(driver->output_buffers_[channel], 0, frames_to_process * sizeof(float));
  }

  const uint32_t num_input_channels = driver->config_.num_inputs;
  for (uint32_t channel = 0; channel < num_input_channels; ++channel) {
    std::memset(driver->input_buffers_[channel], 0, frames_to_process * sizeof(float));
  }

  if (!driver->is_running_.load(std::memory_order_acquire)) {
    return noErr;
  }
  if (!driver->route_monitor_ || !driver->route_monitor_->permitsRendering()) {
    return noErr;
  }

  IAudioCallback* callback = callback_lease.get();
  if (num_input_channels > 0 && !driver->input_abl_storage_.empty()) {
    auto* input_abl = reinterpret_cast<AudioBufferList*>(driver->input_abl_storage_.data());
    input_abl->mNumberBuffers = num_input_channels;
    for (uint32_t channel = 0; channel < num_input_channels; ++channel) {
      input_abl->mBuffers[channel].mNumberChannels = 1;
      input_abl->mBuffers[channel].mDataByteSize = frames_to_process * sizeof(float);
      input_abl->mBuffers[channel].mData = driver->input_buffers_[channel];
    }

    const OSStatus render_status = AudioUnitRender(driver->audio_unit_, ioActionFlags, inTimeStamp,
                                                   /*inputBus=*/1, frames_to_process, input_abl);
    if (render_status != noErr) {
      driver->recordInputRenderFailure();
    }
  }

  const float** input_ptrs = driver->input_buffers_.empty()
                                 ? nullptr
                                 : const_cast<const float**>(driver->input_buffers_.data());
  float** output_ptrs = driver->output_buffers_.data();

  const bool sample_time_valid =
      inTimeStamp != nullptr && (inTimeStamp->mFlags & kAudioTimeStampSampleTimeValid) != 0;
  const int64_t reported_sample_time =
      sample_time_valid ? static_cast<int64_t>(std::llround(inTimeStamp->mSampleTime)) : 0;
  const bool device_position_available = sample_time_valid && reported_sample_time >= 0;
  const int64_t stream_time =
      device_position_available ? reported_sample_time : driver->expected_stream_sample_;
  const bool discontinuity = !driver->stream_timeline_initialized_ || !device_position_available ||
                             stream_time != driver->expected_stream_sample_;
  const bool host_time_valid =
      inTimeStamp != nullptr && (inTimeStamp->mFlags & kAudioTimeStampHostTimeValid) != 0;

  AudioProcessBlock block;
  block.input_buffers = input_ptrs;
  block.output_buffers = output_ptrs;
  block.num_input_channels = static_cast<uint16_t>(num_input_channels);
  block.num_output_channels = static_cast<uint16_t>(num_output_channels);
  block.num_frames = frames_to_process;
  block.device_sample_position =
      device_position_available ? static_cast<uint64_t>(reported_sample_time) : 0;
  block.host_time_nanoseconds =
      host_time_valid ? AudioConvertHostTimeToNanos(inTimeStamp->mHostTime) : 0;
  block.discontinuity = discontinuity;

  auto monitor_lease = driver->performance_monitor_target_.tryAcquire();
  const UInt64 callback_start = monitor_lease ? AudioGetCurrentHostTime() : UInt64{0};
  callback->processAudio(block);
  driver->expected_stream_sample_ = stream_time + frames_to_process;
  driver->stream_timeline_initialized_ = true;
  if (auto* monitor = monitor_lease.get()) {
    const UInt64 callback_end = AudioGetCurrentHostTime();
    const uint64_t duration_ns = AudioConvertHostTimeToNanos(callback_end - callback_start);
    const uint64_t callback_duration_us = static_cast<uint64_t>(duration_ns / 1000u);
    const uint64_t buffer_duration_us =
        (static_cast<uint64_t>(frames_to_process) * 1'000'000ull) / driver->config_.sample_rate;
    const uint32_t active_clips = callback->activeClipCount();
    monitor->recordAudioCallback(callback_duration_us, buffer_duration_us, active_clips,
                                 driver->config_.sample_rate, frames_to_process);
  }

  for (uint32_t channel = 0; channel < num_output_channels && channel < ioData->mNumberBuffers;
       ++channel) {
    std::memcpy(ioData->mBuffers[channel].mData, driver->output_buffers_[channel],
                frames_to_process * sizeof(float));
  }

  return noErr;
}

uint32_t CoreAudioDriver::getChannelCount(AudioDeviceID device_id,
                                          AudioObjectPropertyScope scope) const {
  if (device_id == 0) {
    return 0;
  }

  AudioObjectPropertyAddress address = {kAudioDevicePropertyStreamConfiguration, scope,
                                        kAudioObjectPropertyElementMain};
  UInt32 data_size = 0;
  if (AudioObjectGetPropertyDataSize(device_id, &address, 0, nullptr, &data_size) != noErr ||
      data_size < sizeof(AudioBufferList)) {
    return 0;
  }

  std::vector<uint8_t> storage(data_size);
  auto* buffers = reinterpret_cast<AudioBufferList*>(storage.data());
  if (AudioObjectGetPropertyData(device_id, &address, 0, nullptr, &data_size, buffers) != noErr) {
    return 0;
  }

  uint32_t channels = 0;
  for (UInt32 index = 0; index < buffers->mNumberBuffers; ++index) {
    channels += buffers->mBuffers[index].mNumberChannels;
  }
  return channels;
}

AudioDeviceID CoreAudioDriver::createAggregateDevice(AudioDeviceID input_device_id,
                                                     AudioDeviceID output_device_id) {
  CFStringRef input_uid = copyDeviceUID(input_device_id);
  CFStringRef output_uid = copyDeviceUID(output_device_id);
  if (!input_uid || !output_uid) {
    if (input_uid)
      CFRelease(input_uid);
    if (output_uid)
      CFRelease(output_uid);
    return 0;
  }

  const UInt32 input_clock_domain = getClockDomain(input_device_id);
  const UInt32 output_clock_domain = getClockDomain(output_device_id);
  const bool same_clock_domain =
      input_clock_domain != 0 && input_clock_domain == output_clock_domain;
  const int drift_compensation = same_clock_domain ? 0 : 1;

  CFMutableDictionaryRef input_sub_device = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFDictionarySetValue(input_sub_device, CFSTR(kAudioSubDeviceUIDKey), input_uid);
  CFNumberRef drift_value =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &drift_compensation);
  CFDictionarySetValue(input_sub_device, CFSTR(kAudioSubDeviceDriftCompensationKey), drift_value);
  if (!same_clock_domain) {
    const int max_quality = kAudioSubDeviceDriftCompensationMaxQuality;
    CFNumberRef quality_value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &max_quality);
    CFDictionarySetValue(input_sub_device, CFSTR(kAudioSubDeviceDriftCompensationQualityKey),
                         quality_value);
    CFRelease(quality_value);
  }
  CFRelease(drift_value);

  CFMutableDictionaryRef output_sub_device = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFDictionarySetValue(output_sub_device, CFSTR(kAudioSubDeviceUIDKey), output_uid);

  CFMutableArrayRef sub_device_list =
      CFArrayCreateMutable(kCFAllocatorDefault, 2, &kCFTypeArrayCallBacks);
  CFArrayAppendValue(sub_device_list, output_sub_device);
  CFArrayAppendValue(sub_device_list, input_sub_device);

  std::ostringstream uid_stream;
  uid_stream << "com.orpheus.sdk.aggregate." << static_cast<const void*>(this);
  CFStringRef aggregate_uid = CFStringCreateWithCString(
      kCFAllocatorDefault, uid_stream.str().c_str(), kCFStringEncodingUTF8);

  CFMutableDictionaryRef aggregate_dict = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFDictionarySetValue(aggregate_dict, CFSTR(kAudioAggregateDeviceNameKey),
                       CFSTR("Orpheus SDK I/O Bridge"));
  CFDictionarySetValue(aggregate_dict, CFSTR(kAudioAggregateDeviceUIDKey), aggregate_uid);
  CFDictionarySetValue(aggregate_dict, CFSTR(kAudioAggregateDeviceSubDeviceListKey),
                       sub_device_list);
  CFDictionarySetValue(aggregate_dict, CFSTR(kAudioAggregateDeviceMasterSubDeviceKey), output_uid);
  CFDictionarySetValue(aggregate_dict, CFSTR(kAudioAggregateDeviceIsPrivateKey), kCFBooleanTrue);
  CFDictionarySetValue(aggregate_dict, CFSTR(kAudioAggregateDeviceIsStackedKey), kCFBooleanFalse);

  AudioDeviceID aggregate_id = kAudioObjectUnknown;
  const OSStatus status = AudioHardwareCreateAggregateDevice(aggregate_dict, &aggregate_id);

  CFRelease(input_sub_device);
  CFRelease(output_sub_device);
  CFRelease(sub_device_list);
  CFRelease(aggregate_dict);
  CFRelease(aggregate_uid);
  CFRelease(input_uid);
  CFRelease(output_uid);

  return status == noErr ? aggregate_id : 0;
}

std::vector<AudioStreamID> CoreAudioDriver::enumerateStreams(AudioDeviceID device_id,
                                                             AudioObjectPropertyScope scope) const {
  std::vector<AudioStreamID> streams;
  if (device_id == 0) {
    return streams;
  }
  AudioObjectPropertyAddress address = {kAudioDevicePropertyStreams, scope,
                                        kAudioObjectPropertyElementMain};
  UInt32 data_size = 0;
  if (AudioObjectGetPropertyDataSize(device_id, &address, 0, nullptr, &data_size) != noErr ||
      data_size == 0) {
    return streams;
  }
  streams.resize(data_size / sizeof(AudioStreamID));
  if (AudioObjectGetPropertyData(device_id, &address, 0, nullptr, &data_size, streams.data()) !=
      noErr) {
    streams.clear();
  }
  return streams;
}

std::optional<AudioStreamBasicDescription>
CoreAudioDriver::getStreamFormat(AudioStreamID stream_id,
                                 AudioObjectPropertySelector selector) const {
  if (stream_id == 0) {
    return std::nullopt;
  }
  AudioObjectPropertyAddress address = {selector, kAudioObjectPropertyScopeGlobal,
                                        kAudioObjectPropertyElementMain};
  AudioStreamBasicDescription format{};
  UInt32 data_size = sizeof(format);
  if (AudioObjectGetPropertyData(stream_id, &address, 0, nullptr, &data_size, &format) != noErr ||
      data_size != sizeof(format)) {
    return std::nullopt;
  }
  return format;
}

bool CoreAudioDriver::createRouteMonitor() {
  std::vector<CoreAudioRouteDevice> devices;
  devices.reserve(3);
  devices.push_back({device_id_, false, false});
  devices.push_back({output_device_id_, false, true});
  if (config_.num_inputs > 0) {
    devices.push_back({input_device_id_, true, false});
  }

  std::vector<CoreAudioRouteStream> streams;
  const auto append_streams = [&](AudioDeviceID device_id, AudioObjectPropertyScope scope) {
    for (const AudioStreamID stream_id : enumerateStreams(device_id, scope)) {
      const auto virtual_format = getStreamFormat(stream_id, kAudioStreamPropertyVirtualFormat);
      const auto physical_format = getStreamFormat(stream_id, kAudioStreamPropertyPhysicalFormat);
      if (!virtual_format.has_value() || !physical_format.has_value()) {
        return false;
      }
      const auto existing = std::find_if(streams.begin(), streams.end(),
                                         [stream_id](const CoreAudioRouteStream& stream) {
                                           return stream.stream_id == stream_id;
                                         });
      if (existing == streams.end()) {
        streams.push_back({stream_id, *virtual_format, *physical_format});
      }
    }
    return true;
  };

  if (!append_streams(output_device_id_, kAudioObjectPropertyScopeOutput) ||
      (config_.num_inputs > 0 &&
       !append_streams(input_device_id_, kAudioObjectPropertyScopeInput)) ||
      streams.empty()) {
    return false;
  }

  try {
    route_monitor_ = std::make_unique<CoreAudioRouteMonitor>(
        route_property_api_, config_.sample_rate, config_.buffer_size, std::move(devices),
        std::move(streams));
  } catch (...) {
    route_monitor_.reset();
    return false;
  }
  return true;
}

bool CoreAudioDriver::startRouteMonitorLocked() {
  std::lock_guard<std::mutex> monitor_lock(route_monitor_mutex_);
  if (!route_monitor_ || !route_monitor_->start()) {
    runtime_outcome_.store(AudioDriverRuntimeOutcome::SampleRateListenerFailure,
                           std::memory_order_release);
    route_outcome_.store(AudioRouteRuntimeOutcome::BackendFailure, std::memory_order_release);
    return false;
  }

  route_monitor_->requestCheck();
  const CoreAudioRoutePollResult result = route_monitor_->poll();
  publishRoutePollResult(result);
  if (result == CoreAudioRoutePollResult::NoChange ||
      result == CoreAudioRoutePollResult::RateRestored) {
    refreshActiveRouteLocked();
    return true;
  }

  route_monitor_->stop();
  return false;
}

void CoreAudioDriver::stopRouteMonitor() {
  route_monitor_active_.store(false, std::memory_order_release);
  {
    std::lock_guard<std::mutex> monitor_lock(route_monitor_mutex_);
    if (route_monitor_) {
      route_monitor_->stop();
    }
  }
  if (route_monitor_thread_.joinable()) {
    route_monitor_thread_.join();
  }
}

void CoreAudioDriver::routeMonitorLoop() {
  while (route_monitor_active_.load(std::memory_order_acquire)) {
    route_monitor_->waitForChange();
    if (!route_monitor_active_.load(std::memory_order_acquire)) {
      break;
    }

    CoreAudioRoutePollResult result = CoreAudioRoutePollResult::BackendFailure;
    {
      std::lock_guard<std::mutex> monitor_lock(route_monitor_mutex_);
      if (!route_monitor_active_.load(std::memory_order_acquire)) {
        break;
      }
      result = route_monitor_->poll();
      if (result != CoreAudioRoutePollResult::NoChange &&
          result != CoreAudioRoutePollResult::RateRestored) {
        route_monitor_->stop();
      }
    }

    publishRoutePollResult(result);
    if (result == CoreAudioRoutePollResult::NoChange ||
        result == CoreAudioRoutePollResult::RateRestored) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (is_running_.load(std::memory_order_acquire)) {
        refreshActiveRouteLocked();
      }
      continue;
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      stopRenderingLocked();
    }
    route_monitor_active_.store(false, std::memory_order_release);
    break;
  }
}

void CoreAudioDriver::publishRoutePollResult(CoreAudioRoutePollResult result) {
  switch (result) {
  case CoreAudioRoutePollResult::NoChange:
    runtime_outcome_.store(AudioDriverRuntimeOutcome::Healthy, std::memory_order_release);
    route_outcome_.store(AudioRouteRuntimeOutcome::Healthy, std::memory_order_release);
    break;
  case CoreAudioRoutePollResult::RateRestored:
    runtime_outcome_.store(AudioDriverRuntimeOutcome::SampleRateRestored,
                           std::memory_order_release);
    route_outcome_.store(AudioRouteRuntimeOutcome::Healthy, std::memory_order_release);
    break;
  case CoreAudioRoutePollResult::InputUnavailable:
    unavailable_route_state_.store(AudioRouteState::InputUnavailable, std::memory_order_release);
    runtime_outcome_.store(AudioDriverRuntimeOutcome::BackendFailure, std::memory_order_release);
    route_outcome_.store(AudioRouteRuntimeOutcome::RouteUnavailable, std::memory_order_release);
    break;
  case CoreAudioRoutePollResult::OutputUnavailable:
    unavailable_route_state_.store(AudioRouteState::OutputUnavailable, std::memory_order_release);
    runtime_outcome_.store(AudioDriverRuntimeOutcome::BackendFailure, std::memory_order_release);
    route_outcome_.store(AudioRouteRuntimeOutcome::RouteUnavailable, std::memory_order_release);
    break;
  case CoreAudioRoutePollResult::RouteUnavailable:
    unavailable_route_state_.store(AudioRouteState::Failed, std::memory_order_release);
    runtime_outcome_.store(AudioDriverRuntimeOutcome::BackendFailure, std::memory_order_release);
    route_outcome_.store(AudioRouteRuntimeOutcome::RouteUnavailable, std::memory_order_release);
    break;
  case CoreAudioRoutePollResult::FormatChanged:
    runtime_outcome_.store(AudioDriverRuntimeOutcome::BackendFailure, std::memory_order_release);
    route_outcome_.store(AudioRouteRuntimeOutcome::FormatChanged, std::memory_order_release);
    break;
  case CoreAudioRoutePollResult::ReinitializationRequired:
    runtime_outcome_.store(AudioDriverRuntimeOutcome::SampleRateReinitializationRequired,
                           std::memory_order_release);
    route_outcome_.store(AudioRouteRuntimeOutcome::ReinitializationRequired,
                         std::memory_order_release);
    break;
  case CoreAudioRoutePollResult::BackendFailure:
    runtime_outcome_.store(AudioDriverRuntimeOutcome::BackendFailure, std::memory_order_release);
    route_outcome_.store(AudioRouteRuntimeOutcome::BackendFailure, std::memory_order_release);
    break;
  }
}

void CoreAudioDriver::refreshActiveRouteLocked() {
  active_route_.input_device_id = getDeviceUID(input_device_id_).value_or("");
  active_route_.output_device_id = getDeviceUID(output_device_id_).value_or("");
  active_route_.input_channels = input_channel_map_;
  active_route_.output_channels = output_channel_map_;
  active_route_.available_input_channels = static_cast<uint16_t>(
      std::min<uint32_t>(available_input_channels_, std::numeric_limits<uint16_t>::max()));
  active_route_.available_output_channels = static_cast<uint16_t>(
      std::min<uint32_t>(available_output_channels_, std::numeric_limits<uint16_t>::max()));
  active_route_.requested_sample_rate = config_.sample_rate;
  active_route_.actual_sample_rate = 0;
  active_route_.actual_buffer_frames = 0;

  const auto actual_rate = getOptionalFloat64Property(
      device_id_, kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal);
  if (actual_rate.has_value() && *actual_rate > 0.0) {
    active_route_.actual_sample_rate = static_cast<uint32_t>(*actual_rate);
  }
  active_route_.actual_buffer_frames =
      getOptionalUInt32Property(device_id_, kAudioDevicePropertyBufferFrameSize,
                                kAudioObjectPropertyScopeGlobal)
          .value_or(0);
  active_route_.latency = queryLatencyBreakdown();
  active_route_.input_alive = config_.num_inputs > 0 &&
                              getUInt32Property(input_device_id_, kAudioDevicePropertyDeviceIsAlive,
                                                kAudioObjectPropertyScopeGlobal) != 0;
  active_route_.output_alive =
      getUInt32Property(output_device_id_, kAudioDevicePropertyDeviceIsAlive,
                        kAudioObjectPropertyScopeGlobal) != 0;
}

AudioRouteLatency CoreAudioDriver::queryLatencyBreakdown() const {
  AudioRouteLatency latency;
  uint32_t capture_frames = 0;
  uint32_t playback_frames = 0;
  uint32_t processing_frames = 0;

  const auto input_device_latency = getOptionalUInt32Property(
      input_device_id_, kAudioDevicePropertyLatency, kAudioObjectPropertyScopeInput);
  const auto input_safety_offset = getOptionalUInt32Property(
      input_device_id_, kAudioDevicePropertySafetyOffset, kAudioObjectPropertyScopeInput);
  const auto input_stream_latency =
      getOptionalStreamLatency(input_device_id_, kAudioObjectPropertyScopeInput);
  const auto output_device_latency = getOptionalUInt32Property(
      output_device_id_, kAudioDevicePropertyLatency, kAudioObjectPropertyScopeOutput);
  const auto output_safety_offset = getOptionalUInt32Property(
      output_device_id_, kAudioDevicePropertySafetyOffset, kAudioObjectPropertyScopeOutput);
  const auto output_stream_latency =
      getOptionalStreamLatency(output_device_id_, kAudioObjectPropertyScopeOutput);
  const auto callback_buffer_frames = getOptionalUInt32Property(
      device_id_, kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal);

  std::optional<uint32_t> audio_unit_latency;
  if (audio_unit_ != nullptr) {
    Float64 seconds = 0.0;
    UInt32 size = sizeof(seconds);
    if (AudioUnitGetProperty(audio_unit_, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0,
                             &seconds, &size) == noErr &&
        size == sizeof(seconds) && seconds >= 0.0) {
      const uint32_t rate = active_route_.actual_sample_rate != 0 ? active_route_.actual_sample_rate
                                                                  : config_.sample_rate;
      const uint64_t frames = static_cast<uint64_t>(std::llround(seconds * rate));
      audio_unit_latency =
          static_cast<uint32_t>(std::min<uint64_t>(frames, std::numeric_limits<uint32_t>::max()));
    }
  }

  bool input_complete = config_.num_inputs == 0;
  if (config_.num_inputs > 0) {
    input_complete = addLatencyTerm(input_device_latency, capture_frames) &&
                     addLatencyTerm(input_safety_offset, capture_frames) &&
                     addLatencyTerm(input_stream_latency, capture_frames);
  }
  const bool output_complete = addLatencyTerm(output_device_latency, playback_frames) &&
                               addLatencyTerm(output_safety_offset, playback_frames) &&
                               addLatencyTerm(output_stream_latency, playback_frames);
  const bool processing_complete = addLatencyTerm(callback_buffer_frames, processing_frames) &&
                                   addLatencyTerm(audio_unit_latency, processing_frames);

  latency.capture_frames = capture_frames;
  latency.playback_frames = playback_frames;
  latency.processing_frames = processing_frames;
  latency.complete = input_complete && output_complete && processing_complete;
  return latency;
}

AudioLatencyBreakdown CoreAudioDriver::queryDetailedLatencyBreakdown() const {
  AudioLatencyBreakdown latency;
  if (config_.num_inputs > 0) {
    latency.input_device_frames = getOptionalUInt32Property(
        input_device_id_, kAudioDevicePropertyLatency, kAudioObjectPropertyScopeInput);
    latency.input_safety_offset_frames = getOptionalUInt32Property(
        input_device_id_, kAudioDevicePropertySafetyOffset, kAudioObjectPropertyScopeInput);
    latency.input_stream_frames =
        getOptionalStreamLatency(input_device_id_, kAudioObjectPropertyScopeInput);
  }
  latency.output_device_frames = getOptionalUInt32Property(
      output_device_id_, kAudioDevicePropertyLatency, kAudioObjectPropertyScopeOutput);
  latency.output_safety_offset_frames = getOptionalUInt32Property(
      output_device_id_, kAudioDevicePropertySafetyOffset, kAudioObjectPropertyScopeOutput);
  latency.output_stream_frames =
      getOptionalStreamLatency(output_device_id_, kAudioObjectPropertyScopeOutput);
  latency.callback_buffer_frames = getOptionalUInt32Property(
      device_id_, kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal);

  if (audio_unit_ != nullptr) {
    Float64 seconds = 0.0;
    UInt32 size = sizeof(seconds);
    if (AudioUnitGetProperty(audio_unit_, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0,
                             &seconds, &size) == noErr &&
        size == sizeof(seconds) && seconds >= 0.0) {
      const uint32_t rate = active_route_.actual_sample_rate != 0 ? active_route_.actual_sample_rate
                                                                  : config_.sample_rate;
      const uint64_t frames = static_cast<uint64_t>(std::llround(seconds * rate));
      latency.aggregate_or_audio_unit_frames =
          static_cast<uint32_t>(std::min<uint64_t>(frames, std::numeric_limits<uint32_t>::max()));
    }
  }

  const bool input_complete =
      config_.num_inputs == 0 ||
      (latency.input_device_frames.has_value() && latency.input_safety_offset_frames.has_value() &&
       latency.input_stream_frames.has_value());
  const bool output_complete = latency.output_device_frames.has_value() &&
                               latency.output_safety_offset_frames.has_value() &&
                               latency.output_stream_frames.has_value();
  const bool processing_complete = latency.callback_buffer_frames.has_value() &&
                                   latency.aggregate_or_audio_unit_frames.has_value();
  latency.complete = input_complete && output_complete && processing_complete;
  return latency;
}

uint32_t CoreAudioDriver::queryDeviceLatency() const {
  const AudioRouteLatency latency = queryLatencyBreakdown();
  if (!latency.complete) {
    return 0;
  }
  const uint64_t total = static_cast<uint64_t>(latency.capture_frames) + latency.playback_frames +
                         latency.processing_frames;
  return static_cast<uint32_t>(std::min<uint64_t>(total, std::numeric_limits<uint32_t>::max()));
}

std::optional<std::string> CoreAudioDriver::getDeviceUID(AudioDeviceID device_id) const {
  if (device_id == 0) {
    return std::nullopt;
  }
  CFStringRef uid = copyDeviceUID(device_id);
  if (uid == nullptr) {
    return std::nullopt;
  }
  char buffer[1024] = {};
  const Boolean converted = CFStringGetCString(uid, buffer, sizeof(buffer), kCFStringEncodingUTF8);
  CFRelease(uid);
  if (!converted) {
    return std::nullopt;
  }
  return std::string(buffer);
}

SessionGraphError CoreAudioDriver::setupAudioUnit(AudioDeviceID device_id) {
  AudioComponentDescription description = {};
  description.componentType = kAudioUnitType_Output;
  description.componentSubType = kAudioUnitSubType_HALOutput;
  description.componentManufacturer = kAudioUnitManufacturer_Apple;

  AudioComponent component = AudioComponentFindNext(nullptr, &description);
  if (!component) {
    return SessionGraphError::InternalError;
  }

  OSStatus status = AudioComponentInstanceNew(component, &audio_unit_);
  if (status != noErr || !audio_unit_) {
    return SessionGraphError::InternalError;
  }

  UInt32 enable_input = config_.num_inputs > 0 ? 1 : 0;
  status = AudioUnitSetProperty(audio_unit_, kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Input, 1, &enable_input, sizeof(enable_input));
  if (status != noErr) {
    return SessionGraphError::InternalError;
  }

  UInt32 enable_output = 1;
  status = AudioUnitSetProperty(audio_unit_, kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Output, 0, &enable_output, sizeof(enable_output));
  if (status != noErr) {
    return SessionGraphError::InternalError;
  }

  status = AudioUnitSetProperty(audio_unit_, kAudioOutputUnitProperty_CurrentDevice,
                                kAudioUnitScope_Global, 0, &device_id, sizeof(device_id));
  if (status != noErr) {
    return SessionGraphError::InternalError;
  }

  Float64 requested_sample_rate = static_cast<Float64>(config_.sample_rate);
  AudioObjectPropertyAddress sample_rate_address = {kAudioDevicePropertyNominalSampleRate,
                                                    kAudioObjectPropertyScopeGlobal,
                                                    kAudioObjectPropertyElementMain};
  status = AudioObjectSetPropertyData(device_id, &sample_rate_address, 0, nullptr,
                                      sizeof(requested_sample_rate), &requested_sample_rate);
  if (status != noErr) {
    const auto observed_rate = getOptionalFloat64Property(
        device_id, kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal);
    if (!observed_rate.has_value() || *observed_rate != requested_sample_rate) {
      return SessionGraphError::InternalError;
    }
  }

  AudioStreamBasicDescription output_format = {};
  output_format.mSampleRate = config_.sample_rate;
  output_format.mFormatID = kAudioFormatLinearPCM;
  output_format.mFormatFlags =
      kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
  output_format.mBytesPerPacket = sizeof(float);
  output_format.mFramesPerPacket = 1;
  output_format.mBytesPerFrame = sizeof(float);
  output_format.mChannelsPerFrame = config_.num_outputs;
  output_format.mBitsPerChannel = 32;

  status = AudioUnitSetProperty(audio_unit_, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                                0, &output_format, sizeof(output_format));
  if (status != noErr) {
    return SessionGraphError::InternalError;
  }

  std::vector<SInt32> output_channel_map;
  output_channel_map.reserve(output_channel_map_.size());
  for (const uint16_t channel : output_channel_map_) {
    output_channel_map.push_back(static_cast<SInt32>(channel));
  }
  status = AudioUnitSetProperty(audio_unit_, kAudioOutputUnitProperty_ChannelMap,
                                kAudioUnitScope_Input, 0, output_channel_map.data(),
                                static_cast<UInt32>(output_channel_map.size() * sizeof(SInt32)));
  if (status != noErr) {
    return SessionGraphError::InternalError;
  }

  if (config_.num_inputs > 0) {
    AudioStreamBasicDescription input_format = output_format;
    input_format.mChannelsPerFrame = config_.num_inputs;
    status = AudioUnitSetProperty(audio_unit_, kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Output, 1, &input_format, sizeof(input_format));
    if (status != noErr) {
      return SessionGraphError::InternalError;
    }

    std::vector<SInt32> input_channel_map;
    input_channel_map.reserve(input_channel_map_.size());
    for (const uint16_t channel : input_channel_map_) {
      input_channel_map.push_back(static_cast<SInt32>(input_channel_offset_ + channel));
    }
    status = AudioUnitSetProperty(audio_unit_, kAudioOutputUnitProperty_ChannelMap,
                                  kAudioUnitScope_Output, 1, input_channel_map.data(),
                                  static_cast<UInt32>(input_channel_map.size() * sizeof(SInt32)));
    if (status != noErr) {
      return SessionGraphError::InternalError;
    }
  }

  UInt32 buffer_frames = config_.buffer_size;
  status = AudioUnitSetProperty(audio_unit_, kAudioDevicePropertyBufferFrameSize,
                                kAudioUnitScope_Global, 0, &buffer_frames, sizeof(buffer_frames));
  if (status != noErr) {
    const auto observed_buffer = getOptionalUInt32Property(
        device_id, kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal);
    if (!observed_buffer.has_value() || *observed_buffer != config_.buffer_size) {
      return SessionGraphError::InternalError;
    }
  }

  AURenderCallbackStruct callback = {};
  callback.inputProc = &CoreAudioDriver::renderCallback;
  callback.inputProcRefCon = this;
  status = AudioUnitSetProperty(audio_unit_, kAudioUnitProperty_SetRenderCallback,
                                kAudioUnitScope_Input, 0, &callback, sizeof(callback));
  if (status != noErr) {
    return SessionGraphError::InternalError;
  }

  status = AudioUnitInitialize(audio_unit_);
  return status == noErr ? SessionGraphError::OK : SessionGraphError::InternalError;
}

void CoreAudioDriver::cleanupAudioUnit() {
  if (audio_unit_) {
    AudioUnitUninitialize(audio_unit_);
    AudioComponentInstanceDispose(audio_unit_);
    audio_unit_ = nullptr;
  }

  if (aggregate_device_id_ != 0) {
    AudioHardwareDestroyAggregateDevice(aggregate_device_id_);
    aggregate_device_id_ = 0;
  }

  device_id_ = 0;
  input_device_id_ = 0;
  output_device_id_ = 0;
  input_channel_offset_ = 0;
  available_input_channels_ = 0;
  available_output_channels_ = 0;
  input_channel_map_.clear();
  output_channel_map_.clear();
  route_monitor_.reset();
  input_buffers_.clear();
  output_buffers_.clear();
  input_storage_.clear();
  output_storage_.clear();
  input_abl_storage_.clear();
}

void CoreAudioDriver::stopRenderingLocked() {
  is_running_.store(false, std::memory_order_release);
  if (audio_unit_) {
    AudioOutputUnitStop(audio_unit_);
  }
  callback_target_.replaceAndDrain(nullptr);
}

ORPHEUS_API std::unique_ptr<IAudioDriver> createCoreAudioDriver() {
  return std::make_unique<CoreAudioDriver>();
}

} // namespace orpheus
