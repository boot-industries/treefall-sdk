<!-- SPDX-License-Identifier: MIT -->

# ORP152 — Clip Playback Controls

**Document type:** Public transport contract and implementation record  
**Version target:** Unreleased  
**Status:** Implemented; focused contract tests verified  
**Date:** 2026-07-16

---

## Scope

ORP152 completes the host-neutral clip controls represented by
`ClipMetadata` and `SessionDefaults` in
`include/orpheus/transport_controller.h`:

- independent operator-stop fade duration and curve;
- mute that preserves the configured `gainDb` value;
- stereo balance;
- forward varispeed playback; and
- trigger-to-audible delay.

The controls are metadata, not UI policy. Button layouts, labels, show
programming, ducking, time-stretch/pitch preservation, reverse playback, and
surround panning remain application or future-SDK concerns.

## Public contract

`ClipMetadata` is the atomic update payload and `SessionDefaults` supplies the
same values when `registerClipAudio()` creates a new registry entry. Changing
session defaults does not mutate an existing clip.

`setSessionDefaults()` normalizes pan, playback rate, and delay to the public
bounds. When a session default stop fade exceeds a newly registered clip's
duration, the effective per-clip value is clamped to that duration.

| Field | Valid range | Semantics |
|---|---:|---|
| `stopFadeOutSeconds` | finite, `0` through trimmed duration | Fade used by `stopClip()`, `stopAllClips()`, `stopOtherClips()`, and atomic group choke. |
| `stopFadeOutCurve` | `FadeCurve` | Curve for that operator stop fade. |
| `muted` | boolean | Ramps output to silence through the existing gain smoother without changing persisted `gainDb`; unmuting ramps back to that gain target. |
| `pan` | finite, `[-1, +1]` | Mono uses a constant-power left/right law. Stereo and wider sources use an equal-power balance taper that preserves both first-pair channels at center. Mono sources are duplicated to the first two lanes when available. |
| `playbackRate` | finite, `[0.25, 4.0]` | Forward, pitch-coupled varispeed multiplier. `1.0` is normal speed. |
| `playDelaySeconds` | finite, `[0, 99.9]` | Output-frame delay from accepted start/restart to the first audible sample. The source cursor does not advance during the delay. |

The existing `fadeOutSeconds` and `fadeOutCurve` remain the natural trim-OUT
end envelope; they are not reused for an operator request to stop a clip.

`updateClipMetadata()` validates all fields before it posts its one
control-to-audio command. Validation failure leaves both persistent metadata
and active voices unchanged. Active voices apply the new control values when
that command is consumed; stopped clips capture them at their next start.

For source compatibility, `updateClipFades()` sets both the natural-END and
operator-stop fade-out fields to its `fadeOut*` arguments. Hosts that need
different envelopes use `updateClipMetadata()`.

No transport virtual method was added. `ClipMetadata` and `SessionDefaults`
are public C++ structures, so consumers that compile against their full
layouts must recompile with this SDK revision. This does not change the stable
C ABI version.

## Realtime behavior

The audio thread receives a fixed snapshot of the controls with its start
context and applies later batch updates through the existing bounded SPSC
command ring. Varispeed uses the voice's fractional source cursor, linear
interpolation, and the preallocated read buffer sized for the maximum $4\times$
rate plus interpolation guard frames. Delay advances no source state while it
emits silence. These paths allocate neither memory nor perform blocking I/O in
`processAudio()`.

A mono source is duplicated to the first two discrete lanes when available, so
pan can create a real left/right placement. Wider sources retain their existing
source-to-output mapping; only their first stereo pair receives a balance gain.

## Verification

`clip_controls_test` covers:

1. round-trip persistence and transactional rejection of invalid control
   values;
2. session-default normalization and propagation to a newly registered clip;
3. delay without source-cursor advancement;
4. mute-to-silence while preserving transport advancement, and hard-left
   suppression of the right output;
5. variable-rate source-cursor advancement, including an active rate change;
   and
6. independent, output-frame-counted operator-stop fades.

The pre-existing stop-fade and transport tests remain the regression coverage
for trim boundaries, restart, loop behavior, routing, voice ownership, and
callback delivery.
