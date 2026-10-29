# Progress

## Treefall SDK coherence audit and suite roadmap — 2026-10-29

**Record:** `docs/orp/ORP277 Treefall SDK Coherence Audit, Downstream Suite Sync, and Strategic Roadmap.md`
**Scope:** Post-PR #277 SDK core (`901fc1d33`), Orpheus Suite (`shmui`, `fourtrack`, `freqfinder`, `clip-composer`), and open requirements register.

Conducted a technical audit of codebase coherence, deferred objectives, and downstream synchronization. PR #277 was verified merged at `901fc1d33` via GitHub CLI (`gh pr view 277`). Baseline benchmark metrics cited from ORP274/ORP275 record the remediation of all 20 findings from ORP273, pinned elementary math (`0x212b8a01988459a9`), 4096-frame CoreAudio ceiling, and 0 audio allocations with TruePeak p99 deadline ratio 0.485 (10,352.7 µs vs 21,333.3 µs ceiling). An immediate refactoring/optimization pass is rejected as premature churn prior to downstream adoption.

Key deferred objectives audited: ORP268 Requirement A (critical: transport clock-domain, host timestamp, and discontinuity propagation across `AudioProcessBlock` -> `ITransportController::processAudio` -> `RealtimeTelemetrySnapshot` schema v4); ORP268 Requirement B (high: sized C ABI table handshake in `include/orpheus/abi.h`); ORP276 (ShmUI private source compartmentalization handoff); ORP143 §7 (Windows WASAPI physical hardware acceptance gating); and live verification of PRs #247/#250 (OPEN) and #260 (CLOSED).

Suite status audit (`tools/suite.py status`) and repo-level inspection reveal that downstream applications are drifted or uncommitted: `fourtrack` and `clip-composer` have local branches advancing SDK to `901fc1d33`, `freqfinder` is dirty with a CMake override, and `shmui` is 2 commits ahead of `origin/main` with unpushed private source boundary commits (`219a81a`, `e8c8f1e`). Established a 4-horizon strategic roadmap prioritizing Horizon 1 downstream suite synchronization, Horizon 2 realtime temporal coherence plumbing, Horizon 3 ShmUI compartmentalization, and Horizon 4 WASAPI hardware qualification and stable 1.0 promotion.

## ORP273 consumer-safety review — 2026-10-23

**Records:** ORP275 (review/verification) and ORP276 (ShmUI handoff).
**Branch:** `feat/orp273-sprint-remediation`; merge requires clean hosted CI.

Resolved floating-point contraction drift, extreme-gain exponent conversion,
block-smoother lost updates, inactive-lane replay through direct host route edits,
worker scratch churn, exact-beat rollback/grid dirtiness, failed-batch allocator
state, pre-allocation parser budgets, bounded file reads and save/load limit
symmetry. Isolated allocation hooks, required an actual measured CTest result,
forwarded its Release configuration and separated installed-fixture DLL outputs.
Independent realtime and math/CI reviewers report no remaining code blockers.

Focused Release and ASan/UBSan tests and installed-package/ABI consumers pass.
A real minhost render produces the expected 4-second stereo PCM16 WAV.
The isolated Release deadline gate passes with unchanged ratio 0.5; TruePeak
wall p99 is 10,352.7 microseconds against a 21,333.3 ceiling, with zero guarded
allocations/deallocations. The earlier concurrent-build timing failure remains
recorded in ORP275. The shared scenario golden is `0x212b8a01988459a9`; hosted
cross-platform parity remains required. The first review CI exposed the soft
clipper's remaining `tanh` dependency and x86 deadline misses. The knee is now
pinned without disabling clipping; x86 reductions are vectorized, and VS 2022
uses non-contracting `/fp:precise` without unnecessary floating-environment trap
overhead. ORP275 records failed CI and subsequent scoped accuracy/runtime proof.

No consumer repository, SDK pin or governed ShmUI import changed. All affected
C++ consumers need matching-header rebuilds. Default ShmUI-off does not hide
source files present in the SDK; ORP276 defines the separate upstream handoff.
The 2026-10-22 section below is historical and its cross-platform claims are
superseded by ORP275.

## Refactoring and performance remediation sprint — 2026-10-22

**Branch:** `feat/orp273-sprint-remediation`
**Record:** `docs/orp/ORP274 Refactoring and Performance Remediation Sprint Delivery.md`
**Status:** Complete with recorded deviations; 20 of 20 ORP273 findings landed.

The sprint remediated every finding ORP273 recorded, in ORP273 §5's wave
order: Wave 0 (measurement hygiene) → Wave 1 (realtime correctness) →
Wave 2 (render-path redundancy) → Wave 3 (structural decomposition) →
Wave 4 (tooling, calibration and documentation). Twenty-four commits, one
focused commit per finding.

The two realtime-path defects are closed: the CoreAudio device callback no
longer notifies a waiter (F-01), and every CoreAudio route now has one
explicit callback-frame ceiling, enforced before any buffer is allocated
(F-11). The render path no longer depends on the host libm (F-02), so the
render-hash golden is pinned as a constant and the rendered bytes are
reproducible across platforms.

The release-tree measurement is the sprint's headline number. Against the
base revision `ad62b0f8c`, measured as an unsanitized Release build with the
same harness shape and the same host, the p99 of the worst-case render block
fell from 13,885.4 µs to 12,883.1 µs on the TruePeak routing mix and from
4,258.42 µs to 3,644.54 µs on the 32-voice transport mix. The worst p99
ratio against the 42,666.7 µs frame deadline fell from 0.325 to 0.302,
below the gate's 0.5 ceiling, so the threshold was not moved.

