# OCC104 - v0.2.1 Sprint Plan

**Version:** 1.0
**Status:** Active Planning
**Created:** 2025-11-01
**Target Release:** v0.2.1 (Weeks 1-6, November-December 2025)
**Prerequisites:** v0.2.0 Complete (OCC093), QA Pending (OCC103)

---

## Executive Summary

This document defines the complete development roadmap for Clip Composer v0.2.1 through v0.3.0, organized as 12 focused sprints addressing critical performance issues, UI/UX polish, session management, and backend architecture. The plan builds on the successful v0.2.0 release (OCC093) and addresses user feedback while advancing toward MVP goals.

**Current State:**

- ✅ v0.2.0 Sprint complete (6 UX fixes implemented)
- ⏳ v0.2.0 QA pending (OCC103 template ready)
- ✅ Build system functional (build-launch.sh, clean-relaunch.sh verified)
- ✅ SDK stable (2 minor bugs deferred to ORP097)

**Scope:**

- 12 sprints, 6 weeks total duration
- ~80 tasks across performance, UI, session management, and polish
- Priority-ordered: P0 (blocking) → P1 (MVP-critical) → P2 (polish)

**Key Deliverables:**

1. Performance optimization (CPU, memory, latency)
2. Complete UI/UX refinement (tab bar, status system, clip buttons)
3. Enhanced Clip Edit Dialog with live preview
4. Session management backend (groups, grid sizing, asset management)
5. Preferences system (application, session, audio I/O)
6. Professional typography and branding

---

## Sprint Overview

| Sprint | Focus Area                             | Priority | Duration | Dependencies          |
| ------ | -------------------------------------- | -------- | -------- | --------------------- |
| 1      | Critical Performance & SDK Integration | P0       | 3-5 days | None (blocking)       |
| 2      | Tab Bar & Status System                | P1       | 2-3 days | Sprint 1              |
| 3      | Clip Button Visual System              | P1       | 3-4 days | Sprint 2 optional     |
| 4      | Clip Edit Dialog Refinement            | P1       | 4-5 days | Sprint 1              |
| 5      | Session Management & Clip Groups       | P1       | 3-4 days | Sprint 4.1            |
| 6      | Keyboard Navigation & Playbox          | P2       | 2-3 days | Sprint 3 helpful      |
| 7      | Typography & Application Polish        | P2       | 2-3 days | All previous          |
| 8      | Preferences System                     | P2       | 3-4 days | Sprint 5              |
| 9      | Audio Output Routing                   | P2       | 1-2 days | SDK routing API       |
| 10     | Session Backend Architecture           | P1       | 4-5 days | Sprint 5, 8           |
| 11     | Standard macOS Behaviors               | P2       | 1 day    | None                  |
| 12     | Build System & Development Tools       | P2       | 1 day    | All features complete |

**Total Estimated Duration:** 6 weeks (30 working days)

---

## SPRINT 1: Critical Performance & SDK Integration (P0 - Week 1)

**Priority:** BLOCKING
**Est. Duration:** 3-5 days
**Prerequisites:** Current v0.2.0 codebase
**Goal:** Resolve critical performance issues affecting user experience

### 1.1 Critical Bugs (BLOCKING)

#### Task: Diagnose Memory Leak

**Issue:** Every build consumes disk space, only recovered after system reboot

**Investigation Steps:**

1. Monitor build directory sizes: `du -sh build-*/`
2. Check for orphaned processes: `ps aux | grep orpheus`
3. Inspect temporary files: `/var/folders/`, `/tmp/`
4. Review CMake cache and artifacts
5. Check for file descriptor leaks (macOS: `lsof | grep orpheus`)

**Files to Examine:**

- Build scripts: `build-launch.sh`, `clean-relaunch.sh`
- CMake configuration: `CMakeLists.txt`
- Build directories: `build-*/`, `orpheus_clip_composer_app_artefacts/`

**Success Criteria:**

- [ ] Leak source identified and documented
- [ ] Disk space consumption eliminated or reduced by >90%
- [ ] No system reboot required after 10 consecutive builds

---

#### Task: Fix CPU Usage at Idle

**Issue:** 77% CPU usage (M2 processor) with no playback active

**Investigation Steps:**

1. Profile with Instruments (macOS Time Profiler)
2. Identify hot loops in idle state
3. Check timer callback frequencies
4. Review repaint() loops in UI components
5. Verify no unnecessary polling

**Likely Culprits:**

- `MainComponent.cpp` - main event loop
- `ClipGrid.cpp` - button polling
- `ClipButton.cpp` - state update loops
- `WaveformDisplay.cpp` - unnecessary repaints

**Files to Examine:**

- `MainComponent.cpp:41-47` (75fps state sync timer)
- `ClipGrid.cpp:149-176` (state sync callback)
- `ClipButton.cpp` (repaint logic)

**Target:** <5% CPU at idle (down from 77%)

**Success Criteria:**

- [ ] CPU usage <5% with no playback
- [ ] Computer no longer warm during idle
- [ ] 75fps state sync only active during playback
- [ ] No UI responsiveness regression

---

### 1.2 SDK Gain & Transport-Fade Completion

#### Task: Complete SDK Gain/Fade Integration

**Context:** SDK team has finished work on Clip Gain and Transport-Fade issues (see ORP097 for related SDK bugs, though those are deferred)

**Reference Documents:**

- **ORP097** - SDK Transport/Fade Bug Fixes (2 bugs identified, deferred to SDK team)
  - Bug 6: Transport Restart During Fade-Out (medium priority)
  - Bug 7: Loop Point Fade Behavior (medium priority)

**Note:** ORP097 bugs are SDK-level issues, deferred to future SDK sprint. This task focuses on completing OCC-level integration of gain/fade features already implemented in v0.2.0.

**Action Items:**

1. Verify gain slider functionality in Clip Edit Dialog
2. Confirm fade IN/OUT time controls work correctly
3. Test fade shape selection (if implemented)
4. Ensure gain/fade settings persist in session save/load
5. Document any remaining edge cases

**Files to Verify:**

- `AudioEngine.cpp` - gain application
- `ClipEditDialog.cpp` - gain/fade controls
- `SessionManager.cpp` - gain/fade persistence

**Success Criteria:**

- [ ] Gain control functional (-∞ to +6 dB range)
- [ ] Fade IN/OUT times apply correctly
- [ ] Settings persist across session save/load
- [ ] No regression in v0.2.0 fade fixes (OCC093)

---

### Sprint 1 Success Metrics

- [ ] All P0 bugs resolved (memory leak, CPU usage)
- [ ] Application usable for extended sessions (>1 hour)
- [ ] No blocking issues for subsequent sprints
- [ ] User approval to proceed to Sprint 2

---

## SPRINT 2: Tab Bar & Status System (P1 - Week 1-2)

**Priority:** High (MVP-critical)
**Est. Duration:** 2-3 days
**Dependencies:** Sprint 1 complete
**Goal:** Clean up UI layout, improve user feedback

### 2.1 Merge Tab Bar with Status Bar

#### Task: Create Unified Bottom Bar

**Layout Design:**

```
┌──────────────────────────────────────────────────────────────────┐
│ [Tab 1] [Tab 2] ... [Tab 8] | ◉ ◉ | [Stop All] [Panic] │
│ ← Tabs (flex space) ─────→ | Status | ← Buttons (fixed) → │
└──────────────────────────────────────────────────────────────────┘
```

