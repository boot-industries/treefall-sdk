<!-- SPDX-License-Identifier: MIT -->
# ORP263 SDK Resilience, Responsiveness, and Architecture Review

**Date:** 2026-09-25  
**Review target:** `adcbddc5bc1f03c133ef0520ff7da88f2868325`  
**Scope:** Treefall/Orpheus SDK 0.9.0 source, C++20 public headers, stable C ABI 1.0, supported host-neutral/core and macOS CoreAudio paths  
**Status:** Complete review record; no SDK source or test fixes implemented

## Executive assessment

The SDK has a strong realtime foundation: the transport command pool is fixed at 255
nodes, the active voice array is fixed at 32, the routing matrix is preallocated, prepared
and streaming clip sources publish fixed page arrays, and the normal CoreAudio/Dummy
callback lifetime barriers are explicit. The current tree also has unusually broad
behavioral coverage for command ownership, streaming page pins, deterministic render
hashes, package discovery, and CoreAudio route state.

The review found **confirmed correctness and boundary defects** despite the green test
matrix:

1. A future-schema primary document is silently replaced by an older backup during
   recovery (`LoadSessionWithRecovery`).
2. Session transaction rollback can reuse already-issued TrackId/ClipId values when the
   pre-transaction allocator watermark is not represented by a surviving object.
3. The stable C ABI dereferences fabricated, stale, foreign, or double-destroyed
   non-null session handles; a focused probe reached an ASan/UBSan crash.
4. CoreAudio's render callback clears caller-provided output buffers before validating
   malformed `AudioBufferList` shape and byte sizes.
5. Routing malformed-output validation mutates meter publication state despite the
   documented no-touch boundary.
6. Performance monitor latency is a one-buffer estimate presented as round-trip
   latency; its public coherent-snapshot and any-thread reset contracts do not match the
   independent atomic implementation.
7. The audio command path invokes a loop-anchor commit operation documented as
   control-thread-only.
8. The callback-p99 diagnostic is a half-budget threshold counter, and normal transport
   telemetry supplies no callback duration at all.

**Release-blocker decision: Yes, for the current release candidate.** The future-schema
recovery behavior can silently open stale user data, and the ABI/CoreAudio findings are
confirmed safety-boundary violations on supported paths. The package graph itself is
healthy; the blockers are behavior and ownership, not clean-prefix installation.

The highest-leverage remediation order is:

1. Contain recovery, allocator, invalid-handle, and malformed-callback behavior.
2. Correct diagnostic semantics and establish producer/consumer contracts.
3. Measure and bound worker, driver-manager, maximum-topology, and Windows backend
   responsiveness.
4. Remove the demo/adapter private-implementation boundary and consolidate ABI runtime
   state only after correctness fixes stabilize the contracts.

## Evidence baseline

| Item | Observed value |
|---|---|
| Git revision | `adcbddc5bc1f03c133ef0520ff7da88f2868325` |
| Worktree before review | clean; no pre-existing changes |
| Host | macOS Darwin 25.5.0, arm64 MacBook Pro, Apple M2 Pro |
| CMake | 4.1.0 |
| Configured compiler | AppleClang 21.0.0.21000099 (`/usr/bin/c++`) |
| Python | 3.14.6 |
| libsndfile | 1.2.2 |
| Review configuration | `/tmp/treefall-sdk-review-asan-adcbddc5` |
| Sanitizer flags | ASan + UBSan; `detect_leaks=0` because macOS ASan reports leak detection unsupported |
| CoreAudio | enabled; default-endpoint CoreAudio tests passed |
| WASAPI | disabled on macOS; source/fake review only |
| Stale generated tree | `build-release` contains 0.9.1 package metadata while source is 0.9.0; not used as evidence |

The review used only temporary `/tmp` build/probe directories. No source, test, build
 tree, or generated repository file was intentionally changed. This ORP record is the
requested review artifact.

## Verification completed

### Full current sanitizer suite

```text
ASAN_OPTIONS=halt_on_error=1:detect_leaks=0
UBSAN_OPTIONS=halt_on_error=1
PYTHONDONTWRITEBYTECODE=1
ctest --test-dir /tmp/treefall-sdk-review-asan-adcbddc5 --output-on-failure -j 1
```

Observed: **82/82 passed**, including package, hardware-labeled CoreAudio, stress,
streaming, transport, routing, session, ABI-link, and ShmUI package consumer tests.

### Focused gates

- Transport/routing/realtime: **32/32 passed**.
- Media/streaming/provider: **9/9 passed**.
- Session/graph/scene/routing: **4/4 passed**.
- Driver/Dummy/CoreAudio/manager: **5/5 passed**.
- Installed package/ABI/version/retired-field/add-subdirectory: **11/11 passed**.
- Standalone realtime source audit: passed with 0 hard failures and 0 tracked debt.
- `tests/realtime_audit_test.py`: 5/5 passed.
- TSan configuration on Darwin: **5/5 passed** for `command_ingress_test`,
  `voice_state_tsan_test`, `callback_loss_telemetry_test`, `realtime_harness_test`,
  and `streaming_seek_test`.

