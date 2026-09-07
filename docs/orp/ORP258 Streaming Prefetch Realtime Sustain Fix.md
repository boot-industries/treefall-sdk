# ORP258 Streaming Prefetch Realtime Sustain Fix

**Status:** Remediated on `fix/streaming-prefetch-realtime-sustain`; the affected Debug targets compile, while runtime and downstream CI gates remain unrun by the approved implementation plan.
**Date:** 2026-09-07
**Remediation commit:** `8feafb21`
**Scope:** Worker-owned streaming decode, transactional command-page ownership, source-scoped prime retention, failure-atomic loop-anchor mutations, and the OCC191 transport lockup record.
**Historical record:** Renumbered from ORP254 after the metering branch retained mainline ORP254 and ORP255 records. PR #256, branch `fix/streaming-prefetch-realtime-sustain`, and historical commits `994aa680`, `e1e5e255`, and `21360eb5` remain unchanged identifiers.

## Problem

The original PR #256 implementation moved attached decoding to the worker and held Start/Seek primes until consumption, but review found ownership races at the worker/control boundary:

1. A worker could finish a decode after the two-second wait and publish a separate pin after the control thread began synchronous fallback.
2. A failed multi-page request could clear a page that became resident after the control-thread scan.
3. Loop-anchor ownership was represented only by a start value, so failed re-anchors and queued mutations could release the wrong page or retain superseded pins.
4. Pending Start/Seek reservations compared unrelated active voices by position alone.
5. Persistent trim/loop/metadata state could be changed before command-ring admission, leaving the registry inconsistent when the SPSC ring was full.

## Decision

The remediated branch keeps the accepted worker-owned design and adds explicit ownership transactions.

### Command-fill transaction

`StreamingClipSource` now owns one fixed-size `CommandFillRequest` protected entirely by `m_fillMutex`:

- `uint64_t generation` identifies the request;
- `Pending`, `Filling`, `Succeeded`, `Failed`, `CancelRequested`, and `Cancelled` states make every handoff explicit;
- `residentPinMask` distinguishes READY-page pins from `freshPageMask` pages decoded into the command pool;
- fresh page starts remain unpublished until the final matching-generation commit;
- timeout changes `Pending` directly to `Cancelled`, or `Filling` to `CancelRequested`, and waits for worker acknowledgement before synchronous fallback;
- a concurrent successful final commit wins the timeout race and is adopted exactly once.

The worker checks the matching generation/state before each page and after every decode without holding both `m_fillMutex` and `m_readerMutex`. Late READY pages lose only the worker's guard on rollback; their page contents remain resident. Command-pool pages with zero guards can be reclaimed only outside the active steady window.

The audio thread reads no command-fill handshake field and performs no lock, allocation, decode, or blocking operation.

### Loop-anchor ownership

`LoopAnchorTransition` carries previous and replacement page indices/starts with an inactive sentinel. Preparing a transition primes exactly one page, transfers that command pin to loop-anchor ownership, and advances the control-thread logical anchor. Commit releases only the previous exact page; rollback releases only the replacement and restores the previous anchor. Queued transitions therefore form an ordered ownership chain, and unread commands roll back in reverse order during controller destruction.

`TransportCommand` carries the source and transition. Its existing `SourceCommandLifetime` keeps the registry entry alive until the unread command is consumed or rolled back. New long sources attach to the worker, establish their required anchor, and complete prime/prefill before publication into `AudioFileEntry`; failed preparation leaves only a weak worker reference.

### Failure-atomic controller mutations

`updateClipTrimPoints`, `setClipLoopMode`, and `updateClipMetadata` prepare any anchor transition before posting but commit persistent metadata only after `postCommand()` succeeds. A full ring rolls back the exact transition and releases the retained source lifetime. The audio consumer commits transitions in command order.

Pending Start and Seek release checks now require both the existing handle condition where applicable and `clip.source.get() == pending.source`, preventing an unrelated active source from retaining another source's lease.

## Regression coverage

`tests/transport/streaming_seek_test.cpp` contains deterministic ownership regressions for:

- `StreamingClipSourcePrimeTest.TimeoutCancellationAcknowledgesBeforeFallback`
- `StreamingClipSourcePrimeTest.WorkerRollbackPreservesLateResidentPage`
- `StreamingClipSourcePrimeTest.FailedReanchorRetainsExactOldPage`
- `StreamingClipSourcePrimeTest.LoopAnchorReleasePreservesCommandPin`
- `StreamingSeekMatrixTest.QueueFullTrimUpdatePreservesOldLoopAnchor`
- `StreamingSeekMatrixTest.QueueFullLoopTogglePreservesOldLoopAnchor`
- `StreamingSeekMatrixTest.QueueFullMetadataUpdatePreservesOldLoopAnchor`
- `StreamingSeekMatrixTest.UnrelatedVoiceDoesNotRetainStoppedSourceStartPrime`
- `StreamingSeekMatrixTest.QueuedLoopNudgesReleaseSupersededAnchorsInOrder`

`FaultReader` has disabled-by-default read-call counting, one-call blocking, and selected-later-call failure. `StreamingClipSourceTestAccess` is test-only friend access for deterministic attachment and worker scheduling; it adds no production state or behavior.

The historical loop tests from the original ORP254 record remain in the branch. Their prior app/device and CTest evidence is historical and was not rerun for this remediation.

## Verification boundary

Observed on this branch:

```text
cmake -S . -B build-orp256 -DCMAKE_BUILD_TYPE=Debug \
  -DORPHEUS_ENABLE_REALTIME=ON \
  -DORPHEUS_ENABLE_EXTENDED_TESTS=ON

cmake --build build-orp256 --parallel --target \
  orpheus_transport streaming_seek_test multichannel_transport_test \
  realtime_diagnostics_test realtime_harness_test
```

Configuration and compilation completed. The repository names the executable containing `realtime_telemetry_test.cpp` `realtime_diagnostics_test`; no `realtime_telemetry_test` target exists.

Per the approved blocker plan, no CTest suite, runtime test executable, formatter, sanitizer run, CI pipeline, or merge was executed. The following downstream gates remain explicitly unrun and must be supplied by the integrating CI agent:

```text
ctest --test-dir build --output-on-failure \
  -R '^(streaming_seek_test|transport_controller_test|multichannel_transport_test|realtime_telemetry_test|routing_matrix_test|realtime_harness_test)$'

ctest --test-dir build --output-on-failure \
  -R '^(realtime_static_audit|docs_path_audit|version_contract|cmake_find_package|cmake_package_rejects_previous_minor|shmui_juce_manifest)$'
```

The downstream Release, sanitizer, and TSan requirements remain the exact gates specified by the plan: `RealtimeHarnessTest.MaxTopologySamplePeakAndTruePeakMeetDeadline`, `RealtimeHarnessTest.MaxTopologyTelemetryMeteringIsAllocationFree`, and `VoiceStateTsanTest.HammerQueriesUnderConcurrentRender` plus the complete `streaming_seek_test` executable. No local result is represented as evidence for those gates.

## Historical downstream adoption

Clip Composer's prior submodule pin and the original operator stress report remain historical PR #256 evidence. Downstream consumers must rebase this branch after PR #254's metering head lands, run the integrated routing/metering and streaming gates, and record the resulting SDK pin separately.
