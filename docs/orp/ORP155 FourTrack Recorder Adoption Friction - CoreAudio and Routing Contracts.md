<!-- SPDX-License-Identifier: MIT -->

# ORP155 — FourTrack Recorder Adoption Friction: CoreAudio and Routing Contracts

**Document type:** SDK public-contract implementation record
**Status:** Implemented and verified
**Scope:** Three Orpheus-owned contracts required by the FourTrack recorder; no downstream workaround
**Consumer evidence:** FourTrack `src/fourtrack/engine/engine.cpp`, `src/fourtrack/bounce/mix_bus.cpp`, and `apps/fourtrack-mac/Sources/Model/EngineController.swift` at the `v0.5.3` SDK pin
**Date:** 2026-07-16

---

## 1. Boundary

Orpheus owns the routing matrix, audio-driver configuration, CoreAudio endpoint
resolution, and backend telemetry described here. FourTrack must not copy routing
DSP, open HAL devices directly, downcast a factory-created driver, or infer capture
health from silent samples.

FourTrack will continue to consume the SDK audio file writer and reader, input
stream, streaming clip source and worker, resampler, and routing matrix. The three
items below are SDK contract gaps, not requests for downstream adapters.

## 2. Isolated channel-meter contract

### Pre-implementation finding

`IRoutingMatrix::getChannelMeter(RoutingChannelIndex)` is public in
`include/orpheus/routing_matrix.h`. Its concrete data is produced by
`RoutingMatrix::processRoutingBlock()` in
`src/core/routing/routing_matrix.cpp`.

Before this implementation, the loop accumulated each channel into
`m_group_buffers[group_index]`, then called `processStereoMetering()` on that
cumulative group buffer. Later channel meters therefore included earlier
channels routed to the same group and depended on channel iteration order.

FourTrack uses channel meters as per-strip routed meters. A cumulative meter makes a
quiet later strip display another strip's signal and prevents a reliable pre/post
fader comparison.

### Implemented contract

`RoutingMatrix::processRoutingBlock()` must meter each channel's own
gain/pan-adjusted contribution before that contribution is accumulated into its
group. `getChannelMeter(i)` must be independent of channel order and of signal on
other channels.

The contribution must follow the configured `SourceChannelPolicy`, smoothed gain,
smoothed pan, effective mute/solo state, and discrete output selection used for
routing. Group and master meters continue to observe their respective accumulated
stages.

### Realtime invariants

- No allocation, lock, I/O, logging, or callback on the audio thread.
- Reuse fixed scratch allocated by `RoutingMatrix::initialize()`; do not allocate a
  per-channel vector in `processRoutingBlock()`.
- Preserve the current gain/pan smoothing cadence and the bounded
  `MAX_BUFFER_SIZE` block contract.
- Publish meter atomics with the existing lock-free reader behavior.

### Verification contract

The routing coverage uses two channels in one stereo-pair group: one low
constant and one distinguishable high constant, with non-unity gain and pan.
It renders both channel orders, verifies isolated post-gain/pan readings and
summed group/master readings, and verifies that muting either channel silences
only that channel meter and contribution.

## 3. Stable duplex CoreAudio endpoint contract

### Pre-implementation finding

Before this implementation, `AudioDriverConfig` exposed one
`std::string device_id` plus `device_name`. CoreAudio ignored `device_id`,
selected the default duplex route or matched a mutable display name, and could
not honor independently selected persistent input/output UIDs. Aggregate
creation failure fell back to the output device, leaving capture to fail later.

### Implemented public contract

The ambiguous field is replaced by direction-specific stable identifiers:

```cpp
struct AudioDriverConfig {
  uint32_t sample_rate = 48000;
  uint16_t buffer_size = 512;
  uint16_t num_inputs = 2;
  std::string input_device_id;  // empty = default input
  uint16_t num_outputs = 2;
  std::string output_device_id; // empty = default output
  std::string device_name;
};
```

