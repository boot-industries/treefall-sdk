// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/audio_file_reader.h> // AudioFileFormat, AudioFileMetadata
#include <orpheus/errors.h>

#include <cstdint>
#include <memory>
#include <string>

namespace orpheus {

/// Sample encoding for written audio files (ORP134 G5 / FTR007).
enum class AudioSampleFormat : uint8_t {
  Int16 = 0,  ///< 16-bit signed PCM
  Int24 = 1,  ///< 24-bit signed PCM
  Float32 = 2 ///< 32-bit IEEE float (not valid for FLAC)
};

/// Configuration for a file to be written.
struct AudioFileWriterConfig {
  AudioFileFormat format = AudioFileFormat::WAV;                ///< Container (WAV/AIFF/FLAC)
  uint32_t sample_rate = 48000;                                 ///< Sample rate in Hz
  uint16_t num_channels = 2;                                    ///< Channel count (interleaved)
  AudioSampleFormat sample_format = AudioSampleFormat::Float32; ///< Sample encoding
};

/// Audio file writer interface (ORP134 G5 — requested by FourTrack as FTR007).
///
/// Mirrors IAudioFileReader's shape and error model: open/write/close with
/// Result<> returns. Streams interleaved float32 frames to disk, encoding to
/// the configured container/sample format.
///
/// Thread Safety (same contract as IAudioFileReader):
/// - open(), close(): background/UI thread only (NOT the audio thread)
/// - writeSamples(): background thread only — writing performs blocking file
///   I/O and must NEVER be called from an audio callback. Real-time capture
///   feeds a lock-free ring buffer that a background writer thread drains
///   into this interface (see the ORP134 G7 recorder primitives).
/// - getFramesWritten(), isOpen(): thread-safe queries
///
/// Format support (libsndfile-backed):
/// - WAV:  Int16 / Int24 / Float32
/// - AIFF: Int16 / Int24 / Float32
/// - FLAC: Int16 / Int24 (Float32 is rejected — FLAC is integer-only)
/// - Other formats (MP3/OGG) are not supported yet and are rejected at open.
class IAudioFileWriter {
public:
  virtual ~IAudioFileWriter() = default;

  /// Create/overwrite the target file and write its header.
  ///
  /// @param file_path Destination path (parent directory must exist)
  /// @param config Container format, sample rate, channels, sample encoding
  /// @return SessionGraphError::OK on success;
  ///         InvalidParameter for a bad config (e.g. FLAC + Float32,
  ///         zero channels/rate); NotSupported for unsupported containers;
  ///         InternalError when the file cannot be created.
  virtual SessionGraphError open(const std::string& file_path,
                                 const AudioFileWriterConfig& config) = 0;

  /// Append interleaved float32 frames.
  ///
  /// Samples are expected in [-1.0, 1.0]; integer encodings clip beyond that
  /// (libsndfile clipping is enabled so out-of-range floats saturate instead
  /// of wrapping).
  ///
  /// @param buffer Interleaved samples (num_frames * num_channels floats)
  /// @param num_frames Number of sample frames to write
  /// @return Result containing frames actually written, or an error
  virtual Result<size_t> writeSamples(const float* buffer, size_t num_frames) = 0;

  /// Finalize the file (flush + header fixup) and release resources.
  ///
  /// @return SessionGraphError::OK on success (also when already closed)
  virtual SessionGraphError close() = 0;

  /// Total frames written since open().
  virtual int64_t getFramesWritten() const = 0;

  /// True while a file is open for writing.
  virtual bool isOpen() const = 0;

  /// Metadata describing the file being written (format, rate, channels,
  /// frames written so far). Valid while open and after close().
  virtual AudioFileMetadata metadata() const = 0;
};

/// Create an audio file writer instance.
///
/// Uses libsndfile for encoding (WAV/AIFF/FLAC). Returns nullptr when the
/// SDK was built without libsndfile — callers must check.
std::unique_ptr<IAudioFileWriter> createAudioFileWriter();

} // namespace orpheus
