# ORP128 CoreAudio Device Sample-Rate Change Resilience

**Status:** Advisory / gap report — awaiting SDK driver-layer fix
**Author:** OCC151 field session, 2026-07-07
**Scope:** SDK platform driver (`src/platform/audio_drivers/coreaudio/`)
**Severity:** High for 24/7 broadcast use — silent audio corruption, no crash, no log
**Related:** ORP127 (voice/SRC sprint), OCC151 (Clip Composer transport unification), FTR005 (FourTrack SDK note)

---

## Summary

The macOS CoreAudio driver forces its output device to the engine's requested
nominal sample rate **once**, at `setupAudioUnit()` time, and then never watches
that property again. When the OS or another application changes the device's
nominal sample rate **while the driver is running**, the AudioUnit keeps
rendering at its initialized stream-format rate into a device now clocked at a
different rate. The result is continuous aliased/quantized distortion on every
sample — subjectively a "bitcrusher / Moog crusher" sound — with **no crash, no
underrun, and no log entry**. It persists until the device rate is manually
restored or the driver is re-initialized.

This is not an OCC151 regression. It is a pre-existing driver-layer gap that
OCC151's field testing surfaced.

## Reproduction (observed)

1. Launch an SDK host (Clip Composer) with engine rate 48000 Hz while the default
   output device is at 48000 Hz. Audio is clean.
2. Mid-session, cause the OS to change the default output device's nominal rate
   to 44100 Hz (a background app / system sound / hot-plugged interface / a
   virtual device such as BlackHole or an Avid device grabbing the default and
   re-clocking it).
3. Every subsequently fired clip — including a single clip in isolation —
   renders with constant bitcrush distortion.
4. Manually restoring the device to 48000 Hz (Audio MIDI Setup) clears it
   instantly. The distortion cannot be reproduced while the rates match.

Field evidence (2026-07-07): `/tmp/coreaudio_init.log` recorded a clean match at
init (`DEVICE nominal sample rate: 48000.0 Hz`); `system_profiler SPAudioDataType`
during the fault showed the default output (MacBook Pro Speakers) at
`Current SampleRate: 44100` while the engine and AudioUnit were still at 48000.

## Root cause (code-level)

`src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp`, `setupAudioUnit()`:

- **Line ~390–397** — one-shot `AudioObjectSetPropertyData(...,
  kAudioDevicePropertyNominalSampleRate, ...)` forces the device to the requested
  rate. Correct, but runs only during setup.
- **Line ~408–421** — the AudioUnit input `StreamFormat.mSampleRate` is pinned to
  `config_.sample_rate` for the life of the AudioUnit.
- **Line ~458–489** — the mismatch check (`***CRITICAL*** DEVICE RATE MISMATCH!`)
  also runs only once, at the end of `setupAudioUnit()`. It therefore never fires
  for a *mid-session* change — which is exactly the dangerous case.

The two — and only two — references to `kAudioDevicePropertyNominalSampleRate` in
the driver are this one-shot Set (line 393) and one-shot Get (line 461). There is
**no `AudioObjectAddPropertyListener`** anywhere in the driver. The driver is deaf
to nominal-rate changes after initialization.

## Why it is silent

- The AudioUnit render callback keeps producing valid float buffers at the
  configured frame count, so no underrun path is triggered — `onBufferUnderrun()`
  never fires.
- The corruption happens at the HAL clock boundary (engine-rate samples clocked
  out at the device rate), below the SDK's visibility.
- The only diagnostic that would catch it (`***CRITICAL***`) is init-time-only.

## Recommended SDK fix (host-neutral, driver-layer)

Add a nominal-sample-rate property listener to `CoreAudioDriver` and react when
the device's rate diverges from the running configuration. Preferred behavior, in
priority order:

1. **Re-assert + re-sync.** On a `kAudioDevicePropertyNominalSampleRate` change
   notification, attempt to set the device back to `config_.sample_rate`. If the
   device accepts it, no further action is needed (self-healing).
2. **If the device refuses the requested rate** (e.g. a fixed-rate aggregate /
   virtual device), tear down and rebuild the AudioUnit at the device's new rate
   and surface the change to the host via a driver event, so the host can rebuild
   its transport at the new engine rate (OCC already has a safe swap path —
   OCC151 T6 `setAudioDevice`). This mirrors how hosts already handle an explicit
   device change.
3. **At minimum (interim):** detect the divergence on a periodic query and report
   it through a real, host-visible channel (a driver status/error surfaced up the
   stack), not a one-shot write to `/tmp/coreaudio_init.log`. Silent degradation
   is the worst outcome for a broadcast tool; a loud, actionable warning is a
   strict improvement even before auto-recovery lands.

### Implementation sketch

```cpp
// In start() (after AudioUnitInitialize) or setupAudioUnit():
AudioObjectPropertyAddress srAddr = {kAudioDevicePropertyNominalSampleRate,
                                     kAudioObjectPropertyScopeGlobal,
                                     kAudioObjectPropertyElementMain};
AudioObjectAddPropertyListener(device_id_, &srAddr,
                               &CoreAudioDriver::deviceSampleRateChanged, this);

// In stop()/cleanupAudioUnit(): AudioObjectRemovePropertyListener(...) symmetrically.

// Listener (NOT the audio thread — HAL notification thread):
static OSStatus deviceSampleRateChanged(AudioObjectID, UInt32, const
    AudioObjectPropertyAddress*, void* ctx) {
  auto* self = static_cast<CoreAudioDriver*>(ctx);
  // Query new device rate; if != config_.sample_rate, attempt re-set, else
  // signal the host to rebuild the transport at the new rate. Must not touch
  // the audio thread or take the render lock inline — post the work.
  return noErr;
}
```

Notes for the implementer:
- The listener fires on a HAL-owned thread, not the RT audio thread — do **not**
  block it or do the rebuild inline; hand off to the driver's own control path.
- Register/unregister the listener symmetrically with the AudioUnit lifecycle so
  a stopped driver holds no listener against a device it no longer owns.
- Keep the existing init-time diagnostic, but also route the *runtime* mismatch
  through the same host-visible event so it is never silent again.

## Downstream consumers affected

- **Clip Composer (OCC):** primary reporter. Operator workaround today: keep the
  OS output device at the session's engine rate. Once the driver self-heals or
  emits an event, OCC can drop the manual step and (case 2) rebuild via the
  existing `setAudioDevice` swap.
- **FourTrack (FTR):** same SDK CoreAudio driver, same exposure. See FTR005 for
  the related engine-rate metadata gap; this rate-change gap should be tracked
  alongside it.

## Ground rule

Per OCC151's contract, hosts must **not** paper over this with app-side driver
logic (e.g. polling `system_profiler` from the UI and reopening the driver). The
fix belongs in the SDK CoreAudio adapter so every host benefits and the audio
thread contract stays owned by the driver.

## Provenance

Surfaced during OCC151 post-sprint smoke testing. The transport-unification,
voice-model, and file-rate→engine-rate SRC work (ORP127/OCC151) is unaffected and
verified clean when device and engine rates match. This gap is strictly at the
engine-rate→device-rate (HAL clock) boundary, which no ORP127/OCC151 task covered.
