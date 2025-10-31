# OCC099 - Testing Strategy

**Version:** 1.0
**Date:** 2025-10-30
**Status:** Reference Documentation

Complete testing strategy for Orpheus Clip Composer, including unit tests, integration tests, and manual testing procedures.

---

## Overview

OCC testing follows a 3-tier approach:

1. **Unit Tests** - Test OCC-specific logic without SDK integration
2. **Integration Tests** - Test OCC + SDK together
3. **Manual Testing** - End-to-end validation, performance, stability

---

## Unit Tests (GoogleTest)

### Overview

Test OCC-specific logic in isolation. No SDK dependencies required.

### Framework

- **GoogleTest** (C++ testing framework)
- **Location:** `tests/clip_composer/unit/`
- **Build:** Integrated with CMake

### Example: SessionManager Tests

```cpp
// tests/clip_composer/unit/session_manager_test.cpp
#include <gtest/gtest.h>
#include "Session/SessionManager.h"

class SessionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager = std::make_unique<SessionManager>();
    }

    std::unique_ptr<SessionManager> manager;
};

TEST_F(SessionManagerTest, LoadValidSession) {
    auto result = manager->loadSession("test_fixtures/valid_session.json");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(manager->getClipCount(), 3);
    EXPECT_EQ(manager->getSessionName(), "Test Session");
}

TEST_F(SessionManagerTest, HandleInvalidJson) {
    auto result = manager->loadSession("test_fixtures/invalid.json");

    EXPECT_FALSE(result.success);
    EXPECT_THAT(result.errorMessage, testing::HasSubstr("Invalid JSON"));
}

TEST_F(SessionManagerTest, HandleMissingFile) {
    auto result = manager->loadSession("nonexistent.json");

    EXPECT_FALSE(result.success);
    EXPECT_THAT(result.errorMessage, testing::HasSubstr("not found"));
}

TEST_F(SessionManagerTest, SaveAndLoadRoundTrip) {
    // Create session with 3 clips
    manager->createNewSession("Roundtrip Test");
    manager->addClip(createTestClip(1));
    manager->addClip(createTestClip(2));
    manager->addClip(createTestClip(3));

    // Save to file
    auto saveResult = manager->saveSession("test_output/roundtrip.json");
    EXPECT_TRUE(saveResult.success);

    // Load in new manager instance
    auto manager2 = std::make_unique<SessionManager>();
    auto loadResult = manager2->loadSession("test_output/roundtrip.json");

    EXPECT_TRUE(loadResult.success);
    EXPECT_EQ(manager2->getClipCount(), 3);
    EXPECT_EQ(manager2->getSessionName(), "Roundtrip Test");
}

TEST_F(SessionManagerTest, HandleDuplicateClipHandles) {
    manager->createNewSession("Duplicate Test");

    auto clip1 = createTestClip(1);
    auto clip2 = createTestClip(1);  // Same handle

    EXPECT_TRUE(manager->addClip(clip1).success);
    EXPECT_FALSE(manager->addClip(clip2).success);  // Should reject duplicate
}
```

### Example: ClipMetadata Tests

```cpp
// tests/clip_composer/unit/clip_metadata_test.cpp
#include <gtest/gtest.h>
#include "Session/ClipMetadata.h"

TEST(ClipMetadataTest, ValidateHandleRange) {
    ClipMetadata clip;

    clip.handle = 0;
    EXPECT_FALSE(clip.isValid());  // Handle must be >= 1

    clip.handle = 1;
    EXPECT_TRUE(clip.isValid());

    clip.handle = 960;
    EXPECT_TRUE(clip.isValid());

    clip.handle = 961;
    EXPECT_FALSE(clip.isValid());  // Handle must be <= 960
}

TEST(ClipMetadataTest, ValidateButtonIndex) {
    ClipMetadata clip;
    clip.handle = 1;

    clip.buttonIndex = -1;
    EXPECT_FALSE(clip.isValid());

    clip.buttonIndex = 0;
    EXPECT_TRUE(clip.isValid());

    clip.buttonIndex = 119;
    EXPECT_TRUE(clip.isValid());

    clip.buttonIndex = 120;
    EXPECT_FALSE(clip.isValid());  // Max 120 buttons per tab
}

TEST(ClipMetadataTest, ValidateTabIndex) {
    ClipMetadata clip;
    clip.handle = 1;
    clip.buttonIndex = 0;

    clip.tabIndex = -1;
    EXPECT_FALSE(clip.isValid());

    clip.tabIndex = 0;
    EXPECT_TRUE(clip.isValid());

    clip.tabIndex = 7;
    EXPECT_TRUE(clip.isValid());

    clip.tabIndex = 8;
    EXPECT_FALSE(clip.isValid());  // Max 8 tabs
}

TEST(ClipMetadataTest, ValidateGainRange) {
    ClipMetadata clip;
    clip.handle = 1;
    clip.buttonIndex = 0;
    clip.tabIndex = 0;

    clip.gain = -49.0f;
    EXPECT_FALSE(clip.isValid());  // Below minimum

    clip.gain = -48.0f;
    EXPECT_TRUE(clip.isValid());

    clip.gain = 0.0f;
    EXPECT_TRUE(clip.isValid());

    clip.gain = 12.0f;
    EXPECT_TRUE(clip.isValid());

    clip.gain = 13.0f;
    EXPECT_FALSE(clip.isValid());  // Above maximum
}
```

