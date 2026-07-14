// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/errors.h>
#include <orpheus/export.h>

#include <cstdint>
#include <memory>
#include <string>

namespace orpheus {

/// Audio file format types
enum class AudioFileFormat : uint8_t {
  Unknown = 0,
  WAV = 1,
  AIFF = 2,
  FLAC = 3,
  MP3 = 4, // Future
  OGG = 5  // Future
};

/// Audio file metadata
struct AudioFileMetadata {
  AudioFileFormat format;       ///< File format type
  uint32_t sample_rate;         ///< Sample rate in Hz (e.g., 48000)
  uint16_t num_channels;        ///< Number of audio channels (1=mono, 2=stereo, etc.)
  int64_t duration_samples;     ///< Total duration in sample frames
  uint16_t bit_depth;           ///< Bit depth (16, 24, 32)
  std::string codec;            ///< Codec name (e.g., "PCM", "FLAC")
  std::string file_hash_sha256; ///< SHA-256 hash of file (for integrity verification)

  /// Derived: Duration in seconds
  double durationSeconds() const {
    return static_cast<double>(duration_samples) / static_cast<double>(sample_rate);
  }
};

/// Audio file reader interface
///
/// Provides non-blocking, thread-safe access to audio files.
/// Designed for real-time playback with streaming support.
///
/// Thread Safety:
/// - open(), close(): Must be called from background/UI thread (NOT audio thread)
/// - readSamples(), seek(): Can be called from background thread
/// - getCurrentPosition(): Thread-safe, can be called from any thread
///
/// Typical Usage:
/// 1. Open file on background thread
/// 2. Pre-load initial buffer
/// 3. Stream remaining data in background while audio thread reads from ring buffer
class IAudioFileReader {
public:
  virtual ~IAudioFileReader() = default;

  /// Open an audio file and read metadata
  ///
  /// This function opens the file, validates the format, and extracts metadata.
  /// It should be called from a background or UI thread (NOT audio thread).
  ///
  /// @param file_path Path to audio file (absolute or relative)
  /// @return Result containing metadata, or error
  ///
  /// @note Supports WAV, AIFF, FLAC formats
  /// @note File must exist and be readable
  virtual Result<AudioFileMetadata> open(const std::string& file_path) = 0;

  /// Read audio samples into interleaved buffer
  ///
  /// Reads up to num_samples sample frames into the provided buffer.
  /// Each frame contains num_channels samples (interleaved).
  ///
  /// @param buffer Output buffer (must be at least num_samples * num_channels)
  /// @param num_samples Number of sample frames to read
  /// @return Result containing actual number of frames read, or error
  ///
  /// @note Returns fewer samples than requested at end-of-file
  /// @note Returns 0 when EOF reached
  /// @note Buffer format: [L0, R0, L1, R1, ...] for stereo
  virtual Result<size_t> readSamples(float* buffer, size_t num_samples) = 0;

  /// Read a window of frames ending at (and including) `end_sample`, for
  /// reverse / backward playback (ORP128, FTR018).
  ///
  /// Fills `buffer` with up to `num_frames` interleaved frames whose absolute
  /// frame indices are `[end_sample - num_frames + 1, end_sample]`, written in
  /// FORWARD (ascending-index) order — the caller walks them out in reverse.
  /// Frame indices below 0 are clamped away (the corresponding leading slots of
  /// `buffer` are left as written by the reader; the returned count reflects only
  /// the frames actually read, always the trailing/most-recent frames of the
  /// window). This is the minimal primitive that lets a host stream backward
  /// without buffering forward history itself, lifting the reverse-depth bound of
  /// a fixed local ring.
  ///
  /// The base implementation is a portable seek + read that restores the prior
  /// read position, so every reader supports reverse out of the box; concrete
  /// readers may override with a more efficient path. This is a DEFAULTED virtual
  /// (not pure) precisely so existing readers and consumers keep compiling.
  ///
  /// @param end_sample Absolute frame index of the last (highest-index) frame to
  ///        read. If `end_sample < 0`, nothing is read (returns 0).
  /// @param buffer Output buffer (must hold at least `num_frames * num_channels`
  ///        floats). Frames are written contiguously starting at buffer[0]; when
  ///        the window is clamped at the file start, fewer than `num_frames` are
  ///        written (the caller sees the count in the Result).
  /// @param num_frames Window length in frames.
  /// @return Result containing the number of frames actually read (<= num_frames),
  ///         or an error.
  ///
  /// @note Real-time-safe on readers that pre-reserve scratch; the base
  ///       implementation performs I/O and is intended for background/streaming
  ///       use unless the concrete reader documents otherwise.
  /// @note Restores the read position (`getCurrentPosition()`) it observed on
  ///       entry, so it composes with ordinary forward `readSamples()` streaming.
  virtual Result<size_t> readSamplesEndingAt(int64_t end_sample, float* buffer, size_t num_frames);

  /// Seek to a specific sample position
  ///
  /// Sets the read position for subsequent readSamples() calls.
  ///
  /// @param sample_position Target position in sample frames (0 = start)
  /// @return Error code
  ///
  /// @note Position is clamped to [0, duration_samples]
  /// @note Seeking may be slow for some formats (e.g., compressed)
  virtual SessionGraphError seek(int64_t sample_position) = 0;

  /// Close the audio file
  ///
  /// Releases all resources associated with the file.
  /// Must be called from background/UI thread.
  ///
  /// @note After close(), file must be re-opened before reading
  virtual void close() = 0;

  /// Get current read position
  ///
  /// Thread-safe query of current position.
  ///
  /// @return Current position in sample frames
  virtual int64_t getCurrentPosition() const = 0;

  /// Check if file is currently open
  ///
  /// @return true if file is open and ready to read
  virtual bool isOpen() const = 0;
};

/// Create an audio file reader instance
///
/// Uses libsndfile for decoding (supports WAV, AIFF, FLAC).
///
/// @return Unique pointer to audio file reader
ORPHEUS_API std::unique_ptr<IAudioFileReader> createAudioFileReader();

} // namespace orpheus
