// SPDX-License-Identifier: MIT
//
// ConsoleChipButton — real juce::Button that paints via the Console drawChip
// primitive. Used for the Clip Edit dialog's FLAGS row (Loop / Fade In / Fade
// Out / Stop Others) and any future binary-toggle chip surfaces.
//
// Operator intent: a chip-style toggle reads at a glance — amber-tinted when
// enabled, inset/muted when disabled — and remains a real keyboard-focusable
// component. Painted graphics dressed up as toggles are a UX trap.

#pragma once

#include "ConsoleTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>

class ConsoleChipButton : public juce::Button {
public:
  ConsoleChipButton(const juce::String& name, const juce::String& label)
      : juce::Button(name), m_label(label) {
    setClickingTogglesState(true);
    setWantsKeyboardFocus(true);
  }

  void setLabel(const juce::String& label) {
    if (m_label == label)
      return;
    m_label = label;
    repaint();
  }

private:
  void paintButton(juce::Graphics& g, bool shouldDrawAsHighlighted,
                   bool shouldDrawAsDown) override {
    juce::ignoreUnused(shouldDrawAsDown);
    OCC::Console::drawChip(g, getLocalBounds().toFloat(), m_label, getToggleState());

    if (shouldDrawAsHighlighted && !getToggleState()) {
      g.setColour(juce::Colour(OCC::Design::kBorderHighlight).withAlpha(0.35f));
      g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 3.0f, 1.0f);
    }
    if (hasKeyboardFocus(false)) {
      g.setColour(juce::Colour(OCC::Design::kNeveBlue).withAlpha(0.55f));
      g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.5f), 3.0f, 1.5f);
    }
  }

  juce::String m_label;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConsoleChipButton)
};
