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

constexpr uint32_t kBgPrimary = 0xFF0a1518;   // Deep petrol chassis floor
constexpr uint32_t kBgSecondary = 0xFF0f1f23; // Panel base
constexpr uint32_t kBgSurface = 0xFF17292f;   // Elevated panel
constexpr uint32_t kBgComponent = 0xFF1f363c; // Component well
constexpr uint32_t kBgInset = 0xFF0c1a1e;     // Recessed control

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

constexpr uint32_t kNeveBlue = 0xFF4d8b9e;     // Refined Neve petrol blue
constexpr uint32_t kNeveBlueDark = 0xFF2f6a7d; // Darker variant
constexpr uint32_t kAmber = 0xFFd99450;        // Desaturated VU gold
constexpr uint32_t kAmberGlow = 0xFFebb06a;    // Bright amber highlight
constexpr uint32_t kAccentGreen = 0xFF5fa572;  // EMT plate green
constexpr uint32_t kAccentYellow = 0xFFd4a844; // Warm gold (not harsh yellow)
constexpr uint32_t kAccentOrange = 0xFFd47a3a; // Warm coral-orange
constexpr uint32_t kAccentCyan = 0xFF7ab5c5;   // Soft cyan for highlights

// Legacy compatibility
constexpr uint32_t kAccentTeal = kNeveBlue;

//==============================================================================
// BORDERS & TEXT (Warm, not clinical white)
//==============================================================================

constexpr uint32_t kBorderDefault = 0xFF2d4a52;   // Petrol hairline
constexpr uint32_t kBorderActive = 0xFF4d8b9e;    // Neve blue for active
constexpr uint32_t kBorderHighlight = 0xFF707580; // Bright edge
constexpr uint32_t kTextPrimary = 0xFFede2cc;     // Aged ivory
constexpr uint32_t kTextSecondary = 0xFF8a8276;   // Desaturated tan
constexpr uint32_t kTextMuted = 0xFF5a5448;       // Very muted

//==============================================================================
// METERING COLORS (Warm vintage VU aesthetic)
//==============================================================================

constexpr uint32_t kMeterGreen = 0xFF45a855;  // Softer, warmer green
constexpr uint32_t kMeterYellow = 0xFFc5a530; // Gold, not neon yellow
constexpr uint32_t kMeterOrange = 0xFFc57030; // Warm amber-orange
constexpr uint32_t kMeterRed = 0xFFb53535;    // Deep coral red

//==============================================================================
// EXTENDED CONSOLE PALETTE (design-system named tokens)
// Mirrors --console-* CSS vars from orpheus_design-system_2605/colors_and_type.css
//==============================================================================

constexpr uint32_t kConsoleCoral = 0xFFb04848;  // --console-coral (danger / Stop All)
constexpr uint32_t kConsolePatina = 0xFF4a6b6f; // Oxidized teal accent
constexpr uint32_t kConsoleWalnut = 0xFF3a2820; // Deep walnut cabinetry
constexpr uint32_t kConsoleTan = 0xFFc4a87a;    // Tan label panel

//==============================================================================
// CLIP STATE COLORS (Console button aesthetic)
//==============================================================================

constexpr uint32_t kClipEmpty = 0xFF122024;    // Unlit button well
constexpr uint32_t kClipLoaded = 0xFF1f363c;   // Ready state
constexpr uint32_t kClipPlaying = 0xFF4a8a5a;  // Active green
constexpr uint32_t kClipCued = 0xFFc89456;     // Amber standby
constexpr uint32_t kClipStopping = 0xFFb86838; // Fade-out orange

//==============================================================================
// GROUP COLORS (Neve channel strip inspired)
//==============================================================================

constexpr uint32_t kGroupBlue = 0xFF4d8b9e;   // Neve petrol
constexpr uint32_t kGroupGreen = 0xFF4a8a5a;  // EMT forest
constexpr uint32_t kGroupOrange = 0xFFc47540; // Burnt orange
constexpr uint32_t kGroupRed = 0xFFb04848;    // Deep coral

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

//==============================================================================
// CLIP BUTTON SPECIFIC
//==============================================================================

