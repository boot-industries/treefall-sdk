// SPDX-License-Identifier: MIT
#include <orpheus/audio_driver_manager.h>
#include <orpheus/routing_matrix.h>
#include <orpheus/transport_controller.h>

int main() {
  auto routing = orpheus::createRoutingMatrix();
  orpheus::RoutingConfig config;
  config.num_channels = 1;
  config.num_groups = 1;
  config.num_outputs = 2;
  if (routing->initialize(config) != orpheus::SessionGraphError::OK) {
    return 1;
  }

  auto transport = orpheus::createTransportController(nullptr, 48000);
  auto manager = orpheus::createAudioDriverManager();
  return transport != nullptr && manager != nullptr && routing->maxBlockFrames() >= 1 ? 0 : 1;
}
