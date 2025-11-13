# ORP111 SDK Transport Enhancement Request - Clip Composer Edit Dialog Support

**Date:** 2025-11-11
**Requested By:** Clip Composer Team (Application Layer)
**Priority:** High (Blocking v0.2.1 Edit Dialog Polish)
**Status:** 🟡 SDK Team Review Required
**Related Docs:** ORP109 (SDK Feature Roadmap), OCC093 (v0.2.0 Sprint Complete)

---

## Executive Summary

Clip Composer v0.2.0 Edit Dialog has identified two critical gaps in the SDK's `ITransportController` API that prevent proper edit law enforcement and playhead visualization. Both issues stem from missing real-time playback state APIs and require SDK-level implementation.

**Impact:** Without these features, Clip Composer cannot provide sample-accurate edit law enforcement or professional loop playback behavior expected in broadcast/theater applications.

**Requested Timeline:** v0.3.0 SDK release (estimated 2-4 weeks)

---

## Problem Statement

### Issue 1: No API to Query Clip Playback State

**Current Behavior:**

- `AudioEngine::isClipPlaying(handle)` is a stub that always returns `false`
- Clip Composer cannot detect when playback actually stops (e.g., clip reaches OUT point in non-looped mode)
- 75fps position timer cannot stop gracefully when clip finishes

**Application Impact:**

- Non-looped clips: Playhead continues polling position indefinitely even after audio stops
- Edit Dialog: Cannot distinguish "clip stopped at OUT" from "clip stopped by user"
- Performance: Unnecessary 75fps timer overhead when clip is not actually playing

**Current Workaround:**

- Force timer to run continuously (wasteful)
- Accept that playhead may drift or show stale position data

**Code Reference:**

```cpp
// apps/clip-composer/Source/UI/PreviewPlayer.cpp:209-214
void PreviewPlayer::timerCallback() {
  if (!isPlaying()) {
    // Timer is running but clip stopped - stop timer
    stopTimer();  // ⚠️ NEVER EXECUTES (isPlaying() stub always returns false)
    return;
  }
  // ... position update logic
}
```

---

### Issue 2: Fade Applied at Loop Points

**Current Behavior:**

- SDK transport applies fade-out when clip loops from OUT → IN
- Causes audible "dip" at loop boundary (unprofessional)
- Violates broadcast standard: fades should ONLY apply to initial start and final stop

**Application Impact:**

- Looped clips sound choppy (fade-out at every loop iteration)
- Broadcast operators cannot use looped clips for seamless backgrounds (e.g., ambience, music beds)
- Violates Edit Law #3: "All interim loop points must ignore fades"

**Current Workaround:**

- None (application layer cannot override SDK fade logic)

**Code Reference:**

```cpp
// src/core/transport/transport_controller.cpp:366-371
// PROBLEM: Applies fade-out at loop boundary when loopEnabled == true
if (clipState.position >= clipState.metadata.trimOutSamples) {
  if (clipState.loopEnabled) {
    // Apply fade-out before looping (⚠️ WRONG - should skip fade)
    applyFadeOut(outputBuffer, framesThisBlock, clipState);
    clipState.position = clipState.metadata.trimInSamples; // Loop to IN
  }
}
```

---

## Requested Features

### Feature 1: Clip State Query API

**API Proposal:**

```cpp
// include/orpheus/transport_controller.h

/// Clip playback state (returned by getClipState)
enum class ClipState {
  Stopped,    ///< Clip is not playing
  Playing,    ///< Clip is actively playing
  Stopping,   ///< Clip is fading out (will stop soon)
};

/// Get current playback state of a clip
/// @param handle Clip handle returned by loadClip()
/// @return Current playback state
virtual ClipState getClipState(ClipHandle handle) const = 0;
```

**Implementation Requirements:**

1. Query actual playback state from `TransportController` (not stub)
2. Return `ClipState::Stopped` when clip reaches OUT point (non-looped)
3. Return `ClipState::Playing` when clip is actively rendering audio
4. Return `ClipState::Stopping` during fade-out period (optional, for future fade visualization)

**Thread Safety:**

- Must be callable from message thread (UI thread)
- Must use atomic reads or lock-free queries
- Must NOT block audio thread

**Testing Criteria:**

- [ ] Returns `Playing` immediately after `startClip()` succeeds
- [ ] Returns `Stopped` when non-looped clip reaches OUT point
- [ ] Returns `Playing` continuously for looped clip (never stops)
- [ ] Returns `Stopped` after `stopClip()` completes fade-out

---

### Feature 2: Loop-Aware Fade Logic

**Implementation Proposal:**

Modify `TransportController::processAudio()` to skip fade application at loop boundaries:

```cpp
// src/core/transport/transport_controller.cpp

// OLD (current buggy behavior):
if (clipState.position >= clipState.metadata.trimOutSamples) {
  if (clipState.loopEnabled) {
    applyFadeOut(outputBuffer, framesThisBlock, clipState);  // ⚠️ WRONG
    clipState.position = clipState.metadata.trimInSamples;
  }
}

// NEW (correct behavior):
if (clipState.position >= clipState.metadata.trimOutSamples) {
  if (clipState.loopEnabled) {
    // Loop back to IN WITHOUT fade (seamless loop)
    clipState.position = clipState.metadata.trimInSamples;
    // Skip fade application entirely - loop points must be gapless
  } else {
    // Non-looped: Apply fade-out and stop
    applyFadeOut(outputBuffer, framesThisBlock, clipState);
    clipState.state = ClipState::Stopping;
  }
}
```

**Edit Law Requirements:**

1. **Initial Start:** Apply fade-in when clip starts from user command
2. **Final Stop:** Apply fade-out when clip stops from user command OR reaches OUT (non-looped)
3. **Loop Points:** NEVER apply fade when looping from OUT → IN

