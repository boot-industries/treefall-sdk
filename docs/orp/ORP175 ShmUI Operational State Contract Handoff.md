<!-- SPDX-License-Identifier: MIT -->

# ORP175 — ShmUI Operational State Contract Handoff

**Document type:** Cross-repository generated-contract handoff  
**Status:** Completed; ShmUI PR #23 and SDK PR #244 merged
**Date:** 2026-08-10

## Decision

ShmUI operational-state contract v0.6.0 is sourced from ShmUI `main` commit
`2b1e1977f887cf99b05eff9a2294647553e90ae8` (PR #23). Its generated
foregrounds for `armed`, `cued`, `warning`, and `health` use the mode-appropriate
dark/inverse role, including the three Console light profiles.

The SDK owns the flattened JUCE mirror. The v0.6.0 handoff therefore mirrors
ShmUI `juce/Source`, `juce/tests`, and the adapted CMake file into
`packages/shmui-juce`, with the authoritative package content hash:

```text
f2b6641c53543deae341d7553c7a9949452b36fdf46017cd29b8cfe19380900a
```

`suite/orpheus-suite.json` records the same source revision and hash, and sets
both the SDK `shmui_juce_contract` and ShmUI design/import contract values to
`0.6.0`. The canonical CSS artifact remains pinned to the revision that last
changed that file; its content is unchanged by this operational-role delivery.

## Consumer decisions

| Consumer | Decision | Reason |
|---|---|---|
| Clip Composer | Keep the recorded SDK gitlink at `dcf5cc40`; merge the repaint-gate repair separately. | Its operational projection uses existing v0.5 JUCE primitives and does not reference a v0.6 generated role. |
| FourTrack | Keep generated Swift contract v0.5.0. | PR #66 is a product-local adapter. It does not claim v0.6 generated-role adoption. |
| FreqFinder | Consume current ShmUI `juce/` directly. | It has no generated-artifact pin and links the private `Orpheus::shmui_juce` source target. |

No product receives a compatibility alias or an implicit pin change. A product
moves to v0.6.0 only with its own generated-artifact provenance update and
consumer verification.

## Verification

Executed against the SDK candidate with the ShmUI package mirrored from
`2b1e1977f887cf99b05eff9a2294647553e90ae8`:

- `python3 tools/suite.py validate --json` — valid, zero errors.
- `python3 tools/shmui_juce_manifest.py --check` — 56 files, matching the
  `f2b664…900a` tree hash.
- `ctest --test-dir /private/tmp/omp-orchestration-sdk-build --output-on-failure -R '^cmake_shmui_package_consumer$'` — 1/1 passed.
- FreqFinder configured with this SDK candidate and ShmUI `main`, built
  `FreqFinderTests`, and ran `ctest -R '^FreqFinderTests$'` — 1/1 passed.
- FourTrack PR #66 rebased on `main` after the independent texture renderer
  repair; `FourTrack` and the `console_material_texture`, `fader_scale`, and
  `audio_route_policy` CTest cases passed while retaining its v0.5.0 Swift
  provenance.

## References

[1] GitHub, “feat(shm024): publish operational state parity contract,” pull request #23, ShmUI, Aug. 10, 2026. [Online]. Available: https://github.com/chrislyons/shmui/pull/23

[2] GitHub, “feat(shm024): mirror operational state parity,” pull request #244, Orpheus SDK, Aug. 10, 2026. [Online]. Available: https://github.com/chrislyons/orpheus-sdk/pull/244
