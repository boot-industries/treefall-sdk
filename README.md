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
are opt-in.

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
├── cmake/           # Helper modules (warnings + third-party deps)
├── include/         # Public headers for the Orpheus core library
├── src/             # Core library implementation
├── tests/           # GoogleTest-based smoke tests
└── backup/          # Quarantined legacy SDK content (read-only)
```

