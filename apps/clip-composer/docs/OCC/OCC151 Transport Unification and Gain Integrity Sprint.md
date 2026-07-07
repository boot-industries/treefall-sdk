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

### T7 — Wire SDK resampler into reader path (G5 via ORP127 G6)
- [ ] Wrap the `IAudioFileReader` returned by `create_audio_file_reader_libsndfile` in `ResamplingAudioFileReader` when the file's native rate differs from the engine rate.
- [ ] Remove the silent DBG warning at `AudioEngine.cpp:388-394`; log a single info-level line noting the decorator is active.
- [ ] Verify: a 44.1 kHz file plays at correct pitch in a 48 kHz engine, trim points and fade durations remain sample-accurate.
- [ ] Commit: `feat(occ): resample mismatched-rate files via SDK decorator`.

### T8 — Playgroup-scoped choke via SDK primitive (G6)
- [ ] Locate OCC's "Stop all on play" evaluation site.
- [ ] Implement playgroup scoping in OCC (SDK owns no playgroup concept). Two implementation options — pick one:
  - **A:** Walk playgroup members and call `stopClip(handle)` for each. Simple, correct, may be O(n) on the UI thread.
  - **B:** Call `stopOtherClips(firingHandle)` and instead filter the *choke* application at fire-time (only invoke it if the firing clip has the flag).
- [ ] Add test: firing a clip in group A does not stop a clip in group B.
- [ ] Commit: `fix(occ): scope choke to firing clip playgroups`.

### T9 — Documentation and closure
- [ ] Update `apps/clip-composer/CLAUDE.md` with the transport unification invariant and the new SDK dependency version.
- [ ] Append completion note to this doc with test results and any deferred items.
- [ ] Update `apps/clip-composer/docs/occ/OCC.md` index if it exists.
- [ ] Commit: `docs(occ): record transport unification sprint completion`.

### T10 — Adopt `VoiceMode::MonoWithFadeOverlap` (ORP127 consumption)
- [ ] After `registerClipAudio` for each clip, call `transport->setClipVoiceMode(handle, VoiceMode::MonoWithFadeOverlap)`.
- [ ] Remove or reduce the local dedup wrapper in `AudioEngine::startClip` (`Source/Audio/AudioEngine.cpp:497-538`) — the SDK now enforces the model.
- [ ] Verify: fire-during-fade produces the target behavior (fresh voice alongside fading tail); fire-while-playing restarts in place.
- [ ] Commit: `feat(occ): adopt MonoWithFadeOverlap voice mode`.

### T11 — Set `MaxVoicesPerClip` to 2 (ORP127 consumption)
- [ ] At `AudioEngine` construction, call `transport->setMaxVoicesPerClip(2)`. Rationale: one primary + one fade-tail is all OCC's model needs.
- [ ] Verify: rapid re-fire under stress does not exceed 2 active voices per clip (`getActiveVoiceCount(handle)` check in a test).
- [ ] Commit: `feat(occ): cap voices per clip to 2`.

### T12 — Pin to ORP127 SDK release tag
- [ ] Once the SDK agent tags the ORP127 release, pin OCC's SDK reference to it.
- [ ] Confirm full OCC build + tests against the tagged SDK.
- [ ] Commit: `chore(occ): pin SDK to <tag>`.

---

## Coordination With ORP127

**ORP127 landed 2026-07-08 on branch `feat/orp127-transport-voice-integrity` (11 commits, SDK version 0.2.0 → 0.3.0, 178/178 tests green, TSan clean).** App-side work in this sprint remains largely independent, but several tasks are now simpler and one is superseded:

- **G1 (grid/dialog unification)** — unchanged. Pure OCC refactor.
- **G2 (device-change bypass)** — unchanged, but the call site now goes through the ORP127 `MonoWithFadeOverlap` semantics on re-fire, so the visible behavior will be correct once we set the voice mode.
- **G3 (callback drain)** — unchanged. Still an OCC bug regardless of SDK atomicity.
- **G4 (device swap safety)** — unchanged.
- **G5 (SR mismatch)** — **superseded by ORP127 G6.** `ResamplingAudioFileReader` is a drop-in decorator over `IAudioFileReader` that reports metadata in the target rate. OCC151 T7 becomes: wire the decorator into the reader path and remove the interim refuse-to-load fallback if any exists.
- **G6 (choke scoping)** — the SDK now provides `stopOtherClips(exceptHandle)` as a host-neutral primitive. OCC keeps ownership of playgroup semantics; T8 becomes: implement OCC's "stop others in this clip's playgroup(s)" by walking the playgroup members and calling `stopClip` on each (or by adding a wrapper that filters, then calls the SDK primitive). The SDK correctly does NOT know about playgroups.

**New consumption tasks (add to ledger):**

- **T10 — Adopt `VoiceMode::MonoWithFadeOverlap`.** After registering each clip, call `setClipVoiceMode(handle, VoiceMode::MonoWithFadeOverlap)`. This is the user's canonical model and eliminates OCC's need for the local dedup wrapper at `AudioEngine::startClip` (`Source/Audio/AudioEngine.cpp:497-538`). Wrapper can be removed or reduced to a thin passthrough.
- **T11 — Set `MaxVoicesPerClip` for OCC.** Default SDK cap is 8, max 32. For OCC's model, 2 is sufficient (one primary + one fade-tail). Call `setMaxVoicesPerClip(2)` on the transport at boot.
- **T12 — Pin OCC's SDK dependency to the ORP127 tag** (once tagged). Verify build against the new API.

Sequence:

1. **T1-T3 first** — grid/dialog unification and sync tests. Big audible win.
2. **T10-T12** — adopt the ORP127 API. Small changes, high value.
3. **T4-T6** — callback drain, device-change bypass, device swap safety.
4. **T7-T8** — SR handling via decorator; playgroup choke via SDK primitive.
5. **T9** — docs and closure.

---

## Downstream Consumer Note: FourTrack

A new sibling app (`~/dev/fourtrack`, Apple-only, SwiftUI + C++ core) will consume the Orpheus SDK as an external dependency. This sprint has one indirect obligation: **do not add new OCC-specific coupling to the SDK.** Any SDK need this sprint surfaces should be handed to ORP127, not patched into the SDK from OCC code. See ORP127 for host-neutrality concerns.

---

## Change Log

- **2026-07-07:** Sprint opened, audit findings recorded, task ledger drafted.
- **2026-07-08:** ORP127 landed (SDK 0.3.0). Ledger updated: G5 superseded by SDK resampler, G6 uses new `stopOtherClips` primitive, three new tasks (T10-T12) added for ORP127 API adoption. Sequence adjusted so ORP127 adoption (T10-T12) lands right after grid/dialog unification (T1-T3).
