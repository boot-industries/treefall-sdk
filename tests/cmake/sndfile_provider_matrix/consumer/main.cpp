#include <orpheus/audio_file_capabilities.h>
#include <orpheus/audio_file_reader.h>
#include <orpheus/audio_file_writer.h>
#include <orpheus/audio_input.h>

#include <cmath>
#include <cstddef>
#include <vector>

#ifndef ORPHEUS_EXPECT_PROVIDER
#error "ORPHEUS_EXPECT_PROVIDER must be defined"
#endif

int main() {
  const bool has_sndfile = sizeof(ORPHEUS_EXPECT_PROVIDER) > 1 && ORPHEUS_EXPECT_PROVIDER[0] != 'N';
  auto writer = orpheus::createAudioFileWriter();
  auto reader = orpheus::createAudioFileReader();
  if ((writer != nullptr) != has_sndfile || (reader != nullptr) != has_sndfile) {
    return 1;
  }
  const auto capabilities = orpheus::getAudioFileCapabilities();
  if (capabilities.file_io_available != has_sndfile || capabilities.codecs.size() != 6u) {
    return 6;
  }
  orpheus::AudioFileWriterConfig valid_config;
  valid_config.format = orpheus::AudioFileFormat::WAV;
  valid_config.sample_rate = 48000;
  valid_config.num_channels = 2;
  valid_config.sample_format = orpheus::AudioSampleFormat::Int16;
  const auto preflight = orpheus::preflightAudioFileWrite(valid_config);
  if (preflight !=
      (has_sndfile ? orpheus::SessionGraphError::OK : orpheus::SessionGraphError::NotReady)) {
    return 7;
  }
  const auto missing = orpheus::probeAudioFile("provider-matrix-definitely-missing-input.wav");
  if (missing.error != (has_sndfile ? orpheus::SessionGraphError::InternalError
                                    : orpheus::SessionGraphError::NotReady)) {
    return 8;
  }

  orpheus::AudioInputStreamConfig config;
  config.num_channels = 2;
  config.sample_rate = 48000;
  config.ring_capacity_frames = 8;
  auto stream = orpheus::createAudioInputStream(config);
  if (!stream || stream->numChannels() != 2 || stream->sampleRate() != 48000) {
    return 2;
  }

  const std::vector<float> input = {0.125f, -0.25f, 0.5f, -0.75f, 0.875f, -1.0f};
  if (stream->capture(input.data(), 3) != 3 || stream->framesPending() != 3) {
    return 3;
  }
  std::vector<float> output(input.size(), 0.0f);
  if (stream->drain(output.data(), 3) != 3 || stream->framesPending() != 0) {
    return 4;
  }
  for (size_t index = 0; index < input.size(); ++index) {
    if (std::fabs(output[index] - input[index]) > 0.000001f) {
      return 5;
    }
  }
  return 0;
}
