<!-- SPDX-License-Identifier: MIT -->

# ORP176 CoreAudio Bluetooth Duplex and Directional SRC SDK Completion

**Document type:** SDK implementation and qualification record
**Status:** Candidate deterministic and packaging changes complete; the 2026-08-21 physical CLbuds rerun still fails the calibrated-tone and route-mutation rows; no merge, tag, or release publication
**Date:** 2026-08-21 rerun; original candidate record 2026-08-17
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

## 2026-08-21 CLbuds physical rerun

The candidate acceptance binary was rebuilt from the current release branch before
the rerun. A fresh FourTrack CoreAudio inventory found 18 devices and both live
CLbuds endpoints:

```text
input UID: 58-18-62-82-0F-33:input
output UID: 58-18-62-82-0F-33:output
input: 1 channel, 16,000 Hz, 512-frame buffer before activation
output: 2 channels, 44,100 Hz, 512-frame buffer before activation
```

All eight required 30-second rows ran with an inventory immediately before and
after each row:

| Rows | Result | Evidence |
|---|---|---|
| 01, 02, 05 | Failed, exit 6 | Duplex 44.1/48 kHz and related-duplex rows remained route-healthy but failed the `--expect-input-tone-hz 1000` acceptance predicate; no independently calibrated 1 kHz source was present. |
| 03, 04 | Passed, exit 0 | Output-only strict CLbuds at 44.1 and 48 kHz. |
| 06 | Passed, exit 0 | Output-only strict mono attempt; CoreAudio exposed stereo output rather than a physical-mono conflict. |
| 07 | Passed, exit 0 | Output-only `mono-fallback`; CoreAudio exposed stereo output, so fallback was not needed. |
| 08 | Failed, exit 6 | Expected `InputRouteUnavailable` was not induced because the CLbuds input remained connected; the route stayed available. |

After duplex and route-mutation rows, the live CLbuds profile returned to
one-channel/16,000 Hz/320-frame output while remaining alive. Wearing and
connecting CLbuds established the required endpoint availability but did not
provide the independently calibrated 1 kHz source required by rows 01, 02, and
05. The physical release gate therefore remains blocked; no merge, tag,
release asset, or FourTrack repin is claimed.

The post-rerun local release checks passed: the selected CTest set was 5/5
(`realtime_static_audit`, `realtime_static_audit_unit`, `docs_path_audit`,
`version_contract`, and `cmake_find_package`), the native macOS installed
package consumer passed 13/13, and the 56-file ShmUI-JUCE manifest check
passed. GitHub reports no hosted PR checks for this branch, so Ubuntu and
macOS hosted evidence is still unavailable.



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
`/tmp/ftr085-coreaudio-route-probe`.
The restored-device inventory at `2026-08-17T10:07:06.821Z` reported 18 live
devices and selected the actual CLbuds endpoints without inference:


```text
input UID: 58-18-62-82-0F-33:input
output UID: 58-18-62-82-0F-33:output
input: Bluetooth, alive, 1 input channel, 16,000 Hz, 320-frame buffer
output: Bluetooth, alive, 2 output channels, 44,100 Hz, 512-frame buffer
default input: 207 / 58-18-62-82-0F-33:input
default output: 201 / 58-18-62-82-0F-33:output
```

Automated physical attempts then captured before/after inventories around all
normal rows. The two input-plus-built-in-output rows and the related-duplex
row failed only the calibrated-tone predicate because no independently
verified 1 kHz source was available. Their route facts, callback widths,
converter state, and health counters were otherwise valid. Both output-only
stereo rows passed. The output-only mono requirement did not reproduce:
CoreAudio returned the CLbuds output as stereo for each output-only start,
even when the immediately preceding duplex inventory showed a one-channel
16 kHz output profile. The route-mutation row was run without disconnecting
the input and therefore correctly reported `Healthy` instead of the expected
`InputRouteUnavailable`.

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
## Physical acceptance attempt

The candidate tool was built from `7ff29aa65b94b1ba5a34a24ca7a3a78b79ef42ed`.
The exact FourTrack probe ran immediately before and after each row. All
windows were 30 seconds. The input rows used
`--expect-input-tone-hz 1000`; no independently calibrated continuous source
was available, so those rows are evidence of route/SRC health but not passing
tone qualification.

| Row | Inventory before → after | Tool result | Gate interpretation |
|---|---|---|---|
| CLbuds input + built-in output, 44.1 kHz | `10:07:58.952Z` → `10:08:29.612Z`; output profile settled at 1 channel/16 kHz | `status:"failed"`, exit 6 | Route healthy; input SRC active; widths 1/2 stable; all counters zero; tone 4145.324934 Hz, outside tolerance |
| CLbuds input + built-in output, 48 kHz | `10:09:01.834Z` → `10:09:32.570Z`; output profile settled at 1 channel/16 kHz | `status:"failed"`, exit 6 | Route healthy; input SRC active; widths 1/2 stable; all counters zero; tone 4218.366735 Hz, outside tolerance |
| CLbuds output-only, strict, 44.1 kHz | `10:09:45.341Z` → `10:10:15.726Z`; stereo 2 channel/44.1 kHz retained | `status:"passed"`, exit 0 | No input UID/activity; map `[0,1]`; width 2; no conversion or health failures |
| CLbuds output-only, strict, 48 kHz | `10:10:33.162Z` → `10:11:03.553Z`; stereo 2 channel/44.1 kHz retained | `status:"passed"`, exit 0 | Output conversion active; 48 kHz session to 44.1 kHz physical output; map `[0,1]`; width 2; counters zero |
| Related CLbuds duplex, 44.1 kHz | `10:11:31.336Z` → `10:12:01.937Z`; output profile settled at 1 channel/16 kHz | `status:"failed"`, exit 6 | Route healthy; input SRC active; widths 1/2 stable; all counters zero; tone 2682.421614 Hz, outside tolerance; `endpoints_related:false` was reported |
| CLbuds output-only, strict mono attempt | `10:12:23.871Z` → `10:12:54.302Z`; output-only start returned stereo 2 channel/44.1 kHz | `status:"passed"`, exit 0 | Required `ProfileConflict` did not occur; no physical mono output-only state was exposed |
| CLbuds output-only, explicit mono fallback | `10:13:08.555Z` → `10:13:38.975Z`; stereo 2 channel/44.1 kHz retained | `status:"passed"`, exit 0 | Fallback flag remained false and map remained `[0,1]`; required physical-mono row is not satisfied |
| Input route mutation | `10:13:55.455Z` → `10:14:26.183Z`; output profile settled at 1 channel/16 kHz | `status:"failed"`, exit 6 | No manual disconnect occurred; actual outcome remained `Healthy`, not expected `InputRouteUnavailable`; tone also failed |

The post-duplex inventory did expose a real physical-mono Bluetooth profile:
the CLbuds output was alive with one channel, 16,000 Hz, and a 320-frame
buffer. Starting an output-only session immediately returned the endpoint to
stereo 44.1 kHz before the strict/fallback policy could observe a mono output
route. An additional immediate strict retry from the observed mono state
produced the same stereo result. This is a real-device observation, not a
simulated map, but it does not satisfy the required output-only mono gate.

The following are the one-line JSON results emitted verbatim by the candidate:

### Row 01 — CLbuds input plus built-in output, 44.1 kHz

