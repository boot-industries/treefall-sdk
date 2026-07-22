// SPDX-License-Identifier: MIT
#include "dummy_audio_driver.h"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace orpheus {

DummyAudioDriver::DummyAudioDriver() = default;

DummyAudioDriver::~DummyAudioDriver() {
  stop();
}

SessionGraphError DummyAudioDriver::initialize(const AudioDriverConfig& config) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_running.load(std::memory_order_acquire)) {
    return SessionGraphError::InternalError; // Cannot initialize while running
  }

  // Validate configuration
  if (config.sample_rate == 0 || config.buffer_size == 0) {
    return SessionGraphError::InvalidParameter;
  }

  if (config.num_inputs > 32 || config.num_outputs > 32) {
    return SessionGraphError::InvalidParameter;
  }

  m_config = config;

  // Pre-allocate buffers
  m_input_buffer_storage.clear();
  m_output_buffer_storage.clear();
  m_input_ptrs.clear();
  m_output_ptrs.clear();
  m_event_write.store(0, std::memory_order_relaxed);
  m_event_read.store(0, std::memory_order_relaxed);
  m_dropped_events.store(0, std::memory_order_relaxed);
  m_sample_position.store(0, std::memory_order_relaxed);
  m_next_discontinuity.store(true, std::memory_order_relaxed);

  for (size_t i = 0; i < m_config.num_inputs; ++i) {
    m_input_buffer_storage.emplace_back(m_config.buffer_size, 0.0f);
    m_input_ptrs.push_back(m_input_buffer_storage.back().data());
  }

  for (size_t i = 0; i < m_config.num_outputs; ++i) {
    m_output_buffer_storage.emplace_back(m_config.buffer_size, 0.0f);
    m_output_ptrs.push_back(m_output_buffer_storage.back().data());
  }

  return SessionGraphError::OK;
}

SessionGraphError DummyAudioDriver::start(IAudioCallback* callback) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (!callback) {
    return SessionGraphError::InvalidParameter;
  }

  if (m_running.load(std::memory_order_acquire)) {
    return SessionGraphError::InternalError; // Already running
  }

  // Check if initialized by verifying buffers are allocated
  if (m_output_ptrs.empty()) {
    return SessionGraphError::NotReady; // Must call initialize first
  }

  m_callback = callback;
  m_should_stop.store(false, std::memory_order_release);
  m_running.store(true, std::memory_order_release);

  // Start audio thread
  m_audio_thread = std::thread(&DummyAudioDriver::audioThreadMain, this);

  return SessionGraphError::OK;
}

SessionGraphError DummyAudioDriver::stop() {
  {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running.load(std::memory_order_acquire)) {
      return SessionGraphError::OK; // Already stopped
    }

    m_should_stop.store(true, std::memory_order_release);
  }

  // Wait for audio thread to finish (outside of lock to avoid deadlock)
  if (m_audio_thread.joinable()) {
    m_audio_thread.join();
  }

  m_running.store(false, std::memory_order_release);
  m_callback = nullptr;

  return SessionGraphError::OK;
}

bool DummyAudioDriver::isRunning() const {
  return m_running.load(std::memory_order_acquire);
}

const AudioDriverConfig& DummyAudioDriver::getConfig() const {
  return m_config;
}

std::string DummyAudioDriver::getDriverName() const {
  return "Dummy";
}

uint32_t DummyAudioDriver::getLatencySamples() const {
  // Dummy driver reports buffer size as latency
  return m_config.buffer_size;
}

AudioDriverCapabilities DummyAudioDriver::getCapabilities() const {
  AudioDriverCapabilities caps;
  caps.backend = AudioBackend::Dummy;
  caps.platform = AudioPlatform::Unknown;
  caps.min_output_channels = m_config.num_outputs == 0 ? 0 : 1;
  caps.max_output_channels = m_config.num_outputs;
  caps.min_input_channels = 0;
  caps.max_input_channels = m_config.num_inputs;
  caps.native_sample_rates.push_back(m_config.sample_rate);
  caps.native_buffer_sizes.push_back(m_config.buffer_size);
  const std::string input_endpoint =
      m_config.input_device_id.empty() ? "dummy:default" : m_config.input_device_id;
  const std::string output_endpoint =
      m_config.output_device_id.empty() ? "dummy:default" : m_config.output_device_id;
  for (uint16_t channel = 0; channel < m_config.num_inputs; ++channel) {
    caps.input_channel_ids.push_back(input_endpoint + ":input:" + std::to_string(channel));
  }
  for (uint16_t channel = 0; channel < m_config.num_outputs; ++channel) {
    caps.output_channel_ids.push_back(output_endpoint + ":output:" + std::to_string(channel));
  }
  caps.supports_exclusive_mode = false;
  caps.supports_shared_mode = true;
  caps.supports_device_hot_swap = false;
  caps.supports_input = m_config.num_inputs > 0;
  caps.supports_multichannel_output = m_config.num_outputs > 2;
  caps.reports_hardware_latency = false;
  return caps;
}

