// SPDX-License-Identifier: MIT
#ifdef _WIN32

#include "wasapi_driver.h"

#include <orpheus/audio_driver.h>
#include <orpheus/audio_driver_manager.h>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace orpheus {
namespace {

class FakeRenderRuntime final : public detail::IWASAPIRenderRuntime {
public:
  SessionGraphError initialize(const AudioDriverConfig& requested,
                               detail::WASAPIResolvedFormat& resolved) override {
    ++initialize_calls;
    if (initialize_result != SessionGraphError::OK) {
      return initialize_result;
    }
    resolved.sample_rate = requested.sample_rate;
    resolved.buffer_size = requested.buffer_size;
    resolved.num_outputs = resolved_output_channels;
    resolved.float32 = true;
    initialized = true;
    device_buffer.assign(static_cast<size_t>(resolved_output_channels) * buffer_frames, 0.0f);
    return SessionGraphError::OK;
  }

  detail::WASAPIRenderStatus start() noexcept override {
    ++start_calls;
    started = start_status == detail::WASAPIRenderStatus::Ready;
    return start_status;
  }

  void signalStop() noexcept override {
    stop_signaled.store(true, std::memory_order_release);
  }

  detail::WASAPIRenderStatus wait() noexcept override {
    ++wait_calls;
    if (stop_signaled.load(std::memory_order_acquire)) {
      return detail::WASAPIRenderStatus::Ready;
    }
    const size_t index = wait_index.fetch_add(1, std::memory_order_relaxed);
    if (index < wait_statuses.size()) {
      return wait_statuses[index];
    }
    return detail::WASAPIRenderStatus::NoFrames;
  }

  detail::WASAPIRenderStatus getPadding(uint32_t& padding) noexcept override {
    ++padding_calls;
    padding = configured_padding;
    return padding_status;
  }

  detail::WASAPIRenderStatus acquire(uint32_t, BYTE*& output) noexcept override {
    ++acquire_calls;
    output = reinterpret_cast<BYTE*>(device_buffer.data());
    return acquire_status;
  }

  detail::WASAPIRenderStatus release(uint32_t) noexcept override {
    ++release_calls;
    return release_status;
  }

  detail::WASAPIRenderStatus stop() noexcept override {
    ++stop_calls;
    started = false;
    return stop_status;
  }

  void cleanup() noexcept override {
    ++cleanup_calls;
    initialized = false;
    started = false;
    stop_signaled.store(false, std::memory_order_release);
    wait_index.store(0, std::memory_order_relaxed);
    wait_calls.store(0, std::memory_order_relaxed);
    device_buffer.clear();
  }

  uint32_t bufferFrames() const noexcept override {
    return buffer_frames;
  }
  bool float32() const noexcept override {
    return true;
  }
  uint32_t latencySamples(uint32_t) const noexcept override {
    return 0;
  }

  void resetWaits(std::vector<detail::WASAPIRenderStatus> statuses) {
    wait_statuses = std::move(statuses);
    wait_index.store(0, std::memory_order_relaxed);
    stop_signaled.store(false, std::memory_order_release);
  }

  uint32_t buffer_frames{4};
  uint16_t resolved_output_channels{2};
  uint32_t configured_padding{0};
  SessionGraphError initialize_result{SessionGraphError::OK};
  detail::WASAPIRenderStatus start_status{detail::WASAPIRenderStatus::Ready};
  detail::WASAPIRenderStatus padding_status{detail::WASAPIRenderStatus::Ready};
  detail::WASAPIRenderStatus acquire_status{detail::WASAPIRenderStatus::Ready};
  detail::WASAPIRenderStatus release_status{detail::WASAPIRenderStatus::Ready};
  detail::WASAPIRenderStatus stop_status{detail::WASAPIRenderStatus::Ready};
  std::vector<detail::WASAPIRenderStatus> wait_statuses;
  std::vector<float> device_buffer;
  std::atomic<size_t> wait_index{0};
  std::atomic<size_t> wait_calls{0};
  std::atomic<uint32_t> initialize_calls{0};
  std::atomic<uint32_t> start_calls{0};
  std::atomic<uint32_t> padding_calls{0};
  std::atomic<uint32_t> acquire_calls{0};
  std::atomic<uint32_t> release_calls{0};
  std::atomic<uint32_t> stop_calls{0};
  std::atomic<uint32_t> cleanup_calls{0};
  std::atomic<bool> stop_signaled{false};
  bool initialized{false};
  bool started{false};
};

class CaptureCallback final : public IAudioCallback {
public:
  void processAudio(const AudioProcessBlock& block) noexcept override {
    ++callback_count;
    frames.store(block.num_frames, std::memory_order_release);
    output_channels.store(block.num_output_channels, std::memory_order_release);
    device_position.store(block.device_sample_position, std::memory_order_release);
    host_time.store(block.host_time_nanoseconds, std::memory_order_release);
    discontinuity.store(block.discontinuity, std::memory_order_release);
  }

