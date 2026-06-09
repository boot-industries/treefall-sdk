// SPDX-License-Identifier: MIT

#pragma once

#include "ConsoleActionButton.h"
#include "DesignTokens.h"
#include <juce_gui_basics/juce_gui_basics.h>

/**
 * @brief Floating window to display session history and transport events.
 * Updated with Console design language (OCC149).
 */
class SessionHistoryWindow : public juce::DocumentWindow {
public:
  SessionHistoryWindow();
  ~SessionHistoryWindow() override;

  void closeButtonPressed() override;

  void addHistoryEntry(const juce::String& entry);

private:
  class Content : public juce::Component {
  public:
    Content();
    void paint(juce::Graphics& g) override;
    void resized() override;

    juce::TextEditor m_historyDisplay;
    std::unique_ptr<ConsoleActionButton> m_clearButton;
    std::unique_ptr<ConsoleActionButton> m_copyButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Content)
  };

  std::unique_ptr<Content> m_content;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SessionHistoryWindow)
};