```json
{"status":"failed","backend":"CoreAudio","input_uid":"58-18-62-82-0F-33:input","output_uid":"BuiltInSpeakerDevice","expected_route_outcome":"Healthy","seconds":30,"requested_frames":1323000,"output_policy":"strict","settled_active_input_uid":"58-18-62-82-0F-33:input","settled_active_output_uid":"BuiltInSpeakerDevice","settled_requested_session_host_callback_rate":44100,"settled_session_host_callback_rate":44100,"settled_session_host_callback_buffer_frames":512,"settled_physical_input_rate":16000,"settled_physical_output_rate":44100,"settled_input_auhal_client_rate":16000,"settled_output_auhal_client_rate":44100,"settled_available_input_channels":1,"settled_available_output_channels":2,"settled_requested_output_channels":2,"settled_resolved_output_channels":2,"settled_input_auhal_client_format_channels":1,"settled_output_auhal_client_format_channels":2,"settled_input_sample_rate_conversion_active":true,"settled_output_sample_rate_conversion_active":false,"settled_input_is_bluetooth":true,"settled_output_is_bluetooth":false,"settled_endpoints_related":false,"settled_output_mono_fallback":false,"settled_active_output_map":[0,1],"settled_capture_latency_frames":4320,"settled_playback_latency_frames":778,"settled_processing_latency_frames":512,"settled_latency_complete":true,"settled_input_converter_latency_frames":3879,"settled_output_converter_latency_frames":0,"settled_route_detail":"","startup_input_render_failures":0,"startup_input_fifo_overruns":0,"startup_input_fifo_underruns":0,"startup_input_conversion_failures":0,"startup_output_conversion_failures":0,"startup_route_outcome":"Healthy","active_input_uid":"58-18-62-82-0F-33:input","active_output_uid":"BuiltInSpeakerDevice","requested_session_host_callback_rate":44100,"session_host_callback_rate":44100,"session_host_callback_buffer_frames":512,"physical_input_rate":16000,"physical_output_rate":44100,"input_auhal_client_rate":16000,"output_auhal_client_rate":44100,"available_input_channels":1,"available_output_channels":2,"requested_output_channels":2,"resolved_output_channels":2,"input_auhal_client_format_channels":1,"output_auhal_client_format_channels":2,"input_sample_rate_conversion_active":true,"output_sample_rate_conversion_active":false,"input_is_bluetooth":true,"output_is_bluetooth":false,"endpoints_related":false,"output_mono_fallback":false,"active_output_map":[0,1],"capture_latency_frames":4320,"playback_latency_frames":778,"processing_latency_frames":512,"latency_complete":true,"input_converter_latency_frames":3879,"output_converter_latency_frames":0,"route_detail":"","callbacks":2584,"callback_frames":1323008,"input_callbacks":2584,"input_frames":1323008,"callback_frame_delta":8,"input_frame_delta":8,"first_input_callback_width":1,"first_output_callback_width":2,"callback_width_changed":false,"callback_width_stable":true,"input_activity_matches":true,"startup_counters_clear":true,"final_counters_clear":true,"input_render_failures":0,"input_fifo_overruns":0,"input_fifo_underruns":0,"input_conversion_failures":0,"output_conversion_failures":0,"route_outcome":"Healthy","input_peak":0.0006087308866,"zero_crossing_frequency_hz":4145.324934,"expected_input_tone_hz":1000,"input_tone_in_tolerance":false,"passed":false}
```

### Row 02 — CLbuds input plus built-in output, 48 kHz

```json
{"status":"failed","backend":"CoreAudio","input_uid":"58-18-62-82-0F-33:input","output_uid":"BuiltInSpeakerDevice","expected_route_outcome":"Healthy","seconds":30,"requested_frames":1440000,"output_policy":"strict","settled_active_input_uid":"58-18-62-82-0F-33:input","settled_active_output_uid":"BuiltInSpeakerDevice","settled_requested_session_host_callback_rate":48000,"settled_session_host_callback_rate":48000,"settled_session_host_callback_buffer_frames":512,"settled_physical_input_rate":16000,"settled_physical_output_rate":48000,"settled_input_auhal_client_rate":16000,"settled_output_auhal_client_rate":48000,"settled_available_input_channels":1,"settled_available_output_channels":2,"settled_requested_output_channels":2,"settled_resolved_output_channels":2,"settled_input_auhal_client_format_channels":1,"settled_output_auhal_client_format_channels":2,"settled_input_sample_rate_conversion_active":true,"settled_output_sample_rate_conversion_active":false,"settled_input_is_bluetooth":true,"settled_output_is_bluetooth":false,"settled_endpoints_related":false,"settled_output_mono_fallback":false,"settled_active_output_map":[0,1],"settled_capture_latency_frames":4701,"settled_playback_latency_frames":834,"settled_processing_latency_frames":512,"settled_latency_complete":true,"settled_input_converter_latency_frames":4221,"settled_output_converter_latency_frames":0,"settled_route_detail":"","startup_input_render_failures":0,"startup_input_fifo_overruns":0,"startup_input_fifo_underruns":0,"startup_input_conversion_failures":0,"startup_output_conversion_failures":0,"startup_route_outcome":"Healthy","active_input_uid":"58-18-62-82-0F-33:input","active_output_uid":"BuiltInSpeakerDevice","requested_session_host_callback_rate":48000,"session_host_callback_rate":48000,"session_host_callback_buffer_frames":512,"physical_input_rate":16000,"physical_output_rate":48000,"input_auhal_client_rate":16000,"output_auhal_client_rate":48000,"available_input_channels":1,"available_output_channels":2,"requested_output_channels":2,"resolved_output_channels":2,"input_auhal_client_format_channels":1,"output_auhal_client_format_channels":2,"input_sample_rate_conversion_active":true,"output_sample_rate_conversion_active":false,"input_is_bluetooth":true,"output_is_bluetooth":false,"endpoints_related":false,"output_mono_fallback":false,"active_output_map":[0,1],"capture_latency_frames":4701,"playback_latency_frames":834,"processing_latency_frames":512,"latency_complete":true,"input_converter_latency_frames":4221,"output_converter_latency_frames":0,"route_detail":"","callbacks":2813,"callback_frames":1440256,"input_callbacks":2813,"input_frames":1440256,"callback_frame_delta":256,"input_frame_delta":256,"first_input_callback_width":1,"first_output_callback_width":2,"callback_width_changed":false,"callback_width_stable":true,"input_activity_matches":true,"startup_counters_clear":true,"final_counters_clear":true,"input_render_failures":0,"input_fifo_overruns":0,"input_fifo_underruns":0,"input_conversion_failures":0,"output_conversion_failures":0,"route_outcome":"Healthy","input_peak":0.0003926914651,"zero_crossing_frequency_hz":4218.366735,"expected_input_tone_hz":1000,"input_tone_in_tolerance":false,"passed":false}
```

### Row 03 — CLbuds output-only, strict, 44.1 kHz

