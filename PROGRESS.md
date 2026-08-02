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