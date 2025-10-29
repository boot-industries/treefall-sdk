# Orpheus Clip Composer v0.2.0 Sprint

## UX Fixes & Keyboard Navigation Improvements

**Target:** v0.2.0-alpha
**Priority:** High (blocks beta testing)
**Estimated Effort:** 3-4 days
**Assignee:** Claude Code

---

## Sprint Overview

This sprint addresses 12 critical UX issues discovered during alpha testing of v0.1.0. Focus areas:

- Audio routing defaults
- Edit Dialog playback boundaries
- Keyboard navigation polish
- Transport control fixes

---

## Issues & Acceptance Criteria

### 1. Audio Output Routing (P0 - Blocker)

**Problem:** No audio output when system uses "External Headphones" device.

**Root Cause:** Application not defaulting to system audio output.

**Solution:**

```cpp
// In AudioEngine initialization:
juce::AudioDeviceManager::setCurrentAudioDeviceType("CoreAudio", true);
// OR on Windows:
juce::AudioDeviceManager::setCurrentAudioDeviceType("Windows Audio", true);

// Ensure we use system default output:
auto* device = deviceManager.getCurrentAudioDevice();
if (device == nullptr) {
    // Fallback: open default device
    deviceManager.initialiseWithDefaultDevices(0, 2); // 0 inputs, 2 outputs
}
```

**Acceptance Criteria:**

- [ ] Audio plays through macOS "External Headphones" device
- [ ] Audio follows system output device changes (unplug/replug headphones)
- [ ] Tested on macOS with built-in speakers, USB audio interface, and Bluetooth headphones
- [ ] Tested on Windows with ASIO and WASAPI devices

**Files to Modify:**

- `Source/AudioEngine/AudioEngine.cpp` (device initialization)
- Add debug logging: "Using audio device: [device name]"

---

### 2. Playback Boundary Enforcement (P0 - Blocker)

**Problem:** Playhead escapes IN/OUT bounds in multiple scenarios:

- Cmd/Ctrl+Leftclick on waveform plays from 0, then sets IN > 0
- Playhead exceeds OUT point regardless of loop state

**Root Cause:** Playback position not clamped to [IN, OUT] range.

**Solution:**

```cpp
// In clip playback logic:
void ClipPlayback::updatePosition(int64_t newPosition) {
    // ALWAYS clamp to [trimIn, trimOut]
    position = std::clamp(newPosition, trimIn, trimOut);

    if (position >= trimOut) {
        if (loopEnabled) {
            position = trimIn; // Loop back
        } else {
            stop(); // Stop at OUT
        }
    }
}

// For Cmd+Click jog:
void WaveformDisplay::jogToPosition(int64_t clickPosition) {
    // Clamp BEFORE setting position
    int64_t clampedPosition = std::clamp(clickPosition, trimIn, trimOut);
    transport->seekClip(clipHandle, clampedPosition);
}
```

**Acceptance Criteria:**

- [ ] Dragging IN point → playhead chases correctly
- [ ] IN < > buttons → playhead chases correctly
- [ ] Cmd+Leftclick on waveform → playhead stays within [IN, OUT]
- [ ] Playback stops at OUT (non-loop) or loops to IN (loop enabled)
- [ ] No escaping bounds in any scenario (exhaustive testing)

**Files to Modify:**

- `Source/AudioEngine/TransportAdapter.cpp` (position clamping)
- `Source/UI/WaveformDisplay.cpp` (jog handling)

---

### 3. Click-to-Jog Reliability (P1 - High)

**Problem:** Leftclick on waveform doesn't consistently jog playhead.

**Solution:**

```cpp
void WaveformDisplay::mouseDown(const juce::MouseEvent& event) {
    if (event.mods.isLeftButtonDown() && !event.mods.isCommandDown()) {
        // Pure leftclick = jog
        float normalizedX = event.position.x / getWidth();
        int64_t targetSample = trimIn + (normalizedX * (trimOut - trimIn));

        // Clamp and seek
        targetSample = std::clamp(targetSample, trimIn, trimOut);
        transport->seekClip(clipHandle, targetSample);
    }
}
```

