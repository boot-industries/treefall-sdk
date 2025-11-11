# OCC103 - QA v0.2.0 Results

**Test Date:** [Nov 3, 2025]
**Tester:** [Chris Lyons]
**Build:** main branch (commit: [TO BE FILLED])
**Platform:** macOS [15.6.1]
**Audio Interface:** [Macbook Pro Built-in Speakers]

---

## Test Environment Setup

- [✅] Clean install from latest main branch
- [✅] Deleted all preferences/caches (factory reset)
- [⚠️] Loaded reference session (960 clips, mixed metadata) — only 384 clips possible in current build, and performance got VERY sluggish at 192 clips (only 4 of 8 tabs full)
- [✅] Tested on primary hardware (macOS, CoreAudio)

**Environment Notes:**

```
[Add any setup notes here]
```

---

## Core Functionality Tests

### Performance Baseline

- [❌] **Load session with 960 clips** → Verify <2s load time
  - **Result:** ⏱️ [TIME] seconds
  - **Status:** ❌ FAIL
  - **Notes:** only 384 clips possible in current build, and performance got VERY sluggish at 192 clips (only 4 of 8 tabs full). Playback is only possible from Tab 1 – clips on Tab >1 do not play back (check logs). Session with 192 clips across first four tabs does load quickly.

- [✅] **Trigger 16 simultaneous clips** → Verify no dropouts
  - **Result:** [DESCRIBE BEHAVIOR]
  - **Status:** ✅ PASS
  - **Notes:** Confirmed 32 clips playing back simultaneously. >32 presently not allowed. "Stop All" while all clips playing produced undesirable distortion artifacts (during brief, abrupt cut)

- [❌] **CPU usage during 16-clip playback** → Verify <30%
  - **Result:** 📊 [~113%]
  - **Status:** ❌ FAIL
  - **Notes:** Application idles around 107% CPU with no clips playing, and only jumps to ~113% when 32 clips are playing. This suggests it is not clip playback that is gobbling CPU.

- [?] **Memory stability** → Verify stable over 5 minutes
  - **Result:** You might have to diagnose this further. { Real Memory Size: 793.7 MB | Virtual Memory Size: 20.38 TB | Shared Memory Size: 65.9 MB | Private Memory Size: 672.2 MB |
  - **Status:** ? Not sure
  - **Notes:**

- [✅] **Round-trip latency** → Verify <16ms (512 samples @ 48kHz)
  - **Result:** SEEMS HEALTHY
  - **Status:** ✅ PASS
  - **Notes:** Have not tested this scientifically – but sensory impression is that latency times are correct.

---

## v0.2.0 Fix Verification (from OCC093)

### Fix #1: Stop Others Fade Distortion

**Expected:** Smooth fade-out with no zigzag distortion

**Test Procedure:**

1. Load 2 clips on different buttons
2. Start clip 1
3. While clip 1 is playing, start clip 2
4. Listen to clip 1's fade-out

**Results:**

- [✅] Fade-out is smooth (no distortion)
- [✅] Fade-out is identical to manual stop
- **Status:** ✅ PASS
- **Notes:** No issues observed here. (Editing is extremely sluggish – we will have to fix CPU and memory issues mentioned above.)

---

### Fix #2: 75fps Button State Tracking

**Expected:** Clip buttons update in real-time during playback (visual sync at 75fps)

**Test Procedure:**

1. Load a long clip (>10 seconds)
2. Start playback
3. Observe button state changes (color, progress indicator)

**Results:**

- [✅] Button state updates during playback
- [❌] Visual state matches audio playback accurately
- [❌] No frozen or laggy button states
- **Status:** ❌ FAIL
- **Notes:** We're close. Clip colours are repainted at 75fps, but clip indicator icons are still only refreshing on edit dialog 'OK' confirmation. As the edit dialog is a live player of that clip's audio, changes should be immediately (75fps) reflected on the clip's surface for confidence.

---

### Fix #3: Edit Dialog Time Counter Spacing

**Expected:** No text collision between time counter and waveform

**Test Procedure:**

1. Open Edit Dialog for any clip
2. Observe time counter position
3. Check for overlap with waveform bottom edge

**Results:**

- [✅] Time counter has clear vertical separation from waveform
- [✅] Text is fully readable (no overlap)
- **Status:** ✅ PASS
- **Notes:**

---

### Fix #4: `[` `]` Keyboard Shortcuts Restart Playback

**Expected:** `[` and `]` keys set trim points AND restart playback from new IN point

**CORRECTION:** '[' and ']' keys were switched to ',' and '.' keys to control IN point. Keys /; and /' control the OUT point (and do not need to restart)

**Test Procedure:**

1. Open Edit Dialog for a clip
2. Start playback
3. Press `,` or '.' keys (set IN point)
4. Verify playback restarts from new IN point
5. Press `;` or `'` keys (set OUT point)
6. Verify playback behavior

**Results:**

- [✅] In keys set IN point and restart playback
- [✅] Out keys set OUT point and does not restart
- [✅] Behavior matches `<` `>` mouse buttons
- **Status:** ✅ PASS
- **Notes:** 1) "Shift+"" modifier still not increasing nudge to 15 frames from 1 frame. 2) Playhead is still able to escape In/Out bounds in rare cases, which should be 100% illegal at all times.

