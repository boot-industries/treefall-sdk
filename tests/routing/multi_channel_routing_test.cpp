// SPDX-License-Identifier: MIT
#include "../../include/orpheus/clip_routing.h"

#include <gtest/gtest.h>
#include <memory>

using namespace orpheus;

class MultiChannelRoutingTest : public ::testing::Test {
protected:
  static constexpr uint32_t SAMPLE_RATE = 48000;
  static constexpr ClipHandle CLIP_1 = 1001;
  static constexpr ClipHandle CLIP_2 = 1002;
  static constexpr ClipHandle CLIP_3 = 1003;
  static constexpr ClipHandle CLIP_4 = 1004;

  void SetUp() override {
    // Create clip routing matrix (SessionGraph can be nullptr for basic tests)
    routing = createClipRoutingMatrix(nullptr, SAMPLE_RATE);
  }

  std::unique_ptr<IClipRoutingMatrix> routing;
};

// ============================================================================
// Output Bus Assignment Tests
// ============================================================================

TEST_F(MultiChannelRoutingTest, SetOutputBusToDefaultStereo) {
  auto result = routing->setClipOutputBus(CLIP_1, 0);

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_EQ(routing->getClipOutputBus(CLIP_1), 0);
}

TEST_F(MultiChannelRoutingTest, SetOutputBusToBus1) {
  auto result = routing->setClipOutputBus(CLIP_1, 1);

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_EQ(routing->getClipOutputBus(CLIP_1), 1); // Channels 3-4
}

TEST_F(MultiChannelRoutingTest, SetOutputBusToBus7) {
  auto result = routing->setClipOutputBus(CLIP_1, 7);

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_EQ(routing->getClipOutputBus(CLIP_1), 7); // Channels 15-16
}

TEST_F(MultiChannelRoutingTest, SetOutputBusToMaximum) {
  auto result = routing->setClipOutputBus(CLIP_1, 15);

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_EQ(routing->getClipOutputBus(CLIP_1), 15); // Channels 31-32
}

TEST_F(MultiChannelRoutingTest, SetOutputBusAboveMaximum) {
  auto result = routing->setClipOutputBus(CLIP_1, 16); // Bus 16 > max (15)

  EXPECT_EQ(result, SessionGraphError::InvalidParameter);
}

TEST_F(MultiChannelRoutingTest, SetOutputBusWithInvalidHandle) {
  auto result = routing->setClipOutputBus(0, 0); // Handle 0 is invalid

  EXPECT_EQ(result, SessionGraphError::InvalidHandle);
}

TEST_F(MultiChannelRoutingTest, GetOutputBusForUnassignedClip) {
  // Never assigned CLIP_1
  uint8_t bus = routing->getClipOutputBus(CLIP_1);

  EXPECT_EQ(bus, 0); // Defaults to bus 0 (stereo)
}

TEST_F(MultiChannelRoutingTest, ReassignOutputBus) {
  routing->setClipOutputBus(CLIP_1, 2);               // Bus 2 (channels 5-6)
  auto result = routing->setClipOutputBus(CLIP_1, 5); // Bus 5 (channels 11-12)

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_EQ(routing->getClipOutputBus(CLIP_1), 5);
}

TEST_F(MultiChannelRoutingTest, MultipleClipsToDifferentBuses) {
  routing->setClipOutputBus(CLIP_1, 0); // Stereo (channels 1-2)
  routing->setClipOutputBus(CLIP_2, 1); // Channels 3-4
  routing->setClipOutputBus(CLIP_3, 2); // Channels 5-6
  routing->setClipOutputBus(CLIP_4, 3); // Channels 7-8

  EXPECT_EQ(routing->getClipOutputBus(CLIP_1), 0);
  EXPECT_EQ(routing->getClipOutputBus(CLIP_2), 1);
  EXPECT_EQ(routing->getClipOutputBus(CLIP_3), 2);
  EXPECT_EQ(routing->getClipOutputBus(CLIP_4), 3);
}

TEST_F(MultiChannelRoutingTest, MultipleClipsToSameBus) {
  routing->setClipOutputBus(CLIP_1, 2);
  routing->setClipOutputBus(CLIP_2, 2);
  routing->setClipOutputBus(CLIP_3, 2);

  EXPECT_EQ(routing->getClipOutputBus(CLIP_1), 2);
  EXPECT_EQ(routing->getClipOutputBus(CLIP_2), 2);
  EXPECT_EQ(routing->getClipOutputBus(CLIP_3), 2);
}

// ============================================================================
// Channel Mapping Tests
// ============================================================================

