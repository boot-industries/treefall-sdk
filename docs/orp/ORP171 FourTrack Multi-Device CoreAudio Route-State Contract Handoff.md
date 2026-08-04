<!-- SPDX-License-Identifier: MIT -->

# ORP171 — FourTrack Multi-Device CoreAudio Route-State Contract Handoff

**Document type:** SDK public-contract handoff
**Status:** Proposed; upstream implementation and release pending
**Date:** 2026-08-04
**Consumer:** FourTrack / EightTrack, FTR074
**Current FourTrack SDK pin:** `b452cbfa2b1d11a36bc3f5d69a15e2dea9a80099`
**Related:** ORP155, ORP156, ORP162; FourTrack `docs/ftr/FTR074 Multi-Device CoreAudio I O Investigation and SDK Handoff.md`

---

## 1. Decision requested

FourTrack needs a production-safe, capability-driven CoreAudio route contract for
independently selected input and output endpoints. The SDK must own endpoint
activation, physical-to-logical channel mapping, negotiated route facts, route
timing, and endpoint-runtime outcomes. FourTrack must not open CoreAudio devices,
downcast a factory-created driver, infer a route from display names, or infer
capture health from silent samples.

This document is an SDK handoff, not an implementation record. It does not claim
that the proposed route-state contract is implemented or released. FourTrack must
not consume a dirty SDK worktree revision. The required upstream issue/PR URL and
released SDK revision are intentionally still unassigned.

The existing directional endpoint-ID contract from ORP155 is already the correct
foundation and must be retained. The remaining work is a new, versioned contract
for channel maps, active-route state, timing, and safe runtime change handling.

## 2. Source-verified baseline

### 2.1 Contract already delivered by ORP155/ORP156

The pinned SDK public header exposes independent fields:

```cpp
struct AudioDriverConfig {
  uint32_t sample_rate = 48000;
  uint16_t buffer_size = 512;
  uint16_t num_inputs = 2;
  std::string input_device_id;   // empty = directional system default
  uint16_t num_outputs = 2;
  std::string output_device_id;  // empty = directional system default
  std::string device_name;
};
```

For CoreAudio:

- each non-empty identifier is a persistent
  `kAudioDevicePropertyDeviceUID`, not a display name or enumeration index;
- input and output identifiers resolve independently;
- an unknown or direction-incompatible explicit identifier returns
  `SessionGraphError::InvalidParameter` without default fallback;
- two valid distinct endpoints use an SDK-owned private aggregate; and
- an empty identifier selects only that direction's current system default.

This closes the original FTR020 UID-field blocker. It does **not** provide the
full route-state contract below.

### 2.2 Current consumer gap

FourTrack's current path still constructs the real driver without a route
configuration:

1. `apps/fourtrack-mac/bridge/FourTrackBridge.mm::Doc::build_and_start()` creates
   `createCoreAudioDriver()` and starts the engine without a route request.
2. `src/fourtrack/engine/engine.cpp::Engine::start()` leaves both directional
   SDK IDs empty and configures one input plus two outputs.
3. The bridge's existing device setter records only the advisory capture hint and
   discards the selected output UID.
4. The Swift catalog presents UIDs and directional presence, but it is not proof
   that a route is active.

FTR074 records this path, the host/SDK ownership boundary, and the macOS route
probe. `system_profiler SPAudioDataType` is inventory evidence only; it does not
expose CoreAudio UIDs, channel labels, alive state, rate ranges, buffer values, or
latency properties.

## 3. Required public SDK contract

The following is the recommended canonical shape. Before implementation, reconcile
it with any parallel draft types in the SDK worktree. Do not ship two competing
route-state or latency models.

### 3.1 Channel maps

```cpp
struct AudioRouteChannelMap {
  std::vector<uint16_t> input_channels;
  std::vector<uint16_t> output_channels;
};
```

Add `AudioRouteChannelMap channel_map{}` to `AudioDriverConfig`.

Semantics:

