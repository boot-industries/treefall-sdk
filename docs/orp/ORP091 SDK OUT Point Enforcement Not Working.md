# ORP091: SDK OUT Point Enforcement Not Working (Non-Loop Mode Looping)

**Date:** October 27, 2025
**Severity:** CRITICAL
**Status:** 🔴 BLOCKING OCC v0.3.0 Release
**Reporter:** OCC Team (Chris Lyons)
**Related:** ORP089, ORP090

---

## Executive Summary

SDK's OUT point enforcement documented in ORP089/ORP090 is **NOT working**. Non-loop mode clips are **looping back to position 0** instead of stopping at OUT point. This violates fundamental edit laws and blocks OCC v0.3.0 release.

---

## Problem Description

### Expected Behavior (per ORP090)

**Non-Loop Mode:**

```
Clip plays: IN → OUT → STOP (with fade-out if configured)
```

**Loop Mode:**

```
Clip plays: IN → OUT → restart at IN (seamless loop)
```

### Actual Behavior (Observed in OCC)

**Non-Loop Mode (BROKEN):**

```
Clip plays: IN → OUT → position 0 (illegal loop)
```

**Loop Mode:**

```
Clip plays: IN → OUT → restart at IN (WORKS correctly)
```

---

## Evidence from Logs

### Test Case: Non-Loop Mode Playback

**Setup:**

- Trim: IN=444836, OUT=1389137
- Loop: **DISABLED** (Loop=NO)
- Expected: Stop at OUT point

**Log Evidence:**

```
AudioEngine: Set Cue Buss 10001 loop mode to disabled
PreviewPlayer: Loop disabled
PreviewPlayer: Pos=1341552, OUT=1389137, Loop=NO (tick 640)
PreviewPlayer: Pos=1348720, OUT=1389137, Loop=NO (tick 650)
PreviewPlayer: Pos=1355888, OUT=1389137, Loop=NO (tick 660)
PreviewPlayer: Pos=1362800, OUT=1389137, Loop=NO (tick 670)
PreviewPlayer: Pos=1369968, OUT=1389137, Loop=NO (tick 680)
PreviewPlayer: Pos=1376624, OUT=1389137, Loop=NO (tick 690)
PreviewPlayer: Pos=1383536, OUT=1389137, Loop=NO (tick 700)  ← Near OUT point
PreviewPlayer: Playhead at 0 escaped below IN point 444836    ← SDK looped to 0!
```

**Analysis:**

- Position 1383536 is ~5600 samples from OUT (1389137)
- Next position should be ≥1390000 (past OUT), triggering SDK stop
- Instead, position went to **0** (illegal loop behavior)
- UI detected violation and restarted from IN (workaround)

---

## Root Cause Hypothesis

### SDK Code Path (from ORP090 documentation)

**Location:** `src/core/transport/transport_controller.cpp:436-488`

```cpp
// Check if clip reached trim OUT point
int64_t clipTrimOut = clip.trimOutSamples.load(std::memory_order_acquire);
if (clip.currentSample >= clipTrimOut) {
  bool shouldLoop = clip.loopEnabled.load(std::memory_order_acquire);

  if (shouldLoop) {
    // Loop mode: seek back to IN
    int64_t trimIn = clip.trimInSamples.load(std::memory_order_acquire);
    clip.reader->seek(trimIn);
    clip.currentSample = trimIn;
    // Post loop callback
  } else {
    // Non-loop mode: Stop at OUT point
    // ← THIS CODE PATH IS NOT EXECUTING!
  }
}
```

**Possible Issues:**

1. `clip.loopEnabled` not being set correctly by `setClipLoop()`
2. OUT point check `if (clip.currentSample >= clipTrimOut)` not triggering
3. Non-loop stop logic not implemented/has bugs
4. Loop mode defaulting to true when it should be false

---

## Integration Status

### OCC Side (✅ COMPLETE)

1. **Loop Mode Setting:**

   ```cpp
   // PreviewPlayer.cpp:138-140
   void PreviewPlayer::setLoopEnabled(bool shouldLoop) {
     m_loopEnabled = shouldLoop;
     if (m_cueBussHandle != 0 && m_audioEngine) {
       m_audioEngine->setCueBussLoop(m_cueBussHandle, shouldLoop);  ← Calls SDK
     }
   }
   ```

