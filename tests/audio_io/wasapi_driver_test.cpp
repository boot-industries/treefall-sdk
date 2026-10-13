// SPDX-License-Identifier: MIT
#ifdef _WIN32

#include "wasapi_driver.h"

#include <orpheus/audio_driver.h>
#include <orpheus/audio_driver_manager.h>

#include <gtest/gtest.h>
#include <ksmedia.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
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
    initialized = true; // Model partially acquired resources even on initialization failure.
    if (initialize_result != SessionGraphError::OK) {
      return initialize_result;
    }
    resolved.sample_rate = requested.sample_rate;
    resolved.buffer_size = requested.buffer_size;
    resolved.num_outputs = resolved_output_channels;
    resolved.float32 = output_float32;
    auto& shape = resolved.format_shape;
    shape.sample_rate = requested.sample_rate;
    shape.channels = resolved_output_channels;
    shape.bits_per_sample = output_float32 ? 32 : 16;
    shape.block_align = static_cast<uint16_t>(shape.channels * (shape.bits_per_sample / 8));
    shape.average_bytes_per_second = shape.sample_rate * shape.block_align;
    shape.extensible = extensible;
    shape.valid_bits_per_sample = extensible ? shape.bits_per_sample : 0;
    shape.channel_mask = channel_mask;
    shape.encoding =
        output_float32 ? detail::WASAPISampleType::Float32 : detail::WASAPISampleType::Pcm16;
    if (mutate_resolved) {
      mutate_resolved(resolved);
    }
    device_bytes = static_cast<size_t>(resolved_output_channels) * buffer_frames *
                   (output_float32 ? sizeof(float) : sizeof(int16_t));
    device_buffer.assign(device_bytes + 16, std::byte{0});
    std::fill(device_buffer.begin() + device_bytes, device_buffer.end(), std::byte{0x5a});
    {
      std::lock_guard<std::mutex> lock(wait_mutex);
      worker_finished = false;
      wait_blocked = false;
    }
    return SessionGraphError::OK;
  }

  detail::WASAPIRenderStatus start() noexcept override {
    ++start_calls;
    started = start_status == detail::WASAPIRenderStatus::Ready;
    if (started) {
      std::lock_guard<std::mutex> lock(wait_mutex);
      stop_signaled.store(false, std::memory_order_release);
      wait_blocked = false;
      worker_finished = false;
    }
    return start_status;
  }

  void signalStop() noexcept override {
    {
      std::lock_guard<std::mutex> lock(wait_mutex);
      stop_signaled.store(true, std::memory_order_release);
    }
    wait_cv.notify_all();
  }

  detail::WASAPIRenderStatus wait() noexcept override {
    ++wait_calls;
    std::unique_lock<std::mutex> lock(wait_mutex);
    if (stop_signaled.load(std::memory_order_acquire)) {
      return stop_wake_status;
    }
    const size_t index = wait_index.fetch_add(1, std::memory_order_relaxed);
    if (index < wait_statuses.size()) {
      return wait_statuses[index];
    }
    wait_blocked = true;
    wait_cv.notify_all();
    wait_cv.wait(lock, [this] { return stop_signaled.load(std::memory_order_acquire); });
    return stop_wake_status;
  }

  detail::WASAPIRenderStatus getPadding(uint32_t& padding) noexcept override {
    ++padding_calls;
    padding = configured_padding;
    return padding_status;
  }

  detail::WASAPIRenderStatus acquire(uint32_t frames, BYTE*& output) noexcept override {
    ++acquire_calls;
    acquired_frames.store(frames);
    output = null_buffer ? nullptr : reinterpret_cast<BYTE*>(device_buffer.data());
    if (acquire_status == detail::WASAPIRenderStatus::Ready && output != nullptr) {
      buffer_owned.store(true);
    }
    return acquire_status;
  }

  detail::WASAPIRenderStatus release(uint32_t frames) noexcept override {
    ++release_calls;
    released_frames.store(frames);
    if (release_status == detail::WASAPIRenderStatus::Ready) {
      buffer_owned.store(false);
    }
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
    buffer_owned.store(false);
    stop_signaled.store(false, std::memory_order_release);
    wait_index.store(0, std::memory_order_relaxed);
    wait_calls.store(0, std::memory_order_relaxed);
    device_buffer.clear();
  }

  uint32_t bufferFrames() const noexcept override {
    return buffer_frames;
  }
  bool float32() const noexcept override {
    return output_float32;
  }
  uint32_t latencySamples(uint32_t) const noexcept override {
    return 0;
  }

  float floatSample(size_t index) const {
    float sample = 0.0f;
    std::memcpy(&sample, device_buffer.data() + index * sizeof(sample), sizeof(sample));
    return sample;
  }

  bool bufferCanaryIntact() const {
    return device_buffer.size() == device_bytes + 16 &&
           std::all_of(device_buffer.begin() + device_bytes, device_buffer.end(),
                       [](std::byte value) { return value == std::byte{0x5a}; });
  }

  void resetWaits(std::vector<detail::WASAPIRenderStatus> statuses) {
    wait_statuses = std::move(statuses);
    wait_index.store(0, std::memory_order_relaxed);
    stop_signaled.store(false, std::memory_order_release);
  }

  detail::WASAPIWorkerLauncher workerLauncher() {
    return [this](std::function<void()> worker) {
      return std::thread([this, worker = std::move(worker)] {
        worker();
        {
          std::lock_guard<std::mutex> lock(wait_mutex);
          worker_finished = true;
        }
        wait_cv.notify_all();
      });
    };
  }

  bool waitForWorkerFinished() {
    std::unique_lock<std::mutex> lock(wait_mutex);
    return wait_cv.wait_for(lock, std::chrono::seconds(5), [this] { return worker_finished; });
  }

  bool waitForBlockedWait() {
    std::unique_lock<std::mutex> lock(wait_mutex);
    return wait_cv.wait_for(lock, std::chrono::seconds(5), [this] { return wait_blocked; });
  }

  uint32_t buffer_frames{4};
  uint16_t resolved_output_channels{2};
  uint32_t configured_padding{0};
  bool output_float32{true};
  bool extensible{false};
  uint32_t channel_mask{0};
  bool null_buffer{false};
  std::function<void(detail::WASAPIResolvedFormat&)> mutate_resolved;
  SessionGraphError initialize_result{SessionGraphError::OK};
  detail::WASAPIRenderStatus start_status{detail::WASAPIRenderStatus::Ready};
  detail::WASAPIRenderStatus padding_status{detail::WASAPIRenderStatus::Ready};
  detail::WASAPIRenderStatus acquire_status{detail::WASAPIRenderStatus::Ready};
  detail::WASAPIRenderStatus release_status{detail::WASAPIRenderStatus::Ready};
  detail::WASAPIRenderStatus stop_status{detail::WASAPIRenderStatus::Ready};
  detail::WASAPIRenderStatus stop_wake_status{detail::WASAPIRenderStatus::Ready};
  std::vector<detail::WASAPIRenderStatus> wait_statuses;
  std::vector<std::byte> device_buffer;
  size_t device_bytes{0};
  std::atomic<size_t> wait_index{0};
  std::atomic<size_t> wait_calls{0};
  std::atomic<uint32_t> initialize_calls{0};
  std::atomic<uint32_t> start_calls{0};
  std::atomic<uint32_t> padding_calls{0};
  std::atomic<uint32_t> acquire_calls{0};
  std::atomic<uint32_t> release_calls{0};
  std::atomic<uint32_t> acquired_frames{0};
  std::atomic<uint32_t> released_frames{0};
  std::atomic<uint32_t> stop_calls{0};
  std::atomic<uint32_t> cleanup_calls{0};
  std::atomic<bool> stop_signaled{false};
  std::atomic<bool> buffer_owned{false};
  bool initialized{false};
  bool started{false};
  std::mutex wait_mutex;
  std::condition_variable wait_cv;
  bool worker_finished{false};
  bool wait_blocked{false};
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
    if (emit_samples) {
      for (uint32_t frame = 0; frame < block.num_frames; ++frame) {
        for (uint16_t channel = 0; channel < block.num_output_channels; ++channel) {
          block.output_buffers[channel][frame] =
              source[(frame * block.num_output_channels + channel) % source.size()];
        }
      }
    }
  }

  static constexpr std::array<float, 8> source{-1.5f, 1.5f, -0.5f,    0.5f,
                                               -1.0f, 1.0f, -0.0001f, 0.0001f};
  bool emit_samples{false};
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