- an empty directional vector preserves the existing consecutive mapping
  `0..num_inputs-1` or `0..num_outputs-1`;
- a non-empty vector must exactly match that direction's logical channel count;
- values must be unique; and
- every value must be within the resolved physical direction's channel count.

Validation occurs after both directional endpoints resolve and before the
AudioUnit is started. Invalid maps return `SessionGraphError::InvalidParameter`.
No host-side channel-count assumption is permitted.

### 3.2 Active-route timing and state

```cpp
struct AudioRouteLatency {
  uint32_t capture_frames = 0;
  uint32_t playback_frames = 0;
  uint32_t processing_frames = 0;
  bool complete = false;
};

enum class AudioRouteRuntimeOutcome : uint8_t {
  Healthy,
  RouteUnavailable,
  FormatChanged,
  ReinitializationRequired,
  BackendFailure,
};

struct ActiveAudioRoute {
  std::string input_device_id;
  std::string output_device_id;
  std::vector<uint16_t> input_channels;
  std::vector<uint16_t> output_channels;
  uint16_t available_input_channels = 0;
  uint16_t available_output_channels = 0;
  uint32_t requested_sample_rate = 0;
  uint32_t actual_sample_rate = 0;
  uint32_t actual_buffer_frames = 0;
  AudioRouteLatency latency;
  bool input_alive = false;
  bool output_alive = false;
};
```

Add a control-thread-only `IAudioDriver::getActiveRoute() const` accessor with an
empty/default result for backends that do not implement the contract.

Extend `AudioIoTelemetry` with `AudioRouteRuntimeOutcome route_outcome`. Preserve
existing sample-rate telemetry and source compatibility for existing consumers;
do not silently replace the established backend-runtime outcome enum. The final
public naming must use one canonical outcome model rather than parallel enums with
ambiguous ownership.

The timing invariant is:

```text
round_trip_frames = capture_frames + playback_frames + processing_frames
```

The SDK reports `complete = false` when a mandatory term cannot be read. It must
not estimate a missing term from buffer size, codec assumptions, wall-clock time,
or a guessed aggregate contribution.

### 3.3 Non-CoreAudio behavior

Non-CoreAudio backends may retain the default empty route, zero timing terms, and
`Healthy` route outcome until they implement the contract. They must not fabricate
physical endpoint IDs, maps, alive state, or timing.

`IAudioDriverManager` must not be extended for this feature. Its current
single-active-device and output-oriented `AudioDeviceInfo` abstraction cannot
represent independent directional endpoints and maps.

## 4. CoreAudio implementation contract

### 4.1 Initialization

`CoreAudioDriver` must:

1. resolve the requested input and output UIDs independently;
2. reject unknown or direction-incompatible explicit UIDs without consulting a
   fallback default;
3. retain the two physical endpoint UIDs even when a private aggregate is the
   AudioUnit device;
4. discover each resolved direction's physical channel count;
5. validate and apply both channel maps at AudioUnit configuration time;
6. retain requested and actual sample rate and buffer size; and
7. publish a control-thread `ActiveAudioRoute` only after the route is configured.

The aggregate is an implementation detail. The physical endpoint IDs in
`ActiveAudioRoute` are the route identity FourTrack displays and persists as
provenance.

### 4.2 Timing

The existing CoreAudio latency query returns a combined round-trip quantity. The
new implementation must assign each available HAL/AudioUnit term to exactly one
of capture, playback, or processing. It must document the assignment in the
implementation record and test the sum invariant.

If a mandatory property is unavailable, publish an incomplete route rather than a
false complete compensation value. FourTrack will refuse automatic record
placement for an incomplete route.

### 4.3 Runtime route changes

The SDK must listen off the render callback for at least:

- endpoint alive state;
- nominal sample rate;
- stream format; and
- buffer size.

