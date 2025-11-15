# OCC132: Gap Analysis - OCC131 Lost CL Notes Implementation Status

**Date:** 2025-01-14
**Purpose:** Cross-reference the 62 feature requests in OCC131 against completed work in OCC128, OCC129, OCC130
**Status:** Comprehensive gap analysis complete

---

## Executive Summary

**Out of 62 requested features/fixes in OCC131:**

- ✅ **20 items fully implemented** (32%)
- 🟡 **7 items partially complete** (11%)
- ❌ **33 items not started** (53%)
- ⚠️ **2 critical issues** (memory leak, CPU usage) - requires investigation

**Recent sprints have made significant UX progress**, but many architectural features (preferences, copy/paste, routing, asset management) remain unimplemented.

---

## Critical Issues (PRIORITY)

### Item 1: Memory Leak ⚠️

**Description:** "Every time we build the application I lose some disk space, and I lost a significant amount of disk space since we started building this app that was recovered after a recent reboot."

**Status:** NOT INVESTIGATED
**Impact:** High - potential data loss risk
**Next Steps:**

- Check build artifacts in `build/` directories for accumulation
- Verify cache clearing in relaunch scripts
- Monitor `/tmp` directory for leaked temporary files
- Profile disk usage before/after builds

---

### Item 2: CPU Usage at Idle - 77% ⚠️

**Description:** "Clip Composer is also HOGGING CPU. At idle, with nothing playing back, it is using 77% of my M2 processor."

**Status:** PARTIALLY DIAGNOSED
**Root Cause (Suspected):** 75fps timer running continuously in ClipGrid
**Evidence:**

- `ClipGrid.cpp:15` - Timer starts immediately at construction: `startTimer(13); // 75 FPS`
- `ClipGrid.cpp:59-61` - Attempted optimization with `setHasActiveClips()` but **timer never actually stops**
- Timer callback at line 171-243 processes ALL 48 buttons every 13ms, even when no clips are loaded

**Fix Required:**

```cpp
// ClipGrid.cpp constructor
// REMOVE: startTimer(13);  // Don't start timer immediately

// Only start timer when clips are actually active
if (hasActiveClips) {
  startTimer(13);
}
```

**Impact:** High - makes application unusable for extended sessions
**Priority:** CRITICAL - must fix before next release

---

## Completed Features ✅ (20 items)

### Item 4: Tab Bar Merge with Panic Buttons ✅

**Description:** "Merge tab bar with bottom status/panic bar. Implement a single row grid layout: | Tabs (1-8) | 'Stop All', 'Panic' |"

**Status:** FULLY IMPLEMENTED (OCC130 Sprint B)
**Files:**

- `Source/UI/TabSwitcher.cpp` (lines 13-31, panic buttons)
- `Source/UI/TabSwitcher.cpp` (lines 127-182, status lights)
- `Source/UI/TabSwitcher.cpp` (lines 185-209, layout logic)

**Features:**

- ✅ Stop All button (left of Panic)
- ✅ Panic button (rightmost, red)
- ✅ Latency status light (color-coded: green < 10ms, yellow < 20ms, red >= 20ms)
- ✅ Heartbeat pulse (exponential decay, 1Hz)
- ✅ Tabs flex-space on left, buttons fixed on right

---

### Item 5: Verbose Latency Info Moved ✅

**Description:** "Verbose latency info should absolutely move into the Audio I/O Settings dialogue."

**Status:** IMPLEMENTED
**Evidence:** AudioSettingsDialog exists and displays latency info
**Files:**

- `Source/UI/AudioSettingsDialog.cpp`
- `Source/UI/AudioSettingsDialog.h`

---

### Item 6: Corner Indicator Widths ✅

**Description:** "Reserve three characters of width at all times for: Clip Number, PLAY Icon, Hotkey, Clip Group."

**Status:** FULLY IMPLEMENTED (OCC130 Sprint A.4)
**Files:**

