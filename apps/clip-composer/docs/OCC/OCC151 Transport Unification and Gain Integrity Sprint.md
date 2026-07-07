# OCC151 — Transport Unification and Gain Integrity Sprint

**Status:** Active
**Owner:** Claude (app scope) + Orpheus SDK agent (SDK scope, see ORP127)
**Branch:** `feat/occ-transport-unification` (to be created off `feat/occ-audio-utility-polish`)
**Started:** 2026-07-07
**Related:** ORP127 (SDK companion sprint), OCC110 (SDK Integration Guide), OCC096 (SDK Integration Patterns), OCC023 (Component Architecture)

---

## Problem Statement

Clip Composer exhibits three classes of long-standing playback defects that share root causes:

1. **Transport desync between the main grid and the Edit dialog.** Play/stop, position, and playing-state indicators do not stay in lockstep. The Edit dialog is meant to be a "zoomed in" view of the same player, not a separate one.
2. **Gain stacking.** Two audible instances of the same clip can sum at the master, producing amplitudes that trip the soft limiter and sound wrong.
3. **Erratic bitcrush / sync artifacts.** Sporadic, non-reproducible clicks, envelope discontinuities, and grainy playback — the classic signature of race conditions on non-atomic per-voice state.

The user's target model (canonical for this sprint):

- **One clip identity = one voice**, regardless of assigned playgroup(s).
- **Exception:** a fading-out tail is preserved and may overlap with a fresh voice fired during the fade window (effectively `voices == 2` only during a fade overlap).
- **"Stop all on play" ("choke")** is scoped to the firing clip's playgroup(s), not global.
- **Edit dialog and grid share the same player** for a given clip. No parallel cue-buss handles for the same clip.

---

## Audit Findings (2026-07-07)

Full explorations returned four consolidated reports. Load-bearing findings:

### App-side (owned by this sprint)

- **F-APP-1:** `PreviewPlayer` allocates a dedicated cue-buss `ClipHandle` for the Edit dialog (`Source/UI/PreviewPlayer.cpp:71-106, 193-203`; `Source/UI/ClipEditDialog.cpp:168-170`). The same file plays on two handles concurrently, causing (a) grid/dialog desync and (b) 2× amplitude summing at master when both fire.
- **F-APP-2:** `AudioEngine::startClip` implements the "restart if playing" dedup wrapper (`Source/Audio/AudioEngine.cpp:497-538`), but the device-change reinit path bypasses it and calls the raw SDK `startClip` (`Source/Audio/AudioEngine.cpp:927`). Any voice already in fade-out during that reinit becomes a stacked voice.
- **F-APP-3:** `TransportController::processCallbacks()` is invoked on the audio thread from `AudioEngine::processAudio` (`Source/Audio/AudioEngine.cpp:1278`), destroying `std::function` objects (and their captured shared_ptrs) on the RT thread. Violates real-time safety and contradicts the SPSC design documented in `transport_controller.h:255-261`.
- **F-APP-4:** `setAudioDevice` swaps `m_transportController` and `m_audioDriver` `unique_ptr`s from the UI thread with no atomic exchange (`Source/Audio/AudioEngine.cpp:825-932`). UI-thread queries and any in-flight audio callback can dereference across the swap.
- **F-APP-5:** Sample-rate mismatch between file and engine only emits a `DBG` warning (`Source/Audio/AudioEngine.cpp:388-394`) — no SRC, no user-visible refusal. A 44.1 kHz file in a 48 kHz engine plays 8.8% fast with aliasing that sounds like bitcrush.

### SDK-side (handed off to ORP127)

- F-SDK-1..6 documented in ORP127. Summary: non-atomic per-voice state, per-buffer stair-step stop fade, hard cut at OUT boundary, unsmoothed clip gain, silent minimum on `gain_smoothing_ms`, 4-voice policy that is the wrong shape for OCC.

---

## Sprint Goals

### Primary (must ship this sprint)

