# ORP255 Host-Neutral Multichannel Metering Contract

**Status:** Implementation committed at `24cce0b4861b3bdb72fbb310ac1784482fc86e94`; feature branch delivery pending
**Date:** 2026-09-01
**SDK:** 0.9.0; stable C ABI 1.0
**Scope:** Host-neutral routing meters, bounded schema-3 telemetry, and C++ migration boundary

## Decision

The SDK publishes two complementary routing-meter surfaces:

1. Existing `AudioMeter` getters retain their finite, non-silent signal points
   and legacy LUFS proxy value.
2. `GroupOutputMeterSnapshot` adds one independently measured fixed-capacity
   logical output-lane frame per routing group. `RoutingMeterTelemetry` adds
   canonical aggregate, logical-lane, master, and post-master routing-output
   domains to schema-3 `RealtimeTelemetrySnapshot`.

The payloads contain no segment geometry, colours, peak-hold timers, UI cadence,
FFT history, or application routing policy. Hosts own presentation and history
construction after `RealtimeTelemetry::tryRead()`.

## Signal-point audit

`RoutingMatrix::processRoutingBlock()` is the sole SDK routing-level producer.
Its stages are:

1. source/channel contribution after channel gain, pan, mute, and effective
   channel-solo logic;
2. legacy group aggregate over the group buffer, before group gain/mute/headroom;
3. logical group-output lanes after group gain smoothing, effective group mute,
   and headroom, before master summing;
4. legacy master aggregate after master gain/mute, before protection;
5. post-master routing-output lanes after protection and before driver
   conversion/device handoff.

A routing output is an SDK routing index, not a device-physical channel. The
later `AudioOutputRouteRequest::output_channel_map` performs device mapping.
When groups overlap a routing output, each logical group lane remains measured
independently while the shared post-master routing-output lane contains the
sum.

`RoutingMatrix::processStereoMetering()` preserves the legacy aggregate math,
including the `MeteringMode::LUFS` proxy. New logical lanes always publish a
sample peak in LUFS mode and label the frame `SamplePeak`. The SDK 4x/12-tap
`TruePeakMeter` is an estimator modeled against BS.1770 terminology, not a
standalone standards-conformance claim.

## Public logical-lane contract

`GroupOutputMeterSnapshot` has schema version 1. `groups[g]` is
`RoutingGroupIndex g`; `groups[g].lane_meters[l]` is logical lane `l` of that
group. Only `l < logical_lane_count` is configured, and its routing-matrix
output index is `routing_output_start + l`. `routing_output_start` is never a
device-physical index. `ChannelConfig::output_channel` and
`setChannelRoute()` use the same group-local logical-lane identity.

Under `SourceChannelPolicy::Discrete`, a source route whose lane is outside the
currently rendered group width contributes no signal. The frame reports the
currently rendered width; it does not remap or copy another lane.

Availability is normative:

- `Measured` plus `kAudioMeterSilenceDb` is measured silence;
- `Unconfigured` identifies a lane or array index outside the active topology;
- `Unmeasured` means metering is disabled, a render has not completed, or a
  render/topology publication was rejected;
- `Unsupported` is the terminal default from an implementation that does not
  implement the appended extension.

`raw_block_frames` is the most recent completed internal
`processRoutingBlock()` slice, not an accumulated telemetry window. Large
`processRouting()` calls retain final-slice behavior.

The extension is an appended default virtual method after `maxBlockFrames()`:
older source implementations recompile and return `Unsupported`; no old/new
C++ binary mix is supported.

## Capability matrix

