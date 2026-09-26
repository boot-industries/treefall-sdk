<!-- SPDX-License-Identifier: MIT -->

# ORP264 Phase A Containment Completion

**Date:** 2026-09-26
**Scope:** Containment of ORP263 findings F-01 through F-05 in the Treefall SDK
**Status:** Phase A complete on branch `fix/orp263-phase-a-containment`; F-06
through F-09, R-01 through R-07, and ORP263 Phases B through D remain open

## Executive summary

Five release-blocking defects recorded in
[ORP263](ORP263%20SDK%20Resilience,%20Responsiveness,%20and%20Architecture%20Review.md)
are contained. Each fix landed with a regression that was observed failing
against the pre-fix tree and passing after. The stable C ABI 1.0 function tables
and struct layouts are unchanged, the `Orpheus`/`Treefall` compatibility surfaces
remain intact, and no support-matrix tier is promoted by this work.

| Finding | Defect | Containment |
|---|---|---|
| F-01 | A future-schema primary document was silently replaced by an older `.bak` | Typed internal exception distinguishes unsupported schema from corruption |
| F-02 | Stable `TrackId`/`ClipId` values were reused after rollback and restore | Snapshot carries allocator watermarks; restore is monotonic |
| F-03 | The C ABI dereferenced fabricated, stale, foreign, and destroyed handles | Address-keyed runtime registry validates every session handle |
| F-04 | CoreAudio cleared caller output before validating the `AudioBufferList` shape | Shape validation completes before the first write |
| F-05 | Routing's malformed-output rejection mutated meter publication state | Rejection paths return without any mutation |

## Evidence baseline

Baseline `d387889390d19aeee485dab9abeb183b65d71d4e` on `main`, clean tree. The
`build/` and `build-release/` trees present in the workspace are stale
2026-09-10 generated evidence and were not used for any result below. All work
used a fresh `build-orp263` tree.

## Verification completed

| Gate | Result |
|---|---|
| `orpheus_tests` | Passed |
| `routing_matrix_test` | Passed |
| `coreaudio_driver_test` | Passed (no skips) |
| `abi_link` | Passed — cross-DSO registry shared |
| `cmake_find_package` | Passed |
| `realtime_static_audit` | Passed |
| `docs_path_audit` | Passed |

## Per-finding record

### F-01 — unsupported future schema rejected before backup fallback

`src/core/session/json_io.cpp` wrapped the primary parse in a single
`catch (const std::exception&)`, making `ParseSession`'s unsupported-schema
`std::runtime_error` indistinguishable from corruption. A new internal
`UnsupportedSessionSchema` type carries that condition out of the parser, and
`LoadSessionWithRecovery` catches it before the generic handler. Catch order is
load-bearing because the type derives from `std::runtime_error`.

Pre-fix, `SessionRoundTrip.FutureSchemaPrimaryNeverFallsBackToBackup` reported
`Expected: ... throws an exception of type std::runtime_error. Actual: it throws
nothing` for both `LoadSessionWithRecovery` and `LoadSessionFromFile`. After the
fix the test passes, and the existing corrupt-primary recovery test
`AtomicSaveKeepsBackupAndRecoversCorruptPrimary` is unmodified and still passes,
so genuine corruption still resolves to the backup.

### F-02 — allocator watermarks monotonic across rollback and restore

`SessionGraphSnapshot` is now schema 2 and appends `next_track_id_raw` and
`next_clip_id_raw`. `restore_unchecked` seeds each rebuilt allocator with
`max(snapshot watermark, live watermark)`, and the existing `reserveThrough`
loop supplies `max(extant) + 1`. Schema 1 snapshots are still accepted; their
watermarks are derived from the IDs they contain. The exact-version check
became a range check, so schema 0 and schema `kSchemaVersion + 1` are still
rejected.

Pre-fix the regressions showed direct ID reuse: `actual: 2 vs 2` after
transaction destruction, `3 vs 3` and `1 vs 2` across the removed-ID sequence, and
`2 vs 2` after restoring an older snapshot. All pass after.

### F-03 — every C ABI session handle validated through a shared registry

