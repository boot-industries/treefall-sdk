# OCC136 Loop Fade Regression Debug Session - INCOMPLETE

**Date:** 2025-11-15
**Status:** ❌ FAILED - Issue not resolved
**Session Duration:** ~2 hours
**Outcome:** Multiple attempts to fix loop fade regression unsuccessful

---

## Problem Statement

Three interconnected bugs reported after UX enhancements:

### Bug 1: Gain Stacking on Trim OUT

**Symptom:** Gain accumulates/stacks when using Trim OUT key command
**User Report:** "Gain went through the roof when I used a Trim OUT key command"

### Bug 2: Play State Visual Desync

**Symptom:** Edit Dialog playback stops, but main grid clip button stays lit "active"
**User Report:** "Play state was visually disconnected from main grid clip button. Clip Edit playback stopped, clip button stayed lit 'active'"

### Bug 3: Loop Fade-Out Regression

**Symptom:** Fade OUT occurring at loop crosspoint (regression from previous fixes)
**User Report:** "Fade OUT is occurring on LOOP point again, even though we have fixed this issue at least twice previously. Looped clips are supposed to ignore Fade OUT and Fade IN at Loop Crosspoints"

---

## Initial Hypothesis

**Root Cause:** Line 1365 in `src/core/transport/transport_controller.cpp` contradicts OCC129 specification

**Evidence:**

- Current code: `clip.hasLoopedOnce = false;` in `restartClip()` method
- OCC129 (commit 693293f1): `clip.hasLoopedOnce = true;`
- Comment in code says "Manual restart SHOULD apply clip fade-in" but OCC129 says it should NOT

**Mechanism:**

1. `hasLoopedOnce=false` re-enables clip fades after restart
2. Fades applied at loop crosspoints
3. Multiple loop fades interfere → gain stacking
4. Fade states cause visual desync between Edit Dialog and main grid
5. Loop crosspoint fades directly violate OCC129 seamless loop specification

---

## Attempted Fixes

### Fix Attempt 1: Revert hasLoopedOnce to OCC129 Specification

**File Modified:** `src/core/transport/transport_controller.cpp`
**Line Changed:** 1366 (previously 1365)

**Change:**

```cpp
// FROM (WRONG):
clip.hasLoopedOnce = false;

// TO (CORRECT per OCC129):
clip.hasLoopedOnce = true;
```

**Updated Comment:**

```cpp
// CRITICAL: Manual restart should NOT re-apply clip fade-in
// This matches auto-loop behavior - seamless loops with no fades at crosspoints
// Only the initial startClip() should have clip fade-in, not restartClip()
// OCC129 (commit 693293f1): "perfect button behavior, no zigzag distortion"
clip.hasLoopedOnce = true;
```

**Build Process:**

1. Deleted object file: `build/CMakeFiles/orpheus_transport.dir/src/core/transport/transport_controller.cpp.o`
2. Touched source file to force recompile
3. Rebuilt with verbose output - confirmed recompilation occurred
4. Relaunched app with `./scripts/relaunch-occ.sh`
5. Confirmed new PID (2156)

**Result:** ❌ FAILED - Issue persists

---

## Why the Fix Failed

**Possible Reasons:**

1. **Wrong Root Cause**
   - `hasLoopedOnce` may not be the actual issue
   - The contradiction with OCC129 may be intentional for a different use case
   - Multiple code paths may be setting `hasLoopedOnce` inconsistently

2. **Incomplete Fix**
   - There may be other locations where `hasLoopedOnce` is set incorrectly
   - The issue may involve interaction between multiple systems (PreviewPlayer + TransportController)
   - State synchronization between Edit Dialog and main grid may have separate bugs

3. **Caching Issues**
   - Despite rebuild, old code may still be running (unlikely given forced recompile)
   - macOS code signing cache may need clearing
   - Clip metadata from previous session may be persisting

