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

- `ctest -R realtime_static_audit` runs `tools/realtime_audit.py
  --fail-known-debt` — the STRICT in-repo gate. Since the ORP134 G1
  streaming-reader migration, the transport render path performs no file
  reads/seeks, so any reintroduced audio-thread read is a hard CI failure.
- The runtime side is enforced by `realtime_harness_test` (ORP136 §2.2):
  allocation hooks + `/proc/self/io` sampling prove zero allocations and zero
  media I/O across multi-clip `processAudio()` stress runs, and that streaming
  cache misses emit silence + `BufferUnderrun` instead of blocking.
- `tools/realtime_audit.py --fail-known-debt --include-adjacent` is the
  cross-repo strict gate. It still reports the ADJACENT-repo debt below and
  will pass once the app-side sprints land in those repos.

## Resolved Architecture Debt

- **The transport render path no longer calls file readers (ORP134 G1).**
  `prepareClipAudio()`/`startClip()` build an immutable realtime source on the
  control thread — whole-file PCM for short clips, a worker-fed page ring for
  long files — and `processAudio()` memcpy-reads it position-explicitly. Seek
  became a prefetch hint (no `sf_seek` anywhere near the callback); a
  streaming cache miss renders silence and reports `BufferUnderrun`, never
  blocking. Parity with the old reading path is proven bit-exact by the
  golden render-hash gate.

## Known Architecture Debt

- The routing matrix now carries explicit `SourceChannelPolicy` and
  `DownmixPolicy`, but transport clip rendering still uses stereo pair buffers.
- Clip Composer still performs app-level analyzer work in its audio callback in
  the adjacent repo. The SDK-side target is decimated telemetry copied out of
  callback buffers and consumed by UI/message-thread code. **→ OCC sprint.**
- FourTrack's engine still calls `readSamples` in its own callback in the
  adjacent repo; it should adopt the SDK's prepared/streaming sources.
  **→ FourTrack sprint.**
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
