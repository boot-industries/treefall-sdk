// SPDX-License-Identifier: MIT
#include <orpheus/audio_driver.h>
#include <orpheus/audio_driver_manager.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <optional>
#include <thread>
#include <utility>

using namespace orpheus;

namespace orpheus::detail {
std::unique_ptr<IAudioDriverManager> createAudioDriverManagerForTesting(
    std::function<std::unique_ptr<IAudioDriver>(const std::string&)> factory);
} // namespace orpheus::detail

namespace {

constexpr auto kControlWatchdog = std::chrono::seconds(2);

struct ManagerState {
  std::optional<std::string> device;
  uint32_t sample_rate;
  uint32_t buffer_size;
  IAudioDriver* driver;
};

ManagerState readManagerState(IAudioDriverManager& manager) {
  return {manager.getCurrentDevice(), manager.getCurrentSampleRate(),
          manager.getCurrentBufferSize(), manager.getActiveDriver()};
}

void expectManagerState(const ManagerState& state, IAudioDriver* driver, uint32_t sample_rate,
                        uint32_t buffer_size) {
  EXPECT_EQ(state.device, std::optional<std::string>("dummy"));
  EXPECT_EQ(state.sample_rate, sample_rate);
  EXPECT_EQ(state.buffer_size, buffer_size);
  EXPECT_EQ(state.driver, driver);
}

// The operation remains paused until the test has observed its getters. The
// watchdog detects a lock regression; it is not a control-latency specification.
class ControlPause {
public:
  ControlPause() : entered(m_entered.get_future()), m_releaseFuture(m_release.get_future()) {}

  void block() {
    m_entered.set_value();
    m_releaseFuture.wait();
  }

  void release() {
    m_release.set_value();
  }

private:
  std::promise<void> m_entered;
  std::promise<void> m_release;

public:
  std::future<void> entered;

private:
  std::future<void> m_releaseFuture;
};

struct InjectedDriverState {
  SessionGraphError initialize_result = SessionGraphError::OK;
  SessionGraphError stop_result = SessionGraphError::OK;
  std::function<void()> initialize_hook;
  std::function<void()> configuration_hook;
  std::function<void()> stop_hook;
  std::function<void()> destruction_hook;
  std::atomic<int> initialize_calls{0};
  std::atomic<int> stop_calls{0};
  std::atomic<int> destruction_calls{0};
  std::atomic<bool> running{false};
};

class InjectedDriver final : public IAudioDriver {
public:
  explicit InjectedDriver(std::shared_ptr<InjectedDriverState> state) : m_state(std::move(state)) {}

  ~InjectedDriver() override {
    ++m_state->destruction_calls;
    runHook(m_state->destruction_hook);
  }

  SessionGraphError initialize(const AudioDriverConfig& config) override {
    m_config = config;
    ++m_state->initialize_calls;
    runHook(m_state->initialize_hook);
    return m_state->initialize_result;
  }

  SessionGraphError start(IAudioCallback*) override {
    m_state->running = true;
    return SessionGraphError::OK;
  }

  SessionGraphError stop() override {
    ++m_state->stop_calls;
    // A stop failure can still have made the old backend unusable.
    m_state->running = false;
    runHook(m_state->stop_hook);
    return m_state->stop_result;
  }

  bool isRunning() const override {
    return m_state->running.load();
  }

  const AudioDriverConfig& getConfig() const override {
    runHook(m_state->configuration_hook);
    return m_config;
  }

  std::string getDriverName() const override {
    return "Injected";
  }

  uint32_t getLatencySamples() const override {
    return 0;
  }

private:
  static void runHook(std::function<void()>& stored_hook) {
    // Each pause/hook is one-shot, including the later destructor stop.
    auto hook = std::move(stored_hook);
    if (hook) {
      hook();
    }
  }

