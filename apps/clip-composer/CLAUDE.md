# Orpheus Clip Composer - Application Development Guide

**Workspace:** This repo inherits general conventions from `~/chrislyons/dev/CLAUDE.md`

**Location:** `/apps/clip-composer/`
**Status:** v0.1.0-alpha Released (October 22, 2025)
**Framework:** JUCE 8.0.4
**SDK:** Orpheus SDK M2 (real-time infrastructure)

---

## Purpose

This guide helps developers build the **Orpheus Clip Composer** application—a professional soundboard for broadcast, theater, and live performance. This is an **application** that uses the Orpheus SDK as its audio engine foundation.

**What this guide covers:**

- Building and running Clip Composer
- Integrating with Orpheus SDK modules
- JUCE-specific patterns and conventions
- Session management and UI development
- Threading model and real-time safety

**What this guide does NOT cover:**

- Orpheus SDK core development (see `/CLAUDE.md` at repository root)
- Design specifications (see `docs/OCC/` for design documents)
- SDK module implementation (see `/src/core/` and SDK documentation)

---

## Quick Start

### Prerequisites

1. **Orpheus SDK built and tested:**

   ```bash
   cd /Users/chrislyons/dev/orpheus-sdk
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   cmake --build build
   ctest --test-dir build  # Verify all tests pass
   ```

2. **JUCE Framework installed:**
   - Download from https://juce.com/
   - Recommended: JUCE 7.x (latest stable)
   - License: JUCE Indie or higher (budget ~€800/year for commercial use)

3. **Development environment:**
   - macOS: Xcode 14+ (Apple Clang)
   - Windows: Visual Studio 2019+ (MSVC)
   - Linux: GCC 11+ or Clang 13+

### Building Clip Composer

```bash
cd /Users/chrislyons/dev/orpheus-sdk/apps/clip-composer
# TODO: CMake integration will be added once project structure is finalized
```

---

## Architecture Overview

Clip Composer follows a **5-layer architecture** (see `/docs/OCC/OCC023` for full details):

```
┌─────────────────────────────────────────────────────────┐
│ Layer 1: UI Components (JUCE)                          │
│  - ClipGrid (10×12 buttons × 8 tabs = 960 clips)       │
│  - WaveformDisplay, TransportControls, RoutingPanel    │
│  - RemoteControl (iOS companion app via OSC)           │
└─────────────────────────────────────────────────────────┘
                        ↓ (Message Thread)
┌─────────────────────────────────────────────────────────┐
│ Layer 2: Application Logic                             │
│  - SessionManager (load/save JSON sessions)            │
│  - ClipManager (track metadata, button assignments)    │
│  - RoutingManager (4 Clip Groups → Master)             │
│  - PreferencesManager (user settings)                  │
└─────────────────────────────────────────────────────────┘
                        ↓ (Lock-Free Commands)
┌─────────────────────────────────────────────────────────┐
│ Layer 3: Orpheus SDK Integration                       │
│  - ITransportController (clip playback)                │
│  - IAudioFileReader (WAV/AIFF/FLAC decoding)           │
│  - IRoutingMatrix (multi-channel routing)              │
│  - IPerformanceMonitor (CPU/latency diagnostics)       │
└─────────────────────────────────────────────────────────┘
                        ↓ (Audio Thread)
┌─────────────────────────────────────────────────────────┐
│ Layer 4: Real-Time Audio Processing                    │
│  - IAudioDriver (CoreAudio/ASIO/WASAPI)                │
│  - Mixing, gain smoothing, fade-out                    │
│  - Sample-accurate timing (±1 sample)                  │
└─────────────────────────────────────────────────────────┘
                        ↓ (Hardware)
┌─────────────────────────────────────────────────────────┐
│ Layer 5: Platform Audio I/O                            │
│  - Audio interfaces (2-32 channels)                    │
│  - ASIO/CoreAudio/WASAPI drivers                       │
└─────────────────────────────────────────────────────────┘
```

