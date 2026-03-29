// SPDX-License-Identifier: MIT
#include <orpheus/audio_file_reader.h>
#include <orpheus/channel_format.h>

int main() {
  auto reader = orpheus::createAudioFileReader();
  if (reader != nullptr) {
    (void)reader->isOpen();
  }

  const auto mix = orpheus::MixMatrix::Upmix_Mono_to_Stereo();
  (void)mix.output_channels;

  const auto format = orpheus::ChannelFormat::Stereo();
  (void)format.num_channels;
  return 0;
}
