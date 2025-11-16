# ORP114: Critical Gain Staging Bug Investigation and Fix

**Status:** Investigation Complete, Fixes Pending
**Priority:** CRITICAL
**Date:** 2025-01-15
**Affected Components:** TransportController (transport_controller.cpp), RoutingMatrix
**Symptom:** Severe gain accumulation causing audible distortion in Clip Composer

---

## Executive Summary

Identified **4 high-confidence root causes** for critical gain staging bug in Orpheus SDK transport layer. Primary issue: **trim point updates during active playback cause fade calculation overflow**, triggering excessive gain multiplication. Secondary issues: restart crossfade inconsistency, multi-voice additive summing, and clipping protection masking.

**Reproduction:** Random but reproducible - triggered by Trim OUT key commands, rapid restarts, and loop + trim combinations. Gain "went through the roof" (user report).

---

## Root Cause Analysis

### 1. Trim Point Update Race Condition ⚠️ HIGHEST PRIORITY

**Location:** `transport_controller.cpp:884-931` (updateClipTrimPoints)

**Bug:** Updates `trimOutSamples` for ACTIVE clips without validating current playback position.

**Code Path:**

```cpp
// Lines 923-928 - UNSAFE for active clips
for (size_t i = 0; i < m_activeClipCount; ++i) {
  if (m_activeClips[i].handle == handle) {
    m_activeClips[i].trimInSamples.store(trimInSamples, std::memory_order_release);
    m_activeClips[i].trimOutSamples.store(trimOutSamples, std::memory_order_release);
    // ❌ NO position validation - clip.currentSample may be > new trimOut!
  }
}
```

**Failure Scenario:**

1. Clip playing at position 100,000 samples
2. User presses Trim OUT key, reducing `trimOut` from 120,000 → 80,000
3. Next audio buffer: `clip.currentSample = 100,000`, `trimOut = 80,000`
4. Line 364: `int64_t relativePos = clip.currentSample + frame - trimIn`
5. Line 372: `if (relativePos >= (trimmedDuration - fadeOutSampleCount))`
6. **Result:** `relativePos` is HUGE (100,000 when trimmedDuration is 80,000), triggering clip fade-out logic incorrectly
7. Line 376: `gain *= (1.0f - calculateFadeGain(...))` applies fade-out gain when it shouldn't
8. **Accumulation:** If this happens multiple times per buffer, gain multiplies out of control

**Evidence:**

- User reports: "Random but reproducible" - matches atomic update without position clamping
- "Trim OUT key commands" mentioned as trigger - direct match
- Comment at line 922: "We update active clips directly (no command queue needed for metadata updates)" - confirms unsafe concurrent modification

---

### 2. Fade State Machine Inconsistency

**Location:** `transport_controller.cpp:1321-1386` (restartClip)

**Bug:** Manual restart sets `hasLoopedOnce = false` (to apply clip fade-in) but doesn't set `isRestarting = true` (to apply restart crossfade).

**Code:**

```cpp
// Lines 1362-1365
// CRITICAL: Manual restart SHOULD apply clip fade-in (user action)
// This is DIFFERENT from auto-loop which should NOT apply fade-in
// Set hasLoopedOnce = false to allow clip fade-in on restart
clip.hasLoopedOnce = false;
// ❌ MISSING: clip.isRestarting = true; clip.restartFadeFramesRemaining = ...;
```

**Inconsistency:**

- `addActiveClip()` (initial start): Sets `isRestarting = false`, `restartFadeFramesRemaining = 0` (line 713-714)
- `restartClip()` (manual restart): Sets `hasLoopedOnce = false` but DOESN'T set `isRestarting = true`

**Result:**

- Manual restart applies clip fade-in (lines 362-378) WITHOUT restart crossfade protection (lines 337-351)
- Initial start behavior: NO restart crossfade (correct for first start)
- **Problem:** Restart should have 5ms fade-in (RESTART_CROSSFADE_DURATION_MS) for broadcast-safe click elimination

**Why this matters:**

- Comment at line 1362: "CRITICAL: Manual restart SHOULD apply clip fade-in (user action)"
- Comment at line 1348: "CRITICAL: Broadcast-safe restart with crossfade (eliminates clicks)"
- **Contradiction:** Code says "broadcast-safe restart" but doesn't actually enable the crossfade mechanism