**Acceptance Criteria:**

- [ ] Leftclick anywhere on waveform → playhead jumps to that position
- [ ] Playhead remains within [IN, OUT] bounds
- [ ] Works whether playback is active or paused
- [ ] Visual feedback: playhead line updates immediately

**Files to Modify:**

- `Source/UI/WaveformDisplay.cpp`

---

### 4. Edit Dialog Keyboard Shortcuts (P1 - High)

**Problem:** Missing standard keyboard navigation.

**Solution:**
Implement these shortcuts in `EditDialog.cpp`:

| Key         | Action                                 | Implementation                                                |
| ----------- | -------------------------------------- | ------------------------------------------------------------- |
| SPACE       | Toggle Play/Pause                      | `transport->togglePlayback(clipHandle)`                       |
| ENTER       | OK (save & close)                      | `okButton.triggerClick()`                                     |
| ESC         | Cancel (discard & close)               | `cancelButton.triggerClick()`                                 |
| ? (Shift+/) | Toggle Loop                            | `loopCheckbox.setToggleState(!loopCheckbox.getToggleState())` |
| TAB         | Cycle focus: Name → Trim IN → Trim OUT | Standard JUCE focus traversal                                 |

**TAB Navigation within Time Fields:**

- Trim IN: HH → MM → SS → FF (frames)
- Trim OUT: HH → MM → SS → FF

**Acceptance Criteria:**

- [ ] All 5 shortcuts work as expected
- [ ] TAB cycles through all 3 main fields in order
- [ ] TAB within time fields moves through HH/MM/SS/FF
- [ ] Keyboard-only workflow functional (no mouse required)

**Files to Modify:**

- `Source/UI/EditDialog.cpp` (`keyPressed()` override)

---

### 5. Waveform Zoom Shortcuts (P2 - Medium)

**Problem:** No keyboard shortcuts for zoom.

**Solution:**

```cpp
// Standard browser-style zoom:
bool WaveformDisplay::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress('=', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)) {
        // Cmd+Shift+= (which is Cmd+Plus on US keyboard)
        zoomIn();
        return true;
    }
    if (key == juce::KeyPress('-', juce::ModifierKeys::commandModifier, 0)) {
        // Cmd+Minus
        zoomOut();
        return true;
    }
    return false;
}
```

**Note:** On US keyboards:

- Zoom In = Cmd+Shift+= (user types Cmd+Plus)
- Zoom Out = Cmd+- (user types Cmd+Minus)

**Acceptance Criteria:**

- [ ] Cmd+Plus zooms in (2x increments)
- [ ] Cmd+Minus zooms out (0.5x increments)
- [ ] Zoom centers on playhead position
- [ ] Min zoom: full clip visible, Max zoom: ~1 second visible

**Files to Modify:**

- `Source/UI/WaveformDisplay.cpp`

---

### 6. IN/OUT Modifier Keys (P1 - High)

**Problem:** Cmd+Rightclick often interpreted as Cmd+Leftclick, causing IN/OUT confusion.

**Solution:**

```cpp
void WaveformDisplay::mouseDown(const juce::MouseEvent& event) {
    float normalizedX = event.position.x / getWidth();
    int64_t targetSample = trimIn + (normalizedX * (trimOut - trimIn));

    if (event.mods.isCommandDown() && event.mods.isShiftDown()) {
        // Cmd+Shift+Leftclick = Set OUT
        setTrimOut(targetSample);
    } else if (event.mods.isCommandDown()) {
        // Cmd+Leftclick = Set IN
        setTrimIn(targetSample);
    } else {
        // Pure leftclick = Jog
        jogToPosition(targetSample);
    }
}
```

**Acceptance Criteria:**

- [ ] Cmd+Leftclick sets IN point (blue marker)
- [ ] Cmd+Shift+Leftclick sets OUT point (red marker)
- [ ] No ambiguity between IN/OUT setting
- [ ] Visual feedback: marker updates immediately