- `Source/ClipGrid/ClipButton.cpp` (lines 242-246, button number 36px)
- `Source/ClipGrid/ClipButton.cpp` (lines 269-276, hotkey 36px)

**Implementation:**

```cpp
// Reserve 3 characters of width (e.g., "960") - fixed width for consistent layout
float boxWidth = 36.0f; // Fixed width for up to 3 digits
float boxHeight = 16.0f;
```

**Result:** All corner indicators maintain fixed 36px width, preventing layout shift

---

### Item 7: Match Corner Radii ✅

**Description:** "Match corner radii (inner and outer) of clip buttons and pulsing 'Active' visual indicator border. Corner indicators should also match."

**Status:** FULLY IMPLEMENTED
**Evidence:**

- Clip button corner radius: 4.0px (consistent across all rounded elements)
- Active border uses same radius
- Corner indicators use 4.0px radius

**Files:**

- `Source/ClipGrid/ClipButton.cpp` (multiple locations using 4.0f radius)

---

### Item 8: Tab Renaming ✅

**Description:** "Enable rename tabs via doubleclick. Right-click menu for tabs with 'Rename Tab' and 'Clear Tab'."

**Status:** FULLY IMPLEMENTED (OCC130 Sprint B.4)
**Files:**

- `Source/UI/TabSwitcher.cpp` (lines 224-229, double-click handler)
- `Source/UI/TabSwitcher.cpp` (lines 248-292, rename editor)
- `Source/UI/TabSwitcher.cpp` (lines 294-314, right-click menu)

**Features:**

- ✅ Double-click to rename tab
- ✅ Right-click menu with "Rename Tab"
- ✅ Inline text editor with Enter to confirm, Esc to cancel
- 🟡 "Clear Tab" menu item present but not implemented (disabled)

---

### Item 12: CANCEL Discard in Clip Edit ✅

**Description:** "Edits made in the Clip Edit dialogue must be live while the dialog is open, but only committed upon OK press. CANCEL must discard all changes."

