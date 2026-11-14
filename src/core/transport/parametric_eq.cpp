// SPDX-License-Identifier: MIT
#include <orpheus/parametric_eq.h>

#include "../common/pinned_math.h"
#include "parametric_eq_design.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace orpheus {
namespace {

// Design constants of the matched bilinear fit (see parametric_eq.h [1]).
constexpr double kSigma = 2.0;
constexpr double kSigmaSquared = kSigma * kSigma;
constexpr double kPi = 3.14159265358979323846;

constexpr float kMinimumFrequencyHz = 20.0f;
constexpr double kMaximumFrequencyRatio = 0.49;
constexpr float kMaximumGainDb = 24.0f;
constexpr float kMinimumQ = 0.1f;
constexpr float kMaximumQ = 20.0f;
constexpr double kMagnitudeFloor = 1.0e-20;

bool finiteInRange(float value, float minimum, float maximum) noexcept {
  return std::isfinite(value) && value >= minimum && value <= maximum;
}

bool isShelf(EqBandType type) noexcept {
  return type == EqBandType::LowShelf || type == EqBandType::HighShelf ||
         type == EqBandType::TiltShelf;
}

bool acceptsFirstOrder(EqBandType type) noexcept {
  return isShelf(type) || type == EqBandType::HighPass || type == EqBandType::LowPass;
}

bool validSampleRate(double sampleRate) noexcept {
  return std::isfinite(sampleRate) && sampleRate >= 8000.0 && sampleRate <= 384000.0;
}

bool validBand(const EqBandSettings& band, double sampleRate) noexcept {
  if (static_cast<uint8_t>(band.type) > static_cast<uint8_t>(EqBandType::TiltShelf))
    return false;
  const auto order = static_cast<uint8_t>(band.order);
  if (order != 1 && order != 2)
    return false;
  if (band.order == EqBandOrder::First && !acceptsFirstOrder(band.type))
    return false;
  const auto maximumFrequency = static_cast<float>(sampleRate * kMaximumFrequencyRatio);
  if (!finiteInRange(band.frequencyHz, kMinimumFrequencyHz, maximumFrequency) ||
      !finiteInRange(band.gainDb, -kMaximumGainDb, kMaximumGainDb) ||
      !finiteInRange(band.q, kMinimumQ, kMaximumQ))
    return false;
  return !(isShelf(band.type) && band.order == EqBandOrder::Second &&
           band.q > kParametricEqMaxShelfQ);
}

double powerOfTen(double exponent) noexcept {
  return pinned::exp(exponent * pinned::kLn10);
}

/// The (nu, rho) pair of one analog root pair at normalized frequency x and
/// damping y, matched to the bilinear warp.
struct MatchedPair {
  double nu;
  double rho;
};

MatchedPair matchedPair(double x, double y) noexcept {
  const double t = 2.0 * y * y - 1.0;
  const double x2 = x * x;
  const double nu = std::sqrt(
      std::max(0.0, (x2 * x2) + 2.0 * x2 * kSigmaSquared * t + kSigmaSquared * kSigmaSquared));
  const double kappa = x2 * t + kSigmaSquared;
  const double rho = std::sqrt(2.0 * std::max(0.0, nu + kappa));
  return {nu, rho};
}

detail::EqSvfDesign secondOrderSection(double zeroX, double zeroY, double poleX, double poleY,
                                       double dcGain) noexcept {
  const MatchedPair pole = matchedPair(poleX, poleY);
  const MatchedPair zero = matchedPair(zeroX, zeroY);
  const double rootPole = std::sqrt(pole.nu);
  return {kPi / rootPole, pole.rho / rootPole, dcGain * zero.nu / pole.nu,
          dcGain * zero.rho / rootPole, dcGain};
}

struct FirstOrderPole {
  double g;
  double k;
  double d;
};

double firstOrderRadius(double x) noexcept {
  return std::sqrt(x * x + kSigmaSquared);
}

FirstOrderPole firstOrderPole(double poleX) noexcept {
  const double radius = firstOrderRadius(poleX);
  const double g = std::sqrt(kPi / radius);
  const double g2 = g * g;
  return {g, (1.0 + g2) / g, (1.0 + g2) / (kPi + radius)};
}

detail::EqSvfDesign firstOrderSection(double zeroX, double poleX, double dcGain) noexcept {
  const FirstOrderPole pole = firstOrderPole(poleX);
  const double zeroRadius = firstOrderRadius(zeroX);
  const double scale = pole.d * dcGain;
  return {pole.g, pole.k, scale * zeroRadius, scale * (kPi + zeroRadius) / pole.g,
          scale * kPi / (pole.g * pole.g)};
}

} // namespace