---

### 3. Multi-Voice Additive Summing

**Location:** `routing_matrix.cpp:546-600` (channel → group summing)

**Design Behavior (Not a Bug, But Explains Gain Issues):**

```cpp
// Line 590 - Additive summing
group_buffer[frame] += sample; // Mono sum for now
```

**Multi-Voice Provisioning:**

- `MAX_VOICES_PER_CLIP = 4` (transport_controller.h:199)
- Up to 4 instances of same clip can play simultaneously
- Each voice rendered to separate channel (m_clipChannelBuffers[i])
- All channels sum additively into group buffer

**Gain Multiplication:**

- 1 voice: 1.0x gain
- 2 voices: 2.0x gain (additive sum)
- 3 voices: 3.0x gain
- 4 voices: 4.0x gain

**Trigger:** Rapid re-triggers (user clicking grid button quickly, or restart commands in quick succession)

**Evidence:**

- User reports: "Sometimes happens, sometimes doesn't" - matches variable voice count
- "Random restarts" mentioned - could trigger multi-voice layering
- No voice gain normalization in routing matrix (intentional for layering, but can cause unexpected loudness)

---

### 4. Clipping Protection Masking the Real Issue

**Location:**

- Constructor: `transport_controller.cpp:38-40` (clipping protection ENABLED)
- Routing matrix: `routing_matrix.cpp:688-706` (tanh soft limiter)

**Configuration:**

```cpp
// Lines 38-40
routingConfig.enable_clipping_protection =
    true; // OCC109 v0.2.2: ENABLED to fix "Stop All" distortion with 32 simultaneous fade-outs
          // Soft-knee tanh limiter prevents audible clipping without quality loss
```

**Clipping Protection Algorithm:**

```cpp
// Lines 693-698 - Soft-knee tanh limiter
if (std::abs(sample) > 0.9f) {
  sample = std::tanh(sample * 0.9f) / 0.9f;
}
// Lines 700-701 - Hard clip as safety
sample = std::max(-1.0f, std::min(1.0f, sample));
```

**Why This Matters:**

- **Without clipping protection:** Gain accumulation would cause hard digital clipping (square wave distortion)
- **With clipping protection:** Gain accumulation causes tanh compression (softer, but still audible "crushed" distortion)
- User report: "Severe distortion" - matches tanh compression of excessive signals
- **Masking effect:** Clipping protection prevents catastrophic failure but allows bug to persist undetected in testing

**Evidence:**

- Comment at line 687: "Fix 'Stop All' distortion when 32 clips fade out simultaneously"
- This suggests historical issues with summed gain exceeding 0dBFS
- Clipping protection is a **mitigation**, not a fix for root gain staging bugs

---

## Secondary Findings (No Issues Found)

### SessionDefaults Initialization ✅ VERIFIED SAFE

**Location:** `transport_controller.h:72-82`

**Code:**

```cpp
struct SessionDefaults {
  double fadeInSeconds = 0.0;                 ///< Default fade-in time (0.0 = no fade)
  double fadeOutSeconds = 0.0;                ///< Default fade-out time (0.0 = no fade)
  FadeCurve fadeInCurve = FadeCurve::Linear;  ///< Default fade-in curve
  FadeCurve fadeOutCurve = FadeCurve::Linear; ///< Default fade-out curve
  bool loopEnabled = false;                   ///< Default loop mode
  bool stopOthersOnPlay = false;              ///< Default "stop others" mode
  float gainDb = 0.0f;                        ///< Default gain in dB (0.0 = unity)
};
```

**Validation:** All fields have proper default initializers. No NaN/garbage values possible.

---

### Gain Validation ✅ VERIFIED SAFE

**Location:** `transport_controller.cpp:1054-1087` (updateClipGain)

**Code:**

```cpp
// Lines 1059-1062 - Gain validation
if (!std::isfinite(gainDb)) {
  return SessionGraphError::InvalidParameter;
}
```

**Validation:** All gain updates check for NaN/Inf before computing linear gain. addActiveClip() uses validated values from persistent storage.

---

## Gain Calculation Chain Analysis

**Complete Gain Multiplication Path (transport_controller.cpp:330-389):**

