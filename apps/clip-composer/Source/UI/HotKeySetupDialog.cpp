/*
  ==============================================================================

    HotKeySetupDialog.cpp
    Created: 12 Jan 2026
    Author:  Orpheus Clip Composer

    Sprint 10: HotKey Configuration Dialog (OCC116)
    OCC149: Updated with Console design language

  ==============================================================================
*/

#include "HotKeySetupDialog.h"
#include "ConsoleTheme.h"
#include "DesignTokens.h"

using namespace OCC::Design;

HotKeySetupDialog::HotKeySetupDialog(orpheus::HotKeyManager* hotKeyManager)
    : m_hotKeyManager(hotKeyManager) {
  setSize(450, 350);

  // Title
  addAndMakeVisible(m_titleLabel);
  m_titleLabel.setText("HotKey Configuration", juce::dontSendNotification);
  m_titleLabel.setFont(OCC::Console::consoleFont(18.0f, juce::Font::bold));
  m_titleLabel.setJustificationType(juce::Justification::centred);
  m_titleLabel.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));

  // Scope section
  addAndMakeVisible(m_scopeLabel);
  m_scopeLabel.setText("Define scope of HotKeys:", juce::dontSendNotification);
  m_scopeLabel.setFont(OCC::Console::consoleFont(14.0f, juce::Font::bold));
  m_scopeLabel.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));

  addAndMakeVisible(m_globalScopeButton);
  m_globalScopeButton.setButtonText("Global");
  m_globalScopeButton.setRadioGroupId(1);
  m_globalScopeButton.onClick = [this]() { updateHints(); };
  m_globalScopeButton.setColour(juce::ToggleButton::textColourId, juce::Colour(kTextPrimary));

  addAndMakeVisible(m_pagedScopeButton);
  m_pagedScopeButton.setButtonText("Paged");
  m_pagedScopeButton.setRadioGroupId(1);
  m_pagedScopeButton.onClick = [this]() { updateHints(); };
  m_pagedScopeButton.setColour(juce::ToggleButton::textColourId, juce::Colour(kTextPrimary));

  addAndMakeVisible(m_scopeHintLabel);
  m_scopeHintLabel.setJustificationType(juce::Justification::centredLeft);
  m_scopeHintLabel.setColour(juce::Label::textColourId, juce::Colour(kTextMuted));

  // Set current scope
  if (m_hotKeyManager && m_hotKeyManager->getScope() == orpheus::HotKeyManager::Scope::Global) {
    m_globalScopeButton.setToggleState(true, juce::dontSendNotification);
  } else {
    m_pagedScopeButton.setToggleState(true, juce::dontSendNotification);
  }

  // Action section
  addAndMakeVisible(m_actionLabel);
  m_actionLabel.setText("Define action when multiple buttons have same HotKey:",
                        juce::dontSendNotification);
  m_actionLabel.setFont(OCC::Console::consoleFont(14.0f, juce::Font::bold));
  m_actionLabel.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));

  addAndMakeVisible(m_gangedActionButton);
  m_gangedActionButton.setButtonText("Ganged");
  m_gangedActionButton.setRadioGroupId(2);
  m_gangedActionButton.onClick = [this]() { updateHints(); };
  m_gangedActionButton.setColour(juce::ToggleButton::textColourId, juce::Colour(kTextPrimary));

  addAndMakeVisible(m_overlappedActionButton);
  m_overlappedActionButton.setButtonText("Overlapped");
  m_overlappedActionButton.setRadioGroupId(2);
  m_overlappedActionButton.onClick = [this]() { updateHints(); };
  m_overlappedActionButton.setColour(juce::ToggleButton::textColourId, juce::Colour(kTextPrimary));

  addAndMakeVisible(m_actionHintLabel);
  m_actionHintLabel.setJustificationType(juce::Justification::centredLeft);
  m_actionHintLabel.setColour(juce::Label::textColourId, juce::Colour(kTextMuted));

  // Set current action
  if (m_hotKeyManager && m_hotKeyManager->getMultiButtonAction() ==
                          orpheus::HotKeyManager::MultiButtonAction::Overlapped) {
    m_overlappedActionButton.setToggleState(true, juce::dontSendNotification);
  } else {
    m_gangedActionButton.setToggleState(true, juce::dontSendNotification);
  }

  // Buttons - Primary OK, Default Cancel
  m_okButton = std::make_unique<ConsoleActionButton>("hotkey-ok", ConsoleActionButton::Variant::Primary);
  m_okButton->setLabel("OK");
  m_okButton->onClick = [this]() {
    applySettings();
    if (onOkClicked)
      onOkClicked();
  };
  addAndMakeVisible(m_okButton.get());

  m_cancelButton = std::make_unique<ConsoleActionButton>("hotkey-cancel", ConsoleActionButton::Variant::Default);
  m_cancelButton->setLabel("CANCEL");
  m_cancelButton->onClick = [this]() {
    if (onCancelClicked)
      onCancelClicked();
  };
  addAndMakeVisible(m_cancelButton.get());

  // Update hint labels
  updateHints();
}