The CoreAudio property callback may only atomically mark a generation or pending
change. A control worker performs HAL reads and recovery. It must close callback
admission before a format mismatch can reach `processAudio`, publish
`RouteUnavailable`, `FormatChanged`, or `ReinitializationRequired`, and stop
rendering when safe in-place restoration is impossible.

No CoreAudio property query, device discovery, allocation, lock, or I/O may enter
the render callback. `getActiveRoute()` and telemetry remain control-thread
reads.

## 5. FourTrack consumption after release

After the SDK contract is merged and released, FourTrack will:

1. update its immutable submodule pin;
2. map its platform-neutral `AudioRouteRequest` to the SDK directional IDs and
   channel maps;
3. retain its internal stereo master and project only at the realtime driver
   boundary to one or two selected outputs;
4. latch complete route timing for deterministic record placement;
5. suspend to Idle on terminal route failure without committing an incomplete
   temporary take;
6. expose requested versus resolved route facts through the bridge and Settings;
   and
7. persist only the actual active capture endpoint as session provenance.

FourTrack will not add a second CoreAudio abstraction, route by display name, or
persist output preferences inside the `.trk` bundle.

## 6. SDK verification contract

The upstream implementation is not ready for FourTrack consumption until these
contracts have deterministic coverage:

### Endpoint and map resolution

- unknown explicit input UID returns `InvalidParameter` without default access;
- unknown explicit output UID returns `InvalidParameter` without default access;
- direction-incompatible UIDs reject without fallback;
- valid distinct endpoints create one SDK-owned private aggregate;
- active-route state reports the two physical UIDs, not only the aggregate UID;
- empty maps preserve consecutive mapping;
- valid non-consecutive maps apply in logical order; and
- duplicate, wrong-length, and out-of-range maps reject deterministically.

### Active route and timing

- actual rate, actual buffer, directional counts, maps, and alive state are
  reported from CoreAudio properties;
- complete timing satisfies the split-term sum invariant;
- missing mandatory timing data makes `complete` false; and
- control-thread reads do not query HAL from the render callback.

### Runtime changes

Injected property-API tests must cover alive loss, nominal-rate change,
stream-format change, and buffer-size change. Each case must close render
admission before unsafe audio reaches the host and publish the matching terminal
outcome. Safe restoration and explicit reinitialization must remain distinct.

### Package and consumer evidence

Run the SDK's prescribed build, test, realtime audit, installed-package consumer,
and add-subdirectory consumer checks on the SDK branch. FourTrack then updates
only to the released immutable revision and runs its deterministic mock-driver
route tests before any hardware acceptance.

## 7. Release and handoff checklist

The SDK handoff is complete only when the following values are recorded here and
in the FourTrack FTR074 report:

| Item | Required value | Current value |
|---|---|---|
| Upstream issue/PR | URL and target branch | Not assigned |
| SDK implementation branch | branch name | `feature/orp-output-endpoint-contract` is a dirty worktree; not consumable |
| SDK contract commit | immutable commit | Not available |
| Released SDK revision | tag/package revision | Not available |
| FourTrack pin | immutable gitlink | `b452cbfa2b1d11a36bc3f5d69a15e2dea9a80099` |
| FourTrack adoption | route activation/status path | Blocked pending release |

The current FourTrack pin describes as `v0.6.7-29-gb452cbfa`; no release tag
points at that exact revision. Do not replace the pin with a local SDK worktree
commit.

When the upstream release exists, update this table, ORP171, FTR074, and the
consumer pin together. Record the SDK verification outputs and the FourTrack
adoption tests in the same checkpoint.

## 8. Non-goals

This handoff does not request:

- a FourTrack-specific SDK factory;
- a new `IAudioDriverManager` device abstraction;
- display-name or transport-name heuristics;
- automatic fallback from a missing explicit UID;
- a guessed latency compensation path;
- direct CoreAudio dependencies in FourTrack `src/` or `include/`; or
- Bluetooth behavior claims not supported by CoreAudio-reported capabilities.

The correct stopping point is an explicit unavailable/reconfiguration outcome,
not silent default routing.
