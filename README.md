<!-- SPDX-License-Identifier: MIT -->

# Orpheus SDK

**Professional audio SDK for broadcast, live performance, and DAW applications**

Orpheus is a host-neutral C++20 SDK that provides deterministic session/transport control, sample-accurate clip playback, and real-time audio infrastructure. Built for 24/7 broadcast reliability with zero-allocation audio threads and lock-free command processing.

**Current Release:** v1.0.0-rc.1 (2025-10-31)

## ⚡ Quick Start

**New to Orpheus SDK?** Get up and running in under 5 minutes:

```bash
# Clone repository
git clone https://github.com/orpheus-sdk/orpheus-sdk.git
cd orpheus-sdk

# Build SDK (Debug with AddressSanitizer)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j8

# Run tests (32 tests, should complete in ~2 seconds)
ctest --test-dir build --output-on-failure
```

**Prerequisites:**

- CMake 3.22+
- C++20 compiler (MSVC 2019+, Clang 13+, GCC 11+)
- libsndfile (audio file I/O): `brew install libsndfile` (macOS) or `vcpkg install libsndfile` (Windows)

**Next Steps:**

- **Integrate SDK:** See [`docs/GETTING_STARTED.md`](docs/GETTING_STARTED.md)
- **Migrate from v0.x:** See [`docs/MIGRATION_v0_to_v1.md`](docs/MIGRATION_v0_to_v1.md)
- **View Changelog:** See [`CHANGELOG.md`](CHANGELOG.md)

---

## Table of Contents