### Temporary behavioral probes

1. **Future schema recovery:** schema-999 primary plus valid schema-1 `.bak` returned
   `recovered=1`, `source=<primary>.bak`, and loaded the backup name. This confirms the
   defect; the expected result is an explicit unsupported-schema failure.
2. **Allocator watermark:** after removing TrackId 1, creating temporary TrackId 2 in a
   transaction, rolling back, and creating another track, the new ID was 1. The same
   sequence for ClipId produced `removed=1 temporary=2 after=1`.
3. **CoreAudio malformed output:** direct callback probe with an invalid callback state
   and a one-frame output buffer returned `status=0` and changed sentinel bytes to zero
   before shape validation completed.
4. **Routing malformed output:** a valid measured meter snapshot changed to
   `availability=Unmeasured, coherent=0` after `processRouting(nullptr, nullptr, 16)`.
5. **ABI fabricated session:** `set_tempo(reinterpret_cast<handle>(0x1), 120)` produced
   UBSan misaligned-access diagnostics and an ASan `SEGV`; it did not return a status.
6. **Maximum routing topology:** ASan/UBSan measured 4/2/2 at 18.463 ms p50 and
   256/32/32 at 484.497 ms p50 / 496.610 ms max for 2048 frames. An unsanitized Release
   probe measured 0.301 ms p50 for 4/2/2 and 9.405 ms p50 / 9.780 ms max for 256/32/32;
   the 48 kHz/2048 deadline is 42.667 ms. At 192 kHz the equivalent deadline is
   10.667 ms, so the maximum topology is close to the budget on this host. The existing
   in-tree maximum-topology test uses 512 frames and skips wall-clock assertions under
   sanitizers.
7. **Performance semantics:** one callback `(500 us, 1000 us, 2 clips, 48 kHz, 512)`
   produced `latency=10.666667 ms`, exactly `bufferSize / sampleRate`, while the public
   field is documented as round-trip latency.

## Classification rules

- **Confirmed defect:** reachable path, violated invariant, concrete trigger, and direct
  source/runtime evidence.
- **Resilience risk:** credible reachable failure mechanism with remaining product or
  platform uncertainty.
- **Design debt / test gap:** maintainability or verification limitation not presented as
  a runtime failure.
- **Optional improvement:** bounded structural work not required to correct a confirmed
  defect.

## Ranked findings

### F-01 — Future schema recovery silently substitutes an older backup

**Classification:** Confirmed defect  
**Severity:** High  
**Confidence:** High

**Location:** `src/core/session/json_io.cpp:181-190`, `:488-500`;
`src/core/session/json_io.h:14-27`.

**Invariant and reachable path:** `ParseSession` rejects every schema other than 0 or 1,
including schema 999. `LoadSessionWithRecovery` catches every `std::exception` from the
primary and unconditionally attempts `<path>.bak`. A valid schema-1 backup therefore
converts an unsupported future document into a successful stale-session load. The
recovery result exposes `recoveredFromBackup=true`, but callers using
`LoadSessionFromFile` discard that metadata and receive the older session as if it were
the requested document.

**Concrete impact:** a future SDK or newer session schema can be silently downgraded to
an older backup. The user may edit or render stale data without an explicit unsupported-
schema error. This is deterministic recovery misclassification and a data-integrity
failure, not merely weak diagnostics.

**Trigger:** write a schema-999 primary and a valid schema-1 `.bak`, then call
`LoadSessionWithRecovery(primary)`.

**Smallest safe recommendation:** classify unsupported schema separately from malformed
or I/O-corrupt primary data. Return an explicit unsupported-schema error and do not
consult `.bak` for that class. Preserve backup fallback for malformed/truncated primary
documents. Add a regression that asserts both the future-schema failure and the existing
corrupt-primary recovery success.

**Alternatives and tradeoffs:** a typed recovery result could expose `UnsupportedSchema`
without changing the C ABI, but changing the existing `std::runtime_error` text is not
a stable machine contract. A backup-age prompt is insufficient; the primary schema
must remain authoritative.

**Verification:** failing-before is the schema-999 probe above. Passing-after requires
an explicit error with no `recoveredFromBackup`, while malformed-primary plus valid
backup still returns the backup.

---

### F-02 — Transaction rollback can reuse previously issued stable IDs

**Classification:** Confirmed defect  
**Severity:** High  
**Confidence:** High

**Location:** `src/core/session/session_graph.cpp:177-237`, `:284-360`, `:464-560`;
`include/orpheus/identity.h:83-108`; `include/orpheus/session_graph.h:218-237`.