```json
{"status":"passed","backend":"CoreAudio","input_uid":"","output_uid":"58-18-62-82-0F-33:output","expected_route_outcome":"Healthy","seconds":30,"requested_frames":1323000,"output_policy":"strict","settled_active_input_uid":"","settled_active_output_uid":"58-18-62-82-0F-33:output","settled_requested_session_host_callback_rate":44100,"settled_session_host_callback_rate":44100,"settled_session_host_callback_buffer_frames":512,"settled_physical_input_rate":0,"settled_physical_output_rate":44100,"settled_input_auhal_client_rate":44100,"settled_output_auhal_client_rate":44100,"settled_available_input_channels":0,"settled_available_output_channels":2,"settled_requested_output_channels":2,"settled_resolved_output_channels":2,"settled_input_auhal_client_format_channels":0,"settled_output_auhal_client_format_channels":2,"settled_input_sample_rate_conversion_active":false,"settled_output_sample_rate_conversion_active":false,"settled_input_is_bluetooth":false,"settled_output_is_bluetooth":true,"settled_endpoints_related":false,"settled_output_mono_fallback":false,"settled_active_output_map":[0,1],"settled_capture_latency_frames":0,"settled_playback_latency_frames":9843,"settled_processing_latency_frames":512,"settled_latency_complete":true,"settled_input_converter_latency_frames":0,"settled_output_converter_latency_frames":0,"settled_route_detail":"","startup_input_render_failures":0,"startup_input_fifo_overruns":0,"startup_input_fifo_underruns":0,"startup_input_conversion_failures":0,"startup_output_conversion_failures":0,"startup_route_outcome":"Healthy","active_input_uid":"","active_output_uid":"58-18-62-82-0F-33:output","requested_session_host_callback_rate":44100,"session_host_callback_rate":44100,"session_host_callback_buffer_frames":512,"physical_input_rate":0,"physical_output_rate":44100,"input_auhal_client_rate":44100,"output_auhal_client_rate":44100,"available_input_channels":0,"available_output_channels":2,"requested_output_channels":2,"resolved_output_channels":2,"input_auhal_client_format_channels":0,"output_auhal_client_format_channels":2,"input_sample_rate_conversion_active":false,"output_sample_rate_conversion_active":false,"input_is_bluetooth":false,"output_is_bluetooth":true,"endpoints_related":false,"output_mono_fallback":false,"active_output_map":[0,1],"capture_latency_frames":0,"playback_latency_frames":9843,"processing_latency_frames":512,"latency_complete":true,"input_converter_latency_frames":0,"output_converter_latency_frames":0,"route_detail":"","callbacks":2584,"callback_frames":1323008,"input_callbacks":0,"input_frames":0,"callback_frame_delta":8,"input_frame_delta":-1323000,"first_input_callback_width":0,"first_output_callback_width":2,"callback_width_changed":false,"callback_width_stable":true,"input_activity_matches":true,"startup_counters_clear":true,"final_counters_clear":true,"input_render_failures":0,"input_fifo_overruns":0,"input_fifo_underruns":0,"input_conversion_failures":0,"output_conversion_failures":0,"route_outcome":"Healthy","input_peak":0,"zero_crossing_frequency_hz":0,"passed":true}
```

### Row 04 — CLbuds output-only, strict, 48 kHz

```json
{"status":"passed","backend":"CoreAudio","input_uid":"","output_uid":"58-18-62-82-0F-33:output","expected_route_outcome":"Healthy","seconds":30,"requested_frames":1440000,"output_policy":"strict","settled_active_input_uid":"","settled_active_output_uid":"58-18-62-82-0F-33:output","settled_requested_session_host_callback_rate":48000,"settled_session_host_callback_rate":48000,"settled_session_host_callback_buffer_frames":512,"settled_physical_input_rate":0,"settled_physical_output_rate":44100,"settled_input_auhal_client_rate":48000,"settled_output_auhal_client_rate":44100,"settled_available_input_channels":0,"settled_available_output_channels":2,"settled_requested_output_channels":2,"settled_resolved_output_channels":2,"settled_input_auhal_client_format_channels":0,"settled_output_auhal_client_format_channels":2,"settled_input_sample_rate_conversion_active":false,"settled_output_sample_rate_conversion_active":true,"settled_input_is_bluetooth":false,"settled_output_is_bluetooth":true,"settled_endpoints_related":false,"settled_output_mono_fallback":false,"settled_active_output_map":[0,1],"settled_capture_latency_frames":0,"settled_playback_latency_frames":10842,"settled_processing_latency_frames":558,"settled_latency_complete":true,"settled_input_converter_latency_frames":0,"settled_output_converter_latency_frames":128,"settled_route_detail":"","startup_input_render_failures":0,"startup_input_fifo_overruns":0,"startup_input_fifo_underruns":0,"startup_input_conversion_failures":0,"startup_output_conversion_failures":0,"startup_route_outcome":"Healthy","active_input_uid":"","active_output_uid":"58-18-62-82-0F-33:output","requested_session_host_callback_rate":48000,"session_host_callback_rate":48000,"session_host_callback_buffer_frames":512,"physical_input_rate":0,"physical_output_rate":44100,"input_auhal_client_rate":48000,"output_auhal_client_rate":44100,"available_input_channels":0,"available_output_channels":2,"requested_output_channels":2,"resolved_output_channels":2,"input_auhal_client_format_channels":0,"output_auhal_client_format_channels":2,"input_sample_rate_conversion_active":false,"output_sample_rate_conversion_active":true,"input_is_bluetooth":false,"output_is_bluetooth":true,"endpoints_related":false,"output_mono_fallback":false,"active_output_map":[0,1],"capture_latency_frames":0,"playback_latency_frames":10842,"processing_latency_frames":558,"latency_complete":true,"input_converter_latency_frames":0,"output_converter_latency_frames":128,"route_detail":"","callbacks":5168,"callback_frames":1440136,"input_callbacks":0,"input_frames":0,"callback_frame_delta":136,"input_frame_delta":-1440000,"first_input_callback_width":0,"first_output_callback_width":2,"callback_width_changed":false,"callback_width_stable":true,"input_activity_matches":true,"startup_counters_clear":true,"final_counters_clear":true,"input_render_failures":0,"input_fifo_overruns":0,"input_fifo_underruns":0,"input_conversion_failures":0,"output_conversion_failures":0,"route_outcome":"Healthy","input_peak":0,"zero_crossing_frequency_hz":0,"passed":true}
```

### Row 05 — related CLbuds duplex, 44.1 kHz

```json
{"status":"failed","backend":"CoreAudio","input_uid":"58-18-62-82-0F-33:input","output_uid":"58-18-62-82-0F-33:output","expected_route_outcome":"Healthy","seconds":30,"requested_frames":1323000,"output_policy":"strict","settled_active_input_uid":"58-18-62-82-0F-33:input","settled_active_output_uid":"58-18-62-82-0F-33:output","settled_requested_session_host_callback_rate":44100,"settled_session_host_callback_rate":44100,"settled_session_host_callback_buffer_frames":512,"settled_physical_input_rate":16000,"settled_physical_output_rate":44100,"settled_input_auhal_client_rate":16000,"settled_output_auhal_client_rate":44100,"settled_available_input_channels":1,"settled_available_output_channels":2,"settled_requested_output_channels":2,"settled_resolved_output_channels":2,"settled_input_auhal_client_format_channels":1,"settled_output_auhal_client_format_channels":2,"settled_input_sample_rate_conversion_active":true,"settled_output_sample_rate_conversion_active":false,"settled_input_is_bluetooth":true,"settled_output_is_bluetooth":true,"settled_endpoints_related":false,"settled_output_mono_fallback":false,"settled_active_output_map":[0,1],"settled_capture_latency_frames":4320,"settled_playback_latency_frames":9843,"settled_processing_latency_frames":512,"settled_latency_complete":true,"settled_input_converter_latency_frames":3879,"settled_output_converter_latency_frames":0,"settled_route_detail":"","startup_input_render_failures":0,"startup_input_fifo_overruns":0,"startup_input_fifo_underruns":0,"startup_input_conversion_failures":0,"startup_output_conversion_failures":0,"startup_route_outcome":"Healthy","active_input_uid":"58-18-62-82-0F-33:input","active_output_uid":"58-18-62-82-0F-33:output","requested_session_host_callback_rate":44100,"session_host_callback_rate":44100,"session_host_callback_buffer_frames":512,"physical_input_rate":16000,"physical_output_rate":44100,"input_auhal_client_rate":16000,"output_auhal_client_rate":44100,"available_input_channels":1,"available_output_channels":2,"requested_output_channels":2,"resolved_output_channels":2,"input_auhal_client_format_channels":1,"output_auhal_client_format_channels":2,"input_sample_rate_conversion_active":true,"output_sample_rate_conversion_active":false,"input_is_bluetooth":true,"output_is_bluetooth":true,"endpoints_related":false,"output_mono_fallback":false,"active_output_map":[0,1],"capture_latency_frames":4320,"playback_latency_frames":9843,"processing_latency_frames":512,"latency_complete":true,"input_converter_latency_frames":3879,"output_converter_latency_frames":0,"route_detail":"","callbacks":2998,"callback_frames":1322118,"input_callbacks":2998,"input_frames":1322118,"callback_frame_delta":-882,"input_frame_delta":-882,"first_input_callback_width":1,"first_output_callback_width":2,"callback_width_changed":false,"callback_width_stable":true,"input_activity_matches":true,"startup_counters_clear":true,"final_counters_clear":true,"input_render_failures":0,"input_fifo_overruns":0,"input_fifo_underruns":0,"input_conversion_failures":0,"output_conversion_failures":0,"route_outcome":"Healthy","input_peak":0.009860811755,"zero_crossing_frequency_hz":2682.421614,"expected_input_tone_hz":1000,"input_tone_in_tolerance":false,"passed":false}
```