**Files to Modify:**

- `Source/UI/WaveformDisplay.cpp`

---

### 7. Latchable IN/OUT Navigation (P2 - Medium)

**Problem:** Clicking IN/OUT < > buttons is tedious for large jumps.

**Solution:**

```cpp
class NudgeButton : public juce::Button {
public:
    void mouseDown(const juce::MouseEvent& e) override {
        Button::mouseDown(e);
        startLatchTimer();
    }

    void mouseUp(const juce::MouseEvent& e) override {
        Button::mouseUp(e);
        stopLatchTimer();
    }

private:
    void startLatchTimer() {
        startTimer(500); // Initial delay
        latchInterval = 500;
    }

    void timerCallback() override {
        triggerClick(); // Repeat click

        // Accelerate: 500ms → 250ms → 100ms
        if (latchInterval > 100) {
            latchInterval = std::max(100, latchInterval - 50);
            startTimer(latchInterval);
        }
    }

    int latchInterval = 500;
};
```

**Modifier Behavior:**

- Normal click: move 1 tick (1/75th second @ 48kHz = 640 samples)
- Shift+Click: move 15 ticks (15 Ã— 640 = 9,600 samples)
- Hold: repeat with acceleration

**Keyboard Shortcuts:**

- `[` / `]` : Nudge IN left/right (1 tick)
- `Shift+[` / `Shift+]` : Nudge IN left/right (15 ticks)
- `;` / `'` : Nudge OUT left/right (1 tick)
- `Shift+;` / `Shift+'` : Nudge OUT left/right (15 ticks)

**Acceptance Criteria:**

- [ ] Holding < > buttons repeats action
- [ ] Shift modifier = 15-tick jumps
- [ ] Keyboard shortcuts work identically
- [ ] Acceleration feels responsive (not too fast)

**Files to Modify:**

- `Source/UI/EditDialog.cpp` (custom button class)

---

### 8. Single Edit Dialog Enforcement (P0 - Blocker)

**Problem:** Multiple Edit Dialogs stack, causing confusion and state corruption.

**Solution:**

```cpp
// In MainComponent:
class MainComponent {
private:
    std::unique_ptr<EditDialog> currentEditDialog;

public:
    void openEditDialogForClip(ClipHandle handle) {
        // Close existing dialog if open
        if (currentEditDialog != nullptr) {
            currentEditDialog->closeDialog(false); // Don't save
        }

        // Create new dialog
        currentEditDialog = std::make_unique<EditDialog>(handle, transport, session);
        currentEditDialog->onClose = [this]() {
            currentEditDialog.reset(); // Clear reference
        };

        addAndMakeVisible(currentEditDialog.get());
    }
};
```

**Acceptance Criteria:**

- [ ] Only one Edit Dialog visible at a time
- [ ] Opening new Edit Dialog closes previous one (discards unsaved changes)
- [ ] Closing Edit Dialog (OK/Cancel) clears internal reference
- [ ] No dangling pointers or memory leaks

**Files to Modify:**

- `Source/MainComponent.cpp`
- `Source/UI/EditDialog.cpp`

---

### 9. Transport Control Fixes (P1 - High)

**Problem:** |<< and >>| buttons break playback.

**Desired Behavior:**

