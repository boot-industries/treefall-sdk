# OCC117 - Display and Edit Menu Backend Features - Sprint Plan

**Version:** 1.0
**Date:** 2025-11-12
**Status:** Planning
**Sprint Context:** Backend Architecture Sprint (SpotOn Section 03 Analysis)
**Dependencies:** OCC114 (Codebase Audit), OCC115 (File Menu), OCC116 (Setup Menu)

---

## Executive Summary

This document presents a comprehensive sprint plan for implementing Display Menu and Edit Menu backend features in Clip Composer, based on analysis of the SpotOn Manual Section 03 (Display Menu). SpotOn demonstrates mature UI configuration infrastructure, advanced editing workflows (Paste Special, Undo/Redo), and monitoring capabilities (Level Meters, Play History) that OCC currently lacks.

**Key Finding:** SpotOn's Display/Edit menus reveal **4 major backend systems** for presentation configuration and editing workflows. OCC has basic UI but lacks persistent display preferences, clipboard operations, undo/redo, and output monitoring.

---

## 1. SpotOn Display/Edit Menu Analysis Summary

### 1.1 Core Features Identified

| Feature | SpotOn Has? | OCC Has? | Priority |
|---------|-------------|----------|----------|
| **Grid Layout Configuration** | ✅ Yes (1-160 buttons/page, 8 pages) | ⚠️ Partial (Fixed 48/page, 8 pages) | MEDIUM |
| **Page Name Customization** | ✅ Yes (Edit/Reset, right-click) | ❌ No | LOW |
| **Display Size Settings** | ✅ Yes (Tab/Status/Bevel/Trigger sizes) | ❌ No | LOW |
| **Display Mode Toggles** | ✅ Yes (HotKey/MIDI text, elapsed time) | ⚠️ Partial | MEDIUM |
| **Level Meters with Logging** | ✅ Yes (4 outputs, play history) | ❌ No | HIGH |
| **Undo/Redo System** | ✅ Yes (Multi-level, Ctrl+Z/Y) | ❌ No | HIGH |
| **Paste Special** | ✅ Yes (Selective parameters, AutoFill) | ❌ No | HIGH |
| **Page Operations** | ✅ Yes (Cut/Copy/Paste/Swap/Clear) | ❌ No | MEDIUM |
| **Fill/Clear Buttons** | ✅ Yes (Range operations) | ❌ No | MEDIUM |

---

### 1.2 Key Insights

**SpotOn's Strengths:**
1. **UI Flexibility:** Grid layout (1-160 buttons/page), touchscreen sizes
2. **Editing Workflow:** Paste Special with selective parameters, AutoFill MIDI notes
3. **Undo/Redo:** Multi-level command history (shows count: "Undo (3)")
4. **Output Monitoring:** Level meters per output, timestamped play history (100 tracks)
5. **Display Modes:** HotKey/MIDI text toggle, elapsed vs. countdown time

