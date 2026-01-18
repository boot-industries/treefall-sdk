# OCC140: ORP121 Phase 4 OCC Tasks Handoff

**Status:** Pending
**Date:** 2026-01-18
**Parent:** ORP121 Audio Backend Refactoring Master Plan
**Branch:** `feature/orp121-audio-backend-refactoring`

---

## Context

ORP121 Audio Backend Refactoring Phase 4 (Quality Improvements) is complete for SDK core. Three OCC-specific issues remain that were identified during the audio backend audit but belong to the Clip Composer application layer rather than the SDK.

---

## Remaining OCC Issues

### Q-10: Cue Buss Dynamic Allocation

**Location:** `apps/clip-composer/Source/Audio/AudioEngine.cpp:537-579`

**Problem:** Cue buss allocation may cause memory fragmentation during runtime allocation/deallocation cycles.

**Goal:** Implement pool-based or pre-allocated cue buss management to ensure broadcast-safe operation (no allocations during playback).

**Approach Options:**
1. Pre-allocate fixed pool of cue busses at initialization
2. Use object pool pattern with recycling
3. Lazy allocation with no deallocation (grow-only)

---

### Q-11: Hard-Coded Button Limits

**Location:** `apps/clip-composer/Source/Audio/AudioEngine.h:308`

**Problem:** Current limit is 384 clips, but OCC requires 960 (10×12 buttons × 8 tabs per the product specification).

**Goal:** Increase limit to 960 or make configurable.

**Considerations:**
- Memory impact of 960 vs 384 clip slots
- Whether limit should be compile-time or runtime configurable
- Impact on existing session files

---

### Q-13: Incomplete Doxygen Coverage

**Scope:** OCC-specific headers in `apps/clip-composer/Source/`

**Goal:** Add Doxygen comments to public APIs for documentation generation.

**Priority Files:**
- `Audio/AudioEngine.h` - Core audio integration
- `Session/SessionManager.h` - Session persistence
- `ClipGrid/ClipButton.h` - UI component API

---

## Reference Documents

| Document | Location | Description |
|----------|----------|-------------|
| ORP121 Master Plan | `docs/orp/ORP121 Audio Backend Refactoring Master Plan.md` | Full issue registry and implementation plan |
| Phase 4 Report | `docs/orp/ORP122 Phase 4 Quality Improvements Implementation Report.md` | Completed SDK work |
| Progress Tracking | `.claude/implementation_progress.md` | Current sprint status |
| OCC Architecture | `apps/clip-composer/CLAUDE.md` | Application development guide |

---

## SDK Work Completed (Available to OCC)

Phase 4 SDK improvements now available for OCC integration:

### Q-03: Sample Rate Parameterization
- `RoutingConfig.sample_rate` field added (default: 48000 Hz)
- Enables 44.1 kHz, 96 kHz support

### Q-04: True-Peak Metering (ITU-R BS.1770-4)
- `TruePeakMeter` class with 4x oversampling
- 48-tap polyphase FIR interpolation filter
- Detects inter-sample peaks with ~0.1 dB accuracy

### Q-05: Headroom Management Modes
- `HeadroomMode` enum: None, PerGroup, Global, Logarithmic
- Automatic gain reduction when summing multiple channels
- Broadcast-standard logarithmic mode available

### Q-07: Lock-Free Callback Queue
- SPSC ring buffer replaces mutex-based queue
- Verified with 6 stress tests
- No priority inversion risk

---

## Build Notes

- shmui library linking fixed in `apps/clip-composer/tests/CMakeLists.txt`
- All SDK tests passing (27 routing + 6 callback stress tests)
- Use `./scripts/relaunch-occ.sh` to rebuild and launch Clip Composer

---

## Deferred Items (Not In Scope)

The following ORP121 issues are deferred to a future major version:

- **Q-01:** Standardize case style (snake_case) - breaking API change
- **Q-02:** Normalize terminology (group→bus, handle→id) - breaking API change

These will be addressed when preparing SDK v2.0.

---

## Acceptance Criteria

### Q-10: Cue Buss Dynamic Allocation
- [ ] No runtime allocations during playback
- [ ] Cue buss pool pre-allocated at AudioEngine initialization
- [ ] Memory usage documented

### Q-11: Hard-Coded Button Limits
- [ ] Limit increased to 960 clips minimum
- [ ] Existing sessions load correctly
- [ ] Memory impact assessed and documented

### Q-13: Incomplete Doxygen Coverage
- [ ] All public methods in AudioEngine.h documented
- [ ] All public methods in SessionManager.h documented
- [ ] Doxygen generates without warnings

---

*Document created: 2026-01-18*
