/*
  ==============================================================================

    AboutDialog.h
    Created: 18 Jan 2026
    Author:  Orpheus Clip Composer

    OCC144: About Dialog implementation for macOS standard menu compliance
    OCC149: Updated with Console design language (Design-system alignment)

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ConsoleActionButton.h"

/**
    About dialog showing application information.

    Displays:
    - Application name and logo
    - Version number (from BuildInfo.h)
    - Build date and commit hash
    - Copyright notice
    - Credits
*/
class AboutDialog : public juce::Component {
public:
  AboutDialog();
  ~AboutDialog() override = default;

  void paint(juce::Graphics& g) override;
  void resized() override;

  /** Callback when OK button clicked */
  std::function<void()> onOkClicked;

  /** Get preferred size for the dialog */
  static constexpr int getPreferredWidth() { return 400; }
  static constexpr int getPreferredHeight() { return 320; }

private:
  juce::Label m_titleLabel;
  juce::Label m_versionLabel;
  juce::Label m_buildLabel;
  juce::Label m_copyrightLabel;
  juce::Label m_creditsLabel;
  std::unique_ptr<ConsoleActionButton> m_okButton;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AboutDialog)
};