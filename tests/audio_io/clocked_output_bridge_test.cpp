// SPDX-License-Identifier: MIT
#include <orpheus/clocked_output_bridge.h>
#include <orpheus/live_audio.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#define ORPHEUS_TEST_DEFINE_RT_ALLOC_HOOKS
#include "support/rt_guard.hpp"

namespace orpheus {
namespace {

using tests::support::RtGuardState;
using tests::support::RtSection;

struct Source {
  std::unique_ptr<ILiveAudioFanout> fanout;
  LiveAudioStreamId stream{kInvalidLiveAudioStreamId};
  std::vector<float> samples;
  const float* channel_pointer[1]{};
  uint32_t rate{48000};
  uint32_t max_frames{512};
  uint64_t position{0};

  Source(uint32_t source_rate, uint32_t maximum_frames, uint16_t queue_blocks = 32)
      : samples(maximum_frames, 0.25f), rate(source_rate), max_frames(maximum_frames) {
    LiveAudioFanoutConfig config;
    config.channel_count = 1;
    config.sample_rate = source_rate;
    config.max_block_frames = maximum_frames;
    config.max_streams = 1;
    config.queue_blocks_per_stream = queue_blocks;
    auto created = createLiveAudioFanout(config);
    EXPECT_TRUE(created.isOk());
    fanout = std::move(created.value);
    auto added = fanout->addStream({true});
    EXPECT_TRUE(added.isOk());
    stream = added.value;
    channel_pointer[0] = samples.data();
  }

  void publish(uint32_t frames, bool discontinuity = false) {
    fanout->publish({channel_pointer, 1, frames, rate, position, position * 1000, discontinuity});
    position += frames;
  }
};

std::unique_ptr<IClockedOutputBridge> makeBridge(Source& source, uint32_t destination_rate,
                                                  uint32_t fifo_capacity = 12000,
                                                  uint32_t target_fill = 4800,
                                                  uint32_t destination_max = 512,
                                                  double max_slew = 500.0) {
  ClockedOutputBridgeConfig config;
  config.channel_count = 1;
  config.source_sample_rate = source.rate;
  config.destination_sample_rate = destination_rate;
  config.source_max_block_frames = source.max_frames;
  config.destination_max_block_frames = destination_max;
  config.fifo_capacity_frames = fifo_capacity;
  config.target_fill_frames = target_fill;
  config.max_slew_ppm = max_slew;
  auto created = createClockedOutputBridge(*source.fanout, source.stream, config);
  EXPECT_TRUE(created.isOk()) << created.errorMessage;
  return std::move(created.value);
}

struct SimulationCase {
  uint32_t source_rate;
  uint32_t destination_rate;
  uint32_t source_quantum;
  uint32_t destination_quantum;
  int ppm;
};

class ClockSimulation : public testing::TestWithParam<SimulationCase> {};

TEST_P(ClockSimulation, ThirtySimulatedMinutesStayBoundedWithoutRealtimeAllocation) {
  const auto scenario = GetParam();
  Source source(scenario.source_rate, 512);
  auto bridge = makeBridge(source, scenario.destination_rate);
  std::array<float, 512> output{};
  float* outputs[]{output.data()};

  for (uint32_t fill = 0; fill < 12; ++fill) {
    source.publish(scenario.source_quantum);
    bridge->pumpSource();
  }

  RtGuardState::reset();
  double destination_residual = 0.0;
  for (uint32_t second = 0; second < 30u * 60u; ++second) {
    source.publish(scenario.source_quantum);
    {
      RtSection worker;
      bridge->pumpSource();
    }
    destination_residual += static_cast<double>(scenario.destination_quantum) *
                            (1.0 + static_cast<double>(scenario.ppm) * 1e-6);
    const uint32_t destination_frames = static_cast<uint32_t>(destination_residual);
    destination_residual -= destination_frames;
    {
      RtSection realtime;
      bridge->render(outputs, 1, destination_frames,
                     static_cast<uint64_t>(second) * scenario.destination_rate,
                     static_cast<uint64_t>(second) * 1000000000ull, false);
    }
    const auto status = bridge->status();
    ASSERT_LT(status.fifo_fill_frames, 12000u);
    const double nominal = static_cast<double>(scenario.source_rate) /
                           static_cast<double>(scenario.destination_rate);
    const double correction_ppm = (status.current_ratio / nominal - 1.0) * 1e6;
    ASSERT_LE(std::abs(correction_ppm), 500.001);
  }
  EXPECT_EQ(RtGuardState::totalViolations(), 0u);
  const auto status = bridge->status();
  EXPECT_GT(status.fifo_fill_frames, 0u);
  EXPECT_LT(status.fifo_fill_frames, 12000u);
  const auto expected_rendered = static_cast<uint64_t>(
      static_cast<double>(scenario.destination_quantum) * 30.0 * 60.0 *
      (1.0 + static_cast<double>(scenario.ppm) * 1e-6));
  EXPECT_EQ(status.rendered_frames, expected_rendered);
  EXPECT_EQ(status.silence_frames, 0u);
  EXPECT_EQ(status.underruns, 0u);
}

INSTANTIATE_TEST_SUITE_P(
    ClockRates, ClockSimulation,
    testing::Values(SimulationCase{48000, 48000, 48, 48, -100},
                    SimulationCase{48000, 48000, 48, 48, 0},
                    SimulationCase{48000, 48000, 48, 48, 100},
                    SimulationCase{44100, 48000, 441, 480, -100},
                    SimulationCase{44100, 48000, 441, 480, 0},
                    SimulationCase{44100, 48000, 441, 480, 100},
                    SimulationCase{48000, 44100, 480, 441, -100},
                    SimulationCase{48000, 44100, 480, 441, 0},
                    SimulationCase{48000, 44100, 480, 441, 100}));

TEST(ClockedOutputBridgeTest, UnderflowWritesExactZerosAndCountsMissingFrames) {
  Source source(48000, 16);
  auto bridge = makeBridge(source, 48000, 32, 8, 16);
  std::array<float, 8> output;
  output.fill(9.0f);
  float* outputs[]{output.data()};
  bridge->render(outputs, 1, 8, 0, 0, false);
  EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](float sample) { return sample == 0.0f; }));
  const auto status = bridge->status();
  EXPECT_EQ(status.rendered_frames, 8u);
  EXPECT_EQ(status.silence_frames, 8u);
  EXPECT_EQ(status.underruns, 1u);
}

