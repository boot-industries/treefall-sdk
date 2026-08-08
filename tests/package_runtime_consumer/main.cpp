// SPDX-License-Identifier: MIT
#include <orpheus/audio_driver.h>
#include <orpheus/audio_driver_manager.h>
#include <orpheus/routing_matrix.h>
#include <orpheus/transport_controller.h>

#include <algorithm>
#include <cstddef>
#include <vector>

#include <cstdint>

namespace {

class RecordingCallback final : public orpheus::ITransportCallback {
public:
  void onClipStarted(orpheus::ClipHandle handle, uint32_t voiceId,
                     orpheus::TransportPosition) override {
    ++started;
    lastHandle = handle;
    lastVoiceId = voiceId;
  }

  void onClipStopped(orpheus::ClipHandle, uint32_t, orpheus::TransportPosition) override {}
  void onClipLooped(orpheus::ClipHandle, uint32_t, orpheus::TransportPosition) override {}
  void onBufferUnderrun(orpheus::TransportPosition) override {}

  int started = 0;
  orpheus::ClipHandle lastHandle = 0;
  uint32_t lastVoiceId = 0;
};

} // namespace

bool validLegacyAudioDriverConfig(const orpheus::AudioDriverConfig& config) {
  return config.sample_rate == 48000 && config.buffer_size == 512 && config.num_inputs == 0 &&
         config.input_device_id.empty() && config.num_outputs == 2 &&
         config.output_device_id.empty() && config.device_name.empty() &&
         config.channel_map.input_channels.empty() &&
         config.channel_map.output_channels == std::vector<uint16_t>{0, 1} &&
         config.sample_rate_policy == orpheus::AudioSampleRatePolicy::PreserveDeviceRate;
}

int main() {
  orpheus::AudioDriverConfig legacy{48000, 512, 0, {}, 2, {}, {}, {{}, {0, 1}}};
  if (!validLegacyAudioDriverConfig(legacy)) {
    return 18;
  }
  constexpr uint32_t kSampleRate = 48000;
  constexpr orpheus::ClipHandle kHandle = 17;
  constexpr size_t kFrames = 64;

  auto outputDriver = orpheus::createDummyAudioDriver();
  if (!outputDriver) {
    return 11;
  }
  orpheus::AudioOutputRouteRequest outputRequest;
  outputRequest.output_device_id = "dummy";
  outputRequest.output_channel_map = {1, 0};
  outputRequest.requested_sample_rate = kSampleRate;
  outputRequest.requested_buffer_size = 128;
  if (outputDriver->initializeAudioOutput(outputRequest) != orpheus::SessionGraphError::OK) {
    return 12;
  }
  if (outputDriver->getConfig().channel_map.output_channels != outputRequest.output_channel_map) {
    return 13;
  }
  const auto routeState = outputDriver->getAudioIoRouteState();
  if (routeState.state != orpheus::AudioRouteState::Inactive ||
      routeState.selected_output_device_id != outputRequest.output_device_id ||
      routeState.active_output_channel_map != outputRequest.output_channel_map ||
      routeState.requested_sample_rate != kSampleRate ||
      routeState.actual_sample_rate != kSampleRate ||
      routeState.requested_buffer_size != outputRequest.requested_buffer_size ||
      routeState.actual_buffer_size != outputRequest.requested_buffer_size) {
    return 14;
  }
  const auto activeRoute = outputDriver->getActiveRoute();
  if (!activeRoute.input_device_id.empty() || !activeRoute.output_device_id.empty() ||
      activeRoute.input_alive || activeRoute.output_alive || !activeRoute.input_channels.empty() ||
      !activeRoute.output_channels.empty() || activeRoute.latency.complete) {
    return 15;
  }

  auto manager = orpheus::createAudioDriverManager();
  if (!manager) {
    return 16;
  }
  const auto devices = manager->enumerateDevices();
  const auto dummy = std::find_if(devices.begin(), devices.end(),
                                  [](const auto& device) { return device.deviceId == "dummy"; });
  if (dummy == devices.end() || dummy->maxChannels != 2) {
    return 17;
  }

  auto transport = orpheus::createTransportController(
      nullptr, orpheus::TransportConfig{.sampleRate = static_cast<uint32_t>(kSampleRate)});
  if (!transport) {
    return 1;
  }

  const orpheus::TransportConfig config = transport->getRenderConfig();
  if (config.sampleRate != kSampleRate || config.outputChannels != 2 ||
      config.maxBlockFrames < kFrames) {
    return 2;
  }

  auto* routing = transport->getRoutingMatrix();
  if (routing == nullptr) {
    return 7;
  }
  const auto initialRouting = routing->getRoutingControlSnapshot();
  if (initialRouting.group_count != config.numGroups) {
    return 8;
  }
  auto desiredRouting = initialRouting;
  desiredRouting.groups[0].gain_db = -4.0f;
  desiredRouting.groups[0].configured_mute = true;
  if (routing->applyGroupControlSnapshot(desiredRouting) != orpheus::SessionGraphError::OK) {
    return 9;
  }
  const auto appliedRouting = routing->getRoutingControlSnapshot();
  if (appliedRouting.revision != initialRouting.revision + 1 ||
      appliedRouting.groups[0].gain_db != -4.0f || !appliedRouting.groups[0].configured_mute) {
    return 10;
  }

  std::vector<std::vector<float>> storage(config.outputChannels, std::vector<float>(kFrames, 1.0f));
  std::vector<float*> outputs;
  outputs.reserve(config.outputChannels);
  for (auto& channel : storage) {
    outputs.push_back(channel.data());
  }

  RecordingCallback callback;
  transport->setCallback(&callback);
  if (transport->startClip(kHandle) != orpheus::SessionGraphError::OK) {
    return 3;
  }

  transport->processAudio(outputs.data(), outputs.size(), kFrames);
  if (callback.started != 0) {
    return 4;
  }

  transport->processCallbacks();
  if (callback.started != 1 || callback.lastHandle != kHandle || callback.lastVoiceId == 0) {
    return 5;
  }

  for (const auto& channel : storage) {
    for (float sample : channel) {
      if (sample != 0.0f) {
        return 6;
      }
    }
  }

  transport->setCallback(nullptr);

  return 0;
}