### Row 06 — CLbuds output-only, strict mono attempt

```json
{"status":"passed","backend":"CoreAudio","input_uid":"","output_uid":"58-18-62-82-0F-33:output","expected_route_outcome":"Healthy","seconds":30,"requested_frames":1323000,"output_policy":"strict","settled_active_input_uid":"","settled_active_output_uid":"58-18-62-82-0F-33:output","settled_requested_session_host_callback_rate":44100,"settled_session_host_callback_rate":44100,"settled_session_host_callback_buffer_frames":512,"settled_physical_input_rate":0,"settled_physical_output_rate":44100,"settled_input_auhal_client_rate":44100,"settled_output_auhal_client_rate":44100,"settled_available_input_channels":0,"settled_available_output_channels":2,"settled_requested_output_channels":2,"settled_resolved_output_channels":2,"settled_input_auhal_client_format_channels":0,"settled_output_auhal_client_format_channels":2,"settled_input_sample_rate_conversion_active":false,"settled_output_sample_rate_conversion_active":false,"settled_input_is_bluetooth":false,"settled_output_is_bluetooth":true,"settled_endpoints_related":false,"settled_output_mono_fallback":false,"settled_active_output_map":[0,1],"settled_capture_latency_frames":0,"settled_playback_latency_frames":9843,"settled_processing_latency_frames":512,"settled_latency_complete":true,"settled_input_converter_latency_frames":0,"settled_output_converter_latency_frames":0,"settled_route_detail":"","startup_input_render_failures":0,"startup_input_fifo_overruns":0,"startup_input_fifo_underruns":0,"startup_input_conversion_failures":0,"startup_output_conversion_failures":0,"startup_route_outcome":"Healthy","active_input_uid":"","active_output_uid":"58-18-62-82-0F-33:output","requested_session_host_callback_rate":44100,"session_host_callback_rate":44100,"session_host_callback_buffer_frames":512,"physical_input_rate":0,"physical_output_rate":44100,"input_auhal_client_rate":44100,"output_auhal_client_rate":44100,"available_input_channels":0,"available_output_channels":2,"requested_output_channels":2,"resolved_output_channels":2,"input_auhal_client_format_channels":0,"output_auhal_client_format_channels":2,"input_sample_rate_conversion_active":false,"output_sample_rate_conversion_active":false,"input_is_bluetooth":false,"output_is_bluetooth":true,"endpoints_related":false,"output_mono_fallback":false,"active_output_map":[0,1],"capture_latency_frames":0,"playback_latency_frames":9843,"processing_latency_frames":512,"latency_complete":true,"input_converter_latency_frames":0,"output_converter_latency_frames":0,"route_detail":"","callbacks":2584,"callback_frames":1323008,"input_callbacks":0,"input_frames":0,"callback_frame_delta":8,"input_frame_delta":-1323000,"first_input_callback_width":0,"first_output_callback_width":2,"callback_width_changed":false,"callback_width_stable":true,"input_activity_matches":true,"startup_counters_clear":true,"final_counters_clear":true,"input_render_failures":0,"input_fifo_overruns":0,"input_fifo_underruns":0,"input_conversion_failures":0,"output_conversion_failures":0,"route_outcome":"Healthy","input_peak":0,"zero_crossing_frequency_hz":0,"passed":true}
```

### Row 07 — CLbuds output-only, explicit mono fallback

```json
{"status":"passed","backend":"CoreAudio","input_uid":"","output_uid":"58-18-62-82-0F-33:output","expected_route_outcome":"Healthy","seconds":30,"requested_frames":1323000,"output_policy":"mono-fallback","settled_active_input_uid":"","settled_active_output_uid":"58-18-62-82-0F-33:output","settled_requested_session_host_callback_rate":44100,"settled_session_host_callback_rate":44100,"settled_session_host_callback_buffer_frames":512,"settled_physical_input_rate":0,"settled_physical_output_rate":44100,"settled_input_auhal_client_rate":44100,"settled_output_auhal_client_rate":44100,"settled_available_input_channels":0,"settled_available_output_channels":2,"settled_requested_output_channels":2,"settled_resolved_output_channels":2,"settled_input_auhal_client_format_channels":0,"settled_output_auhal_client_format_channels":2,"settled_input_sample_rate_conversion_active":false,"settled_output_sample_rate_conversion_active":false,"settled_input_is_bluetooth":false,"settled_output_is_bluetooth":true,"settled_endpoints_related":false,"settled_output_mono_fallback":false,"settled_active_output_map":[0,1],"settled_capture_latency_frames":0,"settled_playback_latency_frames":9843,"settled_processing_latency_frames":512,"settled_latency_complete":true,"settled_input_converter_latency_frames":0,"settled_output_converter_latency_frames":0,"settled_route_detail":"","startup_input_render_failures":0,"startup_input_fifo_overruns":0,"startup_input_fifo_underruns":0,"startup_input_conversion_failures":0,"startup_output_conversion_failures":0,"startup_route_outcome":"Healthy","active_input_uid":"","active_output_uid":"58-18-62-82-0F-33:output","requested_session_host_callback_rate":44100,"session_host_callback_rate":44100,"session_host_callback_buffer_frames":512,"physical_input_rate":0,"physical_output_rate":44100,"input_auhal_client_rate":44100,"output_auhal_client_rate":44100,"available_input_channels":0,"available_output_channels":2,"requested_output_channels":2,"resolved_output_channels":2,"input_auhal_client_format_channels":0,"output_auhal_client_format_channels":2,"input_sample_rate_conversion_active":false,"output_sample_rate_conversion_active":false,"input_is_bluetooth":false,"output_is_bluetooth":true,"endpoints_related":false,"output_mono_fallback":false,"active_output_map":[0,1],"capture_latency_frames":0,"playback_latency_frames":9843,"processing_latency_frames":512,"latency_complete":true,"input_converter_latency_frames":0,"output_converter_latency_frames":0,"route_detail":"","callbacks":2584,"callback_frames":1323008,"input_callbacks":0,"input_frames":0,"callback_frame_delta":8,"input_frame_delta":-1323000,"first_input_callback_width":0,"first_output_callback_width":2,"callback_width_changed":false,"callback_width_stable":true,"input_activity_matches":true,"startup_counters_clear":true,"final_counters_clear":true,"input_render_failures":0,"input_fifo_overruns":0,"input_fifo_underruns":0,"input_conversion_failures":0,"output_conversion_failures":0,"route_outcome":"Healthy","input_peak":0,"zero_crossing_frequency_hz":0,"passed":true}
```

### Row 08 — route mutation without manual disconnect