namespace detail {

EqSvfDesign designEqBand(const EqBandSettings& band, double sampleRate) noexcept {
  const double halfAngle = kPi * static_cast<double>(band.frequencyHz) / sampleRate;
  const double cotangent = pinned::cos(halfAngle) / pinned::sin(halfAngle);
  // Frequency-corrected ratio: keeps bell centres and notch zeros on f0.
  const double w = std::sqrt(kSigmaSquared + kPi * kPi * cotangent * cotangent);
  const double gainDb = static_cast<double>(band.gainDb);
  const double gainLinear = powerOfTen(gainDb / 20.0);
  const double gainHalf = powerOfTen(gainDb / 40.0);
  const double gainQuarter = powerOfTen(gainDb / 80.0);
  const double zeta = 1.0 / (2.0 * static_cast<double>(band.q));

  if (band.order == EqBandOrder::First) {
    switch (band.type) {
    case EqBandType::LowShelf:
      return firstOrderSection(w / gainHalf, w * gainHalf, gainLinear);
    case EqBandType::HighShelf:
      return firstOrderSection(w * gainHalf, w / gainHalf, 1.0);
    case EqBandType::TiltShelf:
      return firstOrderSection(w * gainHalf, w / gainHalf, 1.0 / gainHalf);
    case EqBandType::LowPass:
      return firstOrderSection(0.0, w, 1.0);
    case EqBandType::HighPass:
    default: {
      const FirstOrderPole pole = firstOrderPole(w);
      return {pole.g, pole.k, pole.d * w, pole.d * w / pole.g, 0.0};
    }
    }
  }

  switch (band.type) {
  case EqBandType::LowShelf:
    return secondOrderSection(w / gainQuarter, zeta, w * gainQuarter, zeta, gainLinear);
  case EqBandType::HighShelf:
    return secondOrderSection(w * gainQuarter, zeta, w / gainQuarter, zeta, 1.0);
  case EqBandType::TiltShelf:
    return secondOrderSection(w * gainQuarter, zeta, w / gainQuarter, zeta, 1.0 / gainHalf);
  case EqBandType::Bell:
    return secondOrderSection(w, zeta * gainHalf, w, zeta / gainHalf, 1.0);
  default:
    break;
  }

  const MatchedPair pole = matchedPair(w, zeta);
  const double rootPole = std::sqrt(pole.nu);
  const double g = kPi / rootPole;
  const double k = pole.rho / rootPole;
  switch (band.type) {
  case EqBandType::LowPass:
    return {g, k, kSigmaSquared / pole.nu, 2.0 * kSigma / rootPole, 1.0};
  case EqBandType::HighPass:
    return {g, k, w * w / pole.nu, 0.0, 0.0};
  case EqBandType::BandPass:
    return {g, k, 2.0 * zeta * w * kSigma / pole.nu, 2.0 * zeta * w / rootPole, 0.0};
  case EqBandType::Notch:
    return {g, k, kPi * kPi * cotangent * cotangent / pole.nu, 0.0, 1.0};
  case EqBandType::AllPass:
  default:
    return {g, k, 1.0, -k, 1.0};
  }
}

uint32_t parametricEqTransitionFrames(double sampleRate) noexcept {
  return std::max<uint32_t>(
      1u, static_cast<uint32_t>(sampleRate * kParametricEqTransitionSeconds + 0.5));
}

} // namespace detail

ParametricEqValidationError validateParametricEq(const ParametricEqSettings& settings,
                                                 double sampleRate) noexcept {
  if (!validSampleRate(sampleRate))
    return ParametricEqValidationError::InvalidSampleRate;
  for (const auto& band : settings.bands) {
    if (!validBand(band, sampleRate))
      return ParametricEqValidationError::InvalidBand;
  }
  return ParametricEqValidationError::OK;
}