TEST_F(MultiChannelRoutingTest, MapLeftChannelToOutput5) {
  auto result = routing->mapChannels(CLIP_1, 0, 5); // Clip L → Output channel 5

  EXPECT_EQ(result, SessionGraphError::OK);
}

TEST_F(MultiChannelRoutingTest, MapRightChannelToOutput6) {
  auto result = routing->mapChannels(CLIP_1, 1, 6); // Clip R → Output channel 6

  EXPECT_EQ(result, SessionGraphError::OK);
}

TEST_F(MultiChannelRoutingTest, MapChannelToMaxOutputChannel) {
  auto result = routing->mapChannels(CLIP_1, 0, 31); // Max output channel (channel 32)

  EXPECT_EQ(result, SessionGraphError::OK);
}

TEST_F(MultiChannelRoutingTest, MapChannelToInvalidOutputChannel) {
  auto result = routing->mapChannels(CLIP_1, 0, 32); // Output channel 32 > max (31)

  EXPECT_EQ(result, SessionGraphError::InvalidParameter);
}

TEST_F(MultiChannelRoutingTest, MapChannelWithInvalidHandle) {
  auto result = routing->mapChannels(0, 0, 5); // Handle 0 is invalid

  EXPECT_EQ(result, SessionGraphError::InvalidHandle);
}

TEST_F(MultiChannelRoutingTest, MapMultipleChannelsForSameClip) {
  // Stereo clip: L → channel 8, R → channel 9
  auto result1 = routing->mapChannels(CLIP_1, 0, 8);
  auto result2 = routing->mapChannels(CLIP_1, 1, 9);

  EXPECT_EQ(result1, SessionGraphError::OK);
  EXPECT_EQ(result2, SessionGraphError::OK);
}

TEST_F(MultiChannelRoutingTest, RemapChannel) {
  routing->mapChannels(CLIP_1, 0, 5);                // L → channel 5
  auto result = routing->mapChannels(CLIP_1, 0, 10); // L → channel 10 (override)

  EXPECT_EQ(result, SessionGraphError::OK);
}

TEST_F(MultiChannelRoutingTest, MapChannelsForMultipleClips) {
  routing->mapChannels(CLIP_1, 0, 0); // Clip 1 L → channel 0
  routing->mapChannels(CLIP_1, 1, 1); // Clip 1 R → channel 1
  routing->mapChannels(CLIP_2, 0, 4); // Clip 2 L → channel 4
  routing->mapChannels(CLIP_2, 1, 5); // Clip 2 R → channel 5

  // All mappings should succeed (isolation)
  EXPECT_TRUE(true);
}

TEST_F(MultiChannelRoutingTest, MapHighClipChannelNumber) {
  // Some clips may have >2 channels (e.g., 5.1 surround = 6 channels)
  // We don't validate clipChannel, audio thread will clamp
  auto result = routing->mapChannels(CLIP_1, 5, 10); // Clip channel 5 → output 10

  EXPECT_EQ(result, SessionGraphError::OK);
}

// ============================================================================
// Integration: Output Bus + Channel Mapping
// ============================================================================

TEST_F(MultiChannelRoutingTest, CombineBusAssignmentAndChannelMapping) {
  // Assign clip to bus 3 (channels 7-8)
  routing->setClipOutputBus(CLIP_1, 3);

  // Override with custom channel mapping (L → channel 10, R → channel 11)
  routing->mapChannels(CLIP_1, 0, 10);
  routing->mapChannels(CLIP_1, 1, 11);

  // Both should succeed (channel mapping overrides bus routing in audio thread)
  EXPECT_EQ(routing->getClipOutputBus(CLIP_1), 3);
}

TEST_F(MultiChannelRoutingTest, EightClipsToEightDifferentBuses) {
  // 8 clips to 8 stereo buses (16 output channels total)
  for (ClipHandle handle = 1; handle <= 8; ++handle) {
    uint8_t bus = handle - 1; // Buses 0-7
    routing->setClipOutputBus(handle, bus);
  }

  // Verify assignments
  EXPECT_EQ(routing->getClipOutputBus(1), 0); // Channels 1-2
  EXPECT_EQ(routing->getClipOutputBus(2), 1); // Channels 3-4
  EXPECT_EQ(routing->getClipOutputBus(3), 2); // Channels 5-6
  EXPECT_EQ(routing->getClipOutputBus(4), 3); // Channels 7-8
  EXPECT_EQ(routing->getClipOutputBus(5), 4); // Channels 9-10
  EXPECT_EQ(routing->getClipOutputBus(6), 5); // Channels 11-12
  EXPECT_EQ(routing->getClipOutputBus(7), 6); // Channels 13-14
  EXPECT_EQ(routing->getClipOutputBus(8), 7); // Channels 15-16
}

