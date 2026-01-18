/*
  ==============================================================================

    ColorUtils.h
    Created: Shmui-to-JUCE Audio Visualization Port

    Color utilities for gradient ramps and color manipulation.
    Used by orb and other visualizations for theming.

  ==============================================================================
*/

#pragma once

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace shmui {

/**
 * @brief Color utilities for visualization components.
 *
 * Provides color ramps, gradients, and manipulation functions
 * translated from shmui's shader and styling code.
 */
namespace ColorUtils {

//==============================================================================
// Default Colors from shmui

/** Default orb colors (light theme) */
constexpr uint32_t kOrbColor1Light = 0xFFCADCFC; // #CADCFC
constexpr uint32_t kOrbColor2Light = 0xFFA0B9D1; // #A0B9D1

/** Orb color ramp (black -> color1 -> color2 -> white) */
constexpr uint32_t kRampBlack = 0xFF000000;
constexpr uint32_t kRampWhite = 0xFFFFFFFF;

//==============================================================================
// Color Conversion

/**
 * @brief Convert hex string to Colour.
 *
 * @param hex Hex color string (e.g., "#CADCFC" or "CADCFC")
 * @return JUCE Colour object
 */
inline juce::Colour fromHex(const juce::String& hex) {
  juce::String cleaned = hex.trimCharactersAtStart("#");
  return juce::Colour::fromString("FF" + cleaned);
}

/**
 * @brief Convert Colour to normalized RGB values.
 *
 * @param colour Input colour
 * @param r Output red (0-1)
 * @param g Output green (0-1)
 * @param b Output blue (0-1)
 */
inline void toNormalized(const juce::Colour& colour, float& r, float& g, float& b) {
  r = colour.getFloatRed();
  g = colour.getFloatGreen();
  b = colour.getFloatBlue();
}

/**
 * @brief Create Colour from normalized RGB values.
 *
 * @param r Red (0-1)
 * @param g Green (0-1)
 * @param b Blue (0-1)
 * @param a Alpha (0-1)
 * @return JUCE Colour object
 */
inline juce::Colour fromNormalized(float r, float g, float b, float a = 1.0f) {
  return juce::Colour::fromFloatRGBA(r, g, b, a);
}

//==============================================================================
// Color Ramps

/**
 * @brief Apply a 4-color gradient ramp to a grayscale value.
 *
 * Implements the colorRamp function from orb.tsx fragment shader.
 *
 * @param grayscale Input grayscale value (0-1)
 * @param color1 First color (at 0.0)
 * @param color2 Second color (at 0.33)
 * @param color3 Third color (at 0.66)
 * @param color4 Fourth color (at 1.0)
 * @return Interpolated colour
 */
inline juce::Colour colorRamp(float grayscale, const juce::Colour& color1,
                              const juce::Colour& color2, const juce::Colour& color3,
                              const juce::Colour& color4) {
  grayscale = juce::jlimit(0.0f, 1.0f, grayscale);

  if (grayscale < 0.33f) {
    const float t = grayscale * 3.0f;
    return color1.interpolatedWith(color2, t);
  } else if (grayscale < 0.66f) {
    const float t = (grayscale - 0.33f) * 3.0f;
    return color2.interpolatedWith(color3, t);
  } else {
    const float t = (grayscale - 0.66f) * 3.0f;
    return color3.interpolatedWith(color4, t);
  }
}

/**
 * @brief Apply standard orb color ramp.
 *
 * Uses the default orb gradient (black -> primary -> secondary -> white).
 *
 * @param grayscale Input grayscale value (0-1)
 * @param primaryColor Primary orb color
 * @param secondaryColor Secondary orb color
 * @return Interpolated colour
 */
inline juce::Colour orbColorRamp(float grayscale, const juce::Colour& primaryColor,
                                 const juce::Colour& secondaryColor) {
  return colorRamp(grayscale, juce::Colour(kRampBlack), primaryColor, secondaryColor,
                   juce::Colour(kRampWhite));
}

//==============================================================================
// Gradient Creation

/**
 * @brief Create a linear gradient for edge fading.
 *
 * Used by waveform for edge fade effect.
 *
 * @param bounds Rectangle bounds
 * @param fadeWidth Width of fade at each edge
 * @param horizontal True for horizontal, false for vertical
 * @return ColourGradient for the fade effect
 */
inline juce::ColourGradient createEdgeFadeGradient(const juce::Rectangle<float>& bounds,
                                                   float fadeWidth, bool horizontal = true) {
  juce::ColourGradient gradient;

  if (horizontal) {
    gradient = juce::ColourGradient::horizontal(juce::Colours::white, bounds.getX(),
                                                juce::Colours::white, bounds.getRight());

    const float fadePercent = std::min(0.2f, fadeWidth / bounds.getWidth());

    gradient.addColour(0.0, juce::Colours::white);
    gradient.addColour(fadePercent, juce::Colours::transparentWhite);
    gradient.addColour(1.0f - fadePercent, juce::Colours::transparentWhite);
    gradient.addColour(1.0, juce::Colours::white);
  } else {
    gradient = juce::ColourGradient::vertical(juce::Colours::white, bounds.getY(),
                                              juce::Colours::white, bounds.getBottom());

    const float fadePercent = std::min(0.2f, fadeWidth / bounds.getHeight());

    gradient.addColour(0.0, juce::Colours::white);
    gradient.addColour(fadePercent, juce::Colours::transparentWhite);
    gradient.addColour(1.0f - fadePercent, juce::Colours::transparentWhite);
    gradient.addColour(1.0, juce::Colours::white);
  }

  return gradient;
}

/**
 * @brief Create a brightness/VU meter gradient.
 *
 * Green at bottom, yellow in middle, red at top.
 *
 * @param bounds Rectangle bounds
 * @return ColourGradient for VU meter
 */
inline juce::ColourGradient createVUMeterGradient(const juce::Rectangle<float>& bounds) {
  juce::ColourGradient gradient = juce::ColourGradient::vertical(
      juce::Colours::red, bounds.getY(), juce::Colours::green, bounds.getBottom());

  gradient.addColour(0.3, juce::Colours::yellow);

  return gradient;
}

//==============================================================================
// Color Manipulation

/**
 * @brief Lerp between two colours with a factor.
 *
 * @param from Start colour
 * @param to End colour
 * @param factor Interpolation factor (0-1)
 * @return Interpolated colour
 */
inline juce::Colour lerpColour(const juce::Colour& from, const juce::Colour& to, float factor) {
  return from.interpolatedWith(to, juce::jlimit(0.0f, 1.0f, factor));
}

/**
 * @brief Apply brightness scaling to a colour.
 *
 * @param colour Input colour
 * @param brightness Brightness multiplier (1.0 = no change)
 * @return Adjusted colour
 */
inline juce::Colour withBrightness(const juce::Colour& colour, float brightness) {
  return colour.withMultipliedBrightness(brightness);
}

/**
 * @brief Get luminance value for a colour.
 *
 * @param colour Input colour
 * @return Luminance (0-1)
 */
inline float getLuminance(const juce::Colour& colour) {
  // Standard luminance calculation
  return 0.299f * colour.getFloatRed() + 0.587f * colour.getFloatGreen() +
         0.114f * colour.getFloatBlue();
}

/**
 * @brief Invert colour luminance (for dark/light mode toggle).
 *
 * Used by orb shader for uInverted uniform.
 *
 * @param luminance Input luminance (0-1)
 * @param inverted True to invert
 * @return Adjusted luminance
 */
inline float invertLuminance(float luminance, bool inverted) {
  return inverted ? (1.0f - luminance) : luminance;
}

} // namespace ColorUtils

} // namespace shmui
