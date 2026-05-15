// SPDX-License-Identifier: MIT
//
// ConsoleActionButton — real juce::Button that paints via the Console matte-cap
// primitive. Replaces painted-but-not-clickable controls in the inspector and
// the Clip Edit dialog. Keyboard focusable, hover/down aware, accessible.
//
// Operator intent: when something on the chassis looks pressable, the operator
// must be able to actually press it (mouse, keyboard, screen reader). Painted
// graphics dressed up as buttons are a UX trap.

#pragma once

#include "ConsoleTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>

class ConsoleActionButton : public juce::Button {
public:
  using Variant = OCC::Console::ActionVariant;

  ConsoleActionButton(const juce::String& name, Variant variant = Variant::Default)
      : juce::Button(name), m_variant(variant) {
    setWantsKeyboardFocus(true);
  }

  void setVariant(Variant variant) {
    if (m_variant == variant)
      return;
    m_variant = variant;
    repaint();
  }

  Variant getVariant() const noexcept {
    return m_variant;
  }

  void setLabel(const juce::String& label) {
    if (m_labelOverride == label)
      return;
    m_labelOverride = label;
    repaint();
  }

private:
  void paintButton(juce::Graphics& g, bool shouldDrawAsHighlighted,
                   bool shouldDrawAsDown) override {
    const auto label = m_labelOverride.isNotEmpty() ? m_labelOverride : getButtonText();
    OCC::Console::drawActionButton(g, getLocalBounds().toFloat(), label, m_variant,
                                   shouldDrawAsDown, shouldDrawAsHighlighted);

    // Keyboard focus halo — Ghost variant has no fill, so the focus ring is its only affordance.
    if (hasKeyboardFocus(false)) {
      g.setColour(juce::Colour(OCC::Design::kNeveBlue).withAlpha(0.55f));
      g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.5f), 2.5f, 1.5f);
    }
  }

  Variant m_variant;
  juce::String m_labelOverride;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConsoleActionButton)
};