  std::shared_ptr<InjectedDriverState> m_state;
  AudioDriverConfig m_config;
};

enum class PausedOperation { Factory, Initialize, Configuration, Stop };
enum class TransitionResult { Success, FactoryFailure, InitializeFailure, StopFailure };

struct TransitionCase {
  PausedOperation operation;
  TransitionResult result;
  const char* name;
};

class AudioDriverManagerTransitionTest : public ::testing::TestWithParam<TransitionCase> {};

} // namespace

/// Test fixture for AudioDriverManager tests
class AudioDriverManagerTest : public ::testing::Test {
protected:
  void SetUp() override {
    manager = createAudioDriverManager();
    ASSERT_NE(manager, nullptr) << "Failed to create AudioDriverManager";
  }

  void TearDown() override {
    manager.reset();
  }

  std::unique_ptr<IAudioDriverManager> manager;
};

/// Test: Device enumeration returns at least the dummy driver
TEST_F(AudioDriverManagerTest, EnumerateDevices_IncludesDummyDriver) {
  auto devices = manager->enumerateDevices();

  ASSERT_FALSE(devices.empty()) << "enumerateDevices() should return at least one device";

  // First device should always be dummy driver
  EXPECT_EQ(devices[0].deviceId, "dummy");
  EXPECT_EQ(devices[0].name, "Dummy Audio Driver");
  EXPECT_EQ(devices[0].driverType, "Dummy");
  EXPECT_FALSE(devices[0].isDefaultDevice) << "Dummy driver should not be default";
}

TEST_F(AudioDriverManagerTest, EndpointCapabilitiesExposeStableDirectionalChannels) {
  const auto endpoints = manager->enumerateEndpointCapabilities();
  ASSERT_FALSE(endpoints.empty());
  ASSERT_EQ(endpoints.front().device_id, "dummy");
  EXPECT_EQ(endpoints.front().availability, AudioEndpointAvailability::Available);
  EXPECT_FALSE(endpoints.front().supports_input);
  EXPECT_TRUE(endpoints.front().supports_output);
  ASSERT_EQ(endpoints.front().output_channels.size(), 32u);
  EXPECT_EQ(endpoints.front().output_channels[2].stable_id, "dummy:output:2");
  EXPECT_EQ(endpoints.front().output_channels[2].device_index, 2u);
  EXPECT_EQ(endpoints.front().output_channels[2].display_name, "Dummy Output 3");

  const auto dummy = manager->getEndpointCapabilities("dummy");
  ASSERT_TRUE(dummy.has_value());
  EXPECT_EQ(dummy->output_channels.size(), 32u);
  EXPECT_FALSE(manager->getEndpointCapabilities("missing-endpoint").has_value());
}

/// Test: Device enumeration returns valid device info
TEST_F(AudioDriverManagerTest, EnumerateDevices_ValidDeviceInfo) {
  auto devices = manager->enumerateDevices();

  for (const auto& device : devices) {
    // All devices should have non-empty ID
    EXPECT_FALSE(device.deviceId.empty()) << "Device ID should not be empty";

    // All devices should have non-empty name
    EXPECT_FALSE(device.name.empty()) << "Device name should not be empty";

    // All devices should have valid driver type
    EXPECT_FALSE(device.driverType.empty()) << "Driver type should not be empty";

    // All devices should have at least 2 channels (stereo)
    EXPECT_GE(device.minChannels, 2u) << "Min channels should be >= 2";
    EXPECT_GE(device.maxChannels, device.minChannels) << "Max channels should be >= min channels";

    // All devices should support at least one sample rate
    EXPECT_FALSE(device.supportedSampleRates.empty())
        << "Device should support at least one sample rate";

    // All devices should support at least one buffer size
    EXPECT_FALSE(device.supportedBufferSizes.empty())
        << "Device should support at least one buffer size";
  }
}