Verification: the repository gates are green, the full 86-test Debug suite
passes, the release tree passes the registered
`realtime_render_deadline_distribution` gate, the budget validator reports
`✅ All performance budgets met!` against the release tree, the determinism
script and a real offline render smoke both pass, and the render hash is
pinned and asserted.

Evidence limits: the measurements come from one arm64 macOS host; the
cross-platform golden proof is the Windows and Ubuntu Release CI legs; the
pinned mathematics' `std::fma` cost on a baseline x86-64 target was not
measured; and the routing matrix's per-entity meters, the post-master
output-lane true-peak path and F-20's slot-array variant remain deferred.
The deviations are recorded in ORP274 §6 and the evidence limits in §7.

Downstream: `include/orpheus/clip_dsp.h`, `include/orpheus/json.hpp` and
`include/orpheus/session_graph.h` changed, so Clip Composer needs a rebuild
and FreqFinder a package re-consumption. Any artefact that pins a rendered-
audio hash must be regenerated: the scenario hash moves from
`0xfce554ccf7c66ab9` to `0x2885ce8fbb52d42d`. No consumer repository was
modified.

## Refactoring and performance optimization review — 2026-10-21

**Branch:** `docs/orp273-refactoring-performance-review`
**Record:** `docs/orp/ORP273 Refactoring and Performance Optimization Review.md`
**Status:** Review record complete; no SDK source, test, build, or package
change was made.

The review covers the C++20 SDK core at `0325c6400` (branch `main`):
`src/`, `include/`, `adapters/`, `tools/`, `tests/`, and `examples/`.
`packages/shmui-juce/` is a governed import pinned at upstream revision
`d420ec64dcb5fd16baa0b95d5048ee09bb95eee8` and is excluded, as are `apps/`
and the child-application repositories.

It produced 20 ranked findings, each with `path:line` anchors, an explicit
multiplicity bound, a concrete file-level recommendation, a blast-radius
classification, and a downstream-impact statement for Clip Composer and
FreqFinder. Two are rated Critical as released-contract violations: the
CoreAudio device callback reaches `std::atomic::notify_one()` on the route
monitor, and the EqualPower fade path's per-sample `std::sin` is the
documented reason the render-hash golden is not pinned, which leaves the
determinism principle unmet for that curve. The remainder concentrate
memory traffic proportional to configured capacity rather than to active
work in the transport and routing render path, plus measurement-hygiene
gaps in the budget validator and the sanitizer CI timing thresholds.

Every candidate was searched against ORP104, ORP116, ORP121, ORP138,
ORP263 (F-01 to F-09, R-01 to R-07 and the design-debt list), ORP266 (all
16 rows), ORP267, ORP268, ORP134, ORP135, and
`docs/REALTIME_AUDIT.md` §Known Architecture Debt. Items already covered
are dispositioned in the record's reconciliation table and are not re-raised
as findings: the gain-conversion `std::pow` is already mitigated by
control-side precomputation, ORP263 F-01 to F-05 are contained by ORP264,
ORP266 row 7's unenforced budgets and rows 12/13/17's static-audit limits
remain open in their own record, and the minhost split is already owned by
ORP135.

### Verification

- Baseline confirmed before any work: clean worktree, HEAD
  `0325c6400a37be9926b4b6dae532885417cabe9b`, `main` tracking
  `origin/main`.
- `python3 tools/realtime_audit.py --root . --fail-known-debt` — passed, 0
  hard failures and 0 tracked debt findings.
- `python3 tools/realtime_audit.py --root . --fail-known-debt
  --include-adjacent` — 1 tracked debt finding, in Clip Composer's own
  callback; out of scope for this repository.
- `python3 tools/docs_path_audit.py --root .` — passed, 7 documents
  checked.
- `python3 tools/version_contract.py --check` — passed, 0.9.1.
- `python3 tools/shmui_juce_manifest.py --check` — passed, 77 files,
  sha256 `4d2d4c5ba354b52fbeb0c6c59b44bb15bac90e4b65e3aa5c3bb096881a9645d7`.
- `python3 tools/suite.py validate --json` — `"errors": []`.
- Targeted CTest only: `realtime_static_audit`, `docs_path_audit`,
  `multi_clip_stress_test`, `transport_render_hash_test` and
  `performance_monitor_test` passed (5/5, 54.91 s), and
  `realtime_harness_test` passed (1/1, 87.74 s). The full 85-test suite was
  deliberately not run.
- `./build/tools/perf/orpheus_perf_render_click` — 44100 Hz 10.185 ms,
  48000 Hz 10.788 ms, 96000 Hz 24.190 ms.
- `./build/adapters/minhost/orpheus_minhost render-click --session
  tools/fixtures/solo_click.json --out /tmp/orp273_probe.wav --range 0:8` —
  rendered successfully.
- `scripts/check-determinism.sh` — passed; SHA-256
  `1a602fe4e879ec011bdf2b9ce25e0f2ad7f577d62c2c8ee937dc6bc38319d905`.

### Evidence limits

Every measurement was taken on the existing `build/` tree, which is Debug
with AddressSanitizer and UndefinedBehaviorSanitizer enabled; it is **not**
release-representative, and no release baseline was configured. No finding
claims a millisecond saving, a percentage improvement, or callback-deadline
headroom: every cost is stated as an operation count against a stated
multiplicity bound, and per-operation latencies are labelled `[INFERENCE]`.
`node scripts/validate-performance.js` exits 0 and prints "All performance
budgets met!", but the only configured bundle was skipped and neither the
command-latency nor the event-frequency budgets was measured, so that exit
code is not performance evidence. The `perf_render_click` figures time an
end-to-end offline render that includes the WAV file write, not a callback.
No Windows, Linux, or physical-device evidence was gathered. All 255
`path:line` anchors in the record were re-verified to resolve against the
committed tree.

