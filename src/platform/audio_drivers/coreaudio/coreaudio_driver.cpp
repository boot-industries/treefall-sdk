// SPDX-License-Identifier: MIT
#include "coreaudio_driver.h"
#include "coreaudio_sample_rate_transaction.h"

#include "../../../core/common/realtime_counter.h"
#include <orpheus/performance_monitor.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>
#include <utility>

namespace orpheus {
namespace {

bool addLatencyTerm(const std::optional<UInt32>& term, uint32_t& total) {
  if (!term.has_value()) {
    return false;
  }
  const uint64_t sum = static_cast<uint64_t>(total) + *term;
  total = static_cast<uint32_t>(std::min<uint64_t>(sum, std::numeric_limits<uint32_t>::max()));
  return true;
}

AudioRouteRuntimeOutcome outcomeForCompatibility(AudioRouteCompatibilityStatus status) {
  switch (status) {
  case AudioRouteCompatibilityStatus::InputUnavailable:
    return AudioRouteRuntimeOutcome::InputRouteUnavailable;
  case AudioRouteCompatibilityStatus::OutputUnavailable:
    return AudioRouteRuntimeOutcome::OutputRouteUnavailable;
  case AudioRouteCompatibilityStatus::SampleRateUnsupported:
    return AudioRouteRuntimeOutcome::SampleRateUnsupported;
  case AudioRouteCompatibilityStatus::InvalidChannelMap:
    return AudioRouteRuntimeOutcome::ChannelMapInvalid;
  case AudioRouteCompatibilityStatus::PermissionDenied:
    return AudioRouteRuntimeOutcome::PermissionDenied;
  case AudioRouteCompatibilityStatus::BackendFailure:
    return AudioRouteRuntimeOutcome::BackendFailure;
  case AudioRouteCompatibilityStatus::Compatible:
  case AudioRouteCompatibilityStatus::RequiresSampleRateChange:
    return AudioRouteRuntimeOutcome::BackendFailure;
  }
  return AudioRouteRuntimeOutcome::BackendFailure;
}

uint64_t requiredBytes(uint32_t frames) {
  return static_cast<uint64_t>(frames) * sizeof(float);
}

bool checkedPlanarSize(uint32_t channels, uint32_t frames, size_t& size) {
  const uint64_t elements = static_cast<uint64_t>(channels) * frames;
  if (elements > std::numeric_limits<size_t>::max() / sizeof(float)) {
    return false;
  }
  size = static_cast<size_t>(elements);
  return true;
}

bool validSampleTimestamp(const AudioTimeStamp* timestamp, UInt32 frames, uint64_t& base) {
  if (timestamp == nullptr || (timestamp->mFlags & kAudioTimeStampSampleTimeValid) == 0 ||
      !std::isfinite(timestamp->mSampleTime) || timestamp->mSampleTime < 0.0) {
    return false;
  }
  const long double rounded = std::round(static_cast<long double>(timestamp->mSampleTime));
  const long double maximum = static_cast<long double>(std::numeric_limits<uint64_t>::max());
  if (rounded < 0.0L || rounded > maximum || rounded > maximum - static_cast<long double>(frames)) {
    return false;
  }
  base = static_cast<uint64_t>(rounded);
  return true;
}

uint64_t hostTimeForOffset(uint64_t base, uint32_t offset, uint32_t sample_rate) {
  if (sample_rate == 0) {
    return 0;
  }
  const uint64_t quotient = offset / sample_rate;
  const uint64_t remainder = offset % sample_rate;
  const uint64_t offset_ns =
      quotient * 1'000'000'000ull + (remainder * 1'000'000'000ull) / sample_rate;
  if (base > std::numeric_limits<uint64_t>::max() - offset_ns) {
    return 0;
  }
  return base + offset_ns;
}

} // namespace

CoreAudioDriver::CoreAudioDriver()
    : route_resolver_(detail::makeCoreAudioRouteQuery()), property_api_(&production_property_api_) {
}

CoreAudioDriver::CoreAudioDriver(std::shared_ptr<const detail::ICoreAudioRouteQuery> query)
    : route_resolver_(query ? std::move(query) : detail::makeCoreAudioRouteQuery()),
      property_api_(&production_property_api_) {}

CoreAudioDriver::CoreAudioDriver(std::shared_ptr<const detail::ICoreAudioRouteQuery> query,
                                 std::shared_ptr<ICoreAudioPropertyApi> property_api,
                                 detail::CoreAudioDriverDirectionAudit* direction_audit)
    : route_resolver_(query ? std::move(query) : detail::makeCoreAudioRouteQuery()),
      injected_property_api_(std::move(property_api)), direction_audit_(direction_audit) {
  property_api_ = injected_property_api_ ? injected_property_api_.get() : &production_property_api_;
}

CoreAudioDriver::CoreAudioDriver(std::shared_ptr<const detail::ICoreAudioRouteQuery> query,
                                 ICoreAudioPropertyApi& property_api,
                                 detail::CoreAudioDriverDirectionAudit* direction_audit)
    : route_resolver_(query ? std::move(query) : detail::makeCoreAudioRouteQuery()),
      property_api_(&property_api), direction_audit_(direction_audit) {}

CoreAudioDriver::~CoreAudioDriver() {
  stop();
  stopRouteMonitor();
  std::lock_guard<std::mutex> lock(mutex_);
  restoreAutomaticHogModeLocked();
  cleanupAudioUnit();
}

