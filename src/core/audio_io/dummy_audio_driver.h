// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/audio_driver.h>

#include <array>
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
  AudioDriverCapabilities getCapabilities() const override;
  AudioDriverRuntimeInfo getRuntimeInfo() const override;
  bool pollEvent(AudioDriverEvent& event) noexcept override;
  uint64_t droppedEventCount() const noexcept override;

  /// Deterministic test hook. Invokes one complete prepared callback, or emits
  /// CapacityExceeded without invoking the host when frames exceed capacity.
  void renderOnceForTesting(uint32_t frames, IAudioCallback& callback) noexcept;

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

  static constexpr uint64_t kEventCapacity = 64;
  std::array<AudioDriverEvent, kEventCapacity> m_events{};
  std::atomic<uint64_t> m_event_write{0};
  std::atomic<uint64_t> m_event_read{0};
  std::atomic<uint64_t> m_dropped_events{0};
  std::atomic<uint64_t> m_sample_position{0};
  std::atomic<bool> m_next_discontinuity{true};

  void pushEvent(const AudioDriverEvent& event) noexcept;

  mutable std::mutex m_mutex;
};

} // namespace orpheus
