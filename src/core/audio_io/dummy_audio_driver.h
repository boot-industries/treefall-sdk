// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/audio_driver.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace orpheus {

/// Dummy audio driver for testing.
/// Simulates real audio hardware by calling the callback on a separate thread.
class DummyAudioDriver : public IAudioDriver {
public:
  DummyAudioDriver();
  ~DummyAudioDriver() override;

  SessionGraphError initialize(const AudioDriverConfig& config) override;
  SessionGraphError start(IAudioCallback* callback) override;
  SessionGraphError stop() override;
  bool isRunning() const override;
  const AudioDriverConfig& getConfig() const override;
  std::string getDriverName() const override;
  uint32_t getLatencySamples() const override;
  AudioDriverCapabilities getCapabilities() const override;
  ActiveAudioRoute getActiveRoute() const override;
  AudioIoTelemetry getTelemetry() const noexcept override;

private:
  void audioThreadMain();

  AudioDriverConfig config_;
  bool initialized_{false};
  IAudioCallback* callback_{nullptr};
  std::atomic<bool> running_{false};
  std::atomic<bool> should_stop_{false};
  std::atomic<uint64_t> input_render_failures_{0};
  std::thread audio_thread_;

  std::vector<std::vector<float>> input_buffer_storage_;
  std::vector<std::vector<float>> output_buffer_storage_;
  std::vector<const float*> input_ptrs_;
  std::vector<float*> output_ptrs_;

  ActiveAudioRoute active_route_;
  mutable std::mutex mutex_;
};

} // namespace orpheus
