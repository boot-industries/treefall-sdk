// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/audio_file_reader.h>

#include <atomic>
#include <mutex>
#include <sndfile.h>
#include <string>

namespace orpheus {

/// Audio file reader implementation using libsndfile
///
/// Supports WAV, AIFF, FLAC, and other formats via libsndfile.
class AudioFileReaderLibsndfile : public IAudioFileReader {
public:
  AudioFileReaderLibsndfile();
  ~AudioFileReaderLibsndfile() override;

  // IAudioFileReader interface
  Result<AudioFileMetadata> open(const std::string& file_path) override;
  Result<size_t> readSamples(float* buffer, size_t num_samples) override;
  SessionGraphError seek(int64_t sample_position) override;
  void close() override;
  int64_t getCurrentPosition() const override;
  bool isOpen() const override;

private:
  /// Convert libsndfile format to our enum
  AudioFileFormat formatFromSndfile(int format) const;

  /// Convert libsndfile format to codec string
  std::string codecFromSndfile(int format) const;

  /// Calculate SHA-256 hash of file (stub for now)
  std::string calculateFileHash(const std::string& file_path) const;

  // File state
  SNDFILE* m_file;
  SF_INFO m_info;
  AudioFileMetadata m_metadata;
  std::string m_file_path;

  // Thread safety
  mutable std::mutex m_mutex;
  std::atomic<int64_t> m_current_position{0};
  std::atomic<bool> m_is_open{false};
};

} // namespace orpheus