/// Test: Get device info for dummy driver
TEST_F(AudioDriverManagerTest, GetDeviceInfo_DummyDriver) {
  auto deviceInfo = manager->getDeviceInfo("dummy");

  ASSERT_TRUE(deviceInfo.has_value()) << "Dummy driver should always be available";
  EXPECT_EQ(deviceInfo->deviceId, "dummy");
  EXPECT_EQ(deviceInfo->name, "Dummy Audio Driver");
  EXPECT_EQ(deviceInfo->driverType, "Dummy");

  // Verify supported sample rates
  EXPECT_FALSE(deviceInfo->supportedSampleRates.empty());
  EXPECT_TRUE(std::find(deviceInfo->supportedSampleRates.begin(),
                        deviceInfo->supportedSampleRates.end(),
                        48000) != deviceInfo->supportedSampleRates.end())
      << "Dummy driver should support 48000 Hz";

  // Verify supported buffer sizes
  EXPECT_FALSE(deviceInfo->supportedBufferSizes.empty());
  EXPECT_TRUE(std::find(deviceInfo->supportedBufferSizes.begin(),
                        deviceInfo->supportedBufferSizes.end(),
                        512) != deviceInfo->supportedBufferSizes.end())
      << "Dummy driver should support 512 buffer size";
}

/// Test: Get device info for invalid device ID
TEST_F(AudioDriverManagerTest, GetDeviceInfo_InvalidDeviceId) {
  auto deviceInfo = manager->getDeviceInfo("invalid_device_id_12345");

  EXPECT_FALSE(deviceInfo.has_value()) << "Invalid device ID should return std::nullopt";
}

/// Test: Set active device to dummy driver
TEST_F(AudioDriverManagerTest, SetActiveDevice_DummyDriver) {
  SessionGraphError result = manager->setActiveDevice("dummy", 48000, 512);

  EXPECT_EQ(result, SessionGraphError::OK) << "setActiveDevice should succeed for dummy driver";

  // Verify state
  auto currentDevice = manager->getCurrentDevice();
  ASSERT_TRUE(currentDevice.has_value()) << "Current device should be set";
  EXPECT_EQ(currentDevice.value(), "dummy");

  EXPECT_EQ(manager->getCurrentSampleRate(), 48000u);
  EXPECT_EQ(manager->getCurrentBufferSize(), 512u);

  // Verify driver is active
  auto* driver = manager->getActiveDriver();
  EXPECT_NE(driver, nullptr) << "Active driver should not be nullptr";
}

/// Test: Set active device with invalid device ID
TEST_F(AudioDriverManagerTest, SetActiveDevice_InvalidDeviceId) {
  SessionGraphError result = manager->setActiveDevice("invalid_device_id", 48000, 512);

  EXPECT_EQ(result, SessionGraphError::InvalidParameter)
      << "setActiveDevice should fail for invalid device ID";

  // State should remain unchanged
  auto currentDevice = manager->getCurrentDevice();
  EXPECT_FALSE(currentDevice.has_value()) << "Current device should not be set after failure";
}

/// Test: Set active device with invalid sample rate
TEST_F(AudioDriverManagerTest, SetActiveDevice_InvalidSampleRate) {
  SessionGraphError result = manager->setActiveDevice("dummy", 12345, 512);

  EXPECT_EQ(result, SessionGraphError::InvalidParameter)
      << "setActiveDevice should fail for unsupported sample rate";
}

/// Test: Set active device with invalid buffer size
TEST_F(AudioDriverManagerTest, SetActiveDevice_InvalidBufferSize) {
  SessionGraphError result = manager->setActiveDevice("dummy", 48000, 12345);

  EXPECT_EQ(result, SessionGraphError::InvalidParameter)
      << "setActiveDevice should fail for unsupported buffer size";
}

/// Test: Get current device when no device is active
TEST_F(AudioDriverManagerTest, GetCurrentDevice_NoDeviceActive) {
  auto currentDevice = manager->getCurrentDevice();

  EXPECT_FALSE(currentDevice.has_value()) << "No device should be active initially";
}

