# OCC116 - Setup Menu Backend Features - Sprint Plan

**Version:** 1.0
**Date:** 2025-11-12
**Status:** Planning
**Sprint Context:** Backend Architecture Sprint (SpotOn Section 02 Analysis)
**Dependencies:** OCC114 (Codebase Audit), OCC115 (File Menu)

---

## Executive Summary

This document presents a comprehensive sprint plan for implementing Setup Menu backend features in Clip Composer, based on analysis of the SpotOn Manual Section 02 (Setup Menu). SpotOn demonstrates mature external control infrastructure (MIDI, hotkeys, GPI triggers, timecode) and tool integration that OCC currently lacks.

**Key Finding:** SpotOn's Setup Menu reveals **5 major backend systems** for external control and integration. OCC has minimal MIDI support and no infrastructure for external tools, advanced hotkeys, or timecode-based triggers.

---

## 1. SpotOn Setup Menu Analysis Summary

### 1.1 Core Features Identified

| Feature                       | SpotOn Has?                              | OCC Has?   | Priority |
| ----------------------------- | ---------------------------------------- | ---------- | -------- |
| **External Tool Registry**    | ✅ Yes (WAV editor, search, CD burner)   | ❌ No      | MEDIUM   |
| **HotKey Configuration**      | ✅ Yes (Global/Paged, Ganged/Overlapped) | ⚠️ Partial | HIGH     |
| **MIDI Device Management**    | ✅ Yes (Multi-device, monitoring)        | ⚠️ Partial | HIGH     |
| **MIDI Monitoring/Logging**   | ✅ Yes (Timestamped, visual)             | ❌ No      | MEDIUM   |
| **GPI Trigger System**        | ✅ Yes (64 GPIs, emulation)              | ❌ No      | LOW      |
| **Timecode Support**          | ✅ Yes (MIDI TC, SMPTE)                  | ❌ No      | MEDIUM   |
| **Overlapped Playback Modes** | ✅ Yes (Sequential triggers)             | ❌ No      | MEDIUM   |

---

### 1.2 Key Insights

**SpotOn's Strengths:**

1. **External Control:** MIDI, GPI, timecode triggers for automation
2. **Tool Integration:** External WAV editor, search utility, CD burner
3. **HotKey Flexibility:** Global/Paged scope, Ganged/Overlapped modes
4. **MIDI Monitoring:** Real-time message logging, timestamped
5. **Timecode Precision:** MIDI TC, SMPTE LTC with auto-arm

**OCC's Gaps:**

