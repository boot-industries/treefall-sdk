// SPDX-License-Identifier: MIT
#include "dummy_audio_driver.h"

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

void DummyAudioDriver::audioThreadMain() {
  // Calculate sleep time to simulate real-time audio processing
  // Sleep for slightly less than buffer duration to avoid drift
  const double buffer_duration_sec =
      static_cast<double>(m_config.buffer_size) / static_cast<double>(m_config.sample_rate);
  const auto sleep_duration = std::chrono::microseconds(
      static_cast<int64_t>(buffer_duration_sec * 1e6 * 0.95) // 95% to account for jitter
  );

  while (!m_should_stop.load(std::memory_order_acquire)) {
    // Clear input buffers (simulate silence from input device)
    for (auto& buffer : m_input_buffer_storage) {
      std::memset(buffer.data(), 0, buffer.size() * sizeof(float));
    }

    // Clear output buffers
    for (auto& buffer : m_output_buffer_storage) {
      std::memset(buffer.data(), 0, buffer.size() * sizeof(float));
    }

    // Call audio callback (with safety check for shutdown race)
    if (m_callback && m_running.load(std::memory_order_acquire)) {
      const float** input_ptrs = m_config.num_inputs > 0 ? m_input_ptrs.data() : nullptr;

      m_callback->processAudio(input_ptrs, m_output_ptrs.data(), m_config.num_outputs,
                               m_config.buffer_size);
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
