// SPDX-License-Identifier: MIT
#include <orpheus/polyphase_resampler.h>
#include <orpheus/streaming_sample_rate_converter.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#define ORPHEUS_TEST_DEFINE_RT_ALLOC_HOOKS
#include "support/rt_guard.hpp"

namespace orpheus {
namespace {

using tests::support::RtGuardState;
using tests::support::RtSection;
constexpr double kPi = 3.14159265358979323846;

std::unique_ptr<IStreamingSampleRateConverter> makeConverter(uint32_t input_rate,
                                                              uint32_t output_rate,
                                                              uint16_t channels = 1) {
  StreamingSampleRateConfig config;
  config.channel_count = channels;
  config.input_sample_rate = input_rate;
  config.output_sample_rate = output_rate;
  config.max_input_frames = 1024;
  config.max_output_frames = 2048;
  config.taps_per_phase = 32;
  auto result = createStreamingSampleRateConverter(config);
  EXPECT_TRUE(result.isOk()) << result.errorMessage;
  return std::move(result.value);
}

std::vector<float> makeSine(uint32_t rate, double frequency, uint32_t frames,
                            uint16_t channels = 1) {
  std::vector<float> samples(static_cast<size_t>(frames) * channels);
  for (uint32_t frame = 0; frame < frames; ++frame) {
    const float value = static_cast<float>(std::sin(2.0 * kPi * frequency * frame / rate));
    for (uint16_t channel = 0; channel < channels; ++channel) {
      samples[static_cast<size_t>(frame) * channels + channel] = value;
    }
  }
  return samples;
}

std::vector<float> runStreaming(IStreamingSampleRateConverter& converter,
                                const std::vector<float>& input, uint16_t channels,
                                const std::vector<uint32_t>& chunks) {
  std::vector<float> result;
  std::array<float, 4096> output{};
  uint32_t frame_offset = 0;
  size_t chunk_index = 0;
  while (frame_offset < input.size() / channels) {
    const uint32_t frames = std::min<uint32_t>(
        chunks[chunk_index++ % chunks.size()],
        static_cast<uint32_t>(input.size() / channels) - frame_offset);
    StreamingSampleRateResult processed;
    {
      RtSection realtime;
      processed = converter.process(input.data() + static_cast<size_t>(frame_offset) * channels,
                                    frames, output.data(),
                                    std::min<uint32_t>(2048, output.size() / channels));
    }
    EXPECT_EQ(processed.error, SessionGraphError::OK);
    EXPECT_EQ(processed.consumed_frames, frames);
    result.insert(result.end(), output.begin(),
                  output.begin() + static_cast<ptrdiff_t>(processed.produced_frames * channels));
    frame_offset += frames;
  }
  return result;
}

TEST(StreamingSampleRateConverterTest, IdentityCopiesExactlyAndAccountsWithoutAllocation) {
  auto converter = makeConverter(48000, 48000, 2);
  const std::array<float, 8> input{1, -1, 2, -2, 3, -3, 4, -4};
  std::array<float, 8> output{};
  RtGuardState::reset();
  StreamingSampleRateResult result;
  {
    RtSection realtime;
    result = converter->process(input.data(), 4, output.data(), 4);
  }
  EXPECT_EQ(result.error, SessionGraphError::OK);
  EXPECT_EQ(result.consumed_frames, 4u);
  EXPECT_EQ(result.produced_frames, 4u);
  EXPECT_EQ(output, input);
  EXPECT_EQ(RtGuardState::totalViolations(), 0u);
}

class RateCase : public testing::TestWithParam<std::pair<uint32_t, uint32_t>> {};

TEST_P(RateCase, MatchesReferenceLengthAndSineFrequencyAcrossArbitraryChunks) {
  const auto [input_rate, output_rate] = GetParam();
  const auto input = makeSine(input_rate, 1000.0, input_rate);
  auto converter = makeConverter(input_rate, output_rate);
  RtGuardState::reset();
  auto actual = runStreaming(*converter, input, 1, {1, 37, 511, 103, 997});
  EXPECT_EQ(RtGuardState::totalViolations(), 0u);

  PolyphaseResampler reference(input_rate, output_rate, 1, 32);
  std::vector<float> expected;
  std::vector<float> chunk;
  uint32_t offset = 0;
  while (offset < input_rate) {
    const uint32_t frames = std::min<uint32_t>(777, input_rate - offset);
    reference.process(input.data() + offset, frames, chunk);
    expected.insert(expected.end(), chunk.begin(), chunk.end());
    offset += frames;
  }
  EXPECT_EQ(actual.size(), expected.size());

  uint32_t zero_crossings = 0;
  for (size_t index = 1; index < actual.size(); ++index) {
    if (actual[index - 1] <= 0.0f && actual[index] > 0.0f) {
      ++zero_crossings;
    }
  }
  EXPECT_NEAR(static_cast<double>(zero_crossings), 1000.0, 2.0);
}

INSTANTIATE_TEST_SUITE_P(SupportedRatios, RateCase,
                         testing::Values(std::pair<uint32_t, uint32_t>{44100, 48000},
                                         std::pair<uint32_t, uint32_t>{48000, 44100}));

TEST(StreamingSampleRateConverterTest, ChunkBoundariesDoNotChangeOutput) {
  const auto input = makeSine(44100, 440.0, 12000, 2);
  auto one = makeConverter(44100, 48000, 2);
  auto many = makeConverter(44100, 48000, 2);
  const auto contiguous = runStreaming(*one, input, 2, {1024});
  const auto chunked = runStreaming(*many, input, 2, {3, 17, 251, 509});
  ASSERT_EQ(contiguous.size(), chunked.size());
  EXPECT_TRUE(std::equal(contiguous.begin(), contiguous.end(), chunked.begin()));
}

TEST(StreamingSampleRateConverterTest, PreservesSilenceDcImpulseAndChannelIsolation) {
  auto silence_converter = makeConverter(48000, 44100, 2);
  std::vector<float> silence(2048, 0.0f);
  const auto silence_output = runStreaming(*silence_converter, silence, 2, {128});
  EXPECT_TRUE(std::all_of(silence_output.begin(), silence_output.end(),
                          [](float value) { return value == 0.0f; }));

  auto dc_converter = makeConverter(44100, 48000, 2);
  std::vector<float> dc(4096, 0.0f);
  for (size_t frame = 0; frame < dc.size() / 2; ++frame) {
    dc[frame * 2] = 1.0f;
  }
  const auto dc_output = runStreaming(*dc_converter, dc, 2, {256});
  for (size_t frame = 100; frame + 100 < dc_output.size() / 2; ++frame) {
    EXPECT_NEAR(dc_output[frame * 2], 1.0f, 0.02f);
    EXPECT_FLOAT_EQ(dc_output[frame * 2 + 1], 0.0f);
  }

  auto impulse_converter = makeConverter(44100, 48000);
  std::vector<float> impulse(1024, 0.0f);
  impulse[200] = 1.0f;
  const auto impulse_output = runStreaming(*impulse_converter, impulse, 1, {1024});
  EXPECT_GT(*std::max_element(impulse_output.begin(), impulse_output.end()), 0.5f);
}

TEST(StreamingSampleRateConverterTest, RejectedCallsPreservePhaseAndHistory) {
  auto tested = makeConverter(44100, 48000);
  auto control = makeConverter(44100, 48000);
  const auto input = makeSine(44100, 220.0, 1024);
  std::array<float, 2048> tested_output{};
  std::array<float, 2048> control_output{};

  EXPECT_EQ(tested->process(nullptr, 256, tested_output.data(), 2048).error,
            SessionGraphError::InvalidParameter);
  EXPECT_EQ(tested->process(input.data(), 1024, tested_output.data(), 1).error,
            SessionGraphError::InvalidParameter);
  EXPECT_EQ(tested->process(input.data(), 1025, tested_output.data(), 2048).error,
            SessionGraphError::InvalidParameter);

  const auto a = tested->process(input.data(), 1024, tested_output.data(), 2048);
  const auto b = control->process(input.data(), 1024, control_output.data(), 2048);
  ASSERT_EQ(a.error, SessionGraphError::OK);
  ASSERT_EQ(a.produced_frames, b.produced_frames);
  EXPECT_TRUE(std::equal(tested_output.begin(),
                         tested_output.begin() + static_cast<ptrdiff_t>(a.produced_frames),
                         control_output.begin()));
}

TEST(StreamingSampleRateConverterTest, ResetStartsARepeatableNewEpoch) {
  auto converter = makeConverter(44100, 48000);
  const auto input = makeSine(44100, 330.0, 512);
  std::array<float, 1024> first{};
  std::array<float, 1024> second{};
  const auto a = converter->process(input.data(), 512, first.data(), 1024);
  converter->reset();
  const auto b = converter->process(input.data(), 512, second.data(), 1024);
  ASSERT_EQ(a.produced_frames, b.produced_frames);
  EXPECT_TRUE(std::equal(first.begin(), first.begin() + a.produced_frames, second.begin()));
}

TEST(StreamingSampleRateConverterTest, FactoryRejectsInvalidAndOverflowingConfigs) {
  StreamingSampleRateConfig config;
  config.channel_count = 0;
  EXPECT_FALSE(createStreamingSampleRateConverter(config).isOk());
  config.channel_count = 2;
  config.taps_per_phase = 7;
  EXPECT_FALSE(createStreamingSampleRateConverter(config).isOk());
  config.taps_per_phase = std::numeric_limits<uint32_t>::max();
  EXPECT_FALSE(createStreamingSampleRateConverter(config).isOk());
}

} // namespace
} // namespace orpheus
