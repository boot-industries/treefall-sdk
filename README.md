# Orpheus SDK

The Orpheus SDK provides a compact core library together with reference
adapters for integrating with REAPER and a standalone "minhost" utility. The
codebase is designed to be friendly to continuous integration, modern CMake
workflows, and cross-platform development.

## Quickstart

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

The default configuration builds the core library, the REAPER extension
(`reaper_orpheus`), the minimal host executable (`orpheus_minhost`), and the
GoogleTest suite.

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

### REAPER Adapter Panel

The shared library target `reaper_orpheus` exposes metadata and a panel string
that displays the negotiated ABI version. When the adapter is loaded by REAPER
(or another host that understands the extension API) it can surface the panel
text through the host UI.

## Tooling

* **Sanitizers** – AddressSanitizer and UBSan are enabled automatically for
  Debug builds on non-MSVC toolchains.
* **Static analysis** – Repository-wide `.clang-format` and `.clang-tidy`
  configurations enforce a consistent C++ style and checks.
* **Continuous Integration** – GitHub Actions builds and tests the project on
  Linux, macOS, and Windows for every push and pull request.

## Project Layout

```
├── adapters/        # Host integrations (REAPER extension and minhost CLI)
├── cmake/           # Helper modules (warnings + third-party deps)
├── include/         # Public headers for the Orpheus core library
├── src/             # Core library implementation
├── tests/           # GoogleTest-based smoke tests
└── backup/          # Quarantined legacy SDK content (read-only)
```

