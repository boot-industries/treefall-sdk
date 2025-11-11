// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/audio_file_reader.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace orpheus {

/// Waveform data for UI rendering
///
/// Contains downsampled min/max peaks per pixel for efficient waveform display.
/// Each pixel in the UI corresponds to one entry in minPeaks/maxPeaks arrays.
///
/// Thread Safety: This structure is intended to be created on a background thread
/// and passed to the UI thread for rendering. Do not modify after passing to UI.
struct WaveformData {
  std::vector<float> minPeaks; ///< Minimum sample values per pixel (range: -1.0 to 1.0)
  std::vector<float> maxPeaks; ///< Maximum sample values per pixel (range: -1.0 to 1.0)
  uint32_t pixelWidth;         ///< Number of pixels (samples per pixel varies)
  uint32_t channelIndex;       ///< Channel this data represents (0 = left, 1 = right, etc.)
  int64_t startSample;         ///< First sample in range (0-based, inclusive)
  int64_t endSample;           ///< Last sample in range (0-based, exclusive)

  /// Validate waveform data consistency
  /// @return true if data is valid, false if inconsistent
  bool isValid() const {
    return pixelWidth > 0 && minPeaks.size() == pixelWidth && maxPeaks.size() == pixelWidth &&
           startSample >= 0 && endSample > startSample;
  }

  /// Get samples per pixel
  /// @return Number of samples represented by each pixel
  int64_t samplesPerPixel() const {
    if (pixelWidth == 0)
      return 0;
    return (endSample - startSample) / pixelWidth;
  }
};

/// Extended audio file reader with waveform pre-processing
///
/// Extends IAudioFileReader with methods for efficient waveform extraction
/// for UI rendering. Provides downsampled peak data suitable for direct
/// OpenGL/Canvas rendering.
///
/// Thread Safety:
/// - open(), close(): Must be called from background/UI thread (NOT audio thread)
/// - getWaveformData(): Can be called from background thread (may block 10-100ms)
/// - getPeakLevel(): Thread-safe, can be called from any thread
/// - precomputeWaveformAsync(): Thread-safe, spawns background thread
///
/// Performance:
/// - getWaveformData() for 10-minute WAV → 800px should complete in <100ms
/// - getPeakLevel() is cached after first computation
/// - precomputeWaveformAsync() enables instant subsequent queries
///
/// Typical Usage:
/// 1. Open file on background thread
/// 2. Call precomputeWaveformAsync() to pre-cache waveform data
/// 3. Query waveform data for UI at different zoom levels
/// 4. Use getPeakLevel() for normalization
class IAudioFileReaderExtended : public IAudioFileReader {
public:
  ~IAudioFileReaderExtended() override = default;

  /// Generate waveform data for UI rendering
  ///
  /// Reads the specified sample range and downsamples to pixelWidth pixels
  /// by computing min/max peaks for each pixel. This is suitable for direct
  /// rendering in a waveform display.
  ///
  /// @param startSample Start of range (0-based, inclusive)
  /// @param endSample End of range (0-based, exclusive)
  /// @param pixelWidth Target width in pixels (1-10000 typical)
  /// @param channelIndex Channel to extract (0 = left, 1 = right, etc.)
  /// @return Waveform data (min/max peaks per pixel)
  ///
  /// @note This may block (10-100ms for long files), call on background thread
  /// @note For best performance, call precomputeWaveformAsync() first
  /// @note Returns empty WaveformData if file not open or parameters invalid
  ///
  /// Example:
  /// @code
  /// // Get waveform for entire file at 800px width
  /// auto metadata = reader->open("audio.wav");
  /// auto waveform = reader->getWaveformData(
  ///     0, metadata.value.duration_samples, 800, 0);
  /// // Render waveform.minPeaks and waveform.maxPeaks in UI
  /// @endcode
  virtual WaveformData getWaveformData(int64_t startSample, int64_t endSample, uint32_t pixelWidth,
                                       uint32_t channelIndex) = 0;

  /// Get peak level for entire file (for normalization)
  ///
  /// Returns the maximum absolute sample value in the file for the specified
  /// channel. Useful for normalizing waveform display to fit in UI bounds.
  ///
  /// @param channelIndex Channel to analyze (0 = left, 1 = right, etc.)
  /// @return Peak absolute value (0.0 to 1.0+, typically 0.0-1.0 for normalized audio)
  ///
  /// @note Result is cached after first computation
  /// @note Returns 0.0 if file not open or channel index invalid
  /// @note Thread-safe (uses internal caching)
  ///
  /// Example:
  /// @code
  /// float peak = reader->getPeakLevel(0);  // Get left channel peak
  /// // Scale waveform display by 1.0 / peak to normalize
  /// @endcode
  virtual float getPeakLevel(uint32_t channelIndex) = 0;

  /// Pre-compute waveform data on background thread
  ///
  /// Spawns a background thread to pre-process the entire file and cache
  /// waveform data at multiple resolutions (LOD pyramid). This enables
  /// instant subsequent getWaveformData() calls at any zoom level.
  ///
  /// @param callback Called when pre-processing complete (optional)
  ///
  /// @note This spawns background thread, returns immediately
  /// @note Safe to call multiple times (subsequent calls are no-op)
  /// @note getWaveformData() works without this, but will be slower
  ///
  /// Example:
  /// @code
  /// reader->precomputeWaveformAsync([this]() {
  ///     // Update UI to indicate waveform is ready
  ///     updateWaveformDisplay();
  /// });
  /// @endcode
  virtual void precomputeWaveformAsync(std::function<void()> callback) = 0;
};

/// Create extended audio file reader
///
/// Creates an audio file reader with waveform pre-processing capabilities.
/// Uses libsndfile for decoding (supports WAV, AIFF, FLAC).
///
/// @return Unique pointer to extended audio file reader
///
/// Example:
/// @code
/// auto reader = createAudioFileReaderExtended();
/// auto result = reader->open("audio.wav");
/// if (result.isOk()) {
///     auto waveform = reader->getWaveformData(0, result.value.duration_samples, 800, 0);
///     // Render waveform in UI
/// }
/// @endcode
std::unique_ptr<IAudioFileReaderExtended> createAudioFileReaderExtended();

} // namespace orpheus
