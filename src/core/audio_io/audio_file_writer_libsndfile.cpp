// SPDX-License-Identifier: MIT
#include "audio_file_writer_libsndfile.h"

#include <cstring>

namespace orpheus {

AudioFileWriterLibsndfile::AudioFileWriterLibsndfile() : m_file(nullptr) {
  std::memset(&m_info, 0, sizeof(m_info));
}

AudioFileWriterLibsndfile::~AudioFileWriterLibsndfile() {
  close();
}

int AudioFileWriterLibsndfile::sndfileFormatFor(AudioFileFormat format,
                                                AudioSampleFormat sampleFormat) {
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
    return 0; // Unsupported container
  }

  int sub = 0;
  switch (sampleFormat) {
  case AudioSampleFormat::Int16:
    sub = SF_FORMAT_PCM_16;
    break;
  case AudioSampleFormat::Int24:
    sub = SF_FORMAT_PCM_24;
    break;
  case AudioSampleFormat::Float32:
    // FLAC is integer-only; float subformat is invalid there.
    if (format == AudioFileFormat::FLAC) {
      return 0;
    }
    sub = SF_FORMAT_FLOAT;
    break;
  default:
    return 0;
  }

  return major | sub;
}

SessionGraphError AudioFileWriterLibsndfile::open(const std::string& file_path,
                                                  const AudioFileWriterConfig& config) {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Close any previously open file first.
  if (m_file) {
    sf_close(m_file);
    m_file = nullptr;
    m_is_open.store(false, std::memory_order_release);
  }

  if (file_path.empty() || config.sample_rate == 0 || config.num_channels == 0) {
    return SessionGraphError::InvalidParameter;
  }

  // Distinguish "unsupported container" from "invalid combination".
  const bool containerSupported = config.format == AudioFileFormat::WAV ||
                                  config.format == AudioFileFormat::AIFF ||
                                  config.format == AudioFileFormat::FLAC;
  if (!containerSupported) {
    return SessionGraphError::NotSupported;
  }

  const int sfFormat = sndfileFormatFor(config.format, config.sample_format);
  if (sfFormat == 0) {
    return SessionGraphError::InvalidParameter; // e.g. FLAC + Float32
  }

  std::memset(&m_info, 0, sizeof(m_info));
  m_info.samplerate = static_cast<int>(config.sample_rate);
  m_info.channels = static_cast<int>(config.num_channels);
  m_info.format = sfFormat;

  if (!sf_format_check(&m_info)) {
    return SessionGraphError::InvalidParameter;
  }

  m_file = sf_open(file_path.c_str(), SFM_WRITE, &m_info);
  if (!m_file) {
    return SessionGraphError::InternalError;
  }

  // Saturate out-of-range floats when encoding to integer PCM instead of
  // letting them wrap (libsndfile default float->int conversion can wrap).
  sf_command(m_file, SFC_SET_CLIPPING, nullptr, SF_TRUE);

  // libsndfile adds a PEAK chunk to float WAV/AIFF by default, and that chunk
  // embeds a wall-clock timestamp — the same input would produce different
  // bytes on every run, violating the SDK's bit-identical determinism
  // guarantee (and FourTrack's byte-identical export gate, FTR025). Peak
  // metadata is a nicety, determinism is a contract: disable it.
  sf_command(m_file, SFC_SET_ADD_PEAK_CHUNK, nullptr, SF_FALSE);

  m_file_path = file_path;
  m_frames_written.store(0, std::memory_order_release);

  m_metadata = AudioFileMetadata{};
  m_metadata.format = config.format;
  m_metadata.sample_rate = config.sample_rate;
  m_metadata.num_channels = config.num_channels;
  m_metadata.duration_samples = 0;
  switch (config.sample_format) {
  case AudioSampleFormat::Int16:
    m_metadata.bit_depth = 16;
    m_metadata.codec = "PCM";
    break;
  case AudioSampleFormat::Int24:
    m_metadata.bit_depth = 24;
    m_metadata.codec = "PCM";
    break;
  case AudioSampleFormat::Float32:
    m_metadata.bit_depth = 32;
    m_metadata.codec = "Float";
    break;
  }
  if (config.format == AudioFileFormat::FLAC) {
    m_metadata.codec = "FLAC";
  }

  m_is_open.store(true, std::memory_order_release);
  return SessionGraphError::OK;
}

Result<size_t> AudioFileWriterLibsndfile::writeSamples(const float* buffer, size_t num_frames) {
  std::lock_guard<std::mutex> lock(m_mutex);

  Result<size_t> result;
  if (!m_file || !m_is_open.load(std::memory_order_acquire)) {
    result.error = SessionGraphError::NotReady;
    result.errorMessage = "Writer is not open";
    result.value = 0;
    return result;
  }
  if (buffer == nullptr && num_frames > 0) {
    result.error = SessionGraphError::InvalidParameter;
    result.errorMessage = "Null buffer";
    result.value = 0;
    return result;
  }

  const sf_count_t written = sf_writef_float(m_file, buffer, static_cast<sf_count_t>(num_frames));
  if (written < 0 || (static_cast<size_t>(written) != num_frames)) {
    result.error = SessionGraphError::InternalError;
    result.errorMessage = "Short write: " + std::string(sf_strerror(m_file));
    result.value = written > 0 ? static_cast<size_t>(written) : 0;
    return result;
  }

  const int64_t total = m_frames_written.fetch_add(written, std::memory_order_acq_rel) + written;
  m_metadata.duration_samples = total;

  result.error = SessionGraphError::OK;
  result.value = static_cast<size_t>(written);
  return result;
}

SessionGraphError AudioFileWriterLibsndfile::close() {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (!m_file) {
    return SessionGraphError::OK; // Already closed — idempotent
  }

  // sf_close() flushes all encoder state through to the OS; it deliberately
  // does NOT fsync. Durability ordering is host policy — hosts with an
  // explicit fsync discipline (e.g. FourTrack's FTR002 commit-marker order)
  // fsync at their own commit points. An unconditional sf_write_sync here
  // put a multi-millisecond blocking fsync inside every close, which hosts
  // cannot opt out of and which broke FourTrack's sub-ms transport
  // transition bound the moment close() sat on a record-stop path.
  const int err = sf_close(m_file);
  m_file = nullptr;
  m_is_open.store(false, std::memory_order_release);

  return err == 0 ? SessionGraphError::OK : SessionGraphError::InternalError;
}

int64_t AudioFileWriterLibsndfile::getFramesWritten() const {
  return m_frames_written.load(std::memory_order_acquire);
}

bool AudioFileWriterLibsndfile::isOpen() const {
  return m_is_open.load(std::memory_order_acquire);
}

AudioFileMetadata AudioFileWriterLibsndfile::metadata() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  AudioFileMetadata meta = m_metadata;
  meta.duration_samples = m_frames_written.load(std::memory_order_acquire);
  return meta;
}

// Factory function (libsndfile build)
std::unique_ptr<IAudioFileWriter> createAudioFileWriter() {
  return std::make_unique<AudioFileWriterLibsndfile>();
}

} // namespace orpheus
