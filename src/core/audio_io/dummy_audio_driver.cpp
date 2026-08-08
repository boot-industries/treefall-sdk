// SPDX-License-Identifier: MIT
#include "dummy_audio_driver.h"

#include <algorithm>
#include <chrono>

namespace orpheus {

DummyAudioDriver::DummyAudioDriver() = default;

DummyAudioDriver::~DummyAudioDriver() {
  stop();
}

SessionGraphError DummyAudioDriver::initialize(const AudioDriverConfig& config) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (running_.load(std::memory_order_acquire)) {
    return SessionGraphError::InternalError;
  }
  if (config.sample_rate == 0 || config.buffer_size == 0 || config.num_inputs > 32 ||
      config.num_outputs > 32) {
    return SessionGraphError::InvalidParameter;
  }

  config_ = config;
  input_buffer_storage_.assign(config_.num_inputs, std::vector<float>(config_.buffer_size, 0.0f));
  output_buffer_storage_.assign(config_.num_outputs, std::vector<float>(config_.buffer_size, 0.0f));
  input_ptrs_.resize(config_.num_inputs);
  output_ptrs_.resize(config_.num_outputs);
  for (size_t channel = 0; channel < input_buffer_storage_.size(); ++channel) {
    input_ptrs_[channel] = input_buffer_storage_[channel].data();
  }
  for (size_t channel = 0; channel < output_buffer_storage_.size(); ++channel) {
    output_ptrs_[channel] = output_buffer_storage_[channel].data();
  }

  // The dummy backend has no physical endpoint, so it must not fabricate route
  // identity, liveness, channel mapping, or hardware latency facts.
  active_route_ = {};
  input_render_failures_.store(0, std::memory_order_release);
  initialized_ = true;
  return SessionGraphError::OK;
}

