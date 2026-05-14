/*
  ==============================================================================

    MIDIMonitorWindow.cpp
    Created: 12 Jan 2026
    Author:  Orpheus Clip Composer

    Sprint 12: MIDI Monitor Window (OCC116)

  ==============================================================================
*/

#include "MIDIMonitorWindow.h"
#include "DesignTokens.h"

//==============================================================================
// MIDIMonitorWindow

MIDIMonitorWindow::MIDIMonitorWindow(orpheus::MIDIDeviceManager* midiManager)
    : DocumentWindow("MIDI Monitor", juce::Colour(OCC::Design::kBgSurface),
                     DocumentWindow::closeButton | DocumentWindow::minimiseButton) {

  m_content = std::make_unique<Content>(midiManager);
  setContentOwned(m_content.release(), true);

  setUsingNativeTitleBar(true);
  setResizable(true, false);
  centreWithSize(600, 400);
  setVisible(true);

  // Register for MIDI messages
  if (midiManager) {
    midiManager->onMidiMessageReceived = [this](const juce::MidiMessage& msg,
                                                const juce::String& source) {
      if (auto* content = dynamic_cast<Content*>(getContentComponent())) {
        content->addMessage(msg, source);
      }
    };
  }
}

MIDIMonitorWindow::~MIDIMonitorWindow() = default;

void MIDIMonitorWindow::closeButtonPressed() {
  setVisible(false);
}

void MIDIMonitorWindow::addMessage(const juce::MidiMessage& message, const juce::String& source) {
  if (auto* content = dynamic_cast<Content*>(getContentComponent())) {
    content->addMessage(message, source);
  }
}

//==============================================================================
// Content

MIDIMonitorWindow::Content::Content(orpheus::MIDIDeviceManager* midiManager)
    : m_midiManager(midiManager) {

  // Status label
  addAndMakeVisible(m_statusLabel);
  m_statusLabel.setText("MIDI Monitor - Running", juce::dontSendNotification);
  m_statusLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));

  // Log text area
  addAndMakeVisible(m_logText);
  m_logText.setMultiLine(true);
  m_logText.setReadOnly(true);
  m_logText.setScrollbarsShown(true);
  m_logText.setCaretVisible(false);
  m_logText.setColour(juce::TextEditor::backgroundColourId, juce::Colour(OCC::Design::kBgPrimary));
  m_logText.setColour(juce::TextEditor::textColourId, juce::Colour(OCC::Design::kTextPrimary));
  m_logText.setFont(juce::Font(
      juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain)));

  // Buttons
  addAndMakeVisible(m_runButton);
  m_runButton.setButtonText("Run");
  m_runButton.setEnabled(false);
  m_runButton.onClick = [this]() {
    m_isRunning = true;
    m_runButton.setEnabled(false);
    m_stopButton.setEnabled(true);
    m_statusLabel.setText("MIDI Monitor - Running", juce::dontSendNotification);
  };

  addAndMakeVisible(m_stopButton);
  m_stopButton.setButtonText("Stop");
  m_stopButton.onClick = [this]() {
    m_isRunning = false;
    m_runButton.setEnabled(true);
    m_stopButton.setEnabled(false);
    m_statusLabel.setText("MIDI Monitor - Stopped", juce::dontSendNotification);
  };

  addAndMakeVisible(m_clearButton);
  m_clearButton.setButtonText("Clear");
  m_clearButton.onClick = [this]() { clearLog(); };

  addAndMakeVisible(m_copyButton);
  m_copyButton.setButtonText("Copy");
  m_copyButton.onClick = [this]() { copyToClipboard(); };

  addAndMakeVisible(m_exportButton);
  m_exportButton.setButtonText("Export...");
  m_exportButton.onClick = [this]() { exportToFile(); };

  // Start update timer
  startTimer(100); // 10 FPS
}

void MIDIMonitorWindow::Content::paint(juce::Graphics& g) {
  g.fillAll(juce::Colour(OCC::Design::kBgSurface));
}

void MIDIMonitorWindow::Content::resized() {
  auto area = getLocalBounds().reduced(10);

  // Top row: status and buttons
  auto topRow = area.removeFromTop(30);
  m_statusLabel.setBounds(topRow.removeFromLeft(200));

  m_exportButton.setBounds(topRow.removeFromRight(70));
  topRow.removeFromRight(5);
  m_copyButton.setBounds(topRow.removeFromRight(60));
  topRow.removeFromRight(5);
  m_clearButton.setBounds(topRow.removeFromRight(60));
  topRow.removeFromRight(10);
  m_stopButton.setBounds(topRow.removeFromRight(60));
  topRow.removeFromRight(5);
  m_runButton.setBounds(topRow.removeFromRight(60));

  area.removeFromTop(10);

  // Log text area takes remaining space
  m_logText.setBounds(area);
}