4. **Misdiagnosis**
   - The three bugs may not share a common root cause
   - Loop fade issue may be separate from gain stacking
   - Visual desync may be UI-only (not SDK-level)

---

## Previous Session Work (Context)

**Completed UX Enhancements:**

1. ✅ Scaled Tabler transport icons down by 33%
2. ✅ Fixed Play/Stop color state tracking (75fps updates)
3. ✅ Added dark grey backdrop (4px padding) behind green time text on active clip buttons
4. ✅ Added 2px shadow to green time text on active buttons
5. ✅ Added 1px shadow to time text on loaded buttons
6. ✅ Added 1px shadow to clip name text on all buttons

**Fixed PreviewPlayer Gain Stacking:**

- Modified `PreviewPlayer::jumpTo()` to use `seekClip()` return value
- Eliminated race condition that was creating spurious voices
- Used existing 2-voice logic (MAX_VOICES_PER_CLIP enforcement)

**Files Modified in Previous Session:**

- `Source/UI/ClipEditDialog.cpp` - Transport icons and color updates
- `Source/ClipGrid/ClipButton.cpp` - Text shadows and backdrop
- `Source/UI/PreviewPlayer.cpp` - Multi-voice seek fix

---

## Investigation Required

### 1. Verify hasLoopedOnce State Flow

**Action:** Add debug logging to track `hasLoopedOnce` flag throughout clip lifecycle

**Locations to Log:**

- `addActiveClip()` - Initial state (should be `false`)
- Auto-loop in `processAudio()` line 275 - Sets to `true`
- Auto-loop in update loop line 456 - Sets to `true`
- `restartClip()` line 1366 - Now sets to `true` (was `false`)
- Any other location that modifies this flag

**Log Format:**

```cpp
DBG("hasLoopedOnce [" << clip.handle << "] = " << clip.hasLoopedOnce
    << " (source: addActiveClip/auto-loop/restart)");
```

### 2. Check for Multiple restartClip() Calls

**Hypothesis:** Edit Dialog may be calling `restartClip()` repeatedly
**Action:** Add call stack logging when `restartClip()` is invoked

### 3. Verify Trim OUT Key Command Behavior

**Action:** Trace what happens when user presses Trim OUT key:

1. Does it call `updateClipTrimPoints()`?
2. Does it trigger a restart?
3. Does it interact with loop state?

### 4. Check Edit Dialog vs Main Grid State Sync

**Action:** Review `PreviewPlayer` timer callback (line 212-246 in PreviewPlayer.cpp)

- Does it check `isPlaying()` correctly?
- Is there a race between Edit Dialog timer and main grid updates?
- Does stopping Edit Dialog playback update main grid button state?

### 5. Review OCC129 More Carefully

**Action:** Re-read OCC129 commit 693293f1 to understand:

- What exactly was the "perfect button behavior"?
- What was the rapid-fire test scenario?
- Were there ANY exceptions to `hasLoopedOnce = true` rule?

### 6. Check for Loop Mode Changes During Playback

**Hypothesis:** Toggling loop mode while playing may reset `hasLoopedOnce`
**Action:** Review `setClipLoopMode()` - does it modify `hasLoopedOnce`?

---

## Code Locations Reference

### hasLoopedOnce Flag Usage

**Initialize (false):**

- `addActiveClip()` line 717: `clip.hasLoopedOnce = false;`

**Set to true (auto-loop):**

- `processAudio()` line 275: `clip.hasLoopedOnce = true;` (when position >= trimOut)
- Update loop line 456: `clip.hasLoopedOnce = true;` (when clip reached OUT point and looping)

**Set to true (manual restart) - NEW:**

- `restartClip()` line 1366: `clip.hasLoopedOnce = true;` (changed from `false`)

**Checked (skip fades if true):**

- `processAudio()` line 362: `if (!clip.hasLoopedOnce) { /* apply fades */ }`

### Fade Application Logic

