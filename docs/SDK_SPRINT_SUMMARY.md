# SDK Enhancement Sprint - Quick Reference

**Document:** ORP074 - SDK Enhancement Sprint: Clip Metadata Management
**Location:** `/Users/chrislyons/dev/orpheus-sdk/docs/ORP/ORP074.md`
**Status:** Ready for Implementation
**Estimated Effort:** 2-3 developer-weeks

---

## What You Need to Implement

### Three New Methods for TransportController

```cpp
// 1. Update trim points (where clip starts/ends)
SessionGraphError updateClipTrimPoints(ClipHandle handle,
                                       int64_t trimInSamples,
                                       int64_t trimOutSamples);

// 2. Update fades (gain envelopes at start/end)
SessionGraphError updateClipFades(ClipHandle handle,
                                 double fadeInSeconds,
                                 double fadeOutSeconds,
                                 FadeCurve fadeInCurve,
                                 FadeCurve fadeOutCurve);

// 3. Query current trim points
SessionGraphError getClipTrimPoints(ClipHandle handle,
                                   int64_t& trimInSamples,
                                   int64_t& trimOutSamples) const;
```

### New Enum

```cpp
enum class FadeCurve : uint8_t {
    Linear = 0,       // f(x) = x
    EqualPower = 1,   // f(x) = sin(x * π/2)
    Exponential = 2   // f(x) = x²
};
```

---

## Why This Matters

**Current Problem:**

- OCC users can edit trim/fade in Edit Dialog
- Changes work in **preview mode** ✅
- Changes **don't apply to main playback** ❌

**Root Cause:**

- SDK's `registerClipAudio()` only takes file path
- No API to update metadata after registration

**After This Sprint:**

- User edits clip → clicks OK → main playback applies changes ✅

---

## Implementation Checklist

### Phase 1: Data Model (2 hours)

- [ ] Add trim/fade fields to `ClipPlaybackState`
- [ ] Add `updateFadeSamples()` helper method
- [ ] Write unit tests for data model

### Phase 2: API Implementation (4 hours)

- [ ] Implement `updateClipTrimPoints()` with validation
- [ ] Implement `updateClipFades()` with validation
- [ ] Implement `getClipTrimPoints()` query method
- [ ] Add error codes: `InvalidClipTrimPoints`, `InvalidFadeDuration`, `ClipNotRegistered`
- [ ] Write unit tests (valid/invalid inputs, error cases)

### Phase 3: Audio Callback (6 hours)

- [ ] Modify `processAudio()` to apply trim points
- [ ] Modify `processAudio()` to apply fade envelopes
- [ ] Implement `calculateFadeGain()` helper (3 curve types)
- [ ] Write integration tests (trim applied, fades audible)

### Phase 4: OCC Integration (2 hours)

- [ ] Update `AudioEngine::updateClipMetadata()` to call SDK methods
- [ ] Map string curves to enum ("Linear" → FadeCurve::Linear)
- [ ] Manual testing: Edit clip → OK → Play → Verify

### Phase 5: Documentation & Testing (6 hours)

- [ ] Add Doxygen comments to new methods
- [ ] Run all unit tests (GoogleTest)
- [ ] Run all integration tests
- [ ] Run manual test plan (OCC Edit Dialog → main playback)
- [ ] Code review with SDK team
- [ ] Performance profiling (CPU overhead <5%)

---

## Files to Modify

### SDK Core

1. **`include/orpheus/transport_controller.h`** - Add 3 method declarations, FadeCurve enum
2. **`include/orpheus/session_graph_error.h`** - Add 3 error codes
3. **`src/core/transport/clip_playback_state.h`** - Add trim/fade fields (internal)
4. **`src/core/transport/transport_controller.cpp`** - Implement 3 methods + audio callback changes

### OCC Application

5. **`apps/clip-composer/Source/Audio/AudioEngine.cpp`** - Remove TODO, call SDK methods

### Tests

6. **`tests/transport/clip_metadata_test.cpp`** - New unit test file
7. **`tests/transport/clip_playback_integration_test.cpp`** - Add integration tests

---

## Testing Strategy

### Unit Tests (GoogleTest)

