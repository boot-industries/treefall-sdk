// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace orpheus {

inline constexpr size_t kClipDspEqBandCount = 4;
inline constexpr size_t kClipDspMaxChannels = 8;

enum class ClipEqBandType : uint8_t { Bell, LowShelf, HighShelf, HighPass, LowPass };

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

struct ClipEqBandSettings {
  bool enabled = false;
  ClipEqBandType type = ClipEqBandType::Bell;
  float frequencyHz = 1000.0f;
  float gainDb = 0.0f;
  float q = 0.70710678f;
  bool operator==(const ClipEqBandSettings&) const = default;
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

/// Fixed-order per-clip processor program: gate -> four-band EQ -> compressor ->
/// stereo width -> limiter. Disabled/default stages are transparent.
struct ClipDspProgram {
  ClipGateSettings gate;
  std::array<ClipEqBandSettings, kClipDspEqBandCount> eq{};
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
/// calculation on the control thread; processFrame() is allocation-free,
/// lock-free, bounded, and suitable for the realtime render thread.
class ClipDspProcessor {
public:
  ClipDspValidationError prepare(const ClipDspProgram& program, double sampleRate,
                                 size_t channelCount) noexcept;
  void reset() noexcept;
  void processFrame(float* channels, size_t channelCount) noexcept;

  const ClipDspProgram& program() const noexcept {
    return m_program;
  }
  size_t channelCount() const noexcept {
    return m_channelCount;
  }

private:
  struct BiquadCoefficients {
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
  };
  struct BiquadState {
    float z1 = 0.0f;
    float z2 = 0.0f;
  };

  ClipDspProgram m_program{};
  size_t m_channelCount = 0;
  std::array<BiquadCoefficients, kClipDspEqBandCount> m_eqCoefficients{};
  std::array<std::array<BiquadState, kClipDspMaxChannels>, kClipDspEqBandCount> m_eqState{};
  float m_gateEnvelope = 0.0f;
  float m_gateGain = 0.0f;
  float m_gateAttackCoefficient = 0.0f;
  float m_gateReleaseCoefficient = 0.0f;
  float m_gateThresholdLinear = 0.0f;
  float m_compressorEnvelope = 0.0f;
  float m_compressorAttackCoefficient = 0.0f;
  float m_compressorReleaseCoefficient = 0.0f;
  float m_limiterGain = 1.0f;
  float m_limiterReleaseCoefficient = 0.0f;
  float m_limiterCeilingLinear = 1.0f;
};

} // namespace orpheus