2. **AudioEngine Wrapper:**

   ```cpp
   // AudioEngine.cpp:688-698
   void AudioEngine::setCueBussLoop(orpheus::ClipHandle cueBussHandle, bool enabled) {
     if (cueBussHandle < 10001 || !m_transportController)
       return;

     m_transportController->setClipLoop(cueBussHandle, enabled);  ← SDK API call
     DBG("AudioEngine: Set Cue Buss " << cueBussHandle << " loop mode to "
         << (enabled ? "enabled" : "disabled"));
   }
   ```

3. **Verified Log Output:**
   ```
   AudioEngine: Set Cue Buss 10001 loop mode to disabled  ← OCC called SDK correctly
   PreviewPlayer: Loop disabled
   ```

**Conclusion:** OCC is correctly calling SDK's `setClipLoop(handle, false)`.

---

### SDK Side (🔴 NOT WORKING)

**API Used:** `ITransportController::setClipLoop(ClipHandle handle, bool enabled)`

**Expected Result:**

- `enabled=true` → Loop mode (restart at IN when reaching OUT)
- `enabled=false` → Non-loop mode (stop at OUT)

**Actual Result:**

- `enabled=true` → Works correctly (loops back to IN)
- `enabled=false` → **FAILS** (loops to position 0 instead of stopping)

---

## Debugging Questions for SDK Team

### Question 1: Is setClipLoop() Actually Setting the Flag?

**Check:**

```cpp
SessionGraphError TransportController::setClipLoop(ClipHandle handle, bool enabled) {
  // Is clip.loopEnabled being updated correctly?
  // Is it atomic? Thread-safe?
  clip.loopEnabled.store(enabled, std::memory_order_release);
}
```

**Test:** Add debug logging to confirm flag is set:

```cpp
DBG("SDK: setClipLoop(" << handle << ", " << enabled << ") - loopEnabled="
    << clip.loopEnabled.load());
```

---

### Question 2: Is OUT Point Check Even Triggering?

**Check:**

```cpp
// In processAudio() callback
int64_t clipTrimOut = clip.trimOutSamples.load(std::memory_order_acquire);
if (clip.currentSample >= clipTrimOut) {
  // Is this code path executing at all?
  DBG("SDK: Clip " << clip.handle << " reached OUT point " << clipTrimOut
      << " at position " << clip.currentSample);

  bool shouldLoop = clip.loopEnabled.load(std::memory_order_acquire);
  DBG("SDK: Loop mode = " << shouldLoop);
}
```

---

### Question 3: What Happens in Non-Loop Stop Path?

**Check:**

```cpp
// Lines 458-488 (non-loop mode stop logic)
else {
  // Non-loop mode: Stop at OUT point
  // What code is here? Is it implemented?
  // Is it removing the clip correctly?
  // Is it calling onClipStopped callback?
}
```

**Expected Behavior:**

1. Fade out if configured (apply fade-out envelope for last N samples)
2. Mark clip as stopped
3. Remove from active clips
4. Post `onClipStopped` callback to UI thread

---

### Question 4: Why Position 0 After OUT?

**Hypothesis:** Audio file reader is looping back to start when reaching end of file, **BEFORE** SDK OUT point check happens.

**Check:**

```cpp
// Is the audio file reader wrapping to position 0?
// Does the reader respect trim points?
// Is there a separate file-level loop happening?
```

---

## Minimal Reproduction Test

```cpp
// SDK Test Case (add to tests/transport/out_point_enforcement_test.cpp)

TEST(OutPointEnforcementTest, NonLoopModeDoesNotLoop) {
  auto controller = createController();

  // Create clip with OUT point at 1 second
  ClipHandle handle = controller->addClip("test.wav");
  controller->setClipMetadata(handle, {
    .trimInSamples = 0,
    .trimOutSamples = 48000,  // 1 second @ 48kHz
    .loopEnabled = false       // DISABLE loop
  });

  // Start playback
  controller->startClip(handle);

  // Process 1.1 seconds of audio
  for (int i = 0; i < 55; ++i) {  // 55 buffers × 512 samples = 28160 samples > OUT
    controller->processAudio(outputs, 2, 512);
  }

  // Verify clip is STOPPED (not looping)
  int64_t position = controller->getClipPosition(handle);
  EXPECT_EQ(position, -1);  // -1 means clip is stopped

  // Verify onClipStopped callback was fired
  EXPECT_EQ(callbackTester.stoppedCount, 1);
  EXPECT_EQ(callbackTester.loopedCount, 0);  // Should NOT have looped
}
```

