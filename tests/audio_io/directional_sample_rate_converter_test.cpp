// SPDX-License-Identifier: MIT
//
// FTR085: bounded directional sample-rate conversion contracts.

#define ORPHEUS_TEST_DEFINE_RT_ALLOC_HOOKS
#include "../support/rt_guard.hpp"
#include <orpheus/directional_sample_rate_converter.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <vector>

#include <gtest/gtest.h>

namespace {

using orpheus::SessionGraphError;
using orpheus::audio_utils::DirectionalSampleRateConverter;
using orpheus::audio_utils::DirectionalSrcConfig;
using orpheus::audio_utils::DirectionalSrcStatus;

constexpr double kPi = 3.141592653589793238462643383279502884;

DirectionalSrcConfig makeConfig(uint32_t input_rate, uint32_t output_rate,
                                bool allow_correction = false) {
  DirectionalSrcConfig config;
  config.input_rate = input_rate;
  config.output_rate = output_rate;
  config.channels = 1;
  config.max_input_frames = 4096;
  config.max_output_frames = 4096;
  config.fifo_capacity_frames = 32768;
  config.prime_input_frames = 0;
  config.allow_input_rate_correction = allow_correction;
  return config;
}

void pushPlanar(DirectionalSampleRateConverter& converter, const std::vector<float>& input) {
  constexpr uint32_t kChunk = 4096;
  for (size_t offset = 0; offset < input.size();) {
    const uint32_t count = static_cast<uint32_t>(std::min<size_t>(kChunk, input.size() - offset));
    const float* channels[] = {input.data() + offset};
    const auto transfer = converter.pushInput(channels, 1, count);
    ASSERT_EQ(transfer.status, DirectionalSrcStatus::Ok);
    ASSERT_EQ(transfer.input_frames_consumed, count);
    offset += count;
  }
}

std::vector<float> renderPlanar(DirectionalSampleRateConverter& converter, uint32_t frames,
                                uint32_t output_chunk) {
  std::vector<float> output(frames, 0.0F);
  for (uint32_t offset = 0; offset < frames;) {
    const uint32_t count = std::min(output_chunk, frames - offset);
    float* channels[] = {output.data() + offset};
    uint32_t required = 0;
    if (converter.requiredInputFrames(count, required) != DirectionalSrcStatus::Ok) {
      ADD_FAILURE() << "requiredInputFrames failed";
      return {};
    }
    const auto transfer = converter.renderOutput(channels, 1, count);
    if (transfer.status != DirectionalSrcStatus::Ok || transfer.output_frames_produced != count) {
      ADD_FAILURE() << "renderOutput failed";
      return {};
    }
    offset += count;
  }
  return output;
}

std::vector<float> sine(uint32_t rate, double frequency, uint32_t frames) {
  std::vector<float> result(frames);
  for (uint32_t frame = 0; frame < frames; ++frame) {
    result[frame] = static_cast<float>(std::sin(2.0 * kPi * frequency * frame / rate));
  }
  return result;
}

double rms(const std::vector<float>& signal, size_t begin) {
  if (begin >= signal.size()) {
    return 0.0;
  }
  double sum = 0.0;
  for (size_t index = begin; index < signal.size(); ++index) {
    sum += static_cast<double>(signal[index]) * signal[index];
  }
  return std::sqrt(sum / static_cast<double>(signal.size() - begin));
}

double measureFrequency(const std::vector<float>& signal, uint32_t rate, size_t begin) {
  uint32_t crossings = 0;
  for (size_t index = std::max<size_t>(begin, 1); index < signal.size(); ++index) {
    if (signal[index - 1] <= 0.0F && signal[index] > 0.0F) {
      ++crossings;
    }
  }
  const double duration = static_cast<double>(signal.size() - begin) / rate;
  return duration > 0.0 ? crossings / duration : 0.0;
}

std::vector<float> convertSine(uint32_t input_rate, uint32_t output_rate, double frequency,
                               uint32_t output_frames, uint32_t output_chunk = 4096) {
  DirectionalSampleRateConverter converter;
  if (converter.prepare(makeConfig(input_rate, output_rate)) != SessionGraphError::OK) {
    ADD_FAILURE() << "prepare failed";
    return {};
  }
  const uint64_t needed =
      (static_cast<uint64_t>(output_frames) * input_rate + output_rate - 1) / output_rate;
  auto input = sine(input_rate, frequency, static_cast<uint32_t>(needed + 256));
  pushPlanar(converter, input);
  if (!converter.isPrimed()) {
    ADD_FAILURE() << "converter did not prime";
    return {};
  }
  return renderPlanar(converter, output_frames, output_chunk);
}

} // namespace