1. **G1 — Unify the transport for grid + Edit dialog.** Retire the cue-buss parallel path for the same clip. The Edit dialog drives the grid's `ClipHandle` directly, so play/stop/position are single-sourced. Preserve any audition-specific behavior (e.g., gap-free scrub) by extending the grid handle's control surface, not by adding a second handle.
2. **G2 — Kill the device-change bypass.** Route every re-fire through `AudioEngine::startClip` so the one-voice-per-clip dedup wrapper is honored universally.
3. **G3 — Move `processCallbacks()` off the audio thread.** Drain the callback ring from the UI/message thread only, restoring the SPSC contract.
4. **G4 — Make `setAudioDevice` transport swap safe.** Either quiesce the old transport fully before releasing, or use an atomic pointer with a grace period. No dereferences across the swap.

### Secondary (ship if SDK work lands in time)

5. **G5 — SR mismatch UX.** When a loaded file's sample rate differs from the engine, either refuse to load with a clear user-facing message or (preferred if ORP127 adds SRC) route through the SDK resampler. No more silent DBG warnings.
6. **G6 — Playgroup choke semantics.** Verify "Stop all on play" is scoped to the firing clip's assigned playgroup(s), not global. Fix if it is currently global.

### Explicit non-goals

- No JUCE UI restructuring beyond what G1 requires.
- No changes to session file format.
- No SDK internals — all SDK-side voice/atomicity/fade fixes are ORP127 scope.
- No new features. This sprint is correctness only.

---

## Definition of Done

- [ ] Grid button and Edit dialog play/stop/position are visibly and audibly in sync for every clip.
- [ ] Firing the same clip from grid and dialog simultaneously never produces 2× amplitude.
- [ ] Device-change reinit does not produce stacked voices for clips that were in fade-out.
- [ ] `processCallbacks()` executes only on the message thread (verified by adding a debug assert on the audio thread).
- [ ] `setAudioDevice` passes a stress test of rapid device switches during playback without crash or dropout beyond a single expected gap.
- [ ] SR-mismatched files either play at correct pitch (if SDK SRC lands) or are refused with a clear message (interim).
- [ ] All existing OCC tests still pass. New tests cover: grid/dialog play-state sync, device-change with playing clips, callback thread affinity.
- [ ] Manual smoke test: user confirms artifacts (gain stacking, sync bitcrush) no longer reproduce on their reproducer cases.
- [ ] `apps/clip-composer/CLAUDE.md` updated with the "one transport per clip; dialog shares the grid handle" invariant.
- [ ] This doc closed with a completion note appended.

---

## Task Ledger

Tasks are ordered by dependency. Commit after each task with the format `type(occ): short description` and a Co-Authored-By trailer.

### T1 — Branch and baseline
- [ ] Create branch `feat/occ-transport-unification` off current `feat/occ-audio-utility-polish`.
- [ ] Confirm clean build and existing tests pass on branch.
- [ ] Commit: `chore(occ): open transport unification sprint branch`.

### T2 — Unify grid + Edit dialog transport (G1)
- [ ] Remove cue-buss allocation from `PreviewPlayer::setAuditionSource` for the same-clip case. Retain cue-buss pool only for genuinely-different auxiliary sources if any exist; delete the pool entirely if not.
- [ ] `PreviewPlayer::play/stop/isPlaying/getPlaybackSnapshot` operate on the grid `ClipHandle` (via `buttonIndex → handle` lookup).
- [ ] `ClipEditDialog::setClipMetadata` no longer calls `setAuditionSource` for the loaded clip's file.
- [ ] Metadata writes (gain, fades, loop, trim) still flow through `AudioEngine::updateClipMetadata` and only touch the grid handle.
- [ ] Position polling (`PreviewPlayer::timerCallback`) reads grid handle state.
- [ ] Commit: `refactor(occ): edit dialog shares grid clip handle`.

### T3 — Test grid/dialog sync
- [ ] Add integration test: fire from grid, verify dialog shows Playing; fire from dialog, verify grid button shows Playing.
- [ ] Add test: firing same clip twice does not double-count in the transport's voice list.
- [ ] Commit: `test(occ): cover grid/dialog transport sync`.