| Domain | Cardinality and identity | Exact signal point | Peak definition | RMS window | Clip-count granularity/reset | Availability | Surface |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Source/channel | `num_channels`, `RoutingChannelIndex` | Isolated post-channel gain/pan/mute/solo, before group summing | `Peak`/`TruePeak` mode; legacy LUFS proxy in LUFS mode | Current internal slice | One increment per internal slice with a threshold crossing; cumulative until `initialize()` | Exact silence for null/unrouted/effectively muted current slice | Direct legacy getter |
| Aggregate group | `num_groups`, `RoutingGroupIndex` | Legacy two-lane group buffer before group controls | `SamplePeak`, `TruePeak4x`, or `LegacyLufsProxy` | Current internal slice across legacy lanes | One increment per internal slice with any lane crossing; cumulative until `initialize()` | Configured render or exact silence; getter has legacy compatibility semantics | Direct legacy getter and canonical aggregate |
| Logical group-output lane | Each active group × `logical_lane_count`; group/lane identity | Post-group gain smoothing, effective mute, and headroom; before master | `SamplePeak` except `TruePeak4x` in true-peak mode; LUFS is always `SamplePeak` | Most recent routing slice for direct frame; canonical final callback RMS | One increment per lane per internal slice with a crossing; cumulative until `initialize()`; topology changes reset filter history only | `Measured`, `Unmeasured`, or `Unsupported` nested extension; outside width is `Unconfigured` | Direct schema-1 extension and nested canonical telemetry |
| Master aggregate | One matrix-wide meter | After master gain/mute, before protection | Legacy mode definition; LUFS remains `LegacyLufsProxy` | Current internal slice | One increment per internal slice with any output crossing; cumulative until `initialize()` | Measured after successful enabled render; otherwise canonical domain `Unmeasured` | Direct legacy getter and canonical aggregate |
| Post-master routing-output lane | `num_outputs`, `RoutingOutputIndex` | After clipping protection, before driver conversion and device handoff | Existing `TruePeak4x` estimator | Current internal slice | One increment per routing output per internal slice with a crossing; cumulative until `initialize()` | Active configured outputs measured; indices beyond `num_outputs` unconfigured | Direct legacy getter and canonical output lane |
| True peak | State attached to each measured lane | FIR interpolation of the complete sanitized slice | 4x oversampling, 12 taps per phase | Not a separate loudness measure | Filter history clears on all-zero/effectively muted slices and topology changes; clip counters do not clear | Follows owning meter domain | SDK estimator, direct or telemetry |
| Loudness | Mono/stereo control/offline `LoudnessMeter` and `analysis::integratedLufs()` | K-weighted analysis outside routing callback | Integrated/short-term LUFS analysis | Facility-defined windows | Facility state reset by its own `reset()` | Facility-defined | Separate control/offline API; not routing LUFS |

## Coherence, topology, and finite-input rules

Channel group/lane identity is one packed atomic route word. Each accepted
channel-route transaction is serialized by an odd/even route-publication
sequence: the control writer makes it odd before changing any packed word,
advances the saturating route generation within that transaction, and makes it
even only after the complete route state is visible. The audio thread captures
that sequence before and after each slice and accepts a route topology only
when both values match, are even, and the route generation is unchanged.
Accepted route or group-geometry changes reset true-peak histories, advance the
rendered topology revision, and publish that revision. An in-progress or
mid-slice route transaction publishes `Unmeasured` with `coherent == 0`; it
never labels mixed route state coherent.


The group-output publication is protected by one matrix-wide sequence. The
sequence is odd while every group metadata value and lane meter is written and
even only after the complete matrix is published. `copyGroupOutputMeterSnapshot`
performs a bounded atomic copy and sets `coherent` only when its before/after
sequence values match and the publication is complete. `Unsupported` is
terminal for a direct caller; an implemented but concurrent `coherent == 0`
publication may be retried by transport.

Non-finite channel gain, pan, and master gain controls are rejected before
smoothers or live state change. Non-finite source samples and computed
contributions sanitize to zero. Public meter dB fields are always finite or
`kAudioMeterSilenceDb`. `reset()` does not clear cumulative clip counters;
`initialize()` does.

For true peak, the complete sanitized slice is examined first. A whole-slice
zero or effectively muted lane resets filter history and publishes exact
silence. A nonzero lane processes every sample, including trailing zeros,
through the estimator before publication. Topology/width/routing-output changes
reset filter histories but preserve clip counts.

## Canonical schema-3 telemetry

`RealtimeTelemetrySnapshot` retains every schema-2 field in its original order
and appends `routing_meters`. The nested `RoutingMeterTelemetry` payload uses
schema 1. Active standard group/output counts come from `RoutingConfig`, not
from the optional nested extension: `group_aggregate_availability` is measured
through `num_groups` and unconfigured beyond it; output availability is measured
through `num_outputs` and unconfigured beyond it. `group_output_meters.group_count`
is only the optional logical extension count.

