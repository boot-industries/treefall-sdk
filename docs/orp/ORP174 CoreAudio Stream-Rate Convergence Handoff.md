<!-- SPDX-License-Identifier: MIT -->

# ORP174 — CoreAudio Stream-Rate Convergence Handoff

**Document type:** SDK private-implementation handoff

**Status:** Implemented, verified, and pinned by Clip Composer

**Date:** 2026-08-24

**SDK repair commit:** `8f9fec044cd86312cfef763deb083398fe32257c`

**Downstream integration commit:** Clip Composer `c5ea7232`

**Related:** ORP128, ORP171; Clip Composer OCC185

---

## 1. Incident and decision

On the first rebuilt Clip Composer launch, the persisted explicit
`BuiltInSpeakerDevice` route requested 48 kHz / 512 frames while its output
stream was still 44.1 kHz. CoreAudio later converged the stream to 48 kHz;
automatic previous-session restoration constructed a replacement AUHAL. The
replacement had been initialized but did not inherit the application's request
to run. A clean relaunch then played normally.

The SDK contribution was that `CoreAudioRouteMonitor` treated an observed ASBD
rate-only change as a terminal structural change even after the device nominal
rate had been verified. The repair makes device nominal rate authoritative and
separates it from stream-layout identity.

## 2. SDK contract

`CoreAudioRouteMonitor::poll()` keeps its existing per-device order:

1. verify the device is alive;
2. read and, when possible, restore `expected_sample_rate_`;
3. verify the expected buffer size; then
4. compare virtual and physical stream layouts.

Refused or unverifiable nominal-rate restoration remains
`ReinitializationRequired`. A stream notification always closes the render gate
before the control-thread poll. Once the nominal device rate is already
verified, virtual and physical stream updates that differ only in `mSampleRate`
return `NoChange` and reopen that gate. A structural mismatch returns
`FormatChanged` and leaves it closed.

`streamLayoutsEqual(...)` deliberately excludes `mSampleRate`:

> `mSampleRate is intentionally excluded: device nominal rate is authoritative and verified before stream layout.`

The remaining structural ASBD fields are:

- `mFormatID`;
- `mFormatFlags`;
- `mBytesPerPacket`;
- `mFramesPerPacket`;
- `mBytesPerFrame`;
- `mChannelsPerFrame`;
- `mBitsPerChannel`; and
- `mReserved`.

This is private CoreAudio implementation behavior. It adds no public ABI and
keeps the existing post-0.6.7 version line.

## 3. Implementation and deterministic verification

Repair commit `8f9fec044cd86312cfef763deb083398fe32257c`
(`fix(coreaudio): tolerate verified stream-rate convergence`) changes only the
route-monitor comparison and its CoreAudio regression coverage.

The focused SDK gate was:

```sh
cmake -S . -B build-audio-lifecycle -DCMAKE_BUILD_TYPE=Debug \
  -DORP_WITH_TESTS=ON -DORPHEUS_ENABLE_EXTENDED_TESTS=ON
cmake --build build-audio-lifecycle --target coreaudio_driver_test --parallel 6
./build-audio-lifecycle/tests/audio_io/coreaudio_driver_test \
  --gtest_filter='CoreAudioRouteMonitorTest.*'
```

Observed result: **9 tests passed**. The new
`RateOnlyStreamConvergenceUsesVerifiedDeviceRate` starts with matching 44.1 kHz
virtual/physical layouts while the nominal device rate is already verified at
48 kHz, delivers both stream-format notifications, observes a closed gate, and
then observes `NoChange` plus an open gate after the control-thread poll.
`StreamFormatChangeReportsFormatChanged` remains the structural counterexample.

## 4. Hardware observation

The MacBook Pro Speakers were confirmed at 44.1 kHz before first launch;
`./build-launch.sh debug` reconfigured and rebuilt the app against the repaired
gitlink. CoreAudio unified logging recorded this first-launch sequence:

1. AUHAL initially observed a 44.1 kHz two-channel output stream;
2. the device stream converged to 48 kHz;
3. the first output I/O proc (`0xa`) started, then stopped during automatic
   previous-session restoration; and
4. the replacement I/O proc (`0xb`) started at 48 kHz and remained the active
   output route after the old I/O proc stopped.

`system_profiler SPAudioDataType` then reported MacBook Pro Speakers at 48 kHz.
A subsequent `./relaunch.sh` reproduced the same previous-session replacement
pattern and left the replacement output route started.

These are CoreAudio route-lifecycle observations, not a substitute for an
operator's audible-plugout check. The automated environment could not expose
Clip Composer's JUCE main window through macOS Accessibility or capture the
display, so Engineering Diagnostics, tab 1 slot 0 transport/meters, and
speaker audibility were not directly observed. The persisted route was never
substituted with System Default or Dummy.

## 5. Downstream obligations

Clip Composer owns requested-run intent independently of backend observation:

- retain intent after a failed activation or backend self-stop;
- start a replacement driver whenever that intent remains requested;
- clear intent only for explicit `AudioEngine::stop()`; and
- preserve failed transaction status after a rollback restart.

The application must not add endpoint fallback, sleep/retry timing policy, or a
second MainComponent run-intent flag. Existing restoration continues through
`PlaybackCommandDispatcher::quiesceForSessionChange()` and
`AudioEngine::setAudioConfiguration(..., restoreRegistrations=false)`.
