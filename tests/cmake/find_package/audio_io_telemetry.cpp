// SPDX-License-Identifier: MIT
#include <orpheus/audio_driver.h>

#include <cstdint>
#include <string>
#include <type_traits>
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

template <typename T>
concept AcceptsTwoTelemetryFields =
    requires { T{uint64_t{1}, orpheus::AudioRouteRuntimeOutcome::Healthy}; };

template <typename T>
concept AcceptsThreeTelemetryFields = requires {
  T{uint64_t{1}, orpheus::AudioRouteRuntimeOutcome::Healthy,
    orpheus::AudioRouteRuntimeOutcome::Healthy};
};

static_assert(static_cast<uint8_t>(orpheus::AudioRouteRuntimeOutcome::Healthy) == 0);
static_assert(static_cast<uint8_t>(orpheus::AudioRouteRuntimeOutcome::SampleRateUnsupported) == 1);
static_assert(static_cast<uint8_t>(orpheus::AudioRouteRuntimeOutcome::SampleRateChangeFailed) == 2);
static_assert(static_cast<uint8_t>(orpheus::AudioRouteRuntimeOutcome::SampleRateChanged) == 3);
static_assert(static_cast<uint8_t>(orpheus::AudioRouteRuntimeOutcome::BufferSizeChanged) == 4);
static_assert(static_cast<uint8_t>(orpheus::AudioRouteRuntimeOutcome::FormatChanged) == 5);
static_assert(static_cast<uint8_t>(orpheus::AudioRouteRuntimeOutcome::ChannelMapInvalid) == 6);
static_assert(static_cast<uint8_t>(orpheus::AudioRouteRuntimeOutcome::InputRouteUnavailable) == 7);
static_assert(static_cast<uint8_t>(orpheus::AudioRouteRuntimeOutcome::OutputRouteUnavailable) == 8);
static_assert(static_cast<uint8_t>(orpheus::AudioRouteRuntimeOutcome::PermissionDenied) == 9);
static_assert(static_cast<uint8_t>(orpheus::AudioRouteRuntimeOutcome::BackendFailure) == 10);
static_assert(AcceptsTwoTelemetryFields<orpheus::AudioIoTelemetry>);
static_assert(!AcceptsThreeTelemetryFields<orpheus::AudioIoTelemetry>);

} // namespace

int main() {
  orpheus::AudioDriverConfig config{48000, 512, 0, {}, 2, {}, {}, {{}, {0, 1}}};
  TelemetryDriver driver;
  if (driver.initialize(config) != orpheus::SessionGraphError::OK) {
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
  const orpheus::AudioIoTelemetry telemetry{0, orpheus::AudioRouteRuntimeOutcome::Healthy};
  return telemetry.input_render_failures != 0 ||
                 telemetry.route_outcome != orpheus::AudioRouteRuntimeOutcome::Healthy
             ? 3
             : 0;
}
