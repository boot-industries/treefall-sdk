<!-- SPDX-License-Identifier: MIT -->

# Treefall SDK Agent Guide

This file applies to the entire repository. It records repository-specific
engineering and verification rules for automated contributors. Human-facing
product and integration guidance remains in `README.md` and `docs/`.

## Mission and boundaries

Treefall is the public product identity for this host-neutral C++20 audio SDK.
The repository, native target names, executable names, and canonical `orpheus`
C++ namespace remain technical compatibility identities. Optimize for
deterministic behavior, realtime safety, installed-package usability, and
truthful capability reporting.

- SDK core owns reusable transport, routing, media, session, audio I/O,
  diagnostics, and host-neutral workflow contracts.
- Child applications own presentation, UI view models, analyzer histories,
  plugin/editor state, persistence policy, musical scheduling, and interaction
  rules.
- Do not modify child-app repositories, SDK pins, or child-app CI as part of an
  SDK task unless that repository is explicitly in scope.
- Model downstream requirements with SDK-owned fixtures. Do not add app-specific
  policy to core to make a fixture pass.

## Suite temporal coherence

`ARCHITECTURE.md` §Temporal Coherence is a release requirement for the SDK,
ShmUI and every child app. Preserve phase/sample integrity and realtime safety
while eliminating stale meters, counter stutter and delayed interaction.
Reuse existing sample/host timestamps and latency validity; do not create a
competing clock. Carry freshness and discontinuities through bounded transfers.
Demand-driven work must reduce wakeups/CPU without reducing live responsiveness.
Verify worst-case timing and resource behavior under load, not just average FPS.
Known time-coherence failures block release; policy adoption is not proof that
the current implementation meets the requirement.

Historical completion and child-team handoff records are ignored local
provenance. They are not required public documentation links or build inputs.

Current downstream-facing SDK contracts are defined by the installed public
headers, `README.md`, `ARCHITECTURE.md`, and `docs/SUPPORT_MATRIX.md`.
Historical FourTrack handoffs remain local provenance and retain their original
identifiers and claims.

## Sources of truth

- Current source version: SDK 0.9.1 with stable C ABI 1.0. The root CMake project
  remains technically named `orpheus`; Treefall is the active product identity.
- Compatibility package/configuration names: `TreefallSDK` and `OrpheusSDK`;
  both resolve the same physical target graph.
- Platform/backend support: `docs/SUPPORT_MATRIX.md`.
- Installed target manifest: generated package metadata and the clean-prefix
  fixture under `tests/cmake/find_package/`.
- Realtime constraints: `docs/REALTIME_AUDIT.md` and
  `tools/realtime_audit.py`.
- Public API: installed headers under `include/orpheus/`.
- Sprint completion and explicit deferrals: ORP143.

Never promote a planned or merely compiled backend to supported status without
the evidence required by the support matrix. In particular, the merged WASAPI
implementation is not release-supported until hosted Windows package/ABI checks
and a real-device acceptance record pass.

## Build and verification

Baseline local workflow:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DORPHEUS_ENABLE_REALTIME=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

For a behavioral change, run the narrow test or executable that exercises the
changed path, then the relevant package/realtime gate. Before merging a broad SDK
change, run the full configured suite.

Required specialized gates:

```sh
# Installed public-package consumer
ctest --test-dir build --output-on-failure -R '^cmake_find_package$'

# Strict in-repository realtime audit
ctest --test-dir build --output-on-failure -R '^realtime_static_audit$'

# Documentation links and removed paths
ctest --test-dir build --output-on-failure -R '^docs_path_audit$'

# ShmUI imported-content contract
python3 tools/shmui_juce_manifest.py --check
```

Use sanitizer, TSan, deterministic render-hash, and hardware tests when the
changed surface requires them. Do not substitute macOS or Dummy-driver results
for Windows or real-device evidence.

## Public API and package rules

- Public contracts live in `include/orpheus/`; private headers under `src/` must
  not be required by consumers.
- Every public capability needs an installed clean-prefix compile/link/run
  fixture using documented `Orpheus::` targets.
- Run symbol/reference analysis before changing an exported C++ interface.
- Append new virtual methods when a virtual extension is unavoidable; do not
  insert them among existing vtable entries.
- Decorate Windows-visible public symbols with the repository export macro.
- Use stable IDs and `TimePoint`/`TimeRange` across persistence and cross-thread
  boundaries. Do not expose raw graph pointers in snapshots.