**Key Principle:** Layers 1-2 are OCC-specific, Layers 3-5 are Orpheus SDK (shared infrastructure).

---

## Threading Model

Clip Composer uses **3 threads** to maintain real-time performance:

### 1. Message Thread (UI Thread)

- **Owner:** JUCE MessageManager
- **Responsibilities:**
  - Handle UI events (button clicks, keyboard, mouse)
  - Update visual components (waveforms, meters, transport position)
  - Process SDK callbacks (`ITransportCallback::onClipStarted()`, etc.)
  - Save/load sessions (I/O operations allowed)
- **Rules:**
  - NO audio processing
  - NO blocking the audio thread
  - Use lock-free commands to communicate with audio thread

### 2. Audio Thread (Real-Time Thread)

- **Owner:** IAudioDriver (CoreAudio, ASIO, WASAPI)
- **Responsibilities:**
  - Process audio in `processAudio()` callback (~10ms @ 512 samples)
  - Read audio files via `IAudioFileReader`
  - Mix clips via `IRoutingMatrix`
  - Update transport position atomically
  - Post callbacks to message thread (lock-free queue)
- **Rules:**
  - ⛔ NO allocations (no `new`, `std::vector::push_back()`, etc.)
  - ⛔ NO locks (no `std::mutex`, no waiting)
  - ⛔ NO I/O (no file reads, no network calls)
  - ✅ Use lock-free structures (atomic operations only)

### 3. File I/O Thread (Background Thread)

- **Owner:** JUCE ThreadPool or custom worker
- **Responsibilities:**
  - Pre-load audio files for waveform display
  - Scan directories for new clips
  - Write recorded audio to disk
  - Calculate waveform data for UI
- **Rules:**
  - NO interaction with audio thread
  - Communicate with UI via callbacks on message thread

**Thread Safety Verification:**

- SDK provides lock-free primitives (see `ITransportController` implementation)
- Use `juce::MessageManager::callAsync()` for UI updates from background threads
- Never call JUCE UI components from audio thread

---

## SDK Integration Patterns

### Pattern 1: Starting a Clip

```cpp
// In UI button callback (Message Thread):
void ClipButton::mouseDown(const juce::MouseEvent& e)
{
    auto clipHandle = getClipHandle();  // From session metadata

    // Send command to audio thread (lock-free, non-blocking)
    auto result = transportController->startClip(clipHandle);

    if (result != orpheus::SessionGraphError::OK) {
        // Show error in UI
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Playback Error",
            "Failed to start clip: " + std::to_string(static_cast<int>(result))
        );
    }
}

// In audio callback (Audio Thread):
void AudioEngine::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // SDK processes commands and updates audio
    float* outputs[2] = { buffer.getWritePointer(0), buffer.getWritePointer(1) };
    transportController->processAudio(outputs, 2, buffer.getNumSamples());

    // NO UI updates here! Post to message thread via callbacks.
}

// In transport callback (Posted to Message Thread):
void AudioEngine::onClipStarted(orpheus::ClipHandle handle, orpheus::TransportPosition pos)
{
    // Safe to update UI now (we're on message thread)
    clipGrid->highlightPlayingClip(handle);
    transportDisplay->updatePosition(pos);
}
```

### Pattern 2: Loading an Audio File

```cpp
// In session loading (Message Thread):
void SessionManager::loadClip(const juce::File& audioFile)
{
    auto reader = orpheus::createAudioFileReader();

    // This is OK on message thread (not audio thread)
    auto result = reader->open(audioFile.getFullPathName().toStdString());

    if (!result.isOk()) {
        showError("Failed to load: " + result.errorMessage);
        return;
    }

    // Store metadata for UI
    ClipMetadata metadata;
    metadata.sampleRate = result.value.sample_rate;
    metadata.numChannels = result.value.num_channels;
    metadata.durationSamples = result.value.duration_samples;
    metadata.format = result.value.format;

    // Pre-render waveform on background thread
    renderWaveformAsync(reader, metadata);
}
```

