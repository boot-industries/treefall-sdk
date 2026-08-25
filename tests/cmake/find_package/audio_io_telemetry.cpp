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
concept AcceptsAllTelemetryFields = requires {
  T{uint64_t{1}, orpheus::AudioRouteRuntimeOutcome::Healthy,
    uint64_t{2}, uint64_t{3},
    uint64_t{4}, uint64_t{5},
    int32_t{-7}};
};

static_assert(static_cast<uint8_t>(orpheus::AudioSampleRatePolicy::PreserveDeviceRate) == 0);
static_assert(static_cast<uint8_t>(orpheus::AudioSampleRatePolicy::RequestExactRate) == 1);
static_assert(static_cast<uint8_t>(orpheus::AudioSampleRatePolicy::RequestExactRateOrConvert) == 2);
static_assert(static_cast<uint8_t>(orpheus::AudioOutputChannelPolicy::RequireRequestedChannels) ==
              0);
static_assert(static_cast<uint8_t>(orpheus::AudioOutputChannelPolicy::AllowMonoFallback) == 1);
static_assert(static_cast<uint8_t>(orpheus::AudioRouteCompatibilityStatus::Compatible) == 0);
static_assert(
    static_cast<uint8_t>(orpheus::AudioRouteCompatibilityStatus::RequiresSampleRateChange) == 1);
static_assert(static_cast<uint8_t>(orpheus::AudioRouteCompatibilityStatus::InputUnavailable) == 2);
static_assert(static_cast<uint8_t>(orpheus::AudioRouteCompatibilityStatus::OutputUnavailable) == 3);
static_assert(static_cast<uint8_t>(orpheus::AudioRouteCompatibilityStatus::SampleRateUnsupported) ==
              4);
static_assert(static_cast<uint8_t>(orpheus::AudioRouteCompatibilityStatus::InvalidChannelMap) == 5);
static_assert(static_cast<uint8_t>(orpheus::AudioRouteCompatibilityStatus::PermissionDenied) == 6);
static_assert(static_cast<uint8_t>(orpheus::AudioRouteCompatibilityStatus::BackendFailure) == 7);
static_assert(static_cast<uint8_t>(orpheus::AudioRouteCompatibilityStatus::ProfileConflict) == 8);
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
static_assert(static_cast<uint8_t>(orpheus::AudioRouteRuntimeOutcome::ProfileConflict) == 11);
static_assert(static_cast<uint8_t>(orpheus::AudioRouteRuntimeOutcome::InputConversionFailed) == 12);
static_assert(static_cast<uint8_t>(orpheus::AudioRouteRuntimeOutcome::OutputConversionFailed) ==
              13);
static_assert(AcceptsTwoTelemetryFields<orpheus::AudioIoTelemetry>);
static_assert(AcceptsAllTelemetryFields<orpheus::AudioIoTelemetry>);

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
      observed.sample_rate_policy != orpheus::AudioSampleRatePolicy::PreserveDeviceRate ||
      observed.output_channel_policy !=
          orpheus::AudioOutputChannelPolicy::RequireRequestedChannels) {
    return 2;
  }
  const orpheus::AudioIoTelemetry telemetry{0, orpheus::AudioRouteRuntimeOutcome::Healthy};
  const orpheus::AudioIoTelemetry full_telemetry{
      uint64_t{1}, orpheus::AudioRouteRuntimeOutcome::Healthy,
      uint64_t{2}, uint64_t{3},
      uint64_t{4}, uint64_t{5},
      int32_t{-7}};
  if (full_telemetry.route_backend_error != -7) {
    return 4;
  }
  return telemetry.input_render_failures != 0 ||
                 telemetry.route_outcome != orpheus::AudioRouteRuntimeOutcome::Healthy
             ? 3
             : 0;
}