**Testing Criteria:**

- [ ] Looped clip: No audible dip at OUT → IN boundary
- [ ] Looped clip: Seamless gapless loop (sample-accurate)
- [ ] Non-looped clip: Fade-out applied when reaching OUT point
- [ ] Manual stop: Fade-out applied regardless of loop mode

---

## Use Case: Clip Composer Edit Dialog

### User Workflow

1. **User loads 30-second music loop** into Clip Composer
2. **User opens Edit Dialog** (Cmd+Opt+Ctrl+Click on grid button)
3. **User sets trim points:** IN=0s, OUT=8s (trim to 8-second loop)
4. **User enables loop mode** (? key or toggle button)
5. **User presses PLAY** to preview loop

**Expected Behavior:**

- Clip plays from IN (0s) → OUT (8s) seamlessly
- At OUT, clip loops back to IN **without any audible fade**
- Playhead updates at 75fps showing position within 0-8s range
- When user presses STOP, fade-out is applied

**Current Broken Behavior:**

- ❌ Audible fade-out at every loop point (unprofessional)
- ❌ Playhead continues polling even after clip stops (no state API)
- ❌ Application cannot detect when non-looped clip finishes

---

## Technical Details

### Thread Safety Considerations

**Feature 1 (getClipState):**

- Called from message thread (UI thread at 75fps)
- Reads `ClipState` enum from atomic or lock-free structure
- Must NOT allocate or block

**Feature 2 (Loop Fade Logic):**

- Runs on audio thread (real-time thread)
- Already implemented, just needs conditional check: `if (loopEnabled) { skip fade }`
- No allocation, no locks, no I/O

### Performance Impact

**Feature 1:** Negligible (single atomic read, called at 75fps)
**Feature 2:** Negligible (eliminates unnecessary fade DSP at loop points)

### Backward Compatibility

Both features are **non-breaking additions:**

- Feature 1: New API method (existing `isClipPlaying()` deprecated but still functional)
- Feature 2: Bug fix (changes behavior but aligns with broadcast standards)

---

## Implementation Estimates

| Task                                | Effort       | Owner        |
| ----------------------------------- | ------------ | ------------ |
| Feature 1: Add `getClipState()` API | 4 hours      | SDK Team     |
| Feature 1: Unit tests               | 2 hours      | SDK Team     |
| Feature 2: Loop fade logic fix      | 2 hours      | SDK Team     |
| Feature 2: Integration tests        | 2 hours      | SDK Team     |
| Documentation updates               | 1 hour       | SDK Team     |
| **Total**                           | **11 hours** | **SDK Team** |

**Estimated Delivery:** 2 weeks (allowing for code review, testing, CI integration)

---

## Success Metrics

### Feature 1: Clip State Query API

- [ ] `getClipState()` returns accurate state within 1 audio buffer period
- [ ] Clip Composer 75fps timer stops gracefully when clip finishes
- [ ] No performance degradation (profiled at 512-sample buffer, 48kHz)

### Feature 2: Loop-Aware Fade Logic

- [ ] Looped clips have **zero audible artifacts** at loop boundary
- [ ] Spectral analysis shows **sample-accurate loop** (no fade ramp)
- [ ] Manual stop still applies fade-out correctly

---

## Acceptance Criteria

**Feature 1 Ready When:**

1. ✅ `ITransportController::getClipState(handle)` implemented
2. ✅ Returns `Playing` when clip is rendering audio
3. ✅ Returns `Stopped` when clip reaches OUT (non-looped)
4. ✅ Unit tests pass (ClipState accuracy)
5. ✅ Clip Composer Edit Dialog playhead timer stops gracefully

**Feature 2 Ready When:**

1. ✅ Looped clips play gaplessly without fade at OUT → IN
2. ✅ Non-looped clips still apply fade-out at OUT
3. ✅ Manual stop applies fade-out regardless of loop mode
4. ✅ Integration tests pass (24-hour loop stability)

---

## Risk Assessment

**Low Risk:**

- Both features are isolated to `TransportController` (no cross-module dependencies)
- Feature 2 is a simple conditional check (minimal code change)
- No changes to public API surface (Feature 1 is additive only)

**Mitigation:**

- Unit tests for Feature 1 (state accuracy)
- Integration tests for Feature 2 (24-hour loop stability)
- Manual QA with Clip Composer Edit Dialog

---

## References

**SDK Documentation:**

- ORP109: SDK Feature Roadmap for Clip Composer Integration
- ORP110: Performance Monitor API Completion Report
- `src/core/transport/transport_controller.cpp`: Current loop/fade logic

**Application Documentation:**

- OCC093: v0.2.0 Sprint Complete (Edit Dialog implemented)
- `apps/clip-composer/Source/UI/PreviewPlayer.cpp`: 75fps position timer
- `apps/clip-composer/Source/UI/ClipEditDialog.cpp`: Edit law enforcement

**Broadcast Standards:**

- EBU R128: Loudness normalization (requires gapless loops for LU measurement)
- SMPTE 292M: Professional audio interchange (no unintended fades)

---

## Contact

**Requester:** Clip Composer Application Team
**Technical Lead:** SDK Core Team (Transport Module)
**Timeline:** Requested for v0.3.0 SDK release

**Questions/Clarifications:** See `apps/clip-composer/docs/occ/OCC093.md` for full Edit Dialog implementation details.

---

**Next Steps:**

1. SDK team reviews this request (estimated 1 week)
2. SDK team implements features (estimated 2 weeks)
3. Clip Composer team integrates and validates (estimated 1 week)
4. Release SDK v0.3.0 with features (target: December 2025)
