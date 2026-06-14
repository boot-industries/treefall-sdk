// SPDX-License-Identifier: MIT
//
// ConsoleInspectorPanel — the 420 px right-side surface visible in Edit / Routing /
// Preferences operator modes (and as a "now playing" summary in Playout).
//
// Operator narratives served:
//   * Playout — read-at-a-glance summary of clips currently playing, plus Stop All /
//     Cue Buss action buttons the operator can hit without leaving the inspector.
//   * Edit — pointer to the Clip Edit modal dialog (full editor lives there).
//   * Routing — 4-row matrix: GROUP / OUTPUT / GAIN / METER / mute-solo. M / S
//     buttons toggle the SDK routing matrix through MainComponent, and reflect
//     the committed mute/solo state by swapping ConsoleActionButton variants.
//   * Preferences — live device/IO/route summary pulled from the audio snapshot.

#pragma once

#include "../UIState/ClipComposerUiSnapshot.h"
#include "ConsoleActionButton.h"
#include <array>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

class ConsoleInspectorPanel : public juce::Component {
public:
  ConsoleInspectorPanel();

  void setSnapshot(const occ::ui::ClipComposerUiSnapshot& snapshot);
  void setOperatorViewMode(occ::ui::OperatorViewMode mode);

  // Wiring for the Playout footer action buttons (real juce::Buttons, not paint).
  std::function<void()> onStopAll;
  std::function<void()> onCueBuss;

  // Wiring for the Routing matrix per-group mute/solo (no-op until routing model exposes it).
  std::function<void(int /*group*/)> onMutePressed;
  std::function<void(int /*group*/)> onSoloPressed;

  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  void drawPlayout(juce::Graphics& g, juce::Rectangle<int> bounds);
  void drawEdit(juce::Graphics& g, juce::Rectangle<int> bounds);
  void drawRouting(juce::Graphics& g, juce::Rectangle<int> bounds);
  void drawPreferences(juce::Graphics& g, juce::Rectangle<int> bounds);
  void drawSectionHeader(juce::Graphics& g, juce::Rectangle<int>& bounds, const juce::String& title,
                         const juce::String& eyebrow);
  void drawRow(juce::Graphics& g, juce::Rectangle<int> row, juce::Colour stripe,
               const juce::String& left, const juce::String& right);

  // Real interactive controls owned by the panel. Visibility is toggled per operator mode.
  std::unique_ptr<ConsoleActionButton> m_playoutStopAllButton;
  std::unique_ptr<ConsoleActionButton> m_playoutCueBussButton;
  std::array<std::unique_ptr<ConsoleActionButton>, 4> m_routingMuteButtons;
  std::array<std::unique_ptr<ConsoleActionButton>, 4> m_routingSoloButtons;

  void layoutPlayoutFooter(juce::Rectangle<int> footer);
  void layoutRoutingButtons(const juce::Rectangle<int>& tableArea);
  void updateChildVisibility();

  occ::ui::ClipComposerUiSnapshot m_snapshot;
  occ::ui::OperatorViewMode m_mode = occ::ui::OperatorViewMode::Playout;
};
