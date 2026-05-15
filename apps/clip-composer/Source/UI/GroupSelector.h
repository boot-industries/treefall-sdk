// SPDX-License-Identifier: MIT
//
// GroupSelector — 4-button A/B/C/D group selector for the Clip Edit dialog.
// Replaces the prior ComboBox-driven group control. Independent of clip swatch
// colour: this selects the routing channel only.

#pragma once

#include "ConsoleTheme.h"
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

class GroupSelector : public juce::Component {
public:
  GroupSelector() = default;

  void setSelectedGroup(int group) {
    int clamped = juce::jlimit(0, 3, group);
    if (m_selected == clamped)
      return;
    m_selected = clamped;
    repaint();
  }

  int getSelectedGroup() const {
    return m_selected;
  }

  std::function<void(int)> onGroupChanged;

  void paint(juce::Graphics& g) override {
    auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty())
      return;
    constexpr float gap = 4.0f;
    const float w = (bounds.getWidth() - gap * 3.0f) / 4.0f;
    for (int i = 0; i < 4; ++i) {
      auto box = juce::Rectangle<float>(bounds.getX() + i * (w + gap), bounds.getY(), w,
                                        bounds.getHeight());
      OCC::Console::drawGroupButton(g, box, i, i == m_selected);
    }
  }

  void mouseDown(const juce::MouseEvent& e) override {
    auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty())
      return;
    constexpr float gap = 4.0f;
    const float w = (bounds.getWidth() - gap * 3.0f) / 4.0f;
    const float xRel = static_cast<float>(e.x) - bounds.getX();
    const int idx = juce::jlimit(0, 3, static_cast<int>(xRel / (w + gap)));
    setSelectedGroup(idx);
    if (onGroupChanged)
      onGroupChanged(idx);
  }

private:
  int m_selected = 0;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GroupSelector)
};