For CoreAudio, each non-empty identifier is a HAL
`kAudioDevicePropertyDeviceUID`, not an `AudioDeviceID`, enumeration index, or
display name. Each direction resolves independently. Different physical
endpoints use a private aggregate with the output as clock master and drift
compensation only when clock domains require it. Every driver-owned aggregate
is destroyed during reinitialize, cleanup, and destruction.

An unknown or direction-incompatible UID returns
`SessionGraphError::InvalidParameter` without default fallback. An empty field
alone selects that direction's current system default. Driver-manager endpoint
IDs and backend/config fixtures use the directional fields; no `device_id`
alias remains.

### Realtime invariants

- UID lookup, aggregate creation/destruction, format negotiation, and capability
  checks remain control-thread work before `start()`.
- The render callback retains preallocated planar buffers and performs no endpoint
  lookup, allocation, lock, I/O, or logging.
- Endpoint replacement cannot race a running callback; preserve the existing
  stop/cleanup/initialize ownership boundary.

### Verification contract

CoreAudio coverage enumerates distinct input/output UIDs, initializes the
driver with both, and exercises capture/playback. It covers same-device and
one-direction-default configurations, rejects unknown and
direction-incompatible UIDs without fallback, reinitializes the deterministic
private aggregate UID, and confirms driver destruction removes that aggregate.

## 4. Public capture-failure telemetry

### Pre-implementation finding

Before this implementation, CoreAudio counted `AudioUnitRender()` failures in
a concrete-driver-only field. Factory consumers held
`std::unique_ptr<IAudioDriver>` and could not distinguish real silence from a
failed input render without an unavailable implementation downcast. The
counter's `fetch_add` also wrapped instead of saturating.

### Implemented public contract

The public header now exposes this host-neutral value and virtual query:

```cpp
struct AudioIoTelemetry {
  uint64_t input_render_failures = 0;
};

class IAudioDriver {
public:
  virtual AudioIoTelemetry getTelemetry() const noexcept { return {}; }
  // existing interface
};
```

`CoreAudioDriver` overrides `getTelemetry()` and returns its cumulative
capture-bus failure count since the last successful `initialize()`. Its atomic
increment saturates at `UINT64_MAX`. Dummy and custom drivers inherit the zero
default until they report backend failures.

The query is factory-visible diagnostics, not a callback and not an instruction for
FourTrack to repair or retry a backend route.

### Realtime invariants

- Failure increments are lock-free, bounded, `noexcept`, and allocation-free.
- The control-thread query performs only atomic loads and never calls HAL.
- Telemetry publication must not turn a transient capture miss into an audio
  callback failure; pre-zeroed silence remains the callback fallback.

### Verification contract

Public-interface coverage holds CoreAudio as `std::unique_ptr<IAudioDriver>`,
observes the test seam through `getTelemetry()`, verifies successful capture
stays at zero, verifies initialize reset and the dummy zero default, and proves
saturation from `UINT64_MAX - 1` without wraparound.

## 5. Downstream FourTrack migration

After the SDK release containing all three contracts, FourTrack will:

1. bump its pinned SDK release and remove the FTR020 device-routing blocker;
2. map persisted selected UIDs to `input_device_id` and `output_device_id` when it
   constructs the real driver config;
3. keep using `IRoutingMatrix::getChannelMeter()` for routed per-track meters,
   deleting no DSP and adding no meter workaround; and
4. surface `IAudioDriver::getTelemetry()` through its existing snapshot/diagnostic
   path without downcasting or polling from the realtime callback.

FourTrack makes no interim workaround. Its writer, reader, input stream, streaming
source/worker, resampler, and routing-matrix consumption remain SDK-backed.

## 6. Acceptance matrix

