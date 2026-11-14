// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/parametric_eq.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace orpheus {

inline constexpr size_t kClipDspMaxChannels = kParametricEqMaxChannels;
/// EQ and width changes applied with ClipDspProcessor::transitionTo() settle in this time.
inline constexpr double kClipDspTransitionSeconds = kParametricEqTransitionSeconds;

enum class ClipDspValidationError : uint8_t {
  OK,
  InvalidSampleRate,
  InvalidChannelCount,
  InvalidGate,
  InvalidEq,
  InvalidCompressor,
  InvalidWidth,
  InvalidLimiter
};

struct ClipGateSettings {
  bool enabled = false;
  float thresholdDb = -60.0f;
  float attackMs = 2.0f;
  float releaseMs = 100.0f;
  bool operator==(const ClipGateSettings&) const = default;
};

struct ClipCompressorSettings {
  bool enabled = false;
  float thresholdDb = -18.0f;
  float ratio = 4.0f;
  float attackMs = 10.0f;
  float releaseMs = 100.0f;
  float makeupGainDb = 0.0f;
  bool operator==(const ClipCompressorSettings&) const = default;
};

struct ClipWidthSettings {
  bool enabled = false;
  float amount = 1.0f;
  bool operator==(const ClipWidthSettings&) const = default;
};

struct ClipLimiterSettings {
  bool enabled = false;
  float ceilingDb = -0.3f;
  float releaseMs = 50.0f;
  bool operator==(const ClipLimiterSettings&) const = default;
};

/// Fixed-order per-clip processor program: gate -> parametric EQ (up to 8 bands)
/// -> compressor -> stereo width -> limiter. Disabled/default stages are transparent.
struct ClipDspProgram {
  ClipGateSettings gate;
  ParametricEqSettings eq{};
  ClipCompressorSettings compressor;
  ClipWidthSettings width;
  ClipLimiterSettings limiter;
  bool operator==(const ClipDspProgram&) const = default;
};

/// Validate a semantic program for a concrete render rate. Validation is
/// failure-atomic: callers retain the prior program on any non-OK result.
ClipDspValidationError validateClipDspProgram(const ClipDspProgram& program, double sampleRate,
                                              size_t channelCount) noexcept;

/// Prepared, fixed-capacity per-voice processor. prepare() performs coefficient
/// calculation on the control thread; processFrame(), transitionTo() and
/// active() are allocation-free, lock-free, bounded, and suitable for the
/// realtime render thread.
class ClipDspProcessor {
public:
  ClipDspValidationError prepare(const ClipDspProgram& program, double sampleRate,
                                 size_t channelCount) noexcept;
  void reset() noexcept;
  void processFrame(float* channels, size_t channelCount) noexcept;
  /// Adopt another prepared processor's program while keeping the runtime
  /// state. EQ and width changes settle in kClipDspTransitionSeconds; gate,
  /// compressor and limiter settings apply at once. A dynamics stage enabled by
  /// the transition starts idle: gate envelope 0 with gain 1, compressor
  /// envelope 0, limiter gain 1. A different channel count or sample rate
  /// replaces this processor outright. Precondition: `prepared` came from a
  /// successful prepare().
  void transitionTo(const ClipDspProcessor& prepared) noexcept;
  /// True while any stage processes audio, including an EQ or width transition.
  bool active() const noexcept;

  const ClipDspProgram& program() const noexcept {
    return m_program;
  }
  size_t channelCount() const noexcept {
    return m_channelCount;
  }

private:
  struct StageCoefficients {
    float gateThresholdLinear = 0.0f;
    float gateAttack = 0.0f;
    float gateRelease = 0.0f;
    float compressorAttack = 0.0f;
    float compressorRelease = 0.0f;
    float compressorMakeupLinear = 1.0f; ///< 10^(makeupGainDb/20), computed on the control thread
    float limiterRelease = 0.0f;
    float limiterCeilingLinear = 1.0f;
    float widthTarget = 1.0f;
  };

  ClipDspProgram m_program{};
  StageCoefficients m_coefficients{};
  ParametricEqProcessor m_eq{};
  size_t m_channelCount = 0;
  double m_sampleRate = 0.0;
  uint32_t m_transitionFrames = 0;
  float m_gateEnvelope = 0.0f;
  float m_gateGain = 0.0f;
  float m_compressorEnvelope = 0.0f;
  float m_limiterGain = 1.0f;
  float m_widthCurrent = 1.0f;
  float m_widthStep = 0.0f;
  uint32_t m_widthFramesRemaining = 0;
};

} // namespace orpheus
