# Progress

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

## ORP255 — Host-neutral multichannel metering

**Date:** 2026-09-01
**Status:** Review remediation complete; final Debug suite 80/80, package, TSan, and release deadline gates passed.


- SDK 0.9.0 adds schema-3 canonical routing telemetry, nested schema-1 logical
  group-output lanes, packed atomic route identity, finite-input sanitization,
  and true-peak silence/history rules.
- Route updates now serialize through an odd/even publication sequence. Audio
  slices accept topology only when matching-even before/after observations and
  the route generation agree; reinitialization clears the rendered revision.
- Exact reference-signature contracts, non-silent maximum-topology allocation
  coverage, coherent-snapshot TSan bounds, and corrected ORP254 manifest
  provenance close the PR review findings.
- Follow-up checks passed: routing 58/58, realtime diagnostics 10/10,
  multichannel transport 10/10, transport controller 17/17,
  `cmake_find_package`, the non-silent allocation gate, and ThreadSanitizer
  `HammerQueriesUnderConcurrentRender` (984 ms, no warning). Final release
  maxima were 5236.5 us sample peak (p99 5163.04 us, average 4710.97 us) and
  7111.5 us true peak (p99 6954.04 us, average 6575.28 us), below the 10666.7
  us budget. The full configured Debug CTest suite passed 80/80 in 341.21
  seconds.
- Full record: [`ORP255 Host-Neutral Multichannel Metering Contract`](docs/orp/ORP255%20Host-Neutral%20Multichannel%20Metering%20Contract.md).

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