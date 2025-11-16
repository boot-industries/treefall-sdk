# OCC130 Sprint Plan - UX Refinements v0.2.2

**Created:** 2025-01-14
**Status:** Planning
**Target Version:** v0.2.2
**Source:** OCC129 Lost CL Notes v021 (cleaned and reorganized)

---

## Overview

This sprint plan consolidates UX refinement tasks from OCC129, organized by priority and implementation complexity. Items already completed in v0.2.0-v0.2.1 have been removed. Remaining tasks focus on polish, usability, and professional-grade UI refinement.

---

## Status Summary

**From OCC129 Analysis:**

- ✅ **Already Complete:** Loop state, clip gain, transport fades, r/t zoom, fade icons
- 📋 **Remaining Tasks:** 42 tasks organized into 9 categories below
- 🎯 **Priority Focus:** Typography, clip button layout, preferences system

---

## Category 0: Critical Bug Fixes (BLOCKING)

### 0.1 Playhead Visual Past OUT Point (Non-Looped Clips)

**Status:** 🐛 **CRITICAL BUG** - Playhead displays incorrectly on non-looped clips
**Severity:** High (visual only, audio is correct)

**Issue:**

- Non-looped clips: Playhead VISUALLY continues past OUT point by fade out duration
- Audio correctly fades out within OUT point bounds (legal)
- Visual playhead illegally extends beyond OUT point
- Looped clips: No issue (playhead behaves correctly)

**Expected Behavior:**

- Fade OUT time should always be within OUT point bounds
- Playhead should NEVER visually exceed OUT point
- Visual should match audio behavior (already correct)

**Root Cause (Hypothesis):**

- `WaveformDisplay` or `PreviewPlayer` may be adding fade out duration to playhead position
- Non-looped clips may calculate `endPosition = trimOutSamples + fadeOutSamples` instead of `trimOutSamples`
- Looped clips may correctly respect `trimOutSamples` as hard boundary

**Files to Investigate:**

- `Source/UI/WaveformDisplay.cpp` (playhead rendering)
- `Source/UI/PreviewPlayer.cpp` (playback state reporting)
- `Source/Audio/AudioEngine.cpp` (clip position tracking)
- SDK: `src/core/transport/transport_controller.cpp` (fade processing)

**Fix Strategy:**

1. Trace playhead position calculation in `WaveformDisplay::updatePlayhead()`
2. Check if fade out duration is incorrectly added to position
3. Ensure `clipPosition` never exceeds `trimOutSamples` for visual rendering
4. Verify looped clips use same logic (they work correctly)

**Testing:**

- [ ] Load non-looped clip with fade OUT = 1.0s
- [ ] Set OUT point at 10 seconds
- [ ] Play clip and watch playhead
- [ ] Verify playhead stops at OUT point (10s), not 11s
- [ ] Verify audio fades out within OUT point bounds (already working)

**Priority:** Fix before v0.2.2 release (blocking)

---

## Category 1: Typography & Font Migration (HIGH PRIORITY)

### 1.1 Replace Inter with HK Grotesk (CRITICAL)

**Status:** ⚠️ Currently using Inter throughout
**Goal:** Application-wide font change to HK Grotesk

**Location:** `orpheus-sdk/apps/clip-composer/Resources/HKGrotesk_3003/`

**Files to Update:**

- `Source/UI/InterLookAndFeel.h` (rename class, change font references)
- `Source/MainComponent.h/cpp` (update LookAndFeel instance)
- `Source/ClipGrid/ClipButton.cpp` (all font calls - 7 locations)
- `Source/UI/ClipEditDialog.cpp` (all font calls - 15 locations)
- `Source/UI/WaveformDisplay.cpp` (all font calls - 3 locations)
- `Source/UI/TabSwitcher.cpp` (all font calls - 1 location)

**Implementation Notes:**

- HK Grotesk is a variable font (100-900 weights)
- Load font files in CMakeLists.txt or JUCE Projucer
- Test all text rendering (clip buttons, dialogs, menus)
- Ensure JUCE fonts inherit HK Grotesk (no Reaper aesthetic leakage)

**Testing:**

- [ ] All UI text uses HK Grotesk
- [ ] No Inter font visible anywhere
- [ ] Variable weights render correctly
- [ ] Menus and dialogs inherit HK Grotesk

---

## Category 2: Clip Button Refinements (HIGH PRIORITY)

### 2.1 Two-Row Text Layout on Clip Buttons

**Current:** Single clip name with duration below
**Target:** Two discrete text rows

**Row Structure:**

```
Row 1 (top): Clip Name (flex height, word wrap, Regular weight)
Row 2 (bottom): User Notes (single line, fixed height, condensed/all caps)
```

**Implementation:**

