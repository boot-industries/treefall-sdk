// SPDX-License-Identifier: MIT

#pragma once

#include "../UIState/ClipComposerUiSnapshot.h"
#include <juce_gui_basics/juce_gui_basics.h>

class ConsoleInspectorPanel : public juce::Component {
public:
  void setSnapshot(const occ::ui::ClipComposerUiSnapshot& snapshot);
  void setOperatorViewMode(occ::ui::OperatorViewMode mode);

  void paint(juce::Graphics& g) override;

private:
  void drawPlayout(juce::Graphics& g, juce::Rectangle<int> bounds);
  void drawEdit(juce::Graphics& g, juce::Rectangle<int> bounds);
  void drawRouting(juce::Graphics& g, juce::Rectangle<int> bounds);
  void drawPreferences(juce::Graphics& g, juce::Rectangle<int> bounds);
  void drawSectionHeader(juce::Graphics& g, juce::Rectangle<int>& bounds, const juce::String& title,
                         const juce::String& eyebrow);
  void drawRow(juce::Graphics& g, juce::Rectangle<int> row, juce::Colour stripe,
               const juce::String& left, const juce::String& right);

  occ::ui::ClipComposerUiSnapshot m_snapshot;
  occ::ui::OperatorViewMode m_mode = occ::ui::OperatorViewMode::Playout;
};
