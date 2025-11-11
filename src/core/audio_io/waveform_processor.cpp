// SPDX-License-Identifier: MIT
#include <orpheus/audio_file_reader_extended.h>

#include "audio_file_reader_libsndfile.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <mutex>
#include <thread>
#include <vector>

namespace orpheus {

/// Extended audio file reader with waveform pre-processing
///
/// Implementation strategy:
/// - Downsampling: For each pixel, read samplesPerPixel samples and find min/max
/// - Caching: Store peak levels per channel (computed once)
/// - Multi-threading: precomputeWaveformAsync() spawns background thread
/// - Memory optimization: For large files, use streaming reads (no full buffer load)
class AudioFileReaderExtended : public IAudioFileReaderExtended {
public:
  AudioFileReaderExtended() : m_base_reader(std::make_unique<AudioFileReaderLibsndfile>()) {}

  ~AudioFileReaderExtended() override {
    // Wait for background thread to finish
    if (m_precompute_thread.joinable()) {
      m_precompute_thread.join();
    }
  }

  // Forward IAudioFileReader interface to base implementation
  Result<AudioFileMetadata> open(const std::string& file_path) override {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto result = m_base_reader->open(file_path);
    if (result.isOk()) {
      m_metadata = result.value;
      // Reset cached data
      m_peak_levels.clear();
      m_peak_levels.resize(m_metadata.num_channels, -1.0f); // -1.0 = not computed
    }
    return result;
  }

  Result<size_t> readSamples(float* buffer, size_t num_samples) override {
    return m_base_reader->readSamples(buffer, num_samples);
  }

  SessionGraphError seek(int64_t sample_position) override {
    return m_base_reader->seek(sample_position);
  }

  void close() override {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Wait for background thread if running
    if (m_precompute_thread.joinable()) {
      m_precompute_thread.join();
    }

    m_base_reader->close();
    m_peak_levels.clear();
  }

  int64_t getCurrentPosition() const override {
    return m_base_reader->getCurrentPosition();
  }

  bool isOpen() const override {
    return m_base_reader->isOpen();
  }

  // IAudioFileReaderExtended interface
  WaveformData getWaveformData(int64_t startSample, int64_t endSample, uint32_t pixelWidth,
                               uint32_t channelIndex) override {
    WaveformData result;
    result.startSample = startSample;
    result.endSample = endSample;
    result.pixelWidth = 0; // Mark as invalid by default
    result.channelIndex = channelIndex;

    // Validate parameters
    if (!m_base_reader->isOpen()) {
      return result; // Empty result
    }

    if (channelIndex >= m_metadata.num_channels) {
      return result; // Invalid channel
    }

    if (pixelWidth == 0 || startSample < 0 || endSample <= startSample) {
      return result; // Invalid range - keep pixelWidth = 0 to mark as invalid
    }

    // Now set pixelWidth since parameters are valid
    result.pixelWidth = pixelWidth;

    // Clamp to file bounds
    if (endSample > m_metadata.duration_samples) {
      endSample = m_metadata.duration_samples;
    }

    // Use optimized streaming approach: read entire range once and compute pixels
    return computeWaveformStreaming(startSample, endSample, pixelWidth, channelIndex);
  }

