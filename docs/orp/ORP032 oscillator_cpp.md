---
source: standard-notes
sn_filename: "ORP032 oscillator_cpp-3fba7598.txt"
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

#include "orpheus/dsp/oscillator.hpp"



#include <algorithm>

#include <cmath>

#include <numbers>

#include <random>



namespace orpheus::dsp {



namespace {



/// Two-pi constant for phase calculations

constexpr double kTwoPi = 2.0 * std::numbers::pi;



/// Maximum number of unison voices

constexpr uint8_t kMaxUnisonVoices = 8;



/// PolyBLEP residual for band-limiting discontinuities

[[nodiscard]] inline float polyBLEP(float t, float dt) noexcept {

  // t is the current phase position (0-1)

  // dt is the phase increment per sample

  

  if (t < dt) {

    t /= dt;

    return t + t - t * t - 1.0f;

  } else if (t > 1.0f - dt) {

    t = (t - 1.0f) / dt;

    return t * t + t + t + 1.0f;

  }

  return 0.0f;

}



/// Generate white noise sample

[[nodiscard]] inline float generateWhiteNoise() noexcept {

  static thread_local std::mt19937 generator{std::random_device{}()};

  static thread_local std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

  return distribution(generator);

}



/// Pink noise generator using Paul Kellet's refined method

class PinkNoiseGenerator {

public:

  [[nodiscard]] float process() noexcept {

    float white = generateWhiteNoise();

    

    b0_ = 0.99886f * b0_ + white * 0.0555179f;

    b1_ = 0.99332f * b1_ + white * 0.0750759f;

    b2_ = 0.96900f * b2_ + white * 0.1538520f;

    b3_ = 0.86650f * b3_ + white * 0.3104856f;

    b4_ = 0.55000f * b4_ + white * 0.5329522f;

    b5_ = -0.7616f * b5_ - white * 0.0168980f;

    

    float pink = b0_ + b1_ + b2_ + b3_ + b4_ + b5_ + b6_ + white * 0.5362f;

    b6_ = white * 0.115926f;

    

    return pink * 0.11f; // Scale to approximately ±1

  }



private:

  float b0_{0.0f}, b1_{0.0f}, b2_{0.0f}, b3_{0.0f};

  float b4_{0.0f}, b5_{0.0f}, b6_{0.0f};

};



/// Single voice state for unison oscillator

struct Voice {

  double phase{0.0};

  double phaseIncrement{0.0};

  float detuneRatio{1.0f};

  

  void updateIncrement(double baseFreq, double sampleRate) noexcept {

    phaseIncrement = (baseFreq * detuneRatio) / sampleRate;

  }

  

  void advance() noexcept {

    phase += phaseIncrement;

    if (phase >= 1.0) {

      phase -= 1.0;

    }

  }

  

  [[nodiscard]] float processSine() noexcept {

    float sample = std::sin(static_cast<float>(phase * kTwoPi));

    advance();

    return sample;

  }

  

  [[nodiscard]] float processSaw() noexcept {

    float sample = static_cast<float>(2.0 * phase - 1.0);

    sample -= polyBLEP(static_cast<float>(phase), static_cast<float>(phaseIncrement));

    advance();

    return sample;

  }

  

  [[nodiscard]] float processSquare(float pulseWidth) noexcept {

    float sample = (phase < pulseWidth) ? 1.0f : -1.0f;

    sample += polyBLEP(static_cast<float>(phase), static_cast<float>(phaseIncrement));

    sample -= polyBLEP(std::fmod(static_cast<float>(phase + (1.0 - pulseWidth)), 1.0f),

                       static_cast<float>(phaseIncrement));

    advance();

    return sample;

  }

  

  [[nodiscard]] float processTriangle() noexcept {

    float t = static_cast<float>(phase);

    float sample = 4.0f * (std::abs(t - 0.5f) - 0.25f);

    

    // Integrate polyBLEP for triangle (derivative of square)

    float dt = static_cast<float>(phaseIncrement);

    float blep = polyBLEP(t, dt);

    blep -= polyBLEP(std::fmod(t + 0.5f, 1.0f), dt);

    sample += 4.0f * dt * blep;

    

    advance();

    return sample;

  }

};



} // anonymous namespace



/// Private implementation (PImpl idiom for ABI stability)

struct Oscillator::Impl {

