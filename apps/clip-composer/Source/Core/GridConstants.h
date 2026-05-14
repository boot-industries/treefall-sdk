// SPDX-License-Identifier: MIT

#pragma once

namespace occ {

/// Grid layout constants for Clip Composer.
/// Single source of truth for button counts across all components.

static constexpr int MIN_GRID_COLUMNS = 6;
static constexpr int MAX_GRID_COLUMNS = 10;
static constexpr int MIN_GRID_ROWS = 6;
static constexpr int MAX_GRID_ROWS = 10;

static constexpr int DEFAULT_GRID_COLUMNS = 8;
static constexpr int DEFAULT_GRID_ROWS = 6;

static constexpr int BUTTONS_PER_TAB = MAX_GRID_COLUMNS * MAX_GRID_ROWS; // 100 logical slots
static constexpr int NUM_TABS = 8;
static constexpr int TOTAL_BUTTONS = BUTTONS_PER_TAB * NUM_TABS; // 800
static constexpr int MAX_AUDIO_SLOTS = 960; // Audio pre-allocation for future 10x12 x 8

} // namespace occ
