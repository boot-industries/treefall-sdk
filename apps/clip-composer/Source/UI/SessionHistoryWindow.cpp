// SPDX-License-Identifier: MIT

#include "SessionHistoryWindow.h"
#include "BuildInfo.h"
#include "ConsoleTheme.h"
#include "DesignTokens.h"

using namespace OCC::Design;

//==============================================================================
// Content
//==============================================================================
SessionHistoryWindow::Content::Content() {
  m_historyDisplay.setMultiLine(true);
  m_historyDisplay.setReturnKeyStartsNewLine(true);
  m_historyDisplay.setReadOnly(true);
  m_historyDisplay.setScrollbarsShown(true);
  m_historyDisplay.setCaretVisible(false);
  m_historyDisplay.setPopupMenuEnabled(true);
  m_historyDisplay.setColour(juce::TextEditor::backgroundColourId, juce::Colour(kBgPrimary));
  m_historyDisplay.setColour(juce::TextEditor::textColourId, juce::Colour(kTextPrimary));
  m_historyDisplay.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
  m_historyDisplay.setFont(OCC::Console::monoFont(12.0f));
  addAndMakeVisible(m_historyDisplay);

  m_clearButton = std::make_unique<ConsoleActionButton>("history-clear", ConsoleActionButton::Variant::Danger);
  m_clearButton->setLabel("CLEAR");
  m_clearButton->onClick = [this]() { m_historyDisplay.clear(); };
  addAndMakeVisible(m_clearButton.get());

  m_copyButton = std::make_unique<ConsoleActionButton>("history-copy", ConsoleActionButton::Variant::Ghost);
  m_copyButton->setLabel("COPY");
  m_copyButton->onClick = [this]() { juce::SystemClipboard::copyTextToClipboard(m_historyDisplay.getText()); };
  addAndMakeVisible(m_copyButton.get());
}

void SessionHistoryWindow::Content::paint(juce::Graphics& g) {
  // Console chassis background
  g.fillAll(juce::Colour(kBgPrimary));

  // Border
  g.setColour(juce::Colour(kBorderDefault));
  g.drawRect(getLocalBounds(), 1);
}

void SessionHistoryWindow::Content::resized() {
  auto area = getLocalBounds().reduced(12);

  // Top bar with buttons
  auto topBar = area.removeFromTop(34);
  m_clearButton->setBounds(topBar.removeFromLeft(70));
  topBar.removeFromLeft(8);
  m_copyButton->setBounds(topBar.removeFromLeft(70));

  area.removeFromTop(8);

  // History display takes remaining space
  m_historyDisplay.setBounds(area);
}

//==============================================================================
// SessionHistoryWindow
//==============================================================================
SessionHistoryWindow::SessionHistoryWindow()
    : DocumentWindow("Session History", juce::Colour(kBgSurface),
                     juce::DocumentWindow::minimiseButton | juce::DocumentWindow::closeButton) {
  setUsingNativeTitleBar(true);
  setResizable(true, true);
  centreWithSize(500, 400);

  m_content = std::make_unique<Content>();
  setContentOwned(m_content.release(), true);

  setVisible(true);
}

SessionHistoryWindow::~SessionHistoryWindow() = default;

void SessionHistoryWindow::closeButtonPressed() {
  setVisible(false); // Just hide the window
}

void SessionHistoryWindow::addHistoryEntry(const juce::String& entry) {
  if (m_content) {
    m_content->m_historyDisplay.setText(entry + "\n" + m_content->m_historyDisplay.getText());
  }
}