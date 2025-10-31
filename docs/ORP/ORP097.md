# ORP097 - SDK Transport/Fade Bug Fixes for OCC v0.2.0

**Status:** Pending Implementation
**Priority:** Medium
**Assigned To:** SDK Team
**Related:** PR #121 (OCC v0.2.0 Sprint)
**Date:** 2025-10-29

## Purpose

This document specifies two SDK-level bugs discovered during OCC v0.2.0 sprint implementation that require transport controller and fade processing fixes.

## Context

During OCC v0.2.0 UI development, two behavioral issues were identified that cannot be fixed at the application layer. Both require changes to Orpheus SDK's core transport and fade processing systems.

## Bug 6: Transport Restart During Fade-Out

### Current Behavior

When a clip is clicked while it is in the fade-out/stopping state, the transport controller continues the fade-out process to completion. The click is effectively ignored.

### Expected Behavior

When a clip is clicked during fade-out, it should:

1. Immediately cancel the current fade-out
2. Reset playhead to the clip's IN point (trimInSamples)
3. Start playback from the beginning with fade-in (if configured)

### Technical Details

**Affected Components:**

- `src/core/transport/TransportController.cpp` - `startClip()` method
- `src/core/transport/ClipPlayback.cpp` - State machine transitions

**Current State Machine (Simplified):**

```
STOPPED → PLAYING (on start)
PLAYING → STOPPING (on stop, begins fade-out)
STOPPING → STOPPED (fade-out complete)
STOPPING → ??? (on start while stopping) // BUG: Should be PLAYING
```

**Required Changes:**

1. In `TransportController::startClip()`:
   - Check if clip is in `STOPPING` state
   - If true, cancel fade-out timer
   - Reset playhead: `playheadPosition = clip.trimInSamples`
   - Transition to `PLAYING` state
   - Apply fade-in if `clip.fadeInSeconds > 0.0`

2. In `ClipPlayback::processAudio()`:
   - When transitioning from STOPPING → PLAYING, ensure fade envelope resets
   - Clear any residual fade-out coefficients

**Test Case:**

```cpp
// Load clip with fade out time = 1.0s
auto clip = loadClip("test.wav");
clip.fadeOutSeconds = 1.0;

// Start clip, let it play for 0.5s
transport->startClip(clip.handle);
advanceTime(0.5);

// Stop clip (begins fade-out)
transport->stopClip(clip.handle);
advanceTime(0.2); // 200ms into fade-out

// Click again - should restart from IN point
transport->startClip(clip.handle);

// EXPECTED: Playhead at trimInSamples, fade-in active
// ACTUAL: Fade-out continues, click ignored
EXPECT_EQ(transport->getPlayheadPosition(clip.handle), clip.trimInSamples);
EXPECT_EQ(transport->getClipState(clip.handle), PlaybackState::PLAYING);
```

### User Impact

**Use Case:** Radio broadcast operator accidentally stops wrong clip during live show. They immediately click it again to restart, but clip continues fading out, creating dead air.

**Severity:** Medium - Impacts live performance workflows, but workaround exists (wait for fade to complete).

---

## Bug 7: Loop Point Fade Behavior

### Current Behavior

When loop is enabled and playback reaches loop boundaries (LOOP START or LOOP END), the fade processing pipeline applies fade-in/fade-out transitions. This creates unintended crossfades at loop points.

### Expected Behavior

Loop boundaries should produce **seamless, click-free loops** with no fade processing:

1. When playhead reaches trimOutSamples (LOOP END), jump to trimInSamples (LOOP START)
2. No fade-out before jump
3. No fade-in after jump
4. Maintain constant gain throughout loop

**Note:** Fade-in/fade-out should ONLY apply at clip start/stop events, NOT at loop boundaries.

### Technical Details

**Affected Components:**

- `src/core/transport/ClipPlayback.cpp` - Loop boundary detection
- `src/core/audio/FadeProcessor.cpp` - Fade envelope application

**Current Logic (Problematic):**

```cpp
void ClipPlayback::processAudio(float** outputs, int numSamples) {
    for (int i = 0; i < numSamples; ++i) {
        // Check loop boundary
        if (m_playheadSamples >= m_trimOutSamples && m_loopEnabled) {
            applyFadeOut(); // BUG: Should not fade at loop point
            m_playheadSamples = m_trimInSamples;
            applyFadeIn();  // BUG: Should not fade at loop point
        }
        // ... render audio ...
    }
}
```

**Required Changes:**

1. Add `loopBoundaryTransition` flag to distinguish loop jumps from start/stop events
2. In `FadeProcessor::applyFade()`:
   - Skip fade processing if `loopBoundaryTransition == true`
   - Only apply fades for `clipStartEvent` and `clipStopEvent`

**Corrected Logic:**

