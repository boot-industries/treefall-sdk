# ORP127 — Transport Voice Model and Gain Integrity Sprint

**Status:** Active — handoff to SDK agent
**Owner:** Orpheus SDK agent (TBD)
**Branch:** `feat/orp127-transport-voice-integrity` (to be created off `main`)
**Started:** 2026-07-07
**Related:** OCC151 (app companion sprint), OCC110 (SDK Integration Guide - Transport State and Loop), ORP125 (architecture refactor completion), ORP126 (Codex audit)

---

## Problem Statement

The Orpheus SDK's `TransportController` and adjacent routing/gain code exhibit correctness defects that surface as user-audible artifacts in Clip Composer (bitcrush, envelope discontinuities, sporadic clicks, gain stacking). The defects are also blockers for the upcoming FourTrack Apple-native app, which will consume the SDK externally. Fixing them here — not in each host — is the correct scope.

Findings below come from a code exploration on 2026-07-07. See OCC151 for the full context and audit summary.

---

## Findings

### F-SDK-1 — Per-voice state is not thread-safe

`ActiveClip` members `currentSample`, `isStopping`, `fadeOutGain`, `hasLoopedOnce`, and `reader` (`src/core/transport/transport_controller.h:82-130`) are plain (non-atomic) fields. They are mutated from **both threads**:

- Audio thread: normal render loop in `processAudio` (`src/core/transport/transport_controller.cpp:305-527`).
- UI thread: `restartClip` (`:1552-1592`), `seekClip` (`:1622-1634`), and any command handler that runs synchronously.

Consequences:
- Torn writes on `fadeOutGain` mid-fade produce a one-buffer envelope jump — audible as a click or "crush."
- `reader` (a `shared_ptr`) can be swapped on one thread while the other holds a raw pointer, risking use-after-free or refcount races.
- `m_activeClipCount` iteration on the UI thread (`isClipPlaying`, `getClipState` at `:257-285, 1519-1528`) races with audio-thread `addActiveClip`/`removeActiveClip` compaction (`:791-966`).

### F-SDK-2 — Stop fade-out is stair-stepped per buffer

`src/core/transport/transport_controller.cpp:341-345` computes one `clip.fadeOutGain` scalar per buffer, based on `currentSample` at buffer start. Every sample in that buffer receives the same multiplier. Contrast with the per-sample fade-in and clip fade-out envelopes at `:482, :491`. For short fade durations (e.g., 10 ms fade @ 512-sample buffer @ 48 kHz), the "fade" becomes a 1-2 step staircase. Firing "Stop All" on many clips at once compounds this into audible bitcrush-like distortion.

### F-SDK-3 — Hard cut at OUT boundary for non-looped clips

`src/core/transport/transport_controller.cpp:389-395` sets `clip.reader = nullptr` at OUT with `clip.fadeOutGain = 1.0f`. The trailing samples of the OUT buffer render at full gain; the fade begins in the *next* buffer with no reader. Result: an audible click at the OUT boundary of every non-looped clip.

### F-SDK-4 — Clip gain has no smoother

`src/core/transport/transport_controller.cpp:727-732`: the Set Gain command handler writes `clipGainLinear` atomically with no ramp. UI drags apply as step functions on the next buffer boundary → zipper noise.

### F-SDK-5 — `gain_smoothing_ms = 0` is silently clamped to 1 ms

`src/core/routing/gain_smoother.cpp:12` clamps `smoothing_time_ms` to a 1 ms minimum. The transport requests `gain_smoothing_ms = 0.0f` (`src/core/transport/transport_controller.cpp:41-42`) expecting no smoothing; it silently gets ~1 ms of ramp. This is confusing and undocumented, and it means pan smoothers ramp from their initial 0.707 state to 1.0/0.0 in ~1 ms on init — audible transient at first sample of playback for hard-panned channels.

### F-SDK-6 — Voice policy is the wrong shape for the OCC/FourTrack use case

`MAX_VOICES_PER_CLIP = 4` (`src/core/transport/transport_controller.h:249`) with oldest-voice eviction on overflow (`:791-820`). The Start command unconditionally allocates a new `ActiveClip`. There is no built-in "one-voice-per-clip" mode, no fade-tail-overlap semantics, no playgroup-scoped choke.