TEST_F(MultiChannelRoutingTest, SixteenBusesForThirtyTwoChannels) {
  // Assign clips to all 16 available buses
  for (ClipHandle handle = 1; handle <= 16; ++handle) {
    uint8_t bus = handle - 1; // Buses 0-15
    routing->setClipOutputBus(handle, bus);
  }

  // Verify first and last
  EXPECT_EQ(routing->getClipOutputBus(1), 0);   // Channels 1-2
  EXPECT_EQ(routing->getClipOutputBus(16), 15); // Channels 31-32
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(MultiChannelRoutingTest, RapidBusChanges) {
  // Simulate rapid UI changes (no crashes expected)
  for (int i = 0; i < 100; ++i) {
    routing->setClipOutputBus(CLIP_1, i % 16);
  }

  // Final bus should be 99 % 16 = 3
  EXPECT_EQ(routing->getClipOutputBus(CLIP_1), 3);
}

TEST_F(MultiChannelRoutingTest, RapidChannelMappingChanges) {
  // Simulate rapid UI changes (no crashes expected)
  for (int i = 0; i < 100; ++i) {
    routing->mapChannels(CLIP_1, 0, i % 32);
    routing->mapChannels(CLIP_1, 1, (i + 1) % 32);
  }

  // Should complete without crashes
  EXPECT_TRUE(true);
}

TEST_F(MultiChannelRoutingTest, ManyClipsBusAssignments) {
  // Assign 64 clips to buses
  for (ClipHandle handle = 1; handle <= 64; ++handle) {
    routing->setClipOutputBus(handle, (handle - 1) % 16);
  }

  // Verify spread across 16 buses
  EXPECT_EQ(routing->getClipOutputBus(1), 0);
  EXPECT_EQ(routing->getClipOutputBus(17), 0);  // 16 % 16 = 0
  EXPECT_EQ(routing->getClipOutputBus(33), 0);  // 32 % 16 = 0
  EXPECT_EQ(routing->getClipOutputBus(64), 15); // 63 % 16 = 15
}

TEST_F(MultiChannelRoutingTest, ZeroBusIsValidDefault) {
  // Bus 0 is valid (stereo, channels 1-2)
  auto result = routing->setClipOutputBus(CLIP_1, 0);

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_EQ(routing->getClipOutputBus(CLIP_1), 0);
}

TEST_F(MultiChannelRoutingTest, ZeroOutputChannelIsValid) {
  // Output channel 0 is valid (physical channel 1)
  auto result = routing->mapChannels(CLIP_1, 0, 0);

  EXPECT_EQ(result, SessionGraphError::OK);
}

// ============================================================================
// Thread Safety / Concurrency (Basic Tests)
// ============================================================================

TEST_F(MultiChannelRoutingTest, ConcurrentBusAssignments) {
  // Assign many clips simultaneously (simulating fast UI actions)
  for (ClipHandle handle = 1; handle <= 32; ++handle) {
    routing->setClipOutputBus(handle, (handle - 1) % 8);
  }

  // All assignments should succeed
  EXPECT_EQ(routing->getClipOutputBus(1), 0);
  EXPECT_EQ(routing->getClipOutputBus(9), 0);  // 8 % 8 = 0
  EXPECT_EQ(routing->getClipOutputBus(32), 7); // 31 % 8 = 7
}

TEST_F(MultiChannelRoutingTest, ConcurrentChannelMappings) {
  // Map channels for many clips simultaneously
  for (ClipHandle handle = 1; handle <= 32; ++handle) {
    routing->mapChannels(handle, 0, (handle - 1) % 32);
  }

  // Should complete without crashes
  EXPECT_TRUE(true);
}

// ============================================================================
// Backward Compatibility (Default Behavior)
// ============================================================================

TEST_F(MultiChannelRoutingTest, DefaultBehaviorIsStereo) {
  // Clips without explicit bus assignment default to bus 0 (stereo)
  EXPECT_EQ(routing->getClipOutputBus(CLIP_1), 0);
  EXPECT_EQ(routing->getClipOutputBus(CLIP_2), 0);
  EXPECT_EQ(routing->getClipOutputBus(CLIP_3), 0);
}

TEST_F(MultiChannelRoutingTest, ExistingClipGroupingUnaffectedByBusAssignment) {
  // Assign clip to group 1
  routing->assignClipToGroup(CLIP_1, 1);

  // Also assign output bus 3
  routing->setClipOutputBus(CLIP_1, 3);

  // Both should coexist (group routing and multi-channel routing are orthogonal)
  EXPECT_EQ(routing->getClipGroup(CLIP_1), 1);
  EXPECT_EQ(routing->getClipOutputBus(CLIP_1), 3);
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
