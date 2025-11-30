# SDK Team Handoff - Clip Metadata Management Sprint

**Date:** 2025-10-22
**Status:** Ready for Implementation
**Priority:** HIGH (Blocking OCC v0.2.0)
**Estimated Effort:** 2-3 developer-weeks

---

## Executive Summary

OCC v0.1.0-alpha shipped with preview-mode trim/fade editing. Users can edit clip parameters and hear changes in the preview player, but **main clip button playback does not respect edited metadata**. This sprint adds SDK APIs to close that gap.

**What's Complete:**

- ✅ OCC Edit Dialog (trim/fade UI)
- ✅ PreviewPlayer (audio processing with fades)
- ✅ AudioEngine infrastructure (stub method ready)
- ✅ MainComponent wiring (OK button integration)
- ✅ Comprehensive documentation (design + implementation specs)

**What You Need to Build:**

- ⏳ 3 new TransportController methods
- ⏳ Audio callback modifications (apply trim/fade)
- ⏳ Unit + integration tests
- ⏳ Remove TODO in AudioEngine.cpp (wire to SDK)

---

## Documentation

### Primary Document

**📄 ORP074 - SDK Enhancement Sprint: Clip Metadata Management**

- **Location:** `/Users/chrislyons/dev/orpheus-sdk/docs/ORP/ORP074.md`
- **Contents:** Complete API spec, implementation plan, test strategy, timeline
- **Length:** 44 pages (comprehensive)

### Quick Reference

**📄 SDK Sprint Summary**

- **Location:** `/Users/chrislyons/dev/orpheus-sdk/docs/SDK_SPRINT_SUMMARY.md`
- **Contents:** TL;DR version with checklist, code snippets, files to modify
- **Length:** 4 pages (quick start)

### Supporting Documents

**📄 OCC037** - Edit Dialog Preview Enhancements

- What's already implemented in OCC
- Reference implementation for fade processing

**📄 OCC029** - SDK Enhancement Recommendations

- Original gap analysis (5 critical modules)
- Long-term roadmap for SDK real-time features

**📄 OCC027** - API Contracts

- Interface specifications for all OCC↔SDK communication

---

## API You're Implementing

### Three New Methods

```cpp
namespace orpheus {

// Fade curve types
enum class FadeCurve : uint8_t {
    Linear = 0,       // f(x) = x
    EqualPower = 1,   // f(x) = sin(x * π/2)
    Exponential = 2   // f(x) = x²
};

class ITransportController {
public:
    // ... existing methods ...

    /**
     * Update trim points for a registered clip
     * Thread-safe: Can be called from UI thread
     * Takes effect: On next clip start (does not affect currently playing clips)
     */
    virtual SessionGraphError updateClipTrimPoints(ClipHandle handle,
                                                   int64_t trimInSamples,
                                                   int64_t trimOutSamples) = 0;

    /**
     * Update fade settings for a registered clip
     * Thread-safe: Can be called from UI thread
     * Takes effect: On next clip start (does not affect currently playing clips)
     */
    virtual SessionGraphError updateClipFades(ClipHandle handle,
                                             double fadeInSeconds,
                                             double fadeOutSeconds,
                                             FadeCurve fadeInCurve,
                                             FadeCurve fadeOutCurve) = 0;

    /**
     * Get current trim points for a clip (query only)
     * Thread-safe: Can be called from any thread
     */
    virtual SessionGraphError getClipTrimPoints(ClipHandle handle,
                                               int64_t& trimInSamples,
                                               int64_t& trimOutSamples) const = 0;
};

} // namespace orpheus
```

### New Error Codes

```cpp
enum class SessionGraphError {
    // ... existing errors ...

    InvalidClipTrimPoints = 18,  // Trim IN >= trim OUT, or out of bounds
    InvalidFadeDuration = 19,    // Fade duration > clip duration
    ClipNotRegistered = 20       // Clip handle not found
};
```

---

## Implementation Phases

### Phase 1: Data Model Extension (2 hours)

**File:** `src/core/transport/clip_playback_state.h`

Add trim/fade fields to `ClipPlaybackState`:

```cpp
struct ClipPlaybackState {
    // ... existing fields ...

    int64_t trimInSamples = 0;
    int64_t trimOutSamples = INT64_MAX;
    double fadeInSeconds = 0.0;
    double fadeOutSeconds = 0.0;
    FadeCurve fadeInCurve = FadeCurve::Linear;
    FadeCurve fadeOutCurve = FadeCurve::Linear;
    int64_t fadeInSamples = 0;   // Cached
    int64_t fadeOutSamples = 0;  // Cached
};
```