### Pattern 3: Routing Configuration

```cpp
// In routing panel (Message Thread):
void RoutingPanel::assignClipToGroup(orpheus::ClipHandle clip, uint8_t groupIndex)
{
    // This will be lock-free once IRoutingMatrix is implemented
    auto result = routingMatrix->setClipGroup(clip, groupIndex);

    if (result == orpheus::SessionGraphError::OK) {
        updateRoutingDisplay();
    }
}

void RoutingPanel::setGroupGain(uint8_t groupIndex, float gainDb)
{
    // Gain changes are smoothed over 10ms in audio thread
    routingMatrix->setGroupGain(groupIndex, gainDb);
}
```

---

## Session Management

### Session Format (JSON)

Clip Composer sessions use JSON for human readability and version control friendliness (see `/docs/OCC/OCC009` and `/docs/OCC/OCC022` for complete schema).

**Location:** `~/Documents/Orpheus Clip Composer/Sessions/`

**Example structure:**

```json
{
  "sessionMetadata": {
    "name": "Evening Broadcast",
    "version": "1.0.0",
    "createdDate": "2025-10-12T12:00:00Z",
    "sampleRate": 48000
  },
  "clips": [
    {
      "handle": 1,
      "name": "Intro Music",
      "filePath": "/path/to/intro.wav",
      "buttonIndex": 0,
      "tabIndex": 0,
      "clipGroup": 0,
      "trimIn": 0,
      "trimOut": 240000,
      "gain": 0.0,
      "color": "#FF5733"
    }
  ],
  "routing": {
    "clipGroups": [
      { "name": "Music", "gain": 0.0, "mute": false },
      { "name": "SFX", "gain": -3.0, "mute": false },
      { "name": "Voice", "gain": 0.0, "mute": false },
      { "name": "Backup", "gain": -6.0, "mute": false }
    ]
  }
}
```

### Loading a Session

```cpp
void SessionManager::loadSession(const juce::File& sessionFile)
{
    // Parse JSON (use juce::JSON or nlohmann/json)
    auto json = juce::JSON::parse(sessionFile);

    if (json.isVoid()) {
        showError("Invalid session file");
        return;
    }

    // Stop all current playback
    transportController->stopAllClips();

    // Load clips sequentially (message thread, I/O is OK)
    auto clipsArray = json["clips"];
    for (int i = 0; i < clipsArray.size(); ++i) {
        auto clipJson = clipsArray[i];
        loadClipFromJson(clipJson);
    }

    // Restore routing configuration
    auto routingJson = json["routing"];
    restoreRoutingFromJson(routingJson);

    // Update UI
    clipGrid->refresh();
}
```

---

## UI Components (JUCE Conventions)

### Clip Grid (960 Buttons)

**Layout:** 10 columns × 12 rows per tab, 8 tabs total

```cpp
class ClipGrid : public juce::Component
{
public:
    ClipGrid(TransportController* transport, SessionManager* session)
        : transportController(transport), sessionManager(session)
    {
        // Create 10×12 = 120 buttons per tab
        for (int row = 0; row < 12; ++row) {
            for (int col = 0; col < 10; ++col) {
                auto button = std::make_unique<ClipButton>();
                button->onClick = [this, row, col] { onClipTriggered(row, col); };
                buttons.push_back(std::move(button));
                addAndMakeVisible(buttons.back().get());
            }
        }
    }

    void resized() override
    {
        // Use juce::Grid for responsive layout
        juce::Grid grid;
        grid.templateColumns = juce::Array<juce::Grid::TrackInfo>(10, juce::Grid::TrackInfo(1_fr));
        grid.templateRows = juce::Array<juce::Grid::TrackInfo>(12, juce::Grid::TrackInfo(1_fr));

        for (auto& button : buttons) {
            grid.items.add(juce::GridItem(button.get()).withMargin(2));
        }

        grid.performLayout(getLocalBounds());
    }

private:
    void onClipTriggered(int row, int col)
    {
        int buttonIndex = row * 10 + col;
        auto clip = sessionManager->getClipAtButton(currentTab, buttonIndex);

        if (clip.isValid()) {
            transportController->startClip(clip.handle);
        }
    }

    TransportController* transportController;
    SessionManager* sessionManager;
    std::vector<std::unique_ptr<ClipButton>> buttons;
    int currentTab = 0;
};
```

