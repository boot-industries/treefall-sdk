# ORP093: SDK Sprint - Trim Point Boundary Enforcement

**Status:** ✅ COMPLETE (See ORP094.md for completion report)
**Priority:** P0 (Critical - User-Facing Bug)
**Assigned To:** SDK Team
**Reporter:** Clip Composer Team
**Created:** 2025-10-28
**Completed:** 2025-10-28
**Target:** v0.2.2 (Hot Fix)

---

## Executive Summary

The Orpheus SDK's clip playback system does not enforce trim IN/OUT boundaries correctly, allowing playback position to escape the user-defined trim range. This violates the fundamental contract that trim points define the playable region of a clip and creates a trust-breaking UX where the visual playhead does not match what the user hears.

**Impact:** Critical user-facing bug affecting Clip Composer Edit Dialog and any future applications using trim metadata.

**Root Cause:** SDK's `ITransportController` does not enforce position boundaries in the audio callback or loop restart logic.

---

## Problem Statement

### User-Reported Issue

From Clip Composer v0.2.1 testing session (2025-10-28):

> "I am able to make the playhead escape both IN and OUT, which shouldn't be possible – but I can't tell what's doing it. I am also able to break the PLAY button somehow."

### Technical Analysis

**Current Behavior:**

1. User sets trim IN=10000, OUT=50000 samples via `updateClipMetadata()`
2. User starts playback via `startClip()`
3. **BUG:** Playback position reported by `getClipPosition()` exceeds OUT point (e.g., 52000 samples)
4. **BUG:** Position can go below IN point (e.g., 8000 samples) in some scenarios
5. UI displays actual position, creating visual/audio mismatch

**Expected Behavior:**

1. User sets trim IN=10000, OUT=50000 samples
2. User starts playback
3. **Playback ALWAYS starts at IN point (10000)**
4. **Playback position NEVER exceeds OUT point (50000)**
5. **When reaching OUT point:**
   - If loop enabled: Jump to IN point and continue
   - If loop disabled: Stop playback
6. UI displays position that matches audio

**Why This Matters:**

- **Trust:** Users must see what they hear. Visual/audio mismatch destroys confidence in the tool.
- **Sample-Accuracy:** Broadcast/post-production workflows require frame-accurate trim enforcement.
- **Contract Violation:** `updateClipMetadata()` API promises to enforce boundaries but doesn't.

---

## Scope

### In-Scope

**1. Audio Thread Position Enforcement** (src/core/transport/transport_controller.cpp)

- Clamp playback position to [trimIn, trimOut] range every audio callback
- Handle edge case: trimOut enforcement with loop mode
- Handle edge case: trimIn enforcement on manual seek

**2. Loop Restart Logic** (src/core/transport/transport_controller.cpp)

- When reaching trimOut with loop enabled: Jump to trimIn (not 0)
- Ensure restart is sample-accurate (no drift over multiple loops)
- Ensure restart is gap-free (no audio click)

**3. Start Position Enforcement** (src/core/transport/transport_controller.cpp)

- `startClip()` must always begin at trimIn (not file start)
- `seekClip()` (future API) must clamp to [trimIn, trimOut]

**4. Metadata Application Race Condition** (src/core/transport/transport_controller.cpp)

- Ensure `updateClipMetadata()` atomically updates trim points
- Handle case: Metadata updated while clip is playing
- Handle case: Metadata updated during loop restart

**5. Unit Tests** (tests/transport/trim_enforcement_test.cpp)

- Test: Position never escapes [trimIn, trimOut] during playback
- Test: Loop mode restarts at trimIn, not 0
- Test: startClip() begins at trimIn
- Test: Metadata update while playing doesn't cause escape
- Test: OUT point enforcement with various buffer sizes (256, 512, 1024)

### Out-of-Scope

- UI changes (already implemented diagnostics in OCC v0.2.1)
- Fade processing (separate from boundary enforcement)
- Multi-clip playback (this is single-clip enforcement)
- Seek API implementation (mentioned for completeness, not required for fix)

---

## Technical Requirements

### 1. Audio Callback Position Clamping

**File:** `src/core/transport/transport_controller.cpp`
**Function:** `TransportController::processAudio()` or equivalent

**Pseudocode:**

