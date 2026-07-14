<!-- SPDX-License-Identifier: MIT -->

# Orpheus SDK

**Professional audio SDK for broadcast, live performance, and DAW applications**

Orpheus is a host-neutral C++20 SDK that provides deterministic session/transport control, sample-accurate clip playback, and real-time audio infrastructure. Built for 24/7 broadcast reliability with zero-allocation audio threads and lock-free command processing.

**Current version:** 0.3.3 (pre-1.0 SDK; stable C ABI 1.0). The authoritative
value is `project(orpheus VERSION ...)` in [`CMakeLists.txt`](CMakeLists.txt);
`tools/version_contract.py` synchronizes public claims and CI rejects drift.

## ⚡ Quick Start

**New to Orpheus SDK?** Get up and running in under 5 minutes:

```bash
# Clone repository
git clone https://github.com/orpheus-sdk/orpheus-sdk.git
cd orpheus-sdk

# Build SDK (Debug with AddressSanitizer)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j8

# Run tests (270+ unit tests, core suite ~2 seconds)
ctest --test-dir build --output-on-failure
```

**Prerequisites:**

- CMake 3.22+
- C++20 compiler: VS 2022 MSVC, Apple Clang with libc++, or GCC 11+
- libsndfile (audio file I/O): Homebrew, apt, or vcpkg package

**Next Steps:**

- **Integrate SDK:** See [`docs/orp/_process/archive/GETTING_STARTED.md`](docs/orp/_process/archive/GETTING_STARTED.md)
- **Migrate from v0.x:** See [`docs/MIGRATION_v0_to_v1.md`](docs/MIGRATION_v0_to_v1.md)
- **View Changelog:** See [`CHANGELOG.md`](CHANGELOG.md)

## Lightweight Integration Targets

For downstream integrations that only need diagnostics or audio utilities, link the
thin targets instead of the full session/transport stack:

```cmake
find_package(OrpheusSDK CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE Orpheus::diagnostics Orpheus::audio_utils)
```

- `Orpheus::diagnostics` exposes `performance_monitor.h` and `loudness_meter.h`. Use
  `createStandalonePerformanceMonitor()` when no `SessionGraph` is involved.
- `Orpheus::audio_utils` exposes `audio_file_reader.h`,
  `audio_file_reader_extended.h`, and `channel_format.h` for file I/O and format
  conversion helpers.

---

## Table of Contents

