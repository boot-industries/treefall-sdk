<!-- SPDX-License-Identifier: MIT -->

# ORP176 CoreAudio Bluetooth Duplex and Directional SRC SDK Completion

**Document type:** SDK implementation and qualification record
**Status:** Candidate deterministic and packaging changes complete; physical CLbuds gate blocked; no merge, tag, or release publication
**Date:** 2026-08-17
**Consumer:** FourTrack / EightTrack, FTR085
**Implementation baseline:** `release/orp176-coreaudio-directional-src` candidate `7ff29aa65b94b1ba5a34a24ca7a3a78b79ef42ed`, based on `origin/main` `5039642b` plus repair commits `5b777731`, `c1393153`, and `e41a1727`
**Merge commit:** Not created; physical acceptance is a release-blocking prerequisite
**Tag / release:** `v0.8.0` not created or published
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

## Deterministic and package evidence

Candidate verification on 2026-08-17:

| Gate | Result |
|---|---|
| `cmake --preset sdk-debug` | Passed |
| `realtime_static_audit`, `realtime_static_audit_unit`, `polyphase_resampler_test`, `coreaudio_route_probe_test` | 4/4 passed |
| `CoreAudioOutputOnlyInjectedTest.*` | 4/4 passed, including stale-input isolation, duplex reinitialize cleanup, and strict/fallback mono width |
| Directional SRC contracts | Passed for fixed 1 kHz tone/gain and long-run exact consumption at both 16,000→44,100 and 16,000→48,000 |
| `python3 tools/version_contract.py --check` and version/package ctest filter | 5/5 checks passed |
| Post-record `docs_path_audit` and `version_contract` | 2/2 passed |
| Release build and CPack | Built `orpheus-sdk-0.8.0-Darwin-arm64.zip` successfully |
| Extracted macOS package consumer | Configured, built, and ran 13/13 installed `find_package` tests successfully; `Orpheus::audio_driver_coreaudio` was present |
| Full configured release ctest | 79/80 CTest entries passed. The single failing entry was `coreaudio_driver_test`: 39/64 tests passed and 25 route-dependent tests failed because the current hardware route was unavailable. |

The acceptance tool now emits settled post-start and final route facts with
`session_host_callback_rate`, physical endpoint rates, AUHAL client rates,
directional conversion flags, converter latency, endpoint relation, callback
widths, width stability, callback/input frame deltas, and startup/final
telemetry. `--expect-route-outcome` accepts only `healthy`,
`sample-rate-changed`, `buffer-size-changed`, `format-changed`,
`input-route-unavailable`, and `output-route-unavailable`. No generic
`sample_rate` or ambiguous conversion JSON keys remain.

The public comments clarify that `actual_sample_rate` is the session/host-
callback rate; `*_physical_sample_rate` is physical endpoint rate;
`*_client_sample_rate` is the AUHAL client rate; `*_conversion_active` is
directional sample-rate conversion; converter-frame latency is converter
latency; and `endpoints_related` describes the selected persistent endpoint
IDs. Field names, order, and ABI were unchanged.

The full-suite failures are recorded, not suppressed. They are distinct from
the passing resolver, converter, route-monitor, output-only injection, static
audit, version, and installed-package contracts.

## Physical acceptance gate

The FourTrack audit source was taken verbatim from
`a5feb2edf3b732686bbd105d78cc50fbdb6c6b42` and compiled successfully as
`/tmp/ftr085-coreaudio-route-probe`. Its preflight inventory at
`2026-08-17T09:50:20.032Z` reported:

```text
device_list_status=ok total_count=16 count=16
default_input_id=166 uid=BuiltInMicrophoneDevice
default_output_id=159 uid=BuiltInSpeakerDevice
device_id=166 uid=BuiltInMicrophoneDevice
transport_normalized=built-in alive=true
related_device_ids=166,159 related_device_uids=BuiltInMicrophoneDevice,BuiltInSpeakerDevice ok
input_channels=1 output_channels=0
nominal_rate=48000.0
device_id=159 uid=BuiltInSpeakerDevice
transport_normalized=built-in alive=true
related_device_ids=159,166 related_device_uids=BuiltInSpeakerDevice,BuiltInMicrophoneDevice ok
input_channels=0 output_channels=2
nominal_rate=44100.0
```

No `CLbuds` input or output UID appeared in the 16-device inventory. No
physical input/output UID was therefore selected, and no profile-changing
operation, 30-second capture, mono-state run, related-duplex run, or
disconnect mutation was attempted. This follows the handoff gate: a virtual
device, emulated map, inferred Bluetooth label, or stale capture is not
acceptable.

No raw hardware acceptance JSON exists for this candidate because the
inventory preflight blocked the first matrix row. The required `ORP176`
records for settled rates, widths, converter latency, callbacks, tone, and
health counters remain unpopulated by design.

## Candidate package provenance

Local macOS package evidence was generated from candidate
`7ff29aa65b94b1ba5a34a24ca7a3a78b79ef42ed`:

```text
archive: orpheus-sdk-0.8.0-Darwin-arm64.zip
sha256: 6a4989f4e46017beb0c82eb99aeeaad914db4e4a6c0ff092f64210b8af252170
CPack .sha256: 6a4989f4e46017beb0c82eb99aeeaad914db4e4a6c0ff092f64210b8af252170
SHA256SUMS: 6a4989f4e46017beb0c82eb99aeeaad914db4e4a6c0ff092f64210b8af252170
```

The local SPDX 2.3 and in-toto statements identify the candidate source
commit, but they are not GitHub release assets. No Ubuntu archive, GitHub
Release asset URL, merge SHA, tag SHA, or published provenance URL exists.
The workflow implementation is present; its dual-platform GitHub run remains
blocked behind physical qualification and the required PR/merge gate.

## FourTrack adoption boundary

FourTrack must not repin or ship this candidate. After CLbuds is restored, run
the exact audit probe immediately before and after every profile-changing run,
capture each one-line JSON result verbatim, and complete every required
44.1/48 kHz duplex, output-only stereo, physical-mono strict/fallback, related
duplex, and route-mutation row. Only then may the candidate be reviewed,
merged, tagged, published, and adopted.

The source contract remains bounded: FourTrack requests
`RequestExactRateOrConvert` for the fixed session clock and uses
`AllowMonoFallback` only for the observed Bluetooth physical-mono policy.
It consumes the SDK's active output map and callback width verbatim. Output-
only sessions leave input selection empty and must not query, listen to, write
to, or request permission for a persisted input UID.


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
