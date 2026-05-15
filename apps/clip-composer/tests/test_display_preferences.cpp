// SPDX-License-Identifier: MIT
// DisplayPreferences tests for Clip Composer grid density

#include "Core/GridConstants.h"
#include <gtest/gtest.h>
#include <orpheus/app/DisplayPreferences.h>

namespace {

using GridLayout = orpheus::DisplayPreferences::GridLayout;

struct GridLayoutCase {
  GridLayout layout;
  const char* key;
  int columns;
  int rows;
};

constexpr GridLayoutCase kGridLayouts[] = {
    {GridLayout::Columns6Rows6, "6x6", 6, 6},    {GridLayout::Columns8Rows6, "8x6", 8, 6},
    {GridLayout::Columns10Rows6, "10x6", 10, 6}, {GridLayout::Columns6Rows8, "6x8", 6, 8},
    {GridLayout::Columns8Rows8, "8x8", 8, 8},    {GridLayout::Columns10Rows8, "10x8", 10, 8},
    {GridLayout::Columns12Rows8, "12x8", 12, 8}, {GridLayout::Columns6Rows10, "6x10", 6, 10},
    {GridLayout::Columns8Rows10, "8x10", 8, 10}, {GridLayout::Columns10Rows10, "10x10", 10, 10},
};

} // namespace

TEST(DisplayPreferencesTest, GridLayoutPersistenceKeysRoundTrip) {
  for (const auto& testCase : kGridLayouts) {
    const auto key = orpheus::DisplayPreferences::gridLayoutToString(testCase.layout);
    EXPECT_EQ(key, testCase.key);
    EXPECT_EQ(orpheus::DisplayPreferences::stringToGridLayout(key), testCase.layout);
  }
}

TEST(DisplayPreferencesTest, GridLayoutDimensionsMatchSupportedDensities) {
  for (const auto& testCase : kGridLayouts) {
    EXPECT_EQ(orpheus::DisplayPreferences::getGridLayoutColumns(testCase.layout), testCase.columns);
    EXPECT_EQ(orpheus::DisplayPreferences::getGridLayoutRows(testCase.layout), testCase.rows);
  }
}

TEST(DisplayPreferencesTest, UnknownGridLayoutFallsBackToDefault) {
  EXPECT_EQ(orpheus::DisplayPreferences::stringToGridLayout("12x10"), GridLayout::Columns8Rows6);
}

TEST(DisplayPreferencesTest, HighDensityLiveLayoutStaysWithinLogicalTabCapacity) {
  const auto layout = GridLayout::Columns12Rows8;
  EXPECT_EQ(orpheus::DisplayPreferences::getGridLayoutColumns(layout) *
                orpheus::DisplayPreferences::getGridLayoutRows(layout),
            96);
  EXPECT_EQ(occ::BUTTONS_PER_TAB, 100);
  EXPECT_LT(orpheus::DisplayPreferences::getGridLayoutColumns(layout) *
                orpheus::DisplayPreferences::getGridLayoutRows(layout),
            occ::BUTTONS_PER_TAB);
}

TEST(DisplayPreferencesTest, DeferredTenByTwelveLayoutFallsBackToDefault) {
  EXPECT_EQ(orpheus::DisplayPreferences::stringToGridLayout("10x12"), GridLayout::Columns8Rows6);
}