---

### Phase 2: API Implementation (4 hours)

**File:** `src/core/transport/transport_controller.cpp`

Implement three methods with validation:

- Find clip in map
- Validate inputs (bounds checking)
- Update with atomic operations
- Return error codes

See **ORP074 Section "Phase 2"** for complete implementation code.

---

### Phase 3: Audio Callback Integration (6 hours)

**File:** `src/core/transport/transport_controller.cpp`

Modify `processAudio()` to:

1. Read trim/fade settings (atomic load)
2. Check if position within trim range
3. Calculate fade gain (3 curve types)
4. Apply gain to audio samples

**Key Code:**

```cpp
// Fade-in: First N samples from trim IN
int64_t relativePos = filePosition - trimIn;
if (fadeInSamples > 0 && relativePos < fadeInSamples) {
    float fadeInPos = (float)relativePos / fadeInSamples;
    gain *= calculateFadeGain(fadeInPos, fadeInCurve);
}

// Fade-out: Last N samples before trim OUT
int64_t trimmedDuration = trimOut - trimIn;
if (fadeOutSamples > 0 && relativePos >= (trimmedDuration - fadeOutSamples)) {
    int64_t fadeOutRelPos = relativePos - (trimmedDuration - fadeOutSamples);
    float fadeOutPos = (float)fadeOutRelPos / fadeOutSamples;
    gain *= (1.0f - calculateFadeGain(fadeOutPos, fadeOutCurve));
}
```

See **ORP074 Section "Phase 3"** for complete audio callback code.

---

### Phase 4: OCC Integration (2 hours)

**File:** `apps/clip-composer/Source/Audio/AudioEngine.cpp`

Remove TODO comment and call SDK methods:

```cpp
// Map string curves to enum
orpheus::FadeCurve fadeInCurveEnum = orpheus::FadeCurve::Linear;
if (fadeInCurve == "EqualPower") fadeInCurveEnum = orpheus::FadeCurve::EqualPower;
else if (fadeInCurve == "Exponential") fadeInCurveEnum = orpheus::FadeCurve::Exponential;

// Call SDK methods
auto trimResult = m_transportController->updateClipTrimPoints(handle, trimInSamples, trimOutSamples);
auto fadeResult = m_transportController->updateClipFades(handle, fadeInSeconds, fadeOutSeconds,
                                                         fadeInCurveEnum, fadeOutCurveEnum);
```

See **ORP074 Section "Phase 4"** for complete integration code.

---

## Testing Requirements

### Unit Tests (Required)

**File:** `tests/transport/clip_metadata_test.cpp` (NEW)

- ✅ Update trim points → getter returns new values
- ✅ Invalid trim points (IN >= OUT, negative, out of bounds) → error
- ✅ Update fades → cached sample counts recomputed
- ✅ Invalid fade durations (negative, > clip duration) → error
- ✅ Clip not registered → ClipNotRegistered error
- ✅ Fade curve calculation (Linear, EqualPower, Exponential)

See **ORP074 Section "Testing Strategy"** for complete test code.

---

### Integration Tests (Required)

**File:** `tests/transport/clip_playback_integration_test.cpp` (MODIFY)

- ✅ Set trim IN at 2s → playback starts at 2s
- ✅ Set trim OUT at 8s (10s file) → playback stops at 8s
- ✅ Set fade-in 0.5s → verify gain ramp at start
- ✅ Set fade-out 1.0s → verify gain ramp at end
- ✅ Verify Linear, EqualPower, Exponential curves differ

See **ORP074 Section "Integration Tests"** for complete test code.

---

### Manual Testing (Required)

**Location:** OCC Application

**Test Plan:**

1. Load 10-second clip
2. Open Edit Dialog
3. Set IN at 2s, OUT at 8s
4. Set fade-in 1.0s Linear, fade-out 0.5s Exponential
5. Click OK
6. Play main clip button
7. **Verify:** Starts at 2s, stops at 8s, fades audible

**Acceptance Criteria:**

- ✅ Trim points applied (±10ms tolerance)
- ✅ Fades audible and smooth (no clicks)
- ✅ Preview matches main playback
- ✅ No crashes or glitches

---

## Files to Modify

### SDK Core (Your Responsibility)

