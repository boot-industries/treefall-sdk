# ORP162 CoreAudio Capture Channel Mapping and Downstream Pin Handoff

**Date:** 2026-07-24  
**Status:** Delivered for SDK 0.6.7  
**Related:** Orpheus SDK PR #224, FourTrack PR #27, Clip Composer PR #17

## Problem

When CoreAudio joined distinct input and output endpoints into a private aggregate,
AUHAL capture channel zero could still refer to an input lane contributed by the
output sub-device. FourTrack then received silence from the wrong lane even though
the intended microphone endpoint was resolved correctly.

## Contract

`CoreAudioDriver` now:

- resolves the requested input and output DeviceUIDs independently;
- counts the resolved input endpoint's physical capture channels;
- rejects `AudioDriverConfig::num_inputs` when the endpoint cannot satisfy it;
- records the aggregate offset contributed by input-capable output hardware; and
- applies an AUHAL output-scope channel map on capture element 1 so host channel
  zero begins at the explicitly resolved input sub-device.

The map is configured during initialization. The audio callback remains free of
allocation, locks, and device-property queries.

## Release and package lineage

SDK 0.6.7 includes the CoreAudio correction and preserves the SDK 0.6.4–0.6.6
ShmUI-JUCE release lineage. The governed package remains pinned to design-token
contract 0.3.0, source revision `4614e3f588174ce012e3ed5997bea1caeea4ddda`,
and the stable `shmui::TextButton` component-identity behavior delivered in
0.6.6.

## Verification

- The merged SDK branch configures and builds `coreaudio_driver_test` on Apple
  Silicon with Apple Clang 17.
- FourTrack's macOS application and focused test targets build against the SDK
  fix.
- FourTrack reports 225/225 configured contracts passing.
- Physical MacBook microphone capture reaches the armed track and updates live
  Ribbon presence.

The standalone CoreAudio test executable did not complete in the local review
run, including test-list mode, so this record does not claim a fresh completed
SDK CoreAudio runtime suite. The downstream physical-device acceptance is the
behavioral proof for this hardware-dependent correction.

## Downstream pins

- FourTrack pins `third_party/orpheus-sdk` to the SDK 0.6.7 release commit.
- Clip Composer pins the same SDK release, retaining ShmUI-JUCE token contract
  0.3.0 and the shared horizontal `LevelMeter` used by its Master meter.
