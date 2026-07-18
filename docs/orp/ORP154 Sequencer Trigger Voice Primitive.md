<!-- SPDX-License-Identifier: MIT -->

# ORP154 — Sequencer Trigger Voice Primitive

**Document type:** Public audio-utility contract and downstream handoff
**Version target:** Unreleased
**Status:** Implemented and verified
**Date:** 2026-07-16
**Downstream baseline:** FourTrack `1d1399e`

---

## Scope

ORP154 implements the host-neutral one-shot primitive requested by FourTrack's
FTR041 handoff [1]. `include/orpheus/trigger_voice.h` adds:

- `ITriggerVoice`, the composable load/trigger/render contract;
- `TriggerVoice`, the dependency-free implementation; and
- `VoicePolicy::{RetriggerCut, Polyphonic}`.

This is sample playback, not sequencing or synthesis. Pattern persistence,
sample import and resampling, the 10-second host limit, tempo and time-signature
math, grid scheduling, and the `IAudioFileReader` shell remain FourTrack
responsibilities. No existing SDK interface or C ABI table changed.

## Public contract

`loadSample()` runs off the audio thread. It copies interleaved PCM, replaces
all prior playback state, and allocates the complete voice pool. Loading must
not race `trigger()` or `render()`. A null pointer, zero frames, a non-positive
channel count, or a frame/channel size overflow unloads the current sample.

`RetriggerCut` always prepares one sounding voice. `Polyphonic` prepares at
least one and otherwise honors `maxVoices`; a trigger at capacity steals the
oldest active voice at the trigger's sample offset.

`trigger()` and `render()` are `noexcept`, allocate no memory, acquire no lock,
and perform no I/O. Up to `TriggerVoice::kMaxPendingTriggers` (64) events can be
submitted before each `render()` call. Events may be submitted out of offset
order. Excess events, non-finite gain, and non-finite or non-positive pitch
ratios are ignored. Each valid `offsetInBuffer` must be less than the frame
count passed to the following `render()`.

`render()` adds into, rather than clears, `numFrames * loadedChannels()`
interleaved output samples. It applies every queued trigger immediately before
mixing that output frame, advances already-sounding voices before a later
retrigger cuts or steals them, and carries active playback cursors across
callbacks. Linear interpolation honors arbitrary positive `pitchRatio` values;
`1.0F` reads the loaded PCM exactly.

Each `TriggerVoice` owns one sample and one polyphony budget. FourTrack's Seq
reader therefore composes four instances for its four independent sample
columns.

## Downstream adoption

A host schedules all events for one callback, then renders once:

```cpp
#include <orpheus/trigger_voice.h>

orpheus::TriggerVoice voice;
voice.loadSample(interleavedPcm.data(), frameCount, channels,
                 orpheus::VoicePolicy::Polyphonic, 4);

// Audio callback: offsets are relative to this callback buffer.
voice.trigger(firstStepOffset, firstGain, firstPitchRatio);
voice.trigger(secondStepOffset, secondGain, secondPitchRatio);
voice.render(interleavedOutput, callbackFrames);
```

The host must ensure that sample replacement is serialized against the audio
callback. For FourTrack, the trigger primitive is now sufficient to implement
`SeqTrackReader`, migrate `ClickTrackReader` voice bookkeeping, and enable Seq;
the SDK does not own the Click-versus-Seq lifecycle or session-clock walker.

## Verification

`trigger_voice_test` covers:

1. owned PCM copies and interleaved stereo mixing;
2. exact in-buffer offsets, including multiple out-of-order event submissions;
3. retrigger-cut timing;
4. bounded polyphonic layering and oldest-first stealing;
5. fractional-pitch linear interpolation across callbacks;
6. invalid loads, gains, and pitch ratios;
7. deterministic overflow behavior for the fixed 64-event queue;
8. a 480,000-frame sample; and
9. zero allocations and deallocations across 100 guarded trigger/render calls.

The installed `find_package` consumer compiles, links, and runs the public
header through `Orpheus::audio_utils`. The complete configured SDK suite passes
151 of 151 tests, including `realtime_static_audit`, `cmake_find_package`, and
`cmake_add_subdirectory`.

## References

[1] C. Lyons, “FTR041 SDK Handoff — Sequencer Trigger Voice Primitive,”
FourTrack engineering handoff, Jul. 2026. Local source:
`~/dev/fourtrack/docs/ftr/FTR041 SDK Handoff - Sequencer Trigger Voice Primitive.md`.
[Accessed: Jul. 16, 2026].
