/*
  ==============================================================================

    MIDIDevicesDialog.h
    Created: 12 Jan 2026
    Author:  Orpheus Clip Composer

    Sprint 11: MIDI Device Configuration Dialog (OCC116)
    OCC149: Updated with Console design language

  ==============================================================================
*/

#pragma once

#include "ConsoleActionButton.h"
#include "../Core/MIDIDeviceManager.h"
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

/**
    Dialog for configuring MIDI devices and settings.

    Features:
    - MIDI In device list with checkboxes
    - MIDI Out device list with checkboxes
    - Global/Paged scope selection
    - Ganged/Overlapped action selection
    - Send All Notes Off on Panic checkbox
    - Open MIDI Monitor button
*/
class MIDIDevicesDialog : public juce::Component, public juce::ListBoxModel {
public:
  MIDIDevicesDialog(orpheus::MIDIDeviceManager* midiManager);
  ~MIDIDevicesDialog() override = default;

  void paint(juce::Graphics& g) override;
  void resized() override;

  // ListBoxModel overrides
  int getNumRows() override;
  void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height,
                        bool rowIsSelected) override;
  void listBoxItemClicked(int row, const juce::MouseEvent& e) override;

  std::function<void()> onOkClicked;
  std::function<void()> onCancelClicked;
  std::function<void()> onMonitorClicked;

private:
  void refreshDeviceLists();
  void updateHints();
  void applySettings();

  orpheus::MIDIDeviceManager* m_midiManager;

  // Title
  juce::Label m_titleLabel;

  // MIDI In section
  juce::Label m_midiInLabel;
  juce::ListBox m_midiInList;
  juce::StringArray m_midiInDevices;

  // MIDI Out section
  juce::Label m_midiOutLabel;
  juce::ListBox m_midiOutList;
  juce::StringArray m_midiOutDevices;

  // Which list is active (for ListBoxModel)
  bool m_showingInputList = true;

  // Scope section
  juce::Label m_scopeLabel;
  juce::ToggleButton m_globalScopeButton;
  juce::ToggleButton m_pagedScopeButton;

  // Action section
  juce::Label m_actionLabel;
  juce::ToggleButton m_gangedActionButton;
  juce::ToggleButton m_overlappedActionButton;

  // All Notes Off checkbox
  juce::ToggleButton m_sendAllNotesOffCheckbox;

  // Buttons
  std::unique_ptr<ConsoleActionButton> m_monitorButton;
  std::unique_ptr<ConsoleActionButton> m_refreshButton;
  std::unique_ptr<ConsoleActionButton> m_okButton;
  std::unique_ptr<ConsoleActionButton> m_cancelButton;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MIDIDevicesDialog)
};