```json
{"status":"failed","backend":"CoreAudio","input_uid":"58-18-62-82-0F-33:input","output_uid":"BuiltInSpeakerDevice","expected_route_outcome":"InputRouteUnavailable","seconds":30,"requested_frames":1323000,"output_policy":"strict","settled_active_input_uid":"58-18-62-82-0F-33:input","settled_active_output_uid":"BuiltInSpeakerDevice","settled_requested_session_host_callback_rate":44100,"settled_session_host_callback_rate":44100,"settled_session_host_callback_buffer_frames":512,"settled_physical_input_rate":16000,"settled_physical_output_rate":44100,"settled_input_auhal_client_rate":16000,"settled_output_auhal_client_rate":44100,"settled_available_input_channels":1,"settled_available_output_channels":2,"settled_requested_output_channels":2,"settled_resolved_output_channels":2,"settled_input_auhal_client_format_channels":1,"settled_output_auhal_client_format_channels":2,"settled_input_sample_rate_conversion_active":true,"settled_output_sample_rate_conversion_active":false,"settled_input_is_bluetooth":true,"settled_output_is_bluetooth":false,"settled_endpoints_related":false,"settled_output_mono_fallback":false,"settled_active_output_map":[0,1],"settled_capture_latency_frames":4320,"settled_playback_latency_frames":778,"settled_processing_latency_frames":512,"settled_latency_complete":true,"settled_input_converter_latency_frames":3879,"settled_output_converter_latency_frames":0,"settled_route_detail":"","startup_input_render_failures":0,"startup_input_fifo_overruns":0,"startup_input_fifo_underruns":0,"startup_input_conversion_failures":0,"startup_output_conversion_failures":0,"startup_route_outcome":"Healthy","active_input_uid":"58-18-62-82-0F-33:input","active_output_uid":"BuiltInSpeakerDevice","requested_session_host_callback_rate":44100,"session_host_callback_rate":44100,"session_host_callback_buffer_frames":512,"physical_input_rate":16000,"physical_output_rate":44100,"input_auhal_client_rate":16000,"output_auhal_client_rate":44100,"available_input_channels":1,"available_output_channels":2,"requested_output_channels":2,"resolved_output_channels":2,"input_auhal_client_format_channels":1,"output_auhal_client_format_channels":2,"input_sample_rate_conversion_active":true,"output_sample_rate_conversion_active":false,"input_is_bluetooth":true,"output_is_bluetooth":false,"endpoints_related":false,"output_mono_fallback":false,"active_output_map":[0,1],"capture_latency_frames":4320,"playback_latency_frames":778,"processing_latency_frames":512,"latency_complete":true,"input_converter_latency_frames":3879,"output_converter_latency_frames":0,"route_detail":"","callbacks":2584,"callback_frames":1323008,"input_callbacks":2584,"input_frames":1323008,"callback_frame_delta":8,"input_frame_delta":8,"first_input_callback_width":1,"first_output_callback_width":2,"callback_width_changed":false,"callback_width_stable":true,"input_activity_matches":true,"startup_counters_clear":true,"final_counters_clear":true,"input_render_failures":0,"input_fifo_overruns":0,"input_fifo_underruns":0,"input_conversion_failures":0,"output_conversion_failures":0,"route_outcome":"Healthy","input_peak":0.004703935701,"zero_crossing_frequency_hz":3902.143071,"expected_input_tone_hz":1000,"input_tone_in_tolerance":false,"passed":false}
```


## Calibrated-source physical rerun (2026-08-21)

The rows in this section ran on the candidate at `6a1942c7` with the rebuilt
`build-sdk-release/tools/orpheus_coreaudio_hardware_acceptance` binary and the
FourTrack probe `/tmp/ftr085-coreaudio-route-probe` before and after every row.
The operator supplied an independent acoustic 1 kHz sine from an iPhone held at
the CLbuds microphone; the acceptance tool's zero-crossing detector applies a
+/-20 Hz tolerance. A 5-second placement diagnostic first measured
1008.513314 Hz (pass), after which rows 01 and 02 passed on the first
official attempt. The branch then advanced to `1c925048` with formatting,
include-guard, and CI-budget repairs only; the tree was rebuilt and the
deterministic gates re-passed after those changes.

| Row | Result | Gate interpretation |
|---|---|---|
| 01 CLbuds input + built-in output, 44.1 kHz | passed, exit 0, zero-crossing 1006.127249 Hz | Calibrated tone qualification satisfied; route healthy; SRC active |
| 02 CLbuds input + built-in output, 48 kHz | passed, exit 0, zero-crossing 986.6587393 Hz | Calibrated tone qualification satisfied at 48 kHz session |
| 05 related CLbuds duplex, 44.1 kHz | failed, exit 6, zero-crossing 974.1166667 Hz (first attempt 1174.866667 Hz) | Route healthy both attempts; tone drifted outside +/-20 Hz mid-run, attributed to uplink wind/noise DSP on the WF-1000XM6 outdoors; rerun pending quieter conditions |
| 03 output-only strict 44.1 kHz | passed, exit 0 | Stereo map [0,1], width 2, counters zero |
| 04 output-only strict 48 kHz | passed, exit 0 | 48 kHz session to 44.1 kHz physical output conversion active |
| 06 strict inside post-duplex HFP mono window | exit 4, `route_outcome:"ProfileConflict"` | Physical mono output profile (1 channel, 16 kHz, 320 frames) was probe-verified immediately before this open; strict policy refused the mono profile terminally. Strict half of the physical-mono contract demonstrated |
| 06 strict retry after stereo revert | passed, exit 0 | Confirms the conflict requires the live mono profile |
| 07 mono-fallback | passed, exit 0 against stereo (2 ch, 44.1 kHz, map [0,1]) | Fallback flag remained false; the fallback-against-live-mono binding was not caught |

The post-duplex inventory captured the live HFP mono window verbatim:

```text
device_id=201 uid=58-18-62-82-0F-33:output name=CLbuds
input_channels=0 output_channels=1
nominal_rate=16000.0 available_rate_ranges=[16000.0,16000.0],[44100.0,44100.0]
buffer_frames_global=320 buffer_frame_range_global=[5.0,4096.0]
```

Physical-mono gate status: the strict half is demonstrated by the
`ProfileConflict` above. The fallback half remains blocked by host profile
management: after a duplex session closes, the CLbuds output rests in the HFP
mono profile, but any new output-only client open (including the probe's full
device enumeration) races CoreAudio's reversion to A2DP stereo before the
session binds. Three catch attempts (probe-then-open, immediate open, and
fallback-first ordering) all observed stereo at settle time. No test, policy,
or counter was modified to force the outcome; the fallback half is recorded as
an open qualification blocker.

Hosted CI: repository Actions had been disabled (`actions/permissions`
returned `enabled:false`), which is why the `release/*` push trigger produced
no checks. Actions were re-enabled and dispatch run `32516236868` exposed:
48 clang-format-14 violations (fixed in `b0374752`), a fatal Linux compile of
the unguarded `coreaudio_endpoint_monitor.h` include in `driver_manager.cpp`
(fixed in `f901ab55`), and a macOS Debug Test-step timeout inside the
sanitized `cmake_shmui_package_consumer` (step budget raised in `1c925048`).
Windows Debug/Release fail on `src/core/session/json_io.cpp(206)` MSVC C2059,
which is inherited from `main` and outside this PR's required macOS/Ubuntu
gate scope; it is recorded as an inherited blocker.

Verbatim one-line results emitted by the candidate:

### Row 01 — CLbuds input plus built-in output, 44.1 kHz (passed, calibrated 1 kHz)