---

## Integration Tests (with SDK)

### Overview

Test OCC + SDK together. Requires SDK to be built and available.

### Framework

- **GoogleTest** (C++ testing framework)
- **Location:** `tests/clip_composer/integration/`
- **Dependencies:** Orpheus SDK (ITransportController, IAudioDriver, etc.)

### Example: Clip Playback Integration Test

```cpp
// tests/clip_composer/integration/playback_test.cpp
#include <gtest/gtest.h>
#include "AudioEngine/AudioEngine.h"
#include "Session/SessionManager.h"
#include "UI/ClipGrid.h"

class PlaybackIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up SDK components
        transport = orpheus::createTransportController(nullptr, 48000);
        driver = orpheus::createDummyAudioDriver();

        // Set up OCC components
        sessionManager = std::make_unique<SessionManager>();
        clipGrid = std::make_unique<ClipGrid>(transport.get(), sessionManager.get());
    }

    std::unique_ptr<orpheus::ITransportController> transport;
    std::unique_ptr<orpheus::IAudioDriver> driver;
    std::unique_ptr<SessionManager> sessionManager;
    std::unique_ptr<ClipGrid> clipGrid;
};

TEST_F(PlaybackIntegrationTest, PlayClipFromButton) {
    // Load test session
    auto loadResult = sessionManager->loadSession("test_fixtures/single_clip.json");
    EXPECT_TRUE(loadResult.success);

    // Trigger clip from button
    clipGrid->simulateButtonClick(0, 0);  // Tab 0, button (0, 0)

    // Verify SDK received command
    auto clip = sessionManager->getClipAtButton(0, 0);
    auto state = transport->getClipState(clip.handle);
    EXPECT_EQ(state, orpheus::PlaybackState::Playing);
}

TEST_F(PlaybackIntegrationTest, StopAllClips) {
    // Load session with multiple clips
    sessionManager->loadSession("test_fixtures/multi_clip.json");

    // Start 3 clips
    clipGrid->simulateButtonClick(0, 0);
    clipGrid->simulateButtonClick(0, 1);
    clipGrid->simulateButtonClick(0, 2);

    // Verify all playing
    EXPECT_EQ(transport->getActiveClipCount(), 3);

    // Panic button (stop all)
    transport->stopAllClips();

    // Verify all stopped
    EXPECT_EQ(transport->getActiveClipCount(), 0);
}

TEST_F(PlaybackIntegrationTest, VerifySampleAccurateTiming) {
    // Load test session
    sessionManager->loadSession("test_fixtures/single_clip.json");

    // Start clip at known position
    auto clip = sessionManager->getClipAtButton(0, 0);
    transport->startClip(clip.handle);

    // Process exactly 1024 samples
    float* outputs[2] = { outputBuffer[0], outputBuffer[1] };
    transport->processAudio(outputs, 2, 1024);

    // Verify position is exactly 1024 samples
    auto position = transport->getClipPosition(clip.handle);
    EXPECT_EQ(position.samples, 1024);
}
```

### Example: Routing Integration Test

