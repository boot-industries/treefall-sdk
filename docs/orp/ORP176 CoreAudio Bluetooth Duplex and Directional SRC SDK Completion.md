<!-- SPDX-License-Identifier: MIT -->

# ORP176 CoreAudio Bluetooth Duplex and Directional SRC SDK Completion

**Document type:** SDK implementation and qualification record
**Status:** Source and deterministic repair complete; final CLbuds 44.1/48 kHz rerun blocked by device disconnect
**Date:** 2026-08-13
**Consumer:** FourTrack / EightTrack, FTR085
**Implementation baseline:** `main` plus pending activation-admission and startup-FIFO repair
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
5. Capture SRC priming uses the active input callback size rather than the
   endpoint's maximum capacity. Once primed, capture delivery pauses while the
   output AUHAL starts, preventing a blocking Bluetooth output startup from
   exhausting the FIFO before output callbacks can consume it.
6. The route monitor admits only the fully activated route and captures each
   endpoint's nominal rate and buffer size plus each stream's virtual/physical
   formats. This permits directional Bluetooth startup transitions while later
   real mutations remain terminal.
7. Property changes publish one structured terminal outcome, close callback
   admission, and prevent stale route facts or repeated writes.

## Deterministic evidence

Source and deterministic evidence recorded in this session:

| Gate | Result |
|---|---|
| Directional SRC: canonical supported rate pairs, tone/frequency preservation, exact frame accounting, limits, reset, bounded FIFO and callback-path safety | Passed in `polyphase_resampler_test` |
| Resolver: output-only isolation, distinct endpoints, 16 kHz Bluetooth input conversion, strict mono conflict/fallback, related Bluetooth protection, rate-plan decisions | Passed in `coreaudio_route_probe_test` |
| Driver: dual-AUHAL flow, active physical/client facts, converter latency and counters, terminal monitor outcomes, rollback/lifecycle behavior | `coreaudio_driver_test`: 62/62 passed |
| Monitor activation admission: 16 kHz/320-frame input plus 44.1 kHz/512-frame output, activation rate/stream baselines, later terminal mutations | `CoreAudioRouteMonitorTest.*`: 13/13 passed |
| Static callback audit | `realtime_static_audit`, `realtime_static_audit_unit`, and adjacent-consumer audit passed with zero hard failures and zero tracked debt findings |
| Installed package and public version contract | `version_contract`, `cmake_find_package`, and `cmake_package_runtime_consumer` passed against 0.8.0 |
| Complete candidate Debug suite | `/tmp/ftr085-sdk-debug`: 80/80 CTest cases passed |
| Complete candidate Release suite | `/tmp/ftr085-sdk-release`: 80/80 CTest cases passed |
| Realtime static and dynamic allocation gate | Static audit: zero hard/debt findings; prepared SRC transfers: zero allocation/deallocation violations |
| Acceptance CLI contract | Built-in output-only pass and expected unavailable-output initialization pass; no CLbuds result inferred |

On 2026-08-12, live CLbuds testing exposed three defects beyond the original
deterministic qualification. The monitor used one output-sized buffer
expectation for both endpoints, then admitted pre-activation sample-rate and
stream-format facts. At 48 kHz, input priming used the endpoint's maximum
capacity and capture continued while output AUHAL startup blocked, eventually
overflowing the FIFO before output callbacks could drain it.

The repair now baselines rate, buffer, and stream formats per endpoint only
after full activation, primes from the active 320-frame input callback, and
freezes the primed FIFO during output startup. The monitor and complete
CoreAudio-driver deterministic suites pass.

An intermediate 44.1 kHz hardware run passed with 498 callbacks, 219,618
host/input frames, a non-zero captured peak, healthy route outcome, and zero
render, FIFO, or conversion failures. The following 48 kHz run exposed the FIFO
startup defect. CLbuds disconnected before both rates could be rerun against the
final source, so final-source physical capture and FourTrack ARM acceptance are
not claimed.

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

FourTrack now resolves an unavailable persisted output preference through
CoreAudio's current live default without rewriting explicit route overrides. A
disconnected CLbuds probe resolved to `BuiltInSpeakerDevice`, whose advertised
ranges include both 44.1 and 48 kHz; the Swift policy test and signed app build
passed.

The final physical Bluetooth gate remains open. Do not tag a release, mark
FTR085 complete, or advance FourTrack's production SDK pin until CLbuds is
reconnected and both 44.1 and 48 kHz runs record settled endpoint UIDs,
physical/client rates, physical callback width, converter latency, counters,
and tone/frame evidence.

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