---

### Fix #5: Transport Spam Fix (Single Command Per Action)

**Expected:** Click-to-jog sends single command (gap-free seeking)

**Test Procedure:**

1. Open Edit Dialog for a clip
2. Start playback
3. Click on waveform to jog to different position
4. Observe audio behavior (should be gap-free)
5. Rapidly click multiple positions

**Results:**

- [✅] Jog action is gap-free (no stop/start artifacts)
- [✅] Rapid jogs are smooth and responsive
- [✅] No audio glitches during seeking
- **Status:** ✅ PASS
- **Notes:** Great functionality, aside from overall session lag (reported earlier)

---

### Fix #6: Trim Point Edit Laws (Playhead Constraint Enforcement)

**Expected:** Playhead NEVER escapes trim boundaries (all input methods)

**Test Procedure:**

#### 6a. Cmd+Click Set IN Point

1. Open Edit Dialog, start playback
2. Let clip play past 2 seconds
3. Cmd+Click waveform before playhead (set IN before playhead)
4. **Expected:** Playhead jumps to new IN, restarts

**Results:**

- [❌] Playhead jumps to IN when IN is set after playhead
- **Status:** ❌ FAIL
- **Notes:** Playhead continues without restarting at new IN. Restarting via PLAY starts from 0s, not new IN. Trim keys ',' and '.' correctly reset playhead to new IN, reliably and consistently.

#### 6b. Cmd+Shift+Click Set OUT Point

1. Open Edit Dialog, start playback
2. Let clip play past 2 seconds
3. Cmd+Shift+Click waveform before playhead (set OUT <= playhead)
4. **Expected:** Playhead jumps to IN, restarts

**Results:**

- [❌] Playhead jumps to IN when OUT is set before/at playhead
- **Status:** ❌ FAIL
- **Notes:** Playhead returns to 0s instead of new IN. Restarting via PLAY starts from 0s, not new IN. Trim keys ',' and '.' correctly reset playhead to new IN, reliably and consistently.

#### 6c. Keyboard Shortcuts (`,` `.` `;` `'`)

1. Test all 4 keyboard shortcuts with playback active
2. Verify edit laws are enforced

**Results:**

