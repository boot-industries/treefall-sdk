# ORP253 CoreAudio Output-Only Rate Recovery

**Status:** Implementation committed; SDK pull request pending  
**Date:** 2026-08-31  
**Branch:** `fix/orp253-coreaudio-output-rate`  
**Implementation commit:** `1bbd1fd6b5d4df850ad41d3a69cc94b8ddd49b04`  
**Scope:** CoreAudio output-only rate negotiation, passive stream-format monitoring, and deterministic route-plan regressions

## Problem

Clip Composer's OCC190 baseline consumes SDK commit
`5198766e5137ac212865019c100f794fbe3b31d6`. Its output-only adapter already
transferred the explicit output UID, physical map, requested rate, and requested
buffer into `AudioDriverConfig`, but left `sample_rate_policy` at the generated
configuration default, `PreserveDeviceRate`. A physical output at 44.1 kHz
therefore rejected a 48 kHz session request instead of negotiating a safe device
rate or using output sample-rate conversion.

The CoreAudio route monitor had a second, independent failure mechanism. After
`poll()` verified the device nominal rate, it still compared stream formats with
`mSampleRate` included. A stream-format notification carrying only the expected
rate convergence was consequently treated as terminal `FormatChanged`.

## Decision

`CoreAudioDriver::initializeAudioOutput()` now sets the generated configuration
to `AudioSampleRatePolicy::RequestExactRateOrConvert` before calling
`initialize(config)`. The request shape is unchanged. The adapter still transfers
its explicit output UID, `{0, 1}`-style physical map, requested sample rate, and
requested buffer, and still creates no input route.

The public `AudioOutputRouteRequest` comment now states the fixed CoreAudio
output-only policy. `requested_sample_rate` is a session target: CoreAudio first
attempts a safe nominal-rate change and otherwise uses output SRC. Callers cannot
select `PreserveDeviceRate` through this adapter. Dummy and other backend
implementations retain their own output-only semantics. No request field, enum
value, virtual slot, C ABI surface, or compatibility alias changed.

The private stream comparator is now `streamLayoutsEqual`. It excludes only
`AudioStreamBasicDescription::mSampleRate`; it continues comparing format ID,
flags, bytes/packet, frames/packet, bytes/frame, channels/frame, bits/channel,
and reserved fields. `poll()` checks each device's nominal rate first, so that
verified device-rate result remains authoritative for rate changes. A channel
or any other stream-layout mutation remains terminal `FormatChanged`.

## Rate-plan behavior

The resolver contracts use an explicit built-in output at 44,100 Hz advertising
both 44,100 and 48,000 Hz, with a 48,000 Hz
`RequestExactRateOrConvert` request and `num_inputs == 0`:

- When idle and nominal-rate-settable, the route resolves with one output plan
  requesting a 48,000 Hz device write. Output conversion is false and external
  output SRC is false.
- When already running somewhere, no device-rate write is planned. The route
  resolves with output conversion enabled, external output SRC enabled, and a
  planned output client rate of 44,100 Hz.
- When nominal-rate-settable is false, the same no-write 44,100 Hz output SRC
  fallback is selected.

All three cases use `CoreAudioRouteResolver::resolve(config, true)` directly so
the assertions observe `ResolvedCoreAudioRoute::device_rate_plans`. The
output-only cases perform zero input default or input-channel queries. OCC's
pre-SDK validation still rejects a requested rate that the selected endpoint
does not advertise; this change does not weaken that boundary.

## Verification

Observed in `/Users/chrislyons/dev/orpheus-sdk` using the existing configured
Debug tree with CoreAudio and extended tests enabled:

```text
cmake --build build --target coreaudio_driver_test coreaudio_route_probe_test --parallel 6
```

Both targets built successfully. The linker emitted the existing warning about
ignoring duplicate `libgtest.a` libraries.

```text
./build/tests/audio_io/coreaudio_driver_test \
  --gtest_filter='CoreAudioOutputOnlyInjectedTest.OutputRouteRequestUsesCooperativeRatePolicy:CoreAudioRouteMonitorTest.RateOnlyStreamConvergenceUsesVerifiedDeviceRate:CoreAudioRouteMonitorTest.StreamFormatChangeReportsFormatChanged'
```

All 3 focused GoogleTest cases passed. The output-only bridge test exercised
initialization, start, stop, direction state, explicit UID/map transfer, and
`RequestExactRateOrConvert`. The monitor test admitted 44.1 kHz stream values
under a verified 48 kHz device rate, then remained admitted after both stream
properties converged to 48 kHz. The unchanged channel-width mutation test still
returned `FormatChanged`.

```text
ctest --test-dir build --output-on-failure -R '^coreaudio_route_probe_test$'
```

The resolver CTest passed 1/1.

```text
ctest --test-dir build --output-on-failure
```

The configured suite ran 80 CTest entries. 79 entries passed; the only failing
entry was the pre-existing physical `coreaudio_driver_test` default-route
limitation. Its process ran 62 tests: 50 passed, 12 failed, and 1 remained
disabled. Every changed focused contract passed. The exact 12 legacy failures
were:

```text
CoreAudioDriverTest.TelemetrySaturatesAndIsVisibleThroughFactoryInterface
CoreAudioDriverTest.InitializeWithValidConfig
CoreAudioDriverTest.InitializeWithDefaultDevice
CoreAudioDriverTest.StartWithNullCallback
CoreAudioDriverTest.StartAndStop
CoreAudioDriverTest.PlaybackOnlyRouteStartsWithRuntimeRateMonitor
CoreAudioDriverTest.PlaybackOnlyRouteSupports256FrameBuffers
CoreAudioDriverTest.CannotStartTwice
CoreAudioDriverTest.CallbackIsInvoked
CoreAudioDriverTest.PublishesCallbackActiveClipCount
CoreAudioDriverTest.CallbackIsNotInvokedAfterStop
CoreAudioDriverTest.GetLatency
```

Each failure expected `SessionGraphError::OK` from a default or two-input
initialization path but observed `SessionGraphError` value `<02>`,
`InvalidParameter`. The same run passed the route probe, monitor, output-only
bridge, rate-transaction, directional, aggregate, and capture contracts. This
host-specific legacy default-device/two-input limitation is not treated as a
green full-suite result; no new failure occurred.

## Downstream boundary

After this SDK change is merged, Clip Composer must advance
`third_party/orpheus-sdk` and its active metadata pins from
`5198766e5137ac212865019c100f794fbe3b31d6` to the merged SDK `main` SHA. That
follow-up changes only the dependency gitlink and OCC records. No Clip Composer
application source change is part of ORP253.