ParametricEqValidationError parametricEqMagnitudeDb(const ParametricEqSettings& settings,
                                                    double sampleRate,
                                                    std::span<const float> frequenciesHz,
                                                    std::span<float> magnitudesDb) noexcept {
  const auto validation = validateParametricEq(settings, sampleRate);
  if (validation != ParametricEqValidationError::OK)
    return validation;
  if (frequenciesHz.size() != magnitudesDb.size())
    return ParametricEqValidationError::InvalidQuery;
  const double nyquist = 0.5 * sampleRate;
  for (const float frequency : frequenciesHz) {
    if (!std::isfinite(frequency) || frequency < 0.0f || static_cast<double>(frequency) > nyquist)
      return ParametricEqValidationError::InvalidQuery;
  }

  std::array<detail::EqSvfDesign, kParametricEqMaxBands> designs{};
  size_t designCount = 0;
  if (settings.enabled) {
    for (const auto& band : settings.bands) {
      if (band.enabled)
        designs[designCount++] = detail::designEqBand(band, sampleRate);
    }
  }

  // Each result depends only on its own frequency. When the output span starts
  // after the input in memory, write from the end so an overlapping write never
  // lands on a frequency that has not been read yet.
  const bool reverse = std::greater<const float*>{}(magnitudesDb.data(), frequenciesHz.data());
  const size_t count = frequenciesHz.size();
  for (size_t step = 0; step < count; ++step) {
    const size_t index = reverse ? count - 1 - step : step;
    const double frequency = static_cast<double>(frequenciesHz[index]);
    double totalDb = 0.0;
    const bool atNyquist = frequency >= nyquist;
    const double angle = kPi * frequency / sampleRate;
    const double tangent = atNyquist ? 0.0 : pinned::sin(angle) / pinned::cos(angle);
    for (size_t band = 0; band < designCount; ++band) {
      const auto& design = designs[band];
      double power = design.m0 * design.m0;
      if (!atNyquist) {
        const double u = tangent / design.g;
        const double u2 = u * u;
        const double real = design.m2 - design.m0 * u2;
        const double imaginary = design.m1 * u;
        const double poleReal = 1.0 - u2;
        const double poleImaginary = design.k * u;
        power = (real * real + imaginary * imaginary) /
                (poleReal * poleReal + poleImaginary * poleImaginary);
      }
      totalDb += 10.0 * std::log10(std::max(power, kMagnitudeFloor));
    }
    magnitudesDb[index] = static_cast<float>(totalDb);
  }
  return ParametricEqValidationError::OK;
}

namespace {

/// Same filter poles, unity mix: the output is bit-exact input.
template <typename Parameters> Parameters identityOf(const Parameters& parameters) noexcept {
  Parameters identity = parameters;
  identity.m0 = 1.0f;
  identity.m1x = 0.0f;
  identity.m2x = 0.0f;
  return identity;
}

} // namespace

ParametricEqValidationError ParametricEqProcessor::prepare(const ParametricEqSettings& settings,
                                                           double sampleRate,
                                                           size_t channelCount) noexcept {
  const auto validation = validateParametricEq(settings, sampleRate);
  if (validation != ParametricEqValidationError::OK)
    return validation;
  if (channelCount == 0 || channelCount > kParametricEqMaxChannels)
    return ParametricEqValidationError::InvalidChannelCount;

  ParametricEqProcessor prepared;
  prepared.m_settings = settings;
  prepared.m_sampleRate = sampleRate;
  prepared.m_channelCount = channelCount;
  prepared.m_transitionFrames = detail::parametricEqTransitionFrames(sampleRate);
  for (size_t index = 0; index < kParametricEqMaxBands; ++index) {
    const auto& band = settings.bands[index];
    const auto design = detail::designEqBand(band, sampleRate);
    SvfParameters parameters;
    parameters.g = static_cast<float>(design.g);
    parameters.k = static_cast<float>(design.k);
    parameters.m0 = static_cast<float>(design.m0);
    parameters.m1x = static_cast<float>(design.m1 - design.k * design.m0);
    parameters.m2x = static_cast<float>(design.m2 - design.m0);

    auto& runtime = prepared.m_bands[index];
    runtime.enabled = settings.enabled && band.enabled;
    runtime.target = runtime.enabled ? parameters : identityOf(parameters);
  }

  *this = prepared;
  reset();
  return ParametricEqValidationError::OK;
}

void ParametricEqProcessor::reset() noexcept {
  for (size_t index = 0; index < kParametricEqMaxBands; ++index) {
    auto& band = m_bands[index];
    band.framesRemaining = 0;
    band.current = band.target;
    band.coefficients = deriveCoefficients(band.current);
    band.running = band.enabled;
    m_state[index].fill({});
  }
}

ParametricEqProcessor::SvfCoefficients
ParametricEqProcessor::deriveCoefficients(const SvfParameters& parameters) noexcept {
  SvfCoefficients coefficients;
  const float gk = parameters.g + parameters.k;
  const float gt0 = 1.0f / (1.0f + parameters.g * gk);
  coefficients.gt1 = parameters.g * gt0;
  coefficients.gk1 = parameters.g * gk * gt0;
  coefficients.gt2 = parameters.g * coefficients.gt1;
  // y = m0 x + m1x v1 + m2x v2 with v1 = gt0 ic1 + gt1 t0 and
  // v2 = ic2 + gt1 ic1 + gt2 t0, t0 = x - ic2, regrouped by x, ic1 and ic2.
  const float throughGain = parameters.m1x * coefficients.gt1 + parameters.m2x * coefficients.gt2;
  coefficients.inputGain = parameters.m0 + throughGain;
  coefficients.ic1Gain = parameters.m1x * gt0 + parameters.m2x * coefficients.gt1;
  coefficients.ic2Gain = parameters.m2x - throughGain;
  return coefficients;
}