### Waveform Display

```cpp
class WaveformDisplay : public juce::Component, private juce::Timer
{
public:
    void setAudioFile(orpheus::IAudioFileReader* reader, const ClipMetadata& metadata)
    {
        // Pre-render waveform on background thread
        ThreadPool::getInstance()->addJob([this, reader, metadata]() {
            renderWaveform(reader, metadata);

            // Update UI on message thread
            juce::MessageManager::callAsync([this]() {
                repaint();
            });
        });
    }

    void paint(juce::Graphics& g) override
    {
        if (waveformData.empty()) {
            g.setColour(juce::Colours::grey);
            g.drawText("Loading...", getLocalBounds(), juce::Justification::centred);
            return;
        }

        g.setColour(juce::Colours::lightblue);

        // Draw waveform (simplified)
        float width = static_cast<float>(getWidth());
        float height = static_cast<float>(getHeight());
        float midY = height / 2.0f;

        for (size_t i = 0; i < waveformData.size(); ++i) {
            float x = (i / static_cast<float>(waveformData.size())) * width;
            float y = midY + (waveformData[i] * midY * 0.9f);

            g.drawLine(x, midY, x, y, 1.0f);
        }
    }

private:
    std::vector<float> waveformData;
};
```

---

## Testing Strategy

### Unit Tests (GoogleTest)

Test OCC-specific logic without SDK integration:

```cpp
// In tests/clip_composer/session_manager_test.cpp
#include <gtest/gtest.h>
#include "Session/SessionManager.h"

TEST(SessionManagerTest, LoadValidSession) {
    SessionManager manager;
    auto result = manager.loadSession("test_fixtures/valid_session.json");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(manager.getClipCount(), 3);
}

TEST(SessionManagerTest, HandleInvalidJson) {
    SessionManager manager;
    auto result = manager.loadSession("test_fixtures/invalid.json");

    EXPECT_FALSE(result.success);
    EXPECT_THAT(result.errorMessage, testing::HasSubstr("Invalid JSON"));
}
```

### Integration Tests (with SDK)

Test OCC + SDK together:

```cpp
// In tests/clip_composer/integration_test.cpp
TEST(ClipComposerIntegrationTest, PlayClipFromButton) {
    // Set up SDK components
    auto transport = orpheus::createTransportController(nullptr, 48000);
    auto driver = orpheus::createDummyAudioDriver();

    // Set up OCC components
    SessionManager session;
    ClipGrid grid(transport.get(), &session);

    // Load test session
    session.loadSession("test_fixtures/single_clip.json");

    // Trigger clip from button
    grid.simulateButtonClick(0, 0);  // Tab 0, button (0, 0)

    // Verify SDK received command
    auto state = transport->getClipState(session.getClipAtButton(0, 0).handle);
    EXPECT_EQ(state, orpheus::PlaybackState::Playing);
}
```

### Manual Testing Checklist

- [ ] Load session with 960 clips (stress test)
- [ ] Trigger 16 simultaneous clips (performance test)
- [ ] Verify <5ms latency with ASIO driver
- [ ] Test clip group routing (4 groups → master)
- [ ] Verify waveform editor (trim IN/OUT, cue points)
- [ ] Test remote control via OSC (iOS app)
- [ ] Run 24-hour stability test (no crashes, no memory leaks)

---

## Performance Requirements

From `/docs/OCC/OCC026` (Milestone 1 MVP):

