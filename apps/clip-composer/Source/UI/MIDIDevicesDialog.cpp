/*
  ==============================================================================

    MIDIDevicesDialog.cpp
    Created: 12 Jan 2026
    Author:  Orpheus Clip Composer

    Sprint 11: MIDI Device Configuration Dialog (OCC116)
    OCC149: Updated with Console design language

  ==============================================================================
*/

#include "MIDIDevicesDialog.h"
#include "ConsoleTheme.h"
#include "DesignTokens.h"

using namespace OCC::Design;

MIDIDevicesDialog::MIDIDevicesDialog(orpheus::MIDIDeviceManager* midiManager)
    : m_midiManager(midiManager) {
  setSize(500, 500);

  // Title
  addAndMakeVisible(m_titleLabel);
  m_titleLabel.setText("MIDI Device Configuration", juce::dontSendNotification);
  m_titleLabel.setFont(OCC::Console::consoleFont(18.0f, juce::Font::bold));
  m_titleLabel.setJustificationType(juce::Justification::centred);
  m_titleLabel.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));

  // MIDI In section
  addAndMakeVisible(m_midiInLabel);
  m_midiInLabel.setText("MIDI Input Devices:", juce::dontSendNotification);
  m_midiInLabel.setFont(OCC::Console::consoleFont(14.0f, juce::Font::bold));
  m_midiInLabel.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));

  addAndMakeVisible(m_midiInList);
  m_midiInList.setModel(this);
  m_midiInList.setRowHeight(24);
  m_midiInList.setColour(juce::ListBox::backgroundColourId, juce::Colour(kBgComponent));
  m_midiInList.setColour(juce::ListBox::outlineColourId, juce::Colour(kBorderDefault));

  // MIDI Out section
  addAndMakeVisible(m_midiOutLabel);
  m_midiOutLabel.setText("MIDI Output Devices:", juce::dontSendNotification);
  m_midiOutLabel.setFont(OCC::Console::consoleFont(14.0f, juce::Font::bold));
  m_midiOutLabel.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));

  addAndMakeVisible(m_midiOutList);
  m_midiOutList.setRowHeight(24);
  m_midiOutList.setColour(juce::ListBox::backgroundColourId, juce::Colour(kBgComponent));
  m_midiOutList.setColour(juce::ListBox::outlineColourId, juce::Colour(kBorderDefault));

  // Scope section
  addAndMakeVisible(m_scopeLabel);
  m_scopeLabel.setText("MIDI Note Scope:", juce::dontSendNotification);
  m_scopeLabel.setFont(OCC::Console::consoleFont(14.0f, juce::Font::bold));
  m_scopeLabel.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));

  addAndMakeVisible(m_globalScopeButton);
  m_globalScopeButton.setButtonText("Global");
  m_globalScopeButton.setRadioGroupId(1);
  m_globalScopeButton.setColour(juce::ToggleButton::textColourId, juce::Colour(kTextPrimary));

  addAndMakeVisible(m_pagedScopeButton);
  m_pagedScopeButton.setButtonText("Paged");
  m_pagedScopeButton.setRadioGroupId(1);
  m_pagedScopeButton.setColour(juce::ToggleButton::textColourId, juce::Colour(kTextPrimary));

  // Set current scope
  if (m_midiManager && m_midiManager->getScope() == orpheus::MIDIDeviceManager::Scope::Global) {
    m_globalScopeButton.setToggleState(true, juce::dontSendNotification);
  } else {
    m_pagedScopeButton.setToggleState(true, juce::dontSendNotification);
  }

  // Action section
  addAndMakeVisible(m_actionLabel);
  m_actionLabel.setText("Multi-Note Action:", juce::dontSendNotification);
  m_actionLabel.setFont(OCC::Console::consoleFont(14.0f, juce::Font::bold));
  m_actionLabel.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));

  addAndMakeVisible(m_gangedActionButton);
  m_gangedActionButton.setButtonText("Ganged");
  m_gangedActionButton.setRadioGroupId(2);
  m_gangedActionButton.setColour(juce::ToggleButton::textColourId, juce::Colour(kTextPrimary));

  addAndMakeVisible(m_overlappedActionButton);
  m_overlappedActionButton.setButtonText("Overlapped");
  m_overlappedActionButton.setRadioGroupId(2);
  m_overlappedActionButton.setColour(juce::ToggleButton::textColourId, juce::Colour(kTextPrimary));

  // Set current action
  if (m_midiManager && m_midiManager->getMultiNoteAction() ==
                       orpheus::MIDIDeviceManager::MultiNoteAction::Overlapped) {
    m_overlappedActionButton.setToggleState(true, juce::dontSendNotification);
  } else {
    m_gangedActionButton.setToggleState(true, juce::dontSendNotification);
  }

  // All Notes Off checkbox
  addAndMakeVisible(m_sendAllNotesOffCheckbox);
  m_sendAllNotesOffCheckbox.setButtonText("Send 'All Notes Off' on Panic");
  m_sendAllNotesOffCheckbox.setColour(juce::ToggleButton::textColourId, juce::Colour(kTextPrimary));
  if (m_midiManager) {
    m_sendAllNotesOffCheckbox.setToggleState(m_midiManager->getSendAllNotesOffOnPanic(),
                                             juce::dontSendNotification);
  }

  // Buttons - ConsoleActionButton
  m_monitorButton = std::make_unique<ConsoleActionButton>("midi-monitor", ConsoleActionButton::Variant::Ghost);
  m_monitorButton->setLabel("MONITOR...");
  m_monitorButton->onClick = [this]() {
    if (onMonitorClicked)
      onMonitorClicked();
  };
  addAndMakeVisible(m_monitorButton.get());

  m_refreshButton = std::make_unique<ConsoleActionButton>("midi-refresh", ConsoleActionButton::Variant::Default);
  m_refreshButton->setLabel("REFRESH");
  m_refreshButton->onClick = [this]() { refreshDeviceLists(); };
  addAndMakeVisible(m_refreshButton.get());

  m_okButton = std::make_unique<ConsoleActionButton>("midi-ok", ConsoleActionButton::Variant::Primary);
  m_okButton->setLabel("OK");
  m_okButton->onClick = [this]() {
    applySettings();
    if (onOkClicked)
      onOkClicked();
  };
  addAndMakeVisible(m_okButton.get());

  m_cancelButton = std::make_unique<ConsoleActionButton>("midi-cancel", ConsoleActionButton::Variant::Default);
  m_cancelButton->setLabel("CANCEL");
  m_cancelButton->onClick = [this]() {
    if (onCancelClicked)
      onCancelClicked();
  };
  addAndMakeVisible(m_cancelButton.get());

  // Initial device list population
  refreshDeviceLists();
}

