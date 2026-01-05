// SPDX-License-Identifier: MIT
// DesignTokens.h - Unified design token system for Clip Composer
// Inspired by Minimeters plugin aesthetic

#pragma once

#include <cstdint>

namespace OCC::Design {

//==============================================================================
// BACKGROUNDS
//==============================================================================

constexpr uint32_t kBgPrimary = 0xFF000000;   // Pure black
constexpr uint32_t kBgSecondary = 0xFF151515; // Current app bg
constexpr uint32_t kBgSurface = 0xFF252930;   // Elevated surfaces
constexpr uint32_t kBgComponent = 0xFF2a2a2a; // Component bg

//==============================================================================
// ACCENTS
//==============================================================================

constexpr uint32_t kAccentCyan = 0xFFc8e9fd;   // Minimeters cyan
constexpr uint32_t kAccentTeal = 0xFF2a9d8f;   // Current active tab
constexpr uint32_t kAccentGreen = 0xFF00dd00;  // Playing (softened)
constexpr uint32_t kAccentYellow = 0xFFffff00; // Loop indicator
constexpr uint32_t kAccentOrange = 0xFFff8800; // Fade out / stopping

//==============================================================================
// BORDERS & TEXT
//==============================================================================

constexpr uint32_t kBorderDefault = 0xFF404040;
constexpr uint32_t kBorderActive = 0xFF3ab7a8;
constexpr uint32_t kTextPrimary = 0xFFffffff;
constexpr uint32_t kTextSecondary = 0xFF888888;

//==============================================================================
// METERING COLORS
//==============================================================================

constexpr uint32_t kMeterGreen = 0xFF00cc00;
constexpr uint32_t kMeterYellow = 0xFFcccc00;
constexpr uint32_t kMeterOrange = 0xFFcc6600;
constexpr uint32_t kMeterRed = 0xFFcc0000;

//==============================================================================
// CLIP STATE COLORS
//==============================================================================

constexpr uint32_t kClipEmpty = 0xFF1a1a1a;
constexpr uint32_t kClipLoaded = 0xFF2a2a2a;
constexpr uint32_t kClipPlaying = 0xFF00aa00;
constexpr uint32_t kClipCued = 0xFFaa8800;
constexpr uint32_t kClipStopping = 0xFFaa4400;

//==============================================================================
// GROUP COLORS
//==============================================================================

constexpr uint32_t kGroupBlue = 0xFF3498db;   // Group 0 - Blue
constexpr uint32_t kGroupGreen = 0xFF2ecc71;  // Group 1 - Green
constexpr uint32_t kGroupOrange = 0xFFf39c12; // Group 2 - Orange
constexpr uint32_t kGroupRed = 0xFFe74c3c;    // Group 3 - Red

//==============================================================================
// SPACING SCALE (4px base)
//==============================================================================

constexpr float kSpace1 = 4.0f;
constexpr float kSpace2 = 8.0f;
constexpr float kSpace3 = 12.0f;
constexpr float kSpace4 = 16.0f;
constexpr float kSpace6 = 24.0f;
constexpr float kSpace8 = 32.0f;

//==============================================================================
// TYPOGRAPHY SCALE (8-point)
//==============================================================================

constexpr float kFontXS = 10.0f;
constexpr float kFontSM = 12.0f;
constexpr float kFontMD = 14.0f;
constexpr float kFontLG = 16.0f;
constexpr float kFontXL = 18.0f;
constexpr float kFont2XL = 21.0f;
constexpr float kFont3XL = 24.0f;

//==============================================================================
// CORNER RADIUS
//==============================================================================

constexpr float kRadiusSM = 2.0f;
constexpr float kRadiusMD = 4.0f;
constexpr float kRadiusLG = 8.0f;

//==============================================================================
// BORDER THICKNESS
//==============================================================================

constexpr float kBorderThin = 1.0f;
constexpr float kBorderMedium = 2.0f;
constexpr float kBorderThick = 3.0f;

//==============================================================================
// ANIMATION
//==============================================================================

constexpr float kAnimDurationFast = 0.1f;   // 100ms
constexpr float kAnimDurationNormal = 0.2f; // 200ms
constexpr float kAnimDurationSlow = 0.3f;   // 300ms

constexpr int kRefreshRateHz = 60;
constexpr float kPulseFrequency = 2.0f; // Hz for playing state pulse

} // namespace OCC::Design