void waitUntilStopped(const IAudioDriver& driver, FakeRenderRuntime& runtime) {
  ASSERT_TRUE(runtime.waitForWorkerFinished());
  ASSERT_FALSE(driver.isRunning());
}

WAVEFORMATEXTENSIBLE nativeFormat(bool extensible, bool float32, uint16_t channels = 2,
                                  uint32_t mask = 0) {
  WAVEFORMATEXTENSIBLE descriptor{};
  auto& base = descriptor.Format;
  base.wFormatTag =
      extensible ? WAVE_FORMAT_EXTENSIBLE : (float32 ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM);
  base.nChannels = channels;
  base.nSamplesPerSec = 48000;
  base.wBitsPerSample = float32 ? 32 : 16;
  base.nBlockAlign = static_cast<WORD>(channels * (base.wBitsPerSample / 8));
  base.nAvgBytesPerSec = base.nSamplesPerSec * base.nBlockAlign;
  base.cbSize =
      extensible ? static_cast<WORD>(sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) : 0;
  descriptor.Samples.wValidBitsPerSample = base.wBitsPerSample;
  descriptor.dwChannelMask = mask;
  descriptor.SubFormat = float32 ? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT : KSDATAFORMAT_SUBTYPE_PCM;
  return descriptor;
}

size_t descriptorExtent(const WAVEFORMATEXTENSIBLE& descriptor) {
  return sizeof(WAVEFORMATEX) + descriptor.Format.cbSize;
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
  ASSERT_TRUE(runtime->waitForBlockedWait());
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
  WASAPIAudioDriver driver(std::move(fake), runtime->workerLauncher());
  ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);

  CaptureCallback callback;
  ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
  waitUntilStopped(driver, *runtime);
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
  const std::array<detail::WASAPIRenderStatus, 2> statuses = {
      detail::WASAPIRenderStatus::DeviceInvalidated, detail::WASAPIRenderStatus::BackendFailure};
  for (size_t test = 0; test < 3 * statuses.size(); ++test) {
    const size_t failure = test / statuses.size();
    const auto status = statuses[test % statuses.size()];
    SCOPED_TRACE(failure);
    SCOPED_TRACE(static_cast<int>(status));
    auto fake = std::make_unique<FakeRenderRuntime>();
    auto* runtime = fake.get();
    runtime->resetWaits({detail::WASAPIRenderStatus::Ready});
    if (failure == 0) {
      runtime->padding_status = status;
    } else if (failure == 1) {
      runtime->acquire_status = status;
    } else {
      runtime->release_status = status;
    }
    WASAPIAudioDriver driver(std::move(fake), runtime->workerLauncher());
    ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);
    CaptureCallback callback;
    ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
    waitUntilStopped(driver, *runtime);
    EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::BackendFailure);
    EXPECT_EQ(callback.callback_count.load(), failure == 2 ? 1u : 0u);
    EXPECT_EQ(runtime->acquire_calls.load(), failure == 0 ? 0u : 1u);
    EXPECT_EQ(runtime->release_calls.load(), failure == 2 ? 1u : 0u);
    EXPECT_EQ(runtime->buffer_owned.load(), failure == 2);
    EXPECT_EQ(driver.start(&callback), SessionGraphError::NotReady);
    EXPECT_EQ(driver.stop(), SessionGraphError::OK);
    EXPECT_EQ(runtime->stop_calls.load(), 1u);
    const auto callbacks_after_stop = callback.callback_count.load();
    runtime->padding_status = detail::WASAPIRenderStatus::Ready;
    runtime->acquire_status = detail::WASAPIRenderStatus::Ready;
    runtime->release_status = detail::WASAPIRenderStatus::Ready;
    runtime->resetWaits({});
    const auto cleanups = runtime->cleanup_calls.load();
    ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);
    EXPECT_EQ(runtime->cleanup_calls.load(), cleanups + 1);
    EXPECT_FALSE(runtime->buffer_owned.load());
    EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::Healthy);
    EXPECT_EQ(callback.callback_count.load(), callbacks_after_stop);
    ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
    ASSERT_TRUE(runtime->waitForBlockedWait());
    EXPECT_EQ(driver.stop(), SessionGraphError::OK);
    EXPECT_EQ(callback.callback_count.load(), callbacks_after_stop);
  }
}