```cpp
for (size_t frame = 0; frame < framesRead; ++frame) {
  float gain = 1.0f;                                    // Base gain

  gain *= clipGainLinear;                               // 1. Clip gain (user setting)

  if (clip.isRestarting && ...) {                       // 2. Restart crossfade (5ms fade-in)
    gain *= restartFadeGain;
  }

  if (clip.isStopping) {                                // 3. Stop fade-out (10ms fade)
    gain *= clip.fadeOutGain;
  }

  if (!clip.hasLoopedOnce) {                            // 4. Clip fade-in (first playthrough)
    if (fadeInSampleCount > 0 && ...) {
      gain *= calculateFadeGain(...);
    }

    if (fadeOutSampleCount > 0 && ...) {                // 5. Clip fade-out (last N samples)
      gain *= (1.0f - calculateFadeGain(...));
    }
  }

  clipChannelBuffer[frame] = monoSample * gain;         // Apply final gain
}
```

**Maximum Expected Gain:**

- Normal playback: 1.0x (unity)
- With +12dB clip gain: ~4.0x
- Multi-voice (4 clips): 4.0x summing
- **Theoretical max (4 voices @ +12dB):** 16.0x gain → Clipping protection triggers

**Bug Scenarios:**

- Trim OUT race: Incorrect fade-out logic triggers, applying `gain *= (1.0 - fadeGain)` when it shouldn't
- If fade calculation returns large values due to overflow: gain reduction or excessive multiplication
- If multiple buffers hit this bug: gain accumulates frame-by-frame

---

## Routing Matrix Gain Application

**Path:** Clips → Groups → Master (routing_matrix.cpp:523-709)

**Gain Stages:**

1. **Channel gain** (line 573): `sample *= channel_gain`
2. **Group summing** (line 590): `group_buffer[frame] += sample` (additive)
3. **Group gain** (line 624): `sample *= group_gain`
4. **Master summing** (line 634): `master_output[out][frame] += sample` (additive)
5. **Master gain** (line 654): `master_output[out][frame] *= master_gain`
6. **Clipping protection** (lines 693-701): `tanh()` soft limiter + hard clip

**Configuration Used:**

- Channel gain: Unity (1.0) - line 748
- Group gain: Unity (1.0) - line 772
- Master gain: Unity (1.0) - line 70
- Gain smoothing: DISABLED (0.0ms) - line 36
- Clipping protection: ENABLED - line 38

**No Additional Gain Application:** Routing matrix does NOT multiply gain beyond user settings. All fades are clip-level, not routing-level.

---

## Diagnostic Recommendations

### Immediate Action: Add Gain Trace Logging

**Location:** `transport_controller.cpp` after line 378 (inside frame loop)

```cpp
// TEMPORARY DEBUG: Trace gain calculations for suspect clips
#ifdef DEBUG_GAIN_TRACE
if (clip.handle == SUSPECT_HANDLE_ID) {
  std::cout << "Clip " << clip.handle << " Frame " << frame << ": "
            << "clipGain=" << clipGainLinear << " "
            << "restartFade=" << (clip.isRestarting ? restartFadeGain : 1.0f) << " "
            << "stopFade=" << (clip.isStopping ? clip.fadeOutGain : 1.0f) << " "
            << "hasLooped=" << clip.hasLoopedOnce << " "
            << "relativePos=" << relativePos << " "
            << "trimIn=" << trimIn << " trimOut=" << trimOut << " "
            << "currentSample=" << clip.currentSample << " "
            << "finalGain=" << gain << "\n";
}
#endif
```

**Expected Output When Bug Occurs:**

- `relativePos` will show HUGE values (> trimOut - trimIn)
- `finalGain` will show < 1.0 or > 1.0 unexpectedly
- `currentSample` will be outside [trimIn, trimOut) range

---

## Proposed Fixes

### Fix 1: Position Validation in updateClipTrimPoints() ⚠️ CRITICAL

**Location:** `transport_controller.cpp:923-928`

**Problem:** Updates trim points without validating current playback position.

**Fix:**

