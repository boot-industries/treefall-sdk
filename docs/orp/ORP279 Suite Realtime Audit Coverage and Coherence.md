# ORP279 Suite Realtime Audit Coverage and Coherence

**Change ID:** ORP-SUITE-20261103-001

**Date:** 2026-11-03

**Status:** Implementation recorded; final suite qualification pending.

## Scope and change

Against frozen SDK main `898ae63`, commit `2c54382` expands `tools/realtime_audit.py` from named render/command entry points to per-block callees, including telemetry publication, transport events and streaming cached reads. Its optional `--include-adjacent` scan covers FreqFinder's analysis enqueue. Eight audit tests cover discovery and regressions; this is tooling coverage, not a proof that every reachable callback is realtime-safe. No SDK API, consumer pin or ShmUI mirror changed.

## Historical evidence

The prior audit missed injected `std::lock_guard` mutations in `publishFromRealtime`, `postTransportEvent` and `StreamingClipSource::readCached`; the expanded audit detected them. On base FreqFinder `b10ddc2` the adjacent scan reported two hard failures at enqueue's event signal; it was clean against the FreqFinder sprint fixes. The checkpoint records passing audit unit tests, strict realtime audit, documentation path audit and ShmUI manifest check. Rehearsals recorded FreqFinder 230/230 and FourTrack 340/340 against SDK changes; Clip Composer main had 1224/1225 with a separately fixed load-dependent swatch test. These are historical runs, not final qualification of this documentation revision. Durable evidence: `/Users/nesbitt/dev/.suite-audit-261103/reports/`, especially `sdk-audit-mutation.txt`, `rt-audit-adjacent-before.txt` and `rt-audit-adjacent-after.txt`; checkpoint at `../CHECKPOINT.md` relative to that reports directory.

## Final local SDK gate

The coordinated final local build and serial CTest run recorded 85 passed,
0 failed and one skipped out of 86 in 366.28 seconds; the skipped test was
`realtime_render_deadline_distribution`. The run included package, docs audit,
realtime static/unit and manifest checks (durable log
`/Users/nesbitt/dev/.suite-audit-261103/logs/sdk-final-ctest.log`). Separately,
`python3 tools/suite.py validate --json` reported valid, while
`python3 tools/suite.py status --workspace-root /Users/nesbitt/dev --json`
returned nonzero with checkout, pin, token-hash and provenance drift
(`/Users/nesbitt/dev/.suite-audit-261103/logs/sdk-final-suite-status.json`).
Validation is not a clean suite status.
Quiet-machine consumer rehearsal: Clip Composer main `990d378` with sprint
SDK `2c54382`, serial CTest with a private home, recorded 1207 passed,
0 failed and 18 skipped of 1225 in 679.89 seconds. Durable log:
`/Users/nesbitt/dev/.suite-audit-261103/logs/cc-main-sprint-sdk-serial.log`.
This does not erase the earlier 1224/1225 run and its swatch failure above.


The adjacent audit against the actual `/Users/nesbitt/dev` siblings failed on
FreqFinder main `AnalysisEngine.cpp:284,290` event signals and reported one
existing Clip Composer debt at `AudioEngine.cpp:4228`. The clean sprint-source
overlay result above is historical and is not a pass against current main.
Neither the skipped deadline gate nor downstream, hosted and hardware
qualification is established by the local SDK CTest run.

## Findings and limits

- F-01 writer release/acquire ordering was not shown defective, but unbounded reader retries remain a contract concern; bounded retries need an explicit unavailable/stale result.
- F-02 rejected `processAudio` blocks are silent; observable rejection requires a telemetry contract decision. F-03 checked time-domain invalid inputs would change existing behavior. F-09 clock-domain propagation remains ORP268 requirement A and needs API/schema work. These are documented, not implemented.
- F-04 installed CoreAudio linkage was disproved by the fixture's linked driver symbols; F-07 settlement publication cost was modeled at 169 ns optimized / 1.3 µs unoptimized per tagged start, not a suite latency qualification. F-08 concerns allocation failure only.
- The suite manifest was observed stale; SDK PR #278 also edits it. FourTrack's SDK gitlink `ac3d824` was observed at the open PR #278 head, not SDK main. Neither manifest nor pins were changed; integration depends on the eventual PR merge shape.

## Outstanding qualification

Coordinated downstream qualification, the skipped realtime deadline test, hosted CI, hardware and timing under load remain open. The suite integration record is OCC241; sibling records are SHM031, FTR107 and FRQ051.