TEST(WASAPIDriverTest, StopSignalDoesNotAcquireAfterWorkerWake) {
  for (const auto status :
       {detail::WASAPIRenderStatus::Ready, detail::WASAPIRenderStatus::NoFrames,
        detail::WASAPIRenderStatus::Timeout, detail::WASAPIRenderStatus::BackendFailure}) {
    SCOPED_TRACE(static_cast<int>(status));
    const bool terminal = status != detail::WASAPIRenderStatus::Ready &&
                          status != detail::WASAPIRenderStatus::NoFrames;
    auto fake = std::make_unique<FakeRenderRuntime>();
    auto* runtime = fake.get();
    runtime->resetWaits({detail::WASAPIRenderStatus::NoFrames});
    runtime->stop_wake_status = status;
    WASAPIAudioDriver driver(std::move(fake));
    ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);
    CaptureCallback callback;
    ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
    ASSERT_TRUE(runtime->waitForBlockedWait());
    ASSERT_EQ(driver.stop(), SessionGraphError::OK);
    EXPECT_FALSE(driver.isRunning());
    EXPECT_EQ(runtime->stop_calls.load(), 1u);
    EXPECT_EQ(runtime->acquire_calls.load(), 0u);
    EXPECT_EQ(runtime->padding_calls.load(), 0u);
    EXPECT_EQ(runtime->release_calls.load(), 0u);
    EXPECT_EQ(callback.callback_count.load(), 0u);
    EXPECT_EQ(driver.getTelemetry().route_outcome, terminal
                                                       ? AudioRouteRuntimeOutcome::BackendFailure
                                                       : AudioRouteRuntimeOutcome::Healthy);
    runtime->stop_wake_status = detail::WASAPIRenderStatus::Ready;
    if (terminal) {
      EXPECT_EQ(driver.start(&callback), SessionGraphError::NotReady);
      const auto cleanups = runtime->cleanup_calls.load();
      ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);
      EXPECT_EQ(runtime->cleanup_calls.load(), cleanups + 1);
    }
    runtime->resetWaits({});
    ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
    ASSERT_TRUE(runtime->waitForBlockedWait());
    EXPECT_EQ(driver.stop(), SessionGraphError::OK);
    EXPECT_EQ(callback.callback_count.load(), 0u);
  }
}