1. `include/orpheus/transport_controller.h` - Add 3 method declarations, FadeCurve enum
2. `include/orpheus/session_graph_error.h` - Add 3 error codes
3. `src/core/transport/clip_playback_state.h` - Add trim/fade fields (internal)
4. `src/core/transport/transport_controller.cpp` - Implement 3 methods + modify processAudio()

### OCC Application (You'll Test This)

5. `apps/clip-composer/Source/Audio/AudioEngine.cpp:165-192` - Remove TODO, call SDK methods

### Tests (Your Responsibility)

6. `tests/transport/clip_metadata_test.cpp` - NEW file (unit tests)
7. `tests/transport/clip_playback_integration_test.cpp` - Add integration tests

---

## Implementation Schedule

### Day 1 (8 hours)

**Morning:**

- Read ORP074 (full spec)
- Set up feature branch: `feature/clip-metadata-management`
- Phase 1: Data Model Extension (2h)
- Phase 2: API Implementation (2h)

**Afternoon:**

- Write unit tests for Phases 1-2 (4h)
- Run tests, fix bugs

### Day 2 (8 hours)

**Morning:**

- Phase 3: Audio Callback Integration (4h)
- Test with dummy audio driver

**Afternoon:**

- Write integration tests for Phase 3 (4h)
- Run tests, fix bugs

### Day 3 (4 hours)

**Morning:**

- Phase 4: OCC Integration (2h)
- Manual testing with OCC Edit Dialog

**Afternoon:**

- Code review, documentation (2h)
- Merge PR

**Total:** 20 hours (2.5 developer-days)

---

## Definition of Done

**Before Merging:**

- [ ] All 3 methods implemented in TransportController
- [ ] Error enum extended (3 new codes)
- [ ] Unit tests pass (100% coverage)
- [ ] Integration tests pass
- [ ] Manual test plan executed (all items ✅)
- [ ] No memory leaks (valgrind clean)
- [ ] CPU overhead <5% (profile with 16 clips)
- [ ] Code reviewed by 1+ SDK team members
- [ ] Doxygen comments complete
- [ ] ORP074 updated with "Implemented" status

**After OCC v0.2.0 Release:**

- [ ] User feedback: "Main playback matches preview"
- [ ] No trim/fade bugs in first 30 days
- [ ] Feature used by >80% of beta testers

---

## Dependencies

**External Libraries:** None (uses existing SDK infrastructure)

**Blocked On:** None (all OCC work complete)

**Blocking:** OCC v0.2.0-alpha release (cannot ship without this)

**Backward Compatibility:** ✅ Fully compatible (new methods, no breaking changes)

---

## Communication

**Questions:** #orpheus-sdk-dev (Slack)

**GitHub:** Create issue with labels:

- `sdk-enhancement`
- `occ-integration`
- `priority-high`

**Point of Contact:**

- SDK Architecture: [SDK Team Lead]
- OCC Integration: [OCC Team Lead]
- Testing Strategy: [QA Lead]

**Weekly Sync:** Fridays 2pm PT (SDK + OCC teams)

---

## Additional Features (Optional)

If you finish early or want to add more value:

### Feature 1: Bulk Metadata Update (Low Priority, +2 hours)

```cpp
struct ClipMetadataUpdate {
    ClipHandle handle;
    int64_t trimInSamples;
    int64_t trimOutSamples;
    double fadeInSeconds;
    double fadeOutSeconds;
    FadeCurve fadeInCurve;
    FadeCurve fadeOutCurve;
};

SessionGraphError updateMultipleClips(const std::vector<ClipMetadataUpdate>& updates);
```

**Use Case:** User edits multiple clips in batch (same fade to all clips)

---

### Feature 2: Clip Metadata Serialization (Medium Priority, +3 hours)

```cpp
std::vector<ClipMetadataSnapshot> exportAllClipMetadata() const;
SessionGraphError importAllClipMetadata(const std::vector<ClipMetadataSnapshot>& metadata);
```

**Use Case:** Session save/load includes trim/fade settings

---

### Feature 3: Real-Time Fade Adjustment (Low Priority, +4 hours)

```cpp
SessionGraphError updateClipFadesRealTime(ClipHandle handle,
                                          double fadeInSeconds,
                                          double fadeOutSeconds);
```

**Use Case:** Adjust fade while clip is playing (DJ-style live editing)

See **ORP074 Section "Additional Features"** for full specs.

