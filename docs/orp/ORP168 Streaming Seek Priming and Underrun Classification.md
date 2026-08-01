<!-- SPDX-License-Identifier: MIT -->

# ORP168 — Streaming Seek Priming and Underrun Classification

**Document type:** Transport contract correction and downstream handoff
**Version target:** Unreleased; SDK version remains `0.6.7`
**Status:** Implemented and verified on configured Debug transport/realtime gates
**Date:** 2026-08-01
**Request:** OCC176 [1]

---

## Scope

ORP168 corrects the existing `ITransportController::seekClip(ClipHandle, int64_t)` contract for registered streaming sources without changing its signature, vtable slot, C ABI, or FIFO/all-active-voice semantics. An accepted seek now has a reader-validated, pinned first-render working set before its command is published. Clip Composer adoption and SDK pinning remain deferred until a tagged release.

## Implementation

`StreamingClipSource` retains its four-page steady worker window and adds four fixed command-prime pages (`kNumPages == 8`). `PrimeReservation` is an eight-bit POD lease. It supports transactional control-thread priming across a bounded multi-page range, including extensions for loop starts and segment-program starts.

Every page now has an atomic guard:

- `0` is unpinned;
- positive values are command pins;
- `UINT32_MAX` is the transient worker-fill/audio-retirement claim.

The worker claims only steady-window FREE pages, decodes with `decodePage()` while the page remains unpublished, then release-publishes `start`. Command priming pins an already resident matching page in either pool; otherwise it claims only command-prime capacity, decodes every missing page under the reader mutex, and publishes none until the complete request succeeds. Failure releases only pins claimed by that request and leaves the caller token unchanged. Audio retirement cannot reclaim a positive-pin page; command release is fixed bounded atomic work after the render block or on control-thread rejection/teardown cleanup.

`prefill()` returns `SessionGraphError`: the audible page is mandatory and look-ahead remains best effort. For a Start or group-choke Start, it receives the command's `PrimeReservation` and pins the effective trim-IN page. A full steady window therefore uses available command-prime capacity rather than rejecting a valid refire. That lease remains immutable from command publication through the first source reads, then releases after the block; admission and post failures release it exactly once. Worker servicing still has access only to steady-window slots. Unprimed cache misses still return `false` from `read()` without blocking.

`ResamplingAudioFileReader` now propagates wrapped-reader failures from `produceUntil()` instead of treating them as EOF and flushing a synthetic silence tail. A failed wrapped seek leaves the prior resampler state and target position intact.

`TransportController` retains a control-allocated `SourceCommandLifetime` for each unread registered-source Start or Seek command. Seek preparation checks ring capacity before reader work; primes `4 * maxBlockFrames + 2` source frames; uses `max(requestedPosition, trimIn)` for a non-segment target below trim-IN because that is the renderer's first source read; retains loop-wrap and segment-program extensions; and publishes one existing Seek command. Start and Seek page reservations remain through every source read in the consuming callback and then release. Unread lifetimes and pending page primes block unregistration/replacement. Destruction stops the stream worker, releases unread command leases and retained render-block reservations, then destroys the registry.

## Public contract

`seekClip` still clamps to file bounds and applies one command at the next valid render boundary to every active voice. It now returns:

- `InternalError` before preparation when the bounded command ring is full;
- `NotReady` when no voice is published or fixed command-prime capacity is unavailable;
- a concrete reader/preparation error otherwise.

Every rejection is failure-atomic: it publishes no command, changes no cursor, emits no `ClipSeeked`, and synthesizes no `BufferUnderrun`. `seekToCuePoint` continues to delegate to this corrected path.

Existing registrations are not replaced in place. Registered-source start preparation failures propagate instead of using a source-less fallback. Active voices, unread registered-source Starts/Seeks, and pending streaming primes block unregister until clean ownership cutover.

## Rendered regression evidence

`streaming_seek_test` contains 17 contracts, including:

1. a 128-case matrix over 44.1/48/96/192 kHz engine rates, 64/127/512/1024-frame blocks, mono/quad layouts, 0.5x/2x rates, and native/mismatched file rates;
2. 16-bit position-coded WAV markers proving first post-seek audio is the CED-like target marker on every output channel with no underrun;
3. 4x page-boundary coverage, queued FIFO seeks, command-ring saturation, and first-block RT allocation/deallocation guards;
4. transactional read/seek/early-EOF failures, fixed-prime-capacity recovery, and concurrent worker/prime ownership coverage;
5. source-level steady-window exhaustion proving command-prefill uses a command-page bit, can read the requested page, and releases its reservation;
6. a transport Start/refire after steady-window exhaustion, proving acceptance, audible first render, zero `BufferUnderrun`, zero RT allocations/deallocations, and post-render reservation cleanup;
7. looping and non-looping pre-trim seeks after trim-IN eviction, proving the clamped trim-IN first block is audible with zero `BufferUnderrun` and zero RT allocation/deallocation violations;
8. loop-wrap trim-IN, segment transition, fade-overlap, queued Start, exact-EOF Seek, and unregister/replacement lifetime coverage.

Observed on this branch:

- `cmake --build build --target streaming_seek_test realtime_harness_test voice_state_tsan_test transport_controller_test clip_restart_test clip_seek_test -j2`: passed;
- `./build/tests/transport/streaming_seek_test`: 17/17 passed;
- `./build/tests/transport/realtime_harness_test --gtest_filter='RealtimeHarnessTest.Streaming*'`: 5/5 passed;
- `TSAN_OPTIONS=halt_on_error=1 ./build/tests/transport/voice_state_tsan_test`: 2/2 passed, with no ThreadSanitizer output;
- `ctest --test-dir build -V -R '(^streaming_seek_test$|^realtime_harness_test$|^transport_controller_test$|^clip_restart_test$|^clip_seek_test$|^transport_integration_test$)'`: 6/6 CTest executables passed; `realtime_harness_test` reported 8 passed and 1 platform-skipped non-streaming file-I/O test;
- `python3 tools/realtime_audit.py --fail-known-debt`: passed, `0 hard failures, 0 tracked debt findings`.

## Downstream boundary

This SDK-only change does not modify `/Users/chrislyons/dev/clip-composer`, create an OCC-specific API, retry hidden seeks, add a second player, suppress genuine underruns, or bump the SDK project version. Clip Composer should pin a tagged SDK release before adding its OCC176 CED OUT → IN → grid integration regression.

## References

[1] C. Lyons, “OCC176 — Streaming Seek Priming and Underrun Classification SDK Request,” Clip Composer engineering request, Jul. 30, 2026. Local source: `~/dev/clip-composer/docs/occ/OCC176 Streaming Seek Priming and Underrun Classification SDK Request.md`. [Accessed: Jul. 31, 2026].
