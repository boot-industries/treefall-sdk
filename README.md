<!-- SPDX-License-Identifier: MIT -->

# Orpheus SDK

Orpheus is a professional audio SDK that combines a host-neutral C++20 core
with optional adapter layers and UI prototypes. The repository contains the
deterministic session/transport engine, thin integrations for host software, and
a JavaScript workspace ("Shmui") for visualising session data without relying on
third-party cloud services.

## ⚡ Quick Start

**New contributor?** Get up and running in under 5 minutes:

```bash
# Clone and bootstrap (installs deps, builds core, runs tests)
git clone https://github.com/orpheus-sdk/orpheus-sdk.git
cd orpheus-sdk
./scripts/bootstrap-dev.sh

# Start the UI development server
pnpm --filter @orpheus/shmui dev
# Visit http://localhost:4000
```

**Prerequisites:** Node.js 18+, PNPM 8+, CMake 3.22+, C++20 compiler
**Troubleshooting:** See [`docs/TROUBLESHOOTING.md`](docs/TROUBLESHOOTING.md)
**Full guide:** [`docs/GETTING_STARTED.md`](docs/GETTING_STARTED.md)

---

## Table of Contents

- [Overview](#overview)
- [Core Capabilities](#core-capabilities)
- [Repository Layout](#repository-layout)
- [Supported Platforms](#supported-platforms)
- [Getting Started](#getting-started)
  - [C++ Toolchain](#c-toolchain)
  - [Optional Targets](#optional-targets)
  - [JavaScript Workspace](#javascript-workspace)
- [Demo Workflows](#demo-workflows)
  - [Standalone Demo Host](#standalone-demo-host)
  - [Render a Click Track](#render-a-click-track)
- [Tooling & Quality](#tooling--quality)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [License](#license)

## Overview

The Orpheus SDK targets hosts that need deterministic session negotiation and
render pipelines while remaining portable across Windows, macOS, and Linux. The
project is organised around three pillars:

1. **Core library** – `src/` + `include/` expose ABI negotiation, session graph
   modelling, and clip-grid utilities.
2. **Adapters** – thin CMake targets (minhost CLI, REAPER extension) that opt in
   to the features their host requires.
3. **Shmui UI workspace** – a Next.js/Turbopack playground that renders
   transport state and session timelines using local, mock data sources.

## Core Capabilities

- Host-agnostic APIs for session graphs, tempo maps, and clip grids.
- ABI negotiation helpers that keep host/plugin compatibility deterministic.
- **Real-time audio infrastructure** (Milestone M2) for sample-accurate playback:
  - Lock-free transport controller with multi-clip support
  - Audio file reader (WAV/AIFF/FLAC via libsndfile)
  - Platform audio drivers (CoreAudio, WASAPI, ASIO)
  - Network audio (AES67/RTP with PTP sync) - interoperable with Dante, Ravenna, Q-LAN
- Click-track rendering utilities surfaced through the minhost adapter.
- Optional JUCE-based demo host for hands-on exploration of the SDK.
- Smoke tests and tooling that enforce cross-platform correctness.

## Repository Layout

```
├── adapters/        # Host integrations (minhost CLI, optional REAPER shim)
├── apps/            # Standalone JUCE demo host
├── cmake/           # Helper modules and warning policies
├── docs/            # Architecture notes, roadmaps, adapter guides
├── include/         # Public headers for the Orpheus core library
├── packages/
│   ├── engine-native/  # Placeholder for language bindings around the core
│   └── shmui/          # TypeScript workspace for UI demos (Next.js/Turbo)
├── src/             # Core library implementation
├── tests/           # GoogleTest smoke and conformance coverage
└── backup/          # Quarantined legacy SDK content (read-only)
```

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

### JavaScript Workspace

The `packages/shmui` directory houses a pnpm workspace used for UI prototypes
and documentation tooling. It is optional but useful for visualising Orpheus
sessions.

1. Enable pnpm (via [`corepack`](https://nodejs.org/api/corepack.html) or a local
   installation) and bootstrap the workspace:

   ```sh
   pnpm install
   ```

2. Launch the Shmui demo site (Next.js on port 4000):

   ```sh
   pnpm --filter www dev
   ```

   The site uses mocked data by default so it can run without external network
   access.

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
  sources and executes the workspace linting scripts for TypeScript (see
  `package.json`). A project-wide `.clang-tidy` configuration is available for
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
code style (`.clang-format`, `.clang-tidy`) and ensure both `ctest` and relevant
pnpm lint/test scripts pass locally before submitting.

## License

This project is released under the [MIT License](LICENSE).
