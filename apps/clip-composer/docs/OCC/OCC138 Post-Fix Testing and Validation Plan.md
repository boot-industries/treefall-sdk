# OCC138: Post-Fix Testing and Validation Plan

**Status:** Ready for Testing
**Priority:** CRITICAL
**Date:** 2025-01-15
**Cross-Reference:** OCC137 (Impact Analysis), ORP114 (SDK Fixes)
**Blocking:** v0.2.2 Release

---

## Executive Summary

SDK team has **completed all 3 critical fixes** for gain staging bug (ORP114). Fixes are implemented in `transport_controller.cpp` and ready for validation testing.

**Testing Required:** 2.5 hours (critical path + regression)
**OCC Code Changes Required:** NONE
**User Impact:** Critical bug eliminated, no UX changes

---

## SDK Fixes Implemented (ORP114)

### ✅ Fix 1: Position Clamping in updateClipTrimPoints()

**Location:** `transport_controller.cpp` lines 928-943

**What it does:**

- Clamps `clip.currentSample` to new trim range after update
- Seeks reader to clamped position
- Prevents position from escaping into invalid fade zones

**Expected Impact:**

- Edit Dialog trim nudges safe during playback
- No more gain accumulation when reducing Trim OUT

---

### ✅ Fix 2: Bounds Validation in Fade Calculation

**Location:** `transport_controller.cpp` lines 366-386

**What it does:**

- Validates `relativePos` is within trim range BEFORE calculating fades
- Skips fade logic if position is out of bounds
- Defense-in-depth protection (should never trigger after Fix 1)

**Expected Impact:**

- Fail-safe against any future trim race conditions
- No fade calculation overflows even if position escapes

---

### ✅ Fix 3: Restart Crossfade Enabled

**Location:** `transport_controller.cpp` lines 1367-1371

**What it does:**

- Sets `isRestarting = true` in restartClip()
- Applies 5ms broadcast-safe crossfade on manual restart
- Matches behavior of initial clip start

**Expected Impact:**

- Grid button re-triggers smooth and click-free
- Consistent restart behavior (matches startClip)

---

## OCC App-Side Findings (No Changes Required)

### 1. Mouse Click Behavior: ✅ CORRECT

**Finding:** NO debouncing on mouse clicks (confirmed by user requirement)

**Code Evidence:**

```cpp
// ClipButton.cpp line 640-642
// Fire click immediately (don't wait for mouseUp to avoid double-click delay)
// This makes rapid clicking feel responsive
```

**Validation:** Rapid clicking is intentional for multi-voice layering (up to 4 voices)

---

### 2. Trim OUT Key Command Flow: ✅ SAFE (after SDK fix)

**Flow:**

```
User presses `;` or `'` key
  ↓
ClipEditDialog nudgeAction()
  ↓
PreviewPlayer::setTrimPoints()
  ↓
AudioEngine::updateClipMetadata()
  ↓
TransportController::updateClipTrimPoints()
  ↓
[ORP114 Fix 1] Position clamped to new range ✅
```

**Before Fix:** Position could escape, causing fade overflow → gain accumulation
**After Fix:** Position clamped immediately, fade calculations always valid

---

### 3. Timer Accelerator (300ms repeat): ✅ SAFE (after SDK fix)

**Behavior:** Holding `;` or `'` key fires trim updates every 300ms

**Risk Assessment:**

- **Before Fix:** Rapid updates could trigger race condition on EVERY repeat
- **After Fix:** Each update safely clamps position, no accumulation possible

**Testing Note:** Hold key for 5 seconds to verify no gain accumulation over 15+ updates

---

### 4. Multi-Voice Summing: ⚠️ NOT A BUG (by design)

**Behavior:** Rapid grid button clicks can cause 2x-4x gain increase

**Reason:** Intentional multi-voice layering (up to 4 voices per clip)

**Mitigation:**

- Routing matrix clipping protection limits distortion severity
- Users can enable "Stop Others On Play" to prevent layering

**Documentation Update Required:** Add tooltip explaining multi-voice behavior

---

## Critical Path Tests (Must Pass)

### Test 1: Edit Dialog Trim OUT Nudge During Playback

**Priority:** CRITICAL (primary bug trigger)

**Steps:**

1. Load test clip (e.g., `test-assets/sine-1khz-48k-10s.wav`)
2. Open Edit Dialog
3. Start preview playback
4. Press `>` key 20 times rapidly to reduce Trim OUT by 20 ticks
5. Listen for distortion
6. Check meters for gain spikes