```cpp
// tests/clip_composer/integration/routing_test.cpp
#include <gtest/gtest.h>
#include "AudioEngine/AudioEngine.h"
#include "Session/SessionManager.h"
#include "UI/RoutingPanel.h"

class RoutingIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        routingMatrix = orpheus::createRoutingMatrix(48000);
        sessionManager = std::make_unique<SessionManager>();
        routingPanel = std::make_unique<RoutingPanel>(routingMatrix.get());
    }

    std::unique_ptr<orpheus::IRoutingMatrix> routingMatrix;
    std::unique_ptr<SessionManager> sessionManager;
    std::unique_ptr<RoutingPanel> routingPanel;
};

TEST_F(RoutingIntegrationTest, AssignClipToGroup) {
    // Load session
    sessionManager->loadSession("test_fixtures/single_clip.json");
    auto clip = sessionManager->getClipAtButton(0, 0);

    // Assign to group 1
    auto result = routingMatrix->setClipGroup(clip.handle, 1);
    EXPECT_EQ(result, orpheus::SessionGraphError::OK);

    // Verify assignment
    auto groupIndex = routingMatrix->getClipGroup(clip.handle);
    EXPECT_EQ(groupIndex, 1);
}

TEST_F(RoutingIntegrationTest, SetGroupGain) {
    // Set group 0 gain to -6.0 dB
    routingMatrix->setGroupGain(0, -6.0f);

    // Verify gain
    auto gain = routingMatrix->getGroupGain(0);
    EXPECT_NEAR(gain, -6.0f, 0.01f);
}

TEST_F(RoutingIntegrationTest, MuteAndSolo) {
    // Mute group 1
    routingMatrix->setGroupMute(1, true);
    EXPECT_TRUE(routingMatrix->getGroupMute(1));

    // Solo group 2
    routingMatrix->setGroupSolo(2, true);
    EXPECT_TRUE(routingMatrix->getGroupSolo(2));

    // Verify other groups are effectively muted
    EXPECT_TRUE(routingMatrix->isGroupAudible(2));  // Solo group is audible
    EXPECT_FALSE(routingMatrix->isGroupAudible(0)); // Non-solo groups are muted
    EXPECT_FALSE(routingMatrix->isGroupAudible(1)); // Explicitly muted
}
```

---

## Manual Testing Checklist

### Functional Testing

- [ ] **Session Management**
  - [ ] Load session with 960 clips
  - [ ] Save session and verify file contents
  - [ ] Handle missing audio files gracefully
  - [ ] Restore from auto-save after crash

- [ ] **Clip Playback**
  - [ ] Trigger clip from button (mouse click)
  - [ ] Trigger clip from keyboard shortcut
  - [ ] Stop individual clip
  - [ ] Panic button (stop all clips)
  - [ ] Verify sample-accurate playback

- [ ] **Routing**
  - [ ] Assign clip to each of 4 groups
  - [ ] Adjust group gain (verify smooth changes)
  - [ ] Mute/unmute groups
  - [ ] Solo groups (verify correct behavior)
  - [ ] Master gain and mute

- [ ] **Waveform Editor**
  - [ ] Display waveform for loaded clip
  - [ ] Set trim IN/OUT markers
  - [ ] Add/remove cue points
  - [ ] Verify playback respects trim markers

- [ ] **UI/UX**
  - [ ] Switch between 8 tabs
  - [ ] Color-code clips
  - [ ] Display clip names and durations
  - [ ] Keyboard shortcuts (play, stop, panic, tab switching)
  - [ ] Resize window (verify responsive layout)

### Performance Testing

- [ ] **Latency**
  - [ ] Measure round-trip latency with ASIO driver
  - [ ] Target: <5ms
  - [ ] Test with 512 and 1024 sample buffer sizes

- [ ] **CPU Usage**
  - [ ] Baseline (idle): <5%
  - [ ] 4 simultaneous clips: <15%
  - [ ] 16 simultaneous clips: <30%
  - [ ] Stress test: 32 simultaneous clips (should not crash)

- [ ] **Clip Capacity**
  - [ ] Load 960 clips (across 8 tabs)
  - [ ] Verify UI remains responsive
  - [ ] Target: <2 seconds to load session

- [ ] **Memory Usage**
  - [ ] Baseline (empty session): <50 MB
  - [ ] 960 clips loaded: <500 MB
  - [ ] No memory leaks after 1 hour session

### Stability Testing

- [ ] **24-Hour Stress Test**
  - [ ] Continuously trigger random clips
  - [ ] Verify no crashes, no memory leaks
  - [ ] Monitor CPU and memory over time
  - [ ] Target: 0 crashes, stable memory usage

- [ ] **Edge Cases**
  - [ ] Load session with invalid JSON (should fail gracefully)
  - [ ] Load session with missing audio files (should warn, continue)
  - [ ] Trigger clip while audio driver is disconnected
  - [ ] Rapid-fire button clicks (100+ clicks in 10 seconds)
  - [ ] Switch tabs while clips are playing
  - [ ] Adjust gain while clips are playing (verify no clicks/pops)

### Cross-Platform Testing

- [ ] **macOS**
  - [ ] Build and run on macOS 11+ (Big Sur)
  - [ ] Verify CoreAudio integration
  - [ ] Test with native audio interfaces

- [ ] **Windows**
  - [ ] Build and run on Windows 10/11
  - [ ] Verify WASAPI and ASIO integration
  - [ ] Test with native audio interfaces