**Invariant and reachable path:** snapshots contain extant TrackId/ClipId values but not
allocator `nextRaw` watermarks. `restore_unchecked` constructs fresh allocators and calls
`reserveThrough` only for IDs present in the snapshot. A transaction that allocates a
temporary ID after a previously issued ID was removed therefore rolls back to an
allocator whose next value can be below the pre-transaction watermark.

**Concrete impact:** stable IDs can be reused for a different Track or Clip after
rollback or restore. External maps, serialized references, undo history, telemetry, or
application caches can associate the reused ID with the wrong object. This violates the
stable-ID/monotonic-allocator contract even though snapshots themselves contain no raw
pointers.

**Trigger:** allocate and remove ID 1, begin a transaction, allocate temporary ID 2,
destroy/rollback the transaction, then allocate the next object.

**Smallest safe recommendation:** capture and restore both allocator watermarks as
transaction state. If snapshots must preserve monotonicity across persistence, add an
explicit watermark representation and schema migration. A regression must cover removed
IDs and failed insertions, not only the existing no-gap rollback case.

**Alternatives and tradeoffs:** never rebuilding allocators during rollback preserves
watermarks for in-memory transactions but does not solve persisted snapshot reloads.
Persisting watermarks changes the public `SessionGraphSnapshot` schema/layout and every
consumer that serializes it; it does not alter the stable C ABI tables.

**Verification:** failing-before is the TrackId/ClipId probe output
`removed=1 temporary=2 after=1`. Passing-after requires the next allocation to remain
strictly above the pre-transaction watermark for both ID types.

---

### F-03 — C ABI session handles are unchecked before dereference

**Classification:** Confirmed ABI safety defect  
**Severity:** High  
**Confidence:** High

**Location:** `src/core/abi/abi_internal.h:19-30`; `src/core/abi/session_api.cpp:25-27`,
`:30-79`; `src/core/abi/clipgrid_api.cpp:17-146`; `src/core/abi/render_api.cpp:163-188`.

**Invariant and reachable path:** C handles are opaque pointers but are raw C++
`SessionGraph*`, `Track*`, and `Clip*` addresses. Child ownership is checked by pointer
identity, but non-null session handles are not checked against a live-handle registry,
cookie, generation, or owner. `SessionDestroy` unconditionally deletes the cast pointer;
session operations and clipgrid/render operations dereference it. `GuardAbiCall` cannot
contain invalid-pointer undefined behavior or a double-delete fault.
`SessionGetTransportState` is additionally outside `GuardAbiCall`.

**Concrete impact:** fabricated handles, handles from another object system, handles
retained after destruction, and double destruction can crash the host process rather
than return `INVALID_ARGUMENT`/`NOT_FOUND`. The focused probe reached UBSan misaligned
access and ASan `SEGV` in `SessionGraph::set_tempo`.

**Smallest safe recommendation:** validate every session handle against a thread-safe
live-session registry before any cast/dereference; make destroy reject unknown handles
without deleting them; route transport-state reads through exception containment. A
pointer-shaped ABI can retain its table layout while using an internal registry. A
generation-token redesign would be stronger but changes the handle representation and
requires broader migration.

**Alternatives and tradeoffs:** a registry adds control-side synchronization and must
define cross-module ownership. Opaque integer tokens would be safer but change public
handle semantics and all consumers. No v1 table layout change is required for a live-
pointer registry.

**Verification:** add a C ABI probe covering fabricated, stale-after-destroy, foreign,
and double-destroy session handles, plus a throwing transport-state operation. Existing
child-handle tests at `tests/abi_smoke.cpp:146-193` do not cover this session boundary.

---

### F-04 — CoreAudio malformed output is touched before validation

**Classification:** Confirmed CoreAudio safety defect  
**Severity:** High for malformed backend callback shapes; Medium for ordinary CoreAudio operation  
**Confidence:** High

**Location:** `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:719-762`.

**Invariant and reachable path:** `renderCallback` clears every advertised
`AudioBufferList` buffer at `:726-731`, before checking `ioData`, frame capacity,
channel count, null data pointers, and `mDataByteSize` at `:739-754`. The public
realtime boundary requires malformed shapes to be no-touch returns. The callback can
therefore write caller memory on the rejection path; corrupt `mNumberBuffers` or byte
size values can also walk beyond the supplied list/buffer.

**Concrete impact:** the callback can mutate output memory on invalid input and, for
corrupt list metadata, perform an out-of-bounds clear before returning
`BufferSizeChanged`. The direct probe supplied a sentinel buffer with an invalid driver
state and observed all sentinel bytes changed to zero while the callback returned
`noErr`.

**Trigger:** CoreAudio supplies a short/null/undersized `AudioBufferList`, or a test or
future backend adapter supplies one.