```cpp
void ClipPlayback::processAudio(float** outputs, int numSamples) {
    for (int i = 0; i < numSamples; ++i) {
        // Check loop boundary
        if (m_playheadSamples >= m_trimOutSamples && m_loopEnabled) {
            // Seamless loop - NO fades
            m_playheadSamples = m_trimInSamples;
            // Continue rendering without fade processing
        }

        // Apply fades ONLY for clip start/stop events
        if (m_state == PlaybackState::PLAYING && m_fadeInActive) {
            applyFadeIn();
        }
        if (m_state == PlaybackState::STOPPING && m_fadeOutActive) {
            applyFadeOut();
        }

        // ... render audio ...
    }
}
```

**Test Case:**

```cpp
// Load clip with loop enabled, fade in/out configured
auto clip = loadClip("loop.wav");
clip.loopEnabled = true;
clip.fadeInSeconds = 0.5;
clip.fadeOutSeconds = 0.5;
clip.trimInSamples = 0;
clip.trimOutSamples = 48000; // 1 second @ 48kHz

// Start clip, let it loop 3 times
transport->startClip(clip.handle);
advanceTime(3.5); // 3 full loops + 0.5s

// EXPECTED: Constant gain during loop, no fade-in/out at boundaries
// ACTUAL: Fade-in/out applied at each loop point

auto audioBuffer = captureAudio(clip.handle, 3.5);

// Check gain is constant during loops (after initial fade-in)
for (int loopNum = 1; loopNum < 3; ++loopNum) {
    int loopStartSample = loopNum * 48000;
    float gainAtBoundary = audioBuffer[loopStartSample];
    EXPECT_NEAR(gainAtBoundary, 1.0f, 0.01f); // No fade
}
```

### User Impact

**Use Case:** Theater sound designer loops background ambience (rain, traffic, etc.). Current behavior creates rhythmic "pulsing" as gain dips at loop boundaries, breaking immersion.

**Severity:** Medium - Affects loop quality, but short loops minimize audibility. Workaround: use very long files to reduce loop frequency.

---

## Implementation Guidance

### Recommended Approach

1. **Bug 6 (Transport Restart):**
   - Add `handleRestartDuringFadeOut()` method to TransportController
   - Update state machine to allow STOPPING → PLAYING transitions
   - Add unit test: `transport/clip_restart_during_fadeout_test.cpp`
   - Estimated effort: 2-3 hours

2. **Bug 7 (Loop Fades):**
   - Refactor FadeProcessor to distinguish loop jumps from start/stop events
   - Add `FadeContext` enum: `CLIP_START`, `CLIP_STOP`, `LOOP_BOUNDARY`
   - Update ClipPlayback to pass correct context
   - Add unit test: `transport/loop_boundary_fade_test.cpp`
   - Estimated effort: 3-4 hours

### Testing Strategy

**Unit Tests:**

- `transport/clip_restart_during_fadeout_test.cpp` (Bug 6)
- `transport/loop_boundary_fade_test.cpp` (Bug 7)

**Integration Tests:**

- OCC manual testing: Click clip during fade-out (Bug 6)
- OCC manual testing: Enable loop on short clip, listen for clicks/fades (Bug 7)

**Regression Testing:**

- Verify existing fade-in/out behavior still works for normal start/stop
- Verify multi-clip playback unaffected

### Success Criteria

**Bug 6:**

- [ ] Clicking clip during fade-out restarts from IN point
- [ ] Playhead resets to trimInSamples
- [ ] Fade-in applied if configured
- [ ] No audio glitches or clicks

**Bug 7:**

- [ ] Loop boundaries produce seamless transitions
- [ ] No gain dips at loop points
- [ ] Fade-in/out still work for clip start/stop events
- [ ] No audio glitches or clicks at loop boundary

---

## Timeline

**Target Completion:** Within 2 weeks (by 2025-11-12)

**Milestones:**

1. Week 1: Implement fixes, add unit tests
2. Week 2: Integration testing with OCC, regression verification

**Blocking:** OCC v0.2.0 release (PR #121 can merge, but bugs remain in runtime behavior)

---

## Communication

**Slack:** `#orpheus-sdk-dev` (tag @sdk-team)
**GitHub Issue:** Create issues for Bug 6 and Bug 7, link to this document
**Status Updates:** Post progress to `#orpheus-occ-integration` channel

---

## Related Documents

- OCC v0.2.0 Sprint Specification (user request, October 29, 2025)
- PR #121: feat(occ): v0.2.0 Sprint - Clip Edit & Button UI Improvements
- `/src/core/transport/TransportController.h` - Transport API
- `/src/core/audio/FadeProcessor.h` - Fade processing API

---

## References

[1] PR #121 - https://github.com/chrislyons/orpheus-sdk/pull/121
[2] OCC v0.2.0 Sprint Discussion - Internal Slack, October 29, 2025
