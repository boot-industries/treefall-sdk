// SPDX-License-Identifier: MIT
#include <orpheus/clip_dsp.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace orpheus {
namespace {
constexpr float kPi = 3.14159265358979323846f;

bool finiteInRange(float value, float minimum, float maximum) noexcept {
  return std::isfinite(value) && value >= minimum && value <= maximum;
}

float dbToLinear(float db) noexcept {
  return std::pow(10.0f, db / 20.0f);
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

  const float maximumFrequency = static_cast<float>(sampleRate * 0.49);
  for (const auto& band : program.eq) {
    if (!finiteInRange(band.frequencyHz, 20.0f, maximumFrequency) ||
        !finiteInRange(band.gainDb, -24.0f, 24.0f) || !finiteInRange(band.q, 0.1f, 20.0f))
      return ClipDspValidationError::InvalidEq;
  }

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
  prepared.m_gateThresholdLinear = dbToLinear(program.gate.thresholdDb);
  prepared.m_gateAttackCoefficient = timeCoefficient(program.gate.attackMs, sampleRate);
  prepared.m_gateReleaseCoefficient = timeCoefficient(program.gate.releaseMs, sampleRate);
  prepared.m_compressorAttackCoefficient = timeCoefficient(program.compressor.attackMs, sampleRate);
  prepared.m_compressorReleaseCoefficient =
      timeCoefficient(program.compressor.releaseMs, sampleRate);
  prepared.m_limiterReleaseCoefficient = timeCoefficient(program.limiter.releaseMs, sampleRate);
  prepared.m_limiterCeilingLinear = dbToLinear(program.limiter.ceilingDb);

  for (size_t index = 0; index < program.eq.size(); ++index) {
    const auto& band = program.eq[index];
    const float omega = 2.0f * kPi * band.frequencyHz / static_cast<float>(sampleRate);
    const float cosine = std::cos(omega);
    const float sine = std::sin(omega);
    const float alpha = sine / (2.0f * band.q);
    const float amplitude = std::pow(10.0f, band.gainDb / 40.0f);
    const float shelfAlpha = sine * 0.5f * std::sqrt(2.0f);
    const float twiceSqrtAAlpha = 2.0f * std::sqrt(amplitude) * shelfAlpha;

    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a0 = 1.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
    switch (band.type) {
    case ClipEqBandType::Bell:
      b0 = 1.0f + alpha * amplitude;
      b1 = -2.0f * cosine;
      b2 = 1.0f - alpha * amplitude;
      a0 = 1.0f + alpha / amplitude;
      a1 = -2.0f * cosine;
      a2 = 1.0f - alpha / amplitude;
      break;
    case ClipEqBandType::LowShelf:
      b0 = amplitude * ((amplitude + 1.0f) - (amplitude - 1.0f) * cosine + twiceSqrtAAlpha);
      b1 = 2.0f * amplitude * ((amplitude - 1.0f) - (amplitude + 1.0f) * cosine);
      b2 = amplitude * ((amplitude + 1.0f) - (amplitude - 1.0f) * cosine - twiceSqrtAAlpha);
      a0 = (amplitude + 1.0f) + (amplitude - 1.0f) * cosine + twiceSqrtAAlpha;
      a1 = -2.0f * ((amplitude - 1.0f) + (amplitude + 1.0f) * cosine);
      a2 = (amplitude + 1.0f) + (amplitude - 1.0f) * cosine - twiceSqrtAAlpha;
      break;
    case ClipEqBandType::HighShelf:
      b0 = amplitude * ((amplitude + 1.0f) + (amplitude - 1.0f) * cosine + twiceSqrtAAlpha);
      b1 = -2.0f * amplitude * ((amplitude - 1.0f) + (amplitude + 1.0f) * cosine);
      b2 = amplitude * ((amplitude + 1.0f) + (amplitude - 1.0f) * cosine - twiceSqrtAAlpha);
      a0 = (amplitude + 1.0f) - (amplitude - 1.0f) * cosine + twiceSqrtAAlpha;
      a1 = 2.0f * ((amplitude - 1.0f) - (amplitude + 1.0f) * cosine);
      a2 = (amplitude + 1.0f) - (amplitude - 1.0f) * cosine - twiceSqrtAAlpha;
      break;
    case ClipEqBandType::HighPass:
      b0 = (1.0f + cosine) * 0.5f;
      b1 = -(1.0f + cosine);
      b2 = b0;
      a0 = 1.0f + alpha;
      a1 = -2.0f * cosine;
      a2 = 1.0f - alpha;
      break;
    case ClipEqBandType::LowPass:
      b0 = (1.0f - cosine) * 0.5f;
      b1 = 1.0f - cosine;
      b2 = b0;
      a0 = 1.0f + alpha;
      a1 = -2.0f * cosine;
      a2 = 1.0f - alpha;
      break;
    }

    prepared.m_eqCoefficients[index] = {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
  }

  *this = prepared;
  reset();
  return ClipDspValidationError::OK;
}

void ClipDspProcessor::reset() noexcept {
  for (auto& band : m_eqState)
    for (auto& channel : band)
      channel = {};
  m_gateEnvelope = 0.0f;
  m_gateGain = m_program.gate.enabled ? 0.0f : 1.0f;
  m_compressorEnvelope = 0.0f;
  m_limiterGain = 1.0f;
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
        peak > m_gateEnvelope ? m_gateAttackCoefficient : m_gateReleaseCoefficient;
    m_gateEnvelope = envelopeCoefficient * m_gateEnvelope + (1.0f - envelopeCoefficient) * peak;
    const float target = m_gateEnvelope >= m_gateThresholdLinear ? 1.0f : 0.0f;
    const float gainCoefficient =
        target > m_gateGain ? m_gateAttackCoefficient : m_gateReleaseCoefficient;
    m_gateGain = gainCoefficient * m_gateGain + (1.0f - gainCoefficient) * target;
    for (size_t channel = 0; channel < channelCount; ++channel)
      channels[channel] *= m_gateGain;
  }

  for (size_t band = 0; band < m_program.eq.size(); ++band) {
    if (!m_program.eq[band].enabled)
      continue;
    const auto& coefficients = m_eqCoefficients[band];
    for (size_t channel = 0; channel < channelCount; ++channel) {
      auto& state = m_eqState[band][channel];
      const float input = channels[channel];
      const float output = coefficients.b0 * input + state.z1;
      state.z1 = coefficients.b1 * input - coefficients.a1 * output + state.z2;
      state.z2 = coefficients.b2 * input - coefficients.a2 * output;
      channels[channel] = output;
    }
  }

  if (m_program.compressor.enabled) {
    float peak = 0.0f;
    for (size_t channel = 0; channel < channelCount; ++channel)
      peak = std::max(peak, std::abs(channels[channel]));
    const float envelopeCoefficient = peak > m_compressorEnvelope ? m_compressorAttackCoefficient
                                                                  : m_compressorReleaseCoefficient;
    m_compressorEnvelope =
        envelopeCoefficient * m_compressorEnvelope + (1.0f - envelopeCoefficient) * peak;
    const float inputDb = 20.0f * std::log10(std::max(m_compressorEnvelope, 1.0e-12f));
    const float overDb = std::max(0.0f, inputDb - m_program.compressor.thresholdDb);
    const float reductionDb = overDb / m_program.compressor.ratio - overDb;
    const float compressorGain = dbToLinear(reductionDb + m_program.compressor.makeupGainDb);
    for (size_t channel = 0; channel < channelCount; ++channel)
      channels[channel] *= compressorGain;
  }

  if (m_program.width.enabled && channelCount >= 2) {
    const float mid = (channels[0] + channels[1]) * 0.5f;
    const float side = (channels[0] - channels[1]) * 0.5f * m_program.width.amount;
    channels[0] = mid + side;
    channels[1] = mid - side;
  }

  if (m_program.limiter.enabled) {
    float peak = 0.0f;
    for (size_t channel = 0; channel < channelCount; ++channel)
      peak = std::max(peak, std::abs(channels[channel]));
    const float requiredGain = peak > m_limiterCeilingLinear ? m_limiterCeilingLinear / peak : 1.0f;
    if (requiredGain < m_limiterGain)
      m_limiterGain = requiredGain;
    else
      m_limiterGain =
          m_limiterReleaseCoefficient * m_limiterGain + (1.0f - m_limiterReleaseCoefficient);
    for (size_t channel = 0; channel < channelCount; ++channel)
      channels[channel] *= m_limiterGain;
  }
}

} // namespace orpheus