void HotKeySetupDialog::paint(juce::Graphics& g) {
  // Console chassis background
  g.fillAll(juce::Colour(kBgSurface));

  // Title bar - eyebrow + title
  auto titleBar = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(getWidth()), 44.0f);
  g.setColour(juce::Colour(kBgComponent));
  g.fillRect(titleBar);
  g.setColour(juce::Colour(kBorderDefault));
  g.drawHorizontalLine(44, 0.0f, static_cast<float>(getWidth()));

  // Eyebrow
  g.setColour(juce::Colour(kTextSecondary));
  g.setFont(OCC::Console::monoFont(10.0f, juce::Font::bold));
  g.drawText("SETUP", 20, 6, 200, 14, juce::Justification::centredLeft, false);

  // Bold title
  g.setColour(juce::Colour(kTextPrimary));
  g.setFont(OCC::Console::consoleFont(18.0f, juce::Font::bold));
  g.drawText("HotKey Configuration", 20, 20, getWidth() - 40, 22, juce::Justification::centredLeft, false);
}

void HotKeySetupDialog::resized() {
  auto area = getLocalBounds().reduced(15);

  // Title
  m_titleLabel.setBounds(area.removeFromTop(35));
  area.removeFromTop(15); // Spacing

  // Scope section
  m_scopeLabel.setBounds(area.removeFromTop(25));

  auto scopeButtonArea = area.removeFromTop(30);
  m_globalScopeButton.setBounds(scopeButtonArea.removeFromLeft(100));
  scopeButtonArea.removeFromLeft(20);
  m_pagedScopeButton.setBounds(scopeButtonArea.removeFromLeft(100));

  m_scopeHintLabel.setBounds(area.removeFromTop(50));

  area.removeFromTop(15); // Spacing

  // Action section
  m_actionLabel.setBounds(area.removeFromTop(25));

  auto actionButtonArea = area.removeFromTop(30);
  m_gangedActionButton.setBounds(actionButtonArea.removeFromLeft(100));
  actionButtonArea.removeFromLeft(20);
  m_overlappedActionButton.setBounds(actionButtonArea.removeFromLeft(100));

  m_actionHintLabel.setBounds(area.removeFromTop(50));

  // Buttons at bottom
  auto buttonArea = getLocalBounds().reduced(15).removeFromBottom(35);
  m_cancelButton->setBounds(buttonArea.removeFromRight(80));
  buttonArea.removeFromRight(10);
  m_okButton->setBounds(buttonArea.removeFromRight(80));
}

void HotKeySetupDialog::updateHints() {
  // Update scope hint
  if (m_pagedScopeButton.getToggleState()) {
    m_scopeHintLabel.setText("HotKeys are only enabled on the currently selected page.\n"
                             "Ganged/Overlapped action applies to selected page only.",
                             juce::dontSendNotification);
  } else {
    m_scopeHintLabel.setText("HotKeys are enabled globally across all pages.",
                             juce::dontSendNotification);
  }

  // Update action hint
  if (m_gangedActionButton.getToggleState()) {
    m_actionHintLabel.setText("All buttons with the same HotKey will play simultaneously\n"
                              "when the HotKey is pressed.",
                              juce::dontSendNotification);
  } else {
    m_actionHintLabel.setText("Buttons play in sequence, starting with the first that\n"
                              "is not currently playing.",
                              juce::dontSendNotification);
  }
}

void HotKeySetupDialog::applySettings() {
  if (m_hotKeyManager) {
    // Apply scope
    if (m_globalScopeButton.getToggleState()) {
      m_hotKeyManager->setScope(orpheus::HotKeyManager::Scope::Global);
    } else {
      m_hotKeyManager->setScope(orpheus::HotKeyManager::Scope::Paged);
    }

    // Apply action
    if (m_gangedActionButton.getToggleState()) {
      m_hotKeyManager->setMultiButtonAction(orpheus::HotKeyManager::MultiButtonAction::Ganged);
    } else {
      m_hotKeyManager->setMultiButtonAction(orpheus::HotKeyManager::MultiButtonAction::Overlapped);
    }
  }
}