- `|<<` : Jump to IN point (resume if playing, don't start if paused)
- `>>|` : Jump to 5 seconds before OUT point (resume if playing, don't start if paused)

**Solution:**

```cpp
void TransportControls::onRewindButton() {
    bool wasPlaying = transport->isClipPlaying(currentClip);
    transport->seekClip(currentClip, trimIn);

    // Don't change playback state
    // (if was playing, continues playing; if paused, stays paused)
}

void TransportControls::onFastForwardButton() {
    int64_t fiveSecondsBeforeEnd = std::max(trimIn, trimOut - (5 * sampleRate));
    bool wasPlaying = transport->isClipPlaying(currentClip);
    transport->seekClip(currentClip, fiveSecondsBeforeEnd);

    // Don't change playback state
}
```

**Acceptance Criteria:**

- [ ] |<< jumps to IN point without changing play/pause state
- [ ] > > | jumps to 5s from OUT without changing play/pause state
- [ ] If clip < 5 seconds, >>| goes to IN point (graceful fallback)
- [ ] Playback remains seamless (no clicks/pops)

**Files to Modify:**

- `Source/UI/TransportControls.cpp`

---

### 10. Tab Shortcuts & Edit Dialog Override (P2 - Medium)

**Problem:** Cmd+[1-8] conflicts with Edit Dialog fade time shortcuts.

**Solution:**

**Global (Main Grid):**

- `Cmd+Shift+[1-8]` : Switch to Tab 1-8

**Edit Dialog (Local Override):**

- `Cmd+Shift+[1-9]` : Set Fade OUT time (0.1s - 0.9s for 1-9, 1.0s for 0)
- `Cmd+Opt+[1-9]` : Set Fade IN time (0.1s - 0.9s for 1-9, 1.0s for 0)

**Implementation:**

```cpp
// In EditDialog::keyPressed():
bool EditDialog::keyPressed(const juce::KeyPress& key) {
    if (key.getModifiers().isCommandDown() && key.getModifiers().isShiftDown()) {
        // Cmd+Shift+[1-0] = Fade OUT
        int digit = key.getKeyCode() - '0';
        if (digit >= 0 && digit <= 9) {
            float fadeTime = (digit == 0) ? 1.0f : (digit * 0.1f);
            fadeOutSlider.setValue(fadeTime);
            return true;
        }
    }

    if (key.getModifiers().isCommandDown() && key.getModifiers().isAltDown()) {
        // Cmd+Opt+[1-0] = Fade IN
        int digit = key.getKeyCode() - '0';
        if (digit >= 0 && digit <= 9) {
            float fadeTime = (digit == 0) ? 1.0f : (digit * 0.1f);
            fadeInSlider.setValue(fadeTime);
            return true;
        }
    }

    return false;
}
```

**Acceptance Criteria:**

- [ ] Main grid: Cmd+Shift+[1-8] switches tabs
- [ ] Edit Dialog: Cmd+Shift+[1-0] sets fade OUT times (0.1-1.0s)
- [ ] Edit Dialog: Cmd+Opt+[1-0] sets fade IN times (0.1-1.0s)
- [ ] Edit Dialog overrides global tab shortcuts (no tab switching while editing)

**Files to Modify:**

- `Source/MainComponent.cpp` (global shortcuts)
- `Source/UI/EditDialog.cpp` (local overrides)

---

### 11. Enlarge Time Counter (P2 - Medium)

**Problem:** Time counter in Edit Dialog too small to read.

**Solution:**

```cpp
// In EditDialog constructor:
timeLabel.setFont(juce::Font(32.0f, juce::Font::bold)); // Was 18.0f
timeLabel.setJustificationType(juce::Justification::centred);
timeLabel.setColour(juce::Label::textColourId, juce::Colours::white);

// Add more vertical space
timeLabel.setBounds(10, waveformBottom + 20, getWidth() - 20, 50); // Was 30px tall
```

**Acceptance Criteria:**

- [ ] Time counter easily readable from 2 feet away
- [ ] Font size ~32pt (compare to DAW transport displays)
- [ ] Sufficient vertical spacing above/below
- [ ] Updates at 30 FPS during playback (smooth)

**Files to Modify:**

- `Source/UI/EditDialog.cpp`

---

### 12. Right-Click Menu Padding (P3 - Low)

**Problem:** Right-click menu items vertically crowded.

**Solution:**

```cpp
// In ClipButton right-click menu:
juce::PopupMenu menu;
menu.setLookAndFeel(&customLookAndFeel); // If using custom LookAndFeel

// Add padding via custom LookAndFeel:
class CustomMenuLookAndFeel : public juce::LookAndFeel_V4 {
public:
    int getPopupMenuItemHeight(int standardHeight) override {
        return standardHeight + 8; // Add 8px vertical padding
    }

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area, ...) override {
        // Add 4px top/bottom padding
        auto paddedArea = area.reduced(0, 4);
        LookAndFeel_V4::drawPopupMenuItem(g, paddedArea, ...);
    }
};
```

**Acceptance Criteria:**

- [ ] Menu items have 8px total vertical padding (4px top + 4px bottom)
- [ ] Menu feels less cramped
- [ ] Consistent with macOS native menu spacing
- [ ] No visual artifacts (text clipping, misalignment)

**Files to Modify:**

- `Source/UI/ClipButton.cpp`
- Create `Source/UI/CustomLookAndFeel.cpp` (if needed)

---

## Testing Checklist

### Manual Testing (All Issues)

- [ ] Test all keyboard shortcuts (with/without modifiers)
- [ ] Test Edit Dialog workflow end-to-end (open, edit, save, cancel)
- [ ] Test boundary conditions (IN=OUT, clip < 5s, empty clip)
- [ ] Test with different audio devices (built-in, USB, Bluetooth)
- [ ] Test on macOS and Windows (if cross-platform)

### Edge Cases

- [ ] Rapid clicks on latchable buttons (no crashes)
- [ ] Zoom to min/max limits (graceful clamping)
- [ ] Multiple Edit Dialog open attempts (only one visible)
- [ ] Audio device disconnect during playback (graceful fallback)

### Performance

- [ ] No audio dropouts during transport seek
- [ ] UI remains responsive during waveform zoom
- [ ] Time counter updates at 30 FPS (smooth)

---

## Implementation Order

**Day 1-2 (Critical Path):**

1. Issue #1: Audio routing (P0)
2. Issue #2: Playback boundaries (P0)
3. Issue #8: Single Edit Dialog (P0)

**Day 2-3 (High Priority):** 4. Issue #3: Click-to-jog (P1) 5. Issue #4: Keyboard shortcuts (P1) 6. Issue #6: IN/OUT modifiers (P1) 7. Issue #9: Transport controls (P1)

**Day 3-4 (Medium Priority):** 8. Issue #5: Waveform zoom (P2) 9. Issue #7: Latchable buttons (P2) 10. Issue #10: Tab shortcuts (P2) 11. Issue #11: Time counter (P2)

**Day 4 (Polish):** 12. Issue #12: Menu padding (P3) 13. Final testing pass

---

## Success Criteria

- [ ] All 12 issues resolved and tested
- [ ] No regressions in existing functionality
- [ ] Keyboard-only workflow functional (no mouse required for Edit Dialog)
- [ ] Audio routing follows system output device
- [ ] Playback never escapes IN/OUT bounds
- [ ] Edit Dialog feels professional (spacing, responsiveness)

---

## Files to Modify (Summary)

```
Source/
├── AudioEngine/
│   ├── AudioEngine.cpp (issue #1)
│   └── TransportAdapter.cpp (issue #2)
├── UI/
│   ├── WaveformDisplay.cpp (issues #2, #3, #5, #6)
│   ├── EditDialog.cpp (issues #4, #7, #10, #11)
│   ├── TransportControls.cpp (issue #9)
│   ├── ClipButton.cpp (issue #12)
│   └── CustomLookAndFeel.cpp (issue #12, new file)
└── MainComponent.cpp (issues #8, #10)
```

---

## Notes for Claude Code

1. **Prioritize P0 issues first** (audio routing, boundaries, single dialog)
2. **Test incrementally** after each issue (don't batch changes)
3. **Check JUCE documentation** for keyboard event handling (KeyPress API)
4. **Verify thread safety** for any audio engine changes (no locks in audio thread)
5. **Use launch.sh** to run OCC (don't use `open` command)
6. **Log extensively** during debugging (stdout to /tmp/occ.log)

---

**Sprint Start:** [Date]
**Sprint End:** [Date + 4 days]
**Review:** After all issues resolved + testing pass