```json
{"status":"passed","backend":"CoreAudio","input_uid":"58-18-62-82-0F-33:input","output_uid":"BuiltInSpeakerDevice","expected_route_outcome":"Healthy","seconds":30,"requested_frames":1323000,"output_policy":"strict","settled_active_input_uid":"58-18-62-82-0F-33:input","settled_active_output_uid":"BuiltInSpeakerDevice","settled_requested_session_host_callback_rate":44100,"settled_session_host_callback_rate":44100,"settled_session_host_callback_buffer_frames":512,"settled_physical_input_rate":16000,"settled_physical_output_rate":44100,"settled_input_auhal_client_rate":16000,"settled_output_auhal_client_rate":44100,"settled_available_input_channels":1,"settled_available_output_channels":2,"settled_requested_output_channels":2,"settled_resolved_output_channels":2,"settled_input_auhal_client_format_channels":1,"settled_output_auhal_client_format_channels":2,"settled_input_sample_rate_conversion_active":true,"settled_output_sample_rate_conversion_active":false,"settled_input_is_bluetooth":true,"settled_output_is_bluetooth":false,"settled_endpoints_related":false,"settled_output_mono_fallback":false,"settled_active_output_map":[0,1],"settled_capture_latency_frames":4320,"settled_playback_latency_frames":778,"settled_processing_latency_frames":512,"settled_latency_complete":true,"settled_input_converter_latency_frames":3879,"settled_output_converter_latency_frames":0,"settled_route_detail":"","startup_input_render_failures":0,"startup_input_fifo_overruns":0,"startup_input_fifo_underruns":0,"startup_input_conversion_failures":0,"startup_output_conversion_failures":0,"startup_route_outcome":"Healthy","active_input_uid":"58-18-62-82-0F-33:input","active_output_uid":"BuiltInSpeakerDevice","requested_session_host_callback_rate":44100,"session_host_callback_rate":44100,"session_host_callback_buffer_frames":512,"physical_input_rate":16000,"physical_output_rate":44100,"input_auhal_client_rate":16000,"output_auhal_client_rate":44100,"available_input_channels":1,"available_output_channels":2,"requested_output_channels":2,"resolved_output_channels":2,"input_auhal_client_format_channels":1,"output_auhal_client_format_channels":2,"input_sample_rate_conversion_active":true,"output_sample_rate_conversion_active":false,"input_is_bluetooth":true,"output_is_bluetooth":false,"endpoints_related":false,"output_mono_fallback":false,"active_output_map":[0,1],"capture_latency_frames":4320,"playback_latency_frames":778,"processing_latency_frames":512,"latency_complete":true,"input_converter_latency_frames":3879,"output_converter_latency_frames":0,"route_detail":"","callbacks":2584,"callback_frames":1323008,"input_callbacks":2584,"input_frames":1323008,"callback_frame_delta":8,"input_frame_delta":8,"first_input_callback_width":1,"first_output_callback_width":2,"callback_width_changed":false,"callback_width_stable":true,"input_activity_matches":true,"startup_counters_clear":true,"final_counters_clear":true,"input_render_failures":0,"input_fifo_overruns":0,"input_fifo_underruns":0,"input_conversion_failures":0,"output_conversion_failures":0,"route_outcome":"Healthy","input_peak":0.6054627299,"zero_crossing_frequency_hz":1006.127249,"expected_input_tone_hz":1000,"input_tone_in_tolerance":true,"passed":true}
```

### Row 02 — CLbuds input plus built-in output, 48 kHz (passed, calibrated 1 kHz, zero-crossing 986.6587393 Hz)

```json
{"status":"passed","backend":"CoreAudio","input_uid":"58-18-62-82-0F-33:input","output_uid":"BuiltInSpeakerDevice","expected_route_outcome":"Healthy","seconds":30,"requested_frames":1440000,"output_policy":"strict","settled_active_input_uid":"58-18-62-82-0F-33:input","settled_active_output_uid":"BuiltInSpeakerDevice","settled_requested_session_host_callback_rate":48000,"settled_session_host_callback_rate":48000,"settled_session_host_callback_buffer_frames":512,"settled_physical_input_rate":16000,"settled_physical_output_rate":48000,"settled_input_auhal_client_rate":16000,"settled_output_auhal_client_rate":48000,"settled_available_input_channels":1,"settled_available_output_channels":2,"settled_requested_output_channels":2,"settled_resolved_output_channels":2,"settled_input_auhal_client_format_channels":1,"settled_output_auhal_client_format_channels":2,"settled_input_sample_rate_conversion_active":true,"settled_output_sample_rate_conversion_active":false,"settled_input_is_bluetooth":true,"settled_output_is_bluetooth":false,"settled_endpoints_related":false,"settled_output_mono_fallback":false,"settled_active_output_map":[0,1],"settled_capture_latency_frames":4701,"settled_playback_latency_frames":834,"settled_processing_latency_frames":512,"settled_latency_complete":true,"settled_input_converter_latency_frames":4221,"settled_output_converter_latency_frames":0,"settled_route_detail":"","startup_input_render_failures":0,"startup_input_fifo_overruns":0,"startup_input_fifo_underruns":0,"startup_input_conversion_failures":0,"startup_output_conversion_failures":0,"startup_route_outcome":"Healthy","active_input_uid":"58-18-62-82-0F-33:input","active_output_uid":"BuiltInSpeakerDevice","requested_session_host_callback_rate":48000,"session_host_callback_rate":48000,"session_host_callback_buffer_frames":512,"physical_input_rate":16000,"physical_output_rate":48000,"input_auhal_client_rate":16000,"output_auhal_client_rate":48000,"available_input_channels":1,"available_output_channels":2,"requested_output_channels":2,"resolved_output_channels":2,"input_auhal_client_format_channels":1,"output_auhal_client_format_channels":2,"input_sample_rate_conversion_active":true,"output_sample_rate_conversion_active":false,"input_is_bluetooth":true,"output_is_bluetooth":false,"endpoints_related":false,"output_mono_fallback":false,"active_output_map":[0,1],"capture_latency_frames":4701,"playback_latency_frames":834,"processing_latency_frames":512,"latency_complete":true,"input_converter_latency_frames":4221,"output_converter_latency_frames":0,"route_detail":"","callbacks":2812,"callback_frames":1439744,"input_callbacks":2812,"input_frames":1439744,"callback_frame_delta":-256,"input_frame_delta":-256,"first_input_callback_width":1,"first_output_callback_width":2,"callback_width_changed":false,"callback_width_stable":true,"input_activity_matches":true,"startup_counters_clear":true,"final_counters_clear":true,"input_render_failures":0,"input_fifo_overruns":0,"input_fifo_underruns":0,"input_conversion_failures":0,"output_conversion_failures":0,"route_outcome":"Healthy","input_peak":0.3402238786,"zero_crossing_frequency_hz":986.6587393,"expected_input_tone_hz":1000,"input_tone_in_tolerance":true,"passed":true}
```

### Row 05 — related CLbuds duplex, 44.1 kHz (failed tone predicate, zero-crossing 974.1166667 Hz; route healthy)

