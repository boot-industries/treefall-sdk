// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/audio_driver.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace orpheus {

/// Dummy audio driver for testing
/// Simulates real audio hardware by calling the callback on a separate thread
class DummyAudioDriver : public IAudioDriver {
public:
  DummyAudioDriver();
  ~DummyAudioDriver() override;

  // IAudioDriver interface
  SessionGraphError initialize(const AudioDriverConfig& config) override;
  SessionGraphError start(IAudioCallback* callback) override;
  SessionGraphError stop() override;
  bool isRunning() const override;
  const AudioDriverConfig& getConfig() const override;
  std::string getDriverName() const override;
  uint32_t getLatencySamples() const override;

private:
  void audioThreadMain();

  AudioDriverConfig m_config;
  IAudioCallback* m_callback{nullptr};

  std::atomic<bool> m_running{false};
  std::atomic<bool> m_should_stop{false};
  std::thread m_audio_thread;

  // Pre-allocated buffers (to avoid allocations in audio thread)
  std::vector<std::vector<float>> m_input_buffer_storage;
  std::vector<std::vector<float>> m_output_buffer_storage;
  std::vector<const float*> m_input_ptrs;
  std::vector<float*> m_output_ptrs;

  mutable std::mutex m_mutex;
};

} // namespace orpheus
