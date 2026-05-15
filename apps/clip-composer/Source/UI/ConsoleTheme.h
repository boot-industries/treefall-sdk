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
