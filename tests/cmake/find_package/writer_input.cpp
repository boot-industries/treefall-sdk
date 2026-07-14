// SPDX-License-Identifier: MIT
#include <orpheus/audio_file_writer.h>
#include <orpheus/audio_input.h>

int main() {
  auto writer = orpheus::createAudioFileWriter();
  orpheus::AudioInputStreamConfig config;
  config.sample_rate = 48000;
  config.num_channels = 2;
  config.ring_capacity_frames = 1024;
  auto input = orpheus::createAudioInputStream(config);
  return writer != nullptr && input != nullptr ? 0 : 1;
}