  double sampleRate;

  std::atomic<double> frequency{440.0};

  std::atomic<WaveShape> waveShape{WaveShape::Sine};

  std::atomic<float> pulseWidth{0.5f};

  std::atomic<uint8_t> unisonVoices{1};

  std::atomic<float> unisonDetune{0.0f};

  std::atomic<bool> fmMode{false};

  

  std::array<Voice, kMaxUnisonVoices> voices;

  PinkNoiseGenerator pinkNoise;

  

  explicit Impl(double sr) : sampleRate(sr) {

    updateAllVoices();

  }

  

  void updateAllVoices() noexcept {

    const double freq = frequency.load(std::memory_order_relaxed);

    const uint8_t numVoices = unisonVoices.load(std::memory_order_relaxed);

    const float detune = unisonDetune.load(std::memory_order_relaxed);

    

    for (uint8_t i = 0; i < numVoices; ++i) {

      if (numVoices == 1) {

        voices[i].detuneRatio = 1.0f;

      } else {

        // Spread voices across detune range

        float spread = (static_cast<float>(i) / (numVoices - 1) - 0.5f) * 2.0f;

        float cents = spread * detune * 50.0f; // ±50 cents at full detune

        voices[i].detuneRatio = std::pow(2.0f, cents / 1200.0f);

      }

      voices[i].updateIncrement(freq, sampleRate);

    }

  }

  

  [[nodiscard]] float processVoice(Voice& voice, WaveShape shape, float pw) noexcept {

    switch (shape) {

      case WaveShape::Sine:

        return voice.processSine();

      case WaveShape::Sawtooth:

        return voice.processSaw();

      case WaveShape::Square:

        return voice.processSquare(pw);

      case WaveShape::Triangle:

        return voice.processTriangle();

      case WaveShape::WhiteNoise:

        voice.advance(); // Advance phase for sync purposes

        return generateWhiteNoise();

      case WaveShape::PinkNoise:

        voice.advance();

        return pinkNoise.process();

      default:

        return 0.0f;

    }

  }

};



Oscillator::Oscillator(double sampleRate)