TEST(ClockedOutputBridgeTest, OverflowDiscardsOldFramesTowardTargetAndResetsSourceEpoch) {
  Source source(48000, 8, 8);
  auto bridge = makeBridge(source, 48000, 16, 4, 8);
  for (uint32_t block = 0; block < 3; ++block) {
    std::fill(source.samples.begin(), source.samples.end(), static_cast<float>(block + 1));
    source.publish(8);
    bridge->pumpSource();
  }
  const auto status = bridge->status();
  EXPECT_EQ(status.overflows, 1u);
  EXPECT_GE(status.source_discontinuities, 1u);
  EXPECT_LE(status.fifo_fill_frames, 12u);
  EXPECT_GE(status.fifo_fill_frames, 8u);
}

TEST(ClockedOutputBridgeTest, SourceAndDestinationDiscontinuitiesResetOnlyTheirBridge) {
  Source first_source(48000, 16);
  Source second_source(48000, 16);
  auto first = makeBridge(first_source, 48000, 64, 16, 16);
  auto second = makeBridge(second_source, 48000, 64, 16, 16);
  first_source.publish(16, true);
  second_source.publish(16, false);
  first->pumpSource();
  second->pumpSource();
  EXPECT_EQ(first->status().source_discontinuities, 1u);
  EXPECT_EQ(second->status().source_discontinuities, 0u);

  std::array<float, 4> output{};
  float* outputs[]{output.data()};
  first->render(outputs, 1, 4, 0, 0, true);
  EXPECT_EQ(first->status().resets, 1u);
  EXPECT_EQ(second->status().resets, 0u);
}

TEST(ClockedOutputBridgeTest, InvalidRenderShapeSilencesProvidedOutputs) {
  Source source(48000, 16);
  auto bridge = makeBridge(source, 48000, 64, 16, 8);
  std::array<float, 9> output;
  output.fill(1.0f);
  float* outputs[]{output.data()};
  bridge->render(outputs, 1, 9, 0, 0, false);
  EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](float sample) { return sample == 0.0f; }));
  EXPECT_EQ(bridge->status().underruns, 1u);
}

TEST(ClockedOutputBridgeTest, ReconfigurationIsBoundedByPreparedCapacity) {
  Source source(48000, 16);
  auto bridge = makeBridge(source, 48000, 64, 16, 16);
  EXPECT_EQ(bridge->reconfigureDestination(44100, 8), SessionGraphError::OK);
  EXPECT_EQ(bridge->reconfigureDestination(0, 8), SessionGraphError::InvalidParameter);
  EXPECT_EQ(bridge->reconfigureDestination(48000, 17), SessionGraphError::InvalidParameter);
  EXPECT_EQ(bridge->status().resets, 1u);
}

TEST(ClockedOutputBridgeTest, FactoryRejectsInvalidSourceAndCapacity) {
  Source source(48000, 16);
  ClockedOutputBridgeConfig config;
  config.channel_count = 1;
  config.source_max_block_frames = 16;
  config.fifo_capacity_frames = 8;
  EXPECT_FALSE(createClockedOutputBridge(*source.fanout, source.stream, config).isOk());
  config.fifo_capacity_frames = 64;
  config.target_fill_frames = 16;
  EXPECT_FALSE(createClockedOutputBridge(*source.fanout, kInvalidLiveAudioStreamId, config).isOk());
}

} // namespace
} // namespace orpheus