**Expected Result:**

- ✅ No distortion
- ✅ Gain stays at unity (0 dB)
- ✅ Position updates smoothly
- ✅ Playback continues without clicks

**Current Behavior (v0.2.1):** ❌ Gain accumulates, loud distortion

---

### Test 2: Edit Dialog Trim IN Nudge During Playback

**Priority:** HIGH

**Steps:**

1. Load test clip
2. Open Edit Dialog, start preview playback
3. Set Trim OUT to 5 seconds (to create space)
4. Seek to 4 seconds position
5. Press `<` key 10 times to increase Trim IN toward playhead

**Expected Result:**

- ✅ Position clamps to new IN point when IN passes playhead
- ✅ No distortion or gain accumulation
- ✅ Smooth transition to new IN point

**Current Behavior (v0.2.1):** ⚠️ Likely safe (not primary trigger)

---

### Test 3: Loop Mode with Trim OUT Reduction

**Priority:** HIGH

**Steps:**

1. Load test clip
2. Enable loop mode
3. Start playback
4. Wait for 2 loop cycles
5. Open Edit Dialog
6. Press `>` key 10 times to reduce Trim OUT
7. Let clip loop 5 more times

**Expected Result:**

- ✅ Smooth loop transitions
- ✅ No gain accumulation on loop restarts
- ✅ Trim OUT reduction takes effect immediately

**Current Behavior (v0.2.1):** ❌ Gain accumulates on loop restart

---

### Test 4: Rapid Grid Button Re-triggers

**Priority:** MEDIUM (tests restart crossfade)

**Steps:**

1. Load test clip on button 0
2. Click button 0 five times rapidly (< 1 second)
3. Listen for clicks or distortion
4. Check if multiple voices are playing (expected)

**Expected Result:**

- ✅ Smooth restarts with 5ms crossfade (no clicks)
- ✅ Up to 4 voices playing (multi-voice layering, intentional)
- ✅ Clipping protection prevents hard clipping

**Current Behavior (v0.2.1):** ⚠️ Possible clicks, multi-voice summing (2x-4x gain)

---

### Test 5: Waveform Click-to-Seek During Playback

**Priority:** MEDIUM

**Steps:**

1. Load test clip
2. Open Edit Dialog, start preview playback
3. Click waveform at 10 random positions
4. Listen for distortion

**Expected Result:**

- ✅ Smooth seeks, no distortion
- ✅ Position updates accurately

**Current Behavior (v0.2.1):** ✅ Likely safe (uses seekClip, not trim update)

---

## Regression Tests (Must Not Break)

### Regression 1: Normal Playback (No Trim Changes)

**Verify existing behavior unchanged**

**Steps:**

1. Load test clip
2. Start playback, let play to end
3. Measure gain output

**Expected Result:**

- ✅ Unity gain (0 dB)
- ✅ No distortion
- ✅ Smooth playback from IN to OUT

---

### Regression 2: Loop Mode (No Trim Changes)

**Verify seamless loops unchanged**

**Steps:**

1. Load test clip
2. Enable loop mode
3. Start playback
4. Let loop 10 times

**Expected Result:**

- ✅ Seamless loops (no fade at loop point)
- ✅ No gain accumulation
- ✅ No clicks or pops

---

### Regression 3: Clip Fade-In/Out (First Playthrough)

**Verify fade logic unchanged**

**Steps:**

1. Load test clip
2. Set fade-in = 1.0s, fade-out = 1.0s
3. Disable loop mode
4. Start playback, listen for fades

**Expected Result:**

- ✅ Smooth 1s fade-in at start
- ✅ Smooth 1s fade-out at end
- ✅ No mid-clip fade artifacts

---

### Regression 4: Trim Adjustments on Stopped Clip

**Verify stopped clip trim updates unchanged**

**Steps:**

1. Load test clip
2. With playback STOPPED, adjust Trim IN and OUT via Edit Dialog
3. Start playback

**Expected Result:**

- ✅ Trim changes apply correctly
- ✅ Playback starts from new IN point
- ✅ Stops at new OUT point

---

## Performance Tests

### Performance 1: Trim Update Latency

**Measure update responsiveness**

**Steps:**

1. Load test clip
2. Open Edit Dialog, start preview playback
3. Hold `>` key for 5 seconds (15+ rapid updates)
4. Measure UI responsiveness

**Expected Result:**

- ✅ No audio dropouts
- ✅ Waveform playhead tracks smoothly
- ✅ Trim markers update in real-time

---

### Performance 2: Multi-Voice CPU Usage