- Add `m_userNotes` field to `ClipButton` class
- Add "Notes" field to `ClipEditDialog` (below "Clip Name")
- Modify `ClipButton::drawClipHUD()` to render two text rows
- Keep Row 2 tight, condensed typography (possibly all caps)

**Files:**

- `Source/ClipGrid/ClipButton.h` (add `m_userNotes` member)
- `Source/ClipGrid/ClipButton.cpp` (update `drawClipHUD()`)
- `Source/UI/ClipEditDialog.h/cpp` (add Notes field)
- `Source/Session/SessionManager.h/cpp` (save/load notes)

---

### 2.2 Time Display on Clip Buttons (CRITICAL)

**Current:** Duration shows as `MM:SS` when stopped, `▶ elapsed / -remaining` when playing
**Target:** Always show both elapsed and remaining times

**Format:** `00:00:00.00 — 00:03:30.00`

- Left: Time elapsed (00:00:00.00 when stopped)
- Right: Time remaining (full duration when stopped)
- Separator: Em dash with spaces (`—`)
- Size: ~2x current size for better readability

**Row Structure:**

```
Row 1: Clip Number, Beat, Hotkey
Row 2: Clip Name (word wrap, flex height within fixed row)
Row 3: User Notes (single line, condensed)
Row 4: Time Elapsed — Time Remaining (larger, prominent)
Row 5: PLAY Icon, Status Icons, Group Indicator
```

**Implementation:**

- Add third text row to `ClipButton::drawClipHUD()`
- Format times as `HH:MM:SS.FF` (hundredths, not frames)
- Update elapsed/remaining during 75fps polling
- Keep text size ~18-20px (double current 9px duration text)

**Files:**

- `Source/ClipGrid/ClipButton.cpp` (update `drawClipHUD()` and `formatDuration()`)

---

### 2.3 Corner Indicator Sizing & Placement

**Current:** Corner indicators vary in size and placement
**Target:** Consistent 3-character width for all corner indicators

**Indicators:**