- ✅ Update trim points → getter returns new values
- ✅ Invalid trim points → returns error
- ✅ Update fades → cached sample counts recomputed
- ✅ Invalid fade durations → returns error
- ✅ Clip not registered → returns error
- ✅ Fade curve calculation: Linear, EqualPower, Exponential

### Integration Tests

- ✅ Trim IN applied on playback
- ✅ Trim OUT stops playback at correct position
- ✅ Fade-in audible at start (verify gain ramp)
- ✅ Fade-out audible at end (verify gain ramp)
- ✅ No crashes when updating metadata on playing clip

### Manual Tests (OCC)

- ✅ Load clip → Edit Dialog → Set IN at 2s → Play → Starts at 2s
- ✅ Set OUT at 8s (10s file) → Play → Stops at 8s
- ✅ Set fade-in 1.0s Linear → Audible ramp
- ✅ Set fade-out 0.5s Exponential → Audible ramp
- ✅ Preview matches main playback exactly

---

## Key Implementation Details

### Thread Safety

```cpp
// UI thread: Update with atomic store
clip.trimInSamples.store(trimInSamples, std::memory_order_release);

// Audio thread: Read with atomic load
int64_t trimIn = clip.trimInSamples.load(std::memory_order_acquire);
```

### Fade Calculation (Audio Thread)

```cpp
for (size_t i = 0; i < num_frames; ++i) {
    int64_t filePos = clip.currentPosition + i;
    float sample = readAudioSample(clip, filePos);

    float gain = 1.0f;

    // Fade-in: First N samples from trim IN
    int64_t relativePos = filePos - trimIn;
    if (relativePos < fadeInSamples) {
        float fadeInPos = (float)relativePos / fadeInSamples;
        gain *= calculateFadeGain(fadeInPos, fadeInCurve);
    }

    // Fade-out: Last N samples before trim OUT
    int64_t trimmedDuration = trimOut - trimIn;
    if (relativePos >= (trimmedDuration - fadeOutSamples)) {
        int64_t fadeOutRelPos = relativePos - (trimmedDuration - fadeOutSamples);
        float fadeOutPos = (float)fadeOutRelPos / fadeOutSamples;
        gain *= (1.0f - calculateFadeGain(fadeOutPos, fadeOutCurve));
    }

    output_buffers[ch][i] += sample * gain;
}
```

### Fade Curves

```cpp
float calculateFadeGain(float x, FadeCurve curve) {
    switch (curve) {
        case FadeCurve::Linear:
            return x;  // y = x

        case FadeCurve::EqualPower:
            return std::sin(x * M_PI_2);  // y = sin(x * π/2)

        case FadeCurve::Exponential:
            return x * x;  // y = x²
    }
}
```

---

## Success Criteria

**Before Merging:**

- [ ] All unit tests pass (100% coverage)
- [ ] All integration tests pass
- [ ] Manual test plan executed (all items ✅)
- [ ] No memory leaks (valgrind clean)
- [ ] CPU overhead <5% (measured with 16 clips)
- [ ] Code reviewed by 1+ SDK team members
- [ ] Doxygen comments complete

**After OCC v0.2.0 Release:**

- [ ] User feedback: "Main playback matches preview"
- [ ] No trim/fade bugs reported in first 30 days
- [ ] Feature used by >80% of beta testers

---

## Related Documentation

**Complete Specification:** `/Users/chrislyons/dev/orpheus-sdk/docs/ORP/ORP074.md`

**Additional Context:**

- **OCC029** - SDK Enhancement Recommendations (gap analysis)
- **OCC027** - API Contracts (interface specifications)
- **OCC037** - Edit Dialog Preview Enhancements (what's complete)
- **OCC022** - Clip Metadata Schema (data model)

**Reference Implementation:**

- **PreviewPlayer.cpp:226-306** - Fade processing example (OCC application)

---

## Questions?

**Slack:** #orpheus-sdk-dev
**GitHub:** Label issues with `sdk-enhancement` and `occ-integration`
**Point of Contact:** OCC team lead (AudioEngine integration questions)

---

**Status:** ✅ Ready to Start
**Priority:** HIGH (Blocking OCC v0.2.0 main playback)
**Timeline:** Week 7-8 (2-3 developer-days)