TEST(WASAPIDriverTest, NativeDescriptorsAcceptPcm16AndFloat32IncludingExtensible) {
  for (bool extensible : {false, true}) {
    for (bool float32 : {false, true}) {
      SCOPED_TRACE(extensible ? "extensible" : "plain");
      SCOPED_TRACE(float32 ? "float32" : "pcm16");
      const auto descriptor = nativeFormat(extensible, float32, 2, extensible ? 3 : 0);
      detail::WASAPIFormatShape shape;
      ASSERT_TRUE(
          detail::validateWASAPINativeFormat(&descriptor, descriptorExtent(descriptor), 2, shape));
      EXPECT_EQ(shape.sample_rate, 48000u);
      EXPECT_EQ(shape.channels, 2u);
      EXPECT_EQ(shape.bits_per_sample, float32 ? 32u : 16u);
      EXPECT_EQ(shape.block_align, float32 ? 8u : 4u);
      EXPECT_EQ(shape.average_bytes_per_second, float32 ? 384000u : 192000u);
      EXPECT_EQ(shape.extensible, extensible);
      EXPECT_EQ(shape.channel_mask, extensible ? 3u : 0u);
      EXPECT_EQ(shape.valid_bits_per_sample, extensible ? shape.bits_per_sample : 0u);
      EXPECT_EQ(shape.encoding,
                float32 ? detail::WASAPISampleType::Float32 : detail::WASAPISampleType::Pcm16);
    }
  }
}

TEST(WASAPIDriverTest, ReducedRequestedCountRejectsCopiedMaskButAllowsUnspecifiedMask) {
  auto descriptor = nativeFormat(true, true, 6, 0x3f);
  // This is the actual negotiation transformation: reduce count/stride without
  // inventing a new speaker map from the original six-channel mix descriptor.
  descriptor.Format.nChannels = 2;
  descriptor.Format.nBlockAlign = 8;
  descriptor.Format.nAvgBytesPerSec = 48000 * 8;
  detail::WASAPIFormatShape shape;
  EXPECT_FALSE(
      detail::validateWASAPINativeFormat(&descriptor, descriptorExtent(descriptor), 2, shape));
  descriptor.dwChannelMask = 0;
  ASSERT_TRUE(
      detail::validateWASAPINativeFormat(&descriptor, descriptorExtent(descriptor), 2, shape));
  EXPECT_EQ(shape.channels, 2u);
  EXPECT_EQ(shape.channel_mask, 0u);

  for (uint32_t mask : {0x3fu, 0u}) {
    auto fake = std::make_unique<FakeRenderRuntime>();
    auto* runtime = fake.get();
    runtime->extensible = true;
    runtime->channel_mask = mask;
    WASAPIAudioDriver driver(std::move(fake));
    const auto result = driver.initialize(validConfig());
    EXPECT_EQ(result, mask == 0 ? SessionGraphError::OK : SessionGraphError::InvalidParameter);
    CaptureCallback callback;
    if (mask != 0) {
      EXPECT_EQ(driver.start(&callback), SessionGraphError::NotReady);
      EXPECT_FALSE(runtime->initialized);
      EXPECT_EQ(runtime->cleanup_calls.load(), 2u);
    } else {
      EXPECT_EQ(driver.getConfig().num_outputs, 2u);
      ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
      ASSERT_TRUE(runtime->waitForBlockedWait());
      EXPECT_EQ(driver.stop(), SessionGraphError::OK);
      EXPECT_EQ(callback.callback_count.load(), 0u);
    }
  }
}

TEST(WASAPIDriverTest, NativeDescriptorsRejectMalformedShapeAndClosestChannelMismatch) {
  struct Case {
    const char* name;
    std::function<void(WAVEFORMATEXTENSIBLE&)> mutate;
  };
  const std::vector<Case> cases{
      {"closest_count_mismatch", [](auto& d) { d = nativeFormat(true, true, 1, 1); }},
      {"mask_count_mismatch", [](auto& d) { d.dwChannelMask = 1; }},
      {"unsupported_pcm_width", [](auto& d) { d.Format.wBitsPerSample = 24; }},
      {"float_width_mismatch",
       [](auto& d) {
         d.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
         d.Format.wBitsPerSample = 16;
       }},
      {"valid_bits_mismatch", [](auto& d) { d.Samples.wValidBitsPerSample = 15; }},
      {"zero_valid_bits", [](auto& d) { d.Samples.wValidBitsPerSample = 0; }},
      {"block_alignment", [](auto& d) { ++d.Format.nBlockAlign; }},
      {"average_bytes", [](auto& d) { ++d.Format.nAvgBytesPerSec; }},
      {"average_overflow",
       [](auto& d) {
         d.Format.nSamplesPerSec = std::numeric_limits<uint32_t>::max();
         d.Format.nAvgBytesPerSec = d.Format.nSamplesPerSec * d.Format.nBlockAlign;
       }},
      {"zero_rate", [](auto& d) { d.Format.nSamplesPerSec = 0; }},
      {"zero_channels", [](auto& d) { d.Format.nChannels = 0; }},
      {"unknown_tag", [](auto& d) { d.Format.wFormatTag = WAVE_FORMAT_ALAW; }},
      {"unknown_subformat", [](auto& d) { d.SubFormat = GUID{}; }}};
  for (const auto& test : cases) {
    SCOPED_TRACE(test.name);
    auto descriptor = nativeFormat(true, false, 2, 3);
    test.mutate(descriptor);
    detail::WASAPIFormatShape shape;
    shape.sample_rate = 123;
    EXPECT_FALSE(
        detail::validateWASAPINativeFormat(&descriptor, descriptorExtent(descriptor), 2, shape));
    EXPECT_EQ(shape.sample_rate, 0u);
    EXPECT_EQ(shape.encoding, detail::WASAPISampleType::Unsupported);
  }
  for (bool float32 : {false, true}) {
    auto descriptor = nativeFormat(false, float32);
    descriptor.Format.wBitsPerSample = float32 ? 16 : 32;
    detail::WASAPIFormatShape shape;
    EXPECT_FALSE(
        detail::validateWASAPINativeFormat(&descriptor, descriptorExtent(descriptor), 2, shape));
  }
}