```cpp
void TransportController::processAudio(float** outputs, int numChannels, int numSamples) {
    for (auto& clip : activeClips) {
        if (!clip.isPlaying) continue;

        // CRITICAL: Enforce boundaries before processing
        int64_t trimIn = clip.metadata.trimInSamples.load(std::memory_order_acquire);
        int64_t trimOut = clip.metadata.trimOutSamples.load(std::memory_order_acquire);
        int64_t currentPos = clip.playbackPosition.load(std::memory_order_acquire);

        // Check if position has escaped boundaries
        if (currentPos < trimIn) {
            // ERROR: Position escaped below IN point
            DBG("SDK: Position " << currentPos << " < trimIn " << trimIn << " - jumping to IN");
            clip.playbackPosition.store(trimIn, std::memory_order_release);
            currentPos = trimIn;
        }

        if (currentPos >= trimOut) {
            if (clip.loopEnabled) {
                // Loop mode: Jump to IN point
                DBG("SDK: Position " << currentPos << " >= trimOut " << trimOut << " - looping to IN");
                clip.playbackPosition.store(trimIn, std::memory_order_release);
                currentPos = trimIn;
            } else {
                // Non-loop: Stop playback
                DBG("SDK: Position " << currentPos << " >= trimOut " << trimOut << " - stopping");
                clip.isPlaying = false;
                continue;
            }
        }

        // Process audio with enforced position
        processClipAudio(clip, outputs, numChannels, numSamples);
    }
}
```

**Key Points:**

- Use atomic loads for trim points (thread-safe)
- Check boundaries BEFORE processing audio
- Handle loop restart with sample-accurate jump
- Log boundary violations (helps debugging)

---

### 2. Start Position Enforcement

**File:** `src/core/transport/transport_controller.cpp`
**Function:** `TransportController::startClip(int clipIndex)`

**Pseudocode:**

```cpp
bool TransportController::startClip(int clipIndex) {
    auto& clip = clips[clipIndex];

    // Load trim metadata atomically
    int64_t trimIn = clip.metadata.trimInSamples.load(std::memory_order_acquire);

    // CRITICAL: Always start at trim IN point, never at file start
    clip.playbackPosition.store(trimIn, std::memory_order_release);
    clip.isPlaying = true;

    DBG("SDK: Started clip " << clipIndex << " at trim IN point " << trimIn);
    return true;
}
```

**Key Points:**

- Never start at sample 0 (file start) if trimIn > 0
- Atomic position write (thread-safe)

---

### 3. Metadata Update Safety

**File:** `src/core/transport/transport_controller.cpp`
**Function:** `TransportController::updateClipMetadata(...)`

**Pseudocode:**

```cpp
bool TransportController::updateClipMetadata(
    int clipIndex,
    int64_t trimInSamples,
    int64_t trimOutSamples,
    float fadeInSeconds,
    float fadeOutSeconds,
    const std::string& fadeInCurve,
    const std::string& fadeOutCurve
) {
    auto& clip = clips[clipIndex];

    // Validate trim points
    if (trimInSamples < 0 || trimOutSamples <= trimInSamples) {
        DBG("SDK: Invalid trim points - IN: " << trimInSamples << ", OUT: " << trimOutSamples);
        return false;
    }

    // Update atomically (audio thread will read these)
    clip.metadata.trimInSamples.store(trimInSamples, std::memory_order_release);
    clip.metadata.trimOutSamples.store(trimOutSamples, std::memory_order_release);

    // If clip is currently playing, enforce new boundaries immediately
    if (clip.isPlaying) {
        int64_t currentPos = clip.playbackPosition.load(std::memory_order_acquire);

        // Clamp current position to new boundaries
        if (currentPos < trimInSamples) {
            clip.playbackPosition.store(trimInSamples, std::memory_order_release);
            DBG("SDK: Metadata update caused position adjustment to new IN point");
        } else if (currentPos >= trimOutSamples) {
            clip.playbackPosition.store(trimOutSamples - 1, std::memory_order_release);
            DBG("SDK: Metadata update caused position adjustment to new OUT point");
        }
    }

    // Update fade metadata (existing code)
    // ...

    return true;
}
```

**Key Points:**

- Validate trim points before applying
- Use atomic stores (thread-safe)
- Immediately clamp position if clip is playing

---

### 4. Loop Restart Logic

**File:** `src/core/transport/transport_controller.cpp`
**Function:** `processClipAudio()` or equivalent

**Pseudocode:**

