# ORP128 CoreAudio Runtime Sample-Rate Resilience

**Date:** 2026-07-29  
**Status:** Delivered in SDK PR #225

## Problem

A CoreAudio route can change its nominal sample rate after an audio unit starts.
Continuing to render against the stale configured format risks invalid host audio
I/O and makes the failure invisible to the control thread.

## Contract

`CoreAudioDriver` now monitors the nominal sample-rate property of every active
aggregate, output, and input route. Listener callbacks only publish an atomic
notification and close the render gate. A control worker reads and, when
possible, reasserts the configured rate before reopening that gate.

If the property cannot be queried or reasserted, the driver stops rendering and
reports `AudioDriverRuntimeOutcome::SampleRateQueryFailure` or
`AudioDriverRuntimeOutcome::SampleRateReinitializationRequired` through
`IAudioDriver::getTelemetry()`. Listener registration failure is reported as
`AudioDriverRuntimeOutcome::SampleRateListenerFailure`. A recovered route is
reported as `AudioDriverRuntimeOutcome::SampleRateRestored`.

The render gate uses a listener-generation compare-exchange. A notification that
arrives while the worker polls cannot be consumed by that poll; rendering remains
closed until a later poll validates the newly notified route.

## Package coverage

The clean-prefix `cmake_find_package` fixture compiles, links, and runs a
consumer of `AudioIoTelemetry`, `AudioDriverRuntimeOutcome`, and the default
`IAudioDriver::getTelemetry()` behavior through `Orpheus::audio_io`.

## Verification

- `coreaudio_sample_rate_monitor_test` covers listener registration, rate
  restoration, terminal refusal/query failures, and a notification delivered
  during the final property read.
- `coreaudio_driver_test` covers the CoreAudio driver integration and
  playback-only monitor startup smoke path.
- The specialized realtime audit remains the gate for ensuring listener
  notifications and render callbacks stay allocation- and lock-free.

## Hardware limitation

No controllable local physical route was available to record a 48 kHz-to-44.1
kHz transition during this delivery. Deterministic listener/property tests cover
the recovery and terminal paths; a physical-device acceptance remains useful
release evidence when such hardware is available.
