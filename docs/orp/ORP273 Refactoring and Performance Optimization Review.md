<!-- SPDX-License-Identifier: MIT -->

# ORP273 — Refactoring and Performance Optimization Review

**Document type:** Source-grounded refactoring and performance review record
**Date:** 2026-10-21
**Review target:** `0325c6400` (branch `main`)
**Scope:** The C++20 SDK core of this repository — `src/`, `include/`, `adapters/`,
`tools/`, `tests/`, `examples/`, and the build/measurement tooling that governs
them. Excluded by rule: `packages/shmui-juce/` (a governed import pinned at
revision `d420ec64dcb5fd16baa0b95d5048ee09bb95eee8` in
`packages/shmui-juce/shmui-juce-import.json`), `apps/`, the child-application
repositories, and the suite manifest.
**Status:** Complete review record; no SDK source or test fixes implemented

---

## 1. Executive assessment

- The SDK's realtime discipline is genuinely strong and the strict in-repository
  gate is real. `tools/realtime_audit.py --root . --fail-known-debt` reports
  `0 hard failures, 0 tracked debt findings`, and the render path performs no
  file reads. Every finding below is therefore a cost or structure defect inside
  a path that is *already* allocation-free and lock-free — not an audio-thread
  safety violation in the classic sense.
- Two findings are rated **Critical** because they are released-contract
  violations rather than raw CPU cost. **F-01**: the CoreAudio device callback
  reaches `std::atomic::notify_one()` on the route monitor, which
  `docs/REALTIME_AUDIT.md:29-32` forbids ("callbacks do not wait, allocate, or
  notify waiters") and `ARCHITECTURE.md:587` forbids ("System calls"). **F-02**:
  the EqualPower fade path calls `std::sin` per rendered sample, and the
  repository's own render-hash gate states that this is the reason the
  cross-platform golden hash is not pinned
  (`tests/transport/transport_render_hash_test.cpp:16-19`) — leaving the stated
  determinism principle (`ARCHITECTURE.md:42`) unmet for that curve, with the
  remediation named only in a *Proposed* sprint (ORP134 G4) and a test comment.
- The largest single avoidable cost in the render path is **memory traffic
  proportional to configured capacity rather than to active work**. F-03 clears
  all 256 voice-slot routing lanes every block when only the active voices need
  clearing; F-04 advances three out-of-line gain/pan smoothers per channel per
  sample, two of which are discarded under the transport's default
  `SourceChannelPolicy::Discrete`; F-05 constructs and copies a 17,936-byte
  meter telemetry struct on every block; F-08 rescans all 256 routing channels
  once per group per block; F-09 clears meter scratch lanes that are then fully
  overwritten; and F-10 runs a second full per-sample pass over every block in
  TruePeak mode.
- Two **transcendental** costs remain on the audio thread and are not covered by
  any prior record: the per-frame `std::sin` of F-02, and the per-frame
  `std::log10` + `std::pow` of the enabled clip compressor (F-06). The
  gain-conversion `std::pow` that ORP104 Issue #4 and ORP263 R-01 identified is
  **already mitigated** — it now runs on the control-side producer
  (`src/core/transport/transport_controller.cpp:3025`, `:3239`, `:361`), not in
  the audio command consumer — and is therefore not re-raised here.
- The **measurement** position is the weakest part of the estate and is the
  reason two findings are about tooling rather than code. F-07 records that
  `node scripts/validate-performance.js` prints "All performance budgets
  met!" and exits 0 while the only configured bundle was skipped and neither
  the command-latency nor the event-frequency budgets was measured at all.
  F-17 records that sanitizer CI jobs apply unsanitized timing thresholds to
  instrumented measurements. The single shipped performance tool
  (`tools/perf/perf_render_click.cpp`) measures an end-to-end offline render
  that includes file I/O, is not registered as a CTest test, and was run here
  on a **Debug + ASan + UBSan** tree, so it is not release-representative. No
  release-representative callback-budget number exists in this repository.
- Several opportunities are **already covered by prior records** and are
  dispositioned in §3 rather than re-raised: ORP104's gain-`pow` and adapter-LOC
  items, ORP121's true-peak/headroom/pan-law features, ORP266's 16 rows
  (including row 7's unenforced budgets, rows 12/13/17's static-audit limits,
  and rows 4/5's stale architecture text), ORP263 F-01…F-09, and
  `docs/REALTIME_AUDIT.md` §Known Architecture Debt.
- Two findings concern **structure rather than speed** and are argued from
  concrete ownership evidence, not from the presence of matching declarations
  against a public interface header: F-15 (`TransportController` co-locates the
  control-side source registry, the bounded MPSC command pool, the audio-owned
  voice array, the render loop, and telemetry publication) and F-16
  (`RoutingMatrix` co-locates configuration publication, render state, and meter
  state in one class and one 1,735-line translation unit).
- F-19 records that the branch which carried ORP266's Medium and Low
  remediations (`fix/orp266-audit-remediation`) **is not an ancestor of
  `main`**, and that the specific architecture-document corrections from ORP266
  rows 4 and 5 are therefore still absent from the tree.
- This review does **not** claim: any millisecond saving; any improvement in
  callback deadline headroom; any release qualification; any promotion of
  WASAPI, ASIO, or a Linux backend; any change to the C ABI 1.0 tables, the
  public headers under `include/orpheus/`, the `include/treefall/` forwarding
  headers, or the installed target set. Every recommendation in §4 is
  expressed so that it can be implemented without violating the realtime
  contract in `AGENTS.md` §Realtime rules.

---

## 2. Evidence basis

### 2.1 Review target and worktree

| Item | Observed value |
| --- | --- |
| Repository | `/Users/chrislyons/dev/orpheus-sdk` (product identity Treefall, compatibility identity Orpheus) |
| Remote | `origin [EMAIL]:boot-industries/treefall-sdk.git` (fetch and push identical) |
| Review revision | `0325c6400a37be9926b4b6dae532885417cabe9b` |
| Worktree before review | clean (`git status --porcelain` empty) |
| Worktree after review | documentation-only change on branch `docs/orp273-refactoring-performance-review`; no `src/`, `include/`, `tests/`, `adapters/`, `packages/`, `cmake/`, `tools/`, or `CMakeLists.txt` file modified |
| Branch created | `docs/orp273-refactoring-performance-review` from `main` at `0325c6400`; not pushed |
| Last commit touching `src/` or `include/` | `9184892e0` (2026-10-19) — the reviewed sources are identical to the `main` sources |
| SDK version | 0.9.1; stable C ABI 1.0 (`ORPHEUS_ABI_MAJOR 1`, `ORPHEUS_ABI_MINOR 0`) |
| Public header inventory | 39 headers under `include/orpheus/` (36 at the root plus `dsp/oscillator.hpp`, `dsp/scrub_resampler.h`, `adm/entity_graph.h`) and 6 under `include/treefall/` |

### 2.2 Host and build configuration

| Item | Observed value |
| --- | --- |
| Workstation | `cloudkicker` — macOS Darwin 26.5.1, arm64 (local checkout; no remote operation was performed) |
| Compiler | AppleClang 21.0.0 (`/usr/bin/c++`) |
| CMake | 4.1.0 |
| Python | 3.14.6 |
| Node | v20.19.4 |
| Build tree used | `build/` — `CMAKE_BUILD_TYPE=Debug`, `ORPHEUS_ENABLE_REALTIME=ON`, `ORP_WITH_TESTS=ON`, `ORP_ENABLE_ASAN=ON`, `ORP_ENABLE_UBSAN=ON`, generator `Unix Makefiles` |
| **Measurement representativeness** | **Not release-representative.** Every number quoted in this record was produced on a Debug build with AddressSanitizer and UndefinedBehaviorSanitizer enabled. No Release baseline was configured or built, so no release-performance claim is made anywhere in this document. |
| CTest inventory | `ctest --test-dir build -N` → `Total Tests: 85` |
| Release tree state | `build-release/` contains no built binaries and is configured with `CMAKE_BUILD_TYPE=Release` **and** `ORP_ENABLE_ASAN=ON`/`ORP_ENABLE_UBSAN=ON`; it was not used as a measurement baseline. |

### 2.3 Commands run and their verbatim output

All commands were run from the repository root at revision `0325c6400`.

**Baseline**

```text
$ git status --short --branch
## main...origin/main
$ git rev-parse HEAD
0325c6400a37be9926b4b6dae532885417cabe9b
$ git remote -v
origin	[EMAIL]:boot-industries/treefall-sdk.git (fetch)
origin	[EMAIL]:boot-industries/treefall-sdk.git (push)
$ git log --oneline -5
0325c6400 Merge pull request #276 from boot-industries/suite/orp-suite-20261020-001-convergence-record
cd3e1678b docs(suite): record the convergence result and the Clip Composer pin
e885ddcf7 Merge pull request #275 from boot-industries/suite/orp-suite-20261020-001-manifest-convergence
772a4a735 fix(suite): reconcile the suite manifest with the governed ShmUI import
7009e9454 Merge pull request #274 from boot-industries/docs/orp271-closeout-merge-record
```

**Repository gates** — all passed, all exit 0

```text
$ python3 tools/realtime_audit.py --root . --fail-known-debt
Realtime audit passed: 0 hard failures, 0 tracked debt findings.
exit=0

$ python3 tools/docs_path_audit.py --root .
Docs-path audit passed: 7 documents checked.
exit=0

$ python3 tools/version_contract.py --check
SDK version contract is consistent: 0.9.1
exit=0

$ python3 tools/shmui_juce_manifest.py --check
ShmUI-JUCE manifest is consistent: 77 files, sha256 4d2d4c5ba354b52fbeb0c6c59b44bb15bac90e4b65e3aa5c3bb096881a9645d7
exit=0

$ python3 tools/suite.py validate --json
{
  "errors": [],
  "manifest": "/Users/chrislyons/dev/orpheus-sdk/suite/orpheus-suite.json",
  "schema": "/Users/chrislyons/dev/orpheus-sdk/suite/schema/orpheus-suite.schema.json",
  "valid": true
}
exit=0
```

**Adjacent-repo strict gate** (expected to fail on the adjacent repository only)

```text
$ python3 tools/realtime_audit.py --root . --fail-known-debt --include-adjacent
DEBT: Clip Composer AudioEngine callback: /Users/chrislyons/dev/clip-composer/Source/Audio/AudioEngine.cpp:3203: juce::AudioBuffer<float> tempBuffer (Clip Composer wraps/analyzes output in callback)
exit=1
```

This is the single tracked adjacent finding. It is Clip Composer's own callback,
is outside this repository, and is not a finding of this review.

**Targeted behavioural gates** (the full 85-test suite was deliberately not run)

```text
$ ctest --test-dir build --output-on-failure -R '^realtime_static_audit$|^docs_path_audit$|^multi_clip_stress_test$|^transport_render_hash_test$|^performance_monitor_test$'
    Start   5: realtime_static_audit
1/5 Test  #5: realtime_static_audit ............   Passed    0.06 sec
    Start   7: docs_path_audit
2/5 Test  #7: docs_path_audit ..................   Passed    0.65 sec
    Start  27: performance_monitor_test
3/5 Test #27: performance_monitor_test .........   Passed    0.26 sec
    Start  37: multi_clip_stress_test
4/5 Test #37: multi_clip_stress_test ...........   Passed   51.53 sec
    Start  56: transport_render_hash_test
5/5 Test #56: transport_render_hash_test .......   Passed    2.40 sec

100% tests passed, 0 tests failed out of 5
Total Test time (real) =  54.91 sec

$ ctest --test-dir build --output-on-failure -R '^realtime_harness_test$'
    Start  54: realtime_harness_test
1/1 Test #54: realtime_harness_test ............   Passed   87.73 sec

100% tests passed, 0 tests failed out of 1
Total Test time (real) =  87.74 sec
```

**Measurement tools**

```text
$ ./build/tools/perf/orpheus_perf_render_click
Orpheus render_click performance
sample_rate=44100Hz took 10.185 ms
sample_rate=48000Hz took 10.788 ms
sample_rate=96000Hz took 24.190 ms
exit=0

$ ./build/adapters/minhost/orpheus_minhost render-click --session tools/fixtures/solo_click.json --out /tmp/orp273_probe.wav --range 0:8
ABI negotiation
  session    v1.0 ✅
  clipgrid   v1.0 ✅
  render     v1.0 ✅
Rendered click track to "/tmp/orp273_probe.wav"
Suggested render path: out/solo_click_click_44p1k_16b.wav
exit=0

$ scripts/check-determinism.sh
✓ Using checksum command: sha256sum
...
✓ Determinism check completed

SHA-256: 1a602fe4e879ec011bdf2b9ce25e0f2ad7f577d62c2c8ee937dc6bc38319d905
File:    /tmp/orpheus_render_test.wav
exit=0
```

**Budget validator** — note carefully what this does and does not measure

```text
$ node scripts/validate-performance.js
✓ Loaded budgets from budgets.json

📦 Validating Bundle Sizes...

  ⚠️  @orpheus/engine-wasm: dist/ not found (skipping)

⏱️  Command Latency Budgets:

  LoadSession:
    P95 budget: 200 ms
    P99 budget: 500 ms
    ℹ️  Actual latency measurement requires runtime profiling
  RenderClick:
    P95 budget: 1000 ms
    P99 budget: 2000 ms
    ℹ️  Actual latency measurement requires runtime profiling

📡 Event Frequency Budgets:

  TransportTick:
    Max frequency: 30 Hz
    ✓ Validated by frequency-validator.test.ts
  RenderProgress:
    Max frequency: 10 Hz
    ✓ Validated by frequency-validator.test.ts

================================================================================
PERFORMANCE BUDGET VALIDATION SUMMARY
================================================================================

✅ All performance budgets met!

exit=0
```

The exit code is **not** performance evidence. The only configured bundle was
skipped, and neither the command-latency nor the event-frequency budgets was
measured by this script. See F-07.

**Type-size probe** (throwaway program, compiled against the public headers only,
run outside the repository tree; used for the byte figures in F-05)

```text
$ c++ -std=c++20 -I include /tmp/orp273_sizes.cpp -o /tmp/orp273_sizes && /tmp/orp273_sizes
AudioMeter=16
GroupOutputMeterFrame=524
GroupOutputMeterSnapshot=16792
RoutingMeterTelemetry=17936
RealtimeTelemetrySnapshot=18808
kRoutingControlMaxGroups=32 kRoutingMaxOutputs=32 kRealtimeTelemetryMaxGroups=16
```

### 2.4 Reference topology used for the multiplicity bounds

Every per-block bound in §4 is expressed against the SDK's own documented
maximums, not against a hypothetical machine:

| Quantity | Value | Anchor |
| --- | --- | --- |
| Active voices | 32 | `src/core/transport/transport_controller.h:598` (`MAX_ACTIVE_CLIPS`) |
| Source channels per voice | 8 | `include/orpheus/transport_controller.h:238` (`maxSourceChannels`) |
| Routing lanes | 32 × 8 = 256 | `src/core/transport/transport_controller.cpp:183-185` |
| Logical groups | 32 | `include/orpheus/routing_matrix.h:50` (`kRoutingControlMaxGroups`) |
| Routing outputs | 32 | `include/orpheus/routing_matrix.h:52` (`kRoutingMaxOutputs`) |
| Routing slice / default block | 2,048 frames | `include/orpheus/routing_matrix.h:47` (`kRoutingSliceFrames`), `include/orpheus/transport_controller.h:235` (`maxBlockFrames`) |
| Command ingress pool | 255 | `src/core/transport/transport_controller.h:575` (`MAX_COMMANDS`) |
| Blocks per second at 48 kHz / 2,048 frames | 23.4375 | derived from the two anchors above |

Per-second figures in §4 are these block counts multiplied by the per-block
multiplicity. They are **counts of operations, not seconds of CPU**: no
per-operation latency was measured in this session, so any statement about the
time those operations consume is labelled `[INFERENCE]`.

---

## 3. Reconciliation with prior records

Every candidate raised during this review was searched against the prior
records before it became a finding. The table below states where each prior
record already covers the material, and what this review adds on top.

| Prior record | Already covered (disposition) | What this review adds |
| --- | --- | --- |
| **ORP104** Codebase Optimization and Safety Audit (2025-11-09, status *In Progress – Phases 0-2*) | Issue #4 (`std::pow()` in hot loop) — **already mitigated**: the gain conversions now run on the control-side producer at `src/core/transport/transport_controller.cpp:3025`, `:3239`, `:361`, and the audio command consumer stores a precomputed linear value. Not re-raised. Issue #7 (minhost 1,539 LOC vs a 300-LOC guideline) — **already covered**; also carried by ORP135 *Minhost Adapter Decomposition* (*approved for implementation, not yet started*). Not re-raised. Issue #8 (duplicated `SessionGuard` RAII in minhost and REAPER) — **already covered**; not re-raised. Issues #1/#2/#3/#5/#6/#9/#10 — **already covered**; their current code no longer exhibits the described defect. | The per-frame `std::sin` of F-02 and the per-frame `std::log10`+`std::pow` of F-06 are *not* the gain-`pow` path ORP104 Issue #4 recorded; ORP104's coverage map explicitly excludes pan-law and metering cost. |
| **ORP116** Bloat Remediation Sprint (status *Draft*) | Repository bloat, archived TypeScript packages, dependency/override cleanup, fonts/icons, backup files, lockfile size — **all out of scope for this review and not re-raised**. | Nothing. |
| **ORP121** Audio Backend Refactoring Master Plan (archived) | C-04 (pan law wiring) — delivered. Q-04 (true-peak metering) — delivered. Q-05 (headroom management) — delivered, *but* the delivered implementation performs a `groups × channels` rescan per block; that cost is F-08. C-03 (callback mutex) — **already mitigated** by the bounded MPSC ingress. Q-01…Q-13 — code-style and feature items, **out of scope**. | The *cost* of the delivered Q-05 headroom feature (F-08) and the per-sample smoother advancement that Q-04's true-peak work sits beside (F-04) are new. |
| **ORP138** Modernization Audit (Nov 2025) | §3.3 (performance benchmarking infrastructure: "no benchmark framework"; names the render-click tool) — **already covered**; the absence of a CTest registration for `orpheus_perf_render_click` is a consequence of the same gap. §2.3 (`noexcept` underuse) — general. | F-17 identifies a *specific* instance where an unsanitized timing threshold is applied to an instrumented measurement; the generic §2.3 advice does not reach it. |
| **ORP263** F-01 | Future-schema recovery substituting an older backup — **contained by ORP264**; `src/core/session/json_io.cpp:65-68` now raises a distinct `UnsupportedSessionSchema` and the recovery path does not consult the backup for it. Not re-raised. | — |
| **ORP263** F-02 | Transaction rollback reusing issued TrackId/ClipId values — **contained by ORP264**; the snapshot now carries allocator watermarks and restore preserves the maximum. Not re-raised. | F-14 is a *different* property of the same mechanism: the rollback path is `noexcept` yet allocates. F-19 is the `uint64_t` exhaustion edge, also distinct. |
| **ORP263** F-03 | Unchecked C ABI session handles — **contained by ORP264**; a live-session registry now exists. Not re-raised. | — |
| **ORP263** F-04 | CoreAudio malformed output touched before validation — **contained by ORP264**; `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:771-796` now validates the full shape before any write. Not re-raised. | F-01 concerns a *different* statement in the same callback (the waiter notification on the rejection path). |
| **ORP263** F-05 | Routing malformed-output validation mutating meter state — **contained by ORP264**; `src/core/routing/routing_matrix.cpp:923-941` now validates before any mutation. Not re-raised. | — |
| **ORP263** F-06 / F-07 / F-09 | Performance-monitor latency semantics, metric-coherence/reset contracts, callback-p99 semantics — **still open** and correctly owned by that record; this review does not duplicate them. | F-05 touches the *routing-meter telemetry* path, which is a different publication channel from `IPerformanceMonitor`. |
| **ORP263** F-08 | Loop-anchor commit invoked from the audio command consumer — **still open**; owned by that record. | — |
| **ORP263** R-01 | Worst-case command and maximum-topology callback work: 255 commands, 32-voice scans, "Rendering clears all configured routing lanes", and the recommendation to precompute gain conversion. The gain-precomputation half is **done**; the maximum-topology timing half is **still open**. | F-03 is an *extension*: R-01 names the lane clear but not the fact that 248 of 256 lanes are cleared for voices that are not active, nor the byte bound. F-04, F-05, F-06, F-08, F-09 and F-10 are all in the same render path R-01 measured but were not individually identified. |
| **ORP263** R-02 | Single media worker coupling all streaming sources; blocked-reader cancellation — **still open**. | F-20 is a distinct property of the same worker: the per-round snapshot rebuild performs an `O(S²)` identity scan while holding the worker mutex. |
| **ORP263** R-03 … R-07 | Driver-manager rollback/lock scope, CoreAudio converter priming, WASAPI verification, early-EOF media normalisation, pointer/latency semantics — **still open or gated**; not re-raised. | — |
| **ORP263** design-debt items 1-10 | Demo-host session duplication, minhost/REAPER private-header dependency, DSO-local ABI state, dead transport command variants, worker attach duplicates, broad package negative checks, ASIO source-only status, telemetry concurrency coverage, extreme sample-domain gaps, CoreAudio hardware coverage — **already covered**; not re-raised. | — |
| **ORP266** row 1 (Critical) | Temporal coherence not plumbed end to end — **deferred by ORP268 Requirement A**; not re-raised. | — |
| **ORP266** row 2 (High) | C ABI tables have no runtime size handshake — **deferred by ORP268 Requirement B**; not re-raised. | — |
| **ORP266** row 3 | Support matrix lists six targets while the manifest carries seven — **remediated on `main`**: `docs/SUPPORT_MATRIX.md:64-70` now lists all seven. | — |
| **ORP266** row 4 | `ARCHITECTURE.md` names non-existent session-JSON and driver paths — **not remediated on `main`**; see F-19. | F-19 records the non-remediation and its cause. |
| **ORP266** row 5 | `ARCHITECTURE.md` still claims pre-loaded memory and 48 kHz-only — **not remediated on `main`**; see F-19. | F-19 records the non-remediation and its cause. |
| **ORP266** row 6 | `ARCHITECTURE.md` claims atomic graph updates — **not remediated on `main`** (`ARCHITECTURE.md:225`); see F-19. | F-19 records the non-remediation and its cause. |
| **ORP266** row 7 | `budgets.json` command-latency and event-frequency budgets are enforced by nothing, and the validator misreports its own coverage — **still open**. | F-07 adds the *false-green* path that ORP266 row 7 does not name: the one configured bundle is skipped and the summary still reports success. |
| **ORP266** row 8 | Duplicated offline-render golden hash and duplicated session fixtures — **still open**; not re-raised. | — |
| **ORP266** row 9 | `README.md` omits the required specialized gates and contradicts itself on rebuilds — **still open**; not re-raised. | — |
| **ORP266** row 10 | Root-level agent prompt leaks another machine's absolute path — **still open**; not re-raised. | — |
| **ORP266** row 11 | `ROADMAP.md` still lists routing as a planned milestone — **still open**; not re-raised. | — |
| **ORP266** row 12 | The static realtime gate scans a hard-coded list of 24 target functions and omits functions reachable from `processAudio` — **still open**; the bounded-lexical limitation is also stated honestly at `docs/REALTIME_AUDIT.md:41-43`. | F-01 notes that the omitted-path weakness is a *known* limitation, not a new claim. |
| **ORP266** row 13 | The scanner skips any line starting with `*` — **still open**; not re-raised. | — |
| **ORP266** row 14 | ASIO opt-in target references an absent `asio/` directory — **still open**; not re-raised. | — |
| **ORP266** row 15 | Public header count drift — **still open**; the current count is 39 and no repository document states one. | §2.1 records the observed count of 39 without a finding, because no document asserts a different number. |
| **ORP266** row 16 | `ARCHITECTURE.md` `ActiveClip` pseudocode shows a stale `std::atomic<uint64_t> currentFrame` — **not remediated on `main`** (`ARCHITECTURE.md:368`); see F-19. | F-19 records the non-remediation and its cause. |
| **ORP266** row 17 | The enforcement tool is described as an AST parser but is lexical — **still open**; not re-raised. | — |
| **ORP267** DeepWiki Implementation Plan | The execution plan for ORP266, including the disposition of the 16 rows and the split into ORP268 — **still open** for the deferred Critical/High rows. | F-19's recommended action is to land the already-scoped Medium/Low remediation rather than re-plan it. |
| **ORP268** Deferred Audit Remediation Requirements (status *Open — not remediated*) | Requirement A (clock-domain and discontinuity plumbing) and Requirement B (C ABI table size handshake) — **both still open and correctly gated**; not re-raised. | — |
| **ORP134** G4 — Deterministic render/bounce hash harness (record status *Proposed*) | The render-hash harness exists (`tests/transport/transport_render_hash_test.cpp`), and that file names ORP134 G4 as the scope for pinning the fade math. | F-02 is an *extension*: it states the performance cost, the determinism consequence, and that the remediation sits in a *Proposed* record rather than in the deferred-work register. |
| **ORP135** Minhost Adapter Decomposition | The minhost module split is already planned and *approved for implementation, not yet started*. | Not re-raised. F-15 and F-16 concern the SDK's own private headers, which no prior record decomposes. |
| **`docs/REALTIME_AUDIT.md` §Known Architecture Debt** (`docs/REALTIME_AUDIT.md:376-384`) | Routing carries `SourceChannelPolicy`/`DownmixPolicy` while transport clip rendering still uses stereo pair buffers; Clip Composer and FourTrack still do app-level work in their own callbacks — **already recorded**; not re-raised. | F-04's Discrete-policy waste is a *different* property: the routing matrix does implement the policies, but computes two pan gains per sample that the Discrete branch never uses. |

---

## 4. Findings

**Severity rubric used in this record.** **Critical** = realtime-safety defect,
ABI-break risk, or a released-contract violation. **High** = measurable cost on
a supported path, or a structural defect that will produce one. **Medium** =
internal incoherence, duplication, or documentation/measurement debt with a
bounded cost. **Low** = cosmetic, latent, or externally harmless.

**Class legend.** *Refactor* = a code-structure change. *Performance* = a
measurable execution-cost change. *Measurement hygiene* = a change to how the
repository proves or fails to prove its own claims. *Both* = a change that
carries both a structure and a cost consequence.

**Evidence convention.** Every "Observation" cites at least one `path:line`
anchor read in this session. Every "Cost mechanism" states a multiplicity
bound. Multiplicities are operation counts; any statement about the time those
operations consume is labelled `[INFERENCE]`.

---

### F-01 — CoreAudio device callback notifies a waiter from the render callback

| Field | Value |
| --- | --- |
| Class | Both |
| Severity | Critical |
| Novelty | New |
| Status | Open |
| Anchors | `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:787`, `src/platform/audio_drivers/coreaudio/coreaudio_route_monitor.cpp:117`, `src/platform/audio_drivers/coreaudio/coreaudio_route_monitor.cpp:286`, `docs/REALTIME_AUDIT.md:29`, `ARCHITECTURE.md:587` |
| Observation | The CoreAudio output render callback validates the output shape and, on a malformed shape, calls `driver->route_monitor_->closeAdmission()` at `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:787`. `closeAdmission()` publishes the terminal state and then unconditionally calls `signalStateChange()` at `src/platform/audio_drivers/coreaudio/coreaudio_route_monitor.cpp:122`, which performs `generation_.notify_one()` at `src/platform/audio_drivers/coreaudio/coreaudio_route_monitor.cpp:288`. That notifies the route-monitor worker thread, which is parked in `waitForChange()` at `src/platform/audio_drivers/coreaudio/coreaudio_route_monitor.cpp:126-134`. The same `closeAdmission()` call appears a second time at `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:799`, in a block that is unreachable because the first malformed-output branch at `:787-792` already returns. |
| Cost mechanism | One futex wake per malformed callback. `closeAdmission()` signals even when the state is already `Terminal`, because the `compare_exchange_strong` at `src/platform/audio_drivers/coreaudio/coreaudio_route_monitor.cpp:119` is allowed to fail without changing the signalling path, so a device that keeps presenting a malformed shape produces one `notify_one()` **per callback**, indefinitely. A futex wake is a system call; `ARCHITECTURE.md:587` lists system calls under the audio thread's forbidden operations and `docs/REALTIME_AUDIT.md:29-32` states that callbacks "do not wait, allocate, or notify waiters". `[INFERENCE]` The absolute cost of one futex wake is a few microseconds of scheduler work and is not itself a measured dropout here. |
| Recommended change | Split the route-monitor terminal transition into an atomic-only publication callable from the device callback and a signalling step callable only from control/worker context. The callback should store the terminal state and return; the route-monitor worker should observe the terminal state on its next poll (it already re-checks `state_` at `src/platform/audio_drivers/coreaudio/coreaudio_route_monitor.cpp:127` and `:224`). Delete the unreachable second malformed-output block at `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:799-803`. Keep the existing behaviour that the callback stops processing and the worker performs route retirement. |
| Blast radius | None — `src/platform/audio_drivers/coreaudio/` is private; no public C++ header, C ABI 1.0 table, C++ virtual slot, or installed-target change. |
| Downstream impact | Clip Composer: none required (no rebuild, no re-pin). FreqFinder: none required. The change is confined to the CoreAudio backend that the SDK ships, and both consumers obtain it through the SDK package or submodule. |
| Effort | M — the atomic publication and the worker-side observation are already separated in `CoreAudioRouteMonitor`; the work is to add the control-side signalling path and remove one unreachable block. |
| Acceptance test | A CoreAudio test that presents a malformed `AudioBufferList` repeatedly and asserts that the route monitor's terminal state is published while `signalStateChange()` is **not** reached from the callback thread (observable through a test seam on the monitor's generation counter). The existing malformed-output no-touch assertions must continue to pass unchanged. |

---

### F-02 — EqualPower fade evaluates `std::sin` per rendered sample, and the render-hash golden is unpinned because of it

| Field | Value |
| --- | --- |
| Class | Both |
| Severity | Critical |
| Novelty | Extension of ORP134 G4 |
| Status | Open |
| Anchors | `src/core/transport/transport_controller.cpp:3371`, `src/core/transport/transport_controller.cpp:838`, `src/core/transport/transport_controller.cpp:860`, `src/core/transport/transport_controller.cpp:871`, `tests/transport/transport_render_hash_test.cpp:16`, `ARCHITECTURE.md:42` |
| Observation | `calculateFadeGain()` at `src/core/transport/transport_controller.cpp:3371` implements the EqualPower curve as `std::sin(normalizedPosition * M_PI_2)` at `src/core/transport/transport_controller.cpp:3377`; the Linear and Exponential branches are pure arithmetic. The function is called once per rendered frame per fading voice at `src/core/transport/transport_controller.cpp:838` (operator-stop fade), `src/core/transport/transport_controller.cpp:860` (fade-in) and `src/core/transport/transport_controller.cpp:871` (fade-out). The repository's own render-hash gate documents the consequence at `tests/transport/transport_render_hash_test.cpp:16-19`: hashes "are NOT hard-coded as cross-platform golden constants yet, because the EqualPower fade path calls std::sin per sample and libm results differ across platforms", and names ORP134 G4 as the scope for pinning that math. ORP134's record status is *Proposed*, and the item does not appear in `AGENTS.md` §Deferred work. |
| Cost mechanism | Up to 3 curve evaluations per rendered frame per voice, of which only the EqualPower branch calls a transcendental. At 32 voices and 2,048 frames the ceiling is 196,608 `std::sin` calls per block, i.e. 4,608,000 calls per second at 48 kHz with 2,048-frame blocks. `[INFERENCE]` Per-call cost for a `float` `sinf` is on the order of tens of nanoseconds, so the ceiling is on the order of a percent of one core; the *dominant* consequence is not CPU but that the render output is not bit-identical across platforms, which `ARCHITECTURE.md:42` states as a non-negotiable design principle. |
| Recommended change | Replace the EqualPower branch with a platform-independent formulation that is pinned rather than libm-dependent, exactly as the render-hash gate's own comment prescribes ("a polynomial or table, `std::bit_cast` discipline"). A control-thread-initialised, fixed-capacity table of the curve plus bounded interpolation removes both the per-frame `std::sin` and the cross-platform divergence. The table must be prepared before playback, must not be lazily initialised on the audio thread, and must keep the render path allocation-free and bounded. The Linear and Exponential branches stay as they are. |
| Blast radius | None structurally — `calculateFadeGain` is private and `FadeCurve` is already a public enum. The **rendered sample values change**, so any golden that pins rendered bytes must be regenerated, and downstream consumers that compare rendered output byte-for-byte against a stored reference will observe different bytes. |
| Downstream impact | Clip Composer: no rebuild or re-pin for the code change, but any Clip Composer artefact that pins a rendered-audio hash must be regenerated. FreqFinder: no rebuild or re-pin; FreqFinder reads samples rather than comparing rendered hashes. |
| Effort | L — the table, its interpolation discipline, the cross-platform tolerance statement, and the golden regeneration all have to be designed together, and the change deliberately trades a bit-exact libm result for a pinned one. |
| Acceptance test | The render-hash gate extends its current block-size-invariance assertion to assert the **same constant hash** across block sizes and across at least two platforms, and the ORP134 G4 golden constant is added. Until that constant exists, the current gate's explicit comment is the only evidence that the gap is known. |

---

### F-03 — Every audio block clears all 256 voice-slot routing lanes, including lanes of voices that are not playing

| Field | Value |
| --- | --- |
| Class | Performance |
| Severity | High |
| Novelty | Extension of ORP263 R-01 |
| Status | Open |
| Anchors | `src/core/transport/transport_controller.cpp:183`, `src/core/transport/transport_controller.cpp:235`, `src/core/transport/transport_controller.cpp:653`, `src/core/transport/transport_controller.cpp:888`, `include/orpheus/transport_controller.h:236` |
| Observation | The transport sizes its routing lane pool at `maxActiveVoices × maxSourceChannels` (`src/core/transport/transport_controller.cpp:183-185`), which is 32 × 8 = 256 lanes at the documented defaults (`include/orpheus/transport_controller.h:236-238`). All 256 lanes are allocated at `maxBlockFrames` floats each (`:235-238`). At the top of every `processAudio` call the transport clears **every** lane with `std::memset` at `:653-655`, and the render loop then writes only the lanes belonging to currently active voices (`:888-921`, using `laneBase = voiceIndex * maxSourceChannels`). |
| Cost mechanism | 256 lanes × `numFrames` × 4 bytes cleared per block. At 2,048 frames that is 2,097,152 bytes (2 MiB) per block, or 49,152,000 bytes per second at 48 kHz with 2,048-frame blocks. The write-back is proportional to *configured capacity*, not to active work: a one-voice session writes 8 lanes and clears 256, so at least 96.9 % of the cleared bytes are immediately overwritten with nothing and never read. `[INFERENCE]` The clearing also evicts the routing matrix's group and output accumulators from L1/L2 immediately before the routing pass reads them. |
| Recommended change | Maintain the invariant "a voice's lanes are zero whenever that voice is not rendering" at the point where it is cheap: clear a voice's lane range when the voice is admitted and when it is retired, and write an explicit zero on the frame-skip path at `src/core/transport/transport_controller.cpp:844-846` where a read miss currently `continue`s without touching the destination lanes. With that invariant in place the per-block full-pool `memset` at `:653-655` can be deleted. The change must be validated against `transport_render_hash_test` because the skip path currently relies on the memset for its zero. |
| Blast radius | None — private implementation. |
| Downstream impact | Clip Composer: no rebuild, no re-pin; it consumes the SDK through `Orpheus::core` and is unaffected by an internal lane-clearing strategy. FreqFinder: no rebuild, no re-pin; it does not consume the transport render path. |
| Effort | M — the invariant has to be established at three sites (admission, retirement, frame-skip) and the render-hash golden must be re-verified rather than regenerated. |
| Acceptance test | The existing `transport_render_hash_test` passes with an unchanged hash, and a new test seeds a sentinel value into a lane belonging to an inactive voice and asserts that lane reads as zero after a render block, proving the invariant that replaces the blanket memset. |

---

### F-04 — The routing inner loop advances three out-of-line gain/pan smoothers per channel per sample, two of which are discarded under the transport's default policy

| Field | Value |
| --- | --- |
| Class | Performance |
| Severity | High |
| Novelty | New |
| Status | Open |
| Anchors | `src/core/routing/routing_matrix.cpp:1101`, `src/core/routing/routing_matrix.cpp:1110`, `src/core/routing/gain_smoother.cpp:58`, `src/core/routing/gain_smoother.h:71`, `include/orpheus/transport_controller.h:239` |
| Observation | The per-frame channel loop calls `channel.gain_smoother->process()`, `channel.pan_left->process()` and `channel.pan_right->process()` unconditionally at `src/core/routing/routing_matrix.cpp:1101-1103`, before the policy switch. `GainSmoother::process()` is defined out of line in `src/core/routing/gain_smoother.cpp:58` and is therefore not inlinable into the routing translation unit without link-time optimisation; each call performs two atomic acquire loads (`m_has_pending`, `m_pending_target`) before returning the current value. The `SourceChannelPolicy::Discrete` branch at `src/core/routing/routing_matrix.cpp:1110-1117` uses neither `pan_left` nor `pan_right` — the transport's default policy is `Discrete` (`include/orpheus/transport_controller.h:239`). The two pan gains are advanced and discarded on every sample of every channel. |
| Cost mechanism | 3 out-of-line calls per channel per sample. At 256 channels and 2,048 frames that is 1,572,864 calls per block, or 36,864,000 calls per second at 48 kHz with 2,048-frame blocks. Under `Discrete`, 2 of the 3 are unused: 1,048,576 wasted calls per block, 24,576,000 per second. Each call carries two atomic acquire loads. `[INFERENCE]` Each call is a non-inlined call plus two relaxed-order-consistent atomic loads, so the wasted pair is on the order of a few nanoseconds per sample; at the ceiling that is a meaningful fraction of a core, and the cost is paid even for channels whose pan targets never change. |
| Recommended change | Two changes, both allocation-free and bounded. First, make `GainSmoother::process()` an inline function in `src/core/routing/gain_smoother.h` so the call can be inlined at the call site. Second, hoist the smoother state out of the per-sample loop: when `isRamping()` is false the smoother's return value is constant for the whole block, so read it once per block and reuse the value; and advance the pan smoothers only in the policy branches that consume them, so `Discrete` does not advance them at all. Preserve the existing per-sample ramp rate so that the render output is unchanged when a ramp is in progress. |
| Blast radius | None — `src/core/routing/gain_smoother.h` and `.cpp` are private, and `GainSmoother` is not exposed by any public header. |
| Downstream impact | Clip Composer: no rebuild, no re-pin. FreqFinder: no rebuild, no re-pin. Neither consumer includes a routing-matrix private header; both use `include/orpheus/routing_matrix.h` and `include/orpheus/performance_monitor.h`. |
| Effort | M — the inlining is trivial; the per-block hoist must preserve the exact ramp semantics that the render-hash gate depends on. |
| Acceptance test | The render-hash golden is unchanged, and a new test asserts that a `SourceChannelPolicy::Discrete` render leaves the pan smoother targets untouched while a `StereoPairs` render advances them, proving the unused-work removal without changing output. |

---

### F-05 — The audio thread constructs and copies a 17,936-byte meter telemetry struct on every block

| Field | Value |
| --- | --- |
| Class | Performance |
| Severity | High |
| Novelty | New |
| Status | Open |
| Anchors | `src/core/transport/transport_controller.cpp:962`, `src/core/transport/transport_controller.cpp:1126`, `src/core/transport/transport_controller.cpp:1158`, `src/core/transport/transport_controller.cpp:1179`, `include/orpheus/realtime_telemetry.h:52`, `include/orpheus/realtime_telemetry.h:80` |
| Observation | `processAudio` calls `collectRoutingMeters()` once per block at `src/core/transport/transport_controller.cpp:962`. That function declares `RoutingMeterTelemetry current{};` at `src/core/transport/transport_controller.cpp:1126`, which value-initialises the whole struct, then assigns `current.group_output_meters = m_groupOutputMeterScratch;` at `src/core/transport/transport_controller.cpp:1158`, then assigns `m_routingMeterAccumulator = current;` at `src/core/transport/transport_controller.cpp:1179`. `RoutingMeterTelemetry` is 17,936 bytes, of which `GroupOutputMeterSnapshot` is 16,792 bytes; that snapshot is a fixed `std::array<GroupOutputMeterFrame, 32>` and each frame is a fixed `std::array<AudioMeter, 32>`. The struct is also a by-value member of `TransportController` at `src/core/transport/transport_controller.h:818`. |
| Cost mechanism | Three per-block memory operations on the same ~18 KB object: a zero-initialisation of 17,936 bytes, a 16,792-byte assignment, and a 17,936-byte assignment — approximately 52,664 bytes of stores per block, or 1,234,312 bytes per second at 48 kHz with 2,048-frame blocks. The bandwidth figure is modest; the structural figure is not: a single 17,936-byte object plus its 16,792-byte scratch exceeds half of a typical 32 KiB L1 data cache, and it is written on every block even though meter content changes far more slowly than the block rate. `[INFERENCE]` The cache displacement of the routing group and output accumulators is the material cost, not the copy bandwidth; no timing was measured here. |
| Recommended change | Two separable changes. First, eliminate the redundant work: reuse a single member instance instead of value-initialising a fresh `RoutingMeterTelemetry` per block, and drop one of the two whole-struct assignments (`src/core/transport/transport_controller.cpp:1158` and `src/core/transport/transport_controller.cpp:1179`) by making the accumulator the single owner of the snapshot. Second, size the lane storage to the configured topology rather than the maximum: `GroupOutputMeterFrame::lane_meters` is fixed at `kRoutingMaxOutputs` (32) and `GroupOutputMeterSnapshot::groups` is fixed at `kRoutingControlMaxGroups` (32), so a 4-group / 2-output configuration still carries 32 × 32 = 1,024 `AudioMeter` slots, of which 8 are used. The second change alters a public struct's layout and therefore needs the source-compatible treatment described under Blast radius. |
| Blast radius | The first change is **None**. The second change is a **Public C++ header** change (`include/orpheus/realtime_telemetry.h`) plus **Installed package** and **Downstream consumer** recompile. It does **not** touch C ABI 1.0: `RealtimeTelemetrySnapshot` and `RoutingMeterTelemetry` do not appear in the C ABI tables (`include/orpheus/abi.h:103-138`). A source-compatible shape is to keep the fixed arrays and instead publish the configured width in the existing `logical_lane_count` / `group_count` fields, which the struct already carries. |
| Downstream impact | Clip Composer: no rebuild and no re-pin for the first change; the second change would require a rebuild if adopted, because `include/orpheus/realtime_telemetry.h` is a public header. FreqFinder: consumes `include/orpheus/performance_monitor.h` and `include/orpheus/transport_controller.h`, not `realtime_telemetry.h`; the first change requires nothing, the second would require a package re-consumption only if its header set changes. |
| Effort | M — the redundant-copy removal is small and local; the topology-sized lane storage is a public-struct change with a golden-fixture review. |
| Acceptance test | A new test renders N blocks with a fixed topology and asserts that the per-block meter publication performs at most one whole-struct copy (observable through a copy-count seam on the accumulator), and the existing `realtime_telemetry` tests continue to pass with the same published values. |

---

### F-06 — The enabled clip compressor evaluates `std::log10` and `std::pow` once per rendered frame per voice

| Field | Value |
| --- | --- |
| Class | Performance |
| Severity | High |
| Novelty | New |
| Status | Open |
| Anchors | `src/core/transport/clip_dsp.cpp:198`, `src/core/transport/clip_dsp.cpp:201`, `src/core/transport/clip_dsp.cpp:16`, `src/core/transport/clip_dsp.cpp:156`, `src/core/transport/transport_controller.cpp:888` |
| Observation | `ClipDspProcessor::processFrame()` is called once per rendered frame per source-backed voice from `src/core/transport/transport_controller.cpp:888`. When the compressor stage is enabled, the branch computes `20.0f * std::log10(...)` at `src/core/transport/clip_dsp.cpp:198` and then `dbToLinear(reductionDb + makeupGainDb)` at `:201`, where `dbToLinear` is `std::pow(10.0f, db / 20.0f)` at `:16-17`. Both calls are inside the per-frame function; the per-channel loop beneath them is separate and bounded to 8 channels. |
| Cost mechanism | One `std::log10` and one `std::pow` per rendered frame per voice with the compressor enabled. At 32 voices and 2,048 frames that is 65,536 of each per block, or 1,536,000 of each per second at 48 kHz with 2,048-frame blocks. `[INFERENCE]` At tens of nanoseconds per libm call the ceiling is on the order of a few percent of one core. The `makeupGainDb` term folded into the `pow` argument at `src/core/transport/clip_dsp.cpp:201` is constant for the life of the program and can be precomputed; the `reductionDb` term changes every frame and cannot be hoisted without changing the gain law. |
| Recommended change | Precompute the constant makeup term in `ClipDspProcessor::prepare()` — which already runs on the control thread and already computes fixed-count coefficient and gain conversions — and evaluate the compressor gain as the product of a precomputed makeup constant and the per-frame reduction term. This removes one of the two per-frame transcendentals without changing the gain law's structure. Do not attempt to remove the `std::log10` by changing the dB-domain gain law: that is a DSP-contract decision, not a refactor, and it would alter the audible behaviour of the compressor. No table may be lazily initialised inside `processFrame`. |
| Blast radius | None structurally — `ClipDspProcessor`'s public layout in `include/orpheus/clip_dsp.h` need not change if the new constant is a private member. The **rendered sample values change** for compressor-enabled programs, because splitting one `pow` into a product of two factors is not bit-identical. No session or tool fixture in this repository enables the compressor (`grep` over `tests/fixtures/` and `tools/fixtures/` returns no match), so the existing goldens are unaffected. |
| Downstream impact | Clip Composer: no rebuild, no re-pin; it consumes `include/orpheus/clip_dsp.h` and would pick the change up through its submodule bump. FreqFinder: no rebuild, no re-pin; it does not use the clip DSP path. |
| Effort | S — one precomputed member, one changed expression, one control-thread initialisation site. |
| Acceptance test | A new clip-DSP test renders a compressor-enabled program and asserts the per-frame transcendental count through a test seam, or — if no seam is added — asserts that the produced gain sequence matches a reference computed with the precomputed makeup constant to within the documented floating-point tolerance. The existing transport goldens must remain unchanged. |

---

### F-07 — The performance-budget validator reports success when the only configured budget was skipped and neither latency nor frequency was measured

| Field | Value |
| --- | --- |
| Class | Measurement hygiene |
| Severity | High |
| Novelty | Extension of ORP266 row 7 |
| Status | Open |
| Anchors | `scripts/validate-performance.js:47`, `scripts/validate-performance.js:53`, `budgets.json:4`, `budgets.json:11`, `budgets.json:16` |
| Observation | `budgets.json` declares one bundle-size budget for `@orpheus/engine-wasm` (`budgets.json:4-10`), two command-latency budgets (`budgets.json:11-14`) and two event-frequency budgets (`budgets.json:16-19`). `validateBundleSize()` resolves the bundle to `packages/engine-wasm/dist`; when that directory does not exist it logs `⚠️ @orpheus/engine-wasm: dist/ not found (skipping)` and returns without recording a warning or a violation (`scripts/validate-performance.js:47-54`). The `packages/` tree in this checkout contains only `occ-app-platform` and `shmui-juce`, so the configured bundle is always skipped here. The command-latency section prints the limits and states that runtime profiling is required; the event-frequency section prints the limits and asserts validation by a test file that does not exist in this tree. The script then prints `✅ All performance budgets met!` and exits 0. |
| Cost mechanism | One bundle check, two latency budgets and two frequency budgets per invocation; none of the five is measured by this script. The cost is a false-green signal, not CPU: a green exit code is indistinguishable from a fully measured pass, so a reviewer or CI step that trusts the exit code is trusting a measurement that did not happen. |
| Recommended change | Make the summary honest: track every declared budget and report which ones were measured, skipped, or unmeasurable; refuse to print "All performance budgets met!" unless every declared budget was measured, and exit non-zero when a declared budget could not be measured. Either wire the latency and frequency budgets to a real measurement or remove them from `budgets.json` so the file stops asserting a contract nothing enforces. The missing-bundle path should be an explicit failure or an explicitly reported "not measured" rather than a silent skip. |
| Blast radius | None — `scripts/` and `budgets.json` are build-time tooling; no public C++ header, C ABI 1.0, virtual slot, or installed-target change. |
| Downstream impact | Clip Composer: none. FreqFinder: none. Neither consumer depends on this script. |
| Effort | S — the script is 183 lines and the budget file is 21 lines. |
| Acceptance test | The script exits non-zero, or prints a summary that explicitly names the unmeasured budgets, when `packages/engine-wasm/dist` is absent; and a test fixture that supplies a stub `dist/` with an over-budget size causes a non-zero exit. |

---

### F-08 — Headroom compensation rescans every routing channel once per group on every block

| Field | Value |
| --- | --- |
| Class | Performance |
| Severity | Medium |
| Novelty | New |
| Status | Open |
| Anchors | `src/core/routing/routing_matrix.cpp:1167`, `src/core/routing/routing_matrix.cpp:1706`, `src/core/routing/routing_matrix.cpp:1711`, `src/core/routing/routing_matrix.cpp:1672` |
| Observation | The group render loop calls `getHeadroomCompensation(group_index)` once per group at `src/core/routing/routing_matrix.cpp:1167`. In `HeadroomMode::PerGroup` and `HeadroomMode::Logarithmic` that function calls `countActiveChannelsInGroup()`, which iterates the whole channel vector and loads each channel's packed route; in `HeadroomMode::Global` it additionally calls `countTotalActiveChannels()` for every group, repeating the same global count once per group. The route snapshot is already being built for the same block at `src/core/routing/routing_matrix.cpp:1010-1013`. |
| Cost mechanism | Up to `num_groups × num_channels` channel visits per block: 32 × 256 = 8,192 route loads in the per-group modes, and a second 8,192 in the global mode, i.e. up to 16,384 atomic loads and route unpacks per block, or 384,000 per second at 48 kHz with 2,048-frame blocks. The result changes only when the route topology revision or a channel's mute state changes, so the work is repeated for a value that is almost always identical to the previous block's. |
| Recommended change | Accumulate the per-group active-channel counts and the global count once per block, inside the existing route-snapshot loop, into a fixed 32-entry array plus a scalar; then read the cached values in the group loop. The accumulation must stay fixed-capacity, allocation-free, and lock-free, and the counts must be derived from the same route snapshot the render uses so that a topology change is picked up in the block it takes effect. |
| Blast radius | None — private implementation; `HeadroomMode` and its semantics in `include/orpheus/routing_matrix.h` are unchanged. |
| Downstream impact | Clip Composer: no rebuild, no re-pin. FreqFinder: no rebuild, no re-pin. FreqFinder consumes `include/orpheus/routing_matrix.h` but no private header, so the change is invisible to it. |
| Effort | S — one fixed array, one accumulation pass, two call-site substitutions. |
| Acceptance test | A routing test enables each headroom mode, renders a fixed topology, and asserts that the published group output levels are identical to the pre-change values, plus a new assertion that the per-block active-channel count is computed at most once per group per block. |

---

### F-09 — Per-channel meter scratch lanes are cleared before being fully overwritten

| Field | Value |
| --- | --- |
| Class | Performance |
| Severity | Medium |
| Novelty | New |
| Status | Open |
| Anchors | `src/core/routing/routing_matrix.cpp:1096`, `src/core/routing/routing_matrix.cpp:1097`, `src/core/routing/routing_matrix.cpp:1110`, `src/core/routing/routing_matrix.cpp:1145` |
| Observation | With metering enabled, the channel loop clears both scratch lanes of the shared per-channel meter buffer with `std::fill_n` at `src/core/routing/routing_matrix.cpp:1096-1097` before the per-frame loop. For an unmuted, routed channel under `SourceChannelPolicy::Discrete` the left lane is assigned in every frame of the sample loop at `:1113-1115`; under `StereoPairs` the right lane is assigned at `:1125-1131` when more than one output is configured. Muted channels take the `publishChannelMeterSilence()` path and never read the scratch lanes. The group-buffer lane clears at `:1071` are a different buffer and are load-bearing — they clear lanes that a previous block or a previous topology left populated. |
| Cost mechanism | Two `std::fill_n` calls per routed channel per block, each clearing up to 2,048 floats: up to 512 fills and 1,048,576 float zero-writes per block at 256 channels, i.e. 24,576,000 float writes (98,304,000 bytes) per second at 48 kHz with 2,048-frame blocks. The cleared bytes are immediately overwritten for every frame the channel actually renders, and never read on the muted path. `[INFERENCE]` The cost is a fraction of the F-04 smoother traffic in cycles but a comparable order in memory bandwidth. |
| Recommended change | Remove the two scratch clears at `src/core/routing/routing_matrix.cpp:1096-1097` and make the write paths total: every frame that the channel renders must assign both lanes it exposes (assigning an explicit zero to the lane that the active policy does not use), and the muted path must not read the scratch lanes. Keep the group-buffer lane clears at `src/core/routing/routing_matrix.cpp:1071` unchanged. |
| Blast radius | None — private implementation; the published meter values must remain identical. |
| Downstream impact | Clip Composer: no rebuild, no re-pin. FreqFinder: no rebuild, no re-pin. |
| Effort | S — two deletions plus explicit zero-assignment in the branches that previously relied on the clear. |
| Acceptance test | The routing meter tests publish identical peak, RMS, clip-count and true-peak values before and after the change; a new test renders a muted channel followed by an unmuted channel and asserts that the unmuted channel's meter shows no residue from the muted one, which is the invariant the clear was providing. |

---

### F-10 — TruePeak metering performs a second full per-sample pass over every block

| Field | Value |
| --- | --- |
| Class | Performance |
| Severity | Medium |
| Novelty | New |
| Status | Open |
| Anchors | `src/core/routing/routing_matrix.cpp:1579`, `src/core/routing/routing_matrix.cpp:1601`, `src/core/routing/routing_matrix.cpp:1609`, `src/core/routing/routing_matrix.cpp:1232`, `src/core/routing/routing_matrix.cpp:1245` |
| Observation | `processStereoMetering()` first scans every frame of the signal to accumulate peak, sum of squares, a clipped flag and an `any_nonzero` flag (`src/core/routing/routing_matrix.cpp:1579-1591`). When the metering mode is `TruePeak` it then runs a second loop over the same frames, feeding the 4×/12-tap true-peak estimator (`src/core/routing/routing_matrix.cpp:1601` and `src/core/routing/routing_matrix.cpp:1609`). The first loop's `peak_value` is discarded in that branch, because it is reset to zero at `src/core/routing/routing_matrix.cpp:1597` before the second loop overwrites it. The group-lane path repeats the same two-pass structure at `src/core/routing/routing_matrix.cpp:1232-1243` and `src/core/routing/routing_matrix.cpp:1245-1251`. |
| Cost mechanism | One redundant per-sample pass per metered signal. Per block that is `num_channels × num_frames` = 524,288 extra sample visits for the channel meters, plus `num_groups × lanes × num_frames` = 32 × 32 × 2,048 = 2,097,152 for the group-lane meters, i.e. up to 2,621,440 redundant sample visits per block, or 61,440,000 per second at 48 kHz with 2,048-frame blocks. Each visit is a `sanitizeSample` call, an absolute value and a comparison. The true-peak estimator's own per-sample filter work is bounded and inherent to the selected estimator; this finding concerns only the duplicated traversal. |
| Recommended change | In `TruePeak` mode, skip the peak accumulation in the first pass (keep the sum of squares, the clipped flag and the `any_nonzero` flag, which the LUFS and RMS paths still need) and fuse the true-peak estimation into a single pass, so each frame is visited once. The same fusion applies to the group-lane path. The published peak, RMS, clip count and true-peak values must remain identical. |
| Blast radius | None — private implementation; the published meter semantics in `include/orpheus/routing_matrix.h` are unchanged. |
| Downstream impact | Clip Composer: no rebuild, no re-pin. FreqFinder: no rebuild, no re-pin; it consumes `include/orpheus/loudness_meter.h` and `include/orpheus/performance_monitor.h`, neither of which is affected. |
| Effort | M — the two metering paths must be merged without changing the order of the `any_nonzero` and `clipped` decisions, and the LUFS branch must keep its existing behaviour. |
| Acceptance test | A routing test in each metering mode publishes identical peak, RMS and true-peak values before and after the change, and a new test asserts that the true-peak path visits each sample once, observable through a test seam on the estimator's input counter. |

---

### F-11 — Non-converting CoreAudio routes have no SDK-level callback-frame ceiling

| Field | Value |
| --- | --- |
| Class | Refactor |
| Severity | Medium |
| Novelty | New |
| Status | Open |
| Anchors | `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:327`, `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:366`, `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:400`, `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:872` |
| Observation | During initialisation the driver raises `maximum_frames` to the maximum of the configured buffer size and the device-reported maximum buffer frames for the input scope, the output scope and the global scope (`src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:327-364`). The only explicit ceiling is applied to *conversion* routes: when input or output conversion is active and `maximum_frames > 4096`, initialisation fails with `NotSupported` (`src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:366-372`). Non-converting routes have no equivalent cap, and the stored `render_capacity_frames_` (`src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:373`) is whatever the device reported. The callback then dispatches the host render in chunks of `min(config.buffer_size, 4096)` (`src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:400-403`, consumed at `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:872`). |
| Cost mechanism | Per device callback, the number of dispatched host blocks is `ceil(M / C)` where `M` is the device-derived capacity and `C = min(config.buffer_size, 4096)`. `M` is bounded by the device-reported range rather than by an SDK-declared maximum, so the number of host renders per device callback is not fixed by the SDK. Each dispatched block runs the full transport and routing path. `[INFERENCE]` On a device reporting a large maximum buffer, one device callback can trigger a proportionally larger burst of host work than the configured buffer size implies; no physical-device measurement was taken in this session. |
| Recommended change | Establish one explicit supported maximum callback-frame count for all routes, enforce it during initialisation before buffers are allocated and before the AudioUnit is started, and keep the callback-side shape check against that same bound. The existing 4,096-frame conversion limit is a reasonable basis; a larger supported value should be stated explicitly rather than inherited implicitly from device range metadata. |
| Blast radius | None — `src/platform/audio_drivers/coreaudio/` is private; the limit can remain an internal constant. The public `AudioDriverCapabilities` surface is unaffected. |
| Downstream impact | Clip Composer: no rebuild, no re-pin for the SDK change. If the adopted ceiling is below a buffer size Clip Composer can request, that is a *runtime* rejection the app surfaces, not an API change; the app already handles route failures through the driver outcome contract. FreqFinder: no rebuild, no re-pin. |
| Effort | S — one constant, one initialisation check, and reuse of the existing check. |
| Acceptance test | A CoreAudio initialisation test asserts that a route whose device-reported maximum exceeds the SDK ceiling is refused with a typed outcome before any buffer allocation, and that a route within the ceiling still initialises. |

---

### F-12 — Session documents and JSON parsing have no byte, depth, node, or collection budget

| Field | Value |
| --- | --- |
| Class | Refactor |
| Severity | Medium |
| Novelty | New |
| Status | Open |
| Anchors | `src/core/session/json_io.cpp:49`, `src/core/common/json_parser.cpp:50`, `src/core/common/json_parser.cpp:86`, `src/core/common/json_parser.cpp:121`, `include/orpheus/json.hpp:14` |
| Observation | `ReadTextFile()` reads the whole file to EOF into a growable stream and returns the full string (`src/core/session/json_io.cpp:49-59`). `JsonParser::ParseValue()` recurses from both object parsing (`src/core/common/json_parser.cpp:86`) and array parsing (`src/core/common/json_parser.cpp:121`) while building `std::map`, `std::vector` and `std::string` storage. Session parsing then constructs a second graph of tracks, clips, markers and lanes on top of the parsed tree. No document-byte limit, nesting-depth limit, node-count limit, or per-collection limit is enforced anywhere in this path. The public entry points are `ParseSession(const std::string&)` and `LoadSessionFromFile(const std::string&)` (`include/orpheus/session_io.h:22-26`). |
| Cost mechanism | For a document of *B* input bytes, retained parsing storage and graph construction grow with *B* and with the number and total name bytes of decoded elements, and nesting depth grows with *B* as well. Neither *B* nor the element counts have a configured upper bound, and the source text and the parsed tree are both alive while the graph is constructed. The recursion in `ParseValue` is bounded only by the input, so a deeply nested document consumes stack proportional to its nesting depth. This is a **control/document** path, never an audio callback: `AGENTS.md` §Session, media, and workflow invariants places hashing, verification and session serialization off the audio thread, and no scoped session, parser or ABI operation is reachable from `processAudio`. |
| Recommended change | Define explicit document-byte and structural limits, including a maximum nesting depth, and enforce them at both the string and the file entry points so that an oversized or deeply nested document is rejected with an explicit error before excessive allocation or stack consumption. Publish the limits in the public session-I/O documentation so that consumers can size their own inputs. |
| Blast radius | **Public C++ header** — the accepted-document contract of `ParseSession` and `LoadSessionFromFile` changes, even though the enforcement itself is private. No C ABI 1.0 table change; the session C ABI surface exposes the same operations through `orpheus_session_api_v1` (`include/orpheus/abi.h:103-113`) and inherits the same limit. |
| Downstream impact | Clip Composer: consumes `orpheus/media_integrity.h` and session loading through the SDK; no rebuild or re-pin is required for the SDK to carry the limit, but a Clip Composer that loads user-authored session files larger than the new limit would surface a new explicit error and needs to handle it. FreqFinder: does not load session documents through this path; no rebuild, no re-pin. |
| Effort | M — the limits interact with the schema version, the recovery path, and the public error vocabulary. |
| Acceptance test | A session-I/O test feeds a document exceeding each limit and asserts an explicit, distinguishable error, and asserts that a document at the limit still parses. The existing session golden fixtures under `tests/fixtures/session/` and `tools/fixtures/` must continue to parse unchanged. |

---

### F-13 — Session parsing sorts and re-validates the whole track on every clip insertion

| Field | Value |
| --- | --- |
| Class | Performance |
| Severity | Medium |
| Novelty | New |
| Status | Open |
| Anchors | `src/core/session/json_io.cpp:283`, `src/core/session/session_graph.cpp:71`, `src/core/session/session_graph.cpp:110`, `src/core/session/session_graph.cpp:348` |
| Observation | The session parser adds every clip through the ordinary `SessionGraph::add_clip` path (`src/core/session/json_io.cpp:283-305` → `src/core/session/session_graph.cpp:348` → `:602-611`). `Track::add_clip` appends the clip, then sorts the entire track (`src/core/session/session_graph.cpp:110`) and validates the entire clip layout, on every single insertion. A track with *C* clips therefore pays one whole-track sort and one whole-track validation per clip. |
| Cost mechanism | For a track with *C* clips, the sort performs approximately O(C² log C) comparisons in total and the validation O(C²) work — a per-document cost that grows quadratically with the number of clips in the largest track. *C* has no configured maximum, and the bound is per track, so a document with many tracks multiplies it. This is a **control/document** cost: it happens on the thread that loads the session, not on the audio thread, and it is the load latency a user sees when opening a large session. |
| Recommended change | Add a bulk construction path for session loading: reserve the known clip count, append the track's clips, then sort and validate once per track. Keep the per-call sort and validation for ordinary one-at-a-time `add_clip` calls so that the interactive contract does not change. The bulk path must reuse the same validation routine so that a document accepted before is still accepted after. |
| Blast radius | **Public C++ header** if the bulk entry point is declared on the public graph type, or **None** if it is a private `SessionGraph` implementation detail reached through the existing public session-I/O functions. No C ABI 1.0 table change; the C ABI session surface would inherit the improvement. |
| Downstream impact | Clip Composer: no rebuild and no re-pin for the SDK change; it would observe faster session load for large documents. FreqFinder: no rebuild, no re-pin. |
| Effort | M — the bulk path must preserve clip ordering, ID assignment and the exact validation outcome of the incremental path. |
| Acceptance test | A session test loads a synthetic document with a large single track and asserts that the resulting graph is identical — same clip order, same IDs, same validation outcome — to the graph produced by the incremental path, and that the bulk path performs one sort per track. |

---

### F-14 — Transaction rollback allocates inside a `noexcept` path

| Field | Value |
| --- | --- |
| Class | Refactor |
| Severity | Medium |
| Novelty | Extension of ORP263 F-02 |
| Status | Open |
| Anchors | `src/core/session/session_graph.cpp:227`, `src/core/session/session_graph.cpp:232`, `src/core/session/session_graph.cpp:297`, `src/core/session/session_graph.cpp:241` |
| Observation | `SessionGraph::Transaction::rollback()` is declared `noexcept` at `src/core/session/session_graph.cpp:227` and calls `graph_->restore_unchecked(before_)` at `:232`. `restore_unchecked()` at `:297` rebuilds the live graph: it allocates track storage, two `unordered_set`s, the track and clip objects, and copies the clip and track names. `begin_transaction()` at `:241` takes a full `snapshot()` up front. |
| Cost mechanism | One full snapshot at transaction begin, and on rollback one full reconstruction over the graph's tracks, clips, assignments and their names. The peak memory footprint during rollback is the live graph plus the saved snapshot plus the rebuilding graph. Because the reconstruction allocates, an allocation failure inside this `noexcept` function terminates the process rather than surfacing an error. This is a **control/document** path with no audio-thread involvement. |
| Recommended change | Replace the snapshot-and-rebuild rollback with a transaction journal or retained owned state that can be restored through a no-throw swap or ownership transfer, so the rollback path performs no allocation. Preserve the existing semantics: one revision per logical edit, rollback on destruction, rejection of nesting, and the "never lower live ID watermarks" rule that ORP263 F-02 established. |
| Blast radius | **Public C++ header** — the public `SessionGraph::Transaction` representation would need to carry the retained state or journal. No C ABI 1.0 table change; the C ABI session surface inherits the behaviour. |
| Downstream impact | Clip Composer: no rebuild and no re-pin for the SDK change; it consumes `orpheus/media_integrity.h` and the session surface but not the private transaction type. FreqFinder: no rebuild, no re-pin. |
| Effort | L — a journal or retained-state design touches the graph's copy semantics, the snapshot format, and the transaction lifetime rules that the rest of the repository depends on. |
| Acceptance test | A session test begins a transaction, mutates the graph, forces the rollback, and asserts the restored graph is equal to the pre-transaction state; the same test is repeated under an allocation-failure injection to assert that the rollback does not terminate the process. The existing transaction and rollback tests must pass unchanged. |

---

### F-15 — `TransportController` co-locates five distinct ownership domains in one class and one 3,874-line translation unit

| Field | Value |
| --- | --- |
| Class | Refactor |
| Severity | Medium |
| Novelty | New |
| Status | Open |
| Anchors | `src/core/transport/transport_controller.h:581`, `src/core/transport/transport_controller.h:599`, `src/core/transport/transport_controller.h:797`, `src/core/transport/transport_controller.h:818`, `src/core/transport/transport_controller.h:827`, `src/core/transport/transport_controller.cpp:614`, `src/core/transport/transport_controller.cpp:1272` |
| Observation | This finding is argued from concrete ownership evidence, not from the presence of matching declarations against the public interface. The private header `src/core/transport/transport_controller.h` (854 lines) and the implementation `src/core/transport/transport_controller.cpp` (3,874 lines) hold, in one class: the bounded MPSC command pool (`src/core/transport/transport_controller.h:581`, consumed by `processCommands()` at `src/core/transport/transport_controller.cpp:1272`); the audio-thread-owned active voice array (`:599`, driven by `processAudio()` at `:614`); a control-side, mutex-protected source registry (`src/core/transport/transport_controller.h:797-798`); the routing-matrix instance and the meter accumulator (`:818`); and the realtime telemetry bridge (`:827`). The source/context/lifetime-token handoff (`ClipPlaybackContext`, `SourceCommandLifetime`, `TransportCommand`) crosses between the control-owned and audio-owned domains. |
| Cost mechanism | This is a coupling and maintenance cost, not a measured CPU defect. The concrete consequence is that the control-side registry cannot be changed, tested, or reasoned about independently of the audio-thread render state, and the render-path changes recommended by F-03, F-04 and F-05 all land in a file that also contains the registry, the command pool, the voice lifecycle, the meter publication, and the event pump. The per-controller capacities that bound the audio-thread work (255 command nodes, 32 active voices) are unchanged by the split. |
| Recommended change | Extract the control-only audio-file registry and source-preparation ownership — `AudioFileEntry`, `m_audioFilesMutex`, `m_audioFiles`, and the source preparation and worker lifetime — behind a private registry component, and keep the active voices, the command consumer, the render loop and the realtime publication in `TransportController`. Preserve the existing immutable playback-context and `SourceCommandLifetime` handoff exactly, and do not introduce any registry lock into the audio path. Split the implementation into matching translation units (`transport_registry.cpp`, `transport_render.cpp`, `transport_commands.cpp`) so that the file size itself reflects the separation. |
| Blast radius | None — `src/core/transport/transport_controller.h` is private; the public interface in `include/orpheus/transport_controller.h` (1,118 lines) is unchanged, no public C++ header changes, no C ABI 1.0 table change, no C++ virtual slot change. |
| Downstream impact | Clip Composer: no rebuild, no re-pin — it consumes `orpheus/transport_controller.h` (six include sites) and `add_subdirectory`-consumes `Orpheus::core`; the public interface is unchanged. FreqFinder: no rebuild, no re-pin — it consumes `orpheus/transport_controller.h` (three include sites) through the installed package. |
| Effort | L — a several-thousand-line translation unit with a control/audio thread boundary in the middle; the risk is in the move, not the design. |
| Acceptance test | The full transport test set passes unchanged, the strict realtime audit still reports 0 hard failures with the moved code in its new translation units, and the realtime harness test still reports zero allocations and zero media I/O across the multi-clip stress run. |

---

### F-16 — `RoutingMatrix` co-locates configuration publication, render state, and meter state

| Field | Value |
| --- | --- |
| Class | Refactor |
| Severity | Medium |
| Novelty | New |
| Status | Open |
| Anchors | `src/core/routing/routing_matrix.h:34`, `src/core/routing/routing_matrix.h:73`, `src/core/routing/routing_matrix.h:149`, `src/core/routing/routing_matrix.h:258`, `src/core/routing/routing_matrix.h:314`, `src/core/routing/routing_matrix.cpp:923`, `src/core/routing/routing_matrix.cpp:1559` |
| Observation | This finding is argued from concrete state-domain evidence. `ChannelState` (`src/core/routing/routing_matrix.h:34`) holds the route, gain, mute and solo control state together with the channel's meter values and true-peak history; `GroupState` (`src/core/routing/routing_matrix.h:73`) holds control and geometry state together with the group's aggregate meter and its 32 lane meters. `RoutingMatrix` (`src/core/routing/routing_matrix.h:149`) owns the double-buffered configuration publication (`src/core/routing/routing_matrix.h:258`), the render group-control publication (`src/core/routing/routing_matrix.h:314`), the matrix-wide meter publication, and the render buffer storage. The implementation is a single 1,735-line translation unit spanning configuration (`src/core/routing/routing_matrix.cpp:1522-1541`), the render path (`src/core/routing/routing_matrix.cpp:923-1360`) and metering (`src/core/routing/routing_matrix.cpp:1559-1667`). |
| Cost mechanism | A coupling and maintenance cost, not a measured CPU defect. The concrete consequence is that a metering change, a configuration change and a render change all have to be reviewed in the same class and the same file, and the meter state — which is read by other threads and published atomically — cannot be evolved independently of the render path that produces it. The fixed maxima that bound the audio-thread work (256 channels, 32 groups, 32 outputs, 32 lane meters) are unchanged by the split. |
| Recommended change | Extract the meter storage and publication into a private `RoutingMeterState` component with bounded atomic publication and reset operations, and keep the route topology and render ownership in `RoutingMatrix`. Split the implementation into `routing_matrix.cpp` (configuration and topology) and `routing_meter_state.cpp` (metering and publication). The render-thread updates must remain allocation-free, and the existing coherent-publication sequence semantics — the odd/even `m_group_output_meter_publication_sequence` protocol — must be preserved exactly. |
| Blast radius | None — `src/core/routing/routing_matrix.h` is private; the public interface in `include/orpheus/routing_matrix.h` (673 lines) and the public `GroupOutputMeterSnapshot` type are unchanged. No C ABI 1.0 change. |
| Downstream impact | Clip Composer: no rebuild, no re-pin; it consumes `orpheus/routing_matrix.h` and `Orpheus::routing` through `add_subdirectory`. FreqFinder: no rebuild, no re-pin; it consumes `orpheus/routing_matrix.h` (two include sites) through the installed package. |
| Effort | L — the meter publication protocol is load-bearing for the routing telemetry that Clip Composer and FreqFinder display. |
| Acceptance test | The routing and multi-channel routing tests pass unchanged, the published group-output meter snapshot remains byte-identical for a fixed topology, and the strict realtime audit still reports 0 hard failures with the moved code. |

---

### F-17 — Sanitizer CI jobs apply unsanitized timing thresholds to instrumented measurements

| Field | Value |
| --- | --- |
| Class | Measurement hygiene |
| Severity | Medium |
| Novelty | New |
| Status | Open |
| Anchors | `tests/common/performance_monitor_test.cpp:284`, `tests/common/performance_integration_test.cpp:148`, `tests/audio_io/waveform_processor_test.cpp:172`, `tests/audio_io/waveform_processor_test.cpp:281`, `tests/transport/realtime_harness_test.cpp:749` |
| Observation | `PerformanceOfGetMetrics` requires the average of 10,000 `getMetrics()` calls to stay below 1,000 ns (`tests/common/performance_monitor_test.cpp:284-305`). `OverheadMeasurement` warms up for 1,000 calls, measures 100,000, and requires an average below 500 ns and calculated overhead below 0.01 % (`tests/common/performance_integration_test.cpp:148-193`). The libsndfile-enabled waveform tests require each cached-peak lookup to complete in under 100 µs (`tests/audio_io/waveform_processor_test.cpp:172-182` and `:281-288`). None of these assertions is guarded by a sanitizer check, while the transport's own callback-deadline assertions *are* guarded (`tests/transport/realtime_harness_test.cpp:749-790` skips the wall-clock check under sanitizers) and the 10-minute waveform smoke bound is explicitly skipped under sanitizers (`tests/audio_io/waveform_processor_test.cpp:335-387`). The CI Debug job enables ASan/UBSan and runs the suite, and the Linux TSan job explicitly builds and runs both performance-monitor tests. |
| Cost mechanism | Per CTest invocation, 10,000 timed `getMetrics()` calls, 101,000 timed calls in the integration test, and one cached-peak lookup in each of two waveform cases — all with sanitizer instrumentation included in the measured wall clock. `[INFERENCE]` Instrumentation inflates per-call cost by an order of magnitude or more, so these thresholds are simultaneously likely to be noisy and likely to fail once a job happens to be scheduled on a loaded machine. No such failure was observed in this session: `performance_monitor_test` passed in 0.26 s on this Debug+ASan+UBSan tree. |
| Recommended change | Separate functional assertions from timing assertions. Keep the functional and concurrency coverage in the sanitizer jobs, and either skip the timing assertions when sanitizers are enabled — the pattern the realtime harness already uses — or run the timing-budget checks only in an unsanitized performance configuration. Apply the same guard to the cached-peak bound, and make the guard condition explicit in the test rather than relying on a global helper. |
| Blast radius | None — test sources and CTest configuration only; no public C++ header, C ABI 1.0, virtual slot, or installed-target change. |
| Downstream impact | Clip Composer: none. FreqFinder: none. |
| Effort | S — one shared guard plus a few call sites. |
| Acceptance test | A sanitizer-configured run of the three test targets reports the timing assertions as skipped, and an unsanitized run still asserts them; the recorded result of each configuration states which mode it ran in. |

---

### F-18 — The clip DSP and routing findings have no in-repository measurement gate that would detect a regression

| Field | Value |
| --- | --- |
| Class | Measurement hygiene |
| Severity | Medium |
| Novelty | Extension of ORP138 §3.3 |
| Status | Open |
| Anchors | `tools/perf/perf_render_click.cpp:34`, `tools/perf/CMakeLists.txt:2`, `CMakeLists.txt:332`, `scripts/validate-performance.js:125` |
| Observation | The repository ships one performance tool. `tools/perf/perf_render_click.cpp` calls `render->render_click(&spec, output)` once per rate and prints the elapsed wall time (`tools/perf/perf_render_click.cpp:34-58`); the measured interval therefore includes the offline render **and** the WAV file write. `tools/perf/CMakeLists.txt` defines the executable and applies warning flags but registers no `add_test` and no `gtest_discover_tests`, so it is not a CTest test; `CMakeLists.txt:332-333` installs it. The command-latency budgets in `budgets.json` are not measured by any gate, and `scripts/validate-performance.js:125` prints "Actual latency measurement requires runtime profiling" instead. The only per-frame budget evidence in the repository is the opt-in unsanitized `WorstCaseTopologyDeadlineDistribution` harness, which is skipped by default in Debug. |
| Cost mechanism | One measurement per rate per invocation, with no threshold, no comparison, no baseline, and no registration in the test inventory. The practical consequence is that none of F-03, F-04, F-05, F-06, F-08, F-09 or F-10 can be shown to improve or regress by any gate the repository runs. `[INFERENCE]` Without a registered, thresholded, release-configuration measurement, a future refactor of the render path is as likely to regress as to improve, and nothing in the repository would say so. |
| Recommended change | Do not treat the existing tool as the measurement. It measures an end-to-end offline render including file I/O and therefore cannot bound a callback deadline. Add a measurement that exercises the transport and routing render path at a fixed topology in an unsanitized configuration and reports a per-block wall-clock distribution against the frame/rate deadline — the shape the existing opt-in `WorstCaseTopologyDeadlineDistribution` harness already has — and register it so that it is visible in the test inventory. This review does not add that tool; it names the gap. |
| Blast radius | None — build and measurement tooling. |
| Downstream impact | Clip Composer: none. FreqFinder: none. |
| Effort | M — a registered, unsanitized, thresholded render-path measurement, plus a documented policy for which configurations run it. |
| Acceptance test | A registered CTest entry (or an explicitly named CI job) that renders a fixed transport-and-routing topology in an unsanitized configuration and reports per-block wall-clock percentiles against the frame/rate deadline, with the result recorded in the build output. |

---

### F-19 — The branch carrying ORP266's Medium and Low remediations is not an ancestor of `main`, so the architecture-document corrections are still absent

| Field | Value |
| --- | --- |
| Class | Both |
| Severity | Medium |
| Novelty | Extension of ORP266 rows 4, 5, 6 and 16 |
| Status | Open |
| Anchors | `ARCHITECTURE.md:253`, `ARCHITECTURE.md:263`, `ARCHITECTURE.md:267`, `ARCHITECTURE.md:277`, `ARCHITECTURE.md:368`, `docs/orp/ORP266 DeepWiki Coherence Audit.md:154` |
| Observation | ORP266's disposition states that the ten Medium and five Low findings were scoped to the `fix/orp266-audit-remediation` branch, and ORP268 §1 repeats that scoping. That branch exists locally and on `origin`, but `git merge-base --is-ancestor fix/orp266-audit-remediation main` exits 1 — it is **not** an ancestor of `main`. The specific corrections have therefore not landed: `ARCHITECTURE.md:253` still says audio files are "Pre-loaded into memory for real-time access", `ARCHITECTURE.md:254` still says "Currently requires 48kHz (no resampling yet)", `ARCHITECTURE.md:263` still lists WASAPI as "Planned for v1.0", `ARCHITECTURE.md:267` still points at `src/core/audio_io/drivers/`, and `ARCHITECTURE.md:277` still names `include/orpheus/session_json.h` and `src/core/session/session_json.cpp` — none of which exist in this tree. `ARCHITECTURE.md:368` still shows the stale `std::atomic<uint64_t> currentFrame` in the `ActiveClip` pseudocode. One ORP266 row *was* independently remediated on `main`: `docs/SUPPORT_MATRIX.md:64-70` now lists all seven installed targets including `Orpheus::audio_driver_manager`. |
| Cost mechanism | A bounded but recurring cost: a reviewer, an agent, or a downstream integrator who reads `ARCHITECTURE.md` audits or integrates against paths and platform statuses that do not exist. The concrete cost is one wrong-path audit pass per reader, and the stale "48 kHz only, no resampling" statement contradicts the shipped resampling reader and the prepared/streaming source architecture that ORP134 G1 delivered. |
| Recommended change | Land the already-scoped Medium and Low remediations from `fix/orp266-audit-remediation` — or rebase them — rather than re-planning them, so that `ARCHITECTURE.md` matches the tree that ORP134, ORP263 and ORP266 already describe. If the branch cannot be landed as-is, the corrections to `ARCHITECTURE.md:253-254`, `ARCHITECTURE.md:263-264`, `ARCHITECTURE.md:267`, `ARCHITECTURE.md:277` and `ARCHITECTURE.md:368` are small enough to apply directly. The separate branch `fix/orp263-264-contract-hardening` *is* an ancestor of `main` and needs no action. |
| Blast radius | None — documentation only. `docs/ARCHITECTURE.md` is link-checked by `tools/docs_path_audit.py`; `docs/orp/` records are excluded from the audit's checks, so the ORP record itself is not link-audited. |
| Downstream impact | Clip Composer: none. FreqFinder: none. No code, header, package or ABI change is involved. |
| Effort | S — the remediation branch already exists; the work is to land or rebase it. |
| Acceptance test | `git merge-base --is-ancestor fix/orp266-audit-remediation main` exits 0, or the five specific `ARCHITECTURE.md` statements are corrected directly and `python3 tools/docs_path_audit.py --root .` still exits 0. |

---

### F-20 — `MediaStreamWorker::snapshotSources` rebuilds the live-source vector every service round, with an O(S²) identity scan, while holding the worker mutex

| Field | Value |
| --- | --- |
| Class | Performance |
| Severity | Low |
| Novelty | New |
| Status | Open |
| Anchors | `src/core/transport/clip_source.cpp:1272`, `src/core/transport/clip_source.cpp:1287`, `src/core/transport/clip_source.cpp:1330`, `src/core/transport/clip_source.cpp:1336`, `include/orpheus/clip_source.h:122` |
| Observation | `MediaStreamWorker::run()` calls `snapshotSources(live, next)` on every service round while holding the worker mutex (`src/core/transport/clip_source.cpp:1330`; the unlock follows at `:1331`), and then waits up to 2 ms before the next round (`:1336`). `snapshotSources` prunes the previous snapshot, clears and re-reserves `next`, locks every registered weak pointer, and — for each source — scans the entire previous `live` vector to carry its command-fill state forward (`:1287-1292`). This is a **worker thread** operation; the audio thread never acquires this mutex. |
| Cost mechanism | For *S* registered streaming sources, the nested identity search performs up to *S²* pointer comparisons per round, in addition to *S* weak-pointer locks and two vector rebuilds. With a 2 ms wait the round rate is at most 500 per second, so the comparison count is up to 500 × *S²* per second — 512,000 comparisons per second at *S* = 32. The worker mutex is held for the whole rebuild, so the cost extends the critical section that a control-side `attach` also contends for. *S* has no fixed repository cap. |
| Recommended change | Avoid the per-round rebuild and the nested scan: keep a stable slot array with a generation counter, or sort the previous snapshot by source identity and use `lower_bound` to transfer command state in O(S log S). Reuse the persistent vector capacities and preserve the expired-entry pruning. The change must stay entirely on the worker thread and must not move any work onto the audio thread. |
| Blast radius | None — `src/core/transport/clip_source.cpp` is private; `include/orpheus/clip_source.h` and the page-ring constants (`kPageFrames`, `kWindowPages`, `kCommandPrimePages`, `kMaxVoiceDemands`) are unchanged. |
| Downstream impact | Clip Composer: no rebuild, no re-pin. FreqFinder: no rebuild, no re-pin. |
| Effort | M — the snapshot carries command-fill state that must survive the restructure exactly. |
| Acceptance test | The streaming and streaming-seek tests pass unchanged, and a new test registers a bounded number of streaming sources and asserts that the per-round identity work is linear rather than quadratic. |

---

## 5. Sequenced recommendation plan

The waves are ordered so that each depends only on waves above it. No wave
proposes a change that would allocate, lock, perform I/O, log, or own a
`std::function` on the audio thread; every recommendation preallocates on the
control thread, keeps fixed-capacity containers with explicit overflow outcomes,
and preserves the integer-sample canonical time, the prepared-media contract, and
the silence-plus-observable-underrun behaviour on a streaming miss.

### Wave 0 — Measurement hygiene (no code behaviour changes)

| Finding | What lands | Dependency | Blast radius |
| --- | --- | --- | --- |
| F-07 | The budget validator reports what it measured and fails when a declared budget could not be measured. | None | None |
| F-17 | Timing assertions are guarded under sanitizers, following the pattern the realtime harness already uses. | None | None |
| F-18 | A registered, unsanitized, thresholded render-path measurement is added. | None | None |
| F-19 | The ORP266 Medium/Low remediation branch is landed or its five `ARCHITECTURE.md` statements are corrected directly. | None | None |

**Safe to land without a downstream rebuild.** All four are build, test or
documentation changes. Wave 0 is the prerequisite for judging Waves 1 to 3: the
measurements in F-03 through F-10 have no release-representative baseline
today, so landing a performance change before F-18 would be an unmeasurable
claim.

### Wave 1 — Realtime-path correctness and contract

| Finding | What lands | Dependency | Blast radius |
| --- | --- | --- | --- |
| F-01 | The CoreAudio device callback publishes the route-monitor terminal state atomically and does not notify a waiter; the unreachable duplicate malformed-output block is removed. | None | None |
| F-02 | The EqualPower fade math is pinned to a platform-independent formulation and the render-hash golden constant is added. | F-18 (the golden must be measurable across platforms before it is pinned) | Rendered sample values change; goldens regenerate |
| F-11 | One explicit SDK callback-frame ceiling for all CoreAudio routes, enforced at initialisation. | None | None |

**Safe to land without a downstream rebuild**, with one caveat: F-02 changes
rendered bytes, so a downstream consumer that pins a rendered-audio hash must
regenerate it. No consumer rebuild, no submodule re-pin and no package
re-consumption are required for any of the three.

### Wave 2 — Allocation, redundancy and duplication removal in the render path

| Finding | What lands | Dependency | Blast radius |
| --- | --- | --- | --- |
| F-03 | The per-block full-lane memset is replaced by a voice-lifetime lane-zero invariant. | F-18 (to show the change) | None |
| F-04 | `GainSmoother::process()` is inlined; the smoother state is hoisted out of the per-sample loop; the pan smoothers are not advanced under `Discrete`. | F-03 (same render path; the lane invariant changes the surrounding code) | None |
| F-09 | The redundant meter scratch clears are removed and the write paths made total. | F-05 (same block) | None |
| F-10 | The true-peak metering path is fused into a single per-sample pass. | None | None |
| F-08 | Headroom channel counts are accumulated once per block. | F-10 (same block) | None |
| F-05 | The redundant meter telemetry copies are removed; topology-sized lane storage is evaluated separately. | F-09, F-10 | None for the copy removal; public C++ header if the lane storage is resized |
| F-06 | The compressor's constant makeup term is precomputed on the control thread. | None | None |
| F-13 | Session parsing uses a bulk per-track construction path. | None | Public C++ header if the entry point is public |
| F-20 | The worker snapshot avoids the per-round rebuild and the nested identity scan. | None | None |

**Safe to land without a downstream rebuild** for F-03, F-04, F-06, F-08, F-09,
F-10 and F-20: all are private implementation changes with no public header,
virtual slot, or ABI change, and every rendered-output change must keep the
existing goldens valid. F-05's copy removal and F-13's bulk path are also safe
in their private form. A **Clip Composer submodule re-pin** is required only if
F-05's topology-sized lane storage is adopted, because that changes
`include/orpheus/realtime_telemetry.h`; a **FreqFinder package
re-consumption** is required only for the same reason, and only if FreqFinder
transitively includes that header.

### Wave 3 — Structural decomposition

| Finding | What lands | Dependency | Blast radius |
| --- | --- | --- | --- |
| F-12 | Explicit document-byte and structural limits for session and JSON parsing. | None | Public C++ header (accepted-document contract) |
| F-14 | A no-throw, non-allocating transaction rollback. | None | Public C++ header (transaction representation) |
| F-15 | The control-side source registry is extracted from `TransportController`. | F-03, F-04, F-05 (the render path must be settled first so the split does not move code twice) | None |
| F-16 | Meter storage and publication are extracted from `RoutingMatrix`. | F-08, F-09, F-10 (the metering path must be settled first) | None |

**Wave 3 requires a Clip Composer submodule re-pin and a FreqFinder package
re-consumption** for F-12 and F-14, because both change a public header
(`include/orpheus/session_io.h` semantics and
`include/orpheus/session_graph.h` respectively) and the downstream consumers
compile against those headers. F-15 and F-16 are private-header decompositions:
they require a rebuild of the SDK and, for consumers that build from source, a
submodule re-pin (Clip Composer) or package re-consumption (FreqFinder) — but
they do not change any public header, so no consumer source change is needed.

### Wave 4 — Tooling, measurement and documentation consolidation

| Finding | What lands | Dependency | Blast radius |
| --- | --- | --- | --- |
| F-18 | The registered render-path measurement, once its thresholds have been calibrated against the Wave 2 results. | Wave 2 | None |
| F-19 | The architecture document brought back in line with the tree. | None | None |

Wave 4 is deliberately last: its thresholds should be calibrated against the
Wave 2 render path, not against the one being changed.

---

## 6. Downstream consumer impact

### 6.1 What the two consumers actually consume

Clip Composer consumes the SDK as a git submodule at
`third_party/orpheus-sdk`, pinned to revision `e885ddcf7d3113f5794b31103cd40e90f0d03745`,
and builds it with `add_subdirectory`, requiring `Orpheus::core`.

| Public header Clip Composer includes | Include sites | Installed target |
| --- | --- | --- |
| `orpheus/transport_controller.h` | 6 | `Orpheus::core` |
| `orpheus/media_integrity.h` | 5 | `Orpheus::core` |
| `orpheus/audio_driver.h` | 4 | `Orpheus::core` / `Orpheus::audio_io` |
| `orpheus/routing_matrix.h` | 1 | `Orpheus::core` / `Orpheus::routing` |
| `orpheus/channel_format.h` | 1 | `Orpheus::core` |
| `orpheus/audio_driver_manager.h` | 1 | `Orpheus::audio_driver_manager` |
| `orpheus/performance_monitor.h` | 1 | `Orpheus::core` |
| `orpheus/clip_dsp.h` | 1 | `Orpheus::core` |
| `orpheus/audio_file_reader.h` | 1 | `Orpheus::audio_utils` |

FreqFinder consumes the SDK as an **installed package**
(`find_package(OrpheusSDK CONFIG REQUIRED)`), requires `Orpheus::core` and
`Orpheus::audio_utils`, and additionally uses the private `Orpheus::shmui_juce`
target.

| Public header FreqFinder includes | Include sites | Installed target |
| --- | --- | --- |
| `orpheus/transport_controller.h` | 3 | `Orpheus::core` |
| `orpheus/routing_matrix.h` | 2 | `Orpheus::core` / `Orpheus::routing` |
| `orpheus/performance_monitor.h` | 1 | `Orpheus::core` |
| `orpheus/loudness_meter.h` | 1 | `Orpheus::audio_utils` |
| `orpheus/dsp/oscillator.hpp` | 1 | `Orpheus::core` |
| `orpheus/audio_graph.h` | 1 | `Orpheus::core` |
| `orpheus/audio_analysis.h` | 1 | `Orpheus::audio_utils` |

Neither consumer includes `include/orpheus/realtime_telemetry.h`,
`include/orpheus/session_graph.h` or `include/orpheus/session_io.h` directly.
Both consumers are out of scope for this review and were not modified.

### 6.2 Per-wave consumer action

| Wave | Clip Composer | FreqFinder |
| --- | --- | --- |
| Wave 0 (measurement hygiene) | Nothing. | Nothing. |
| Wave 1 (realtime-path correctness) | Nothing — no rebuild, no re-pin. Regenerate any Clip Composer artefact that pins a rendered-audio hash, because F-02 changes rendered bytes. | Nothing. |
| Wave 2 (render-path redundancy) | Nothing for F-03, F-04, F-06, F-08, F-09, F-10, F-13, F-20. A **submodule re-pin** is required only if F-05's topology-sized `GroupOutputMeterFrame::lane_meters` storage is adopted, because that changes `include/orpheus/realtime_telemetry.h`. | Nothing for the same findings. A **package re-consumption** is required only for the F-05 lane-storage change, and only if the installed header set changes. |
| Wave 3 (structural decomposition) | A **submodule re-pin** and a rebuild are required for F-12 and F-14, which change the accepted-document contract and the public transaction representation respectively. F-15 and F-16 change no public header, so a re-pin and rebuild are sufficient and no consumer source change is needed. | A **package re-consumption** and a rebuild are required for F-12 and F-14 for the same reasons. F-15 and F-16 need a re-consumption and a rebuild but no source change. |
| Wave 4 (tooling and documentation) | Nothing. | Nothing. |

### 6.3 Contract surfaces that no wave may change

The following are preserved by every recommendation in §4, and a change to any of
them would require its own record:

- the three C ABI 1.0 tables and their layouts in `include/orpheus/abi.h:103-138`,
  and every existing `treefall_*` C wrapper;
- the public headers under `include/orpheus/` and the forwarding headers under
  `include/treefall/`, except where a finding explicitly names a **Public C++
  header** blast radius and proposes a source-compatible shape;
- the installed target set — `Orpheus::core`, `Orpheus::diagnostics`,
  `Orpheus::audio_utils`, `Orpheus::audio_io`, `Orpheus::audio_driver_manager`,
  `Orpheus::routing`, `Orpheus::transport` — and the `Orpheus::shmui_juce`
  package target;
- the `packages/shmui-juce` governed import, its pinned upstream revision, and
  its design-token contract version, all of which go through
  `tools/shmui_juce_manifest.py --sync` / `--check`;
- the SDK version 0.9.1 and the C ABI 1.0 version pair.

---

## 7. Explicitly excluded and gated items

The following are **not findings of this review**. Each is gated by
`AGENTS.md` §Deferred work, `docs/SUPPORT_MATRIX.md`, `ORP143` §7, or
`ORP170`/`ORP173`, and this review follows the ORP266 §4 gate-statement pattern
of recording them as correctly gated rather than as gaps.

- **Windows/WASAPI release-support promotion.** WASAPI remains implemented but
  not release-supported. `AGENTS.md` §Deferred work gates promotion on the
  evidence in `ORP143` §7, and `AGENTS.md` §Sources of truth states explicitly
  that the merged WASAPI implementation "is not release-supported until hosted
  Windows package/ABI checks and a real-device acceptance record pass". No
  finding in this record proposes promotion, and no Windows-hosted check was
  performed or claimed.
- **ASIO.** ASIO stays an optional, source-only integration requiring an
  external SDK, excluded from the installed manifest and exports. ORP266 row 14
  already records that its enabled Windows path references an absent `asio/`
  source directory; that row is open and owned by its own record, and is not
  re-raised here.
- **Linux production backend.** Dummy remains the only advertised Linux driver.
  ALSA, JACK and PipeWire remain distinct future providers. This review makes no
  Linux-backend claim and ran no Linux verification.
- **ORP170 candidate and ORP173 stable promotion.** Both remain gated on the
  external acceptance records enumerated in ORP173. The observed development
  snapshot reviewed here is not a release claim, and this review does not
  create or propose a candidate or stable snapshot.
- **ORP266 rows 1 and 2 / ORP268 Requirements A and B.** The clock-domain and
  discontinuity plumbing and the C ABI table size handshake remain deferred
  requirements owned by ORP268. No recommendation in §4 changes the C ABI
  layout or the transport render entry point's signature.
- **ORP263 F-06, F-07 and F-09.** The performance-monitor latency semantics,
  metric-coherence and reset contracts, and the callback-p99 semantics remain
  open and owned by ORP263. F-05 touches the routing-meter publication channel,
  which is a different surface, and does not claim to resolve them.
- **ORP263 R-01 … R-07.** The maximum-topology timing qualification, the
  single-worker fairness and cancellation question, the driver-manager rollback
  and lock scope, the CoreAudio converter priming deadline, the WASAPI
  verification gap, the early-EOF media normalisation decision, and the
  pointer/latency invalidation semantics remain open in that record. F-03 is
  explicitly an extension of R-01's lane-clear observation, not a replacement
  for its timing qualification.
- **`packages/shmui-juce/`.** A governed import at pinned upstream revision
  `d420ec64dcb5fd16baa0b95d5048ee09bb95eee8`, with design-token contract
  version 0.6.0. It is not a refactor target, and no API-shape change is
  proposed there; any change goes through the documented import procedure and
  `tools/shmui_juce_manifest.py --sync` / `--check`.
- **The child applications.** Clip Composer and FreqFinder are read-only impact
  context for this review. `AGENTS.md` §Mission and boundaries forbids modifying
  child-app repositories as part of an SDK task, and neither repository was
  modified.

---

## 8. Verification performed

### 8.1 What was run

| Check | Command | Result |
| --- | --- | --- |
| Worktree and revision | `git status --short --branch`, `git rev-parse HEAD`, `git remote -v`, `git log --oneline -5` | Clean tree at `0325c6400a37be9926b4b6dae532885417cabe9b` on `main` tracking `origin/main` |
| Strict realtime audit | `python3 tools/realtime_audit.py --root . --fail-known-debt` | `Realtime audit passed: 0 hard failures, 0 tracked debt findings.` — exit 0 |
| Adjacent-repo strict audit | `python3 tools/realtime_audit.py --root . --fail-known-debt --include-adjacent` | 1 tracked debt finding, in Clip Composer's own callback — exit 1, as expected |
| Docs path audit | `python3 tools/docs_path_audit.py --root .` | `Docs-path audit passed: 7 documents checked.` — exit 0 |
| Version contract | `python3 tools/version_contract.py --check` | `SDK version contract is consistent: 0.9.1` — exit 0 |
| ShmUI manifest | `python3 tools/shmui_juce_manifest.py --check` | `ShmUI-JUCE manifest is consistent: 77 files, sha256 4d2d4c5ba…9645d7` — exit 0 |
| Suite manifest | `python3 tools/suite.py validate --json` | `"errors": []`, `"valid": true` — exit 0 |
| CTest inventory | `ctest --test-dir build -N` | `Total Tests: 85` |
| Targeted gates | `ctest --test-dir build --output-on-failure -R '^realtime_static_audit$|^docs_path_audit$|^multi_clip_stress_test$|^transport_render_hash_test$|^performance_monitor_test$'` | 5/5 passed, 0 failed, 54.91 s total |
| Realtime harness | `ctest --test-dir build --output-on-failure -R '^realtime_harness_test$'` | 1/1 passed, 0 failed, 87.74 s total |
| Performance tool | `./build/tools/perf/orpheus_perf_render_click` | 44100 Hz → 10.185 ms; 48000 Hz → 10.788 ms; 96000 Hz → 24.190 ms — exit 0 |
| Minhost render | `./build/adapters/minhost/orpheus_minhost render-click --session tools/fixtures/solo_click.json --out /tmp/orp273_probe.wav --range 0:8` | Rendered; exit 0 |
| Determinism smoke | `scripts/check-determinism.sh` | `✓ Determinism check completed`; SHA-256 `1a602fe4e879ec011bdf2b9ce25e0f2ad7f577d62c2c8ee937dc6bc38319d905` — exit 0 |
| Budget validator | `node scripts/validate-performance.js` | `✅ All performance budgets met!` — exit 0, **but** the only configured bundle was skipped and no latency or frequency budget was measured; see F-07 |
| Type sizes | throwaway program compiled against `include/` with `-std=c++20` | `AudioMeter=16`, `GroupOutputMeterFrame=524`, `GroupOutputMeterSnapshot=16792`, `RoutingMeterTelemetry=17936`, `RealtimeTelemetrySnapshot=18808` |

### 8.2 Limitations

- **No release-representative measurement was taken.** Every number in this
  record comes from the `build/` tree, which is Debug with AddressSanitizer and
  UndefinedBehaviorSanitizer enabled. `build-release/` was not usable as a
  baseline: it contains no built binaries and is configured with
  `CMAKE_BUILD_TYPE=Release` *and* `ORP_ENABLE_ASAN=ON`/`ORP_ENABLE_UBSAN=ON`.
  Consequently **no finding in this record claims a millisecond saving, a
  percentage improvement, or a callback-deadline headroom gain.** Every cost
  mechanism is stated as an operation count against a stated multiplicity
  bound.
- **The full 85-test suite was not run.** The targeted gates above were run
  instead, because the host's full build and full-suite cost is outside the
  budget of a documentation-only review. A gate is claimed as passing only where
  it was actually run in this session.
- **No per-operation latency was measured.** Statements about the cost of a
  `std::sin`, a `std::log10`, a `std::pow`, a futex wake, or a memcpy are
  labelled `[INFERENCE]` and are not used to support a severity rating; the
  ratings are driven by contract violations, multiplicity, and structural
  consequence instead.
- **No Windows, Linux, or physical-device evidence was gathered.** The WASAPI
  and ASIO sections are source-level observations only, and no Windows-hosted
  check is claimed.
- **The `perf_render_click` figures are not callback measurements.** The timed
  interval is the whole `render->render_click()` call, which includes the
  offline render of four bars and the WAV file write
  (`tools/perf/perf_render_click.cpp:34-58`). They must not be read as
  per-block or per-callback timings.
- **The budget validator's exit code is not performance evidence**, for the
  reasons given in F-07.
- **The determinism smoke check is single-platform.** `scripts/check-determinism.sh`
  produced one SHA-256 on this host. The script's own output states that
  cross-platform validation requires running it on other platforms; F-02's
  cross-platform claim rests on the repository's own test comment, not on a
  cross-platform run performed here.
- **The findings are source-grounded, not measurement-grounded.** They identify
  where cost and structure are concentrated and how to bound them; they do not
  rank the findings by measured CPU impact, because no such measurement was
  possible in this session.

---

## 9. References

External factual claims in this record are cited in IEEE style. Repo-observed
claims use `path:line` anchors and need no external citation.

[1] J. L. Hennessy and D. A. Patterson, *Computer Architecture: A Quantitative
Approach*, 6th ed. Cambridge, MA, USA: MIT Press, 2019, ch. 3 (instruction-level
parallelism and the cost of transcendental operations) and ch. 6 (memory
hierarchy, cache capacity and the cost of streaming a working set larger than
the last-level cache).

[2] M. D. Hill and V. J. Reddi, "Accelerator-Level Scalability," in *ACM
Queue*, vol. 19, no. 6, pp. 1–6, 2011, doi: 10.1145/3454124.3454131. (Cited
only for the general statement that a working set larger than the cache
hierarchy's level-1 data cache degrades throughput; the per-block byte counts in
§4 are counted directly from the source, not taken from this reference.)

[3] ISO/IEC 9899:2018, *Programming languages — C*, ISO, Geneva, Switzerland,
2018, §7.12.4 (`sin`, `cos`, `log10`, `pow` are not required to be correctly
rounded, so their results may differ between implementations). (Cited for the
statement that the EqualPower fade's `std::sin` result is implementation-
dependent, which is the premise of F-02.)

[4] IEEE Standard for Floating-Point Arithmetic (IEEE 754-2019), IEEE,
Piscataway, NJ, USA, 2019, §9.2.1 and §11. (Cited for the statement that a
deterministic, reproducible numeric result requires a pinned evaluation order
and a pinned implementation, not merely a fixed expression.)

[5] `docs/REALTIME_AUDIT.md` — the repository's realtime contract; see
§Callback Rules (`docs/REALTIME_AUDIT.md:3-45`), §Current Gates
(`docs/REALTIME_AUDIT.md:365-374`), §Known Architecture Debt
(`docs/REALTIME_AUDIT.md:376-384`) and §Platform Matrix
(`docs/REALTIME_AUDIT.md:386-400`).

[6] `AGENTS.md` — the repository's agent guide; see §Realtime rules, §Public API
and package rules, §Session, media, and workflow invariants, and §Deferred work.

[7] `ARCHITECTURE.md` — the repository's architecture document; see §Design
Principles (`ARCHITECTURE.md:36-48`), §Temporal Coherence
(`ARCHITECTURE.md:50-90`), §Core Library (`ARCHITECTURE.md:210-311`) and
§Threading Model (`ARCHITECTURE.md:538-621`).

[8] `docs/orp/ORP104 Codebase Optimization and Safety Audit.md` (2025-11-09),
`docs/orp/ORP116 Bloat Remediation Sprint.md`,
`docs/orp/archive/ORP121 Audio Backend Refactoring Master Plan.md` (2026-01-18),
`docs/orp/ORP138 Modernization Audit.md` (Nov 2025),
`docs/orp/ORP263 SDK Resilience, Responsiveness, and Architecture Review.md`
(2026-09-25), `docs/orp/ORP264 Phase A Containment Completion.md` (2026-09-26),
`docs/orp/ORP266 DeepWiki Coherence Audit.md` (2026-09-29),
`docs/orp/ORP267 DeepWiki Implementation Plan.md` (2026-09-29),
`docs/orp/ORP268 Deferred Audit Remediation Requirements.md` (2026-09-29),
`docs/orp/ORP134 NEXT Sprint - Streaming Reader and Platform Primitives.md`,
`docs/orp/ORP135 Minhost Adapter Decomposition.md`, and
`docs/orp/ORP143 Reliability and Adoption Sprint Completion and Child-App
Handoff.md`.

---

**Record type:** Local historical provenance. This file lives under `docs/orp/`,
which `.gitignore:102` excludes from the checkout, so it is **not** public
product documentation and is not part of any public documentation link set. It
records a review and its recommendations; it does not claim that any
recommendation has been implemented.
