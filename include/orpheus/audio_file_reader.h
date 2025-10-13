// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <memory>
#include <orpheus/transport_controller.h> // For SessionGraphError
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

/// Result wrapper for operations that can fail
template <typename T> struct Result {
  T value;                  ///< The result value (valid if error == OK)
  SessionGraphError error;  ///< Error code
  std::string errorMessage; ///< Human-readable error message

  /// Check if operation succeeded
  bool isOk() const {
    return error == SessionGraphError::OK;
  }

  /// Get value (throws if error)
  T& operator*() {
    if (!isOk()) {
      throw std::runtime_error("Result error: " + errorMessage);
    }
    return value;
  }

  const T& operator*() const {
    if (!isOk()) {
      throw std::runtime_error("Result error: " + errorMessage);
    }
    return value;
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
std::unique_ptr<IAudioFileReader> createAudioFileReader();

} // namespace orpheus
