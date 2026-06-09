/*
  ==============================================================================

    MIDIMonitorWindow.h
    Created: 12 Jan 2026
    Author:  Orpheus Clip Composer

    Sprint 12: MIDI Monitor Window (OCC116)
    OCC149: Updated with Console design language

  ==============================================================================
*/

#pragma once

#include "ConsoleActionButton.h"
#include "../Core/MIDIDeviceManager.h"
#include <deque>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

/**
    Window for real-time MIDI message monitoring.

    Features:
    - Timestamped MIDI messages (most recent at top)
    - Run/Stop/Clear controls
    - Copy to clipboard
    - Export to file
*/
class MIDIMonitorWindow : public juce::DocumentWindow {
public:
  MIDIMonitorWindow(orpheus::MIDIDeviceManager* midiManager);
  ~MIDIMonitorWindow() override;

  void closeButtonPressed() override;

  /** Add a MIDI message to the log */
  void addMessage(const juce::MidiMessage& message, const juce::String& source);

private:
  class Content;
  std::unique_ptr<Content> m_content;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MIDIMonitorWindow)
};

//==============================================================================
/**
    Content component for MIDI Monitor window.
*/
class MIDIMonitorWindow::Content : public juce::Component, private juce::Timer {
public:
  Content(orpheus::MIDIDeviceManager* midiManager);
  ~Content() override = default;

  void paint(juce::Graphics& g) override;
  void resized() override;

  void addMessage(const juce::MidiMessage& message, const juce::String& source);

private:
  void timerCallback() override;
  void updateLogDisplay();
  void clearLog();
  void copyToClipboard();
  void exportToFile();

  struct LogEntry {
    juce::Time timestamp;
    juce::String source;
    juce::String messageType;
    int channel;
    int data1;
    int data2;
  };

  orpheus::MIDIDeviceManager* m_midiManager;

  juce::Label m_statusLabel;
  juce::TextEditor m_logText;

  std::unique_ptr<ConsoleActionButton> m_runButton;
  std::unique_ptr<ConsoleActionButton> m_stopButton;
  std::unique_ptr<ConsoleActionButton> m_clearButton;
  std::unique_ptr<ConsoleActionButton> m_copyButton;
  std::unique_ptr<ConsoleActionButton> m_exportButton;

  std::deque<LogEntry> m_logEntries;
  bool m_isRunning = true;
  juce::CriticalSection m_logLock;

  static constexpr size_t MAX_LOG_ENTRIES = 1000;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Content)
};