- Make success, invalid input, unavailable capability, capacity refusal, and
  runtime failure distinguishable.
- Prefer a clean cutover: migrate every in-repository caller and remove obsolete
  paths rather than leaving aliases or shims.

Documented installed targets include:

- `Orpheus::core`
- `Orpheus::diagnostics`
- `Orpheus::audio_utils`
- `Orpheus::audio_io`
- `Orpheus::audio_driver_manager`
- `Orpheus::routing`
- `Orpheus::transport`

`packages/shmui-juce` is an `add_subdirectory` package with
`Orpheus::shmui_juce`; OpenGL is opt-in through
`SHMUI_JUCE_ENABLE_OPENGL` and `Orpheus::shmui_juce_gl`.

Treefall compatibility rules:

- New integrations may use `Treefall::` targets, `include/treefall/...`
  forwarding headers, and the `treefall` namespace alias. Existing `Orpheus::`
  targets, `include/orpheus/...` headers, and `orpheus` code remain supported.
- `TREEFALL_*` macros/types and `treefall_*` C wrappers are additive over the
  stable C ABI 1.0 tables and layouts. Keep all old C exports indefinitely
  during ABI 1.0.
- An appended C++ virtual method is source-compatible only after rebuilding
  consumers and subclasses with the matching headers. It is not safe to call
  through an old prebuilt C++ subclass. Legacy removal requires a separately
  approved major migration; do not invent a deprecation deadline.

## Realtime rules

Audio callbacks must not allocate, lock, perform file/network I/O, log, own
callbacks through allocating wrappers, or execute unbounded work.

- Preallocate on the control thread.
- Use fixed-capacity queues/rings with explicit overflow outcomes.
- Keep callback work deterministic and bounded for every public block size.
- Use atomic publication or a proven SPSC contract for cross-thread state; never
  create a data race with plain-struct overwrite.
- Cache media before callback use. A streaming miss emits silence plus an
  observable underrun and never performs synchronous file I/O.
- Keep canonical time as integer samples. Seconds, beats, and timecode are
  derived views.
- Realtime telemetry is a bounded transport/routing/diagnostic bridge, not an
  analyzer framework. Drain it on the message thread.

## Session, media, and workflow invariants

- Media identity is a versioned fingerprint, not a path.
- Hashing, verification, session serialization, migration, and recovery occur
  off the audio thread.
- Preserve the last valid session document across failed or interrupted saves.
- Unsupported future schemas must remain distinguishable from recoverable
  corruption.
- `SessionGraph` transactions coalesce one logical edit into one revision,
  rollback on destruction, reject nesting, and restore ID allocator watermarks.
- Application undo stacks and presentation state stay outside the graph.

## ShmUI import updates

ShmUI is the upstream source authority. Orpheus owns a governed imported copy.
For an intentional import update:

1. update the imported files and named upstream revision together;
2. preserve the declared target/component/module/token contract;
3. run `python3 tools/shmui_juce_manifest.py --sync`;
4. run the manifest check; and
5. build and run the pinned JUCE non-OpenGL consumer.

Do not hand-edit only the manifest hash or enable OpenGL by default.

## Tests and documentation

Tests must defend observable behavior and fail on plausible regressions. Prefer
boundaries, invariants, transitions, overflow, ordering, deterministic output,
and real error paths over source-text or plumbing assertions.

- Match existing GoogleTest/CMake conventions.
- Keep fixtures deterministic and full-suite safe.
- Session schema changes must update both `tests/fixtures/session/` and
  `tools/fixtures/` golden sets.
- Public contract changes update headers, package fixtures, and migration
  guidance. Applicable local contract records may be updated separately; ORP
  records are ignored provenance and must not be linked from public docs.
- External factual claims in documentation require IEEE-style citations.
- Do not claim verification that was not directly observed.

## Deferred work

Do not silently implement or mark these complete:

- ORP170 candidate and stable promotion remain gated on the external acceptance
  records enumerated in ORP173; the observed development snapshot is not a
  release claim.
- Windows/WASAPI support promotion remains gated on the evidence in ORP143 §7.

Reopen any of these only with an explicit task and the stated prerequisite
proof.

## Git delivery

Use focused commits with the repository format:

```text
type(scope): imperative description
```
Keep the working tree clean, update documentation as part of completion, and
verify that the intended commit reached `origin/main` when the task requires a
mainline delivery.
