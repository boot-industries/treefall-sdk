// SPDX-License-Identifier: MIT

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
 * @brief Floating window to display session history and transport events.
 */
class SessionHistoryWindow : public juce::DocumentWindow {
public:
  SessionHistoryWindow();
  ~SessionHistoryWindow() override;

  void closeButtonPressed() override;

  void addHistoryEntry(const juce::String& entry);

private:
  juce::TextEditor m_historyDisplay;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SessionHistoryWindow)
};