**Implementation:**

- Single-row grid layout
- Tabs get flexible space (expand to fill)
- Status indicators vertically stacked between sections
- Panic buttons get minimum fixed width

**Files:**

- `MainComponent.cpp` - layout logic in `resized()`
- New `StatusBar` component (or integrate into `MainComponent`)

**Success Criteria:**

- [ ] Single-row layout implemented
- [ ] Tabs, status, buttons properly spaced
- [ ] Layout responsive to window resizing

---

#### Task: Implement Status Indicators

**Indicator 1: Latency Status Light**

- Uses existing latency color logic (green/yellow/red)
- Shows current audio driver latency status
- Size: Match Panic button height

**Indicator 2: Heartbeat Light**

- Pulses once per second (application health check)
- Green = healthy, Red = audio thread stalled
- Size: Match Panic button height

**Arrangement:**

- Vertically stacked between tabs and buttons
- Height matches Panic button heights exactly
- Minimal width (circular indicators preferred)

**Files:**

- New `StatusIndicator` component
- `MainComponent.cpp` - integrate into layout

**Success Criteria:**

- [ ] Two status lights visible and functional
- [ ] Latency light reflects current driver status
- [ ] Heartbeat pulses at 1 Hz
- [ ] Heights match Panic buttons exactly

---

#### Task: Relocate Verbose Latency Info

**Current:** Latency details displayed in main UI
**Target:** Move to Audio I/O Settings dialogue

**Rationale:**

- Cleaner main UI (less clutter)
- Info still accessible when needed
- Follows professional software UX patterns

**Files:**

