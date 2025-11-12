# OCC112 Sprint Roadmap - Gap Closure and Forward Progress

**Planning Date:** November 12, 2025
**Based On:** OCC111 Gap Audit Report (47 gaps identified)
**Target Versions:** v0.2.3 (gap closure), v0.3.0 (SDK integration), v1.0 (production)
**Planning Horizon:** 16 weeks (4 months)
**Status:** 📋 Comprehensive Sprint Plan

---

## Executive Summary

This roadmap provides concrete sprint plans to close the **47 gaps** identified in OCC111 and advance toward v1.0 production readiness.

### Sprint Architecture

**Total Sprints:** 9
- **Series A (Gap Closure):** 5 sprints, 20-31 person-days
- **Series B (Forward Progress):** 4 sprints, 33-45 person-days
- **Total Effort:** 53-76 person-days (~11-15 weeks for 1 developer)

### Sprint Timeline (1 Developer)

| Sprint | Focus                     | Duration | Cumulative | Milestone     |
| ------ | ------------------------- | -------- | ---------- | ------------- |
| A1     | Quick Wins & Verification | 2-3 days | Week 1     | -             |
| A2     | OCC110 SDK Integration    | 1-2 days | Week 1     | -             |
| A3     | Performance & QA          | 3-5 days | Week 2     | v0.2.3 release |
| A4     | Automated Testing         | 5-8 days | Week 3-4   | -             |
| A5     | UI/UX Polish              | 5-8 days | Week 5-6   | v0.2.4 release |
| B1     | SDK Integration (v0.3.0)  | 10-12 days | Week 8-9   | -             |
| B2     | 4 Clip Groups & Routing   | 5-8 days | Week 10-11 | v0.3.0 release |
| B3     | Production Hardening      | 10-15 days | Week 13-15 | v1.0-rc.1     |
| B4     | Advanced Features         | 8-10 days | Week 16-17 | v1.0 release  |

### Version Milestone Plan

- **v0.2.3** (Week 2): All P0 gaps closed, 100% QA pass rate
- **v0.2.4** (Week 6): P1 gaps closed, automated testing foundation
- **v0.3.0** (Week 11): SDK integration complete, 4 Clip Groups functional
- **v1.0** (Week 17): Production-ready, all features complete

---

## Gap-Closing Sprints (Series A)

### Sprint A1: Quick Wins & Critical Verification

**Objective:** Resolve 4 P0 blockers with minimal effort, establish confidence in codebase state

**Duration:** 2-3 person-days
**Priority:** CRITICAL - Must complete before other work
**Dependencies:** None
**Target Version:** v0.2.3

#### Deliverables

1. **Multi-Tab Playback Verification Report**
   - Re-run OCC103 multi-tab test (Test ID: 270-297)
   - Verify clips on Tabs 2-8 play audio
   - Document pass/fail for each tab (1-8)
   - **If PASS:** Update OCC105 with new test results
   - **If FAIL:** Verify AudioEngine.h has `MAX_CLIPS = 384`, debug routing

2. **CPU Usage Verification Report**
   - Measure CPU idle with Activity Monitor (expect <10%)
   - Measure CPU with 16 clips playing (expect ~35%)
   - Measure CPU with 32 clips playing (expect ~50%)
   - Document results in OCC105v2 test log

3. **MAX_CLIPS Code Inspection**
   - Read AudioEngine.h line ~25
   - Verify `static constexpr int MAX_CLIPS = 384;` exists
   - If not, implement OCC106:56-81 fix (10 minutes)

4. **Memory Test with 384 Clips**
   - Load 384 clips across all 8 tabs
   - Monitor memory usage (expect <200MB)
   - Run for 5 minutes, check for leaks
   - Document in OCC105v2

5. **Version Number Audit & Update**
   - Search all OCC docs for version mismatches
   - Update stale v0.2.0 refs to v0.2.2
   - Update roadmap refs to current milestones

#### Acceptance Criteria

- [ ] Multi-tab test completed, results documented (pass or fail)
- [ ] CPU usage measured and documented (expect <10% idle)
- [ ] MAX_CLIPS verified to be 384 (or fixed if wrong)
- [ ] Memory usage tested at 384 clips (<200MB)
- [ ] All version numbers consistent across docs

#### Success Metrics

- **Multi-Tab Status:** ✅ VERIFIED or ❌ BUG FILED
- **CPU Idle:** <10% (vs. claimed <10% in OCC108)
- **Memory:** <200MB at 384 clips
- **Documentation:** 0 version mismatches

#### Testing Checklist

- [ ] Load 8 clips (one per tab), play each, verify audio
- [ ] Load 384 clips (all tabs full), play multiple, verify audio
- [ ] Measure CPU idle (no clips loaded, app running)
- [ ] Measure CPU with 16 simultaneous clips playing
- [ ] Measure CPU with 32 simultaneous clips playing
- [ ] Monitor memory during 5-minute session (Instruments Leaks tool)
- [ ] Verify no memory leaks (memory stays flat)

#### Risk Assessment

**Technical Risks:**
- **Risk:** Multi-tab test may fail, revealing critical bug
  - **Mitigation:** If fail, escalate to Sprint A3 (bug fix sprint)
- **Risk:** Memory may exceed 200MB at 384 clips
  - **Mitigation:** Acceptable if <500MB, defer optimization to v0.3.0

**Success Probability:** 90% (most items are verification, not implementation)

---

### Sprint A2: OCC110 SDK Integration

**Objective:** Implement OCC110 integration guide - fix `isClipPlaying()` stub to enable graceful timer management

**Duration:** 1-2 person-days
**Priority:** CRITICAL - P0 blocker
**Dependencies:** Sprint A1 complete (baseline verification)
**Target Version:** v0.2.3
**References:** OCC110:37-53, OCC111:EC-1

#### Deliverables

1. **AudioEngine.h Modifications**
   - Add `getClipHandleForButton()` helper method
   - Add `m_buttonToHandle` map (button index → ClipHandle)
   - Follow OCC110:106-125 spec

2. **AudioEngine.cpp Modifications**
   - Implement `getClipHandleForButton()` (OCC110:117-125)
   - Replace `isClipPlaying()` stub (OCC110:130-155)
   - Update `loadClip()` to populate `m_buttonToHandle` (OCC110:160-184)

3. **PreviewPlayer.cpp Modifications**
   - Fix `timerCallback()` to use `isClipPlaying()` (OCC110:209-232)
   - Add graceful timer stop when clip finishes
   - Update final playhead position display

4. **ClipEditDialog.cpp Integration**
   - Verify integration per OCC110:243-274 example
   - Test loop mode with timer management
   - Test non-looped clips (timer should stop at OUT point)

