<!-- SPDX-License-Identifier: MIT -->

# ORP144 — Public Transport Runtime Contract and Clip Composer Handoff

**Document type:** SDK release completion record and downstream handoff
**Version target:** `0.4.0`
**Status:** SDK `v0.4.0` tagged and published; Clip Composer adoption pending
**Date:** 2026-07-14

---

## 1. Scope and release truth

Version `0.4.0` adds a host-neutral public rendering boundary to the existing
transport controller. The factory still returns
`std::unique_ptr<ITransportController>`, so factory callers need only recompile.
The new methods are pure virtual, however: custom interface implementations and
test doubles must implement all three runtime methods. The pre-1.0 minor version
records that implementer source migration explicitly.

The root CMake project now supplies `0.4.0` to the generated version header,
release JSON, package-version file, and CPack metadata. Those artifacts consume
`PROJECT_VERSION`; their templates do not contain an independent version literal
and require no separate version edit.

Release tag `v0.4.0` is published at merge commit `1f4bd48f` from PR #209.
The Release package candidate was built and inspected before tagging. Clip
Composer adoption remains pending and is not inferred from the SDK release.

---

## 2. Public transport runtime contract

### 2.1 Renderer configuration

```cpp
struct TransportRenderConfig {
  uint32_t sampleRate = 0;
  uint32_t outputChannels = 0;
  uint32_t maxBlockFrames = 0;
};

virtual TransportRenderConfig getRenderConfig() const noexcept = 0;
```

`getRenderConfig()` is a control-thread query returning a value copy. Its fields
report:

- `sampleRate`: the sample rate supplied when the concrete controller was
  constructed;
- `outputChannels`: the transport's configured output count; its routing
  topology is fixed after construction in this release; and
- `maxBlockFrames`: the concrete renderer's hard per-call block limit.

No concrete transport type or private transport header is needed to query this
shape.

Hosts must not call `getRoutingMatrix()->initialize(...)` on a live transport in
`0.4.0`. Routing-matrix initialization rebuilds channel state, while the
transport's stereo channel layout is established during construction. A later
typed routing-configuration API must own that transition rather than exposing an
apparently safe partial reconfiguration.

### 2.2 Audio render entry point

```cpp
virtual void processAudio(float** outputBuffers,
                          size_t numChannels,
                          size_t numFrames) noexcept = 0;
```

`processAudio()` is the host's public planar-output render entry point. The host
supplies exactly `getRenderConfig().outputChannels` writable buffers and no more
than `getRenderConfig().maxBlockFrames` frames per call.

The method is non-reentrant and belongs to exactly one audio thread. It is the
sole consumer of the control-to-audio SPSC command ring. Its realtime contract
forbids allocation, locks, blocking, file or network I/O, and host callback
dispatch. Control sources must continue to funnel all transport mutations
through exactly one producer thread; UI, MIDI, OSC, or other producers may not
call the SPSC producer side concurrently.

### 2.3 Control-thread callback and tempo pump

```cpp
virtual void processCallbacks() = 0;
```

`processCallbacks()` belongs to exactly one control/message thread and is the
sole consumer of the audio-to-control SPSC event ring. It must not run
concurrently with `setCallback()`. Hosts must pump it even when no callback
object is installed because the same pump republishes `SessionGraph` tempo for
lock-free transport-position beat queries.

---

## 3. Source and installed-package compatibility status

| Consumer path | Repository evidence | Status for `0.4.0` |
| --- | --- | --- |
| Public source include | `include/orpheus/transport_controller.h` declares `TransportRenderConfig`, all three interface methods, and `createTransportController()`. | Verified through `transport_controller_test`; no concrete transport include remains in that test target. |
| Source-tree CMake target | `src/core/transport/CMakeLists.txt` gives `orpheus_transport` a public build include path and defines the `Orpheus::transport` alias. | Focused build and 14/14 transport tests passed. |
| Installed headers and target | Root `CMakeLists.txt` installs `include/orpheus`, exports `orpheus_transport`, and installs package configuration; `cmake/OrpheusSDKConfig.cmake.in` creates the stable `Orpheus::transport` alias. | `cmake_package_runtime_consumer` installed to a clean prefix, resolved the candidate package, linked only `Orpheus::transport`, built, and ran successfully. |
| Release metadata | Root `project(orpheus VERSION 0.4.0)` feeds the generated header, release JSON, CMake package version, and CPack fields. | CPack produced `orpheus-sdk-0.4.0-Darwin-arm64.zip` plus SHA-256; the archived metadata and public transport header report the expected candidate. |

The installed package uses CMake `SameMinorVersion` compatibility. The positive
consumer resolves `0.4.0`, while `cmake_package_rejects_previous_minor` proves
the same package is rejected for a `0.3.0` request rather than advertising a
source-incompatible interface to a `0.3.x` consumer.

The source-tree regression and installed consumer both create the transport
through its public factory, query the render config, render a bounded block, and
drain callbacks through `ITransportController`. The installed fixture includes
only `<orpheus/transport_controller.h>` from the clean prefix; its compiler flags
contain no SDK source-tree include path.

---

## 4. Clip Composer handoff

Clip Composer must perform a clean public-interface cutover in its own repository:

1. Store the factory result as
   `std::unique_ptr<orpheus::ITransportController>` rather than a pointer to the
   concrete SDK transport class.
2. Delete its include of the SDK-internal transport header.
3. Delete the concrete-controller downcast used to reach rendering or callback
   methods.
4. Query `getRenderConfig()` when attaching/reconfiguring its audio driver and
   honor the published channel count and maximum block size.
5. Call `processAudio()` only from its single audio callback thread.
6. Call `processCallbacks()` from exactly one message/control-thread pump,
   including when no transport callback object is installed.
7. Keep all transport mutations serialized onto one control producer so the
   SDK's SPSC ownership contract is not violated.
8. Validate both the source-submodule target and a clean installed-package
   `find_package` path before advancing the app's SDK pin.

This handoff records required downstream work only. No Clip Composer header,
ownership model, downcast, package pin, build, or runtime validation is claimed
as changed by ORP144.

---

## 5. Release evidence and remaining gates

Verified candidate checks:

- Debug CTest: **146/146 passed**;
- Release CTest: **146/146 passed**;
- focused `transport_controller_test`: **15/15 passed**;
- public runtime contract filter: **2/2 passed**;
- clean-prefix `cmake_package_runtime_consumer`: install/configure/build/run
  passed against `OrpheusSDK 0.4.0`;
- clean-prefix `cmake_package_rejects_previous_minor`: `0.4.0` correctly rejected
  a `0.3.0` package request;
- CPack produced `orpheus-sdk-0.4.0-Darwin-arm64.zip` and its SHA-256 file; and
- archive inspection confirmed metadata version `0.4.0` and the public runtime
  declarations in the packaged transport header.

`RealtimeHarnessTest.FileBackedRenderDoesNoFileIO` remains skipped on macOS
because `/proc/self/io` is unavailable; the containing CTest target passes and
the static realtime audit remains green.

Remaining gate:

- migrate Clip Composer to the public interface and run its build/test/smoke and
  installed-package gates.

The accurate state is **SDK-released/downstream-pending**.
