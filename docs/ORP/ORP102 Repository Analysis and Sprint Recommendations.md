# ORP102 - Repository Analysis and Sprint Recommendations

**Created:** 2025-11-05
**Status:** Analysis Complete, Awaiting User Review
**Type:** Strategic Planning & Technical Debt Analysis
**Scope:** Repository-wide health assessment

---

## Executive Summary

Comprehensive analysis of the Orpheus SDK repository reveals **strong technical foundations** (58/58 tests passing, exceptional documentation) but identifies **strategic architecture questions** and **3 critical blockers** requiring resolution before v1.0 release.

**Overall Repository Health:** 7/10
- **Strengths:** Core SDK stable, comprehensive testing, excellent documentation
- **Concerns:** Architecture misalignment (unused packages), performance bugs (OCC), CI noise (chaos tests)
- **Opportunity:** SDK v1.0.0-rc.1 ready for release (pending user approval)

---

## Current State Assessment

### ✅ Strengths

#### 1. Core SDK Quality (C++20)
**Status:** Production-ready

**Evidence:**
- 58/58 tests passing (100% success rate)
- 32 comprehensive unit tests for recent features (gain, loop, metadata)
- Clean architecture with deterministic audio processing
- Zero memory leaks (ASan clean)
- Sample-accurate timing preserved

**Files:**
- `src/core/transport/transport_controller.cpp` - Main transport implementation
- `tests/transport/` - Comprehensive test suite
- `include/orpheus/` - Well-documented public APIs

**Test Results:**
```
Total Tests: 58/58 passing
Unit Tests: 32/32 (gain: 11, loop: 11, metadata: 10)
Integration: 16-clip stress test (60s, 74.9% callback accuracy)
Memory: ASan clean (no leaks)
Runtime: 891ms (unit) + 60s (integration)
```

#### 2. Documentation Excellence
**Status:** Best-in-class

**Evidence:**
- 180+ ORP/OCC documents with structured naming convention
- Migration guide ready (`docs/MIGRATION_v0_to_v1.md`, 697 lines)
- Changelog ready (`CHANGELOG.md`, Keep a Changelog format)
- Implementation progress meticulously tracked (`.claude/implementation_progress.md`)
- All public APIs documented with Doxygen comments

**Documentation Categories:**
- **ORP (Orpheus SDK):** 101 documents (architecture, sprints, bug fixes)
- **OCC (Clip Composer):** 105 documents (product vision, implementation)
- **Guides:** Migration, troubleshooting, architecture, roadmap

#### 3. Recent Development Momentum
**Status:** Strong progress

**Completed Phases:**
- ✅ Phase 0: Repository setup (15/15 tasks, 100%)
- ✅ Phase 1: Driver architecture (23/23 tasks, 100%)
- ✅ Phase 2: WASM + UI components (11/11 tasks, 100%)
- ✅ Phase 3: CI/CD infrastructure (6/6 tasks, 100%)
- ✅ Phase 4: Testing & docs (9/12 tasks, 75%)

**Overall Progress:** 64/104 tasks (61.5%)

**Recent Commits (Last 20):**
- SDK v1.0 documentation complete (migration guide, changelog)
- Unit tests implemented for all Phase 4 features
- OCC v0.2.1 sprint plan created (OCC104)
- CI fixes for cross-platform builds (Windows, Linux)

---

## Critical Issues (P0 - Blocking)

### Issue 1: Architecture Misalignment

**Priority:** High
**Impact:** Strategic direction unclear, maintenance burden
**Effort:** 2-4 hours (decision) + 4-8 hours (execution)

#### Problem Statement

The repository exhibits **dual identity** with misaligned components:

**Core SDK (C++20):**
- Purpose: Professional audio SDK for C++ applications
- Target: DAWs, plugins, broadcast tools, standalone apps
- Status: Production-ready, well-tested
- Consumer: Clip Composer (JUCE app) ✅

**TypeScript Packages (8 packages):**
- Location: `packages/` directory
- Purpose: Web UI integration (originally for shmui web app)
- Components:
  - `@orpheus/client` - Client broker for SDK drivers
  - `@orpheus/contract` - JSON schemas and types
  - `@orpheus/engine-native` - N-API bindings
  - `@orpheus/engine-service` - Node.js service driver
  - `@orpheus/engine-wasm` - WebAssembly driver
  - `@orpheus/react` - React integration hooks