**Smallest safe recommendation:** validate the complete output shape and per-buffer byte
capacity before the first `memset`. Only clear/write after validation. Keep malformed
returns no-touch and preserve the typed terminal route outcome. Add injected callback
tests for null list, too-few buffers, null data, short byte size, oversized frame count,
and valid silence failure paths.

**Alternatives and tradeoffs:** clamping an oversized byte size would still violate
no-touch semantics; rejecting before mutation is safer. This is a platform backend fix,
not a C ABI or public-header change.

**Verification:** failing-before is the sentinel probe. Passing-after requires every
malformed case to leave output bytes unchanged and return the documented route result;
valid buffers must still be zeroed on conversion/route failure.

---

### F-05 — Routing malformed-output validation mutates meter state

**Classification:** Confirmed no-touch contract defect  
**Severity:** Medium  
**Confidence:** High

**Location:** `src/core/routing/routing_matrix.cpp:923-947`; public contract
`include/orpheus/routing_matrix.h:596-610`.

**Invariant and reachable path:** `processRouting` claims to validate output pointers
before control refresh, metering publication, or scratch mutation. For null output or
a null configured lane it writes `m_group_output_meter_availability=Unmeasured` and
`m_group_output_meter_coherent=0` at `:934-945` before returning. The output buffers
remain untouched, but the observable meter publication is changed on a rejected call.

**Concrete impact:** a malformed call can erase the last coherent measured telemetry
snapshot and make a valid preceding render appear unmeasured. This affects message-side
meter displays and any host using the rejection as a no-op probe.

**Smallest safe recommendation:** perform all validation before any state mutation. If
the contract intends malformed calls to invalidate telemetry, document and test that
explicitly; otherwise leave the previous snapshot intact.

**Verification:** failing-before is the probe transition from measured/coherent to
unmeasured/incoherent. Passing-after checks both null output and null lane cases leave
the prior snapshot byte-for-byte unchanged.

---

### F-06 — Performance latency is a buffer estimate presented as round-trip latency

**Classification:** Confirmed semantic defect  
**Severity:** Medium  
**Confidence:** High

**Location:** `include/orpheus/performance_monitor.h:18-29`;
`src/core/common/performance_monitor.cpp:113-119`; route contract
`include/orpheus/audio_driver.h:96-104`, `:178-195`.

**Invariant and reachable path:** `PerformanceMetrics::latencyMs` is documented as
round-trip latency, but the implementation stores exactly
`bufferSize / sampleRate * 1000`. It does not include measured capture, device,
converter, stream, processing, or playback terms. The route-latency structures explicitly
require missing mandatory terms to remain unknown and forbid substituting buffer-size
estimates.

**Concrete impact:** diagnostics and UI can report a plausible but false latency for two
routes with identical callback buffers and different hardware/stream latency. The probe
confirmed the 512-frame/48-kHz value of 10.666667 ms.

**Smallest safe recommendation:** either remove/rename the estimated field in a
versioned API or populate it from the backend's measured `AudioRouteLatency` and expose
`complete=false` when terms are missing. Do not label a buffer estimate as measured
round-trip latency.

**Alternatives and tradeoffs:** a compatibility-preserving documentation correction can
retain the field but rename its meaning; a semantic correction should avoid changing
the C ABI and should be covered by installed consumers.

**Verification:** compare two fake routes with equal callback buffers and distinct
measured route terms; the monitor must not report the same claimed round-trip value.
The current buffer-estimate probe is the failing-before control.

---

### F-07 — Performance metrics coherence and reset contracts do not match implementation

**Classification:** Confirmed contract defects  
**Severity:** Medium  
**Confidence:** High

**Location:** `include/orpheus/performance_monitor.h:32-60`, `:62-87`;
`src/core/common/performance_monitor.cpp:43-71`, `:86-137`.

**Invariant and reachable path:** `getMetrics()` promises an atomically consistent
same-moment snapshot but independently relaxed-loads CPU, latency, underrun, active
clip, and total-sample fields while the producer updates them separately. Reset methods
are documented callable from any thread, but `resetUnderrunCount` is a store-zero racing
`fetch_add`, and `resetPeakCpuUsage` is load-then-store racing peak publication. These
are not C++ data races because the fields are atomic, but they are semantic lost-update
and cross-generation tuple failures.

**Concrete impact:** UI can display a tuple that never existed at one callback boundary;
concurrent reset can erase an underrun or be overwritten by an in-flight peak update.
The existing tests only race readers/resets and do not run a producer against them.

**Smallest safe recommendation:** either explicitly redefine the API as independent
eventually consistent counters, or add a bounded coherent publication mechanism and a
producer-quiescent/epoch reset protocol. A generation/seqlock must preserve the
allocation-free callback contract.

