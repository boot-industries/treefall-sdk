<!-- SPDX-License-Identifier: MIT -->

# ORP270 — Transport Initial Metadata Registration Delivery

**Document type:** Public API delivery and review record  
**Date:** 2026-10-19  
**Branch:** `feat/transport-initial-metadata-registration-main`  
**Status:** Merged to `main`; no release promotion and no support-tier change

## Decision and behavior

`ITransportController::registerClipAudioWithMetadata(handle, file_path,
metadata)` registers a clip source and its complete initial `ClipMetadata` in a
single control-thread operation. The source and the metadata are committed
together, the operation consumes no bounded render-command node, and no
audio-thread state is touched. Invalid metadata leaves the handle unregistered
and reports the same error codes `updateClipMetadata()` reports. The default
interface implementation returns `NotSupported`, so an implementation that
does not support the operation keeps compiling and truthfully reports that it
does not support it.

The method is intended for the initial configuration of a transport that has
not started its audio callback. It follows the same handle contract as
`registerClipAudio()`: a zero handle is rejected, and a handle that is already
registered returns `NotReady`. Metadata is validated against the opened file,
so the file's channel count and duration stay authoritative: a
`trimOutSamples` of `0` resolves to the file duration, and the supplied
speaker patch is validated against the file's channel count. The registered
DSP program is stored on the clip entry and prepared per voice when the clip
starts, so the registered program, fades, trims, segments, and routing take
effect at playback.

`updateClipMetadata()` remains the operation for live metadata changes. It
publishes a prepared processor through a render command so the audio thread
applies the change at a render boundary.

## Public interface and ABI posture

The new virtual is appended at the end of `ITransportController`, after
`getCommandIngressTelemetry()`. No existing virtual slot is renumbered, which
is the repository rule for an unavoidable virtual extension. The extension is
C++-only and additive: `include/orpheus/abi.h`, the `include/treefall/`
forwarding headers, and the C ABI 1.0 tables and layouts are unchanged, and
the SDK version stays 0.9.1.

An appended C++ virtual method is source-compatible only after consumers and
subclasses are rebuilt against the matching header. A prebuilt subclass must
not receive the new call. The installed clean-prefix fixtures
`legacy_transport_implementation.cpp` and `legacy_transport_defaults.cpp`
implement `ITransportController` without overriding the new method; they
continue to compile and link against the installed package, which is the
executable proof that the extension is additive.

## Review checkpoints

| Checkpoint | Verdict | Evidence |
| --- | --- | --- |
| R1 public interface extension | Pass | The virtual is appended after `getCommandIngressTelemetry()`; the default body returns `SessionGraphError::NotSupported`. Repository-wide reference analysis found three implementors: `TransportController` (overrides it) and the two legacy clean-prefix fixtures (do not, and still compile). Other references hold `ITransportController*` or `std::unique_ptr` and are not subclasses. |
| R2 C ABI stability | Pass | No file under `include/orpheus/abi.h` or `include/treefall/` changed. `tools/version_contract.py --check` passes at version 0.9.1. |
| R3 realtime safety | Pass | `registerClipAudioImpl` takes `m_audioFilesMutex`, mutates a local `AudioFileEntry`, and only then calls `m_audioFiles.emplace`. No `TransportCommand` node is acquired, so `getCommandIngressTelemetry().attemptedCount` is unchanged across registration. File open and resampling stay on the control thread. |
| R4 DSP application | Pass | The clip-source context created at start copies the registered fields and prepares the stored program: `context->dspProcessor.prepare(entry.dsp, m_sampleRate, entry.metadata.num_channels)`. Trims, fades, gain, pan, playback rate, play delay, loop, segments, routing group, voice mode, and the stop-fade envelope are copied from the same entry. |
| R5 refactor equivalence | Pass | `validateClipMetadataFields` and `applyClipMetadata` reproduce the previous inline `updateClipMetadata` behavior field by field: the same trim resolution, the same validation order, the same error codes (`InvalidClipTrimPoints`, `InvalidParameter`, `InvalidFadeDuration`), the same speaker-patch and group-width checks against the file channel count, the same publish ordering, and the same 22-field assignment set. `fileChannels` remains in use, so no unused-variable warning is introduced. |
| R6 registration atomicity and lifetime | Pass | Validation runs before `m_audioFiles.emplace`; on failure the local entry is destroyed, releasing the opened reader, the resampler decorator, and the source command lifetime, and the mutex is released by RAII. A second registration for the same handle returns `NotReady`. |
| R7 metadata round trip | Pass | `getClipMetadata` returns the registered fields, including the resolved `trimOutSamples` when the caller supplied `0`. The resolution, the handle contract, and the per-voice DSP preparation are now stated in the README. |
| R8 test quality | Pass | `ClipMetadataTest.RegistersFullInitialMetadataWithoutRenderCommands` asserts the rejected registration leaves the handle unregistered, that no render-command node is consumed, and that every metadata field round-trips. The clean-prefix fixture asserts the same observable contract against the installed package. Both fail if the capability regresses. |
| R9 realtime audit and packaging gates | Pass | The realtime static audit, the installed clean-prefix package gate, the documentation path audit, and the ShmUI-JUCE manifest check pass with the new method present. |

## Behavioral coverage

`tests/transport/clip_metadata_test.cpp` covers the rejected-registration
path, the absence of a render-command node, and the full metadata round trip.
`tests/cmake/find_package/transport_routing.cpp` repeats the registration,
read-back, and no-command assertions against the installed package through the
documented `Orpheus::` targets.

## Validation recipe

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DORPHEUS_ENABLE_REALTIME=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
ctest --test-dir build --output-on-failure -R '^cmake_find_package$'
ctest --test-dir build --output-on-failure -R '^realtime_static_audit$'
ctest --test-dir build --output-on-failure -R '^docs_path_audit$'
ctest --test-dir build --output-on-failure -R '^version_contract$'
python3 tools/shmui_juce_manifest.py --check
python3 tools/version_contract.py --check
```

## Verification

Local Debug build on macOS (AppleClang 21.0.0, CMake 4.1.0) configured with
`ORPHEUS_ENABLE_REALTIME=ON`. The full configured CTest suite and the
mandatory gates are recorded in this repository's `PROGRESS.md` entry for this
delivery. The clean-prefix package gate
(`cmake_find_package`, 22.42 s) passed with the new method and the new fixture
assertions in place.

## Provenance and limits

The change was authored outside this repository and re-created here by
cherry-pick, with `git patch-id --stable` proving the committed patch is
identical to the source commit. The source branch carried the work in a
repository where it was not merged, while a downstream consumer already called
the new API against an unmerged SDK revision. This delivery places the change
on `main` through review, and the downstream consumer is re-pinned to the
merged revision.

This record claims no release promotion, no support-tier change, and no
Windows or WASAPI evidence. WASAPI remains unpromoted source and fake-test
code, and the open WASAPI platform-qualification pull request is not merged by
this delivery.