`TransportController` is the sole SDK routing-telemetry producer. It consumes the
matrix snapshot immediately after its completed routing render. A default or
legacy matrix implementation that returns `Unsupported` leaves canonical
aggregate, master, and post-master output domains measured. A concurrent
implemented extension that remains incoherent marks only the nested logical
output domain unmeasured. A routing failure or disabled metering marks the whole
canonical frame unmeasured and zeros both windows; legacy fields are still
filled at their existing due-callback boundary and may be stale after failure.

The audio-thread accumulator starts on the first successful standard measured
render. It resets on a nested topology-revision change, invalidation, or
successful enqueue. Peak is the saturating sum of successful transport-callback
frame counts since the prior successful enqueue and uses the maximum over that
window. RMS is the final successful callback's `numFrames` and uses that final
callback's RMS. A zero-frame callback extends neither window. A full ring drops
new frames without overwriting unread frames; the accumulator remains pending
until enqueue succeeds. Sequence gaps and `droppedSnapshotCount()` expose loss.

## Exact SDK source anchors

- `include/orpheus/routing_matrix.h`: `AudioMeter`, `MeteringMode`,
  `GroupOutputMeterFrame`, `GroupOutputMeterSnapshot`, `IRoutingMatrix`, and
  `IRoutingCallback::onClippingDetected`.
- `src/core/routing/routing_matrix.cpp:943`:
  `RoutingMatrix::processRoutingBlock`.
- `src/core/routing/routing_matrix.cpp:1450`:
  `RoutingMatrix::processStereoMetering`.
- `src/core/routing/routing_matrix.cpp:701`:
  `RoutingMatrix::copyGroupOutputMeterSnapshot`.
- `src/core/routing/routing_matrix.cpp:632`, `:651`, `:670`, and `:686`:
  `getChannelMeter`, `getGroupMeter`, `getOutputMeter`, and `getMasterMeter`.
- `src/core/transport/transport_controller.cpp:595`:
  `TransportController::processAudio`; `:1071` onward contains the canonical
  accumulator helpers and `collectRoutingMeters`.
- `src/core/common/realtime_telemetry.cpp:14`, `:31`, and `:54`:
  `RealtimeTelemetry::beginRealtimeBlock`, `publishFromRealtime`, and `tryRead`.
- `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:719`:
  `CoreAudioDriver::renderCallback`; `src/platform/audio_drivers/wasapi/
  wasapi_driver.cpp:553` `WASAPIAudioDriver::audioLoop`; and
  `src/core/audio_io/dummy_audio_driver.cpp:255`
  `DummyAudioDriver::audioThreadMain`. These boundaries hand off/convert
  planar buffers and do not produce routing audio-level meters.
- `tests/cmake/find_package/transport_routing.cpp`: installed clean-prefix
  consumer static layout and real 0.9.0 routing render/copy/read fixture.

## Consumer audit

- `TransportController` is the sole SDK routing-telemetry producer.
- Clip Composer `Source/Audio/AudioEngine.cpp` currently consumes only legacy
  aggregate `group_meters`.
- FourTrack `src/fourtrack/engine/engine.cpp` currently consumes only
  `getChannelMeter`.
- In-repository/public-package consumers are
  `tests/common/realtime_telemetry_test.cpp`,
  `tests/transport/transport_controller_test.cpp`,
  `tests/cmake/find_package/workflow_contracts.cpp`, and
  `tests/cmake/find_package/transport_routing.cpp`.
- ShmUI's similarly named `MeterGroup` is a UI component and is not an
  `AudioMeter` consumer.

No Clip Composer, FourTrack, ShmUI, downstream gitlink, or application source is
changed by this SDK branch.

## Adjacent provenance

The neighboring ShmUI fine-segment source is commit
`a714344d93e8e84ddb1c194a500f26655cf73149`. The adjacent SDK import/package
handoff is commit `a5474085`. The ORP254 handoff reports Fine committed at 12
grits and a downstream reported Debug gate of 754/754; that is downstream
user-provided evidence, not an SDK test rerun here. The complete imported
manifest in this branch, `packages/shmui-juce/shmui-juce-import.json`, records
source revision
`cd83c31b14535e0f6f5ae38327b5245d3a80ecdc` and content SHA-256
`3741685e84433a3dfa4efebc1d930ef02f0b678f0b2cd162e6cb140022b273d9`.