### T4 — Remove device-change bypass (G2)
- [ ] In `AudioEngine::setAudioDevice`, replace the direct `m_transportController->startClip(handle)` call at line 927 with `this->startClip(buttonIndex)` (or the equivalent that goes through the dedup wrapper).
- [ ] Confirm restart-on-device-change still works for clips that were playing.
- [ ] Commit: `fix(occ): route device-change restarts through dedup wrapper`.

### T5 — Move callback drain off audio thread (G3)
- [ ] Remove `m_transportController->processCallbacks()` from `AudioEngine::processAudio`.
- [ ] Drain the callback ring on the message thread — either from a JUCE `Timer` at ~60 Hz on `AudioEngine`, or piggyback on an existing UI timer.
- [ ] Add a debug assert in `processCallbacks` that fails if called from the audio thread (guarded by a thread-id check the driver stashes when it enters `processAudio`).
- [ ] Commit: `fix(occ): drain transport callbacks on message thread`.

### T6 — Safe transport swap on device change (G4)
- [ ] Rework `setAudioDevice`: stop the old driver → wait for its stop callback → construct the new transport/driver → atomically publish the new pair.
- [ ] Guard UI-thread queries (`getClipState`, meter reads) with a shared_mutex or ensure they only run when a "device stable" flag is true.
- [ ] Stress test: rapid device switches while multiple clips are playing.
- [ ] Commit: `fix(occ): make audio device swap crash-safe`.

### T7 — Sample-rate mismatch handling (G5, contingent)
- [ ] If ORP127 has landed SDK-side SRC: wire the SDK resampler into the reader path. Verify pitch is correct.
- [ ] If SDK SRC has not landed: surface a modal/toast on load with clear text ("This file is 44.1 kHz but the engine runs at 48 kHz. Playback will be incorrect. Convert the file or change engine sample rate.").
- [ ] Commit: `feat(occ): handle sample-rate mismatch explicitly`.

### T8 — Playgroup choke scoping (G6)
- [ ] Locate the "Stop all on play" evaluation site.
- [ ] Confirm it filters by the firing clip's assigned playgroup(s). If it is global, fix to be playgroup-scoped.
- [ ] Add test covering: firing a clip in group A does not stop a clip in group B.
- [ ] Commit: `fix(occ): scope choke to firing clip playgroups`.

### T9 — Documentation and closure
- [ ] Update `apps/clip-composer/CLAUDE.md` with the transport unification invariant.
- [ ] Append completion note to this doc with test results and any deferred items.
- [ ] Update `apps/clip-composer/docs/occ/OCC.md` index if it exists.
- [ ] Commit: `docs(occ): record transport unification sprint completion`.

---

## Coordination With ORP127

App-side work in this sprint is designed to be **independent of** the SDK sprint — G1-G4 are pure OCC changes and do not require SDK API changes. G5 (SR mismatch) becomes cleaner if the SDK adds a resampler but has an interim fallback. G6 depends only on OCC internals.

However, the app-side work does **not fully solve** the user's reported symptoms on its own. The bitcrush artifacts specifically require SDK-side atomicity and per-sample fade fixes (see ORP127 G1, G2). Both sprints must land for the user to hear the full improvement. Sequence recommendation:

1. **This sprint T1-T3 first** — kills the grid/dialog desync and the same-file 2× stacking immediately. Big audible win with low risk.
2. **ORP127 in parallel** — atomicity, per-sample fades, voice policy.
3. **This sprint T4-T8 after ORP127 lands** — the callback thread and device-change fixes are cleaner once the SDK has proper voice semantics.

---

## Downstream Consumer Note: FourTrack

A new sibling app (`~/dev/fourtrack`, Apple-only, SwiftUI + C++ core) will consume the Orpheus SDK as an external dependency. This sprint has one indirect obligation: **do not add new OCC-specific coupling to the SDK.** Any SDK need this sprint surfaces should be handed to ORP127, not patched into the SDK from OCC code. See ORP127 for host-neutrality concerns.

---

## Change Log

- **2026-07-07:** Sprint opened, audit findings recorded, task ledger drafted.
