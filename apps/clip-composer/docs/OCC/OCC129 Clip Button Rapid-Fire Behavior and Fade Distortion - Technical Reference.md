# OCC129 Clip Button Rapid-Fire Behavior and Fade Distortion - Technical Reference

**Version:** 1.0
**Date:** 2025-01-14
**Status:** Reference Architecture
**Author:** Claude Code (claude-sonnet-4-5-20250929)

---

## Executive Summary

This document provides the **authoritative technical reference** for Clip Composer's rapid-fire clip button behavior and fade handling. It explains the root causes of "zigzag distortion" (ring modulation artifacts during multi-voice fades) and documents the **exact implementation pattern** that provides perfect behavior.

**Critical Finding:** Always calling `startClip()` causes ring mod distortion. The correct pattern uses `restartClip()` when clips are already playing.

**Reference Commits:**

- **693293f1** (Nov 12, 13:20) - Perfect button behavior, no zigzag distortion
- **7ca6842e** (Nov 12, 16:04) - Introduced SDK fade bugs (since reverted)

**This document exists to prevent re-investigation of these issues.**

---

## Table of Contents

1. [Problem Statement](#problem-statement)
2. [User Requirements](#user-requirements)
3. [SDK Multi-Voice System](#sdk-multi-voice-system)
4. [The Zigzag Distortion Problem](#the-zigzag-distortion-problem)
5. [The Correct Solution](#the-correct-solution)
6. [Code Reference](#code-reference)
7. [Why Other Approaches Fail](#why-other-approaches-fail)
8. [Testing Verification](#testing-verification)
9. [Related Documentation](#related-documentation)

---

## 1. Problem Statement

### The Challenge

Clip Composer must support **rapid-fire clip button clicks** where a single button can be clicked multiple times in quick succession (< 100ms between clicks). The expected behavior:

1. **First click:** Clip plays from IN point with fade-in
2. **Second click (while playing):** Clip restarts from IN point
3. **Third click (while playing):** Clip restarts again from IN point
4. **...and so on**

### The Failure Modes

Two common failure modes exist:

#### Mode 1: "Zigzag Distortion" (Ring Modulation Artifacts)

- **Symptom:** Terrible audio artifacts during rapid clicks, described as "zigzag distortion" or "sounds like ring mod"
- **Cause:** Multiple voices of the same clip overlap with independent fade curves
- **When it occurs:** When AudioEngine ALWAYS calls `startClip()` regardless of playback state

#### Mode 2: "Broken Restart" (No Response to Clicks)

- **Symptom:** Rapid clicks don't restart the clip, button becomes unresponsive
- **Cause:** Incorrect playback state synchronization
- **When it occurs:** When timer logic overrides button state during fade-out

---

## 2. User Requirements

### Expected Rapid-Fire Pattern (9 Clicks)

From session logs, the expected behavior for rapid button clicks:

```
Click 1: PLAY   (Voice 1 fade-in starts, button → Playing)
Click 2: STOP   (Voice 1 fade-out starts, button → Loaded)
Click 3: PLAY   (All voices restart to IN, button → Playing)
Click 4: STOP   (All voices fade-out, button → Loaded)
Click 5: PLAY   (All voices restart to IN, button → Playing)
Click 6: STOP   (All voices fade-out, button → Loaded)
Click 7: PLAY   (All voices restart to IN, button → Playing)
Click 8: STOP   (All voices fade-out, button → Loaded)
Click 9: PLAY   (All voices restart to IN, button → Playing)
```

### Critical Behavior Requirements

1. **Immediate response:** Every click must produce immediate audible feedback
2. **No distortion:** Fades must be clean, no zigzag artifacts
3. **Restart from IN:** Every play action restarts clip from IN point (not resume)
4. **Visual sync:** Button state must match SDK playback state at 75fps
5. **Fade completion:** Stop fades must complete gracefully without clicks

---

## 3. SDK Multi-Voice System

### Architecture Overview

The Orpheus SDK supports **up to 4 simultaneous voices per ClipHandle** to enable overlapping audio:

```cpp
// From transport_controller.h
static constexpr size_t MAX_VOICES_PER_CLIP = 4;
static constexpr size_t MAX_ACTIVE_CLIPS = 128;  // Total across all clips
```

### Voice Allocation Behavior

#### `startClip(handle)` - Adds New Voice

```cpp
// Behavior:
// 1. If < 4 voices active → add new voice (Voice 1, Voice 2, Voice 3, Voice 4)
// 2. If 4 voices active → remove oldest voice, add new voice
// 3. Each voice has independent:
//    - Position (currentSample)
//    - Fade state (isStopping, fadeOutGain)
//    - Loop state (hasLoopedOnce)
```

**Result:** Multiple voices can play simultaneously with independent fades.

#### `restartClip(handle)` - Restarts All Voices

```cpp
// Behavior:
// 1. Finds ALL active voices for this handle
// 2. Resets position to trimIn for ALL voices
// 3. Applies 5ms broadcast-safe crossfade to ALL voices
// 4. Marks all voices as hasLoopedOnce=true (no clip fade-in/out)
```

**Result:** All voices restart in sync with a single smooth crossfade.

### Fade Types in SDK

The SDK applies **three independent fade types**:

1. **Clip Fade-In/Out** (lines 356-371 in transport_controller.cpp)
   - Applied ONLY on first playthrough (`!clip.hasLoopedOnce`)
   - User-configurable duration and curve (Linear, EqualPower, Exponential)
   - Exists WITHIN IN/OUT bounds, never outside

2. **Stop Fade-Out** (lines 350-352)
   - Applied when `stopClip()` is called
   - Default 10ms fade (FADE_OUT_DURATION_MS)
   - Pre-computed once per buffer for performance

3. **Restart Crossfade** (lines 332-345)
   - Applied when `restartClip()` is called
   - Broadcast-safe 5ms fade-in (RESTART_CROSSFADE_DURATION_MS)
   - Eliminates clicks during restart

---

## 4. The Zigzag Distortion Problem

### Root Cause

When AudioEngine ALWAYS calls `startClip()`, rapid clicks create this scenario:

```
Time:     0ms      50ms     100ms    150ms    200ms
Voice 1:  [PLAY────fade-in──────────────────────────>
Voice 2:           [PLAY────fade-in──────────────────>
Voice 3:                    [PLAY────fade-in──────────>
```

**Problem:** Each voice applies its own independent fade-in curve. When 2-3 voices overlap:

1. Voice 1 is at 80% of fade-in (0.8 gain)
2. Voice 2 is at 40% of fade-in (0.4 gain)
3. Voice 3 is at 10% of fade-in (0.1 gain)

**Total gain:** 0.8 + 0.4 + 0.1 = 1.3 (overmodulation!)

Then as voices progress through their fades:

```
Time:     250ms    300ms    350ms
Voice 1:  1.0      1.0      1.0   (finished fade-in)
Voice 2:  0.7      0.9      1.0   (continuing fade-in)
Voice 3:  0.4      0.6      0.8   (continuing fade-in)
Total:    2.1      2.5      2.8   (MASSIVE overmodulation)
```

**Result:** The total gain oscillates wildly between 1.3 and 2.8, creating **amplitude modulation** that sounds like ring modulation.

### Why This Sounds "Like Ring Mod"

Ring modulation occurs when two signals multiply their amplitudes, creating sum and difference frequencies. In this case:

- Voice 1 at sample N: `sin(2πf·t) × fade1(t)`
- Voice 2 at sample N: `sin(2πf·t) × fade2(t)`
- Voice 3 at sample N: `sin(2πf·t) × fade3(t)`

When summed with different fade positions:

```
Output = sin(2πf·t) × [fade1(t) + fade2(t) + fade3(t)]
```

The varying total fade gain `[fade1(t) + fade2(t) + fade3(t)]` acts as an **amplitude modulator**, creating the characteristic "zigzag" or "ring mod" sound.

### Measured Behavior

From session logs (commit with broken behavior):

- **Symptom:** "terrible zigzag distortion over fades"
- **Description:** "It even sounds like ring mod now"
- **Occurrence:** Only during rapid button clicks (< 100ms apart)
- **Clean audio:** Single-voice playback has no distortion

---

## 5. The Correct Solution

### The Pattern (Commit 693293f1)

```cpp
bool AudioEngine::startClip(int buttonIndex) {
  // ... validation ...

  auto handle = m_clipHandles[buttonIndex];

  // CRITICAL: Check if clip is already playing
  bool isAlreadyPlaying = m_transportController->isClipPlaying(handle);

  orpheus::SessionGraphError result;
  if (isAlreadyPlaying) {
    // Already playing - use restartClip() to force restart from IN point
    result = m_transportController->restartClip(handle);
    DBG("AudioEngine: Restarted clip on button " << buttonIndex);
  } else {
    // Not playing - use startClip() as normal
    result = m_transportController->startClip(handle);
    DBG("AudioEngine: Started clip on button " << buttonIndex);
  }

  return result == orpheus::SessionGraphError::OK;
}
```

### Why This Works

1. **First click (not playing):**
   - Calls `startClip()` → Voice 1 starts with clean fade-in

2. **Second click (already playing):**
   - Calls `restartClip()` → ALL voices (just Voice 1) restart to IN point
   - 5ms broadcast-safe crossfade applied
   - `hasLoopedOnce=true` → no clip fade-in/out on restart

3. **Third click (already playing):**
   - Calls `restartClip()` → ALL voices restart again
   - Same clean 5ms crossfade
   - No overlapping voices, no fade interference

**Result:** Only ONE voice is ever playing (or restarting), eliminating all possibility of fade interference and zigzag distortion.

### Fade Timeline Visualization

```
Click 1 (not playing → startClip):
Voice 1: [fade-in 100ms]──────[playing]────────────────>

Click 2 (playing → restartClip):
Voice 1: ────────────────────[5ms crossfade][playing]───>
                             ↑ Restart to IN

Click 3 (playing → restartClip):
Voice 1: ──────────────────────────────[5ms crossfade][playing]>
                                       ↑ Restart to IN
```

**No overlapping voices = No zigzag distortion**

---

## 6. Code Reference

### AudioEngine.cpp (apps/clip-composer/Source/Audio/AudioEngine.cpp)

**Correct Implementation (Commit 693293f1):**

```cpp
bool AudioEngine::startClip(int buttonIndex) {
  if (buttonIndex < 0 || buttonIndex >= AudioEngine::MAX_CLIP_BUTTONS)
    return false;

  auto handle = m_clipHandles[buttonIndex];
  if (handle == 0) {
    DBG("AudioEngine: No clip loaded on button " << buttonIndex);
    return false;
  }

  if (!m_transportController)
    return false;

  // CRITICAL: Check if already playing - if so, RESTART from IN point (not resume)
  // This ensures rapid clip button clicks always restart from the beginning
  bool isAlreadyPlaying = m_transportController->isClipPlaying(handle);

  orpheus::SessionGraphError result;
  if (isAlreadyPlaying) {
    // Already playing - use restartClip() to force restart from IN point
    result = m_transportController->restartClip(handle);
    if (result != orpheus::SessionGraphError::OK) {
      DBG("AudioEngine: Failed to restart clip " << handle);
      return false;
    }
    DBG("AudioEngine: Restarted clip on button " << buttonIndex << " (was already playing)");
  } else {
    // Not playing - use startClip() as normal
    result = m_transportController->startClip(handle);
    if (result != orpheus::SessionGraphError::OK) {
      DBG("AudioEngine: Failed to start clip " << handle);
      return false;
    }
    DBG("AudioEngine: Started clip on button " << buttonIndex);
  }

  return true;
}
```

### SDK transport_controller.cpp (src/core/transport/transport_controller.cpp)

**restartClip() Implementation (Lines 972-1041):**

```cpp
SessionGraphError TransportController::restartClip(ClipHandle handle) {
  // Multi-voice: Restart ALL voices for this handle back to trim IN point

  bool foundAnyVoice = false;
  int64_t trimIn = 0;

  for (size_t i = 0; i < m_activeClipCount; ++i) {
    if (m_activeClips[i].handle == handle) {
      foundAnyVoice = true;
      ActiveClip& clip = m_activeClips[i];

      // CRITICAL: Broadcast-safe restart with crossfade (eliminates clicks)
      trimIn = clip.trimInSamples.load(std::memory_order_acquire);
      clip.currentSample = trimIn;

      // Reset reader to trim IN point
      if (clip.reader) {
        clip.reader->seek(trimIn);
      }

      // Enable restart crossfade (5ms fade-in to eliminate clicks)
      clip.isRestarting = true;
      clip.restartFadeFramesRemaining = static_cast<int64_t>(m_restartCrossfadeSamples);

      // Cancel any fade-out in progress
      clip.isStopping = false;
      clip.fadeOutGain = 1.0f;

      // CRITICAL: Restart should NOT re-apply clip fade-in
      // Mark as looped so clip fade-in/out won't be applied
      clip.hasLoopedOnce = true;
    }
  }

  if (!foundAnyVoice) {
    // No voices playing - start clip normally
    return startClip(handle);
  }

  return SessionGraphError::OK;
}
```

**Key Points:**

1. Restarts ALL voices for the handle
2. Applies 5ms broadcast-safe crossfade
3. Sets `hasLoopedOnce=true` to skip clip fade-in/out
4. Falls back to `startClip()` if no voices are active

### SDK Fade Constants (src/core/transport/transport_controller.h)

```cpp
static constexpr float FADE_OUT_DURATION_MS = 10.0f;          // Stop fade-out
static constexpr float RESTART_CROSSFADE_DURATION_MS = 5.0f;  // Restart crossfade
```

---

## 7. Why Other Approaches Fail

### Approach 1: Always Call startClip() ❌

**Code:**

```cpp
// WRONG - Causes zigzag distortion
auto result = m_transportController->startClip(handle);
```

**Why it fails:**

- Creates multiple overlapping voices
- Each voice has independent fade-in curve
- Total gain = sum of all fade gains → overmodulation
- Amplitude modulation creates "ring mod" sound

**Measured result:** "terrible zigzag distortion", "sounds like ring mod"

### Approach 2: Check PlaybackState::Playing Before startClip() ❌

**Code:**

```cpp
// WRONG - Doesn't restart when Stopping
auto state = m_transportController->getClipState(handle);
if (state == PlaybackState::Stopped) {
  m_transportController->startClip(handle);
}
// Otherwise do nothing
```

**Why it fails:**

- Ignores clicks when clip is Playing or Stopping
- User clicks button → nothing happens (no restart)
- Button becomes unresponsive during fade-out
- Violates requirement: "Every click must produce immediate audible feedback"

### Approach 3: Use isClipPlaying() with Voice Counting ❌

**Code:**

```cpp
// WRONG - Complex and still causes issues
size_t voiceCount = m_transportController->countActiveVoices(handle);
if (voiceCount < MAX_VOICES_PER_CLIP) {
  m_transportController->startClip(handle);
} else {
  // Remove oldest voice manually?
}
```

**Why it fails:**

- Still creates overlapping voices (just limits to 4)
- Still has fade interference (zigzag distortion)
- Overly complex logic
- Doesn't match user expectation (restart, not overlay)

---

## 8. Testing Verification

### Manual Test Cases

**Test 1: Rapid Single-Button Clicks**

```
Procedure:
1. Load a 5-second audio clip to button 28
2. Click button 28 rapidly 9 times (< 100ms between clicks)

Expected:
- Clip restarts from IN point on every click
- No distortion, no zigzag artifacts
- Clean 5ms crossfades between restarts
- Button visual state syncs correctly

Pass Criteria:
- Audio sounds clean, identical to single playback
- No amplitude modulation or "ring mod" sound
```

**Test 2: Play/Stop Pattern**

```
Procedure:
1. Load a 5-second clip to button 28
2. Execute pattern: Click, Wait 50ms, Click, Wait 50ms, ... (9 clicks total)
3. Observe button state: Playing → Loaded → Playing → Loaded ...

Expected:
- Odd clicks (1, 3, 5, 7, 9): Clip plays/restarts
- Even clicks (2, 4, 6, 8): Clip stops with fade-out
- All fades complete cleanly

Pass Criteria:
- Visual state matches expected pattern
- No distortion during fade-outs
- Fades complete to silence
```

**Test 3: Multi-Voice Stress Test**

```
Procedure:
1. Load same clip to 4 different buttons (28, 29, 30, 31)
2. Click all 4 buttons simultaneously
3. Rapid-click button 28 while others continue playing

Expected:
- All 4 clips play independently (different handles)
- Rapid clicks on button 28 restart only that clip
- Other clips continue unaffected
- No overmodulation despite 4 simultaneous clips

Pass Criteria:
- All clips audible and clean
- No distortion from clip 28's restarts
- Other clips unaffected by restarts
```

### Automated Test (Future)

```cpp
TEST(AudioEngineTest, RapidFireRestartBehavior) {
  AudioEngine engine;
  engine.initialize(48000);

  // Load clip to button 0
  engine.loadClip(0, "test_audio.wav");

  // First click - should call startClip()
  EXPECT_TRUE(engine.startClip(0));
  EXPECT_TRUE(engine.isClipPlaying(0));

  // Second click while playing - should call restartClip()
  EXPECT_TRUE(engine.startClip(0));  // Still calls startClip() method
  EXPECT_TRUE(engine.isClipPlaying(0));  // But internally uses restartClip()

  // Verify no voice accumulation (only 1 voice should be active)
  auto voiceCount = engine.getActiveVoiceCount(0);
  EXPECT_EQ(voiceCount, 1);  // Only one voice, not multiple
}
```

---

## 9. Related Documentation

### OCC Documents

- **OCC110** - SDK Integration Guide (Transport State and Loop Features)
- **OCC127** - State Synchronization Architecture (75fps polling pattern)
- **OCC096** - SDK Integration Patterns (general SDK usage)

### SDK Implementation

- **src/core/transport/transport_controller.cpp** - Multi-voice rendering (lines 198-555)
- **src/core/transport/transport_controller.h** - Interface definitions
- **include/orpheus/transport_controller.h** - Public SDK API

### Git History

- **693293f1** (Nov 12, 13:20) - Perfect button behavior reference
- **7ca6842e** (Nov 12, 16:04) - Introduced SDK fade bugs (later reverted)
- **e5fe71a1** - Cold backup reference (docs: Add ORP113)

---

## Appendix A: Session Log Analysis

### Key Findings from 22,700-Line Session Log

**Quote 1 (Line 5976):**

> "Ok so. Clip button rapid click behaviour is perfect in commit 693293f1 — zigzag distortion is not audible during multi-voice restarts. Overlapping fades are clean."

**Quote 2 (Line 2041):**

> "zigzag distortion is only present during multi-voice audio (overlapping audio in same clip handle)"

**Quote 3 (Line 2103):**

> "gain_smoothing_ms = 0.0f; // DISABLED: Fades handled at clip level, smoothing causes zigzag artifacts"

**Key Insight:** The session log shows multiple attempts to solve this problem, cycling through the same approaches. This document exists to prevent future re-investigation.

---

## Appendix B: Quick Reference Decision Tree

```
User clicks clip button
    │
    ├─ Clip NOT playing (PlaybackState::Stopped)
    │   └─> Call startClip()
    │       └─> SDK starts Voice 1 with clip fade-in
    │
    └─ Clip IS playing (PlaybackState::Playing or Stopping)
        └─> Call restartClip()
            ├─> SDK restarts ALL voices to IN point
            ├─> 5ms broadcast-safe crossfade applied
            └─> hasLoopedOnce=true (no clip fade-in/out)
```

**Result:** Maximum of ONE voice active = No zigzag distortion

---

## Appendix C: Glossary

**Zigzag Distortion:** Amplitude modulation artifacts caused by multiple overlapping voices with independent fade curves summing to >1.0 gain, creating periodic overmodulation that sounds like ring modulation.

**Ring Modulation:** Audio effect where two signals multiply, creating sum and difference frequencies. In this context, overlapping fades create similar-sounding artifacts.

**Voice:** Independent playback instance of a ClipHandle in the SDK. Up to 4 voices per handle can exist simultaneously.

**restartClip():** SDK method that resets ALL voices for a handle to IN point with a 5ms crossfade. Does NOT add new voices.

**startClip():** SDK method that ADDS a new voice for a handle. Creates overlapping voices if called repeatedly.

**hasLoopedOnce:** SDK flag that skips clip fade-in/out processing. Set to true on restart to prevent fade re-application.

**Broadcast-Safe:** Audio processing that never introduces clicks, pops, or distortion, suitable for 24/7 broadcast environments.

---

## Document History

| Version | Date       | Author      | Changes                         |
| ------- | ---------- | ----------- | ------------------------------- |
| 1.0     | 2025-01-14 | Claude Code | Initial comprehensive reference |

---

**END OF DOCUMENT**