TEST(WASAPIDriverTest, NativeDescriptorExtentIsCheckedBeforeExtensionReads) {
  const auto descriptor = nativeFormat(true, false, 2, 3);
  detail::WASAPIFormatShape shape;
  EXPECT_FALSE(detail::validateWASAPINativeFormat(nullptr, sizeof(descriptor), 2, shape));
  for (size_t extent : {size_t{0}, sizeof(WAVEFORMATEX) - 1, sizeof(WAVEFORMATEX),
                        sizeof(WAVEFORMATEXTENSIBLE) - 1}) {
    SCOPED_TRACE(extent);
    // Allocate exactly the declared extent, so over-reading is sanitizer-visible.
    std::vector<std::byte> bytes(extent);
    if (extent != 0) {
      std::memcpy(bytes.data(), &descriptor, extent);
    }
    EXPECT_FALSE(detail::validateWASAPINativeFormat(bytes.data(), extent, 2, shape));
  }
  auto truncated_extension = descriptor;
  truncated_extension.Format.cbSize =
      static_cast<WORD>(sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX) - 1);
  EXPECT_FALSE(detail::validateWASAPINativeFormat(&truncated_extension, sizeof(truncated_extension),
                                                  2, shape));
  auto exaggerated_extension = descriptor;
  exaggerated_extension.Format.cbSize = std::numeric_limits<WORD>::max();
  EXPECT_FALSE(detail::validateWASAPINativeFormat(&exaggerated_extension,
                                                  sizeof(exaggerated_extension), 2, shape));
  auto plain = nativeFormat(false, false);
  plain.Format.cbSize = 1;
  EXPECT_FALSE(detail::validateWASAPINativeFormat(&plain, sizeof(WAVEFORMATEX), 2, shape));
  std::array<std::byte, sizeof(WAVEFORMATEXTENSIBLE) + 1> unaligned{};
  std::memcpy(unaligned.data() + 1, &descriptor, sizeof(descriptor));
  ASSERT_TRUE(
      detail::validateWASAPINativeFormat(unaligned.data() + 1, sizeof(descriptor), 2, shape));
  EXPECT_EQ(shape.encoding, detail::WASAPISampleType::Pcm16);
  EXPECT_FALSE(detail::validateWASAPINativeFormat(&descriptor, sizeof(descriptor), 0, shape));
}

TEST(WASAPIDriverTest, ResolvedShapeAndProjectionFailuresCleanUpBeforeAdmission) {
  struct Case {
    const char* name;
    std::function<void(detail::WASAPIResolvedFormat&)> mutate;
  };
  const std::vector<Case> cases{
      {"missing_shape", [](auto& r) { r.format_shape = {}; }},
      {"rate_projection", [](auto& r) { ++r.sample_rate; }},
      {"channel_projection", [](auto& r) { r.format_shape.channels = 1; }},
      {"encoding_projection", [](auto& r) { r.float32 = false; }},
      {"runtime_encoding_projection",
       [](auto& r) {
         r.float32 = false;
         r.format_shape.bits_per_sample = 16;
         r.format_shape.block_align = 4;
         r.format_shape.average_bytes_per_second = 192000;
         r.format_shape.encoding = detail::WASAPISampleType::Pcm16;
       }},
      {"buffer_projection", [](auto& r) { r.buffer_size = 3; }},
      {"zero_buffer", [](auto& r) { r.buffer_size = 0; }},
      {"unsupported_width", [](auto& r) { r.format_shape.bits_per_sample = 24; }},
      {"block_alignment", [](auto& r) { --r.format_shape.block_align; }},
      {"average_bytes", [](auto& r) { --r.format_shape.average_bytes_per_second; }},
      {"extensible_valid_bits",
       [](auto& r) {
         r.format_shape.extensible = true;
         r.format_shape.valid_bits_per_sample = 16;
       }},
      {"extensible_mask", [](auto& r) {
         r.format_shape.extensible = true;
         r.format_shape.valid_bits_per_sample = 32;
         r.format_shape.channel_mask = 1;
       }}};
  for (const auto& test : cases) {
    SCOPED_TRACE(test.name);
    auto fake = std::make_unique<FakeRenderRuntime>();
    auto* runtime = fake.get();
    runtime->mutate_resolved = test.mutate;
    WASAPIAudioDriver driver(std::move(fake));
    EXPECT_EQ(driver.initialize(validConfig()), SessionGraphError::InvalidParameter);
    EXPECT_EQ(runtime->cleanup_calls.load(), 2u);
    EXPECT_FALSE(runtime->initialized);
    EXPECT_FALSE(runtime->buffer_owned.load());
    EXPECT_TRUE(runtime->device_buffer.empty());
    CaptureCallback callback;
    EXPECT_EQ(driver.start(&callback), SessionGraphError::NotReady);
    EXPECT_EQ(runtime->start_calls.load(), 0u);
    EXPECT_EQ(callback.callback_count.load(), 0u);
    runtime->mutate_resolved = {};
    EXPECT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);
    EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::Healthy);
  }
}

