// SPDX-License-Identifier: MIT
#include <orpheus/audio_driver.h>

namespace {

class TelemetryDriver final : public orpheus::IAudioDriver {
public:
  orpheus::SessionGraphError initialize(const orpheus::AudioDriverConfig& config) override {
    config_ = config;
    return orpheus::SessionGraphError::OK;
  }

  orpheus::SessionGraphError start(orpheus::IAudioCallback*) override {
    return orpheus::SessionGraphError::OK;
  }

  orpheus::SessionGraphError stop() override {
    return orpheus::SessionGraphError::OK;
  }

  bool isRunning() const override {
    return false;
  }

  const orpheus::AudioDriverConfig& getConfig() const override {
    return config_;
  }

  std::string getDriverName() const override {
    return "TelemetryFixture";
  }

  uint32_t getLatencySamples() const override {
    return 0;
  }

private:
  orpheus::AudioDriverConfig config_;
};

} // namespace

int main() {
  TelemetryDriver driver;
  const orpheus::AudioIoTelemetry telemetry = driver.getTelemetry();
  return telemetry.input_render_failures != 0 ||
                 telemetry.runtime_outcome != orpheus::AudioDriverRuntimeOutcome::Healthy
             ? 1
             : 0;
}
