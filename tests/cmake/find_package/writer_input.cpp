// SPDX-License-Identifier: MIT
#include <array>
#include <orpheus/audio_input.h>

int main() {
  orpheus::AudioInputStreamConfig config;
  config.sample_rate = 48000;
  config.num_channels = 2;
  config.ring_capacity_frames = 1024;
  auto input = orpheus::createAudioInputStream(config);
  if (!input)
    return 1;
  const std::array<float, 8> expected{0.25f, -0.25f, 0.5f, -0.5f, 0.75f, -0.75f, 1.0f, -1.0f};
  std::array<float, 8> actual{};
  if (input->capture(expected.data(), 4) != 4 || input->framesPending() != 4)
    return 2;
  if (input->drain(actual.data(), 2) != 2 || input->framesPending() != 2 ||
      input->drain(actual.data() + 4, 2) != 2 || input->framesPending() != 0)
    return 3;
  return actual == expected ? 0 : 4;
}