- [Feature Highlights](#feature-highlights)
- [Core Capabilities](#core-capabilities)
- [Repository Layout](#repository-layout)
- [Supported Platforms](#supported-platforms)
- [Getting Started](#getting-started)
  - [C++ Toolchain](#c-toolchain)
  - [Optional Targets](#optional-targets)
  - [Running Tests](#running-tests)
- [Demo Workflows](#demo-workflows)
  - [Standalone Demo Host](#standalone-demo-host)
  - [Render a Click Track](#render-a-click-track)
- [Applications Built on Orpheus SDK](#applications-built-on-orpheus-sdk)
- [Tooling & Quality](#tooling--quality)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [License](#license)

## Feature Highlights

The SDK provides comprehensive clip playback control, metadata persistence, and professional workflow features:

### 🎚️ Gain Control API

```cpp
// Per-clip gain adjustment (-96 to +12 dB)
transport->updateClipGain(handle, -6.0f);  // Half amplitude
```

### 🔁 Loop Mode API

```cpp
// Seamless clip looping (no fade-out at boundary)
transport->setClipLoopMode(handle, true);
```

### 💾 Persistent Metadata

```cpp
// Batch update all clip settings (survives stop/start cycles)
ClipMetadata metadata;
metadata.trimInSamples = 1000;
metadata.gainDb = -6.0f;
metadata.loopEnabled = true;
transport->updateClipMetadata(handle, metadata);
```

### ⚡ Seamless Restart & Seek

```cpp
// Gap-free restart from IN point (sample-accurate)
transport->restartClip(handle);

// Sample-accurate seek for waveform scrubbing
transport->seekClip(handle, position);
```

### 🎛️ ORP109 Professional Features

**Added 2025-11-11:** Seven major features for professional workflows:

```cpp
// 1. Routing Matrix - Professional N×M audio routing
auto routing = createClipRoutingMatrix(sessionGraph, 48000);
routing->assignClipToGroup(clipHandle, 0);
routing->setGroupGain(0, -3.0f);

// 2. Audio Device Selection - Runtime device management
auto driverManager = createAudioDriverManager();
auto devices = driverManager->enumerateDevices();
driverManager->setActiveDevice(deviceId, 48000, 512);

// 3. Performance Monitoring - Real-time CPU/latency diagnostics
auto perfMonitor = createPerformanceMonitor(sessionGraph);
auto metrics = perfMonitor->getMetrics();

// 4. Waveform Pre-Processing - Fast UI rendering
auto reader = createAudioFileReaderExtended();
auto waveform = reader->getWaveformData(0, duration, 800, 0);

// 5. Scene/Preset System - Snapshot management
auto sceneManager = createSceneManager(sessionGraph);
std::string sceneId = sceneManager->captureScene("Act 1");

// 6. Cue Points/Markers - In-clip navigation
transport->addCuePoint(handle, 120000, "Verse 1", 0xFF0000FF);
transport->seekToCuePoint(handle, 0);

// 7. Multi-Channel Routing - 8-32 channel interfaces
routing->setClipOutputBus(clipHandle, 2);  // Route to channels 5-6
```

**Features:** 7 new APIs, 23 new data structures, 165+ new tests
**Documentation:** See [`docs/MIGRATION_v0_to_v1.md`](docs/MIGRATION_v0_to_v1.md) for complete guide

---

**See:** [`CHANGELOG.md`](CHANGELOG.md) for full release notes
**Migration:** [`docs/MIGRATION_v0_to_v1.md`](docs/MIGRATION_v0_to_v1.md) for upgrade guide

---

## Overview

The Orpheus SDK provides deterministic session/transport control for professional audio applications. Built for broadcast and live performance with 24/7 reliability.

**Key Design Principles:**

1. **Host-neutral Core** – C++20 library works across DAWs, plugins, and standalone apps
2. **Real-time Safe** – Zero allocations on audio thread, lock-free command processing
3. **Sample-accurate** – ±0 sample tolerance for transport operations
4. **Deterministic** – Same input → same output, always (bit-identical)

## Core Capabilities

### Transport & Playback

- **Multi-clip transport** – Simultaneous clip playback (tested with 16 clips)
- **Gain control** – Per-clip gain adjustment (-96 to +12 dB)
- **Loop mode** – Seamless clip looping with boundary enforcement
- **Trim points** – Sample-accurate IN/OUT boundaries
- **Fade curves** – Linear, EqualPower, Exponential
- **Restart/Seek** – Gap-free position control (±0 samples)
- **Cue points** – In-clip markers with seek-to-cue (ORP109)

### Audio I/O

- **Audio file reader** – WAV/AIFF/FLAC via libsndfile
- **Platform drivers** – CoreAudio (supported), Dummy (supported); WASAPI and Linux device backends are not yet release-supported
- **Dummy driver** – Testing and offline rendering
- **Device selection** – Runtime device enumeration and hot-swap (ORP109)
- **Waveform processing** – Fast downsampling for UI rendering (ORP109)

### Routing & Mixing

- **Routing matrix** – Professional N×M routing with solo/mute/metering (ORP109)
- **Multi-channel** – Support for 2-32 channel configurations (ORP109)
- **Clip Groups** – 4 Clip Groups → Master (simplified API for OCC)

### Performance & Diagnostics

- **Performance monitoring** – Real-time CPU/latency/underrun tracking (ORP109)
- **Real-time metering** – Peak/RMS/TruePeak/LUFS (ORP109)
- **Callback timing histogram** – Jitter profiling (ORP109)

### Workflow Management

- **Scene/Preset system** – Lightweight snapshot management (ORP109)
- **Session JSON** – Human-readable format with metadata persistence
- **Metadata persistence** – Clip settings survive stop/start cycles

### Developer Tools

- **Session graphs** – Tempo maps, clip grids, metadata storage
- **ABI negotiation** – Deterministic host/plugin compatibility
- **Click-track rendering** – Via minhost CLI adapter
- **Comprehensive tests** – 270+ unit tests (165+ ORP109), AddressSanitizer clean

## Repository Layout

```
├── adapters/           # Host integrations (minhost CLI, REAPER extension)
├── apps/               # In-tree apps (wave-finder smoke shell, juce-demo-host)
├── cmake/              # CMake helper modules and compiler policies
├── docs/               # Architecture, roadmaps, API reference, ORP documents
├── include/            # Public C++ headers (install these with your app)
├── packages/           # Shared C++/JUCE app packages (occ-app-platform, shmui-juce)
├── src/                # Core library implementation (C++20)
│   ├── core/           # Transport, routing, audio I/O, session
│   └── platform/       # Platform-specific drivers (CoreAudio, WASAPI, ASIO)
├── tests/              # GoogleTest unit tests (270+ tests, sanitizer-clean)
└── CHANGELOG.md        # Release notes and version history
```

**Package status (one authoritative sentence each):**

- `packages/occ-app-platform` — **active** C++ application-platform helpers
  (session recovery, preferences, health telemetry) consumed by the external
  Clip Composer repo through its SDK submodule.
- `packages/shmui-juce` — **active** JUCE UI component library (waveform,
  meters, transport widgets) consumed by downstream JUCE apps; not part of the
  core SDK libraries.
- The former **TypeScript** packages (`@orpheus/engine-*`, `@orpheus/client`,
  `@orpheus/shmui`) are **archived** — see
  [`docs/orp/_process/archive/DECISION_PACKAGES.md`](docs/orp/_process/archive/DECISION_PACKAGES.md)
  for the rationale (C++ SDK focus).

## Supported Platforms

The host-neutral core, Dummy driver, installed package, and conformance fixtures
are required on macOS, Windows, and Linux. Device backend support is narrower:
CoreAudio is supported on macOS; WASAPI is not yet a supported release backend;
and ALSA, JACK, and PipeWire are not implemented. Exact compiler, architecture,
backend, and unavailable-capability status lives in
[`docs/SUPPORT_MATRIX.md`](docs/SUPPORT_MATRIX.md). Planned backends are not
shipped capabilities.

## Getting Started

### C++ Toolchain

1. Install the prerequisites:
   - CMake 3.22+
   - A C++20-capable compiler (MSVC 2019+, Clang 13+, or GCC 11+)
   - Ninja or Make (optional, for faster incremental builds)
2. Configure, build, and test the core library:

   ```sh
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```

   These commands produce the `orpheus_core` static library, build the
   `orpheus_minhost` adapter, and run the GoogleTest suite by default.

### Optional Targets

Additional components are disabled unless explicitly requested during
configuration:

- **Real-time audio infrastructure** (M2 modules, enabled by default):

  ```sh
  # Disable if you only need offline rendering
  cmake -S . -B build -DORPHEUS_ENABLE_REALTIME=OFF
  ```

  Includes:
  - `orpheus_transport` – Lock-free transport controller for clip playback
  - `orpheus_audio_io` – Audio file reader and dummy driver (requires libsndfile)

  **Install libsndfile:** `brew install libsndfile` (macOS) or `vcpkg install libsndfile` (Windows)

- **JUCE demo application** – build an interactive host for session inspection:

  ```sh
  cmake -S . -B build -DORPHEUS_ENABLE_APP_JUCE_HOST=ON
  cmake --build build --target orpheus_demo_host_app
  ```

- **Host integrations** – toggle adapters via CMake cache entries. See
  [`docs/orp/_process/archive/ADAPTERS.md`](docs/orp/_process/archive/ADAPTERS.md)
  for the full list of flags and host requirements.

### Running Tests

Run all tests with detailed output:

```bash
# All tests (270+ tests, core unit suite ~2 seconds)
ctest --test-dir build --output-on-failure

# Specific test suite
./build/tests/transport/clip_gain_test        # Gain control tests
./build/tests/transport/clip_loop_test        # Loop mode tests
./build/tests/transport/clip_metadata_test    # Metadata persistence tests

# 16-clip stress test (60 seconds runtime)
./build/tests/transport/multi_clip_stress_test
```

**Test Coverage:**

- **Gain control:** 11/11 tests passing
- **Loop mode:** 11/11 tests passing
- **Metadata persistence:** 10/10 tests passing
- **Integration:** 16-clip stress test (60s, no memory leaks)

## Development Workflow

### Multi-Instance Development

This repository supports running multiple Claude Code instances simultaneously for focused development:

#### SDK Instance (Core Library Development)

**Working Directory:** `~/dev/orpheus-sdk` (repository root)

**Focus:** C++ core library, cross-platform packages, SDK-level infrastructure

**Start Instance:**

```bash
cd ~/dev/orpheus-sdk
claude-code
```

**Use For:**

- Core library changes (`src/`, `include/`)
- Transport, routing, session management
- SDK-level tests and benchmarks
- Cross-platform compatibility
- Documentation in `docs/orp/`

#### Clip Composer Instance (Application Development)

**Clip Composer is an external downstream repository** —
[`chrislyons/clip-composer`](https://github.com/chrislyons/clip-composer)
(local checkout: `~/dev/clip-composer`). It consumes this SDK as a git
submodule at `third_party/orpheus-sdk`. The former in-tree
`apps/clip-composer/` subdirectory was archived on 2026-07-09 (see
`docs/orp/ORP131`).

**Use the Clip Composer repo for:**

- Application-specific features, UI components, and workflows
- OCC documentation (`docs/occ/` in that repo)
- App builds and CI (both live there, not here)

**Use this SDK repo for:** the transport, routing, audio_io, and ABI work
Clip Composer depends on — then bump the submodule pin in the app repo.

#### When to Use Which Repo

| Task                         | Repo          | Reason                    |
| ---------------------------- | ------------- | ------------------------- |
| Fix transport controller bug | SDK           | Core library modification |
| Add new clip button feature  | Clip Composer | Application UI change     |
| Update audio driver          | SDK           | Platform infrastructure   |
| Implement session dialog     | Clip Composer | Application-specific UI   |
| Add routing matrix test      | SDK           | Core library testing      |
| Fix waveform display         | Clip Composer | Application UI component  |

**See also:** `CLAUDE.md` Multi-Instance Usage section for complete documentation

## Demo Workflows

### Standalone Demo Host

`OrpheusDemoHost` dynamically loads the Orpheus ABI shared libraries at runtime
and mirrors the demo workflow:

1. **File → Open Session…** – load a session JSON file.
2. **Session → Trigger ClipGrid Scene** – negotiate the clip grid.
3. **Session → Render WAV Stems…** – write rendered stems to disk.

The executable (`OrpheusDemoHost` plus the platform extension) is emitted inside
your build directory.

### Render a Click Track

Use the minhost CLI to generate a two-bar click track with an overridden tempo:

```sh
./build/orpheus_minhost \
  --session tools/fixtures/solo_click.json \
  --render click.wav \
  --bars 2 \
  --bpm 100
```

Omit `--render` to run a transport simulation and print the proposed render
graph instead of writing audio.

## Tooling & Quality

- **Sanitizers** – AddressSanitizer and UBSan are enabled automatically for
  Debug builds on non-MSVC toolchains.
- **Formatting & linting** – GitHub Actions runs `clang-format` against the C++
  sources. A project-wide `.clang-tidy` configuration is available for
  local static analysis, but it is not currently a required CI gate.
- **Continuous Integration** – GitHub Actions builds and tests the C++ targets
  on Linux, macOS, and Windows, verifies sanitizer builds, and checks for
  accidentally committed binary artifacts.

To experiment with `clang-tidy` locally, configure a build with
`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` and invoke `clang-tidy -p build` (or the
LLVM `run-clang-tidy.py` helper) on the files you want to inspect.

## Applications Built on Orpheus SDK

The Orpheus SDK provides the foundation for a family of professional audio applications:

### Orpheus Clip Composer (OCC) — external repo

**Professional soundboard for broadcast, theater, and live performance**

- **Repo:** [`chrislyons/clip-composer`](https://github.com/chrislyons/clip-composer)
  — a standalone downstream repository that consumes this SDK as a git
  submodule (`third_party/orpheus-sdk`). Extracted from this repo's former
  `apps/clip-composer/` subdirectory on 2026-07-09 (archival report:
  `docs/orp/ORP131`).
- **Features:** Clip triggering (384 buttons, 960-slot capacity), waveform editing, multi-channel routing, operator modes, cue-bus audition
- **Market:** Broadcast playout, theater sound design, live performance
- **Documentation:** `docs/occ/` in the Clip Composer repo (not here)

**SDK Requirements:** Real-time transport, audio drivers (CoreAudio/ASIO/WASAPI), routing matrix, performance monitor

### Orpheus FourTrack — external repo

**Portastudio-style multitrack recorder for macOS/iOS**

- **Repo:** `chrislyons/fourtrack` (local: `~/dev/fourtrack`) — consumes this
  SDK as a git submodule; exercises the SDK's host-neutrality (routing matrix,
  readers, CoreAudio input capture).

### Orpheus Wave Finder (in-tree)

**App-platform smoke-test shell** (`apps/wave-finder/`)

- **Status:** In-tree development shell exercising `packages/occ-app-platform`.
  Note: this is NOT the real FreqFinder analyzer, which lives in its own
  external repo (`~/dev/freqfinder`).

### Orpheus FX Engine

**LLM-powered effects processing and creative workflows**

- **Status:** Planned — not yet started
- **Features:** DSP integration, real-time parameter automation, LLM hooks

---

## Documentation

### PREFIX Registry

**ORP Docs (SDK):**
**PREFIX:** ORP
**Next Doc:** ORP138
**Location:** `docs/orp/`

**Discovery command:**

```bash
ls -1 docs/orp/ | sort
```

**OCC Docs (Clip Composer):** live in the external Clip Composer repo
([`chrislyons/clip-composer`](https://github.com/chrislyons/clip-composer),
`docs/occ/`) — not in this repository.

Documentation follows workspace pattern `docs/<prefix>/<PREFIX><NUM>.(md|mdx)` — see the workspace `CLAUDE.md` for full conventions.

### Reference Documentation

- [`docs/orp/_process/archive/ADAPTERS.md`](docs/orp/_process/archive/ADAPTERS.md) – adapter catalog, build flags, and
  host-specific notes.
- [`ROADMAP.md`](ROADMAP.md) – planned milestones and long-term initiatives.
- [`ARCHITECTURE.md`](ARCHITECTURE.md) – design considerations for the modular
  core.
- [Clip Composer repo](https://github.com/chrislyons/clip-composer) – Orpheus Clip Composer application + OCC documentation (external)
- [`docs/archive/AGENTS.md`](docs/archive/AGENTS.md) – coding assistant guidelines for AI tools
- [`CLAUDE.md`](CLAUDE.md) – Claude Code development guide

## Contributing

Issues and pull requests are welcome. Please discuss substantial changes in an
issue before opening a PR so design goals remain aligned. Follow the existing
code style (`.clang-format`, `.clang-tidy`) and ensure `ctest` passes locally before submitting.

## License

This project is released under the [MIT License](LICENSE).