- [ ] **Linux** (future)
  - [ ] Build and run on Ubuntu 22.04+
  - [ ] Verify ALSA/JACK integration

---

## Automated Testing (CI/CD)

### GitHub Actions Workflow

```yaml
# .github/workflows/test-occ.yml
name: OCC Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest, windows-latest]
        build_type: [Debug, Release]

    steps:
      - uses: actions/checkout@v3

      - name: Install Dependencies
        run: |
          # Install libsndfile, JUCE, etc.

      - name: Build SDK
        run: |
          cmake -S . -B build -DCMAKE_BUILD_TYPE=${{ matrix.build_type }}
          cmake --build build

      - name: Build OCC
        run: |
          cd apps/clip-composer
          cmake -S . -B build -DCMAKE_BUILD_TYPE=${{ matrix.build_type }}
          cmake --build build

      - name: Run Unit Tests
        run: |
          cd apps/clip-composer/build
          ctest --output-on-failure --label-regex "unit"

      - name: Run Integration Tests
        run: |
          cd apps/clip-composer/build
          ctest --output-on-failure --label-regex "integration"
```

---

## Performance Benchmarking

### Latency Benchmark

```cpp
// tests/clip_composer/benchmarks/latency_benchmark.cpp
#include <benchmark/benchmark.h>
#include "AudioEngine/AudioEngine.h"

static void BM_ClipStartLatency(benchmark::State& state) {
    auto transport = orpheus::createTransportController(nullptr, 48000);

    for (auto _ : state) {
        auto start = std::chrono::high_resolution_clock::now();
        transport->startClip(1);
        auto end = std::chrono::high_resolution_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        state.SetIterationTime(elapsed.count() / 1e6);
    }
}
BENCHMARK(BM_ClipStartLatency)->UseManualTime();

BENCHMARK_MAIN();
```

### CPU Usage Benchmark

```cpp
// tests/clip_composer/benchmarks/cpu_benchmark.cpp
#include <benchmark/benchmark.h>
#include "AudioEngine/AudioEngine.h"

static void BM_ProcessAudio_16Clips(benchmark::State& state) {
    auto transport = orpheus::createTransportController(nullptr, 48000);

    // Start 16 clips
    for (int i = 1; i <= 16; ++i) {
        transport->startClip(i);
    }

    float outputBuffer[2][512];
    float* outputs[2] = { outputBuffer[0], outputBuffer[1] };

    for (auto _ : state) {
        transport->processAudio(outputs, 2, 512);
    }

    state.SetItemsProcessed(state.iterations() * 512);
}
BENCHMARK(BM_ProcessAudio_16Clips);

BENCHMARK_MAIN();
```

---

## Test Fixtures

### Directory Structure

```
tests/clip_composer/
├── unit/
│   ├── session_manager_test.cpp
│   ├── clip_metadata_test.cpp
│   └── routing_panel_test.cpp
├── integration/
│   ├── playback_test.cpp
│   ├── routing_test.cpp
│   └── session_loading_test.cpp
├── benchmarks/
│   ├── latency_benchmark.cpp
│   └── cpu_benchmark.cpp
└── test_fixtures/
    ├── single_clip.json
    ├── multi_clip.json
    ├── invalid.json
    └── audio/
        ├── test_clip_1.wav
        ├── test_clip_2.wav
        └── test_clip_3.wav
```

### Example Test Fixture (JSON)

```json
// tests/clip_composer/test_fixtures/single_clip.json
{
  "sessionMetadata": {
    "name": "Test Session",
    "version": "1.0.0",
    "createdDate": "2025-01-01T00:00:00Z",
    "sampleRate": 48000
  },
  "clips": [
    {
      "handle": 1,
      "name": "Test Clip",
      "filePath": "tests/clip_composer/test_fixtures/audio/test_clip_1.wav",
      "buttonIndex": 0,
      "tabIndex": 0,
      "clipGroup": 0,
      "trimIn": 0,
      "trimOut": 48000,
      "gain": 0.0,
      "color": "#FF5733"
    }
  ],
  "routing": {
    "clipGroups": [
      { "name": "Music", "gain": 0.0, "mute": false },
      { "name": "SFX", "gain": 0.0, "mute": false },
      { "name": "Voice", "gain": 0.0, "mute": false },
      { "name": "Backup", "gain": 0.0, "mute": false }
    ]
  }
}
```

---

## Related Documentation

- **OCC096** - SDK Integration Patterns (code examples for testing)
- **OCC098** - UI Components (component implementations)
- **OCC100** - Performance Requirements (performance targets)

---

**Last Updated:** 2025-10-30
**Maintainer:** OCC Development Team
**Status:** Reference Documentation