**OCC's Gaps:**
- ❌ **No undo/redo system** (destructive edits, no history)
- ❌ **No paste special** (can't selectively copy parameters)
- ⚠️ **Basic clipboard** (no page copy/paste, no swap)
- ❌ **No level meters** (no per-output monitoring)
- ❌ **No play history logging** (can't see recent tracks per output)
- ❌ **No display preferences** (sizes, modes not persistent)

---

## 2. Sprint Breakdown

### Sprint 15: Display Preferences System (MEDIUM)
**Complexity:** Small (S)
**Estimated Duration:** 3-5 days
**Priority:** MEDIUM
**Dependencies:** OCC115 Sprint 1 (Application Data Folder)

#### 2.1 Scope

**A. Display Preferences Storage**

Store application-wide display settings in preferences:

**Features:**
- Page tab height (Small/Medium/Large)
- Status bar height (Small/Medium/Large)
- Bevel edge width (None/5%/10%/15%/20%)
- Button trigger size (Small/Medium/Large)
- Display mode toggles (HotKey Text, MIDI Text, Button Triggers, Edged Text)
- Elapsed time mode (Countdown vs Count-up)
- Level meter display mode (Horizontal vs Vertical)

**Implementation:**
```cpp
// New class: DisplayPreferences
class DisplayPreferences {
public:
    enum class Size {
        Small,
        Medium,
        Large
    };

    enum class BevelWidth {
        None,
        Percent5,
        Percent10,
        Percent15,
        Percent20
    };

    enum class ButtonTextMode {
        None,
        HotKey,
        MidiNote
    };

    DisplayPreferences();

    // Page tab settings
    void setPageTabHeight(Size size);
    Size getPageTabHeight() const { return m_pageTabHeight; }

    // Status bar settings
    void setStatusBarHeight(Size size);
    Size getStatusBarHeight() const { return m_statusBarHeight; }

    // Button appearance
    void setBevelWidth(BevelWidth width);
    BevelWidth getBevelWidth() const { return m_bevelWidth; }

    void setButtonTriggerSize(Size size);
    Size getButtonTriggerSize() const { return m_buttonTriggerSize; }

    // Display modes
    void setButtonTextMode(ButtonTextMode mode);
    ButtonTextMode getButtonTextMode() const { return m_buttonTextMode; }

    void setShowButtonTriggers(bool show);
    bool getShowButtonTriggers() const { return m_showButtonTriggers; }

    void setEdgedText(bool edged);
    bool getEdgedText() const { return m_edgedText; }

    void setElapsedTimeMode(bool elapsed);  // false = countdown, true = elapsed
    bool getElapsedTimeMode() const { return m_elapsedTimeMode; }

    // Level meter
    void setLevelMeterVertical(bool vertical);
    bool getLevelMeterVertical() const { return m_levelMeterVertical; }

    // Persistence
    void saveToPreferences(juce::PropertiesFile& props);
    void loadFromPreferences(juce::PropertiesFile& props);

private:
    Size m_pageTabHeight = Size::Medium;
    Size m_statusBarHeight = Size::Medium;
    BevelWidth m_bevelWidth = BevelWidth::Percent10;
    Size m_buttonTriggerSize = Size::Medium;
    ButtonTextMode m_buttonTextMode = ButtonTextMode::HotKey;
    bool m_showButtonTriggers = true;
    bool m_edgedText = false;
    bool m_elapsedTimeMode = false;  // false = countdown
    bool m_levelMeterVertical = false;  // false = horizontal
};
```

**B. Display Menu Implementation**

Add Display menu with configuration dialogs:

**Menu Structure:**
```
Display
├── Layout...                    (Future: Grid layout editor)
├── Page Names                   ▶
│   ├── Edit Page Names...       (Opens page name editor)
│   └── Reset Page Names...      (Confirmation dialog)
├── Page Tab Height              ▶
│   ├── ● Small
│   ├── ○ Medium
│   └── ○ Large
├── Status Bar Height            ▶
│   ├── ○ Small
│   ├── ● Medium
│   └── ○ Large
├── Bevel Edge Width (10%)       ▶
│   ├── None, 5%, 10%, 15%, 20%
├── Button Trigger Size (M)      ▶
│   ├── Small, Medium, Large
├── ✓ HotKey Text                (Radio group with MIDI Text)
├── ○ Midi In Text               (Radio group with HotKey Text)
├── ✓ Button Triggers            (Show remote trigger indicator)
├── Edged Midi/HotKey Text       (Drop shadow)
├── Elapsed Time Count  Ctrl+T   (Toggle countdown/elapsed)
└── Level Meters        Ctrl+L   (Opens level meter window)
```

**C. Page Name Customization**

Allow users to customize page names (session-specific):

**Implementation:**
```cpp
// In SessionManager.h
struct SessionData {
    // ... existing fields ...

    std::array<juce::String, 8> pageNames = {"1", "2", "3", "4", "5", "6", "7", "8"};
};

// New dialog: PageNameEditor
class PageNameEditor : public juce::Component {
public:
    PageNameEditor(SessionManager* sessionManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    SessionManager* m_sessionManager;

    juce::Label m_titleLabel;
    juce::ListBox m_pageList;
    juce::TextEditor m_nameEditor;

    juce::TextButton m_resetButton;
    juce::TextButton m_okButton;
    juce::TextButton m_cancelButton;

    std::array<juce::String, 8> m_pageNames;

    void onPageSelected(int pageIndex);
    void onResetButtonClicked();
    void applyChanges();
};
```

**D. Elapsed Time Toggle**

Toggle between countdown (time remaining) and count-up (elapsed time):

**Implementation:**
```cpp
// In ClipButton::paint()
void ClipButton::paint(juce::Graphics& g) {
    // ... existing button rendering ...

    // Draw time display
    auto timeStr = getTimeDisplayString();
    g.drawText(timeStr, getTimeDisplayBounds(), juce::Justification::centredRight);
}

juce::String ClipButton::getTimeDisplayString() const {
    if (!m_isPlaying) {
        return formatTime(m_clipData.durationSamples);
    }

    auto elapsed = audioEngine->getClipElapsedTime(m_buttonIndex);
    auto remaining = audioEngine->getClipRemainingTime(m_buttonIndex);

    if (displayPreferences->getElapsedTimeMode()) {
        // Count-up mode (elapsed time)
        return formatTime(elapsed);
    } else {
        // Countdown mode (time remaining)
        return formatTime(remaining);
    }
}
```

#### 2.2 Acceptance Criteria

- [ ] User can set page tab height (Small/Medium/Large) via Display menu
- [ ] User can set status bar height (Small/Medium/Large) via Display menu
- [ ] User can set bevel edge width (None/5%/10%/15%/20%) via Display menu
- [ ] User can set button trigger size (Small/Medium/Large) via Display menu
- [ ] User can toggle HotKey Text / MIDI Text / None (mutually exclusive)
- [ ] User can toggle "Button Triggers" indicator (remote control visual)
- [ ] User can toggle "Edged Text" (drop shadow on HotKey/MIDI text)
- [ ] User can toggle elapsed time mode (Ctrl+T keyboard shortcut)
- [ ] User can edit page names via Display > Page Names > Edit
- [ ] User can reset page names to defaults (1..8) with confirmation
- [ ] Right-click page tab shows "Edit Tab Name..." context menu
- [ ] Caret character (^) forces line break in page names
- [ ] All display settings persist across app restarts (application preferences)
- [ ] Page names persist in session file (session-specific)

#### 2.3 Technical Requirements

- Store display settings in `PropertiesFile` (application preferences)
- Store page names in session JSON (session-specific)
- Update ClipButton rendering based on display preferences
- Update TabComponent height based on page tab height setting
- Update StatusBar height based on status bar height setting
- Implement Ctrl+T keyboard shortcut for elapsed time toggle
- Right-click context menu on page tabs
- Confirmation dialog for "Reset Page Names"

---

### Sprint 16: Undo/Redo System (HIGH)
**Complexity:** Large (L)
**Estimated Duration:** 2-3 weeks
**Priority:** HIGH
**Dependencies:** None

#### 2.1 Scope

**A. Command Pattern Implementation**

Implement undo/redo using Command pattern:

**Features:**
- Multi-level undo/redo stack (configurable max depth)
- Show undo count in menu ("Undo (3)")
- Keyboard shortcuts (Ctrl+Z, Ctrl+Y)
- Session dirty tracking (mark session modified after commands)
- Command types: ClipEdit, ClipDelete, PageCopy, PasteSpecial, FillButtons, ClearButtons

**Implementation:**
```cpp
// New class: UndoManager
class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual juce::String getDescription() const = 0;
};

class UndoManager {
public:
    UndoManager(size_t maxDepth = 50);

    // Execute command and add to undo stack
    void executeCommand(std::unique_ptr<Command> command);

    // Undo/Redo
    bool canUndo() const { return m_undoIndex > 0; }
    bool canRedo() const { return m_undoIndex < m_commandStack.size(); }

    void undo();
    void redo();

    // Get descriptions for menu display
    juce::String getUndoDescription() const;
    juce::String getRedoDescription() const;

    int getUndoCount() const { return m_undoIndex; }
    int getRedoCount() const { return static_cast<int>(m_commandStack.size()) - m_undoIndex; }

    // Clear stack (on session load/new)
    void clear();

private:
    std::vector<std::unique_ptr<Command>> m_commandStack;
    size_t m_undoIndex = 0;
    size_t m_maxDepth;

    void trimStack();
};
```

**B. Concrete Command Implementations**

Implement commands for common operations:

**Implementation:**
```cpp
// Command: Edit Clip Properties
class EditClipCommand : public Command {
public:
    EditClipCommand(SessionManager* sessionManager,
                    int buttonIndex,
                    const ClipData& oldData,
                    const ClipData& newData)
        : m_sessionManager(sessionManager),
          m_buttonIndex(buttonIndex),
          m_oldData(oldData),
          m_newData(newData) {}

    void execute() override {
        m_sessionManager->setClip(m_buttonIndex, m_newData);
    }

    void undo() override {
        m_sessionManager->setClip(m_buttonIndex, m_oldData);
    }

    juce::String getDescription() const override {
        return "Edit Clip " + juce::String(m_buttonIndex + 1);
    }

private:
    SessionManager* m_sessionManager;
    int m_buttonIndex;
    ClipData m_oldData;
    ClipData m_newData;
};

// Command: Clear Buttons
class ClearButtonsCommand : public Command {
public:
    ClearButtonsCommand(SessionManager* sessionManager,
                        int startButton,
                        int endButton)
        : m_sessionManager(sessionManager),
          m_startButton(startButton),
          m_endButton(endButton) {

        // Save current state
        for (int i = startButton; i <= endButton; ++i) {
            m_savedClips.push_back(m_sessionManager->getClip(i));
        }
    }

    void execute() override {
        for (int i = m_startButton; i <= m_endButton; ++i) {
            m_sessionManager->clearClip(i);
        }
    }

    void undo() override {
        for (size_t i = 0; i < m_savedClips.size(); ++i) {
            m_sessionManager->setClip(m_startButton + i, m_savedClips[i]);
        }
    }

    juce::String getDescription() const override {
        return "Clear Buttons " + juce::String(m_startButton + 1) + ".." +
               juce::String(m_endButton + 1);
    }

private:
    SessionManager* m_sessionManager;
    int m_startButton;
    int m_endButton;
    std::vector<ClipData> m_savedClips;
};

// Command: Paste Special
class PasteSpecialCommand : public Command {
public:
    PasteSpecialCommand(SessionManager* sessionManager,
                        const std::vector<int>& targetButtons,
                        const ClipData& sourceClip,
                        const PasteSpecialOptions& options)
        : m_sessionManager(sessionManager),
          m_targetButtons(targetButtons),
          m_sourceClip(sourceClip),
          m_options(options) {

        // Save current state
        for (int buttonIndex : targetButtons) {
            m_savedClips[buttonIndex] = m_sessionManager->getClip(buttonIndex);
        }
    }

    void execute() override {
        for (int buttonIndex : m_targetButtons) {
            auto clipData = m_sessionManager->getClip(buttonIndex);
            applyPasteSpecial(clipData, m_sourceClip, m_options);
            m_sessionManager->setClip(buttonIndex, clipData);
        }
    }

    void undo() override {
        for (const auto& [buttonIndex, clipData] : m_savedClips) {
            m_sessionManager->setClip(buttonIndex, clipData);
        }
    }

    juce::String getDescription() const override {
        return "Paste Special (" + juce::String(m_targetButtons.size()) + " buttons)";
    }

private:
    SessionManager* m_sessionManager;
    std::vector<int> m_targetButtons;
    ClipData m_sourceClip;
    PasteSpecialOptions m_options;
    std::map<int, ClipData> m_savedClips;

    void applyPasteSpecial(ClipData& target, const ClipData& source,
                           const PasteSpecialOptions& opts);
};
```

**C. Edit Menu Integration**

Add Undo/Redo to Edit menu:

**Menu Structure:**
```
Edit
├── ↶ Undo (3)          Ctrl+Z
├── ↷ Redo              Ctrl+Y
├── ──────────────
├── Paste Special...
├── ──────────────
├── Cut Page
├── Copy Page
├── Paste Page
├── Swap Page
├── Clear Page...
├── ──────────────
├── Fill Buttons...
└── Clear Buttons...
```

**Implementation:**
```cpp
// In MainComponent
void MainComponent::createEditMenu() {
    juce::PopupMenu menu;

    // Undo/Redo
    auto undoDesc = undoManager->canUndo() ?
        "Undo (" + juce::String(undoManager->getUndoCount()) + ")" :
        "Undo";
    menu.addItem(1, undoDesc, undoManager->canUndo(), false, juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0));

    auto redoDesc = undoManager->canRedo() ?
        "Redo" :
        "Redo";
    menu.addItem(2, redoDesc, undoManager->canRedo(), false, juce::KeyPress('y', juce::ModifierKeys::commandModifier, 0));

    menu.addSeparator();

    // ... rest of Edit menu ...
}

void MainComponent::handleEditMenuResult(int result) {
    switch (result) {
        case 1: undoManager->undo(); break;
        case 2: undoManager->redo(); break;
        // ... other menu items ...
    }
}
```

#### 2.2 Acceptance Criteria

- [ ] User can undo last edit (Ctrl+Z)
- [ ] User can redo last undo (Ctrl+Y)
- [ ] Undo count shown in menu ("Undo (3)")
- [ ] Undo disabled when stack empty
- [ ] Redo disabled when at top of stack
- [ ] Undo/redo works for clip edits (name, color, gain, etc.)
- [ ] Undo/redo works for clear buttons
- [ ] Undo/redo works for fill buttons
- [ ] Undo/redo works for paste special
- [ ] Undo/redo works for page operations (copy/paste/swap/clear)
- [ ] Undo stack cleared on session load/new
- [ ] Undo stack limited to max depth (50 commands)
- [ ] Session marked dirty after executing command

#### 2.3 Technical Requirements

- Implement Command pattern (base class + concrete commands)
- Store command stack in UndoManager
- Snapshot clip data before/after edits
- Keyboard shortcuts (Ctrl+Z, Ctrl+Y)
- Update menu items dynamically (show count, enable/disable)
- Clear undo stack on session load/new
- Test with all edit operations (clip edit, clear, fill, paste special)

---

### Sprint 17: Paste Special System (HIGH)
**Complexity:** Large (L)
**Estimated Duration:** 2-3 weeks
**Priority:** HIGH
**Dependencies:** Sprint 16 (Undo/Redo System)

#### 2.1 Scope

**A. Paste Special Dialog**

Selective parameter copying from clipboard:

**Features:**
- Parameter selection checkboxes (Levels, Fades, External Triggers, Misc)
- Paste scope: Individual, Global, Current Page, Range, Output-specific
- AutoFill for MIDI notes (sequential assignment)
- Gain Absolute vs. Gain Relative
- Confirmation dialog before apply

**Implementation:**
```cpp
// New class: PasteSpecialDialog
struct PasteSpecialOptions {
    // Levels
    bool mute = false;
    bool pan = false;
    bool gainAbsolute = false;
    bool gainRelative = false;
    float gainRelativeDb = 0.0f;

    // Fades
    bool fadeIn = false;
    bool fadeInLaw = false;
    bool fadeOut = false;
    bool fadeOutLaw = false;

    // External Triggers
    bool midiInPlay = false;
    bool midiInPlayAutoFill = false;
    bool midiInStop = false;
    bool midiPlayOut = false;
    bool midiPlayOutAutoFill = false;

    // Misc
    bool output = false;
    bool color = false;
    bool group = false;
    bool speed = false;
    bool loop = false;
    bool hotKey = false;
    bool playNext = false;
    bool playDelay = false;
    bool stopAll = false;

    // Paste scope
    enum class Scope {
        Individual,   // Apply to individual buttons (one at a time)
        Global,       // Apply to all buttons across all pages
        CurrentPage,  // Apply to all buttons on current page
        Range         // Apply to range of buttons (from..to)
    };
    Scope scope = Scope::Individual;

    int rangeStart = 1;
    int rangeEnd = 320;

    bool restrictToOutput = false;
    juce::String outputRestriction;  // e.g., "A", "B", "C", "D"
};

class PasteSpecialDialog : public juce::Component {
public:
    PasteSpecialDialog(SessionManager* sessionManager,
                       const ClipData& sourceClip);

    void paint(juce::Graphics& g) override;
    void resized() override;

    const PasteSpecialOptions& getOptions() const { return m_options; }

private:
    SessionManager* m_sessionManager;
    ClipData m_sourceClip;
    PasteSpecialOptions m_options;

    // Levels section
    juce::ToggleButton m_muteCheckbox;
    juce::ToggleButton m_panCheckbox;
    juce::ToggleButton m_gainAbsoluteCheckbox;
    juce::ToggleButton m_gainRelativeCheckbox;
    juce::Slider m_gainRelativeSlider;

    // Fades section
    juce::ToggleButton m_fadeInCheckbox;
    juce::ToggleButton m_fadeInLawCheckbox;
    juce::ToggleButton m_fadeOutCheckbox;
    juce::ToggleButton m_fadeOutLawCheckbox;

    // External Triggers section
    juce::ToggleButton m_midiInPlayCheckbox;
    juce::ToggleButton m_midiInPlayAutoFillCheckbox;
    juce::ToggleButton m_midiInStopCheckbox;
    juce::ToggleButton m_midiPlayOutCheckbox;
    juce::ToggleButton m_midiPlayOutAutoFillCheckbox;

    // Misc section
    juce::ToggleButton m_outputCheckbox;
    juce::ToggleButton m_colorCheckbox;
    juce::ToggleButton m_groupCheckbox;
    juce::ToggleButton m_speedCheckbox;
    juce::ToggleButton m_loopCheckbox;
    juce::ToggleButton m_hotKeyCheckbox;
    juce::ToggleButton m_playNextCheckbox;
    juce::ToggleButton m_playDelayCheckbox;
    juce::ToggleButton m_stopAllCheckbox;

    // Paste scope section
    juce::Label m_scopeLabel;
    juce::ToggleButton m_scopeIndividualRadio;
    juce::ToggleButton m_scopeGlobalRadio;
    juce::ToggleButton m_scopeCurrentPageRadio;
    juce::ToggleButton m_scopeRangeRadio;
    juce::TextEditor m_rangeStartEditor;
    juce::TextEditor m_rangeEndEditor;
    juce::ToggleButton m_restrictToOutputCheckbox;
    juce::ComboBox m_outputRestrictionCombo;

    juce::TextButton m_clearAllButton;
    juce::TextButton m_okButton;
    juce::TextButton m_cancelButton;

    void onGainRelativeCheckboxChanged();
    void onFadeInCheckboxChanged();
    void onFadeOutCheckboxChanged();
    void onMidiAutoFillChanged();
    void onScopeRadioChanged();
    void onClearAllButtonClicked();
    void onOkButtonClicked();
};
```

**B. AutoFill MIDI Notes**

Automatically assign sequential MIDI notes:

**Implementation:**
```cpp
// In PasteSpecialCommand::execute()
void PasteSpecialCommand::execute() {
    int autoFillIndex = 0;

    for (int buttonIndex : m_targetButtons) {
        auto clipData = m_sessionManager->getClip(buttonIndex);

        // Apply selected parameters
        if (m_options.pan) clipData.pan = m_sourceClip.pan;
        if (m_options.color) clipData.color = m_sourceClip.color;
        if (m_options.fadeOut) clipData.fadeOut = m_sourceClip.fadeOut;
        if (m_options.fadeOutLaw) clipData.fadeOutLaw = m_sourceClip.fadeOutLaw;

        // Gain (Absolute or Relative)
        if (m_options.gainAbsolute) {
            clipData.gainDb = m_sourceClip.gainDb;
        } else if (m_options.gainRelative) {
            clipData.gainDb += m_options.gainRelativeDb;
        }

        // MIDI In/Play with AutoFill
        if (m_options.midiInPlay) {
            if (m_options.midiInPlayAutoFill) {
                // AutoFill: Increment MIDI note sequentially
                clipData.midiNote = m_sourceClip.midiNote + autoFillIndex;
                clipData.midiChannel = m_sourceClip.midiChannel;
                autoFillIndex++;
            } else {
                // No AutoFill: Use same MIDI note
                clipData.midiNote = m_sourceClip.midiNote;
                clipData.midiChannel = m_sourceClip.midiChannel;
            }
        }

        m_sessionManager->setClip(buttonIndex, clipData);
    }
}
```

**C. Confirmation Dialog**

Show summary before applying Paste Special:

**Implementation:**
```cpp
void PasteSpecialDialog::onOkButtonClicked() {
    // Build summary string
    juce::StringArray params;
    if (m_options.pan) params.add("Pan");
    if (m_options.color) params.add("Color");
    if (m_options.fadeOut) params.add("FadeOut");
    if (m_options.midiInPlay) params.add(m_options.midiInPlayAutoFill ? "MidiIn Play-Auto" : "MidiIn Play");

    juce::String scopeDesc;
    switch (m_options.scope) {
        case PasteSpecialOptions::Scope::Individual:
            scopeDesc = "individual buttons";
            break;
        case PasteSpecialOptions::Scope::Global:
            scopeDesc = "all buttons";
            break;
        case PasteSpecialOptions::Scope::CurrentPage:
            scopeDesc = "buttons on current page";
            break;
        case PasteSpecialOptions::Scope::Range:
            scopeDesc = "buttons " + juce::String(m_options.rangeStart) + ".." +
                        juce::String(m_options.rangeEnd);
            break;
    }

    if (m_options.restrictToOutput) {
        scopeDesc += " on output " + m_options.outputRestriction;
    }

    juce::String message = "Paste parameter(s):-\n" +
                           params.joinIntoString(",") +
                           "\nfrom Btn" + juce::String(m_sourceClip.buttonIndex + 1) +
                           " to " + scopeDesc;

    auto result = juce::AlertWindow::show(
        juce::MessageBoxIconType::QuestionIcon,
        "Confirm",
        message,
        "Yes", "No"
    );

    if (result == 1) {
        applyPasteSpecial();
        closeDialog();
    }
}
```

#### 2.2 Acceptance Criteria

- [ ] User can copy button to clipboard (right-click > Copy)
- [ ] User can access Paste Special from Edit menu
- [ ] Paste Special dialog shows all parameter categories (Levels, Fades, External Triggers, Misc)
- [ ] User can select individual parameters to paste
- [ ] User can choose Gain Absolute or Gain Relative (with dB offset)
- [ ] Fade law checkboxes automatically checked when fade time selected
- [ ] AutoFill checkboxes enabled only when MIDI paste selected AND scope is range
- [ ] User can select paste scope (Individual, Global, Current Page, Range)
- [ ] Range scope shows "From" and "To" text editors
- [ ] User can restrict paste to specific output (A/B/C/D)
- [ ] "Clear all Paste Special parameters" button clears all checkboxes
- [ ] Confirmation dialog shows summary before applying
- [ ] Paste Special integrated with Undo/Redo system
- [ ] Status bar hint shows selected parameters when using button right-click > Paste Special

#### 2.3 Technical Requirements

- Store clipboard clip data in application state
- Implement PasteSpecialOptions struct
- Implement PasteSpecialDialog UI component
- Implement applyPasteSpecial() logic
- Integrate with UndoManager (PasteSpecialCommand)
- AutoFill MIDI notes: sourceNote + index
- Test with all parameter combinations
- Test with all paste scopes (Individual, Global, Page, Range)
- Test output restriction filter

---

### Sprint 18: Page Operations and Bulk Edits (MEDIUM)
**Complexity:** Medium (M)
**Estimated Duration:** 1-2 weeks
**Priority:** MEDIUM
**Dependencies:** Sprint 16 (Undo/Redo), Sprint 17 (Paste Special)

#### 2.1 Scope

**A. Page Clipboard Operations**

Copy/paste/swap entire pages:

**Features:**
- Cut Page (copy + clear current page)
- Copy Page (copy current page to clipboard)
- Paste Page (paste clipboard onto current page)
- Swap Page (exchange current page with clipboard)
- Clear Page (clear all buttons on current page, with confirmation)

**Implementation:**
```cpp
// In SessionManager
class SessionManager {
public:
    // ... existing methods ...

    // Page operations
    void cutPage(int pageIndex);
    void copyPage(int pageIndex);
    void pastePage(int pageIndex);
    void swapPage(int pageIndex);
    void clearPage(int pageIndex);

    bool hasPageClipboard() const { return m_pageClipboard.has_value(); }

private:
    std::optional<std::array<ClipData, 48>> m_pageClipboard;
};

void SessionManager::copyPage(int pageIndex) {
    m_pageClipboard = std::array<ClipData, 48>();

    for (int i = 0; i < 48; ++i) {
        int buttonIndex = pageIndex * 48 + i;
        (*m_pageClipboard)[i] = m_clips[buttonIndex];
    }
}

void SessionManager::pastePage(int pageIndex) {
    if (!m_pageClipboard.has_value()) {
        return;
    }

    for (int i = 0; i < 48; ++i) {
        int buttonIndex = pageIndex * 48 + i;
        m_clips[buttonIndex] = (*m_pageClipboard)[i];
    }

    notifyListeners();
}

void SessionManager::swapPage(int pageIndex) {
    if (!m_pageClipboard.has_value()) {
        return;
    }

    std::array<ClipData, 48> tempPage;

    // Save current page
    for (int i = 0; i < 48; ++i) {
        int buttonIndex = pageIndex * 48 + i;
        tempPage[i] = m_clips[buttonIndex];
    }

    // Paste clipboard onto current page
    pastePage(pageIndex);

    // Update clipboard with saved page
    m_pageClipboard = tempPage;
}
```

**B. Fill Buttons**

Load same audio file into range of buttons:

**Implementation:**
```cpp
// New dialog: FillButtonsDialog
class FillButtonsDialog : public juce::Component {
public:
    FillButtonsDialog(SessionManager* sessionManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    SessionManager* m_sessionManager;

    juce::Label m_rangeLabel;
    juce::TextEditor m_rangeStartEditor;
    juce::TextEditor m_rangeEndEditor;
    juce::TextButton m_allButtonsButton;

    juce::TextButton m_okButton;
    juce::TextButton m_cancelButton;

    void onAllButtonsClicked();
    void onOkClicked();
};

void FillButtonsDialog::onOkClicked() {
    // Open file chooser
    juce::FileChooser chooser("Select Audio file to fill button/s",
                              juce::File::getSpecialLocation(
                                  juce::File::userHomeDirectory),
                              "*.wav;*.aiff;*.mp3;*.flac");

    if (chooser.browseForFileToOpen()) {
        auto selectedFile = chooser.getResult();

        int rangeStart = m_rangeStartEditor.getText().getIntValue();
        int rangeEnd = m_rangeEndEditor.getText().getIntValue();

        // Confirmation dialog
        juce::String message = "Load buttons " + juce::String(rangeStart) + ".." +
                               juce::String(rangeEnd) + " with " +
                               selectedFile.getFileName();

        auto result = juce::AlertWindow::show(
            juce::MessageBoxIconType::QuestionIcon,
            "Confirm",
            message,
            "OK", "Cancel"
        );

        if (result == 1) {
            // Execute command
            auto command = std::make_unique<FillButtonsCommand>(
                m_sessionManager, rangeStart - 1, rangeEnd - 1, selectedFile
            );
            undoManager->executeCommand(std::move(command));
            closeDialog();
        }
    }
}
```

**C. Clear Buttons**

Clear range of buttons:

**Implementation:**
```cpp
// New dialog: ClearButtonsDialog
class ClearButtonsDialog : public juce::Component {
public:
    ClearButtonsDialog(SessionManager* sessionManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    SessionManager* m_sessionManager;

    juce::Label m_rangeLabel;
    juce::TextEditor m_rangeStartEditor;
    juce::TextEditor m_rangeEndEditor;
    juce::TextButton m_allButtonsButton;

    juce::TextButton m_okButton;
    juce::TextButton m_cancelButton;

    void onAllButtonsClicked();
    void onOkClicked();
};

void ClearButtonsDialog::onOkClicked() {
    int rangeStart = m_rangeStartEditor.getText().getIntValue();
    int rangeEnd = m_rangeEndEditor.getText().getIntValue();

    // Confirmation dialog
    juce::String message = "Clear buttons " + juce::String(rangeStart) + ".." +
                           juce::String(rangeEnd);

    auto result = juce::AlertWindow::show(
        juce::MessageBoxIconType::QuestionIcon,
        "Confirm",
        message,
        "OK", "Cancel"
    );

    if (result == 1) {
        // Execute command
        auto command = std::make_unique<ClearButtonsCommand>(
            m_sessionManager, rangeStart - 1, rangeEnd - 1
        );
        undoManager->executeCommand(std::move(command));
        closeDialog();
    }
}
```

#### 2.2 Acceptance Criteria

- [ ] User can cut current page (Edit > Cut Page)
- [ ] User can copy current page (Edit > Copy Page)
- [ ] User can paste page from clipboard (Edit > Paste Page)
- [ ] User can swap current page with clipboard (Edit > Swap Page)
- [ ] User can clear current page (Edit > Clear Page..., with confirmation)
- [ ] Page operations disabled when no page in clipboard
- [ ] User can fill range of buttons with audio file (Edit > Fill Buttons...)
- [ ] Fill Buttons shows file chooser, then range dialog, then confirmation
- [ ] "All Buttons" button auto-fills range (1..320)
- [ ] User can clear range of buttons (Edit > Clear Buttons...)
- [ ] Clear Buttons shows range dialog, then confirmation
- [ ] All page operations integrated with Undo/Redo
- [ ] Keyboard shortcuts work (if defined)

#### 2.3 Technical Requirements

- Implement page clipboard (std::optional<std::array<ClipData, 48>>)
- Implement Cut/Copy/Paste/Swap/Clear commands
- Implement FillButtonsDialog and ClearButtonsDialog
- File chooser for Fill Buttons (*.wav, *.aiff, *.mp3, *.flac)
- Range validation (1..320, start <= end)
- Confirmation dialogs before destructive operations
- Integrate with UndoManager
- Update menu item enable/disable based on clipboard state

---

### Sprint 19: Level Meters and Play History (HIGH)
**Complexity:** Large (L)
**Estimated Duration:** 2-3 weeks
**Priority:** HIGH
**Dependencies:** OCC115 Sprint 5 (Event Logging)

#### 2.1 Scope

**A. Level Meters Window**

Display signal levels on first 4 outputs with clip tracking:

**Features:**
- Horizontal or vertical display mode
- Show up to 3 buttons per output (color-coded: Green=1, Yellow=2, Red=3+)
- Stop buttons per output (enable/disable toggle)
- Right-click individual button to stop
- Right-click output letter for timestamped play log (last 100 tracks)
- Tooltip on hover shows track count
- Keyboard shortcut: Ctrl+L

**Implementation:**
```cpp
// New class: LevelMetersWindow
class LevelMetersWindow : public juce::Component,
                          private juce::Timer {
public:
    LevelMetersWindow(AudioEngine* audioEngine,
                      SessionManager* sessionManager,
                      DisplayPreferences* displayPreferences);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    AudioEngine* m_audioEngine;
    SessionManager* m_sessionManager;
    DisplayPreferences* m_displayPreferences;

    struct OutputState {
        std::vector<int> playingButtons;  // Up to 3 shown
        float levelDb;
    };

    std::array<OutputState, 4> m_outputStates;

    juce::ToggleButton m_stopEnabledButton;
    std::array<juce::TextButton, 4> m_stopButtons;

    juce::Label m_displayModeLabel;

    void updateOutputStates();
    void paintHorizontalMode(juce::Graphics& g);
    void paintVerticalMode(juce::Graphics& g);

    void onOutputRightClick(int outputIndex);
    void onButtonRightClick(int outputIndex, int buttonIndex);
    void onOutputStopClicked(int outputIndex);
    void onDisplayModeClicked();
};

void LevelMetersWindow::updateOutputStates() {
    for (int outputIndex = 0; outputIndex < 4; ++outputIndex) {
        auto& state = m_outputStates[outputIndex];

        // Get playing buttons on this output
        state.playingButtons = m_audioEngine->getPlayingButtonsOnOutput(outputIndex);

        // Limit to 3 shown
        if (state.playingButtons.size() > 3) {
            state.playingButtons.resize(3);
        }

        // Get level
        state.levelDb = m_audioEngine->getOutputLevelDb(outputIndex);
    }
}

void LevelMetersWindow::paintHorizontalMode(juce::Graphics& g) {
    const int outputHeight = 100;

    for (int outputIndex = 0; outputIndex < 4; ++outputIndex) {
        auto& state = m_outputStates[outputIndex];

        int y = outputIndex * outputHeight;

        // Draw playing buttons (up to 3)
        for (size_t i = 0; i < state.playingButtons.size(); ++i) {
            int buttonIndex = state.playingButtons[i];
            auto clipData = m_sessionManager->getClip(buttonIndex);

            juce::Colour textColor = juce::Colours::green;
            if (state.playingButtons.size() == 2) textColor = juce::Colours::yellow;
            if (state.playingButtons.size() >= 3) textColor = juce::Colours::red;

            g.setColour(textColor);
            g.drawText(juce::String(buttonIndex + 1) + ": " + clipData.displayName,
                       10, y + i * 20, 300, 20,
                       juce::Justification::centredLeft);
        }

        // Draw level meter
        drawLevelMeter(g, 350, y, 200, outputHeight - 10, state.levelDb);

        // Draw output letter
        g.setColour(juce::Colours::lightblue);
        g.setFont(24.0f);
        g.drawText(juce::String(static_cast<char>('A' + outputIndex)),
                   600, y, 50, outputHeight,
                   juce::Justification::centred);
    }
}
```

**B. Play History Logging**

Track last 100 played tracks per output with timestamps:

**Implementation:**
```cpp
// New class: PlayHistoryLogger
class PlayHistoryLogger {
public:
    struct PlayLogEntry {
        juce::Time timestamp;
        int buttonIndex;
        juce::String displayName;
        juce::String filePath;
    };

    PlayHistoryLogger();

    void logPlay(int outputIndex, int buttonIndex, const juce::String& displayName,
                 const juce::String& filePath);

    std::vector<PlayLogEntry> getPlayHistory(int outputIndex) const;

    void clear(int outputIndex);
    void clearAll();

private:
    static constexpr size_t MAX_HISTORY_SIZE = 100;

    std::array<std::vector<PlayLogEntry>, 4> m_playHistory;
};

void PlayHistoryLogger::logPlay(int outputIndex, int buttonIndex,
                                 const juce::String& displayName,
                                 const juce::String& filePath) {
    if (outputIndex < 0 || outputIndex >= 4) return;

    PlayLogEntry entry;
    entry.timestamp = juce::Time::getCurrentTime();
    entry.buttonIndex = buttonIndex;
    entry.displayName = displayName;
    entry.filePath = filePath;

    // Add to front (most recent first)
    m_playHistory[outputIndex].insert(m_playHistory[outputIndex].begin(), entry);

    // Limit size
    if (m_playHistory[outputIndex].size() > MAX_HISTORY_SIZE) {
        m_playHistory[outputIndex].resize(MAX_HISTORY_SIZE);
    }
}
```

**C. Play History Dialog**

Show timestamped play log for specific output:

**Implementation:**
```cpp
// New dialog: PlayHistoryDialog
class PlayHistoryDialog : public juce::Component {
public:
    PlayHistoryDialog(int outputIndex,
                      PlayHistoryLogger* playHistoryLogger);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    int m_outputIndex;
    PlayHistoryLogger* m_playHistoryLogger;

    juce::Label m_titleLabel;
    juce::TextEditor m_logText;
    juce::TextButton m_closeButton;

    void updateLogDisplay();
};

void PlayHistoryDialog::updateLogDisplay() {
    auto playHistory = m_playHistoryLogger->getPlayHistory(m_outputIndex);

    juce::String logText;

    for (const auto& entry : playHistory) {
        logText << entry.timestamp.formatted("%H:%M:%S - %D09: ")
                << "Display Title\n";
        logText << entry.timestamp.formatted("%H:%M:%S - %D17: ")
                << entry.displayName << "\n";
        logText << entry.timestamp.formatted("%H:%M:%S - %D26: ")
                << "Untitled12\n";  // Example from SpotOn
        // ... format according to SpotOn's format ...
    }

    m_logText.setText(logText, false);
}
```

#### 2.2 Acceptance Criteria

- [ ] User can open Level Meters window (Display > Level Meters, Ctrl+L)
- [ ] Level Meters shows 4 outputs (A, B, C, D)
- [ ] Each output shows up to 3 playing buttons (numbered)
- [ ] Button text color-coded: Green (1 button), Yellow (2), Red (3+)
- [ ] Level meter bargraph per output
- [ ] Stop buttons per output (enable/disable toggle in status bar panel)
- [ ] Right-click individual button to stop it
- [ ] Right-click output letter shows "List Tracks played..." menu item
- [ ] Play history dialog shows last 100 tracks with timestamps
- [ ] Play history format matches SpotOn (HH:MM:SS - D09: Display Title)
- [ ] Tooltip on output shows track count when hovering
- [ ] Display mode toggle (Horizontal vs Vertical)
- [ ] Vertical mode shows simpler display (no track names, just meters)
- [ ] Vertical mode tooltip shows track count
- [ ] Vertical mode right-click still shows play history

#### 2.3 Technical Requirements

- Implement LevelMetersWindow UI component
- Track playing buttons per output in AudioEngine
- Implement PlayHistoryLogger (circular buffer, 100 entries per output)
- Log play events when clip starts (timestamp, button, name, file)
- Implement PlayHistoryDialog
- Update level meters at 10 FPS (100ms timer)
- Display mode toggle (horizontal/vertical) stored in DisplayPreferences
- Stop buttons per output (call AudioEngine::stopAllOnOutput(index))
- Right-click context menus (stop button, list tracks)
- Color-coded text rendering (Green/Yellow/Red)

---

## 3. Deferred Features (Lower Priority)

### Sprint 20: Grid Layout Configuration (LOW - Phase 3)
**Complexity:** Large (L)
**Estimated Duration:** 2-3 weeks
**Priority:** LOW (Phase 3)
**Rationale:** OCC's fixed 8x6 grid (48 buttons/page) is sufficient for MVP

**Features (if implemented):**
- Interactive grid layout editor (drag to configure)
- Variable button counts (1-160 buttons/page)
- Dynamic page count (based on total buttons / buttons per page)
- Grid layout persistence (session-specific)

**Decision:** Defer to Phase 3. Focus on editing workflows (Undo/Redo, Paste Special) first.

---

## 4. Implementation Roadmap

### Phase 1: Display Preferences and Editing Foundation (Weeks 1-4)
**Focus:** UI configuration, undo/redo infrastructure

1. **Sprint 15:** Display Preferences System (1 week)
   - Page names, display sizes, mode toggles
   - Application preferences storage

2. **Sprint 16:** Undo/Redo System (2-3 weeks)
   - Command pattern implementation
   - Multi-level undo stack
   - Edit menu integration

### Phase 2: Advanced Editing and Paste Special (Weeks 5-8)
**Focus:** Selective parameter copying, bulk operations

3. **Sprint 17:** Paste Special System (2-3 weeks)
   - Parameter selection dialog
   - AutoFill MIDI notes
   - Confirmation dialogs

4. **Sprint 18:** Page Operations and Bulk Edits (1-2 weeks)
   - Page clipboard (Cut/Copy/Paste/Swap)
   - Fill/Clear Buttons

### Phase 3: Monitoring and Diagnostics (Weeks 9-11)
**Focus:** Output monitoring, play history logging

5. **Sprint 19:** Level Meters and Play History (2-3 weeks)
   - Level meters window
   - Play history logging
   - Timestamped play log per output

### Phase 4: Advanced Layout (Future)
**Focus:** Grid layout customization

6. **Sprint 20:** Grid Layout Configuration (Deferred)

---

## 5. Cross-Cutting Concerns

### 5.1 Threading Model

All Display/Edit Menu features respect OCC's threading model:

**Message Thread:**
- Display preferences dialogs
- Undo/Redo command execution
- Paste Special dialogs
- Page operations
- Level meters UI updates (100ms timer)

**Audio Thread:**
- Level meter data collection (output levels, playing buttons)
- Play history logging (non-blocking, lock-free queue)

**Background Threads:**
- None required for Display/Edit features

### 5.2 Persistence

**Application-Wide Preferences (NOT session-specific):**
- Page tab height (Small/Medium/Large)
- Status bar height (Small/Medium/Large)
- Bevel edge width (None/5%/10%/15%/20%)
- Button trigger size (Small/Medium/Large)
- Display mode toggles (HotKey/MIDI text, button triggers, edged text)
- Elapsed time mode (Countdown vs Elapsed)
- Level meter display mode (Horizontal vs Vertical)

**Session-Specific Settings:**
- Page names (8 custom names per session)
- Undo/Redo stack (cleared on session load/new)
- Page clipboard (cleared on session load/new)

**Transient State (NOT persisted):**
- Button clipboard (single button copy/paste)
- Paste Special options (dialog state)
- Level meters window position/size

### 5.3 Cross-Platform Compatibility

**Platform-Specific Considerations:**
- Keyboard shortcuts (Ctrl vs Cmd on macOS)
- PropertiesFile location (platform-specific app data folder)
- File chooser dialogs (native vs JUCE)

### 5.4 Performance

**Critical Metrics:**
- Undo/Redo latency: <50ms (command execution + UI update)
- Paste Special: <100ms for 320 buttons (bulk operation)
- Level meters update rate: 10 FPS (100ms timer, non-blocking)
- Play history logging: <1ms (lock-free enqueue)

---

## 6. Testing Strategy

### 6.1 Unit Tests

**Test Coverage:**
- `DisplayPreferences` save/load
- `UndoManager` push/pop/clear
- `PasteSpecialOptions` parameter application
- `PlayHistoryLogger` circular buffer (100 entries)

### 6.2 Integration Tests

**Test Scenarios:**
- Edit clip → Undo → Redo (verify state restoration)
- Paste Special with AutoFill MIDI (verify sequential assignment)
- Copy Page → Paste Page (verify 48 buttons copied)
- Swap Page (verify page exchange)
- Level meters show correct playing buttons per output
- Play history logs 100 entries, oldest dropped

### 6.3 Manual Testing

**Test Cases:**
- Change page tab height → Restart app (verify persistence)
- Undo 50 edits → Redo 50 edits (verify stack depth limit)
- Paste Special to range of buttons on specific output (verify filtering)
- Right-click output letter in level meters → View play history
- Toggle elapsed time mode (Ctrl+T) → Verify countdown/count-up

---

## 7. Documentation Requirements

### 7.1 User Documentation

**Topics:**
- Display preferences (page tab height, status bar height, bevel width, etc.)
- Page name customization (Edit/Reset, caret character for line breaks)
- Undo/Redo keyboard shortcuts (Ctrl+Z, Ctrl+Y)
- Paste Special workflow (Copy → Paste Special → Select parameters)
- AutoFill MIDI notes (sequential assignment)
- Page operations (Cut/Copy/Paste/Swap/Clear)
- Fill/Clear Buttons (range selection)
- Level meters (Ctrl+L, horizontal/vertical mode, play history)

### 7.2 Developer Documentation

**Topics:**
- Command pattern implementation (base class, concrete commands)
- Undo/Redo stack management (max depth, trimming)
- Paste Special parameter selection (options struct)
- Play history logging (lock-free queue, circular buffer)
- Level meters data collection (audio thread → message thread)

---

## 8. Dependencies on Other OCC Systems

### 8.1 OCC115 Sprint 1 - Application Data Folder

**Dependency:** Display preferences stored in preferences folder

**Impact:** Must complete OCC115 Sprint 1 before Sprint 15.

### 8.2 OCC115 Sprint 5 - Event Logging

**Dependency:** Play history logging uses event log infrastructure

**Impact:** Recommended to complete OCC115 Sprint 5 before Sprint 19.

### 8.3 AudioEngine API Extensions

**New Methods Required:**
```cpp
// In AudioEngine.h
std::vector<int> getPlayingButtonsOnOutput(int outputIndex) const;
float getOutputLevelDb(int outputIndex) const;
void stopAllOnOutput(int outputIndex);
```

---

## 9. Success Metrics

### 9.1 User Workflow Efficiency

- Undo/Redo reduces editing errors by >80% (no destructive edits)
- Paste Special reduces bulk edit time by >70% (vs. manual per-button edits)
- Page operations reduce session setup time by >50% (copy/paste entire pages)

### 9.2 Editing Reliability

- Undo/Redo stack handles 50+ commands without performance degradation
- Paste Special AutoFill assigns 320 MIDI notes in <100ms
- Level meters update at 10 FPS without audio thread interference

### 9.3 Monitoring and Diagnostics

- Level meters show real-time playing buttons per output (4 outputs)
- Play history logs 100 tracks per output with timestamps
- Play history dialog accessible via right-click (1-click access)

---

## 10. Related Documentation

- **OCC114** - Backend Infrastructure Audit (Current State)
- **OCC115** - File Menu Backend Features (Session Management)
- **OCC116** - Setup Menu Backend Features (External Control)
- **SpotOn Manual Section 03** - Display Menu (Peer Analysis)

---

**Last Updated:** 2025-11-12
**Maintainer:** OCC Development Team
**Status:** Planning
**Sprint Context:** Backend Architecture Sprint
