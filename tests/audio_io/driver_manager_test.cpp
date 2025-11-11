// SPDX-License-Identifier: MIT
#include <orpheus/audio_driver_manager.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <thread>

using namespace orpheus;

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