  std::atomic<uint32_t> callback_count{0};
  std::atomic<uint32_t> frames{0};
  std::atomic<uint32_t> output_channels{0};
  std::atomic<uint64_t> device_position{1};
  std::atomic<uint64_t> host_time{1};
  std::atomic<bool> discontinuity{false};
};

AudioDriverConfig validConfig() {
  AudioDriverConfig config;
  config.sample_rate = 48000;
  config.buffer_size = 4;
  config.num_inputs = 0;
  config.num_outputs = 2;
  return config;
}

void waitUntilStopped(const IAudioDriver& driver) {
  for (size_t attempt = 0; attempt < 10000 && driver.isRunning(); ++attempt) {
    std::this_thread::yield();
  }
  ASSERT_FALSE(driver.isRunning());
}

TEST(WASAPIDriverTest, FactoryReportsTruthfulSharedModeCapabilities) {
  auto driver = createWASAPIAudioDriver();
  ASSERT_NE(driver, nullptr);
  EXPECT_EQ(driver->getDriverName(), "WASAPI");
  const auto capabilities = driver->getCapabilities();
  EXPECT_EQ(capabilities.backend, AudioBackend::WASAPI);
  EXPECT_EQ(capabilities.platform, AudioPlatform::Windows);
  EXPECT_TRUE(capabilities.supports_shared_mode);
  EXPECT_FALSE(capabilities.supports_exclusive_mode);
  EXPECT_FALSE(capabilities.supports_input);
}

TEST(WASAPIDriverTest, RejectsUnsupportedInputBeforeRuntimeMutation) {
  auto fake = std::make_unique<FakeRenderRuntime>();
  auto* runtime = fake.get();
  WASAPIAudioDriver driver(std::move(fake));
  auto config = validConfig();
  config.num_inputs = 1;
  EXPECT_EQ(driver.initialize(config), SessionGraphError::InvalidParameter);
  EXPECT_EQ(runtime->initialize_calls.load(), 0u);

  config = validConfig();
  config.input_device_id = "wasapi:input";
  EXPECT_EQ(driver.initialize(config), SessionGraphError::InvalidParameter);
  EXPECT_EQ(runtime->initialize_calls.load(), 0u);
}

TEST(WASAPIDriverTest, RejectsUnsupportedExplicitOutputEndpointsWithoutFallback) {
  auto driver = createWASAPIAudioDriver();
  ASSERT_NE(driver, nullptr);

  auto config = validConfig();
  config.output_device_id = "unsupported:explicit-endpoint";
  EXPECT_EQ(driver->initialize(config), SessionGraphError::InvalidParameter);

  config.output_device_id = "wasapi:";
  EXPECT_EQ(driver->initialize(config), SessionGraphError::InvalidParameter);
}

TEST(WASAPIDriverTest, RejectsResolvedChannelMismatchBeforeDriverCommit) {
  auto fake = std::make_unique<FakeRenderRuntime>();
  auto* runtime = fake.get();
  runtime->resolved_output_channels = 1;
  WASAPIAudioDriver driver(std::move(fake));

  EXPECT_EQ(driver.initialize(validConfig()), SessionGraphError::InvalidParameter);
  EXPECT_EQ(runtime->initialize_calls.load(), 1u);
  EXPECT_FALSE(driver.isRunning());
  EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::Healthy);
}

TEST(WASAPIDriverTest, PublishesZeroUntilDeviceClockCorrelationExists) {
  auto fake = std::make_unique<FakeRenderRuntime>();
  auto* runtime = fake.get();
  runtime->resetWaits({detail::WASAPIRenderStatus::Ready, detail::WASAPIRenderStatus::NoFrames});
  WASAPIAudioDriver driver(std::move(fake));
  ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);

  CaptureCallback callback;
  ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
  for (size_t attempt = 0; attempt < 10000 && callback.callback_count.load() == 0; ++attempt) {
    std::this_thread::yield();
  }
  ASSERT_EQ(callback.callback_count.load(), 1u);
  EXPECT_EQ(callback.frames.load(), 4u);
  EXPECT_EQ(callback.output_channels.load(), 2u);
  EXPECT_EQ(callback.device_position.load(), 0u);
  EXPECT_EQ(callback.host_time.load(), 0u);
  EXPECT_TRUE(callback.discontinuity.load());
  EXPECT_EQ(driver.stop(), SessionGraphError::OK);
}

TEST(WASAPIDriverTest, TerminalWaitFailureRequiresReinitialize) {
  auto fake = std::make_unique<FakeRenderRuntime>();
  auto* runtime = fake.get();
  runtime->resetWaits({detail::WASAPIRenderStatus::Timeout});
  WASAPIAudioDriver driver(std::move(fake));
  ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);

  CaptureCallback callback;
  ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
  waitUntilStopped(driver);
  EXPECT_EQ(callback.callback_count.load(), 0u);
  EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::BackendFailure);
  EXPECT_EQ(driver.start(&callback), SessionGraphError::NotReady);
  EXPECT_EQ(driver.stop(), SessionGraphError::OK);
  EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::BackendFailure);

  runtime->resetWaits({detail::WASAPIRenderStatus::NoFrames});
  ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);
  EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::Healthy);
}

