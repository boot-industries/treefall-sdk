# ORP261 ShmUI Segmented Meter Import Handoff

**Status:** Imported package handoff
**Date:** 2026-09-09

## Decision

Import ShmUI’s additive segmented Peak/RMS `LevelMeter` API through the governed
ShmUI → Treefall SDK source-authority path without changing the SDK release
version, C ABI, governed token-contract version, or existing continuous meter
path.

The imported package exposes `MeterBallistics::PeakRms`,
`MeterFillStyle::Segmented`, paired `setLevelPair` / `setLevelPairDB` producers,
and the SHM026 native fixture. Existing consumers continue using the continuous
single-value API unless they opt in.

## Imported provenance

| Field | Value |
| --- | --- |
| Upstream repository | `shmui` |
| Source revision | `f41fbd3fe4326a37d025a6c97a74c84cfa6ea2b6` |
| Governed content SHA-256 | `d0caa5f0daaa2324eae5c2c977e7c68fee7384f2b5eb836a08534f128370e3f3` |
| Token-contract version | `0.6.0` |
| Package target | `Orpheus::shmui_juce` |

`packages/shmui-juce/shmui-juce-import.json` records this source identity and
hash. The import was created only through ShmUI’s `scripts/sync-juce.sh`; no SDK
package file was hand-patched. The imported dependency-aware CMake definition
retains downstream Linux `CURL::libcurl` propagation.

## Package contract

SHM026 uses one lock-free 64-bit atomic per channel to publish coherent IEEE-754
peak/RMS pairs. Its visible-only JUCE timer consumes the pair at 60 Hz; no
audio-thread UI work is added. Scalar setters publish the same value as peak and
RMS. Fine-segment presentation resolves positive requested gaps to physical
pixels, caps grit count, maps colours from fixed midpoint dB positions, and
retains threshold and clip behavior.

Fine-segment consumers use a twelve-grit green-to-yellow transition. Needle
attack is immediate, release is bounded by the configured dB-per-second rate,
and reset and invalid-style sanitization are covered by the native fixture.
The style and API additions are additive; existing consumers preserve their
continuous gradient and legacy ballistics behavior.

## Verification

The governed delivery gate is:

```text
python3 tools/shmui_juce_manifest.py --sync
python3 tools/shmui_juce_manifest.py --check
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DORPHEUS_ENABLE_REALTIME=ON
cmake --build build --target shmui_shm026_segmented_meter_fixture --parallel 6
ctest --test-dir build --output-on-failure -R '^shmui_shm026_segmented_meter$'
ctest --test-dir build --output-on-failure
```

The SHM026 fixture covers coherent publication, 1×/2× gap rounding, bounded
segment count, fixed-position colour transitions, needle attack/release, reset,
and invalid-style sanitization. The full Debug suite includes the configured
`cmake_find_package`, `realtime_static_audit`, and `docs_path_audit` gates.

## Downstream adoption path

1. Fast-forward the verified ShmUI integration commit to ShmUI `main`.
2. Fast-forward this governed import to Treefall SDK `main`.
3. Clip Composer pins that exact verified SDK `main` commit and runs its native
   meter, full CTest, smoke, visual, and sustained-playback gates.

This preserves an auditable upstream → SDK package → application gitlink chain.

## Files

- `packages/shmui-juce/`
- `packages/shmui-juce/shmui-juce-import.json`
- `docs/orp/ORP261 ShmUI Segmented Meter Import Handoff.md`