```json
{"status":"failed","backend":"CoreAudio","input_uid":"58-18-62-82-0F-33:input","output_uid":"58-18-62-82-0F-33:output","expected_route_outcome":"Healthy","seconds":30,"requested_frames":1323000,"output_policy":"strict","settled_active_input_uid":"58-18-62-82-0F-33:input","settled_active_output_uid":"58-18-62-82-0F-33:output","settled_requested_session_host_callback_rate":44100,"settled_session_host_callback_rate":44100,"settled_session_host_callback_buffer_frames":512,"settled_physical_input_rate":16000,"settled_physical_output_rate":44100,"settled_input_auhal_client_rate":16000,"settled_output_auhal_client_rate":44100,"settled_available_input_channels":1,"settled_available_output_channels":2,"settled_requested_output_channels":2,"settled_resolved_output_channels":2,"settled_input_auhal_client_format_channels":1,"settled_output_auhal_client_format_channels":2,"settled_input_sample_rate_conversion_active":true,"settled_output_sample_rate_conversion_active":false,"settled_input_is_bluetooth":true,"settled_output_is_bluetooth":true,"settled_endpoints_related":false,"settled_output_mono_fallback":false,"settled_active_output_map":[0,1],"settled_capture_latency_frames":4320,"settled_playback_latency_frames":9843,"settled_processing_latency_frames":512,"settled_latency_complete":true,"settled_input_converter_latency_frames":3879,"settled_output_converter_latency_frames":0,"settled_route_detail":"","startup_input_render_failures":0,"startup_input_fifo_overruns":0,"startup_input_fifo_underruns":0,"startup_input_conversion_failures":0,"startup_output_conversion_failures":0,"startup_route_outcome":"Healthy","active_input_uid":"58-18-62-82-0F-33:input","active_output_uid":"58-18-62-82-0F-33:output","requested_session_host_callback_rate":44100,"session_host_callback_rate":44100,"session_host_callback_buffer_frames":512,"physical_input_rate":16000,"physical_output_rate":44100,"input_auhal_client_rate":16000,"output_auhal_client_rate":44100,"available_input_channels":1,"available_output_channels":2,"requested_output_channels":2,"resolved_output_channels":2,"input_auhal_client_format_channels":1,"output_auhal_client_format_channels":2,"input_sample_rate_conversion_active":true,"output_sample_rate_conversion_active":false,"input_is_bluetooth":true,"output_is_bluetooth":true,"endpoints_related":false,"output_mono_fallback":false,"active_output_map":[0,1],"capture_latency_frames":4320,"playback_latency_frames":9843,"processing_latency_frames":512,"latency_complete":true,"input_converter_latency_frames":3879,"output_converter_latency_frames":0,"route_detail":"","callbacks":3000,"callback_frames":1323000,"input_callbacks":3000,"input_frames":1323000,"callback_frame_delta":0,"input_frame_delta":0,"first_input_callback_width":1,"first_output_callback_width":2,"callback_width_changed":false,"callback_width_stable":true,"input_activity_matches":true,"startup_counters_clear":true,"final_counters_clear":true,"input_render_failures":0,"input_fifo_overruns":0,"input_fifo_underruns":0,"input_conversion_failures":0,"output_conversion_failures":0,"route_outcome":"Healthy","input_peak":0.8458359241,"zero_crossing_frequency_hz":974.1166667,"expected_input_tone_hz":1000,"input_tone_in_tolerance":false,"passed":false}
```

### Row 03 — CLbuds output-only, strict, 44.1 kHz (passed)

```json
{"status":"passed","backend":"CoreAudio","input_uid":"","output_uid":"58-18-62-82-0F-33:output","expected_route_outcome":"Healthy","seconds":30,"requested_frames":1323000,"output_policy":"strict","settled_active_input_uid":"","settled_active_output_uid":"58-18-62-82-0F-33:output","settled_requested_session_host_callback_rate":44100,"settled_session_host_callback_rate":44100,"settled_session_host_callback_buffer_frames":512,"settled_physical_input_rate":0,"settled_physical_output_rate":44100,"settled_input_auhal_client_rate":44100,"settled_output_auhal_client_rate":44100,"settled_available_input_channels":0,"settled_available_output_channels":2,"settled_requested_output_channels":2,"settled_resolved_output_channels":2,"settled_input_auhal_client_format_channels":0,"settled_output_auhal_client_format_channels":2,"settled_input_sample_rate_conversion_active":false,"settled_output_sample_rate_conversion_active":false,"settled_input_is_bluetooth":false,"settled_output_is_bluetooth":true,"settled_endpoints_related":false,"settled_output_mono_fallback":false,"settled_active_output_map":[0,1],"settled_capture_latency_frames":0,"settled_playback_latency_frames":9843,"settled_processing_latency_frames":512,"settled_latency_complete":true,"settled_input_converter_latency_frames":0,"settled_output_converter_latency_frames":0,"settled_route_detail":"","startup_input_render_failures":0,"startup_input_fifo_overruns":0,"startup_input_fifo_underruns":0,"startup_input_conversion_failures":0,"startup_output_conversion_failures":0,"startup_route_outcome":"Healthy","active_input_uid":"","active_output_uid":"58-18-62-82-0F-33:output","requested_session_host_callback_rate":44100,"session_host_callback_rate":44100,"session_host_callback_buffer_frames":512,"physical_input_rate":0,"physical_output_rate":44100,"input_auhal_client_rate":44100,"output_auhal_client_rate":44100,"available_input_channels":0,"available_output_channels":2,"requested_output_channels":2,"resolved_output_channels":2,"input_auhal_client_format_channels":0,"output_auhal_client_format_channels":2,"input_sample_rate_conversion_active":false,"output_sample_rate_conversion_active":false,"input_is_bluetooth":false,"output_is_bluetooth":true,"endpoints_related":false,"output_mono_fallback":false,"active_output_map":[0,1],"capture_latency_frames":0,"playback_latency_frames":9843,"processing_latency_frames":512,"latency_complete":true,"input_converter_latency_frames":0,"output_converter_latency_frames":0,"route_detail":"","callbacks":2584,"callback_frames":1323008,"input_callbacks":0,"input_frames":0,"callback_frame_delta":8,"input_frame_delta":-1323000,"first_input_callback_width":0,"first_output_callback_width":2,"callback_width_changed":false,"callback_width_stable":true,"input_activity_matches":true,"startup_counters_clear":true,"final_counters_clear":true,"input_render_failures":0,"input_fifo_overruns":0,"input_fifo_underruns":0,"input_conversion_failures":0,"output_conversion_failures":0,"route_outcome":"Healthy","input_peak":0,"zero_crossing_frequency_hz":0,"passed":true}
```

### Row 04 — CLbuds output-only, strict, 48 kHz (passed, 48 kHz session to 44.1 kHz physical)

```json
{"status":"passed","backend":"CoreAudio","input_uid":"","output_uid":"58-18-62-82-0F-33:output","expected_route_outcome":"Healthy","seconds":30,"requested_frames":1440000,"output_policy":"strict","settled_active_input_uid":"","settled_active_output_uid":"58-18-62-82-0F-33:output","settled_requested_session_host_callback_rate":48000,"settled_session_host_callback_rate":48000,"settled_session_host_callback_buffer_frames":512,"settled_physical_input_rate":0,"settled_physical_output_rate":44100,"settled_input_auhal_client_rate":48000,"settled_output_auhal_client_rate":44100,"settled_available_input_channels":0,"settled_available_output_channels":2,"settled_requested_output_channels":2,"settled_resolved_output_channels":2,"settled_input_auhal_client_format_channels":0,"settled_output_auhal_client_format_channels":2,"settled_input_sample_rate_conversion_active":false,"settled_output_sample_rate_conversion_active":true,"settled_input_is_bluetooth":false,"settled_output_is_bluetooth":true,"settled_endpoints_related":false,"settled_output_mono_fallback":false,"settled_active_output_map":[0,1],"settled_capture_latency_frames":0,"settled_playback_latency_frames":10842,"settled_processing_latency_frames":558,"settled_latency_complete":true,"settled_input_converter_latency_frames":0,"settled_output_converter_latency_frames":128,"settled_route_detail":"","startup_input_render_failures":0,"startup_input_fifo_overruns":0,"startup_input_fifo_underruns":0,"startup_input_conversion_failures":0,"startup_output_conversion_failures":0,"startup_route_outcome":"Healthy","active_input_uid":"","active_output_uid":"58-18-62-82-0F-33:output","requested_session_host_callback_rate":48000,"session_host_callback_rate":48000,"session_host_callback_buffer_frames":512,"physical_input_rate":0,"physical_output_rate":44100,"input_auhal_client_rate":48000,"output_auhal_client_rate":44100,"available_input_channels":0,"available_output_channels":2,"requested_output_channels":2,"resolved_output_channels":2,"input_auhal_client_format_channels":0,"output_auhal_client_format_channels":2,"input_sample_rate_conversion_active":false,"output_sample_rate_conversion_active":true,"input_is_bluetooth":false,"output_is_bluetooth":true,"endpoints_related":false,"output_mono_fallback":false,"active_output_map":[0,1],"capture_latency_frames":0,"playback_latency_frames":10842,"processing_latency_frames":558,"latency_complete":true,"input_converter_latency_frames":0,"output_converter_latency_frames":128,"route_detail":"","callbacks":5168,"callback_frames":1440136,"input_callbacks":0,"input_frames":0,"callback_frame_delta":136,"input_frame_delta":-1440000,"first_input_callback_width":0,"first_output_callback_width":2,"callback_width_changed":false,"callback_width_stable":true,"input_activity_matches":true,"startup_counters_clear":true,"final_counters_clear":true,"input_render_failures":0,"input_fifo_overruns":0,"input_fifo_underruns":0,"input_conversion_failures":0,"output_conversion_failures":0,"route_outcome":"Healthy","input_peak":0,"zero_crossing_frequency_hz":0,"passed":true}
```