5. **Integration Test Suite**
   - Test non-looped clip: timer stops at OUT point
   - Test looped clip: timer runs indefinitely
   - Test manual stop: timer stops after fade-out
   - Test rapid start/stop cycles

#### Acceptance Criteria

- [ ] `isClipPlaying()` queries SDK `getClipState()` (no more stub)
- [ ] PreviewPlayer timer stops gracefully when clip finishes
- [ ] Non-looped clips show correct final playhead position (OUT point)
- [ ] Looped clips keep timer running (no premature stop)
- [ ] Manual stop triggers timer stop after fade-out completes
- [ ] CPU savings: Timer idle when no clips playing (~2% savings per OCC110:562)

#### Implementation Steps

**Step 1: Add Button-to-Handle Mapping (30 minutes)**

```cpp
// AudioEngine.h (private section)
private:
  orpheus::ClipHandle getClipHandleForButton(int buttonIndex) const;
  std::unordered_map<int, orpheus::ClipHandle> m_buttonToHandle;
```

```cpp
// AudioEngine.cpp
orpheus::ClipHandle AudioEngine::getClipHandleForButton(int buttonIndex) const {
  auto it = m_buttonToHandle.find(buttonIndex);
  return (it != m_buttonToHandle.end()) ? it->second : 0;
}
```

**Step 2: Replace isClipPlaying() Stub (15 minutes)**

```cpp
// AudioEngine.cpp:239-242 (BEFORE)
bool AudioEngine::isClipPlaying(int buttonIndex) const {
  // TODO (Week 5-6): Query m_transportController->getClipState(handle)
  return false;
}

// AudioEngine.cpp:239-250 (AFTER - per OCC110:137-155)
bool AudioEngine::isClipPlaying(int buttonIndex) const {
  if (!m_transportController)
    return false;

  orpheus::ClipHandle handle = getClipHandleForButton(buttonIndex);
  if (handle == 0)
    return false;

  auto state = m_transportController->getClipState(handle);
  return (state == orpheus::PlaybackState::Playing ||
          state == orpheus::PlaybackState::Stopping);
}
```

**Step 3: Update loadClip() to Track Mapping (10 minutes)**

```cpp
// AudioEngine.cpp (in loadClip method)
bool AudioEngine::loadClip(const juce::String& filePath, int buttonIndex) {
  // ... existing registration logic ...

  // Track button-to-handle mapping (NEW)
  m_buttonToHandle[buttonIndex] = handle;

  return true;
}
```

**Step 4: Fix PreviewPlayer Timer (30 minutes)**

```cpp
// PreviewPlayer.cpp:209-220 (per OCC110:209-232)
void PreviewPlayer::timerCallback() {
  // Query AudioEngine for real playback state
  if (!m_audioEngine->isClipPlaying(m_cueButtonHandle)) {
    // Clip has stopped - gracefully stop timer
    stopTimer();

    // Update UI to show final position
    if (m_positionCallback) {
      m_positionCallback(m_lastKnownPosition); // Show OUT point
    }

    return;
  }

  // Clip is still playing - update position
  int64_t position = m_audioEngine->getClipPosition(m_cueButtonHandle);
  if (m_positionCallback) {
    m_positionCallback(position);
  }

  m_lastKnownPosition = position; // Cache for final update
}
```

**Step 5: Testing (1-2 hours)**

Run all tests from OCC110:527-550:
- [ ] Non-looped clip stops at OUT, timer stops, playhead shows OUT
- [ ] Looped clip runs indefinitely, timer never stops
- [ ] Manual stop (STOP button) stops timer after fade-out
- [ ] Keyboard shortcut stop (SPACE) stops timer after fade-out
- [ ] Rapid start/stop cycles don't crash

#### Success Metrics

- **Timer Efficiency:** Timer idle when no clips playing (0% CPU vs. ~2% before)
- **UX Polish:** Playhead shows correct final position (no drift)
- **Code Quality:** 0 TODO comments in AudioEngine.cpp
- **Test Pass Rate:** 100% (5 integration tests)

#### Risk Assessment

**Technical Risks:**
- **Risk:** Handle mapping may be incorrect for Cue Busses (IDs 10001+)
  - **Mitigation:** Test both clip buttons (0-383) and Cue Busses (10001+)
- **Risk:** Timer may stop prematurely if `Stopping` state not checked
  - **Mitigation:** Implementation checks both `Playing` and `Stopping`

**Success Probability:** 95% (straightforward implementation, well-documented)

---

### Sprint A3: Performance Investigation & QA Validation

**Objective:** Investigate performance degradation at 192 clips, complete comprehensive QA testing

**Duration:** 3-5 person-days
**Priority:** CRITICAL - P0 blocker investigation
**Dependencies:** Sprint A1, A2 complete
**Target Version:** v0.2.3
**References:** OCC111:AC-1, OCC111:EC-7

#### Deliverables

1. **Performance Profiling Report**
   - Profile Edit Dialog with 192 clips loaded (4 tabs full)
   - Identify CPU hotspots (waveform rendering, memory allocation)
   - Measure Edit Dialog open latency (current vs. target <100ms)
   - Document findings in OCC113 Performance Investigation Report

2. **Root Cause Analysis**
   - Confirm hypothesis: "Waveform rendering loads entire file into memory"
   - Measure memory allocation during Edit Dialog open (Instruments)
   - Identify fix: SDK Waveform Pre-Processing API (ORP110 Feature 4)
   - Document in OCC113

3. **Short-Term Workaround (Optional)**
   - If critical, implement waveform thumbnail cache
   - Trade memory for speed (pre-render thumbnails on load)
   - Measure improvement (expect 30-50% latency reduction)

4. **Complete OCC103 QA Test Suite**
   - Run all 42 tests from OCC103 specification
   - Document results in OCC105v2 Manual Test Log
   - Achieve 100% pass rate (or document blockers)

5. **QA Test Result Reconciliation**
   - Compare OCC105 (v0.2.0), OCC108 claims (v0.2.1), new results (v0.2.3)
   - Identify any regressions or contradictions
   - Update OCC108 if completion claims were inaccurate

#### Acceptance Criteria

- [ ] Performance profiling complete, hotspots identified
- [ ] Root cause confirmed (waveform loading or other)
- [ ] Workaround implemented (if needed for v0.2.3 release)
- [ ] OCC103 test suite 100% complete (all 42 tests run)
- [ ] OCC105v2 test log created with all results
- [ ] Pass rate ≥95% (max 2 non-critical failures allowed)

#### Implementation Steps

**Step 1: Performance Profiling (4 hours)**

1. Load 192 clips (4 tabs full)
2. Open Edit Dialog 10 times, measure latency each time
3. Profile with Instruments (Time Profiler + Allocations)
4. Identify top 5 CPU hotspots
5. Measure memory allocation per Edit Dialog open

