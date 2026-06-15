// SPDX-License-Identifier: MIT
// Clip Edit dialog layout policy tests for OCC149c advanced disclosure behavior.

#include "UI/ClipEditDialogLayoutPolicy.h"
#include <gtest/gtest.h>

TEST(ClipEditDialogLayoutPolicyTest, ExpandedAdvancedSectionReservesSecondaryControlHeight) {
  constexpr int grid = 10;

  EXPECT_EQ(occ::clip_edit::advancedSectionContentHeight(true, grid), 156);
}

TEST(ClipEditDialogLayoutPolicyTest, CollapsedAdvancedSectionReservesNoSecondaryControlHeight) {
  constexpr int grid = 10;

  EXPECT_EQ(occ::clip_edit::advancedSectionContentHeight(false, grid), 0);
}

TEST(ClipEditDialogLayoutPolicyTest, DisclosureSectionHeightKeepsEyebrowVisibleWhenCollapsed) {
  constexpr int grid = 10;
  constexpr int eyebrowHeight = 14;
  constexpr int eyebrowGap = 4;

  EXPECT_EQ(occ::clip_edit::advancedSectionTotalHeight(false, grid, eyebrowHeight, eyebrowGap),
            eyebrowHeight + eyebrowGap);
  EXPECT_GT(occ::clip_edit::advancedSectionTotalHeight(true, grid, eyebrowHeight, eyebrowGap),
            eyebrowHeight + eyebrowGap);
}