| Concern | SDK implementation proof | Contract test | FourTrack adoption proof |
|---|---|---|---|
| Isolated channel meter | Meter pre-accumulation contribution with fixed scratch | Two distinguishable channels remain order-independent; group/master still sum | Per-strip routed meters isolate each track under gain, pan, mute, and solo |
| Duplex endpoint IDs | Separate HAL UID resolution; owned aggregate lifecycle; no fallback | Requested input/output, defaults, unknown IDs, direction mismatch, aggregate cleanup | Selected device UIDs become the active routes after reopen and device change |
| Capture telemetry | Factory-visible `AudioIoTelemetry`; saturating atomic count | Failure, success, reset, dummy-default, and saturation cases through `IAudioDriver` | Silent input and failed capture are distinguishable without a concrete-driver cast |

## 7. Implementation record

### Routing meters

- `RoutingMatrix::initialize()` owns one fixed `m_channel_meter_buffer`.
- `RoutingMatrix::processRoutingBlock()` writes each channel's isolated
  gain/pan-adjusted contribution into that scratch before group accumulation.
- Channel mute, solo exclusion, and null input publish current silence instead
  of retaining a stale reading. Group and master meters retain accumulated-stage
  readings.
- `ChannelMetersReportIsolatedEffectiveContributions` verifies distinguishable
  non-unity gain/pan contributions in both channel orders.
  `ChannelMetersPublishCurrentSilenceForMuteSoloAndNullInput` verifies both
  channels independently under mute plus solo and null-input behavior.
  `GroupAndMasterMetersRetainSummedStageReadings` verifies summed-stage meters.

### Directional endpoint selection

- `AudioDriverConfig` now exposes `input_device_id` and `output_device_id`.
  The former `device_id` member was removed without an alias.
- CoreAudio resolves each non-empty field as a persistent
  `kAudioDevicePropertyDeviceUID`; only an empty direction selects its current
  system default. Stream configuration validates the requested direction.
- Distinct physical endpoints use a driver-owned private, non-stacked aggregate.
  Output is its clock master. Input drift compensation is enabled only when the
  endpoints do not report the same non-zero clock domain.
- Aggregate creation failure, unknown UIDs, and direction-incompatible UIDs
  return `SessionGraphError::InvalidParameter`; there is no output-only fallback.
  Reinitialize, cleanup, and destruction release the owned aggregate.
- CoreAudio manager enumeration publishes bare DeviceUID values. Dummy
  capability channel IDs use the directional config fields. WASAPI defaults
  only for an empty output field and returns `InvalidParameter` for unsupported
  or unknown explicit output IDs.

CoreAudio lifecycle coverage uses factory-created `IAudioDriver` instances and
the host's real DeviceUIDs. On the verification host, distinct default input and
output endpoints, one-direction-default selection, an explicit same-device
duplex endpoint, direction mismatch, aggregate UID reuse, aggregate destruction,
and unknown input/output UIDs all executed without a hardware-dependent skip.
The Windows-only WASAPI fixture covers explicit unsupported and unknown output
IDs; it was not compiled or executed by the macOS build.

### Capture-failure telemetry

- Public `AudioIoTelemetry` and the default
  `IAudioDriver::getTelemetry() noexcept` make diagnostics factory-visible
  without requiring a concrete-driver downcast.
- CoreAudio resets its counter during successful initialize, increments it with
  a bounded saturating CAS loop, and preserves pre-zeroed input silence after a
  failed `AudioUnitRender()`. Dummy inherits the zero default.
- Tests verify factory visibility, successful-capture zero, initialize reset,
  dummy zero, and saturation at `UINT64_MAX`.

### Verification

Observed on macOS 26 with the configured `build-ci` tree:

```text
cmake --build build-ci --parallel 8
  completed successfully

coreaudio_driver_test: 31 passed, 0 hardware-dependent skips
dummy_driver_test:     14 passed
driver_manager_test:   20 passed
routing_matrix_test:   39 passed

ctest --test-dir build-ci --output-on-failure
  100% tests passed, 0 failed out of 150
```

The complete CTest run also passed `realtime_static_audit`,
`docs_path_audit`, `version_contract`, `cmake_find_package`, package-consumer,
and add-subdirectory checks. Windows build/ABI evidence remains dependent on a
Windows CI run after the branch is pushed; this macOS verification does not
claim that platform result.