## Clip Composer SDK pin in the suite manifest — 2026-10-19

**Branch:** `chore/suite-clip-composer-pin-20261019`
**Status:** Manifest and prose updated; no release promotion and no
support-tier change.

The suite manifest now records Clip Composer's SDK gitlink at
`f37503b23f72d7c0bd78ff1276be3c5d4817cee8`, the SDK `main` merge commit that
carries the transport initial-metadata registration path (PR #270). The
previous entry named `56a81fcdff445b43d1ba4d9b6684efbbffd11e32` and was
already stale: Clip Composer had been pinned to an unmerged SDK branch commit
since the registration work was authored outside this repository. FourTrack's
pin is unchanged at `56a81fcdff445b43d1ba4d9b6684efbbffd11e32`; that
repository is out of scope here and its pin stays an exact, reachable
revision. `manifest_revision` moves to `2026-10-19`.

## Transport initial metadata registration — 2026-10-19

**Branch:** `feat/transport-initial-metadata-registration-main`
**Pull request:** [#270](https://github.com/boot-industries/treefall-sdk/pull/270)
**Record:** `docs/orp/ORP270 Transport Initial Metadata Registration Delivery.md`
**Status:** Implementation complete; evidence limitations are recorded below.

`ITransportController::registerClipAudioWithMetadata()` registers a clip
source and its complete initial `ClipMetadata` in one control-thread
operation. The source and the metadata are committed together, the operation
consumes no bounded render-command node, and invalid metadata leaves the
handle unregistered. The virtual is appended at the end of
`ITransportController`, so no existing virtual slot is renumbered and the
default implementation keeps custom implementations source-compatible. The
extension is C++-only: the stable C ABI 1.0 tables and layouts are unchanged
and the SDK version stays 0.9.1. C++ consumers and subclasses must be
rebuilt; the installed clean-prefix legacy fixtures implement the interface
without overriding the new method and still compile and link.

The change was authored outside this repository and re-created here by
cherry-pick. `git patch-id --stable` gives
`2499545c31e8001f6e4185ab3986ccfc476c1f30` for both this branch's transport
commit and the source commit, so the committed patch is identical.

### Verification

- Local Debug build on macOS (AppleClang 21.0.0, CMake 4.1.0) with
  `-DORPHEUS_ENABLE_REALTIME=ON`: configure and build exit 0.
- Full configured CTest: 84/84 passed, 0 failed (430.99 s).
- `cmake_find_package` (22.42 s) passed with the new method and its new
  failure codes in the installed clean-prefix consumer; `clip_metadata_test`
  passed with the new transport case.
- `tools/realtime_audit.py --fail-known-debt`: 0 hard failures, 0 tracked debt
  findings. `tools/docs_path_audit.py --root .`: passed, 7 documents checked.
  `tools/version_contract.py --check`: consistent at 0.9.1.
  `tools/shmui_juce_manifest.py --check`: consistent, 58 files, sha256
  `c258caee7eb1c639e2fe7aa654b926aa216d5232149aad52423f897c91167655`.
- `git diff --check`: clean. The `src` and `include` trees are clean under the
  pipeline's `clang-format-14` lint gate.

The hosted pipeline for this pull request runs the same gates on macOS,
Ubuntu, and Windows-2022; its observed result is recorded on the pull request.

### Evidence limits

No release promotion, no version bump, and no support-tier change. No
Windows, WASAPI, or real-device evidence is claimed: WASAPI remains
unpromoted source and fake-test code, and the open WASAPI
platform-qualification pull request is not merged by this delivery.

## Floating development remote tracking and coordinator repair — 2026-10-14

Completed the SDK coordinator repair on `fix/suite-checkpoint-provenance`.
Development now has a null snapshot pointer and status/doctor follow each
repository's declared remote-tracking default branch. Missing tracking refs are
reported unavailable rather than replaced with local `HEAD`; local/remote
revision mismatch is reported as drift. Floating source-workspace dependency
pins follow the dependency's declared tracking branch, while git-submodule pins
remain exact declared revisions. Explicit snapshots and candidate/stable
channels remain pinned. Snapshot observation leaves the floating pointer null
and preserves all existing historical snapshot objects.

Updated the SDK ShmUI-JUCE generated-artifact expectation from the actual
imported package: source revision
`1f2c87a089d975eec1396360173da87a926835ed`, content hash
`c258caee7eb1c639e2fe7aa654b926aa216d5232149aad52423f897c91167655`, token
contract `0.6.0`. The suite manifest records explicit downstream SDK gitlink
expectations at `56a81fcdff445b43d1ba4d9b6684efbbffd11e32` and SDK version
`0.9.1`. No historical snapshot or child-application source was changed.

Added behavior coverage for floating branch drift/convergence, immutable
candidate and explicit snapshots, unavailable remote-tracking refs,
unconfigured pinned channels, snapshot observation immutability, and floating
source-workspace pins versus exact submodule pins. The decision and safe status
recipe are in `suite/README.md` and
`docs/orp/ORP269 Suite Floating Development Coordinator Repair.md`. The schema
already supports floating channels with a null snapshot; no schema change or
version bump is needed.

Parent verification passed: all 13 coordinator behavior cases, manifest/schema
validation, governed import check and actual-workspace doctor smoke (healthy,
zero errors). Full configured SDK CTest passed 84/84, zero failures (233.84 s).
Parent corrected the floating status classification and remaining live
canonical/Swift provenance declarations before those final results.
Mainline repair commit: `8f4115de6`. No release, hardware, or FourTrack native
runtime qualification is claimed.

## Governed meter and shared-source streaming integration — 2026-10-14

Integrated on `fix/shared-stream-governed-integration`, based on SDK mainline
`3ca1fa06e`, retaining the merged initial-loop-fade correction. The historical
`95d5e3` nested checkpoint was reviewed, not adopted wholesale.

Streaming voices publish bounded four-buffer coherent demand snapshots.
Six steady pages remain shared per decoder; independent old/fresh cursors
retain current/look-ahead pages, with transient copy pins excluding retirement
and reuse. Worker refill remains current-first and source-fair. Four command
pages, command rollback, loop-anchor ownership and observable misses remain.
Matching C++ consumers must rebuild; no virtual extension or stable C ABI
layout/export change is introduced.

The public installed-package 40-second streaming smoke passed: one source,
two overlapping voices, exactly one Started settlement for each start/refire,
zero underruns before refire and after two seconds of paced sustain.
Measured RMS: initial 0.210766, distant 0.210034, overlap 0.204558,
sustain 0.190817. Nine focused SDK suites passed in 39.25 seconds, including
streaming demand coherence, capacity/miss/idle recovery, transient pins,
command-pool reuse, voice removal, fade controls and realtime render checks.

Governed ShmUI inputs are committed at
`1f2c87a`; token contract remains `0.6.0`, OpenGL remains opt-in.
Upstream synchronization and SDK manifest checks passed, with 58 files and hash
`c258caee7eb1c639e2fe7aa654b926aa216d5232149aad52423f897c91167655`.
The authoritative native fixture passed rendering, transitions, physical-scale,
interval-peak and paint-safe callback coverage. Two matched 60-second native
workload pairs retained repaint throttling and chrome/table/ladder reuse,
with zero active cache rebuilds and zero hidden/settled paints.
Observed rates remained approximately 24 Hz despite the 33 ms cap;
this is not a zero-miss 30-fps or audio-to-photon qualification.
SHM026 records CPU, observation-age, missed-slot and bounded resource tradeoffs.

Full configured SDK build and CTest passed: 84/84 gates, zero failed,
223.70 seconds. The generated native fixture directory
`build/apps/wave-finder/shmui-juce` passed SHM026 separately in 1.22 seconds.
The public streaming consumer was relinked against the final installed library
and passed again with the source/voice/settlement/underrun/RMS results above.
ORP261 records the current governed qualification and application handoff.

Clip Composer adoption is delegated to its application agent: no application
sources, gitlink or build were changed for this integration. Hardware/acoustic
and non-macOS acceptance remain outside this local verification;
admission/readiness and the two-voice policy are unchanged.

## Clip Composer SDK loop fade-in resolution — 2026-10-13

Applied the targeted transport envelope correction on
`fix/main-initial-loop-fade-in`, based on local `main` at `ff45db0c1`.
Loop mode no longer bypasses the initial enabled fade-in. Existing
`hasLoopedOnce` state suppresses it after automatic wraps and resets on explicit
start/refire; no new clock, allocation, public signature or ABI layout is added.
Natural fade-out remains non-loop-only; operator Stop remains independently
configured. No Clip Composer sources, pins, voice admission/readiness policy,
ShmUI import metadata or unrelated stacked-branch changes were modified.

Changed `src/core/transport/transport_controller.cpp` and added three rendered
regressions in `tests/transport/clip_controls_test.cpp`: all three fade curves
with and without looping, repeated wraps/refire/Stop/fresh start, and OFF.
Public usage guidance and the unreleased changelog now describe this behavior.

Observed verification:

- `cmake --build build-orp263 --parallel 8` passed.
- Ten selected CTest gates passed, zero failures, in 24.75 s: clip controls,
  fade processing, loop, restart, stop envelope, voice mode, strict realtime
  audit, audit unit tests, documentation paths and clean-prefix package.
- Cue-point playback, realtime harness, deterministic render hashes and sample
  rate conversion passed four additional CTest gates in 85.93 s.
- A separate temporary consumer compiled and ran against the relocated
  installed package using only public headers and `Orpheus::` targets.
  With 48 kHz prepared PCM, nonzero trim-in and a 1,024-frame callback crossing
  a 512-frame loop boundary: initial/refire midpoint = 0.125, wrap = 0.25,
  operator Stop midpoint = 0.125 and end = 0, OFF start/wrap = 0.25,
  non-loop natural OUT midpoint = 0.125 and final source frame = 0.00104167.
  The first smoke link lacked the installed Debug libraries' sanitizer runtime;
  matching ASan/UBSan compiler/link flags resolved it and the executable passed.
- The temporary consumer source/build/media were removed. Documentation paths
  passed again after the public documentation updates (1.30 s).

Hardware/acoustic qualification was not performed. The reported intermittent
underrun origin remains unknown; no refusal suppression or safety-policy change
is included. The reported import-manifest mismatch was not investigated.
Remote publication/mainline merge remains separate; no push was requested.

## Upstream meter publication and presentation fix — 2026-09-10

Corrected the actual number flow, rather than requiring downstream timer
workarounds. Default telemetry now publishes each completed callback instead
of every eighth. Governed ShmUI source
`5502863957ce7d59f04a76b3808eac8b187f40f8` late-latches meter pairs in `paint()`,
uses display synchronization instead of an independent meter timer, and avoids
unchanged/hidden repaint work. Waveform playhead invalidation no longer waits
for an additional throttle timer. Import hash:
`d8a542e85681c7475e8f0d217b1d00be1025d8dbf88013c9a96fa2aa39ef6f0e`.

Both regressions were observed before fixing: the first rendered interval
returned no telemetry, and the first actual meter paint showed stale pixels.
Both pass after fixing. Clip indicators latch in the current paint; bounded
notifications preserve detection time and run outside the paint stack so user
callbacks can safely destroy the component. Monotonic elapsed-time ballistics
remove refresh-rate dependence and steady-peak needle oscillation.

Measured on this M2 workstation:

- Eight channels/eight prepared voices, 44.1 kHz, 512 frames, Debug SDK:
  first publication moved from callback 8 to callback 1, removing seven
  withheld intervals (81.270 ms). Interval durations are not photon latency.
- Across 1,024 timed callbacks after warmup: old-cadence median 1,621.583 us;
  default-per-block median 1,630.833 us, p99 2,027.125 us, max 2,247.958 us,
  against an 11,609.977 us callback deadline. Audio PCM hash was identical
  (`f64a6980912cc383`), with zero guarded C++ allocation/deallocation violations.
- Native eight-meter smoke: prior component performed 336 unchanged paints in
  about one second (128.961 ms process CPU). Final corrected smoke performed
  zero unchanged paints (24.913 ms process CPU), eight paints after updating all
  eight levels, and zero hidden paints. Harness/event-loop CPU is included;
  this is not a sustained thermal qualification.
- ShmUI's four native boundary fixtures passed, including immediate-pixel and
  paint-safe destruction regressions. SDK multichannel and diagnostics tests
  passed 10/10 each. Realtime harness passed 11 cases with the Linux `/proc`
  file-I/O case skipped on macOS.
- The release deadline fixture now distinguishes DSP execution cost from shared
  runner preemption on macOS and Linux by enforcing maximum per-thread CPU plus
  wall-clock p99 against the callback deadline; Windows retains strict
  wall-clock maximum because `GetThreadTimes` advances at a 15.625 ms quantum on
  the hosted runner. Every platform still reports wall-clock maximum. The local
  macOS Release check passed sample-peak at 4,922 us maximum thread CPU /
  2,500.29 us wall p99 and true-peak at 3,340 us maximum thread CPU /
  3,351.38 us wall p99, against a 10,666.67 us deadline.

Final SDK verification: `cmake --build build --parallel 6` and the complete
configured CTest suite passed **82/82** in **205.13 s**, including clean-prefix
package consumers, strict realtime audit, governed import and native ShmUI
fixtures. The temporary rendering/flow executables, source trees and compiler
database link were removed after recording their results.

The existing event FIFO/drop semantics and interval-peak retention are unchanged.
No downstream application source or SDK pin was modified during this correction.
Existing application binaries must be rebuilt against the corrected upstream
headers/libraries; no universal end-to-end zero-latency claim is made.

## Suite synchronization and temporal correctness — 2026-09-10

Shared change `ORP-SUITE-20260910-001` uses published runtime baseline
`ca949b2e0af63346d92bb6f2a51ac3c746c2745b` and governed ShmUI source
`f41fbd3fe4326a37d025a6c97a74c84cfa6ea2b6`, token contract `0.6.0`.
The existing SDK mirror passes unchanged. Consumer pins/provenance and suite
remote/hash metadata are synchronized on local working branches; the original
dirty Clip Composer checkout is preserved, with its update isolated.
Publication, original Clip Composer integration and a new reachable suite
snapshot remain separate from this local handoff.

Observed verification:

- SDK clean-prefix package, realtime audit, docs audit, ShmUI manifest and suite
  manifest gates: 5/5 passed (11.84 s).
- Suite quick checks: 5/5 passed. ShmUI token/Swift consumer and registry closure
  passed; registry closure covered 56 items and 66 files.
- FourTrack: exact Swift provenance passed, SwiftUI/bridge/core rebuilt,
  333/333 CTest cases passed (17.61 s). Mock CLI four/eight-track record/bounce
  smoke each produced 2,048 samples at 48 kHz.
- FreqFinder: Release Standalone/AU/VST3 built and CTest passed 1/1 (1.66 s);
  Debug CTest passed 1/1 (12.15 s). Debug VST3 manifest helper still fails
  because ASan is loaded too late, including with the suggested runtime
  environment on the outer build.
- Clip Composer's isolated full build and CTest passed 747 tests, zero failed,
  one intentional CPU-performance skip (748 entries, 335.56 s). Its original
  staged/unstaged meter checkout remains untouched and not integrated.

Reconnaissance and digital-metering research are recorded in ORP255 §9.
The canonical public `ARCHITECTURE.md` temporal contract and all five active
repository guides now make phase integrity, continuously coherent visuals and
demand-driven resource use joint release gates. Existing sample-accurate locks,
`AudioProcessBlock` timestamps, and directional latency validity are foundations
to extend, not replace.

A compiled telemetry experiment confirmed first publication at callback 8
(85.333 ms of 48 kHz/512-frame audio), 64 queued snapshots and one drop after
520 callbacks without a consumer. Per-block publication already works
(10.667 ms); the current snapshot occupies 18,808 bytes on this arm64 build.
This demonstrates publication behavior, not physical screen latency.
No timing implementation, audible-to-photon measurement, thermal qualification,
Windows backend promotion, or phase-coherence regression claim is made here.

## Windows CI baseline repair — 2026-09-02

**Status:** Implementation committed on `fix/windows-ci-baseline`; hosted rerun
is required for final platform evidence.

- Ordered `windows.h` before the property-key macro header and BCrypt/WASAPI
  SDK headers. This fixes Windows SDK type/macro prerequisites without changing
  runtime behavior.
- Stopped propagating `ORPHEUS_USING_DLL` through `Orpheus::core`: the ABI
  libraries may be shared, while realtime/runtime libraries remain static.
  Static factories were previously declared `dllimport`, causing MSVC
  `__imp_createRoutingMatrix` and `__imp_createTransportController` failures
  in session tests.
- Focused local build and CTest passed for `media_integrity_test`,
  `driver_manager_test`, `scene_manager_test`, and `scene_routing_test`.
- Hosted CI follow-up exposed two additional baseline defects: the WASAPI
  acceptance callback still used the retired callback signature, and the
  declared WASAPI factory had no definition. Both are corrected in the
  current branch.
- Windows shared-core test executables now stage the ABI DLLs beside each
  executable, preventing `0xc0000135` CTest failures after a successful build.
- WASAPI terminal failures now publish telemetry before clearing `running`, so
  a stopped driver cannot expose stale healthy status.
- Package consumers now map uninstalled Visual Studio configurations to the
  configuration actually installed and stage shared ABI DLLs beside fixtures.
- The standalone package runtime fixture also stages ABI DLLs beside its
  executable; this closes the remaining installed-consumer `0xc0000135` path.
- Cross-platform package gates now hash imported ShmUI text with canonical LF
  endings and an explicit POSIX path order, select the active multi-config
  install for compile-failure fixtures, and pass the active configuration to
  nested ShmUI CTest.
- Ubuntu's ShmUI package gate now installs the JUCE-required ALSA and libcurl
  development headers, and the consumer explicitly links the discovered CURL
  target required by JUCE's static core module. Nested producer and consumer
  builds now pass the selected multi-config explicitly before installation and
  execution. The Windows sndfile provider matrix clears the outer vcpkg
  toolchain, forces its fake package directory, uses Visual Studio's
  multi-config generator, builds with the selected configuration, and invokes
  the generated `.exe` from its configuration directory. The Windows CTest
  step now allows 15 minutes for the multi-config package gates.

## ORP257 — Host-neutral multichannel metering

**Date:** 2026-09-01
**Status:** Review remediation complete; final Debug suite 80/80, package, TSan, and release deadline gates passed; record renumbered to ORP257 to preserve the mainline ORP255 namespace.
- Integration remediation rebased this branch onto current `main`
  (`ddf0b2d3a8c9686e9b8c646c337acb736a6d21f2`), retained the mainline ORP254
  commercialization and ORP255 strategy records, resolved the Windows SDK
  include ordering, and renumbered the pending records to ORP256 and ORP257.


- SDK 0.9.0 adds schema-3 canonical routing telemetry, nested schema-1 logical
  group-output lanes, packed atomic route identity, finite-input sanitization,
  and true-peak silence/history rules.
- Route updates now serialize through an odd/even publication sequence. Audio
  slices accept topology only when matching-even before/after observations and
  the route generation agree; reinitialization clears the rendered revision.
- Exact reference-signature contracts, non-silent maximum-topology allocation
  coverage, coherent-snapshot TSan bounds, and corrected ORP256 manifest
  provenance close the PR review findings.
- Follow-up checks passed: routing 58/58, realtime diagnostics 10/10,
  multichannel transport 10/10, transport controller 17/17,
  `cmake_find_package`, the non-silent allocation gate, and ThreadSanitizer
  `HammerQueriesUnderConcurrentRender` (984 ms, no warning). Final release
  maxima were 5236.5 us sample peak (p99 5163.04 us, average 4710.97 us) and
  7111.5 us true peak (p99 6954.04 us, average 6575.28 us), below the 10666.7
  us budget. The full configured Debug CTest suite passed 80/80 in 341.21
  seconds.
- Windows portability follow-up: `media_integrity.cpp` and
  `driver_manager.cpp` now include `windows.h` before BCrypt and WASAPI SDK
  headers, ensuring their `NTSTATUS` and property-key definitions are
  available. These build-only corrections leave the public metering contract
  unchanged.
- **Scope boundary:** The remaining hosted Windows failures—WASAPI property-key
  declarations and session-test linkage—are broader platform-baseline work
  outside ORP257. Per delivery decision, this PR retains only the safe
  include-order corrections; platform repair is a separate task.




- Full record: [`ORP257 Host-Neutral Multichannel Metering Contract`](docs/orp/ORP257%20Host-Neutral%20Multichannel%20Metering%20Contract.md).

## ORP258 — Streaming prefetch realtime sustain remediation

**Date:** 2026-09-07
**Branch:** `fix/streaming-prefetch-realtime-sustain` (PR #256; historical implementation commits `994aa680`, `e1e5e255`, and `21360eb5`)
- **Implementation commit:** `8feafb21`.
**Status:** Transactional worker/page ownership and failure-atomic loop mutations implemented; affected Debug targets compiled. Runtime tests, downstream CI, and merge remain unrun by the approved plan.

- Replaced the generation-less fill atomics with a mutex-owned
  `CommandFillRequest` state machine, generation fence, cancellation
  acknowledgement, and separate late-resident/fresh-page masks.
- Added exact loop-anchor transitions, source-lifetime retention through queued
  commands, reverse-order unread-command rollback, source-qualified pending
  Start/Seek lease release, and queue-admission atomicity for trim/loop/metadata
  mutations.
- Added deterministic streaming ownership and queue-full regressions in
  `tests/transport/streaming_seek_test.cpp`.
- Record: [`ORP258 Streaming Prefetch Realtime Sustain Fix`](docs/orp/ORP258%20Streaming%20Prefetch%20Realtime%20Sustain%20Fix.md).
- Verification is compile-only in `build-orp256`; the downstream CTest,
  Release, sanitizer, and TSan gates are documented in ORP258 and remain
  unrun.

## ORP253 — CoreAudio output-only rate recovery

**Date:** 2026-08-31  
**Status:** Implementation committed at `1bbd1fd6b5d4df850ad41d3a69cc94b8ddd49b04`; SDK PR pending. Clip Composer pin advancement waits for the merged SDK `main` SHA.

- CoreAudio output-only requests now use `RequestExactRateOrConvert`, preserving
  explicit UID/map/rate/buffer transfer without changing the public request shape.
- Safe idle/settable built-in outputs plan the requested nominal-rate write;
  busy or non-settable outputs use bounded output SRC at the physical rate.
- Stream monitoring treats the verified device nominal rate as authoritative and
  ignores only stream-format sample-rate convergence; other layout changes remain
  terminal `FormatChanged`.
- Focused bridge, resolver, and monitor contracts passed. The configured 80-entry
  suite reproduced the documented 12 legacy default-device/two-input
  `InvalidParameter` cases in `coreaudio_driver_test`; no new failure occurred.
- Full record: [`ORP253 CoreAudio Output-Only Rate Recovery`](docs/orp/ORP253%20CoreAudio%20Output-Only%20Rate%20Recovery.md).

## ORP176 — CoreAudio Bluetooth duplex and directional SRC

**Date:** 2026-08-11
**Status:** Deterministic and package qualification complete; physical CLbuds acceptance remains blocked.

### Delivered

- CoreAudio now plans physical endpoint rates independently, applies only safe
  device-global writes, and performs bounded directional conversion at the I/O boundary.
- Route state reports physical/AUHAL client rates, virtual and callback widths,
  conversion latency, Bluetooth relationship, mono fallback, and callback health.
- Converted callback chunks use one session-frame base and advance the timeline only after
  the full callback, so multi-chunk delivery is contiguous.

### Evidence and boundary

- `polyphase_resampler_test`, `coreaudio_route_probe_test`, and `coreaudio_driver_test`
  passed after the callback-timeline review correction; `realtime_static_audit` and its
  unit contract also passed.
- The repository's complete configured debug suite (80/80) and three release/package
  contracts passed before review. No CLbuds endpoint is present locally, so no Bluetooth
  hardware acceptance, release tag, or downstream production pin is claimed.
- Full record: [`ORP176 CoreAudio Bluetooth Duplex and Directional SRC SDK Completion`](docs/orp/ORP176%20CoreAudio%20Bluetooth%20Duplex%20and%20Directional%20SRC%20SDK%20Completion.md).

## ORP174 — Cooperative CoreAudio rate negotiation

**Date:** 2026-08-08
**Status:** Implemented and merged as [PR #242](https://github.com/chrislyons/orpheus-sdk/pull/242) at
`498c02222f11a81f3dcc3e726d0355bdb20866ae`; downstream adoption deferred to FTR079.
**Pre-sprint SDK baseline:** `8333a04a47cd9c5f8a2dcd78fb185f6984b2069e`

### Delivered

- The public C++ runtime taxonomy is `AudioRouteRuntimeOutcome`; telemetry has
  only cumulative input-render failures and that outcome. The package is
  version `0.7.0`; the stable C ABI remains `1.0`.
- CoreAudio uses an injectable complete property API, a bounded listener-driven
  rate transaction with reverse rollback, scoped automatic hog-mode lifecycle,
  passive directional route monitoring, and a terminal admission latch.
- Render facts publish atomically before start. A callback validates complete
  hardware buffers, renders the full input span once, delivers contiguous
  client chunks without clamping, and copies all output frames.

### Evidence and boundary

- The configured `sdk-debug` tree built successfully and its 80-test CTest
  configuration passed, including installed package consumers and CoreAudio
  hardware-tagged coverage.
- Installed consumers passed current-minor acceptance, previous-minor
  rejection, exact telemetry assertions, retired enum rejection, and retired
  telemetry-field rejection.
- `PYTHONDONTWRITEBYTECODE=1 python3 tools/realtime_audit.py` passed with zero
  hard failures and zero tracked debt findings.
- No Windows WASAPI compile or hardware evidence is claimed. FTR079 retains
  FourTrack manual validation for built-in/USB routes, same-device duplex,
  distinct private aggregates, default-device changes, unsupported rates,
  external 44.1/48 kHz rate and buffer changes, permission denial, and
  disconnect/reconnect.
- Full details: [`ORP174 Cooperative CoreAudio Rate Negotiation Handoff`](docs/orp/ORP174%20Cooperative%20CoreAudio%20Rate%20Negotiation%20Handoff.md).

## ORP172 — Non-mutating CoreAudio route compatibility

**Date:** 2026-08-08  
**Status:** Implemented and merged as [PR #240](https://github.com/chrislyons/orpheus-sdk/pull/240) at
`33cd334151bec0e00a495bb4339845791917cb74`; handoff evidence merged as
[PR #241](https://github.com/chrislyons/orpheus-sdk/pull/241) at
`5d0d44aa4eb6c6e89f84f83050ed119399063092`.

### Delivered

- `IAudioDriver::probeRoute()` now reports an `AudioRouteCompatibility` result
  without mutating driver or CoreAudio state. The base implementation returns
  `BackendFailure`; CoreAudio and Dummy provide concrete probes.
- CoreAudio resolves active directional endpoints, validates maps and requested
  rates, and reports current-rate and `DeviceIsRunningSomewhere` facts through a
  read-only query path. A route probe never creates an aggregate or AudioUnit,
  writes a property, starts I/O, registers listeners, or requests TCC access.
- `PreserveDeviceRate` reports a supported rate mismatch as `Compatible` with
  mismatch flags; `RequestExactRate` reports it as
  `RequiresSampleRateChange`. Probe classification does not authorize rate
  writes or guarantee activation.

### Evidence and boundary

- Current local fast CTest suite passed 63/63 cases, including
  `coreaudio_route_probe_test`, `dummy_driver_test`, and
  `driver_manager_test`. Debug route targets built successfully.
- No physical-device or permission-denial claim is made. FTR078 retains
  transactional rate writes and rollback, passive monitoring, runtime taxonomy,
  and FourTrack adoption.
- Full details: [`ORP172 Non-Mutating CoreAudio Route Compatibility Handoff`](docs/orp/ORP172%20Non-Mutating%20CoreAudio%20Route%20Compatibility%20Handoff.md).

## ORP128 — CoreAudio runtime sample-rate resilience

**Date:** 2026-07-28; mainline reconciliation 2026-08-01
**Branch:** `feat/orp128-coreaudio-rate-resilience` (historical delivery branch)
**Status:** Implemented and merged as [PR #228](https://github.com/chrislyons/orpheus-sdk/pull/228) at `b7533e57b15bc37f581e4118f560b5e34bc60667`.

### Delivered

- Active CoreAudio routes now register nominal-sample-rate listeners for the AU
  route and every physical input/output endpoint.
- Listener notifications only close an atomic render gate and signal a control
  worker. The worker reasserts the configured rate outside the render callback.
- A refused reassertion or rate-query failure stops rendering and is exposed as
  `AudioIoTelemetry::runtime_outcome`; hosts must explicitly reinitialize.
- Explicit directional endpoint IDs remain immutable. The driver does not
  select a fallback device, rebuild an AudioUnit, resample, or invoke a host
  callback while a rate mismatch is pending.

### Evidence

- ASan/UBSan Debug focused CoreAudio suite passed 12 contracts, covering all
  deterministic monitor outcomes plus live playback-route startup,
  initialization, admitted-callback teardown, directional routes, aggregate
  capture, duplex capture, and capture-failure telemetry.
- Deterministic fake-property coverage proves listener registration/removal,
  no post-teardown callback, successful recovery, refused recovery, query
  failure, and rendering gate behavior.
- `tools/realtime_audit.py --root . --fail-known-debt` passed with zero hard
  failures and zero tracked-debt findings.

### Configured-suite observation

- At the ORP128 focused checkpoint, `docs_path_audit`,
  `cmake_shmui_package_consumer`, and `coreaudio_driver_test` did not pass.
- The eight missing documentation paths are corrected in this current record.
  [PR #229](https://github.com/chrislyons/orpheus-sdk/pull/229), merged at
  `30abdedeb5134976ad35382a159c168bb3178e54`, aligned all six installed
  ShmUI package-consumer profiles with the generated v0.5.0 token contract;
  its Release `cmake_shmui_package_consumer` CTest command exited successfully.
- On this host, 12 legacy CoreAudio cases that rely on the default
  two-input-channel configuration return `InvalidParameter`; the same run
  passed the focused output, directional, aggregate, and capture contracts
  listed above. This record does not treat the complete suite as green.

### Limitation

No controllable macOS device was available locally to record a live nominal
48 kHz → 44.1 kHz transition or rejected reassertion. The deterministic fake
covers both paths; no hardware recovery/refusal support claim is made.

## Realtime boundary remediation — 2026-08-02

**Branch:** `realtime-boundary-remediation-20260802`  
**Base:** `1854a6eb8be69469dcd2110aae4042fcb5fc1503`  
**Status:** In progress; authority documents restored and policy reconciled.

The remediation follows the audited nine-phase order in
`docs/tmp/realtime-boundary-audit-plan.md`. CoreAudio remains the only shipped
production device backend; WASAPI is unpromoted source/fake-test code; ASIO is
source-only; Linux exposes Dummy only; callback timing defaults OFF.


### Remediation evidence

- The deterministic fast suite passed 61/61 CTest cases with the capped
  four-job build/test configuration. `tools/realtime_audit.py --fail-known-debt`,
  `tools/docs_path_audit.py --root .`, and `git diff --check` passed.
- The extended unsanitized package/platform gates passed: provider matrix,
  invalid/non-native backend rejection, native backend disable, find-package,
  runtime consumer, previous-minor rejection, add-subdirectory, and the ShmUI
  package consumer. The disabled ABI-link entry remained disabled.
- The extended stress set passed 5/5 when run serially to avoid wall-clock
  contention: queue stress, voice-state liveness, realtime harness, streaming
  seek, and multiclip stress. Running the realtime harness concurrently with
  four stress processes can exceed its intentionally strict unsanitized timing
  budget; the serial evidence is the valid measurement.
- UBSan-only Debug passed 61/61 CTest cases. ASan evidence is unavailable on
  this AppleClang 17/macOS host: a minimal `-fsanitize=address,undefined`
  probe hangs in AddressSanitizer initialization before `main`. A genuine
  TSan voice-state build succeeds, but its executable exits 139 before emitting
  runtime diagnostics; no TSan claim is made.
- The deterministic CoreAudio selection/monitor/capture subset passed 13/13.
  The full physical CoreAudio target passed 25 tests, skipped one unavailable
  same-device-duplex case, and rejected 12 legacy default-device cases because
  this workstation has no default route matching their two-input request. No
  complete physical-hardware pass is claimed.
- Windows hosted CI, WASAPI hardware, and Linux production-device evidence
  remain unavailable. The support matrix continues to keep WASAPI unpromoted,
  ASIO source-only, and Linux Dummy-only.

**Status:** Implementation complete; evidence limitations are recorded above.