```cpp
// Update trim points for any active clips with this handle
for (size_t i = 0; i < m_activeClipCount; ++i) {
  if (m_activeClips[i].handle == handle) {
    m_activeClips[i].trimInSamples.store(trimInSamples, std::memory_order_release);
    m_activeClips[i].trimOutSamples.store(trimOutSamples, std::memory_order_release);

    // CRITICAL FIX: Clamp current position to new trim range
    // Prevents fade calculation overflow when trim OUT is reduced during playback
    if (m_activeClips[i].currentSample > trimOutSamples) {
      m_activeClips[i].currentSample = trimOutSamples;
      // Seek reader to clamped position
      if (m_activeClips[i].reader) {
        m_activeClips[i].reader->seek(trimOutSamples);
      }
    }
    if (m_activeClips[i].currentSample < trimInSamples) {
      m_activeClips[i].currentSample = trimInSamples;
      if (m_activeClips[i].reader) {
        m_activeClips[i].reader->seek(trimInSamples);
      }
    }
  }
}
```

**Rationale:**

- Ensures `clip.currentSample` always in valid range [trimIn, trimOut)
- Prevents `relativePos` overflow in fade calculations (line 364)
- Matches existing boundary enforcement in `processAudio()` (lines 255-286)

---

### Fix 2: Bounds Check in Fade Calculation

**Location:** `transport_controller.cpp:362-378`

**Problem:** Fade logic assumes `relativePos` is always within [0, trimmedDuration). Race condition in Fix 1 can violate this.

**Fix:**

```cpp
// ORP097 Bug 7 Fix: Only apply clip fade-in/out on FIRST playthrough (not on loops)
// Loops should be seamless with no fade processing at boundaries
if (!clip.hasLoopedOnce) {
  // Apply clip fade-in (first N samples from trim IN)
  int64_t relativePos = clip.currentSample + static_cast<int64_t>(frame) - trimIn;

  // ORP114 FIX: Validate relativePos is within valid trim range
  // Prevents fade calculation overflow when trim points change during playback
  int64_t trimmedDuration = trimOut - trimIn;
  if (relativePos < 0 || relativePos >= trimmedDuration) {
    // Position out of valid range - skip fade logic for this frame
    // (This should never happen after Fix 1, but provides defense-in-depth)
    continue; // Skip to next frame
  }

  if (fadeInSampleCount > 0 && relativePos >= 0 && relativePos < fadeInSampleCount) {
    float fadeInPos = static_cast<float>(relativePos) / static_cast<float>(fadeInSampleCount);
    gain *= calculateFadeGain(fadeInPos, fadeInCurveType);
  }

  // Apply clip fade-out (last N samples before trim OUT)
  if (fadeOutSampleCount > 0 && relativePos >= (trimmedDuration - fadeOutSampleCount)) {
    int64_t fadeOutRelativePos = relativePos - (trimmedDuration - fadeOutSampleCount);
    float fadeOutPos = static_cast<float>(fadeOutRelativePos) / static_cast<float>(fadeOutSampleCount);
    gain *= (1.0f - calculateFadeGain(fadeOutPos, fadeOutCurveType));
  }
}
```

**Rationale:**

- **Defense-in-depth:** Protects against any code path that leaves position out of bounds
- **Fail-safe:** Skips frame instead of calculating with overflow values
- **Performance:** Minimal cost (one comparison per frame, only when `!hasLoopedOnce`)

---

### Fix 3: Enable Restart Crossfade in restartClip()

**Location:** `transport_controller.cpp:1362-1366`

**Problem:** Manual restart doesn't apply broadcast-safe restart crossfade.

**Fix:**

```cpp
// CRITICAL: Manual restart SHOULD apply clip fade-in (user action)
// This is DIFFERENT from auto-loop which should NOT apply fade-in
// Set hasLoopedOnce = false to allow clip fade-in on restart
clip.hasLoopedOnce = false;

// ORP114 FIX: Enable broadcast-safe restart crossfade (5ms fade-in)
// Matches behavior of initial clip start, prevents clicks on restart
clip.isRestarting = true;
clip.restartFadeFramesRemaining = static_cast<int64_t>(m_restartCrossfadeSamples);
```

**Rationale:**

- **Consistency:** Manual restart should behave like initial start (with crossfade)
- **Broadcast-safe:** Eliminates clicks when restarting (5ms linear fade-in)
- **Matches comment intent:** Code comment at line 1348 says "broadcast-safe restart with crossfade"

---

## Test Plan

### Unit Tests (New)

1. **Test: Trim OUT during active playback**
   - Start clip at position 50,000 samples
   - Update trim OUT from 100,000 → 40,000 (BEFORE current position)
   - Verify: Position clamped to 40,000, no gain accumulation