TEST(DirectionalSampleRateConverterTest, RejectsUnsupportedPairsAndInvalidResources) {
  DirectionalSampleRateConverter converter;
  EXPECT_EQ(converter.prepare(makeConfig(16'000, 24'000)), SessionGraphError::NotSupported);
  EXPECT_EQ(converter.prepare(makeConfig(8'000, 48'000)), SessionGraphError::NotSupported);

  auto invalid = makeConfig(16'000, 48'000);
  invalid.channels = 0;
  EXPECT_EQ(converter.prepare(invalid), SessionGraphError::InvalidParameter);
  invalid = makeConfig(16'000, 48'000);
  invalid.fifo_capacity_frames = 2048;
  invalid.max_input_frames = 512;
  EXPECT_EQ(converter.prepare(invalid), SessionGraphError::InvalidParameter);
  invalid = makeConfig(16'000, 48'000);
  invalid.prime_input_frames = 16'385;
  EXPECT_EQ(converter.prepare(invalid), SessionGraphError::InvalidParameter);
}

TEST(DirectionalSampleRateConverterTest, PreservesSineFrequencyAndPassbandGain) {
  const auto upsampled_44k = convertSine(16'000, 44'100, 1'000.0, 40'000);
  const auto upsampled_48k = convertSine(16'000, 48'000, 1'000.0, 40'000);
  const auto downsampled = convertSine(48'000, 24'000, 1'000.0, 12'000);

  EXPECT_NEAR(measureFrequency(upsampled_44k, 44'100, 2'000), 1'000.0, 2.0);
  EXPECT_NEAR(measureFrequency(upsampled_48k, 48'000, 2'000), 1'000.0, 2.0);
  EXPECT_NEAR(measureFrequency(downsampled, 24'000, 1'000), 1'000.0, 2.0);
  EXPECT_NEAR(rms(upsampled_44k, 2'000), 1.0 / std::sqrt(2.0), 0.03);
  EXPECT_NEAR(rms(upsampled_48k, 2'000), 1.0 / std::sqrt(2.0), 0.03);
  EXPECT_NEAR(rms(downsampled, 1'000), 1.0 / std::sqrt(2.0), 0.03);
}

TEST(DirectionalSampleRateConverterTest, RejectsDownsamplingStopband) {
  DirectionalSampleRateConverter converter;
  ASSERT_EQ(converter.prepare(makeConfig(44'100, 16'000)), SessionGraphError::OK);
  auto input = sine(44'100, 10'000.0, 20'000);
  pushPlanar(converter, input);
  const auto output = renderPlanar(converter, 7'000, 2'000);
  EXPECT_LT(rms(output, 700), 0.08);
}

TEST(DirectionalSampleRateConverterTest, UsesTheExpectedImpulseOrder) {
  DirectionalSampleRateConverter converter;
  ASSERT_EQ(converter.prepare(makeConfig(44'100, 48'000)), SessionGraphError::OK);
  std::vector<float> input(2'000, 0.0F);
  input[0] = 1.0F;
  pushPlanar(converter, input);
  const auto output = renderPlanar(converter, 2'000, 1'024);
  const auto peak = std::max_element(output.begin(), output.end(), [](float lhs, float rhs) {
    return std::abs(lhs) < std::abs(rhs);
  });
  ASSERT_NE(peak, output.end());
  EXPECT_LT(static_cast<size_t>(std::distance(output.begin(), peak)), 32u);
  EXPECT_GT(std::abs(*peak), 0.5F);
}

TEST(DirectionalSampleRateConverterTest, MatchesLongRunRationalFrameConsumption) {
  DirectionalSampleRateConverter converter;
  ASSERT_EQ(converter.prepare(makeConfig(16'000, 44'100)), SessionGraphError::OK);
  std::vector<float> input(16'256, 0.0F);
  pushPlanar(converter, input);

  uint64_t consumed = 0;
  constexpr uint32_t kOutputFrames = 44'000;
  for (uint32_t remaining = kOutputFrames; remaining > 0;) {
    const uint32_t count = std::min<uint32_t>(remaining, 4096);
    float* output[] = {input.data()};
    const auto transfer = converter.renderOutput(output, 1, count);
    ASSERT_EQ(transfer.status, DirectionalSrcStatus::Ok);
    consumed += transfer.input_frames_consumed;
    remaining -= count;
  }
  const uint64_t expected = (static_cast<uint64_t>(kOutputFrames) * 16'000) / 44'100;
  EXPECT_EQ(consumed, expected);
}

TEST(DirectionalSampleRateConverterTest, CorrectionChangesConsumptionOnlyWhenEnabled) {
  auto run = [](bool enabled, int32_t ppm) {
    DirectionalSampleRateConverter converter;
    auto config = makeConfig(16'000, 44'100, enabled);
    EXPECT_EQ(converter.prepare(config), SessionGraphError::OK);
    if (enabled) {
      EXPECT_TRUE(converter.setInputRateCorrectionPpm(ppm));
    } else {
      EXPECT_FALSE(converter.setInputRateCorrectionPpm(ppm));
    }
    std::vector<float> input(8'000, 0.0F);
    pushPlanar(converter, input);
    uint64_t consumed = 0;
    for (uint32_t remaining = 20'000; remaining > 0;) {
      const uint32_t count = std::min<uint32_t>(remaining, 4096);
      float* output[] = {input.data()};
      const auto transfer = converter.renderOutput(output, 1, count);
      EXPECT_EQ(transfer.status, DirectionalSrcStatus::Ok);
      consumed += transfer.input_frames_consumed;
      remaining -= count;
    }
    return consumed;
  };

  const auto nominal = run(false, 1'000);
  const auto corrected = run(true, 1'000);
  EXPECT_NE(nominal, corrected);
}

TEST(DirectionalSampleRateConverterTest, ChunkingAndResetAreDeterministic) {
  auto config = makeConfig(16'000, 48'000);
  DirectionalSampleRateConverter first;
  DirectionalSampleRateConverter second;
  ASSERT_EQ(first.prepare(config), SessionGraphError::OK);
  ASSERT_EQ(second.prepare(config), SessionGraphError::OK);
  const auto input = sine(16'000, 440.0, 5'000);

  pushPlanar(first, input);
  for (size_t offset = 0; offset < input.size();) {
    const uint32_t count = static_cast<uint32_t>(std::min<size_t>(113, input.size() - offset));
    const float* channels[] = {input.data() + offset};
    ASSERT_EQ(second.pushInput(channels, 1, count).status, DirectionalSrcStatus::Ok);
    offset += count;
  }
  const auto first_output = renderPlanar(first, 12'000, 4096);
  const auto second_output = renderPlanar(second, 12'000, 257);
  ASSERT_EQ(first_output.size(), second_output.size());
  EXPECT_EQ(first_output, second_output);

  first.reset();
  EXPECT_FALSE(first.isPrimed());
  EXPECT_EQ(first.bufferedInputFrames(), 0u);
  EXPECT_EQ(first.latencyOutputFrames(), 381u);
}

TEST(DirectionalSampleRateConverterTest, EnforcesStartupUnderflowOverflowAndBufferOwnership) {
  DirectionalSampleRateConverter converter;
  uint32_t unused_required = 0;
  EXPECT_EQ(converter.requiredInputFrames(1, unused_required), DirectionalSrcStatus::NotPrepared);
  const auto config = makeConfig(16'000, 48'000);
  ASSERT_EQ(converter.prepare(config), SessionGraphError::OK);

  std::vector<float> sentinel(64, 7.0F);
  float* output[] = {sentinel.data()};
  EXPECT_EQ(converter.renderOutput(output, 1, 1).status, DirectionalSrcStatus::InputUnderflow);
  EXPECT_TRUE(
      std::all_of(sentinel.begin(), sentinel.end(), [](float value) { return value == 7.0F; }));

  std::vector<float> input(128, 0.0F);
  pushPlanar(converter, input);
  EXPECT_TRUE(converter.isPrimed());
  EXPECT_EQ(converter.renderOutput(output, 1, 1).status, DirectionalSrcStatus::Ok);

  DirectionalSampleRateConverter bounded;
  auto small = makeConfig(16'000, 48'000);
  small.max_input_frames = 128;
  small.max_output_frames = 128;
  small.fifo_capacity_frames = 1024;
  ASSERT_EQ(bounded.prepare(small), SessionGraphError::OK);
  std::vector<float> block(128, 0.0F);
  const float* block_channels[] = {block.data()};
  for (int index = 0; index < 8; ++index) {
    EXPECT_EQ(bounded.pushInput(block_channels, 1, 128).status, DirectionalSrcStatus::Ok);
  }
  EXPECT_EQ(bounded.pushInput(block_channels, 1, 1).status, DirectionalSrcStatus::InputOverflow);
}

TEST(DirectionalSampleRateConverterTest, ReportsLatencyFromConfiguredPrime) {
  auto config = makeConfig(16'000, 48'000);
  config.prime_input_frames = 64;
  DirectionalSampleRateConverter converter;
  ASSERT_EQ(converter.prepare(config), SessionGraphError::OK);
  EXPECT_EQ(converter.latencyOutputFrames(), 573u);
}

TEST(DirectionalSampleRateConverterTest, PreparedTransfersAreRealtimeAllocationFree) {
  DirectionalSampleRateConverter converter;
  const auto config = makeConfig(16'000, 48'000);
  ASSERT_EQ(converter.prepare(config), SessionGraphError::OK);
  std::vector<float> input(1024, 0.125F);
  std::vector<float> output(1024, 0.0F);
  const float* input_channels[] = {input.data()};
  float* output_channels[] = {output.data()};

  bool transfers_ok = true;
  orpheus::tests::support::RtGuardState::reset();
  {
    orpheus::tests::support::RtSection realtime;
    for (int iteration = 0; iteration < 8; ++iteration) {
      const auto pushed = converter.pushInput(input_channels, 1, 1024);
      uint32_t required = 0;
      const auto required_status = converter.requiredInputFrames(1024, required);
      const auto rendered = converter.renderOutput(output_channels, 1, 1024);
      transfers_ok = transfers_ok && pushed.status == DirectionalSrcStatus::Ok &&
                     required_status == DirectionalSrcStatus::Ok &&
                     rendered.status == DirectionalSrcStatus::Ok &&
                     rendered.output_frames_produced == 1024;
    }
  }

  EXPECT_TRUE(transfers_ok);
  EXPECT_EQ(orpheus::tests::support::RtGuardState::allocViolations(), 0u);
  EXPECT_EQ(orpheus::tests::support::RtGuardState::deallocViolations(), 0u);
}