void MIDIMonitorWindow::Content::addMessage(const juce::MidiMessage& message,
                                            const juce::String& source) {
  if (!m_isRunning)
    return;

  juce::ScopedLock lock(m_logLock);

  LogEntry entry;
  entry.timestamp = juce::Time::getCurrentTime();
  entry.source = source;
  entry.channel = message.getChannel();
  entry.data1 = 0;
  entry.data2 = 0;

  if (message.isNoteOn()) {
    entry.messageType = "Note On";
    entry.data1 = message.getNoteNumber();
    entry.data2 = message.getVelocity();
  } else if (message.isNoteOff()) {
    entry.messageType = "Note Off";
    entry.data1 = message.getNoteNumber();
    entry.data2 = message.getVelocity();
  } else if (message.isController()) {
    entry.messageType = "CC";
    entry.data1 = message.getControllerNumber();
    entry.data2 = message.getControllerValue();
  } else if (message.isProgramChange()) {
    entry.messageType = "Program";
    entry.data1 = message.getProgramChangeNumber();
  } else if (message.isPitchWheel()) {
    entry.messageType = "Pitch";
    entry.data1 = message.getPitchWheelValue();
  } else if (message.isMidiClock()) {
    entry.messageType = "Clock";
  } else if (message.isMidiStart()) {
    entry.messageType = "Start";
  } else if (message.isMidiStop()) {
    entry.messageType = "Stop";
  } else if (message.isMidiContinue()) {
    entry.messageType = "Continue";
  } else if (message.isQuarterFrame()) {
    entry.messageType = "MTC QF";
    entry.data1 = message.getQuarterFrameValue();
  } else {
    entry.messageType = "Other";
  }

  // Add to front (most recent first)
  m_logEntries.push_front(entry);

  // Limit size
  while (m_logEntries.size() > MAX_LOG_ENTRIES) {
    m_logEntries.pop_back();
  }
}

void MIDIMonitorWindow::Content::timerCallback() {
  updateLogDisplay();
}

void MIDIMonitorWindow::Content::updateLogDisplay() {
  juce::ScopedLock lock(m_logLock);

  juce::String logText;

  // Header
  logText << "Time        | Source              | Ch | Type     | Data\n";
  logText << "------------|---------------------|----|---------|---------\n";

  for (const auto& entry : m_logEntries) {
    logText << entry.timestamp.formatted("%H:%M:%S.")
            << juce::String(entry.timestamp.getMilliseconds()).paddedLeft('0', 3) << " | ";
    logText << entry.source.paddedRight(' ', 19) << " | ";
    logText << juce::String(entry.channel).paddedLeft(' ', 2) << " | ";
    logText << entry.messageType.paddedRight(' ', 8) << " | ";

    if (entry.messageType == "Note On" || entry.messageType == "Note Off") {
      logText << juce::MidiMessage::getMidiNoteName(entry.data1, true, true, 4)
              << " vel=" << entry.data2;
    } else if (entry.messageType == "CC") {
      logText << "CC" << entry.data1 << " val=" << entry.data2;
    } else if (entry.messageType == "Program") {
      logText << "Pgm " << entry.data1;
    } else if (entry.messageType == "Pitch") {
      logText << entry.data1;
    } else if (entry.data1 != 0 || entry.data2 != 0) {
      logText << entry.data1 << "/" << entry.data2;
    }

    logText << "\n";
  }

  // Only update if changed (to avoid flicker)
  if (m_logText.getText() != logText) {
    m_logText.setText(logText, false);
  }
}

void MIDIMonitorWindow::Content::clearLog() {
  juce::ScopedLock lock(m_logLock);
  m_logEntries.clear();
  m_logText.clear();
}

void MIDIMonitorWindow::Content::copyToClipboard() {
  juce::SystemClipboard::copyTextToClipboard(m_logText.getText());
}

void MIDIMonitorWindow::Content::exportToFile() {
  juce::FileChooser chooser(
      "Export MIDI Log",
      juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("midi_log.txt"),
      "*.txt");

  if (chooser.browseForFileToSave(true)) {
    auto file = chooser.getResult();
    file.replaceWithText(m_logText.getText());
  }
}