The planning handoff referenced a separately published revision beginning
`1a3148f`; that abbreviated value is not substituted for the complete revision
recorded by the checked-out manifest above. Clip Composer adjacent provenance
is implementation commit `bf41f9cb` and verification/documentation commit
`da1a1ea0`. Neither downstream result is recast as SDK verification.

## Migration

- Rebuild every C++ host, custom `IRoutingMatrix` implementation, and
  `RealtimeTelemetry` consumer against SDK 0.9.0. Do not mix old and new C++
  binaries; the appended virtual extension, enlarged snapshot, and enqueue
  signature change the C++ ABI. The stable C ABI remains 1.0.
- Consume `RealtimeTelemetrySnapshot` schema 3 and nested logical output
  payload schema 1. Keep existing schema-2 fields for compatibility with finite
  non-silent input; they remain legacy values and may be stale after a routing
  failure.
- Treat routing LUFS as `LegacyLufsProxy`, not integrated loudness. Use the
  separate control/offline `LoudnessMeter` for K-weighted loudness analysis.
- Account for the intentional corrections: non-finite input/control values
  sanitize or reject rather than propagate NaN/Inf, and a whole-signal-silent
  true-peak block resets filter history and reports exact silence rather than
  FIR residue.
- Treat `routing_output_start` as a routing-matrix lane. Apply device mapping
  later through `AudioOutputRouteRequest::output_channel_map`.
- Require the 0.9.0 package and a complete C++ rebuild; the stable C ABI does
  not remove the C++ binary migration requirement.

## Verification record

Observed on this branch:

```text
python3 tools/version_contract.py --check
SDK version contract is consistent: 0.9.0

cmake --build build --parallel
configured Debug build completed; existing AppleClang duplicate-gtest linker
warnings remained.

Focused routing meter tests
9/9 passed, including logical lanes, overlapping routes, availability,
topology revision, true-peak silence/history, finite sanitization,
transactional validation, and LUFS compatibility.

Focused transport canonical tests
5/5 passed, including independent logical telemetry, retained peaks across ring
drops, failure/unavailable distinction, unsupported extension behavior, and
schema stamping.

Post-review remediation added deterministic coverage for an in-progress route
transaction, reinitialization revision reset, the exact
`publishFromRealtime(const RealtimeTelemetrySnapshot&) noexcept` member
signature, non-silent maximum-topology allocation behavior, and coherent
snapshot bounds. The follow-up Debug checks observed 58/58 routing tests,
10/10 realtime-diagnostics tests, the non-silent 32-group/32-output allocation
gate, the installed `cmake_find_package` fixture, and the concurrent query
harness passing.


Release max-topology deadline test, unsanitized `build-release`
Sample peak: average 4710.97 us, p99 5163.04 us, maximum 5236.5 us.
True peak: average 6575.28 us, p99 6954.04 us, maximum 7111.5 us.
Budget: 10666.7 us (512 frames at 48 kHz). Both observed maxima were below the
budget in this controlled local release run.


```

The exact separate TSan command was executed on macOS after adding the
serialized packed-route publication protocol. The concurrent query test now
asserts bounds for every coherent group-output snapshot and passed with no
ThreadSanitizer warning. The governed ShmUI manifest check passed with 57
files and content SHA-256

`3741685e84433a3dfa4efebc1d930ef02f0b678f0b2cd162e6cb140022b273d9`.

The required 11-test Debug contract set passed 11/11, and the complete
configured Debug CTest suite passed 80/80 in 341.21 seconds. The full suite
included package, static-audit, documentation, routing, transport, CoreAudio,
and stress gates; the new maximum-topology harness ran as part of the suite.


## Reference

[1] ITU-R, Recommendation ITU-R BS.1770-4, “Algorithms to measure audio
programme loudness and true-peak audio level,” Oct. 2015. [Online]. Available:
https://www.itu.int/rec/R-REC-BS.1770-4-201510-S