```cpp
void TransportController::processClipAudio(Clip& clip, float** outputs, int numChannels, int numSamples) {
    int64_t trimIn = clip.metadata.trimInSamples.load(std::memory_order_acquire);
    int64_t trimOut = clip.metadata.trimOutSamples.load(std::memory_order_acquire);
    int64_t position = clip.playbackPosition.load(std::memory_order_acquire);

    int samplesRemaining = numSamples;
    int outOffset = 0;

    while (samplesRemaining > 0) {
        // Calculate samples until OUT point
        int64_t samplesToOut = trimOut - position;
        int samplesToProcess = std::min(static_cast<int64_t>(samplesRemaining), samplesToOut);

        if (samplesToProcess > 0) {
            // Read and process audio
            readAudioFromFile(clip, position, samplesToProcess, outputs, outOffset, numChannels);
            position += samplesToProcess;
            outOffset += samplesToProcess;
            samplesRemaining -= samplesToProcess;
        }

        // Check if we reached OUT point
        if (position >= trimOut) {
            if (clip.loopEnabled) {
                // Loop: Jump back to IN point (sample-accurate, no gap)
                position = trimIn;
                DBG("SDK: Loop restart - position reset to " << trimIn);
            } else {
                // Non-loop: Stop and fill remaining buffer with silence
                fillSilence(outputs, outOffset, samplesRemaining, numChannels);
                clip.isPlaying = false;
                DBG("SDK: Reached OUT point - stopping playback");
                break;
            }
        }
    }

    // Write back position atomically
    clip.playbackPosition.store(position, std::memory_order_release);
}
```

**Key Points:**

- Handle loop restart mid-buffer (gap-free)
- Fill remaining buffer with silence if non-loop stops
- Atomic position updates

---

## Testing Strategy

### Unit Tests (GoogleTest)

**File:** `tests/transport/trim_enforcement_test.cpp`

**Test Cases:**

```cpp
TEST(TrimEnforcementTest, PositionNeverEscapesOutPoint) {
    // Setup: Clip with trim IN=10000, OUT=50000
    // Play for 5 seconds at 48kHz (240000 samples)
    // Assert: Position never exceeds 50000
}

TEST(TrimEnforcementTest, PositionNeverEscapesInPoint) {
    // Setup: Clip with trim IN=10000, OUT=50000
    // Attempt to manually set position to 5000
    // Assert: Position clamped to 10000
}

TEST(TrimEnforcementTest, LoopModeRestartsAtTrimIn) {
    // Setup: Clip with trim IN=10000, OUT=50000, loop=true
    // Play until reaching OUT point
    // Assert: Next sample read is from position 10000 (not 0)
}

TEST(TrimEnforcementTest, NonLoopModeStopsAtTrimOut) {
    // Setup: Clip with trim IN=10000, OUT=50000, loop=false
    // Play until reaching OUT point
    // Assert: Playback stops, isPlaying=false
}

TEST(TrimEnforcementTest, StartClipBeginsAtTrimIn) {
    // Setup: Clip with trim IN=10000, OUT=50000
    // Call startClip()
    // Assert: Initial position is 10000 (not 0)
}

TEST(TrimEnforcementTest, MetadataUpdateWhilePlaying) {
    // Setup: Clip playing at position 30000
    // Update trim IN=20000, OUT=40000
    // Assert: Position clamped to 40000 (new OUT)
}

TEST(TrimEnforcementTest, BufferSizeBoundaryConditions) {
    // Setup: Clip with trim OUT=50256, buffer=256 samples
    // Play until OUT point is mid-buffer
    // Assert: Exact stop at 50256, no overshoot
}
```

### Integration Tests (Clip Composer)

**Manual Test Procedure:**

1. **Baseline Test:**
   - Load audio file (60 seconds)
   - Set trim IN=5s, OUT=10s
   - Play clip
   - **Verify:** Playback starts at 5s, stops at 10s
   - **Verify:** No console warnings about boundary escape

2. **Loop Test:**
   - Same setup as above
   - Enable loop mode
   - Let play for 30 seconds (3 loops)
   - **Verify:** Playback restarts at 5s each time (not 0s)
   - **Verify:** No audio clicks on loop restart

3. **Live Metadata Update Test:**
   - Start playback at position 7s
   - While playing, change OUT to 8s
   - **Verify:** Playback stops at new OUT (8s)
   - **Verify:** No crash, no glitches

4. **Jog/Seek Test:**
   - Click on waveform at 3s (before IN point)
   - **Verify:** Playback jumps to IN point (5s), not clicked position
   - Click on waveform at 12s (after OUT point)
   - **Verify:** Playback jumps to OUT-1 sample

---

## Acceptance Criteria

### Definition of Done

