---
source: standard-notes
sn_filename: "ORP031 oscillator_hpp-e1a58879.txt"
prefix: orp
original_format: lexical
imported: 2026-05-01
status: archive
related:
  - audio_processing_requires_determinism
  - cpp20_for_audio_dsp
  - deterministic-audio-processing
---

// SPDX-License-Identifier: MIT

#pragma once



#include <array>

#include <atomic>

#include <cstddef>

#include <cstdint>

#include <memory>

#include <optional>

#include <span>



namespace orpheus::dsp {



/// Supported oscillator waveform types

enum class WaveShape : uint8_t {

  Sine,      ///< Pure sine wave (analytical)

  Triangle,  ///< Band-limited triangle wave

  Sawtooth,  ///< Band-limited sawtooth wave

  Square,    ///< Band-limited square/pulse wave (50% duty by default)

  WhiteNoise,///< White noise (uniform distribution)

  PinkNoise  ///< Pink noise (1/f spectrum)

};



/// High-quality, alias-suppressed oscillator with unison support

/**

- The Oscillator class provides professional-grade waveform generation with:
- - Multiple waveforms (sine, triangle, saw, square, noise)
- - PolyBLEP band-limiting for alias suppression
- - Unison/detune for richer sounds (up to 8 voices)
- - Variable pulse width for square waves
- - Thread-safe parameter updates
- - Zero-latency phase reset

 *

- Example usage:
- @code
- orpheus::dsp::Oscillator osc(48000.0);
- osc.setWaveShape(WaveShape::Sawtooth);
- osc.setFrequency(440.0);
- osc.setUnisonVoices(3);
- osc.setUnisonDetune(0.05);
- 
- for (size_t i = 0; i < bufferSize; ++i) {
- buffer[i] = osc.process();
- }
- @endcode

 */

class Oscillator {

public:

  /// Construct an oscillator for a given sample rate

  explicit Oscillator(double sampleRate);

  

  /// Destructor

  ~Oscillator();

  

  // Prevent copying (contains unique state)

  Oscillator(const Oscillator&) = delete;

  Oscillator& operator=(const Oscillator&) = delete;

  

  // Allow moving

  Oscillator(Oscillator&&) noexcept;

  Oscillator& operator=(Oscillator&&) noexcept;

  

  /// Generate the next sample

  [[nodiscard]] float process() noexcept;

  

  /// Process a block of samples into the provided buffer

  void processBlock(std::span<float> output) noexcept;

  

  /// Set the oscillator frequency in Hz

  void setFrequency(double freqHz) noexcept;

  

  /// Get the current frequency in Hz

  [[nodiscard]] double getFrequency() const noexcept;

  

  /// Set the waveform shape

  void setWaveShape(WaveShape shape) noexcept;

  

  /// Get the current waveform shape

  [[nodiscard]] WaveShape getWaveShape() const noexcept;

  

  /// Set pulse width for square waves (0.0 to 1.0, default 0.5)

  void setPulseWidth(float width) noexcept;

  

  /// Get the current pulse width

  [[nodiscard]] float getPulseWidth() const noexcept;

  

  /// Set the number of unison voices (1-8, default 1)

  void setUnisonVoices(uint8_t voices) noexcept;

  

  /// Get the number of unison voices

  [[nodiscard]] uint8_t getUnisonVoices() const noexcept;

  

  /// Set unison detune amount (0.0 to 1.0, default 0.0)

  /// 1.0 corresponds to approximately ±50 cents

  void setUnisonDetune(float detune) noexcept;

  

  /// Get the current unison detune amount

  [[nodiscard]] float getUnisonDetune() const noexcept;

  

  /// Reset the phase to zero (or specified phase in radians)

  void resetPhase(float phaseRadians = 0.0f) noexcept;

  

  /// Set the sample rate (triggers internal reinitialization)

  void setSampleRate(double sampleRate);

  

  /// Get the current sample rate

  [[nodiscard]] double getSampleRate() const noexcept;

  

  /// Enable/disable FM modulation mode (for use as FM operator)

  void setFMMode(bool enabled) noexcept;

  

  /// Process with frequency modulation input (radians per sample)

  [[nodiscard]] float processFM(float fmInput) noexcept;



private:

  struct Impl;

  std::unique_ptr<Impl> pImpl_;

};



/// Concept for types that can act as oscillator outputs

template<typename T>

concept OscillatorOutput = std::is_floating_point_v<T>;



/// High-performance sine oscillator (optimized for LFO use cases)

/**

- Lightweight sine-only oscillator using a direct digital synthesis approach.
- Useful for LFOs, vibrato, and tremolo where only sine waves are needed.

 */

class SineOscillator {

public:

  explicit SineOscillator(double sampleRate);

  

  void setFrequency(double freqHz) noexcept;

  [[nodiscard]] double getFrequency() const noexcept;

  

  [[nodiscard]] float process() noexcept;

  void processBlock(std::span<float> output) noexcept;

  

  void resetPhase(float phaseRadians = 0.0f) noexcept;

  void setSampleRate(double sampleRate) noexcept;



private:

  double sampleRate_;

  double phaseIncrement_{0.0};

  double phase_{0.0};

  double frequency_{440.0};

};



} // namespace orpheus::dsp