**Location:** `transport_controller.cpp` lines 360-378

**Behavior:**

- If `hasLoopedOnce == false`: Apply clip fade-in and fade-out
- If `hasLoopedOnce == true`: Skip clip fades (seamless loop)

**Fades Applied:**

1. Clip fade-in: First N samples from trim IN
2. Clip fade-out: Last N samples before trim OUT

### Multi-Voice Logic

**MAX_VOICES_PER_CLIP:** 4 (OCC uses 2)
**Active Clips:** Up to 64 simultaneous voices globally

**Key Functions:**

- `startClip()`: Adds new voice (removes oldest if at capacity)
- `stopClip()`: Stops ALL voices for handle
- `restartClip()`: Restarts ALL voices for handle back to IN point
- `seekClip()`: Seeks ALL voices for handle to same position

---

## Questions for User

1. **What specific actions trigger the bugs?**
   - Does it happen on first loop, or only after multiple loops?
   - Does toggling Loop ON/OFF while playing trigger it?
   - Does clicking waveform to jog playhead trigger it?
   - Does Trim OUT key command trigger it immediately or after some action?

2. **Are all three bugs always present together?**
   - Or do they occur independently?
   - Which bug is most critical/reproducible?

3. **What does the audio callback log show?**
   - Can we see evidence of multiple voices being created?
   - Can we see fade gains being applied at loop points?

4. **What was working before?**
   - When was the last time loop behavior was correct?
   - What changed between then and now?

---

## CRITICAL FINDING: Fixes Were Backwards

**Discovery:** After failure, checked known good checkpoint (85029da9) and found our "fixes" implemented the OPPOSITE of what was working.

### Known Good State (85029da9 - Last Night's Checkpoint)

**PreviewPlayer.cpp `jumpTo()` method:**

```cpp
bool wasPlaying = m_audioEngine->isClipPlaying(m_buttonIndex);

if (wasPlaying) {
  // Clip is playing - seek to new position
  bool seeked = m_audioEngine->seekClip(m_buttonIndex, samplePosition);
  // ...
} else {
  // Clip is stopped - start playback from clicked position
  bool started = m_audioEngine->startClip(m_buttonIndex);
  // ...
}
```

**TransportController.cpp `restartClip()` method:**

```cpp
// CRITICAL: Manual restart SHOULD apply clip fade-in (user action)
// This is DIFFERENT from auto-loop which should NOT apply fade-in
// Set hasLoopedOnce = false to allow clip fade-in on restart
clip.hasLoopedOnce = false;
```

### What We Changed (WRONG)

**PreviewPlayer.cpp:**

- ❌ Changed to use `seekClip()` return value instead of `isClipPlaying()`
- **This was WRONG** - known good code used `isClipPlaying()` approach

**TransportController.cpp:**

- ❌ Changed `hasLoopedOnce = false` to `hasLoopedOnce = true`
- **This was WRONG** - known good code had `hasLoopedOnce = false`

### Implications

1. **Our fixes made things WORSE, not better**
2. **Known good checkpoint (85029da9) had OPPOSITE implementation**
3. **OCC129 reference may have been misinterpreted or applies to different context**
4. **Bugs likely introduced in UI commits (fb854551, 87a4e266, 076da56b), NOT SDK**

### Commits to Investigate

**Known Good:**

- ✅ `85029da9` - Feature/occ135 gap analysis (CHECKPOINT - WORKING)

**Potentially Safe:**

- ⚠️ `fb854551` - fix: optimize Clip Composer frontend UX
  - Transport button colors
  - Dialog height 800→850px
  - Status indicator positioning

**Suspected Break Point:**

- ❌ `87a4e266` - feat: implement Tabler Icons for transport controls
  - Only modified ClipEditDialog.cpp
  - Added icon resources
  - No SDK changes visible

**Untrusted:**

