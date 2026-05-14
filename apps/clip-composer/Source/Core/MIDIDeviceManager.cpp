/*
  ==============================================================================

    MIDIDeviceManager.cpp
    Created: 12 Jan 2026
    Author:  Orpheus Clip Composer

    Sprint 11: MIDI Device Management (OCC116)

  ==============================================================================
*/

#include "MIDIDeviceManager.h"
#include "../Audio/AudioEngine.h"
#include "../Session/SessionManager.h"

namespace orpheus {

//==============================================================================
MIDIDeviceManager::MIDIDeviceManager() {
  load();
  reopenDevices();
}

MIDIDeviceManager::~MIDIDeviceManager() {
  // Close all MIDI devices
  m_midiInputs.clear();
  m_midiOutputs.clear();
}

//==============================================================================
// Device Enumeration

juce::StringArray MIDIDeviceManager::getAvailableMidiInDevices() const {
  juce::StringArray devices;
  auto midiInputs = juce::MidiInput::getAvailableDevices();
  for (const auto& device : midiInputs) {
    devices.add(device.name);
  }
  return devices;
}

juce::StringArray MIDIDeviceManager::getAvailableMidiOutDevices() const {
  juce::StringArray devices;
  auto midiOutputs = juce::MidiOutput::getAvailableDevices();
  for (const auto& device : midiOutputs) {
    devices.add(device.name);
  }
  return devices;
}

//==============================================================================
// Device Selection

bool MIDIDeviceManager::enableMidiInDevice(const juce::String& deviceName) {
  juce::ScopedLock lock(m_midiLock);

  // Already enabled?
  if (m_midiInputs.find(deviceName) != m_midiInputs.end()) {
    return true;
  }

  // Find device
  auto devices = juce::MidiInput::getAvailableDevices();
  for (const auto& device : devices) {
    if (device.name == deviceName) {
      auto midiInput = juce::MidiInput::openDevice(device.identifier, this);
      if (midiInput) {
        midiInput->start();
        m_midiInputs[deviceName] = std::move(midiInput);
        m_enabledMidiInDevices.insert(deviceName);
        save();
        if (onDeviceListChanged) {
          juce::MessageManager::callAsync([this]() { onDeviceListChanged(); });
        }
        return true;
      }
    }
  }
  return false;
}

void MIDIDeviceManager::disableMidiInDevice(const juce::String& deviceName) {
  juce::ScopedLock lock(m_midiLock);

  auto it = m_midiInputs.find(deviceName);
  if (it != m_midiInputs.end()) {
    it->second->stop();
    m_midiInputs.erase(it);
  }
  m_enabledMidiInDevices.erase(deviceName);
  save();

  if (onDeviceListChanged) {
    juce::MessageManager::callAsync([this]() { onDeviceListChanged(); });
  }
}

std::vector<juce::String> MIDIDeviceManager::getEnabledMidiInDevices() const {
  return std::vector<juce::String>(m_enabledMidiInDevices.begin(), m_enabledMidiInDevices.end());
}

bool MIDIDeviceManager::isMidiInDeviceEnabled(const juce::String& deviceName) const {
  return m_enabledMidiInDevices.find(deviceName) != m_enabledMidiInDevices.end();
}

bool MIDIDeviceManager::enableMidiOutDevice(const juce::String& deviceName) {
  juce::ScopedLock lock(m_midiLock);

  // Already enabled?
  if (m_midiOutputs.find(deviceName) != m_midiOutputs.end()) {
    return true;
  }

  // Find device
  auto devices = juce::MidiOutput::getAvailableDevices();
  for (const auto& device : devices) {
    if (device.name == deviceName) {
      auto midiOutput = juce::MidiOutput::openDevice(device.identifier);
      if (midiOutput) {
        m_midiOutputs[deviceName] = std::move(midiOutput);
        m_enabledMidiOutDevices.insert(deviceName);
        save();
        if (onDeviceListChanged) {
          juce::MessageManager::callAsync([this]() { onDeviceListChanged(); });
        }
        return true;
      }
    }
  }
  return false;
}

void MIDIDeviceManager::disableMidiOutDevice(const juce::String& deviceName) {
  juce::ScopedLock lock(m_midiLock);

  m_midiOutputs.erase(deviceName);
  m_enabledMidiOutDevices.erase(deviceName);
  save();

  if (onDeviceListChanged) {
    juce::MessageManager::callAsync([this]() { onDeviceListChanged(); });
  }
}

std::vector<juce::String> MIDIDeviceManager::getEnabledMidiOutDevices() const {
  return std::vector<juce::String>(m_enabledMidiOutDevices.begin(), m_enabledMidiOutDevices.end());
}

bool MIDIDeviceManager::isMidiOutDeviceEnabled(const juce::String& deviceName) const {
  return m_enabledMidiOutDevices.find(deviceName) != m_enabledMidiOutDevices.end();
}

//==============================================================================
// Configuration

void MIDIDeviceManager::setScope(Scope scope) {
  if (m_scope != scope) {
    m_scope = scope;
    save();
    notifyChanged();
  }
}

void MIDIDeviceManager::setMultiNoteAction(MultiNoteAction action) {
  if (m_multiNoteAction != action) {
    m_multiNoteAction = action;
    m_lastTriggeredButtonForNote.clear();
    save();
    notifyChanged();
  }
}

void MIDIDeviceManager::setSendAllNotesOffOnPanic(bool enabled) {
  if (m_sendAllNotesOffOnPanic != enabled) {
    m_sendAllNotesOffOnPanic = enabled;
    save();
    notifyChanged();
  }
}

//==============================================================================
// MIDI Actions

void MIDIDeviceManager::sendAllNotesOff() {
  if (!m_sendAllNotesOffOnPanic) {
    return;
  }

  juce::ScopedLock lock(m_midiLock);

  for (auto& [name, output] : m_midiOutputs) {
    if (output) {
      // Send CC 123 (All Notes Off) on all channels
      for (int channel = 1; channel <= 16; ++channel) {
        auto msg = juce::MidiMessage::allNotesOff(channel);
        output->sendMessageNow(msg);
      }
    }
  }
}

void MIDIDeviceManager::setSessionManager(SessionManager* sessionManager) {
  m_sessionManager = sessionManager;
}

void MIDIDeviceManager::setAudioEngine(AudioEngine* audioEngine) {
  m_audioEngine = audioEngine;
}

void MIDIDeviceManager::setCurrentTab(int tabIndex) {
  m_currentTab = tabIndex;
}

//==============================================================================
// MIDI Learn Mode

void MIDIDeviceManager::startMidiLearnMode(std::function<void(int note, int channel)> callback) {
  juce::ScopedLock lock(m_midiLock);
  m_midiLearnCallback = callback;
}

void MIDIDeviceManager::cancelMidiLearnMode() {
  juce::ScopedLock lock(m_midiLock);
  m_midiLearnCallback = nullptr;
}

//==============================================================================
// OCC144: Per-Button MIDI Note Assignment

void MIDIDeviceManager::assignMidiNote(int globalButtonIndex, int note, int channel) {
  if (globalButtonIndex < 0 || globalButtonIndex >= MAX_BUTTONS) {
    return;
  }

  if (note >= 0 && note <= 127) {
    m_buttonMidiNotes[globalButtonIndex] = {note, channel};
    DBG("MIDIDeviceManager: Assigned MIDI note " << note << " (Ch " << channel << ") to button "
                                                 << globalButtonIndex);
  } else {
    clearMidiNote(globalButtonIndex);
  }

  save();
  notifyChanged();
}

std::pair<int, int> MIDIDeviceManager::getMidiNote(int globalButtonIndex) const {
  auto it = m_buttonMidiNotes.find(globalButtonIndex);
  if (it != m_buttonMidiNotes.end()) {
    return it->second;
  }
  return {-1, -1};
}

void MIDIDeviceManager::clearMidiNote(int globalButtonIndex) {
  auto it = m_buttonMidiNotes.find(globalButtonIndex);
  if (it != m_buttonMidiNotes.end()) {
    m_buttonMidiNotes.erase(it);
    DBG("MIDIDeviceManager: Cleared MIDI note for button " << globalButtonIndex);
    save();
    notifyChanged();
  }
}

bool MIDIDeviceManager::hasMidiNote(int globalButtonIndex) const {
  return m_buttonMidiNotes.find(globalButtonIndex) != m_buttonMidiNotes.end();
}

juce::String MIDIDeviceManager::getMidiNoteDescription(int globalButtonIndex) const {
  auto it = m_buttonMidiNotes.find(globalButtonIndex);
  if (it != m_buttonMidiNotes.end()) {
    auto [note, channel] = it->second;
    return noteNumberToName(note) + " (Ch " + juce::String(channel) + ")";
  }
  return juce::String();
}

juce::String MIDIDeviceManager::noteNumberToName(int noteNumber) {
  static const char* noteNames[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                    "F#", "G",  "G#", "A",  "A#", "B"};
  int octave = (noteNumber / 12) - 1;
  int noteIndex = noteNumber % 12;
  return juce::String(noteNames[noteIndex]) + juce::String(octave);
}

//==============================================================================
// MidiInputCallback

void MIDIDeviceManager::handleIncomingMidiMessage(juce::MidiInput* source,
                                                  const juce::MidiMessage& message) {
  juce::String sourceName = source ? source->getName() : "Unknown";

  // Notify listeners (for MIDI monitor)
  if (onMidiMessageReceived) {
    juce::MessageManager::callAsync(
        [this, message, sourceName]() { onMidiMessageReceived(message, sourceName); });
  }

  // Handle MIDI learn mode
  if (m_midiLearnCallback && message.isNoteOn()) {
    auto callback = m_midiLearnCallback;
    m_midiLearnCallback = nullptr; // One-shot

    juce::MessageManager::callAsync(
        [callback, message]() { callback(message.getNoteNumber(), message.getChannel()); });
    return;
  }

  // Handle note messages for clip triggering
  if (message.isNoteOn()) {
    handleNoteOn(message.getNoteNumber(), message.getChannel(), message.getVelocity(), sourceName);
  } else if (message.isNoteOff()) {
    handleNoteOff(message.getNoteNumber(), message.getChannel());
  }
}

void MIDIDeviceManager::handleNoteOn(int noteNumber, int channel, int velocity,
                                     const juce::String& source) {
  if (m_audioEngine == nullptr) {
    return;
  }

  (void)velocity;
  (void)source;

  // OCC144: Find all buttons with matching MIDI note assignment
  std::vector<int> matchingButtons;

  // Determine search range based on scope
  int startIndex = 0;
  int endIndex = MAX_BUTTONS;

  if (m_scope == Scope::Paged) {
    // Only search current tab.
    startIndex = m_currentTab * occ::BUTTONS_PER_TAB;
    endIndex = startIndex + occ::BUTTONS_PER_TAB;
  }

  for (const auto& [buttonIndex, noteAssignment] : m_buttonMidiNotes) {
    auto [assignedNote, assignedChannel] = noteAssignment;

    // Check if in range (for Paged scope)
    if (buttonIndex < startIndex || buttonIndex >= endIndex) {
      continue;
    }

    // Match note (channel 0 = omni/any channel)
    if (assignedNote == noteNumber && (assignedChannel == 0 || assignedChannel == channel)) {
      matchingButtons.push_back(buttonIndex);
    }
  }

  if (matchingButtons.empty()) {
    return;
  }

  // Trigger based on multi-note action
  if (m_multiNoteAction == MultiNoteAction::Ganged) {
    // Play all matching buttons simultaneously
    for (int buttonIndex : matchingButtons) {
      m_audioEngine->startClip(buttonIndex);
    }
  } else {
    // Overlapped mode: Play next in sequence
    int lastIndex = 0;
    auto it = m_lastTriggeredButtonForNote.find(noteNumber);
    if (it != m_lastTriggeredButtonForNote.end()) {
      lastIndex = it->second;
    }

    // Find next button that's not already playing
    int nextIndex = -1;
    for (size_t i = 0; i < matchingButtons.size(); ++i) {
      int candidateIndex =
          (lastIndex + 1 + static_cast<int>(i)) % static_cast<int>(matchingButtons.size());
      int buttonIndex = matchingButtons[candidateIndex];

      if (!m_audioEngine->isClipPlaying(buttonIndex)) {
        nextIndex = candidateIndex;
        break;
      }
    }

    // If all playing, choose the first one in rotation
    if (nextIndex == -1) {
      nextIndex = (lastIndex + 1) % static_cast<int>(matchingButtons.size());
    }

    // Play selected button
    if (nextIndex >= 0 && nextIndex < static_cast<int>(matchingButtons.size())) {
      m_audioEngine->startClip(matchingButtons[nextIndex]);
      m_lastTriggeredButtonForNote[noteNumber] = nextIndex;
    }
  }
}

void MIDIDeviceManager::handleNoteOff(int noteNumber, int channel) {
  // Could be used for "play while held" mode in the future
  (void)noteNumber;
  (void)channel;
}

//==============================================================================
// Persistence

juce::PropertiesFile::Options MIDIDeviceManager::getPropertiesFileOptions() const {
  juce::PropertiesFile::Options options;
  options.applicationName = "OrpheusClipComposer";
  options.filenameSuffix = ".mididevices";
  options.osxLibrarySubFolder = "Application Support";
  options.folderName = "OrpheusClipComposer";
  options.storageFormat = juce::PropertiesFile::storeAsXML;
  return options;
}

void MIDIDeviceManager::save() {
  juce::PropertiesFile prefs(getPropertiesFileOptions());

  prefs.setValue("scope", scopeToString(m_scope));
  prefs.setValue("multiNoteAction", multiNoteActionToString(m_multiNoteAction));
  prefs.setValue("sendAllNotesOffOnPanic", m_sendAllNotesOffOnPanic);

  // Save enabled device lists
  juce::StringArray inDevices;
  for (const auto& name : m_enabledMidiInDevices) {
    inDevices.add(name);
  }
  prefs.setValue("enabledMidiInDevices", inDevices.joinIntoString("|"));

  juce::StringArray outDevices;
  for (const auto& name : m_enabledMidiOutDevices) {
    outDevices.add(name);
  }
  prefs.setValue("enabledMidiOutDevices", outDevices.joinIntoString("|"));

  // OCC144: Save per-button MIDI note assignments
  juce::String midiNotesData;
  for (const auto& [buttonIndex, noteAssignment] : m_buttonMidiNotes) {
    auto [note, channel] = noteAssignment;
    // Format: buttonIndex:note:channel;
    midiNotesData +=
        juce::String(buttonIndex) + ":" + juce::String(note) + ":" + juce::String(channel) + ";";
  }
  prefs.setValue("buttonMidiNotes", midiNotesData);

  prefs.saveIfNeeded();
}

void MIDIDeviceManager::load() {
  juce::PropertiesFile prefs(getPropertiesFileOptions());

  m_scope = stringToScope(prefs.getValue("scope", "Paged"));
  m_multiNoteAction = stringToMultiNoteAction(prefs.getValue("multiNoteAction", "Ganged"));
  m_sendAllNotesOffOnPanic = prefs.getBoolValue("sendAllNotesOffOnPanic", true);

  // Load enabled device lists
  juce::String inDevicesStr = prefs.getValue("enabledMidiInDevices", "");
  if (inDevicesStr.isNotEmpty()) {
    juce::StringArray inDevices;
    inDevices.addTokens(inDevicesStr, "|", "");
    for (const auto& name : inDevices) {
      m_enabledMidiInDevices.insert(name);
    }
  }

  juce::String outDevicesStr = prefs.getValue("enabledMidiOutDevices", "");
  if (outDevicesStr.isNotEmpty()) {
    juce::StringArray outDevices;
    outDevices.addTokens(outDevicesStr, "|", "");
    for (const auto& name : outDevices) {
      m_enabledMidiOutDevices.insert(name);
    }
  }

  // OCC144: Load per-button MIDI note assignments
  m_buttonMidiNotes.clear();
  juce::String midiNotesData = prefs.getValue("buttonMidiNotes", "");
  if (midiNotesData.isNotEmpty()) {
    juce::StringArray entries;
    entries.addTokens(midiNotesData, ";", "");

    for (const auto& entry : entries) {
      if (entry.isEmpty())
        continue;

      juce::StringArray parts;
      parts.addTokens(entry, ":", "");
      if (parts.size() >= 3) {
        int buttonIndex = parts[0].getIntValue();
        int note = parts[1].getIntValue();
        int channel = parts[2].getIntValue();

        if (buttonIndex >= 0 && buttonIndex < MAX_BUTTONS && note >= 0 && note <= 127) {
          m_buttonMidiNotes[buttonIndex] = {note, channel};
        }
      }
    }
    DBG("MIDIDeviceManager: Loaded " << m_buttonMidiNotes.size()
                                     << " custom MIDI note assignments");
  }
}

void MIDIDeviceManager::reopenDevices() {
  // Re-open previously enabled devices
  auto inDevicesCopy = m_enabledMidiInDevices;
  m_enabledMidiInDevices.clear();
  for (const auto& name : inDevicesCopy) {
    enableMidiInDevice(name);
  }

  auto outDevicesCopy = m_enabledMidiOutDevices;
  m_enabledMidiOutDevices.clear();
  for (const auto& name : outDevicesCopy) {
    enableMidiOutDevice(name);
  }
}

void MIDIDeviceManager::notifyChanged() {
  if (onConfigChanged) {
    onConfigChanged();
  }
}

//==============================================================================
// Utility

juce::String MIDIDeviceManager::scopeToString(Scope scope) {
  switch (scope) {
  case Scope::Global:
    return "Global";
  case Scope::Paged:
    return "Paged";
  }
  return "Paged";
}

MIDIDeviceManager::Scope MIDIDeviceManager::stringToScope(const juce::String& str) {
  if (str == "Global")
    return Scope::Global;
  return Scope::Paged;
}

juce::String MIDIDeviceManager::multiNoteActionToString(MultiNoteAction action) {
  switch (action) {
  case MultiNoteAction::Ganged:
    return "Ganged";
  case MultiNoteAction::Overlapped:
    return "Overlapped";
  }
  return "Ganged";
}

MIDIDeviceManager::MultiNoteAction
MIDIDeviceManager::stringToMultiNoteAction(const juce::String& str) {
  if (str == "Overlapped")
    return MultiNoteAction::Overlapped;
  return MultiNoteAction::Ganged;
}

} // namespace orpheus
