# Realtime Audio Audit

This checklist is the first hardening gate for Orpheus SDK audio code. It is
about realtime safety and architecture contracts, not micro-optimizing DSP.

## Callback Rules

- No locks, sleeps, blocking file/network I/O, hidden logging, heap allocation,
  container growth, or `std::function` ownership changes on realtime callbacks.
- Callback diagnostics must be opt-in and allocation-free. Production callbacks
  should record simple counters only.
- Audio-file open, decode setup, hashing, resampler setup, and cue/trim
  prewarming must happen before playback via non-realtime preparation APIs.
- Stop/shutdown must use explicit callback lifetime coordination. Fixed sleeps
  are not acceptable as a resource-safety boundary.

## Current Gates

- `ctest -R realtime_static_audit` scans driver callbacks for hard failures.
- `tools/realtime_audit.py --include-adjacent` also reports tracked debt in
  `~/dev/clip-composer`, `~/dev/fourtrack`, and `~/dev/freqfinder` when those
  repos are present beside the SDK.
- `tools/realtime_audit.py --fail-known-debt --include-adjacent` is the future
  strict gate after the streaming-reader and app telemetry migrations land.

## Known Architecture Debt

- The transport render path still calls file readers directly. `prepareClipAudio`
  gives hosts an explicit prewarm point, but the target architecture is a
  non-realtime streamer feeding realtime-owned clip buffers.
- The routing matrix now carries explicit `SourceChannelPolicy` and
  `DownmixPolicy`, but transport clip rendering still uses stereo pair buffers.
- Clip Composer still performs app-level analyzer work in its audio callback in
  the adjacent repo. The SDK-side target is decimated telemetry copied out of
  callback buffers and consumed by UI/message-thread code.
- See `docs/APP_REALTIME_DEBT_REMEDIATION.md` for Clip Composer and FourTrack
  patch guidance.

## Platform Matrix

- macOS: CoreAudio is the first low-latency backend and must remain the strictest
  callback-safety gate.
- Windows: WASAPI should expose shared/exclusive capability details. ASIO is
  optional because of external SDK constraints.
- Linux: ALSA, JACK, and PipeWire should report capabilities separately; do not
  silently flatten them into one generic Linux backend.
- iOS: RemoteIO should share the same callback and preparation contracts, with
  platform-specific capability limits exposed through `AudioDriverCapabilities`.
