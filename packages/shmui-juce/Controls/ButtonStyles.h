/*
  ==============================================================================

    ButtonStyles.h
    Created: shmui Button System

    Style and size enumerations for the button component system.

  ==============================================================================
*/

#pragma once

#include "../Utils/DesignTokens.h"
#include "../Utils/ShmuiTheme.h"
#include <JuceHeader.h>

namespace shmui {

//==============================================================================
/**
 * @brief Visual style variants for buttons.
 */
enum class ButtonStyle {
  Primary,     ///< Solid fill, high contrast - main actions
  Secondary,   ///< Outlined, medium contrast - secondary actions
  Ghost,       ///< No background until hover - toolbar buttons
  Destructive, ///< Red/warning color - delete, stop
  Success,     ///< Green color - confirm, save
  Muted        ///< Low contrast, subtle - disabled-looking but clickable
};

//==============================================================================
/**
 * @brief Size variants for buttons.
 */
enum class ButtonSize {
  XSmall, ///< 24px - Compact toolbars
  Small,  ///< 32px - Standard toolbar
  Medium, ///< 40px - Default
  Large,  ///< 48px - Prominent actions
  XLarge  ///< 56px - Hero/transport
};

//==============================================================================
/**
 * @brief Get the height in pixels for a button size.
 */
inline float getButtonHeight(ButtonSize size) {
  switch (size) {
  case ButtonSize::XSmall:
    return 24.0f;
  case ButtonSize::Small:
    return 32.0f;
  case ButtonSize::Medium:
    return 40.0f;
  case ButtonSize::Large:
    return 48.0f;
  case ButtonSize::XLarge:
    return 56.0f;
  default:
    return 40.0f;
  }
}

//==============================================================================
/**
 * @brief Get the icon size for a button size.
 */
inline float getIconSizeForButton(ButtonSize size) {
  switch (size) {
  case ButtonSize::XSmall:
    return 16.0f;
  case ButtonSize::Small:
    return 20.0f;
  case ButtonSize::Medium:
    return 24.0f;
  case ButtonSize::Large:
    return 32.0f;
  case ButtonSize::XLarge:
    return 40.0f;
  default:
    return 24.0f;
  }
}

//==============================================================================
/**
 * @brief Get the font height for a button size.
 */
inline float getFontHeightForButton(ButtonSize size) {
  switch (size) {
  case ButtonSize::XSmall:
    return 11.0f;
  case ButtonSize::Small:
    return 12.0f;
  case ButtonSize::Medium:
    return 14.0f;
  case ButtonSize::Large:
    return 16.0f;
  case ButtonSize::XLarge:
    return 18.0f;
  default:
    return 14.0f;
  }
}

//==============================================================================
/**
 * @brief Get the corner radius for a button size.
 */
inline float getCornerRadiusForButton(ButtonSize size) {
  switch (size) {
  case ButtonSize::XSmall:
    return 4.0f;
  case ButtonSize::Small:
    return 5.0f;
  case ButtonSize::Medium:
    return 6.0f;
  case ButtonSize::Large:
    return 8.0f;
  case ButtonSize::XLarge:
    return 10.0f;
  default:
    return 6.0f;
  }
}

//==============================================================================
/**
 * @brief Get the padding for a button size.
 */
inline float getPaddingForButton(ButtonSize size) {
  switch (size) {
  case ButtonSize::XSmall:
    return 4.0f;
  case ButtonSize::Small:
    return 6.0f;
  case ButtonSize::Medium:
    return 8.0f;
  case ButtonSize::Large:
    return 10.0f;
  case ButtonSize::XLarge:
    return 12.0f;
  default:
    return 8.0f;
  }
}

//==============================================================================
/**
 * @brief Color scheme for a button style.
 */
struct ButtonColors {
  juce::Colour background;
  juce::Colour backgroundHover;
  juce::Colour backgroundPressed;
  juce::Colour foreground;
  juce::Colour foregroundDisabled;
  juce::Colour border;
  juce::Colour borderHover;
};

/**
 * @brief Get the token-driven, flavor-aware colors for a button style.
 *
 * Resolves every variant from the active @ref ShmuiTheme surface + accent
 * tokens, mirroring the web `_shared/primitives.jsx` Btn variant→token map so
 * the JUCE and web buttons tint identically across the Lab / Console flavors:
 *
 *   default/secondary : bg = bg-raise,                      fg = fg,        bd = stroke
 *   primary           : bg = mix(accent 22%, bg-card),      fg = accent,    bd = accent
 *   ghost             : bg = transparent,                   fg = fg-muted,  bd = transparent
 *   destructive       : bg = mix(danger 20%, bg-card),      fg = danger,    bd = danger
 *   success           : bg = mix(meter.green 22%, bg-card), fg = green,     bd = green
 *   muted             : bg = bg-card,                       fg = fg-muted,  bd = stroke
 *
 * Orpheus is dark-first and each flavor already encodes its own surfaces, so a
 * single theme-driven path replaces the former hardcoded light/dark split.
 */
inline ButtonColors getButtonColors(ButtonStyle style, const ShmuiTheme& theme) {
  ButtonColors colors;

  // color-mix(in oklab, tint P%, bg-card) ≈ bgCard blended toward tint.
  auto mixWithCard = [&theme](juce::Colour tint, float proportion) {
    return theme.bgCard.interpolatedWith(tint, proportion);
  };

  switch (style) {
  case ButtonStyle::Primary:
    colors.background = mixWithCard(theme.accent, 0.22f);
    colors.backgroundHover = mixWithCard(theme.accent, 0.32f);
    colors.backgroundPressed = mixWithCard(theme.accent, 0.16f);
    colors.foreground = theme.accent;
    colors.foregroundDisabled = theme.accent.withAlpha(0.5f);
    colors.border = theme.accent;
    colors.borderHover = theme.accent.brighter(0.15f);
    break;

  case ButtonStyle::Secondary:
    colors.background = theme.bgRaise;
    colors.backgroundHover = theme.bgRaise.brighter(0.08f);
    colors.backgroundPressed = theme.bgRaise.darker(0.10f);
    colors.foreground = theme.fg;
    colors.foregroundDisabled = theme.fgMuted;
    colors.border = theme.stroke;
    colors.borderHover = theme.fg;
    break;

  case ButtonStyle::Ghost:
    colors.background = juce::Colours::transparentBlack;
    colors.backgroundHover = theme.fg.withAlpha(0.06f);
    colors.backgroundPressed = theme.fg.withAlpha(0.10f);
    colors.foreground = theme.fgMuted;
    colors.foregroundDisabled = theme.fgMuted.withAlpha(0.5f);
    colors.border = juce::Colours::transparentBlack;
    colors.borderHover = juce::Colours::transparentBlack;
    break;

  case ButtonStyle::Destructive:
    colors.background = mixWithCard(theme.danger, 0.20f);
    colors.backgroundHover = mixWithCard(theme.danger, 0.30f);
    colors.backgroundPressed = mixWithCard(theme.danger, 0.14f);
    colors.foreground = theme.danger;
    colors.foregroundDisabled = theme.danger.withAlpha(0.5f);
    colors.border = theme.danger;
    colors.borderHover = theme.danger.brighter(0.15f);
    break;

  case ButtonStyle::Success:
    colors.background = mixWithCard(theme.meter.green, 0.22f);
    colors.backgroundHover = mixWithCard(theme.meter.green, 0.32f);
    colors.backgroundPressed = mixWithCard(theme.meter.green, 0.16f);
    colors.foreground = theme.meter.green;
    colors.foregroundDisabled = theme.meter.green.withAlpha(0.5f);
    colors.border = theme.meter.green;
    colors.borderHover = theme.meter.green.brighter(0.15f);
    break;

  case ButtonStyle::Muted:
    colors.background = theme.bgCard;
    colors.backgroundHover = theme.bgCard.brighter(0.06f);
    colors.backgroundPressed = theme.bgCard.darker(0.08f);
    colors.foreground = theme.fgMuted;
    colors.foregroundDisabled = theme.fgMuted.withAlpha(0.5f);
    colors.border = theme.stroke;
    colors.borderHover = theme.stroke.brighter(0.10f);
    break;
  }

  return colors;
}

/**
 * @brief Deprecated — use getButtonColors(style, theme).
 *
 * Retained for one release as a thin forwarder onto the active default theme so
 * existing call sites keep compiling. The former hardcoded light/dark palettes
 * are gone; both forwarders now resolve through the flavor-aware token path.
 */
[[deprecated("use getButtonColors(style, theme)")]]
inline ButtonColors getButtonColorsLight(ButtonStyle style) {
  return getButtonColors(style, defaultTheme());
}

/** @brief Deprecated — use getButtonColors(style, theme). */
[[deprecated("use getButtonColors(style, theme)")]]
inline ButtonColors getButtonColorsDark(ButtonStyle style) {
  return getButtonColors(style, defaultTheme());
}

} // namespace shmui