TEST(WASAPIDriverTest, CallbackOutputEncodesPcm16AndExtensibleFormats) {
  for (bool extensible : {false, true}) {
    for (bool float32 : {false, true}) {
      SCOPED_TRACE(extensible ? "extensible" : "plain");
      SCOPED_TRACE(float32 ? "float32" : "pcm16");
      auto fake = std::make_unique<FakeRenderRuntime>();
      auto* runtime = fake.get();
      runtime->output_float32 = float32;
      runtime->extensible = extensible;
      runtime->channel_mask = extensible ? 3 : 0;
      runtime->resetWaits({detail::WASAPIRenderStatus::Ready});
      WASAPIAudioDriver driver(std::move(fake));
      ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);
      EXPECT_EQ(runtime->cleanup_calls.load(), 1u);
      CaptureCallback callback;
      callback.emit_samples = true;
      EXPECT_EQ(driver.start(nullptr), SessionGraphError::InvalidParameter);
      ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
      ASSERT_TRUE(
          runtime->waitForBlockedWait()); // Release/conversion completed before observation.
      EXPECT_EQ(driver.start(&callback), SessionGraphError::NotReady);
      EXPECT_EQ(driver.initialize(validConfig()), SessionGraphError::NotReady);
      EXPECT_EQ(runtime->initialize_calls.load(), 1u);
      ASSERT_EQ(callback.callback_count.load(), 1u);
      EXPECT_EQ(callback.frames.load(), 4u);
      EXPECT_EQ(callback.output_channels.load(), 2u);
      ASSERT_EQ(runtime->release_calls.load(), 1u);
      EXPECT_EQ(runtime->acquired_frames.load(), 4u);
      EXPECT_EQ(runtime->released_frames.load(), 4u);
      EXPECT_FALSE(runtime->buffer_owned.load());
      EXPECT_TRUE(runtime->bufferCanaryIntact());
      for (size_t sample = 0; sample < CaptureCallback::source.size(); ++sample) {
        if (float32) {
          EXPECT_FLOAT_EQ(runtime->floatSample(sample), CaptureCallback::source[sample]);
        } else {
          int16_t encoded = 0;
          std::memcpy(&encoded, runtime->device_buffer.data() + sample * sizeof(encoded),
                      sizeof(encoded));
          const float expected = std::clamp(CaptureCallback::source[sample], -1.0f, 1.0f);
          // Half one PCM16 quantization step, with floating-point comparison slack.
          EXPECT_NEAR(static_cast<float>(encoded) / 32767.0f, expected, 0.5001f / 32767.0f);
        }
      }
      EXPECT_EQ(driver.stop(), SessionGraphError::OK);
      EXPECT_FALSE(driver.isRunning());
      EXPECT_EQ(callback.callback_count.load(), 1u);
      EXPECT_EQ(runtime->stop_calls.load(), 1u);
      EXPECT_FALSE(runtime->buffer_owned.load());
      EXPECT_EQ(runtime->cleanup_calls.load(), 1u); // Stop retains the initialized route.
      EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::Healthy);
    }
  }
}

TEST(WASAPIDriverTest, WaitTimeoutFailureAndInvalidationRequireExplicitReinitialize) {
  for (const auto status :
       {detail::WASAPIRenderStatus::Timeout, detail::WASAPIRenderStatus::BackendFailure,
        detail::WASAPIRenderStatus::DeviceInvalidated}) {
    SCOPED_TRACE(static_cast<int>(status));
    auto fake = std::make_unique<FakeRenderRuntime>();
    auto* runtime = fake.get();
    runtime->resetWaits({status});
    WASAPIAudioDriver driver(std::move(fake), runtime->workerLauncher());
    ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);
    CaptureCallback callback;
    ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
    waitUntilStopped(driver, *runtime);
    EXPECT_EQ(runtime->wait_calls.load(), 1u);
    EXPECT_EQ(runtime->padding_calls.load(), 0u);
    EXPECT_EQ(runtime->acquire_calls.load(), 0u);
    EXPECT_EQ(runtime->release_calls.load(), 0u);
    EXPECT_EQ(callback.callback_count.load(), 0u);
    EXPECT_FALSE(runtime->buffer_owned.load());
    EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::BackendFailure);
    EXPECT_EQ(driver.start(&callback), SessionGraphError::NotReady);
    EXPECT_EQ(driver.stop(), SessionGraphError::OK);
    EXPECT_EQ(runtime->stop_calls.load(), 1u);
    EXPECT_EQ(driver.stop(), SessionGraphError::OK);
    EXPECT_EQ(runtime->stop_calls.load(), 1u);
    EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::BackendFailure);
    const auto cleanups = runtime->cleanup_calls.load();
    runtime->resetWaits({detail::WASAPIRenderStatus::NoFrames, detail::WASAPIRenderStatus::Ready});
    ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);
    EXPECT_EQ(runtime->cleanup_calls.load(), cleanups + 1);
    EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::Healthy);
    ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
    ASSERT_TRUE(runtime->waitForBlockedWait());
    EXPECT_EQ(callback.callback_count.load(), 1u);
    EXPECT_TRUE(callback.discontinuity.load());
    EXPECT_EQ(driver.stop(), SessionGraphError::OK);
    EXPECT_EQ(callback.callback_count.load(), 1u);
    EXPECT_EQ(runtime->release_calls.load(), 1u);
  }
}