**Verify 4-voice playback stable**

**Steps:**

1. Load test clip on button 0
2. Click button 0 rapidly to create 4 voices
3. Monitor CPU usage in Activity Monitor

**Expected Result:**

- ✅ CPU < 30% with 4 voices playing
- ✅ No audio dropouts
- ✅ Routing matrix handles summing correctly

---

## Edge Case Tests

### Edge 1: Trim OUT Reduced Past Playhead Position

**Verify Fix 1 clamping works**

**Steps:**

1. Load 10-second test clip
2. Open Edit Dialog, start playback
3. Seek to 8 seconds
4. Rapidly reduce Trim OUT to 5 seconds (past playhead)

**Expected Result:**

- ✅ Position clamps to new OUT point (5s)
- ✅ No distortion or gain spike
- ✅ Playback continues from new OUT position

---

### Edge 2: Trim IN Increased Past Playhead Position

**Verify Fix 1 clamping works (reverse direction)**

**Steps:**

1. Load 10-second test clip
2. Open Edit Dialog, start playback
3. Seek to 2 seconds
4. Rapidly increase Trim IN to 5 seconds (past playhead)

**Expected Result:**

- ✅ Position clamps to new IN point (5s)
- ✅ No distortion
- ✅ Playback continues from new IN position

---

### Edge 3: Trim OUT = Trim IN (Zero-Length Clip)

**Verify bounds checking prevents crash**

**Steps:**

1. Load test clip
2. Set Trim IN = 5s, Trim OUT = 5s (zero-length)
3. Attempt to start playback

**Expected Result:**

- ✅ SDK rejects invalid trim points (SDK validation)
- ✅ No crash or undefined behavior

---

## Test Environment Setup

### Required Assets

```
test-assets/
├── sine-1khz-48k-10s.wav    # 10s mono sine wave @ 1kHz, 48kHz
├── stereo-music-48k-30s.wav # 30s stereo music clip, 48kHz
└── drums-48k-loop-4bars.wav # 4-bar drum loop, 48kHz
```

### Audio Settings

- Sample Rate: 48000 Hz
- Buffer Size: 512 samples (10.7ms latency)
- Output Device: Default CoreAudio device

### Monitoring Tools

- Logic Pro or Ableton Live (for metering)
- Activity Monitor (CPU usage)
- Console.app (debug logs)

---

## Test Execution Checklist

**Pre-Test:**

- [ ] Rebuild SDK and OCC from clean state
- [ ] Launch OCC, verify version shows latest commit
- [ ] Open Console.app, filter for "AudioEngine" and "TransportController"
- [ ] Load test assets into OCC session

**Critical Path Tests (60 min):**

- [ ] Test 1: Edit Dialog Trim OUT nudge (15 min)
- [ ] Test 2: Edit Dialog Trim IN nudge (15 min)
- [ ] Test 3: Loop mode with Trim OUT reduction (15 min)
- [ ] Test 4: Rapid grid button re-triggers (10 min)
- [ ] Test 5: Waveform click-to-seek (5 min)

**Regression Tests (60 min):**

- [ ] Regression 1: Normal playback (10 min)
- [ ] Regression 2: Loop mode (10 min)
- [ ] Regression 3: Clip fades (15 min)
- [ ] Regression 4: Trim on stopped clip (10 min)
- [ ] Performance 1: Trim update latency (5 min)
- [ ] Performance 2: Multi-voice CPU (10 min)

**Edge Cases (30 min):**

- [ ] Edge 1: Trim OUT past playhead (10 min)
- [ ] Edge 2: Trim IN past playhead (10 min)
- [ ] Edge 3: Zero-length clip (10 min)

**Post-Test:**

- [ ] Review debug logs for warnings/errors
- [ ] Document any unexpected behavior
- [ ] Update OCC137 with test results

---

## Success Criteria

**Must Pass (Blocking v0.2.2 Release):**

1. ✅ Test 1 (Trim OUT nudge) - NO distortion
2. ✅ Test 3 (Loop + Trim OUT) - NO gain accumulation
3. ✅ All Regression tests pass unchanged

**Should Pass (Non-Blocking):**

1. ✅ Test 4 (Rapid re-triggers) - Smooth crossfades
2. ✅ Edge cases handle gracefully

**Known Limitations (Not Bugs):**

1. ⚠️ Multi-voice summing (2x-4x gain) with rapid clicks - INTENTIONAL
2. ⚠️ Clipping protection softens distortion - INTENTIONAL

---

## Failure Response Plan