**Step 2: Root Cause Analysis (2 hours)**

1. Review profiling results
2. Confirm waveform rendering hypothesis
3. Estimate SDK Waveform API integration effort
4. Document in OCC113

**Step 3: Workaround Implementation (1 day, optional)**

If latency >500ms and user-facing, implement cache:

```cpp
// WaveformCache.h
class WaveformCache {
public:
  struct Thumbnail {
    std::vector<float> minSamples;
    std::vector<float> maxSamples;
    uint32_t numBins; // e.g., 1000 bins
  };

  Thumbnail getThumbnail(const juce::String& filePath);
private:
  std::unordered_map<juce::String, Thumbnail> m_cache;
};
```

Trade memory (~10KB per thumbnail × 384 clips = ~4MB) for speed.

**Step 4: QA Testing (1-2 days)**

Run all OCC103 tests:
- Core Functionality (8 tests)
- v0.2.0 Fix Verification (6 tests)
- Multi-Tab Isolation (8 tests)
- Regression Testing (12 tests)
- Stability Testing (8 tests)

Document each result (PASS/FAIL/SKIP), provide notes.

#### Success Metrics

- **Profiling Complete:** Top 5 hotspots identified
- **Root Cause Confirmed:** Documented in OCC113
- **QA Pass Rate:** ≥95% (40/42 tests PASS)
- **Edit Dialog Latency:** Measured baseline for v0.3.0 target (<100ms)

#### Risk Assessment

**Technical Risks:**
- **Risk:** Root cause may not be waveform rendering
  - **Mitigation:** Profile comprehensively, check all Edit Dialog operations
- **Risk:** Workaround may not provide enough speedup
  - **Mitigation:** Acceptable - main fix is v0.3.0 SDK integration
- **Risk:** QA tests may reveal new P0 bugs
  - **Mitigation:** Document as blockers, add to Sprint A4

**Success Probability:** 80% (profiling always succeeds, but fix may need to be deferred)

---

### Sprint A4: Automated Testing Foundation

**Objective:** Establish automated regression test suite to prevent future regressions

**Duration:** 5-8 person-days
**Priority:** HIGH - P1 technical debt
**Dependencies:** Sprint A3 complete (QA baseline established)
**Target Version:** v0.2.4
**References:** OCC111:TD-4, OCC099 (testing strategy)

#### Deliverables

1. **GoogleTest Integration**
   - Add GoogleTest to CMakeLists.txt
   - Create `apps/clip-composer/tests/` directory
   - Set up test runner in CI/CD

2. **Core Functionality Tests**
   - AudioEngine initialization/shutdown (5 tests)
   - Clip loading/unloading (8 tests)
   - Clip triggering/stopping (10 tests)
   - Multi-tab isolation (8 tests)
   - Session save/load (5 tests)

3. **Edit Dialog Tests**
   - Trim point validation (6 tests)
   - Edit law enforcement (8 tests)
   - Loop mode (4 tests)
   - Keyboard shortcuts (10 tests)

4. **Performance Regression Tests**
   - CPU usage benchmarks (3 tests)
   - Memory usage benchmarks (3 tests)
   - Latency benchmarks (3 tests)

5. **CI/CD Integration**
   - Add test stage to GitHub Actions workflow
   - Run tests on every PR
   - Block merge if tests fail

#### Acceptance Criteria

- [ ] GoogleTest integrated, compiles without errors
- [ ] ≥50 unit tests implemented
- [ ] All tests pass (100% pass rate)
- [ ] Tests run in CI/CD on every PR
- [ ] Test coverage ≥60% (measured with gcov/lcov)

#### Implementation Steps

**Step 1: GoogleTest Setup (2 hours)**

```cmake
# apps/clip-composer/CMakeLists.txt
find_package(GTest REQUIRED)
enable_testing()
add_subdirectory(tests)
```

```cmake
# apps/clip-composer/tests/CMakeLists.txt
add_executable(clip_composer_tests
  test_audio_engine.cpp
  test_clip_edit_dialog.cpp
  test_clip_grid.cpp
  test_session.cpp
)
target_link_libraries(clip_composer_tests
  clip_composer_lib
  GTest::GTest
  GTest::Main
)
add_test(NAME clip_composer_tests COMMAND clip_composer_tests)
```

**Step 2: AudioEngine Tests (1 day)**

```cpp
// tests/test_audio_engine.cpp
TEST(AudioEngineTest, InitializeAndShutdown) {
  AudioEngine engine;
  EXPECT_TRUE(engine.initialize(48000, 512));
  EXPECT_TRUE(engine.start());
  EXPECT_TRUE(engine.stop());
}

TEST(AudioEngineTest, LoadClipValidFile) {
  AudioEngine engine;
  engine.initialize(48000, 512);
  EXPECT_TRUE(engine.loadClip("/path/to/valid.wav", 0));
}

TEST(AudioEngineTest, LoadClipInvalidFile) {
  AudioEngine engine;
  engine.initialize(48000, 512);
  EXPECT_FALSE(engine.loadClip("/path/to/invalid.wav", 0));
}

TEST(AudioEngineTest, TriggerClipNotLoaded) {
  AudioEngine engine;
  engine.initialize(48000, 512);
  EXPECT_FALSE(engine.triggerClip(0)); // No clip loaded
}

TEST(AudioEngineTest, IsClipPlayingStub) {
  AudioEngine engine;
  engine.initialize(48000, 512);
  EXPECT_FALSE(engine.isClipPlaying(0)); // Should use SDK now, not stub
}
```

**Step 3: Edit Dialog Tests (1-2 days)**

```cpp
// tests/test_clip_edit_dialog.cpp
TEST(ClipEditDialogTest, TrimInMustBeLessThanTrimOut) {
  ClipEditDialog dialog;
  dialog.setTrimIn(8000);
  dialog.setTrimOut(4000); // Invalid: IN > OUT
  EXPECT_LT(dialog.getTrimIn(), dialog.getTrimOut()); // Should auto-correct
}

TEST(ClipEditDialogTest, PlayheadNeverEscapesInOutBounds) {
  ClipEditDialog dialog;
  dialog.setTrimIn(1000);
  dialog.setTrimOut(8000);
  dialog.setPlayheadPosition(9000); // Beyond OUT
  EXPECT_LE(dialog.getPlayheadPosition(), 8000); // Should clamp to OUT
}
```

**Step 4: Performance Tests (1 day)**