- **Latency:** <5ms round-trip (ASIO driver)
- **CPU Usage:** <30% with 16 simultaneous clips (Intel i5 8th gen)
- **Clip Capacity:** 960 clips loaded (across 8 tabs)
- **File Formats:** WAV, AIFF, FLAC (via libsndfile)
- **Sample Rates:** 44.1kHz, 48kHz, 96kHz
- **Channels:** 2-32 (depending on audio interface)
- **MTBF:** >100 hours continuous operation

**Optimization Guidelines:**

- Pre-load audio files on background thread
- Use lock-free structures for all UI↔Audio communication
- Profile with Instruments (macOS) or Visual Studio Profiler (Windows)
- Target <10% CPU for UI rendering (leave headroom for audio)

---

## Design Documentation Reference

All design specifications live in `/docs/OCC/`:

**Product & Vision:**

- **OCC021** - Product Vision (authoritative) - Market positioning, competitive analysis
- **OCC026** - MVP Definition - 6-month plan, acceptance criteria

**Technical Specifications:**

- **OCC027** - API Contracts - C++ interfaces between OCC and SDK
- **OCC023** - Component Architecture - 5-layer architecture, threading model
- **OCC022** - Clip Metadata Schema - Complete JSON schema
- **OCC024** - User Interaction Flows - 8 complete workflows

**Technology Decisions:**

- **OCC025** - UI Framework Decision - JUCE vs Electron (JUCE recommended)
- **OCC028** - DSP Library Evaluation - Rubber Band vs SoundTouch (Rubber Band recommended)
- **OCC029** - SDK Enhancement Recommendations - Gap analysis, 5 critical modules
- **OCC030** - SDK Status Report - Current SDK status, implementation timeline

**Always reference design docs** before implementing features. If design is incomplete, update design docs first, then code.

---

## Development Workflow

### Phase 1: SDK Integration (Months 1-2)

**Goal:** Get basic clip playback working with SDK

1. ✅ SDK modules ready (ITransportController, IAudioFileReader, IAudioDriver)
2. Create JUCE project structure
3. Integrate SDK headers and libraries
4. Build "Hello World" OCC with dummy audio driver
5. Load a single clip and play it back
6. Verify audio callback integration

**Deliverable:** Single-clip playback demo

### Phase 2: Core UI (Months 2-3)

**Goal:** Build 960-button clip grid and basic transport controls

1. Implement ClipGrid component (10×12 buttons × 8 tabs)
2. Add transport controls (play, stop, panic)
3. Implement session loading (JSON parsing)
4. Add waveform display (pre-rendered on background thread)
5. Integrate keyboard shortcuts (trigger clips via keys)

**Deliverable:** Multi-clip triggering with visual feedback

### Phase 3: Routing & Mixing (Months 3-4)

**Goal:** Implement 4 Clip Groups with routing controls

1. Wait for SDK IRoutingMatrix implementation (Month 3-4)
2. Build routing panel UI (4 groups, gain/mute controls)
3. Assign clips to groups (metadata + UI)
4. Test 16 simultaneous clips with routing
5. Verify click-free gain smoothing

**Deliverable:** Full routing matrix with group controls

### Phase 4: Advanced Features (Months 4-6)

**Goal:** Waveform editor, remote control, diagnostics

1. Implement waveform editor (trim IN/OUT, cue points)
2. Add performance monitor UI (CPU meter, latency display)
3. Integrate OSC server for remote control (iOS app)
4. Add session save/load with metadata
5. Polish UI (themes, accessibility, keyboard navigation)

**Deliverable:** MVP ready for beta testing

### Phase 5: Beta & Polish (Month 6)

**Goal:** Beta testing, bug fixes, cross-platform validation

1. Recruit 10 beta testers (broadcast, theater, live performance)
2. Fix critical bugs reported in beta
3. Optimize performance (CPU, memory, latency)
4. Verify cross-platform compatibility (macOS + Windows)
5. Write user documentation

**Deliverable:** OCC MVP v1.0 release

