# ORP136 TODO and Incomplete-Feature Triage

**Status:** Triage / Hand-off — catalogue only, no code changes in this doc
**Author:** Cleanup sprint, 2026-07-10
**Scope:** In-tree `TODO` markers under `src/` and `adapters/`
**Severity:** Mixed — includes at least one correctness bug (see RT-1)
**Related:** ORP135 (minhost decomposition), routing/scene subsystems

---

## Context

A cleanup-sprint audit found **10 `TODO` markers** in the C++ core. These are not
formatting debt — several flag **incomplete features or latent correctness
bugs** that need owning-team scheduling, not a blind refactor-pass fix. This doc
catalogues each with a severity and a suggested owner so the work can be
scheduled deliberately. No `TODO` text or code is changed here.

Line anchors are as of 2026-07-10; verify before acting.

## Findings

### Correctness / behaviour bugs (schedule first)

| ID | Location | Marker | Impact |
|----|----------|--------|--------|
| **RT-1** | `src/core/routing/routing_matrix.cpp:437` | `snapshot.timestamp_ms = 0; // TODO: Get actual timestamp` | Routing snapshots always report a **0 ms timestamp**. Any consumer that time-orders or ages snapshots (metering UI, logging) gets wrong/constant timing. Likely a real bug, not just a stub. |
| **RT-2** | `src/core/routing/routing_matrix.cpp:652` and `:720` | `// TODO: Proper stereo metering would meter both channels` (×2) | Stereo metering meters **one channel only** → understated/incorrect level readings for stereo material. Two sites, presumably peak + RMS paths. |

### Incomplete features (scoped work, need API extension)

| ID | Location | Marker | Notes |
|----|----------|--------|-------|
| **SC-1** | `src/core/session/scene_manager.cpp:232` | `// TODO: Capture clip assignments from SessionGraph.` | Scene capture does not persist clip assignments. |
| **SC-2** | `src/core/session/scene_manager.cpp:280` | `// TODO: Stop all playback (requires ITransportController reference)` | Scene recall cannot stop playback — needs a transport reference wired into `SceneManager`. |
| **SC-3** | `src/core/session/scene_manager.cpp:312` | `// TODO: Restore clip assignments (requires SessionGraph API extension)` | Counterpart to SC-1; blocked on a `SessionGraph` API addition. |
| **IO-1** | `src/core/audio_io/audio_file_reader_libsndfile.cpp:253` | `// TODO: Implement SHA-256 hashing` | Content hashing for determinism/integrity is stubbed. Relevant to the offline-first / bit-identical guarantees; decide whether it is required for a shipped feature or aspirational. |

### Performance / integration (lower priority)

| ID | Location | Marker | Notes |
|----|----------|--------|-------|
| **PF-1** | `src/core/audio_io/waveform_processor.cpp:163` | `// TODO: Pre-compute LOD pyramid at multiple resolutions` | Waveform LOD optimisation; UI responsiveness, not correctness. |
| **TC-1** | `src/core/transport/transport_controller.cpp:1123` | `// TODO: Report error (too many active clips globally)` | Global active-clip cap is enforced but the error is swallowed — a diagnostics gap, confirm it is not silently dropping clips. |
| **CA-1** | `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:272` | `// TODO: Get active clip count from transport controller (for now, use 0)` | Driver hard-codes active-clip count to 0; affects any load-based logic in the CoreAudio path. macOS-only. |

## Suggested routing

- **RT-1, RT-2** → routing/DSP owner. Treat RT-1/RT-2 as bugs; each wants a
  regression test in `tests/` (routing snapshot timestamp; stereo meter on a
  known-asymmetric stereo signal).
- **SC-1..SC-3** → session/scene owner; SC-3 gated on a `SessionGraph` API
  addition, so scope SC-1/SC-3 together.
- **IO-1** → audio-io owner; first a product decision (is SHA-256 hashing a
  committed feature?), then implement or delete the stub + comment.
- **PF-1, TC-1, CA-1** → schedule opportunistically; TC-1 and CA-1 are worth a
  quick confirm that neither silently drops data.

## Verification (per fix, when scheduled)

Each item lands on its own branch off `main` → PR, with a test that fails before
and passes after. Do not batch unrelated items into one commit. Full suite
(`ctest --test-dir build`) must stay green; Debug ASan+UBSan clean.

## Hand-off

This is a report. Convert each row into a tracked task for the owning team.
Nothing here should be fixed blind as part of a formatting/refactor sweep.