**Expected:** ✅ PASS
**Actual:** ❌ FAIL (clip loops to position 0 instead of stopping)

---

## Impact on OCC

### Current Workaround (Fragile)

OCC has re-added UI-layer OUT point enforcement to compensate for broken SDK:

```cpp
// PreviewPlayer::timerCallback() - Line 335-347
if (currentPos < m_trimInSamples) {
  if (m_ticksSinceRestart > RESTART_GRACE_TICKS) {
    // SDK looped to 0 illegally - force restart from IN
    DBG("PreviewPlayer: Playhead at " << currentPos
        << " escaped below IN point - FORCING restart");
    play();  // Workaround: restart when SDK misbehaves
  }
}
```

**Problems:**

1. **Not sample-accurate** - UI polls at 75 FPS (~13ms granularity), SDK should enforce at buffer level (~10ms)
2. **Race conditions** - UI thread reads position while audio thread updates it
3. **Architectural violation** - UI shouldn't enforce audio rules
4. **User-visible glitch** - Playhead briefly escapes bounds before UI catches it

---

### User-Visible Symptoms

1. **Non-loop clips loop unexpectedly** - Violates edit laws, confuses users
2. **Playhead jumps to position 0** - Visual glitch in waveform display
3. **Unexpected restart from IN** - UI workaround triggers, but feels like a bug

---

## Requested SDK Fix

### Priority: CRITICAL (P0)

**Blocks:** OCC v0.3.0 release
**Affects:** All applications using SDK's OUT point enforcement (not just OCC)

### Required Changes

1. **Verify `setClipLoop()` correctly sets `clip.loopEnabled` flag**
2. **Add debug logging to OUT point enforcement code path**
3. **Implement/fix non-loop stop logic:**
   - Apply fade-out if configured
   - Mark clip as stopped
   - Remove from active clips
   - Post `onClipStopped` callback
4. **Add unit test for non-loop mode (see Minimal Reproduction above)**
5. **Verify audio file reader doesn't auto-loop at file end**

---

## Timeline

**Reported:** October 27, 2025 (immediately after ORP089 integration)
**Deadline:** ASAP (blocking OCC v0.3.0)
**Estimated Fix Time:** 2-4 hours (assuming simple flag/logic bug)

---

## Verification Plan

Once SDK fix is available:

1. **Run SDK unit test:** `out_point_enforcement_test::NonLoopModeDoesNotLoop` → must PASS
2. **Manual OCC test:**
   - Load clip in Edit Dialog
   - Set OUT point to 5 seconds
   - Disable loop mode
   - Play to completion
   - **Expected:** Playback stops at OUT (no loop, no position 0)
3. **Check logs:** No "escaped below IN point" messages
4. **Verify loop mode still works:** Enable loop → should restart at IN (not position 0)

---

## References

[1] ORP089 - OUT Point Enforcement Implementation
[2] ORP090 - ORP089 Sprint Summary (Loop Mode Verification)
[3] `src/core/transport/transport_controller.cpp:436-488` - OUT point enforcement code
[4] `apps/clip-composer/Source/UI/PreviewPlayer.cpp:335-347` - OCC workaround
[5] SDK_POSITION_TRACKING.md - Position tracking best practices

---

## Attachments

**Log Evidence:** `/tmp/occ.log` (lines showing position 0 after OUT point)
**OCC Integration Code:** `AudioEngine::setCueBussLoop()`, `PreviewPlayer::setLoopEnabled()`
**Suggested Test Case:** `NonLoopModeDoesNotLoop` (above)

---

**Submitted By:** OCC Team (Chris Lyons)
**Assigned To:** SDK Core Team
**Sprint:** ORP091 - OUT Point Enforcement Fix
**Related Issues:** ORP089, ORP090

---

🔴 **CRITICAL:** This bug blocks OCC v0.3.0 release. Non-loop mode MUST stop at OUT point, not loop to position 0.
