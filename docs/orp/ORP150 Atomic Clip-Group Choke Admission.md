<!-- SPDX-License-Identifier: MIT -->

# ORP150 — Atomic Clip-Group Choke Admission

**Document type:** Public API implementation record and downstream guidance  
**Version target:** `0.5.1`  
**Status:** Implemented and focused gates verified  
**Date:** 2026-07-15

## Contract

`ITransportController::startClipWithGroupChoke(handle)` appends a host-neutral
public transport operation for metadata-owned exclusive groups. The control
thread resolves the registered clip and its immutable start context, then posts
one fixed-capacity SPSC command. It does not post one stop command per peer.

The audio-thread consumer first applies the firing handle's configured
`VoiceMode` and admits the voice. Start-event processing occurs before peer
mutation. Only after admission succeeds does the consumer start the normal
configured stop fade on active voices whose handle differs from the firing
handle and whose `ClipMetadata::routingGroup` matches. Other groups and all
voices of the firing handle remain unchanged. Surviving callbacks therefore
retain Start-before-peer-Stop order, but publication is not guaranteed when the
bounded callback ring overflows; hosts detect that history gap through ORP151
callback-loss telemetry.

Failure is all-or-nothing for peers:

- invalid/unregistered/unavailable source or control-side allocation failure is
  returned before command publication;
- a full command ring returns `SessionGraphError::InternalError` without
  publishing any part of the operation; and
- realtime voice-pool refusal attempts the existing
  `onActiveClipLimitReached` event and skips the choke. ORP151 callback-loss
  telemetry detects if the bounded event ring cannot publish that notification.

The method shares the existing single-control-producer contract. Hosts retain
policy outside the SDK when their grouping is not represented by registered
`ClipMetadata::routingGroup`; they must not emulate this atomic operation with a
sequence of `stopClip()` calls.

## Public-package evidence

The clean-prefix `find_package` consumer calls the method through
`std::unique_ptr<ITransportController>` while including only installed public
headers. A second installed consumer models an implementation of the preceding
interface, deliberately omits the appended method, remains concrete, and
observes the safe default `SessionGraphError::NotSupported`.

Focused rendered coverage exercises successful group isolation, queue-full and
voice-pool refusal, `MonoWithFadeOverlap` refire bounds, attempted callback
ordering, repeated saturation determinism, and transactional routing-metadata
rejection/success across both persistent and active state.

## Verification record

Observed in the isolated implementation worktree on 2026-07-15:

- focused transport selection: 4/4 CTest targets passed;
- `atomic_group_choke_test`: its six rendered contract tests passed;
- `cmake_find_package`: 1/1 passed, including the installed legacy custom
  implementation fixture;
- `realtime_static_audit`: 1/1 passed; and
- `docs_path_audit`: 1/1 passed.

The first package-gate attempt was correctly blocked because the configured
install tree had not yet built `inspect_session`; after the installable targets
were built, the clean-prefix configure/build/run fixture passed.