SessionGraphError CoreAudioDriver::initialize(const AudioDriverConfig& config) {
  if (config.sample_rate == 0 || config.buffer_size == 0 || config.num_outputs == 0) {
    return SessionGraphError::InvalidParameter;
  }
  if (is_running_.load(std::memory_order_acquire)) {
    return SessionGraphError::NotReady;
  }

  // Resolution is intentionally the first backend operation. It is the only
  // source of advertised-rate support facts and performs no activation writes.
  const detail::ResolvedCoreAudioRoute resolved_route = route_resolver_.resolve(config);

  stopRouteMonitor();
  std::lock_guard<std::mutex> lock(mutex_);
  if (is_running_.load(std::memory_order_acquire)) {
    return SessionGraphError::NotReady;
  }

  if (!restoreAutomaticHogModeLocked()) {
    route_outcome_.store(AudioRouteRuntimeOutcome::BackendFailure, std::memory_order_release);
    cleanupAudioUnit();
    return SessionGraphError::InternalError;
  }
  cleanupAudioUnit();
  config_ = config;
  active_route_ = {};
  expected_stream_sample_ = 0;
  stream_timeline_initialized_ = false;
  unavailable_route_state_.store(AudioRouteState::Failed, std::memory_order_release);

  if (!resolved_route.resolved) {
    const AudioRouteRuntimeOutcome outcome =
        outcomeForCompatibility(resolved_route.compatibility.status);
    if (resolved_route.compatibility.status == AudioRouteCompatibilityStatus::InputUnavailable) {
      unavailable_route_state_.store(AudioRouteState::InputUnavailable, std::memory_order_release);
    } else if (resolved_route.compatibility.status ==
               AudioRouteCompatibilityStatus::OutputUnavailable) {
      unavailable_route_state_.store(AudioRouteState::OutputUnavailable, std::memory_order_release);
    } else if (resolved_route.compatibility.status ==
               AudioRouteCompatibilityStatus::PermissionDenied) {
      unavailable_route_state_.store(AudioRouteState::PermissionDenied, std::memory_order_release);
    }
    route_outcome_.store(outcome, std::memory_order_release);
    return SessionGraphError::InvalidParameter;
  }

  if (config.sample_rate_policy == AudioSampleRatePolicy::PreserveDeviceRate &&
      (resolved_route.compatibility.input_rate_change_required ||
       resolved_route.compatibility.output_rate_change_required)) {
    route_outcome_.store(AudioRouteRuntimeOutcome::SampleRateChanged, std::memory_order_release);
    return SessionGraphError::InvalidParameter;
  }

  input_device_id_ = resolved_route.input_device_id;
  output_device_id_ = resolved_route.output_device_id;
  available_input_channels_ = resolved_route.input_channel_count;
  available_output_channels_ = resolved_route.output_channel_count;
  input_channel_map_ = resolved_route.input_channel_map;
  output_channel_map_ = resolved_route.output_channel_map;
  device_id_ = output_device_id_;

  if (config.sample_rate_policy == AudioSampleRatePolicy::RequestExactRate) {
    const AudioRouteRuntimeOutcome hog_result = disableAutomaticHogModeLocked();
    if (hog_result != AudioRouteRuntimeOutcome::Healthy) {
      route_outcome_.store(hog_result, std::memory_order_release);
      cleanupAudioUnit();
      return SessionGraphError::InternalError;
    }
  }

  const bool rate_transaction_active =
      config.sample_rate_policy == AudioSampleRatePolicy::RequestExactRate;
  std::optional<CoreAudioSampleRateTransaction> rate_transaction;
  if (rate_transaction_active) {
    rate_transaction.emplace(*property_api_, resolved_route, config.sample_rate);
    if (config.num_inputs > 0) {
      notifyInputOperation(
          detail::CoreAudioDriverDirectionAudit::InputDirectionOperation::RateTransaction);
    }
    const AudioRouteRuntimeOutcome transaction_result = rate_transaction->begin();
    if (transaction_result != AudioRouteRuntimeOutcome::Healthy) {
      route_outcome_.store(transaction_result, std::memory_order_release);
      if (!restoreAutomaticHogModeLocked()) {
        route_outcome_.store(AudioRouteRuntimeOutcome::BackendFailure, std::memory_order_release);
      }
      cleanupAudioUnit();
      return SessionGraphError::InvalidParameter;
    }
  }

  if (resolved_route.requires_private_aggregate) {
    notifyInputOperation(detail::CoreAudioDriverDirectionAudit::InputDirectionOperation::Aggregate);
    input_channel_offset_ = getChannelCount(output_device_id_, kAudioObjectPropertyScopeInput);
    aggregate_device_id_ = createAggregateDevice(input_device_id_, output_device_id_);
    if (aggregate_device_id_ == 0) {
      route_outcome_.store(AudioRouteRuntimeOutcome::BackendFailure, std::memory_order_release);
      restoreAutomaticHogModeLocked();
      cleanupAudioUnit();
      return SessionGraphError::InvalidParameter;
    }
    device_id_ = aggregate_device_id_;
  }

  const SessionGraphError setup_result = setupAudioUnit(device_id_);
  if (setup_result != SessionGraphError::OK) {
    route_outcome_.store(AudioRouteRuntimeOutcome::BackendFailure, std::memory_order_release);
    restoreAutomaticHogModeLocked();
    cleanupAudioUnit();
    return setup_result;
  }

  refreshActiveRouteLocked();
  if (active_route_.output_device_id.empty() || active_route_.actual_sample_rate == 0 ||
      active_route_.actual_buffer_frames == 0 ||
      (config_.num_inputs > 0 && active_route_.input_device_id.empty())) {
    route_outcome_.store(AudioRouteRuntimeOutcome::BackendFailure, std::memory_order_release);
    restoreAutomaticHogModeLocked();
    cleanupAudioUnit();
    active_route_ = {};
    return SessionGraphError::InternalError;
  }

  uint32_t maximum_frames = active_route_.actual_buffer_frames;
  const auto consider_capacity = [&](AudioDeviceID endpoint,
                                     AudioObjectPropertyScope scope) -> bool {
    const auto capacity = queryMaximumBufferFrames(endpoint, scope);
    if (!capacity.has_value()) {
      return false;
    }
    maximum_frames = std::max(maximum_frames, *capacity);
    return true;
  };
  if (config_.num_inputs > 0) {
    notifyInputOperation(detail::CoreAudioDriverDirectionAudit::InputDirectionOperation::Capacity);
    if (!consider_capacity(input_device_id_, kAudioObjectPropertyScopeInput)) {
      route_outcome_.store(AudioRouteRuntimeOutcome::BackendFailure, std::memory_order_release);
      restoreAutomaticHogModeLocked();
      cleanupAudioUnit();
      return SessionGraphError::InternalError;
    }
  }
  if (!consider_capacity(output_device_id_, kAudioObjectPropertyScopeOutput) ||
      !consider_capacity(device_id_, kAudioObjectPropertyScopeGlobal)) {
    route_outcome_.store(AudioRouteRuntimeOutcome::BackendFailure, std::memory_order_release);
    restoreAutomaticHogModeLocked();
    cleanupAudioUnit();
    return SessionGraphError::InternalError;
  }
  if (!allocateRenderBuffers(maximum_frames)) {
    route_outcome_.store(AudioRouteRuntimeOutcome::BackendFailure, std::memory_order_release);
    restoreAutomaticHogModeLocked();
    cleanupAudioUnit();
    return SessionGraphError::InternalError;
  }

  if (!createRouteMonitor()) {
    route_outcome_.store(AudioRouteRuntimeOutcome::BackendFailure, std::memory_order_release);
    restoreAutomaticHogModeLocked();
    cleanupAudioUnit();
    return SessionGraphError::InternalError;
  }

  render_capacity_frames_ = maximum_frames;
  render_sample_rate_.store(active_route_.actual_sample_rate, std::memory_order_release);
  render_max_callback_frames_.store(maximum_frames, std::memory_order_release);
  render_chunk_frames_.store(config_.buffer_size, std::memory_order_release);
  input_render_failures_.store(0, std::memory_order_release);
  route_outcome_.store(AudioRouteRuntimeOutcome::Healthy, std::memory_order_release);
  if (rate_transaction.has_value()) {
    rate_transaction->commit();
  }
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

  if (render_sample_rate_.load(std::memory_order_acquire) == 0) {
    render_sample_rate_.store(active_route_.actual_sample_rate, std::memory_order_release);
    render_max_callback_frames_.store(render_capacity_frames_, std::memory_order_release);
    render_chunk_frames_.store(config_.buffer_size, std::memory_order_release);
  }

  if (!startRouteMonitorLocked()) {
    route_monitor_->stop();
    stopRenderingLocked();
    return SessionGraphError::InternalError;
  }

  callback_target_.replaceAndDrain(callback);
  expected_stream_sample_ = 0;
  stream_timeline_initialized_ = false;
  is_running_.store(true, std::memory_order_release);

  const OSStatus status = AudioOutputUnitStart(audio_unit_);
  if (status != noErr) {
    stopRenderingLocked();
    route_monitor_->stop();
    publishTerminalRouteOutcome(AudioRouteRuntimeOutcome::BackendFailure);
    return SessionGraphError::InternalError;
  }

  route_monitor_active_.store(true, std::memory_order_release);
  try {
    route_monitor_thread_ = std::thread(&CoreAudioDriver::routeMonitorLoop, this);
  } catch (...) {
    stopRenderingLocked();
    route_monitor_->stop();
    publishTerminalRouteOutcome(AudioRouteRuntimeOutcome::BackendFailure);
    return SessionGraphError::InternalError;
  }

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
  const AudioRouteRuntimeOutcome outcome = route_outcome_.load(std::memory_order_acquire);
  if (is_running_.load(std::memory_order_acquire) && outcome == AudioRouteRuntimeOutcome::Healthy) {
    state.state = AudioRouteState::Running;
  } else if (outcome == AudioRouteRuntimeOutcome::InputRouteUnavailable) {
    state.state = AudioRouteState::InputUnavailable;
    state.detail = "The selected CoreAudio input endpoint is unavailable.";
  } else if (outcome == AudioRouteRuntimeOutcome::OutputRouteUnavailable) {
    state.state = AudioRouteState::OutputUnavailable;
    state.detail = "The selected CoreAudio output endpoint is unavailable.";
  } else if (outcome == AudioRouteRuntimeOutcome::PermissionDenied) {
    state.state = AudioRouteState::PermissionDenied;
    state.detail = "CoreAudio denied access to the selected route.";
  } else if (outcome == AudioRouteRuntimeOutcome::SampleRateChanged ||
             outcome == AudioRouteRuntimeOutcome::BufferSizeChanged ||
             outcome == AudioRouteRuntimeOutcome::FormatChanged) {
    state.state = AudioRouteState::ReconfigurationRequired;
    state.detail = "The active CoreAudio format or route changed; reinitialize explicitly.";
  } else if (outcome != AudioRouteRuntimeOutcome::Healthy) {
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

void CoreAudioDriver::notifyInputOperation(
    detail::CoreAudioDriverDirectionAudit::InputDirectionOperation operation) const noexcept {
  if (direction_audit_ != nullptr) {
    direction_audit_->beforeInputOperation(operation);
  }
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

  if (ioData != nullptr) {
    for (UInt32 index = 0; index < ioData->mNumberBuffers; ++index) {
      if (ioData->mBuffers[index].mData != nullptr && ioData->mBuffers[index].mDataByteSize != 0) {
        std::memset(ioData->mBuffers[index].mData, 0, ioData->mBuffers[index].mDataByteSize);
      }
    }
  }

  const uint32_t max_frames = driver->render_max_callback_frames_.load(std::memory_order_acquire);
  const uint32_t num_input_channels = driver->config_.num_inputs;
  const uint32_t num_output_channels = driver->config_.num_outputs;
  if (max_frames != 0 && inNumberFrames <= max_frames) {
    for (uint32_t channel = 0; channel < num_input_channels; ++channel) {
      std::memset(driver->input_buffers_[channel], 0,
                  static_cast<size_t>(inNumberFrames) * sizeof(float));
    }
    for (uint32_t channel = 0; channel < num_output_channels; ++channel) {
      std::memset(driver->output_buffers_[channel], 0,
                  static_cast<size_t>(inNumberFrames) * sizeof(float));
    }
  }

  const uint64_t frame_bytes = requiredBytes(inNumberFrames);
  bool malformed_output = ioData == nullptr || max_frames == 0 || inNumberFrames > max_frames;
  if (!malformed_output) {
    if (ioData->mNumberBuffers < num_output_channels) {
      malformed_output = true;
    } else {
      for (uint32_t channel = 0; channel < num_output_channels; ++channel) {
        const AudioBuffer& buffer = ioData->mBuffers[channel];
        if (buffer.mData == nullptr || static_cast<uint64_t>(buffer.mDataByteSize) < frame_bytes) {
          malformed_output = true;
          break;
        }
      }
    }
  }
  if (malformed_output) {
    if (driver->route_monitor_) {
      driver->route_monitor_->closeAdmission();
    }
    driver->publishTerminalRouteOutcome(AudioRouteRuntimeOutcome::BufferSizeChanged);
    return noErr;
  }

  if (!driver->is_running_.load(std::memory_order_acquire) || !driver->route_monitor_ ||
      !driver->route_monitor_->permitsRendering()) {
    return noErr;
  }

  auto callback_lease = driver->callback_target_.tryAcquire();
  if (!callback_lease) {
    return noErr;
  }

  if (num_input_channels > 0) {
    auto* input_abl = reinterpret_cast<AudioBufferList*>(driver->input_abl_storage_.data());
    input_abl->mNumberBuffers = num_input_channels;
    for (uint32_t channel = 0; channel < num_input_channels; ++channel) {
      input_abl->mBuffers[channel].mNumberChannels = 1;
      input_abl->mBuffers[channel].mDataByteSize = static_cast<UInt32>(frame_bytes);
      input_abl->mBuffers[channel].mData = driver->input_buffers_[channel];
    }
    driver->notifyInputOperation(
        detail::CoreAudioDriverDirectionAudit::InputDirectionOperation::InputRender);
    const OSStatus render_status = AudioUnitRender(driver->audio_unit_, ioActionFlags, inTimeStamp,
                                                   /*inputBus=*/1, inNumberFrames, input_abl);
    if (render_status != noErr) {
      driver->recordInputRenderFailure();
    }
  }

  uint64_t sample_base = 0;
  const bool sample_valid = validSampleTimestamp(inTimeStamp, inNumberFrames, sample_base);
  uint64_t host_base = 0;
  const bool host_valid =
      inTimeStamp != nullptr && (inTimeStamp->mFlags & kAudioTimeStampHostTimeValid) != 0;
  if (host_valid) {
    host_base = AudioConvertHostTimeToNanos(inTimeStamp->mHostTime);
  }

  const uint32_t sample_rate = driver->render_sample_rate_.load(std::memory_order_acquire);
  const uint32_t chunk_frames = driver->render_chunk_frames_.load(std::memory_order_acquire);
  if (chunk_frames == 0 || sample_rate == 0) {
    if (driver->route_monitor_) {
      driver->route_monitor_->closeAdmission();
    }
    driver->publishTerminalRouteOutcome(AudioRouteRuntimeOutcome::BackendFailure);
    return noErr;
  }

  auto monitor_lease = driver->performance_monitor_target_.tryAcquire();
  const UInt64 callback_start = monitor_lease ? AudioGetCurrentHostTime() : UInt64{0};
  IAudioCallback* callback = callback_lease.get();
  bool first_chunk = true;
  for (uint32_t offset = 0; offset < inNumberFrames;) {
    const uint32_t frames = std::min(chunk_frames, inNumberFrames - offset);
    for (uint32_t channel = 0; channel < num_input_channels; ++channel) {
      driver->input_chunk_buffers_[channel] = driver->input_buffers_[channel] + offset;
    }
    for (uint32_t channel = 0; channel < num_output_channels; ++channel) {
      driver->output_chunk_buffers_[channel] = driver->output_buffers_[channel] + offset;
    }

    AudioProcessBlock block;
    block.input_buffers =
        driver->input_chunk_buffers_.empty() ? nullptr : driver->input_chunk_buffers_.data();
    block.output_buffers = driver->output_chunk_buffers_.data();
    block.num_input_channels = static_cast<uint16_t>(num_input_channels);
    block.num_output_channels = static_cast<uint16_t>(num_output_channels);
    block.num_frames = frames;
    block.device_sample_position = sample_valid ? sample_base + offset : 0;
    block.host_time_nanoseconds =
        host_valid ? hostTimeForOffset(host_base, offset, sample_rate) : 0;
    block.discontinuity =
        sample_valid
            ? (first_chunk && (!driver->stream_timeline_initialized_ ||
                               block.device_sample_position != driver->expected_stream_sample_))
            : first_chunk;
    callback->processAudio(block);
    first_chunk = false;
    offset += frames;
  }

  if (sample_valid) {
    driver->expected_stream_sample_ = sample_base + inNumberFrames;
    driver->stream_timeline_initialized_ = true;
  } else {
    driver->expected_stream_sample_ = 0;
    driver->stream_timeline_initialized_ = false;
  }

  if (monitor_lease) {
    const UInt64 callback_end = AudioGetCurrentHostTime();
    const uint64_t duration_ns = AudioConvertHostTimeToNanos(callback_end - callback_start);
    const uint64_t callback_duration_us = duration_ns / 1000u;
    const uint64_t buffer_duration_us =
        (static_cast<uint64_t>(inNumberFrames) * 1'000'000ull) / sample_rate;
    if (auto* monitor = monitor_lease.get()) {
      monitor->recordAudioCallback(callback_duration_us, buffer_duration_us,
                                   callback->activeClipCount(), sample_rate, inNumberFrames);
    }
  }

  for (uint32_t channel = 0; channel < num_output_channels; ++channel) {
    std::memcpy(ioData->mBuffers[channel].mData, driver->output_buffers_[channel], frame_bytes);
  }
  return noErr;
}

uint32_t CoreAudioDriver::getChannelCount(AudioDeviceID device_id,
                                          AudioObjectPropertyScope scope) const {
  if (device_id == 0) {
    return 0;
  }
  const AudioObjectPropertyAddress address = {kAudioDevicePropertyStreamConfiguration, scope,
                                              kAudioObjectPropertyElementMain};
  UInt32 data_size = 0;
  if (property_api_->getPropertyDataSize(device_id, &address, &data_size) != noErr ||
      data_size < sizeof(AudioBufferList)) {
    return 0;
  }
  std::vector<uint8_t> storage(data_size);
  auto* buffers = reinterpret_cast<AudioBufferList*>(storage.data());
  if (property_api_->getPropertyData(device_id, &address, &data_size, buffers) != noErr) {
    return 0;
  }
  uint64_t channels = 0;
  for (UInt32 index = 0; index < buffers->mNumberBuffers; ++index) {
    channels += buffers->mBuffers[index].mNumberChannels;
  }
  return channels > std::numeric_limits<uint32_t>::max() ? 0 : static_cast<uint32_t>(channels);
}

CFStringRef CoreAudioDriver::copyDeviceUID(AudioDeviceID device_id) const {
  if (device_id == 0) {
    return nullptr;
  }
  const AudioObjectPropertyAddress address = {kAudioDevicePropertyDeviceUID,
                                              kAudioObjectPropertyScopeGlobal,
                                              kAudioObjectPropertyElementMain};
  CFStringRef uid = nullptr;
  UInt32 data_size = sizeof(uid);
  return property_api_->getPropertyData(device_id, &address, &data_size, &uid) == noErr ? uid
                                                                                        : nullptr;
}

UInt32 CoreAudioDriver::getClockDomain(AudioDeviceID device_id) const {
  if (device_id == 0) {
    return 0;
  }
  const AudioObjectPropertyAddress address = {kAudioDevicePropertyClockDomain,
                                              kAudioObjectPropertyScopeGlobal,
                                              kAudioObjectPropertyElementMain};
  UInt32 domain = 0;
  UInt32 size = sizeof(domain);
  property_api_->getPropertyData(device_id, &address, &size, &domain);
  return domain;
}

UInt32 CoreAudioDriver::getUInt32Property(AudioDeviceID device_id,
                                          AudioObjectPropertySelector selector,
                                          AudioObjectPropertyScope scope) const {
  if (device_id == 0) {
    return 0;
  }
  const auto value = getOptionalUInt32Property(device_id, selector, scope);
  return value.value_or(0);
}

std::optional<UInt32>
CoreAudioDriver::getOptionalUInt32Property(AudioDeviceID device_id,
                                           AudioObjectPropertySelector selector,
                                           AudioObjectPropertyScope scope) const {
  if (device_id == 0) {
    return std::nullopt;
  }
  const AudioObjectPropertyAddress address = {selector, scope, kAudioObjectPropertyElementMain};
  UInt32 value = 0;
  UInt32 size = sizeof(value);
  if (property_api_->getPropertyData(device_id, &address, &size, &value) != noErr ||
      size != sizeof(value)) {
    return std::nullopt;
  }
  return value;
}

std::optional<Float64>
CoreAudioDriver::getOptionalFloat64Property(AudioDeviceID device_id,
                                            AudioObjectPropertySelector selector,
                                            AudioObjectPropertyScope scope) const {
  if (device_id == 0) {
    return std::nullopt;
  }
  const AudioObjectPropertyAddress address = {selector, scope, kAudioObjectPropertyElementMain};
  Float64 value = 0.0;
  UInt32 size = sizeof(value);
  if (property_api_->getPropertyData(device_id, &address, &size, &value) != noErr ||
      size != sizeof(value)) {
    return std::nullopt;
  }
  return value;
}

std::optional<UInt32>
CoreAudioDriver::getOptionalStreamLatency(AudioDeviceID device_id,
                                          AudioObjectPropertyScope scope) const {
  if (device_id == 0) {
    return std::nullopt;
  }
  const AudioObjectPropertyAddress streams_address = {kAudioDevicePropertyStreams, scope,
                                                      kAudioObjectPropertyElementMain};
  UInt32 size = 0;
  if (property_api_->getPropertyDataSize(device_id, &streams_address, &size) != noErr ||
      size == 0 || size % sizeof(AudioStreamID) != 0) {
    return std::nullopt;
  }
  std::vector<AudioStreamID> streams(size / sizeof(AudioStreamID));
  if (property_api_->getPropertyData(device_id, &streams_address, &size, streams.data()) != noErr) {
    return std::nullopt;
  }
  UInt32 latency = 0;
  bool observed = false;
  for (const AudioStreamID stream : streams) {
    const AudioObjectPropertyAddress address = {kAudioStreamPropertyLatency,
                                                kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};
    UInt32 stream_latency = 0;
    UInt32 data_size = sizeof(stream_latency);
    if (property_api_->getPropertyData(stream, &address, &data_size, &stream_latency) == noErr &&
        data_size == sizeof(stream_latency)) {
      latency = std::max(latency, stream_latency);
      observed = true;
    }
  }
  return observed ? std::optional<UInt32>(latency) : std::nullopt;
}

std::vector<AudioStreamID> CoreAudioDriver::enumerateStreams(AudioDeviceID device_id,
                                                             AudioObjectPropertyScope scope) const {
  std::vector<AudioStreamID> streams;
  if (device_id == 0) {
    return streams;
  }
  const AudioObjectPropertyAddress address = {kAudioDevicePropertyStreams, scope,
                                              kAudioObjectPropertyElementMain};
  UInt32 data_size = 0;
  if (property_api_->getPropertyDataSize(device_id, &address, &data_size) != noErr ||
      data_size == 0 || data_size % sizeof(AudioStreamID) != 0) {
    return streams;
  }
  streams.resize(data_size / sizeof(AudioStreamID));
  if (property_api_->getPropertyData(device_id, &address, &data_size, streams.data()) != noErr) {
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
  const AudioObjectPropertyAddress address = {selector, kAudioObjectPropertyScopeGlobal,
                                              kAudioObjectPropertyElementMain};
  AudioStreamBasicDescription format{};
  UInt32 size = sizeof(format);
  if (property_api_->getPropertyData(stream_id, &address, &size, &format) != noErr ||
      size != sizeof(format)) {
    return std::nullopt;
  }
  return format;
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

bool CoreAudioDriver::createRouteMonitor() {
  std::vector<CoreAudioRouteDevice> devices;
  devices.reserve(config_.num_inputs > 0 ? 3 : 2);
  devices.push_back({output_device_id_, false, true, false});
  if (config_.num_inputs > 0) {
    notifyInputOperation(detail::CoreAudioDriverDirectionAudit::InputDirectionOperation::Monitor);
    devices.push_back({input_device_id_, true, false, false});
  }
  if (aggregate_device_id_ != 0) {
    devices.push_back({aggregate_device_id_, false, false, true});
  }

  std::vector<CoreAudioRouteStream> streams;
  const auto append_streams = [&](AudioDeviceID device_id, AudioObjectPropertyScope scope,
                                  bool input) {
    if (input) {
      notifyInputOperation(
          detail::CoreAudioDriverDirectionAudit::InputDirectionOperation::StreamEnumeration);
    }
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

  if (!append_streams(output_device_id_, kAudioObjectPropertyScopeOutput, false) ||
      (config_.num_inputs > 0 &&
       !append_streams(input_device_id_, kAudioObjectPropertyScopeInput, true)) ||
      streams.empty()) {
    return false;
  }

  try {
    route_monitor_ = std::make_unique<CoreAudioRouteMonitor>(
        *property_api_, active_route_.actual_sample_rate, active_route_.actual_buffer_frames,
        std::move(devices), std::move(streams));
  } catch (...) {
    route_monitor_.reset();
    return false;
  }
  return true;
}

bool CoreAudioDriver::startRouteMonitorLocked() {
  std::lock_guard<std::mutex> monitor_lock(route_monitor_mutex_);
  if (!route_monitor_ || !route_monitor_->start()) {
    publishTerminalRouteOutcome(AudioRouteRuntimeOutcome::BackendFailure);
    return false;
  }
  route_monitor_->requestCheck();
  const CoreAudioRoutePollResult result = route_monitor_->poll();
  publishRoutePollResult(result);
  return result == CoreAudioRoutePollResult::NoChange;
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
    if (!route_monitor_) {
      break;
    }
    route_monitor_->waitForChange();
    if (!route_monitor_active_.load(std::memory_order_acquire)) {
      break;
    }

    CoreAudioRoutePollResult result = CoreAudioRoutePollResult::NoChange;
    {
      std::lock_guard<std::mutex> monitor_lock(route_monitor_mutex_);
      if (route_monitor_->isTerminal()) {
        result = CoreAudioRoutePollResult::BackendFailure;
      } else {
        result = route_monitor_->poll();
      }
    }

    if (result == CoreAudioRoutePollResult::NoChange &&
        route_outcome_.load(std::memory_order_acquire) == AudioRouteRuntimeOutcome::Healthy) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (is_running_.load(std::memory_order_acquire)) {
        refreshActiveRouteLocked();
      }
      continue;
    }

    if (route_outcome_.load(std::memory_order_acquire) == AudioRouteRuntimeOutcome::Healthy) {
      if (result == CoreAudioRoutePollResult::BackendFailure && route_monitor_->isTerminal()) {
        publishTerminalRouteOutcome(AudioRouteRuntimeOutcome::BackendFailure);
      } else {
        publishRoutePollResult(result);
      }
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stopRenderingLocked();
    }
    route_monitor_active_.store(false, std::memory_order_release);
    {
      std::lock_guard<std::mutex> monitor_lock(route_monitor_mutex_);
      if (route_monitor_) {
        route_monitor_->stop();
      }
    }
    break;
  }
}

void CoreAudioDriver::publishTerminalRouteOutcome(AudioRouteRuntimeOutcome outcome) noexcept {
  AudioRouteRuntimeOutcome expected = AudioRouteRuntimeOutcome::Healthy;
  route_outcome_.compare_exchange_strong(expected, outcome, std::memory_order_acq_rel,
                                         std::memory_order_acquire);
}

void CoreAudioDriver::publishRoutePollResult(CoreAudioRoutePollResult result) noexcept {
  switch (result) {
  case CoreAudioRoutePollResult::NoChange:
    return;
  case CoreAudioRoutePollResult::SampleRateChanged:
    publishTerminalRouteOutcome(AudioRouteRuntimeOutcome::SampleRateChanged);
    break;
  case CoreAudioRoutePollResult::BufferSizeChanged:
    publishTerminalRouteOutcome(AudioRouteRuntimeOutcome::BufferSizeChanged);
    break;
  case CoreAudioRoutePollResult::FormatChanged:
    publishTerminalRouteOutcome(AudioRouteRuntimeOutcome::FormatChanged);
    break;
  case CoreAudioRoutePollResult::InputUnavailable:
    unavailable_route_state_.store(AudioRouteState::InputUnavailable, std::memory_order_release);
    publishTerminalRouteOutcome(AudioRouteRuntimeOutcome::InputRouteUnavailable);
    break;
  case CoreAudioRoutePollResult::OutputUnavailable:
    unavailable_route_state_.store(AudioRouteState::OutputUnavailable, std::memory_order_release);
    publishTerminalRouteOutcome(AudioRouteRuntimeOutcome::OutputRouteUnavailable);
    break;
  case CoreAudioRoutePollResult::BackendFailure:
    publishTerminalRouteOutcome(AudioRouteRuntimeOutcome::BackendFailure);
    break;
  }
}

void CoreAudioDriver::refreshActiveRouteLocked() {
  if (config_.num_inputs > 0) {
    notifyInputOperation(
        detail::CoreAudioDriverDirectionAudit::InputDirectionOperation::RefreshUid);
    active_route_.input_device_id = getDeviceUID(input_device_id_).value_or("");
  } else {
    active_route_.input_device_id.clear();
  }
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
  if (actual_rate.has_value() && std::isfinite(*actual_rate) && *actual_rate > 0.0 &&
      *actual_rate <= std::numeric_limits<uint32_t>::max()) {
    active_route_.actual_sample_rate = static_cast<uint32_t>(std::llround(*actual_rate));
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

  std::optional<UInt32> input_device_latency;
  std::optional<UInt32> input_safety_offset;
  std::optional<UInt32> input_stream_latency;
  if (config_.num_inputs > 0) {
    notifyInputOperation(detail::CoreAudioDriverDirectionAudit::InputDirectionOperation::Latency);
    input_device_latency = getOptionalUInt32Property(input_device_id_, kAudioDevicePropertyLatency,
                                                     kAudioObjectPropertyScopeInput);
    input_safety_offset = getOptionalUInt32Property(
        input_device_id_, kAudioDevicePropertySafetyOffset, kAudioObjectPropertyScopeInput);
    input_stream_latency =
        getOptionalStreamLatency(input_device_id_, kAudioObjectPropertyScopeInput);
  }
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
        size == sizeof(seconds) && std::isfinite(seconds) && seconds >= 0.0) {
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
    notifyInputOperation(detail::CoreAudioDriverDirectionAudit::InputDirectionOperation::Latency);
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
        size == sizeof(seconds) && std::isfinite(seconds) && seconds >= 0.0) {
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

std::optional<uint32_t>
CoreAudioDriver::queryMaximumBufferFrames(AudioDeviceID device_id,
                                          AudioObjectPropertyScope scope) const {
  if (device_id == 0) {
    return std::nullopt;
  }
  const AudioObjectPropertyAddress address = {kAudioDevicePropertyBufferFrameSizeRange, scope,
                                              kAudioObjectPropertyElementMain};
  const auto input_fallback = [&]() -> std::optional<uint32_t> {
    if (scope == kAudioObjectPropertyScopeInput && active_route_.actual_buffer_frames != 0) {
      return active_route_.actual_buffer_frames;
    }
    return std::nullopt;
  };
  UInt32 data_size = 0;
  if (property_api_->getPropertyDataSize(device_id, &address, &data_size) != noErr ||
      data_size == 0 || data_size % sizeof(AudioValueRange) != 0) {
    return input_fallback();
  }
  std::vector<AudioValueRange> ranges;
  try {
    ranges.resize(data_size / sizeof(AudioValueRange));
  } catch (...) {
    return input_fallback();
  }
  if (property_api_->getPropertyData(device_id, &address, &data_size, ranges.data()) != noErr) {
    return input_fallback();
  }
  uint32_t maximum = 0;
  for (const AudioValueRange& range : ranges) {
    if (!std::isfinite(range.mMaximum) || range.mMaximum <= 0.0 ||
        range.mMaximum > static_cast<Float64>(std::numeric_limits<uint32_t>::max())) {
      return std::nullopt;
    }
    const Float64 rounded = std::ceil(range.mMaximum);
    if (rounded <= 0.0 || rounded > static_cast<Float64>(std::numeric_limits<uint32_t>::max())) {
      return std::nullopt;
    }
    maximum = std::max(maximum, static_cast<uint32_t>(rounded));
  }
  return maximum == 0 ? std::nullopt : std::optional<uint32_t>(maximum);
}

bool CoreAudioDriver::allocateRenderBuffers(uint32_t maximum_frames) noexcept {
  size_t input_size = 0;
  size_t output_size = 0;
  if (!checkedPlanarSize(config_.num_inputs, maximum_frames, input_size) ||
      !checkedPlanarSize(config_.num_outputs, maximum_frames, output_size)) {
    return false;
  }
  if (config_.num_inputs > 0 &&
      config_.num_inputs - 1 >
          (std::numeric_limits<size_t>::max() - sizeof(AudioBufferList)) / sizeof(AudioBuffer)) {
    return false;
  }
  try {
    input_storage_.assign(input_size, 0.0f);
    output_storage_.assign(output_size, 0.0f);
    input_buffers_.resize(config_.num_inputs);
    output_buffers_.resize(config_.num_outputs);
    input_chunk_buffers_.resize(config_.num_inputs);
    output_chunk_buffers_.resize(config_.num_outputs);
    for (uint16_t channel = 0; channel < config_.num_inputs; ++channel) {
      input_buffers_[channel] =
          input_storage_.data() + static_cast<size_t>(channel) * maximum_frames;
    }
    for (uint16_t channel = 0; channel < config_.num_outputs; ++channel) {
      output_buffers_[channel] =
          output_storage_.data() + static_cast<size_t>(channel) * maximum_frames;
    }
    if (config_.num_inputs > 0) {
      const size_t buffer_list_size =
          sizeof(AudioBufferList) +
          static_cast<size_t>(config_.num_inputs - 1) * sizeof(AudioBuffer);
      input_abl_storage_.assign(buffer_list_size, 0);
    } else {
      input_abl_storage_.clear();
    }
  } catch (...) {
    return false;
  }
  return true;
}

std::optional<std::string> CoreAudioDriver::getDeviceUID(AudioDeviceID device_id) const {
  CFStringRef uid = copyDeviceUID(device_id);
  if (uid == nullptr) {
    return std::nullopt;
  }
  char buffer[1024] = {};
  const Boolean converted = CFStringGetCString(uid, buffer, sizeof(buffer), kCFStringEncodingUTF8);
  CFRelease(uid);
  return converted ? std::optional<std::string>(std::string(buffer)) : std::nullopt;
}

SessionGraphError CoreAudioDriver::setupAudioUnit(AudioDeviceID device_id) {
  AudioComponentDescription description = {};
  description.componentType = kAudioUnitType_Output;
  description.componentSubType = kAudioUnitSubType_HALOutput;
  description.componentManufacturer = kAudioUnitManufacturer_Apple;
  AudioComponent component = AudioComponentFindNext(nullptr, &description);
  if (!component || AudioComponentInstanceNew(component, &audio_unit_) != noErr || !audio_unit_) {
    return SessionGraphError::InternalError;
  }

  UInt32 enable_input = config_.num_inputs > 0 ? 1 : 0;
  if (config_.num_inputs > 0) {
    notifyInputOperation(
        detail::CoreAudioDriverDirectionAudit::InputDirectionOperation::AudioUnitEnable);
  }
  OSStatus status =
      AudioUnitSetProperty(audio_unit_, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1,
                           &enable_input, sizeof(enable_input));
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

  if (config_.num_inputs > 0) {
    notifyInputOperation(
        detail::CoreAudioDriverDirectionAudit::InputDirectionOperation::AudioUnitConfigure);
  }
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

AudioRouteRuntimeOutcome CoreAudioDriver::disableAutomaticHogModeLocked() noexcept {
  if (automatic_hog_mode_changed_) {
    return AudioRouteRuntimeOutcome::Healthy;
  }
  const AudioObjectPropertyAddress address = {kAudioHardwarePropertyHogModeIsAllowed,
                                              kAudioObjectPropertyScopeGlobal,
                                              kAudioObjectPropertyElementMain};
  UInt32 allowed = 0;
  UInt32 size = sizeof(allowed);
  if (property_api_->getPropertyData(kAudioObjectSystemObject, &address, &size, &allowed) !=
          noErr ||
      size != sizeof(allowed)) {
    return AudioRouteRuntimeOutcome::BackendFailure;
  }
  if (allowed == 0) {
    previous_hog_mode_allowed_ = 0;
    return AudioRouteRuntimeOutcome::Healthy;
  }
  Boolean settable = 0;
  if (property_api_->isPropertySettable(kAudioObjectSystemObject, &address, &settable) != noErr ||
      settable == 0) {
    return AudioRouteRuntimeOutcome::BackendFailure;
  }
  const UInt32 previous = allowed;
  const UInt32 disabled = 0;
  if (property_api_->setPropertyData(kAudioObjectSystemObject, &address, sizeof(disabled),
                                     &disabled) != noErr) {
    return AudioRouteRuntimeOutcome::BackendFailure;
  }
  allowed = 1;
  size = sizeof(allowed);
  if (property_api_->getPropertyData(kAudioObjectSystemObject, &address, &size, &allowed) !=
          noErr ||
      size != sizeof(allowed) || allowed != 0) {
    return AudioRouteRuntimeOutcome::BackendFailure;
  }
  previous_hog_mode_allowed_ = previous;
  automatic_hog_mode_changed_ = true;
  return AudioRouteRuntimeOutcome::Healthy;
}

bool CoreAudioDriver::restoreAutomaticHogModeLocked() noexcept {
  if (!automatic_hog_mode_changed_) {
    return true;
  }
  const AudioObjectPropertyAddress address = {kAudioHardwarePropertyHogModeIsAllowed,
                                              kAudioObjectPropertyScopeGlobal,
                                              kAudioObjectPropertyElementMain};
  const UInt32 previous = previous_hog_mode_allowed_;
  UInt32 observed = 0;
  UInt32 size = sizeof(observed);
  if (property_api_->setPropertyData(kAudioObjectSystemObject, &address, sizeof(previous),
                                     &previous) != noErr ||
      property_api_->getPropertyData(kAudioObjectSystemObject, &address, &size, &observed) !=
          noErr ||
      size != sizeof(observed) || observed != previous) {
    return false;
  }
  automatic_hog_mode_changed_ = false;
  previous_hog_mode_allowed_ = 0;
  return true;
}

void CoreAudioDriver::cleanupAudioUnit() {
  if (audio_unit_) {
    AudioUnitUninitialize(audio_unit_);
    AudioComponentInstanceDispose(audio_unit_);
    audio_unit_ = nullptr;
  }
  callback_target_.replaceAndDrain(nullptr);
  render_capacity_frames_ = 0;
  render_sample_rate_.store(0, std::memory_order_release);
  render_max_callback_frames_.store(0, std::memory_order_release);
  render_chunk_frames_.store(0, std::memory_order_release);

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
  input_chunk_buffers_.clear();
  output_chunk_buffers_.clear();
  input_storage_.clear();
  output_storage_.clear();
  input_abl_storage_.clear();
}

void CoreAudioDriver::stopRenderingLocked() {
  if (audio_unit_) {
    AudioOutputUnitStop(audio_unit_);
  }
  callback_target_.replaceAndDrain(nullptr);
  is_running_.store(false, std::memory_order_release);
  render_sample_rate_.store(0, std::memory_order_release);
  render_max_callback_frames_.store(0, std::memory_order_release);
  render_chunk_frames_.store(0, std::memory_order_release);
  if (!restoreAutomaticHogModeLocked()) {
    publishTerminalRouteOutcome(AudioRouteRuntimeOutcome::BackendFailure);
  }
}

ORPHEUS_API std::unique_ptr<IAudioDriver> createCoreAudioDriver() {
  return std::make_unique<CoreAudioDriver>();
}

} // namespace orpheus