/// Test: Get current sample rate returns default
TEST_F(AudioDriverManagerTest, GetCurrentSampleRate_Default) {
  uint32_t sampleRate = manager->getCurrentSampleRate();

  EXPECT_EQ(sampleRate, 48000u) << "Default sample rate should be 48000 Hz";
}

/// Test: Get current buffer size returns default
TEST_F(AudioDriverManagerTest, GetCurrentBufferSize_Default) {
  uint32_t bufferSize = manager->getCurrentBufferSize();

  EXPECT_EQ(bufferSize, 512u) << "Default buffer size should be 512 frames";
}

/// Test: Get active driver when no device is active
TEST_F(AudioDriverManagerTest, GetActiveDriver_NoDeviceActive) {
  auto* driver = manager->getActiveDriver();

  EXPECT_EQ(driver, nullptr) << "No driver should be active initially";
}

/// Test: Device change callback registration
TEST_F(AudioDriverManagerTest, SetDeviceChangeCallback_CallbackFires) {
  bool callbackFired = false;
  manager->setDeviceChangeCallback([&callbackFired]() { callbackFired = true; });

  // Trigger device change by setting active device
  SessionGraphError result = manager->setActiveDevice("dummy", 48000, 512);
  ASSERT_EQ(result, SessionGraphError::OK);

  EXPECT_TRUE(callbackFired) << "Device change callback should fire after setActiveDevice";
}

/// Test: Unregister device change callback
TEST_F(AudioDriverManagerTest, SetDeviceChangeCallback_Unregister) {
  bool callbackFired = false;

  // Register callback
  manager->setDeviceChangeCallback([&callbackFired]() { callbackFired = true; });

  // Unregister callback
  manager->setDeviceChangeCallback(nullptr);

  // Trigger device change
  SessionGraphError result = manager->setActiveDevice("dummy", 48000, 512);
  ASSERT_EQ(result, SessionGraphError::OK);

  EXPECT_FALSE(callbackFired) << "Callback should not fire after unregister";
}

/// Test: Hot-swap devices (dummy -> dummy with different settings)
TEST_F(AudioDriverManagerTest, HotSwap_ChangeSampleRate) {
  // Set initial device
  SessionGraphError result = manager->setActiveDevice("dummy", 48000, 512);
  ASSERT_EQ(result, SessionGraphError::OK);

  // Hot-swap to different sample rate
  result = manager->setActiveDevice("dummy", 44100, 512);
  ASSERT_EQ(result, SessionGraphError::OK);

  // Verify new state
  EXPECT_EQ(manager->getCurrentSampleRate(), 44100u);
  EXPECT_EQ(manager->getCurrentBufferSize(), 512u);

  auto* driver = manager->getActiveDriver();
  EXPECT_NE(driver, nullptr) << "Driver should still be active after hot-swap";
}

/// Test: Hot-swap devices (dummy -> dummy with different buffer size)
TEST_F(AudioDriverManagerTest, HotSwap_ChangeBufferSize) {
  // Set initial device
  SessionGraphError result = manager->setActiveDevice("dummy", 48000, 512);
  ASSERT_EQ(result, SessionGraphError::OK);

  // Hot-swap to different buffer size
  result = manager->setActiveDevice("dummy", 48000, 256);
  ASSERT_EQ(result, SessionGraphError::OK);

  // Verify new state
  EXPECT_EQ(manager->getCurrentSampleRate(), 48000u);
  EXPECT_EQ(manager->getCurrentBufferSize(), 256u);
}

/// Test: Enumerate devices returns at least one device on all platforms
TEST_F(AudioDriverManagerTest, EnumerateDevices_AtLeastOneDevice) {
  auto devices = manager->enumerateDevices();

  EXPECT_GE(devices.size(), 1u) << "At least one device (dummy) should be available";
}

