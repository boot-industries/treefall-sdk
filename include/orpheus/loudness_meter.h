// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace orpheus {

/// Lightweight K-weighted loudness meter for stereo or mono streams.
///
/// This control/offline facility maintains a 3 s short-term window and
/// accumulates overlapping 400 ms blocks with absolute/relative gating. Its
/// terminology is modeled against BS.1770; it is not a standalone
/// standards-conformance implementation.
class LoudnessMeter {
public:
  static constexpr float kSilenceLufs = -100.0f;

  explicit LoudnessMeter(double sample_rate = 48000.0) {
    setSampleRate(sample_rate);
  }

  void setSampleRate(double sample_rate) {
    sampleRate_ = std::max(8000.0, sample_rate);
    segmentTargetSamples_ =
        std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(sampleRate_ * 0.1)));
    for (auto& filter : filters_) {
      filter.configure(sampleRate_);
    }
    reset();
  }

  void reset() {
    for (auto& filter : filters_) {
      filter.reset();
    }

    segmentEnergy_ = 0.0;
    segmentSamples_ = 0;
    recentSegments_.clear();
    recentShortTermSegments_.clear();
    gatingBlocks_.clear();
    shortTermLufs_ = kSilenceLufs;
    integratedLufs_ = kSilenceLufs;
  }

  void processBuffer(const float* mono, std::size_t num_frames) {
    processBuffer(mono, nullptr, num_frames);
  }

  void processBuffer(const float* left, const float* right, std::size_t num_frames) {
    if (left == nullptr && right == nullptr) {
      return;
    }

    for (std::size_t index = 0; index < num_frames; ++index) {
      const float leftSample = left != nullptr ? left[index] : 0.0f;
      const float rightSample = right != nullptr ? right[index] : 0.0f;

      const float weightedLeft = filters_[0].process(leftSample);
      const float weightedRight = filters_[1].process(rightSample);

      segmentEnergy_ += static_cast<double>(weightedLeft) * static_cast<double>(weightedLeft) +
                        static_cast<double>(weightedRight) * static_cast<double>(weightedRight);
      ++segmentSamples_;

      if (segmentSamples_ >= segmentTargetSamples_) {
        finalizeSegment();
      }
    }
  }

  [[nodiscard]] float shortTermLufs() const {
    return shortTermLufs_;
  }
  [[nodiscard]] float integratedLufs() const {
    return integratedLufs_;
  }

