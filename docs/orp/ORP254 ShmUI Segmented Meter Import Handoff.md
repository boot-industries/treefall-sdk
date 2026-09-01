# ORP254 ShmUI Segmented Meter Import Handoff

**Status:** Imported package handoff

## Decision

Import ShmUI’s additive segmented Peak/RMS `LevelMeter` API without changing the
SDK release version, C ABI, governed token-contract version, or the existing
continuous meter path.

The imported package exposes `MeterBallistics::PeakRms`,
`MeterFillStyle::Segmented`, paired `setLevelPair` / `setLevelPairDB` producers,
and the SHM026 native fixture. Existing consumers continue using the continuous
single-value API unless they opt in.

## Imported Provenance

| Field | Value |
| --- | --- |
| Upstream repository | `shmui` |
| Source revision | `a714344d93e8e84ddb1c194a500f26655cf73149` |
| Governed content SHA-256 | `3bcf27228f873d410ec075fed1b1c4cd96d81793d5baaa05a3f35e8be967cfb9` |
| Token-contract version | `0.6.0` |
| Package target | `Orpheus::shmui_juce` |

`packages/shmui-juce/shmui-juce-import.json` records this source identity and
hash. The import was created only through ShmUI’s `scripts/sync-juce.sh`; no
SDK package file was hand-patched.

## Package Contract

SHM026 uses one lock-free 64-bit atomic per channel to publish coherent
IEEE-754 peak/RMS pairs. Its visible-only JUCE timer consumes the pair at 60 Hz;
no audio-thread UI work is added. Fine-segment presentation resolves a positive
requested gap to at least one physical pixel, caps grit count at 1024, maps
colours from fixed midpoint dB positions, and retains existing yellow/orange/red
threshold behaviour.

Fine-segment consumers use a twelve-grit green-to-yellow transition; the
medium-segment treatment retains six. The generic style field remains
additive and consumer-configurable.

This is a package API addition only. The SDK version remains unchanged and
existing `LevelMeter` consumers preserve their continuous gradient and legacy
ballistics behavior.

## Verification

Observed in the adjacent SDK import worktree:

```text
python3 tools/shmui_juce_manifest.py --sync
python3 tools/shmui_juce_manifest.py --check
# ShmUI-JUCE manifest is consistent: 58 files,
# sha256 3bcf27228f873d410ec075fed1b1c4cd96d81793d5baaa05a3f35e8be967cfb9

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure \
  -R '^(shmui_juce_manifest|cmake_shmui_package_consumer)$'
# 2/2 passed
```

The package-consumer fixture compiled the imported `LevelMeter.cpp` through the
SDK’s clean producer/consumer path. The SHM026 fixture target explicitly enables
JUCE modal-loop support so its visible-peer timer pump is available in the
governed package build. Fine-segment transition coverage moved to twelve grits
in upstream revision `a714344`, then was synchronized and reverified rather
than patched here.

## Downstream Adoption Path

1. Push `feat/shm026-segmented-peak-rms-meter` and
   `feat/orp254-shmui-segmented-meter`.
2. Clip Composer fetches `origin/feat/orp254-shmui-segmented-meter` into its
   submodule and asserts the fetched SHA is the one tested locally.
3. Clip Composer records that remote-reachable SDK SHA as its gitlink only
   after its native fixture, meter tests, and full CTest gate pass.

This preserves an auditable upstream → SDK package → application gitlink chain.
