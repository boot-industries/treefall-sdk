// SPDX-License-Identifier: MIT
#include <orpheus/audio_driver.h>

#include <vector>

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
  orpheus::AudioDriverConfig legacy{48000, 512, 0, {}, 2, {}, {}, {{}, {0, 1}}};
  TelemetryDriver driver;
  if (driver.initialize(legacy) != orpheus::SessionGraphError::OK) {
    return 1;
  }
  const auto& observed = driver.getConfig();
  if (observed.sample_rate != 48000 || observed.buffer_size != 512 || observed.num_inputs != 0 ||
      !observed.input_device_id.empty() || observed.num_outputs != 2 ||
      !observed.output_device_id.empty() || !observed.device_name.empty() ||
      !observed.channel_map.input_channels.empty() ||
      observed.channel_map.output_channels != std::vector<uint16_t>{0, 1} ||
      observed.sample_rate_policy != orpheus::AudioSampleRatePolicy::PreserveDeviceRate) {
    return 2;
  }
  const orpheus::AudioIoTelemetry telemetry = driver.getTelemetry();
  return telemetry.input_render_failures != 0 ||
                 telemetry.runtime_outcome != orpheus::AudioDriverRuntimeOutcome::Healthy
             ? 3
             : 0;
}