void MIDIDevicesDialog::paint(juce::Graphics& g) {
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
  g.drawText("MIDI Device Configuration", 20, 20, getWidth() - 40, 22, juce::Justification::centredLeft, false);
}

void MIDIDevicesDialog::resized() {
  auto area = getLocalBounds().reduced(15);

  // Title
  m_titleLabel.setBounds(area.removeFromTop(35));
  area.removeFromTop(10);

  // MIDI In section
  m_midiInLabel.setBounds(area.removeFromTop(25));
  m_midiInList.setBounds(area.removeFromTop(100));
  area.removeFromTop(15);

  // MIDI Out section
  m_midiOutLabel.setBounds(area.removeFromTop(25));
  m_midiOutList.setBounds(area.removeFromTop(100));
  area.removeFromTop(15);

  // Scope row
  auto scopeRow = area.removeFromTop(25);
  m_scopeLabel.setBounds(scopeRow.removeFromLeft(120));
  m_globalScopeButton.setBounds(scopeRow.removeFromLeft(80));
  m_pagedScopeButton.setBounds(scopeRow.removeFromLeft(80));
  area.removeFromTop(5);

  // Action row
  auto actionRow = area.removeFromTop(25);
  m_actionLabel.setBounds(actionRow.removeFromLeft(120));
  m_gangedActionButton.setBounds(actionRow.removeFromLeft(80));
  m_overlappedActionButton.setBounds(actionRow.removeFromLeft(100));
  area.removeFromTop(10);

  // All Notes Off checkbox
  m_sendAllNotesOffCheckbox.setBounds(area.removeFromTop(25));

  // Buttons at bottom
  auto buttonArea = getLocalBounds().reduced(15).removeFromBottom(35);
  m_cancelButton->setBounds(buttonArea.removeFromRight(80));
  buttonArea.removeFromRight(10);
  m_okButton->setBounds(buttonArea.removeFromRight(80));
  buttonArea.removeFromRight(10);
  m_refreshButton->setBounds(buttonArea.removeFromRight(80));
  buttonArea.removeFromRight(10);
  m_monitorButton->setBounds(buttonArea.removeFromRight(80));
}