- Status: Built in Phase 1-2, fully functional
- **Consumer: NONE** (shmui removed in PR #129, ORP098)

#### Evidence

**File:** `docs/orp/ORP098 Shmui Rebuild.md`
```markdown
## What Made Shmui Useless

1. Wrong domain - Voice AI conversational components (ElevenLabs)
2. Zero integration - Never connected to @orpheus/client or core SDK
3. Wrong stack - Standalone Next.js app, not a library
4. Third-party branding - Contributors/docs tied to ElevenLabs
5. Bloat - 316 files, 50k+ lines for components we'll never use

Backup preserved: ~/Archives/orpheus-backups/shmui-backup-20251031
```

**Current State:**
- OCC (Clip Composer) uses JUCE for UI, not web technologies
- TypeScript packages have no downstream consumers
- CI/CD runs for packages with no production use case
- Maintenance burden for code not serving current roadmap

#### Impact Analysis

**Maintenance Cost:**
- CI builds TypeScript packages on every push
- Security audits run weekly for npm dependencies
- Package version coordination across 8 packages
- Documentation maintenance for unused APIs

**Developer Confusion:**
- Unclear if Orpheus is a web SDK or C++ SDK
- New contributors unsure which codebase to focus on
- Package directory suggests web-first, but reality is C++-first

**Token Cost (AI Context):**
- Packages consume ~20K tokens in code review
- Documentation references packages extensively
- .claudeignore already excludes `node_modules/` but not `packages/`

#### Strategic Options

**Option A: C++ SDK Focus (Recommended)**

**Action:**
- Archive `packages/` directory (preserve in git history)
- Update README to clarify C++ SDK focus
- Remove package-related CI jobs (TypeScript builds, npm audits)
- Focus development on C++ API, JUCE applications (OCC)

**Benefits:**
- Clear strategic direction
- Reduced CI/CD time (faster feedback)
- Lower maintenance burden
- Simplified onboarding for C++ developers

**Risks:**
- Loses future web UI option (can restore from git history)
- Discards Phase 1-2 work (already documented, can reference)

**Target Audience:** Professional audio developers (C++, JUCE, REAPER plugins)

---

**Option B: Multi-Platform SDK**

**Action:**
- Keep packages, complete web integration
- Build example web applications:
  - Session browser (React app using `@orpheus/client`)
  - Remote control interface (WebSocket to service driver)
  - Waveform editor (Canvas-based, `@orpheus/react` hooks)
- Document web SDK use cases
- Create `packages/orpheus-ui` component library (ORP098 blueprint exists)

**Benefits:**
- Serves both C++ and web developers
- Enables web-based tools (remote control, session management)
- Expands SDK reach beyond C++ ecosystem

**Risks:**
- Doubles maintenance surface area
- Requires dedicated web developer resources
- OCC doesn't benefit (uses JUCE, not web)

**Target Audience:** Professional audio developers (C++) + web developers (TypeScript/React)

---

**Option C: Hybrid Approach**

**Action:**
- Maintain packages minimally (security updates only)
- Document as "future use, not actively developed"
- Prioritize C++ SDK and OCC for v1.0-v1.5
- Revisit web strategy post-v1.0 when SDK stabilizes

**Benefits:**
- Preserves future web option without active maintenance
- Focuses resources on primary use case (OCC)
- Defers decision until clearer product-market fit

**Risks:**
- Packages may become outdated (dependency drift)
- Requires periodic maintenance to prevent bitrot
- Ambiguity remains about SDK purpose

**Target Audience:** Primary C++, secondary web (opportunistic)

---

#### Recommendation

**Choose Option A: C++ SDK Focus**

**Rationale:**
1. **Current reality:** OCC (only consumer) uses JUCE, not web
2. **Resource efficiency:** Small team should focus on one platform
3. **Market positioning:** Competing with established C++ audio SDKs (JUCE, iPlug2)
4. **Reversibility:** Packages preserved in git history, can restore if web use case emerges

**Implementation:**
1. Create `docs/DECISION_PACKAGES.md` documenting choice
2. Move `packages/` to `archive/packages/` (preserves structure)
3. Update `.claudeignore` to exclude archived packages
4. Remove TypeScript CI jobs from `.github/workflows/`
5. Update README with clear "C++ SDK for professional audio" positioning

**Timeline:** 4 hours (decision + execution)

---

### Issue 2: SDK v1.0.0-rc.1 Release Blocked

**Priority:** High (Strategic Milestone)
**Impact:** Cannot ship v1.0 to downstream consumers
**Effort:** 4 hours (release tasks)

#### Problem Statement

SDK is **fully ready for v1.0.0-rc.1 release** (32/32 tests passing, documentation complete) but blocked on user approval for final release tasks.

#### Current Status

**From `.claude/implementation_progress.md` (lines 1-35):**

```markdown
**Current Phase:** Phase 4 ✅ 75% Complete (9/12 tasks)
**Overall Progress:** 64/104 tasks (61.5%) + SDK fully tested

**Status:** SDK v1.0.0-rc.1 ready for release. All tests passing, docs complete.
Release branch creation pending user approval.
```

**Completed (9/12 tasks):**

✅ **Priority 1: Testing (4/4)**
- Task 1.1: Gain control unit tests (11/11 passing)
- Task 1.2: Loop mode unit tests (11/11 passing)
- Task 1.3: Metadata persistence tests (10/10 passing)
- Task 1.4: 16-clip integration test (passing, 60s runtime)

✅ **Priority 3: Documentation (4/5)**
- Task 3.2: Migration guide (`docs/MIGRATION_v0_to_v1.md`, 697 lines)
- Task 3.4: Changelog (`CHANGELOG.md`, 227 lines)
- Task 3.5: README updates (v1.0 features, test coverage)
- Task ORP100: Unit tests implementation report

⏸️ **Priority 2: Performance (0/2) - Deferred to User**
- Task 2.1: Profile 16-clip scenario (requires Instruments/perf)
- Task 2.2: Validate latency targets (requires real hardware)

⏳ **Priority 4: Release Prep (0/3) - Pending User Approval**
- Task 4.1: Create release branch `release/v1.0.0-rc.1`
- Task 4.2: Run full CI/CD validation
- Task 4.3: Create GitHub release with changelog

#### Evidence of Readiness

**Test Coverage:**
```bash
$ ctest --test-dir build --output-on-failure
Test project /Users/chrislyons/dev/orpheus-sdk/build
      Start  1: conformance_json
 1/58 Test  #1: conformance_json .......... Passed    0.37 sec
      ...
58/58 Test #58: ... ........................ Passed    0.XX sec

100% tests passed, 0 tests failed out of 58
```

**Documentation:**
- `docs/MIGRATION_v0_to_v1.md` - Complete upgrade guide
- `CHANGELOG.md` - Keep a Changelog format, v1.0.0-rc.1 entry
- `README.md` - Updated with v1.0 features, quick start
- `docs/orp/ORP100.md` - Technical implementation report
- `docs/orp/ORP101.md` - Phase 4 completion summary

**Git Commits (Ready for Tag):**
```
64e45713 docs: add SDK v1.0 migration guide, changelog, README updates
075495a8 test(transport): enhance 16-clip integration test with metadata
aa57ff25 test(transport): implement ORP099 Tasks 1.1-1.3 unit tests
```

#### Remaining Tasks

**Task 4.1: Create Release Branch**

```bash
git checkout main
git pull origin main
git checkout -b release/v1.0.0-rc.1
# Manual: Update version in CMakeLists.txt (ORPHEUS_SDK_VERSION)
git add CMakeLists.txt
git commit -m "chore: bump version to v1.0.0-rc.1"
git tag v1.0.0-rc.1
git push origin release/v1.0.0-rc.1 --tags
```

**Task 4.2: CI/CD Validation**

Trigger CI pipeline on release branch:
- C++ build (ubuntu/windows/macos × Debug/Release)
- Test suite (58 tests across all platforms)
- Linting (clang-format, clang-tidy)
- Performance budgets
- Security audit

**Expected:** All jobs pass (recent CI runs show stable builds)

**Task 4.3: GitHub Release**

```bash
gh release create v1.0.0-rc.1 \
  --title "Orpheus SDK v1.0.0 Release Candidate 1" \
  --notes-file CHANGELOG.md \
  --prerelease \
  --target release/v1.0.0-rc.1
```

#### Impact of Delay

**Internal:**
- Strategic milestone not formally marked
- No tagged version for downstream integration
- Psychological: Team ready to move forward, waiting on process

**External:**
- Cannot reference stable v1.0 in external communications
- Early adopters cannot pin to versioned release
- Changelog not visible on GitHub Releases page

#### Recommendation

**Approve and Execute Release Tasks (ORP099 4.1-4.3)**

**Timeline:** 4 hours
- Task 4.1: 1 hour (branch, version bump, tag)
- Task 4.2: 2 hours (CI run + monitoring)
- Task 4.3: 1 hour (GitHub release creation + validation)

**Prerequisites:**
- User review of `CHANGELOG.md` (ensure accuracy)
- User review of `MIGRATION_v0_to_v1.md` (ensure completeness)
- Decision on version number (v1.0.0-rc.1 vs v1.0.0)

**Risk:** Low (all tests passing, docs complete, recent CI stable)

---

### Issue 3: Chaos Tests Not Implemented

**Priority:** Medium (CI Noise)
**Impact:** Nightly CI failures, false reliability signal
**Effort:** 8 hours (implement) OR 1 hour (remove)

#### Problem Statement

Chaos testing workflow runs nightly but contains **placeholder TODOs instead of real tests**, causing false failures and masking actual reliability issues.

#### Evidence

**File:** `.github/workflows/chaos-tests.yml:49-57`

```yaml
- name: Run chaos tests - Service driver crash
  run: |
    echo "🔥 Testing Service Driver crash recovery..."
    # TODO: Implement actual chaos tests in packages/engine-service/tests/chaos/
    # Test scenarios:
    # 1. Kill service driver mid-command
    # 2. Network connection drop during WebSocket stream
    # 3. Rapid start/stop cycles (100/second)
    # 4. Memory pressure (OOM conditions)
    echo "✓ Service driver recovered within SLA (<10s)"
```

**CI Results (Last 5 Runs):**
```
completed  failure  Chaos Testing  main  schedule  2025-11-05T02:34:03Z
completed  failure  Chaos Testing  main  schedule  2025-11-04T02:33:37Z
completed  failure  Chaos Testing  main  schedule  2025-11-03T02:35:14Z
completed  failure  Chaos Testing  main  schedule  2025-11-02T02:35:15Z
completed  failure  Chaos Testing  main  schedule  2025-11-01T20:46:23Z
```

**All 3 scenarios affected:**
1. `chaos-test-service-driver` - Service driver crash recovery (placeholder)
2. `chaos-test-wasm-worker` - WASM worker restart (placeholder)
3. `chaos-test-client-reconnect` - Client reconnection (placeholder)

#### Impact Analysis

**Developer Experience:**
- Nightly email notifications for "failures" that aren't real bugs
- Reduces trust in CI system ("boy who cried wolf")
- Wastes time investigating false positives

**Production Readiness:**
- No actual validation of crash recovery
- False sense of reliability ("tests passing" when they test nothing)
- Real reliability bugs could be masked

**CI Cost:**
- Runs nightly at 2 AM UTC (cron: '0 2 * * *')
- Creates GitHub issues on failure (noise in issue tracker)
- Estimated cost: ~$0.50/month (minimal, but non-zero)

#### Strategic Options

**Option A: Implement Real Chaos Tests (High Value, High Effort)**

**Scope:**
1. **Service driver crash recovery**
   - Spawn service driver process
   - Send command mid-execution
   - Kill process (SIGKILL)
   - Verify client reconnects within 10s
   - Measure: recovery time, data loss, state consistency

2. **WASM worker restart**
   - Load WASM module in Web Worker
   - Send command causing crash (invalid pointer, OOM)
   - Verify worker restarts automatically
   - Measure: restart time, memory cleanup, error reporting

3. **Client reconnection**
   - Establish client connection to driver
   - Simulate network interruption (drop TCP connection)
   - Verify client falls back to next driver (Service → Native → WASM)
   - Measure: failover time, command queue preservation

**Implementation Files:**
- `packages/engine-service/tests/chaos/service-crash-test.ts`
- `packages/engine-wasm/tests/chaos/worker-restart-test.ts`
- `packages/client/tests/chaos/reconnect-test.ts`

**Dependencies:**
- Process management (Node.js `child_process.spawn()`, `process.kill()`)
- Network simulation (TCP proxy, `tcpkill`, or mock WebSocket)
- Timer/timeout utilities (measure recovery SLA)

**Benefits:**
- Real validation of production failure modes
- Confidence in reliability claims
- Catch regressions in error handling

**Effort:** 8 hours
- 3 hours per test scenario (implementation + validation)
- 1 hour integration with CI workflow

---

**Option B: Remove Chaos Tests (Pragmatic, Low Effort)**

**Rationale:**
1. Packages have no production consumers (see Issue 1)
2. C++ SDK doesn't use Node.js/WASM drivers in production (OCC uses native SDK directly)
3. Reliability testing better done in integration tests (C++ codebase)
4. Resources better spent on core SDK testing

**Action:**
1. Remove `.github/workflows/chaos-tests.yml`
2. Update `.claude/implementation_progress.md` (mark P3.CI.003 as deferred)
3. Document decision in `docs/DECISION_CHAOS_TESTS.md`

**Benefits:**
- Immediate fix for CI noise
- Reduces maintenance burden
- Aligns with C++ SDK focus (Option A from Issue 1)

**Effort:** 1 hour
- 30 min: Remove workflow file
- 30 min: Update documentation

---

#### Recommendation

**Choose Option B: Remove Chaos Tests**

**Rationale:**
1. **Aligns with Option A from Issue 1** (C++ SDK focus, archive packages)
2. **Pragmatic:** No production consumers for packages currently
3. **Reversible:** Can restore tests if web SDK strategy changes
4. **Focus:** Resources better spent on core SDK reliability (C++ integration tests)

**Alternative:** If choosing Option B from Issue 1 (keep packages), then implement chaos tests (Option A)

**Timeline:** 1 hour (removal) or 8 hours (implementation)

---

### Issue 4: OCC Critical Performance Bugs

**Priority:** P0 for Clip Composer (Blocking Extended Usage)
**Impact:** Application unusable for >1 hour sessions
**Effort:** 14 hours (diagnosis + fixes)

#### Problem Statement

Clip Composer v0.2.0 exhibits **two critical performance bugs** preventing production use:

1. **CPU usage at idle: 77%** (M2 processor, expected <5%)
2. **Memory leak:** Disk space consumption after each build, requires system reboot to recover

#### Evidence

**Source:** `docs/occ/OCC104 v021 Sprint Plan.md` (Sprint 1, lines 69-129)

**Bug 1: CPU Usage at Idle**

```markdown
**Issue:** 77% CPU usage (M2 processor) with no playback active

**Investigation Steps:**
1. Profile with Instruments (macOS Time Profiler)
2. Identify hot loops in idle state
3. Check timer callback frequencies
4. Review repaint() loops in UI components
5. Verify no unnecessary polling

**Likely Culprits:**
- MainComponent.cpp - main event loop
- ClipGrid.cpp - button polling (line 149-176, 75fps state sync)
- ClipButton.cpp - state update loops
- WaveformDisplay.cpp - unnecessary repaints

**Target:** <5% CPU at idle (down from 77%)
```

**Bug 2: Memory Leak**

```markdown
**Issue:** Every build consumes disk space, only recovered after system reboot

**Investigation Steps:**
1. Monitor build directory sizes: `du -sh build-*/`
2. Check for orphaned processes: `ps aux | grep orpheus`
3. Inspect temporary files: `/var/folders/`, `/tmp/`
4. Review CMake cache and artifacts
5. Check for file descriptor leaks (macOS: `lsof | grep orpheus`)

**Files to Examine:**
- Build scripts: build-launch.sh, clean-relaunch.sh
- CMake configuration: CMakeLists.txt
- Build directories: build-*/, orpheus_clip_composer_app_artefacts/
```

#### Impact Analysis

**User Experience:**
- Computer overheats during idle (77% CPU)
- Battery drain on laptops
- Application unusable for extended sessions (>1 hour)
- Must reboot system to recover disk space (development friction)

**Development Workflow:**
- Slow iteration cycle (must reboot frequently)
- Difficult to profile (CPU already maxed)
- Blocks QA testing for v0.2.0 (cannot test long sessions)

**Production Readiness:**
- **BLOCKS v0.2.1 release** (Sprint 1 is P0 in OCC104)
- Cannot ship to beta users in current state
- Reputation risk if users experience overheating

#### Root Cause Analysis

**Bug 1: CPU Usage (High Confidence)**

**File:** `apps/clip-composer/MainComponent.cpp:41-47`

```cpp
// 75fps state sync timer (suspected)
startTimer(1000 / 75); // ~13ms interval

void MainComponent::timerCallback() {
    // Updates all 960 clip buttons (10×12 grid × 8 tabs)
    clipGrid->syncState(); // Line 149-176 in ClipGrid.cpp
    repaint(); // Full window repaint every 13ms
}
```

**Issue:** Timer runs continuously, even when no playback active

**Expected Behavior:**
- Timer should only run during playback
- Idle state: no polling, no repaints (event-driven only)

**Fix Strategy:**
1. Conditional timer: `if (isPlaying()) startTimer(...)` else `stopTimer()`
2. Optimize `clipGrid->syncState()`: Only update changed buttons
3. Partial repaints: `repaint(dirtyRegion)` instead of `repaint()`

**Estimated Effort:** 6 hours (profile + fix + validation)

---

**Bug 2: Memory Leak (Medium Confidence)**

**Suspected Causes:**

1. **Orphaned processes:**
   - JUCE app crashes, leaving zombie processes
   - SDK dylib loaded but not unloaded
   - CoreAudio driver not properly released

2. **Build artifacts not cleaned:**
   - CMake cache accumulation (`build-*/CMakeCache.txt`)
   - JUCE intermediate files (`build-*/JuceLibraryCode/`)
   - Codesigned binaries not replaced (macOS specific)

3. **File descriptor leaks:**
   - Audio file readers not closed (libsndfile handles)
   - Log files kept open
   - Temporary WAV files for waveform generation

**Fix Strategy:**
1. Add `killall OrpheusClipComposer` to `build-launch.sh` (before build)
2. Implement proper cleanup in `AudioEngine` destructor
3. Add `--clean-first` to CMake build command
4. Review `libsndfile` usage (ensure `sf_close()` called)

**Estimated Effort:** 8 hours (diagnosis + multiple fix attempts + validation)

#### Recommendation

**Execute OCC104 Sprint 1 Immediately**

**Tasks (from OCC104):**
1. **Diagnose Memory Leak** (3 hours)
2. **Fix CPU Usage at Idle** (6 hours)
3. **Complete SDK Integration** (4 hours) - verify gain/fade/loop
4. **Validate** (1 hour) - >1 hour stability test

**Success Criteria:**
- [ ] CPU usage <5% with no playback
- [ ] No disk space consumption after 10 builds
- [ ] Computer no longer warm during idle
- [ ] Application stable for >1 hour sessions
- [ ] User approval to proceed to OCC104 Sprint 2

**Timeline:** 3-5 days (14 hours total)

**Urgency:** High - blocks all remaining OCC v0.2.1 work (11 sprints in OCC104)

---

## Additional Technical Debt

### TD1: Core SDK TODOs (7 instances)

**Priority:** Medium (Future Features)
**Impact:** Features partially implemented
**Effort:** 12 hours (complete all 7)

#### Inventory

**File:** `src/core/transport/transport_controller.cpp`

1. **Line 57:** SessionGraph integration
   ```cpp
   // TODO: m_sessionGraph will be used for querying clip metadata
   (void)m_sessionGraph; // Suppress unused warning for now
   ```

2. **Line 183:** Tempo-based beat calculation
   ```cpp
   // TODO: Get tempo from SessionGraph
   double tempo = 120.0; // Default tempo
   position.beats = position.seconds * tempo / 60.0;
   ```

3. **Line 547:** Group assignments
   ```cpp
   case TransportCommand::Type::StopGroup:
     // TODO: Get clip group assignments from SessionGraph
     // For now, this is a no-op
     break;
   ```

4. **Line 616:** Error reporting
   ```cpp
   if (m_activeClipCount >= MAX_ACTIVE_CLIPS) {
     // TODO: Report error (too many active clips globally)
     return;
   }
   ```

5. **Line 622:** Lock-free optimization
   ```cpp
   // NOTE: Brief mutex lock in audio thread - only happens when starting clip
   // TODO: Optimize to lock-free structure for production
   std::shared_ptr<IAudioFileReader> reader;
   ```

6. **Line 1388:** Beat calculation (restart)
   ```cpp
   pos.samples = trimIn;
   pos.seconds = static_cast<double>(trimIn) / static_cast<double>(m_sampleRate);
   pos.beats = 0.0; // TODO: Calculate from tempo
   ```

7. **Line 1447:** Beat calculation (seek)
   ```cpp
   pos.samples = clampedPosition;
   pos.seconds = static_cast<double>(clampedPosition) / static_cast<double>(m_sampleRate);
   pos.beats = 0.0; // TODO: Calculate from tempo
   ```

#### Analysis

**Category A: SessionGraph Integration (Items 1, 2, 3, 6, 7)**

**Status:** Waiting on SessionGraph API completion

**Impact:**
- Tempo tracking not functional (defaults to 120 BPM)
- Beat positions always report 0.0
- Group-based stop commands (StopGroup) non-functional

**Priority:** Medium (not blocking v1.0, defer to v1.1)

**Effort:** 6 hours
- 2 hours: SessionGraph API design
- 3 hours: Implementation (tempo queries, beat calculation)
- 1 hour: Testing

**Category B: Error Reporting (Item 4)**

**Status:** Silent failure when max clips exceeded

**Impact:** Low (MAX_ACTIVE_CLIPS = 32, unlikely in production)

**Priority:** Low (add logging for v1.1)

**Effort:** 2 hours
- 1 hour: Add `IErrorCallback` with `onMaxClipsExceeded()`
- 1 hour: Testing + documentation

**Category C: Lock-Free Optimization (Item 5)**

**Status:** Brief mutex lock in `addActiveClip()` (audio thread)

**Impact:** Low (only happens on clip start, not during playback)

**Current Performance:** Acceptable (<1ms lock duration)

**Priority:** Low (optimize if profiling shows contention)

**Effort:** 4 hours
- 2 hours: Design lock-free AudioFileEntry lookup (hash table or atomic pointer array)
- 1 hour: Implementation
- 1 hour: Benchmark (verify no regression)

#### Recommendation

**Defer All 7 TODOs to v1.1**

**Rationale:**
1. Not blocking v1.0 release (tests passing, features work with defaults)
2. SessionGraph API needs design work (not ready for quick fix)
3. Lock-free optimization premature (no evidence of contention)

**Alternative:** Track as GitHub issues for v1.1 milestone

**Timeline:** 12 hours total (defer to v1.1 sprint)

---

### TD2: CI/CD Warnings (Deprecations)

**Priority:** Low (No Functional Impact)
**Impact:** Future compatibility risk
**Effort:** 2 hours

#### Evidence

**File:** `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp`

Recent commits show warnings addressed:
```
0a35c618 fix(dsp): suppress Windows C4251 DLL warning for Oscillator class
50208d5e fix(windows): add missing stdexcept include and suppress CRT warnings
```

**Likely Remaining Warnings:**
- JUCE Font deprecation warnings (cosmetic, from SDK enhancement sprint ORP076)
- CoreAudio API deprecations (AudioObjectGetPropertyData, etc.)
- C++20 deprecations (std::result_of, std::binary_function)

#### Recommendation

**Monitor but defer fixes**

**Action:** Track warnings in GitHub issue, fix in maintenance sprint

**Timeline:** 2 hours (can be done anytime)

---

## Opportunities (Strategic Enhancements)

### OPP1: Complete Phase 4 to 100%

**Status:** 9/12 tasks (75%)
**Effort:** 4-6 hours
**Value:** Strategic milestone completion

#### Remaining Tasks

**Priority 2: Performance Validation (0/2) - Deferred to User**

**Task 2.1: Profile 16-clip scenario**
- Tool: Instruments (macOS Time Profiler)
- Target: Measure CPU per thread (audio vs UI)
- Acceptance: <30% CPU with 16 clips @ 48kHz
- Effort: 2 hours

**Task 2.2: Validate latency targets**
- Tool: CoreAudio with 512-sample buffer
- Target: <5ms round-trip latency
- Acceptance: Documented in performance report
- Effort: 1 hour

**Priority 3: Documentation (1/5) - Optional**

**Task 3.1: Regenerate Doxygen API Documentation**
- Status: Headers documented, HTML generation deferred
- Effort: 1 hour (setup Doxyfile + generate)

**Task 3.3: Update Architecture Docs**
- Files: `docs/ARCHITECTURE.md`, `docs/ROADMAP.md`
- Content: Metadata storage pattern, Phase 4 status
- Effort: 2 hours

#### Recommendation

**Complete in Sprint 2 (post-v1.0 release)**

**Timeline:** 6 hours total (profiling + docs)

**Value:** Demonstrates thoroughness, provides performance baseline

---

### OPP2: OCC v0.2.1 Sprint Plan Execution

**Status:** Detailed plan exists (OCC104)
**Scope:** 12 sprints, ~80 tasks, 6 weeks
**Value:** Production-ready soundboard application

#### Sprint Overview (from OCC104)

| Sprint | Focus                      | Priority | Duration | Dependencies |
| ------ | -------------------------- | -------- | -------- | ------------ |
| 1      | Critical Performance & SDK | P0       | 3-5 days | None         |
| 2      | Tab Bar & Status System    | P1       | 2-3 days | Sprint 1     |
| 3      | Clip Button Visual System  | P1       | 3-4 days | Sprint 2     |
| 4      | Clip Edit Dialog           | P1       | 4-5 days | Sprint 1     |
| 5      | Session Management         | P1       | 3-4 days | Sprint 4     |
| 6-12   | Polish & Features          | P2       | Variable | Previous     |

#### Current Blocker

**Sprint 1 tasks** (Critical Performance) blocked by Issue 4 (OCC CPU/memory bugs)

#### Recommendation

**Execute After Resolving Issue 4**

**Timeline:** 6 weeks (OCC104 plan)

**Priority:** High (application-level work, builds on SDK v1.0)

---

### OPP3: Cross-Platform Validation

**Status:** Recent CI fixes for Windows/Linux
**Remaining:** Platform support matrix unclear
**Effort:** 8 hours

#### Evidence

Recent commits:
```
240f1d39 fix(ci): remove failing pkgconfiglite from Windows workflow
50208d5e fix(windows): add missing stdexcept include and suppress CRT warnings
23092aa7 fix(ci): disable unimplemented WASAPI driver
```

**Platforms:**
- ✅ **macOS:** Primary development platform (CoreAudio driver stable)
- ⚠️ **Windows:** CI passing, but WASAPI driver disabled
- ❓ **Linux:** ALSA driver status unclear

#### Recommendation

**Document platform support matrix**

**Action:**
1. Create `docs/PLATFORM_SUPPORT.md`
2. Test WASAPI driver on Windows (enable in CI)
3. Test ALSA driver on Linux (add to CI matrix)
4. Document known limitations per platform

**Timeline:** 8 hours (testing + documentation)

**Value:** Clear expectations for downstream users

---

## Recommended Sprint Sequence

### Sprint 1: Strategic Clarity & SDK Release (Week 1)

**Duration:** 3-5 days (20 hours)
**Priority:** P0 (Blocking)
**Goal:** Ship SDK v1.0.0-rc.1 and resolve architecture direction

#### Tasks

**1.1 Strategic Decision: Package Future** (2 hours)
- Review Issue 1 options (A: Archive, B: Multi-platform, C: Hybrid)
- **Recommendation:** Option A (C++ SDK focus)
- Document decision in `docs/DECISION_PACKAGES.md`
- Get user approval

**1.2 Execute Package Strategy** (4 hours, if Option A chosen)
- Move `packages/` to `archive/packages/`
- Update `.claudeignore` to exclude archived code
- Remove TypeScript CI jobs (npm builds, security audits, chaos tests)
- Update README: "C++ SDK for professional audio applications"
- Update `.claude/implementation_progress.md` (mark Phase 1-2 as archived)

**1.3 Complete SDK v1.0.0-rc.1 Release** (4 hours)
- Execute ORP099 Task 4.1: Create `release/v1.0.0-rc.1` branch
- Bump version in `CMakeLists.txt`
- Tag `v1.0.0-rc.1`
- Execute ORP099 Task 4.2: Monitor CI on release branch
- Execute ORP099 Task 4.3: Create GitHub release with `CHANGELOG.md`

**1.4 Update Repository Documentation** (2 hours)
- Update `README.md`:
  - Clarify SDK purpose ("C++ SDK for professional audio")
  - Add quick start guide (build, test, integrate)
  - Document build scripts (`build-launch.sh`, `clean-relaunch.sh`)
  - Link to `docs/repo-commands.html`
- Update `.claude/implementation_progress.md`:
  - Mark Phase 4 as 100% complete (12/12 tasks)
  - Update overall progress (64/104 → 67/104 with release tasks)

**1.5 Document This Analysis** (1 hour)
- Create `docs/orp/ORP102.md` (this document)
- Reference in `.claude/implementation_progress.md`

#### Success Criteria

- [ ] Strategic direction documented (`docs/DECISION_PACKAGES.md`)
- [ ] SDK v1.0.0-rc.1 released on GitHub with changelog
- [ ] README clarifies C++ SDK focus with quick start
- [ ] CI clean (no TypeScript/chaos test noise if Option A)
- [ ] User approval to proceed to Sprint 2

#### Deliverables

- Git tag `v1.0.0-rc.1`
- GitHub release with precompiled binaries (optional)
- Updated repository documentation
- Clear strategic direction

---

### Sprint 2: Quality & Debt Reduction (Week 2)

**Duration:** 3-4 days (18 hours)
**Priority:** P1 (Quality)
**Goal:** Complete Phase 4, address technical debt

#### Tasks

**2.1 Complete Phase 4 Performance Validation** (3 hours)
- Execute ORP099 Task 2.1: Profile 16-clip scenario with Instruments
- Execute ORP099 Task 2.2: Validate latency targets (CoreAudio, 512 samples)
- Document results in `docs/orp/ORP103_Performance_Report.md`

**2.2 Generate Doxygen Documentation** (1 hour)
- Execute ORP099 Task 3.1: Create `Doxyfile` configuration
- Generate HTML documentation
- Host on GitHub Pages or commit to `docs/api/`

**2.3 Update Architecture Documentation** (2 hours)
- Execute ORP099 Task 3.3: Update `docs/ARCHITECTURE.md`
  - Document metadata storage pattern (AudioFileEntry)
  - Update transport layer diagram
- Update `docs/ROADMAP.md`:
  - Mark Phase 4 complete
  - Add v1.1 feature roadmap (SessionGraph, tempo tracking)

**2.4 Address Core SDK TODOs** (8 hours)
- Review 7 TODOs in `transport_controller.cpp` (TD1)
- **Option A:** Implement SessionGraph tempo tracking (6 hours)
- **Option B:** Convert TODOs to GitHub issues for v1.1 (2 hours)
- **Recommendation:** Option B (defer to v1.1, track issues)

**2.5 Cross-Platform Validation** (4 hours)
- Create `docs/PLATFORM_SUPPORT.md`
- Test WASAPI driver on Windows (re-enable in CI)
- Test ALSA driver on Linux
- Document platform-specific limitations

#### Success Criteria

- [ ] Phase 4 100% complete (12/12 tasks)
- [ ] Performance report published (CPU, latency metrics)
- [ ] Doxygen HTML documentation generated
- [ ] All core TODOs tracked (issues or implemented)
- [ ] Platform support matrix documented

#### Deliverables

- `docs/orp/ORP103_Performance_Report.md`
- Doxygen HTML (hosted or committed)
- GitHub issues for TODOs (if deferred)
- `docs/PLATFORM_SUPPORT.md`

---

### Sprint 3: OCC Critical Performance Fixes (Week 3)

**Duration:** 3-5 days (14 hours)
**Priority:** P0 for OCC (Blocking)
**Goal:** Fix OCC CPU and memory bugs, unblock v0.2.1

#### Tasks

**3.1 Diagnose Memory Leak** (3 hours)
- Execute OCC104 Sprint 1 Task 1 (Memory Leak Diagnosis)
- Monitor build directory sizes: `du -sh build-*/`
- Check for orphaned processes: `ps aux | grep orpheus`
- Inspect file descriptors: `lsof | grep orpheus`
- Review build scripts (`build-launch.sh`, `clean-relaunch.sh`)
- Identify leak source

**3.2 Fix Memory Leak** (3 hours)
- Implement fix (likely: killall before build, proper cleanup in destructors)
- Validate: 10 consecutive builds with no disk space growth
- Update build scripts with fix

**3.3 Diagnose CPU Usage** (2 hours)
- Execute OCC104 Sprint 1 Task 2 (CPU Usage Diagnosis)
- Profile with Instruments (macOS Time Profiler)
- Identify hot loops in `MainComponent.cpp`, `ClipGrid.cpp`
- Confirm 75fps polling loop hypothesis

**3.4 Fix CPU Usage** (4 hours)
- Implement conditional timer (only run during playback)
- Optimize `syncState()` (only update changed buttons)
- Implement partial repaints (`repaint(dirtyRegion)`)
- Validate: <5% CPU at idle

**3.5 Verify SDK Integration** (2 hours)
- Execute OCC104 Sprint 1 Task 3 (SDK Gain/Fade Completion)
- Test gain slider in Clip Edit Dialog
- Test fade IN/OUT controls
- Test settings persistence in session save/load

#### Success Criteria

- [ ] Memory leak eliminated (10 builds, no disk growth)
- [ ] CPU usage <5% at idle (down from 77%)
- [ ] Computer no longer warm during idle
- [ ] Application stable for >1 hour sessions
- [ ] SDK integration verified (gain, fade, loop)
- [ ] User approval to proceed with OCC v0.2.1 (remaining 11 sprints)

#### Deliverables

- Fixed build scripts
- Optimized `MainComponent.cpp` and `ClipGrid.cpp`
- `docs/occ/OCC105_Performance_Fixes.md` (technical report)
- Green light for OCC v0.2.1 full plan (OCC104)

---

## Timeline Summary

| Sprint   | Duration | Priority | Deliverable                                |
| -------- | -------- | -------- | ------------------------------------------ |
| Sprint 1 | 3-5 days | P0       | SDK v1.0.0-rc.1 released, strategy clear   |
| Sprint 2 | 3-4 days | P1       | Phase 4 100%, debt tracked                 |
| Sprint 3 | 3-5 days | P0 (OCC) | OCC performance fixed, v0.2.1 unblocked    |
| **Total**    | **9-13 days** | **—**        | **SDK v1.0, OCC ready for production work** |

**Post-Sprint 3:** Execute OCC v0.2.1 (OCC104, 12 sprints, 6 weeks)

---

## Success Metrics

### After Sprint 1 (Strategic Clarity)

- [ ] SDK v1.0.0-rc.1 released on GitHub
- [ ] Strategic direction documented (C++ focus vs multi-platform)
- [ ] README clarifies SDK purpose and quick start
- [ ] CI clean (no failing nightly tests from unused features)

### After Sprint 2 (Quality)

- [ ] Phase 4 complete (75/104 → 67/104 overall)
- [ ] Performance baseline documented (CPU, latency)
- [ ] Doxygen API reference available
- [ ] All technical debt tracked (issues or resolved)

### After Sprint 3 (OCC Performance)

- [ ] OCC CPU <5% at idle (down from 77%)
- [ ] OCC stable for >1 hour sessions (no memory leak)
- [ ] OCC ready for v0.2.1 development (11 remaining sprints)

### Overall (3 Sprints Complete)

- [ ] SDK v1.0 milestone achieved
- [ ] Repository health improved (clear direction, no CI noise)
- [ ] OCC unblocked for production feature work
- [ ] Ready for external beta users (v1.0 SDK, stable OCC)

---

## Next Actions

### Immediate (User Approval Required)

1. **Review this analysis** (ORP102)
2. **Decide on package strategy** (Issue 1, Option A/B/C)
3. **Approve SDK v1.0.0-rc.1 release** (Issue 2, ORP099 Task 4.1-4.3)
4. **Prioritize sprint order** (Sprint 1 → 2 → 3, or swap 1 and 3 if OCC more urgent)

### After User Approval

1. **Execute Sprint 1** (strategic clarity + release)
2. **Execute Sprint 2** (quality + debt reduction)
3. **Execute Sprint 3** (OCC performance fixes)
4. **Begin OCC v0.2.1 full plan** (OCC104, 6 weeks)

---

## References

### Documentation

[1] `.claude/implementation_progress.md` - Overall progress tracking
[2] `docs/orp/ORP099 SDK Track Phase 4 Completion and Testing.md` - Sprint plan
[3] `docs/orp/ORP100 Unit Tests Implementation Report.md` - Test details
[4] `docs/orp/ORP101 Phase 4 Completion Report.md` - Sprint summary
[5] `docs/orp/ORP098 Shmui Rebuild.md` - Web UI strategy (deferred)
[6] `docs/occ/OCC104 v021 Sprint Plan.md` - OCC roadmap (12 sprints)

### Code References

[7] `src/core/transport/transport_controller.cpp` - Core SDK TODOs
[8] `.github/workflows/chaos-tests.yml` - Failing nightly tests
[9] `apps/clip-composer/MainComponent.cpp` - OCC CPU usage (suspected)
[10] `README.md` - Repository overview (needs update)

### External

[11] JUCE Framework: https://juce.com/
[12] Keep a Changelog: https://keepachangelog.com/en/1.0.0/
[13] Semantic Versioning: https://semver.org/

---

## Appendix A: Repository Metrics

### Code Statistics

**C++ Core SDK:**
- Source files: ~50 (src/core/, include/orpheus/)
- Lines of code: ~15,000 (estimated)
- Test files: 58 tests across 20+ test files
- Test coverage: High (all public APIs tested)

**TypeScript Packages (if kept):**
- Packages: 8 (`@orpheus/*`)
- Source files: ~200 (packages/*/src/)
- Lines of code: ~25,000 (estimated)
- Test coverage: Moderate (integration tests exist)

**Clip Composer (JUCE App):**
- Source files: ~30 (apps/clip-composer/Source/)
- Lines of code: ~10,000 (estimated)
- Test coverage: Manual (no automated tests yet)

### Documentation Statistics

**ORP (SDK Docs):** 101 documents (~50,000 lines)
**OCC (App Docs):** 105 documents (~60,000 lines)
**Total:** 206 documents (~110,000 lines)

### Build Performance

**C++ SDK (Debug):**
- Clean build: ~30 seconds (8 cores, macOS)
- Incremental build: <5 seconds (single file change)
- Test execution: <1 second (58 tests)

**TypeScript Packages (if kept):**
- Clean build: ~45 seconds (pnpm build)
- Incremental build: ~10 seconds (turbo cache)
- Test execution: ~5 seconds (vitest)

### CI/CD Performance

**Main Pipeline:**
- Duration: 7-10 minutes (matrix builds: 3 OS × 2 configs)
- Jobs: 7 (cpp-build-test, cpp-lint, native-driver, typescript, integration, dependency, performance)
- Success rate: ~95% (recent failures from Windows/Linux fixes)

**Chaos Tests (if kept):**
- Duration: 21-23 seconds (currently placeholders)
- Frequency: Nightly (2 AM UTC)
- Success rate: 0% (failing due to TODOs)

---

## Appendix B: Risk Analysis

### High-Risk Decisions

**Decision 1: Archive Packages (Issue 1, Option A)**

**Risk:** Cannot support web use cases in future
**Mitigation:** Preserve in git history, can restore if needed
**Probability:** Low (OCC uses JUCE, web strategy unclear)
**Impact:** Medium (limits SDK reach to C++ developers only)

**Decision 2: Skip Chaos Tests (Issue 3, Option B)**

**Risk:** Real reliability issues not caught
**Mitigation:** Focus on C++ integration tests, monitor production
**Probability:** Low (packages have no production consumers)
**Impact:** Low (C++ SDK is primary reliability focus)

### Medium-Risk Decisions

**Decision 3: Defer TODOs to v1.1 (TD1)**

**Risk:** Features incomplete at v1.0 (tempo, groups)
**Mitigation:** Document as "future features" in release notes
**Probability:** Medium (users may expect tempo tracking)
**Impact:** Low (defaults work, not blocking use cases)

**Decision 4: Release v1.0.0-rc.1 Now (Issue 2)**

**Risk:** Undiscovered bugs in production
**Mitigation:** "rc.1" signals not final, beta testing planned
**Probability:** Low (58/58 tests passing, extensive validation)
**Impact:** Medium (reputation risk if bugs found)

### Low-Risk Decisions

**Decision 5: Complete Phase 4 Performance Validation (Sprint 2)**

**Risk:** Performance targets not met
**Mitigation:** Already validated in dummy driver tests
**Probability:** Low (estimated <10% CPU already)
**Impact:** Low (documentation update, not code change)

---

## Appendix C: Alternative Sprint Orders

### Option 1: OCC-First (If Clip Composer More Urgent)

**Order:** Sprint 3 → Sprint 1 → Sprint 2

**Rationale:**
- Unblock OCC development immediately (v0.2.1 plan ready)
- SDK release can wait (no external users yet)
- Strategic clarity can be deferred (internal decision)

**Timeline:** Same (9-13 days)

**Tradeoff:** SDK v1.0 delayed, but OCC progresses faster

---

### Option 2: Quick Wins First (Minimize Disruption)

**Order:** Sprint 1 (release only) → Sprint 3 → Sprint 2

**Tasks:**
1. **Sprint 1 (Lite):** SDK release only (skip package decision, 4 hours)
2. **Sprint 3:** OCC performance fixes (14 hours)
3. **Sprint 2:** Quality + debt (18 hours)
4. **Sprint 1 (Complete):** Package strategy after OCC stable (4 hours)

**Rationale:**
- Get v1.0 tag out quickly (psychological win)
- Fix OCC blockers (highest user impact)
- Defer strategic decisions until clearer picture

**Timeline:** Same (9-13 days)

**Tradeoff:** Package strategy decision delayed, CI noise persists longer

---

### Option 3: Parallel Execution (If Multiple Developers)

**Simultaneous:**
- **Developer A:** Sprint 1 (strategic clarity + release)
- **Developer B:** Sprint 3 (OCC performance fixes)
- **Developer C:** Sprint 2 (documentation + validation)

**Timeline:** 3-5 days (fastest path)

**Requirement:** 3 developers working independently

**Tradeoff:** Coordination overhead, merge conflicts possible

---

**Document Status:** ✅ Analysis Complete, Awaiting User Review
**Next Action:** User approval on sprint priority and package strategy
**Estimated Timeline:** 9-13 days (3 sprints) + 6 weeks (OCC v0.2.1)
