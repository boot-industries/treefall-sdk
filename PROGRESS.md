# Progress

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

## Release-hardening boundary/provenance pass — 2026-08-02

**Status:** Paused at the user's request; the checkpoint is committed and
pushed, while implementation remains in progress.

### Changes made before pause

- Replaced `tools/suite.py` with a fail-closed coordinator design:
  - Draft 2020-12 schema validation hook with structured diagnostics;
  - canonical URL comparison for HTTPS, SSH, and scp-style Git remotes;
  - immutable-tag advertisement probes through `git ls-remote`;
  - shared read-only preflight for status, doctor, verify, sync, update,
    rollback, coordinate, and release;
  - artifact content/provenance reporting in `snapshot_status()`;
  - durable per-operation journals, backup refs, partial envelopes, and
    explicit `recover --action complete|restore`;
  - dry-run `planned`/`blocked` outcomes and apply `applied`/`partial` exits;
  - nested dirty-worktree checks and serial nested-build environment.
- Added the stdlib-only `orpheus_artifact_provenance` package surface and the
  pinned suite requirement file. Its descriptor-relative inventory rejects
  symlinks, traversal, special files, and unsupported no-follow APIs.
- Extended the suite schema and manifest records with artifact scope and
  immutable snapshot references. The manifest currently validates successfully
  under a temporary environment containing `jsonschema==4.23.0`.
- Began the public/private boundary cutover:
  - removed SDK ShmUI package install/export/build registration and obsolete
    SDK mirror/check files;
  - renamed private ShmUI JUCE targets to `ShmUI::juce` and
    `ShmUI::juce_gl`;
  - changed Wave Finder to require explicit `SHMUI_JUCE_SOURCE_DIR`;
  - changed the private sync wrapper to require an explicit staging
    destination and reject SDK/package paths.

### Verification observed

- `python3 -m py_compile tools/suite.py
  orpheus_artifact_provenance/__init__.py
  orpheus_artifact_provenance/cli.py` passed.
- `tools/suite.py validate --json` passed with the temporary pinned
  `jsonschema` environment.
- Descriptor inventory and symlink-escape smoke checks passed.
- Real-workspace `status --json` returned `status: blocked` with the expected
  historical snapshot drift, unadvertised immutable refs, dirty SDK worktree,
  and fetch-only/missing push capability records. No mutation was attempted.
- SDK branch `audit/release-hardening-2026-08-02` is pushed at
  `2b9c9daac21338b4d3f600e2b225b7463a5d24d2`.
- ShmUI private follow-up branch `audit/private-juce-staging-2026-08-02` is
  pushed at `5f63b3a3bfcfc68581c7d5ccaab0b447a9cd532d2`; the user's existing
  `audit/audio-ui-boundary-2026-08-02` branch was not rewritten.

### Still required after resume

- Finish and verify active SDK/private documentation and CI changes.
- Add focused suite, provenance, and release-evidence red-team fixtures.
- Complete installed public-package scans and ABI/profile proof.
- Exercise isolated mutation failures and both recovery actions.
- Run qcheck and the configured CTest regression suite.
- Retain the existing macOS-only evidence boundary; no Windows/WASAPI
  hardware claim is made.