The user's canonical model:
- Default: **one voice per clip identity**, regardless of playgroup assignment.
- **Exception:** a stopped-and-fading tail persists; a new fire during the fade window starts a fresh voice that coexists with the tail (`voices == 2` only during fade overlap).
- **Choke:** "Stop all on play" is scoped to the firing clip's assigned playgroup(s), not global.

Hosts should be able to opt into this mode via the SDK API; OCC will opt in universally. FourTrack likely uses a different model (fixed per-track voice), but should share the same well-defined semantics.

### F-SDK-7 — No sample-rate conversion in the reader path

`apps/clip-composer/Source/Audio/AudioEngine.cpp:388-394` warns on SR mismatch but there is no SRC anywhere in the SDK's reader → transport chain. A file at a different rate plays at the wrong pitch with aliasing. This is a host-neutral concern: every consumer will hit it.

---

## Sprint Goals

### Primary

1. **G1 — Make per-voice state thread-safe.** Either (a) move *all* mutation onto the audio thread via commands only (preferred, matches SPSC design), or (b) make the hot fields atomic with acquire/release ordering. `restartClip`, `seekClip`, and every method that touches `ActiveClip` from the UI thread must be revisited.
2. **G2 — Per-sample stop fade.** Replace the per-buffer stair-step in `processAudio` with a per-sample envelope, matching the fade-in / fade-out code path.
3. **G3 — Fix OUT-boundary hard cut.** Apply the fade-out envelope before nulling the reader, or defer reader release until after the fade completes.
4. **G4 — Add a clip-gain smoother.** Ramp `clipGainLinear` changes over a configurable time (default 5-10 ms). Honor `gain_smoothing_ms = 0` as "no smoothing" instead of silently clamping to 1 ms — either lift the clamp or expose a real disable flag.
5. **G5 — Voice policy API.** Add a per-clip (or transport-level) voice mode:
   - `VoiceMode::MonoWithFadeOverlap` — one primary voice; fading tail persists; new fire during fade starts fresh voice.
   - `VoiceMode::Polyphonic` — current behavior, up to `MAX_VOICES_PER_CLIP`.
   Document semantics in OCC110 and the SDK reference. OCC will select `MonoWithFadeOverlap` universally.

### Secondary

6. **G6 — Sample-rate conversion.** Add a resampler (SDK-level, host-neutral) so mismatched files play at correct pitch. Deterministic, offline-capable, works in the render path. Consider `libsamplerate` or a hand-rolled polyphase for determinism.
7. **G7 — Choke scope API.** Expose a "stop others in playgroup" primitive at the transport/routing boundary so hosts can implement choke without walking the voice list themselves.

### Explicit non-goals

