# Progress

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

**Date:** 2026-08-13
**Status:** Source and deterministic repair complete; final CLbuds rerun blocked by device disconnect.

### Delivered

- CoreAudio now plans physical endpoint rates independently, applies only safe
  device-global writes, and performs bounded directional conversion at the I/O boundary.
- Route state reports physical/AUHAL client rates, virtual and callback widths,
  conversion latency, Bluetooth relationship, mono fallback, and callback health.
- Converted callback chunks use one session-frame base and advance the timeline only after
  the full callback, so multi-chunk delivery is contiguous.

### Evidence and boundary

- A 2026-08-12 CLbuds inventory supplied the missing directional facts: its
  `:input` endpoint is 16 kHz/320 frames/one channel, while `:output` is
  44.1 kHz/512 frames/two channels.
- Physical ARM exposed three startup defects. Monitoring admitted pre-activation
  rate/format facts and used one output-sized buffer expectation for every
  endpoint. A 48 kHz duplex run also exhausted the input FIFO while the output
  AUHAL blocked during startup because priming used maximum capacity rather than
  the active 320-frame callback and capture continued while output startup was
  unable to consume it.
- Monitoring now admits the fully activated route and captures rate, buffer,
  and stream-format baselines per endpoint. Capture priming uses the current
  input callback size, then freezes the primed FIFO until output startup
  completes. Later real mutations remain terminal.
- `CoreAudioRouteMonitorTest.*` passes 13 contracts; the complete
  `coreaudio_driver_test` binary passes 62 contracts.
- An intermediate repaired 44.1 kHz SDK hardware run passed with 498 callbacks,
  219,618 host/input frames, a non-zero input peak, healthy route outcome, and
  zero render/FIFO/conversion failures. The subsequent 48 kHz run exposed the
  startup FIFO defect above. CLbuds then disconnected before the final
  44.1/48 kHz rerun, so final-source physical capture is not claimed.
- FourTrack separately repairs stale persisted-output resolution and verified
  the disconnected CLbuds UID falls back to the live built-in default supporting
  both session rates.
- No release tag or downstream production pin is claimed.
### Candidate verification

- The isolated Debug build (`/tmp/ftr085-sdk-debug`) passed the complete 80/80
  CTest suite, including package consumers, CoreAudio route/driver contracts,
  realtime audit, and stress labels.
- The isolated Release build (`/tmp/ftr085-sdk-release`) passed the complete
  80/80 CTest suite with the same package and platform gates.
- `python3 tools/realtime_audit.py --root . --include-adjacent` passed with
  zero hard failures and zero tracked debt findings. Prepared directional
  converter transfers are covered by the runtime allocation guard.
- `orpheus_coreaudio_hardware_acceptance` now reports requested/actual route
  rates and buffers, physical/virtual/client widths, directional latency,
  conversion/FIFO counters, bounded capture evidence, and stable terminal
  outcomes. Built-in output-only smoke and expected unavailable-output
  initialization checks passed; no CLbuds hardware result is inferred.

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