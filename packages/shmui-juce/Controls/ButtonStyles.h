/*
  ==============================================================================

    ButtonStyles.h
    Created: shmui Button System

    Style and size enumerations for the button component system.

  ==============================================================================
*/

#pragma once

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
 * @brief Get colors for a button style (light theme).
 */
inline ButtonColors getButtonColorsLight(ButtonStyle style) {
  ButtonColors colors;

  switch (style) {
  case ButtonStyle::Primary:
    colors.background = juce::Colour(0xFF1A1A1A);
    colors.backgroundHover = juce::Colour(0xFF2D2D2D);
    colors.backgroundPressed = juce::Colour(0xFF0A0A0A);
    colors.foreground = juce::Colour(0xFFFFFFFF);
    colors.foregroundDisabled = juce::Colour(0xFF808080);
    colors.border = juce::Colours::transparentBlack;
    colors.borderHover = juce::Colours::transparentBlack;
    break;

  case ButtonStyle::Secondary:
    colors.background = juce::Colours::transparentBlack;
    colors.backgroundHover = juce::Colour(0x10000000);
    colors.backgroundPressed = juce::Colour(0x20000000);
    colors.foreground = juce::Colour(0xFF1A1A1A);
    colors.foregroundDisabled = juce::Colour(0xFF808080);
    colors.border = juce::Colour(0xFFD0D0D0);
    colors.borderHover = juce::Colour(0xFF1A1A1A);
    break;

  case ButtonStyle::Ghost:
    colors.background = juce::Colours::transparentBlack;
    colors.backgroundHover = juce::Colour(0x10000000);
    colors.backgroundPressed = juce::Colour(0x20000000);
    colors.foreground = juce::Colour(0xFF1A1A1A);
    colors.foregroundDisabled = juce::Colour(0xFF808080);
    colors.border = juce::Colours::transparentBlack;
    colors.borderHover = juce::Colours::transparentBlack;
    break;

  case ButtonStyle::Destructive:
    colors.background = juce::Colour(0xFFDC2626);
    colors.backgroundHover = juce::Colour(0xFFEF4444);
    colors.backgroundPressed = juce::Colour(0xFFB91C1C);
    colors.foreground = juce::Colour(0xFFFFFFFF);
    colors.foregroundDisabled = juce::Colour(0xFFFF9999);
    colors.border = juce::Colours::transparentBlack;
    colors.borderHover = juce::Colours::transparentBlack;
    break;

  case ButtonStyle::Success:
    colors.background = juce::Colour(0xFF16A34A);
    colors.backgroundHover = juce::Colour(0xFF22C55E);
    colors.backgroundPressed = juce::Colour(0xFF15803D);
    colors.foreground = juce::Colour(0xFFFFFFFF);
    colors.foregroundDisabled = juce::Colour(0xFF99FF99);
    colors.border = juce::Colours::transparentBlack;
    colors.borderHover = juce::Colours::transparentBlack;
    break;

  case ButtonStyle::Muted:
    colors.background = juce::Colour(0xFFF5F5F5);
    colors.backgroundHover = juce::Colour(0xFFE5E5E5);
    colors.backgroundPressed = juce::Colour(0xFFD5D5D5);
    colors.foreground = juce::Colour(0xFF737373);
    colors.foregroundDisabled = juce::Colour(0xFFA3A3A3);
    colors.border = juce::Colours::transparentBlack;
    colors.borderHover = juce::Colours::transparentBlack;
    break;
  }

  return colors;
}

/**
 * @brief Get colors for a button style (dark theme).
 */
inline ButtonColors getButtonColorsDark(ButtonStyle style) {
  ButtonColors colors;

  switch (style) {
  case ButtonStyle::Primary:
    colors.background = juce::Colour(0xFFFFFFFF);
    colors.backgroundHover = juce::Colour(0xFFE5E5E5);
    colors.backgroundPressed = juce::Colour(0xFFD5D5D5);
    colors.foreground = juce::Colour(0xFF1A1A1A);
    colors.foregroundDisabled = juce::Colour(0xFF808080);
    colors.border = juce::Colours::transparentBlack;
    colors.borderHover = juce::Colours::transparentBlack;
    break;

  case ButtonStyle::Secondary:
    colors.background = juce::Colours::transparentWhite;
    colors.backgroundHover = juce::Colour(0x20FFFFFF);
    colors.backgroundPressed = juce::Colour(0x30FFFFFF);
    colors.foreground = juce::Colour(0xFFFFFFFF);
    colors.foregroundDisabled = juce::Colour(0xFF808080);
    colors.border = juce::Colour(0xFF404040);
    colors.borderHover = juce::Colour(0xFFFFFFFF);
    break;

  case ButtonStyle::Ghost:
    colors.background = juce::Colours::transparentWhite;
    colors.backgroundHover = juce::Colour(0x20FFFFFF);
    colors.backgroundPressed = juce::Colour(0x30FFFFFF);
    colors.foreground = juce::Colour(0xFFFFFFFF);
    colors.foregroundDisabled = juce::Colour(0xFF808080);
    colors.border = juce::Colours::transparentBlack;
    colors.borderHover = juce::Colours::transparentBlack;
    break;

  case ButtonStyle::Destructive:
    colors.background = juce::Colour(0xFFDC2626);
    colors.backgroundHover = juce::Colour(0xFFEF4444);
    colors.backgroundPressed = juce::Colour(0xFFB91C1C);
    colors.foreground = juce::Colour(0xFFFFFFFF);
    colors.foregroundDisabled = juce::Colour(0xFFFF9999);
    colors.border = juce::Colours::transparentBlack;
    colors.borderHover = juce::Colours::transparentBlack;
    break;

  case ButtonStyle::Success:
    colors.background = juce::Colour(0xFF16A34A);
    colors.backgroundHover = juce::Colour(0xFF22C55E);
    colors.backgroundPressed = juce::Colour(0xFF15803D);
    colors.foreground = juce::Colour(0xFFFFFFFF);
    colors.foregroundDisabled = juce::Colour(0xFF99FF99);
    colors.border = juce::Colours::transparentBlack;
    colors.borderHover = juce::Colours::transparentBlack;
    break;

  case ButtonStyle::Muted:
    colors.background = juce::Colour(0xFF262626);
    colors.backgroundHover = juce::Colour(0xFF363636);
    colors.backgroundPressed = juce::Colour(0xFF464646);
    colors.foreground = juce::Colour(0xFFA3A3A3);
    colors.foregroundDisabled = juce::Colour(0xFF737373);
    colors.border = juce::Colours::transparentBlack;
    colors.borderHover = juce::Colours::transparentBlack;
    break;
  }

  return colors;
}

} // namespace shmui