---

## Known Limitations & Workarounds

### During Development (SDK Modules Pending)

**IRoutingMatrix not ready yet (Month 3-4):**

- **Workaround:** Use 1:1 clip-to-output routing (bypass groups)
- **Impact:** Can't test group routing UI, but basic playback works

**Platform drivers in progress (Month 2):**

- **Workaround:** Use dummy audio driver for development
- **Impact:** No real audio output, but logic verification works

**IPerformanceMonitor not ready yet (Month 4-5):**

- **Workaround:** Use JUCE's built-in CPU meter
- **Impact:** No detailed diagnostics, but basic monitoring works

### MVP Limitations (Deferred to v1.0)

From `/docs/OCC/OCC026`:

- No VST3/AU plugin hosting (v1.0 feature)
- No recording (playback only for MVP)
- No time-stretching (Rubber Band integration in v1.0)
- No network streaming (local files only)
- No aux sends (4 groups → master only)

**All limitations documented** in design docs and user-facing documentation.

---

## Troubleshooting

### Build Issues

**Problem:** `fatal error: 'orpheus/transport_controller.h' file not found`
**Solution:** Ensure SDK is built and CMake can find it:

```bash
cd /Users/chrislyons/dev/orpheus-sdk
cmake --build build
# Verify install includes are correct
```

**Problem:** Linker errors with libsndfile
**Solution:** Install libsndfile and rebuild SDK:

```bash
brew install libsndfile  # macOS
vcpkg install libsndfile  # Windows
```

**Problem:** JUCE modules not found
**Solution:** Set `JUCE_PATH` environment variable or update CMakeLists.txt:

```cmake
set(JUCE_PATH "/path/to/JUCE" CACHE PATH "JUCE framework location")
```

### Runtime Issues

**Problem:** Audio dropouts (buffer underruns)
**Solution:** Increase buffer size in audio settings (512 → 1024 samples)

**Problem:** High CPU usage (>50% with 8 clips)
**Solution:** Profile with Instruments/Visual Studio, check for:

- Waveform rendering on audio thread (should be background thread)
- UI updates blocking audio thread (use `callAsync()`)
- Inefficient file I/O (should be cached/pre-loaded)

**Problem:** Clips not starting (startClip() fails)
**Solution:** Check SDK error codes:

```cpp
auto result = transportController->startClip(handle);
if (result != orpheus::SessionGraphError::OK) {
    // Log specific error
    std::cerr << "Error: " << static_cast<int>(result) << std::endl;
}
```

---

## Communication with SDK Team

**Slack Channel:** `#orpheus-occ-integration`
**GitHub Issues:** Tag with `occ-blocker` for urgent SDK needs
**Weekly Sync:** Fridays, 30 minutes

**When to escalate to SDK team:**

- SDK API doesn't match OCC027 interface spec
- Performance issues in SDK modules (e.g., high CPU, buffer underruns)
- Missing functionality needed for OCC MVP
- Cross-platform bugs in SDK (macOS vs Windows)

**What SDK team provides:**

- Real-time audio infrastructure (Modules 1-5)
- Test fixtures (sample sessions, audio files)
- Integration examples (minimal playback code)
- Performance benchmarks (CPU, latency, memory)

**What OCC team provides:**

- Real-world usage patterns (drives SDK API design)
- Beta testing feedback (stability, performance, usability)
- Integration test results (reports bugs/issues)

---

## Success Metrics

From `/docs/OCC/OCC030` Section 10.3:

- **Month 2:** OCC playing real audio (CoreAudio/WASAPI)
- **Month 4:** OCC 16-clip demo with routing
- **Month 6:** OCC MVP beta (10 users)

**Definition of Done for MVP:**

- [ ] 960 clips loaded and displayable
- [ ] 16 simultaneous clips playing with routing
- [ ] <5ms latency with ASIO driver
- [ ] <30% CPU with 16 clips (Intel i5 8th gen)
- [ ] Session save/load with JSON
- [ ] Waveform editor (trim IN/OUT)
- [ ] Remote control via OSC (iOS app)
- [ ] 10 beta users successfully running OCC for 1+ hour sessions
- [ ] Zero crashes in 24-hour stability test

