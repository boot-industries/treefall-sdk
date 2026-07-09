// SPDX-License-Identifier: MIT
// Playgroup-scoped choke policy tests (OCC151 T8 / G6)
//
// "Stop Others On Play" must be scoped to the firing clip's playgroup (clipGroup
// 0-3), not global and not the visible tab. The canonical requirement: firing a
// clip in group A must NOT stop a clip in group B.

#include "../Source/Core/ChokePolicy.h"

#include <gtest/gtest.h>

namespace {

SessionManager::ClipData clipInGroup(int group) {
  SessionManager::ClipData clip;
  clip.clipGroup = group;
  return clip;
}

} // namespace

// The headline requirement: group A firing does not stop group B.
TEST(ChokePolicyTest, FiringGroupADoesNotStopGroupB) {
  const auto firing = clipInGroup(0); // group A
  const auto other = clipInGroup(1);  // group B
  EXPECT_FALSE(occ::shouldChokeStop(firing, /*firingGlobalIndex=*/10, other,
                                    /*otherGlobalIndex=*/20, /*otherIsActive=*/true));
}

// A playing clip in the SAME group is choked.
TEST(ChokePolicyTest, FiringStopsActiveClipInSameGroup) {
  const auto firing = clipInGroup(2);
  const auto other = clipInGroup(2);
  EXPECT_TRUE(occ::shouldChokeStop(firing, 10, other, 20, /*otherIsActive=*/true));
}

// An inactive (stopped) clip in the same group is left alone.
TEST(ChokePolicyTest, DoesNotStopInactiveClipInSameGroup) {
  const auto firing = clipInGroup(2);
  const auto other = clipInGroup(2);
  EXPECT_FALSE(occ::shouldChokeStop(firing, 10, other, 20, /*otherIsActive=*/false));
}

// A clip never chokes itself, even if it is active and in "its own" group.
TEST(ChokePolicyTest, NeverStopsSelf) {
  const auto firing = clipInGroup(1);
  const auto self = clipInGroup(1);
  EXPECT_FALSE(occ::shouldChokeStop(firing, /*firingGlobalIndex=*/42, self,
                                    /*otherGlobalIndex=*/42, /*otherIsActive=*/true));
}

// Scoping holds across all four groups: only the matching group is choked.
TEST(ChokePolicyTest, OnlyMatchingGroupIsChoked) {
  for (int firingGroup = 0; firingGroup < 4; ++firingGroup) {
    const auto firing = clipInGroup(firingGroup);
    for (int otherGroup = 0; otherGroup < 4; ++otherGroup) {
      const auto other = clipInGroup(otherGroup);
      const bool expected = (otherGroup == firingGroup); // same group, active, different index
      EXPECT_EQ(occ::shouldChokeStop(firing, 1, other, 2, /*otherIsActive=*/true), expected)
          << "firingGroup=" << firingGroup << " otherGroup=" << otherGroup;
    }
  }
}