- No changes to the public `IAudioFileReader` interface unless G6 requires it (in which case add a new interface, don't break the old one).
- No adapter changes (REAPER, standalone, etc.) beyond what the API changes require.
- No performance regression: RT budget in `processAudio` must not grow measurably.
- No new dependencies without explicit approval (`libsamplerate` in G6 is a candidate but should be discussed first).

---

## Host-Neutrality Requirements (Critical)

This SDK is consumed by:
- **Clip Composer** (JUCE-based macOS/Windows/Linux desktop, in-repo at `apps/clip-composer/`)
- **FourTrack** (SwiftUI + C++ core, Apple-only, external repo at `~/dev/fourtrack`, consumes SDK via CMake `FetchContent` pinned to a tag)
- **REAPER adapter** (existing)
- **Future hosts** (TBD)

Every API change in this sprint must:
- Compile clean under C++20 with no host framework dependencies.
- Not assume JUCE, message-thread pumps, or any specific UI stack.
- Be usable from a Swift/ObjC++ bridge (no C++ template soup in public headers where a plain-C-shaped API can do the job).
- Ship with a standalone test that does not link JUCE.

If a proposed change would require host-specific glue, redesign it. The point of the SDK is that FourTrack should be able to link `Orpheus::transport` and get correct behavior without knowing OCC exists.

---

## Definition of Done

- [ ] All `ActiveClip` state mutation is either audio-thread-only (via commands) or properly synchronized. Verified with ThreadSanitizer on a stress test that hammers `startClip`/`stopClip`/`restartClip`/`seekClip` from the UI thread while playback runs.
- [ ] Stop fade-out is per-sample. Test: 10 ms stop fade @ 512-sample buffer @ 48 kHz produces a smooth exponential curve (measured, not eyeballed).
- [ ] Non-looped clip OUT boundary has no click. Test: null-file test that fades a sine to silence at OUT and measures peak amplitude of the transition.
- [ ] Clip gain changes ramp over the configured smoothing time. Test: rapid gain automation produces no zipper noise (measured).
- [ ] `VoiceMode::MonoWithFadeOverlap` implemented and covered by tests: (a) fire while playing does not stack, (b) fire during fade tail starts fresh voice alongside tail, (c) fade tail completes and is torn down cleanly.
- [ ] Existing 270+ SDK tests pass. New tests cover atomicity, per-sample fades, voice modes.
- [ ] Public API changes documented in OCC110 (SDK Integration Guide) and any relevant SDK header.
- [ ] `packages/occ-app-platform/` (or wherever OCC integrates) updates in tandem so OCC continues to build against the new API. Coordinate with OCC151 owner.
- [ ] No RT-budget regression measured on the standard `TransportController` render benchmark.
- [ ] This doc closed with a completion note.

---

## Task Ledger

Tasks are ordered by risk / dependency. Commit granularly with `type(scope): imperative description` + Co-Authored-By trailer. Do not batch.

### T1 — Branch and baseline
- [ ] Create branch `feat/orp127-transport-voice-integrity` off `main`.
- [ ] Confirm clean build, `ctest` green, sanitizers clean on Debug.
- [ ] Commit: `chore(orp): open ORP127 sprint branch`.

### T2 — ThreadSanitizer harness
- [ ] Add a stress test that runs playback while UI-thread `startClip`/`stopClip`/`restartClip`/`seekClip` fire at high rate.
- [ ] Run under TSan. Record the current failures as the baseline.
- [ ] Commit: `test(transport): tsan stress harness for voice state`.

### T3 — Move UI-thread mutations onto the command queue (G1)
- [ ] Convert `restartClip`, `seekClip`, and any other UI-thread mutation site into commands processed in `processCommands` on the audio thread.
- [ ] Where a return value is expected (e.g., `isClipPlaying`), read from atomically-published snapshots, not live voice fields.
- [ ] Ensure `m_activeClipCount` is read via an atomic or a snapshot vector.
- [ ] Confirm TSan is clean on the T2 harness.
- [ ] Commit: `fix(transport): route all voice mutations through command queue`.

### T4 — Per-sample stop fade (G2)
- [ ] Replace the per-buffer stair-step at `transport_controller.cpp:341-345` with a per-sample decrement (linear or exponential — match the fade curve semantics used elsewhere).
- [ ] Add a test that measures the envelope shape across a stop fade and verifies smoothness.
- [ ] Commit: `fix(transport): per-sample stop fade envelope`.

### T5 — OUT-boundary fade (G3)
- [ ] Rework the OUT handling at `transport_controller.cpp:389-395`: begin fade-out earlier so it completes at OUT, or continue rendering the reader through the fade tail before releasing.
- [ ] Add a test that measures peak amplitude of the OUT transition and asserts it is below a smoothness threshold.
- [ ] Commit: `fix(transport): eliminate hard cut at clip OUT boundary`.

### T6 — Clip gain smoother (G4)
- [ ] Add a smoother to `clipGainLinear` writes in the Set Gain command handler.
- [ ] Default smoothing time: 5 ms (document in header). Configurable via a per-clip or per-transport parameter.
- [ ] Fix `gain_smoother.cpp:12` — either lift the 1 ms clamp, or make it explicit and document. Add a real "smoothing disabled" mode if needed.
- [ ] Test: rapid gain automation produces no measurable zipper.
- [ ] Commit: `fix(transport): smooth clip gain changes`.

### T7 — Voice mode API (G5)
- [ ] Design the `VoiceMode` enum and the API surface: `TransportController::setClipVoiceMode(ClipHandle, VoiceMode)` or a per-clip config field.
- [ ] Implement `MonoWithFadeOverlap`:
  - Fire while a voice is playing (not fading): no-op or restart (define semantics; OCC currently expects restart).
  - Fire while a voice is fading: start a fresh voice; the fading voice is orphaned and completes naturally.
  - Choke ("Stop all on play"): expose as a separate flag; scoping is host's responsibility, but the SDK provides the primitive.
- [ ] Keep `Polyphonic` as an opt-in mode that preserves current behavior.
- [ ] Tests: cover all three fire scenarios in `MonoWithFadeOverlap` mode.
- [ ] Commit: `feat(transport): add voice mode API with mono-with-fade-overlap`.

### T8 — Sample-rate conversion (G6, contingent on scope confirmation)
- [ ] Confirm approach with the user before starting: `libsamplerate` (SC-quality, adds dep) vs. hand-rolled polyphase (deterministic, no dep, more code).
- [ ] Add resampler to the reader path so mismatched files play at correct pitch.
- [ ] Test: 44.1 kHz sine loaded in a 48 kHz engine plays at the correct frequency (measured via FFT).
- [ ] Commit: `feat(transport): sample-rate conversion in reader path`.

### T9 — Choke primitive (G7)
- [ ] Add a transport-level `stopAllInPlaygroup(playgroupId)` (or similar) so hosts can implement choke without walking the voice list.
- [ ] Test: firing in group A does not affect voices in group B.
- [ ] Commit: `feat(transport): playgroup-scoped stop primitive`.

### T10 — Documentation and closure
- [ ] Update OCC110 (SDK Integration Guide) with voice mode semantics, gain smoothing, fade behavior.
- [ ] Update relevant SDK headers with API-doc comments.
- [ ] Update `docs/orp/ORP.md` index if it exists.
- [ ] Append completion note to this doc.
- [ ] Bump SDK version if the public API changed. Coordinate a tag for OCC and FourTrack to pin against.
- [ ] Commit: `docs(orp): record ORP127 sprint completion`.

---

## Coordination With OCC151

- **T2-T4 land first** — atomicity is the highest-value fix and unblocks OCC151's callback/device work.
- **T5-T6** can land in parallel with OCC151 T4-T5.
- **T7 (voice mode API)** should be reviewed with the OCC151 owner before merge — OCC151 has to consume the new API.
- **T8 (SRC)** — OCC151 G5 has an interim fallback (refuse-to-load with a message). SRC can land after OCC151 closes.

Cross-repo build gate: after any public API change, the OCC151 owner must confirm the app still builds and passes tests against the new SDK. Do not merge SDK changes to `main` without that confirmation.

---

## Host-Neutrality Checklist (per PR)

Before merging any PR in this sprint, verify:

- [ ] No JUCE/Tauri/Cocoa headers in SDK public interface.
- [ ] Public API is usable from C via a shim (or is already C-shaped).
- [ ] Standalone test exists that does not link any host framework.
- [ ] Any new dependency is discussed and approved.
- [ ] Behavior is deterministic (same input → same output, bit-identical) unless documented otherwise.
- [ ] No allocations in the audio thread render path.

---

## Downstream Consumer Note: FourTrack

FourTrack (`~/dev/fourtrack`, planned) will consume this SDK via CMake `FetchContent` pinned to a tag. This sprint is the first real forcing function for the SDK to prove its host-neutral consumption path. Two specific asks:

1. **Verify `Orpheus::transport`, `Orpheus::routing`, `Orpheus::render`, `Orpheus::audio_utils`, and `Orpheus::diagnostics` are cleanly consumable from an external CMake project.** Add a smoke test that builds a minimal external consumer and links against the installed targets. This is roughly one hour of work but pays back forever.
2. **Voice mode design must accommodate FourTrack's model** — fixed per-track voice, no choke, no polyphony. `VoiceMode::MonoStrict` (fire while playing = restart, no fade overlap) may be worth adding as a third mode. Confirm with the OCC151 owner or the user before finalizing the enum.

---

## Change Log

- **2026-07-07:** Sprint opened, findings recorded, task ledger drafted.
