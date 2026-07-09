// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/audio_file_writer.h>

#include <atomic>
#include <mutex>
#include <sndfile.h>
#include <string>

namespace orpheus {

/// Audio file writer implementation using libsndfile (ORP134 G5 / FTR007).
///
/// Supports WAV, AIFF (Int16/Int24/Float32) and FLAC (Int16/Int24).
class AudioFileWriterLibsndfile : public IAudioFileWriter {
public:
  AudioFileWriterLibsndfile();
  ~AudioFileWriterLibsndfile() override;

  // IAudioFileWriter interface
  SessionGraphError open(const std::string& file_path,
                         const AudioFileWriterConfig& config) override;
  Result<size_t> writeSamples(const float* buffer, size_t num_frames) override;
  SessionGraphError close() override;
  int64_t getFramesWritten() const override;
  bool isOpen() const override;
  AudioFileMetadata metadata() const override;

private:
  /// Map (container, sample format) to a libsndfile format word.
  /// Returns 0 for unsupported combinations.
  static int sndfileFormatFor(AudioFileFormat format, AudioSampleFormat sampleFormat);

  // File state
  SNDFILE* m_file;
  SF_INFO m_info;
  AudioFileMetadata m_metadata;
  std::string m_file_path;

  // Thread safety (open/write/close serialized; queries lock-free)
  mutable std::mutex m_mutex;
  std::atomic<int64_t> m_frames_written{0};
  std::atomic<bool> m_is_open{false};
};

} // namespace orpheus