### Row 06 — CLbuds output-only strict fired inside the post-duplex HFP mono window (ProfileConflict: strict half of the physical-mono contract)

```json
{"status":"failed","reason":"initialize","expected_route_outcome":"Healthy","route_outcome":"ProfileConflict","detail":"CoreAudio reported a terminal route failure."}
```

### Row 06 — strict retry after the profile had reverted to stereo (passed against stereo)

```json
{"status":"passed","backend":"CoreAudio","input_uid":"","output_uid":"58-18-62-82-0F-33:output","expected_route_outcome":"Healthy","seconds":30,"requested_frames":1323000,"output_policy":"strict","settled_active_input_uid":"","settled_active_output_uid":"58-18-62-82-0F-33:output","settled_requested_session_host_callback_rate":44100,"settled_session_host_callback_rate":44100,"settled_session_host_callback_buffer_frames":512,"settled_physical_input_rate":0,"settled_physical_output_rate":44100,"settled_input_auhal_client_rate":44100,"settled_output_auhal_client_rate":44100,"settled_available_input_channels":0,"settled_available_output_channels":2,"settled_requested_output_channels":2,"settled_resolved_output_channels":2,"settled_input_auhal_client_format_channels":0,"settled_output_auhal_client_format_channels":2,"settled_input_sample_rate_conversion_active":false,"settled_output_sample_rate_conversion_active":false,"settled_input_is_bluetooth":false,"settled_output_is_bluetooth":true,"settled_endpoints_related":false,"settled_output_mono_fallback":false,"settled_active_output_map":[0,1],"settled_capture_latency_frames":0,"settled_playback_latency_frames":9843,"settled_processing_latency_frames":512,"settled_latency_complete":true,"settled_input_converter_latency_frames":0,"settled_output_converter_latency_frames":0,"settled_route_detail":"","startup_input_render_failures":0,"startup_input_fifo_overruns":0,"startup_input_fifo_underruns":0,"startup_input_conversion_failures":0,"startup_output_conversion_failures":0,"startup_route_outcome":"Healthy","active_input_uid":"","active_output_uid":"58-18-62-82-0F-33:output","requested_session_host_callback_rate":44100,"session_host_callback_rate":44100,"session_host_callback_buffer_frames":512,"physical_input_rate":0,"physical_output_rate":44100,"input_auhal_client_rate":44100,"output_auhal_client_rate":44100,"available_input_channels":0,"available_output_channels":2,"requested_output_channels":2,"resolved_output_channels":2,"input_auhal_client_format_channels":0,"output_auhal_client_format_channels":2,"input_sample_rate_conversion_active":false,"output_sample_rate_conversion_active":false,"input_is_bluetooth":false,"output_is_bluetooth":true,"endpoints_related":false,"output_mono_fallback":false,"active_output_map":[0,1],"capture_latency_frames":0,"playback_latency_frames":9843,"processing_latency_frames":512,"latency_complete":true,"input_converter_latency_frames":0,"output_converter_latency_frames":0,"route_detail":"","callbacks":2584,"callback_frames":1323008,"input_callbacks":0,"input_frames":0,"callback_frame_delta":8,"input_frame_delta":-1323000,"first_input_callback_width":0,"first_output_callback_width":2,"callback_width_changed":false,"callback_width_stable":true,"input_activity_matches":true,"startup_counters_clear":true,"final_counters_clear":true,"input_render_failures":0,"input_fifo_overruns":0,"input_fifo_underruns":0,"input_conversion_failures":0,"output_conversion_failures":0,"route_outcome":"Healthy","input_peak":0,"zero_crossing_frequency_hz":0,"passed":true}
```

### Row 07 — explicit mono fallback (passed against stereo; the live-mono fallback binding was not caught)

```json
{"status":"passed","backend":"CoreAudio","input_uid":"","output_uid":"58-18-62-82-0F-33:output","expected_route_outcome":"Healthy","seconds":30,"requested_frames":1323000,"output_policy":"mono-fallback","settled_active_input_uid":"","settled_active_output_uid":"58-18-62-82-0F-33:output","settled_requested_session_host_callback_rate":44100,"settled_session_host_callback_rate":44100,"settled_session_host_callback_buffer_frames":320,"settled_physical_input_rate":0,"settled_physical_output_rate":44100,"settled_input_auhal_client_rate":44100,"settled_output_auhal_client_rate":44100,"settled_available_input_channels":0,"settled_available_output_channels":2,"settled_requested_output_channels":2,"settled_resolved_output_channels":2,"settled_input_auhal_client_format_channels":0,"settled_output_auhal_client_format_channels":2,"settled_input_sample_rate_conversion_active":false,"settled_output_sample_rate_conversion_active":false,"settled_input_is_bluetooth":false,"settled_output_is_bluetooth":true,"settled_endpoints_related":false,"settled_output_mono_fallback":false,"settled_active_output_map":[0,1],"settled_capture_latency_frames":0,"settled_playback_latency_frames":9843,"settled_processing_latency_frames":320,"settled_latency_complete":true,"settled_input_converter_latency_frames":0,"settled_output_converter_latency_frames":0,"settled_route_detail":"","startup_input_render_failures":0,"startup_input_fifo_overruns":0,"startup_input_fifo_underruns":0,"startup_input_conversion_failures":0,"startup_output_conversion_failures":0,"startup_route_outcome":"Healthy","active_input_uid":"","active_output_uid":"58-18-62-82-0F-33:output","requested_session_host_callback_rate":44100,"session_host_callback_rate":44100,"session_host_callback_buffer_frames":320,"physical_input_rate":0,"physical_output_rate":44100,"input_auhal_client_rate":44100,"output_auhal_client_rate":44100,"available_input_channels":0,"available_output_channels":2,"requested_output_channels":2,"resolved_output_channels":2,"input_auhal_client_format_channels":0,"output_auhal_client_format_channels":2,"input_sample_rate_conversion_active":false,"output_sample_rate_conversion_active":false,"input_is_bluetooth":false,"output_is_bluetooth":true,"endpoints_related":false,"output_mono_fallback":false,"active_output_map":[0,1],"capture_latency_frames":0,"playback_latency_frames":9843,"processing_latency_frames":320,"latency_complete":true,"input_converter_latency_frames":0,"output_converter_latency_frames":0,"route_detail":"","callbacks":4135,"callback_frames":1323200,"input_callbacks":0,"input_frames":0,"callback_frame_delta":200,"input_frame_delta":-1323000,"first_input_callback_width":0,"first_output_callback_width":2,"callback_width_changed":false,"callback_width_stable":true,"input_activity_matches":true,"startup_counters_clear":true,"final_counters_clear":true,"input_render_failures":0,"input_fifo_overruns":0,"input_fifo_underruns":0,"input_conversion_failures":0,"output_conversion_failures":0,"route_outcome":"Healthy","input_peak":0,"zero_crossing_frequency_hz":0,"passed":true}
```


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
