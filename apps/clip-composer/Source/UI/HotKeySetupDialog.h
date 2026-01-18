/*
  ==============================================================================

    HotKeySetupDialog.h
    Created: 12 Jan 2026
    Author:  Orpheus Clip Composer

    Sprint 10: HotKey Configuration Dialog (OCC116)

  ==============================================================================
*/

#pragma once

#include "../Core/HotKeyManager.h"
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

/**
    Dialog for configuring hotkey scope and multi-button action.

    Features:
    - Global/Paged scope selection (radio buttons)
    - Ganged/Overlapped action selection (radio buttons)
    - Hint labels explaining each option
*/
class HotKeySetupDialog : public juce::Component {
public:
  HotKeySetupDialog(orpheus::HotKeyManager* hotKeyManager);
  ~HotKeySetupDialog() override = default;

  void paint(juce::Graphics& g) override;
  void resized() override;

  std::function<void()> onOkClicked;
  std::function<void()> onCancelClicked;

private:
  void updateHints();
  void applySettings();

  orpheus::HotKeyManager* m_hotKeyManager;

  // Title
  juce::Label m_titleLabel;

  // Scope section
  juce::Label m_scopeLabel;
  juce::ToggleButton m_globalScopeButton;
  juce::ToggleButton m_pagedScopeButton;
  juce::Label m_scopeHintLabel;

  // Action section
  juce::Label m_actionLabel;
  juce::ToggleButton m_gangedActionButton;
  juce::ToggleButton m_overlappedActionButton;
  juce::Label m_actionHintLabel;

  // Buttons
  juce::TextButton m_okButton;
  juce::TextButton m_cancelButton;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HotKeySetupDialog)
};
