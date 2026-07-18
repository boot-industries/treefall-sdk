<!-- SPDX-License-Identifier: MIT -->

# Orpheus SDK Agent Guide

This file applies to the entire repository. It records repository-specific
engineering and verification rules for automated contributors. Human-facing
product and integration guidance remains in `README.md` and `docs/`.

## Mission and boundaries

Orpheus is a host-neutral C++20 audio SDK. Optimize for deterministic behavior,
realtime safety, installed-package usability, and truthful capability reporting.

- SDK core owns reusable transport, routing, media, session, audio I/O,
  diagnostics, and host-neutral workflow contracts.
- Child applications own presentation, UI view models, analyzer histories,
  plugin/editor state, persistence policy, musical scheduling, and interaction
  rules.
- Do not modify child-app repositories, SDK pins, or child-app CI as part of an
  SDK task unless that repository is explicitly in scope.
- Model downstream requirements with SDK-owned fixtures. Do not add app-specific
  policy to core to make a fixture pass.

The ORP141 completion and child-team handoff is recorded in
`docs/orp/ORP143 Reliability and Adoption Sprint Completion and Child-App Handoff.md`.
`docs/orp/ORP142 Downstream Consumer Adoption Notes.md` remains the non-binding
adoption guide.

Current FourTrack-facing SDK contracts are:

- `docs/orp/ORP154 Sequencer Trigger Voice Primitive.md` for the standalone
  one-shot voice utility;
- `docs/orp/ORP155 FourTrack Recorder Adoption Friction - CoreAudio and Routing Contracts.md`
  for directional endpoints, isolated routing meters, and capture telemetry;
- `docs/orp/ORP156 ORP155 Implementation Handoff.md` for verification and
  delivery evidence.

## Sources of truth

- Version: `project(orpheus VERSION ...)` in `CMakeLists.txt`.
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
- Public contract changes update headers, package fixtures, migration/handoff
  guidance, and the appropriate `docs/orp/ORP1NN ...` record together.
- External factual claims in documentation require IEEE-style citations.
- Do not claim verification that was not directly observed.

## Deferred work

Do not silently implement or mark these complete:

- R5 one-shot voice work remains gated on two independent existing consumers and
  a shipped/soaked R1–R4 release.
- The Release Operating Model jobs listed in ORP143 §8.2 were deferred as a unit.
- Windows/WASAPI support promotion remains gated on the evidence in ORP143 §7.

Reopen any of these only with an explicit task and the stated prerequisite
proof.

## Git delivery

Use focused commits with the repository format:

```text
type(scope): imperative description

Co-Authored-By: Claude <noreply@anthropic.com>
```

Keep the working tree clean, update documentation as part of completion, and
verify that the intended commit reached `origin/main` when the task requires a
mainline delivery.