A new `orpheus_abi_runtime` target owns an address-keyed
`std::unordered_set` guarded by a `std::mutex`. The three ABI libraries link it
`PRIVATE`, so in a shared build they all resolve one registry instance and a
handle created through one validates in the others. `ToSession`, the unchecked
cast, was deleted outright; every session entry point now resolves through
`ResolveSessionHandle`, and `SessionDestroy` unregisters and deletes under one
lock so a null, unknown, or already-destroyed handle is an idempotent no-op.
`SessionGetTransportState` moved inside `GuardAbiCall`.

Pre-fix, the fabricated-handle regression aborted the process:
`AddressSanitizer: SEGV session_graph.cpp:501 in
orpheus::core::SessionGraph::set_tempo(double)` with exit status 134. The
cross-DSO `abi_link` consumption checks are after-only by construction: they
reference behavior that has no pre-fix observable, because the pre-fix code
crashed rather than returning a status. The runtime is an internal artifact and
is deliberately absent from `installed-targets.json.in`, the installed-target
manifest assertion, and the documented `Orpheus::` target list.

### F-04 — CoreAudio output shape validated before the first write

The leading `memset` loop over advertised `mDataByteSize` was deleted from
`CoreAudioDriver::renderCallback`. Validation now runs first and checks four
properties: exact buffer-count equality against the configured lane count, one
channel per buffer, non-null `mData`, and sufficient byte capacity. Exact count
equality plus the `mNumberChannels != 1u` clause is what rejects a packed or
interleaved list. After validation, the callback clears exactly
`native_frame_bytes` per configured lane. A test-only `renderCallbackForTesting`
forwarder was added; it changes no behavior.

Pre-fix, `CoreAudioDriverTest.MalformedOutputShapeIsNoTouch` reported six
sentinel failures and `ValidShapeClearsExactlyFrameBytesWhenNotRunning` reported
512 over-cleared samples past the 512-frame window. Both pass after, with the
test suite running on this host's default output endpoint and no skips.

### F-05 — routing rejects malformed output with no state mutation

The four `std::memory_order_release` stores that wrote `Unmeasured` and cleared
the coherence flag on the two rejection paths were removed. Control refresh,
scratch mutation, and meter publication remain strictly after the validation
loop. The comparison helper in the test compares published fields rather than
object representation, because the trivially-copyable snapshot's padding is
neither guaranteed to be initialized nor preserved by a copy.

Pre-fix, `RoutingMatrixTest.MalformedOutputShapeIsNoTouch` reported
`Value of: meterSnapshotsMatch(measured, after_null_master) Actual: false` and
the same for the null-lane case. All 58 routing tests pass after.

## Deferred work

- ORP263 F-06 through F-09 are not addressed by this sprint.
- ORP263 R-01 through R-07 are not addressed by this sprint.
- ORP263 Phases B, C, and D remain open.
- ORP170 candidate and stable promotion and Windows/WASAPI promotion remain
  gated on the external acceptance records enumerated in ORP173 and ORP143 §7.
- A generation-token session handle representation, which would additionally
  reject a destroyed handle whose address is later reused, is a separate change.

## Unavailable or bounded evidence

1. The address-keyed ABI registry cannot reject a destroyed handle whose
   `SessionGraph` address is later reused by a new session; a generation-token
   handle representation is a separate, out-of-scope change.
2. In a static ABI build the registry is per final link unit, so
   cross-shared-library handle sharing is provided only when the ABI libraries
   are built shared (the default).
3. `SessionGetTransportState` is now inside `GuardAbiCall`, but no throwing
   input was inducible because `SessionGraph::transport_state()`
   (`src/core/session/session_graph.cpp:531-535`) is a pure scalar read; the
   guard is defense-in-depth, and the reachable crash class is proven by the
   fabricated, foreign, and destroyed-handle assertions instead.
4. Windows shared-build link and DLL staging for the new runtime artifact are
   evidenced only by the Windows CI legs, not by a local run on this macOS
   host.
5. Linux TSan evidence for the new registry surface was not collected on this
   macOS host; the concurrency regression is present and runs in the suite, but
   the sanitizer verdict comes from CI.
