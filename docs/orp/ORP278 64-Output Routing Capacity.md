<!-- SPDX-License-Identifier: MIT -->

# ORP278 — 64-Output Routing Capacity

**Date:** 2026-11-03  
**Branch:** `feat/orp278-64-output-routing` (from `origin/main` `898ae630`)  
**Driver:** Clip Composer OCC241 opens every output of a multichannel device and
patches buses to any of up to 64 device outputs.

## Decision

`orpheus::kRoutingMaxOutputs` rises from 32 to 64. The routing matrix, the
transport controller factory, the routing meter state, the group output meter
snapshot, the realtime telemetry snapshot and the dummy audio driver all
accept up to 64 output lanes. `kRoutingControlMaxGroups` (32 groups),
`TransportConfig::maxActiveVoices` (32 voices) and the 256 source-channel
ceiling are unchanged.

## Widened sites

| Site | Change |
|---|---|
| `include/orpheus/routing_matrix.h` | `kRoutingMaxOutputs = 64`; doc comments |
| `src/core/routing/routing_matrix.cpp` `initialize` | output bound is `kRoutingMaxOutputs` (was literal 32) |
| `src/core/routing/routing_matrix.cpp` `processRouting` | `output_slice_ptrs[kRoutingMaxOutputs]` stack scratch |
| `src/core/routing/routing_matrix.cpp` `processRoutingBlock` | `occupied_lane_masks` is `std::array<uint64_t, kRoutingControlMaxGroups>`; `occupied_output_mask`, `lanes_to_clear`, `occupiedLanes` are `uint64_t`; every `uint32_t{1} << lane\|output` is `uint64_t{1} << …` |
| `src/core/routing/routing_matrix.h` | `m_previous_occupied_lane_masks` is `uint64_t` per group |
| `src/core/transport/transport_controller.cpp` `createTransportController` | `outputChannels` bound is `kRoutingMaxOutputs` |
| `src/core/audio_io/dummy_audio_driver.cpp` | `initialize`, `initializeAudioOutput` and `probeRoute` bound outputs by `kRoutingMaxOutputs`; the input bound stays 32 |

Arrays already sized from `kRoutingMaxOutputs` (lane meters, lane peak/RMS/clip
state, true-peak meters, telemetry output meters and availability, transport
meter scratch) grow automatically. The packed channel route
(`group` in bits 0–15, `lane` in bits 16–31) and `route_snapshots` already
carry lanes above 31 and are unchanged.

Not changed, deliberately:

- `clip_routing.cpp` (`MAX_OUTPUT_CHANNELS = 32`) and `channel_format.h`
  (`MAX_FORMAT_CHANNELS = 32`) belong to the legacy per-clip bus and channel
  layout models, not the routing matrix.
- The enumerated dummy endpoint still lists 32 output channels
  (`driver_manager.cpp`); it describes a virtual device rather than a routing
  cap, and consumers assert that shape.
- Platform drivers (CoreAudio, WASAPI) bound outputs by the configuration they
  are given and carry no fixed 32-channel cap.

## ABI and migration

C ABI 1.0 is unchanged: `include/orpheus/abi.h` embeds no array sized by
`kRoutingMaxOutputs`. Public C++ layouts changed: `GroupOutputMeterSnapshot`
and `RealtimeTelemetrySnapshot` carry 64 lane meters instead of 32, and the
internal routing state grew. **Rebuild every consumer** against the new
headers (the C++ ABI is not stable across this change). Hosts that hard-coded
32 outputs keep working; hosts that sized from `kRoutingMaxOutputs` gain the
capacity automatically. The audio callback remains allocation-free and
lock-free; the added state is fixed-size and the stack scratch grows by at
most a few hundred bytes.

## Tests

`tests/routing/routing_matrix_test.cpp`:

- `SixtyFourOutputRenderRoutesGroupToLastTwoLanes` — a group routed to outputs
  62–63 of a 64-output render produces signal on exactly those lanes; output
  meters 62–63 and group lane meters 0–1 (start 62) publish.
- `LogicalLanesAtOrAboveThirtyTwoAreMarkedOccupied` — a discrete channel on
  logical lane 40 renders to output 40 only and its lane meter publishes
  (regression for the 32-bit occupancy mask).
- `InitializeAcceptsMaxOutputsAndRejectsOneMore` — 64 outputs initialize, 65
  are rejected.

## Verification

See the delivery notes of the OCC241 PR that consumes this change. Commands:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DORPHEUS_ENABLE_REALTIME=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

including the `cmake_find_package`, `realtime_static_audit` and
`docs_path_audit` CTest gates.
