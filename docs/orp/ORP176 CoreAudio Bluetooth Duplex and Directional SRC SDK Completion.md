<!-- SPDX-License-Identifier: MIT -->

# ORP176 CoreAudio Bluetooth Duplex and Directional SRC SDK Completion

**Document type:** SDK implementation and qualification record
**Status:** Source and deterministic/package qualification complete; CLbuds activation repair awaiting physical acceptance
**Date:** 2026-08-12
**Consumer:** FourTrack / EightTrack, FTR085
**Implementation baseline:** `main` plus pending directional-buffer monitor repair
**SDK version:** 0.8.0

## Decision

Orpheus SDK 0.8.0 implements the CoreAudio side of FTR085. It preserves a host-selected
44.1/48 kHz session clock while a CoreAudio input endpoint may run at its own physical
rate, including a 16 kHz Bluetooth microphone. Conversion occurs only at the SDK I/O
boundary. The host receives settled physical/client rate and width facts, converter
latencies, and callback-health counters; it does not probe, resample, or infer a route.

The scope is deliberately bounded. Existing same-rate routes retain their prior behavior.
The only width exception is same-headset Bluetooth duplex where an explicitly requested
logical stereo output cannot be physically prepared: strict policy rejects the route;
`AllowMonoFallback` accepts a single physical output channel and exposes callback width
one. FourTrack retains its stereo program path and folds the final monitor to
`0.5f * (L + R)`.

## Implemented public contract

`include/orpheus/audio_driver.h` extends append-only route contracts:

- `AudioSampleRatePolicy::RequestExactRateOrConvert` requests the session client rate
  while allowing direction-specific conversion when a supported physical rate differs.
- `AudioOutputChannelPolicy::AllowMonoFallback` authorizes only the documented
  same-headset physical-width fallback.
- `AudioRouteCompatibility` reports conversion requirements, planned client rates,
  settled route width, Bluetooth/endpoint relation, and the rate-plan decision.
- `ActiveAudioRoute`, `AudioRouteLatency`, and `AudioIoTelemetry` report physical and
  client rates, virtual/client widths, converter activity/latency, mono fallback, and
  saturating conversion/FIFO/capture failure counters.
- `IAudioDriver::getAudioIoRouteState()` is the authoritative detailed route/latency
  snapshot. Existing telemetry fields retain their order and semantics.

The 0.8.0 bump follows the existing single-source `project(orpheus VERSION ...)` contract;
C ABI remains 1.0 because the C-facing ABI contract is unchanged.

## CoreAudio activation model

1. Endpoint discovery resolves persistent DeviceUIDs independently and validates input
   and output direction before activation. Empty IDs select only that direction's default.
2. Resolver facts include transport, physical nominal rate/ranges, channel-map capacity,
   related-device identity, and a deduplicated per-device global-rate plan.
3. Cooperative sample-rate transactions write only plan entries requiring a write, wait
   for listener-confirmed settlement, and roll back only a write owned by that transaction.
   Related Bluetooth endpoints retain their observed native rate instead of receiving an
   unsafe device-global write.
4. CoreAudio configures independent input/output AUHAL paths. The bounded
   `DirectionalSampleRateConverter` transfers capture from physical to session rate and
   render from session to physical rate without allocation, locks, or I/O in the callback.
   Converted callbacks derive every chunk position from one session-frame base and advance
   the timeline only after the full callback, preserving contiguous sample positions.
5. The route monitor baselines each physical endpoint's own buffer size when
   listener admission begins. This is required for directional Bluetooth
   routes, where a 16 kHz input may use 320 frames while a 44.1 kHz output uses
   512 frames; subsequent mutation remains terminal.
6. The route monitor treats property changes as terminal, publishes one structured outcome,
   closes callback admission, and prevents stale route facts or repeated writes.

## Deterministic evidence

Source and deterministic evidence recorded in this session:

| Gate | Result |
|---|---|
| Directional SRC: canonical supported rate pairs, tone/frequency preservation, exact frame accounting, limits, reset, bounded FIFO and callback-path safety | Passed in `polyphase_resampler_test` |
| Resolver: output-only isolation, distinct endpoints, 16 kHz Bluetooth input conversion, strict mono conflict/fallback, related Bluetooth protection, rate-plan decisions | Passed in `coreaudio_route_probe_test` |
| Driver: dual-AUHAL flow, active physical/client facts, converter latency and counters, terminal monitor outcomes, rollback/lifecycle behavior | Passed in `coreaudio_driver_test` |
| Static callback audit | `realtime_static_audit` and `realtime_static_audit_unit` passed |
| Complete configured SDK tree | 80/80 CTest contracts passed in `build-sdk-debug` |
| Installed package and public version contract | `version_contract`, `cmake_find_package`, and `cmake_package_runtime_consumer` passed against 0.8.0 |

On 2026-08-12, the CLbuds endpoints became available for the first physical
activation attempt. Their separate UIDs report a 16 kHz, 320-frame, one-channel
input and a 44.1 kHz, 512-frame, two-channel output. ARM exposed a false
`BufferSizeChanged` outcome because monitoring applied the output callback size
to every endpoint. The per-endpoint baseline repair has deterministic coverage;
the CLbuds capture/monitor run must still demonstrate live frames and audibility.

## FourTrack adoption boundary

FourTrack must request `RequestExactRateOrConvert` for the fixed session clock and use
`AllowMonoFallback` only for its existing Bluetooth same-headset policy. It must use the
SDK's active output map and callback width verbatim; no Swift-side duplicated mono
channel or UI/control-path SRC is permitted. Take placement remains latched at record
start using:

```text
round_trip_frames = capture_frames + playback_frames + processing_frames
```

`processing_frames` includes active input/output converter latency from the settled SDK
route state. Output-only sessions must leave input selection empty: no input capability
query, listener, device write, capture failure, or permission request is allowed.

## Remaining physical gate

Physical Bluetooth acceptance is not claimed. The 2026-08-11 machine inventory had no
CLbuds endpoint, so the required Bluetooth-input-plus-speakers, same-headset stereo,
same-headset mono strict/fallback, and unrelated Bluetooth endpoint measurements remain
unexecuted. Do not tag a release, mark FTR085 complete, or advance FourTrack's production
SDK pin until a CLbuds run records the settled endpoint UIDs, physical/client rates,
physical callback width, converter latency, counters, and tone/frame evidence.

## References

[1] FourTrack, “FTR085 CoreAudio Bluetooth Duplex and Directional SRC SDK Handoff,”
Aug. 11, 2026, §§ Acceptance matrix and Handoff deliverables.

[2] Apple Inc., “Device input using the HAL Output Audio Unit,” *Technical Note
TN2091*, Jan. 21, 2014. [Online]. Available:
https://developer.apple.com/library/archive/technotes/tn2091/_index.html.
[Accessed: Aug. 11, 2026].

[3] Apple Inc., “kAudioDevicePropertyNominalSampleRate,” *Core Audio
Documentation*. [Online]. Available:
https://developer.apple.com/documentation/coreaudio/kaudiodevicepropertynominalsamplerate.
[Accessed: Aug. 11, 2026].

[4] Apple Inc., “kAudioDevicePropertyLatency,” *Core Audio Documentation*.
[Online]. Available:
https://developer.apple.com/documentation/coreaudio/kaudiodevicepropertylatency.
[Accessed: Aug. 11, 2026].
