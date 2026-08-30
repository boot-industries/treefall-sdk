# ORP252 Tagged Start Settlement Contract

**Status:** Complete  
**Date:** 2026-08-30  
**Scope:** Public C++ transport contract, realtime publication, callback identity, active-voice reconciliation, and OCC application-history inspection

## Problem

A `ClipHandle` identifies a registered clip slot, not one accepted start request. A delayed start rejection or stop callback can therefore refer to the same slot and even a recycled voice ID as a newer command. Downstream playout controllers need a durable request identity that survives command processing, callback loss, fade overlap, and voice-ID reuse without inferring command ownership from slot order.

## Decision

The transport now accepts a host-provided `StartRequestTag` on `startClip` and `startClipWithGroupChoke`. Zero remains the explicitly untagged path. Nonzero tags are copied into the accepted voice, callback events, active-voice publication, and a retained start-settlement snapshot.

The public settlement contract is:

- `StartSettlementOutcome::{Started, ActiveVoiceLimitRejected}`;
- `StartSettlementRecord`, containing sequence, request tag, handle, voice ID, command-processing position, and outcome;
- schema-1 `StartSettlementSnapshot`, retaining 64 ordered records plus oldest/latest sequence, overwrite count, and sequence-exhaustion state;
- `ITransportController::getStartSettlementSnapshot()` as a lock-free any-thread value query;
- schema-2 `ActiveVoiceSnapshotEntry::newestStartRequestTag` for exact active-voice reconciliation.

`ITransportCallback::onClipStarted`, `onClipStopped`, `onClipLooped`, and `onActiveClipLimitReached` now carry the same request tag. Every in-repository implementation migrated to the new signatures; no legacy callback alias remains.

## Realtime and retention invariants

The audio thread is the sole settlement writer. Every snapshot header and record field is atomic. Publication uses a bounded odd-revision, payload, even-revision sequence; readers accept only equal even revisions. The writer performs fixed work bounded by the 64-record capacity and performs no allocation, lock, logging, or I/O.

Only nonzero tags consume settlement sequence or capacity. Sequence begins at 1. A full snapshot drops the oldest record and saturating-increments `overwrittenCount`. The command that advances `UINT64_MAX - 1` publishes sequence `UINT64_MAX` and marks the snapshot exhausted. Later tagged commands are refused before voice mutation and publish neither a settlement nor callback; untagged commands remain available. Sequence never wraps.

A successful start records its nonzero voice ID. Voice-pool refusal records voice ID zero and `ActiveVoiceLimitRejected`. In-place mono restarts replace the active voice's request tag and chronological start ordinal, so `newestVoiceId` and `newestStartRequestTag` always describe the same accepted start. Stop and loop callbacks copy the tag from the exact retiring or looping voice before voice-array compaction.

## OCC application-platform addition

`orpheus::UndoManager` now exposes non-mutating `peekUndoCommand()` and `peekRedoCommand()` queries. They return the command that the next history action would apply, or `nullptr` when unavailable. The index is not advanced. This lets an application authorize a playlist history target before invoking undo or redo.

## Downstream adoption

Clip Composer must allocate nonzero tags monotonically, pass them on every main transport start, consume settlement records in sequence, and match lifecycle events by tag plus generation and voice ID. Callback delivery remains useful for presentation and stop events, but a slot-only callback must not settle a tagged start.

A transport rebuild is the recovery boundary for retention overflow or sequence exhaustion. A fresh transport begins with schema 1, an empty settlement snapshot, and zero cursors.

## Verification

Observed on 2026-08-30 in the existing Debug tree:

```text
cmake --build build --target transport_controller_test callback_loss_telemetry_test --parallel 6
ctest --test-dir build --output-on-failure -R '^(transport_controller_test|callback_loss_telemetry_test|realtime_static_audit)$'
```

All three focused CTest cases passed. Contracts cover empty schema-1 publication, tag-zero exclusion, ordered tagged positions, exact callback identity, voice-limit rejection, 64-record overwrite, sequence saturation, voice-ID reuse, schema-2 newest-tag publication, and concurrent snapshot coherence. `realtime_static_audit` reported no violation in the tagged command or settlement path.