**Verification:** producer/consumer stress under TSan with unique callback tuples; assert
only published tuples or the documented independent-counter invariants. Add a controlled
reset race that proves the chosen reset ordering and no lost count.

---

### F-08 — Loop-anchor commit is invoked from the audio command consumer

**Classification:** Confirmed threading-contract defect; current implementation impact remains bounded  
**Severity:** Medium  
**Confidence:** High

**Location:** control-only contract `include/orpheus/clip_source.h:198-209`; audio
invocation `src/core/transport/transport_controller.cpp:1607-1609`.

**Invariant and reachable path:** `prepareLoopAnchorTransition`, `commitLoopAnchorTransition`,
and `rollbackLoopAnchorTransition` are documented as control-thread operations.
`processCommands` runs on the audio consumer and calls
`cmd.loopAnchorSource->commitLoopAnchorTransition(...)` after command processing. The
current commit body only performs bounded atomic pin release and clears the transition,
so no crash was observed; nevertheless the public/internal ownership contract is
violated on every accepted loop-anchor transition.

**Concrete impact:** future non-atomic state added to the transition can introduce an
audio-thread race; current code makes the ownership model dependent on an implementation
accident rather than its declared contract.

**Smallest safe recommendation:** either make the transition object explicitly
audio-safe and document/test that narrower contract, or move commit to a control-side
state machine that cannot race preparation/rollback. Preserve failure atomicity for
queue rejection and teardown.

**Verification:** add a transition test that records the executing thread or uses a
production seam to assert the declared owner; run it with the existing queue-full,
failed-reanchor, and loop-pin tests.

---

### F-09 — Callback-p99 diagnostic is not a p99 metric and normal transport timing is absent

**Classification:** Confirmed semantic/integration defect  
**Severity:** Medium  
**Confidence:** High

**Location:** `include/orpheus/realtime_diagnostics.h:9-18`;
`src/core/common/realtime_diagnostics.cpp:13-27`;
`src/core/transport/transport_controller.cpp:627-628`.

**Invariant and reachable path:** `callback_p99_over_budget_count` increments when one
callback exceeds half the buffer budget; no percentile distribution is stored or
calculated. Normal `TransportController::processAudio` calls
`beginRealtimeBlock(frames, sampleRate)` without a duration, so its budget counters stay
zero for every ordinary transport callback.

**Concrete impact:** consumers can interpret the field as statistical p99 evidence when
it is only a severe threshold count, and can interpret zero as deadline health when no
duration was supplied. This is diagnostic truthfulness, not an audio callback allocation
or lock defect.

**Smallest safe recommendation:** document the exact half-budget threshold and add an
availability/duration source field in a versioned schema, or measure callback duration
at the driver seam where timing is already opt-in. Do not synthesize a p99 value from
two counters.

**Verification:** assert the current threshold semantics directly, then verify a normal
transport render either supplies a measured duration or exposes timing as unavailable.

## Plausible resilience and responsiveness risks

These are credible reachable mechanisms, not confirmed product failures under the
requested standard.

### R-01 — Worst-case command and maximum-topology callback work

`processCommands` can detach 255 commands and scan up to 32 voices per command;
`UpdateGain` computes `std::pow` in the audio command consumer. Rendering clears all
configured routing lanes and processes the configured voice/source topology. The
maximum routing probe was allocation-free and below the 48 kHz/2048 deadline when
unsanitized, but reached 9.780 ms at 256/32/32. At 192 kHz the corresponding budget is
10.667 ms. Existing in-tree deadline assertions are 512-frame tests and skip under
sanitizers.

**Recommendation:** measure a 32-voice/8-source/2048-frame case with 255 metadata/gain/
choke commands, p50/p95/p99/max, allocation/I/O guards, and the actual negotiated
sample rate. Precompute gain conversion before publication if the measurement shows
the audio-thread cost is material. No unbounded loop was found.

### R-02 — Single media worker couples all streaming sources and teardown

`MediaStreamWorker::run` services command demand and steady windows sequentially;
a slow reader can delay later sources. Destruction joins the worker but does not cancel
an in-flight ordinary reader decode. The 2-second command-prime timeout is not a hard
upper bound when cancellation must wait for an in-flight read. This threatens control
latency, later-source cache misses, and shutdown, not the audio callback.

**Recommendation:** add a supported reader cancellation/timeout policy, per-source
service fairness or isolated workers, and teardown tests with a blocked reader. Measure
source-count growth before adding workers.

### R-03 — Driver-manager rollback and lock scope

Candidate initialization precedes old stop, but a backend stop failure can leave the
retained old driver internally stopped/terminal while manager fields still report the
old route. The manager mutex also spans hardware enumeration, candidate initialization,
and old stop; destructor stop is performed under the mutex. The public `getActiveDriver`
contract is explicitly a non-leased borrowed pointer.