void ParametricEqProcessor::beginRamp(size_t band, const SvfParameters& target) noexcept {
  auto& runtime = m_bands[band];
  runtime.target = target;
  runtime.running = true;
  if (runtime.current == target) {
    runtime.framesRemaining = 0;
    runtime.coefficients = deriveCoefficients(runtime.current);
    if (!runtime.enabled)
      stopBand(band);
    return;
  }
  const auto frames = static_cast<float>(m_transitionFrames);
  runtime.step.g = (target.g - runtime.current.g) / frames;
  runtime.step.k = (target.k - runtime.current.k) / frames;
  runtime.step.m0 = (target.m0 - runtime.current.m0) / frames;
  runtime.step.m1x = (target.m1x - runtime.current.m1x) / frames;
  runtime.step.m2x = (target.m2x - runtime.current.m2x) / frames;
  runtime.framesRemaining = m_transitionFrames;
}

void ParametricEqProcessor::advanceRamp(size_t band) noexcept {
  auto& runtime = m_bands[band];
  --runtime.framesRemaining;
  if (runtime.framesRemaining == 0) {
    runtime.current = runtime.target;
    if (!runtime.enabled) {
      stopBand(band);
      return;
    }
  } else {
    runtime.current.g += runtime.step.g;
    runtime.current.k += runtime.step.k;
    runtime.current.m0 += runtime.step.m0;
    runtime.current.m1x += runtime.step.m1x;
    runtime.current.m2x += runtime.step.m2x;
  }
  runtime.coefficients = deriveCoefficients(runtime.current);
}

void ParametricEqProcessor::stopBand(size_t band) noexcept {
  auto& runtime = m_bands[band];
  runtime.running = false;
  runtime.framesRemaining = 0;
  runtime.coefficients = deriveCoefficients(runtime.current);
  m_state[band].fill({});
}

void ParametricEqProcessor::transitionTo(const ParametricEqProcessor& prepared) noexcept {
  if (m_channelCount == 0 || m_channelCount != prepared.m_channelCount ||
      m_sampleRate != prepared.m_sampleRate) {
    *this = prepared;
    return;
  }
  m_settings = prepared.m_settings;
  for (size_t index = 0; index < kParametricEqMaxBands; ++index) {
    auto& runtime = m_bands[index];
    const auto& next = prepared.m_bands[index];
    const bool wasEnabled = runtime.enabled;
    runtime.enabled = next.enabled;
    if (next.enabled) {
      if (!runtime.running) {
        m_state[index].fill({});
        runtime.current = identityOf(next.target);
        beginRamp(index, next.target);
      } else if (runtime.framesRemaining == 0 || !(runtime.target == next.target)) {
        beginRamp(index, next.target);
      }
      // Otherwise the pending ramp already heads to this target: keep it.
    } else if (runtime.running) {
      // A band already ramping out keeps its ramp; re-requesting the bypass
      // must not postpone it.
      if (wasEnabled)
        beginRamp(index, identityOf(runtime.current));
    } else {
      runtime.target = next.target;
      runtime.current = next.current;
      runtime.coefficients = next.coefficients;
      runtime.framesRemaining = 0;
    }
  }
}

void ParametricEqProcessor::processFrame(float* channels, size_t channelCount) noexcept {
  if (channels == nullptr || channelCount == 0)
    return;
  channelCount = std::min({channelCount, m_channelCount, kParametricEqMaxChannels});

  for (size_t band = 0; band < kParametricEqMaxBands; ++band) {
    if (!m_bands[band].running)
      continue;
    if (m_bands[band].framesRemaining > 0) {
      advanceRamp(band);
      if (!m_bands[band].running)
        continue;
    }
    const auto& c = m_bands[band].coefficients;
    auto& states = m_state[band];
    for (size_t channel = 0; channel < channelCount; ++channel) {
      auto& state = states[channel];
      const float x = channels[channel];
      const float y = c.inputGain * x + (c.ic1Gain * state.ic1 + c.ic2Gain * state.ic2);
      const float t0 = x - state.ic2;
      const float t1 = c.gt1 * t0 - c.gk1 * state.ic1;
      const float t2 = c.gt2 * t0 + c.gt1 * state.ic1;
      state.ic1 = state.ic1 + 2.0f * t1;
      state.ic2 = state.ic2 + 2.0f * t2;
      channels[channel] = y;
    }
  }
}

bool ParametricEqProcessor::active() const noexcept {
  return std::any_of(m_bands.begin(), m_bands.end(),
                     [](const BandRuntime& band) { return band.running; });
}

} // namespace orpheus