- [What's New in v1.0](#whats-new-in-v10)
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

## What's New in v1.0

Orpheus SDK v1.0 introduces comprehensive clip playback control and metadata persistence:

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

### Audio I/O

- **Audio file reader** – WAV/AIFF/FLAC via libsndfile
- **Platform drivers** – CoreAudio (macOS), WASAPI/ASIO (Windows)
- **Dummy driver** – Testing and offline rendering

### Routing & Mixing

- **Routing matrix** – 4 Clip Groups → Master (or custom topologies)
- **Multi-channel** – Support for 2-32 channel configurations

### Developer Tools

- **Session graphs** – Tempo maps, clip grids, metadata storage
- **ABI negotiation** – Deterministic host/plugin compatibility
- **Click-track rendering** – Via minhost CLI adapter
- **Comprehensive tests** – 32 unit tests, AddressSanitizer clean

## Repository Layout

```
├── adapters/           # Host integrations (minhost CLI, REAPER extension)
├── apps/               # Applications (Clip Composer, demo host)
├── cmake/              # CMake helper modules and compiler policies
├── docs/               # Architecture, roadmaps, API reference, ORP documents
├── include/            # Public C++ headers (install these with your app)
├── src/                # Core library implementation (C++20)
│   ├── core/           # Transport, routing, audio I/O, session
│   └── platform/       # Platform-specific drivers (CoreAudio, WASAPI, ASIO)
├── tests/              # GoogleTest unit tests (58 tests, 100% passing)
└── CHANGELOG.md        # Release notes and version history
```

**Note:** TypeScript packages previously in `packages/` have been archived. See [`docs/DECISION_PACKAGES.md`](docs/DECISION_PACKAGES.md) for rationale (C++ SDK focus).

## Supported Platforms

The core SDK is regularly built and tested on:

- Windows (MSVC toolchains for x64)
- macOS (Clang toolchains for x86_64 and arm64)
- Linux (GCC and Clang)

Other platforms may work but are not part of automated coverage.

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
  [`docs/ADAPTERS.md`](docs/ADAPTERS.md) for the full list of flags and host
  requirements.

### Running Tests

Run all tests with detailed output:

```bash
# All tests (32 tests, ~2 seconds)
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

**Working Directory:** `~/dev/orpheus-sdk/apps/clip-composer`

**Focus:** Tauri desktop application, JUCE UI components, end-user features

**Start Instance:**

```bash
cd ~/dev/orpheus-sdk/apps/clip-composer
claude-code

# Or use the shortcut script:
~/dev/orpheus-sdk/scripts/start-clip-composer-instance.sh
```

**Use For:**

- Application-specific features
- UI components and workflows
- OCC-specific functionality
- Documentation in `apps/clip-composer/docs/occ/`

#### When to Use Which Instance

| Task                         | Instance      | Reason                    |
| ---------------------------- | ------------- | ------------------------- |
| Fix transport controller bug | SDK           | Core library modification |
| Add new clip button feature  | Clip Composer | Application UI change     |
| Update audio driver          | SDK           | Platform infrastructure   |
| Implement session dialog     | Clip Composer | Application-specific UI   |
| Add routing matrix test      | SDK           | Core library testing      |
| Fix waveform display         | Clip Composer | Application UI component  |

#### Instance Isolation Benefits

- **No config collision** – Separate `.claude/` directories
- **Context-appropriate skills** – Different tool sets per instance
- **Clear documentation boundaries** – ORP vs OCC prefixes
- **Independent progress tracking** – Separate implementation logs

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

### Orpheus Clip Composer (OCC)

**Professional soundboard for broadcast, theater, and live performance**

- **Status:** Design phase complete, implementation starting (6-month MVP)
- **Features:** Clip triggering (960 buttons), waveform editing, multi-channel routing, iOS remote control
- **Market:** Broadcast playout, theater sound design, live performance
- **Documentation:** [`apps/clip-composer/docs/OCC/`](apps/clip-composer/docs/OCC/) – Complete design package (11 documents, ~5,300 lines)
  - [`OCC021 Product Vision`](apps/clip-composer/docs/OCC/OCC021%20Orpheus%20Clip%20Composer%20-%20Product%20Vision.md) – Market positioning, competitive analysis
  - [`OCC026 MVP Definition`](apps/clip-composer/docs/OCC/OCC026%20Milestone%201%20-%20MVP%20Definition%20v1.0.md) – 6-month plan with deliverables
  - [`OCC029 SDK Enhancements`](apps/clip-composer/docs/OCC/OCC029%20SDK%20Enhancement%20Recommendations%20for%20Clip%20Composer%20v1.0.md) – Required SDK modules
  - [`PROGRESS.md`](apps/clip-composer/docs/OCC/PROGRESS.md) – Design phase complete report

**SDK Requirements:** Real-time transport, audio drivers (CoreAudio/ASIO/WASAPI), routing matrix, performance monitor

### Orpheus Wave Finder

**Harmonic calculator and frequency scope for audio analysis**

- **Status:** Planned for v1.0 (Months 7-12)
- **Features:** FFT analysis, harmonic detection, frequency visualization

### Orpheus FX Engine

**LLM-powered effects processing and creative workflows**

- **Status:** Planned for v1.0 (Months 10-12)
- **Features:** DSP integration, real-time parameter automation, LLM hooks

---

## Documentation

### PREFIX Registry

**ORP Docs (SDK):**
**PREFIX:** ORP
**Next Doc:** ORP098.md
**Location:** `docs/orp/`

**Discovery command:**

```bash
ls -1 docs/orp/ | grep -E "^ORP[0-9]{3,4}\.(md|mdx)$" | sort
```

**OCC Docs (Clip Composer):**
**PREFIX:** OCC
**Next Doc:** OCC096.md
**Location:** `apps/clip-composer/docs/occ/`

**Discovery command:**

```bash
ls -1 apps/clip-composer/docs/occ/ | grep -E "^OCC[0-9]{3,4}\.(md|mdx)$" | sort
```

Documentation follows workspace pattern `docs/<prefix>/<PREFIX><NUM>.(md|mdx)` — see `~/chrislyons/dev/CLAUDE.md` for full conventions.

### Reference Documentation

- [`docs/ADAPTERS.md`](docs/ADAPTERS.md) – adapter catalog, build flags, and
  host-specific notes.
- [`ROADMAP.md`](ROADMAP.md) – planned milestones and long-term initiatives.
- [`ARCHITECTURE.md`](ARCHITECTURE.md) – design considerations for the modular
  core.
- [`apps/clip-composer/docs/occ/`](apps/clip-composer/docs/occ/) – Orpheus Clip Composer design documentation (complete)
- [`AGENTS.md`](AGENTS.md) – coding assistant guidelines for AI tools
- [`CLAUDE.md`](CLAUDE.md) – Claude Code development guide

## Contributing

Issues and pull requests are welcome. Please discuss substantial changes in an
issue before opening a PR so design goals remain aligned. Follow the existing
code style (`.clang-format`, `.clang-tidy`) and ensure `ctest` passes locally before submitting.

## License

This project is released under the [MIT License](LICENSE).