/// Test: Dummy driver supports common sample rates
TEST_F(AudioDriverManagerTest, DummyDriver_SupportsCommonSampleRates) {
  auto deviceInfo = manager->getDeviceInfo("dummy");
  ASSERT_TRUE(deviceInfo.has_value());

  const std::vector<uint32_t> commonRates = {44100, 48000, 96000};
  for (uint32_t rate : commonRates) {
    EXPECT_TRUE(std::find(deviceInfo->supportedSampleRates.begin(),
                          deviceInfo->supportedSampleRates.end(),
                          rate) != deviceInfo->supportedSampleRates.end())
        << "Dummy driver should support " << rate << " Hz";
  }
}

/// Test: Dummy driver supports common buffer sizes
TEST_F(AudioDriverManagerTest, DummyDriver_SupportsCommonBufferSizes) {
  auto deviceInfo = manager->getDeviceInfo("dummy");
  ASSERT_TRUE(deviceInfo.has_value());

  const std::vector<uint32_t> commonSizes = {128, 256, 512, 1024};
  for (uint32_t size : commonSizes) {
    EXPECT_TRUE(std::find(deviceInfo->supportedBufferSizes.begin(),
                          deviceInfo->supportedBufferSizes.end(),
                          size) != deviceInfo->supportedBufferSizes.end())
        << "Dummy driver should support buffer size " << size;
  }
}