```cpp
// tests/test_performance.cpp
TEST(PerformanceTest, CpuIdleLessThan20Percent) {
  AudioEngine engine;
  engine.initialize(48000, 512);
  engine.start();
  std::this_thread::sleep_for(std::chrono::seconds(5));
  float cpu = engine.getCpuUsage();
  EXPECT_LT(cpu, 20.0f) << "CPU idle should be <20%";
}

TEST(PerformanceTest, MemoryUnder200MBWith384Clips) {
  AudioEngine engine;
  engine.initialize(48000, 512);
  for (int i = 0; i < 384; ++i) {
    engine.loadClip("/path/to/test.wav", i);
  }
  size_t memoryMB = getProcessMemoryMB();
  EXPECT_LT(memoryMB, 200) << "Memory should be <200MB with 384 clips";
}
```

**Step 5: CI/CD Integration (2 hours)**

```yaml
# .github/workflows/test.yml
name: Test Clip Composer
on: [push, pull_request]
jobs:
  test:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v3
      - name: Install dependencies
        run: brew install cmake googletest
      - name: Build
        run: |
          cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
          cmake --build build
      - name: Test
        run: ctest --test-dir build --output-on-failure
```

#### Success Metrics

- **Test Count:** ≥50 unit tests
- **Test Pass Rate:** 100%
- **Test Coverage:** ≥60%
- **CI/CD:** Tests run on every PR, block merge on failure

#### Risk Assessment

**Technical Risks:**
- **Risk:** JUCE components hard to unit test (require GUI context)
  - **Mitigation:** Focus on AudioEngine (non-GUI), mock JUCE components for Edit Dialog
- **Risk:** Test coverage may fall short of 60%
  - **Mitigation:** Acceptable - 40-50% coverage still valuable

**Success Probability:** 85% (standard GoogleTest integration, may take longer than estimated)

---

### Sprint A5: UI/UX Polish

**Objective:** Complete UI/UX gaps (arrow keys, time editor counters, 4 Clip Groups verification)

**Duration:** 5-8 person-days
**Priority:** HIGH - P1 UX gaps
**Dependencies:** Sprint A4 complete
**Target Version:** v0.2.4
**References:** OCC111:Gap 10-12

#### Deliverables

1. **Arrow Key Navigation**
   - Implement arrow key navigation in ClipGrid
   - Up/Down: Navigate between rows (buttons 0→48, 48→96, etc.)
   - Left/Right: Navigate between columns (buttons 0→1→2, etc.)
   - Tab: Navigate to next tab
   - Shift+Tab: Navigate to previous tab

2. **Time Editor Counters (Grabbable Units)**
   - Convert time editor text fields to counter UI
   - Click HH/MM/SS/FF to select unit
   - Arrow keys: Adjust selected unit (±1 frame, ±1 second, etc.)
   - Shift+Arrow: Adjust selected unit by 10× (±10 frames, ±10 seconds)
   - Enter: Commit changes, move to next field
   - Escape: Cancel changes

3. **4 Clip Groups UI Verification**
   - Test routing controls in main UI
   - Verify audio output to correct busses (Group A, B, C, D)
   - Document test results in OCC105v2
   - Create user guide for 4 Clip Groups workflow

4. **Keyboard Shortcut Reference Card**
   - Create in-app keyboard shortcut overlay (press `?` to show)
   - Document all shortcuts (clip triggering, edit dialog, transport)
   - Include in Help menu

5. **UX Documentation Update**
   - Update OCC024 User Workflows with new keyboard shortcuts
   - Add screenshots of time editor counters
   - Document 4 Clip Groups routing setup

#### Acceptance Criteria

- [ ] Arrow keys navigate between clips in ClipGrid
- [ ] Time editor supports grabbable unit navigation
- [ ] 4 Clip Groups routing verified and documented
- [ ] Keyboard shortcut overlay implemented (press `?`)
- [ ] User documentation updated

#### Implementation Steps

**Step 1: Arrow Key Navigation (1-2 days)**

```cpp
// ClipGrid.cpp
bool ClipGrid::keyPressed(const juce::KeyPress& key) {
  if (key.isKeyCode(juce::KeyPress::upKey)) {
    navigateVertical(-1); // Move up one row
    return true;
  }
  if (key.isKeyCode(juce::KeyPress::downKey)) {
    navigateVertical(+1); // Move down one row
    return true;
  }
  if (key.isKeyCode(juce::KeyPress::leftKey)) {
    navigateHorizontal(-1); // Move left one column
    return true;
  }
  if (key.isKeyCode(juce::KeyPress::rightKey)) {
    navigateHorizontal(+1); // Move right one column
    return true;
  }
  return Component::keyPressed(key);
}

void ClipGrid::navigateVertical(int direction) {
  int currentButton = m_selectedButtonIndex;
  int newButton = currentButton + (direction * 8); // 8 columns per row
  if (newButton >= 0 && newButton < 48) {
    setSelectedButton(newButton);
  }
}
```

**Step 2: Time Editor Counters (2-3 days)**

```cpp
// TimeEditorComponent.h
class TimeEditorComponent : public juce::Component {
public:
  enum class Unit { Hours, Minutes, Seconds, Frames };

  void setTime(int64_t samples);
  int64_t getTime() const;
  void setSelectedUnit(Unit unit);

private:
  void paint(juce::Graphics& g) override;
  void mouseDown(const juce::MouseEvent& e) override;
  bool keyPressed(const juce::KeyPress& key) override;

  Unit m_selectedUnit = Unit::Seconds;
  int m_hours = 0, m_minutes = 0, m_seconds = 0, m_frames = 0;
};
```

Render each unit as a separate clickable box, highlight selected unit.

**Step 3: 4 Clip Groups Verification (1 day)**

1. Load 4 clips, assign to different groups (A, B, C, D)
2. Verify audio output routing (use Logic Pro or Reaper to monitor busses)
3. Test group muting, soloing, volume controls
4. Document workflow in OCC105v2

**Step 4: Keyboard Shortcut Overlay (1 day)**

```cpp
// KeyboardShortcutOverlay.cpp
class KeyboardShortcutOverlay : public juce::Component {
public:
  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colours::black.withAlpha(0.8f));
    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawMultiLineText(getShortcutText(), 50, 50, getWidth() - 100);
  }

  juce::String getShortcutText() {
    return R"(
    Keyboard Shortcuts:

    Clip Grid:
    - Arrows: Navigate between clips
    - Space: Stop All
    - 1-9, 0: Trigger clip (row 1)
    - Shift+1-9, 0: Trigger clip (row 2)

    Edit Dialog:
    - Space: Play/Stop
    - , / .: Trim IN left/right
    - ; / ': Trim OUT left/right
    - Shift+trim keys: Nudge ±15 frames
    - ?: Toggle loop mode
    - Cmd+W: Close dialog
    )";
  }
};
```

Show overlay when user presses `?` key.

#### Success Metrics

