// SPDX-License-Identifier: MIT
#include <orpheus/tape_varispeed.h>

#include <array>

int main() {
  orpheus::PreparedTapeVarispeed processor;
  orpheus::TapeVarispeedConfig config;
  config.sample_rate = 48000;
  config.channels = 1;
  config.max_input_frames = 64;
  config.max_output_frames = 128;
  if (processor.prepare(config) != orpheus::SessionGraphError::OK) {
    return 1;
  }
  std::array<float, 64> input{};
  std::array<float, 128> output{};
  const auto result =
      processor.process(input.data(), input.size(), output.data(), output.size(), 1.0);
  return result.error == orpheus::SessionGraphError::OK ? 0 : 1;
}
