<!-- SPDX-License-Identifier: MIT -->
# Orpheus SDK

The Orpheus SDK packages a host-neutral core for working with sessions,
clip grids, and renders. Optional adapters provide thin integration layers
for specific hosts, including a minimal standalone utility and a REAPER
extension. The codebase is designed to be friendly to continuous
integration, modern CMake workflows, and cross-platform development.

## Quickstart

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

By default the build produces the Orpheus core libraries, the reference
`orpheus_minhost` adapter, and the GoogleTest suite. Host-specific adapters
and demo applications are opt-in.

### Standalone Demo Host

To compile the JUCE-based demonstration host, enable the dedicated CMake flag:

```sh
cmake -S . -B build -DORPHEUS_ENABLE_APP_JUCE_HOST=ON
cmake --build build --target orpheus_demo_host_app
```

`OrpheusDemoHost` dynamically loads the Orpheus ABI shared libraries at
runtime. The menu flow mirrors the demo brief: **File → Open Session…** loads a
session JSON, **Session → Trigger ClipGrid Scene** negotiates the clip grid,
and **Session → Render WAV Stems…** writes the rendered stems to disk. The app
shows the active session summary and requires no DAW or plug-in.
The executable is produced as `OrpheusDemoHost` (with the usual platform
extension) inside your build directory.

### Rendering a Click Track

```sh
./build/orpheus_minhost \
  --session tools/fixtures/solo_click.json \
  --render click.wav \
  --bars 2 \
  --bpm 100
```

This command loads the provided session JSON, overrides the tempo to 100 BPM,
and renders a two-bar click track to `click.wav`. If you omit `--render`, the
minhost instead runs a short transport simulation using the session tempo (or
your override) and prints the suggested render path.

### Adapter Overview

Adapters are thin shims over the Orpheus SDK core. See
[`docs/ADAPTERS.md`](docs/ADAPTERS.md) for the complete matrix of supported
hosts and build flags.

## Tooling

* **Sanitizers** – AddressSanitizer and UBSan are enabled automatically for
  Debug builds on non-MSVC toolchains.
* **Static analysis** – Repository-wide `.clang-format` and `.clang-tidy`
  configurations enforce a consistent C++ style and checks.
* **Continuous Integration** – GitHub Actions builds and tests the project on
  Linux, macOS, and Windows for every push and pull request.

## Project Layout

```
├── adapters/        # Host integrations (minhost CLI, optional REAPER shim)
├── apps/            # Standalone demo hosts (JUCE demo)
├── cmake/           # Helper modules (warnings + third-party deps)
├── include/         # Public headers for the Orpheus core library
├── src/             # Core library implementation
├── tests/           # GoogleTest-based smoke tests
└── backup/          # Quarantined legacy SDK content (read-only)
```