- **Arrow Key Navigation:** 100% functional (4 directions + Tab)
- **Time Editor:** Grabbable units implemented, tested
- **4 Clip Groups:** Routing verified, documented
- **Keyboard Overlay:** Implemented, shows all shortcuts

#### Risk Assessment

**Technical Risks:**
- **Risk:** Time editor counter UI may be complex to implement
  - **Mitigation:** Use existing JUCE Label components, custom rendering
- **Risk:** Arrow key navigation may conflict with existing shortcuts
  - **Mitigation:** Test comprehensively, document any conflicts

**Success Probability:** 90% (standard UI implementation, well-scoped)

---

## Forward-Progress Sprints (Series B)

### Sprint B1: SDK Integration (v0.3.0) - Performance APIs

**Objective:** Integrate SDK IPerformanceMonitor and Waveform Pre-Processing APIs to fix critical performance gaps

**Duration:** 10-12 person-days
**Priority:** HIGH - Required for professional workflows
**Dependencies:** Sprint A5 complete, SDK features available (ORP110)
**Target Version:** v0.3.0
**References:** OCC111:Gap 1, 3; ORP110 Feature 3, 4

#### Deliverables

1. **IPerformanceMonitor Integration**
   - Use SDK's IPerformanceMonitor for real CPU metrics
   - Replace placeholder in TransportControls (OCC109:218)
   - Display per-thread CPU (audio vs. UI thread)
   - Add memory tracking via SDK

2. **Waveform Pre-Processing API Integration**
   - Integrate SDK Waveform Pre-Processing (ORP110 Feature 4)
   - Pre-generate waveform thumbnails on clip load
   - Edit Dialog loads thumbnails (not full audio files)
   - Measure latency improvement (expect 192-clip session: 2s → <500ms)

3. **Performance Monitoring Dashboard**
   - Add advanced performance view (optional overlay)
   - Show per-clip CPU/memory usage
   - Show audio thread vs. UI thread metrics
   - Export performance logs for debugging

4. **Performance Regression Tests**
   - Add automated performance benchmarks to CI/CD
   - CPU usage: idle, 16 clips, 32 clips
   - Memory usage: 0 clips, 192 clips, 384 clips
   - Edit Dialog latency: empty, 192 clips
   - Fail CI if performance degrades >10%

5. **Documentation Updates**
   - Create OCC114 SDK Integration Report (IPerformanceMonitor)
   - Create OCC115 SDK Integration Report (Waveform API)
   - Update OCC100 performance targets with new baselines

#### Acceptance Criteria

- [ ] CPU monitoring shows real values (not 0%)
- [ ] Per-thread CPU metrics displayed (audio vs. UI)
- [ ] Edit Dialog latency at 192 clips: <500ms (vs. "VERY sluggish")
- [ ] Memory usage at 384 clips: <200MB (measured)
- [ ] Performance regression tests integrated in CI/CD

#### Implementation Steps

**Step 1: IPerformanceMonitor Integration (2 days)**

```cpp
// TransportControls.cpp
void TransportControls::updatePerformanceMetrics() {
  if (!m_audioEngine || !m_audioEngine->getPerformanceMonitor())
    return;

  auto metrics = m_audioEngine->getPerformanceMonitor()->getMetrics();

  // Update CPU display (per-thread)
  float audioCpu = metrics.audioThreadCpuPercent;
  float uiCpu = metrics.messageThreadCpuPercent;
  m_cpuLabel->setText(juce::String::formatted("CPU: %.1f%% (audio) / %.1f%% (UI)",
                                               audioCpu, uiCpu));

  // Update memory display
  float memoryMB = metrics.memoryUsageMB;
  m_memoryLabel->setText(juce::String::formatted("MEM: %.0f MB", memoryMB));

  // Color-code based on thresholds
  if (audioCpu > 80.0f) {
    m_cpuLabel->setColour(juce::Label::textColourId, juce::Colours::red);
  } else if (audioCpu > 50.0f) {
    m_cpuLabel->setColour(juce::Label::textColourId, juce::Colours::orange);
  } else {
    m_cpuLabel->setColour(juce::Label::textColourId, juce::Colours::green);
  }
}
```

**Step 2: Waveform Pre-Processing Integration (4-5 days)**

```cpp
// WaveformPreProcessor.h (SDK integration)
class WaveformPreProcessor {
public:
  struct Thumbnail {
    std::vector<float> minSamples;
    std::vector<float> maxSamples;
    uint32_t numBins;
  };

  // Pre-generate thumbnail on clip load (background thread)
  void generateThumbnail(const juce::String& filePath,
                         std::function<void(Thumbnail)> callback);

  // Get cached thumbnail (instant)
  std::optional<Thumbnail> getCachedThumbnail(const juce::String& filePath);

private:
  std::unordered_map<juce::String, Thumbnail> m_cache;
};
```

Integration in AudioEngine:

```cpp
// AudioEngine.cpp
bool AudioEngine::loadClip(const juce::String& filePath, int buttonIndex) {
  // ... existing registration ...

  // Pre-generate waveform thumbnail (async)
  m_waveformPreProcessor->generateThumbnail(filePath, [this, buttonIndex](auto thumbnail) {
    // Thumbnail ready - notify UI
    if (onThumbnailReady) {
      onThumbnailReady(buttonIndex, thumbnail);
    }
  });

  return true;
}
```

Edit Dialog uses cached thumbnails:

```cpp
// ClipEditDialog.cpp
void ClipEditDialog::loadWaveform(const juce::String& filePath) {
  // Try to get cached thumbnail first
  auto thumbnail = m_audioEngine->getCachedThumbnail(filePath);
  if (thumbnail.has_value()) {
    m_waveformDisplay->setThumbnail(thumbnail.value());
    m_waveformDisplay->repaint();
    return; // Instant load!
  }

  // Fallback: Load full audio (slow)
  loadFullAudioFile(filePath);
}
```

**Step 3: Performance Dashboard (2 days, optional)**

Create advanced performance overlay (press Cmd+Shift+P to toggle):
- Per-clip CPU usage (top 10 consumers)
- Audio thread latency graph (real-time)
- Memory allocation graph
- Export logs button

**Step 4: Performance Regression Tests (1-2 days)**

```cpp
// tests/test_performance.cpp
TEST(PerformanceTest, EditDialogLatencyUnder500msAt192Clips) {
  AudioEngine engine;
  engine.initialize(48000, 512);
  for (int i = 0; i < 192; ++i) {
    engine.loadClip("/path/to/test.wav", i);
  }

  auto start = std::chrono::high_resolution_clock::now();
  ClipEditDialog dialog(&engine, 0);
  dialog.loadWaveform("/path/to/test.wav");
  auto end = std::chrono::high_resolution_clock::now();

  auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  EXPECT_LT(latency, 500) << "Edit Dialog latency should be <500ms at 192 clips";
}
```

