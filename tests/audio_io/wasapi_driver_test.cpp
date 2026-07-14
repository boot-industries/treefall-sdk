// SPDX-License-Identifier: MIT
#ifdef _WIN32

#include <orpheus/audio_driver.h>
#include <orpheus/audio_driver_manager.h>

#include <gtest/gtest.h>

namespace orpheus {
namespace {

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
