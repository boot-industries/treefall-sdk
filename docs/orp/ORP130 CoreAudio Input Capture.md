# ORP130 CoreAudio Input Capture

**Status:** Complete — landed on `feat/orp-coreaudio-input`
**Author:** FTR023 SDK handoff, 2026-07-09
**Scope:** SDK platform driver (`src/platform/audio_drivers/coreaudio/`)
**Severity:** High — blocked all recording in any host using the CoreAudio driver
**Related:** FTR023 (FourTrack handoff / bug report), ORP128 (CoreAudio device sample-rate resilience — same subsystem), FTR020 (device-UID routing — additive follow-up), ORP127 (voice/SRC sprint)

---

## Summary

The macOS CoreAudio driver was **output-only**. `setupAudioUnit()` explicitly
disabled the AudioUnit input scope and the render callback never called
`AudioUnitRender`, so the input buffers handed to a host's `processAudio` were
always zero-filled — regardless of `config.num_inputs`. Any host that recorded
produced a valid, correctly-sized, but completely **silent** file.

This was surfaced by FourTrack (FTR023): its core correctly requests one input
channel, consumes `input_buffers[0]`, and writes it to the armed track, yet
recordings were silent. The FourTrack core and shell were correct end to end;
the driver never captured.

## Root cause (pre-fix)

In `coreaudio_driver.cpp`:

1. Input IO was hard-disabled — `enableIO = 0` on `kAudioUnitScope_Input`,
   element 1, unconditionally.
2. `AudioUnitRender` was never called. A HAL output render callback fires for
   the *output* bus and must **explicitly pull** input (bus 1); captured samples
   never arrive in `ioData`.
3. `input_buffers_` were sized and zeroed once in `initialize()` and handed to
   the host forever silent.

The gap shipped undetected because no real-driver input leg was ever exercised:
the SDK's CoreAudio test used `num_inputs = 0`, the dummy driver simulates
silence, and FourTrack's record tests ran against a `MockAudioDriver` that
synthesizes input. The real capture path had no coverage.

## Fix

Three changes in `coreaudio_driver.cpp`, all guarded on `config_.num_inputs > 0`
so pure-playback hosts keep the historical output-only behavior:

1. **Enable input IO** — `enableIO = 1` on `kAudioOutputUnitProperty_EnableIO`,
   `kAudioUnitScope_Input`, element 1 (bus 1).
2. **Set the input stream format** — the planar float32 ASBD on
   `kAudioUnitScope_Output`, element 1 (the format the AU delivers to the render
   pull), mirroring the output format with the input channel count so
   `AudioUnitRender` fills the planar `input_buffers_` directly, no deinterleave.
3. **Pull input in the render callback** — before invoking `processAudio`, call
   `AudioUnitRender(audio_unit_, ioActionFlags, inTimeStamp, /*inputBus=*/1,
   frames, &inputABL)` into a pre-allocated `AudioBufferList` whose `mData`
   pointers alias `input_buffers_`.

**Real-time discipline:** the `AudioBufferList` and its backing storage are
allocated once in `initialize()` (member `input_abl_storage_`); the audio thread
performs no allocation, lock, or I/O. Input buffers are pre-zeroed each callback
so a failed or absent render delivers silence rather than stale samples, and a
transient `AudioUnitRender` miss is never propagated up the audio graph.

The AudioUnit subtype is `kAudioUnitSubType_HALOutput`, which supports both
capture (bus 1) and playback (bus 0) on a single unit — no second unit is needed.

## Verification

- SDK builds clean (Debug, ASan/UBSan): `orpheus_audio_driver_coreaudio`,
  `coreaudio_driver_test`, `driver_manager_test`.
- Full CoreAudio suite (22 tests) and driver-manager suite (20 tests) pass.
- New regression tests (`coreaudio_driver_test.cpp`):
  - `InitializeWithInputEnabled` — capabilities report input support with
    `num_inputs = 1`.
  - `InputBufferReachesCallback` — with `num_inputs = 1`, a non-null input buffer
    reaches `processAudio` (the structural guard the pre-fix driver could not
    pass) and, when the input carries a live signal, non-zero energy arrives.
- The capture path was independently validated against a raw hand-written HAL
  loopback and against `ffmpeg`: input IO enable, input format set, and every
  `AudioUnitRender(bus 1)` call returned `noErr`, and a real non-null input
  buffer reached the callback each cycle. Non-zero *energy* could not be produced
  in the headless session — the built-in mic, BlackHole, and every capture path
  returned OS silence (no TCC microphone grant / no live signal), which is an
  environment limitation, not a code defect (see below).

## Host prerequisite (documented, not an SDK issue)

Live capture also requires the host to hold a microphone permission. A macOS app
needs `NSMicrophoneUsageDescription` (present in FourTrack), and under the App
Sandbox additionally `com.apple.security.device.audio-input`. A headless CLI run
with no TCC grant receives OS silence even with a correct driver — which is why
this class of environment cannot assert non-zero capture energy, and why the
regression test's energy branch is environment-gated.

## Relationship to FTR020

FTR020 asks the SDK to route the CoreAudio driver to a *chosen non-default*
device (device-UID field + enumeration). ORP130 is upstream of that: without
input capture, no device — default or chosen — records. FTR020 device routing
remains a separate, additive follow-up that layers on top of this fix.
