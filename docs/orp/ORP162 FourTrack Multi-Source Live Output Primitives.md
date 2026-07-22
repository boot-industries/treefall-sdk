# ORP162 FourTrack Multi-Source Live Output Primitives

**Status:** Implemented for Orpheus SDK `v0.6.4` on `feat/orp162-fourtrack-live-output` (2026-07-22)

**Related:** FTR050, FTR051, FTR052, ORP155, ORP156, ORP161

## Scope

ORP162 adds source-neutral live PCM delivery primitives for FourTrack's selected
stems and completed heard-master bus. The SDK copies and clocks only
caller-supplied PCM. It does not select buses, mix stems, own output routes,
create WebRTC sessions or virtual devices, or define application UI.

The installed `Orpheus::audio_utils` contract now includes:

- `include/orpheus/live_audio.h`: fixed-slot per-stream SPSC fan-out with
  complete-block drop semantics, independent stalled-consumer accounting,
  discontinuity propagation, live-edge discard, and monotonic status counters;
- `include/orpheus/streaming_sample_rate_converter.h`: factory-prepared,
  caller-buffered streaming conversion using the existing normalized-cutoff
  Blackman-windowed-sinc quality policy with 32 taps per phase; and
- `include/orpheus/clocked_output_bridge.h`: a bounded source-to-destination
  clock bridge with a fixed MPMC frame queue, fractional two-tap FIR phase,
  bounded fill correction, overflow-to-target behavior, and exact underflow
  silence.

FourTrack creates separate fan-outs for every selected stem and for heard
master, then attaches one bridge per enabled source/destination pair. The source
fan-out and stream outlive each bridge.

## Driver contract

`IAudioDriver` now reports `AudioDriverRuntimeInfo` and exposes a conservative
polling interface for fixed-size `AudioDriverEvent` records. Existing custom
drivers remain source-compatible through default virtual implementations.

CoreAudio resolves and reports the actual persistent directional UIDs, queries
physical endpoint buffer-frame ranges before allocation, allocates callback
storage for the negotiated maximum, and never truncates an oversized callback.
A callback beyond prepared capacity leaves CoreAudio output silent, skips the
host callback, emits one `CapacityExceeded` event when queue space permits, and
marks the next complete callback discontinuous. A fixed 64-slot sequence queue
accepts events from the render callback and property-listener serial queue;
queue saturation drops the newest event and increments `droppedEventCount()`.
Physical endpoint and directional-default listeners report route, format,
capacity, and removal changes without reinitializing the AudioUnit.

The dummy backend reports deterministic runtime information and provides a
non-exported synchronous variable-block test hook. Its oversize path follows the
same silence, event, and next-discontinuity contract without hardware or sleeps.
The released ORP155 `input_device_id` / `output_device_id` route contract is
unchanged; no `device_id` compatibility field was restored.

## Verification

The configured build completed. The seven ORP162 gates passed:

- `live_audio_fanout_test`;
- `streaming_sample_rate_converter_test`;
- `clocked_output_bridge_test`;
- `dummy_driver_test`;
- `coreaudio_driver_test`;
- `cmake_find_package`; and
- `realtime_static_audit`.

The bridge gate exercised deterministic 30-minute equivalent pacing for
−100, 0, and +100 ppm at 48→48, 44.1→48, and 48→44.1 kHz. Guarded fan-out,
converter, pump, and render calls reported zero allocation/deallocation
violations. The installed-prefix fixture included all three public headers,
drained known stereo PCM, and verified identity conversion without private
headers.

The complete configured CTest run executed 157 tests. After synchronizing the
`v0.6.4` version claims, `version_contract` passed; the previously reported
`gain_smoother_test` timing failure also passed in isolation. Four pre-existing
sanitizer-sensitive performance thresholds remained below their required host
rates when rerun serially: `OscillatorTest.ProcessesEfficiently`,
`PerformanceMonitorTest.PerformanceOfGetMetrics`,
`PerformanceIntegrationTest.OverheadMeasurement`, and
`MultiClipStressTest.RapidStartStop`. No CI/performance-threshold repair is part
of ORP162. All ORP162 functional, package, realtime-static, dummy, and CoreAudio
capacity/event gates passed.

The available macOS host proved default output UID reporting, maximum-frame
reporting, full delivery through capacity, oversize silence, ordered event
polling, stale UID rejection, and output-only routing. Input/distinct-endpoint
hardware cases remained skipped where the required physical topology was not
available; those skips are not evidence for unexercised routing combinations.