- [✅] `,` (set IN) enforces edit law
- [✅] `.` (set OUT) enforces edit law
- [❌] `;` (nudge OUT left) enforces edit law
- [❌] `'` (nudge OUT right) enforces edit law
- **Status:** ❌ FAIL
- **Notes:** Trim keys `,` and `.` correctly reset playhead to new IN, reliably and consistently. Trim keys `;` and `'` adjust OUT point but do not restart playhead at new IN. (Previously we had decided that OUT adjust should not restart playhead: I will reverse this decision. All trim adjusts should restart playhead from IN.)

#### 6d. Time Editor Inputs

1. Open Edit Dialog, start playback
2. Manually type new OUT time before playhead
3. **Expected:** Playhead jumps to IN, restarts

**Results:**

- [❌] Time editor enforces edit law
- **Status:** ❌ FAIL
- **Notes:** Passes as per old rules where OUT trim would not affect playhead, but let's update this as above so that all trim adjusts restart playhead from IN. Time editor fields accept time, and IN field correctly sends playhead to new IN point. Time IN and OUT fields both allow me to set time well past the time limit of the clip, which shouldn't be possible. Interestingly, Time IN is still not permitted to be greated than Time OUT in this scenario (this is correct). [ **ADD HERE: Don't treat time fields as text fields, treat them as counters. This means that each unit (eg. HH, MM, SS, FF) should be 'grabbable.' This is similar to ProTools transport, where users can quickly move through time units for convenient value entry. Example: User clicks MM value, MM accepts text entry, arrow key right to HH or arrow key left to SS, ENTER confirms entire time field. CAUTION: Enter must not confirm entire EDIT DIALOG when time field is being edited.** ]

#### 6e. Shift+Click Nudge (10x Acceleration)

1. Open Edit Dialog
2. Shift+click nudge buttons repeatedly
3. Verify 10x acceleration and no crashes

**Results:**

- [ ] Shift+click provides 10x acceleration
- [ ] No crashes during rapid nudging
- **Status:** ✅ PASS / ❌ FAIL
- **Notes:** This has been misinterpreted and we will have to revisit this. Shift+click is not a thing. Shift+ mofifier is supposed to affect nudge value of trim KEYS (1 frame vs. modified 15 frames).

---

## Edit Dialog Workflows

### Loop Mode

- [❌] **Loop toggle** → Clip loops infinitely at trim OUT point
  - **Status:** ❌ FAIL
  - **Notes:** Works reliably until I cmd+shift+click the waveform to set a new OUT point, at which point the playhead escapes the OUT point and does not loop. If I adjust the OUT via trim keys, trim buttons, or "SET" button, playhead correctly obeys the OUT point and loops back to IN. All other behaviour seems to work, including live tracking of loop button state.

### Waveform Interactions

- [❌] **Cmd+Click waveform (set IN)** → Playhead jumps to IN, restarts
  - **Status:** ❌ FAIL
  - **Notes:** IN is adjusted but playhead does not restart at new IN

- [❌] **Cmd+Shift+Click waveform (set OUT)** → If OUT <= playhead, jump to IN and restart
  - **Status:** ❌ FAIL
  - **Notes:** This allows the playhead to escape the OUT point, as noted above.

### Keyboard Navigation

- [❌] **`,` `.` `;` `'`** → Edit laws enforced for all shortcuts
  - **Status:** ❌ FAIL
  - **Notes:** Details provided above (IN keys behave correctly, OUT keys should be updated)

### Time Editor

- [❌] **Time editor inputs** → Edit laws enforced, playhead respects boundaries
  - **Status:** ❌ FAIL
  - **Notes:** Details provided above (IN keys behave correctly, OUT keys should be updated)

---

## Multi-Tab Isolation (Critical)

**Expected:** Clips on different tabs are fully isolated (no cross-tab triggering)

**Test Procedure:**

1. Load clip on Tab 1, button 1
2. Load clip on Tab 2, button 1
3. Switch to Tab 1, trigger button 1
4. **Expected:** Only Tab 1 clip plays (Tab 2 clip does NOT play)
5. Switch to Tab 2, trigger button 1
6. **Expected:** Only Tab 2 clip plays (Tab 1 clip does NOT play)
7. Repeat for all 8 tabs

**Results:**

- [ ] Tab 1 isolated (no cross-tab triggering)
- [ ] Tab 2 isolated
- [ ] Tab 3 isolated
- [ ] Tab 4 isolated
- [ ] Tab 5 isolated
- [ ] Tab 6 isolated
- [ ] Tab 7 isolated
- [ ] Tab 8 isolated
- **Status:** ❌ FAIL
- **Notes:** Clips on Tabs 2 through 8 are not playing back. Audio only heard on clip buttons 1-48 despite 49-192 being populated.

---

## Regression Testing (v0.1.0 Features)

### Session Management

- [✅] **Session save/load** → Metadata preserved (trim, fade, gain, loop, color)
  - **Status:** ✅ PASS
  - **Notes:** No issues observed

### Routing Controls

- [ ] **4 Clip Groups functional** → Routing controls work correctly
  - **Status:**
  - **Notes:** Not able to test this in present build.

### Keyboard Shortcuts

- [✅] **Space bar** → Stop/Start
- [?] **Arrow keys** → Navigation
- [tbd] **Modifier combos** → All working correctly
  - **Status:** Mixed
  - **Notes:** 1) SPACE correctly Starts and Stops in Edit Dialog, and Stops only in main clip grid (SPACE should not Start in main clip grid, it should effectively be "STOP ALL"). 2) Arrow keys do nothing. Did you mean Trim keys? Details on Trim keys provided above. 3) Modifier keys are mostly working, it seems. Shift+ modifier for trim keys seems to be the biggest gap here. Other details provided above.