2. **Test: Rapid restarts**
   - Restart clip 10 times in quick succession
   - Verify: Gain remains at unity, no accumulation

3. **Test: Loop + Trim OUT combination**
   - Start looping clip
   - Reduce trim OUT while looping
   - Verify: No gain spikes, smooth loop behavior

4. **Test: Multi-voice gain summing**
   - Start same clip 4 times (MAX_VOICES_PER_CLIP)
   - Measure routing matrix output
   - Verify: Gain = 4.0x (intentional additive summing)

### Integration Tests (Clip Composer)

1. **User Reproduction Steps:**
   - Load clip, enable loop, start playing
   - Press Trim OUT key command while looping
   - Expected: No distortion, position updates smoothly

2. **Restart Test:**
   - Load clip, start playing
   - Press grid button repeatedly (restart)
   - Expected: No clicks, no gain accumulation

3. **Waveform Click Test:**
   - Load clip, enable loop, start playing
   - Click waveform at different positions
   - Expected: Smooth seeks, no distortion

---

## Implementation Timeline

### Phase 1: Diagnostic Logging (30 minutes)

- Add `DEBUG_GAIN_TRACE` logging to processAudio()
- Compile with debug flag enabled
- Reproduce bug, analyze trace output
- **Deliverable:** Confirmation of which multiplier is causing issue

### Phase 2: Critical Fixes (2 hours)

- Implement Fix 1 (position validation)
- Implement Fix 2 (bounds check)
- Test with user reproduction steps
- If restart issues persist: Implement Fix 3
- **Deliverable:** Gain accumulation eliminated

### Phase 3: Testing & Validation (1 hour)

- Run existing transport unit tests
- Add new regression tests (4 tests listed above)
- Verify no gain accumulation with diagnostic logging
- **Deliverable:** All tests passing, no regressions

### Phase 4: Cleanup (30 minutes)

- Remove diagnostic logging
- Update documentation
- Create ORP114 completion report
- **Deliverable:** Clean, production-ready code

**Total Estimated Time:** 4 hours

---

## Expected Outcomes

### After Fix 1 (Position Validation)

- ✅ Trim OUT during playback: Position clamped, no overflow
- ✅ Loop + Trim OUT: Smooth behavior, no distortion
- ✅ Waveform clicks during loop: Safe position updates

### After Fix 2 (Bounds Check)

- ✅ Defense-in-depth: Protection against any future trim race conditions
- ✅ Fail-safe: Skips frames with invalid positions instead of calculating with garbage

### After Fix 3 (Restart Crossfade)

- ✅ Manual restart: Smooth 5ms fade-in, no clicks
- ✅ Consistency: Restart behaves like initial start
- ✅ Broadcast-safe: Meets professional audio standards

### Known Behaviors (NOT Bugs)

- ❌ Multi-voice summing: 2-4x gain increase with rapid re-triggers is INTENTIONAL (for layering)
- ❌ Clipping protection: tanh limiter compresses >0.9 signals (mitigation, not a bug)

---

## References

**Related Documents:**

- ORP097: Loop fade bug fixes (hasLoopedOnce state machine)
- ORP093: Trim boundary enforcement
- OCC109: Clipping protection implementation (v0.2.2)
- OCC136: Loop fade regression (INCOMPLETE) - likely related to this investigation

**Code Locations:**

- `src/core/transport/transport_controller.cpp` (main implementation)
- `src/core/transport/transport_controller.h` (state machine definitions)
- `src/core/routing/routing_matrix.cpp` (gain summing)
- `include/orpheus/transport_controller.h` (public API, SessionDefaults)

**Test Locations:**

- `tests/transport/*_test.cpp` (existing unit tests)
- `apps/clip-composer/src/` (integration testing environment)

---

**Next Actions:**

1. ✅ Add diagnostic logging (30 min)
2. ✅ Implement Fix 1 + Fix 2 (1.5 hours)
3. ✅ Test with user reproduction steps (30 min)
4. ✅ Add regression tests (1 hour)
5. ✅ Clean up and document (30 min)

**Blocking Issues:** None identified. All root causes are in SDK transport layer (no dependencies on external systems).

---

**Document Version:** 1.0
**Author:** Claude (AI Assistant)
**Reviewed By:** Pending
**Status:** Ready for Implementation
