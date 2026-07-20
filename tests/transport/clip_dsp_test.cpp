// SPDX-License-Identifier: MIT
#define ORPHEUS_TEST_DEFINE_RT_ALLOC_HOOKS
#include "../support/rt_guard.hpp"
#include <orpheus/clip_dsp.h>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <type_traits>

namespace orpheus {
namespace {

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
  program.eq[0].enabled = true;
  program.eq[0].type = ClipEqBandType::Bell;
  program.eq[0].frequencyHz = 1000.0f;
  program.eq[0].gainDb = 6.0f;
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
  for (auto& band : program.eq) {
    band.enabled = true;
    band.gainDb = 1.0f;
  }
  program.compressor.enabled = true;
  program.width.enabled = true;
  program.limiter.enabled = true;

  ClipDspProcessor processor;
  ASSERT_EQ(processor.prepare(program, 48000.0, 2), ClipDspValidationError::OK);
  orpheus::tests::support::RtGuardState::reset();
  {
    orpheus::tests::support::RtSection realtime;
    std::array<float, 2> frame{0.25f, -0.125f};
    for (size_t index = 0; index < 48000; ++index)
      processor.processFrame(frame.data(), frame.size());
  }
  EXPECT_EQ(orpheus::tests::support::RtGuardState::totalViolations(), 0u);
}

TEST(ClipDspTest, ProcessorHasFixedTriviallyCopyableState) {
  static_assert(std::is_trivially_copyable_v<ClipDspProcessor>);
  EXPECT_LE(sizeof(ClipDspProcessor), 4096u);
}

} // namespace
} // namespace orpheus
