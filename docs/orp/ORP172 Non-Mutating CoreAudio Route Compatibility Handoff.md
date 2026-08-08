<!-- SPDX-License-Identifier: MIT -->

# ORP172 — Non-Mutating CoreAudio Route Compatibility Handoff

**Document type:** SDK implementation and downstream handoff
**Status:** Implemented and merged; downstream adoption deferred to FTR078
**Date:** 2026-08-08
**Consumer:** FourTrack / EightTrack, FTR077 and FTR078
**Implementation PR:** [orpheus-sdk#240](https://github.com/chrislyons/orpheus-sdk/pull/240)
**Merged SDK revision:** `33cd334151bec0e00a495bb4339845791917cb74`
**Pre-sprint SDK baseline:** `dcf5cc402dc04e7d5bf31938c51cb9c9a304da9a`
**Related:** ORP155, ORP156, ORP171; FourTrack FTR077 and FTR078

---

## 1. Decision and adoption boundary

FTR077 required a host-neutral CoreAudio compatibility probe that FourTrack can
run on a control thread before replacing its live engine. ORP172 delivers that
probe in the SDK. It classifies the route without activation and without
claiming that a route can be started.

The probe is side-effect free. It does not:

- create or destroy a private aggregate device;
- create or configure an AUHAL/AudioUnit;
- write a CoreAudio property, including nominal sample rate;
- start or stop I/O;
- register or remove a property listener;
- request microphone permission or invoke TCC APIs; or
- mutate active-route, session, or driver state.

FourTrack must not adopt this API in FTR077. FTR078 owns the later runtime
migration: runtime-outcome taxonomy cleanup, transactional rate writes and
rollback, passive monitoring, and application adoption. FourTrack's SDK
submodule pin and source tree were not changed by ORP172.

The current FourTrack `main` baseline remains `6492e21d43c89f3b7b0cd85234fd214988c764a3`.
Its embedded SDK gitlink was pre-existing modified state and was not touched.

## 2. Public contract

`include/orpheus/audio_driver.h` now exposes the additive policy and result
contract:

```cpp
enum class AudioSampleRatePolicy : uint8_t {
  PreserveDeviceRate,
  RequestExactRate,
};

enum class AudioRouteCompatibilityStatus : uint8_t {
  Compatible,
  RequiresSampleRateChange,
  InputUnavailable,
  OutputUnavailable,
  SampleRateUnsupported,
  InvalidChannelMap,
  PermissionDenied,
  BackendFailure,
};

struct AudioRouteCompatibility {
  AudioRouteCompatibilityStatus status =
      AudioRouteCompatibilityStatus::BackendFailure;
  std::string resolved_input_device_id;
  std::string resolved_output_device_id;
  uint32_t requested_sample_rate = 0;
  uint32_t current_input_sample_rate = 0;
  uint32_t current_output_sample_rate = 0;
  bool input_rate_change_required = false;
  bool output_rate_change_required = false;
  bool input_is_running_somewhere = false;
  bool output_is_running_somewhere = false;
  std::string detail;
};
```

`AudioDriverConfig::sample_rate_policy` is appended after the existing
`channel_map` member and defaults to `PreserveDeviceRate`. The existing
positionally initialized source form remains valid after recompilation:

```cpp
orpheus::AudioDriverConfig legacy{
  48000, 512, 0, {}, 2, {}, {}, {{}, {0, 1}}
};
```

`IAudioDriver::probeRoute(const AudioDriverConfig&) const` is an appended
non-pure virtual. Its base implementation returns `BackendFailure`, so
WASAPI and custom/pre-existing derived drivers retain a valid default without
a probe override. Only CoreAudio and the dummy driver override it.

This is source compatibility after recompilation, not a binary-layout or
binary-vtable compatibility guarantee. The new configuration member changes
object layout, and the appended virtual changes the virtual table for newly
compiled consumers. Existing derived-driver fixtures intentionally omit a
probe override to exercise the source contract.

## 3. Resolver contract

The private CoreAudio implementation consists of
`coreaudio_route_resolver.{h,cpp}` and a read-only `ICoreAudioRouteQuery` seam.
The production query uses only `AudioObjectGetPropertyData` and
`AudioObjectGetPropertyDataSize`. It owns no AudioUnit, aggregate, listener,
property-set, TCC, or driver-state handle.

`CoreAudioDriver` keeps its ordinary default constructor and has a private
header-only constructor accepting a fake query for deterministic tests. The
fake is injected only into the private implementation seam; no query type is
installed or exposed through `IAudioDriver`.

### 3.1 Active directions and precedence

A direction is active only when its logical channel count is nonzero. Output
resolution is always attempted before input resolution. An empty active UID
selects only that direction's current default. A non-empty UID selects only
that exact persistent `kAudioDevicePropertyDeviceUID`; an unknown or
direction-incompatible UID never falls back to a default.

Inactive input does no input work. Its UID and channel map are ignored, and all
public input result fields remain empty, zero, or false. In particular, no
input endpoint, channel, range, current-rate, running-state, permission,
listener, aggregate, AUHAL, or TCC operation is made.

The first applicable classification is fixed:

| Order | Condition | Result |
| ---: | --- | --- |
| 1 | `num_outputs == 0` | `OutputUnavailable` |
| 2 | `sample_rate == 0` or `buffer_size == 0` | `BackendFailure` |
| 3 | Resolve active output | `OutputUnavailable`, `PermissionDenied`, or `BackendFailure` according to the query result |
| 4 | Resolve active input, only when `num_inputs > 0` | Direction-mapped unavailable, permission, or backend failure |
| 5 | Validate output then input maps after both active endpoints resolve | `InvalidChannelMap` |
| 6 | Query advertised ranges, output then input | `SampleRateUnsupported`, `PermissionDenied`, or `BackendFailure` |
| 7 | Query current nominal rate and running state, output then input | `PermissionDenied` or `BackendFailure` |
| 8 | Classify the complete facts | `Compatible` or `RequiresSampleRateChange` |

Endpoint unavailability precedes malformed maps. Unsupported rates stop before
current-rate or running-state queries. A confirmed authorization failure maps
to `PermissionDenied`; an unclassified CoreAudio read failure maps to
`BackendFailure`. No authorization-request API is called to manufacture a
permission result.

The resolver sets `resolved` only after every active endpoint resolves and
every active channel map validates. Both `Compatible` and
`RequiresSampleRateChange` are therefore activation-eligible. Unsupported-rate
and failure statuses are not.

### 3.2 Maps, rates, and shared devices

The stateless channel-map helper is shared by probing and activation:

- an empty active map becomes consecutive physical channels beginning at zero;
- a non-empty map must have exactly the logical channel count;
- entries must be unique; and
- entries must be within the resolved physical direction's channel count.

Nominal CoreAudio `Float64` rates are accepted only when finite, positive, no
greater than `uint32_t` max, and within `0.001 Hz` of an integer. Only the
rounded value is published. Advertised ranges are usable only when both bounds
are finite, positive, and ordered. An integer request is supported inclusively
when it is within `0.001 Hz` of either bound.

For a same-device duplex route, the global advertised ranges, current nominal
rate, and running-state fact are read once for the shared physical device and
copied to both directional result fields. Directional channel counts remain
scope-specific. The fake ledger asserts the one-read invariant.

The final successful classification is:

| Requested rate | Current rate | Policy | Result |
| --- | --- | --- | --- |
| Unsupported | Either | Either | `SampleRateUnsupported` |
| Supported | Equals request | Either | `Compatible` |
| Supported | Differs | `PreserveDeviceRate` | `Compatible`, with per-direction mismatch flags |
| Supported | Differs | `RequestExactRate` | `RequiresSampleRateChange`, with per-direction mismatch flags |

`sample_rate_policy` affects classification only in ORP172. It authorizes no
rate write, rollback, listener sequencing, or monitoring behavior. Successful
results have an empty `detail`. Failure details are bounded ASCII tokens in
the form `<operation>:<input|output>`, at most 96 bytes, with no OS error text
appended.

## 4. Activation boundary

`CoreAudioDriver::initialize()` now consumes the resolver's already-resolved
physical IDs and maps. It accepts only `resolved` routes whose status is
`Compatible` or `RequiresSampleRateChange`. It still performs the existing
activation-side AUHAL setup and nominal-rate behavior only after this boundary.

A private aggregate remains an activation concern and is created only for
resolved, distinct active input/output devices. The probe itself cannot create
one. FTR078 must address the requested-rate transaction and rollback semantics
before FourTrack treats `RequiresSampleRateChange` as an adoption-safe runtime
outcome.

## 5. Informational endpoint running state

`AudioEndpointCapabilities` and the private CoreAudio endpoint facts now carry
`is_running_somewhere`. The catalog projection preserves the value, and the
manager reads `kAudioDevicePropertyDeviceIsRunningSomewhere` for informational
enumeration.

The catalog value is not used as the probe's live fact. `probeRoute()` reads the
same CoreAudio property through its private query seam. In both places, the
value means only that CoreAudio reports the endpoint running somewhere. It is
not a process-ownership, microphone-use, TCC, or Bluetooth-input reliability
guarantee. A failed catalog read leaves the field false and does not alter
endpoint availability.

## 6. Verification record

### 6.1 Implementation and deterministic contracts

The final Debug route targets rebuilt successfully:

```text
cmake --build --preset sdk-debug --target \
  coreaudio_route_probe_test coreaudio_driver_test \
  driver_manager_test dummy_driver_test
```

The focused unsanitized CTest command passed all three executables and all 60
GoogleTest cases:

```text
ctest --test-dir build-sdk-fast \
  -R '^(coreaudio_route_probe_test|driver_manager_test|dummy_driver_test)$' -VV
```

Observed case totals:

- `dummy_driver_test`: 21 passed;
- `coreaudio_route_probe_test`: 12 passed; and
- `driver_manager_test`: 27 passed, including three endpoint-catalog cases.

The route-probe cases cover same-device global-read deduplication, both rate
policies, explicit UID no-fallback and direction mismatch, output-only
inactive-input suppression, static-precedence rejection, map validation,
unsupported-rate query short-circuiting, passive query failures, permission
short-circuiting, malformed nominal rates, activation aggregate eligibility,
and driver-state immutability. The dummy cases cover valid/unavailable/invalid
probe results and unchanged state.

### 6.2 Consumer and package contracts

Each external fixture compiled and ran the exact legacy aggregate with the
`PreserveDeviceRate` default:

- installed `find_package` fixture: 13/13 tests passed;
- installed package-runtime consumer: 1/1 test passed; and
- `add_subdirectory` consumer: configured and built successfully; its smoke
  executable returned success.

The recompiled `TelemetryDriver` fixture has no probe override and passed,
which proves the intended post-cutover source contract for a pre-existing
derived driver.

### 6.3 Full configured unsanitized suite

The complete unsanitized `sdk-fast` suite completed with the following JUnit
summary:

```text
ctest --test-dir build-sdk-fast --output-junit /tmp/orp172-sdk-fast.xml
 tests=63 failures=0 disabled=0 skipped=0
```

The realtime static audit passed in both the focused unsanitized run and the
Debug build. The complete configured SDK Debug CTest invocation was also
attempted:

```text
ctest --test-dir build-sdk-debug --output-on-failure
```

On this macOS environment, `conformance_json` timed out at its 300-second test
limit without producing test output, CTest started `orpheus_tests`, and the
1800-second command deadline elapsed. No complete Debug CTest pass is claimed.
The Debug targets compiled successfully; the passing suite evidence above is
from the unsanitized configured tree.

No physical-hardware or permission-denial evidence was claimed. The route
compatibility proof is deterministic fake-query/property-projection coverage,
which is the only safe evidence for the non-mutating boundary without a
separate hardware and TCC test fixture.

## 7. FourTrack follow-up

FTR078 may adopt this API only after it is independently merged and qualified.
The downstream sequence is:

1. update the FourTrack SDK submodule to the immutable ORP172 revision or a
   later released SDK package;
2. call `IAudioDriver::probeRoute()` from a control thread before replacing the
   live engine;
3. present resolved directional UIDs, current rates, mismatch flags, running
   facts, and bounded failure details without treating `is_running_somewhere` as
   ownership or permission proof;
4. retain the live engine when the probe is not activation-eligible;
5. use `RequiresSampleRateChange` only with FTR078's verified rate transaction,
   rollback, monitoring, and runtime-outcome contract; and
6. keep all CoreAudio object access and route mutation in the SDK rather than
   reimplementing it in FourTrack.

ORP172 does not claim that the current FourTrack engine can safely adopt a
sample-rate mismatch, prove microphone authorization, or replace the live
engine without FTR078's remaining work.