- ❌ `076da56b` - feat: enhance Clip Composer UX with refined icons and text styling
  - Current HEAD
  - Includes all previous commits

### Action Required

**Next session MUST:**

1. **Revert ALL uncommitted changes**

   ```bash
   cd /Users/chrislyons/dev/orpheus-sdk
   git stash push -m "Failed debug session 2025-11-15 - fixes were backwards"
   ```

2. **Test at known good checkpoint**

   ```bash
   git checkout 85029da9
   ./scripts/relaunch-occ.sh
   # TEST: Do bugs exist here?
   ```

3. **If bugs DON'T exist at 85029da9:**
   - Bisect between 85029da9 and 076da56b
   - Find exact commit that introduced bugs
   - Focus on UI code, NOT SDK code

4. **If bugs DO exist at 85029da9:**
   - Problem is older than we thought
   - May need to go back further in history
   - Re-evaluate what "known good" actually means

### Why Our Approach Failed

1. **Assumed OCC129 was gospel** - but known good code contradicted it
2. **Didn't verify known good state first** - should have checked 85029da9 BEFORE making changes
3. **Changed multiple things at once** - PreviewPlayer AND TransportController
4. **Didn't test incrementally** - made both changes, built, tested once
5. **Misread the evidence** - OCC129 may apply to different scenario/context

---

## Next Steps (For Future Session)

### Immediate Actions

1. **Add Diagnostic Logging**
   - Instrument `hasLoopedOnce` state changes
   - Log all `restartClip()` calls with stack traces
   - Log fade gain calculations at loop boundaries

2. **Reproduce Bugs Systematically**
   - Create minimal reproduction steps for each bug
   - Test each bug independently
   - Record audio callback output during reproduction

3. **Review Git History**
   - Check when `restartClip()` was last modified
   - Review OCC129 commit (693293f1) in detail
   - Look for any subsequent changes that may have regressed behavior

### Alternative Approaches

1. **Revert restartClip() to OCC129 commit state**
   - Go back to exact code from 693293f1
   - Compare current implementation line-by-line

2. **Separate the Three Bugs**
   - Treat each as independent issue
   - Fix sequentially rather than assuming shared root cause

3. **Test with Simplified Scenario**
   - Single clip, no other features
   - Remove all fades (set to 0 seconds)
   - Disable multi-voice (force MAX_VOICES_PER_CLIP = 1)

---

## References

- **OCC129:** Clip Button Rapid-Fire Behavior and Fade Distortion - Technical Reference
  - Commit 693293f1: "perfect button behavior, no zigzag distortion"
  - Specifies `hasLoopedOnce = true` in `restartClip()`

- **OCC127:** State Synchronization Architecture - Continuous Polling Pattern
  - 75fps continuous polling (not callback-based)
  - Timer runs continuously when Edit Dialog open

- **PreviewPlayer.cpp:** Edit Dialog playback engine
  - Lines 140-183: `jumpTo()` method (recently fixed for multi-voice)
  - Lines 212-246: Timer callback (75fps position updates)

- **Transport Controller:** `src/core/transport/transport_controller.cpp`
  - Lines 1321-1386: `restartClip()` method
  - Lines 360-378: Fade application logic (checks `hasLoopedOnce`)

---

## Final State

**Code Changes Made:**

- ✅ `transport_controller.cpp:1366` - Changed `hasLoopedOnce = false` to `true`
- ✅ Updated comment to reference OCC129 specification
- ✅ Forced recompile and verified build
- ✅ Relaunched application

**Build Status:** Compiled successfully, no errors

**Runtime Status:** Application launched (PID 2156)

**Bug Status:** ❌ All three bugs still present after fix

**Conclusion:** The `hasLoopedOnce` hypothesis was incorrect, or there are additional factors at play. Further investigation required with systematic debugging approach.

---

**Session End:** 2025-11-15 17:50
**Next Session:** Requires fresh debugging approach with instrumentation/logging
