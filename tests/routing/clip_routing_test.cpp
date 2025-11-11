// SPDX-License-Identifier: MIT
#include "../../include/orpheus/clip_routing.h"

#include <gtest/gtest.h>
#include <memory>

using namespace orpheus;

class ClipRoutingMatrixTest : public ::testing::Test {
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
// Clip Assignment Tests
// ============================================================================

TEST_F(ClipRoutingMatrixTest, AssignClipToValidGroup) {
  auto result = routing->assignClipToGroup(CLIP_1, 0);

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_EQ(routing->getClipGroup(CLIP_1), 0);
}

TEST_F(ClipRoutingMatrixTest, AssignClipToGroup1) {
  auto result = routing->assignClipToGroup(CLIP_1, 1);

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_EQ(routing->getClipGroup(CLIP_1), 1);
}

TEST_F(ClipRoutingMatrixTest, AssignClipToGroup2) {
  auto result = routing->assignClipToGroup(CLIP_1, 2);

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_EQ(routing->getClipGroup(CLIP_1), 2);
}

TEST_F(ClipRoutingMatrixTest, AssignClipToGroup3) {
  auto result = routing->assignClipToGroup(CLIP_1, 3);

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_EQ(routing->getClipGroup(CLIP_1), 3);
}

TEST_F(ClipRoutingMatrixTest, AssignClipToUnassignedGroup) {
  routing->assignClipToGroup(CLIP_1, 0);
  auto result = routing->assignClipToGroup(CLIP_1, 255); // UNASSIGNED_GROUP

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_EQ(routing->getClipGroup(CLIP_1), 255);
}

TEST_F(ClipRoutingMatrixTest, AssignClipToInvalidGroup) {
  auto result = routing->assignClipToGroup(CLIP_1, 4); // Only 0-3 are valid

  EXPECT_EQ(result, SessionGraphError::InvalidParameter);
}

TEST_F(ClipRoutingMatrixTest, ReassignClipToDifferentGroup) {
  routing->assignClipToGroup(CLIP_1, 0);
  auto result = routing->assignClipToGroup(CLIP_1, 1);

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_EQ(routing->getClipGroup(CLIP_1), 1);
}

TEST_F(ClipRoutingMatrixTest, AssignMultipleClipsToSameGroup) {
  routing->assignClipToGroup(CLIP_1, 0);
  routing->assignClipToGroup(CLIP_2, 0);
  routing->assignClipToGroup(CLIP_3, 0);

  EXPECT_EQ(routing->getClipGroup(CLIP_1), 0);
  EXPECT_EQ(routing->getClipGroup(CLIP_2), 0);
  EXPECT_EQ(routing->getClipGroup(CLIP_3), 0);
}

TEST_F(ClipRoutingMatrixTest, AssignMultipleClipsToDifferentGroups) {
  routing->assignClipToGroup(CLIP_1, 0);
  routing->assignClipToGroup(CLIP_2, 1);
  routing->assignClipToGroup(CLIP_3, 2);
  routing->assignClipToGroup(CLIP_4, 3);

  EXPECT_EQ(routing->getClipGroup(CLIP_1), 0);
  EXPECT_EQ(routing->getClipGroup(CLIP_2), 1);
  EXPECT_EQ(routing->getClipGroup(CLIP_3), 2);
  EXPECT_EQ(routing->getClipGroup(CLIP_4), 3);
}

TEST_F(ClipRoutingMatrixTest, GetClipGroupForUnassignedClip) {
  // Never assigned CLIP_1
  EXPECT_EQ(routing->getClipGroup(CLIP_1), 255); // UNASSIGNED_GROUP
}

TEST_F(ClipRoutingMatrixTest, AssignInvalidClipHandle) {
  auto result = routing->assignClipToGroup(0, 0); // ClipHandle 0 is invalid

  EXPECT_EQ(result, SessionGraphError::InvalidHandle);
}

// ============================================================================
// Group Gain Tests
// ============================================================================

TEST_F(ClipRoutingMatrixTest, SetGroupGainToValidValue) {
  auto result = routing->setGroupGain(0, -6.0f);

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_FLOAT_EQ(routing->getGroupGain(0), -6.0f);
}

TEST_F(ClipRoutingMatrixTest, SetGroupGainToZeroDb) {
  routing->setGroupGain(0, -12.0f);
  auto result = routing->setGroupGain(0, 0.0f); // Unity gain

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_FLOAT_EQ(routing->getGroupGain(0), 0.0f);
}

TEST_F(ClipRoutingMatrixTest, SetGroupGainToMaximum) {
  auto result = routing->setGroupGain(0, 12.0f); // +12 dB

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_FLOAT_EQ(routing->getGroupGain(0), 12.0f);
}

TEST_F(ClipRoutingMatrixTest, SetGroupGainToMinimum) {
  auto result = routing->setGroupGain(0, -60.0f); // -60 dB (near silence)

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_FLOAT_EQ(routing->getGroupGain(0), -60.0f);
}

TEST_F(ClipRoutingMatrixTest, SetGroupGainClampsAboveMaximum) {
  routing->setGroupGain(0, 20.0f); // Above +12 dB max

  // Should clamp to +12 dB
  EXPECT_FLOAT_EQ(routing->getGroupGain(0), 12.0f);
}

TEST_F(ClipRoutingMatrixTest, SetGroupGainClampsBelowMinimum) {
  routing->setGroupGain(0, -100.0f); // Below -60 dB min

  // Should clamp to -60 dB
  EXPECT_FLOAT_EQ(routing->getGroupGain(0), -60.0f);
}

TEST_F(ClipRoutingMatrixTest, SetGroupGainForInvalidGroup) {
  auto result = routing->setGroupGain(4, 0.0f); // Only 0-3 are valid

  EXPECT_EQ(result, SessionGraphError::InvalidParameter);
}

TEST_F(ClipRoutingMatrixTest, GetGroupGainForUnsetGroup) {
  // Never set gain for group 0 (should default to 0 dB)
  EXPECT_FLOAT_EQ(routing->getGroupGain(0), 0.0f);
}

TEST_F(ClipRoutingMatrixTest, GetGroupGainForInvalidGroup) {
  float gain = routing->getGroupGain(4); // Invalid group

  EXPECT_FLOAT_EQ(gain, 0.0f); // Returns 0 dB for invalid group
}

// ============================================================================
// Group Mute Tests
// ============================================================================

TEST_F(ClipRoutingMatrixTest, MuteGroup) {
  auto result = routing->setGroupMute(0, true);

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_TRUE(routing->isGroupMuted(0));
}

TEST_F(ClipRoutingMatrixTest, UnmuteGroup) {
  routing->setGroupMute(0, true);
  auto result = routing->setGroupMute(0, false);

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_FALSE(routing->isGroupMuted(0));
}

TEST_F(ClipRoutingMatrixTest, GroupIsUnmutedByDefault) {
  EXPECT_FALSE(routing->isGroupMuted(0));
}

TEST_F(ClipRoutingMatrixTest, SetGroupMuteForInvalidGroup) {
  auto result = routing->setGroupMute(4, true); // Only 0-3 are valid

  EXPECT_EQ(result, SessionGraphError::InvalidParameter);
}

TEST_F(ClipRoutingMatrixTest, IsGroupMutedForInvalidGroup) {
  bool is_muted = routing->isGroupMuted(4); // Invalid group

  EXPECT_TRUE(is_muted); // Invalid groups are considered muted
}

TEST_F(ClipRoutingMatrixTest, MuteMultipleGroups) {
  routing->setGroupMute(0, true);
  routing->setGroupMute(1, true);
  routing->setGroupMute(2, true);

  EXPECT_TRUE(routing->isGroupMuted(0));
  EXPECT_TRUE(routing->isGroupMuted(1));
  EXPECT_TRUE(routing->isGroupMuted(2));
  EXPECT_FALSE(routing->isGroupMuted(3)); // Not muted
}

// ============================================================================
// Group Solo Tests
// ============================================================================

TEST_F(ClipRoutingMatrixTest, SoloGroupMutesOthers) {
  routing->setGroupSolo(0, true);

  EXPECT_TRUE(routing->isGroupSoloed(0));
  EXPECT_FALSE(routing->isGroupMuted(0)); // Solo'd group is NOT muted
  EXPECT_TRUE(routing->isGroupMuted(1));  // Other groups ARE muted
  EXPECT_TRUE(routing->isGroupMuted(2));
  EXPECT_TRUE(routing->isGroupMuted(3));
}

TEST_F(ClipRoutingMatrixTest, UnsoloGroupRestoresNormalMuting) {
  routing->setGroupSolo(0, true);
  routing->setGroupSolo(0, false); // Unsolo

  EXPECT_FALSE(routing->isGroupSoloed(0));
  EXPECT_FALSE(routing->isGroupMuted(0));
  EXPECT_FALSE(routing->isGroupMuted(1));
  EXPECT_FALSE(routing->isGroupMuted(2));
  EXPECT_FALSE(routing->isGroupMuted(3));
}

TEST_F(ClipRoutingMatrixTest, SoloMultipleGroups) {
  routing->setGroupSolo(0, true);
  routing->setGroupSolo(2, true);

  // Both solo'd groups should play
  EXPECT_FALSE(routing->isGroupMuted(0));
  EXPECT_FALSE(routing->isGroupMuted(2));

  // Non-solo'd groups should be muted
  EXPECT_TRUE(routing->isGroupMuted(1));
  EXPECT_TRUE(routing->isGroupMuted(3));
}

TEST_F(ClipRoutingMatrixTest, SoloOverridesExplicitMute) {
  routing->setGroupMute(0, true); // Explicitly mute group 0
  routing->setGroupSolo(1, true); // Solo group 1

  // Group 0 is muted (solo active, not solo'd)
  EXPECT_TRUE(routing->isGroupMuted(0));

  // Group 1 is NOT muted (solo'd)
  EXPECT_FALSE(routing->isGroupMuted(1));
}

TEST_F(ClipRoutingMatrixTest, MuteDuringSoloMode) {
  routing->setGroupSolo(0, true); // Solo group 0
  routing->setGroupMute(0, true); // Also mute group 0

  // Explicit mute should take precedence (mute + solo = muted)
  EXPECT_TRUE(routing->isGroupMuted(0));
}

TEST_F(ClipRoutingMatrixTest, SetGroupSoloForInvalidGroup) {
  auto result = routing->setGroupSolo(4, true); // Only 0-3 are valid

  EXPECT_EQ(result, SessionGraphError::InvalidParameter);
}

TEST_F(ClipRoutingMatrixTest, IsGroupSoloedForInvalidGroup) {
  bool is_soloed = routing->isGroupSoloed(4); // Invalid group

  EXPECT_FALSE(is_soloed); // Invalid groups are not soloed
}

TEST_F(ClipRoutingMatrixTest, GroupIsNotSoloedByDefault) {
  EXPECT_FALSE(routing->isGroupSoloed(0));
}

// ============================================================================
// Route to Master Tests
// ============================================================================

TEST_F(ClipRoutingMatrixTest, DisableGroupRoutingToMaster) {
  auto result = routing->routeGroupToMaster(0, false);

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_FALSE(routing->isGroupRoutedToMaster(0));
}

TEST_F(ClipRoutingMatrixTest, EnableGroupRoutingToMaster) {
  routing->routeGroupToMaster(0, false);
  auto result = routing->routeGroupToMaster(0, true);

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_TRUE(routing->isGroupRoutedToMaster(0));
}

TEST_F(ClipRoutingMatrixTest, GroupIsRoutedToMasterByDefault) {
  EXPECT_TRUE(routing->isGroupRoutedToMaster(0));
}

TEST_F(ClipRoutingMatrixTest, RouteGroupToMasterForInvalidGroup) {
  auto result = routing->routeGroupToMaster(4, true); // Only 0-3 are valid

  EXPECT_EQ(result, SessionGraphError::InvalidParameter);
}

TEST_F(ClipRoutingMatrixTest, IsGroupRoutedToMasterForInvalidGroup) {
  bool is_routed = routing->isGroupRoutedToMaster(4); // Invalid group

  EXPECT_FALSE(is_routed); // Invalid groups are not routed
}

TEST_F(ClipRoutingMatrixTest, DisableMultipleGroupsRoutingToMaster) {
  routing->routeGroupToMaster(0, false);
  routing->routeGroupToMaster(1, false);
  routing->routeGroupToMaster(2, false);

  EXPECT_FALSE(routing->isGroupRoutedToMaster(0));
  EXPECT_FALSE(routing->isGroupRoutedToMaster(1));
  EXPECT_FALSE(routing->isGroupRoutedToMaster(2));
  EXPECT_TRUE(routing->isGroupRoutedToMaster(3)); // Still routed
}

// ============================================================================
// Integration / Multi-Group Scenarios
// ============================================================================

TEST_F(ClipRoutingMatrixTest, SixteenClipsAcrossFourGroups) {
  // Assign 16 clips to 4 groups (4 clips per group)
  for (ClipHandle handle = 1; handle <= 16; ++handle) {
    uint8_t group = (handle - 1) / 4; // Groups: 0,0,0,0, 1,1,1,1, 2,2,2,2, 3,3,3,3
    routing->assignClipToGroup(handle, group);
  }

  // Verify assignments
  EXPECT_EQ(routing->getClipGroup(1), 0);
  EXPECT_EQ(routing->getClipGroup(4), 0);
  EXPECT_EQ(routing->getClipGroup(5), 1);
  EXPECT_EQ(routing->getClipGroup(8), 1);
  EXPECT_EQ(routing->getClipGroup(9), 2);
  EXPECT_EQ(routing->getClipGroup(12), 2);
  EXPECT_EQ(routing->getClipGroup(13), 3);
  EXPECT_EQ(routing->getClipGroup(16), 3);
}

TEST_F(ClipRoutingMatrixTest, IndependentGroupGainControl) {
  // Configure different gains for each group
  routing->setGroupGain(0, -6.0f);
  routing->setGroupGain(1, 0.0f); // Unity
  routing->setGroupGain(2, -3.0f);
  routing->setGroupGain(3, -12.0f);

  EXPECT_FLOAT_EQ(routing->getGroupGain(0), -6.0f);
  EXPECT_FLOAT_EQ(routing->getGroupGain(1), 0.0f);
  EXPECT_FLOAT_EQ(routing->getGroupGain(2), -3.0f);
  EXPECT_FLOAT_EQ(routing->getGroupGain(3), -12.0f);
}

TEST_F(ClipRoutingMatrixTest, SoloAllGroupsAllowsAll) {
  // Solo all groups
  routing->setGroupSolo(0, true);
  routing->setGroupSolo(1, true);
  routing->setGroupSolo(2, true);
  routing->setGroupSolo(3, true);

  // All groups should be playing (none muted by solo logic)
  EXPECT_FALSE(routing->isGroupMuted(0));
  EXPECT_FALSE(routing->isGroupMuted(1));
  EXPECT_FALSE(routing->isGroupMuted(2));
  EXPECT_FALSE(routing->isGroupMuted(3));
}

TEST_F(ClipRoutingMatrixTest, MuteWhileSoloed) {
  routing->setGroupSolo(0, true); // Solo group 0
  routing->setGroupMute(0, true); // Also mute group 0

  // Explicit mute should apply even when solo'd
  EXPECT_TRUE(routing->isGroupMuted(0));
  EXPECT_TRUE(routing->isGroupMuted(1)); // Not solo'd
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(ClipRoutingMatrixTest, RapidParameterChanges) {
  // Simulate rapid UI changes (no crashes/errors expected)
  for (int i = 0; i < 100; ++i) {
    routing->setGroupGain(0, static_cast<float>(i % 12) - 6.0f);
    routing->setGroupMute(1, i % 2 == 0);
    routing->setGroupSolo(2, i % 3 == 0);
    routing->routeGroupToMaster(3, i % 2 != 0);
  }

  // Should complete without crashes
  EXPECT_TRUE(true);
}

TEST_F(ClipRoutingMatrixTest, ReassignClipManyTimes) {
  // Reassign same clip multiple times
  for (int i = 0; i < 100; ++i) {
    routing->assignClipToGroup(CLIP_1, i % 4);
  }

  // Final assignment should be group 3 (99 % 4 = 3, last iteration i=99)
  EXPECT_EQ(routing->getClipGroup(CLIP_1), 3);
}

TEST_F(ClipRoutingMatrixTest, ConcurrentAssignments) {
  // Assign many clips simultaneously (simulating fast UI actions)
  for (ClipHandle handle = 1; handle <= 64; ++handle) {
    routing->assignClipToGroup(handle, (handle - 1) % 4);
  }

  // Verify assignments
  EXPECT_EQ(routing->getClipGroup(1), 0);
  EXPECT_EQ(routing->getClipGroup(64), 3);
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
