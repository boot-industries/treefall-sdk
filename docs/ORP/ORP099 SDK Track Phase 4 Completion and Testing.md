# ORP099 - SDK Track: Phase 4 Completion & Testing

**Created:** 2025-10-31
**Status:** Active
**Priority:** High
**Estimated Duration:** 5-7 days
**Owner:** SDK Development Agent

---

## Purpose

Complete ORP068 Phase 4 (documentation & release preparation) and fill testing gaps from ORP074-076 SDK enhancement sprint. Prepare SDK v1.0 release candidate with full test coverage, performance validation, and production-ready documentation.

---

## Context

**Background:**

- ORP068 Phase 0-3 complete (55/104 tasks, 52.9%)
- ORP074-076 SDK enhancements complete (persistent metadata, gain control, loop mode)
- Phase 4 tasks (0/14) pending: documentation cleanup, release prep, changelog

**Blockers Resolved:**

- OCC v0.2.0 sprint complete (no SDK changes needed for OCC release)
- Shmui package removed (PR #129 merged)

**Related Documents:**

- ORP068 - Implementation Plan v2.0 (Phase 4 tasks)
- ORP074 - SDK Enhancement Sprint (planning)
- ORP076 - Implementation Report (gain/loop/metadata features)
- `.claude/archive/progress.md` - Historical progress tracking

---

## Task List

### Priority 1: Testing (Critical Path) ⚡

#### Task 1.1: Write Unit Tests for Gain Control API

**Status:** 🔴 Not Started
**Files to Create:** `tests/transport/clip_gain_test.cpp`
**Estimated Time:** 4 hours

**Requirements:**

- [ ] Test `updateClipGain(handle, gainDb)` with valid range (-96.0 to +12.0 dB)
- [ ] Test gain edge cases (0.0 dB, -inf dB, silence)
- [ ] Test invalid inputs (handle out of range, NaN, infinity)
- [ ] Test dB-to-linear conversion accuracy (`std::pow(10.0f, gainDb / 20.0f)`)
- [ ] Test gain changes during playback (no clicks/pops)
- [ ] Test concurrent gain changes across multiple clips
- [ ] Test gain persistence (survive session reload)
- [ ] All tests pass with ASan/UBSan (no memory issues)

**Acceptance Criteria:**

- 8+ test cases covering happy path + edge cases
- 100% pass rate in CI (Debug + Release builds)
- No audio thread violations (AddressSanitizer clean)

**Reference Code:**

- `include/orpheus/transport_controller.h:updateClipGain()`
- `src/core/transport/transport_controller.cpp:processAudio()` (gain application)
- Existing tests: `tests/transport/multi_clip_stress_test.cpp`

---

#### Task 1.2: Write Unit Tests for Loop Mode API

**Status:** 🔴 Not Started
**Files to Create:** `tests/transport/clip_loop_test.cpp`
**Estimated Time:** 4 hours

**Requirements:**

- [ ] Test `setClipLoopMode(handle, true)` enables looping
- [ ] Test `setClipLoopMode(handle, false)` disables looping
- [ ] Test loop boundary behavior (trim OUT → trim IN seek)
- [ ] Test `onClipLooped()` callback firing
- [ ] Test loop mode with trim points (respects IN/OUT)
- [ ] Test loop mode with fade-out (crossfade on loop point)
- [ ] Test loop mode persistence (survive session reload)
- [ ] Test edge case: loop enabled on one-shot clip (no audio file underrun)

**Acceptance Criteria:**

- 8+ test cases covering loop enable/disable + boundary conditions
- Verify sample-accurate loop point (±1 sample)
- 100% pass rate in CI
- No audio dropouts at loop boundaries

**Reference Code:**

- `include/orpheus/transport_controller.h:setClipLoopMode()`
- `src/core/transport/transport_controller.cpp:425` (loop seek logic)
- ORP076 Section 3.3 (Loop Mode Implementation)

---

#### Task 1.3: Write Unit Tests for Persistent Metadata Storage

**Status:** 🔴 Not Started
**Files to Create:** `tests/transport/clip_metadata_test.cpp`
**Estimated Time:** 3 hours

**Requirements:**

- [ ] Test metadata survives `stopClip()` → `startClip()` cycle
- [ ] Test trim points persist (IN/OUT samples)
- [ ] Test fade curves persist (IN/OUT seconds + curve type)
- [ ] Test gain persists (dB value)
- [ ] Test loop mode persists (bool flag)
- [ ] Test metadata for multiple clips (no cross-contamination)
- [ ] Test metadata cleared on clip unload

**Acceptance Criteria:**

- 6+ test cases covering all metadata fields
- Verify all fields stored in `AudioFileEntry` structure
- 100% pass rate in CI

**Reference Code:**

- `src/core/transport/transport_controller.h:AudioFileEntry` structure
- `src/core/transport/transport_controller.cpp:addActiveClip()` (load logic)
- ORP076 Section 2 (Persistent Metadata Implementation)

---

#### Task 1.4: Integration Test - 16 Simultaneous Clips with Metadata

**Status:** 🔴 Not Started
**Files to Modify:** `tests/transport/multi_clip_stress_test.cpp`
**Estimated Time:** 3 hours

**Requirements:**

- [ ] Load 16 clips with different gain settings (-12, -6, 0, +3 dB)
- [ ] Load 8 clips with loop enabled, 8 without
- [ ] Load clips with various trim points (0-50% offset)
- [ ] Start all 16 clips simultaneously
- [ ] Verify no audio dropouts (sample-accurate timing)
- [ ] Verify CPU usage <30% (Intel i5 8th gen baseline)
- [ ] Verify memory stable (no leaks over 60 seconds)
- [ ] Verify all clips playing with correct gain/loop/trim

**Acceptance Criteria:**

- Test runs for 60 seconds without dropouts
- CPU usage logged and within budget
- All 16 clips audible with correct metadata applied
- Pass on macOS, Windows, Linux (CI matrix)

**Reference Code:**

- Existing: `tests/transport/multi_clip_stress_test.cpp`
- Performance targets: OCC100 (Performance Requirements)

---

### Priority 2: Performance Validation

#### Task 2.1: Profile 16-Clip Scenario

**Status:** 🔴 Not Started
**Tools:** Instruments (macOS), perf (Linux), VS Profiler (Windows)
**Estimated Time:** 4 hours

**Requirements:**

- [ ] Run `multi_clip_stress_test` with profiler attached
- [ ] Measure CPU usage per thread (audio thread vs UI thread)
- [ ] Identify hotspots (>5% CPU in single function)
- [ ] Measure audio callback duration (target: <10ms @ 512 samples, 48kHz)
- [ ] Measure memory allocations (target: ZERO on audio thread)
- [ ] Generate performance report (CSV + flamegraph)

**Acceptance Criteria:**

- Audio thread CPU <25% (leaves headroom for host application)
- No allocations detected on audio thread (ASan + profiler)
- Callback duration <10ms (99.9th percentile)
- Report saved to `docs/ORP/performance_report_16clips.md`

**Commands:**

```bash
# macOS
instruments -t "Time Profiler" ./build/tests/transport/multi_clip_stress_test

# Linux
perf record -g ./build/tests/transport/multi_clip_stress_test
perf report

# Windows
vsperf /launch:multi_clip_stress_test.exe
```

---

#### Task 2.2: Validate Latency Targets

**Status:** 🔴 Not Started
**Estimated Time:** 2 hours

**Requirements:**

- [ ] Measure round-trip latency (button press → audio out)
- [ ] Test with 128, 256, 512, 1024 sample buffers
- [ ] Verify <5ms latency @ 512 samples (target for OCC)
- [ ] Document latency vs buffer size tradeoff
- [ ] Test on reference hardware (Intel i5 8th gen)

**Acceptance Criteria:**

- Latency report with graph (buffer size vs milliseconds)
- 512 samples @ 48kHz = 10.67ms buffer + <5ms processing = <16ms total
- Report saved to `docs/ORP/latency_validation.md`

---

### Priority 3: Documentation (Phase 4 Tasks)

#### Task 3.1: Regenerate API Documentation (Doxygen)

**Status:** 🔴 Not Started
**Estimated Time:** 1 hour

**Requirements:**

- [ ] Update Doxygen comments for `updateClipGain()`
- [ ] Update Doxygen comments for `setClipLoopMode()`
- [ ] Regenerate HTML docs: `doxygen Doxyfile`
- [ ] Verify all new APIs appear in `docs/html/index.html`
- [ ] Fix any Doxygen warnings

**Acceptance Criteria:**

- Zero Doxygen warnings
- Gain control API documented with parameter ranges
- Loop mode API documented with behavior notes
- HTML output generated in `docs/html/`

**Commands:**

```bash
cd /Users/chrislyons/dev/orpheus-sdk
doxygen Doxyfile
open docs/html/index.html
```

---

#### Task 3.2: Create SDK Migration Guide (v0 → v1)

**Status:** 🔴 Not Started
**Files to Create:** `docs/MIGRATION_v0_to_v1.md`
**Estimated Time:** 3 hours

**Requirements:**

- [ ] Document breaking changes (if any)
- [ ] Document new APIs (gain control, loop mode, persistent metadata)
- [ ] Provide code examples (before/after snippets)
- [ ] Document deprecated APIs (if any)
- [ ] Add migration checklist for downstream users

**Acceptance Criteria:**

- Migration guide covers all ORP074-076 changes
- Code examples compile and run
- Clear upgrade path for users on SDK v0.x

**Reference:**

- `packages/contract/MIGRATION.md` (contract migration template)

---

#### Task 3.3: Consolidate Architecture Documentation

**Status:** 🔴 Not Started
**Files to Update:** `docs/ARCHITECTURE.md`, `docs/ROADMAP.md`
**Estimated Time:** 2 hours

**Requirements:**

- [ ] Update `ARCHITECTURE.md` with metadata storage pattern
- [ ] Update transport layer diagram (add persistent storage)
- [ ] Update `ROADMAP.md` with Phase 4 completion status
- [ ] Archive deprecated docs to `docs/deprecated/`
- [ ] Create `docs/INDEX.md` with all active documentation

**Acceptance Criteria:**

- Architecture docs reflect current implementation (ORP076 changes)
- Roadmap shows Phase 0-3 complete, Phase 4 in progress
- No orphaned/outdated docs in `docs/` root

---

#### Task 3.4: Generate Changelog for SDK v1.0

**Status:** 🔴 Not Started
**Files to Create:** `CHANGELOG.md`
**Estimated Time:** 2 hours

**Requirements:**

- [ ] Extract all commits since last SDK release (or initial commit)
- [ ] Categorize changes: Added, Changed, Fixed, Deprecated, Removed
- [ ] Highlight breaking changes (if any)
- [ ] Link to relevant ORP documents (ORP074, ORP076, etc.)
- [ ] Follow Keep a Changelog format (https://keepachangelog.com/)

**Acceptance Criteria:**

- Changelog covers all SDK changes (Phase 0-4)
- Version tagged as `v1.0.0` (or `v1.0.0-rc.1` for release candidate)
- Changelog committed to repo root

**Commands:**

```bash
git log --oneline --since="2025-10-01" > changelog_raw.txt
# Manually categorize into CHANGELOG.md
```

---

#### Task 3.5: Update README with Quick Start Guide

**Status:** 🔴 Not Started
**Files to Update:** `README.md`
**Estimated Time:** 1 hour

**Requirements:**

- [ ] Add "Quick Start" section with build commands
- [ ] Add "Running Tests" section with ctest examples
- [ ] Add "New Features" section highlighting ORP074-076 APIs
- [ ] Update build badges (CI status)
- [ ] Add link to full documentation (`docs/`)

**Acceptance Criteria:**

- New users can build SDK from README instructions
- Quick Start takes <5 minutes to complete
- Links to detailed docs are accurate

---

### Priority 4: Release Preparation

#### Task 4.1: Create Release Candidate Branch

**Status:** 🔴 Not Started
**Estimated Time:** 30 minutes

**Requirements:**

- [ ] Create branch `release/v1.0.0-rc.1` from `main`
- [ ] Update version numbers in `CMakeLists.txt`
- [ ] Update version in `package.json` (TypeScript packages)
- [ ] Tag commit with `v1.0.0-rc.1`

**Commands:**

```bash
git checkout main
git pull origin main
git checkout -b release/v1.0.0-rc.1
# Update version numbers (manual edit)
git add .
git commit -m "chore: bump version to v1.0.0-rc.1"
git tag v1.0.0-rc.1
git push origin release/v1.0.0-rc.1 --tags
```

---

#### Task 4.2: Run Full CI/CD Validation

**Status:** 🔴 Not Started
**Estimated Time:** 1 hour (CI runtime)

**Requirements:**

- [ ] Trigger CI pipeline on release branch
- [ ] Verify all jobs pass (C++ build, TS build, lint, tests)
- [ ] Verify performance budgets pass
- [ ] Verify security audit clean (no high/critical vulnerabilities)
- [ ] Verify dependency check clean (no circular deps)

**Acceptance Criteria:**

- All CI jobs green on release branch
- No regressions from main branch
- Release branch ready for merge

---

#### Task 4.3: Create GitHub Release

**Status:** 🔴 Not Started
**Estimated Time:** 30 minutes

**Requirements:**

- [ ] Draft GitHub release for `v1.0.0-rc.1`
- [ ] Attach changelog excerpt
- [ ] Attach build artifacts (if applicable)
- [ ] Mark as "pre-release" (release candidate)
- [ ] Publish release

**Commands:**

```bash
gh release create v1.0.0-rc.1 \
  --title "SDK v1.0.0 Release Candidate 1" \
  --notes-file CHANGELOG.md \
  --prerelease
```

---

## Success Criteria (Track Complete)

- [ ] All Priority 1 tests written and passing (gain, loop, metadata, 16-clip integration)
- [ ] Performance validated (<30% CPU, <5ms latency, zero allocations)
- [ ] Documentation complete (Doxygen, migration guide, architecture, changelog, README)
- [ ] Release candidate tagged and published (`v1.0.0-rc.1`)
- [ ] CI/CD green on release branch
- [ ] Phase 4 marked complete in ORP068 tracking (14/14 tasks)

---

## Dependencies

**External:**

- None (all SDK work is self-contained)

**Internal:**

- OCC v0.2.0 release (parallel track, no blocking dependencies)

**Coordination:**

- If OCC needs SDK changes, pause this track and address OCC blockers first

---

## Next Steps After Completion

1. **SDK v1.0 Final Release** - Promote RC to stable after 1 week testing period
2. **OCC Integration** - Integrate SDK v1.0 into OCC codebase
3. **Advanced Features** - Resume ORP068 Phase 5+ (plugin system, MIDI, automation)

---

## Notes for Agent

**Working Directory:** `/Users/chrislyons/dev/orpheus-sdk` (repo root)

**Build Commands:**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

**Test Execution:**

```bash
# Single test
./build/tests/transport/clip_gain_test

# All transport tests
ctest --test-dir build -R transport --output-on-failure
```

**Documentation Generation:**

```bash
doxygen Doxyfile  # Regenerate API docs
```

**When Blocked:**

- Check `.claude/archive/progress.md` for historical context
- Reference ORP076 for implementation details
- Ask user for clarification (don't assume missing requirements)

---

## References

[1] ORP068 Implementation Plan v2.0: `docs/integration/ORP068 Implementation Plan v2.0...md`
[2] ORP076 Implementation Report: `docs/ORP/ORP076.md`
[3] OCC100 Performance Requirements: `apps/clip-composer/docs/OCC/OCC100.md`
[4] Archived Progress Tracking: `.claude/archive/progress.md`

---

**Created:** 2025-10-31
**Last Updated:** 2025-10-31
**Status:** Active - Ready for SDK development agent
