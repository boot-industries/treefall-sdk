/*
  ==============================================================================

    AudioAnalyzer.h
    Created: Shmui-to-JUCE Audio Visualization Port

    Core audio analysis engine providing FFT analysis, RMS calculation, and
    frequency band splitting. This serves as the foundation for all visualization
    components (waveform, bar visualizer, orb, matrix).

    Thread-safe design: Audio thread writes, UI thread reads via atomic operations.

  ==============================================================================
*/

#pragma once

#include <array>
#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace shmui {

/**
 * @brief Core audio analysis engine for visualization components.
 *
 * Provides FFT analysis, RMS calculation, and frequency band splitting
 * translated from the shmui React/TypeScript implementation.
 *
 * Thread Safety:
 * - Audio thread calls pushSamples() and processBlock()
 * - UI thread calls get*() methods
 * - Uses lock-free atomic operations for cross-thread communication
 */
class AudioAnalyzer {
public:
  //==============================================================================
  // Critical constants from shmui (tuned values)

  /** FFT order for waveform visualization (256 samples) */
  static constexpr int kWaveformFFTOrder = 8;
  static constexpr int kWaveformFFTSize = 1 << kWaveformFFTOrder; // 256

  /** FFT order for spectrum/bar visualization (2048 samples) */
  static constexpr int kSpectrumFFTOrder = 11;
  static constexpr int kSpectrumFFTSize = 1 << kSpectrumFFTOrder; // 2048

  /** Default smoothing time constant (0.8 for smooth visualization) */
  static constexpr float kDefaultSmoothing = 0.8f;

  /** Frequency range for voice-focused display (5% to 40% of bins) */
  static constexpr float kFrequencyRangeStart = 0.05f;
  static constexpr float kFrequencyRangeEnd = 0.40f;

  /** Exponential smoothing factor for volume */
  static constexpr float kVolumeSmoothingFactor = 0.2f;

  /** dB normalization range */
  static constexpr float kMinDb = -100.0f;
  static constexpr float kMaxDb = -10.0f;

  //==============================================================================

  /**
   * @brief Analysis mode determines FFT size and behavior
   */
  enum class AnalysisMode {
    Waveform, ///< 256-sample FFT for waveform display
    Spectrum  ///< 2048-sample FFT for detailed frequency bands
  };

  //==============================================================================

  /** Create an analyzer with specified mode */
  explicit AudioAnalyzer(AnalysisMode mode = AnalysisMode::Waveform);
  ~AudioAnalyzer() = default;

  //==============================================================================
  // Audio Thread Methods (call from processBlock)

  /**
   * @brief Push audio samples for analysis.
   *
   * Call this from your audio callback with incoming samples.
   * Thread-safe for audio thread.
   *
   * @param samples Pointer to audio samples
   * @param numSamples Number of samples to process
   */
  void pushSamples(const float* samples, int numSamples);

  /**
   * @brief Process a JUCE audio buffer.
   *
   * Convenience method that processes all channels and averages them.
   *
   * @param buffer Audio buffer to process
   */
  void processBlock(const juce::AudioBuffer<float>& buffer);

  //==============================================================================
  // UI Thread Methods (call from paint/timerCallback)

  /**
   * @brief Get normalized frequency data (0-1 range).
   *
   * Returns frequency magnitude data suitable for visualization.
   * Values are smoothed according to smoothingTimeConstant.
   *
   * @param outData Vector to fill with frequency data
   */
  void getFrequencyData(std::vector<float>& outData) const;

  /**
   * @brief Get mirrored frequency data for symmetric display.
   *
   * Creates left/right mirrored data as used in shmui's MicrophoneWaveform.
   *
   * @param outData Vector to fill with mirrored frequency data
   */
  void getMirroredFrequencyData(std::vector<float>& outData) const;

  /**
   * @brief Get current RMS level (0-1 range).
   *
   * Smoothed RMS value suitable for level meters.
   *
   * @return Current smoothed RMS level
   */
  float getRMSLevel() const;

  /**
   * @brief Get current peak level (0-1 range).
   *
   * @return Current peak level
   */
  float getPeakLevel() const;

  /**
   * @brief Split frequency data into bands for bar visualization.
   *
   * Implements the frequency band splitting algorithm from bar-visualizer.tsx.
   *
   * @param outBands Vector to fill with band levels
   * @param numBands Number of frequency bands to create
   * @param loPass Low frequency cutoff bin index (default: 100)
   * @param hiPass High frequency cutoff bin index (default: 600)
   */
  void getFrequencyBands(std::vector<float>& outBands, int numBands = 5, int loPass = 100,
                         int hiPass = 600) const;

  //==============================================================================
  // Configuration

  /**
   * @brief Set the smoothing time constant.
   *
   * Higher values (closer to 1.0) create smoother, slower-responding visualizations.
   * Lower values create more responsive but jumpier displays.
   *
   * @param smoothing Smoothing factor (0.0 to 1.0)
   */
  void setSmoothingTimeConstant(float smoothing);

  /**
   * @brief Set the sensitivity multiplier.
   *
   * Scales the output values for visualization.
   *
   * @param sensitivity Sensitivity multiplier (default: 1.0)
   */
  void setSensitivity(float sensitivity);

  /**
   * @brief Get current FFT size.
   */
  int getFFTSize() const {
    return fftSize;
  }

  /**
   * @brief Get number of frequency bins.
   */
  int getFrequencyBinCount() const {
    return fftSize / 2;
  }

  //==============================================================================
  // Static Utility Functions

  /**
   * @brief Calculate RMS of audio samples.
   *
   * @param samples Pointer to audio samples
   * @param numSamples Number of samples
   * @return RMS value
   */
  static float calculateRMS(const float* samples, int numSamples);

  /**
   * @brief Normalize a dB value to 0-1 range with perceptual scaling.
   *
   * Implements the normalizeDb function from bar-visualizer.tsx.
   *
   * @param value dB value to normalize
   * @return Normalized value (0-1)
   */
  static float normalizeDb(float value);

  /**
   * @brief Apply exponential smoothing.
   *
   * @param current Current value
   * @param target Target value
   * @param factor Smoothing factor (0-1)
   * @return Smoothed value
   */
  static float smoothValue(float current, float target, float factor);

private:
  //==============================================================================

  void performFFT();
  void updateSmoothedData();

  //==============================================================================

  // FFT Configuration
  int fftOrder;
  int fftSize;
  std::unique_ptr<juce::dsp::FFT> fft;

  // FFT Buffers (audio thread writes)
  std::vector<float> fftData; // Time domain -> Frequency domain
  std::vector<float> fifo;    // Input sample FIFO
  int fifoIndex = 0;
  bool fftDataReady = false;

  // Smoothed output (UI thread reads)
  std::vector<float> smoothedFrequencyData;
  std::atomic<float> smoothedRMS{0.0f};
  std::atomic<float> peakLevel{0.0f};

  // Configuration
  std::atomic<float> smoothingTimeConstant{kDefaultSmoothing};
  std::atomic<float> sensitivity{1.0f};

  // Thread synchronization
  mutable juce::SpinLock dataLock;

  // Pre-allocated buffer for mono mixdown (avoids allocation on audio thread)
  std::vector<float> monoMixBuffer;
  int monoMixBufferSize = 0;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioAnalyzer)
};

} // namespace shmui
