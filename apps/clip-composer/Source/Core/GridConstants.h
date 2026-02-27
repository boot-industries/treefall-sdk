// SPDX-License-Identifier: MIT

#pragma once

namespace occ {

/// Grid layout constants for Clip Composer.
/// Single source of truth for button counts across all components.

static constexpr int BUTTONS_PER_TAB = 48; // 6 rows x 8 cols (MVP layout)
static constexpr int NUM_TABS = 8;
static constexpr int TOTAL_BUTTONS = BUTTONS_PER_TAB * NUM_TABS; // 384
static constexpr int MAX_AUDIO_SLOTS = 960; // Pre-allocated for v1.0 (10x12 x 8 tabs)

} // namespace occ