/// Test: Thread safety - concurrent getCurrentDevice() calls
TEST_F(AudioDriverManagerTest, ThreadSafety_ConcurrentGetCurrentDevice) {
  // Set a device first
  manager->setActiveDevice("dummy", 48000, 512);

  // Launch multiple threads reading current device
  std::atomic<int> successCount{0};
  std::vector<std::thread> threads;

  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([this, &successCount]() {
      for (int j = 0; j < 100; ++j) {
        auto device = manager->getCurrentDevice();
        if (device.has_value() && device.value() == "dummy") {
          successCount++;
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(successCount.load(), 1000) << "All concurrent reads should succeed";
}

TEST_P(AudioDriverManagerTransitionTest, PausedOperationsAllowConcurrentAndReentrantGetters) {
  const auto test_case = GetParam();
  auto old_state = std::make_shared<InjectedDriverState>();
  auto candidate_state = std::make_shared<InjectedDriverState>();
  std::function<void()> factory_hook;
  int creations = 0;
  auto manager = detail::createAudioDriverManagerForTesting(
      [&](const std::string&) -> std::unique_ptr<IAudioDriver> {
        if (++creations == 1) {
          return std::make_unique<InjectedDriver>(old_state);
        }
        if (factory_hook) {
          auto hook = std::move(factory_hook);
          hook();
        }
        if (test_case.result == TransitionResult::FactoryFailure) {
          return nullptr;
        }
        return std::make_unique<InjectedDriver>(candidate_state);
      });
  ASSERT_EQ(manager->setActiveDevice("dummy", 48000, 512), SessionGraphError::OK);
  auto* old_driver = manager->getActiveDriver();
  ASSERT_NE(old_driver, nullptr);
  ASSERT_EQ(old_driver->start(nullptr), SessionGraphError::OK);

  ControlPause pause;
  std::atomic<bool> allow_reentry{false};
  std::optional<ManagerState> reentrant_state;
  auto paused_hook = [&] {
    pause.block();
    // On a lock regression the external getter times out; skip a same-thread
    // deadlock and let the failed test release and join its controlled work.
    if (allow_reentry.load()) {
      reentrant_state = readManagerState(*manager);
    }
  };
  switch (test_case.operation) {
  case PausedOperation::Factory:
    factory_hook = paused_hook;
    break;
  case PausedOperation::Initialize:
    candidate_state->initialize_hook = paused_hook;
    break;
  case PausedOperation::Configuration:
    candidate_state->configuration_hook = paused_hook;
    break;
  case PausedOperation::Stop:
    old_state->stop_hook = paused_hook;
    break;
  }
  if (test_case.result == TransitionResult::InitializeFailure) {
    candidate_state->initialize_result = SessionGraphError::NotReady;
  }
  if (test_case.result == TransitionResult::StopFailure) {
    old_state->stop_result = SessionGraphError::InternalError;
  }
  int notifications = 0;
  std::optional<ManagerState> notified_state;
  manager->setDeviceChangeCallback([&] {
    ++notifications;
    notified_state = readManagerState(*manager);
  });

  auto transition =
      std::async(std::launch::async, [&] { return manager->setActiveDevice("dummy", 44100, 256); });
  const auto entered = pause.entered.wait_for(kControlWatchdog);
  auto getters = std::async(std::launch::async, [&] { return readManagerState(*manager); });
  const auto getters_ready = getters.wait_for(kControlWatchdog);
  allow_reentry =
      entered == std::future_status::ready && getters_ready == std::future_status::ready;
  pause.release();
  const auto sampled_state = getters.get();
  const auto result = transition.get();

  EXPECT_EQ(entered, std::future_status::ready);
  EXPECT_EQ(getters_ready, std::future_status::ready)
      << "State getters must complete before the backend operation is released";
  expectManagerState(sampled_state, old_driver, 48000, 512);
  ASSERT_TRUE(reentrant_state.has_value());
  expectManagerState(*reentrant_state, old_driver, 48000, 512);

  if (test_case.result == TransitionResult::Success) {
    EXPECT_EQ(result, SessionGraphError::OK);
    auto* new_driver = manager->getActiveDriver();
    EXPECT_NE(new_driver, old_driver);
    ASSERT_TRUE(notified_state.has_value());
    expectManagerState(*notified_state, new_driver, 44100, 256);
    expectManagerState(readManagerState(*manager), new_driver, 44100, 256);
    EXPECT_EQ(notifications, 1);
    EXPECT_EQ(old_state->stop_calls.load(), 1);
    EXPECT_EQ(old_state->destruction_calls.load(), 1);
    EXPECT_EQ(candidate_state->destruction_calls.load(), 0);
  } else {
    const auto expected_result = test_case.result == TransitionResult::InitializeFailure
                                     ? SessionGraphError::NotReady
                                     : SessionGraphError::InternalError;
    EXPECT_EQ(result, expected_result);
    expectManagerState(readManagerState(*manager), old_driver, 48000, 512);
    EXPECT_EQ(notifications, 0);
    EXPECT_EQ(old_state->destruction_calls.load(), 0);
    if (test_case.result == TransitionResult::StopFailure) {
      EXPECT_EQ(old_state->stop_calls.load(), 1);
      EXPECT_FALSE(old_driver->isRunning());
    } else {
      EXPECT_EQ(old_state->stop_calls.load(), 0);
      EXPECT_TRUE(old_driver->isRunning());
    }
    EXPECT_EQ(candidate_state->destruction_calls.load(),
              test_case.result == TransitionResult::FactoryFailure ? 0 : 1);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ControlledTransitions, AudioDriverManagerTransitionTest,
    ::testing::Values(
        TransitionCase{PausedOperation::Factory, TransitionResult::Success, "Factory"},
        TransitionCase{PausedOperation::Initialize, TransitionResult::Success, "Initialize"},
        TransitionCase{PausedOperation::Configuration, TransitionResult::Success, "Configuration"},
        TransitionCase{PausedOperation::Stop, TransitionResult::Success, "OldStop"},
        TransitionCase{PausedOperation::Factory, TransitionResult::FactoryFailure,
                       "FactoryFailure"},
        TransitionCase{PausedOperation::Initialize, TransitionResult::InitializeFailure,
                       "InitializeFailure"},
        TransitionCase{PausedOperation::Stop, TransitionResult::StopFailure, "OldStopFailure"}),
    [](const ::testing::TestParamInfo<TransitionCase>& info) { return info.param.name; });

TEST(AudioDriverManagerInjectedTest, PausedNotificationDoesNotBlockGettersOrAnotherTransition) {
  auto manager = detail::createAudioDriverManagerForTesting([](const std::string&) {
    return std::make_unique<InjectedDriver>(std::make_shared<InjectedDriverState>());
  });
  ASSERT_EQ(manager->setActiveDevice("dummy", 48000, 512), SessionGraphError::OK);

  ControlPause notification_pause;
  std::atomic<int> notifications{0};
  manager->setDeviceChangeCallback([&] {
    if (++notifications == 1) {
      notification_pause.block();
    }
  });
  auto first =
      std::async(std::launch::async, [&] { return manager->setActiveDevice("dummy", 44100, 256); });
  const auto entered = notification_pause.entered.wait_for(kControlWatchdog);
  auto getters = std::async(std::launch::async, [&] { return readManagerState(*manager); });
  const auto getters_ready = getters.wait_for(kControlWatchdog);
  std::optional<ManagerState> notified_publication;
  if (getters_ready == std::future_status::ready) {
    notified_publication = getters.get();
  }
  auto second =
      std::async(std::launch::async, [&] { return manager->setActiveDevice("dummy", 96000, 128); });
  const auto second_ready = second.wait_for(kControlWatchdog);
  notification_pause.release();
  if (getters.valid()) {
    getters.get();
  }

  EXPECT_EQ(first.get(), SessionGraphError::OK);
  EXPECT_EQ(second.get(), SessionGraphError::OK);
  EXPECT_EQ(entered, std::future_status::ready);
  EXPECT_EQ(getters_ready, std::future_status::ready);
  EXPECT_EQ(second_ready, std::future_status::ready)
      << "Notifications must not hold the transition mutex";
  ASSERT_TRUE(notified_publication.has_value());
  EXPECT_NE(notified_publication->driver, nullptr);
  EXPECT_EQ(notified_publication->device, std::optional<std::string>("dummy"));
  EXPECT_EQ(notified_publication->sample_rate, 44100u);
  EXPECT_EQ(notified_publication->buffer_size, 256u);
  expectManagerState(readManagerState(*manager), manager->getActiveDriver(), 96000, 128);
  EXPECT_EQ(notifications.load(), 2);
}

TEST(AudioDriverManagerInjectedTest, DestructionStopsAndDestroysBackendOutsideStateLock) {
  auto state = std::make_shared<InjectedDriverState>();
  auto manager = detail::createAudioDriverManagerForTesting(
      [&](const std::string&) { return std::make_unique<InjectedDriver>(state); });
  ASSERT_EQ(manager->setActiveDevice("dummy", 48000, 512), SessionGraphError::OK);
  auto* manager_during_destruction = manager.get();
  auto* driver = manager->getActiveDriver();
  std::optional<ManagerState> stop_state;
  std::optional<ManagerState> destruction_state;
  // These are synchronous backend reentries, not external calls racing lifetime.
  state->stop_hook = [&] { stop_state = readManagerState(*manager_during_destruction); };
  state->destruction_hook = [&] {
    destruction_state = readManagerState(*manager_during_destruction);
  };
  manager.reset();

  ASSERT_TRUE(stop_state.has_value());
  expectManagerState(*stop_state, driver, 48000, 512);
  ASSERT_TRUE(destruction_state.has_value());
  expectManagerState(*destruction_state, nullptr, 48000, 512);
  EXPECT_EQ(state->stop_calls.load(), 1);
  EXPECT_EQ(state->destruction_calls.load(), 1);
}

TEST(AudioDriverManagerInjectedTest, ConcurrentTransitionsRetainOwnershipUntilSerializedCommit) {
  auto old_state = std::make_shared<InjectedDriverState>();
  auto first_state = std::make_shared<InjectedDriverState>();
  auto second_state = std::make_shared<InjectedDriverState>();
  std::atomic<int> creations{0};
  IAudioDriver* first_driver = nullptr;
  auto manager = detail::createAudioDriverManagerForTesting(
      [&](const std::string&) -> std::unique_ptr<IAudioDriver> {
        switch (++creations) {
        case 1:
          return std::make_unique<InjectedDriver>(old_state);
        case 2: {
          auto driver = std::make_unique<InjectedDriver>(first_state);
          first_driver = driver.get();
          return driver;
        }
        default:
          return std::make_unique<InjectedDriver>(second_state);
        }
      });
  ASSERT_EQ(manager->setActiveDevice("dummy", 48000, 512), SessionGraphError::OK);
  auto* old_driver = manager->getActiveDriver();
  ControlPause initialize_pause;
  first_state->initialize_hook = [&] { initialize_pause.block(); };
  std::optional<ManagerState> old_stop_publication;
  std::optional<ManagerState> first_stop_publication;
  old_state->stop_hook = [&] { old_stop_publication = readManagerState(*manager); };
  first_state->stop_hook = [&] { first_stop_publication = readManagerState(*manager); };

  auto first =
      std::async(std::launch::async, [&] { return manager->setActiveDevice("dummy", 44100, 256); });
  const auto entered = initialize_pause.entered.wait_for(kControlWatchdog);
  auto getters = std::async(std::launch::async, [&] { return readManagerState(*manager); });
  const auto getters_ready = getters.wait_for(kControlWatchdog);
  std::optional<ManagerState> paused_publication;
  if (getters_ready == std::future_status::ready) {
    paused_publication = getters.get();
  }
  std::promise<void> second_started;
  auto started = second_started.get_future();
  auto second = std::async(std::launch::async, [&] {
    second_started.set_value();
    return manager->setActiveDevice("dummy", 96000, 128);
  });
  const auto started_ready = started.wait_for(kControlWatchdog);
  const auto second_while_paused = second.wait_for(std::chrono::milliseconds(100));
  const int creations_while_paused = creations.load();
  const int old_stops_while_paused = old_state->stop_calls.load();
  const int old_destructions_while_paused = old_state->destruction_calls.load();
  const int first_destructions_while_paused = first_state->destruction_calls.load();
  initialize_pause.release();
  if (getters.valid()) {
    getters.get();
  }

  EXPECT_EQ(first.get(), SessionGraphError::OK);
  EXPECT_EQ(second.get(), SessionGraphError::OK);
  EXPECT_EQ(entered, std::future_status::ready);
  EXPECT_EQ(getters_ready, std::future_status::ready);
  EXPECT_EQ(started_ready, std::future_status::ready);
  EXPECT_EQ(second_while_paused, std::future_status::timeout);
  EXPECT_EQ(creations_while_paused, 2);
  EXPECT_EQ(old_stops_while_paused, 0);
  EXPECT_EQ(old_destructions_while_paused, 0);
  EXPECT_EQ(first_destructions_while_paused, 0);
  ASSERT_TRUE(paused_publication.has_value());
  expectManagerState(*paused_publication, old_driver, 48000, 512);
  ASSERT_TRUE(old_stop_publication.has_value());
  expectManagerState(*old_stop_publication, old_driver, 48000, 512);
  ASSERT_TRUE(first_stop_publication.has_value());
  expectManagerState(*first_stop_publication, first_driver, 44100, 256);
  expectManagerState(readManagerState(*manager), manager->getActiveDriver(), 96000, 128);
  EXPECT_EQ(old_state->stop_calls.load(), 1);
  EXPECT_EQ(first_state->stop_calls.load(), 1);
  EXPECT_EQ(old_state->destruction_calls.load(), 1);
  EXPECT_EQ(first_state->destruction_calls.load(), 1);
  EXPECT_EQ(second_state->destruction_calls.load(), 0);
}