namespace ClipButton {
// Typography (HK Grotesk)
constexpr float kFontButtonNumber = 21.6f; // Empty state button number
constexpr float kFontHotkey = 12.0f;       // Hotkey label
constexpr float kFontBeatOffset = 11.0f;   // Beat offset indicator
constexpr float kFontClipName = 18.0f;     // Clip name (primary)
constexpr float kFontTimeDisplay = 15.0f;  // Elapsed/remaining time
constexpr float kFontGroupLabel = 10.8f;   // Group badge text

// Indicator box dimensions
constexpr float kIndicatorBoxWidth = 36.0f;  // Width for number/hotkey boxes
constexpr float kIndicatorBoxHeight = 16.0f; // Height for indicator boxes
constexpr float kPlayBoxWidth = 24.0f;       // Play icon box width
constexpr float kBeatAreaWidth = 40.0f;      // Beat offset display area

// Status icons
constexpr float kSmallIconSize = 14.0f; // Status icon dimensions
constexpr float kIconGap = 4.0f;        // Gap between status icons
constexpr float kIconRadius = 5.0f;     // Loop icon arc radius
constexpr float kHexagonRadius = 7.0f;  // Stop-others hexagon radius
constexpr float kTriangleSize = 7.0f;   // Play triangle size

// Animation
constexpr float kPulseCycleMs = 500.0f;      // Playing state pulse cycle
constexpr float kHoverBrightnessMax = 0.15f; // Max hover brightness boost
constexpr float kHoverGlowAlpha = 0.35f;     // Hover glow opacity
constexpr float kPlayingBorderWidth = 5.0f;  // Thick border for playing state
constexpr float kPlayboxBorderWidth = 1.5f;  // Thin border for playbox outline

// Alpha values
constexpr float kLoadedAlpha = 0.9f;         // Loaded clip background
constexpr float kTextShadowAlpha = 0.5f;     // Text shadow opacity
constexpr float kProgressBgAlpha = 0.3f;     // Progress bar background
constexpr float kGroupBadgeAlpha = 0.8f;     // Group badge background
constexpr float kBackdropAlpha = 0.85f;      // Time display backdrop
constexpr float kBrightnessThreshold = 0.8f; // Threshold for text color switch
} // namespace ClipButton

//==============================================================================
// LAYOUT CONSTANTS (Dialog-specific)
//==============================================================================

namespace Layout {
constexpr int kRowHeight = 32;       // Standard row height (matches kSpace8)
constexpr int kSmallRow = 24;        // Compact row height (matches kSpace6)
constexpr int kSectionGap = 16;      // Gap between sections (matches kSpace4)
constexpr int kItemGap = 8;          // Gap between items (matches kSpace2)
constexpr int kDialSize = 64;        // Rotary dial diameter
constexpr int kTimeFieldWidth = 110; // Time input field width
constexpr int kButtonHeight = 40;    // Standard button height (25% taller than default)
} // namespace Layout

//==============================================================================
// WAVEFORM DISPLAY (WaveformDisplay.cpp)
//==============================================================================

namespace Waveform {
// Dimensions
constexpr float kScaleWidth = 40.0f;      // dB scale column width
constexpr float kTimeScaleHeight = 30.0f; // Time scale row height
constexpr float kHandleTolerance = 8.0f;  // Click tolerance for trim handles
constexpr float kMarkerWidth = 2.0f;      // Trim marker line width
constexpr float kPlayheadWidth = 3.0f;    // Playhead line width
constexpr float kHandleWidth = 6.0f;      // Trim handle width
constexpr float kHandleHeight = 12.0f;    // Trim handle height

// Pagination (auto-scroll during playback)
constexpr float kPaginationLeftTrigger = 0.10f;  // 10% from left edge
constexpr float kPaginationRightTrigger = 0.90f; // 90% from left edge
constexpr float kPaginationCenterOffset = 0.40f; // Position playhead at 10% from left

// Zoom levels (index → factor)
constexpr float kZoomFactors[] = {1.0f, 2.0f, 4.0f, 8.0f, 16.0f};
constexpr int kMaxZoomLevel = 4;

// Colors
constexpr uint32_t kBgWaveform = 0xFF1a1a1a;     // Waveform area background
constexpr uint32_t kBgTimeScale = 0xFF0f0f0f;    // Time scale area background
constexpr uint32_t kWaveformBlue = 0xFF4a9eff;   // Waveform line color
constexpr uint32_t kTrimInMagenta = 0xFFff00ff;  // IN marker (SpotOn standard)
constexpr uint32_t kTrimOutCyan = 0xFF00ffff;    // OUT marker (SpotOn standard)
constexpr uint32_t kPlayheadYellow = 0xFFffff00; // Playhead/audition highlight
constexpr uint32_t kShadedRegion = 0x80000000;   // 50% black for trimmed regions

// Alpha values
constexpr float kMarkerAlpha = 0.8f;             // Trim marker opacity
constexpr float kPlayheadAlpha = 0.9f;           // Playhead opacity
constexpr float kAuditionHighlightAlpha = 0.15f; // Audition region highlight
constexpr float kCenterLineAlpha = 0.2f;         // Center line opacity
constexpr float kScaleTextAlpha = 0.7f;          // dB/time scale text opacity
} // namespace Waveform

} // namespace OCC::Design