**Recommendation:** inject factory failures and re-entrant getters/stop callbacks,
define whether stop failure means pointer preservation or usable-route preservation,
and narrow manager lock scope where backend callbacks permit it.

### R-04 — CoreAudio converter priming holds the driver mutex for up to two seconds

Input conversion start polls with 1 ms sleeps while holding `mutex_` until a two-second
deadline. This is control-side and bounded, not a realtime callback violation, but
concurrent stop/status operations can be delayed.

**Recommendation:** measure the priming path with delayed input and define a
control-thread latency contract. Keep the explicit deadline; do not replace it with a
fixed sleep as a safety boundary.

### R-05 — WASAPI channel-mask, PCM16, and timeout policy remain unverified

Source accepts PCM16 and extensible formats, checks channel count, but does not
explicitly reconcile a copied mix-format `dwChannelMask` with a changed requested channel
count. A two-second event wait becomes terminal backend failure. The macOS review could
not build or run WASAPI; only source and conditional fake-test inspection is evidence.

**Recommendation:** run Windows fake tests for PCM16, extensible channel masks, reduced
counts, malformed device buffers, and all wait/acquire/release outcomes, followed by the
manual physical acceptance workflow. Do not promote WASAPI support from the current
source-only posture.

### R-06 — Prepared media accepts early EOF as zero-padded success

`PreparedClipSource::decode` returns a valid source when a reader returns zero before
the metadata-advertised length, leaving a zero tail. `StreamingClipSource::decodePage`
rejects the analogous short read. This is a data-integrity consistency risk for
truncated media, not a confirmed transport defect because registration metadata and
source preparation are currently the surrounding contract.

**Recommendation:** decide whether early EOF is normalization or corruption, make
prepared/streaming behavior identical, and add fake-reader publication-atomicity tests.

### R-07 — Core and performance public pointers/latency have explicit cross-generation semantics

`RealtimeDiagnostics::snapshot()` and `PerformanceMonitor::getMetrics()` use independent
relaxed atomics. The latter claims coherence and is a confirmed defect; the former is
best classified as an eventually consistent aggregate unless coherence is promised.
Raw `Track*`, `Clip*`, and `CommittedClip::clip` pointers are invalidated by removal,
restore, and graph rebuilds. Scene/arrangement state is intentionally excluded from
transaction snapshots.

**Recommendation:** document invalidation and aggregate semantics, or add bounded
coherent publication only where consumers require it. Do not put app undo/presentation
policy into core.

## Design debt and focused verification gaps

1. **Demo host duplicates the session implementation.** `apps/juce-demo-host/CMakeLists.txt:27-41` compiles `session_graph.cpp` and `json_io.cpp` into the app, while `Main.cpp:100-160,291-325` dynamically loads ABI libraries and casts an opaque ABI handle to `SessionGraph*`. This is a confirmed optional-host boundary failure, not a core package blocker. Choose either canonical C++ linking or C-ABI-only operation.
2. **Minhost and REAPER require private `json_io.h`.** Their CMake files add `src/core/session`, and sources include the non-installed header. They cannot be clean installed-prefix consumers. Replace this with a public session I/O API or keep them explicitly source-only.
3. **ABI logger/telemetry state is DSO-local.** `src/CMakeLists.txt:60-77` embeds core/common objects in three shared ABI libraries. Orpheus/Treefall names share state within one DSO, but setting a callback through one ABI DSO need not configure another. Centralize only if process-wide callback state is intended; otherwise document DSO-local semantics.
4. **Dead transport command variants.** `TransportCommand::Type::UpdateStopOthers` and `SetVoiceMode` are declared but not handled in `processCommands`; current public setters persist policy on the control side. Remove the dead variants or route them through a defined state transition; do not treat the no-op cases as runtime defects.
5. **Worker attach permits duplicate weak entries.** `MediaStreamWorker::attach` unconditionally appends. Normal transport attaches once, but public `StreamingClipSource` permits repeated attachment. Add identity deduplication or document idempotence.
6. **Package negative checks are broad but not symbol-specific.** Retired enum/field fixtures require a build failure, not a particular diagnostic; no fixture reads both installed manifests and compares every target entry. Add a manifest/export consistency check and a private-include negative compile.
7. **ASIO is source-only.** The optional ASIO target is not in the installed manifest/export. This is intentional under the support matrix, not a defect; add a package fixture only if ASIO becomes a supported installed target.
8. **Telemetry concurrency is mostly tested serially.** The 64-slot ring, drop policy, and schema stamping are strong; actual producer/consumer stress and cross-publication routing/voice coherence are missing. Add a TSan consumer stress and force drop/recovery.
9. **Extreme sample-domain and referential-integrity gaps remain.** Snapshot conversion can accept very large finite beat values before integer conversion; clip assignments validate ID syntax but not membership in extant clips; JSON duplicate keys are first-value-wins. These are validation/design risks, not observed failures.
10. **CoreAudio hardware coverage is default-endpoint only.** CoreAudio driver/route tests passed on the available Mac, but explicit stable-UID acceptance, Bluetooth/aggregate teardown, physical unplug, and external rate/buffer changes were not run.

