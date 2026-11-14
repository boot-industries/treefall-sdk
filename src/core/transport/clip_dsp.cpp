// SPDX-License-Identifier: MIT
#include <orpheus/clip_dsp.h>

#include "../common/pinned_math.h"
#include "parametric_eq_design.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace orpheus {
namespace {

bool finiteInRange(float value, float minimum, float maximum) noexcept {
  return std::isfinite(value) && value >= minimum && value <= maximum;
}

float dbToLinear(float db) noexcept {
  return orpheus::pinned::pow10(db / 20.0f);
}

float timeCoefficient(float milliseconds, double sampleRate) noexcept {
  return std::exp(-1.0f / (milliseconds * 0.001f * static_cast<float>(sampleRate)));
}
} // namespace

ClipDspValidationError validateClipDspProgram(const ClipDspProgram& program, double sampleRate,
                                              size_t channelCount) noexcept {
  if (!std::isfinite(sampleRate) || sampleRate < 8000.0 || sampleRate > 384000.0)
    return ClipDspValidationError::InvalidSampleRate;
  if (channelCount == 0 || channelCount > kClipDspMaxChannels)
    return ClipDspValidationError::InvalidChannelCount;

  if (!finiteInRange(program.gate.thresholdDb, -96.0f, 0.0f) ||
      !finiteInRange(program.gate.attackMs, 0.1f, 100.0f) ||
      !finiteInRange(program.gate.releaseMs, 1.0f, 5000.0f))
    return ClipDspValidationError::InvalidGate;

  if (validateParametricEq(program.eq, sampleRate) != ParametricEqValidationError::OK)
    return ClipDspValidationError::InvalidEq;

  if (!finiteInRange(program.compressor.thresholdDb, -96.0f, 0.0f) ||
      !finiteInRange(program.compressor.ratio, 1.0f, 20.0f) ||
      !finiteInRange(program.compressor.attackMs, 0.1f, 500.0f) ||
      !finiteInRange(program.compressor.releaseMs, 1.0f, 5000.0f) ||
      !finiteInRange(program.compressor.makeupGainDb, -24.0f, 24.0f))
    return ClipDspValidationError::InvalidCompressor;

  if (!finiteInRange(program.width.amount, 0.0f, 2.0f))
    return ClipDspValidationError::InvalidWidth;

  if (!finiteInRange(program.limiter.ceilingDb, -24.0f, 0.0f) ||
      !finiteInRange(program.limiter.releaseMs, 1.0f, 5000.0f))
    return ClipDspValidationError::InvalidLimiter;

  return ClipDspValidationError::OK;
}

ClipDspValidationError ClipDspProcessor::prepare(const ClipDspProgram& program, double sampleRate,
                                                 size_t channelCount) noexcept {
  const auto validation = validateClipDspProgram(program, sampleRate, channelCount);
  if (validation != ClipDspValidationError::OK)
    return validation;

  ClipDspProcessor prepared;
  prepared.m_program = program;
  prepared.m_channelCount = channelCount;
  prepared.m_sampleRate = sampleRate;
  prepared.m_transitionFrames = detail::parametricEqTransitionFrames(sampleRate);
  auto& coefficients = prepared.m_coefficients;
  coefficients.gateThresholdLinear = dbToLinear(program.gate.thresholdDb);
  coefficients.gateAttack = timeCoefficient(program.gate.attackMs, sampleRate);
  coefficients.gateRelease = timeCoefficient(program.gate.releaseMs, sampleRate);
  coefficients.compressorAttack = timeCoefficient(program.compressor.attackMs, sampleRate);
  coefficients.compressorRelease = timeCoefficient(program.compressor.releaseMs, sampleRate);
  coefficients.compressorMakeupLinear = dbToLinear(program.compressor.makeupGainDb);
  coefficients.limiterRelease = timeCoefficient(program.limiter.releaseMs, sampleRate);
  coefficients.limiterCeilingLinear = dbToLinear(program.limiter.ceilingDb);
  coefficients.widthTarget = program.width.enabled ? program.width.amount : 1.0f;
  if (prepared.m_eq.prepare(program.eq, sampleRate, channelCount) !=
      ParametricEqValidationError::OK)
    return ClipDspValidationError::InvalidEq;

  *this = prepared;
  reset();
  return ClipDspValidationError::OK;
}

void ClipDspProcessor::reset() noexcept {
  m_eq.reset();
  m_gateEnvelope = 0.0f;
  m_gateGain = m_program.gate.enabled ? 0.0f : 1.0f;
  m_compressorEnvelope = 0.0f;
  m_limiterGain = 1.0f;
  m_widthCurrent = m_coefficients.widthTarget;
  m_widthFramesRemaining = 0;
}

