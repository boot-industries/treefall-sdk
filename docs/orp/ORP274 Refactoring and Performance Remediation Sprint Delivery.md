<!-- SPDX-License-Identifier: MIT -->

# ORP274 - Refactoring and Performance Remediation Sprint Delivery

**Document type:** Sprint delivery record for the ORP273 remediation
**Date:** 2026-10-22
**Sprint branch:** `feat/orp273-sprint-remediation`
**Base revision:** `ad62b0f8ca1f4a705fd28451b9cd5e805a0cc2b2` (branch `main`)
**Implements:** `docs/orp/ORP273 Refactoring and Performance Optimization Review.md` (findings F-01 … F-20)
**Scope:** `src/core/transport/`, `src/core/routing/`, `src/core/session/`,
`src/core/common/`, `src/platform/audio_drivers/coreaudio/`,
`include/orpheus/`, `tests/`, `scripts/`, `.github/workflows/`,
`ARCHITECTURE.md`, `budgets.json`
**Status:** In progress

## 1. Executive summary

ORP273 recorded twenty findings against the SDK's render path, realtime
backends, session layer, build tooling and documentation. This sprint lands a
remediation for every one of them, in the wave order ORP273 §5 prescribes:
Wave 0 (measurement hygiene) → Wave 1 (realtime correctness) → Wave 2
(render-path redundancy) → Wave 3 (structural decomposition) → Wave 4
(tooling, calibration and documentation).

The wave order is not cosmetic. Waves 1 through 3 change rendered bytes,
metering arithmetic and per-block work; Wave 0's job is to make the repository
able to *prove* what those waves did. Landing a performance change before a
registered, unsanitized, thresholded measurement exists would produce exactly
the kind of unmeasurable claim ORP273 §5 warns about. So the sprint starts by
making the performance-budget validator honest (F-07), by guarding timing
assertions under sanitizers (F-17), by registering the render-path deadline
measurement that already exists in the tree but was invisible (F-18), and by
bringing `ARCHITECTURE.md` back in line with the code (F-19).

Wave 1 then fixes the realtime-path defects: the CoreAudio device callback no
longer notifies a waiter (F-01), every CoreAudio route gets one explicit
callback-frame ceiling (F-11), and the render path's libm dependence is
replaced with pinned, platform-independent elementary functions so the
render-hash golden can finally be pinned as a constant (F-02).

Wave 2 removes the per-block and per-sample redundancy the review measured:
the whole-pool lane memset (F-03), the three out-of-line smoother calls per
sample (F-04), the redundant meter scratch clears (F-09), the second
full-sample TruePeak pass (F-10), the per-group headroom rescan (F-08), the
17,936-byte telemetry rebuild (F-05), the compressor's constant `pow` term
(F-06), the per-clip session sort/validate (F-13) and the worker's quadratic
identity scan (F-20).

Wave 3 is structural: explicit JSON and session-document limits (F-12), an
allocation-free transaction rollback (F-14), the extraction of the control-side
clip-source registry (F-15) and of the matrix-level meter state (F-16).

Wave 4 calibrates the F-18 threshold against the Wave 2 render path and
finalises the documentation.

Three scope decisions are taken here and are not reopened later. They are
stated in full in §6:

