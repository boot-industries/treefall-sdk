# ORP254 Streaming Prefetch Realtime Sustain Fix

**Status:** Complete  
**Date:** 2026-09-06  
**Scope:** `StreamingClipSource` prefetch/prime realtime ownership, command-prime release lifetime, loop-restart page residency, and the OCC191 transport lockup record

## Problem

Clip Composer reported repeated transport lockups and `BUFFER UNDERRUN` events under sustained operator churn (`<`/`>`/`;`/`'` trim nudges, left-arrow live seeks, loop toggles, rapid restarts). Instrumented runs reproduced 7–13 underruns per stress session. Three distinct failure layers surfaced:

1. **Control-thread decode starvation.** `primeForCommand` decoded up to 4 pages synchronously on the message thread while holding `m_readerMutex`, blocking the 10 ms-polled `MediaStreamWorker` that must keep the resident window ahead of the real-time consumer. Any multi-second worker stall became an underrun because the window had only 2 pages of lookahead.
2. **Premature prime release.** Pending Start/Seek primes were released at the end of the first render block regardless of whether the primed page had been consumed. A competing command (seek/restart churn) landing in the same block retired the just-released page before the restart's first render, producing `READ MISS … resident=[]` and an underrun on the restart position.
3. **Loop-restart page not resident.** With loop enabled, the audio-thread loop boundary restarts playback at trim-IN instantly; it cannot block or prime. When the worker window sat at a distant demand, the trim-IN page was not resident and the loop-restart read missed.

A wiring gap compounded layer 3: the initial loop-anchor pin was attached only to `setClipLoopMode`, but the app toggles loop through `updateClipMetadata`, so the anchor never engaged during app testing.

## Decision

Three coupled fixes, delivered in two commits on `fix/streaming-prefetch-realtime-sustain` (PR #256):

1. **Worker-owned decode for attached sources.** `primeForCommand`/`prefill` on an attached source publish a demand request (`m_commandDemandStart`/`m_commandDemandFrames`) and wait on `m_fillCv` bounded by `kFillWaitTimeout` (2 s), with a synchronous decode fallback on timeout. The unattached (unit-test) direct-drive path is unchanged. New `serviceCommandDemand()` is a worker pass that fills every demanded page, pins them as command-owned, and publishes the completion mask. `ensurePreparedSourceLocked` attaches before priming so the worker is the sole decoder from the first pass. The resident window widened from 4 to 6 pages (1 behind + demand + 4 ahead ≈ 8.2 s @ 48 kHz) and the worker poll tightened from 10 ms to 2 ms, servicing command demand before steady refills.
2. **Consumption-aware prime hold.** `PendingStartReservation`/`PendingSeekReservation` gained `primedStart`. Release keeps a reservation until every voice on the source has read past the primed page (`sourcePosition >= primedStart + kPageFrames`). Released on consumption, never on block count.
3. **Loop-restart page anchor.** New public `StreamingClipSource::pinLoopAnchor(pos)` / `releaseLoopAnchor()` pin the page covering trim-IN resident for the entire time loop mode is enabled. Wired through **every** mutation path a consumer can use: `updateClipMetadata` (the path Clip Composer actually uses), `updateClipTrimPoints`, `setClipLoopMode`, and source creation when loop is already enabled. Trim-IN nudges re-anchor; loop-off unpins.

## Realtime and retention invariants

- The audio thread never primes, decodes, or blocks; it only reads resident pages. All new synchronization uses the existing atomic guard protocol (`0` free, `(0, kClaimed)` pinned, `kClaimed` claimed) plus `m_fillCv` notification from the worker.
- The control thread holds `m_fillMutex` only while waiting; it never holds it while taking `m_readerMutex` (lock-ordering rule). Timeout fallback decode happens after releasing the wait lock.
- `pinLoopAnchor` runs on the control thread; `releaseLoopAnchor` is guard-decrement only and is audio-thread safe.
- Command-prime capacity remains bounded by `kCommandPrimePages`; the widened `PrimeReservation::pageMask` is now `uint16_t`.

## Regression tests

Both new tests are proven to **fail without their fix** and pass with it:

- `StreamingSeekMatrixTest.LoopViaMetadataPinsAnchorForLoopRestart` — loop enabled through `updateClipMetadata`, seek near trim-OUT, render across the loop boundary; asserts zero underruns.
- `StreamingSeekMatrixTest.LoopNudgeChurnKeepsRestartPageResident` — trim-IN nudges while looping (the app's `<`/`>` churn) then seek + boundary render. Neutered anchor: 4 underruns with `READ MISS` on the nudged page; with fix: 0.

## Verification

- SDK full suite: 79/80 (the one failure, `coreaudio_driver_test`, fails at the base commit on this host — no audio input device).
- ASan+UBSan clean on `streaming_seek_test`, `realtime_harness_test`, `transport_controller_test` (22+8+17 tests).
- `cmake_find_package`, `realtime_static_audit`, `docs_path_audit`, and the ShmUI-JUCE manifest check all pass.
- App suite (clip-composer @ `occ191-sdk-verify`, submodule at this branch): 748 passed / 1 intentional CPU skip; `RapidFarForwardLiveSeeksDoNotUnderrun` passes.
- Real-device (operator-driven stress): after the fix, 114 accepted starts + 54 seeks + 4 loop toggles + nudge churn over 186 s with **zero underruns and zero read misses**. Operator: "I'm not able to break it."

## Lessons learned

1. **A single wiring path is not the contract.** Loop mode was settable through `setClipLoopMode`, `updateClipMetadata`, and `updateClipTrimPoints`; the app used a different one than the SDK author assumed. Any capability that changes audio-thread behavior must be wired through every public mutation path a consumer can reach, or the fix silently no-ops.
2. **Prime lifetime is a consumption contract, not a block-count contract.** Releasing a reservation after "a block was processed" is wrong when a competing command can retire the page in the same block. Pin until consumed, then release.
3. **Regression tests must be proven to fail without the fix.** The first loop-anchor test passed even with the anchor neutered (the seek path's own trim-IN prime masked the gap). A regression test that does not fail without its fix is not a regression test.
4. **Instrument before blaming.** `READ MISS` page-level instrumentation turned a vague "transport lockup" into three precise root causes in one session. Strip the instrumentation before delivery (the delivered branch contains none).
5. **Real-device stress is the acceptance gate.** SDK and unit suites cannot reproduce operator churn (rapid nudge + loop + seek interleavings). The operator's inability to break playback is the deliverable's proof, alongside the deterministic tests.

## Downstream adoption

Clip Composer should keep its submodule pin on `e1e5e255` (or later) and re-run the `occ191-sdk-verify` suite plus the manual stress sequence (trim nudges with loop on, far live seeks, rapid restarts) before any further transport work.
