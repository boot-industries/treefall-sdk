// SPDX-License-Identifier: MIT
#include <orpheus/audio_file_capabilities.h>

#include "audio_file_format.h"

#include <climits>
#include <cstdint>

namespace orpheus {
namespace {
constexpr AudioSampleFormat pcmFormats[] = {AudioSampleFormat::Int16, AudioSampleFormat::Int24,
                                            AudioSampleFormat::Float32};
constexpr AudioSampleFormat flacFormats[] = {AudioSampleFormat::Int16, AudioSampleFormat::Int24};
constexpr AudioFileCodecCapabilities codecs[] = {
    {AudioFileFormat::Unknown, false, {}},     {AudioFileFormat::WAV, true, pcmFormats},
    {AudioFileFormat::AIFF, true, pcmFormats}, {AudioFileFormat::FLAC, true, flacFormats},
    {AudioFileFormat::MP3, false, {}},         {AudioFileFormat::OGG, false, {}}};
} // namespace

#if defined(ORPHEUS_AUDIO_FILE_CAPABILITIES_HAVE_SNDFILE)
int sndfileFormatFor(AudioFileFormat format, AudioSampleFormat sampleFormat) noexcept {
  int major = 0;
  switch (format) {
  case AudioFileFormat::WAV:
    major = SF_FORMAT_WAV;
    break;
  case AudioFileFormat::AIFF:
    major = SF_FORMAT_AIFF;
    break;
  case AudioFileFormat::FLAC:
    major = SF_FORMAT_FLAC;
    break;
  default:
    return 0;
  }
  switch (sampleFormat) {
  case AudioSampleFormat::Int16:
    return major | SF_FORMAT_PCM_16;
  case AudioSampleFormat::Int24:
    return major | SF_FORMAT_PCM_24;
  case AudioSampleFormat::Float32:
    return format == AudioFileFormat::FLAC ? 0 : major | SF_FORMAT_FLOAT;
  default:
    return 0;
  }
}

AudioFileFormat audioFileFormatFromSndfile(int format) noexcept {
  switch (format & SF_FORMAT_TYPEMASK) {
  case SF_FORMAT_WAV:
    return AudioFileFormat::WAV;
  case SF_FORMAT_AIFF:
    return AudioFileFormat::AIFF;
  case SF_FORMAT_FLAC:
    return AudioFileFormat::FLAC;
  default:
    return AudioFileFormat::Unknown;
  }
}

uint16_t audioFileBitDepthFromSndfile(int format) noexcept {
  switch (format & SF_FORMAT_SUBMASK) {
  case SF_FORMAT_PCM_16:
    return 16;
  case SF_FORMAT_PCM_24:
    return 24;
  case SF_FORMAT_PCM_32:
  case SF_FORMAT_FLOAT:
    return 32;
  default:
    return 0;
  }
}

const char* audioFileCodecFromSndfile(int format) noexcept {
  switch (format & SF_FORMAT_SUBMASK) {
  case SF_FORMAT_PCM_16:
    return "PCM_16";
  case SF_FORMAT_PCM_24:
    return "PCM_24";
  case SF_FORMAT_PCM_32:
    return "PCM_32";
  case SF_FORMAT_FLOAT:
    return "FLOAT";
  default:
    return "UNKNOWN";
  }
}

AudioFileMetadata audioFileMetadataFromSndfile(const SF_INFO& info) {
  AudioFileMetadata metadata{};
  metadata.format = audioFileFormatFromSndfile(info.format);
  metadata.sample_rate = static_cast<uint32_t>(info.samplerate);
  metadata.num_channels = static_cast<uint16_t>(info.channels);
  metadata.duration_samples = info.frames;
  metadata.bit_depth = audioFileBitDepthFromSndfile(info.format);
  metadata.codec = audioFileCodecFromSndfile(info.format);
  return metadata;
}
#endif

AudioFileCapabilities getAudioFileCapabilities() noexcept {
#if defined(ORPHEUS_AUDIO_FILE_CAPABILITIES_HAVE_SNDFILE)
  return {true, codecs};
#else
  return {false, codecs};
#endif
}

SessionGraphError preflightAudioFileWrite(const AudioFileWriterConfig& config) noexcept {
  if (config.sample_rate == 0 || config.sample_rate > static_cast<uint32_t>(INT_MAX) ||
      config.num_channels == 0) {
    return SessionGraphError::InvalidParameter;
  }
  if (config.format != AudioFileFormat::WAV && config.format != AudioFileFormat::AIFF &&
      config.format != AudioFileFormat::FLAC) {
    return SessionGraphError::NotSupported;
  }
  if (config.sample_format != AudioSampleFormat::Int16 &&
      config.sample_format != AudioSampleFormat::Int24 &&
      !(config.sample_format == AudioSampleFormat::Float32 &&
        config.format != AudioFileFormat::FLAC)) {
    return SessionGraphError::InvalidParameter;
  }
#if defined(ORPHEUS_AUDIO_FILE_CAPABILITIES_HAVE_SNDFILE)
  // sf_format_check accepts rates that the FLAC encoder cannot initialize.
  if (config.format == AudioFileFormat::FLAC && config.sample_rate > 655350)
    return SessionGraphError::InvalidParameter;
  SF_INFO info{};
  info.samplerate = static_cast<int>(config.sample_rate);
  info.channels = config.num_channels;
  info.format = sndfileFormatFor(config.format, config.sample_format);
  return sf_format_check(&info) ? SessionGraphError::OK : SessionGraphError::InvalidParameter;
#else
  return SessionGraphError::NotReady;
#endif
}

Result<AudioFileMetadata> probeAudioFile(const std::string& path) {
  Result<AudioFileMetadata> result{};
  if (path.empty()) {
    result.error = SessionGraphError::InvalidParameter;
    result.errorMessage = "Audio file path must not be empty";
    return result;
  }
#if defined(ORPHEUS_AUDIO_FILE_CAPABILITIES_HAVE_SNDFILE)
  SF_INFO info{};
  SNDFILE* file = sf_open(path.c_str(), SFM_READ, &info);
  if (!file) {
    result.error = SessionGraphError::InternalError;
    result.errorMessage = "Failed to open audio file: " + std::string(sf_strerror(nullptr));
    return result;
  }
  // No decoded sample buffers or media hashing: release the header handle before metadata strings.
  const int closeResult = sf_close(file);
  if (info.frames <= 0 || info.samplerate <= 0 || info.channels <= 0 ||
      info.channels > UINT16_MAX) {
    result.error = SessionGraphError::InvalidParameter;
    result.errorMessage = "Invalid audio file format";
    return result;
  }
  if (closeResult != 0) {
    result.error = SessionGraphError::InternalError;
    result.errorMessage = "Failed to close audio file after probing";
    return result;
  }
  result.value = audioFileMetadataFromSndfile(info);
  result.error = SessionGraphError::OK;
#else
  result.error = SessionGraphError::NotReady;
  result.errorMessage = "Audio file provider is unavailable";
#endif
  return result;
}
} // namespace orpheus
