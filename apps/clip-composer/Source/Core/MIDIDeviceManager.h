/*
  ==============================================================================

    MIDIDeviceManager.h
    Created: 12 Jan 2026
    Author:  Orpheus Clip Composer

    Sprint 11: MIDI Device Management (OCC116)
    Handles MIDI input/output device configuration and message routing.

  ==============================================================================
*/

#pragma once

#include "GridConstants.h"
#include <functional>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <map>
#include <memory>
#include <set>
#include <vector>

// Forward declarations
class SessionManager;
class AudioEngine;

namespace orpheus {

/**
    Manages MIDI device configuration and message handling.

    Features:
    - Multiple MIDI In/Out devices
    - Global/Paged scope for MIDI note triggers
    - Ganged/Overlapped modes for duplicate note assignments
    - Send "All Notes Off" on panic
    - MIDI learn mode for clip assignment
*/
class MIDIDeviceManager : public juce::MidiInputCallback {
public:
  //==============================================================================
  enum class Scope {
    Global, // MIDI notes trigger clips on any tab
    Paged   // MIDI notes only trigger clips on current tab
  };

  enum class MultiNoteAction {
    Ganged,    // All buttons with same MIDI note play together
    Overlapped // Buttons play in sequence (round-robin)
  };

  //==============================================================================
  MIDIDeviceManager();
  ~MIDIDeviceManager() override;

  //==============================================================================
  // Device Enumeration

  /** Get list of available MIDI input devices */
  juce::StringArray getAvailableMidiInDevices() const;

  /** Get list of available MIDI output devices */
  juce::StringArray getAvailableMidiOutDevices() const;

  //==============================================================================
  // Device Selection (Multiple)

  /** Enable a MIDI input device */
  bool enableMidiInDevice(const juce::String& deviceName);

  /** Disable a MIDI input device */
  void disableMidiInDevice(const juce::String& deviceName);

  /** Get list of enabled MIDI input device names */
  std::vector<juce::String> getEnabledMidiInDevices() const;

  /** Check if a MIDI input device is enabled */
  bool isMidiInDeviceEnabled(const juce::String& deviceName) const;

  /** Enable a MIDI output device */
  bool enableMidiOutDevice(const juce::String& deviceName);

  /** Disable a MIDI output device */
  void disableMidiOutDevice(const juce::String& deviceName);

  /** Get list of enabled MIDI output device names */
  std::vector<juce::String> getEnabledMidiOutDevices() const;

  /** Check if a MIDI output device is enabled */
  bool isMidiOutDeviceEnabled(const juce::String& deviceName) const;

  //==============================================================================
  // Configuration

  void setScope(Scope scope);
  Scope getScope() const {
    return m_scope;
  }

  void setMultiNoteAction(MultiNoteAction action);
  MultiNoteAction getMultiNoteAction() const {
    return m_multiNoteAction;
  }

  void setSendAllNotesOffOnPanic(bool enabled);
  bool getSendAllNotesOffOnPanic() const {
    return m_sendAllNotesOffOnPanic;
  }

  //==============================================================================
  // MIDI Actions

  /** Send "All Notes Off" CC message to all enabled output devices */
  void sendAllNotesOff();

  /** Set session manager for clip queries */
  void setSessionManager(SessionManager* sessionManager);

  /** Set audio engine for clip triggering */
  void setAudioEngine(AudioEngine* audioEngine);

  /** Set current tab index (for Paged scope) */
  void setCurrentTab(int tabIndex);

  //==============================================================================
  // MIDI Learn Mode

  /** Start MIDI learn mode. Next received note will call the callback. */
  void startMidiLearnMode(std::function<void(int note, int channel)> callback);

  /** Cancel MIDI learn mode */
  void cancelMidiLearnMode();

  /** Check if currently in MIDI learn mode */
  bool isInMidiLearnMode() const {
    return m_midiLearnCallback != nullptr;
  }

  //==============================================================================
  // OCC144: Per-Button MIDI Note Assignment

  static constexpr int MAX_BUTTONS = occ::TOTAL_BUTTONS;

  /** Assign a MIDI note to a button */
  void assignMidiNote(int globalButtonIndex, int note, int channel = 1);

  /** Get assigned MIDI note/channel for a button (-1 if none) */
  std::pair<int, int> getMidiNote(int globalButtonIndex) const;

  /** Clear MIDI note assignment for a button */
  void clearMidiNote(int globalButtonIndex);

  /** Check if button has a MIDI note assigned */
  bool hasMidiNote(int globalButtonIndex) const;

  /** Get human-readable description (e.g., "C4 (Ch 1)") */
  juce::String getMidiNoteDescription(int globalButtonIndex) const;

  /** Static helper: Convert note number to name (e.g., 60 -> "C4") */
  static juce::String noteNumberToName(int noteNumber);

  //==============================================================================
  // Persistence

  void save();
  void load();

  //==============================================================================
  // Callbacks

  /** Called when a MIDI message is received (for monitoring) */
  std::function<void(const juce::MidiMessage&, const juce::String& source)> onMidiMessageReceived;

  /** Called when configuration changes */
  std::function<void()> onConfigChanged;

  /** Called when device list changes */
  std::function<void()> onDeviceListChanged;

  //==============================================================================
  // Utility

  static juce::String scopeToString(Scope scope);
  static Scope stringToScope(const juce::String& str);

  static juce::String multiNoteActionToString(MultiNoteAction action);
  static MultiNoteAction stringToMultiNoteAction(const juce::String& str);

private:
  //==============================================================================
  // MidiInputCallback override
  void handleIncomingMidiMessage(juce::MidiInput* source,
                                 const juce::MidiMessage& message) override;

  //==============================================================================
  void handleNoteOn(int noteNumber, int channel, int velocity, const juce::String& source);
  void handleNoteOff(int noteNumber, int channel);

  juce::PropertiesFile::Options getPropertiesFileOptions() const;
  void notifyChanged();
  void reopenDevices();

  //==============================================================================
  Scope m_scope = Scope::Paged;
  MultiNoteAction m_multiNoteAction = MultiNoteAction::Ganged;
  bool m_sendAllNotesOffOnPanic = true;

  // Active MIDI devices
  std::map<juce::String, std::unique_ptr<juce::MidiInput>> m_midiInputs;
  std::map<juce::String, std::unique_ptr<juce::MidiOutput>> m_midiOutputs;

  // Enabled device names (for persistence)
  std::set<juce::String> m_enabledMidiInDevices;
  std::set<juce::String> m_enabledMidiOutDevices;

  // Track last triggered button per MIDI note for overlapped mode
  std::map<int, int> m_lastTriggeredButtonForNote; // midiNote -> index in matching set

  // Service references
  SessionManager* m_sessionManager = nullptr;
  AudioEngine* m_audioEngine = nullptr;
  int m_currentTab = 0;

  // MIDI learn mode
  std::function<void(int note, int channel)> m_midiLearnCallback;

  // OCC144: Per-button MIDI note assignments
  // Key: globalButtonIndex, Value: (noteNumber, channel)
  std::map<int, std::pair<int, int>> m_buttonMidiNotes;

  // Thread safety
  juce::CriticalSection m_midiLock;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MIDIDeviceManager)
};

} // namespace orpheus
