// SPDX-License-Identifier: MIT
#include <orpheus/audio_driver_manager.h>
#include <orpheus/routing_matrix.h>
#include <orpheus/transport_controller.h>
#include <vector>

int main() {
  auto routing = orpheus::createRoutingMatrix();
  orpheus::RoutingConfig config;
  config.num_channels = 1;
  config.num_groups = 1;
  config.num_outputs = 2;
  config.enable_metering = false;
  config.enable_clipping_protection = false;
  if (routing->initialize(config) != orpheus::SessionGraphError::OK) {
    return 1;
  }

  constexpr uint32_t frames = 4096;
  std::vector<float> input(frames, 0.5f);
  const float* inputs[1] = {input.data()};
  std::vector<float> left(frames, -999.0f);
  std::vector<float> right(frames, -999.0f);
  float* outputs[2] = {left.data(), right.data()};
  if (routing->processRouting(inputs, outputs, frames) != orpheus::SessionGraphError::OK ||
      left[2048] < 0.3f || right.back() < 0.3f) {
    return 2;
  }

  auto transport = orpheus::createTransportController(
      nullptr, orpheus::TransportConfig{.sampleRate = static_cast<uint32_t>(48000)});
  auto manager = orpheus::createAudioDriverManager();
  if (transport == nullptr || manager == nullptr || routing->maxBlockFrames() < 1) {
    return 3;
  }
  if (transport->startClipWithGroupChoke(0) != orpheus::SessionGraphError::InvalidHandle ||
      transport->startClipWithGroupChoke(42) != orpheus::SessionGraphError::ClipNotRegistered) {
    return 4;
  }
  return 0;
}
