// SPDX-License-Identifier: MIT
#ifdef _WIN32

#include <orpheus/audio_driver.h>
#include <orpheus/audio_driver_manager.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

namespace {

class AcceptanceCallback final : public orpheus::IAudioCallback {
public:
  void processAudio(const float**, float** outputs, size_t channels, size_t frames) override {
    for (size_t channel = 0; channel < channels; ++channel) {
      std::fill_n(outputs[channel], frames, 0.0f);
    }
    callbacks_.fetch_add(1, std::memory_order_relaxed);
    frames_.fetch_add(frames, std::memory_order_relaxed);
  }

  uint64_t callbacks() const {
    return callbacks_.load(std::memory_order_relaxed);
  }
  uint64_t frames() const {
    return frames_.load(std::memory_order_relaxed);
  }

private:
  std::atomic<uint64_t> callbacks_{0};
  std::atomic<uint64_t> frames_{0};
};

} // namespace

int main() {
  auto manager = orpheus::createAudioDriverManager();
  const auto devices = manager->enumerateDevices();
  auto selected = std::find_if(devices.begin(), devices.end(), [](const auto& device) {
    return device.driverType == "WASAPI" && device.isDefaultDevice;
  });
  if (selected == devices.end()) {
    selected = std::find_if(devices.begin(), devices.end(),
                            [](const auto& device) { return device.driverType == "WASAPI"; });
  }
  if (selected == devices.end() || selected->supportedSampleRates.empty() ||
      selected->supportedBufferSizes.empty()) {
    std::cout << "{\"status\":\"failed\",\"reason\":\"no-active-wasapi-endpoint\"}\n";
    return 1;
  }

  const uint32_t sampleRate =
      std::find(selected->supportedSampleRates.begin(), selected->supportedSampleRates.end(),
                48000u) != selected->supportedSampleRates.end()
          ? 48000u
          : selected->supportedSampleRates.front();
  const uint32_t bufferSize = selected->supportedBufferSizes.front();
  if (manager->setActiveDevice(selected->deviceId, sampleRate, bufferSize) !=
      orpheus::SessionGraphError::OK) {
    std::cout << "{\"status\":\"failed\",\"reason\":\"device-initialize\"}\n";
    return 2;
  }

  orpheus::IAudioDriver* driver = manager->getActiveDriver();
  AcceptanceCallback callback;
  if (driver == nullptr || driver->start(&callback) != orpheus::SessionGraphError::OK) {
    std::cout << "{\"status\":\"failed\",\"reason\":\"callback-start\"}\n";
    return 3;
  }
  std::this_thread::sleep_for(std::chrono::seconds(3));
  const auto stopResult = driver->stop();
  const auto& negotiated = driver->getConfig();
  const bool passed = stopResult == orpheus::SessionGraphError::OK && callback.callbacks() > 0;

  std::cout << "{\"status\":\"" << (passed ? "passed" : "failed")
            << "\",\"backend\":\"WASAPI\",\"sampleRate\":" << negotiated.sample_rate
            << ",\"bufferFrames\":" << negotiated.buffer_size
            << ",\"channels\":" << negotiated.num_outputs
            << ",\"callbacks\":" << callback.callbacks() << ",\"frames\":" << callback.frames()
            << "}\n";
  return passed ? 0 : 4;
}

#endif