    : pImpl_(std::make_unique<Impl>(sampleRate)) {}



Oscillator::~Oscillator() = default;



Oscillator::Oscillator(Oscillator&&) noexcept = default;

Oscillator& Oscillator::operator=(Oscillator&&) noexcept = default;



float Oscillator::process() noexcept {

  const uint8_t numVoices = pImpl_->unisonVoices.load(std::memory_order_relaxed);

  const WaveShape shape = pImpl_->waveShape.load(std::memory_order_relaxed);

  const float pw = pImpl_->pulseWidth.load(std::memory_order_relaxed);

  

  float sum = 0.0f;

  for (uint8_t i = 0; i < numVoices; ++i) {

    sum += pImpl_->processVoice(pImpl_->voices[i], shape, pw);

  }

  

  // Normalize by voice count to prevent clipping

  return numVoices > 0 ? sum / numVoices : 0.0f;

}



void Oscillator::processBlock(std::span<float> output) noexcept {

  for (auto& sample : output) {

    sample = process();

  }

}



void Oscillator::setFrequency(double freqHz) noexcept {

  pImpl_->frequency.store(freqHz, std::memory_order_relaxed);

  pImpl_->updateAllVoices();

}



double Oscillator::getFrequency() const noexcept {

  return pImpl_->frequency.load(std::memory_order_relaxed);

}



void Oscillator::setWaveShape(WaveShape shape) noexcept {

  pImpl_->waveShape.store(shape, std::memory_order_relaxed);

}



WaveShape Oscillator::getWaveShape() const noexcept {

  return pImpl_->waveShape.load(std::memory_order_relaxed);

}



void Oscillator::setPulseWidth(float width) noexcept {

  pImpl_->pulseWidth.store(std::clamp(width, 0.01f, 0.99f), std::memory_order_relaxed);

}



float Oscillator::getPulseWidth() const noexcept {

  return pImpl_->pulseWidth.load(std::memory_order_relaxed);

}



void Oscillator::setUnisonVoices(uint8_t voices) noexcept {

  voices = std::clamp(voices, uint8_t{1}, kMaxUnisonVoices);

  pImpl_->unisonVoices.store(voices, std::memory_order_relaxed);

  pImpl_->updateAllVoices();

}



uint8_t Oscillator::getUnisonVoices() const noexcept {

  return pImpl_->unisonVoices.load(std::memory_order_relaxed);

}



void Oscillator::setUnisonDetune(float detune) noexcept {

  pImpl_->unisonDetune.store(std::clamp(detune, 0.0f, 1.0f), std::memory_order_relaxed);

  pImpl_->updateAllVoices();

}



float Oscillator::getUnisonDetune() const noexcept {

  return pImpl_->unisonDetune.load(std::memory_order_relaxed);

}



void Oscillator::resetPhase(float phaseRadians) noexcept {

  float normalizedPhase = std::fmod(phaseRadians / kTwoPi, 1.0f);

  if (normalizedPhase < 0.0f) normalizedPhase += 1.0f;

  

  for (auto& voice : pImpl_->voices) {

    voice.phase = normalizedPhase;

  }

}



void Oscillator::setSampleRate(double sampleRate) {

  pImpl_->sampleRate = sampleRate;

  pImpl_->updateAllVoices();

}



double Oscillator::getSampleRate() const noexcept {

  return pImpl_->sampleRate;

}



void Oscillator::setFMMode(bool enabled) noexcept {

  pImpl_->fmMode.store(enabled, std::memory_order_relaxed);

}



float Oscillator::processFM(float fmInput) noexcept {

  // Simple FM: add modulation to phase before processing

  const uint8_t numVoices = pImpl_->unisonVoices.load(std::memory_order_relaxed);

  const WaveShape shape = pImpl_->waveShape.load(std::memory_order_relaxed);

  const float pw = pImpl_->pulseWidth.load(std::memory_order_relaxed);

  

  float sum = 0.0f;

  for (uint8_t i = 0; i < numVoices; ++i) {

    auto& voice = pImpl_->voices[i];

    

    // Store original phase, apply FM, process, restore

    double origPhase = voice.phase;

    voice.phase = std::fmod(voice.phase + fmInput / kTwoPi, 1.0);

    if (voice.phase < 0.0) voice.phase += 1.0;

    

    sum += pImpl_->processVoice(voice, shape, pw);

    

    voice.phase = origPhase;

    voice.advance();

  }

  

  return numVoices > 0 ? sum / numVoices : 0.0f;

}



// SineOscillator implementation (lightweight alternative)



SineOscillator::SineOscillator(double sampleRate)

    : sampleRate_(sampleRate) {

  setFrequency(440.0);

}



void SineOscillator::setFrequency(double freqHz) noexcept {

  frequency_ = freqHz;

  phaseIncrement_ = kTwoPi * freqHz / sampleRate_;

}



double SineOscillator::getFrequency() const noexcept {

  return frequency_;

}



float SineOscillator::process() noexcept {

  float sample = std::sin(static_cast<float>(phase_));

  phase_ += phaseIncrement_;

  if (phase_ >= kTwoPi) {

    phase_ -= kTwoPi;

  }

  return sample;

}



void SineOscillator::processBlock(std::span<float> output) noexcept {

  for (auto& sample : output) {

    sample = process();

  }

}



void SineOscillator::resetPhase(float phaseRadians) noexcept {

  phase_ = phaseRadians;

  while (phase_ < 0.0) phase_ += kTwoPi;

  while (phase_ >= kTwoPi) phase_ -= kTwoPi;

}



void SineOscillator::setSampleRate(double sampleRate) noexcept {

  sampleRate_ = sampleRate;

  setFrequency(frequency_); // Recalculate phase increment

}



} // namespace orpheus::dsp
