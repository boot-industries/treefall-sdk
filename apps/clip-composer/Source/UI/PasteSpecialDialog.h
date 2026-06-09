/*
  ==============================================================================

    PasteSpecialDialog.h
    Created: 12 Jan 2026
    Author:  Orpheus Clip Composer

    Sprint 17: Paste Special Dialog (OCC117)
    OCC149: Updated with Console design language

  ==============================================================================
*/

#pragma once

#include "ConsoleActionButton.h"
#include "../Core/ClipCommands.h"
#include "../Session/SessionManager.h"
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

/**
    Dialog for selective parameter pasting (Paste Special).

    Features:
    - Parameter selection checkboxes (grouped by category)
    - Gain Absolute vs Relative options
    - AutoFill for MIDI notes
    - Paste scope selection (Individual, Global, Current Page, Range)
    - Confirmation before apply
*/
class PasteSpecialDialog : public juce::Component {
public:
  PasteSpecialDialog(SessionManager* sessionManager, const SessionManager::ClipData& sourceClip,
                     int currentTab);
  ~PasteSpecialDialog() override = default;

  void paint(juce::Graphics& g) override;
  void resized() override;

  /** Get the configured options */
  orpheus::PasteSpecialOptions getOptions() const;

  /** Get the target global indices based on scope selection */
  std::vector<int> getTargetIndices() const;

  std::function<void()> onOkClicked;
  std::function<void()> onCancelClicked;

private:
  void updateControlStates();
  void clearAllOptions();

  SessionManager* m_sessionManager;
  SessionManager::ClipData m_sourceClip;
  int m_currentTab;

  // Title
  juce::Label m_titleLabel;
  juce::Label m_sourceLabel;

  // Levels section
  juce::Label m_levelsLabel;
  juce::ToggleButton m_gainAbsoluteCheckbox;
  juce::ToggleButton m_gainRelativeCheckbox;
  juce::Slider m_gainRelativeSlider;
  juce::Label m_gainRelativeValueLabel;

  // Fades section
  juce::Label m_fadesLabel;
  juce::ToggleButton m_fadeInCheckbox;
  juce::ToggleButton m_fadeInCurveCheckbox;
  juce::ToggleButton m_fadeOutCheckbox;
  juce::ToggleButton m_fadeOutCurveCheckbox;

  // Misc section
  juce::Label m_miscLabel;
  juce::ToggleButton m_colorCheckbox;
  juce::ToggleButton m_clipGroupCheckbox;
  juce::ToggleButton m_loopCheckbox;
  juce::ToggleButton m_stopOthersCheckbox;

  // Scope section
  juce::Label m_scopeLabel;
  juce::ToggleButton m_scopeCurrentPageRadio;
  juce::ToggleButton m_scopeAllPagesRadio;
  juce::ToggleButton m_scopeRangeRadio;
  juce::TextEditor m_rangeStartEditor;
  juce::Label m_rangeDashLabel;
  juce::TextEditor m_rangeEndEditor;

  // Buttons
  std::unique_ptr<ConsoleActionButton> m_clearAllButton;
  std::unique_ptr<ConsoleActionButton> m_okButton;
  std::unique_ptr<ConsoleActionButton> m_cancelButton;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PasteSpecialDialog)
};