---

## Success Metrics

**Immediate (Post-Sprint):**

- ✅ OCC main playback applies edited trim/fade
- ✅ No audio dropouts (0 buffer underruns)
- ✅ CPU overhead <5%

**Long-Term (Post-v0.2.0):**

- ✅ >90% user satisfaction ("matches preview")
- ✅ No bug reports in first 30 days
- ✅ >80% feature adoption

---

## Related Work

**Already Complete:**

- ✅ OCC Edit Dialog (Phases 2 & 3)
- ✅ PreviewPlayer with fade processing
- ✅ WaveformDisplay with 2x resolution
- ✅ Transport position bar visualization
- ✅ AudioEngine infrastructure (stub method)
- ✅ MainComponent wiring (OK button)

**Future Work (Not This Sprint):**

- Recording support (OCC v1.0)
- DSP processing (Rubber Band integration)
- Remote control (OSC/MIDI)
- Advanced transport features (master/slave linking)

See **OCC029 Section "Long-Term Roadmap"** for full future plan.

---

## Risk Mitigation

| Risk                            | Mitigation                                    |
| ------------------------------- | --------------------------------------------- |
| Thread-safety issues            | Use atomic operations, extensive testing      |
| Audio glitches during fades     | Pre-compute fade samples, optimize math       |
| Performance regression          | Profile before/after, <5% overhead target     |
| Incorrect fade curve math       | Unit tests with known values, manual audition |
| OCC integration breaks features | Regression testing, beta feedback             |

---

## Reference Implementation

**PreviewPlayer.cpp:226-306** - Fade processing already working in OCC preview mode

- Demonstrates frame-by-frame gain calculation
- Three curve types implemented
- Thread-safe atomic operations
- Use as reference for SDK implementation

**Location:** `/Users/chrislyons/dev/orpheus-sdk/apps/clip-composer/Source/UI/PreviewPlayer.cpp`

---

## Final Checklist

**Before Starting:**

- [ ] Read ORP074 (full specification)
- [ ] Read SDK Sprint Summary (quick reference)
- [ ] Review PreviewPlayer.cpp (reference implementation)
- [ ] Set up feature branch
- [ ] Create GitHub issue/PR

**During Implementation:**

- [ ] Write code incrementally (Phase 1 → 2 → 3 → 4)
- [ ] Write tests alongside code (not after)
- [ ] Run tests frequently (catch bugs early)
- [ ] Profile performance (CPU overhead)
- [ ] Update Doxygen comments

**Before Merging:**

- [ ] All tests pass (unit + integration + manual)
- [ ] Code reviewed
- [ ] No memory leaks
- [ ] Documentation complete
- [ ] ORP074 marked "Implemented"

---

**Status:** ✅ **READY TO START**
**Priority:** HIGH (Blocking OCC v0.2.0)
**Timeline:** Week 7-8 (20 hours / 2.5 days)

**Questions?** Contact OCC team or post in #orpheus-sdk-dev

---

## Appendix: Quick Links

**Documentation:**

- 📄 **ORP074** (Full Spec): `/Users/chrislyons/dev/orpheus-sdk/docs/ORP/ORP074.md`
- 📄 **Sprint Summary**: `/Users/chrislyons/dev/orpheus-sdk/docs/SDK_SPRINT_SUMMARY.md`
- 📄 **OCC037** (Preview Enhancements): `/Users/chrislyons/dev/orpheus-sdk/apps/clip-composer/docs/OCC/OCC037.md`
- 📄 **OCC029** (SDK Gap Analysis): `/Users/chrislyons/dev/orpheus-sdk/apps/clip-composer/docs/OCC/OCC029.md`

**Code References:**

- 📂 **PreviewPlayer.cpp**: `apps/clip-composer/Source/UI/PreviewPlayer.cpp:226-306`
- 📂 **AudioEngine stub**: `apps/clip-composer/Source/Audio/AudioEngine.cpp:165-192`
- 📂 **TransportController**: `src/core/transport/transport_controller.cpp`

**Test Files:**

- 📂 **Unit tests**: `tests/transport/clip_metadata_test.cpp` (NEW)
- 📂 **Integration tests**: `tests/transport/clip_playback_integration_test.cpp` (MODIFY)

---

**Good luck! This is a high-impact feature that will complete the OCC Edit Dialog integration. The OCC team has built all the infrastructure—you're implementing the final piece of the puzzle.**
