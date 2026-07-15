<!-- SPDX-License-Identifier: MIT -->

# ORP151 — Callback Loss Telemetry and Active Voice Reconciliation

**Document type:** SDK public-contract completion record and host adoption guide  
**Version target:** `0.5.1`  
**Status:** Implemented and verified in the working tree; release pending  
**Date:** 2026-07-15

---

## 1. Problem and scope

The transport already used a fixed audio-to-control SPSC callback ring and
correctly refused to block the audio thread when that ring filled. The only loss
signal was a private counter. An installed host could drain the callbacks it did
receive and mistake the resulting model for a complete stream, especially when
the final events in a burst were the events dropped.

ORP151 makes that loss public and reconcilable without changing callback order,
callback signatures, or realtime ownership:

- every attempted transport callback event receives a monotonic lifetime
  sequence before the ring capacity check;
- successful posts and ring-full drops update coherent cumulative telemetry;
- hosts can poll the telemetry even when no later event reaches the ring; and
- hosts can replace an incomplete callback-derived model with a coherent,
  fixed-capacity aggregate of the voices that actually survived.

The change does not add a second callback queue, storage policy, logging, or an
application-specific ledger.

---

## 2. Installed public contract

`include/orpheus/transport_controller.h` now publishes these fixed-layout,
trivially-copyable contracts:

```cpp
struct TransportCallbackTelemetry {
  uint32_t schemaVersion;
  uint32_t reserved;
  uint64_t lastAttemptedSequence;
  uint64_t lastPostedSequence;
  uint64_t cumulativeDroppedCount;
  uint64_t lastDroppedSequence;
  uint64_t activeVoiceSnapshotSequence;
};

struct ActiveVoiceSnapshotEntry {
  ClipHandle handle;
  uint32_t activeVoiceCount;
  uint32_t newestVoiceId;
  PlaybackState state;
  // fixed-layout flags and newest voice sample fields
  TransportPosition newestPosition;
};

struct ActiveVoiceSnapshot {
  uint32_t schemaVersion;
  uint32_t entryCount;
  uint32_t totalActiveVoiceCount;
  uint32_t reserved;
  uint64_t publicationSequence;
  std::array<ActiveVoiceSnapshotEntry, kActiveVoiceSnapshotCapacity> entries;
};
```

The appended interface queries are:

```cpp
virtual TransportCallbackTelemetry
getCallbackDeliveryTelemetry() const noexcept;

virtual ActiveVoiceSnapshot getActiveVoiceSnapshot() const noexcept;
```

The new methods have default empty implementations, so a recompiled custom
`ITransportController` implementation remains source-compatible without adding
overrides. This is not a C++ binary-compatibility guarantee. The repository
guarantees stable C ABI 1.0; `ITransportController` is a C++ virtual interface.
All C++ consumers and custom implementations must recompile for `0.5.1` before
calling the appended slots. Existing callback signatures and retained-event
dispatch order are unchanged.

### 2.1 Callback telemetry semantics

- Sequence, drop, and watermark fields are zero when a controller is
  constructed; `schemaVersion` is the published schema constant.
- `lastAttemptedSequence` advances before each callback-ring capacity check.
- `lastPostedSequence` is the attempted sequence of the most recent event that
  entered the ring. It can jump across a dropped range after the ring recovers.
- `cumulativeDroppedCount` advances exactly once per ring-full rejection.
- `lastDroppedSequence` identifies the most recent rejected attempted sequence.
- `activeVoiceSnapshotSequence` is the reconciliation watermark published at
  the same epilogue. When telemetry containing a drop is visible, the voice
  snapshot for at least this publication sequence is already visible.
- `processCallbacks()` does not reset or reduce any field.
- Values belong to one controller lifetime and reset only when a new controller
  is constructed.
- Public counters saturate at `UINT64_MAX`; they never wrap to a smaller value.

The drop counter, rather than arrival of a later callback, is the authoritative
loss signal. A host therefore detects a dropped final burst by polling
`getCallbackDeliveryTelemetry()`.

### 2.2 Active voice snapshot semantics

`ActiveVoiceSnapshot` has capacity 32, matching the transport's hard active-voice
ceiling. Entries `[0, entryCount)` have distinct, nonzero `ClipHandle` keys.
`totalActiveVoiceCount` includes normal voices and fading tails.

Each entry aggregates all surviving voices for one handle:

