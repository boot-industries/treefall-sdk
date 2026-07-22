// SPDX-License-Identifier: MIT
#include <orpheus/live_audio.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>

#define ORPHEUS_TEST_DEFINE_RT_ALLOC_HOOKS
#include "support/rt_guard.hpp"

namespace orpheus {
namespace {

using tests::support::RtGuardState;
using tests::support::RtSection;

struct Fixture {
  std::unique_ptr<ILiveAudioFanout> fanout;
  LiveAudioStreamId fast{kInvalidLiveAudioStreamId};
  LiveAudioStreamId stalled{kInvalidLiveAudioStreamId};

  Fixture(uint16_t queue_blocks = 2) {
    LiveAudioFanoutConfig config;
    config.channel_count = 2;
    config.sample_rate = 48000;
    config.max_block_frames = 4;
    config.max_streams = 3;
    config.queue_blocks_per_stream = queue_blocks;
    auto created = createLiveAudioFanout(config);
    EXPECT_TRUE(created.isOk()) << created.errorMessage;
    fanout = std::move(created.value);
    auto first = fanout->addStream({true});
    auto second = fanout->addStream({true});
    EXPECT_TRUE(first.isOk());
    EXPECT_TRUE(second.isOk());
    fast = first.value;
    stalled = second.value;
  }
};

LiveAudioBlockView block(const float* const* channels, uint32_t frames,
                         uint64_t position, bool discontinuity = false) {
  return {channels, 2, frames, 48000, position, position * 1000, discontinuity};
}

TEST(LiveAudioFanoutTest, DeliversExactIndependentCompleteBlocksWithoutRealtimeAllocation) {
  Fixture fixture;
  const std::array<float, 4> left{1.0f, 2.0f, 3.0f, 4.0f};
  const std::array<float, 4> right{-1.0f, -2.0f, -3.0f, -4.0f};
  const float* channels[]{left.data(), right.data()};

  RtGuardState::reset();
  {
    RtSection realtime;
    fixture.fanout->publish(block(channels, 4, 23, true));
  }
  EXPECT_EQ(RtGuardState::totalViolations(), 0u);

  std::array<float, 8> destination{};
  LiveAudioBlockInfo info;
  ASSERT_TRUE(fixture.fanout->drain(fixture.fast, destination.data(), 4, info));
  EXPECT_EQ(destination, (std::array<float, 8>{1, -1, 2, -2, 3, -3, 4, -4}));
  EXPECT_EQ(info.channel_count, 2);
  EXPECT_EQ(info.frame_count, 4u);
  EXPECT_EQ(info.sample_rate, 48000u);
  EXPECT_EQ(info.sample_position, 23u);
  EXPECT_EQ(info.host_time_nanoseconds, 23000u);
  EXPECT_TRUE(info.discontinuity);

  destination.fill(0.0f);
  ASSERT_TRUE(fixture.fanout->drain(fixture.stalled, destination.data(), 4, info));
  EXPECT_EQ(destination, (std::array<float, 8>{1, -1, 2, -2, 3, -3, 4, -4}));
}

TEST(LiveAudioFanoutTest, StalledStreamDropsIncomingBlockWithoutOverwritingSibling) {
  Fixture fixture;
  const std::array<float, 2> left{1, 2};
  const std::array<float, 2> right{10, 20};
  const float* channels[]{left.data(), right.data()};
  std::array<float, 4> destination{};
  LiveAudioBlockInfo info;

  fixture.fanout->publish(block(channels, 2, 0));
  ASSERT_TRUE(fixture.fanout->drain(fixture.fast, destination.data(), 2, info));
  fixture.fanout->publish(block(channels, 2, 2));
  ASSERT_TRUE(fixture.fanout->drain(fixture.fast, destination.data(), 2, info));
  fixture.fanout->publish(block(channels, 2, 4));
  ASSERT_TRUE(fixture.fanout->drain(fixture.fast, destination.data(), 2, info));

  const auto fast = fixture.fanout->streamStatus(fixture.fast);
  const auto stalled = fixture.fanout->streamStatus(fixture.stalled);
  EXPECT_EQ(fast.accepted_blocks, 3u);
  EXPECT_EQ(fast.dropped_blocks, 0u);
  EXPECT_EQ(stalled.accepted_blocks, 2u);
  EXPECT_EQ(stalled.dropped_blocks, 1u);
  EXPECT_EQ(stalled.dropped_frames, 2u);
  EXPECT_EQ(stalled.queue_depth_blocks, 2u);
  EXPECT_EQ(stalled.queue_high_water_blocks, 2u);

  ASSERT_TRUE(fixture.fanout->drain(fixture.stalled, destination.data(), 2, info));
  EXPECT_EQ(info.sample_position, 0u);
  ASSERT_TRUE(fixture.fanout->drain(fixture.stalled, destination.data(), 2, info));
  EXPECT_EQ(info.sample_position, 2u);
  EXPECT_FALSE(fixture.fanout->drain(fixture.stalled, destination.data(), 2, info));
}

TEST(LiveAudioFanoutTest, RejectsInvalidViewsAndPreservesBlockOnBadDrain) {
  Fixture fixture;
  const std::array<float, 2> left{1, 2};
  const std::array<float, 2> right{3, 4};
  const float* channels[]{left.data(), right.data()};

  auto invalid = block(channels, 2, 0);
  invalid.sample_rate = 44100;
  fixture.fanout->publish(invalid);
  invalid.sample_rate = 48000;
  invalid.channels = nullptr;
  fixture.fanout->publish(invalid);
  EXPECT_EQ(fixture.fanout->streamStatus(fixture.fast).invalid_blocks, 2u);

  fixture.fanout->publish(block(channels, 2, 8));
  LiveAudioBlockInfo info;
  std::array<float, 4> destination{};
  EXPECT_FALSE(fixture.fanout->drain(fixture.fast, nullptr, 2, info));
  EXPECT_FALSE(fixture.fanout->drain(fixture.fast, destination.data(), 1, info));
  EXPECT_EQ(fixture.fanout->streamStatus(fixture.fast).queue_depth_blocks, 1u);
  EXPECT_EQ(fixture.fanout->streamStatus(fixture.fast).invalid_drains, 2u);
  ASSERT_TRUE(fixture.fanout->drain(fixture.fast, destination.data(), 2, info));
  EXPECT_EQ(info.sample_position, 8u);
}

TEST(LiveAudioFanoutTest, DisabledStreamCopiesNothingAndResetRequiresDisabledState) {
  Fixture fixture;
  auto disabled = fixture.fanout->addStream({false});
  ASSERT_TRUE(disabled.isOk());
  const std::array<float, 1> left{1};
  const std::array<float, 1> right{2};
  const float* channels[]{left.data(), right.data()};
  fixture.fanout->publish(block(channels, 1, 0));
  EXPECT_EQ(fixture.fanout->streamStatus(disabled.value).published_blocks, 0u);
  EXPECT_EQ(fixture.fanout->streamStatus(disabled.value).accepted_blocks, 0u);
  EXPECT_EQ(fixture.fanout->resetStream(fixture.fast), SessionGraphError::NotReady);
  EXPECT_EQ(fixture.fanout->setStreamEnabled(fixture.fast, false), SessionGraphError::OK);
  EXPECT_EQ(fixture.fanout->resetStream(fixture.fast), SessionGraphError::OK);
}

TEST(LiveAudioFanoutTest, DiscardKeepsLatestAndMarksItDiscontinuous) {
  Fixture fixture(4);
  const std::array<float, 1> left{1};
  const std::array<float, 1> right{2};
  const float* channels[]{left.data(), right.data()};
  fixture.fanout->publish(block(channels, 1, 0));
  fixture.fanout->publish(block(channels, 1, 1));
  fixture.fanout->publish(block(channels, 1, 2));
  EXPECT_EQ(fixture.fanout->discardToLatest(fixture.fast), 2u);

  std::array<float, 2> destination{};
  LiveAudioBlockInfo info;
  ASSERT_TRUE(fixture.fanout->drain(fixture.fast, destination.data(), 1, info));
  EXPECT_EQ(info.sample_position, 2u);
  EXPECT_TRUE(info.discontinuity);
  const auto status = fixture.fanout->streamStatus(fixture.fast);
  EXPECT_EQ(status.queue_depth_blocks, 0u);
  EXPECT_EQ(status.drained_blocks, 1u);
  EXPECT_GE(status.discontinuities, 1u);
}

TEST(LiveAudioFanoutTest, SeparateStemAndMasterFanoutsShareNoState) {
  Fixture stem;
  Fixture master;
  const std::array<float, 1> stem_left{1};
  const std::array<float, 1> stem_right{2};
  const std::array<float, 1> master_left{10};
  const std::array<float, 1> master_right{20};
  const float* stem_channels[]{stem_left.data(), stem_right.data()};
  const float* master_channels[]{master_left.data(), master_right.data()};
  stem.fanout->publish(block(stem_channels, 1, 11));
  master.fanout->publish(block(master_channels, 1, 99));

  std::array<float, 2> destination{};
  LiveAudioBlockInfo info;
  ASSERT_TRUE(stem.fanout->drain(stem.fast, destination.data(), 1, info));
  EXPECT_EQ(destination, (std::array<float, 2>{1, 2}));
  EXPECT_EQ(info.sample_position, 11u);
  ASSERT_TRUE(master.fanout->drain(master.fast, destination.data(), 1, info));
  EXPECT_EQ(destination, (std::array<float, 2>{10, 20}));
  EXPECT_EQ(info.sample_position, 99u);
}

TEST(LiveAudioFanoutTest, FactoryAndStreamIdsFailClosed) {
  LiveAudioFanoutConfig invalid;
  invalid.channel_count = 0;
  EXPECT_FALSE(createLiveAudioFanout(invalid).isOk());

  Fixture fixture;
  EXPECT_EQ(fixture.fanout->setStreamEnabled(kInvalidLiveAudioStreamId, true),
            SessionGraphError::InvalidHandle);
  LiveAudioBlockInfo info;
  float destination[2]{};
  EXPECT_FALSE(fixture.fanout->drain(kInvalidLiveAudioStreamId, destination, 1, info));
  EXPECT_EQ(fixture.fanout->resetStream(kInvalidLiveAudioStreamId),
            SessionGraphError::InvalidHandle);
}

} // namespace
} // namespace orpheus