TEST(WASAPIDriverTest, ZeroBufferFramesRejectsInitializationAndReleasesPartialResources) {
  auto fake = std::make_unique<FakeRenderRuntime>();
  auto* runtime = fake.get();
  runtime->buffer_frames = 0;
  WASAPIAudioDriver driver(std::move(fake));
  EXPECT_EQ(driver.initialize(validConfig()), SessionGraphError::InvalidParameter);
  EXPECT_EQ(runtime->cleanup_calls.load(), 2u);
  EXPECT_FALSE(runtime->initialized);
  EXPECT_TRUE(runtime->device_buffer.empty());
  CaptureCallback callback;
  EXPECT_EQ(driver.start(&callback), SessionGraphError::NotReady);
  EXPECT_EQ(runtime->start_calls.load(), 0u);
  runtime->buffer_frames = 4;
  ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);
  EXPECT_EQ(driver.getConfig().buffer_size, 4u);
}

TEST(WASAPIDriverTest, MalformedPaddingAndNullAcquiredBuffersAreTerminal) {
  for (unsigned failure = 0; failure < 3; ++failure) {
    SCOPED_TRACE(failure);
    auto fake = std::make_unique<FakeRenderRuntime>();
    auto* runtime = fake.get();
    runtime->resetWaits({detail::WASAPIRenderStatus::Ready});
    if (failure < 2) {
      runtime->configured_padding = runtime->buffer_frames + failure;
    } else {
      runtime->null_buffer = true;
    }
    WASAPIAudioDriver driver(std::move(fake), runtime->workerLauncher());
    ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);
    CaptureCallback callback;
    ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
    waitUntilStopped(driver, *runtime);
    EXPECT_EQ(callback.callback_count.load(), 0u);
    EXPECT_EQ(runtime->padding_calls.load(), 1u);
    EXPECT_EQ(runtime->acquire_calls.load(), failure == 2 ? 1u : 0u);
    EXPECT_EQ(runtime->release_calls.load(), 0u);
    EXPECT_FALSE(runtime->buffer_owned.load());
    EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::BackendFailure);
    EXPECT_EQ(driver.start(&callback), SessionGraphError::NotReady);
    EXPECT_EQ(driver.stop(), SessionGraphError::OK);
    EXPECT_EQ(runtime->stop_calls.load(), 1u);
    runtime->null_buffer = false;
    runtime->configured_padding = 0;
    runtime->resetWaits({});
    const auto cleanups = runtime->cleanup_calls.load();
    ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);
    EXPECT_EQ(runtime->cleanup_calls.load(), cleanups + 1);
    ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
    ASSERT_TRUE(runtime->waitForBlockedWait());
    EXPECT_EQ(driver.stop(), SessionGraphError::OK);
    EXPECT_EQ(callback.callback_count.load(), 0u);
  }
}

TEST(WASAPIDriverTest, PaddingNoFramesSkipsAcquireAndPartialPaddingRendersOnlyAvailableFrames) {
  for (bool no_frames : {false, true}) {
    SCOPED_TRACE(no_frames);
    auto fake = std::make_unique<FakeRenderRuntime>();
    auto* runtime = fake.get();
    runtime->resetWaits({detail::WASAPIRenderStatus::Ready});
    runtime->configured_padding = 1;
    runtime->padding_status =
        no_frames ? detail::WASAPIRenderStatus::NoFrames : detail::WASAPIRenderStatus::Ready;
    WASAPIAudioDriver driver(std::move(fake));
    ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);
    CaptureCallback callback;
    callback.emit_samples = true;
    ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
    ASSERT_TRUE(runtime->waitForBlockedWait());
    EXPECT_TRUE(driver.isRunning());
    EXPECT_EQ(runtime->acquire_calls.load(), no_frames ? 0u : 1u);
    EXPECT_EQ(runtime->release_calls.load(), no_frames ? 0u : 1u);
    EXPECT_EQ(callback.callback_count.load(), no_frames ? 0u : 1u);
    if (!no_frames) {
      EXPECT_EQ(callback.frames.load(), 3u);
      EXPECT_EQ(runtime->acquired_frames.load(), 3u);
      EXPECT_EQ(runtime->released_frames.load(), 3u);
      for (size_t sample = 0; sample < 6; ++sample) {
        EXPECT_FLOAT_EQ(runtime->floatSample(sample), CaptureCallback::source[sample]);
      }
      EXPECT_FLOAT_EQ(runtime->floatSample(6), 0.0f);
      EXPECT_FLOAT_EQ(runtime->floatSample(7), 0.0f);
      EXPECT_TRUE(runtime->bufferCanaryIntact());
    }
    const auto callbacks = callback.callback_count.load();
    EXPECT_EQ(driver.stop(), SessionGraphError::OK);
    EXPECT_EQ(callback.callback_count.load(), callbacks);
    EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::Healthy);
  }
}