- Remove from `MainComponent.cpp`
- Add to audio settings dialog (create if doesn't exist)

**Success Criteria:**

- [ ] Latency details removed from main UI
- [ ] Details accessible in Audio I/O Settings
- [ ] No information loss (all metrics preserved)

---

### 2.2 Tab Management

#### Task: Enable Tab Renaming

**Trigger:** Double-click on tab
**Behavior:**

- Inline text edit appears
- Current name highlighted (ready for typing)
- Save on Enter or focus loss
- ESC to cancel

**Files:**

- Tab component (or create `TabButton` component)
- Session management for persistence

**Success Criteria:**

- [ ] Double-click triggers rename
- [ ] Text edit inline and responsive
- [ ] Names persist in session save/load

---

#### Task: Tab Right-Click Menu

**Menu Options:**

1. **Rename Tab** - Same as double-click
2. **Clear Tab** - Remove all clips from tab (with confirmation)

**Clear Tab Behavior:**

- Show confirmation dialog (see 2.3)
- Clears all 120 clips on tab (10×12 grid)
- Preserves tab name

**Files:**

- Tab component event handlers
- `ClipGrid.cpp` - clear all clips method

**Success Criteria:**

- [ ] Right-click menu appears
- [ ] Rename option functional
- [ ] Clear Tab shows confirmation (no accidental clears)

---

### 2.3 Clear Action Warnings

#### Task: Universal Clear Confirmation

**Applies to:**

- Clear All Clips (all tabs)
- Remove Clip (single clip)
- Clear Tab (all clips on tab)
- Any destructive action

**Dialog Design:**

```
┌─────────────────────────────────────────┐
│ ⚠️ Clear Tab "Music"?                   │
│                                         │
│ This will remove all 120 clips from    │
│ this tab. This action cannot be undone. │
│                                         │
│           [Cancel]  [Clear Tab]         │
└─────────────────────────────────────────┘
```

**Pattern:**

- Reusable `ConfirmationDialog` component
- Clear description of action
- Cancel button (default, ESC key)
- Destructive action button (red, requires explicit click)

**Files:**

- New `ConfirmationDialog` component
- Apply to all clear actions:
  - `MainComponent` - Clear All
  - `ClipGrid` - Clear Tab
  - `ClipButton` - Remove Clip

**Success Criteria:**

- [ ] All clear actions require confirmation
- [ ] Cancel is default action (ESC key)
- [ ] No accidental data loss possible

---

### Sprint 2 Success Metrics

- [ ] Unified bottom bar implemented
- [ ] Status indicators functional
- [ ] Tab management complete (rename, clear)
- [ ] Universal confirmation dialogs prevent accidental data loss
- [ ] User approval to proceed

---

## SPRINT 3: Clip Button Visual System (P1 - Week 2)

**Priority:** High (MVP-critical)
**Est. Duration:** 3-4 days
**Dependencies:** Sprint 2 optional
**Goal:** Consistent indicators, time display, corner elements

### 3.1 Corner Indicator System

#### Task: Standardize Corner Indicator Dimensions

**Baseline:** Use PLAY icon (bottom-left, green) as size reference

**Requirements:**

- **Guarantee:** 3-character width at all times (handles 100+ clips)
- **Apply to:** All four corners (clip number, hotkey, play icon, group)
- **Reference:** Bottom-left PLAY icon already looks good

**Files:**

- `ClipButton.cpp:drawClipHUD()` - main HUD rendering
- `ClipButton.cpp:drawStatusIcons()` - icon grid

**Success Criteria:**

- [ ] All corner indicators exactly 3-char width
- [ ] Dimensions consistent across all button states
- [ ] Scales properly with button size

---

#### Task: Top-Left Clip Number

**Style:**

- **Color:** White background (default)
- **Dark Mode Logic:** Flip to dark (black @0.95) when clip color is near-white
- **Threshold:** Extreme (only brightest clip colors trigger dark mode)
- **Width:** 3-char guarantee

**Files:**

- `ClipButton.cpp` - clip number rendering
- Color contrast calculation utility

**Success Criteria:**

- [ ] Clip number always readable
- [ ] Dark mode triggers only for near-white clip colors
- [ ] 3-char width maintained

---

#### Task: Top-Right Hotkey Indicator

**Style:**

- **Outline:** Thin stroke, no background fill
- **Color:** Uses clip button text color (light/dark adaptive)
- **Dimensions:** Match other corner indicators (3-char width)

**Format:**

- Single key: `F1`, `F2`, etc.
- Modifier combo: `⌘1`, `⌥F`, etc.

**Files:**

- `ClipButton.cpp` - add hotkey corner indicator
- Hotkey assignment system (if not yet implemented)

**Success Criteria:**

- [ ] Hotkey displayed in top-right corner
- [ ] Thin outline style (no background)
- [ ] Color matches clip button text
- [ ] 3-char width guarantee

---

#### Task: Bottom-Right Group Indicator

**Current State:** Group indicator exists but may be misaligned

**Requirements:**

- **Resize:** Match 3-char width of other indicators
- **Corner Placement:** Align with other corners (consistent padding)
- **Style:** Solid background with group color

**Files:**

- `ClipButton.cpp` - group badge rendering

**Success Criteria:**

- [ ] Group indicator matches 3-char width
- [ ] Corner alignment consistent
- [ ] Group color clearly visible

---

#### Task: Corner Radii Matching

**Goal:** Visual cohesion across all UI elements

**Apply to:**

- All corner indicators (4 corners)
- "Active" border (clip currently playing)
- Clip button itself (background)

**Ensure:**

- Inner and outer radii consistent
- All corner indicators use same radius
- Active border radius matches button radius

**Files:**

- `ClipButton.cpp` - all radius constants
- Consider centralizing radius values in constants file

**Success Criteria:**

- [ ] All corner radii visually match
- [ ] Inner/outer radii consistent
- [ ] No misaligned or jarring corners

---

### 3.2 Beat Indicator

#### Task: Add Beat Display

**Position:** To the right of Top-Left Clip Number

**Format:** `  // {value}`
**Example:** `05  // 3+` (clip 05, assigned to beat 3+)

**Style:**

- **Color:** Same as Clip Number background (white or black @0.95)
- **Typeface:** Thin, minimal, legible (not dominating)
- **Spacing:** Clear separation from clip number

**Files:**

- `ClipButton.cpp` - add beat indicator rendering
- `ClipButton.h` - add `m_beatOffset` member (if missing)
- `ClipEditDialog.cpp` - beat assignment dropdown (see Sprint 4)

**Success Criteria:**

- [ ] Beat value visible when assigned
- [ ] Clear separation from clip number
- [ ] Thin, minimal visual style
- [ ] Blank when no beat assigned

---

### 3.3 Text Layout Grid

#### Task: Implement 5-Row Text System

**Current State:** Basic layout exists, needs enforcement

**Row Structure:**

```
┌────────────────────────────────────────┐
│ [Clip #] [Beat] [Hotkey]              │ Row 1: Fixed height
│ Clip Name (word wrap)                 │ Row 2: Flex height (bounded)
│ USER COMMENT (TIGHT/CONDENSED)        │ Row 3: Fixed height, tight
│ 00:15.00 — 03:30.00                   │ Row 4: Fixed height, large
│ [PLAY] [Icons...] [GRP]               │ Row 5: Fixed height
└────────────────────────────────────────┘
```

**Row Details:**

**Row 1:** Single line, fixed height

- Left: Clip Number + Beat
- Right: Hotkey
- Font: Regular weight

**Row 2:** Word wrap, flex height (within bounds)

- Clip Name (user-editable)
- Font: Regular weight
- Max lines: 2-3 (depending on button size)

**Row 3:** Single line, fixed height

- User comment (optional)
- Font: Tight/condensed, possibly all caps
- Max length: Truncate with ellipsis

**Row 4:** Single line, fixed height (large font)

- Time display (see 3.4)
- Font: 2× current size

**Row 5:** Single line, fixed height

- Left: PLAY icon
- Center: Status icons (grid)
- Right: Group indicator

**Constraint:** Width always > height (landscape clip buttons)

**Files:**

- `ClipButton.cpp:drawClipHUD()` - layout logic

**Success Criteria:**

- [ ] Clear 5-row structure visible
- [ ] Text adapts to button size
- [ ] No text overflow or collision
- [ ] Word wrap works correctly in Row 2

---

### 3.4 Enhanced Time Display

#### Task: Larger, Dual-Field Time Display

**Size:** Approximately 2× current size (Row 4, prominent)

**Format:** `[elapsed] — [remaining]` (em dash with spaces)

**Examples:**

- Playing: `00:00:15.00 — 00:03:15.00` (15s elapsed, 3m15s remaining)
- Inactive: `00:00:00.00 — 00:03:30.00` (shows total duration)

**Style:**

- **Color:** Clip button text color (light/dark adaptive)
- **Font:** Larger than current (approximately 2× size)
- **Alignment:** Centered in Row 4

**Files:**

- `ClipButton.cpp` - time rendering in Row 4
- `ClipButton.cpp` - time format calculation

**Success Criteria:**

- [ ] Time prominently displayed (2× current size)
- [ ] Elapsed and remaining clearly separated
- [ ] Easy to read during live performance
- [ ] Updates in real-time during playback

---

### 3.5 Icon Grid Spacing

#### Task: Tighten Indicator Icon Grid

**Current State:** Status icons in Row 5 center (LOOP, STOP OTHERS, etc.)

**Adjustments:**

- **Horizontal spacing:** Slightly tighter (reduce gaps)
- **Vertical alignment:** All icons centered with PLAY and GROUP
- **Row height:** Match bottom corner indicators (PLAY, GROUP)

**Files:**

- `ClipButton.cpp:drawStatusIcons()` - icon grid layout

**Success Criteria:**

- [ ] Compact, aligned icon grid
- [ ] All icons vertically centered in Row 5
- [ ] No visual crowding or overlap

---

### 3.6 Clip Group Abbreviations

#### Task: Implement Automatic Abbreviation Logic

**Algorithm:** Generate <3-char abbreviations from user labels (not hard-coded)

**Examples:**

- 'Group 3' → 'G3'
- 'Music' → 'MUS'
- 'Soccer' → 'SOC'
- 'Effects' → 'FX'
- 'SFX' → 'SFX' (already short)
- 'Background Ambience' → 'BGA'

**Rules:**

1. If name ≤3 chars, use as-is
2. If "Group N" pattern, use "GN"
3. If single word, take first 3 chars (uppercase)
4. If multiple words, take first letter of each word (up to 3)
5. Common abbreviations (SFX, BGM, VOX) preserved

**Files:**

- New utility function: `abbreviateGroupName(string name)`
- Apply in `ClipButton.cpp` - group indicator rendering

**Success Criteria:**

- [ ] Abbreviation algorithm works with arbitrary names
- [ ] All abbreviations ≤3 characters
- [ ] Common patterns recognized (Group N, SFX, etc.)
- [ ] User-friendly output (intuitive abbreviations)

---

### Sprint 3 Success Metrics

- [ ] All corner indicators standardized (3-char width)
- [ ] 5-row text layout implemented
- [ ] Enhanced time display prominent and readable
- [ ] Beat indicator functional
- [ ] Group abbreviations automatic and intuitive
- [ ] Visual cohesion across all clip buttons

---

## SPRINT 4: Clip Edit Dialog Refinement (P1 - Week 2-3)

**Priority:** High (MVP-critical)
**Est. Duration:** 4-5 days
**Dependencies:** Sprint 1 complete
**Goal:** Live preview, validation, keyboard workflow

### 4.1 CRITICAL: Live Preview with Commit/Cancel

#### Task: Implement Live Edit Preview

**Behavior:**

- **While dialog open:** All edits audible/visible in real-time
- **Commit (OK button):** Apply changes permanently
- **Cancel (ESC or Cancel button):** Discard ALL changes, restore exact pre-edit state

**Challenge:** Store original state, apply temporary changes, revert or commit

**Implementation:**

1. On dialog open, deep-copy current clip state to `m_originalState`
2. All edits modify working state (live preview)
3. On OK: commit working state to `AudioEngine`
4. On Cancel: restore `m_originalState`, discard working state

**State to Backup:**

- Clip name, color, group assignment
- Trim IN/OUT points
- Fade IN/OUT times and shapes
- Gain level
- Loop mode, Stop Others mode
- Hotkey assignment
- User comment

**Files:**

- `ClipEditDialog.cpp` - add state backup/restore logic
- `ClipEditDialog.h` - add `m_originalState` member

**Success Criteria:**

- [ ] User hears/sees changes live while editing
- [ ] OK button commits all changes permanently
- [ ] Cancel button truly reverts (no residual changes)
- [ ] No audio glitches during live preview

---

### 4.2 Text Input Improvements

#### Task: Auto-Select Clip Name on Tab

**Trigger:** TAB key to Clip Name field
**Behavior:** All text highlighted, ready for typing (no need to Cmd+A)

**Files:**

- `ClipEditDialog.cpp` - Clip Name field focus handler

**Success Criteria:**

- [ ] Text auto-selected on Tab focus
- [ ] Works consistently (doesn't require multiple tabs)

---

#### Task: Fade Time Numerical-Only Input

**Fields:** Fade IN, Fade OUT time inputs

**Current Issue:** Accepts all keyboard chars, interferes with Trim shortcuts

**Required:**

- **Accept:** Numerical input only (0-9, decimal point)
- **Format:** Automatically format to `HH:MM:SS.FF`
- **Reject:** Letter keys (especially `[` `]` `;` `'` used for trim shortcuts)

**Files:**

- `ClipEditDialog.cpp` - fade time input validation
- Consider custom `TimeInputField` component

**Success Criteria:**

- [ ] Only numerical input accepted
- [ ] Proper time formatting applied
- [ ] Trim shortcuts work when fade fields have focus
- [ ] No interference between text input and shortcuts

---

### 4.3 Keyboard Shortcuts

#### Task: Implement Loop Toggle ('?')

**Shortcut:** '?' (Shift+'/') toggles LOOP mode

**Current Status:** Non-functional (key handler exists but doesn't work)

**Files:**

- `ClipEditDialog.cpp` - key handler for '?'

**Success Criteria:**

- [ ] '?' key toggles loop mode
- [ ] Visual feedback (button state changes)
- [ ] Works consistently regardless of focus

---

#### Task: Add NO KEYBOARD FOCUS Tab Stop

**Tab Sequence:**

1. Clip Name → (text input active)
2. Fade IN → (text input active)
3. Fade OUT → (text input active)
4. NO FOCUS → (all text inputs inactive)

**Purpose:** Return keyboard to inactive state so shortcuts work

**Implementation:**

- Add invisible focus target after last text input
- Captures Tab key after Fade OUT
- Doesn't accept text input

**Files:**

- `ClipEditDialog.cpp` - tab focus management

**Success Criteria:**

- [ ] 4th tab press deactivates all text inputs
- [ ] Keyboard shortcuts work in NO FOCUS state
- [ ] Visual feedback (no text input cursor visible)

---

#### Task: Implement ESC Text Field Escape

**Current:** ESC closes dialog (immediate exit)

**Required:** Two-level ESC behavior

1. **First ESC:** Exit current text input field (return to NO FOCUS)
2. **Second ESC:** Close Clip Edit dialog (existing behavior)

**Files:**

- `ClipEditDialog.cpp` - ESC key handler

**Success Criteria:**

- [ ] First ESC exits text field (doesn't close dialog)
- [ ] Second ESC closes dialog
- [ ] Works consistently for all text inputs

---

### 4.4 Control Redesigns

#### Task: Convert Gain Slider to Dropdown

**Current:** Slider control for gain
**Target:** Dropdown to match Fade time controls

**Style:** Match Fade IN/OUT dropdown appearance

**Format:** `Gain: { 0.0 dB }`

**Position:** Above Fade IN section (where 'Gain:' label currently is)

**Range:** -∞ to +6 dB (standard)

**Note:** Mind vertical spacing during rebuild (don't create collisions)

**Files:**

- `ClipEditDialog.cpp` - replace slider with dropdown

**Success Criteria:**

- [ ] Gain dropdown matches fade dropdown style
- [ ] Full range accessible (-∞ to +6 dB)
- [ ] No layout collisions or spacing issues

---

#### Task: Add Beat Dropdown

**Position:** To right of GAIN dropdown, above Trim OUT section

**Label:** 'Beat:'

**Options:**

- Default: Blank (unassigned)
- Standard: `1, 1+, 2, 2+, 3, 3+, 4, 4+`
- Easter Egg: Option+click = `1, 1++, 2, 2++, 3, 3++, 4, 4++`

**Purpose:** Assign clip to beat offset for musical timing (future feature hook)

**Files:**

- `ClipEditDialog.cpp` - new beat dropdown
- `ClipButton.cpp` - display beat value (see Sprint 3.2)

**Success Criteria:**

- [ ] Beat dropdown functional
- [ ] Values displayed in clip button (Sprint 3.2)
- [ ] Easter egg Option+click works
- [ ] Blank default (unassigned)

---

#### Task: Redesign Transport Icons

**Current Issue:** Transport control icons are ugly

**Action:**

- Create new, professional transport control icons
- Match pro audio software aesthetic (Pro Tools, Logic, Reaper)
- Icons: Play, Stop, Loop, Jog Forward, Jog Backward

**Files:**

- Icon assets (SVG or PNG)
- `ClipEditDialog.cpp` - icon rendering

**Success Criteria:**

- [ ] New icons professional and polished
- [ ] Consistent visual style
- [ ] User approval on aesthetics

---

### 4.5 Layout Fixes

#### Task: Mirror Trim IN/OUT Layouts

**Current Issue:** Trim OUT buttons misaligned with Trim IN

**Required Layout (Horizontally Mirrored):**

```
Trim IN:  [SET] [GRID] [TIME_FIELD] [GRID] [<] [GRID] [>] [GRID] [CLR aligned with SET]
Trim OUT: [SET] [GRID] [TIME_FIELD] [GRID] [<] [GRID] [>] [GRID] [CLR aligned with SET]
```

**Note:** [GRID] = spacing/grid column, [CLR] = Clear button

**Files:**

- `ClipEditDialog.cpp:resized()` - layout logic

**Success Criteria:**

- [ ] IN and OUT sections mirror each other horizontally
- [ ] Clear buttons aligned with SET buttons
- [ ] Consistent spacing throughout

---

#### Task: Make OK/CANCEL Taller

**Current:** OK/CANCEL buttons may be too small

**Required:**

- **Increase height:** 50% taller
- **Add bottom margin:** Add spacing above buttons (below COLOUR/GROUP row)

**Files:**

- `ClipEditDialog.cpp` - button sizing

**Success Criteria:**

- [ ] Buttons 50% taller (more prominent)
- [ ] Clear spacing below COLOUR/GROUP row
- [ ] Better visual hierarchy (buttons stand out)

---

### 4.6 Trim Playhead Synchronization

#### Task: Fix Cmd+Click Trim Point Updates

**Current Issue:**

- Cmd+Click (set IN) and Cmd+Shift+Click (set OUT) don't update playhead
- Buttons and keyboard shortcuts work correctly
- Waveform clicks don't update playhead

**Required:** Playhead respects IN/OUT set by waveform clicks

**Also Verify:** Text input to Trim fields updates playhead

**Files:**

- `ClipEditDialog.cpp` - waveform click handlers
- `ClipEditDialog.cpp` - trim point edit laws (from OCC093)

**Success Criteria:**

- [ ] Cmd+Click (set IN) updates playhead, restarts
- [ ] Cmd+Shift+Click (set OUT) enforces edit law, updates playhead
- [ ] Text input to trim fields updates playhead
- [ ] All trim input methods consistent

---

### 4.7 Waveform Performance

#### Task: Improve Waveform Load Speed

**Current Issue:** Slow loading when dialog opens

**Action:**

1. Profile waveform generation (identify bottleneck)
2. Optimize rendering algorithm
3. Consider caching waveform data
4. Refer to `.claude/agents/occ/waveform-optimization.md` (if exists)

**Files:**

- `WaveformDisplay.cpp` - waveform generation and rendering

**Target:** <100ms generation time (from file open to display)

**Success Criteria:**

- [ ] Dialog opens quickly (<100ms waveform ready)
- [ ] No visible lag when opening Edit Dialog
- [ ] No regression in waveform visual quality

---

### Sprint 4 Success Metrics

- [ ] Live preview functional with commit/cancel
- [ ] Text input improved (auto-select, numerical-only)
- [ ] Keyboard shortcuts complete (loop toggle, ESC behavior, tab sequence)
- [ ] Control redesigns complete (gain dropdown, beat dropdown, transport icons)
- [ ] Layout fixes applied (mirrored trim sections, taller OK/CANCEL)
- [ ] Trim playhead synchronization fixed
- [ ] Waveform performance improved (<100ms load)

---

## SPRINT 5: Session Management & Clip Groups (P1 - Week 3)

**Priority:** High (MVP-critical)
**Est. Duration:** 3-4 days
**Dependencies:** Sprint 4.1 (live preview) desirable
**Goal:** Backend systems, group management

### 5.1 Clip Group System

#### Task: Make Groups Session-Wide

**Current:** Groups are per-clip (each clip stores its own group)

**Required:** Session-wide group list, editable in any Clip Edit dialogue

**Design:**

- **Session maintains:** List of groups (name, color, routing)
- **Clips reference:** Group by ID or index
- **Default groups:** 'Group 1', 'Group 2', 'Group 3', 'Group 4' (user-editable names)

**UI Access:**

- **Clip Edit Dialog:** Dropdown shows all session groups
- **Fallback (if dropdown editing not feasible):** Add group editor to Session Preferences (Sprint 8)

**Visual:**

- Paint list item backgrounds in Clip Group's color
- Show group abbreviation (see Sprint 3.6)

**Files:**

- `SessionManager` - add group list management
- `ClipEditDialog.cpp` - group dropdown
- Session JSON schema - add groups array
- Preferences system (if needed)

**Success Criteria:**

- [ ] Groups managed at session level
- [ ] All clips can be assigned to any group
- [ ] Group names editable (via dialog or preferences)
- [ ] Group list persists in session save/load
- [ ] Dropdown shows all groups with colors

---

### 5.2 Clip Grid Resizing

#### Task: Implement Dynamic Grid Dimensions

**Current:** Fixed 10×12 grid (120 clips per tab)

**Required:** User-configurable grid dimensions

**Constraint:** Width always > height (landscape buttons)

**Range:** Determine min/max grid sizes

- **Minimum:** 5×4 = 20 clips per tab (?)
- **Maximum:** 12×8 = 96 clips per tab (?)
- **Default:** 10×12 = 120 clips per tab

**Behavior:**

- Buttons auto-resize based on grid dimensions
- Maintain width > height constraint
- Preserve clip assignments when resizing

**Storage:** Save as session preference

**UI Access:** Session Preferences tab (Sprint 8)

**Files:**

- `ClipGrid.cpp` - dynamic grid sizing
- `SessionManager` - grid size persistence
- Preferences system

**Success Criteria:**

- [ ] Grid dimensions user-configurable
- [ ] Buttons auto-resize correctly
- [ ] Width > height constraint maintained
- [ ] Grid size saved with session
- [ ] Clip assignments preserved during resize

---

### 5.3 Clip Operations

#### Task: Implement Copy/Paste

**Full Copy:**

- Copy entire clip (all metadata + audio reference)
- Paste to new button location
- Includes empty spaces (can copy to any button)

**Paste Special:**

- Copy only clip states (Color, Stop Others, Loop, Fades, Gain, Group, etc.)
- Does NOT copy audio file or clip name
- Applies states to existing clip

**Access:**

- Full Copy: Cmd+C / Cmd+V
- Paste Special: Right-click menu → "Paste Special"

**Files:**

- `ClipButton` - copy/paste handlers
- `ClipGrid` - clipboard management
- `SessionManager` - clip duplication logic

**Success Criteria:**

- [ ] Cmd+C copies clip (full copy)
- [ ] Cmd+V pastes to selected button
- [ ] Paste Special applies states only
- [ ] Copy/paste works across tabs

---

#### Task: Add Ctrl+Click for Right-Click Menu

**Purpose:** Right-clicking risky in live scenarios (might stop clip accidentally)

**Behavior:** Ctrl+Click opens right-click menu (alternative to right-click)

**Preserve:** Existing Ctrl+Opt+Cmd+Click for direct Clip Edit (if exists)

**Files:**

- `ClipButton.cpp:mouseDown()` - click handler

**Success Criteria:**

- [ ] Ctrl+Click opens right-click menu
- [ ] Existing modifier combos preserved
- [ ] Safer workflow for live use

---

### Sprint 5 Success Metrics

- [ ] Groups session-wide and editable
- [ ] Grid dimensions configurable
- [ ] Copy/paste functional (full and paste special)
- [ ] Ctrl+Click menu alternative implemented
- [ ] All features persist in session save/load

---

## SPRINT 6: Keyboard Navigation & Playbox (P2 - Week 3-4)

**Priority:** Medium (polish)
**Est. Duration:** 2-3 days
**Dependencies:** Sprint 3 (visual system) helpful
**Goal:** Keyboard-first workflow

### 6.1 Playbox Navigation System

#### Task: Implement Playbox Outline

**Visual:** Thin white outline on very outside perimeter of last-launched clip

**Persistence:** Remains visible regardless of playback state

- Different from "Active" border (only shown during playback)
- Playbox tracks last-fired clip (even if stopped)

**Files:**

- `ClipButton.cpp` - playbox outline rendering
- `ClipGrid.cpp` - playbox tracking

**Success Criteria:**

- [ ] Outline tracks last-fired clip
- [ ] Visible in all states (playing, stopped)
- [ ] Thin white outline (distinct from Active border)

---

#### Task: Arrow Key Navigation

**Keys:** Up, Down, Left, Right move playbox

**Behavior:**

- **Tap:** Move one button
- **Hold:** Latch and accelerate (same logic as Trim `< >` buttons in Edit Dialog)

**Files:**

- `ClipGrid.cpp` - key event handling
- Arrow key acceleration logic (reuse from Trim buttons if possible)

**Success Criteria:**

- [ ] Arrow keys navigate grid
- [ ] Hold accelerates movement
- [ ] Wraps at edges (or stops at boundary - TBD)

---

#### Task: Enter Key Playback

**Behavior:** Enter key = PLAY/STOP for selected clip (clip with playbox)

**Toggle:**

- If clip stopped → start
- If clip playing → stop

**Benefit:** Keyboard-only clip triggering (no mouse required)

**Files:**

- `ClipGrid.cpp` - Enter key handler

**Success Criteria:**

- [ ] Enter fires selected clip (toggles play/stop)
- [ ] Works consistently with arrow key navigation

---

### 6.2 Zoom Controls (Clip Edit Only)

#### Task: Implement R/T Zoom

**Keys:**

- **R:** Zoom out
- **T:** Zoom in

**Standard:** Pro Tools convention (industry standard)

**Active:** Only when Clip Edit dialog open

**Replaces:** Failed attempts with `/−` and `/=` keys

**Uses:** Existing zoom levels (if zoom already implemented)

**Files:**

- `ClipEditDialog.cpp` - key handler for R/T

**Success Criteria:**

- [ ] R zooms out, T zooms in
- [ ] Only active in Clip Edit dialog
- [ ] Uses Pro Tools convention

---

#### Task: Guarantee 1× Zoom Shows Full File

**Requirement:** At 1× zoom, ENTIRE audio file always visible (no horizontal scrolling)

**Files:**

- `WaveformDisplay.cpp` - zoom calculation

**Success Criteria:**

- [ ] 1× zoom shows complete file (start to end)
- [ ] No horizontal scrolling at 1×
- [ ] Higher zoom levels show detail

---

### Sprint 6 Success Metrics

- [ ] Playbox navigation functional (arrow keys, Enter)
- [ ] R/T zoom implemented (Clip Edit only)
- [ ] 1× zoom shows full file
- [ ] Keyboard-first workflow complete

---

## SPRINT 7: Typography & Application Polish (P2 - Week 4)

**Priority:** Medium (polish)
**Est. Duration:** 2-3 days
**Dependencies:** All previous sprints for complete effect
**Goal:** Visual cohesion, branding

### 7.1 Font Migration

#### Task: Replace Inter with HK Grotesk

**Location:** `/Users/chrislyons/dev/orpheus-sdk/apps/clip-composer/Resources/HKGrotesk_3003`

**Scope:** ALL fonts, including JUCE defaults

**Goal:** Eliminate inherited Reaper aesthetic (Inter is Reaper's font)

**Files:**

- All UI components
- Font loading in `Main.cpp` or `MainComponent.cpp`
- JUCE LookAndFeel overrides

**Success Criteria:**

- [ ] HK Grotesk loaded successfully
- [ ] All UI text uses HK Grotesk
- [ ] No Inter remnants
- [ ] Font rendering clean on all platforms

---

### 7.2 Spelling & Branding

#### Task: Canadian Spelling

**Change:** Color → Colour throughout codebase

**Scope:** All source files, UI text, documentation

**Files:** All source files containing "color"

**Success Criteria:**

- [ ] Canadian spelling consistent throughout
- [ ] No "color" remnants in user-facing text
- [ ] Code comments updated

---

#### Task: Update Application Name

**Current:** "Orpheus Clip Composer"
**Target:** "Clip Composer"

**Rationale:** Orpheus is the SDK, not the application

**Update:**

- Header bar (main window title)
- About dialog
- Build configuration (app bundle name)
- Documentation references (where appropriate)

**Files:**

- `MainComponent.cpp` - window title
- About dialog component
- CMakeLists.txt / build configuration

**Success Criteria:**

- [ ] App called "Clip Composer" everywhere
- [ ] No "Orpheus Clip Composer" in UI
- [ ] Build artifacts reflect new name

---

#### Task: Add Application Icon

**Purpose:** Runtime testing identification (easier to spot in dock/taskbar)

**Action:**

- Create or source simple icon
- Add to application bundle

**Files:**

- Resources directory
- App bundle configuration (macOS: Info.plist, Windows: .rc file)

**Success Criteria:**

- [ ] Icon appears in dock/taskbar
- [ ] Icon visible in application switcher
- [ ] Professional appearance

---

### Sprint 7 Success Metrics

- [ ] HK Grotesk font throughout application
- [ ] Canadian spelling consistent
- [ ] Application name updated to "Clip Composer"
- [ ] Application icon added
- [ ] Professional, polished appearance

---

## SPRINT 8: Preferences System (P2 - Week 4-5)

**Priority:** Medium (MVP-critical for configurability)
**Est. Duration:** 3-4 days
**Dependencies:** Sprint 5.1 (groups), 5.2 (grid sizing)
**Goal:** User customization

### 8.1 Preferences Dialog Structure

#### Task: Create Unified Preferences Dialog

**Trigger:** Cmd+, to open, ESC to close

**Tabs:** 3 tabs total

1. Application (app-wide settings)
2. Session (per-session settings)
3. Audio I/O (audio driver settings)

**Pattern:** Single dialog, clearly distinct tab areas

**Files:**

- New `PreferencesDialog` component
- Tab navigation system

**Success Criteria:**

- [ ] Cmd+, opens Preferences
- [ ] ESC closes Preferences
- [ ] 3 tabs clearly distinct
- [ ] Tab switching functional

---

### 8.2 Application Preferences Tab

#### Task: Implement Application Settings

**Options:**

- Default session directory
- Tab bar placement: top, bottom, left, right
- (More planned - leave extensible)

**Storage:** Application-level settings (persists across all sessions)

**Files:**

- `PreferencesDialog` - Application tab UI
- Application settings storage (user defaults or config file)

**Success Criteria:**

- [ ] Default session directory configurable
- [ ] Tab bar placement options functional
- [ ] Settings persist across app launches

---

### 8.3 Session Preferences Tab

#### Task: Implement Session Settings

**Options:**

- Clip grid dimensions (see Sprint 5.2)
- Clip Group list editor (see Sprint 5.1)
- Default Fade IN/OUT times and shapes
- Default clip color
- (More planned - leave extensible)

**Storage:** Per-session settings (saved in session file)

**Files:**

- `PreferencesDialog` - Session tab UI
- Session settings storage (session JSON)

**Success Criteria:**

- [ ] Grid dimensions configurable
- [ ] Group list editable
- [ ] Default fade times/shapes settable
- [ ] Default clip color settable
- [ ] Settings saved with session

---

### 8.4 Audio I/O Preferences Tab

#### Task: Migrate Audio I/O Settings

**Source:** Current Audio I/O Settings dialogue (if exists)

**Destination:** Audio I/O tab in Preferences dialog

**Replaces:** Audio I/O menu item in Audio menu (move to Preferences)

**Content:** Everything currently in Audio I/O dialogue

- Audio device selection
- Sample rate
- Buffer size
- Input/output channel mapping
- Latency display (verbose, moved from main UI in Sprint 2)

**Files:**

- Move existing Audio I/O dialogue into Preferences tabs
- Remove standalone Audio I/O menu item

**Success Criteria:**

- [ ] All audio settings in Preferences dialog
- [ ] Audio I/O menu item removed (or redirects to Preferences)
- [ ] No functionality lost

---

### Sprint 8 Success Metrics

- [ ] Preferences dialog functional (Cmd+,)
- [ ] All 3 tabs implemented
- [ ] Application settings persist app-wide
- [ ] Session settings persist per-session
- [ ] Audio I/O settings migrated successfully

---

## SPRINT 9: Audio Output Routing (P2 - Week 5)

**Priority:** Medium (MVP feature)
**Est. Duration:** 1-2 days
**Dependencies:** SDK routing matrix API available
**Goal:** Flexible audio output

### 9.1 Audio Output Selection

#### Task: Implement Multi-Output Support

**Current:** System default audio output only

**Required:** Any connected audio output device selectable

**Integration:** Simple routing via Orpheus Routing Matrix API

- SDK provides routing matrix
- OCC provides UI for device selection

**Files:**

- `AudioEngine.cpp` - device enumeration and selection
- Audio settings UI (in Preferences, Sprint 8)

**Success Criteria:**

- [ ] User can select any connected audio output
- [ ] Device list refreshes when devices added/removed
- [ ] Selected device persists in session

---

### Sprint 9 Success Metrics

- [ ] Multi-output support functional
- [ ] Device selection UI in Preferences
- [ ] Audio routing works correctly

---

## SPRINT 10: Session Backend Architecture (P1 - Week 5-6)

**Priority:** High (MVP-critical for session management)
**Est. Duration:** 4-5 days
**Dependencies:** Sprint 5 (group system), Sprint 8 (preferences)
**Goal:** Robust session management

**Reference:** SpotOn Manual for implementation patterns (user mention)

### 10.1 Save Format Implementation

#### Task: Regular Session Files

**Format:** JSON referencing assets folder

**Current State:** Basic save/load exists (v0.2.0)

**Enhancement:** Ensure all new features saved

- Groups (Sprint 5)
- Preferences (Sprint 8)
- Grid size (Sprint 5)
- Beat assignments (Sprint 4)
- Hotkeys (Sprint 3)

**Files:**

- `SessionManager.cpp` - session save/load
- Session JSON schema documentation

**Success Criteria:**

- [ ] All features persist in session save/load
- [ ] No data loss
- [ ] Session loads correctly in fresh application instance

---

#### Task: Blocks (Type A - Reference Only)

**Content:** Clip configurations, references assets folder

**Purpose:** Share clip arrangements without audio files

- Useful for templates
- Smaller file size
- Audio files must exist in target session

**Format:** JSON subset of session file (clips only)

**Files:**

- `SessionManager` - block export/import
- New block format documentation

**Success Criteria:**

- [ ] Can export clip blocks (no audio)
- [ ] Can import blocks into existing session
- [ ] References resolve correctly

---

#### Task: Blocks (Type B - Include Audio)

**Content:** Clip configurations + audio assets

**Purpose:** Self-contained clip arrangements

- Share complete arrangements
- Portable (includes audio)
- Larger file size

**Format:** JSON + bundled audio files (ZIP or folder)

**Files:**

- `SessionManager` - block bundling/extraction
- Asset packaging system

**Success Criteria:**

- [ ] Can export clip blocks with audio
- [ ] Can import blocks with audio
- [ ] Self-contained and portable

---

#### Task: Packed Sessions

**Content:** Full session + all audio assets

**Purpose:** Archive or share complete projects

- Complete session backup
- Share entire project with collaborators
- Portable

**Format:** ZIP archive (session JSON + audio folder)

**Files:**

- `SessionManager` - session packaging
- ZIP compression integration

**Success Criteria:**

- [ ] Can create packed session archive
- [ ] Can open packed session
- [ ] All assets included and functional

---

### 10.2 Asset Management

#### Task: Asset Copy on Import

**Behavior:** When clip loaded, copy audio to `/audio` folder in session directory

**Session Load:** Look for assets in session `/audio` folder first, fall back to original paths

**Benefits:**

- Portable sessions (all assets local)
- No broken file references
- Easy project archival

**Files:**

- `SessionManager` - asset copy logic
- File operations (copy, verify)

**Success Criteria:**

- [ ] Audio files copied to session `/audio` folder on import
- [ ] Session loads from local `/audio` folder
- [ ] Fall back to original paths if local copy missing

---

### 10.3 Additional Backend Features

#### Task: Implement Clip Lists

**Purpose:** TBD (refer to SpotOn Manual - user mention)

**Possible Use Cases:**

- Playlist of clips for sequential playback
- Cue sheet for show/broadcast
- Template clip collections

**Files:**

- `SessionManager` - clip list management

**Success Criteria:**

- [ ] Clip list functionality operational
- [ ] (TBD based on SpotOn Manual reference)

---

#### Task: Implement Session Logs

**Purpose:** Track session activity, changes, events

**Use Cases:**

- Audit trail (what clips played when)
- Debugging (track errors, warnings)
- Analytics (usage patterns)

**Format:** JSON or CSV log file

**Files:**

- `SessionManager` - logging system
- Log file writer

**Success Criteria:**

- [ ] Session logging active
- [ ] Events logged (clip play, edit, save, etc.)
- [ ] Log file readable and useful

---

### Sprint 10 Success Metrics

- [ ] Regular session files enhanced (all features persist)
- [ ] Blocks (Type A & B) functional
- [ ] Packed sessions functional
- [ ] Asset management robust (copy on import)
- [ ] Clip lists implemented
- [ ] Session logs functional

---

## SPRINT 11: Standard macOS Behaviors (P2 - Week 6)

**Priority:** Medium (polish)
**Est. Duration:** 1 day
**Dependencies:** None (can run anytime)
**Goal:** Native OS integration

### 11.1 Standard Key Commands

#### Task: Verify/Implement Standard Commands

**Commands:**

- **Cmd+Q:** Quit
- **Cmd+S:** Save
- **Cmd+Shift+S:** Save As
- **Cmd+Shift+F:** Fullscreen
- **Cmd+,:** Preferences (already in Sprint 8)

**Verify:** Check if already implemented, add if missing

**Files:**

- Main menu configuration
- Key handlers in `MainComponent` or application root

**Success Criteria:**

- [ ] All standard commands functional
- [ ] Commands work as expected (macOS conventions)
- [ ] Shortcuts displayed in menus

---

### Sprint 11 Success Metrics

- [ ] All standard macOS key commands functional
- [ ] Application feels native to macOS

---

## SPRINT 12: Build System & Development Tools (P2 - Week 6)

**Priority:** Medium (developer experience)
**Est. Duration:** 1 day
**Dependencies:** All features complete
**Goal:** Efficient development workflow

### 12.1 Build Scripts

#### Task: Verify build-launch.sh

**Location:** `/Users/chrislyons/dev/orpheus-sdk/apps/clip-composer/build-launch.sh`

**Status:** ✅ Verified to exist (2.5K, Oct 31 00:39)

**Action:**

- Review script for reliability
- Ensure quick build-test cycle
- Document usage if needed

**Success Criteria:**

- [ ] Script functional and reliable
- [ ] Quick rebuild and launch (<30 seconds)
- [ ] Usage documented (if complex)

---

#### Task: Verify clean-relaunch.sh

**Location:** `/Users/chrislyons/dev/orpheus-sdk/apps/clip-composer/clean-relaunch.sh`

**Status:** ✅ Verified to exist (1.3K, Oct 31 00:39)

**Action:**

- Review script for reliability
- Ensure clean rebuild works
- Document when to use (vs. build-launch.sh)

**Success Criteria:**

- [ ] Script functional and reliable
- [ ] Clean slate builds work correctly
- [ ] Usage documented

---

### Sprint 12 Success Metrics

- [ ] Build scripts verified and documented
- [ ] Efficient developer workflow established

---

## DEFERRED: SDK Bugs (Low Priority)

**Reference:** ORP097 - SDK Transport/Fade Bug Fixes for OCC v0.2.0

These are SDK-level bugs identified during v0.2.0 development, assigned to SDK team, deferred to future SDK maintenance sprint. Not blocking OCC development.

### Bug 6: Transport Restart During Fade-Out

**Status:** Deferred to SDK team (ORP097)
**Severity:** Medium
**Effort:** 2-3 hours

**Issue:** Clicking clip during fade-out doesn't restart playback

**Expected Behavior:** Cancel fade, restart from IN point with fade-in

**Workaround:** Wait for fade-out to complete, then click again

**SDK Components:**

- `src/core/transport/TransportController.cpp:startClip()`
- State machine transitions (STOPPING → PLAYING)

---

### Bug 7: Loop Point Fade Behavior

**Status:** Deferred to SDK team (ORP097)
**Severity:** Medium
**Effort:** 3-4 hours

**Issue:** Fades applied at loop boundaries (should be seamless)

**Expected Behavior:** No fades at loop points, only at clip start/stop

**Workaround:** Use long audio files (minimize loop frequency)

**SDK Components:**

- `src/core/transport/ClipPlayback.cpp:processAudio()`
- Loop point handling in fade envelope

---

**Decision:** Not blocking OCC development, defer to dedicated SDK maintenance sprint when SDK team has bandwidth.

---

## Execution Guidelines for Claude Code

### Development Workflow

1. **Pre-Sprint Planning:**
   - Review sprint prerequisites
   - Check for blocking dependencies
   - Read relevant existing code
   - Verify task understanding with user

2. **During Sprint:**
   - Work incrementally (one task at a time)
   - Test after each task completion
   - Build frequently using `build-launch.sh`
   - Update progress tracking
   - Ask for user feedback on UI/UX changes

3. **Post-Sprint:**
   - Complete sprint success metrics checklist
   - Document any deviations from plan
   - Create session report for significant work
   - Get user approval before proceeding to next sprint

### Code Quality Standards

**Read Before Writing:**

- Always read existing code before modifying
- Match existing coding style and patterns
- Follow JUCE framework conventions
- Respect SDK integration boundaries (Layers 3-5)

**Testing:**

- Build after each task using `build-launch.sh`
- Manual smoke test for UI changes
- Profile performance for CPU/memory changes
- Verify no regressions from previous work

**Performance:**

- No allocations in audio thread
- Lock-free audio↔UI communication
- <5% CPU at idle target (Sprint 1)
- <100ms waveform generation (Sprint 4)

### Tool Usage

**Recommended:**

- **Read:** For examining existing code
- **Grep/Glob:** For searching codebase before changes
- **Bash:** For build scripts (`build-launch.sh`, `clean-relaunch.sh`)
- **Edit:** For targeted code changes
- **Write:** Only for new files (prefer Edit for existing files)

**Documentation:**

- Create session reports for significant work
- Update PROGRESS.md as work proceeds
- Reference related OCC/ORP documents
- Follow workspace documentation conventions

### Communication

**Ask User Before:**

- Major architectural decisions
- UI/UX design choices requiring visual approval
- Deviating from sprint plan
- Skipping or deferring tasks

**Report Immediately:**

- Blocking issues or bugs
- Performance regressions
- Build failures
- Critical decisions needed

---

## Sprint Execution Priority

**Recommended Order:**

**Week 1:**

- ✅ Sprint 1 (Critical performance - BLOCKING)
- ✅ Sprint 2 (Tab bar, status system)

**Week 2:**

- ✅ Sprint 3 (Clip button visual system)
- ✅ Sprint 4 (Clip Edit Dialog refinement)

**Week 3:**

- ✅ Sprint 5 (Session management, groups)
- ✅ Sprint 6 (Keyboard navigation)

**Week 4:**

- ✅ Sprint 7 (Typography, branding)
- ✅ Sprint 8 (Preferences system)

**Week 5:**

- ✅ Sprint 9 (Audio output routing)
- ✅ Sprint 10 (Session backend architecture)

**Week 6:**

- ✅ Sprint 11 (Standard macOS behaviors)
- ✅ Sprint 12 (Build tools)

**Alternative:** Tackle highest-priority sprints first based on user needs (Sprint 1 is BLOCKING, must go first)

---

## Success Criteria (Overall)

### v0.2.1 Release Readiness

**Performance:**

- [ ] <5% CPU at idle (down from 77%)
- [ ] No memory leaks (stable over 1+ hour session)
- [ ] <10ms audio latency maintained

**UI/UX:**

- [ ] Unified bottom bar (tabs, status, panic)
- [ ] Clip button visual system complete (5-row layout, corner indicators)
- [ ] Clip Edit Dialog refined (live preview, keyboard workflow)
- [ ] Typography professional (HK Grotesk throughout)

**Session Management:**

- [ ] Groups session-wide and editable
- [ ] Grid dimensions configurable
- [ ] Asset management robust (copy on import)
- [ ] Multiple save formats (regular, blocks, packed)

**Preferences:**

- [ ] Unified preferences dialog (Cmd+,)
- [ ] Application, session, and audio I/O settings configurable
- [ ] Settings persist correctly

**Polish:**

- [ ] Standard macOS key commands functional
- [ ] Application icon added
- [ ] Canadian spelling consistent
- [ ] Application name updated ("Clip Composer")

**Developer Experience:**

- [ ] Build scripts verified and documented
- [ ] Quick rebuild workflow (<30 seconds)

---

## Related Documents

**Sprint Context:**

- [OCC093](./OCC093%20v020%20Sprint%20-%20Completion%20Report.md) - v0.2.0 Sprint Completion Report
- [OCC103](./OCC103%20QA%20v020%20Results.md) - QA Test Template for v0.2.0
- [PROGRESS.md](../../PROGRESS.md) - Current Implementation Status
- [ORP097](../../../../docs/orp/ORP097%20SDK%20Transport%20-%20Fade%20Bug%20Fixes%20for%20OCC%20v020.md) - SDK Bugs (Deferred)

**Architecture:**

- [CLAUDE.md](../../CLAUDE.md) - Application Development Guide
- [OCC027](./OCC027%20API%20Contracts.md) - API Contracts (OCC ↔ SDK)
- [OCC023](./archive/OCC023.md) - Component Architecture v1.0 (archived)

**Product:**

- [OCC021](./OCC021%20Product%20Vision.md) - Product Vision (authoritative)
- [OCC026](./OCC026%20MVP%20Plan.md) - 6-Month MVP Plan

**Implementation Guides:**

- [OCC096](./OCC096.md) - SDK Integration Patterns
- [OCC098](./OCC098.md) - UI Components
- [OCC099](./OCC099.md) - Testing Strategy
- [OCC100](./OCC100.md) - Performance Requirements

---

## Changelog

**Version 1.0** (2025-11-01)

- Initial sprint plan created
- 12 sprints defined (6 weeks)
- ~80 tasks organized by priority
- Document references corrected (ORP097, OCC103)
- Build script status verified
- Ready for execution

---

## Notes

**Ready for Execution:** This document is the definitive v0.2.1 sprint plan. All sprints are actionable and prioritized. Begin with Sprint 1 (critical performance issues - BLOCKING) and proceed sequentially or by priority as needed.

**User Approval Required:** Get user sign-off before starting each sprint, especially for UI/UX changes requiring visual approval.

**Flexibility:** Sprint order can be adjusted based on user priorities, but Sprint 1 (performance) must be completed first as it's BLOCKING.

**Deferred Work:** SDK bugs (ORP097) are intentionally deferred to SDK team and not blocking OCC development.

---

**Document Status:** ✅ Active Planning
**Next Action:** Begin Sprint 1 (Critical Performance & SDK Integration)
**Estimated Completion:** End of December 2025 (6 weeks from now)
