<!-- SPDX-License-Identifier: MIT -->

# ORP158 — Installed ShmUI-JUCE Package Qualification

**Document type:** SDK package-contract implementation record  
**Status:** Implemented and verified on macOS arm64  
**Date:** 2026-07-19  
**Scope:** ORP157 clean-prefix, non-OpenGL package qualification; no downstream repository change

---

## 1. Ref and checkout relationship

`git fetch --tags origin` reported current refs. At inspection:

| Ref | Commit | Relationship |
| --- | --- | --- |
| `origin/main` | `e663ade3f8022119fa968953c3e2ce95dfc1bd1e` | Exactly `v0.6.2` |
| `v0.6.2^{commit}` | `e663ade3f8022119fa968953c3e2ce95dfc1bd1e` | Immutable release tag |
| This checkout | `b6c07ddee239d34c2c874c1a76341af50e30bcde` | `docs/orp157-suite-coherence-handoff`, described as `v0.6.1-11-gb6c07dde` |

`git rev-list --left-right --count v0.6.2^{commit}...origin/main` returned `0 0`.
`git rev-list --left-right --count origin/main...HEAD` returned `2 1`: the checkout is one branch-local ORP157 handoff commit beyond its merge base while refreshed `main` contains two commits not in that checkout. No version, tag, downstream pin, or downstream application was changed.

## 2. Installed non-OpenGL contract

The optional `Orpheus::shmui_juce` path now has an SDK-owned package fixture that:

1. provisions pinned JUCE `8.0.4` from the existing checksum-verified archive;
2. configures Orpheus with `ORPHEUS_ENABLE_SHMUI_JUCE=ON` and
   `SHMUI_JUCE_ENABLE_OPENGL=OFF`;
3. builds and installs to an isolated prefix; and
4. configures, builds, links, and runs a separate FreqFinder-shaped consumer using
   only `OrpheusSDK_DIR` for Orpheus.

The consumer requires and links exactly:

- `Orpheus::core`;
- `Orpheus::audio_utils`; and
- `Orpheus::shmui_juce`.

It compiles installed `<ShmUI.h>`, installed Orpheus headers, calls the public
analysis utility, and resolves a Shmui theme symbol from the installed archive.
It rejects a present `Orpheus::shmui_juce_gl` target. It receives neither an
`ORPHEUS_SDK_SOURCE_DIR` nor a `SHMUI_JUCE_SOURCE_DIR`; no sibling-source or
application fallback is used.

`orpheus_shmui_juce` now compiles its JUCE-dependent sources in a private object
library and exports its installed archive/header path independently. Its installed
link interface names the five required application-provided JUCE module targets:
`juce_core`, `juce_audio_basics`, `juce_audio_formats`, `juce_dsp`, and
`juce_gui_basics`. The public target manifest now reports those exact lowercase
CMake target names. The imported-content manifest remained unchanged and passed.

The installed headers are under `include/orpheus/shmui-juce`; the installed target
publishes that prefix-relative include path. OpenGL remains disabled unless
`SHMUI_JUCE_ENABLE_OPENGL=ON` is explicitly selected.

## 3. Supported distribution contract and limitation

**Observed supported contract:** an application first establishes compatible JUCE
module targets (the fixture uses pinned JUCE 8.0.4 via `FetchContent`), then
resolves the installed Orpheus package and links `Orpheus::shmui_juce`. This is
compatible with FreqFinder's existing application-owned JUCE provisioning and does
not require an Orpheus or Shmui source-tree override.

**Observed limitation:** the JUCE package emitted into the producer's install prefix
was not independently consumable on this host. `find_package(JUCE 8.0.4 CONFIG)`
from that prefix failed before consumer compilation because its installed
`JUCEConfig.cmake` referenced absent `LV2_HELPER.cmake` and `VST3_HELPER.cmake`.
The attempted consumer also required C language enablement for JUCE. This record
therefore does not claim a standalone Orpheus-plus-bundled-JUCE prefix. The
application-provided JUCE contract above is the only qualified package route.

No Windows configuration, ABI, package, or hardware evidence was collected. This
work does not change the WASAPI support status. The fixture renders no audio
callback, and no realtime code or callback invariant changed.

## 4. Observed verification

All results below were observed in `build-whitebox` on macOS arm64:

```text
ctest --test-dir build-whitebox --output-on-failure \
  -R '^cmake_shmui_package_consumer$'
  passed: 1/1 (1632.54 s)

ctest --test-dir build-whitebox --output-on-failure \
  -R '^(cmake_find_package|realtime_static_audit)$'
  passed: 2/2 (270.85 s)

python3 tools/shmui_juce_manifest.py --check
  ShmUI-JUCE manifest is consistent: 105 files,
  sha256 c96a217f403dc208ef8ba29449a45f82c4258fb730e4863c09410a8b0ddddc5c

ctest --test-dir build-whitebox --output-on-failure -R '^docs_path_audit$'
  passed: 1/1 (1.01 s)

cmake --build build-whitebox --parallel 8
ctest --test-dir build-whitebox --output-on-failure
  147/152 passed; 5 failed (1824.43 s)
  failures: OscillatorTest.ProcessesEfficiently, performance_monitor_test,
  performance_integration_test, multi_clip_stress_test, coreaudio_driver_test
```

The full-suite result is not a release-qualified pass. The observed oscillator
throughput was 229879 samples/s against its 500000 samples/s threshold, and the
performance monitor measured `getMetrics()` at 2372 ns against its 1000 ns
threshold. The CoreAudio run reported unavailable default-device topology,
skipped five hardware-dependent cases, and failed 15 driver assertions inside
`coreaudio_driver_test`. This record does not assign those failures to ORP158;
the changed package fixture, installed-package gate, realtime static audit,
manifest check, and documentation-path audit passed in the same configured tree.

The package fixture has an explicit 5400-second CTest timeout because a fresh
macOS JUCE producer/consumer build exceeds the suite's default 1500-second test
timeout.