TEST(WASAPIDriverTest, StopBackendFailureRejectsRestartUntilCleanReinitialize) {
  for (const auto status : {detail::WASAPIRenderStatus::BackendFailure,
                            detail::WASAPIRenderStatus::DeviceInvalidated}) {
    SCOPED_TRACE(static_cast<int>(status));
    auto fake = std::make_unique<FakeRenderRuntime>();
    auto* runtime = fake.get();
    runtime->stop_status = status;
    WASAPIAudioDriver driver(std::move(fake));
    ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);
    CaptureCallback callback;
    ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
    ASSERT_TRUE(runtime->waitForBlockedWait());
    EXPECT_EQ(driver.stop(), SessionGraphError::InternalError);
    EXPECT_FALSE(driver.isRunning());
    EXPECT_FALSE(runtime->started);
    EXPECT_EQ(runtime->stop_calls.load(), 1u);
    EXPECT_EQ(runtime->acquire_calls.load(), 0u);
    EXPECT_EQ(callback.callback_count.load(), 0u);
    EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::BackendFailure);
    EXPECT_EQ(driver.start(&callback), SessionGraphError::NotReady);
    runtime->stop_status = detail::WASAPIRenderStatus::Ready;
    const auto cleanups = runtime->cleanup_calls.load();
    ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);
    EXPECT_EQ(runtime->cleanup_calls.load(), cleanups + 1);
    ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
    ASSERT_TRUE(runtime->waitForBlockedWait());
    EXPECT_EQ(driver.stop(), SessionGraphError::OK);
    EXPECT_EQ(callback.callback_count.load(), 0u);
    EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::Healthy);
  }
}

TEST(WASAPIDriverTest, InitializationFailureCleansPartialResourcesAndPermitsRetry) {
  for (const auto error : {SessionGraphError::NotReady, SessionGraphError::InternalError,
                           SessionGraphError::InvalidParameter}) {
    SCOPED_TRACE(static_cast<int>(error));
    auto fake = std::make_unique<FakeRenderRuntime>();
    auto* runtime = fake.get();
    runtime->initialize_result = error;
    WASAPIAudioDriver driver(std::move(fake));
    EXPECT_EQ(driver.initialize(validConfig()), error);
    EXPECT_EQ(runtime->initialize_calls.load(), 1u);
    EXPECT_EQ(runtime->cleanup_calls.load(), 2u);
    EXPECT_FALSE(runtime->initialized);
    EXPECT_FALSE(runtime->buffer_owned.load());
    CaptureCallback callback;
    EXPECT_EQ(driver.start(&callback), SessionGraphError::NotReady);
    EXPECT_EQ(runtime->start_calls.load(), 0u);
    runtime->initialize_result = SessionGraphError::OK;
    ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);
    ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
    ASSERT_TRUE(runtime->waitForBlockedWait());
    EXPECT_EQ(driver.stop(), SessionGraphError::OK);
    EXPECT_EQ(callback.callback_count.load(), 0u);
  }
}

TEST(WASAPIDriverTest, StartFailureRollbackControlsRetryAdmissionWithoutDispatch) {
  for (bool rollback_fails : {false, true}) {
    SCOPED_TRACE(rollback_fails);
    auto fake = std::make_unique<FakeRenderRuntime>();
    auto* runtime = fake.get();
    runtime->start_status = detail::WASAPIRenderStatus::BackendFailure;
    runtime->stop_status = rollback_fails ? detail::WASAPIRenderStatus::BackendFailure
                                          : detail::WASAPIRenderStatus::Ready;
    WASAPIAudioDriver driver(std::move(fake));
    ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);
    CaptureCallback callback;
    EXPECT_EQ(driver.start(&callback), SessionGraphError::InternalError);
    EXPECT_FALSE(driver.isRunning());
    EXPECT_EQ(runtime->stop_calls.load(), 1u);
    EXPECT_EQ(runtime->wait_calls.load(), 0u);
    EXPECT_EQ(callback.callback_count.load(), 0u);
    EXPECT_EQ(driver.getTelemetry().route_outcome, rollback_fails
                                                       ? AudioRouteRuntimeOutcome::BackendFailure
                                                       : AudioRouteRuntimeOutcome::Healthy);
    runtime->start_status = detail::WASAPIRenderStatus::Ready;
    runtime->stop_status = detail::WASAPIRenderStatus::Ready;
    if (rollback_fails) {
      EXPECT_EQ(driver.start(&callback), SessionGraphError::NotReady);
      const auto cleanups = runtime->cleanup_calls.load();
      ASSERT_EQ(driver.initialize(validConfig()), SessionGraphError::OK);
      EXPECT_EQ(runtime->cleanup_calls.load(), cleanups + 1);
    }
    ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
    ASSERT_TRUE(runtime->waitForBlockedWait());
    EXPECT_EQ(driver.stop(), SessionGraphError::OK);
    EXPECT_EQ(callback.callback_count.load(), 0u);
    EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::Healthy);
  }
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