- `activeVoiceCount` is the exact surviving count;
- `state` is `Playing` when any voice is playing and `Stopping` only when all
  voices for the handle are stopping; and
- the newest fields come from the voice with the greatest transport start
  sample; when starts share that sample, the later accepted start wins via an
  internal wrap-aware chronological ordinal rather than numeric voice-ID order.

`publicationSequence` starts at zero and advances after each completed
`processAudio()` block. A returned value never mixes fields from different
publications.

Voice IDs remain nonzero across `uint32_t` rollover. The bounded audio-thread
allocator skips zero and every ID still owned by an active voice. A separate
`uint64_t` start ordinal uses serial-number arithmetic across wrap. Before any
surviving pair could become separated by half the serial range, the fixed live
set is rank-rebased in place. No allocation or lock is introduced, and removal
or slot compaction preserves both identities.

---

## 3. Realtime and concurrency design

The audio thread remains the sole event and snapshot producer. Callback
telemetry uses scalar `uint64_t` atomics. The active snapshot uses fixed arrays of
lock-free scalar atomics. Both use an atomic publication revision so a
non-realtime reader retries when it overlaps a write. Every payload field is
atomic; no plain-struct overwrite races with a reader.

At the common `processAudio()` epilogue, the producer publishes the resulting
active-voice snapshot first and callback telemetry second. A host that observes
telemetry publication therefore cannot accept a pre-event snapshot as the
reconciliation state. `activeVoiceSnapshotSequence` makes that ordering
testable rather than implicit.

Publication is bounded by the configured maximum of 32 active voices. It does
not allocate, lock, call a host callback, perform I/O, or log. Compile-time
checks require the scalar atomic widths used by publication to be always
lock-free on a supported build target.

`getCallbackDeliveryTelemetry()` is an any-thread lock-free query.
`getActiveVoiceSnapshot()` is a lock-free, non-realtime query: it may retry a
concurrent publication and returns the fixed array by value. Callback draining
remains the single control-thread consumer of the event ring.

---

## 4. Host reconciliation guidance

An installed host should retain its last observed cumulative drop count. At each
message-thread pump:

1. poll `getCallbackDeliveryTelemetry()`;
2. call `processCallbacks()` to dispatch retained events;
3. poll telemetry again;
4. if `cumulativeDroppedCount` increased relative to the host baseline, mark the
   callback-derived interval incomplete; and
5. obtain `getActiveVoiceSnapshot()` and reconcile handle presence, per-handle
   voice count, aggregate state, and newest sample position before presenting a
   healthy model again.

For a stable reconciliation observation while audio is running, poll telemetry
and then copy the active snapshot. Accept the copy only when
`snapshot.publicationSequence >= telemetry.activeVoiceSnapshotSequence`;
otherwise retry. Poll telemetry again after callback dispatch to detect a drop
that occurred during the pump. The host stores the new cumulative value as its
own baseline; it never asks the SDK to reset counters.

A reconciled snapshot describes current surviving state. It does not recreate
which dropped historical callbacks occurred or authorize a host to fabricate
ledger rows. Hosts that persist lifecycle history must retain the detected gap
as an integrity condition and use their own policy for indeterminate history.

---

## 5. Verification evidence

Observed on macOS arm64, Debug, with realtime support enabled:

- `callback_loss_telemetry_test`: **6/6 passed**; the cases force 302
  attempted events without draining, observe exactly 255 retained and 47
  dropped with last dropped sequence 302, prove the reconciliation watermark,
  verify a later retained sequence 303 without counter reset, verify the
  zero-drop path, hammer concurrent coherent publication, aggregate one
  handle across playing/fading voices and compaction, cross `uint32_t`
  voice-ID rollover without zero or active-ID collision, and prove that a
  same-sample start after ordinal/voice-ID wrap owns every newest field;
- focused transport CTest set (`transport_controller_test`,
  `callback_queue_stress_test`, `callback_loss_telemetry_test`, and
  `voice_state_tsan_test`): **4/4 passed**;
- installed clean-prefix `cmake_find_package`: **1/1 passed**; one consumer
  renders a real start and observes nonzero concrete telemetry/snapshot state,
  while a complete pre-ORP151-style custom `ITransportController` compiles and
  runs using the new default methods;
- `realtime_static_audit`: **1/1 passed**;
- `realtime_harness_test`: **1/1 passed**; and
- `docs_path_audit`: **1/1 passed**.

No release, version, tag, or downstream application action is claimed by this
record.