## Dependency and migration analysis

| Finding | Prerequisite behavior | Independent workstreams | Affected callers/symbols | Exported/API/ABI impact | CMake/package impact | Fixture/session/media/routing impact | Documentation/support impact |
|---|---|---|---|---|---|---|---|
| F-01 future schema | Distinguish unsupported schema from corruption | Recovery classifier; persistence tests | `LoadSessionWithRecovery`, `LoadSessionFromFile` | No C table change; possible typed C++ result | None | Session recovery fixtures/golden set | Session recovery contract |
| F-02 ID reuse | Preserve allocator watermarks through rollback/restore | Transaction state; snapshot schema migration | `SessionGraph::Transaction`, `restore_unchecked`, `IdAllocator` | Public `SessionGraphSnapshot` schema/layout if persisted; no C ABI | None | Session graph/round-trip/identity fixtures | Stable-ID and transaction contract |
| F-03 invalid ABI handle | Validate live session ownership before cast | Handle registry/token design; exception boundary | All C ABI session/clipgrid/render calls; adapters/tests | Keep v1 table layout; handle semantics may remain pointer-shaped | ABI link and C fixtures | Session/ABI fixtures | C ABI handle/error contract |
| F-04 CoreAudio malformed ABL | Validate before memset | CoreAudio callback guard; injected backend tests | `CoreAudioDriver::renderCallback` | No public API/ABI change | CoreAudio target/test fixture | Audio driver fixture | CoreAudio realtime boundary |
| F-05 routing no-touch | Validate before meter mutation | Routing validation/publication separation | `RoutingMatrix::processRouting` | No ABI change | Routing target | Routing no-touch/meter fixture | Routing realtime contract |
| F-06 latency estimate | Obtain measured route terms or rename field | Performance/backend integration | `PerformanceMonitorImpl::recordAudioCallback`, `PerformanceMetrics` | C++ public semantics; stable C ABI unchanged | Diagnostics target/package consumers | Performance/package workflow fixture | Latency contract |
| F-07 metric coherence/reset | Define coherent or independent semantics | Seqlock/epoch or producer-quiescent reset | `getMetrics`, reset methods | C++ API semantics; no C table change | Diagnostics target | Performance/TSan fixture | Diagnostics contract |
| F-08 loop-anchor owner | Make transition audio-safe or control-owned | State-machine refactor | `processCommands`, `commitLoopAnchorTransition` | Internal ABI only | None | Streaming seek/loop fixture | Threading contract |
| F-09 p99/duration truth | Define threshold name and timing source | Diagnostics schema/driver timing | `RealtimeDiagnostics`, transport begin block | Public diagnostic field meaning/schema; no C ABI | Diagnostics/package consumers | Telemetry/performance fixtures | Diagnostics semantics |
| R-02 worker fairness | Bounded/cancellable media service | Worker scheduling/reader contract | `MediaStreamWorker`, `StreamingClipSource` | Internal source API; public clip-source classes | Transport target | Streaming teardown/fairness fixtures | Media worker latency |
| R-03 manager rollback | Define route preservation on stop failure | Manager/backend state contract | `setActiveDevice`, manager destructor | Public manager semantics; no C ABI | Driver-manager package consumer | Factory-injected manager fixture | Device hot-swap contract |
| D-01 demo/adapter boundary | Remove private source dependency | Demo architecture/adapter API | Demo, minhost, REAPER | No core ABI; consumer migration required | Clean-prefix fixture/negative compile | Package examples/adapters | Consumer integration guidance |
| D-03 ABI DSO state | Decide process-wide vs DSO-local callbacks | Common runtime/link graph | Logger/telemetry wrappers | Potential target graph migration | All three ABI DSOs/package manifests | ABI link/callback fixtures | ABI callback ownership |

## Proposed execution sequence and completion criteria

### Phase A — Immediate containment and correctness

1. **Recovery classification:** reject future schema before backup fallback.
   - Done when schema-999 + valid backup returns an explicit unsupported-schema error;
     malformed primary + valid backup still recovers; primary remains untouched.
2. **Allocator identity:** preserve TrackId/ClipId watermarks across transaction
   rollback and persisted restore.
   - Done when removed-ID and failed-insertion probes allocate only values above the
     pre-transaction watermark; revision/transaction tests remain green.