Add to CI/CD workflow.

#### Success Metrics

- **CPU Monitoring:** Real values displayed (not 0%)
- **Edit Dialog Latency:** <500ms at 192 clips (vs. "VERY sluggish" before)
- **Memory Usage:** <200MB at 384 clips (measured and verified)
- **Performance Regression:** CI/CD fails if performance degrades >10%

#### Risk Assessment

**Technical Risks:**
- **Risk:** SDK Waveform API may not be ready in time
  - **Mitigation:** Verify ORP110 Feature 4 completion status before sprint start
- **Risk:** Waveform pre-processing may not improve latency enough
  - **Mitigation:** Fallback to full audio load if thumbnail quality insufficient
- **Risk:** IPerformanceMonitor may not provide per-thread metrics
  - **Mitigation:** Use SDK metrics if available, fallback to system APIs

**Success Probability:** 80% (depends on SDK API readiness)

---

### Sprint B2: 4 Clip Groups & Advanced Routing

**Objective:** Complete 4 Clip Groups UI, add advanced routing features (cue busses, effects sends)

**Duration:** 5-8 person-days
**Priority:** MEDIUM - Required for v0.3.0 MVP
**Dependencies:** Sprint B1 complete
**Target Version:** v0.3.0
**References:** OCC021 (product vision), OCC026 (MVP plan)

#### Deliverables

1. **4 Clip Groups UI**
   - Group selector for each clip (dropdown: None, A, B, C, D)
   - Group volume faders in transport controls
   - Group mute/solo buttons
   - Visual indicator on clip buttons (color-coded by group)

2. **Cue Buss Monitoring**
   - Add "Monitor Cue" button in Edit Dialog
   - Route Cue Buss to separate output (headphone monitoring)
   - Level meter for Cue Buss output

3. **Effects Send Busses (Future-Proofing)**
   - Design routing architecture for effects sends
   - Add 4 effects send busses (FX A, B, C, D)
   - Per-clip send level controls (deferred to v1.0 UI)

4. **Routing Matrix Visualization**
   - Add routing matrix view (optional advanced UI)
   - Visual representation of clip → group → output routing
   - Drag-and-drop routing (future enhancement)

5. **Integration Testing**
   - Test 4-clip scenario: one clip per group
   - Verify independent group volume controls
   - Verify mute/solo functionality
   - Test cue buss monitoring with Edit Dialog

#### Acceptance Criteria

- [ ] Each clip can be assigned to group A, B, C, D, or None
- [ ] Group volume faders functional (0-100%, -∞ to +6dB)
- [ ] Group mute/solo buttons functional
- [ ] Cue Buss monitoring works in Edit Dialog
- [ ] Routing matrix view implemented (optional)

#### Implementation Steps

**Step 1: Group Selector UI (1-2 days)**

```cpp
// ClipButton.cpp
void ClipButton::mouseDown(const juce::MouseEvent& e) {
  if (e.mods.isRightButtonDown()) {
    juce::PopupMenu menu;
    menu.addItem(1, "Edit Clip");
    menu.addSeparator();
    menu.addSectionHeader("Assign to Group");
    menu.addItem(10, "None", true, m_groupId == 0);
    menu.addItem(11, "Group A", true, m_groupId == 1);
    menu.addItem(12, "Group B", true, m_groupId == 2);
    menu.addItem(13, "Group C", true, m_groupId == 3);
    menu.addItem(14, "Group D", true, m_groupId == 4);

    int result = menu.show();
    if (result >= 10 && result <= 14) {
      setGroupId(result - 10);
    }
  }
}
```

**Step 2: Group Volume Faders (1 day)**

```cpp
// TransportControls.cpp
void TransportControls::createGroupControls() {
  const char* groupNames[] = { "Group A", "Group B", "Group C", "Group D" };

  for (int i = 0; i < 4; ++i) {
    auto slider = std::make_unique<juce::Slider>(juce::Slider::LinearVertical);
    slider->setRange(0.0, 1.0);
    slider->setValue(0.75); // -6dB default
    slider->onValueChange = [this, i]() {
      m_audioEngine->setGroupVolume(i, slider->getValue());
    };
    addAndMakeVisible(slider.get());
    m_groupVolumeSliders.push_back(std::move(slider));

    auto muteButton = std::make_unique<juce::TextButton>("M");
    muteButton->setClickingTogglesState(true);
    muteButton->onClick = [this, i, muteButton]() {
      m_audioEngine->setGroupMuted(i, muteButton->getToggleState());
    };
    addAndMakeVisible(muteButton.get());
    m_groupMuteButtons.push_back(std::move(muteButton));
  }
}
```

**Step 3: Cue Buss Monitoring (1-2 days)**

```cpp
// ClipEditDialog.cpp
void ClipEditDialog::createMonitorButton() {
  m_monitorButton = std::make_unique<juce::TextButton>("Monitor Cue");
  m_monitorButton->setClickingTogglesState(true);
  m_monitorButton->onClick = [this]() {
    bool monitor = m_monitorButton->getToggleState();
    m_audioEngine->setCueBussMonitoring(m_cueBussHandle, monitor);
    m_monitorButton->setColour(juce::TextButton::buttonColourId,
                                monitor ? juce::Colours::green : juce::Colours::grey);
  };
  addAndMakeVisible(m_monitorButton.get());
}
```

SDK integration:

```cpp
// AudioEngine.cpp
void AudioEngine::setCueBussMonitoring(orpheus::ClipHandle handle, bool monitor) {
  if (!m_transportController)
    return;

  // Route Cue Buss to separate output channel (e.g., channels 2-3 for headphones)
  orpheus::RoutingConfig config;
  config.cueBussOutputChannel = monitor ? 2 : 0; // 0 = disabled, 2 = output channel 2
  m_transportController->updateRoutingConfig(handle, config);
}
```

**Step 4: Effects Send Busses (1 day, design only)**

Design routing architecture, implement SDK-level support, defer UI to v1.0:

```cpp
// AudioEngine.h (future API)
class AudioEngine {
public:
  void setClipSendLevel(int buttonIndex, int sendBussId, float level); // 0.0-1.0
  float getClipSendLevel(int buttonIndex, int sendBussId) const;
};
```

Document design in OCC116 Routing Architecture.

**Step 5: Integration Testing (1 day)**

Test scenarios:
1. Load 4 clips, assign to different groups
2. Adjust group volume faders, verify audio levels
3. Mute/solo groups, verify audio routing
4. Enable cue buss monitoring, verify headphone output
5. Test edge cases (all clips in one group, no clips in any group)

#### Success Metrics