### Visual Components

- [✅] **Waveform display** → Rendering correctly, no visual glitches
  - **Status:** ✅ PASS
  - **Notes:** Still a bit slow to load when Edit Dialog opens, but I think it's better than it was.

---

## Stability Testing

### 30-Minute Session

**Test Procedure:**

1. Run application for 30 minutes
2. Perform varied workflows (load clips, trigger, edit, save)
3. Monitor for crashes, memory leaks, performance degradation

**Results:**

- [✅] No crashes during 30-minute session
- [?] No memory leaks (memory stable)
- [?] Performance remains consistent
- **Status:** Mixed –– still very concerning CPU usage
- **Notes:**

---

## Critical Bugs Found

**Count:** [NUMBER]

### Bug #1 (if any)

- **Severity:** Critical / Major / Minor
- **Description:**
- **Steps to Reproduce:**
- **Expected Behavior:**
- **Actual Behavior:**
- **GitHub Issue:** #[NUMBER]

---

## Minor Bugs Found

**Count:** [NUMBER]

### Bug #1 (if any)

- **Description:**
- **GitHub Issue:** #[NUMBER]

---

## Overall Assessment

**Total Tests:** [NUMBER]
**Passed:** [NUMBER]
**Failed:** [NUMBER]
**Pass Rate:** [PERCENTAGE]%

### Acceptance Criteria

- [ ] All test cases pass (0 critical bugs, <3 minor bugs)
- [ ] No regressions from v0.1.0
- [ ] No crashes during 30-minute test session
- [ ] Ready for v0.2.0-alpha release

### Recommendation

- ✅ **APPROVED for release** - All critical tests passed
- ⚠️ **APPROVED with minor issues** - Non-blocking bugs found, defer to v0.2.1
- ❌ **REJECTED** - Critical bugs found, must fix before release

**Tester Signature:** [NAME]
**Date:** [DATE]

---

## References

[1] OCC093 v0.2.0 Sprint - Completion Report
[2] OCC102 v0.2.0 Release & v0.2.1 Planning (Task 1.1)
[3] OCC099 Testing Strategy

---

**Document Status:** Draft - Awaiting Manual Testing
**Created:** 2025-10-31
**Last Updated:** 2025-10-31