- ❌ **No external tool registry** (can't launch WAV editor from OCC)
- ⚠️ **Basic MIDI** (no monitoring, no overlapped modes)
- ❌ **No GPI infrastructure** (legacy hardware triggers)
- ❌ **No timecode support** (broadcast/theater requirement)
- ❌ **No hotkey scope control** (Global vs. Paged)

---

## 2. Sprint Breakdown

### Sprint 9: External Tool Registry (MEDIUM)

**Complexity:** Small (S)
**Estimated Duration:** 3-5 days
**Priority:** MEDIUM
**Dependencies:** OCC115 Sprint 1 (Application Data Folder)

#### 2.1 Scope

**A. External Tool Configuration**

Allow users to configure external applications for common workflows:

**Features:**

- WAV file editor integration (Audacity, Adobe Audition, etc.)
- Audio file search utility
- File browser integration
- Store tool paths in application preferences

**Implementation:**

```cpp
// New class: ExternalToolManager
class ExternalToolManager {
public:
    enum class ToolType {
        WAVEditor,
        SearchUtility,
        FileBrowser
    };

    ExternalToolManager();

    // Set external tool path
    void setToolPath(ToolType type, const juce::File& executablePath);
    juce::File getToolPath(ToolType type) const;
    void clearToolPath(ToolType type);

    // Launch external tool with file argument
    bool launchTool(ToolType type, const juce::File& file);
    bool isToolConfigured(ToolType type) const;

    // Persistence
    void saveToPreferences(juce::PropertiesFile& props);
    void loadFromPreferences(juce::PropertiesFile& props);

private:
    std::map<ToolType, juce::File> m_toolPaths;

    juce::String getToolPreferenceKey(ToolType type) const;
};

void ExternalToolManager::setToolPath(ToolType type, const juce::File& executablePath) {
    if (!executablePath.existsAsFile()) {
        showError("Executable not found: " + executablePath.getFullPathName());
        return;
    }

    m_toolPaths[type] = executablePath;
    saveToPreferences(getApplicationProperties());
}

bool ExternalToolManager::launchTool(ToolType type, const juce::File& file) {
    if (!isToolConfigured(type)) {
        showError("Tool not configured. Please set path in Setup menu.");
        return false;
    }

    auto toolPath = m_toolPaths[type];

    // Launch process with file argument
    juce::ChildProcess process;
    juce::StringArray args;
    args.add(toolPath.getFullPathName());
    args.add(file.getFullPathName());

    if (process.start(args)) {
        logEvent("Launched " + getToolName(type) + ": " + file.getFileName());
        return true;
    } else {
        showError("Failed to launch " + getToolName(type));
        return false;
    }
}
```

**B. Setup Menu Integration**

Add Setup menu with tool configuration dialogs:

**Menu Structure:**

```
Setup
├── WAV Editor...           (Opens file selector)
├── Search Utility...       (Opens file selector)
├── HotKeys...             (Opens hotkey config dialog)
├── MIDI Devices...        (Opens MIDI config dialog)
└── (Future) GPI Triggers...
```

**Implementation:**

```cpp
// In Setup menu handler
void MainComponent::onSetupWAVEditor() {
    juce::FileChooser chooser("Select WAV file editor application",
                              juce::File::getSpecialLocation(
                                  juce::File::globalApplicationsDirectory),
                              "*.exe;*.app");

    if (chooser.browseForFileToOpen()) {
        auto selectedFile = chooser.getResult();
        externalToolManager->setToolPath(
            ExternalToolManager::ToolType::WAVEditor, selectedFile);

        // Show confirmation
        showNotification("WAV Editor set to: " + selectedFile.getFileName());
    } else {
        // User cancelled - offer to clear existing selection
        if (externalToolManager->isToolConfigured(
                ExternalToolManager::ToolType::WAVEditor)) {
            auto result = juce::AlertWindow::show(
                juce::MessageBoxIconType::QuestionIcon,
                "Clear WAV Editor",
                "Clear selected editor utility?",
                "Yes", "No"
            );

            if (result == 1) {
                externalToolManager->clearToolPath(
                    ExternalToolManager::ToolType::WAVEditor);
            }
        }
    }
}
```

**C. Right-Click Menu Integration**

Add "Edit in External Editor" to clip button right-click menu:

**Implementation:**

```cpp
// In ClipButton right-click menu
void ClipButton::showPopupMenu() {
    juce::PopupMenu menu;

    // ... existing menu items ...

    if (externalToolManager->isToolConfigured(
            ExternalToolManager::ToolType::WAVEditor)) {
        auto editorName = externalToolManager->getToolPath(
            ExternalToolManager::ToolType::WAVEditor).getFileNameWithoutExtension();
        menu.addItem(10, "Edit in " + editorName + "...", true);
    } else {
        menu.addItem(10, "Edit in External Editor... (not configured)", false);
    }

    // ... handle menu selection ...

    if (result == 10) {
        // Launch external editor
        auto clipData = sessionManager->getClip(m_buttonIndex);
        juce::File audioFile(clipData.filePath);

        if (audioFile.existsAsFile()) {
            externalToolManager->launchTool(
                ExternalToolManager::ToolType::WAVEditor, audioFile);
        }
    }
}
```

#### 2.2 Acceptance Criteria

- [ ] User can set WAV editor path via Setup > WAV Editor
- [ ] User can set search utility path via Setup > Search Utility
- [ ] Cancelling file selector offers to clear existing selection
- [ ] Tool paths persist across app restarts (saved in preferences)
- [ ] Right-click menu shows "Edit in [EditorName]..." when editor configured
- [ ] Clicking menu item launches external editor with audio file path
- [ ] Cross-platform executable selection (_.exe, _.app, no extension)
- [ ] Error handling for missing/invalid executables

#### 2.3 Technical Requirements

- Use `juce::ChildProcess` for launching external tools
- Store tool paths in `PropertiesFile` (application preferences)
- Validate executable existence before launching
- Log tool launches to event log (OCC115 Sprint 5)
- Handle spaces in file paths (quote arguments)
- Test on macOS, Windows, Linux

---

### Sprint 10: HotKey Configuration System (HIGH)

**Complexity:** Medium (M)
**Estimated Duration:** 1-2 weeks
**Priority:** HIGH
**Dependencies:** None

#### 2.1 Scope

**A. HotKey Scope (Global vs. Paged)**

Control whether hotkeys trigger clips on current tab only or across all tabs:

**Features:**

- **Global scope:** Hotkey triggers clip on any tab (search all tabs)
- **Paged scope:** Hotkey only triggers clips on current tab
- Persisted in application preferences (not session-specific)

**Implementation:**

```cpp
// In SessionManager or new HotKeyManager class
class HotKeyManager {
public:
    enum class Scope {
        Global,  // Hotkey searches all tabs
        Paged    // Hotkey searches current tab only
    };

    enum class MultiButtonAction {
        Ganged,     // All buttons with same hotkey play together
        Overlapped  // Buttons play in sequence (round-robin)
    };

    HotKeyManager();

    void setScope(Scope scope);
    Scope getScope() const { return m_scope; }

    void setMultiButtonAction(MultiButtonAction action);
    MultiButtonAction getMultiButtonAction() const { return m_multiButtonAction; }

    // Find clips matching hotkey
    std::vector<ClipData> findClipsForHotKey(juce::KeyPress key,
                                              int currentTab) const;

    // Trigger hotkey (respects scope and action)
    void triggerHotKey(juce::KeyPress key, int currentTab);

    // Persistence
    void saveToPreferences(juce::PropertiesFile& props);
    void loadFromPreferences(juce::PropertiesFile& props);

private:
    Scope m_scope = Scope::Paged;
    MultiButtonAction m_multiButtonAction = MultiButtonAction::Ganged;

    // Track last triggered button for overlapped mode
    std::map<juce::KeyPress, int> m_lastTriggeredButton;
};

std::vector<ClipData> HotKeyManager::findClipsForHotKey(
    juce::KeyPress key, int currentTab) const {

    std::vector<ClipData> matchingClips;

    if (m_scope == Scope::Paged) {
        // Search current tab only
        for (int buttonIndex = 0; buttonIndex < 48; ++buttonIndex) {
            auto clip = sessionManager->getClip(currentTab, buttonIndex);
            if (clip.hotKey == key) {
                matchingClips.push_back(clip);
            }
        }
    } else {
        // Search all tabs (Global scope)
        for (int tabIndex = 0; tabIndex < 8; ++tabIndex) {
            for (int buttonIndex = 0; buttonIndex < 48; ++buttonIndex) {
                auto clip = sessionManager->getClip(tabIndex, buttonIndex);
                if (clip.hotKey == key) {
                    matchingClips.push_back(clip);
                }
            }
        }
    }

    return matchingClips;
}

void HotKeyManager::triggerHotKey(juce::KeyPress key, int currentTab) {
    auto matchingClips = findClipsForHotKey(key, currentTab);

    if (matchingClips.empty()) {
        return;  // No clips with this hotkey
    }

    if (m_multiButtonAction == MultiButtonAction::Ganged) {
        // Play all matching clips simultaneously
        for (const auto& clip : matchingClips) {
            audioEngine->startClip(clip.buttonIndex);
        }
    } else {
        // Overlapped mode: Play next in sequence
        int lastIndex = m_lastTriggeredButton[key];

        // Find next button that's not already playing
        int nextIndex = -1;
        for (size_t i = 0; i < matchingClips.size(); ++i) {
            int candidateIndex = (lastIndex + 1 + i) % matchingClips.size();
            auto& clip = matchingClips[candidateIndex];

            if (!audioEngine->isClipPlaying(clip.buttonIndex)) {
                nextIndex = candidateIndex;
                break;
            }
        }

        // If all playing, choose one with least time remaining
        if (nextIndex == -1) {
            // Find clip with least time remaining
            int64_t minRemaining = std::numeric_limits<int64_t>::max();
            for (size_t i = 0; i < matchingClips.size(); ++i) {
                auto& clip = matchingClips[i];
                auto remaining = audioEngine->getClipRemainingTime(clip.buttonIndex);
                if (remaining < minRemaining) {
                    minRemaining = remaining;
                    nextIndex = i;
                }
            }
        }

        // Play selected clip
        if (nextIndex != -1) {
            audioEngine->startClip(matchingClips[nextIndex].buttonIndex);
            m_lastTriggeredButton[key] = nextIndex;
        }
    }
}
```

**B. HotKey Setup Dialog**

UI for configuring hotkey scope and multi-button action:

**Implementation:**

```cpp
// New UI component: HotKeySetupDialog
class HotKeySetupDialog : public juce::Component {
public:
    HotKeySetupDialog(HotKeyManager* hotKeyManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    HotKeyManager* m_hotKeyManager;

    juce::Label m_scopeLabel;
    juce::ToggleButton m_globalScopeButton;
    juce::ToggleButton m_pagedScopeButton;
    juce::Label m_scopeHintLabel;

    juce::Label m_actionLabel;
    juce::ToggleButton m_gangedActionButton;
    juce::ToggleButton m_overlappedActionButton;
    juce::Label m_actionHintLabel;

    juce::TextButton m_okButton;
    juce::TextButton m_cancelButton;

    void updateHints();
    void applySettings();
};

void HotKeySetupDialog::HotKeySetupDialog(HotKeyManager* hotKeyManager)
    : m_hotKeyManager(hotKeyManager) {

    // Scope section
    addAndMakeVisible(m_scopeLabel);
    m_scopeLabel.setText("Define scope of HotKeys", juce::dontSendNotification);

    addAndMakeVisible(m_globalScopeButton);
    m_globalScopeButton.setButtonText("Global");
    m_globalScopeButton.setRadioGroupId(1);

    addAndMakeVisible(m_pagedScopeButton);
    m_pagedScopeButton.setButtonText("Paged");
    m_pagedScopeButton.setRadioGroupId(1);

    // Set current scope
    if (m_hotKeyManager->getScope() == HotKeyManager::Scope::Global) {
        m_globalScopeButton.setToggleState(true, juce::dontSendNotification);
    } else {
        m_pagedScopeButton.setToggleState(true, juce::dontSendNotification);
    }

    addAndMakeVisible(m_scopeHintLabel);
    m_scopeHintLabel.setJustificationType(juce::Justification::centred);

    // Action section
    addAndMakeVisible(m_actionLabel);
    m_actionLabel.setText("Define action when Multiple Buttons are assigned the same HotKey",
                          juce::dontSendNotification);

    addAndMakeVisible(m_gangedActionButton);
    m_gangedActionButton.setButtonText("Ganged");
    m_gangedActionButton.setRadioGroupId(2);

    addAndMakeVisible(m_overlappedActionButton);
    m_overlappedActionButton.setButtonText("Overlapped");
    m_overlappedActionButton.setRadioGroupId(2);

    // Set current action
    if (m_hotKeyManager->getMultiButtonAction() == HotKeyManager::MultiButtonAction::Ganged) {
        m_gangedActionButton.setToggleState(true, juce::dontSendNotification);
    } else {
        m_overlappedActionButton.setToggleState(true, juce::dontSendNotification);
    }

    addAndMakeVisible(m_actionHintLabel);
    m_actionHintLabel.setJustificationType(juce::Justification::centred);

    // Buttons
    addAndMakeVisible(m_okButton);
    m_okButton.setButtonText("OK");
    m_okButton.onClick = [this] { applySettings(); };

    addAndMakeVisible(m_cancelButton);
    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.onClick = [this] { closeDialog(); };

    // Update hint labels
    updateHints();

    // Listen for radio button changes
    m_pagedScopeButton.onClick = [this] { updateHints(); };
    m_overlappedActionButton.onClick = [this] { updateHints(); };
}

void HotKeySetupDialog::updateHints() {
    // Update scope hint
    if (m_pagedScopeButton.getToggleState()) {
        m_scopeHintLabel.setText(
            "HotKeys are only enabled on the currently selected page\n"
            "Ganged/Overlapped action applies to selected page only",
            juce::dontSendNotification
        );
    } else {
        m_scopeHintLabel.setText(
            "HotKeys are enabled globally across all pages",
            juce::dontSendNotification
        );
    }

    // Update action hint
    if (m_gangedActionButton.getToggleState()) {
        m_actionHintLabel.setText(
            "All buttons with the same HotKey will play simultaneously when the HotKey is pressed",
            juce::dontSendNotification
        );
    } else {
        m_actionHintLabel.setText(
            "Buttons play in numerical sequence, starting with first not currently playing",
            juce::dontSendNotification
        );
    }
}
```

**C. Visual Indicators for Overlapped Mode**

Show underline on hotkey text when button is part of overlapped sequence:

**Implementation:**

```cpp
// In ClipButton::paint()
void ClipButton::paint(juce::Graphics& g) {
    // ... existing button rendering ...

    // Draw hotkey label
    if (!m_clipData.hotKey.isEmpty()) {
        g.setColour(juce::Colours::white);
        g.setFont(12.0f);

        juce::String hotKeyText = m_clipData.hotKey.getTextDescription();

        // Underline if part of overlapped sequence
        if (hotKeyManager->getMultiButtonAction() ==
            HotKeyManager::MultiButtonAction::Overlapped &&
            hotKeyManager->hasMultipleButtonsWithSameHotKey(m_clipData.hotKey)) {

            // Draw underline
            auto bounds = getHotKeyTextBounds();
            g.drawLine(bounds.getX(), bounds.getBottom(),
                       bounds.getRight(), bounds.getBottom(),
                       1.0f);
        }

        g.drawText(hotKeyText, getHotKeyTextBounds(),
                   juce::Justification::centred);
    }
}
```

#### 2.2 Acceptance Criteria

- [ ] Setup > HotKeys dialog allows selecting Global or Paged scope
- [ ] Setup > HotKeys dialog allows selecting Ganged or Overlapped action
- [ ] Hint labels update dynamically based on selection
- [ ] Paged scope: Hotkeys only trigger clips on current tab
- [ ] Global scope: Hotkeys trigger clips across all tabs
- [ ] Ganged mode: All clips with same hotkey play simultaneously
- [ ] Overlapped mode: Clips play in sequence (next not playing, or least time remaining)
- [ ] Overlapped mode: Hotkey text underlined on buttons
- [ ] Settings persist across app restarts
- [ ] HotKeys disabled warning shown if hotkeys globally disabled

#### 2.3 Technical Requirements

- Store scope and action in `PropertiesFile` (application preferences)
- Track last triggered button per hotkey (for overlapped sequence)
- Implement `getClipRemainingTime()` in AudioEngine
- Test with 2+ buttons assigned same hotkey
- Test with buttons on different tabs (Global scope)
- Visual underline only when overlapped mode AND multiple buttons with same hotkey

---

### Sprint 11: MIDI Device Management (HIGH)

**Complexity:** Large (L)
**Estimated Duration:** 2-3 weeks
**Priority:** HIGH
**Dependencies:** OCC115 Sprint 5 (Event Logging)

#### 2.1 Scope

**A. MIDI Device Enumeration and Configuration**

Extend current MIDI support to match SpotOn's capabilities:

**Features:**

- Multiple MIDI In devices (not just one)
- Multiple MIDI Out devices
- Device enable/disable per device
- Global vs. Paged scope for MIDI notes
- Ganged vs. Overlapped action for duplicate MIDI note assignments
- Send "All Notes Off" on panic (configurable)

**Implementation:**

```cpp
// Extend existing MIDI support (OCC likely has basic MIDI)
class MIDIDeviceManager {
public:
    enum class Scope {
        Global,  // MIDI notes trigger clips on any tab
        Paged    // MIDI notes only trigger clips on current tab
    };

    enum class MultiNoteAction {
        Ganged,     // All buttons with same MIDI note play together
        Overlapped  // Buttons play in sequence
    };

    MIDIDeviceManager();

    // Device enumeration
    juce::StringArray getAvailableMidiInDevices() const;
    juce::StringArray getAvailableMidiOutDevices() const;

    // Device selection (multiple)
    void addMidiInDevice(const juce::String& deviceName);
    void removeMidiInDevice(const juce::String& deviceName);
    std::vector<juce::String> getEnabledMidiInDevices() const;

    void addMidiOutDevice(const juce::String& deviceName);
    void removeMidiOutDevice(const juce::String& deviceName);
    std::vector<juce::String> getEnabledMidiOutDevices() const;

    // Configuration
    void setScope(Scope scope);
    Scope getScope() const { return m_scope; }

    void setMultiNoteAction(MultiNoteAction action);
    MultiNoteAction getMultiNoteAction() const { return m_multiNoteAction; }

    void setSendAllNotesOffOnPanic(bool enabled);
    bool getSendAllNotesOffOnPanic() const { return m_sendAllNotesOffOnPanic; }

    // MIDI message handling
    void handleIncomingMidiMessage(const juce::MidiMessage& message);
    void sendAllNotesOff();

    // Persistence (global, not session-specific)
    void saveToPreferences(juce::PropertiesFile& props);
    void loadFromPreferences(juce::PropertiesFile& props);

private:
    Scope m_scope = Scope::Paged;
    MultiNoteAction m_multiNoteAction = MultiNoteAction::Ganged;
    bool m_sendAllNotesOffOnPanic = true;

    std::vector<std::unique_ptr<juce::MidiInput>> m_midiInputs;
    std::vector<std::unique_ptr<juce::MidiOutput>> m_midiOutputs;

    std::vector<juce::String> m_enabledMidiInDevices;
    std::vector<juce::String> m_enabledMidiOutDevices;

    // Track last triggered button for overlapped mode
    std::map<int, int> m_lastTriggeredButtonForNote;  // midiNote -> buttonIndex

    void openMidiDevices();
    void closeMidiDevices();
};

void MIDIDeviceManager::handleIncomingMidiMessage(const juce::MidiMessage& message) {
    if (!message.isNoteOn()) {
        return;  // Only handle note-on messages for clip triggers
    }

    int noteNumber = message.getNoteNumber();
    int channel = message.getChannel();

    // Find clips matching this MIDI note
    auto matchingClips = sessionManager->findClipsForMidiNote(noteNumber, channel,
                                                               sessionManager->getActiveTab());

    if (matchingClips.empty()) {
        return;
    }

    if (m_multiNoteAction == MultiNoteAction::Ganged) {
        // Play all matching clips simultaneously
        for (const auto& clip : matchingClips) {
            audioEngine->startClip(clip.buttonIndex);
        }
    } else {
        // Overlapped mode: Play next in sequence
        int lastIndex = m_lastTriggeredButtonForNote[noteNumber];

        // Find next button that's not already playing
        int nextIndex = -1;
        for (size_t i = 0; i < matchingClips.size(); ++i) {
            int candidateIndex = (lastIndex + 1 + i) % matchingClips.size();
            auto& clip = matchingClips[candidateIndex];

            if (!audioEngine->isClipPlaying(clip.buttonIndex)) {
                nextIndex = candidateIndex;
                break;
            }
        }

        // If all playing, choose one with least time remaining
        if (nextIndex == -1) {
            int64_t minRemaining = std::numeric_limits<int64_t>::max();
            for (size_t i = 0; i < matchingClips.size(); ++i) {
                auto& clip = matchingClips[i];
                auto remaining = audioEngine->getClipRemainingTime(clip.buttonIndex);
                if (remaining < minRemaining) {
                    minRemaining = remaining;
                    nextIndex = i;
                }
            }
        }

        // Play selected clip
        if (nextIndex != -1) {
            audioEngine->startClip(matchingClips[nextIndex].buttonIndex);
            m_lastTriggeredButtonForNote[noteNumber] = nextIndex;
        }
    }
}

void MIDIDeviceManager::sendAllNotesOff() {
    if (!m_sendAllNotesOffOnPanic) {
        return;
    }

    // Send MIDI "All Notes Off" (CC 123) to all output devices
    for (auto& midiOutput : m_midiOutputs) {
        for (int channel = 1; channel <= 16; ++channel) {
            juce::MidiMessage allNotesOff =
                juce::MidiMessage::allNotesOff(channel);
            midiOutput->sendMessageNow(allNotesOff);
        }
    }
}
```

**B. MIDI Devices Dialog**

UI for configuring MIDI devices:

**Implementation:**

```cpp
// New UI component: MIDIDevicesDialog
class MIDIDevicesDialog : public juce::Component {
public:
    MIDIDevicesDialog(MIDIDeviceManager* midiManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    MIDIDeviceManager* m_midiManager;

    juce::Label m_titleLabel;

    // MIDI In/Out device lists
    juce::Label m_midiInLabel;
    juce::ListBox m_midiInList;
    juce::Label m_midiOutLabel;
    juce::ListBox m_midiOutList;

    // Scope and action
    juce::Label m_scopeLabel;
    juce::ToggleButton m_globalScopeButton;
    juce::ToggleButton m_pagedScopeButton;
    juce::Label m_scopeHintLabel;

    juce::Label m_actionLabel;
    juce::ToggleButton m_gangedActionButton;
    juce::ToggleButton m_overlappedActionButton;
    juce::Label m_actionHintLabel;

    // Send All Notes Off checkbox
    juce::ToggleButton m_sendAllNotesOffCheckbox;

    // Buttons
    juce::TextButton m_monitorButton;
    juce::TextButton m_okButton;
    juce::TextButton m_cancelButton;

    void updateDeviceLists();
    void updateHints();
    void applySettings();
    void openMidiMonitor();
};
```

**C. MIDI Note Assignment to Clips**

Store MIDI note per clip (similar to hotkey):

**Implementation:**

```cpp
// In ClipData struct (SessionManager.h)
struct ClipData {
    // ... existing fields ...

    int midiNote = -1;       // MIDI note number (0-127, -1 = none)
    int midiChannel = 1;     // MIDI channel (1-16)
};

// In ClipEditDialog
void ClipEditDialog::addMidiNoteSelector() {
    addAndMakeVisible(m_midiNoteLabel);
    m_midiNoteLabel.setText("MIDI Note:", juce::dontSendNotification);

    addAndMakeVisible(m_midiNoteCombo);
    m_midiNoteCombo.addItem("(None)", 1);
    for (int note = 0; note <= 127; ++note) {
        m_midiNoteCombo.addItem(juce::MidiMessage::getMidiNoteName(note, true, true, 4) +
                                " (" + juce::String(note) + ")", note + 2);
    }

    addAndMakeVisible(m_midiChannelSlider);
    m_midiChannelSlider.setRange(1, 16, 1);
    m_midiChannelSlider.setValue(1);

    addAndMakeVisible(m_setFromNextMidiInputButton);
    m_setFromNextMidiInputButton.setButtonText("Set from next MIDI Input");
    m_setFromNextMidiInputButton.onClick = [this] {
        midiManager->setMidiLearnMode(true, [this](int note, int channel) {
            m_midiNoteCombo.setSelectedId(note + 2);
            m_midiChannelSlider.setValue(channel);
        });
    };
}
```

#### 2.2 Acceptance Criteria

- [ ] Setup > MIDI Devices dialog shows available MIDI In/Out devices
- [ ] User can enable/disable multiple MIDI In devices (checkboxes)
- [ ] User can enable/disable multiple MIDI Out devices (checkboxes)
- [ ] User can select Global or Paged scope for MIDI notes
- [ ] User can select Ganged or Overlapped action for duplicate MIDI notes
- [ ] User can enable/disable "Send All Notes Off on Panic"
- [ ] MIDI note assignment per clip (via Clip Edit Dialog)
- [ ] "Set from next MIDI Input" button for MIDI learn
- [ ] Overlapped mode: MIDI note text underlined on buttons
- [ ] Panic button sends "All Notes Off" to all MIDI Out devices (if enabled)
- [ ] Settings persist across app restarts (global preferences)
- [ ] Warning shown if MIDI In/Out disabled

#### 2.3 Technical Requirements

- Use JUCE `MidiInput` and `MidiOutput` classes
- Handle multiple MIDI devices concurrently
- Store enabled devices in `PropertiesFile` (global preferences)
- Store MIDI note/channel per clip in session JSON
- Implement MIDI learn mode (capture next incoming note)
- Send MIDI "All Notes Off" (CC 123) on all channels (1-16)
- Test with multiple MIDI controllers
- Test Ganged vs. Overlapped modes

---

### Sprint 12: MIDI Monitoring and Logging (MEDIUM)

**Complexity:** Small (S)
**Estimated Duration:** 3-5 days
**Priority:** MEDIUM
**Dependencies:** Sprint 11 (MIDI Device Management), OCC115 Sprint 5 (Event Logging)

#### 2.1 Scope

**A. MIDI Message Monitor**

Real-time MIDI message logging window:

**Features:**

- Timestamped MIDI messages (most recent at top)
- Display: Timestamp, Source, Channel, Note/CC, Velocity, State
- Run/Stop/Clear controls
- MIDI timecode detection (green if present, red if absent)
- Copy to clipboard
- Export to log file

**Implementation:**

```cpp
// New UI component: MIDIMonitorDialog
class MIDIMonitorDialog : public juce::Component,
                          private juce::Timer {
public:
    MIDIMonitorDialog(MIDIDeviceManager* midiManager);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    struct MIDILogEntry {
        juce::Time timestamp;
        juce::String source;  // Device name
        int channel;
        juce::String messageType;  // Note On, Note Off, CC, etc.
        int data1;  // Note number or CC number
        int data2;  // Velocity or CC value
    };

    MIDIDeviceManager* m_midiManager;

    juce::Label m_timecodeLabel;
    juce::TextEditor m_logText;

    juce::TextButton m_runButton;
    juce::TextButton m_stopButton;
    juce::TextButton m_clearButton;
    juce::TextButton m_copyButton;
    juce::TextButton m_exportButton;
    juce::TextButton m_closeButton;

    std::vector<MIDILogEntry> m_logEntries;
    bool m_isRunning = true;
    juce::CriticalSection m_logLock;

    void addMidiMessage(const juce::MidiMessage& message, const juce::String& source);
    void updateLogDisplay();
    void clearLog();
    void copyToClipboard();
    void exportToFile();
};

void MIDIMonitorDialog::addMidiMessage(const juce::MidiMessage& message,
                                       const juce::String& source) {
    if (!m_isRunning) return;

    juce::ScopedLock lock(m_logLock);

    MIDILogEntry entry;
    entry.timestamp = juce::Time::getCurrentTime();
    entry.source = source;
    entry.channel = message.getChannel();

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
    } else if (message.isMidiClock()) {
        entry.messageType = "Clock";
    } else {
        entry.messageType = "Other";
    }

    // Add to front (most recent first)
    m_logEntries.insert(m_logEntries.begin(), entry);

    // Limit log size
    if (m_logEntries.size() > 1000) {
        m_logEntries.resize(1000);
    }
}

void MIDIMonitorDialog::updateLogDisplay() {
    juce::ScopedLock lock(m_logLock);

    juce::String logText;

    for (const auto& entry : m_logEntries) {
        logText << entry.timestamp.formatted("%H:%M:%S.%f") << " | "
                << entry.source << " | "
                << "Ch:" << entry.channel << " | "
                << entry.messageType << " | "
                << entry.data1 << "/" << entry.data2 << "\n";
    }

    m_logText.setText(logText, false);
}

void MIDIMonitorDialog::timerCallback() {
    // Update display every 100ms
    updateLogDisplay();

    // Update timecode status
    if (midiManager->hasRecentTimecode()) {
        m_timecodeLabel.setColour(juce::Label::textColourId, juce::Colours::green);
        m_timecodeLabel.setText("MIDI Timecode: " +
                                midiManager->getCurrentTimecode().toString(),
                                juce::dontSendNotification);
    } else {
        m_timecodeLabel.setColour(juce::Label::textColourId, juce::Colours::red);
        m_timecodeLabel.setText("MIDI Timecode: Not detected",
                                juce::dontSendNotification);
    }
}
```

**B. Status Bar MIDI Indicator**

Show MIDI activity and timecode in status bar:

**Implementation:**

```cpp
// In main window status bar
class StatusBar : public juce::Component,
                  private juce::Timer {
public:
    void timerCallback() override {
        // Update MIDI timecode display
        if (midiManager->hasRecentTimecode()) {
            m_midiTimecodeLabel.setColour(juce::Label::textColourId, juce::Colours::green);
            m_midiTimecodeLabel.setText("TC: " + midiManager->getCurrentTimecode().toString(),
                                        juce::dontSendNotification);
        } else {
            m_midiTimecodeLabel.setColour(juce::Label::textColourId, juce::Colours::darkgrey);
            m_midiTimecodeLabel.setText("TC: --:--:--:--",
                                        juce::dontSendNotification);
        }

        // Flash on incoming MIDI message
        if (midiManager->hasRecentMidiActivity()) {
            m_midiActivityLabel.setColour(juce::Label::backgroundColourId, juce::Colours::red);
        } else {
            m_midiActivityLabel.setColour(juce::Label::backgroundColourId, juce::Colours::darkgrey);
        }
    }

private:
    juce::Label m_midiTimecodeLabel;
    juce::Label m_midiActivityLabel;
};
```

#### 2.2 Acceptance Criteria

- [ ] "Monitor" button in MIDI Devices dialog opens MIDI Monitor window
- [ ] MIDI Monitor displays timestamped messages (most recent first)
- [ ] Run/Stop buttons control logging
- [ ] Clear button clears log
- [ ] Copy button copies log to clipboard
- [ ] Export button saves log to file
- [ ] MIDI timecode label shows green if timecode detected, red otherwise
- [ ] Status bar shows MIDI timecode (if present)
- [ ] Status bar flashes red on MIDI activity
- [ ] Log limited to 1000 entries (rolling buffer)

#### 2.3 Technical Requirements

- Use `juce::Timer` for periodic UI updates (100ms)
- Thread-safe log entry (use `juce::CriticalSection`)
- Detect MIDI timecode (MTC quarter-frame messages)
- Parse MTC into HH:MM:SS:FF format
- Log format: "HH:MM:SS.ms | Source | Ch:N | Type | Data1/Data2"
- Export log as timestamped text file

---

## 3. Deferred Features (Lower Priority)

### Sprint 13: GPI Trigger System (LOW - Phase 3)

**Complexity:** Extra Large (XL)
**Estimated Duration:** 4-5 weeks
**Priority:** LOW (Phase 3)
**Rationale:** Legacy hardware, limited use case for modern workflows

**Features (if implemented):**

- Game port joystick support (GPI 1-4)
- Up to 64 GPIs with emulation
- GPI actions: Play, Stop, Pause, UnPause, Step to Next
- GPI emulation: Hotkey, PC Clock, MIDI Note, MIDI TC, SMPTE TC
- Visual indicators (LED, status bar)
- HoldOff period (debounce)

**Decision:** Defer to Phase 3. Focus on MIDI/OSC for external control (modern workflow).

---

### Sprint 14: Timecode Support (MEDIUM - Phase 2)

**Complexity:** Large (L)
**Estimated Duration:** 2-3 weeks
**Priority:** MEDIUM (Phase 2)
**Rationale:** Broadcast/theater requirement, but lower priority than data safety

**Features (if implemented):**

- MIDI Timecode (MTC) parsing
- SMPTE LTC timecode input (via audio interface)
- Timecode formats: 24NDF, 25NDF, 30NDF, 29.97DF
- Timecode-triggered clip playback
- Auto-arm mode (re-arm when timecode rewinds)
- Timecode nudge (adjust trigger times)

**Decision:** Defer to Phase 2. Focus on file safety (auto-backup) first.

---

## 4. Implementation Roadmap

### Phase 1: External Tools and HotKeys (Weeks 1-3)

**Focus:** User workflow enhancement, hotkey flexibility

1. **Sprint 9:** External Tool Registry (1 week)
   - WAV editor, search utility configuration
   - Right-click menu integration

2. **Sprint 10:** HotKey Configuration System (2 weeks)
   - Global/Paged scope
   - Ganged/Overlapped modes
   - HotKey Setup dialog

### Phase 2: MIDI Infrastructure (Weeks 4-7)

**Focus:** External control, MIDI monitoring

3. **Sprint 11:** MIDI Device Management (3 weeks)
   - Multiple MIDI In/Out devices
   - Global/Paged scope, Ganged/Overlapped
   - MIDI note assignment per clip

4. **Sprint 12:** MIDI Monitoring and Logging (1 week)
   - MIDI Monitor window
   - Status bar MIDI indicators
   - Timecode detection

### Phase 3: Advanced Control (Future)

**Focus:** Legacy hardware, timecode triggers

5. **Sprint 13:** GPI Trigger System (Deferred)
6. **Sprint 14:** Timecode Support (Deferred)

---

## 5. Cross-Cutting Concerns

### 5.1 Threading Model

All Setup Menu features respect OCC's threading model:

**Message Thread:**

- External tool launch (blocking process start is OK)
- MIDI device enumeration/configuration
- HotKey configuration dialogs
- MIDI monitoring UI updates

**Audio Thread:**

- MIDI message handling (lock-free dispatch to clip triggers)
- **NO MIDI device open/close** (must be done on message thread)

**Background Threads:**

- MIDI monitoring log writes (optional buffering)

### 5.2 Persistence

**Application-Wide Preferences (NOT session-specific):**

- External tool paths (WAV editor, search utility)
- HotKey scope (Global/Paged)
- HotKey multi-button action (Ganged/Overlapped)
- MIDI device selection (enabled devices)
- MIDI scope (Global/Paged)
- MIDI multi-note action (Ganged/Overlapped)
- Send "All Notes Off" on panic (enabled/disabled)

**Session-Specific Settings:**

- MIDI note assignment per clip
- Hotkey assignment per clip (already exists)

### 5.3 Cross-Platform Compatibility

**Platform-Specific Considerations:**

- **macOS:** MIDI device names (CoreMIDI)
- **Windows:** MIDI device names (WinMM, DirectSound)
- **Linux:** MIDI device names (ALSA)
- External tool paths (_.app on macOS, _.exe on Windows, no extension on Linux)

### 5.4 Performance

**Critical Metrics:**

- MIDI message latency: <5ms (from MIDI In to clip trigger)
- HotKey latency: <10ms (from key press to clip trigger)
- MIDI Monitor UI update: 10 FPS (100ms timer)
- External tool launch: <1s (spawn process)

---

## 6. Testing Strategy

### 6.1 Unit Tests

**Test Coverage:**

- `ExternalToolManager` tool path validation
- `HotKeyManager` scope and action logic
- `MIDIDeviceManager` device enumeration
- `MIDIDeviceManager` Ganged vs. Overlapped modes
- MIDI message parsing (Note On/Off, CC, timecode)

### 6.2 Integration Tests

**Test Scenarios:**

- Launch external WAV editor with audio file path
- HotKey triggers clip in Global scope (across tabs)
- HotKey triggers clip in Paged scope (current tab only)
- Overlapped hotkey plays clips in sequence
- MIDI Note triggers clip in Global scope
- MIDI Note triggers clip in Paged scope
- Overlapped MIDI note plays clips in sequence
- "All Notes Off" sent on panic

### 6.3 Manual Testing

**Test Cases:**

- Connect multiple MIDI controllers (test multi-device)
- Assign same hotkey to 3+ buttons (test Ganged/Overlapped)
- Assign same MIDI note to 3+ buttons (test Ganged/Overlapped)
- Right-click clip button → "Edit in Audition" (test external tool launch)
- MIDI Monitor shows real-time messages
- MIDI timecode detected and displayed

---

## 7. Documentation Requirements

### 7.1 User Documentation

**Topics:**

- External tool configuration (WAV editor, search utility)
- HotKey scope (Global vs. Paged)
- HotKey multi-button actions (Ganged vs. Overlapped)
- MIDI device configuration
- MIDI note assignment to clips
- MIDI learn mode
- MIDI monitoring
- Timecode display (status bar)

### 7.2 Developer Documentation

**Topics:**

- External tool registry API
- HotKey manager API
- MIDI device manager API
- MIDI message handling (lock-free)
- Overlapped mode implementation (round-robin)
- Timecode parsing (MTC quarter-frames)

---

## 8. Dependencies on Other OCC Systems

### 8.1 OCC115 Sprint 1 - Application Data Folder

**Dependency:** External tool paths stored in preferences folder

**Impact:** Must complete OCC115 Sprint 1 before Sprint 9.

### 8.2 OCC115 Sprint 5 - Event Logging

**Dependency:** MIDI messages logged to event log

**Impact:** Recommended to complete OCC115 Sprint 5 before Sprint 12.

### 8.3 AudioEngine API Extensions

**New Methods Required:**

```cpp
// In AudioEngine.h
int64_t getClipRemainingTime(int buttonIndex) const;  // For overlapped mode
bool isClipPlaying(int buttonIndex) const;             // Already exists
```

---

## 9. Success Metrics

### 9.1 User Workflow Efficiency

- External tool launch reduces round-trip time by >50% (no manual file open)
- HotKey overlapped mode enables sound effect layering (3+ simultaneous triggers)
- MIDI control enables hands-free operation (keyboard shortcuts not required)

### 9.2 External Control Reliability

- MIDI message latency <5ms (sample-accurate triggering)
- MIDI multi-device support handles 3+ controllers concurrently
- HotKey global scope enables cross-tab triggering (8-tab navigation)

### 9.3 Monitoring and Diagnostics

- MIDI Monitor captures 100% of incoming messages
- Timecode detection works with MTC and SMPTE sources
- Status bar provides real-time MIDI activity feedback

---

## 10. Related Documentation

- **OCC114** - Backend Infrastructure Audit (Current State)
- **OCC115** - File Menu Backend Features (Session Management)
- **SpotOn Manual Section 02** - Setup Menu (Peer Analysis)

---

**Last Updated:** 2025-11-12
**Maintainer:** OCC Development Team
**Status:** Planning
**Sprint Context:** Backend Architecture Sprint
