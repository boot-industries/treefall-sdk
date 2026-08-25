<!-- SPDX-License-Identifier: MIT -->

# ORP251 CoreAudio AudioOutputUnitStart Status Telemetry

**Document type:** SDK implementation and downstream handoff record
**Status:** Implemented on the ORP251 feature branch; FourTrack adoption pending
**Date:** 2026-08-25
**Consumer:** FourTrack / EightTrack
**Issue:** [#251](https://github.com/chrislyons/orpheus-sdk/issues/251)
**Related:** ORP155, ORP156, ORP171

## Decision

CoreAudio route startup retains the native status returned by
`AudioOutputUnitStart` without changing the existing generic error or terminal
route-outcome contracts. The diagnostic is a bounded append to the C++
telemetry header, not a new route outcome, retry policy, startup order, or
CoreAudio dependency in the host-neutral public header.

## Public contract

`include/orpheus/audio_driver.h` appends this member to `AudioIoTelemetry`,
after the existing failure counters:

```cpp
int32_t route_backend_error = 0;
```

The field is a signed 32-bit scalar because macOS `OSStatus` values must be
preserved exactly without exposing a CoreAudio type in the public header.
`0` is the sentinel meaning that no platform-native backend error code is
available. A nonzero value is the platform-native code for the terminal route
outcome; it is not an alternative outcome category.

`AudioRouteRuntimeOutcome::BackendFailure` remains the terminal category and
`SessionGraphError::InternalError` remains the startup error. The stable C ABI
is unchanged. `AudioIoTelemetry` is returned by value, so installed C++
consumers must rebuild against the new header and library together.

## CoreAudio publication and lifetime

`CoreAudioDriver` stores the diagnostic in an atomic signed 32-bit field. It
records only a non-`noErr` result from the stored `AudioOutputUnitStart`
function, for both the optional input-conversion AudioUnit and the required
output AudioUnit, before executing the existing cleanup and failure
publication paths. Other route failures do not synthesize a native code, and
`AudioIoRouteState::detail` remains unchanged.

A successful `initialize()` resets `route_backend_error` to `0` alongside the
existing per-route telemetry counters. Failed initialization does not clear a
previous completed route's diagnostic, matching the existing failure-counter
lifetime: the value is replaced only by a later startup status or a successful
fresh configuration. The function-pointer injection is a private CoreAudio
test seam; it is not an `IAudioDriver` API or an AudioUnit abstraction.

## FourTrack adoption boundary

FourTrack must rebuild against the released SDK and carry
`route_backend_error` through the macOS bridge's `FTAudioRouteState` and
`apply_telemetry_facts`, expose it through `EngineController`'s route model,
and include the numeric native status in a failed new-session route message
only when `route_outcome` is `BackendFailure` and `route_backend_error != 0`.
FourTrack must consume this scalar rather than parse backend-specific text.
Existing route startup and retry behavior remains unchanged. Bridge and
Swift-model coverage must exercise both zero and nonzero values.

## Verification record

The output-only CoreAudio regression passed:

- `CoreAudioOutputOnlyInjectedTest.OutputStartFailurePublishesNativeStatus`
  initialized a real output-only AUHAL route, injected `OSStatus(-50'001)`,
  returned `SessionGraphError::InternalError`, retained
  `BackendFailure`, published the exact signed `route_backend_error`, left
  `isRunning() == false`, invoked no callback, and reset the field to `0`
  after successful reinitialization;
- `PreserveRateMismatchIsRejectedWithoutPropertyWrites` retained
  `SampleRateChanged` with `route_backend_error == 0`; and
- the registered `coreaudio_driver_test` executable and `cmake_find_package`
  installed-package fixture passed.

The final configured Debug tree built successfully and
`ctest --test-dir build --output-on-failure` passed all 80/80 enabled tests,
including CoreAudio, Dummy, realtime, installed-package, and
add-subdirectory contracts. No hardware test was skipped in this run; the
CoreAudio executable reports one disabled test. The changed-file
`clang-format --dry-run --Werror` check and `git diff --check` passed.
`scripts/lint-cpp.sh` remains red on pre-existing formatting violations in
unrelated files outside this change. WASAPI still has no local Windows compile
or execution evidence.

## References

[1] Apple Inc., “AudioOutputUnitStart(_:),” *Audio Toolbox Documentation*.
[Online]. Available:
https://developer.apple.com/documentation/audiotoolbox/audiooutputunitstart%28_%3A%29.
[Accessed: Aug. 25, 2026].