- **Group Assignment:** 100% functional (dropdown, color indicator)
- **Group Controls:** Volume, mute, solo all functional
- **Cue Buss Monitoring:** Works correctly in Edit Dialog
- **Test Pass Rate:** 100% (all routing scenarios pass)

#### Risk Assessment

**Technical Risks:**
- **Risk:** SDK routing API may not support 4 groups
  - **Mitigation:** Verify ORP110 Feature 1 (Routing Matrix API) completion
- **Risk:** Cue buss monitoring may introduce audio glitches
  - **Mitigation:** Test thoroughly, use SDK's routing API (designed for this)

**Success Probability:** 85% (depends on SDK routing API)

---

### Sprint B3: Production Hardening

**Objective:** Prepare for v1.0 production release - stability, error handling, edge cases

**Duration:** 10-15 person-days
**Priority:** CRITICAL for v1.0
**Dependencies:** Sprint B2 complete
**Target Version:** v1.0-rc.1

#### Deliverables

1. **Error Handling Audit**
   - Audit all error paths (file not found, device failure, out of memory)
   - Add graceful degradation (don't crash, show error dialog)
   - Log all errors to crash report file

2. **Edge Case Testing**
   - Test extreme scenarios:
     - 0 clips loaded
     - 960 clips loaded (20 tabs × 48 buttons)
     - Very long audio files (>1 hour)
     - Very short audio files (<10ms)
     - Corrupted audio files
     - Disconnected audio device
   - Document results, fix crashes

3. **Crash Reporting Integration**
   - Integrate crash reporter (e.g., Sentry, Crashlytics)
   - Capture stack traces on crash
   - Send crash reports automatically (with user consent)

4. **Diagnostic Script**
   - Create `collect-diagnostics.sh` script
   - Gather system info, logs, crash reports
   - User runs script, attaches to bug reports

5. **Long-Session Stability Testing**
   - Run app for 8 hours continuously
   - Load/unload clips every 5 minutes (stress test)
   - Monitor memory for leaks (expect flat memory usage)
   - Document any crashes or performance degradation

#### Acceptance Criteria

- [ ] No crashes in edge case testing (0% crash rate)
- [ ] Error dialogs shown for all error paths
- [ ] Crash reporter integrated, tested
- [ ] Diagnostic script created, tested
- [ ] 8-hour stability test passes (no crashes, no leaks)

#### Success Metrics

- **Crash Rate:** 0% (in controlled testing)
- **Error Handling:** 100% coverage (all error paths have dialogs)
- **Memory Leaks:** 0 (Instruments shows flat memory over 8 hours)
- **Diagnostic Script:** Works on macOS 10.15+ and 11.0+

---

### Sprint B4: Advanced Features & Polish

**Objective:** Add nice-to-have features for v1.0, final UX polish

**Duration:** 8-10 person-days
**Priority:** LOW - Post-v1.0 enhancements
**Dependencies:** Sprint B3 complete
**Target Version:** v1.0

#### Deliverables

1. **User-Configurable Limiter**
   - Add limiter threshold setting to Audio Settings dialog
   - Range: 0.0 to 1.0 (default: 0.9)
   - Real-time update (no need to restart)

2. **Advanced Metering**
   - Add peak/RMS/LUFS meters to master output
   - Color-coded thresholds (green/yellow/red)
   - Clip indicator (lights up when limiting occurs)

3. **Keyboard Shortcut Customization**
   - Add keyboard shortcut editor (macOS Preferences)
   - Allow users to remap all shortcuts
   - Export/import shortcut profiles

4. **Session Templates**
   - Create "New from Template" dialog
   - Bundled templates: Broadcast (8 tabs), Theater (4 tabs), Podcast (2 tabs)
   - Users can save custom templates

5. **Final UX Polish**
   - Refine visual design (icons, colors, spacing)
   - Add tooltips to all buttons
   - Improve accessibility (VoiceOver support)
   - Create video tutorials

#### Acceptance Criteria

- [ ] All 5 deliverables implemented
- [ ] User testing feedback incorporated
- [ ] v1.0 release-ready

---

## Sprint Sequencing

### Dependency Graph

```
A1 (Quick Wins) ──┬──> A2 (SDK Integration) ──┬──> A3 (Performance) ──> A4 (Testing) ──> A5 (UI/UX)
                  │                             │
                  └─────────────────────────────┘
                           (can run in parallel if A1 verification passes)

A5 ──> B1 (SDK Perf APIs) ──> B2 (Routing) ──> B3 (Hardening) ──> B4 (Polish) ──> v1.0
```

### Critical Path

**Critical Path:** A1 → A2 → A3 → B1 → B3 → v1.0
**Total Duration (Critical Path):** 21-32 person-days (~4-6 weeks for 1 developer)

### Parallel Opportunities

- **Sprint A1 verification tasks** can run in parallel (4 independent tests)
- **Sprint A4 test writing** can run in parallel with **Sprint A5 UI work** (if 2 developers available)
- **Sprint B2 routing UI** can start before **Sprint B1 performance** completes (if SDK APIs ready)

---

## Resource Requirements

### Skills Needed per Sprint

| Sprint | C++ | JUCE | QA  | Docs | SDK Integration |
| ------ | --- | ---- | --- | ---- | --------------- |
| A1     | ⭐   | ⭐   | ⭐⭐⭐ | ⭐   | -               |
| A2     | ⭐⭐⭐ | ⭐⭐  | ⭐   | ⭐   | ⭐⭐⭐             |
| A3     | ⭐⭐  | ⭐   | ⭐⭐⭐ | ⭐⭐  | -               |
| A4     | ⭐⭐⭐ | ⭐   | ⭐⭐  | -    | -               |
| A5     | ⭐⭐  | ⭐⭐⭐ | ⭐   | ⭐   | -               |
| B1     | ⭐⭐⭐ | ⭐⭐  | ⭐   | ⭐   | ⭐⭐⭐             |
| B2     | ⭐⭐  | ⭐⭐⭐ | ⭐   | ⭐   | ⭐⭐              |
| B3     | ⭐⭐  | ⭐   | ⭐⭐⭐ | ⭐⭐  | -               |
| B4     | ⭐   | ⭐⭐⭐ | ⭐   | ⭐⭐  | -               |

**Legend:** ⭐ = Basic, ⭐⭐ = Intermediate, ⭐⭐⭐ = Advanced

### Tooling Requirements

| Tool              | Sprints    | Purpose                  |
| ----------------- | ---------- | ------------------------ |
| Xcode/CLion       | All        | C++ IDE                  |
| JUCE Framework    | All        | UI framework             |
| GoogleTest        | A4, B3     | Unit testing             |
| Instruments       | A1, A3, B3 | Profiling, memory leaks  |
| Activity Monitor  | A1, A3     | CPU/memory measurement   |
| GitHub Actions    | A4, B1     | CI/CD                    |
| Sentry/Crashlytics| B3         | Crash reporting          |

### External Dependencies

| Dependency                 | Sprints | Risk      | Mitigation                           |
| -------------------------- | ------- | --------- | ------------------------------------ |
| SDK IPerformanceMonitor    | B1      | MEDIUM    | Verify ORP110 Feature 3 completion   |
| SDK Waveform API           | B1      | MEDIUM    | Verify ORP110 Feature 4 completion   |
| SDK Routing Matrix API     | B2      | LOW       | Verify ORP110 Feature 1 completion   |
| GoogleTest availability    | A4      | LOW       | Standard package, easy to install    |
| Crash reporting service    | B3      | LOW       | Multiple options (Sentry, Crashlytics)|

---

## Risk Assessment

### Technical Risks

| Risk                                      | Probability | Impact   | Mitigation                              |
| ----------------------------------------- | ----------- | -------- | --------------------------------------- |
| Multi-tab test fails (OCC105 contradiction)| 40%         | CRITICAL | Sprint A1 verifies immediately          |
| SDK APIs not ready for B1 integration    | 30%         | HIGH     | Verify ORP110 status before sprint     |
| Performance target not met at 192 clips  | 50%         | HIGH     | Fallback: Document limitation, fix v0.4.0 |
| Automated testing takes longer than estimated | 60%    | MEDIUM   | Defer some tests to Sprint B3           |
| Crash reporting integration issues       | 20%         | LOW      | Multiple vendor options available       |

### Process Risks

| Risk                                    | Probability | Impact | Mitigation                          |
| --------------------------------------- | ----------- | ------ | ----------------------------------- |
| Sprint A1 reveals more P0 bugs          | 30%         | HIGH   | Add Sprint A6 contingency (3-5 days)|
| QA testing reveals regressions          | 40%         | MEDIUM | Automated tests catch early (Sprint A4) |
| Documentation falls behind              | 50%         | LOW    | Allocate 1 day per sprint for docs |
| Resource allocation conflicts           | 20%         | MEDIUM | Prioritize P0 sprints first         |

---

## Success Metrics

### Sprint-Level KPIs

| Sprint | Primary KPI                     | Target             | Measurement Method        |
| ------ | ------------------------------- | ------------------ | ------------------------- |
| A1     | Multi-tab verification          | PASS or BUG FILED  | Manual test               |
| A2     | isClipPlaying() replaced        | 0 TODO comments    | Code inspection           |
| A3     | QA pass rate                    | ≥95%               | OCC103 test spec          |
| A4     | Test count                      | ≥50 unit tests     | ctest output              |
| A5     | Arrow key navigation functional | 100%               | Manual test               |
| B1     | Edit Dialog latency at 192 clips| <500ms             | Stopwatch measurement     |
| B2     | 4 Clip Groups functional        | 100%               | Manual routing test       |
| B3     | Crash rate (8 hour test)        | 0%                 | Stability test            |
| B4     | User satisfaction               | ≥4.5/5 stars       | User testing feedback     |

### Version Milestone KPIs

| Version | KPI                           | Target             | Measurement Method    |
| ------- | ----------------------------- | ------------------ | --------------------- |
| v0.2.3  | P0 gaps closed                | 8/8 (100%)         | OCC111 audit          |
| v0.2.3  | QA pass rate                  | ≥95%               | OCC103 test spec      |
| v0.2.4  | P1 gaps closed                | 12/12 (100%)       | OCC111 audit          |
| v0.2.4  | Automated test coverage       | ≥60%               | gcov/lcov             |
| v0.3.0  | Edit Dialog latency           | <500ms at 192 clips| Profiling             |
| v0.3.0  | 4 Clip Groups functional      | 100%               | Manual test           |
| v1.0    | Crash rate (production)       | <0.1%              | Crash reporter        |
| v1.0    | User satisfaction             | ≥4.5/5 stars       | App Store reviews     |

---

## Conclusion

This roadmap provides a clear path from v0.2.2 (current) to v1.0 (production-ready) over **11-15 weeks**.

### Immediate Actions (Week 1)

1. **Execute Sprint A1** (Quick Wins & Verification) - 2-3 days
   - Verify multi-tab playback
   - Verify CPU usage
   - Verify MAX_CLIPS
   - Test memory at 384 clips

2. **Execute Sprint A2** (OCC110 SDK Integration) - 1-2 days
   - Replace `isClipPlaying()` stub
   - Implement PreviewPlayer timer fix
   - Test graceful timer management

3. **Release v0.2.3** (End of Week 2)
   - Tag release
   - Create CHANGELOG.md
   - Update documentation

### Strategic Priorities

**Short-term (v0.2.3-v0.2.4, 6 weeks):**
- Close all P0 and P1 gaps
- Establish automated testing foundation
- Polish UI/UX for professional users

**Medium-term (v0.3.0, 11 weeks):**
- Integrate SDK performance APIs
- Fix 192-clip performance degradation
- Complete 4 Clip Groups feature

**Long-term (v1.0, 17 weeks):**
- Production hardening (error handling, stability)
- Advanced features (limiter config, metering, templates)
- User testing and feedback incorporation

### Success Criteria

**v1.0 Production Readiness:**
- [ ] All 47 gaps from OCC111 closed
- [ ] 100% QA test pass rate
- [ ] 0% crash rate in 8-hour stability test
- [ ] <500ms Edit Dialog latency at 192 clips
- [ ] ≥60% automated test coverage
- [ ] User satisfaction ≥4.5/5 stars

**Estimated Total Effort:** 53-76 person-days (~11-15 weeks for 1 developer)

---

## References

[1] OCC111 - Gap Audit Report (47 gaps identified)
[2] OCC021 - Product Vision (broadcast/theater market, €500-1,500 price point)
[3] OCC026 - 6-Month MVP Plan (v1.0 at 12 months)
[4] OCC099 - Testing Strategy
[5] OCC100 - Performance Requirements
[6] OCC103 - QA v0.2.0 Tests
[7] OCC105 - QA v0.2.0 Manual Test Log
[8] OCC108 - v0.2.1 Sprint Report
[9] OCC109 - v0.2.2 Sprint Report
[10] OCC110 - SDK Integration Guide
[11] ORP110 - SDK Features for Clip Composer Integration
[12] ORP112 - SDK Verification Report

---

**Document Status:** Complete
**Created:** 2025-11-12
**Planning Horizon:** 16 weeks (v0.2.3 → v1.0)
**Next Review:** After Sprint A1 completion (Week 1)
**Related Documents:** OCC111 Gap Audit Report (input), OCC113+ (sprint reports as sprints complete)
