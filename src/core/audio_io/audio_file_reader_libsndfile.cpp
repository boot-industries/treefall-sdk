// SPDX-License-Identifier: MIT
#include "audio_file_reader_libsndfile.h"

#include <cstring>
#include <sstream>

namespace orpheus {

AudioFileReaderLibsndfile::AudioFileReaderLibsndfile() : m_file(nullptr) {
  std::memset(&m_info, 0, sizeof(m_info));
  std::memset(&m_metadata, 0, sizeof(m_metadata));
}

AudioFileReaderLibsndfile::~AudioFileReaderLibsndfile() {
  close();
}

Result<AudioFileMetadata> AudioFileReaderLibsndfile::open(const std::string& file_path) {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Close any previously open file
  if (m_file) {
    sf_close(m_file);
    m_file = nullptr;
  }

  // Open file
  std::memset(&m_info, 0, sizeof(m_info));
  m_file = sf_open(file_path.c_str(), SFM_READ, &m_info);

  if (!m_file) {
    Result<AudioFileMetadata> result;
    result.error = SessionGraphError::InternalError;
    result.errorMessage = "Failed to open audio file: " + std::string(sf_strerror(nullptr));
    m_is_open.store(false, std::memory_order_release);
    return result;
  }

  // Validate format
  if (m_info.frames <= 0 || m_info.samplerate <= 0 || m_info.channels <= 0) {
    sf_close(m_file);
    m_file = nullptr;

    Result<AudioFileMetadata> result;
    result.error = SessionGraphError::InvalidParameter;
    result.errorMessage = "Invalid audio file format";
    m_is_open.store(false, std::memory_order_release);
    return result;
  }

  // Fill metadata
  m_metadata.format = formatFromSndfile(m_info.format);
  m_metadata.sample_rate = static_cast<uint32_t>(m_info.samplerate);
  m_metadata.num_channels = static_cast<uint16_t>(m_info.channels);
  m_metadata.duration_samples = m_info.frames;
  m_metadata.codec = codecFromSndfile(m_info.format);
  m_metadata.file_hash_sha256 = calculateFileHash(file_path);

  // Determine bit depth (approximate from format)
  int subformat = m_info.format & SF_FORMAT_SUBMASK;
  if (subformat == SF_FORMAT_PCM_16) {
    m_metadata.bit_depth = 16;
  } else if (subformat == SF_FORMAT_PCM_24) {
    m_metadata.bit_depth = 24;
  } else if (subformat == SF_FORMAT_PCM_32 || subformat == SF_FORMAT_FLOAT) {
    m_metadata.bit_depth = 32;
  } else {
    m_metadata.bit_depth = 16; // Default
  }

  m_file_path = file_path;
  m_current_position.store(0, std::memory_order_release);
  m_is_open.store(true, std::memory_order_release);

  Result<AudioFileMetadata> result;
  result.value = m_metadata;
  result.error = SessionGraphError::OK;
  return result;
}

Result<size_t> AudioFileReaderLibsndfile::readSamples(float* buffer, size_t num_samples) {
  // NOTE: NO MUTEX LOCK HERE - this is called from the audio thread
  // We assume that each reader instance is only accessed from ONE audio thread
  // and that open/close/seek are NOT called while audio is playing
  // This is enforced by the TransportController design

  Result<size_t> result;

  if (!m_file) {
    result.error = SessionGraphError::NotReady;
    result.errorMessage = "File not open";
    result.value = 0;
    return result;
  }

  // Read interleaved samples (libsndfile maintains internal state)
  sf_count_t read = sf_readf_float(m_file, buffer, static_cast<sf_count_t>(num_samples));

  if (read < 0) {
    result.error = SessionGraphError::InternalError;
    result.errorMessage = "Failed to read samples: " + std::string(sf_strerror(m_file));
    result.value = 0;
    return result;
  }

  // Update position atomically
  int64_t new_position = m_current_position.load(std::memory_order_relaxed) + read;
  m_current_position.store(new_position, std::memory_order_release);

  result.error = SessionGraphError::OK;
  result.value = static_cast<size_t>(read);
  return result;
}

SessionGraphError AudioFileReaderLibsndfile::seek(int64_t sample_position) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (!m_file) {
    return SessionGraphError::NotReady;
  }

  // Clamp to valid range
  if (sample_position < 0) {
    sample_position = 0;
  }
  if (sample_position > m_info.frames) {
    sample_position = m_info.frames;
  }

  // Seek
  sf_count_t result = sf_seek(m_file, sample_position, SEEK_SET);
  if (result < 0) {
    return SessionGraphError::InternalError;
  }

  m_current_position.store(sample_position, std::memory_order_release);
  return SessionGraphError::OK;
}

void AudioFileReaderLibsndfile::close() {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_file) {
    sf_close(m_file);
    m_file = nullptr;
  }

  m_is_open.store(false, std::memory_order_release);
  m_current_position.store(0, std::memory_order_release);
}

int64_t AudioFileReaderLibsndfile::getCurrentPosition() const {
  return m_current_position.load(std::memory_order_acquire);
}

bool AudioFileReaderLibsndfile::isOpen() const {
  return m_is_open.load(std::memory_order_acquire);
}

AudioFileFormat AudioFileReaderLibsndfile::formatFromSndfile(int format) const {
  int major = format & SF_FORMAT_TYPEMASK;

  switch (major) {
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

std::string AudioFileReaderLibsndfile::codecFromSndfile(int format) const {
  int subformat = format & SF_FORMAT_SUBMASK;

  switch (subformat) {
  case SF_FORMAT_PCM_16:
    return "PCM_16";
  case SF_FORMAT_PCM_24:
    return "PCM_24";
  case SF_FORMAT_PCM_32:
    return "PCM_32";
  case SF_FORMAT_FLOAT:
    return "FLOAT";
  case SF_FORMAT_FLAC:
    return "FLAC";
  default:
    return "UNKNOWN";
  }
}

std::string AudioFileReaderLibsndfile::calculateFileHash(const std::string& file_path) const {
  // TODO: Implement SHA-256 hashing
  // For now, return placeholder
  (void)file_path;
  return "SHA256_NOT_IMPLEMENTED";
}

// Factory function
std::unique_ptr<IAudioFileReader> createAudioFileReader() {
  return std::make_unique<AudioFileReaderLibsndfile>();
}

} // namespace orpheus