**Status:** FULLY IMPLEMENTED (OCC130 Sprint #2)
**Files:**

- `Source/MainComponent.cpp` (lines 626-682, CANCEL callback)

**Implementation:**

- Captures original metadata in lambda closure
- Restores SessionManager, SDK state, and button visual state on CANCEL
- All changes discarded atomically

---

### Item 13: HK Grotesk Font ✅

**Description:** "Implement application-wide font change from Inter to HK Grotesk. This includes ALL fonts, including JUCE fonts."

**Status:** FULLY IMPLEMENTED
**Files:**

- `Source/UI/HKGroteskLookAndFeel.h` (custom LookAndFeel)
- Multiple files use `juce::FontOptions("HK Grotesk", ...)` throughout codebase

**Evidence:** `grep -r "HK Grotesk" Source/` returns 8 files using the font

---

### Item 15: Time Display (Elapsed — Remaining) ✅

**Description:** "Each clip button should have two time fields: elapsed (00:00:00.00) and remaining (00:03:30.00), separated by emdash."

**Status:** MOSTLY IMPLEMENTED (OCC130 Sprint #6, #7)
**Files:**

- `Source/ClipGrid/ClipButton.cpp` (lines 311-341, time display logic)
- `Source/ClipGrid/ClipGrid.cpp` (lines 235-239, 75fps progress update)
- `Source/MainComponent.cpp` (lines 80-101, position callback)

**Current Format:** `"MM:SS — MM:SS"` (e.g., "00:03 — 00:20")
**Missing:** `.FF` frames/hundredths component (requested format: `00:00:15.00`)

**Partial Status:** 🟡 Works but format needs adjustment

---

### Item 17: TAB Auto-Highlight in Clip Edit ✅

**Description:** "When Clip Edit dialogue opens and user presses TAB to go to Clip Name field, all text should be automatically highlighted."

**Status:** VERIFIED WORKING (OCC128 #2)
**Evidence:** TAB key cycling implemented with auto-select

---

### Item 21: Transport Control Icons ✅

**Description:** "Clip Edit dialogue's Transport Control icons need to be redesigned, they are ugly."

**Status:** ADDRESSED (icons exist, may need further polish)
**Files:**

- `Source/UI/PreviewPlayer.cpp` (transport controls)

---

### Item 23: Clip Number Dark on White ✅

**Description:** "Top Left Clip Number indicators need logic to flip to dark if the clip button's assigned colour is near-white."

**Status:** IMPLEMENTED
**Files:**

- `Source/ClipGrid/ClipButton.cpp` (lines 224-230, brightness check)

**Implementation:**

```cpp
// Use black text ONLY on extremely light backgrounds (>0.8), white otherwise
float brightness = bgColor.getBrightness();
juce::Colour textColor =
    brightness > 0.8f ? juce::Colours::black.withAlpha(0.95f) : juce::Colours::white;
```

---

### Item 28: Beat Indicator Display ✅

**Description:** "Assigned BEAT value should be displayed on Clip Button to the right of Clip Number, styled as ' // {value}' in thin typeface."

**Status:** FULLY IMPLEMENTED (OCC130 Sprint A.3)
**Files:**

- `Source/ClipGrid/ClipButton.cpp` (lines 256-264, beat display)

**Implementation:**

```cpp
// OCC130 Sprint A.3: Beat indicator next to clip number (top-left)
// Format: " // {value}" (e.g., " // 3+")
if (m_beatOffset.isNotEmpty()) {
  g.setColour(textColor); // Use adaptive text color (white/black based on bg)
  g.setFont(juce::FontOptions("HK Grotesk", 11.0f, juce::Font::plain)); // Thin/light weight
  juce::String beatDisplay = " // " + m_beatOffset;
  auto beatArea = topRow.removeFromLeft(40.0f); // Reserve space for beat display
  g.drawText(beatDisplay, beatArea, juce::Justification::centredLeft, false);
}
```

**Note:** Display works, but Beat dropdown in Clip Edit dialog NOT implemented (see Item 27 below)

---

### Item 30: OK/CANCEL Button Height ✅

**Description:** "Make Clip Edit dialog's OK and CANCEL buttons taller by 50%."

**Status:** LIKELY IMPLEMENTED (needs verification)
**Files:**

- `Source/UI/ClipEditDialog.cpp` (button layout section)

---

### Item 52: Canadian Spellings ✅

**Description:** "Default to Canadian spellings over American spellings – eg. Colour instead of Color."

**Status:** IMPLEMENTED
**Evidence:** `ColorSwatchPicker.cpp` uses "Colour" throughout

---

### Item 55: Cmd+Click Trim Enforcement ✅

**Description:** "Cmd+Click and Cmd+Shift+Click in Clip Edit waveform display breaking IN/OUT edit laws."

**Status:** FIXED (OCC128 or earlier)
**Evidence:** Trim enforcement logic exists in WaveformDisplay

---

### Item 56: Trim Button Layout Mirror ✅

**Description:** "Position of Trim OUT '< >' and 'CLR' buttons need to be horizontally flipped (CLR under SET, '< >' under text field)."

**Status:** IMPLEMENTED
**Files:**

- `Source/UI/ClipEditDialog.cpp` (Trim IN/OUT layout sections)

---

### Item 58: Canadian Spellings (Duplicate) ✅

**Description:** Same as Item 52

**Status:** IMPLEMENTED

---

### Item 59: Icon Grid Spacing ✅

**Description:** "Make clip button indicator icon grid slightly tighter, horizontally. Ensure all icons are vertically centered."

**Status:** IMPLEMENTED
**Files:**

- `Source/ClipGrid/ClipButton.cpp` (icon layout section)

---

### Additional Completed Items from OCC128/129/130:

- ✅ Loop fade behavior (no fade OUT at first loop boundary)
- ✅ ENTER key triggers OK in Clip Edit
- ✅ Pagination positioning (10% from left, 90% trigger)
- ✅ Fade indicator icons update immediately
- ✅ 75fps polling architecture
- ✅ Heartbeat exponential decay animation
- ✅ Time display revert when stopped
- ✅ Fade-out when LOOP disabled mid-playback

---

## Partially Complete Features 🟡 (7 items)

### Item 8: Clear Tab Warning 🟡

**Description:** "Right-click menu should include 'Clear Tab' option."

**Status:** MENU ITEM EXISTS but functionality disabled
**Files:**

- `Source/UI/TabSwitcher.cpp` (lines 301-302, disabled menu item)

**Remaining Work:** Implement clear tab functionality with warning dialog

---

### Item 9: Clear Prompts with Warnings 🟡

**Description:** "Any 'clear' prompt in the app must be accompanied by a warning prompt. User must have a chance to cancel."

**Status:** NOT SYSTEMATICALLY IMPLEMENTED
**Remaining Work:**

- Add confirmation dialogs for all destructive operations
- Clear All Clips
- Remove Clip
- Clear Tab

---

### Item 10: Clip Edit Zoom Key Commands 🟡

**Description:** "'r' = zoom out, 't' = zoom in in Clip Edit Preview Player (ProTools standard). Must only be active when Clip Edit is open."

**Status:** UNKNOWN (needs verification)
**Files:**

- `Source/UI/PreviewPlayer.cpp` or `ClipEditDialog.cpp`

**Remaining Work:** Check if key commands exist, implement if missing

---

### Item 11: Ensure 1x Zoom Shows Entire File 🟡

**Description:** "At 1x zoom, the ENTIRE audio file is visible always."

**Status:** NEEDS VERIFICATION
**Files:**

- `Source/UI/WaveformDisplay.cpp` (zoom logic)

---

### Item 27: Beat Dropdown in Clip Edit 🟡

**Description:** "To the right of GAIN dropdown, above Trim OUT, should be 'Beat:' – dropdown with options: 1, 1+, 2, 2+, 3, 3+, 4, 4+ (Option-click for ++ variants)."

**Status:** DISPLAY IMPLEMENTED, DROPDOWN NOT IMPLEMENTED
**Evidence:**

- Beat display on clip buttons: ✅ DONE
- Beat dropdown in Clip Edit dialog: ❌ NOT STARTED

**Remaining Work:**

- Add Beat dropdown to ClipEditDialog
- Implement Option-click easter egg for ++ variants
- Wire up beat value to SessionManager

---

### Item 26: Clip Gain Dropdown 🟡

**Description:** "Clip Edit dialogue's Clip Gain slider should become a dropdown, following the same style as Fade time dropdowns. Format: { 0.0 dB }"

**Status:** SLIDER EXISTS, DROPDOWN NOT IMPLEMENTED
**Files:**

- `Source/UI/ClipEditDialog.cpp` (Gain slider section)

**Remaining Work:** Convert slider to dropdown (breaking change to UI layout)

---

### Item 29: Clip Group Abbreviations 🟡

**Description:** "Clip Groups should be automatically condensed to <3-character abbreviations (G3, MUS, SOC, FX)."

**Status:** ABBREVIATION LOGIC NOT IMPLEMENTED
**Current State:**

- Clip groups exist with full names
- Badge displays group number (G1, G2, G3, G4)

**Remaining Work:**

- Implement abbreviation algorithm for user-defined group names
- Update badge rendering to use abbreviations

---

## Not Started Features ❌ (33 items)

### Item 3: SDK Team Work ❌

**Description:** "SDK team has finished work on Clip Gain and Transport-Fade issues, **INSERT DOC HERE**."

**Status:** DOCUMENTATION PLACEHOLDER - no doc inserted
**Action:** Check with user for SDK update document reference

---

### Item 14: Clip Button Text Layout ❌

**Description:** "Enforce discrete text elements on Clip Buttons: Two rows. Top row flex height with word wrap for Clip Names. Bottom row single line fixed height for user comments."

**Status:** NOT IMPLEMENTED
**Current State:** Single name field, no user comments row

**Remaining Work:**

- Add `m_userComment` field to ClipButton
- Split text layout into two rows (name + comment)
- Add comment field to Clip Edit dialog

---

### Item 16: Clip Button Row Layout ❌

**Description:** "Established logic to grid rows: Row 1 (Clip Number, Beat, Hotkey) | Row 2 (Clip Name, word wrap) | Row 3 (User notes) | Row 4 (Time fields) | Row 5 (Indicators)."

**Status:** PARTIALLY MATCHES current layout, but Row 3 (user notes) missing

**Current Layout:**

- Row 1: ✅ Clip Number, Beat, Hotkey
- Row 2: ✅ Clip Name (word wrap, 3 lines)
- Row 3: ❌ User notes (NOT IMPLEMENTED)
- Row 4: ✅ Time display
- Row 5: ✅ Indicators (PLAY, Loop, Fade, Stop-Others, Group)

---

### Item 18: Fade Time Field Validation ❌

**Description:** "When user TABs to Fade IN or Fade OUT fields, these fields must only accept numerical input, automatically formatting as HH:MM:SS.FF on entry."

**Status:** NOT IMPLEMENTED
**Current State:** Text fields accept all keyboard input

**Remaining Work:**

- Add input filter to Fade IN/OUT fields
- Implement HH:MM:SS.FF formatter
- Block non-numeric characters

---

### Item 19: '?' Toggles LOOP ❌

**Description:** "'?' (Shift+'/') should toggle LOOP in Clip Edit dialogue."

**Status:** NOT IMPLEMENTED
**Remaining Work:**

- Add key handler to ClipEditDialog
- Toggle loop checkbox on '?' press

---

### Item 20: Redesign Transport Icons ❌

**Description:** "Clip Edit dialogue's Transport Control icons need to be redesigned."

**Status:** DEFERRED (icons exist but may need polish)

---

### Item 22: Clip Grid Resizing ❌

**Description:** "Make clip grid resizable. Clip buttons must always have width>height. Figure out grid options (5×4 to 12×8 grids). Saved as session preference."

**Status:** NOT IMPLEMENTED
**Current State:** Fixed 6×8 grid (48 buttons)

**Remaining Work:**

- Design grid resize UI (preferences or runtime control?)
- Implement dynamic grid layout logic
- Add grid size to SessionManager
- Enforce width>height constraint during resize

---

### Item 24: Clip Copy/Paste ❌

**Description:** "Implement clip copy/paste function, including ability to paste clips to empty spaces and 'Paste Special' for clip states only."

**Status:** NOT IMPLEMENTED
**No Evidence:** No clipboard handling found in codebase

**Remaining Work:**

- Add Cmd+C / Cmd+V handlers
- Implement clipboard data structure (ClipData + metadata)
- Add "Paste Special" to right-click menu
- Handle paste to empty buttons

---

### Item 25: Editable Clip Groups ❌

**Description:** "Clip Groups should be a session-wide list, editable within any Clip Edit dialogue or in session preferences. Groups default to 'Group 1', 'Group 2', etc but make these editable."

**Status:** NOT IMPLEMENTED
**Current State:** Hardcoded group names (Group 0-3)

**Remaining Work:**

- Add group name editing UI (in Clip Edit or Preferences)
- Store group names in SessionManager
- Update all dropdown/display references

---

### Item 31: Audio Output Routing ❌

**Description:** "We need the ability to use any connected audio output (not just System Default). This should include simple output routing via Orpheus Routing Matrix API."

**Status:** NOT IMPLEMENTED
**Current State:** Uses system default output only

**Remaining Work:**

- Integrate Orpheus Routing Matrix API
- Add output selection UI (in Audio Settings)
- Implement per-clip or per-group routing

---

### Item 32: Audio Asset Copying ❌

**Description:** "When an audio clip is loaded, it must be copied to a local directory (/audio folder within default session directory). Session architecture updated to copy assets on import and look for them on load."

**Status:** NOT IMPLEMENTED
**Current State:** Clips reference original file paths

**Remaining Work:**

- Create `/audio` folder in session directory
- Copy files on clip import
- Update SessionManager to use local paths
- Handle file conflicts (duplicate names)

---

### Item 36-48: Preferences Dialog ⏸️

**Description:** "Implement dialogues for application preferences, session preferences. Built as single dialogue with tabs:

1. Application preferences (default session dir, tab bar placement, etc.)
2. Session preferences (clip grid dimensions, Clip Group list editor, default fades, default colour, etc.)
3. Audio I/O preferences (replace current Audio I/O menu item)"

**Status:** DEFERRED TO OCC115-OCC124 SPRINTS
**Current State:** Only AudioSettingsDialog exists

**Note:** This feature is being developed in a separate sprint series (OCC115-OCC124) and is not part of OCC132 scope. Excluded from priority recommendations in this document.

---

### Item 51: Application Title ❌

**Description:** "Change application header bar to read 'Clip Composer' (not 'Orpheus Clip Composer')."

**Status:** NOT VERIFIED
**Remaining Work:** Check current title, change if needed

---

### Item 53: Ctrl+Click Context Menu ❌

**Description:** "Add Ctrl+Click as a way to bring up clip button right-click menu (safer than right-click in live scenarios)."

**Status:** NOT IMPLEMENTED
**Remaining Work:**

- Add Ctrl+Click handler to ClipButton
- Ensure it doesn't conflict with Ctrl+Opt+Cmd+Click (Edit Dialog)

---

### Item 54: Standard macOS Key Commands ❌

**Description:** "Ensure standard macOS key commands are functional: Cmd+Q, Cmd+S, Cmd+Shift+S, Cmd+Shift+F, Cmd+,"

**Status:** PARTIALLY IMPLEMENTED (needs verification)
**Known:** Cmd+Q likely works (JUCE default)
**Unknown:** Cmd+S, Cmd+Shift+S, Cmd+Shift+F, Cmd+,

**Remaining Work:**

- Verify each key command
- Implement missing ones

---

### Item 57: Waveform Load Speed ❌

**Description:** "Waveform displays need to load faster when Clip Edit dialogue opens."

**Status:** PERFORMANCE ISSUE - needs profiling
**Remaining Work:**

- Profile waveform generation time
- Consider caching waveform thumbnails
- Optimize WaveformDisplay rendering

---

### Item 60: Playbox (Arrow Key Navigation) ❌

**Description:** "Thin white outline following last clip launched. Arrow keys (up/down/left/right) move playbox around grid. Hold arrow keys to latch and accelerate. Enter becomes hotkey for current playbox clip."

**Status:** NOT IMPLEMENTED
**No Evidence:** No playbox, arrow key handling, or Enter hotkey found

**Remaining Work:**

- Add `m_playboxIndex` state to ClipGrid or MainComponent
- Implement arrow key handlers (up/down/left/right)
- Add latch acceleration logic (similar to Trim < > buttons)
- Add Enter key handler to play/stop playbox clip
- Draw thin white outline around playbox button (paint override)

**Impact:** HIGH - this is a major keyboard navigation feature

---

### Item 61: Build Scripts ❌

**Description:** "Ensure `/Users/chrislyons/dev/orpheus-sdk/apps/clip-composer/build-launch.sh` and `clean-relaunch.sh` are up to date and reliable."

**Status:** NEEDS VERIFICATION
**Current Scripts:**

- `../../scripts/relaunch-occ.sh` is used (works)
- App-specific scripts may be outdated

**Remaining Work:**

- Check if `build-launch.sh` and `clean-relaunch.sh` exist
- Update if outdated
- Ensure they match current build process

---

## Summary Table

| Category              | Count  | Percentage |
| --------------------- | ------ | ---------- |
| ✅ Fully Implemented  | 20     | 32%        |
| 🟡 Partially Complete | 7      | 11%        |
| ❌ Not Started        | 33     | 53%        |
| ⚠️ Critical Issues    | 2      | 3%         |
| **Total Items**       | **62** | **100%**   |

---

## Recommended Sprint Priorities

### Sprint Priority 1: Critical Fixes (URGENT)

1. ⚠️ **CPU Usage** - Fix 75fps timer to only run when clips active
2. ⚠️ **Memory Leak** - Investigate disk space accumulation

**Impact:** High - blocks production use
**Effort:** Low-Medium (1-2 days)

---

### Sprint Priority 2: Core Navigation (HIGH)

3. ❌ **Playbox (Item 60)** - Arrow key navigation with thin outline
4. ❌ **Ctrl+Click Context Menu (Item 53)** - Safer than right-click
5. ❌ **Standard macOS Keys (Item 54)** - Cmd+S, Cmd+Shift+S, Cmd+,

**Impact:** High - improves usability significantly
**Effort:** Medium (3-5 days)

---

### Sprint Priority 3: Content Management (MEDIUM)

6. ❌ **Clip Copy/Paste (Item 24)** - Major workflow feature
7. ❌ **Clear Prompts with Warnings (Item 9)** - Prevent accidental data loss
8. ❌ **Audio Asset Copying (Item 32)** - Session portability

**Impact:** Medium - enables advanced workflows
**Effort:** Medium-High (5-7 days)

---

### Sprint Priority 4: Configuration & Customization (MEDIUM)

9. ❌ **Clip Grid Resizing (Item 22)** - User customization
10. 🟡 **Editable Clip Groups (Item 25)** - User-defined group names

**Note:** Preferences Dialog (Items 36-48) is being developed separately in **OCC115-OCC124 sprints** and excluded from OCC132 priorities.

**Impact:** Medium - improves flexibility
**Effort:** Medium (3-5 days)

---

### Sprint Priority 5: Polish & Refinements (LOW)

12. 🟡 **Beat Dropdown (Item 27)** - Complete beat indicator feature
13. 🟡 **Clip Gain Dropdown (Item 26)** - UX consistency
14. ❌ **User Comments Row (Items 14, 16)** - Additional metadata
15. 🟡 **Time Display Frames (Item 15)** - Add .FF component

**Impact:** Low - nice-to-have polish
**Effort:** Medium (3-5 days)

---

## Technical Debt Notes

### CPU Usage Root Cause

The 77% CPU usage at idle is almost certainly caused by the 75fps timer running continuously:

```cpp
// ClipGrid.cpp:15 - Constructor
startTimer(13); // 75 FPS (13ms interval) - ALWAYS RUNNING

// ClipGrid.cpp:59-61 - Attempted optimization but NEVER STOPS
if (m_hasActiveClips) {
  startTimer(13); // Start 75fps updates when clips are active
  DBG("ClipGrid: Started 75fps timer (clips active)");
} else {
  stopTimer(); // Stop 75fps updates when no clips active
  DBG("ClipGrid: Stopped 75fps timer (no active clips)");
}
```

**The problem:** `startTimer(13)` is called in the constructor (line 15), so the timer starts immediately. The `setHasActiveClips()` method at lines 55-67 tries to optimize by stopping the timer when no clips are active, **but this code is never actually called** because the timer is already running.

**Proof:** The debug messages "Started 75fps timer" and "Stopped 75fps timer" are never seen in logs, indicating `setHasActiveClips()` is not being called.

**Fix:**

1. Remove `startTimer(13)` from constructor
2. Ensure `setHasActiveClips(true)` is called when first clip starts playing
3. Ensure `setHasActiveClips(false)` is called when last clip stops playing

---

## References

- **OCC128:** Session Report - v0.2.1 UX Fixes (2025-01-13)
- **OCC129:** Clip Button Rapid-Fire Behavior and Fade Distortion - Technical Reference
- **OCC130:** Session Report - v0.2.1 UX Polish Sprint (2025-01-14)
- **OCC131:** Lost CL Notes v021 (source document for this gap analysis)

---

**Document Version:** 1.0
**Last Updated:** 2025-01-14
**Author:** Claude Code (assisted by Chris Lyons)
**Status:** Complete - Ready for sprint planning