AudioDriverRuntimeInfo DummyAudioDriver::getRuntimeInfo() const {
  AudioDriverRuntimeInfo info;
  if (m_config.num_inputs > 0) {
    info.selected_input_device_id =
        m_config.input_device_id.empty() ? "dummy:default" : m_config.input_device_id;
  }
  if (m_config.num_outputs > 0) {
    info.selected_output_device_id =
        m_config.output_device_id.empty() ? "dummy:default" : m_config.output_device_id;
  }
  info.negotiated_sample_rate = m_config.sample_rate;
  info.maximum_callback_frames = m_config.buffer_size;
  info.input_channels = m_config.num_inputs;
  info.output_channels = m_config.num_outputs;
  info.latency_samples = getLatencySamples();
  info.supports_runtime_events = false;
  return info;
}

void DummyAudioDriver::pushEvent(const AudioDriverEvent& event) noexcept {
  const uint64_t write = m_event_write.load(std::memory_order_relaxed);
  const uint64_t read = m_event_read.load(std::memory_order_acquire);
  if (write - read >= kEventCapacity) {
    m_dropped_events.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  m_events[write % kEventCapacity] = event;
  m_event_write.store(write + 1, std::memory_order_release);
}

bool DummyAudioDriver::pollEvent(AudioDriverEvent& event) noexcept {
  const uint64_t read = m_event_read.load(std::memory_order_relaxed);
  const uint64_t write = m_event_write.load(std::memory_order_acquire);
  if (read == write) {
    return false;
  }
  event = m_events[read % kEventCapacity];
  m_event_read.store(read + 1, std::memory_order_release);
  return true;
}

uint64_t DummyAudioDriver::droppedEventCount() const noexcept {
  return m_dropped_events.load(std::memory_order_relaxed);
}

void DummyAudioDriver::renderOnceForTesting(uint32_t frames, IAudioCallback& callback) noexcept {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  const uint64_t host_time =
      static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
  if (frames == 0 || frames > m_config.buffer_size) {
    if (frames > m_config.buffer_size) {
      pushEvent({AudioDriverEventType::CapacityExceeded, m_config.sample_rate, frames, host_time,
                 SessionGraphError::InvalidParameter});
      m_next_discontinuity.store(true, std::memory_order_release);
    }
    return;
  }
  for (auto& buffer : m_input_buffer_storage) {
    std::fill_n(buffer.data(), frames, 0.0f);
  }
  for (auto& buffer : m_output_buffer_storage) {
    std::fill_n(buffer.data(), frames, 0.0f);
  }
  AudioProcessBlock block;
  block.input_buffers = m_config.num_inputs > 0 ? m_input_ptrs.data() : nullptr;
  block.output_buffers = m_config.num_outputs > 0 ? m_output_ptrs.data() : nullptr;
  block.num_input_channels = m_config.num_inputs;
  block.num_output_channels = m_config.num_outputs;
  block.num_frames = frames;
  block.device_sample_position = m_sample_position.load(std::memory_order_relaxed);
  block.host_time_nanoseconds = host_time;
  block.discontinuity = m_next_discontinuity.exchange(false, std::memory_order_acq_rel);
  callback.processAudio(block);
  m_sample_position.fetch_add(frames, std::memory_order_relaxed);
}

void DummyAudioDriver::audioThreadMain() {
  // Calculate sleep time to simulate real-time audio processing
  // Sleep for slightly less than buffer duration to avoid drift
  const double buffer_duration_sec =
      static_cast<double>(m_config.buffer_size) / static_cast<double>(m_config.sample_rate);
  const auto sleep_duration = std::chrono::microseconds(
      static_cast<int64_t>(buffer_duration_sec * 1e6 * 0.95) // 95% to account for jitter
  );

  while (!m_should_stop.load(std::memory_order_acquire)) {
    if (m_callback && m_running.load(std::memory_order_acquire)) {
      renderOnceForTesting(m_config.buffer_size, *m_callback);
    }

    // Sleep to simulate real-time constraints
    std::this_thread::sleep_for(sleep_duration);
  }
}

// Factory function
std::unique_ptr<IAudioDriver> createDummyAudioDriver() {
  return std::make_unique<DummyAudioDriver>();
}

} // namespace orpheus