### If Critical Path Test Fails:

**Step 1:** Capture debug logs showing failure

```bash
# In Console.app, filter for:
AudioEngine
TransportController
CRITICAL
WARNING
```

**Step 2:** Document exact reproduction steps

**Step 3:** Create minimal test case (simplest trigger)

**Step 4:** Report to SDK team with logs and test case

**Step 5:** DO NOT release v0.2.2 until fix validated

---

### If Regression Test Fails:

**Step 1:** Compare behavior to v0.2.1 (known good baseline)

**Step 2:** Determine if regression is in SDK or OCC

**Step 3:** If SDK regression, revert ORP114 fixes and re-test

**Step 4:** If OCC regression, check for unintended side effects

---

## Documentation Updates Required

### User Manual Updates

**Section:** Edit Dialog - Trim Controls

```markdown
**Tip:** You can adjust Trim IN and OUT points during playback.
The playhead position will automatically adjust to stay within
the new trim range.
```

**Section:** Grid Buttons - Multi-Voice Layering

```markdown
**Multi-Voice Layering:** Clicking the same grid button rapidly
allows up to 4 voices to play simultaneously, creating a layered
effect. To prevent this and ensure exclusive playback, enable
"Stop Others On Play" in clip settings.
```

---

### Tooltip Updates

- Grid Button: "Click to play. Rapid clicks layer up to 4 voices."
- Trim OUT Handle: "Drag to adjust end point. Updates during playback."
- Loop Toggle: "Enable seamless looping between IN and OUT points."

---

## Release Notes Template (v0.2.2)

```markdown
# Orpheus Clip Composer v0.2.2

**Release Date:** 2025-01-15
**Type:** Critical Bug Fix

## 🐛 Fixed

### Critical Gain Staging Bug (ORP114/OCC137)

Fixed severe audio distortion caused by gain accumulation during trim adjustments.

**What was broken:**

- Edit Dialog: Adjusting Trim OUT during preview caused loud distortion
- Loop Mode: Trim changes while looping caused gain accumulation
- Grid Buttons: Manual restarts produced clicks

**What's fixed:**

- ✅ Trim adjustments safe during playback (position clamped automatically)
- ✅ Loop + trim workflow smooth and distortion-free
- ✅ Grid button restarts smooth with broadcast-safe crossfades

**Technical Details:**

- Added position validation to trim point updates
- Enhanced fade calculation bounds checking
- Enabled restart crossfade for manual clip restarts
- See ORP114 and OCC137 for complete investigation report

## 📚 Documentation

### Multi-Voice Behavior Clarified

Rapid grid button clicks intentionally layer up to 4 voices for creative
layering effects. To prevent this, enable "Stop Others On Play" in clip
settings.

## 🙏 Credits

Special thanks to users who reported distortion issues during Edit Dialog
workflows. Your feedback helped us identify and fix this critical bug.

---

**Full Changelog:** See OCC137, ORP114
**Known Issues:** None blocking production use
**Next Release:** v0.3.0 (new features TBD)
```

---

## Timeline

**Today (2025-01-15):**

- ✅ SDK fixes implemented (ORP114) - COMPLETE
- ⏳ OCC testing (this document) - 2.5 hours
- ⏳ Documentation updates - 30 min
- ⏳ Release v0.2.2 - 15 min

**Total Time Remaining:** 3 hours 15 min

---

## References

**Investigation Documents:**

- ORP114: SDK-level investigation and fixes (COMPLETE)
- OCC137: OCC impact analysis (COMPLETE)
- OCC136: Loop fade regression debug (INCOMPLETE - likely related)

**Code Locations:**

- SDK fixes: `src/core/transport/transport_controller.cpp`
  - Lines 928-943 (Fix 1: Position clamping)
  - Lines 366-386 (Fix 2: Bounds validation)
  - Lines 1367-1371 (Fix 3: Restart crossfade)
- OCC trim flow: `Source/UI/ClipEditDialog.cpp` lines 2303-2415
- OCC preview: `Source/UI/PreviewPlayer.cpp` lines 49-77

**Related Fixes:**

- ORP097: Loop fade state machine (hasLoopedOnce)
- ORP093: Trim boundary enforcement
- OCC109: Clipping protection (v0.2.2)

---

**Next Action:** Execute critical path tests (start with Test 1)

**Blocking:** None - SDK fixes complete, ready to test

**Owner:** QA/Testing team (or developer self-test)

---

**Document Version:** 1.0
**Status:** Ready for Execution
**Estimated Completion:** 2025-01-15 evening
