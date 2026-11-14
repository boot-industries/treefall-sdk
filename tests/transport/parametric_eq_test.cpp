// SPDX-License-Identifier: MIT
#include "../support/fnv1a64.hpp"
#include "../support/rt_guard.hpp"
#include "transport/parametric_eq_design.h"
#include <orpheus/parametric_eq.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

namespace orpheus {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr float kButterworthQ = 0.70710678f;

EqBandSettings band(EqBandType type, EqBandOrder order, float frequencyHz, float gainDb, float q) {
  return {true, type, order, frequencyHz, gainDb, q};
}

EqBandSettings second(EqBandType type, float frequencyHz, float gainDb, float q) {
  return band(type, EqBandOrder::Second, frequencyHz, gainDb, q);
}

EqBandSettings first(EqBandType type, float frequencyHz, float gainDb) {
  return band(type, EqBandOrder::First, frequencyHz, gainDb, kButterworthQ);
}

ParametricEqSettings single(const EqBandSettings& settings) {
  ParametricEqSettings eq;
  eq.enabled = true;
  eq.bands[0] = settings;
  return eq;
}

ParametricEqSettings sixBandLayout() {
  ParametricEqSettings eq;
  eq.enabled = true;
  eq.bands[0] = second(EqBandType::HighPass, 30.0f, 0.0f, kButterworthQ);
  eq.bands[1] = second(EqBandType::LowShelf, 160.0f, 4.0f, 0.3f);
  eq.bands[2] = second(EqBandType::Bell, 1000.0f, -3.0f, 1.5f);
  eq.bands[3] = second(EqBandType::Bell, 4000.0f, 2.5f, 2.0f);
  eq.bands[4] = second(EqBandType::HighShelf, 16000.0f, -5.0f, 0.1f);
  eq.bands[5] = second(EqBandType::LowPass, 18000.0f, 0.0f, kButterworthQ);
  return eq;
}

/// Analog prototype of one band, s = j f / f0.
std::complex<double> analogResponse(const EqBandSettings& settings, double frequency) {
  using C = std::complex<double>;
  const C s(0.0, frequency / static_cast<double>(settings.frequencyHz));
  const double gainDb = settings.gainDb;
  const double q = settings.q;
  const double a = std::pow(10.0, gainDb / 40.0);
  const double g = std::pow(10.0, gainDb / 20.0);
  const double r = std::sqrt(g);
  const double rootA = std::sqrt(a);
  if (settings.order == EqBandOrder::First) {
    switch (settings.type) {
    case EqBandType::LowShelf:
      return g * (s / r + 1.0) / (s * r + 1.0);
    case EqBandType::HighShelf:
      return (s * r + 1.0) / (s / r + 1.0);
    case EqBandType::TiltShelf:
      return (s * r + 1.0) / (s / r + 1.0) / r;
    case EqBandType::LowPass:
      return 1.0 / (s + 1.0);
    case EqBandType::HighPass:
      return s / (s + 1.0);
    default:
      ADD_FAILURE() << "no first-order prototype";
      return 1.0;
    }
  }
  const C s2 = s * s;
  const C poles = s2 + s / q + 1.0;
  switch (settings.type) {
  case EqBandType::LowShelf:
    return a * (s2 + (rootA / q) * s + a) / (a * s2 + (rootA / q) * s + 1.0);
  case EqBandType::HighShelf:
    return a * (a * s2 + (rootA / q) * s + 1.0) / (s2 + (rootA / q) * s + a);
  case EqBandType::TiltShelf:
    return (a * s2 + (rootA / q) * s + 1.0) / (s2 + (rootA / q) * s + a);
  case EqBandType::Bell:
    return (s2 + s * a / q + 1.0) / (s2 + s / (a * q) + 1.0);
  case EqBandType::LowPass:
    return 1.0 / poles;
  case EqBandType::HighPass:
    return s2 / poles;
  case EqBandType::BandPass:
    return (s / q) / poles;
  default:
    ADD_FAILURE() << "no second-order prototype";
    return 1.0;
  }
}

double analogDb(const ParametricEqSettings& settings, double frequency) {
  double total = 0.0;
  for (const auto& entry : settings.bands) {
    if (entry.enabled)
      total += 20.0 * std::log10(std::abs(analogResponse(entry, frequency)));
  }
  return total;
}

std::vector<float> sweep(double sampleRate) {
  std::vector<float> frequencies;
  const double top = std::min(20000.0, 0.499 * sampleRate);
  for (double frequency = 20.0; frequency <= top; frequency *= 1.01)
    frequencies.push_back(static_cast<float>(frequency));
  return frequencies;
}

std::vector<float> queryDb(const ParametricEqSettings& settings, double sampleRate,
                           const std::vector<float>& frequencies) {
  std::vector<float> magnitudes(frequencies.size(), 0.0f);
  EXPECT_EQ(parametricEqMagnitudeDb(settings, sampleRate, frequencies, magnitudes),
            ParametricEqValidationError::OK);
  return magnitudes;
}

float queryDbAt(const ParametricEqSettings& settings, double sampleRate, float frequency) {
  return queryDb(settings, sampleRate, {frequency}).front();
}

/// Steady-state gain of a mono processor at an integer frequency, by lock-in
/// detection over exactly one second after half a second of settling.
double lockInGainDb(ParametricEqProcessor& processor, double sampleRate, double frequency) {
  const auto settle = static_cast<size_t>(sampleRate / 2.0);
  const auto measured = static_cast<size_t>(sampleRate);
  double inPhase = 0.0;
  double quadrature = 0.0;
  for (size_t n = 0; n < settle + measured; ++n) {
    const double phase = 2.0 * kPi * frequency * static_cast<double>(n) / sampleRate;
    float sample = static_cast<float>(0.25 * std::sin(phase));
    processor.processFrame(&sample, 1);
    if (n >= settle) {
      inPhase += sample * std::sin(phase);
      quadrature += sample * std::cos(phase);
    }
  }
  const double amplitude =
      2.0 * std::hypot(inPhase, quadrature) / static_cast<double>(measured) / 0.25;
  return 20.0 * std::log10(amplitude);
}

struct DesignReference {
  EqBandSettings band;
  std::array<double, 5> expected;
};

TEST(ParametricEqTest, DesignMatchesReferenceValues) {
  // Independent double-precision model of the documented design formulas,
  // evaluated at the float parameter values the API receives.
  const std::array<DesignReference, 13> references{{
      {second(EqBandType::LowShelf, 160.0f, 12.0f, 0.1f),
       {0.00740965663195, 9.99466101282, 1.0032415189, 19.9426308146, 3.98107170553}},
      {second(EqBandType::LowShelf, 160.0f, -12.0f, 0.3f),
       {0.0147892840834, 3.33280796158, 0.999697682118, 1.67030535705, 0.251188643151}},
      {second(EqBandType::HighShelf, 16000.0f, 12.0f, 0.1f),
       {0.50809631504, 3.4021921597, 2.01415714453, 6.44382728135, 1.0}},
      {second(EqBandType::HighShelf, 16000.0f, -6.0f, 0.3f),
       {0.665262971074, 2.56237977697, 0.681126071682, 1.95691358445, 1.0}},
      {second(EqBandType::TiltShelf, 1600.0f, 12.0f, 0.7071f),
       {0.148129502413, 1.42050142696, 1.99518813949, 1.41577955361, 0.501187233627}},
      {second(EqBandType::Bell, 1000.0f, 6.0f, 1.0f),
       {0.065529147309, 0.709023347481, 1.00130093338, 1.4146867665, 1.0}},
      {second(EqBandType::Notch, 60.0f, 0.0f, 10.0f),
       {0.00392701094213, 0.100000623444, 0.999999968749, 0.0, 1.0}},
      {second(EqBandType::LowPass, 15000.0f, 0.0f, 0.7071f),
       {1.02963641344, 1.69096449429, 0.429663074955, 1.3109737983, 1.0}},
      {second(EqBandType::HighPass, 80.0f, 0.0f, 0.7071f),
       {0.0052360065156, 1.41423503327, 0.999999999724, 0.0, 0.0}},
      {second(EqBandType::BandPass, 2000.0f, 0.0f, 2.0f),
       {0.131594355854, 0.503289132863, 0.0420160845963, 0.501531474674, 0.0}},
      {second(EqBandType::AllPass, 500.0f, 0.0f, 0.7071f),
       {0.0327295016747, 1.41453413016, 1.0, -1.41453413016, 1.0}},
      {band(EqBandType::LowShelf, EqBandOrder::First, 160.0f, 12.0f, 0.7071f),
       {0.0724462998303, 13.8757737623, 1.00008288431, 14.0928854561, 3.98107170553}},
      {band(EqBandType::HighPass, EqBandOrder::First, 80.0f, 0.0f, 0.7071f),
       {0.0723600540867, 13.8921396634, 0.999994444451, 13.8197028329, 0.0}},
  }};

  for (const auto& reference : references) {
    const auto design = detail::designEqBand(reference.band, 48000.0);
    const std::array<double, 5> actual{design.g, design.k, design.m0, design.m1, design.m2};
    for (size_t index = 0; index < actual.size(); ++index) {
      const double expected = reference.expected[index];
      const double tolerance = expected == 0.0 ? 1.0e-12 : std::abs(expected) * 1.0e-9;
      EXPECT_NEAR(actual[index], expected, tolerance)
          << "type " << static_cast<int>(reference.band.type) << " order "
          << static_cast<int>(reference.band.order) << " f0 " << reference.band.frequencyHz
          << " value " << index;
    }
  }
}

struct AccuracyCase {
  EqBandSettings band;
  double limitDb;
};

TEST(ParametricEqTest, MagnitudeMatchesAnalogPrototype) {
  const std::array<AccuracyCase, 16> cases{{
      {second(EqBandType::LowShelf, 160.0f, 12.0f, 0.1f), 0.05},
      {second(EqBandType::LowShelf, 160.0f, 12.0f, 0.3f), 0.05},
      {second(EqBandType::LowShelf, 160.0f, -12.0f, kButterworthQ), 0.05},
      {second(EqBandType::HighShelf, 16000.0f, 12.0f, 0.1f), 0.45},
      {second(EqBandType::HighShelf, 16000.0f, 12.0f, 0.3f), 0.45},
      {second(EqBandType::Bell, 1000.0f, 12.0f, 1.0f), 0.05},
      {second(EqBandType::Bell, 10000.0f, 12.0f, 1.0f), 0.9},
      {second(EqBandType::Bell, 3000.0f, -24.0f, 20.0f), 0.15},
      {second(EqBandType::LowPass, 15000.0f, 0.0f, kButterworthQ), 0.7},
      {second(EqBandType::HighPass, 30.0f, 0.0f, kButterworthQ), 0.05},
      {second(EqBandType::BandPass, 10000.0f, 0.0f, kButterworthQ), 0.8},
      {second(EqBandType::TiltShelf, 1600.0f, 12.0f, 0.3f), 0.2},
      {first(EqBandType::LowShelf, 160.0f, 12.0f), 0.05},
      {first(EqBandType::HighShelf, 16000.0f, 12.0f), 0.4},
      {first(EqBandType::LowPass, 10000.0f, 0.0f), 0.6},
      {first(EqBandType::HighPass, 80.0f, 0.0f), 0.05},
  }};

  for (const double sampleRate : {44100.0, 48000.0, 96000.0}) {
    const auto frequencies = sweep(sampleRate);
    for (const auto& entry : cases) {
      const auto settings = single(entry.band);
      const auto magnitudes = queryDb(settings, sampleRate, frequencies);
      double worst = 0.0;
      for (size_t index = 0; index < frequencies.size(); ++index) {
        const double analog = analogDb(settings, frequencies[index]);
        if (analog > -40.0)
          worst = std::max(worst, std::abs(magnitudes[index] - analog));
      }
      EXPECT_LE(worst, entry.limitDb)
          << "fs " << sampleRate << " type " << static_cast<int>(entry.band.type) << " order "
          << static_cast<int>(entry.band.order) << " f0 " << entry.band.frequencyHz;
    }
  }
}

TEST(ParametricEqTest, TiltPairPivotsAndMirrors) {
  for (const float q : {0.1f, 0.3f}) {
    ParametricEqSettings tilt;
    tilt.enabled = true;
    tilt.bands[0] = second(EqBandType::LowShelf, 160.0f, -12.0f, q);
    tilt.bands[1] = second(EqBandType::HighShelf, 16000.0f, 12.0f, q);
    auto mirrored = tilt;
    mirrored.bands[0].gainDb = 12.0f;
    mirrored.bands[1].gainDb = -12.0f;

    for (const double sampleRate : {44100.0, 48000.0, 96000.0}) {
      const bool high = sampleRate > 50000.0;
      EXPECT_LE(std::abs(queryDbAt(tilt, sampleRate, 1600.0f)), high ? 0.12 : 0.35)
          << "q " << q << " fs " << sampleRate;

      const auto frequencies = sweep(sampleRate);
      const auto magnitudes = queryDb(tilt, sampleRate, frequencies);
      const auto mirror = queryDb(mirrored, sampleRate, frequencies);
      double worstAnalog = 0.0;
      double worstMirror = 0.0;
      for (size_t index = 0; index < frequencies.size(); ++index) {
        worstAnalog =
            std::max(worstAnalog, std::abs(magnitudes[index] - analogDb(tilt, frequencies[index])));
        worstMirror =
            std::max(worstMirror, std::abs(static_cast<double>(magnitudes[index]) + mirror[index]));
      }
      EXPECT_LE(worstAnalog, high ? 0.15 : 0.35) << "q " << q << " fs " << sampleRate;
      EXPECT_LE(worstMirror, 1.0e-6) << "q " << q << " fs " << sampleRate;
    }
  }
}

TEST(ParametricEqTest, BellCentreAndNotchZeroLandOnTarget) {
  for (const double sampleRate : {44100.0, 48000.0}) {
    for (const auto& bell : {second(EqBandType::Bell, 8000.0f, 12.0f, 4.0f),
                             second(EqBandType::Bell, 12000.0f, -12.0f, 2.0f)}) {
      EXPECT_NEAR(queryDbAt(single(bell), sampleRate, bell.frequencyHz), bell.gainDb, 0.02)
          << "fs " << sampleRate << " f0 " << bell.frequencyHz;
    }
    for (const float frequency : {60.0f, 1000.0f, 12000.0f}) {
      const auto notch = second(EqBandType::Notch, frequency, 0.0f, 2.0f);
      EXPECT_LE(queryDbAt(single(notch), sampleRate, frequency), -60.0f)
          << "fs " << sampleRate << " f0 " << frequency;
    }
  }
}

TEST(ParametricEqTest, AllPassIsUnityMagnitude) {
  for (const double sampleRate : {44100.0, 48000.0, 96000.0}) {
    ParametricEqSettings settings;
    settings.enabled = true;
    settings.bands[0] = second(EqBandType::AllPass, 500.0f, 0.0f, kButterworthQ);
    settings.bands[1] = second(EqBandType::AllPass, 12000.0f, 0.0f, 8.0f);
    for (const float magnitude : queryDb(settings, sampleRate, sweep(sampleRate)))
      EXPECT_LE(std::abs(magnitude), 1.0e-6f) << "fs " << sampleRate;
  }
}

TEST(ParametricEqTest, ProcessorRealizesQueriedMagnitude) {
  {
    const auto settings = sixBandLayout();
    for (const double frequency : {40.0, 160.0, 1000.0, 4000.0, 12000.0}) {
      ParametricEqProcessor processor;
      ASSERT_EQ(processor.prepare(settings, 48000.0, 1), ParametricEqValidationError::OK);
      EXPECT_NEAR(lockInGainDb(processor, 48000.0, frequency),
                  queryDbAt(settings, 48000.0, static_cast<float>(frequency)), 0.02)
          << "frequency " << frequency;
    }
  }
  for (const auto& entry :
       {second(EqBandType::HighPass, 20.0f, 0.0f, kButterworthQ),
        second(EqBandType::LowShelf, 20.0f, 12.0f, kButterworthQ),
        first(EqBandType::LowShelf, 40.0f, 12.0f), first(EqBandType::HighPass, 20.0f, 0.0f)}) {
    const auto settings = single(entry);
    ParametricEqProcessor processor;
    ASSERT_EQ(processor.prepare(settings, 192000.0, 1), ParametricEqValidationError::OK);
    EXPECT_NEAR(lockInGainDb(processor, 192000.0, entry.frequencyHz),
                queryDbAt(settings, 192000.0, entry.frequencyHz), 0.02)
        << "type " << static_cast<int>(entry.type) << " order " << static_cast<int>(entry.order);
  }
}

struct TransitionResult {
  double ratio = 0.0;
  bool exactIdentityAfter = true;
};

/// Mono 48 kHz sine; transition at frame 48000. Ratio of the largest sample
/// step during the transition window to the larger steady-state step.
TransitionResult measureTransition(const ParametricEqSettings& from, const ParametricEqSettings& to,
                                   double frequency) {
  constexpr double kRate = 48000.0;
  ParametricEqProcessor processor;
  ParametricEqProcessor next;
  EXPECT_EQ(processor.prepare(from, kRate, 1), ParametricEqValidationError::OK);
  EXPECT_EQ(next.prepare(to, kRate, 1), ParametricEqValidationError::OK);

  TransitionResult result;
  double steadyBefore = 0.0;
  double steadyAfter = 0.0;
  double transition = 0.0;
  float previous = 0.0f;
  for (size_t n = 0; n < 96000; ++n) {
    if (n == 48000)
      processor.transitionTo(next);
    const float input =
        static_cast<float>(0.25 * std::sin(2.0 * kPi * frequency * static_cast<double>(n) / kRate));
    float sample = input;
    processor.processFrame(&sample, 1);
    const double step = std::abs(static_cast<double>(sample) - previous);
    previous = sample;
    if (n >= 24000 && n < 48000)
      steadyBefore = std::max(steadyBefore, step);
    else if (n >= 48000 && n < 60000)
      transition = std::max(transition, step);
    else if (n >= 72000)
      steadyAfter = std::max(steadyAfter, step);
    if (n >= 72000 && std::bit_cast<uint32_t>(sample) != std::bit_cast<uint32_t>(input))
      result.exactIdentityAfter = false;
  }
  result.ratio = transition / std::max(steadyBefore, steadyAfter);
  EXPECT_EQ(processor.settings(), to);
  return result;
}

TEST(ParametricEqTest, TransitionsAreClickFree) {
  const auto shelf = [](float gainDb, float q) {
    return single(second(EqBandType::LowShelf, 160.0f, gainDb, q));
  };
  struct Case {
    const char* name;
    ParametricEqSettings from;
    ParametricEqSettings to;
    double frequency;
  };
  auto bandOff = shelf(12.0f, 0.3f);
  bandOff.bands[0].enabled = false;
  auto eqOff = shelf(12.0f, kButterworthQ);
  eqOff.enabled = false;
  auto orderOne = shelf(12.0f, kButterworthQ);
  orderOne.bands[0].order = EqBandOrder::First;
  const std::vector<Case> cases{
      {"gain nudge", shelf(12.0f, kButterworthQ), shelf(11.5f, kButterworthQ), 60.0},
      {"shelf Q up", shelf(12.0f, 0.1f), shelf(12.0f, 0.3f), 60.0},
      {"shelf Q down", shelf(-12.0f, 0.3f), shelf(-12.0f, 0.1f), 60.0},
      {"band enable", bandOff, shelf(12.0f, 0.3f), 60.0},
      {"band disable", shelf(12.0f, 0.3f), bandOff, 60.0},
      {"type change", shelf(12.0f, 0.3f), single(second(EqBandType::Bell, 100.0f, 12.0f, 4.0f)),
       60.0},
      {"frequency octave", shelf(12.0f, kButterworthQ),
       single(second(EqBandType::LowShelf, 320.0f, 12.0f, kButterworthQ)), 60.0},
      {"high-pass move", single(second(EqBandType::HighPass, 20.0f, 0.0f, kButterworthQ)),
       single(second(EqBandType::HighPass, 40.0f, 0.0f, kButterworthQ)), 60.0},
      {"bell Q", single(second(EqBandType::Bell, 1000.0f, 12.0f, 4.0f)),
       single(second(EqBandType::Bell, 1000.0f, 12.0f, 8.0f)), 1000.0},
      {"order change", shelf(12.0f, kButterworthQ), orderOne, 60.0},
      {"EQ bypass", shelf(12.0f, kButterworthQ), eqOff, 60.0},
  };

  for (const auto& entry : cases) {
    const auto result = measureTransition(entry.from, entry.to, entry.frequency);
    EXPECT_LE(result.ratio, 1.25) << entry.name;
    const bool disables = !entry.to.enabled || !entry.to.bands[0].enabled;
    if (disables)
      EXPECT_TRUE(result.exactIdentityAfter) << entry.name;
  }

  // A disabled band ramps out, stops processing and keeps its settings.
  ParametricEqProcessor processor;
  ParametricEqProcessor next;
  ASSERT_EQ(processor.prepare(shelf(12.0f, 0.3f), 48000.0, 1), ParametricEqValidationError::OK);
  ASSERT_EQ(next.prepare(bandOff, 48000.0, 1), ParametricEqValidationError::OK);
  processor.transitionTo(next);
  EXPECT_TRUE(processor.active());
  for (size_t n = 0; n < 480; ++n) {
    float sample = 0.125f;
    processor.processFrame(&sample, 1);
  }
  EXPECT_FALSE(processor.active());
  EXPECT_EQ(processor.settings().bands[0].gainDb, 12.0f);
  EXPECT_EQ(processor.settings().bands[0].q, 0.3f);
  EXPECT_FALSE(processor.settings().bands[0].enabled);
}

TEST(ParametricEqTest, DisabledEqIsBitExactIdentity) {
  auto bypassed = sixBandLayout();
  bypassed.enabled = false;
  for (const auto& settings : {ParametricEqSettings{}, bypassed}) {
    ParametricEqProcessor processor;
    ASSERT_EQ(processor.prepare(settings, 48000.0, 2), ParametricEqValidationError::OK);
    EXPECT_FALSE(processor.active());
    for (const float value : {0.375f, -0.625f, 1.0e-30f, 3.0f}) {
      std::array<float, 2> frame{value, -value};
      processor.processFrame(frame.data(), frame.size());
      EXPECT_EQ(std::bit_cast<uint32_t>(frame[0]), std::bit_cast<uint32_t>(value));
      EXPECT_EQ(std::bit_cast<uint32_t>(frame[1]), std::bit_cast<uint32_t>(-value));
    }
  }
}

TEST(ParametricEqTest, ValidationRejectsInvalidBandsAndKeepsPriorState) {
  const auto accepted = sixBandLayout();
  ParametricEqProcessor processor;
  ASSERT_EQ(processor.prepare(accepted, 48000.0, 2), ParametricEqValidationError::OK);

  const auto expectRejected = [&](const ParametricEqSettings& settings, double sampleRate,
                                  size_t channels, ParametricEqValidationError expected) {
    EXPECT_EQ(processor.prepare(settings, sampleRate, channels), expected);
    EXPECT_EQ(processor.settings(), accepted);
  };
  const auto withBand = [&](const EqBandSettings& entry) {
    auto settings = accepted;
    settings.bands[7] = entry;
    settings.bands[7].enabled = false; // disabled bands are validated too
    return settings;
  };

  expectRejected(withBand(second(EqBandType::LowShelf, 160.0f, 6.0f, 4.01f)), 48000.0, 2,
                 ParametricEqValidationError::InvalidBand);
  expectRejected(withBand(second(EqBandType::TiltShelf, 1600.0f, 6.0f, 4.01f)), 48000.0, 2,
                 ParametricEqValidationError::InvalidBand);
  EXPECT_EQ(
      validateParametricEq(
          withBand(band(EqBandType::HighShelf, EqBandOrder::First, 8000.0f, 6.0f, 10.0f)), 48000.0),
      ParametricEqValidationError::OK);
  expectRejected(withBand(band(EqBandType::Bell, EqBandOrder::First, 1000.0f, 6.0f, 1.0f)), 48000.0,
                 2, ParametricEqValidationError::InvalidBand);
  auto badType = second(EqBandType::Bell, 1000.0f, 0.0f, 1.0f);
  badType.type = static_cast<EqBandType>(9);
  expectRejected(withBand(badType), 48000.0, 2, ParametricEqValidationError::InvalidBand);
  auto badOrder = second(EqBandType::Bell, 1000.0f, 0.0f, 1.0f);
  badOrder.order = static_cast<EqBandOrder>(3);
  expectRejected(withBand(badOrder), 48000.0, 2, ParametricEqValidationError::InvalidBand);
  expectRejected(
      withBand(second(EqBandType::Bell, static_cast<float>(0.49 * 48000.0 * 1.001), 0.0f, 1.0f)),
      48000.0, 2, ParametricEqValidationError::InvalidBand);
  expectRejected(
      withBand(second(EqBandType::Bell, 1000.0f, std::numeric_limits<float>::quiet_NaN(), 1.0f)),
      48000.0, 2, ParametricEqValidationError::InvalidBand);
  expectRejected(accepted, 7999.0, 2, ParametricEqValidationError::InvalidSampleRate);
  expectRejected(accepted, 48000.0, 0, ParametricEqValidationError::InvalidChannelCount);
  expectRejected(accepted, 48000.0, 9, ParametricEqValidationError::InvalidChannelCount);
}

TEST(ParametricEqTest, MagnitudeQueryRejectsBadQueries) {
  const auto settings = sixBandLayout();
  constexpr float kSentinel = 123.0f;
  const auto expectInvalid = [&](std::vector<float> frequencies, size_t outputs) {
    std::vector<float> magnitudes(outputs, kSentinel);
    EXPECT_EQ(parametricEqMagnitudeDb(settings, 48000.0, frequencies, magnitudes),
              ParametricEqValidationError::InvalidQuery);
    for (const float value : magnitudes)
      EXPECT_EQ(value, kSentinel);
  };
  expectInvalid({1000.0f, 2000.0f}, 1);
  expectInvalid({1000.0f, 24001.0f}, 2);
  expectInvalid({1000.0f, std::numeric_limits<float>::quiet_NaN()}, 2);
  expectInvalid({-1.0f}, 1);

  // Both band edges of the query domain are accepted.
  std::vector<float> edges{0.0f, 24000.0f};
  std::vector<float> magnitudes(edges.size(), kSentinel);
  EXPECT_EQ(parametricEqMagnitudeDb(settings, 48000.0, edges, magnitudes),
            ParametricEqValidationError::OK);
  for (const float value : magnitudes)
    EXPECT_TRUE(std::isfinite(value));
}

TEST(ParametricEqTest, DesignGridIsStable) {
  const std::array<EqBandType, 9> types{
      EqBandType::Bell,     EqBandType::LowShelf, EqBandType::HighShelf,
      EqBandType::HighPass, EqBandType::LowPass,  EqBandType::Notch,
      EqBandType::BandPass, EqBandType::AllPass,  EqBandType::TiltShelf};
  uint32_t noise = 1u;
  size_t cases = 0;
  for (const double sampleRate : {8000.0, 44100.0, 48000.0, 96000.0, 192000.0, 384000.0}) {
    for (const double requested : {20.0, 50.0, 160.0, 1000.0, 5000.0, 12000.0, 16000.0, -1.0}) {
      const auto frequency = static_cast<float>(requested < 0.0 ? 0.49 * sampleRate : requested);
      if (frequency < 20.0f || frequency > static_cast<float>(0.49 * sampleRate))
        continue;
      for (const float gainDb : {-24.0f, -12.0f, 0.0f, 12.0f, 24.0f}) {
        for (const float q : {0.1f, 0.3f, kButterworthQ, 4.0f, 20.0f}) {
          for (const auto type : types) {
            for (const auto order : {EqBandOrder::First, EqBandOrder::Second}) {
              const auto entry = band(type, order, frequency, gainDb, q);
              const auto settings = single(entry);
              if (validateParametricEq(settings, sampleRate) != ParametricEqValidationError::OK)
                continue;
              ++cases;
              const auto design = detail::designEqBand(entry, sampleRate);
              ASSERT_TRUE(std::isfinite(design.g) && std::isfinite(design.k) &&
                          std::isfinite(design.m0) && std::isfinite(design.m1) &&
                          std::isfinite(design.m2));
              ASSERT_GT(design.g, 0.0);
              ASSERT_GT(design.k, 0.0);

              ParametricEqProcessor processor;
              ASSERT_EQ(processor.prepare(settings, sampleRate, 1),
                        ParametricEqValidationError::OK);
              for (size_t n = 0; n < 256; ++n) {
                noise = noise * 1664525u + 1013904223u;
                float sample = static_cast<float>(static_cast<int32_t>(noise) >> 8) * 0x1p-23f;
                processor.processFrame(&sample, 1);
                ASSERT_TRUE(std::isfinite(sample))
                    << "fs " << sampleRate << " f0 " << frequency << " gain " << gainDb << " q "
                    << q << " type " << static_cast<int>(type);
              }
            }
          }
        }
      }
    }
  }
  EXPECT_GT(cases, 0u);
}

TEST(ParametricEqTest, RealtimeTransitionsAreAllocationFree) {
  ParametricEqSettings a;
  a.enabled = true;
  for (size_t index = 0; index < kParametricEqMaxBands; ++index)
    a.bands[index] = second(EqBandType::Bell, 100.0f * static_cast<float>(index + 1), 3.0f, 1.0f);
  auto b = a;
  for (auto& entry : b.bands)
    entry.gainDb = -3.0f;
  b.bands[3].enabled = false;

  ParametricEqProcessor processor;
  ParametricEqProcessor preparedA;
  ParametricEqProcessor preparedB;
  ASSERT_EQ(processor.prepare(a, 48000.0, 2), ParametricEqValidationError::OK);
  ASSERT_EQ(preparedA.prepare(a, 48000.0, 2), ParametricEqValidationError::OK);
  ASSERT_EQ(preparedB.prepare(b, 48000.0, 2), ParametricEqValidationError::OK);
  orpheus::tests::support::RtGuardState::reset();
  {
    orpheus::tests::support::RtSection realtime;
    std::array<float, 2> frame{0.25f, -0.125f};
    for (size_t index = 0; index < 48000; ++index) {
      if (index == 10000)
        processor.transitionTo(preparedB);
      if (index == 20000)
        processor.transitionTo(preparedA);
      processor.processFrame(frame.data(), frame.size());
      (void)processor.active();
    }
  }
  EXPECT_EQ(orpheus::tests::support::RtGuardState::totalViolations(), 0u);
}

TEST(ParametricEqTest, SixBandRenderHashIsPinned) {
  const auto layout = sixBandLayout();
  auto changed = layout;
  changed.bands[2].gainDb = -1.0f;
  ParametricEqProcessor processor;
  ParametricEqProcessor next;
  ASSERT_EQ(processor.prepare(layout, 48000.0, 2), ParametricEqValidationError::OK);
  ASSERT_EQ(next.prepare(changed, 48000.0, 2), ParametricEqValidationError::OK);

  uint32_t state = 1u;
  uint64_t hash = orpheus::tests::support::kFnv1a64Offset;
  for (size_t n = 0; n < 48000; ++n) {
    if (n == 24000)
      processor.transitionTo(next);
    std::array<float, 2> frame{};
    for (auto& sample : frame) {
      state = state * 1664525u + 1013904223u;
      // Exact scaling: the int-to-float conversion and power-of-two multiply
      // are both exact, so no contraction or rounding mode can change them.
      sample = static_cast<float>(static_cast<int32_t>(state) >> 8) * 0x1p-25f;
    }
    processor.processFrame(frame.data(), frame.size());
    const std::array<uint32_t, 2> bits{std::bit_cast<uint32_t>(frame[0]),
                                       std::bit_cast<uint32_t>(frame[1])};
    hash = orpheus::tests::support::Fnv1a64(bits, hash);
  }
  // Pinned from the first macOS run; Linux and Windows must reproduce it bit for bit.
  EXPECT_EQ(hash, 0x236f9e61d5831444ull) << std::hex << "0x" << hash;
}

TEST(ParametricEqTest, ProcessorIsFixedTriviallyCopyable) {
  static_assert(std::is_trivially_copyable_v<ParametricEqProcessor>);
  SUCCEED();
}

} // namespace
} // namespace orpheus
