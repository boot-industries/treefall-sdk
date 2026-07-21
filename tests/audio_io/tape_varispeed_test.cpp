// SPDX-License-Identifier: MIT

#include <orpheus/tape_varispeed.h>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <vector>

namespace orpheus {
namespace {

TapeVarispeedConfig config(uint16_t channels = 1) {
  TapeVarispeedConfig value;
  value.sample_rate = 48000;
  value.channels = channels;
  value.max_input_frames = 1024;
  value.max_output_frames = 2048;
  return value;
}

TEST(TapeVarispeedTest, RejectsInvalidConfigurationWithoutInstallingIt) {
  PreparedTapeVarispeed processor;
  EXPECT_EQ(processor.prepare(config()), SessionGraphError::OK);
  const auto before = processor.applied_rate();
  auto invalid = config();
  invalid.min_rate = 2.0;
  invalid.max_rate = 0.5;
  EXPECT_EQ(processor.prepare(invalid), SessionGraphError::InvalidParameter);
  EXPECT_DOUBLE_EQ(processor.applied_rate(), before);
}

TEST(TapeVarispeedTest, UnityIsExactBypass) {
  PreparedTapeVarispeed processor;
  ASSERT_EQ(processor.prepare(config(2)), SessionGraphError::OK);
  const std::array<float, 12> input = {0.25f,  -0.5f,   1.0f, 0.0f, -0.75f, 0.5f,
                                       0.125f, -0.125f, 0.0f, 1.0f, 0.5f,   -1.0f};
  std::array<float, input.size()> output{};
  const auto result =
      processor.process(input.data(), input.size() / 2, output.data(), output.size() / 2, 1.0);
  EXPECT_EQ(result.error, SessionGraphError::OK);
  EXPECT_EQ(result.input_frames_consumed, input.size() / 2);
  EXPECT_EQ(result.output_frames_produced, input.size() / 2);
  EXPECT_EQ(output, input);
}

TEST(TapeVarispeedTest, StereoStaysLinkedAndDoesNotCrossfeed) {
  PreparedTapeVarispeed processor;
  ASSERT_EQ(processor.prepare(config(2)), SessionGraphError::OK);
  std::vector<float> input(1024 * 2, 0.0f);
  for (size_t frame = 0; frame < 1024; ++frame) {
    input[frame * 2] = std::sin(static_cast<float>(frame) * 0.05f);
  }
  std::vector<float> output(2048 * 2, 0.0f);
  const auto result = processor.process(input.data(), 1024, output.data(), 2048, 0.5);
  ASSERT_EQ(result.error, SessionGraphError::OK);
  ASSERT_GT(result.output_frames_produced, 0u);
  for (size_t frame = 0; frame < result.output_frames_produced; ++frame) {
    EXPECT_FLOAT_EQ(output[frame * 2 + 1], 0.0f);
  }
}

TEST(TapeVarispeedTest, InvalidProcessInputDoesNotMutateRateOrPhase) {
  PreparedTapeVarispeed processor;
  ASSERT_EQ(processor.prepare(config()), SessionGraphError::OK);
  std::array<float, 128> input{};
  std::array<float, 256> output{};
  EXPECT_EQ(processor.process(input.data(), input.size(), output.data(), output.size(), NAN).error,
            SessionGraphError::InvalidParameter);
  EXPECT_DOUBLE_EQ(processor.applied_rate(), 1.0);
  EXPECT_EQ(processor.process(input.data(), input.size(), output.data(), output.size(), 1.0).error,
            SessionGraphError::OK);
}

} // namespace
} // namespace orpheus
