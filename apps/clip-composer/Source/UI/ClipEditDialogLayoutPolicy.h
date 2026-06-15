// SPDX-License-Identifier: MIT

#pragma once

namespace occ::clip_edit {

inline constexpr int kAdvancedDialSizePx = 56;

inline constexpr int advancedSectionContentHeight(bool expanded, int grid) {
  if (!expanded)
    return 0;

  return (kAdvancedDialSizePx + grid * 2) // Gain/pitch dial row plus value labels
         + grid                           // Gap before trim nudge controls
         + grid * 3                       // SET / nudge / CLR row
         + grid                           // Gap before fade-curve controls
         + grid * 3;                      // Fade-curve combo row
}

inline constexpr int advancedSectionTotalHeight(bool expanded, int grid, int eyebrowHeight,
                                                int eyebrowGap) {
  return eyebrowHeight + eyebrowGap + advancedSectionContentHeight(expanded, grid);
}

} // namespace occ::clip_edit
