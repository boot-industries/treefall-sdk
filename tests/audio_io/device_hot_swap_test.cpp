// SPDX-License-Identifier: MIT
#include <orpheus/audio_driver.h>
#include <orpheus/audio_driver_manager.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

using namespace orpheus;

/// Simple audio callback for testing (generates silence)
class SilenceCallback : public IAudioCallback {
public:
  void processAudio(const float** /*input_buffers*/, float** output_buffers, size_t num_channels,
                    size_t num_frames) override {
    // Generate silence
    for (size_t ch = 0; ch < num_channels; ++ch) {
      for (size_t i = 0; i < num_frames; ++i) {
        output_buffers[ch][i] = 0.0f;
      }
    }
    m_callbackCount++;
  }

  int getCallbackCount() const {
    return m_callbackCount.load();
  }

private:
  std::atomic<int> m_callbackCount{0};
};

/// Test fixture for device hot-swap integration tests
class DeviceHotSwapTest : public ::testing::Test {
protected:
  void SetUp() override {
    manager = createAudioDriverManager();
    ASSERT_NE(manager, nullptr) << "Failed to create AudioDriverManager";

    callback = std::make_unique<SilenceCallback>();
  }

  void TearDown() override {
    // Stop driver before cleanup
    auto* driver = manager->getActiveDriver();
    if (driver && driver->isRunning()) {
      driver->stop();
    }
    manager.reset();
    callback.reset();
  }

  std::unique_ptr<IAudioDriverManager> manager;
  std::unique_ptr<SilenceCallback> callback;
};

/// Test: Hot-swap from dummy driver to another dummy driver instance
TEST_F(DeviceHotSwapTest, HotSwap_DummyToDummy_NoAudioThread) {
  // Start with dummy driver at 48kHz/512
  SessionGraphError result = manager->setActiveDevice("dummy", 48000, 512);
  ASSERT_EQ(result, SessionGraphError::OK);

  auto* driver1 = manager->getActiveDriver();
  ASSERT_NE(driver1, nullptr);

  // Hot-swap to dummy driver at 44.1kHz/256
  result = manager->setActiveDevice("dummy", 44100, 256);
  ASSERT_EQ(result, SessionGraphError::OK);

  auto* driver2 = manager->getActiveDriver();
  ASSERT_NE(driver2, nullptr);

  // Verify state changed
  EXPECT_EQ(manager->getCurrentSampleRate(), 44100u);
  EXPECT_EQ(manager->getCurrentBufferSize(), 256u);
}

/// Test: Hot-swap with audio thread running
TEST_F(DeviceHotSwapTest, HotSwap_WithAudioThreadRunning) {
  // Start with dummy driver
  SessionGraphError result = manager->setActiveDevice("dummy", 48000, 512);
  ASSERT_EQ(result, SessionGraphError::OK);

  auto* driver = manager->getActiveDriver();
  ASSERT_NE(driver, nullptr);

  // Start audio callback
  result = driver->start(callback.get());
  ASSERT_EQ(result, SessionGraphError::OK);

  // Wait for some callbacks
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  int initialCallbackCount = callback->getCallbackCount();
  EXPECT_GT(initialCallbackCount, 0) << "Audio callbacks should be firing";

  // Hot-swap to different settings (this will stop the driver)
  result = manager->setActiveDevice("dummy", 44100, 256);
  ASSERT_EQ(result, SessionGraphError::OK);

  // Verify new driver is available
  auto* newDriver = manager->getActiveDriver();
  ASSERT_NE(newDriver, nullptr);

  // Verify driver is not running (hot-swap stops playback)
  EXPECT_FALSE(newDriver->isRunning()) << "Driver should be stopped after hot-swap";

  // Restart audio callback with new driver
  result = newDriver->start(callback.get());
  ASSERT_EQ(result, SessionGraphError::OK);

  // Wait for callbacks on new driver
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  int finalCallbackCount = callback->getCallbackCount();
  EXPECT_GT(finalCallbackCount, initialCallbackCount)
      << "Audio callbacks should resume after hot-swap";
}

/// Test: Verify no crashes during hot-swap
TEST_F(DeviceHotSwapTest, HotSwap_NoCrashes) {
  // Rapidly hot-swap between configurations
  for (int i = 0; i < 10; ++i) {
    uint32_t sampleRate = (i % 2 == 0) ? 48000 : 44100;
    uint32_t bufferSize = (i % 2 == 0) ? 512 : 256;

    SessionGraphError result = manager->setActiveDevice("dummy", sampleRate, bufferSize);
    ASSERT_EQ(result, SessionGraphError::OK) << "Hot-swap iteration " << i << " failed";

    // Verify state
    EXPECT_EQ(manager->getCurrentSampleRate(), sampleRate);
    EXPECT_EQ(manager->getCurrentBufferSize(), bufferSize);
  }
}

/// Test: Hot-swap callback fires on device change
TEST_F(DeviceHotSwapTest, HotSwap_CallbackFires) {
  int callbackCount = 0;
  manager->setDeviceChangeCallback([&callbackCount]() { callbackCount++; });

  // Perform multiple hot-swaps
  manager->setActiveDevice("dummy", 48000, 512);
  EXPECT_EQ(callbackCount, 1) << "Callback should fire after first device change";

  manager->setActiveDevice("dummy", 44100, 512);
  EXPECT_EQ(callbackCount, 2) << "Callback should fire after second device change";

  manager->setActiveDevice("dummy", 48000, 256);
  EXPECT_EQ(callbackCount, 3) << "Callback should fire after third device change";
}

