// SPDX-License-Identifier: MIT

#include "SessionHistoryWindow.h"

//==============================================================================
SessionHistoryWindow::SessionHistoryWindow()
    : DocumentWindow("Session History", juce::Colours::darkgrey,
                     juce::DocumentWindow::minimiseButton | juce::DocumentWindow::closeButton) {
  setUsingNativeTitleBar(true);
  setResizable(true, true);
  centreWithSize(500, 400);

  m_historyDisplay.setMultiLine(true);
  m_historyDisplay.setReturnKeyStartsNewLine(true);
  m_historyDisplay.setReadOnly(true);
  m_historyDisplay.setScrollbarsShown(true);
  m_historyDisplay.setCaretVisible(false);
  m_historyDisplay.setPopupMenuEnabled(true);
  m_historyDisplay.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
  m_historyDisplay.setColour(juce::TextEditor::textColourId, juce::Colours::lightgrey);
  m_historyDisplay.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
  m_historyDisplay.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::plain)));

  setContentOwned(&m_historyDisplay, false);

  setVisible(true);
}

SessionHistoryWindow::~SessionHistoryWindow() {}

void SessionHistoryWindow::closeButtonPressed() {
  setVisible(false); // Just hide the window
}

void SessionHistoryWindow::addHistoryEntry(const juce::String& entry) {
  // Prepend new entry to the top, so latest event is always visible
  m_historyDisplay.setText(entry + "\n" + m_historyDisplay.getText());
}