int MIDIDevicesDialog::getNumRows() {
  return m_showingInputList ? static_cast<int>(m_midiInDevices.size())
                            : static_cast<int>(m_midiOutDevices.size());
}

void MIDIDevicesDialog::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height,
                                         bool rowIsSelected) {
  auto& devices = m_showingInputList ? m_midiInDevices : m_midiOutDevices;

  if (rowNumber < 0 || rowNumber >= devices.size()) {
    return;
  }

  auto deviceName = devices[rowNumber];

  // Check if device is enabled
  bool isEnabled = false;
  if (m_midiManager) {
    if (m_showingInputList) {
      isEnabled = m_midiManager->isMidiInDeviceEnabled(deviceName);
    } else {
      isEnabled = m_midiManager->isMidiOutDeviceEnabled(deviceName);
    }
  }

  // Background
  if (rowIsSelected) {
    g.fillAll(juce::Colour(kNeveBlue).withAlpha(0.3f));
  }

  // Checkbox
  auto checkBounds = juce::Rectangle<int>(5, (height - 16) / 2, 16, 16);
  g.setColour(juce::Colours::white);
  g.drawRect(checkBounds);
  if (isEnabled) {
    g.fillRect(checkBounds.reduced(3));
  }

  // Device name
  g.setColour(juce::Colours::white);
  g.setFont(OCC::Console::consoleFont(11.0f));
  g.drawText(deviceName, 28, 0, width - 30, height, juce::Justification::centredLeft);
}

void MIDIDevicesDialog::listBoxItemClicked(int row, const juce::MouseEvent& e) {
  auto& devices = m_showingInputList ? m_midiInDevices : m_midiOutDevices;

  if (row < 0 || row >= devices.size() || !m_midiManager) {
    return;
  }

  auto deviceName = devices[row];

  // Toggle device
  if (m_showingInputList) {
    if (m_midiManager->isMidiInDeviceEnabled(deviceName)) {
      m_midiManager->disableMidiInDevice(deviceName);
    } else {
      m_midiManager->enableMidiInDevice(deviceName);
    }
    m_midiInList.repaint();
  } else {
    if (m_midiManager->isMidiOutDeviceEnabled(deviceName)) {
      m_midiManager->disableMidiOutDevice(deviceName);
    } else {
      m_midiManager->enableMidiOutDevice(deviceName);
    }
    m_midiOutList.repaint();
  }

  (void)e; // Unused
}

void MIDIDevicesDialog::refreshDeviceLists() {
  if (m_midiManager) {
    m_midiInDevices = m_midiManager->getAvailableMidiInDevices();
    m_midiOutDevices = m_midiManager->getAvailableMidiOutDevices();

    m_showingInputList = true;
    m_midiInList.updateContent();
    m_midiInList.repaint();
  }
}

void MIDIDevicesDialog::applySettings() {
  if (m_midiManager) {
    // Apply scope
    if (m_globalScopeButton.getToggleState()) {
      m_midiManager->setScope(orpheus::MIDIDeviceManager::Scope::Global);
    } else {
      m_midiManager->setScope(orpheus::MIDIDeviceManager::Scope::Paged);
    }

    // Apply action
    if (m_gangedActionButton.getToggleState()) {
      m_midiManager->setMultiNoteAction(orpheus::MIDIDeviceManager::MultiNoteAction::Ganged);
    } else {
      m_midiManager->setMultiNoteAction(orpheus::MIDIDeviceManager::MultiNoteAction::Overlapped);
    }

    // Apply All Notes Off setting
    m_midiManager->setSendAllNotesOffOnPanic(m_sendAllNotesOffCheckbox.getToggleState());
  }
}