SessionGraphError DummyAudioDriver::initializeAudioOutput(const AudioOutputRouteRequest& request) {
  if (request.output_channel_map.empty() || request.requested_sample_rate == 0 ||
      request.requested_buffer_size == 0 || request.requested_buffer_size > 0xffffu ||
      request.output_channel_map.size() > 32 ||
      (!request.output_device_id.empty() && request.output_device_id != "dummy")) {
    return SessionGraphError::InvalidParameter;
  }
  std::vector<bool> seen(32, false);
  for (const uint16_t channel : request.output_channel_map) {
    if (channel >= seen.size() || seen[channel]) {
      return SessionGraphError::InvalidParameter;
    }
    seen[channel] = true;
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

SessionGraphError DummyAudioDriver::start(IAudioCallback* callback) {
  if (running_.load(std::memory_order_acquire)) {
    return SessionGraphError::InternalError;
  }
  if (!initialized_) {
    return SessionGraphError::NotReady;
  }
  if (callback == nullptr) {
    return SessionGraphError::InvalidParameter;
  }

  callback_ = callback;
  should_stop_.store(false, std::memory_order_release);
  running_.store(true, std::memory_order_release);
  try {
    audio_thread_ = std::thread(&DummyAudioDriver::audioThreadMain, this);
  } catch (...) {
    running_.store(false, std::memory_order_release);
    callback_ = nullptr;
    return SessionGraphError::InternalError;
  }
  return SessionGraphError::OK;
}

SessionGraphError DummyAudioDriver::stop() {
  should_stop_.store(true, std::memory_order_release);
  if (audio_thread_.joinable()) {
    audio_thread_.join();
  }
  std::lock_guard<std::mutex> lock(mutex_);
  running_.store(false, std::memory_order_release);
  callback_ = nullptr;
  return SessionGraphError::OK;
}

bool DummyAudioDriver::isRunning() const {
  return running_.load(std::memory_order_acquire);
}

const AudioDriverConfig& DummyAudioDriver::getConfig() const {
  return config_;
}

std::string DummyAudioDriver::getDriverName() const {
  return "Dummy";
}

uint32_t DummyAudioDriver::getLatencySamples() const {
  return config_.buffer_size;
}

AudioDriverCapabilities DummyAudioDriver::getCapabilities() const {
  AudioDriverCapabilities caps;
  caps.backend = AudioBackend::Dummy;
  caps.platform = AudioPlatform::Unknown;
  caps.min_output_channels = config_.num_outputs == 0 ? 0 : 1;
  caps.max_output_channels = config_.num_outputs;
  caps.min_input_channels = 0;
  caps.max_input_channels = config_.num_inputs;
  caps.native_sample_rates.push_back(config_.sample_rate);
  caps.native_buffer_sizes.push_back(config_.buffer_size);
  caps.supports_shared_mode = true;
  caps.supports_input = config_.num_inputs > 0;
  caps.supports_multichannel_output = config_.num_outputs > 2;
  caps.supports_device_hot_swap = false;
  caps.reports_hardware_latency = false;
  return caps;
}

ActiveAudioRoute DummyAudioDriver::getActiveRoute() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return active_route_;
}

AudioIoRouteState DummyAudioDriver::getAudioIoRouteState() const {
  std::lock_guard<std::mutex> lock(mutex_);
  AudioIoRouteState state;
  state.state = running_.load(std::memory_order_acquire) ? AudioRouteState::Running
                                                         : AudioRouteState::Inactive;
  state.selected_input_device_id = config_.input_device_id;
  state.selected_output_device_id = config_.output_device_id;
  state.requested_sample_rate = config_.sample_rate;
  state.actual_sample_rate = config_.sample_rate;
  state.requested_buffer_size = config_.buffer_size;
  state.actual_buffer_size = config_.buffer_size;
  state.active_output_channel_map = config_.channel_map.output_channels;
  if (state.active_output_channel_map.empty()) {
    state.active_output_channel_map.resize(config_.num_outputs);
    for (uint16_t channel = 0; channel < config_.num_outputs; ++channel) {
      state.active_output_channel_map[channel] = channel;
    }
  }
  state.active_input_channel_map = config_.channel_map.input_channels;
  state.detail = "Dummy driver has no physical endpoint or hardware latency.";
  return state;
}

AudioIoTelemetry DummyAudioDriver::getTelemetry() const noexcept {
  return {input_render_failures_.load(std::memory_order_acquire),
          AudioDriverRuntimeOutcome::Healthy, AudioRouteRuntimeOutcome::Healthy};
}

AudioRouteCompatibility DummyAudioDriver::probeRoute(const AudioDriverConfig& config) const {
  AudioRouteCompatibility compatibility;
  compatibility.requested_sample_rate = config.sample_rate;
  if (config.num_outputs == 0) {
    compatibility.status = AudioRouteCompatibilityStatus::OutputUnavailable;
    compatibility.detail = "config:output";
    return compatibility;
  }
  if (config.sample_rate == 0 || config.buffer_size == 0) {
    compatibility.status = AudioRouteCompatibilityStatus::BackendFailure;
    compatibility.detail = "config:output";
    return compatibility;
  }

  const auto resolve_id = [](const std::string& requested, std::string& resolved,
                             const char* direction, std::string& detail) {
    if (requested.empty() || requested == "dummy") {
      resolved = "dummy";
      return true;
    }
    detail = std::string("resolve:") + direction;
    return false;
  };
  if (!resolve_id(config.output_device_id, compatibility.resolved_output_device_id, "output",
                  compatibility.detail)) {
    compatibility.status = AudioRouteCompatibilityStatus::OutputUnavailable;
    return compatibility;
  }
  if (config.num_inputs > 0 &&
      !resolve_id(config.input_device_id, compatibility.resolved_input_device_id, "input",
                  compatibility.detail)) {
    compatibility.status = AudioRouteCompatibilityStatus::InputUnavailable;
    return compatibility;
  }

  const auto resolve_map = [](const std::vector<uint16_t>& requested, uint16_t logical_count,
                              std::vector<uint16_t>& resolved) {
    if (logical_count > 32) {
      return false;
    }
    if (requested.empty()) {
      resolved.resize(logical_count);
      for (uint16_t channel = 0; channel < logical_count; ++channel) {
        resolved[channel] = channel;
      }
      return true;
    }
    if (requested.size() != logical_count) {
      return false;
    }
    resolved.clear();
    resolved.reserve(requested.size());
    for (const uint16_t channel : requested) {
      if (channel >= 32 || std::find(resolved.begin(), resolved.end(), channel) != resolved.end()) {
        return false;
      }
      resolved.push_back(channel);
    }
    return true;
  };

  std::vector<uint16_t> resolved_output;
  if (!resolve_map(config.channel_map.output_channels, config.num_outputs, resolved_output)) {
    compatibility.status = AudioRouteCompatibilityStatus::InvalidChannelMap;
    compatibility.detail = "map:output";
    return compatibility;
  }
  if (config.num_inputs > 0) {
    std::vector<uint16_t> resolved_input;
    if (!resolve_map(config.channel_map.input_channels, config.num_inputs, resolved_input)) {
      compatibility.status = AudioRouteCompatibilityStatus::InvalidChannelMap;
      compatibility.detail = "map:input";
      return compatibility;
    }
  }

  compatibility.current_output_sample_rate = config.sample_rate;
  if (config.num_inputs > 0) {
    compatibility.current_input_sample_rate = config.sample_rate;
  }
  compatibility.status = AudioRouteCompatibilityStatus::Compatible;
  compatibility.detail.clear();
  return compatibility;
}

void DummyAudioDriver::audioThreadMain() {
  const auto buffer_duration = std::chrono::duration<double>(
      static_cast<double>(config_.buffer_size) / static_cast<double>(config_.sample_rate));
  const auto sleep_duration =
      std::chrono::duration_cast<std::chrono::microseconds>(buffer_duration * 0.95);
  uint64_t stream_position = 0;

  while (!should_stop_.load(std::memory_order_acquire)) {
    const auto block_started = std::chrono::steady_clock::now();
    IAudioCallback* callback = nullptr;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      callback = callback_;
    }
    if (callback == nullptr) {
      break;
    }

    for (auto& buffer : output_buffer_storage_) {
      std::fill(buffer.begin(), buffer.end(), 0.0f);
    }

    AudioProcessBlock block;
    block.input_buffers = input_ptrs_.empty() ? nullptr : input_ptrs_.data();
    block.output_buffers = output_ptrs_.empty() ? nullptr : output_ptrs_.data();
    block.num_input_channels = config_.num_inputs;
    block.num_output_channels = config_.num_outputs;
    block.num_frames = config_.buffer_size;
    block.device_sample_position = stream_position;
    block.host_time_nanoseconds = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(block_started.time_since_epoch())
            .count());
    block.discontinuity = false;
    callback->processAudio(block);
    stream_position += config_.buffer_size;

    std::this_thread::sleep_for(sleep_duration);
  }
}

std::unique_ptr<IAudioDriver> createDummyAudioDriver() {
  return std::make_unique<DummyAudioDriver>();
}

} // namespace orpheus
