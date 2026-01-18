// SPDX-License-Identifier: MIT
// DesignTokens.h - Unified design token system for Clip Composer
// Neo-vintage console aesthetic inspired by Neve hardware

#pragma once

#include <cstdint>

namespace OCC::Design {

//==============================================================================
// NEVE-INSPIRED PALETTE
// Based on modern Neve console aesthetic: brushed aluminum, warm indicators,
// premium tactile feel. Deep charcoal with subtle blue undertone.
//==============================================================================

//==============================================================================
// BACKGROUNDS (Deep slate with subtle warmth)
//==============================================================================

constexpr uint32_t kBgPrimary = 0xFF0d0e10;   // Near-black with blue undertone
constexpr uint32_t kBgSecondary = 0xFF1a1c1f; // Console chassis
constexpr uint32_t kBgSurface = 0xFF252830;   // Elevated panels
constexpr uint32_t kBgComponent = 0xFF2d3139; // Component wells
constexpr uint32_t kBgInset = 0xFF1f2125;     // Recessed areas

//==============================================================================
// CONSOLE METALLICS (Brushed aluminum simulation)
//==============================================================================

constexpr uint32_t kMetalLight = 0xFF8a9199; // Highlight edge
constexpr uint32_t kMetalMid = 0xFF5a6169;   // Mid-tone aluminum
constexpr uint32_t kMetalDark = 0xFF3a4049;  // Shadow edge
constexpr uint32_t kMetalWarm = 0xFF6a635d;  // Warm aluminum tint

//==============================================================================
// ACCENTS (Neve-inspired, premium feel)
//==============================================================================

constexpr uint32_t kNeveBlue = 0xFF5a9ab5;     // Signature Neve blue-teal
constexpr uint32_t kNeveBlueDark = 0xFF3d7a8f; // Darker variant
constexpr uint32_t kAmber = 0xFFe8a445;        // Warm amber (VU meter gold)
constexpr uint32_t kAmberGlow = 0xFFf5c466;    // Bright amber highlight
constexpr uint32_t kAccentGreen = 0xFF48b060;  // Softer, warmer green
constexpr uint32_t kAccentYellow = 0xFFd4a844; // Warm gold (not harsh yellow)
constexpr uint32_t kAccentOrange = 0xFFd47a3a; // Warm coral-orange
constexpr uint32_t kAccentCyan = 0xFF7ab5c5;   // Soft cyan for highlights

// Legacy compatibility
constexpr uint32_t kAccentTeal = kNeveBlue;

//==============================================================================
// BORDERS & TEXT (Warm, not clinical white)
//==============================================================================

constexpr uint32_t kBorderDefault = 0xFF404550;   // Subtle slate border
constexpr uint32_t kBorderActive = 0xFF5a9ab5;    // Neve blue for active
constexpr uint32_t kBorderHighlight = 0xFF707580; // Bright edge
constexpr uint32_t kTextPrimary = 0xFFf0ece6;     // Warm off-white (cream)
constexpr uint32_t kTextSecondary = 0xFF8a8a8a;   // Muted gray
constexpr uint32_t kTextMuted = 0xFF5a5a5a;       // Very muted

//==============================================================================
// METERING COLORS (Warm vintage VU aesthetic)
//==============================================================================

constexpr uint32_t kMeterGreen = 0xFF45a855;  // Softer, warmer green
constexpr uint32_t kMeterYellow = 0xFFc5a530; // Gold, not neon yellow
constexpr uint32_t kMeterOrange = 0xFFc57030; // Warm amber-orange
constexpr uint32_t kMeterRed = 0xFFb53535;    // Deep coral red

//==============================================================================
// CLIP STATE COLORS (Console button aesthetic)
//==============================================================================

constexpr uint32_t kClipEmpty = 0xFF1f2225;    // Unlit button well
constexpr uint32_t kClipLoaded = 0xFF2d3139;   // Ready state
constexpr uint32_t kClipPlaying = 0xFF3a8548;  // Active green glow
constexpr uint32_t kClipCued = 0xFFa87830;     // Amber standby
constexpr uint32_t kClipStopping = 0xFFa85a30; // Fade-out coral

//==============================================================================
// GROUP COLORS (Neve channel strip inspired)
//==============================================================================

constexpr uint32_t kGroupBlue = 0xFF5a9ab5;   // Neve blue
constexpr uint32_t kGroupGreen = 0xFF4a9a60;  // Console green
constexpr uint32_t kGroupOrange = 0xFFd48a40; // Warm orange
constexpr uint32_t kGroupRed = 0xFFc55050;    // Coral red

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
