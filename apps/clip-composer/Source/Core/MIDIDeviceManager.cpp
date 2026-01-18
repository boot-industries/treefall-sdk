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
  if (m_sessionManager == nullptr || m_audioEngine == nullptr) {
    return;
  }

  // Find clips matching this MIDI note
  // Note: This requires ClipData to have midiNote and midiChannel fields
  // For now, this is a placeholder - actual implementation depends on SessionManager
  // having the ability to query clips by MIDI note

  // TODO: Implement clip-to-MIDI note mapping in SessionManager::ClipData
  // and query matching clips here based on m_scope setting

  (void)noteNumber;
  (void)channel;
  (void)velocity;
  (void)source;
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