3. **ABI handle safety:** add live session validation and contain transport-state reads.
   - Done when fabricated/stale/foreign/double-destroy probes return stable status
     without ASan/UBSan reports; valid Orpheus/Treefall cross-name handles still work.
4. **CoreAudio/routing malformed shapes:** move all validation before output/meter
   mutation.
   - Done when sentinel output and prior meter snapshots remain unchanged for every
     malformed case; valid zero/conversion paths still pass.
5. **Diagnostic truth:** correct latency/p99 meaning and decide whether transport
   duration is unavailable or measured.
   - Done when installed consumers and focused tests distinguish measured, estimated,
     and unavailable latency/callback timing.

These workstreams are independent at the code level except that the ABI and CoreAudio
changes both require lifecycle tests before release.

### Phase B — Resilience and responsiveness

- **Media worker:** add blocked-reader teardown, cancellation, source-count fairness,
  and duplicate-attach tests. Completion requires a bounded, documented teardown/prime
  result with no leaked pins.
- **Driver manager:** inject candidate-init failure, old-stop failure, callback reentry,
  and concurrent getter timing. Completion requires an explicit retained-vs-usable
  route state contract and documented lock/latency bounds.
- **CoreAudio:** measure converter-prime latency and run explicit UID/hardware
  acceptance. Completion requires no fixed-sleep safety boundary and typed route
  outcomes on physical route changes.
- **Maximum realtime topology:** benchmark 32 voices, 8 source channels, 2048 frames,
  and 255-command mixes at supported rates. Completion requires p99/max deadline and
  zero allocation/I/O evidence, with a precomputed gain path if needed.
- **Windows:** run PCM16/channel-mask/terminal fake matrix and the manual
  `wasapi-hardware-acceptance` workflow. WASAPI remains deferred until package/ABI and
  physical-device evidence exist.

### Phase C — Structural refactoring

- Choose one model for the JUCE demo: canonical C++ target or C ABI only. Remove
  private `json_io.h` and `SessionGraph*` handle casts.
- Give minhost/REAPER a public session I/O boundary or explicitly classify them as
  source-only adapters; add a clean-prefix negative/positive consumer.
- Decide whether logger/telemetry state is process-wide. If yes, centralize the runtime
  object graph and update all ABI link/package fixtures; if no, document DSO-local
  semantics and add cross-DSO tests.
- Remove dead command variants and define stable pointer invalidation/ID-based consumer
  guidance.

### Phase D — Documentation and long-term hardening

- Publish the recovery distinction, stable-ID watermark rule, C ABI handle error
  contract, measured-latency semantics, and worker/driver latency boundaries.
- Add manifest/export consistency, private-header negative, extreme-media,
  cross-publication telemetry, and same-source multi-voice fixtures.
- Keep external Clip Composer, FourTrack, and FreqFinder impact explicitly unverified
  until their checked-out sources and pins are available.

## Open questions

None. Remaining uncertainties are platform/runtime evidence boundaries, not unresolved
repository facts.

## Commands run and unavailable evidence

### Commands run

- Clean ASan/UBSan configure and build under `/tmp/treefall-sdk-review-asan-adcbddc5`.
- Full `ctest` suite: 82/82 passed.
- Focused transport/routing, media, session, driver, package/ABI, and telemetry suites.
- `python3 tools/realtime_audit.py --root . --fail-known-debt`.
- `PYTHONDONTWRITEBYTECODE=1 python3 tests/realtime_audit_test.py`.
- Separate Darwin TSan configure/build and five focused TSan tests.
- Temporary routing maximum-topology, no-touch, session recovery, allocator watermark,
  CoreAudio malformed callback, performance semantics, and invalid ABI handle probes.
- Unsanitized Release routing maximum-topology probe.

### Unavailable or bounded evidence

- Windows/WASAPI compile, fake runtime, package, and physical-device acceptance were
  not run on this macOS host.
- Linux TSan CI and Linux `/proc/self/io` syscall evidence were not independently run;
  local Darwin TSan passed, while the direct Linux I/O guard is unavailable on Darwin.
- macOS ASan does not support `detect_leaks=1`; all sanitizer runs used
  `detect_leaks=0`. This is a host-tool limitation, not a leak verdict.
- CoreAudio tests used the available default endpoint. Explicit stable-UID hardware
  acceptance, Bluetooth/aggregate teardown, physical unplug, and external rate/buffer
  mutation were not run.
- External Clip Composer, FourTrack, and FreqFinder repositories were not available as
  verified source and are not assessed here.
- `build-release` is stale generated evidence (`0.9.1`) and was excluded from current
  test conclusions.

**fix first** (F-01 through F-05 and F-08/F-09 contract corrections), **refactor next** (demo/adapter package boundaries, ABI runtime state, worker fairness, and driver-manager lock scope), **defer** (optional ShmUI/analyzer policy, ASIO packaging, and other externally gated expansions).
