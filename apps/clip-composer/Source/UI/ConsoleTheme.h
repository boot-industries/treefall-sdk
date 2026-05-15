// SPDX-License-Identifier: MIT

#pragma once

#include "../UIState/ClipComposerUiSnapshot.h"
#include "DesignTokens.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace OCC::Console {

namespace Metrics {
static constexpr int kLiveTopStripHeight = 36;
static constexpr int kLiveBottomStripHeight = 52;
static constexpr int kFullChromeHeight = 88;
static constexpr int kFullBottomStripHeight = 52;
static constexpr int kInspectorWidth = 420;
static constexpr int kGridPaddingComfortable = 10;
static constexpr int kGridPaddingDense = 8;
static constexpr int kGridGapComfortable = 6;
static constexpr int kGridGapDense = 4;
static constexpr int kCellRadius = 4;
static constexpr int kControlRadius = 3;
static constexpr int kLiveButtonHeight = 36;
} // namespace Metrics

inline juce::Colour colour(uint32_t argb) {
  return juce::Colour(argb);
}

inline juce::ColourGradient verticalGradient(juce::Colour top, juce::Colour bottom,
                                             juce::Rectangle<float> bounds) {
  return {top, bounds.getX(), bounds.getY(), bottom, bounds.getX(), bounds.getBottom(), false};
}

inline void fillVerticalGradient(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour top,
                                 juce::Colour bottom) {
  g.setGradientFill(verticalGradient(top, bottom, bounds));
  g.fillRect(bounds);
}

inline void drawMatteCap(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour top,
                         juce::Colour bottom, float radius = Metrics::kControlRadius) {
  g.setGradientFill(verticalGradient(top, bottom, bounds));
  g.fillRoundedRectangle(bounds, radius);
  g.setColour(juce::Colours::white.withAlpha(0.10f));
  g.drawLine(bounds.getX() + 1.0f, bounds.getY() + 1.0f, bounds.getRight() - 1.0f,
             bounds.getY() + 1.0f, 1.0f);
  g.setColour(juce::Colours::black.withAlpha(0.55f));
  g.drawRoundedRectangle(bounds, radius, 1.0f);
  g.setColour(juce::Colours::black.withAlpha(0.35f));
  g.drawLine(bounds.getX() + 1.0f, bounds.getBottom() - 1.0f, bounds.getRight() - 1.0f,
             bounds.getBottom() - 1.0f, 1.0f);
}

inline juce::Font consoleFont(float size, int style = juce::Font::plain) {
  return juce::FontOptions("HK Grotesk", size, style);
}

inline juce::Font monoFont(float size, int style = juce::Font::plain) {
  return juce::FontOptions("JetBrains Mono", size, style);
}

// Eyebrow label: small UPPERCASE letter-spaced section header (e.g., "NAME", "GROUP").
inline void drawEyebrow(juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& text,
                        juce::Colour colour = juce::Colour(OCC::Design::kTextSecondary)) {
  g.setColour(colour);
  g.setFont(consoleFont(10.0f, juce::Font::bold));
  // JUCE doesn't expose letter-spacing on Graphics::drawText, so we widen via setHorizontalScale on
  // the AttributedString path. Sufficient to use sentence-cap spacing visually for now.
  g.drawText(text.toUpperCase(), bounds.toNearestInt(), juce::Justification::centredLeft, false);
}

// Inset field: recessed input well used for text editors and time fields in the Console flavor.
inline void
drawInsetField(juce::Graphics& g, juce::Rectangle<float> bounds,
               juce::Colour borderOverride = juce::Colour(OCC::Design::kBorderDefault)) {
  g.setColour(colour(OCC::Design::kBgInset));
  g.fillRoundedRectangle(bounds, 3.0f);
  g.setColour(borderOverride);
  g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);
}

// Chip toggle: amber-tinted when enabled, inset/muted when disabled.
inline void drawChip(juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& label,
                     bool enabled) {
  const auto amber = colour(OCC::Design::kAmber);
  const auto muted = colour(OCC::Design::kTextSecondary);
  const auto cream = colour(OCC::Design::kTextPrimary);

  if (enabled) {
    g.setColour(amber.withAlpha(0.20f));
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(amber);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);
    g.setColour(amber);
  } else {
    drawInsetField(g, bounds);
    g.setColour(muted);
  }

  g.setFont(consoleFont(11.0f, juce::Font::bold));
  g.drawText(label, bounds.toNearestInt(), juce::Justification::centred, false);
  (void)cream;
}

// Action button: matte cap with variant treatment (primary blue, danger coral, amber, ghost).
enum class ActionVariant { Default, Primary, Danger, Amber, Ghost };