1. **Top Left:** Clip Number (white rounded box) - ✅ Already consistent
2. **Top Right:** Hotkey (thin outline, transparent background, clip button's text color)
3. **Bottom Left:** PLAY Icon (green box) - ✅ Already sized well
4. **Bottom Right:** Clip Group (abbreviated to 3 chars max)

**Hotkey Indicator (NEW):**

- No background color (transparent)
- Thin outline (1-2px, matches clip button text color)
- Same dimensions as other corner indicators
- Light outline if button is dark, dark outline if button is light

**Group Abbreviation Logic:**

- `Group 1` → `G1`
- `Group 2` → `G2`
- `Music` → `MUS`
- `Soccer` → `SOC`
- `Effects` → `FX`
- `SFX` → `SFX`

**Implementation:**

- Add abbreviation logic to `ClipButton` or `SessionManager`
- Update `drawClipHUD()` to render abbreviated group names
- Add hotkey indicator rendering (top-right corner)
- Match all corner indicator dimensions (including inner/outer corner radii)

**Files:**

- `Source/ClipGrid/ClipButton.cpp` (corner indicator rendering)
- `Source/Session/SessionManager.h/cpp` (group abbreviation helper)

---

### 2.4 Beat Indicator on Clip Buttons

**Current:** Beat offset stored but not displayed
**Target:** Show beat value next to clip number (top-left area)

**Format:** ` // {value}` (e.g., ` // 3+`, ` // 1++`)

- Color: White if button is non-white, black @0.95 if near-white
- Typeface: Thin, minimal (HK Grotesk Light?)
- Position: To the right of clip number corner indicator

**Implementation:**

- Update `drawClipHUD()` to render beat offset after clip number
- Use same color logic as clip number (white/black based on background)
- Keep typeface thin and non-dominant

**Files:**

- `Source/ClipGrid/ClipButton.cpp` (update `drawClipHUD()`)

---

### 2.5 Top Left Clip Number - Near-White Logic

**Current:** Always white text in clip number indicator
**Target:** Flip to dark text on near-white buttons

**Logic:**

```cpp
if (m_clipColor.getBrightness() > 0.95f) {
  numberTextColor = juce::Colours::black.withAlpha(0.95f);
} else {
  numberTextColor = juce::Colours::white;
}
```

**Use extreme threshold** (>0.95) to keep white text as default.

**Files:**

- `Source/ClipGrid/ClipButton.cpp` (update number box rendering in `drawClipHUD()`)

---

### 2.6 Clip Button Corner Radius Consistency

**Current:** Clip buttons have 4px corner radius, Active border has unknown radius
**Target:** Match corner radii (inner and outer) across:

- Clip button background
- Pulsing "Active" border
- Corner indicators

**Implementation:**

- Verify `CORNER_RADIUS = 4` is used consistently
- Check Active border rendering (white pulsing glow)
- Update corner indicators to match 4px radius

**Files:**

- `Source/ClipGrid/ClipButton.cpp` (paint method)

---

## Category 3: Clip Edit Dialog Enhancements (MEDIUM PRIORITY)

### 3.1 TAB Key Behavior - Add "No Focus" State

**Current:** TAB cycles: Clip Name → Fade IN → Fade OUT → (wraps to Clip Name)
**Target:** Add fourth state: "No Keyboard Focus"

**Cycle:**

1. Clip Name
2. Fade IN Time
3. Fade OUT Time
4. **No Focus** (deselect all fields, keyboard inactive)

**ESC Key Behavior:**

- **First ESC:** Exit text field (if focused)
- **Second ESC:** Exit Clip Edit dialog

**ENTER Key Behavior:**

- If text field focused → Confirm field (blur)
- If no text field focused → Confirm dialog (OK)

**Status:** ✅ ESC already works this way, just need to add "No Focus" state to TAB cycle

**Files:**

- `Source/UI/ClipEditDialog.cpp` (update `keyPressed()` TAB handling)

---

### 3.2 Fade Time Fields - Numeric Input Only

**Current:** Fade IN/OUT fields accept all keyboard characters
**Target:** Accept only numeric input, auto-format as `HH:MM:SS.FF`

**Behavior:**

- User types: `12345` → Formats to `00:01:23.45`
- Prevents alphabetic characters
- Allows TAB navigation without interference
- Prevents < > key command conflicts during text entry

**Implementation:**

- Add input filter to `m_fadeInCombo` and `m_fadeOutCombo`
- Or replace ComboBox with TextEditor + formatter
- Auto-format on keystroke or on blur

**Files:**

- `Source/UI/ClipEditDialog.cpp` (fade field setup)

---

### 3.3 Clip Name Field - Auto-Highlight on TAB

**Current:** TAB selects Clip Name field but doesn't highlight text
**Target:** All text automatically highlighted when TAB reaches Clip Name

**Implementation:**

- Call `m_nameEditor->selectAll()` when TAB focuses Clip Name
- Makes editing faster (type immediately to replace)

**Files:**

- `Source/UI/ClipEditDialog.cpp` (`keyPressed()` TAB handling)

---

### 3.4 '?' Key to Toggle LOOP

**Current:** '?' key does not toggle loop in Clip Edit dialogue
**Target:** Shift+'/' (?) toggles loop button

**Status:** Verify if this is already implemented (loop button is a ToggleButton)

**Files:**

- `Source/UI/ClipEditDialog.cpp` (`keyPressed()` method)

---

### 3.5 Transport Control Icons - Redesign

**Current:** Transport icons are "ugly" (user feedback)
**Target:** Professional icon set for preview transport

**Icons to Redesign:**

- Skip to Start
- Play
- Stop
- Skip to End
- Loop

**Suggestions:**

- Use custom SVG paths (juce::DrawableButton with custom drawables)
- Match Clip Composer aesthetic (clean, minimal, HK Grotesk vibe)
- Consider icon libraries (Feather Icons, Heroicons, custom design)

**Files:**

- `Source/UI/ClipEditDialog.cpp` (transport button creation in `buildPhase2UI()`)

---

### 3.6 Gain Dropdown (Replace Slider)

**Current:** Gain is a slider (vertical)
**Target:** Gain dropdown (matches Fade time dropdowns)

**Format:** `Gain: { 0.0 dB }`

- Range: -30.0 dB to +10.0 dB
- Step: 0.5 dB increments
- Default: 0.0 dB

**Layout:**

- Move above Fade IN section
- Align with current "Gain:" label position
- Match style of Fade time dropdowns

**Files:**

- `Source/UI/ClipEditDialog.h` (replace `m_gainSlider` with `m_gainCombo`)
- `Source/UI/ClipEditDialog.cpp` (rebuild gain control in `buildPhase3UI()`)

---

### 3.7 Beat Field (Manual Beat Tag)

**Current:** Beat offset exists but no UI for manual entry
**Target:** Beat dropdown to right of Gain dropdown (above Trim OUT)

**Label:** `Beat:`
**Options:**

- Default: (blank/unassigned)
- Standard: `1`, `1+`, `2`, `2+`, `3`, `3+`, `4`, `4+`
- **Easter Egg:** Hold Option key → Changes to `1`, `1++`, `2`, `2++`, `3`, `3++`, `4`, `4++`

**Layout:**

```
[Gain: {0.0 dB}]    [Beat: {3+}]
[Trim IN controls]  [Trim OUT controls]
```

**Files:**

- `Source/UI/ClipEditDialog.h` (add `m_beatCombo`)
- `Source/UI/ClipEditDialog.cpp` (add beat dropdown, Option key handling)

---

### 3.8 OK/CANCEL Button Sizing & Spacing

**Current:** OK/CANCEL buttons are standard height
**Target:** 50% taller, with bottom margin above them

**Changes:**

- Increase button height: ~40px → ~60px
- Add bottom margin to COLOR/GROUP row (create space)
- Keep horizontal layout (OK left, CANCEL right)

**Files:**

- `Source/UI/ClipEditDialog.cpp` (button sizing in `resized()`)

---

### 3.9 Trim IN/OUT Layout Mirroring

**Current:** CLR buttons not aligned with SET buttons
**Target:** Horizontally mirror Trim IN and Trim OUT layouts

**Layout:**

```
Trim IN                        Trim OUT
[Time Editor]                  [Time Editor]
[< >]                          [< >]
[SET] [CLR]                    [SET] [CLR]
```

**Both sections should mirror each other** (CLR under SET on both sides).

**Status:** ⚠️ Need to verify current layout, may already be correct

**Files:**

- `Source/UI/ClipEditDialog.cpp` (`resized()` method)

---

## Category 4: Tab Bar & Status Bar Merge (HIGH PRIORITY)

### 4.1 Single-Row Tab + Panic Bar

**Current:** Tab bar separate from transport controls
**Target:** Merge into single row with two sections

**Layout:**

```
| [Tab 1] [Tab 2] ... [Tab 8]  |  [Stop All] [Panic] |
|      (flex space)             |   (min space)       |
```

**Sections:**

1. **Tabs (1-8):** Takes remaining horizontal space
2. **Panic Buttons:** Only the horizontal space they need

**Implementation:**

- Merge `TabSwitcher` and `TransportControls` into single component
- Use horizontal layout with flex space allocation
- Keep tab styling consistent with current design

**Files:**

- `Source/UI/TabSwitcher.h/cpp` (merge with transport)
- `Source/Transport/TransportControls.h/cpp` (refactor into TabSwitcher)
- `Source/MainComponent.cpp` (update layout in `resized()`)

---

### 4.2 Status Indicator Lights (Latency + Heartbeat)

**Current:** Verbose latency info displayed as text
**Target:** Replace with two small status indicator lights

**Lights:**

1. **Latency Status:** Color-coded (green/yellow/red based on latency)
2. **Heartbeat:** Pulses once per second (confidence monitor)

**Layout:**

```
| Tabs | [●] [●] | Panic |
         ↑    ↑
      Latency Heartbeat
```

**Placement:** Vertically stacked, between Tabs and Panic sections
**Sizing:** Match vertical spacing of Panic buttons (not higher or lower)

**Color Logic (Latency):**

- Green: < 10ms
- Yellow: 10-20ms
- Red: > 20ms

**Implementation:**

- Remove verbose latency text from `TransportControls`
- Add two circular indicator components
- Update latency status in 75fps polling loop
- Add timer for 1Hz heartbeat pulse

**Files:**

- `Source/Transport/TransportControls.cpp` (remove text, add lights)
- `Source/MainComponent.cpp` (update latency status in timer callback)

---

### 4.3 Move Verbose Latency to Audio I/O Settings

**Current:** Latency info shown in main window
**Target:** Move to Audio I/O Settings dialogue

**Rationale:** Nice to see, but shouldn't clutter main UI

**Implementation:**

- Add latency info panel to `AudioSettingsDialog`
- Show buffer size, sample rate, calculated latency
- Update in real-time if dialog is open

**Files:**

- `Source/UI/AudioSettingsDialog.h/cpp` (add latency info panel)

---

## Category 5: Tab Renaming & Context Menu (MEDIUM PRIORITY)

### 5.1 Double-Click to Rename Tab

**Current:** Tabs have labels but no rename functionality
**Target:** Double-click tab to rename

**Behavior:**

- Double-click tab → Inline text editor appears
- Type new name → Press ENTER or click away to confirm
- Press ESC to cancel

**Implementation:**

- Add `mouseDoubleClick()` handler to `TabSwitcher`
- Show inline `TextEditor` over clicked tab
- Save tab name to session on confirm

**Files:**

- `Source/UI/TabSwitcher.h/cpp` (add rename functionality)
- `Source/Session/SessionManager.h/cpp` (save/load tab names)

---

### 5.2 Right-Click Tab Menu

**Current:** No tab context menu
**Target:** Right-click menu with 'Rename Tab' and 'Clear Tab'

**Menu Items:**

1. **Rename Tab** (same as double-click)
2. **Clear Tab** (removes all clips from tab, with warning prompt)

**Implementation:**

- Add `mouseDown()` right-click handler to `TabSwitcher`
- Show popup menu
- Call rename or clear logic based on selection

**Files:**

- `Source/UI/TabSwitcher.h/cpp` (context menu)

---

### 5.3 Warning Prompts for Clear Actions

**Current:** No confirmation before clearing clips
**Target:** All 'clear' actions must show warning prompt

**Actions Requiring Prompts:**

- Clear All Clips (global menu)
- Remove Clip (single clip)
- Clear Tab (from tab context menu)

**Prompt Format:**

```
Are you sure you want to clear Tab 3?
This will remove all 48 clips from this tab.

[Cancel]  [Clear]
```

**Implementation:**

- Add `juce::AlertWindow::showOkCancelBox()` before clear actions
- User must click "Clear" to proceed
- "Cancel" aborts operation

**Files:**

- `Source/MainComponent.cpp` (clear actions)
- `Source/UI/TabSwitcher.cpp` (clear tab)

---

## Category 6: Clip Copy/Paste System (MEDIUM PRIORITY)

### 6.1 Copy/Paste Clips Around Grid

**Goal:** Copy entire clip (audio + metadata) to another button

**Keyboard Shortcuts:**

- `Cmd+C` with button selected → Copy clip
- `Cmd+V` with empty button selected → Paste clip

**Behavior:**

- Source clip remains intact
- Target button loads copied clip
- If target is occupied, show warning prompt

**Implementation:**

- Add clipboard storage for `ClipMetadata`
- Add copy/paste handlers to `ClipGrid` or `MainComponent`
- Support keyboard shortcuts globally

**Files:**

- `Source/MainComponent.h/cpp` (clipboard storage, handlers)
- `Source/ClipGrid/ClipGrid.h/cpp` (button selection state)

---

### 6.2 Paste Special (Clip States Only)

**Goal:** Copy only clip settings (color, loop, fades, etc.) without audio file

**Menu:** Right-click clip button → "Paste Special"

**Copies:**

- Color
- Stop Others state
- Loop state
- Fade IN/OUT times and curves
- Clip Gain
- Clip Group

**Does NOT Copy:**

- Audio file path
- Clip name
- User notes
- Trim IN/OUT points

**Implementation:**

- Add `m_clipboardStates` separate from `m_clipboardClip`
- Add "Paste Special" to clip button context menu
- Apply states to target clip without changing audio

**Files:**

- `Source/MainComponent.cpp` (paste special handler)
- `Source/ClipGrid/ClipButton.cpp` (context menu item)

---

## Category 7: Clip Groups (Session-Wide List) (MEDIUM PRIORITY)

### 7.1 Editable Clip Group Names

**Current:** Groups default to "Group 1", "Group 2", etc.
**Target:** User-editable group names (session-wide list)

**UI:**

- Clip Edit dialogue shows dropdown with group list
- Group names editable inline in dropdown (if possible)
- Or: Add "Edit Groups..." button → Opens preferences dialog

**Storage:**

- Save group names in session JSON
- Default to "Group 1", "Group 2", "Group 3", "Group 4"

**Implementation:**

- Add `groupNames` array to `SessionManager`
- Update `ClipEditDialog` dropdown to show custom names
- Add edit functionality (inline or via preferences)

**Files:**

- `Source/Session/SessionManager.h/cpp` (group names storage)
- `Source/UI/ClipEditDialog.cpp` (group dropdown)

---

### 7.2 Clip Group List Editor in Preferences

**Current:** No group editor
**Target:** Session Preferences tab with Clip Group list editor

**UI:**

- List of 4 groups (expandable in future)
- Editable names
- Color picker for each group (visual indicator)
- Background color in dropdown matches group color

**Implementation:**

- Build preferences dialog (see Category 8)
- Add Clip Group editor to Session Preferences tab
- Update group dropdown in Clip Edit dialog to use these colors

**Files:**

- `Source/UI/PreferencesDialog.h/cpp` (new component)
- `Source/Session/SessionManager.h/cpp` (group metadata)

---

## Category 8: Preferences System (HIGH PRIORITY)

### 8.1 Build Application & Session Preferences Dialog

**Current:** No preferences UI
**Target:** Single dialog with 3 tabs

**Tabs:**

1. **Application Preferences** (global settings)
2. **Session Preferences** (current session settings)
3. **Audio I/O** (replaces current Audio I/O Settings dialog)

**Keyboard Shortcut:** `Cmd+,` to open, `ESC` to close

**Implementation:**

- Create new `PreferencesDialog` component
- Add `juce::TabbedComponent` with 3 tabs
- Wire up `Cmd+,` in `MainComponent::keyPressed()`

**Files:**

- `Source/UI/PreferencesDialog.h/cpp` (new component)
- `Source/MainComponent.cpp` (keyboard shortcut)

---

### 8.2 Application Preferences Tab

**Settings:**

- Default session directory (file picker)
- Tab bar placement: Top, Bottom, Left, Right (future enhancement)
- (More planned - defer to future sprints)

**Implementation:**

- Add controls to Application Preferences tab
- Save to global config file (JSON or JUCE PropertiesFile)
- Load on application startup

**Files:**

- `Source/UI/PreferencesDialog.cpp` (Application tab)
- Add global settings manager (new class)

---

### 8.3 Session Preferences Tab

**Settings:**

- Clip grid dimensions (future: resizable grid)
- Clip Group list editor (see 7.2)
- Default Fade IN/OUT times and shapes
- Default clip color

**Implementation:**

- Add controls to Session Preferences tab
- Save to session JSON
- Apply defaults when loading new clips

**Files:**

- `Source/UI/PreferencesDialog.cpp` (Session tab)
- `Source/Session/SessionManager.h/cpp` (session defaults)

---

### 8.4 Audio I/O Preferences Tab

**Current:** Audio I/O Settings is separate dialog
**Target:** Move into Preferences dialog as third tab

**Settings:**

- Audio device selection
- Sample rate, buffer size
- Input/output channel routing
- Verbose latency info (moved from main UI)

**Implementation:**

- Move `AudioSettingsDialog` content into Preferences tab
- Remove standalone Audio I/O Settings dialog
- Update menu to open Preferences → Audio I/O tab

**Files:**

- `Source/UI/PreferencesDialog.cpp` (Audio I/O tab)
- `Source/UI/AudioSettingsDialog.h/cpp` (refactor into Preferences)
- `Source/MainComponent.cpp` (update menu handler)

---

## Category 9: Playbox Navigation (MEDIUM PRIORITY)

### 9.1 Playbox Outline (Last Clip Launched)

**Goal:** Thin white outline follows last clip launched

**Behavior:**

- Outline persists even when clip stops playing
- Outline is separate from "Active" pulsing border
- Helps user see which clip was last triggered

**Implementation:**

- Add `m_lastLaunchedClip` index to `ClipGrid`
- Update in `onClipTriggered()`
- Render thin white outline in `ClipButton::paint()`

**Files:**

- `Source/ClipGrid/ClipGrid.h/cpp` (track last launched)
- `Source/ClipGrid/ClipButton.cpp` (render playbox outline)

---

### 9.2 Arrow Key Navigation

**Goal:** Arrow keys move playbox around grid

**Keyboard:**

- `↑` `↓` `←` `→` move playbox to adjacent button
- Hold arrow key → Latch and accelerate (like Trim buttons)
- `ENTER` → Trigger clip under playbox (PLAY if stopped, STOP if playing)

**Latch Logic:**

- Initial delay: 300ms
- Acceleration: 300ms → 150ms → 75ms → 40ms (matches Trim button behavior)

**Implementation:**

- Add `m_playboxIndex` to `ClipGrid`
- Add arrow key handlers in `MainComponent::keyPressed()`
- Add timer for latch acceleration
- Add ENTER key handler to trigger playbox clip

**Files:**

- `Source/ClipGrid/ClipGrid.h/cpp` (playbox state)
- `Source/MainComponent.cpp` (arrow key handlers)
- `Source/ClipGrid/ClipButton.cpp` (render playbox outline)

---

## Category 10: Audio & Session Management (LOW PRIORITY)

### 10.1 Output Routing (Non-Default Audio Devices)

**Current:** Always uses system default audio output
**Target:** Allow user to select any connected audio output

**UI:**

- Add output device dropdown to Audio I/O Preferences
- Show all available audio devices
- Save selection to session or global config

**Implementation:**

- Query available audio devices via JUCE `AudioDeviceManager`
- Add device selection to `AudioSettingsDialog` (or Preferences → Audio I/O)
- Update `AudioEngine` to use selected device

**Files:**

- `Source/Audio/AudioEngine.h/cpp` (device selection)
- `Source/UI/AudioSettingsDialog.cpp` (device dropdown)

---

### 10.2 Orpheus Routing Matrix Integration (FUTURE)

**Goal:** Simple output routing via Orpheus Routing Matrix API

**Status:** Defer until Orpheus SDK exposes routing API

**Notes:**

- Multi-channel routing (2-32 channels)
- Route clip groups to different outputs
- Requires SDK enhancement

---

### 10.3 Audio Asset Management (Local Directory)

**Current:** Audio clips reference original file paths
**Target:** Copy audio assets to session directory on import

**Behavior:**

- On clip import → Copy file to `<session_dir>/audio/`
- On session save → Save relative paths to audio files
- On session load → Look for audio in `<session_dir>/audio/` first, then fallback to original paths

**Session Architecture:**

```
MySession.occproj/
├── session.json
└── audio/
    ├── clip_001.wav
    ├── clip_002.flac
    └── clip_003.aiff
```

**Implementation:**

- Add audio copy logic to `SessionManager::loadClipToButton()`
- Update session JSON to save relative paths
- Add file not found warnings on session load

**Files:**

- `Source/Session/SessionManager.h/cpp` (audio copy logic)
- Session JSON schema update

---

## Category 11: Miscellaneous Polish (LOW PRIORITY)

### 11.1 Application Header Bar - "Clip Composer"

**Current:** May say "Orpheus Clip Composer"
**Target:** Change to "Clip Composer"

**Rationale:** Orpheus is the SDK, Clip Composer is the application

**Files:**

- `Source/Main.cpp` (window title)
- JUCE Projucer settings (if applicable)

---

### 11.2 Application Icon (Runtime Testing)

**Current:** No custom icon
**Target:** Add simple application icon for runtime testing

**Implementation:**

- Design or find simple icon (musical note, waveform, grid)
- Add to Resources directory
- Set via JUCE Projucer or CMakeLists.txt

**Files:**

- `Resources/` (add icon file)
- CMakeLists.txt or JUCE Projucer

---

### 11.3 Ctrl+Click for Right-Click Menu

**Current:** Right-click only
**Target:** Ctrl+Click as alternative to right-click

**Rationale:** Right-clicking is risky in live scenarios (accidental clicks)

**Behavior:**

- Ctrl+Click on clip button → Show context menu
- Does NOT affect Ctrl+Opt+Cmd+Click (direct Edit Dialog access)

**Files:**

- `Source/ClipGrid/ClipButton.cpp` (`mouseDown()` handler)

---

### 11.4 Standard macOS Key Commands

**Commands:**

- `Cmd+Q` → Quit application
- `Cmd+S` → Save session
- `Cmd+Shift+S` → Save session as...
- `Cmd+Shift+F` → Toggle fullscreen
- `Cmd+,` → Open preferences

**Status:** Verify which are already implemented

**Files:**

- `Source/MainComponent.cpp` (menu handlers, `keyPressed()`)

---

### 11.5 Canadian Spelling

**Current:** Mix of American and Canadian spellings
**Target:** Default to Canadian spellings

**Examples:**

- "Color" → "Colour"
- "Organize" → "Organise" (if applicable)

**Implementation:**

- Search and replace in UI strings
- Update menu labels, dialog text, tooltips

**Files:**

- All UI components (grep for "Color", "color")

---

### 11.6 Waveform Load Speed Optimization

**Current:** Waveforms load slowly when Clip Edit dialogue opens
**Target:** Faster waveform display loading

**Possible Optimizations:**

- Pre-calculate waveform data on clip import
- Cache waveform thumbnails
- Use lower resolution for initial display, high-res on zoom

**Files:**

- `Source/UI/WaveformDisplay.cpp` (waveform generation)
- `Source/Session/SessionManager.cpp` (cache waveform data)

---

### 11.7 Clip Button Indicator Icon Grid Tightening

**Current:** Icon grid spacing may be loose
**Target:** Slightly tighter horizontal spacing

**Goal:** Ensure all icons vertically centered with PLAY and GROUP indicators
**Row Height:** Match bottom corner indicators

**Files:**

- `Source/ClipGrid/ClipButton.cpp` (`drawStatusIcons()`)

---

### 11.8 Build Script Updates

**Scripts:**

- `/Users/chrislyons/dev/orpheus-sdk/apps/clip-composer/build-launch.sh`
- `/Users/chrislyons/dev/orpheus-sdk/apps/clip-composer/clean-relaunch.sh`

**Goal:** Ensure scripts are up to date and reliable

**Status:** Verify scripts work with current build system

**Files:**

- `build-launch.sh`
- `clean-relaunch.sh`

---

## Priority Breakdown

### Blocking (v0.2.2 - Must Fix First)

0. 🐛 **Playhead visual bug** (non-looped clips extend past OUT point)

### Must-Have (v0.2.2)

1. ✅ Typography migration (Inter → HK Grotesk)
2. ✅ Clip button time display (elapsed — remaining)
3. ✅ Tab + Panic bar merge
4. ✅ Preferences dialog (3 tabs)
5. ✅ Clip copy/paste
6. ✅ Warning prompts for clear actions

### Should-Have (v0.2.3)

7. Two-row text layout (Clip Name + User Notes)
8. Tab renaming (double-click + context menu)
9. Clip Groups (editable names + abbreviations)
10. Playbox navigation (arrow keys + ENTER)
11. Beat indicator on clip buttons
12. Corner indicator sizing consistency

### Nice-to-Have (v0.3.0+)

13. Gain dropdown (replace slider)
14. Beat field in Edit Dialog
15. Transport icon redesign
16. Audio asset management (local copy)
17. Output routing (non-default devices)
18. Waveform load speed optimization

---

## Implementation Order (Recommended)

### Sprint 0 (Immediate): Critical Bug Fix

0. **Fix playhead visual bug** (non-looped clips past OUT point) - BLOCKING

### Sprint A (Week 1-2): Typography & Core Layout

1. Replace Inter with HK Grotesk (all files)
2. Clip button time display (elapsed — remaining)
3. Corner indicator sizing & consistency
4. Beat indicator on clip buttons

### Sprint B (Week 3-4): Preferences & Tab System

5. Build Preferences Dialog (3 tabs)
6. Application Preferences tab
7. Session Preferences tab
8. Audio I/O Preferences tab (move from standalone)
9. Tab + Panic bar merge
10. Status indicator lights (latency + heartbeat)
11. Tab renaming (double-click + context menu)

### Sprint C (Week 5-6): Clip Management

12. Two-row text layout (Clip Name + User Notes)
13. Clip copy/paste (full clip + paste special)
14. Warning prompts for clear actions
15. Clip Groups (editable names + abbreviations)

### Sprint D (Week 7-8): Edit Dialog Polish

16. TAB key "No Focus" state
17. Fade time fields (numeric input only)
18. Clip Name auto-highlight on TAB
19. Gain dropdown (replace slider)
20. Beat field (with Option key easter egg)
21. OK/CANCEL button sizing
22. Transport icon redesign

### Sprint E (Week 9-10): Navigation & Polish

23. Playbox navigation (arrow keys + ENTER)
24. Ctrl+Click for right-click menu
25. Standard macOS key commands
26. Canadian spelling pass
27. Waveform load speed optimization
28. Build script updates

---

## Testing Checklist (Per Sprint)

### Sprint 0 (Critical Bug Fix)

- [ ] Load non-looped clip with fade OUT = 1.0s
- [ ] Set OUT point at 10 seconds
- [ ] Play clip and verify playhead stops at 10s (not 11s)
- [ ] Verify audio fades correctly within OUT bounds
- [ ] Test with various fade OUT times (0.5s, 2.0s, 3.0s)
- [ ] Verify looped clips still work correctly (no regression)

### Sprint A

- [ ] HK Grotesk loaded and renders correctly
- [ ] No Inter font visible anywhere in UI
- [ ] Clip button time display shows elapsed — remaining
- [ ] Corner indicators all same size (3-char width)
- [ ] Beat indicator appears next to clip number

### Sprint B

- [ ] Preferences dialog opens with Cmd+,
- [ ] All 3 tabs functional (Application, Session, Audio I/O)
- [ ] Tab bar merged with Panic buttons (single row)
- [ ] Status lights show latency and heartbeat
- [ ] Double-click tab to rename works
- [ ] Right-click tab shows context menu

### Sprint C

- [ ] Clip Name and User Notes appear on separate rows
- [ ] Cmd+C / Cmd+V copies and pastes clips
- [ ] "Paste Special" only copies states (not audio)
- [ ] Clear actions show warning prompts
- [ ] Clip Groups show custom names and abbreviations

### Sprint D

- [ ] TAB cycles through Name → Fade IN → Fade OUT → No Focus
- [ ] Fade fields only accept numeric input
- [ ] TAB to Clip Name auto-highlights text
- [ ] Gain dropdown replaces slider
- [ ] Beat dropdown shows 1/1+/2/2+/3/3+/4/4+ (Option for 1++/2++/etc.)
- [ ] OK/CANCEL buttons 50% taller

### Sprint E

- [ ] Arrow keys move playbox around grid
- [ ] ENTER triggers clip under playbox
- [ ] Ctrl+Click shows clip context menu
- [ ] Cmd+Q, Cmd+S, Cmd+Shift+S, Cmd+, all work
- [ ] All UI text uses "Colour" (Canadian spelling)

---

## Dependencies & Risks

### External Dependencies

- HK Grotesk font files (already in Resources)
- JUCE framework (already integrated)
- Orpheus SDK (routing API for future features)

### Technical Risks

- Font loading may require JUCE custom font API
- Preferences dialog state management (global vs session)
- Copy/paste clipboard handling (cross-platform)
- Audio asset copying (file I/O on clip import)

### Design Risks

- HK Grotesk may render differently than Inter (test thoroughly)
- Two-row text layout may not fit on small clip buttons (test grid sizing)
- Tab + Panic bar merge may feel cramped (prototype first)

---

## Deferred Items (Not in v0.2.x)

1. **Resizable clip grid** (user-defined rows/columns) - v0.3.0+
2. **Orpheus Routing Matrix integration** - Pending SDK API
3. **Remote control via OSC** (iOS app) - v0.4.0+
4. **BPM detection and beatgrid** - v0.5.0+
5. **Time signature configuration** - v0.5.0+

---

## Success Criteria

**v0.2.2 is complete when:**

- ✅ HK Grotesk replaces Inter throughout application
- ✅ Clip buttons show elapsed — remaining times prominently
- ✅ Preferences dialog functional with 3 tabs
- ✅ Tab renaming works (double-click + context menu)
- ✅ Clip copy/paste implemented (full + paste special)
- ✅ All clear actions have warning prompts
- ✅ Build runs without errors on macOS
- ✅ All Sprint A-C testing checklist items pass

**Documentation:**

- This sprint plan (OCC130)
- Session report after each sprint (OCC131, OCC132, etc.)
- Update CLAUDE.md if architectural changes made

---

**End of OCC130**