1. **F-02's scope is extended** beyond the EqualPower fade to the three libm
   dependencies that reach the rendered samples — the fade's `std::sin`, the
   pan law's `std::cos`/`std::sin` and the clip gain's `std::pow`. ORP273's own
   acceptance test for F-02 ("the same constant hash across … at least two
   platforms") is unsatisfiable while any of them remains libm-dependent.
2. **F-06 keeps the dB-domain gain law** and lands a bit-exact fast path,
   because ORP273 F-06 explicitly excludes a DSP-contract change.
3. **F-20 keeps `include/orpheus/clip_source.h` unchanged** by using the
   sort/`lower_bound` variant of the record's remedy; the slot-array variant
   is deferred because it would change an installed public header, which
   ORP273 §6.2 does not account for.

The sprint changes three installed public headers: `include/orpheus/clip_dsp.h`
(F-06), `include/orpheus/json.hpp` (F-12) and `include/orpheus/session_graph.h`
(F-13, F-14). F-02 additionally changes the rendered sample values, so any
consumer artefact that pins a rendered-audio hash must be regenerated. No
consumer repository is modified by this sprint.

## 2. Sprint plan and wave order

The wave assignment below is ORP273 §5's. Each finding lands as its own
focused commit; the commit SHA is filled in as the waves land and the
authoritative per-finding record is §3.

| Wave | Finding | What lands | Dependency | Blast radius | Commit |
| --- | --- | --- | --- | --- | --- |
| 0 | F-07 | The budget validator reports what it measured and fails when a declared budget could not be measured. | None | None | _pending_ |
| 0 | F-17 | Timing assertions are guarded under sanitizers, via a shared `tests/support/sanitizer_guard.hpp`. | None | None | _pending_ |
| 0 / 4 | F-18 | The already-existing worst-case deadline harness becomes a registered CTest gate with environment-overridable observations, warm-ups, rates and a clamped p99 ratio; thresholds are calibrated after Wave 2. | None / Wave 2 | None | _pending_ |
| 0 | F-19 | The five stale `ARCHITECTURE.md` statements are corrected against the tree. | None | None | _pending_ |
| 1 | F-01 | The CoreAudio device callback publishes the route-monitor terminal state atomically and does not notify a waiter; the unreachable duplicate malformed-output block is removed. | None | None | _pending_ |
| 1 | F-02 | The render path's libm dependence is replaced by pinned elementary functions and the render-hash golden is pinned as a constant. | F-18 | Rendered sample values change; goldens regenerate | _pending_ |
| 1 | F-11 | One explicit SDK callback-frame ceiling for all CoreAudio routes, enforced at initialisation before buffers are allocated. | None | None | _pending_ |
| 2 | F-03 | The per-block full-lane memset is replaced by a voice-lifetime lane-zero invariant. | F-18 | None | _pending_ |
| 2 | F-04 | `GainSmoother::process()` is inlined; the smoother ramp state is hoisted out of the per-sample loop. | F-03 | None | _pending_ |
| 2 | F-09 | The redundant meter scratch clears are removed and the write paths made total. | F-05 | None | _pending_ |
| 2 | F-10 | The true-peak metering path is fused into a single per-sample pass. | None | None | _pending_ |
| 2 | F-08 | Headroom channel counts are accumulated once per block. | F-10 | None | _pending_ |
| 2 | F-05 | The redundant meter telemetry copies are removed and replaced by field-wise writes. | F-09, F-10 | None | _pending_ |
| 2 | F-06 | The compressor's constant makeup term is precomputed on the control thread. | None | None | _pending_ |
| 2 | F-13 | Session parsing uses a bulk per-track construction path. | None | Public C++ header (`include/orpheus/session_graph.h`) | _pending_ |
| 2 | F-20 | The worker snapshot uses a sorted lookup instead of a nested identity scan. | None | None | _pending_ |
| 3 | F-12 | Explicit document-byte and structural limits for session and JSON parsing. | None | Public C++ header (`include/orpheus/json.hpp`) | _pending_ |
| 3 | F-14 | A no-throw, non-allocating transaction rollback. | None | Public C++ header (`include/orpheus/session_graph.h`) | _pending_ |
| 3 | F-15 | The control-side source registry is extracted from `TransportController`. | F-03, F-04, F-05 | None | _pending_ |
| 3 | F-16 | Meter storage and publication are extracted from `RoutingMatrix`. | F-08, F-09, F-10 | None | _pending_ |
| 4 | F-18 (calibration) | The registered measurement's p99 ratio is calibrated against the Wave 2 render path. | Wave 2 | None | _pending_ |
| 4 | F-19 (verification) | `ARCHITECTURE.md` corrections verified against the tree. | None | None | _pending_ |

**Wave 0 is the prerequisite for judging Waves 1 to 3.** No release-
representative measurement of the render path exists in this repository
today, so a performance change landed before F-18 would be an unmeasurable
claim.

**Wave 1 is safe to land without a downstream rebuild**, with one caveat:
F-02 changes rendered bytes, so a consumer that pins a rendered-audio hash
must regenerate it.

**Wave 2 is safe to land without a downstream rebuild** except for F-13, which
adds a public method to `include/orpheus/session_graph.h`, and F-06, which adds
one private member to `include/orpheus/clip_dsp.h`. Every rendered-output change
must keep the existing goldens valid.

**Wave 3 requires a Clip Composer rebuild and a FreqFinder package
re-consumption** for F-12 and F-14, because both change an installed public
header. F-15 and F-16 are private-header decompositions: they require an SDK
rebuild, and a re-pin or re-consumption for consumers that build from source,
but no consumer source change.

**Wave 4 is deliberately last**: the thresholds are calibrated against the
Wave 2 render path, not against the path being changed.

### 2.1 Scope decisions taken at sprint start

These three decisions are fixed at the start of the sprint and are recorded
here so that they are not reopened mid-implementation. Their consequences are
detailed in §6.

1. **F-02 scope extension.** The finding as recorded names the EqualPower
   fade's `std::sin`. The sprint also replaces the pan law's `std::cos`/
   `std::sin` and the clip gain's `std::pow`, because those reach the
   rendered samples and ORP273's own acceptance test for F-02 requires the
   same constant hash across platforms. Leaving any of them libm-dependent
   would make that acceptance test unsatisfiable. The six libm callsites in
   `src/core/transport/transport_controller.cpp` (`:109`, `:116`, `:361`,
   `:3025`, `:3239`, `:3377`) are the only ones in that translation unit, and
   `src/core/transport/clip_dsp.cpp:16-18` holds the seventh.
2. **F-06 keeps the dB-domain gain law.** The finding's remedy is applied as a
   bit-exact fast path: the per-frame `std::pow` is skipped only in the
   non-reducing case, where its result is provably the constant makeup gain.
   The `std::log10` remains, because removing it would change the DSP
   contract, which ORP273 F-06 explicitly excludes.
3. **F-20 keeps `include/orpheus/clip_source.h` unchanged.** The sorted
   `lower_bound` variant of the remedy is used instead of the slot-array
   variant, because the slot-array variant would change an installed public
   header and ORP273 §6.2 does not account for that consequence.
