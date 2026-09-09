// SPDX-License-Identifier: MIT
#include <treefall/audio_file_capabilities.h>
#include <treefall/audio_file_reader.h>
#include <treefall/audio_file_writer.h>

int main() {
  const auto capabilities = treefall::getAudioFileCapabilities();
  if (!capabilities.file_io_available || capabilities.codecs.empty()) {
    return 1;
  }
  treefall::AudioFileWriterConfig config;
  config.format = treefall::AudioFileFormat::WAV;
  config.sample_format = treefall::AudioSampleFormat::Int16;
  config.num_channels = 2;
  if (treefall::preflightAudioFileWrite(config) == treefall::SessionGraphError::InvalidParameter) {
    return 2;
  }
  return 0;
}