---

## File Organization

```
/apps/clip-composer/
├── CLAUDE.md              # This file (development guide)
├── CMakeLists.txt         # JUCE + Orpheus SDK integration
├── README.md              # Getting started for contributors
├── Source/                # JUCE application code
│   ├── Main.cpp           # Entry point
│   ├── MainComponent.h/cpp # Top-level application component
│   ├── ClipGrid/          # Clip grid UI (960 buttons)
│   │   ├── ClipGrid.h/cpp
│   │   ├── ClipButton.h/cpp
│   │   └── TabSelector.h/cpp
│   ├── AudioEngine/       # SDK integration layer
│   │   ├── AudioEngine.h/cpp
│   │   ├── TransportAdapter.h/cpp
│   │   └── RoutingAdapter.h/cpp
│   ├── Session/           # Session management
│   │   ├── SessionManager.h/cpp
│   │   ├── ClipMetadata.h/cpp
│   │   └── JsonParser.h/cpp
│   └── UI/                # Additional UI components
│       ├── WaveformDisplay.h/cpp
│       ├── TransportControls.h/cpp
│       ├── RoutingPanel.h/cpp
│       └── PerformanceMonitor.h/cpp
├── Resources/             # Icons, assets, session templates
│   ├── Icons/
│   ├── Templates/
│   └── Themes/
├── .claude/               # OCC-specific progress tracking
│   ├── README.md
│   └── implementation_progress.md
└── tests/                 # OCC-specific tests (optional, can use SDK test framework)
    ├── unit/
    └── integration/
```

---

## Quick Reference: Common Tasks

### Load and play a single clip:

```cpp
auto reader = orpheus::createAudioFileReader();
reader->open("/path/to/clip.wav");

auto transport = orpheus::createTransportController(nullptr, 48000);
transport->startClip(clipHandle);

auto driver = orpheus::createDummyAudioDriver();
driver->start(audioCallback);
```

### Stop all clips (panic button):

```cpp
transportController->stopAllClips();
```

### Update waveform display:

```cpp
waveformDisplay->setAudioFile(reader.get(), metadata);
waveformDisplay->repaint();
```

### Save current session:

```cpp
sessionManager->saveSession(juce::File("~/Documents/OCC/my_session.json"));
```

---

## Additional Resources

**Orpheus SDK Documentation:**

- `/CLAUDE.md` - SDK development guide (core principles)
- `/README.md` - Repository overview, build instructions
- `/ARCHITECTURE.md` - SDK design rationale
- `/ROADMAP.md` - Milestones, timeline

**OCC Design Documentation:**

- `/docs/OCC/README.md` - Complete documentation index
- `/docs/OCC/CLAUDE.md` - Design documentation standards
- `/docs/OCC/PROGRESS.md` - Design phase completion report

**JUCE Resources:**

- https://juce.com/learn/documentation - Official JUCE docs
- https://github.com/juce-framework/JUCE/tree/master/examples - Example projects
- https://forum.juce.com/ - Community forum

**External Libraries:**

- libsndfile: https://libsndfile.github.io/libsndfile/
- Rubber Band: https://breakfastquay.com/rubberband/ (v1.0 integration)

---

**Remember:** Clip Composer is a professional tool for broadcast, theater, and live performance. Design for 24/7 reliability, ultra-low latency, and zero crashes. When in doubt, favor simplicity, determinism, and user autonomy over short-term convenience.

**Last Updated:** October 22, 2025
**Status:** v0.1.0-alpha Released - v0.2.0 Planning In Progress
**Release:** https://github.com/chrislyons/orpheus-sdk/releases/tag/v0.1.0-alpha
**Next Review:** After v0.2.0 feature planning or first beta feedback