TEST(WASAPIDriverTest, LauncherExceptionRollsBackToRetryableInitializedState) {
  auto fake = std::make_unique<FakeRenderRuntime>();
  auto* runtime = fake.get();
  runtime->resetWaits({detail::WASAPIRenderStatus::NoFrames});
  std::atomic<uint32_t> launches{0};
  detail::WASAPIWorkerLauncher launcher = [&launches](std::function<void()> worker) {
    if (launches.fetch_add(1, std::memory_order_relaxed) == 0) {
      throw std::runtime_error("injected worker launch failure");
    }
    return std::thread(std::move(worker));
  };
  WASAPIAudioDriver driver(std::move(fake), std::move(launcher));
  ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);

  CaptureCallback callback;
  EXPECT_EQ(driver.start(&callback), SessionGraphError::InternalError);
  EXPECT_FALSE(driver.isRunning());
  EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::Healthy);

  EXPECT_EQ(driver.start(&callback), SessionGraphError::OK);
  EXPECT_EQ(driver.stop(), SessionGraphError::OK);
  EXPECT_EQ(runtime->stop_calls.load(), 2u);
}

TEST(WASAPIDriverTest, LauncherRollbackStopFailureRequiresReinitialize) {
  auto fake = std::make_unique<FakeRenderRuntime>();
  auto* runtime = fake.get();
  runtime->stop_status = detail::WASAPIRenderStatus::BackendFailure;
  detail::WASAPIWorkerLauncher launcher = [](std::function<void()>) -> std::thread {
    throw std::runtime_error("injected worker launch failure");
  };
  WASAPIAudioDriver driver(std::move(fake), std::move(launcher));
  ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);

  CaptureCallback callback;
  EXPECT_EQ(driver.start(&callback), SessionGraphError::InternalError);
  EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::BackendFailure);
  EXPECT_EQ(driver.start(&callback), SessionGraphError::NotReady);

  runtime->stop_status = detail::WASAPIRenderStatus::Ready;
  EXPECT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);
  EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::Healthy);
}

TEST(WASAPIDriverTest, PaddingAcquireAndReleaseFailuresAreTerminal) {
  const std::array<detail::WASAPIRenderStatus, 3> statuses = {
      detail::WASAPIRenderStatus::DeviceInvalidated, detail::WASAPIRenderStatus::BackendFailure,
      detail::WASAPIRenderStatus::BackendFailure};
  for (size_t failure = 0; failure < statuses.size(); ++failure) {
    auto fake = std::make_unique<FakeRenderRuntime>();
    auto* runtime = fake.get();
    runtime->resetWaits({detail::WASAPIRenderStatus::Ready});
    if (failure == 0) {
      runtime->padding_status = statuses[failure];
    } else if (failure == 1) {
      runtime->acquire_status = detail::WASAPIRenderStatus::DeviceInvalidated;
    } else {
      runtime->release_status = detail::WASAPIRenderStatus::BackendFailure;
    }
    WASAPIAudioDriver driver(std::move(fake));
    ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);
    CaptureCallback callback;
    ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
    waitUntilStopped(driver);
    EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::BackendFailure);
    EXPECT_EQ(driver.stop(), SessionGraphError::OK);
  }
}

TEST(WASAPIDriverTest, StopSignalDoesNotAcquireAfterWorkerWake) {
  auto fake = std::make_unique<FakeRenderRuntime>();
  auto* runtime = fake.get();
  runtime->resetWaits({detail::WASAPIRenderStatus::NoFrames});
  WASAPIAudioDriver driver(std::move(fake));
  ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);

  CaptureCallback callback;
  ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
  ASSERT_EQ(driver.stop(), SessionGraphError::OK);
  EXPECT_EQ(runtime->acquire_calls.load(), 0u);
}

TEST(WASAPIDriverTest, ManagerReportsOnlyQueriedEndpointFormats) {
  auto manager = createAudioDriverManager();
  ASSERT_NE(manager, nullptr);
  const auto devices = manager->enumerateDevices();
  ASSERT_FALSE(devices.empty());
  EXPECT_EQ(devices.front().deviceId, "dummy");
  for (const auto& device : devices) {
    if (device.driverType != "WASAPI") {
      continue;
    }
    EXPECT_EQ(device.deviceId.rfind("wasapi:", 0), 0u);
    EXPECT_FALSE(device.name.empty());
    EXPECT_GT(device.maxChannels, 0u);
    EXPECT_FALSE(device.supportedSampleRates.empty());
    EXPECT_FALSE(device.supportedBufferSizes.empty());
  }
}

} // namespace
} // namespace orpheus

#endif