void ClipDspProcessor::transitionTo(const ClipDspProcessor& prepared) noexcept {
  if (m_channelCount == 0 || m_channelCount != prepared.m_channelCount ||
      m_sampleRate != prepared.m_sampleRate) {
    *this = prepared;
    return;
  }
  const bool gateWasEnabled = m_program.gate.enabled;
  const bool compressorWasEnabled = m_program.compressor.enabled;
  const bool limiterWasEnabled = m_program.limiter.enabled;
  m_eq.transitionTo(prepared.m_eq);
  m_program = prepared.m_program;
  m_coefficients = prepared.m_coefficients;

  if (m_program.gate.enabled && !gateWasEnabled) {
    m_gateEnvelope = 0.0f;
    m_gateGain = 1.0f;
  }
  if (m_program.compressor.enabled && !compressorWasEnabled)
    m_compressorEnvelope = 0.0f;
  if (m_program.limiter.enabled && !limiterWasEnabled)
    m_limiterGain = 1.0f;

  if (m_coefficients.widthTarget == m_widthCurrent) {
    m_widthFramesRemaining = 0;
  } else {
    m_widthStep =
        (m_coefficients.widthTarget - m_widthCurrent) / static_cast<float>(m_transitionFrames);
    m_widthFramesRemaining = m_transitionFrames;
  }
}

bool ClipDspProcessor::active() const noexcept {
  return m_program.gate.enabled || m_program.compressor.enabled || m_program.limiter.enabled ||
         m_program.width.enabled || m_widthFramesRemaining > 0 || m_eq.active();
}

void ClipDspProcessor::processFrame(float* channels, size_t channelCount) noexcept {
  if (channels == nullptr || channelCount == 0)
    return;
  channelCount = std::min({channelCount, m_channelCount, kClipDspMaxChannels});

  if (m_program.gate.enabled) {
    float peak = 0.0f;
    for (size_t channel = 0; channel < channelCount; ++channel)
      peak = std::max(peak, std::abs(channels[channel]));
    const float envelopeCoefficient =
        peak > m_gateEnvelope ? m_coefficients.gateAttack : m_coefficients.gateRelease;
    m_gateEnvelope = envelopeCoefficient * m_gateEnvelope + (1.0f - envelopeCoefficient) * peak;
    const float target = m_gateEnvelope >= m_coefficients.gateThresholdLinear ? 1.0f : 0.0f;
    const float gainCoefficient =
        target > m_gateGain ? m_coefficients.gateAttack : m_coefficients.gateRelease;
    m_gateGain = gainCoefficient * m_gateGain + (1.0f - gainCoefficient) * target;
    for (size_t channel = 0; channel < channelCount; ++channel)
      channels[channel] *= m_gateGain;
  }

  m_eq.processFrame(channels, channelCount);

  if (m_program.compressor.enabled) {
    float peak = 0.0f;
    for (size_t channel = 0; channel < channelCount; ++channel)
      peak = std::max(peak, std::abs(channels[channel]));
    const float envelopeCoefficient = peak > m_compressorEnvelope
                                          ? m_coefficients.compressorAttack
                                          : m_coefficients.compressorRelease;
    m_compressorEnvelope =
        envelopeCoefficient * m_compressorEnvelope + (1.0f - envelopeCoefficient) * peak;
    const float inputDb = 20.0f * std::log10(std::max(m_compressorEnvelope, 1.0e-12f));
    const float overDb = inputDb - m_program.compressor.thresholdDb;
    float compressorGain;
    if (overDb <= 0.0f) {
      // No reduction: reductionDb would be +0.0f, so the gain is exactly the
      // constant makeup gain. Skip the per-frame pow.
      compressorGain = m_coefficients.compressorMakeupLinear;
    } else {
      const float reductionDb = overDb / m_program.compressor.ratio - overDb;
      compressorGain = dbToLinear(reductionDb + m_program.compressor.makeupGainDb);
    }
    for (size_t channel = 0; channel < channelCount; ++channel)
      channels[channel] *= compressorGain;
  }

  const bool widthRamping = m_widthFramesRemaining > 0;
  if (widthRamping) {
    --m_widthFramesRemaining;
    if (m_widthFramesRemaining == 0)
      m_widthCurrent = m_coefficients.widthTarget;
    else
      m_widthCurrent += m_widthStep;
  }
  if ((m_program.width.enabled || widthRamping) && channelCount >= 2) {
    const float mid = (channels[0] + channels[1]) * 0.5f;
    const float side = (channels[0] - channels[1]) * 0.5f * m_widthCurrent;
    channels[0] = mid + side;
    channels[1] = mid - side;
  }

  if (m_program.limiter.enabled) {
    float peak = 0.0f;
    for (size_t channel = 0; channel < channelCount; ++channel)
      peak = std::max(peak, std::abs(channels[channel]));
    const float requiredGain = peak > m_coefficients.limiterCeilingLinear
                                   ? m_coefficients.limiterCeilingLinear / peak
                                   : 1.0f;
    if (requiredGain < m_limiterGain)
      m_limiterGain = requiredGain;
    else
      m_limiterGain =
          m_coefficients.limiterRelease * m_limiterGain + (1.0f - m_coefficients.limiterRelease);
    for (size_t channel = 0; channel < channelCount; ++channel)
      channels[channel] *= m_limiterGain;
  }
}

} // namespace orpheus
