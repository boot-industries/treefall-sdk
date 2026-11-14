// SPDX-License-Identifier: MIT
#include "../support/rt_guard.hpp"
#include <orpheus/clip_dsp.h>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <type_traits>

namespace orpheus {
namespace {

constexpr double kPi = 3.14159265358979323846;

/// Steady-state gain in dB of a mono processor at an integer frequency, by
/// lock-in detection over one second after half a second of settling.
double lockInGainDb(ClipDspProcessor& processor, double sampleRate, double frequency) {
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
  return 20.0 *
         std::log10(2.0 * std::hypot(inPhase, quadrature) / static_cast<double>(measured) / 0.25);
}

TEST(ClipDspTest, LowShelfQShapesTheResponse) {
  // A low Q widens the shelf transition: +12 dB at 160 Hz with Q 0.1 still
  // lifts 1 kHz by about 4.25 dB, while the classic slope leaves it flat.
  for (const auto& [q, expectedDb] : {std::pair{0.1f, 4.251}, std::pair{0.70710678f, 0.011}}) {
    ClipDspProgram program;
    program.eq.enabled = true;
    program.eq.bands[0] = {true, EqBandType::LowShelf, EqBandOrder::Second, 160.0f, 12.0f, q};
    ClipDspProcessor processor;
    ASSERT_EQ(processor.prepare(program, 48000.0, 1), ClipDspValidationError::OK);
    EXPECT_NEAR(lockInGainDb(processor, 48000.0, 1000.0), expectedDb, 0.05) << "q " << q;
  }
}

TEST(ClipDspTest, DefaultProgramIsExactlyTransparent) {
  ClipDspProcessor processor;
  ASSERT_EQ(processor.prepare({}, 48000.0, 2), ClipDspValidationError::OK);

  std::array<float, 2> frame{0.375f, -0.625f};
  processor.processFrame(frame.data(), frame.size());

  EXPECT_FLOAT_EQ(frame[0], 0.375f);
  EXPECT_FLOAT_EQ(frame[1], -0.625f);
}

TEST(ClipDspTest, InvalidPrepareRetainsPriorProgram) {
  ClipDspProgram accepted;
  accepted.width.enabled = true;
  accepted.width.amount = 0.0f;

  ClipDspProcessor processor;
  ASSERT_EQ(processor.prepare(accepted, 48000.0, 2), ClipDspValidationError::OK);

  auto invalid = accepted;
  invalid.limiter.ceilingDb = 1.0f;
  EXPECT_EQ(processor.prepare(invalid, 48000.0, 2), ClipDspValidationError::InvalidLimiter);
  EXPECT_EQ(processor.program(), accepted);
}

TEST(ClipDspTest, WidthStageCollapsesStereoToMono) {
  ClipDspProgram program;
  program.width.enabled = true;
  program.width.amount = 0.0f;
  ClipDspProcessor processor;
  ASSERT_EQ(processor.prepare(program, 48000.0, 2), ClipDspValidationError::OK);

  std::array<float, 2> frame{1.0f, -0.5f};
  processor.processFrame(frame.data(), frame.size());

  EXPECT_FLOAT_EQ(frame[0], 0.25f);
  EXPECT_FLOAT_EQ(frame[1], 0.25f);
}

TEST(ClipDspTest, LimiterRunsAfterWidthAndHonorsCeiling) {
  ClipDspProgram program;
  program.width.enabled = true;
  program.width.amount = 2.0f;
  program.limiter.enabled = true;
  program.limiter.ceilingDb = -6.02059991f;
  ClipDspProcessor processor;
  ASSERT_EQ(processor.prepare(program, 48000.0, 2), ClipDspValidationError::OK);

  std::array<float, 2> frame{0.8f, -0.8f};
  processor.processFrame(frame.data(), frame.size());

  EXPECT_NEAR(std::abs(frame[0]), 0.5f, 1.0e-5f);
  EXPECT_NEAR(std::abs(frame[1]), 0.5f, 1.0e-5f);
}

TEST(ClipDspTest, EnabledEqChangesAnImpulseWithoutProducingInvalidSamples) {
  ClipDspProgram program;
  program.eq.enabled = true;
  program.eq.bands[0].enabled = true;
  program.eq.bands[0].type = EqBandType::Bell;
  program.eq.bands[0].frequencyHz = 1000.0f;
  program.eq.bands[0].gainDb = 6.0f;
  ClipDspProcessor processor;
  ASSERT_EQ(processor.prepare(program, 48000.0, 1), ClipDspValidationError::OK);

  float first = 1.0f;
  processor.processFrame(&first, 1);
  EXPECT_TRUE(std::isfinite(first));
  EXPECT_NE(first, 1.0f);

  float tail = 0.0f;
  processor.processFrame(&tail, 1);
  EXPECT_TRUE(std::isfinite(tail));
  EXPECT_NE(tail, 0.0f);
}

TEST(ClipDspTest, EnabledFullChainIsAllocationFreeInRealtimeSection) {
  ClipDspProgram program;
  program.gate.enabled = true;
  program.eq.enabled = true;
  for (auto& band : program.eq.bands) {
    band.enabled = true;
    band.gainDb = 1.0f;
  }
  program.compressor.enabled = true;
  program.width.enabled = true;
  program.limiter.enabled = true;
  auto changed = program;
  for (auto& band : changed.eq.bands)
    band.gainDb = -2.0f;
  changed.width.amount = 0.5f;

  ClipDspProcessor processor;
  ClipDspProcessor next;
  ASSERT_EQ(processor.prepare(program, 48000.0, 2), ClipDspValidationError::OK);
  ASSERT_EQ(next.prepare(changed, 48000.0, 2), ClipDspValidationError::OK);
  orpheus::tests::support::RtGuardState::reset();
  {
    orpheus::tests::support::RtSection realtime;
    std::array<float, 2> frame{0.25f, -0.125f};
    for (size_t index = 0; index < 48000; ++index) {
      if (index == 24000)
        processor.transitionTo(next);
      processor.processFrame(frame.data(), frame.size());
    }
  }
  EXPECT_EQ(orpheus::tests::support::RtGuardState::totalViolations(), 0u);
}

TEST(ClipDspTest, CompressorBelowThresholdAppliesOnlyTheConstantMakeupGain) {
  // ORP273 F-06: below the threshold the compressor does not reduce, so the
  // applied gain is exactly the constant makeup gain and the per-frame pow is
  // skipped. The result must be unchanged by that shortcut.
  ClipDspProgram program;
  program.compressor.enabled = true;
  program.compressor.thresholdDb = 0.0f;
  program.compressor.ratio = 4.0f;
  program.compressor.makeupGainDb = 6.0f;

  ClipDspProcessor processor;
  ASSERT_EQ(processor.prepare(program, 48000.0, 1), ClipDspValidationError::OK);

  const float input = 0.01f;
  const float expected = input * std::pow(10.0f, 6.0f / 20.0f);
  float first = 0.0f;
  for (size_t index = 0; index < 64; ++index) {
    // The input is presented afresh every frame: a growing signal would
    // eventually cross the threshold and start reducing, which is a different
    // branch of the gain law.
    float sample = input;
    processor.processFrame(&sample, 1);
    if (index == 0) {
      first = sample;
    } else {
      EXPECT_FLOAT_EQ(sample, first) << "the applied gain must not drift between frames";
    }
    EXPECT_NEAR(sample, expected, 1e-6f);
  }
}

/// Largest sample step on channel 0 while a stereo 60 Hz sine runs for one
/// second and `swap` is applied at the half-second mark, relative to the
/// larger steady-state step before and after.
template <typename Swap> double transitionStepRatio(ClipDspProcessor& processor, Swap&& swap) {
  double steadyBefore = 0.0;
  double transition = 0.0;
  double steadyAfter = 0.0;
  float previous = 0.0f;
  for (size_t n = 0; n < 48000; ++n) {
    if (n == 24000)
      swap();
    const auto input =
        static_cast<float>(0.25 * std::sin(2.0 * kPi * 60.0 * static_cast<double>(n) / 48000.0));
    std::array<float, 2> frame{input, input};
    processor.processFrame(frame.data(), frame.size());
    const double step = std::abs(static_cast<double>(frame[0]) - previous);
    previous = frame[0];
    if (n >= 12000 && n < 24000)
      steadyBefore = std::max(steadyBefore, step);
    else if (n >= 24000 && n < 30000)
      transition = std::max(transition, step);
    else if (n >= 36000)
      steadyAfter = std::max(steadyAfter, step);
  }
  return transition / std::max(steadyBefore, steadyAfter);
}

TEST(ClipDspTest, TransitionKeepsDynamicsAndEqState) {
  ClipDspProgram program;
  program.gate.enabled = true;
  program.gate.thresholdDb = -60.0f;
  program.eq.enabled = true;
  program.eq.bands[0] = {true,  EqBandType::LowShelf, EqBandOrder::Second, 160.0f,
                         12.0f, 0.70710678f};
  program.compressor.enabled = true;
  program.compressor.thresholdDb = -6.0f;
  program.compressor.ratio = 4.0f;
  auto changed = program;
  changed.eq.bands[0].gainDb = 11.5f;

  ClipDspProcessor next;
  ASSERT_EQ(next.prepare(changed, 48000.0, 2), ClipDspValidationError::OK);

  ClipDspProcessor kept;
  ASSERT_EQ(kept.prepare(program, 48000.0, 2), ClipDspValidationError::OK);
  const double keptRatio = transitionStepRatio(kept, [&] { kept.transitionTo(next); });
  EXPECT_LE(keptRatio, 1.25);
  EXPECT_EQ(kept.program(), changed);

  // Replacing the processor outright resets every filter and envelope: the
  // metric must catch that click.
  ClipDspProcessor replaced;
  ASSERT_EQ(replaced.prepare(program, 48000.0, 2), ClipDspValidationError::OK);
  const double replacedRatio = transitionStepRatio(replaced, [&] { replaced = next; });
  EXPECT_GT(replacedRatio, 1.25);
  EXPECT_GT(replacedRatio, keptRatio);
}

TEST(ClipDspTest, WidthTransitionRampsThenSettlesExactly) {
  ClipDspProgram program;
  program.width.enabled = true;
  program.width.amount = 1.0f;
  auto collapsed = program;
  collapsed.width.amount = 0.0f;
  ClipDspProgram disabled;

  ClipDspProcessor processor;
  ClipDspProcessor toCollapsed;
  ClipDspProcessor toDisabled;
  ASSERT_EQ(processor.prepare(program, 48000.0, 2), ClipDspValidationError::OK);
  ASSERT_EQ(toCollapsed.prepare(collapsed, 48000.0, 2), ClipDspValidationError::OK);
  ASSERT_EQ(toDisabled.prepare(disabled, 48000.0, 2), ClipDspValidationError::OK);

  processor.transitionTo(toCollapsed);
  for (size_t n = 0; n < 480; ++n) {
    std::array<float, 2> frame{1.0f, -0.5f};
    processor.processFrame(frame.data(), frame.size());
    if (n == 0) {
      EXPECT_NE(frame[0], frame[1]);
      EXPECT_LT(std::abs(frame[0] - frame[1]), 1.5f);
    }
    if (n == 479) {
      EXPECT_EQ(frame[0], 0.25f);
      EXPECT_EQ(frame[1], 0.25f);
    }
  }

  processor.transitionTo(toDisabled);
  EXPECT_TRUE(processor.active());
  for (size_t n = 0; n < 960; ++n) {
    std::array<float, 2> frame{1.0f, -0.5f};
    processor.processFrame(frame.data(), frame.size());
    if (n >= 480) {
      EXPECT_EQ(frame[0], 1.0f);
      EXPECT_EQ(frame[1], -0.5f);
    }
  }
  EXPECT_FALSE(processor.active());
}

TEST(ClipDspTest, RepeatedTransitionKeepsPendingWidthRamp) {
  // Unrelated metadata updates re-send the same program; the width ramp must
  // still settle 480 frames after the first request.
  ClipDspProgram program;
  program.width.enabled = true;
  program.width.amount = 1.0f;
  auto collapsed = program;
  collapsed.width.amount = 0.0f;

  ClipDspProcessor once;
  ClipDspProcessor repeated;
  ClipDspProcessor next;
  ASSERT_EQ(once.prepare(program, 48000.0, 2), ClipDspValidationError::OK);
  ASSERT_EQ(repeated.prepare(program, 48000.0, 2), ClipDspValidationError::OK);
  ASSERT_EQ(next.prepare(collapsed, 48000.0, 2), ClipDspValidationError::OK);
  once.transitionTo(next);
  for (size_t n = 0; n < 512; ++n) {
    if (n % 128 == 0)
      repeated.transitionTo(next);
    std::array<float, 2> a{1.0f, -0.5f};
    std::array<float, 2> b{1.0f, -0.5f};
    once.processFrame(a.data(), a.size());
    repeated.processFrame(b.data(), b.size());
    EXPECT_EQ(b[0], a[0]) << "frame " << n;
    EXPECT_EQ(b[1], a[1]) << "frame " << n;
    if (n >= 479) {
      EXPECT_EQ(b[0], 0.25f) << "frame " << n;
      EXPECT_EQ(b[1], 0.25f) << "frame " << n;
    }
  }
}

TEST(ClipDspTest, ActiveCoversEnabledStagesAndPendingTransitions) {
  const auto preparedActive = [](const ClipDspProgram& program) {
    ClipDspProcessor processor;
    EXPECT_EQ(processor.prepare(program, 48000.0, 1), ClipDspValidationError::OK);
    return processor.active();
  };
  EXPECT_FALSE(preparedActive({}));

  ClipDspProgram gate;
  gate.gate.enabled = true;
  EXPECT_TRUE(preparedActive(gate));

  ClipDspProgram emptyEq;
  emptyEq.eq.enabled = true;
  EXPECT_FALSE(preparedActive(emptyEq));

  ClipDspProgram bypassedBand;
  bypassedBand.eq.bands[0].enabled = true;
  bypassedBand.eq.bands[0].gainDb = 6.0f;
  EXPECT_FALSE(preparedActive(bypassedBand));

  auto enabledBand = bypassedBand;
  enabledBand.eq.enabled = true;
  auto disabledBand = enabledBand;
  disabledBand.eq.bands[0].enabled = false;
  ClipDspProcessor processor;
  ClipDspProcessor next;
  ASSERT_EQ(processor.prepare(enabledBand, 48000.0, 1), ClipDspValidationError::OK);
  ASSERT_EQ(next.prepare(disabledBand, 48000.0, 1), ClipDspValidationError::OK);
  EXPECT_TRUE(processor.active());
  processor.transitionTo(next);
  for (size_t n = 0; n < 480; ++n) {
    EXPECT_TRUE(processor.active()) << "frame " << n;
    float sample = 0.5f;
    processor.processFrame(&sample, 1);
  }
  EXPECT_FALSE(processor.active());
}

TEST(ClipDspTest, ProcessorHasFixedTriviallyCopyableState) {
  static_assert(std::is_trivially_copyable_v<ClipDspProcessor>);
  EXPECT_LE(sizeof(ClipDspProcessor), 4096u);
}

} // namespace
} // namespace orpheus