  float getPeakLevel(uint32_t channelIndex) override {
    if (!m_base_reader->isOpen()) {
      return 0.0f;
    }

    if (channelIndex >= m_metadata.num_channels) {
      return 0.0f;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Return cached value if available
    if (m_peak_levels[channelIndex] >= 0.0f) {
      return m_peak_levels[channelIndex];
    }

    // Compute peak level for entire file
    float peak = computePeakLevelForChannel(channelIndex);
    m_peak_levels[channelIndex] = peak;

    return peak;
  }

  void precomputeWaveformAsync(std::function<void()> callback) override {
    if (!m_base_reader->isOpen()) {
      if (callback) {
        callback(); // Call immediately if not open
      }
      return;
    }

    // Check if already running
    if (m_precompute_running.load(std::memory_order_acquire)) {
      if (callback) {
        callback(); // Already running, call callback immediately
      }
      return;
    }

    // Wait for previous thread if exists
    if (m_precompute_thread.joinable()) {
      m_precompute_thread.join();
    }

    // Spawn background thread
    m_precompute_running.store(true, std::memory_order_release);

    m_precompute_thread = std::thread([this, callback]() {
      // Pre-compute peak levels for all channels
      for (uint32_t ch = 0; ch < m_metadata.num_channels; ++ch) {
        getPeakLevel(ch); // This will cache the result
      }

      // TODO: Pre-compute LOD pyramid at multiple resolutions
      // For now, we just compute peak levels
      // Future enhancement: Store waveform data at 100px, 400px, 1600px, etc.

      m_precompute_running.store(false, std::memory_order_release);

      // Call callback on completion
      if (callback) {
        callback();
      }
    });
  }

private:
  /// Optimized streaming waveform computation
  /// Reads entire range once and computes all pixels in a single pass
  WaveformData computeWaveformStreaming(int64_t startSample, int64_t endSample, uint32_t pixelWidth,
                                        uint32_t channelIndex) {
    WaveformData result;
    result.startSample = startSample;
    result.endSample = endSample;
    result.pixelWidth = pixelWidth;
    result.channelIndex = channelIndex;
    result.minPeaks.resize(pixelWidth, std::numeric_limits<float>::max());
    result.maxPeaks.resize(pixelWidth, std::numeric_limits<float>::lowest());

    // Seek to start position
    SessionGraphError seekErr = m_base_reader->seek(startSample);
    if (seekErr != SessionGraphError::OK) {
      // Fill with zeros on error
      std::fill(result.minPeaks.begin(), result.minPeaks.end(), 0.0f);
      std::fill(result.maxPeaks.begin(), result.maxPeaks.end(), 0.0f);
      return result;
    }

    int64_t totalSamples = endSample - startSample;
    double samplesPerPixel = static_cast<double>(totalSamples) / static_cast<double>(pixelWidth);

    // Read in large chunks for better I/O performance
    const size_t CHUNK_SIZE = 32768; // 32K frames at a time
    std::vector<float> buffer(CHUNK_SIZE * m_metadata.num_channels);

    int64_t samplesProcessed = 0;
    while (samplesProcessed < totalSamples) {
      size_t samplesToRead = static_cast<size_t>(
          std::min(static_cast<int64_t>(CHUNK_SIZE), totalSamples - samplesProcessed));

      auto readResult = m_base_reader->readSamples(buffer.data(), samplesToRead);
      if (!readResult.isOk() || readResult.value == 0) {
        break; // EOF or error
      }

      size_t framesRead = readResult.value;

      // Process each sample and update corresponding pixel
      for (size_t i = 0; i < framesRead; ++i) {
        int64_t globalSampleIndex = samplesProcessed + static_cast<int64_t>(i);
        uint32_t pixelIndex = static_cast<uint32_t>(globalSampleIndex / samplesPerPixel);

        if (pixelIndex >= pixelWidth) {
          pixelIndex = pixelWidth - 1; // Clamp to last pixel
        }

        float sample = buffer[i * m_metadata.num_channels + channelIndex];
        result.minPeaks[pixelIndex] = std::min(result.minPeaks[pixelIndex], sample);
        result.maxPeaks[pixelIndex] = std::max(result.maxPeaks[pixelIndex], sample);
      }

      samplesProcessed += framesRead;
    }

    // Handle pixels that had no samples (edge case)
    for (uint32_t i = 0; i < pixelWidth; ++i) {
      if (result.minPeaks[i] == std::numeric_limits<float>::max()) {
        result.minPeaks[i] = 0.0f;
        result.maxPeaks[i] = 0.0f;
      }
    }

    return result;
  }

  /// Compute peak level for entire file (single channel)
  float computePeakLevelForChannel(uint32_t channelIndex) {
    // Save current position
    int64_t originalPosition = m_base_reader->getCurrentPosition();

    // Seek to start
    SessionGraphError seekErr = m_base_reader->seek(0);
    if (seekErr != SessionGraphError::OK) {
      return 0.0f;
    }

    // Read entire file in chunks and find peak
    const size_t CHUNK_SIZE = 8192; // Read 8K frames at a time
    std::vector<float> buffer(CHUNK_SIZE * m_metadata.num_channels);

    float peak = 0.0f;
    int64_t totalSamplesProcessed = 0;

    while (totalSamplesProcessed < m_metadata.duration_samples) {
      size_t samplesToRead = static_cast<size_t>(std::min(
          static_cast<int64_t>(CHUNK_SIZE), m_metadata.duration_samples - totalSamplesProcessed));

      auto result = m_base_reader->readSamples(buffer.data(), samplesToRead);
      if (!result.isOk() || result.value == 0) {
        break; // EOF or error
      }

      size_t framesRead = result.value;

      // Extract channel data and find peak
      for (size_t i = 0; i < framesRead; ++i) {
        float sample = buffer[i * m_metadata.num_channels + channelIndex];
        peak = std::max(peak, std::abs(sample));
      }

      totalSamplesProcessed += framesRead;
    }

    // Restore original position
    m_base_reader->seek(originalPosition);

    return peak;
  }

  std::unique_ptr<AudioFileReaderLibsndfile> m_base_reader;
  AudioFileMetadata m_metadata;

  // Caching
  std::vector<float> m_peak_levels; ///< Cached peak levels per channel (-1.0 = not computed)
  std::mutex m_mutex;               ///< Protects peak_levels and precompute state

  // Background precomputation
  std::thread m_precompute_thread;
  std::atomic<bool> m_precompute_running{false};
};

// Factory function
std::unique_ptr<IAudioFileReaderExtended> createAudioFileReaderExtended() {
  return std::make_unique<AudioFileReaderExtended>();
}

} // namespace orpheus
