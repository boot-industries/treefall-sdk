<!-- SPDX-License-Identifier: MIT -->

# ORP156 — ORP155 Implementation Handoff

**Document type:** Engineering handoff
**Status:** Completed; pull request merged
**Branch:** `feat/orp154-fourtrack-contracts`
**Base:** `main` at `1ab644c6`
**Date:** 2026-07-16

---

## Objective

Finish every contract in [[ORP155 FourTrack Recorder Adoption Friction - CoreAudio and Routing Contracts]], verify the resulting SDK behavior, commit the branch, push it, and open a pull request to `main`. Do not merge the pull request.

## Worktree received from the prior session

The branch has one staged new document and eight unstaged implementation/test files:

- `docs/orp/ORP155 FourTrack Recorder Adoption Friction - CoreAudio and Routing Contracts.md` — restored from local branch `docs/orp154-fourtrack-handoff`; still marked **Proposed** and must become a truthful implementation record after verification.
- `include/orpheus/audio_driver.h`
- `src/core/audio_io/dummy_audio_driver.cpp`
- `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp`
- `src/platform/audio_drivers/coreaudio/coreaudio_driver.h`
- `src/platform/audio_drivers/driver_manager.cpp`
- `src/platform/audio_drivers/wasapi/wasapi_driver.cpp`
- `tests/audio_io/coreaudio_driver_test.cpp`
- `tests/audio_io/dummy_driver_test.cpp`

`git diff --check` passed at the last inspection. Do not discard the staged ORP155 document or the uncommitted contract work.

## Contract status

### Isolated routing channel meters

The checked-out routing implementation already appears to meet ORP155 §2:

- `RoutingMatrix::initialize()` preallocates `m_channel_meter_buffer`.
- `RoutingMatrix::processRoutingBlock()` fills this scratch with each channel's own post-gain/pan contribution before group accumulation.
- Existing coverage includes `ChannelMetersReportIsolatedEffectiveContributions`, `ChannelMetersPublishCurrentSilenceForMuteSoloAndNullInput`, and `GroupAndMasterMetersRetainSummedStageReadings`.

Do not duplicate the implementation. Confirm the tests cover the required combined gain/pan/order/mute and group/master-sum contract. Strengthen a test only if an observable ORP155 requirement is missing.

### Direction-specific duplex endpoint IDs

The intended public contract is `AudioDriverConfig::input_device_id` and `output_device_id`; there must be no `device_id` compatibility alias.

Current implementation work:

- CoreAudio interprets non-empty directional IDs as `kAudioDevicePropertyDeviceUID` values.
- An empty ID selects only that direction's current system default.
- CoreAudio validates each endpoint direction through stream configuration.
- Distinct endpoints create a private aggregate with output as clock master; drift compensation is enabled only for different clock domains.
- Aggregate creation failure returns `SessionGraphError::InvalidParameter`; no output-only fallback is permitted.
- Cleanup/reinitialize destroys a driver-owned aggregate.
- CoreAudio driver-manager enumeration now emits bare persistent DeviceUID values, not `coreaudio:<AudioDeviceID>`.
- The macOS manager factory accepts bare CoreAudio UIDs for all non-dummy devices.
- Dummy capability endpoint IDs use the new directional fields.
- WASAPI defaults only when `output_device_id` is empty; a non-empty unsupported ID returns `InvalidParameter`.

Review the cross-platform cutover before committing. The WASAPI source is not compiled on macOS, so specifically verify its declaration/control flow and any Windows configuration fixtures.

### Factory-visible capture-failure telemetry

The public `AudioIoTelemetry` and default `IAudioDriver::getTelemetry() noexcept` have been added. CoreAudio overrides this query, resets the counter during `initialize()`, and increments with a saturating CAS loop. The render callback retains its pre-zeroed input fallback after a failed `AudioUnitRender()`.

CoreAudio tests now use the factory-visible `IAudioDriver::getTelemetry()` query. The internal CoreAudio header exposes test-only helpers to set/increment the counter for deterministic saturation coverage. Dummy inherits the zero default.

## Required remaining test work

Finish CoreAudio endpoint lifecycle coverage:

1. Enumerate or derive distinct valid default input/output DeviceUIDs.
2. Initialize using those explicit UIDs, start capture/playback, and assert callbacks arrive with zero telemetry failures.
3. Confirm same-device and one-direction-default behavior where hardware permits; skip only when the host cannot provide the required physical topology.
4. Confirm unknown input and output UIDs independently return `InvalidParameter` without a default fallback or active aggregate.
5. Reinitialize the same driver with a distinct-endpoint configuration. Reuse of the deterministic private aggregate UID must succeed, proving prior aggregate cleanup.
6. Ensure the telemetry tests demonstrate factory visibility, successful-capture zero, successful-initialize reset, dummy zero, and saturation at `UINT64_MAX`.

A prior attempt to add a CoreAudio test helper was removed after being inserted into the wrong function. `tests/audio_io/coreaudio_driver_test.cpp` must be rebuilt and rerun before relying on earlier results.

## Verification record and required reruns

Observed before the final CoreAudio-test cleanup:

```sh
cmake --build build-ci --target \
  coreaudio_driver_test dummy_driver_test driver_manager_test routing_matrix_test \
  --parallel 8
ctest --test-dir build-ci --output-on-failure \
  -R '^(coreaudio_driver_test|dummy_driver_test|driver_manager_test|routing_matrix_test)$'
build-ci/tests/routing/routing_matrix_test
build-ci/tests/audio_io/driver_manager_test
```

The direct executable runs passed:

- `routing_matrix_test`: 39 tests
- `driver_manager_test`: 20 tests

The selected CTest invocation returned success and reported `coreaudio_driver_test` and `dummy_driver_test` passing. Its displayed output omitted the successful lines for the manager and routing tests; this was reported to the harness. Do not treat that partial display as substitute evidence: rerun the focused tests after completing the CoreAudio test work, preferably as direct executables as well.

Before delivery, run:

```sh
cmake --build build-ci --parallel 8
ctest --test-dir build-ci --output-on-failure
```

Resolve every branch-attributable failure. Use the repository's configured platform-specific checks if available. Run `git diff --check` before committing.

## Continuation verification

The continuation completed the CoreAudio endpoint lifecycle matrix on physical
hardware. Explicit distinct UIDs, same-device duplex, each one-direction-default
configuration, direction-incompatible UIDs, unknown UIDs, aggregate reuse, and
aggregate destruction all executed without a hardware-dependent skip.

Direct executable results:

- `coreaudio_driver_test`: 31 passed
- `dummy_driver_test`: 14 passed
- `driver_manager_test`: 20 passed
- `routing_matrix_test`: 39 passed

The complete `build-ci` tree built successfully. CTest passed 150/150, including
the realtime static audit, documentation path audit, version contract, installed
package consumers, and add-subdirectory checks. The Windows-only WASAPI source
and fixture were reviewed and updated, but this macOS run does not claim Windows
compile or execution evidence.

## Delivery record

- Commit: `feat(audio): add directional endpoint contracts`
- Branch: `feat/orp154-fourtrack-contracts`
- Pull request: [#218](https://github.com/chrislyons/orpheus-sdk/pull/218)
- Target: `main`
- Merge: `1b579f0f` into `main`

During merge preparation, `main` already contained
`ORP154 Sequencer Trigger Voice Primitive`. To preserve unique document IDs,
the FourTrack contract and its handoff were renumbered from ORP154/ORP155 to
ORP155/ORP156 before merging.

The synchronized merge result built successfully and passed 151/151 configured
CTest tests before PR merge.

## Documentation and delivery

After verification:

1. Update ORP155 from **Proposed** to a truthful implementation record with the exact changed contracts, tests, verification outputs, and hardware-dependent skips.
2. Add ORP155 and this handoff document to `docs/orp/INDEX.md` and `docs/orp/ORP.md` following existing index conventions.
3. Re-check public-header consumers for stale `device_id` usage and concrete-driver telemetry downcasts.
4. Commit using the repository convention, including `Co-Authored-By: Claude <noreply@anthropic.com>`.
5. Push `feat/orp154-fourtrack-contracts` and open a PR targeting `main`; do not merge it.
