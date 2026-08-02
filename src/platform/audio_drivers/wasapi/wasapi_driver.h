// SPDX-License-Identifier: MIT
#pragma once

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <audioclient.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <orpheus/audio_driver.h>
#include <orpheus/realtime_diagnostics.h>

#include "../../../core/common/realtime_borrowed_target.h"

namespace orpheus {

class IPerformanceMonitor;

namespace detail {

enum class WASAPIRenderStatus : uint8_t {
  Ready,
  NoFrames,
  Timeout,
  DeviceInvalidated,
  BackendFailure,
};

struct WASAPIResolvedFormat {
  uint32_t sample_rate{0};
  uint16_t buffer_size{0};
  uint16_t num_outputs{0};
  bool float32{false};
};

/// Testable boundary around COM/audio-client state and the worker wait path.
class IWASAPIRenderRuntime {
public:
  virtual ~IWASAPIRenderRuntime() = default;

  virtual SessionGraphError initialize(const AudioDriverConfig& requested,
                                       WASAPIResolvedFormat& resolved) = 0;
  virtual WASAPIRenderStatus start() noexcept = 0;
  virtual void signalStop() noexcept = 0;
  virtual WASAPIRenderStatus wait() noexcept = 0;
  virtual WASAPIRenderStatus getPadding(uint32_t& padding) noexcept = 0;
  virtual WASAPIRenderStatus acquire(uint32_t frames, BYTE*& device_buffer) noexcept = 0;
  virtual WASAPIRenderStatus release(uint32_t frames) noexcept = 0;
  virtual WASAPIRenderStatus stop() noexcept = 0;
  virtual void cleanup() noexcept = 0;
  virtual uint32_t bufferFrames() const noexcept = 0;
  virtual bool float32() const noexcept = 0;
  virtual uint32_t latencySamples(uint32_t sample_rate) const noexcept = 0;
};

using WASAPIWorkerLauncher = std::function<std::thread(std::function<void()>)>;

std::unique_ptr<IWASAPIRenderRuntime> createWASAPIRenderRuntime();

} // namespace detail

class WASAPIAudioDriver final : public IAudioDriver {
public:
  WASAPIAudioDriver();
  explicit WASAPIAudioDriver(std::unique_ptr<detail::IWASAPIRenderRuntime> runtime,
                             detail::WASAPIWorkerLauncher worker_launcher = {});
  ~WASAPIAudioDriver() override;

  SessionGraphError initialize(const AudioDriverConfig& requested) override;
  SessionGraphError start(IAudioCallback* callback) override;
  SessionGraphError stop() override;
  bool isRunning() const override;
  const AudioDriverConfig& getConfig() const override;
  std::string getDriverName() const override;
  uint32_t getLatencySamples() const override;
  AudioIoTelemetry getTelemetry() const noexcept override;
  AudioDriverCapabilities getCapabilities() const override;
  void setPerformanceMonitor(IPerformanceMonitor* monitor) override;

private:
  void audioLoop() noexcept;
  void markTerminalFailure() noexcept;
  void clearCallback() noexcept;
  void cleanup() noexcept;
  bool hasResources() const noexcept;

  AudioDriverConfig config_{};
  std::unique_ptr<detail::IWASAPIRenderRuntime> runtime_;
  detail::WASAPIWorkerLauncher worker_launcher_;
  std::thread thread_;
  std::vector<float> planar_storage_;
  std::vector<float*> planar_pointers_;
  detail::RealtimeBorrowedTarget<IAudioCallback> callback_target_;
  detail::RealtimeBorrowedTarget<IPerformanceMonitor> performance_monitor_target_;
  std::atomic<bool> running_{false};
  std::atomic<bool> initialized_{false};
  std::atomic<bool> reinitialize_required_{false};
  std::atomic<AudioDriverRuntimeOutcome> runtime_outcome_{AudioDriverRuntimeOutcome::Healthy};
  detail::WASAPIResolvedFormat resolved_{};
};

} // namespace orpheus

#endif
