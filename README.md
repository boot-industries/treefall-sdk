<!-- SPDX-License-Identifier: MIT -->
# Orpheus SDK

The Orpheus Software Development Kit (SDK) supplies a host-neutral core for
negotiating sessions, clip grids, and renders. Thin adapter layers expose the
core to host applications, including a minimal standalone utility and a REAPER
extension, while remaining portable across Windows, macOS, and Linux.

## Table of Contents

- [Overview](#overview)
- [Key Features](#key-features)
- [Supported Platforms](#supported-platforms)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Quickstart](#quickstart)
  - [Optional Targets](#optional-targets)
- [Demo Workflows](#demo-workflows)
  - [Standalone Demo Host](#standalone-demo-host)
  - [Render a Click Track](#render-a-click-track)
- [Tooling & Quality](#tooling--quality)
- [Repository Layout](#repository-layout)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [License](#license)

## Overview

Orpheus offers a modern C++ foundation for hosts that need to negotiate and
render session data without relying on a particular digital audio workstation
(DAW). The core is intentionally host-neutral and built to be:

- **Modular** – adapters opt into only the integrations they require.
- **Portable** – CMake-based workflows keep Windows, macOS, and Linux builds in
  sync.
- **Automation-friendly** – the project embraces continuous integration and
  static analysis to maintain code health.

## Key Features

- Host-agnostic session negotiation and render APIs.
- Reference adapters, including a minimal command-line host and an optional
  REAPER extension.
- JUCE-based demo application for interactive exploration of the SDK.
- Click-track rendering utilities with overridable render specifications.
- GoogleTest-based smoke tests covering core functionality.

## Supported Platforms

The SDK is regularly built and tested on:

- Windows (MSVC toolchains for x64)
- macOS (Clang toolchains for x86_64 and arm64)
- Linux (GCC and Clang)

Other platforms may work but are not part of the automated coverage.

## Getting Started

### Prerequisites

- CMake 3.20 or newer.
- A C++20-capable compiler (MSVC 2019+, Clang 13+, or GCC 11+).
- Ninja or Make (optional, but recommended for faster incremental builds).
- Git, for fetching submodules and adapters as needed.

### Quickstart

Configure, build, and run the test suite in Debug mode:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

By default these commands produce the Orpheus core libraries, the
`orpheus_minhost` adapter, and the GoogleTest suite.

### Optional Targets

Additional components are disabled unless explicitly requested during
configuration:

- **JUCE demo application** – enable the standalone demonstration host:

  ```sh
  cmake -S . -B build -DORPHEUS_ENABLE_APP_JUCE_HOST=ON
  cmake --build build --target orpheus_demo_host_app
  ```

- **Host integrations** – each adapter advertises a CMake option documented in
  [`docs/ADAPTERS.md`](docs/ADAPTERS.md). Toggle only the integrations your
  environment supports.

## Demo Workflows

### Standalone Demo Host

`OrpheusDemoHost` dynamically loads the Orpheus ABI shared libraries at
runtime. The menu flow mirrors the demo brief:

1. **File → Open Session…** – load a session JSON file.
2. **Session → Trigger ClipGrid Scene** – negotiate the clip grid.
3. **Session → Render WAV Stems…** – write rendered stems to disk.

The application summarizes the active session and runs without a DAW or plugin
host. The resulting executable (`OrpheusDemoHost` with the usual platform
extension) is emitted inside your build directory.

### Render a Click Track

Generate a two-bar click track with an overridden tempo:

```sh
./build/orpheus_minhost \
  --session tools/fixtures/solo_click.json \
  --render click.wav \
  --bars 2 \
  --bpm 100
```

If you omit `--render`, the minhost performs a brief transport simulation and
prints the suggested render path instead of writing audio.

## Tooling & Quality

- **Sanitizers** – AddressSanitizer and UBSan are enabled automatically for
  Debug builds on non-MSVC toolchains.
- **Static analysis** – Repository-wide `.clang-format` and `.clang-tidy`
  configurations enforce a consistent style and catch common issues.
- **Continuous Integration** – GitHub Actions builds and tests the project on
  Linux, macOS, and Windows for every push and pull request.

## Repository Layout

```
├── adapters/        # Host integrations (minhost CLI, optional REAPER shim)
├── apps/            # Standalone demo hosts (JUCE demo)
├── cmake/           # Helper modules (warnings + third-party deps)
├── include/         # Public headers for the Orpheus core library
├── src/             # Core library implementation
├── tests/           # GoogleTest-based smoke tests
└── backup/          # Quarantined legacy SDK content (read-only)
```

## Documentation

- [`docs/ADAPTERS.md`](docs/ADAPTERS.md) – adapter catalog, build flags, and
  host-specific notes.
- [`ROADMAP.md`](ROADMAP.md) – planned milestones and long-term initiatives.
- [`ARCHITECTURE.md`](ARCHITECTURE.md) – design considerations for the modular
  core (if available in your checkout).

## Contributing

Issues and pull requests are welcome. Please open a discussion or issue before
contributing substantial changes so that design goals remain aligned. Follow
the existing code style (`.clang-format`, `.clang-tidy`) and ensure `ctest`
passes locally before submitting.

## License

This project is released under the [MIT License](LICENSE).