- [ ] All unit tests pass (7 tests in trim_enforcement_test.cpp)
- [ ] All integration tests pass (4 manual tests above)
- [ ] No regressions in existing transport tests
- [ ] Code review approved by 2 SDK team members
- [ ] Performance: No measurable CPU increase (<0.5% overhead)
- [ ] Memory: No new allocations in audio thread
- [ ] Documentation: Update ITransportController.h with boundary guarantees
- [ ] Clip Composer: No console warnings during 10-minute playback session

### Success Metrics

- **Zero** boundary escape warnings in 1-hour stress test
- **100%** of manual integration tests pass
- **<1ms** added latency (imperceptible)
- **Bit-identical** loop restart (deterministic)

---

## Implementation Plan

### Phase 1: Core Enforcement (2 days)

**Day 1:**

- [ ] Implement position clamping in processAudio()
- [ ] Implement start position enforcement in startClip()
- [ ] Write unit tests (TrimEnforcementTest)
- [ ] Run tests, fix failures

**Day 2:**

- [ ] Implement loop restart logic
- [ ] Implement metadata update safety
- [ ] Add atomic memory ordering
- [ ] Code review with SDK team

### Phase 2: Integration & Testing (1 day)

**Day 3:**

- [ ] Test with Clip Composer Edit Dialog
- [ ] Run 1-hour stress test
- [ ] Verify determinism with check-determinism.sh
- [ ] Document API guarantees in header
- [ ] Merge to main

### Phase 3: Hot Fix Release (0.5 days)

**Day 4:**

- [ ] Tag SDK version v0.2.2
- [ ] Update Clip Composer to use SDK v0.2.2
- [ ] Test OCC with new SDK
- [ ] Release OCC v0.2.2 with fix

**Total:** 3.5 days (1 developer)

---

## Risk Analysis

### High Risk

**1. Race Condition in Metadata Update**

- **Risk:** Metadata updated mid-loop restart causes crash
- **Mitigation:** Use atomic operations, write comprehensive unit test
- **Contingency:** Add mutex if atomics prove insufficient (measure performance impact)

**2. Loop Restart Audio Click**

- **Risk:** Gap-free loop restart is hard to achieve sample-accurately
- **Mitigation:** Pre-validate with unit test that checks for discontinuities
- **Contingency:** Accept minor click if gap-free proves impossible (document limitation)

### Medium Risk

**3. Performance Overhead**

- **Risk:** Boundary checks every audio callback add CPU cost
- **Mitigation:** Use branch prediction hints, atomic loads (fast)
- **Contingency:** Profile with Instruments, optimize hot path if needed

**4. Backward Compatibility**

- **Risk:** Existing applications rely on current (broken) behavior
- **Mitigation:** This is a bug fix, not API change - no compatibility concern
- **Contingency:** Add legacy mode flag if absolutely necessary (discouraged)

### Low Risk

**5. Buffer Size Edge Cases**

- **Risk:** OUT point enforcement fails with unusual buffer sizes (e.g., 37 samples)
- **Mitigation:** Unit test with prime-number buffer sizes
- **Contingency:** Add explicit validation for min buffer size (e.g., 32 samples)

---

## Dependencies

### Upstream

- None (self-contained SDK fix)

### Downstream

- **Clip Composer v0.2.2:** Must update to SDK v0.2.2 after this fix ships
- **Future Applications:** Will inherit correct boundary enforcement

---

## References

### Code Locations

- `src/core/transport/transport_controller.h` - ITransportController API
- `src/core/transport/transport_controller.cpp` - Implementation
- `tests/transport/trim_enforcement_test.cpp` - Unit tests (new file)
- `apps/clip-composer/Source/UI/PreviewPlayer.cpp` - Diagnostic logging (OCC)

### Related Issues

- ORP089 - OUT Point Enforcement Architecture
- ORP090 - Loop Mode Implementation
- Clip Composer v0.2.1 - User-reported playhead escape bug

### Design Decisions

- **Decision:** Enforce at SDK level, not UI level
- **Rationale:** UI must never show different position than audio plays (trust)
- **Alternative Considered:** UI-side clamping (rejected - violates truth principle)

---

## Notes

### Performance Considerations

- Atomic loads are fast (~1-2 cycles on modern CPUs)
- Branch prediction will favor "position in bounds" case
- Expected overhead: <0.1% CPU

### Future Enhancements (Post-Fix)

- [ ] Add `seekClip(int64_t samplePosition)` API (ORP094)
- [ ] Add `getValidPositionRange()` API for UI feedback
- [ ] Add trim point change callback for real-time UI updates

---

**Document Version:** 1.0
**Last Updated:** 2025-10-28
**Next Review:** After implementation complete