private:
  struct Biquad {
    void configure(const std::array<double, 3>& b_coefficients,
                   const std::array<double, 3>& a_coefficients) {
      b = b_coefficients;
      a = a_coefficients;
    }

    void reset() {
      z1 = 0.0;
      z2 = 0.0;
    }

    [[nodiscard]] float process(float sample) {
      const double output = b[0] * static_cast<double>(sample) + z1;
      z1 = b[1] * static_cast<double>(sample) - a[1] * output + z2;
      z2 = b[2] * static_cast<double>(sample) - a[2] * output;
      return static_cast<float>(output);
    }

    std::array<double, 3> b{1.0, 0.0, 0.0};
    std::array<double, 3> a{1.0, 0.0, 0.0};
    double z1 = 0.0;
    double z2 = 0.0;
  };

  struct ChannelFilter {
    void configure(double sample_rate) {
      const auto highShelfCoefficients =
          designHighShelf(sample_rate, kHighShelfFrequencyHz, kHighShelfGainDb, 1.0);
      const auto highPassCoefficients =
          designHighPass(sample_rate, kHighPassFrequencyHz, kHighPassQ);
      highShelf.configure(highShelfCoefficients[0], highShelfCoefficients[1]);
      highPass.configure(highPassCoefficients[0], highPassCoefficients[1]);
    }

    void reset() {
      highShelf.reset();
      highPass.reset();
    }

    [[nodiscard]] float process(float sample) {
      return highPass.process(highShelf.process(sample));
    }

    static void normalize(std::array<double, 3>& b, std::array<double, 3>& a, double a0, double a1,
                          double a2) {
      b[0] /= a0;
      b[1] /= a0;
      b[2] /= a0;
      a = {1.0, a1 / a0, a2 / a0};
    }

    static void configureShelfCoefficients(std::array<double, 3>& b, std::array<double, 3>& a,
                                           double sample_rate, double frequency_hz, double gain_db,
                                           double slope) {
      constexpr double pi = 3.14159265358979323846;

      const double A = std::pow(10.0, gain_db / 40.0);
      const double omega = 2.0 * pi * frequency_hz / sample_rate;
      const double sine = std::sin(omega);
      const double cosine = std::cos(omega);
      const double alpha = sine / 2.0 * std::sqrt((A + 1.0 / A) * (1.0 / slope - 1.0) + 2.0);
      const double beta = 2.0 * std::sqrt(A) * alpha;

      b = {
          A * ((A + 1.0) + (A - 1.0) * cosine + beta),
          -2.0 * A * ((A - 1.0) + (A + 1.0) * cosine),
          A * ((A + 1.0) + (A - 1.0) * cosine - beta),
      };

      normalize(b, a, (A + 1.0) - (A - 1.0) * cosine + beta, 2.0 * ((A - 1.0) - (A + 1.0) * cosine),
                (A + 1.0) - (A - 1.0) * cosine - beta);
    }

    static void configureHighPassCoefficients(std::array<double, 3>& b, std::array<double, 3>& a,
                                              double sample_rate, double frequency_hz, double q) {
      constexpr double pi = 3.14159265358979323846;

      const double omega = 2.0 * pi * frequency_hz / sample_rate;
      const double sine = std::sin(omega);
      const double cosine = std::cos(omega);
      const double alpha = sine / (2.0 * q);

      b = {
          (1.0 + cosine) / 2.0,
          -(1.0 + cosine),
          (1.0 + cosine) / 2.0,
      };

      normalize(b, a, 1.0 + alpha, -2.0 * cosine, 1.0 - alpha);
    }

    static std::array<std::array<double, 3>, 2>
    designHighShelf(double sample_rate, double frequency_hz, double gain_db, double slope) {
      std::array<double, 3> b{};
      std::array<double, 3> a{};
      configureShelfCoefficients(b, a, sample_rate, frequency_hz, gain_db, slope);
      return {b, a};
    }

    static std::array<std::array<double, 3>, 2> designHighPass(double sample_rate,
                                                               double frequency_hz, double q) {
      std::array<double, 3> b{};
      std::array<double, 3> a{};
      configureHighPassCoefficients(b, a, sample_rate, frequency_hz, q);
      return {b, a};
    }

    static constexpr double kHighShelfFrequencyHz = 1681.974450955533;
    static constexpr double kHighShelfGainDb = 4.0;
    static constexpr double kHighPassFrequencyHz = 38.13547087602444;
    static constexpr double kHighPassQ = 0.5003270373238773;

    Biquad highShelf;
    Biquad highPass;
  };

  void finalizeSegment() {
    if (segmentSamples_ == 0) {
      return;
    }

    const double meanSquare = segmentEnergy_ / static_cast<double>(segmentSamples_);
    recentShortTermSegments_.push_back(meanSquare);
    if (recentShortTermSegments_.size() > kShortTermSegmentCount) {
      recentShortTermSegments_.pop_front();
    }

    recentSegments_.push_back(meanSquare);
    if (recentSegments_.size() > kIntegratedSegmentCount) {
      recentSegments_.pop_front();
    }

    updateShortTerm();
    updateIntegrated();

    segmentEnergy_ = 0.0;
    segmentSamples_ = 0;
  }

  void updateShortTerm() {
    if (recentShortTermSegments_.empty()) {
      shortTermLufs_ = kSilenceLufs;
      return;
    }

    double energy = 0.0;
    for (const double value : recentShortTermSegments_) {
      energy += value;
    }

    shortTermLufs_ =
        lufsFromMeanSquare(energy / static_cast<double>(recentShortTermSegments_.size()));
  }

  void updateIntegrated() {
    if (recentSegments_.size() == kIntegratedSegmentCount) {
      double blockEnergy = 0.0;
      for (const double value : recentSegments_) {
        blockEnergy += value;
      }
      gatingBlocks_.push_back(blockEnergy / static_cast<double>(kIntegratedSegmentCount));
    }

    if (gatingBlocks_.empty()) {
      integratedLufs_ = kSilenceLufs;
      return;
    }

    std::vector<double> absoluteGated;
    absoluteGated.reserve(gatingBlocks_.size());

    for (const double blockEnergy : gatingBlocks_) {
      if (lufsFromMeanSquare(blockEnergy) >= kAbsoluteGateLufs) {
        absoluteGated.push_back(blockEnergy);
      }
    }

    if (absoluteGated.empty()) {
      integratedLufs_ = kSilenceLufs;
      return;
    }

    double ungatedEnergy = 0.0;
    for (const double blockEnergy : absoluteGated) {
      ungatedEnergy += blockEnergy;
    }
    ungatedEnergy /= static_cast<double>(absoluteGated.size());

    const float relativeGate = lufsFromMeanSquare(ungatedEnergy) - 10.0f;

    double integratedEnergy = 0.0;
    std::size_t integratedCount = 0;
    for (const double blockEnergy : absoluteGated) {
      if (lufsFromMeanSquare(blockEnergy) >= relativeGate) {
        integratedEnergy += blockEnergy;
        ++integratedCount;
      }
    }

    integratedLufs_ =
        integratedCount > 0
            ? lufsFromMeanSquare(integratedEnergy / static_cast<double>(integratedCount))
            : kSilenceLufs;
  }

  [[nodiscard]] static float lufsFromMeanSquare(double mean_square) {
    if (mean_square <= 1.0e-12) {
      return kSilenceLufs;
    }

    return static_cast<float>(-0.691 + 10.0 * std::log10(mean_square));
  }

  static constexpr std::size_t kShortTermSegmentCount = 30; // 3 s / 100 ms
  static constexpr std::size_t kIntegratedSegmentCount = 4; // 400 ms / 100 ms
  static constexpr float kAbsoluteGateLufs = -70.0f;

  double sampleRate_ = 48000.0;
  std::size_t segmentTargetSamples_ = 4800;
  std::array<ChannelFilter, 2> filters_{};
  double segmentEnergy_ = 0.0;
  std::size_t segmentSamples_ = 0;
  std::deque<double> recentSegments_;
  std::deque<double> recentShortTermSegments_;
  std::vector<double> gatingBlocks_;
  float shortTermLufs_ = kSilenceLufs;
  float integratedLufs_ = kSilenceLufs;
};

} // namespace orpheus