inline void drawActionButton(juce::Graphics& g, juce::Rectangle<float> bounds,
                             const juce::String& label, ActionVariant variant, bool down = false,
                             bool hover = false) {
  juce::Colour top, bottom, text;
  switch (variant) {
  case ActionVariant::Primary:
    top = colour(OCC::Design::kNeveBlue);
    bottom = colour(OCC::Design::kNeveBlueDark);
    text = juce::Colours::white;
    break;
  case ActionVariant::Danger:
    top = colour(OCC::Design::kConsoleCoral);
    bottom = colour(OCC::Design::kConsoleCoral).darker(0.35f);
    text = juce::Colours::white;
    break;
  case ActionVariant::Amber:
    top = colour(OCC::Design::kAmber);
    bottom = colour(OCC::Design::kAmber).darker(0.30f);
    text = juce::Colour(0xff1a1410);
    break;
  case ActionVariant::Ghost:
    top = juce::Colours::transparentBlack;
    bottom = juce::Colours::transparentBlack;
    text = colour(OCC::Design::kTextSecondary);
    break;
  case ActionVariant::Default:
  default:
    top = juce::Colour(0xff383d40);
    bottom = juce::Colour(0xff2a2e31);
    text = colour(OCC::Design::kTextPrimary);
    break;
  }

  if (variant != ActionVariant::Ghost) {
    auto adjustedTop = down ? top.darker(0.10f) : (hover ? top.brighter(0.04f) : top);
    auto adjustedBottom = down ? bottom.darker(0.10f) : bottom;
    drawMatteCap(g, bounds, adjustedTop, adjustedBottom, 2.5f);
  } else if (hover) {
    g.setColour(juce::Colour(OCC::Design::kBorderDefault).withAlpha(0.35f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 2.5f, 1.0f);
  }

  g.setColour(text);
  g.setFont(consoleFont(11.0f, juce::Font::bold));
  g.drawText(label.toUpperCase(), bounds.toNearestInt(), juce::Justification::centred, false);
}

// Group selector (A/B/C/D channel buttons). Selected fills with that group's signature colour.
inline juce::Colour groupColour(int index) {
  switch (index) {
  case 0:
    return colour(OCC::Design::kGroupBlue);
  case 1:
    return colour(OCC::Design::kGroupGreen);
  case 2:
    return colour(OCC::Design::kGroupOrange);
  case 3:
    return colour(OCC::Design::kGroupRed);
  default:
    return colour(OCC::Design::kBorderDefault);
  }
}

inline juce::String groupLabel(int index) {
  static const char* labels[] = {"A", "B", "C", "D"};
  return (index >= 0 && index < 4) ? juce::String(labels[index]) : juce::String();
}

inline void drawGroupButton(juce::Graphics& g, juce::Rectangle<float> bounds, int groupIndex,
                            bool selected) {
  // Design-kit channel-strip routing: selected = fully lit with the group's
  // signature colour + cream halo (you can see this is the active routing).
  // Unselected = backlit preview of the group colour so the operator can read
  // all four channels at once without selecting them.
  const auto gc = groupColour(groupIndex);
  if (selected) {
    // Saturated fill, top→bottom gradient for tactile depth.
    drawMatteCap(g, bounds, gc.brighter(0.10f), gc.darker(0.20f), 3.0f);
    // Cream halo around the selected button — design-kit "lit channel" signal.
    g.setColour(juce::Colour(OCC::Design::kTextPrimary).withAlpha(0.95f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.5f);
    g.setColour(juce::Colours::white);
  } else {
    // Backlit preview — the group colour shines through the inset chassis at
    // low intensity so the operator sees what each channel represents.
    auto base = juce::Colour(OCC::Design::kBgInset).interpolatedWith(gc, 0.30f);
    g.setColour(base);
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(gc.withAlpha(0.55f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);
    g.setColour(colour(OCC::Design::kTextPrimary).withAlpha(0.85f));
  }
  g.setFont(consoleFont(13.0f, juce::Font::bold));
  g.drawText(groupLabel(groupIndex), bounds.toNearestInt(), juce::Justification::centred, false);
}

inline juce::String operatorModeLabel(occ::ui::OperatorViewMode mode) {
  switch (mode) {
  case occ::ui::OperatorViewMode::Playout:
    return "PLAYOUT";
  case occ::ui::OperatorViewMode::Edit:
    return "EDIT";
  case occ::ui::OperatorViewMode::Routing:
    return "ROUTING";
  case occ::ui::OperatorViewMode::Preferences:
    return "PREFERENCES";
  }
  return {};
}

} // namespace OCC::Console