/// Test: Verify state consistency after hot-swap
TEST_F(DeviceHotSwapTest, HotSwap_StateConsistency) {
  // Set initial device
  manager->setActiveDevice("dummy", 48000, 512);

  // Query initial state
  auto device1 = manager->getCurrentDevice();
  uint32_t sampleRate1 = manager->getCurrentSampleRate();
  uint32_t bufferSize1 = manager->getCurrentBufferSize();

  ASSERT_TRUE(device1.has_value());
  EXPECT_EQ(device1.value(), "dummy");
  EXPECT_EQ(sampleRate1, 48000u);
  EXPECT_EQ(bufferSize1, 512u);

  // Hot-swap
  manager->setActiveDevice("dummy", 44100, 256);

  // Query new state
  auto device2 = manager->getCurrentDevice();
  uint32_t sampleRate2 = manager->getCurrentSampleRate();
  uint32_t bufferSize2 = manager->getCurrentBufferSize();

  ASSERT_TRUE(device2.has_value());
  EXPECT_EQ(device2.value(), "dummy");
  EXPECT_EQ(sampleRate2, 44100u);
  EXPECT_EQ(bufferSize2, 256u);

  // Verify all getters return consistent state
  EXPECT_EQ(manager->getCurrentDevice().value(), "dummy");
  EXPECT_EQ(manager->getCurrentSampleRate(), 44100u);
  EXPECT_EQ(manager->getCurrentBufferSize(), 256u);
}

#ifdef __APPLE__
/// Test: Hot-swap from dummy to CoreAudio device (macOS only)
TEST_F(DeviceHotSwapTest, HotSwap_DummyToCoreAudio) {
  // Start with dummy driver
  SessionGraphError result = manager->setActiveDevice("dummy", 48000, 512);
  ASSERT_EQ(result, SessionGraphError::OK);

  // Enumerate devices to find a CoreAudio device
  auto devices = manager->enumerateDevices();
  std::optional<AudioDeviceInfo> coreAudioDevice;
  for (const auto& device : devices) {
    if (device.driverType == "CoreAudio") {
      coreAudioDevice = device;
      break;
    }
  }

  if (!coreAudioDevice.has_value()) {
    GTEST_SKIP() << "No CoreAudio device available for testing";
  }

  // Hot-swap to CoreAudio device
  result = manager->setActiveDevice(coreAudioDevice->deviceId, 48000, 512);
  ASSERT_EQ(result, SessionGraphError::OK) << "Hot-swap to CoreAudio device failed";

  // Verify state
  auto currentDevice = manager->getCurrentDevice();
  ASSERT_TRUE(currentDevice.has_value());
  EXPECT_EQ(currentDevice.value(), coreAudioDevice->deviceId);

  // Verify driver is active
  auto* driver = manager->getActiveDriver();
  ASSERT_NE(driver, nullptr);
  EXPECT_EQ(driver->getDriverName(), "CoreAudio");
}

/// Test: Hot-swap from CoreAudio back to dummy
TEST_F(DeviceHotSwapTest, HotSwap_CoreAudioToDummy) {
  // Enumerate devices to find a CoreAudio device
  auto devices = manager->enumerateDevices();
  std::optional<AudioDeviceInfo> coreAudioDevice;
  for (const auto& device : devices) {
    if (device.driverType == "CoreAudio") {
      coreAudioDevice = device;
      break;
    }
  }

  if (!coreAudioDevice.has_value()) {
    GTEST_SKIP() << "No CoreAudio device available for testing";
  }

  // Start with CoreAudio device
  SessionGraphError result = manager->setActiveDevice(coreAudioDevice->deviceId, 48000, 512);
  ASSERT_EQ(result, SessionGraphError::OK);

  // Hot-swap back to dummy
  result = manager->setActiveDevice("dummy", 48000, 512);
  ASSERT_EQ(result, SessionGraphError::OK);

  // Verify state
  auto currentDevice = manager->getCurrentDevice();
  ASSERT_TRUE(currentDevice.has_value());
  EXPECT_EQ(currentDevice.value(), "dummy");

  // Verify driver is dummy
  auto* driver = manager->getActiveDriver();
  ASSERT_NE(driver, nullptr);
  EXPECT_EQ(driver->getDriverName(), "Dummy");
}
#endif

/// Test: Hot-swap during audio playback (stress test)
TEST_F(DeviceHotSwapTest, HotSwap_DuringPlayback_StressTest) {
  // Start with dummy driver and audio running
  manager->setActiveDevice("dummy", 48000, 512);
  auto* driver = manager->getActiveDriver();
  ASSERT_NE(driver, nullptr);

  driver->start(callback.get());
  ASSERT_TRUE(driver->isRunning());

  // Perform rapid hot-swaps
  for (int i = 0; i < 5; ++i) {
    // Wait for some callbacks
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Hot-swap
    uint32_t sampleRate = (i % 2 == 0) ? 44100 : 48000;
    SessionGraphError result = manager->setActiveDevice("dummy", sampleRate, 512);
    ASSERT_EQ(result, SessionGraphError::OK);

    // Restart audio
    driver = manager->getActiveDriver();
    ASSERT_NE(driver, nullptr);
    result = driver->start(callback.get());
    ASSERT_EQ(result, SessionGraphError::OK);
  }

  // Verify callbacks were received
  EXPECT_GT(callback->getCallbackCount(), 0) << "Audio callbacks should have fired during test";
}

/// Test: Verify no memory leaks during hot-swap
TEST_F(DeviceHotSwapTest, HotSwap_NoMemoryLeaks) {
  // Perform many hot-swaps to detect memory leaks
  // (This test relies on AddressSanitizer to detect leaks)
  for (int i = 0; i < 100; ++i) {
    manager->setActiveDevice("dummy", 48000, 512);
    manager->setActiveDevice("dummy", 44100, 256);
  }

  // If we reach here without crashes or sanitizer errors, test passes
  SUCCEED();
}
