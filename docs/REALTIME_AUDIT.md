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
- Realtime entry points validate pointer arrays, exact configured channel
  counts, and bounded frame counts before draining commands or writing output.
  Invalid shapes are no-touch returns; a valid zero-frame block still advances
  bounded control state.
- Any-thread pending observations are clamped to queue capacity, and cumulative
  sequence/drop diagnostics saturate rather than wrap.
- Registered transport sources remain pinned by unread commands, active voices,
  and pending streaming page primes. Stop, replacement, panic, natural end, and
  teardown release each ownership pin exactly once.
- Borrowed callback targets are admitted with a single strong CAS. Control-side
  replacement closes admission, drains leases, and only then destroys the old
  target; callbacks do not wait, allocate, or notify waiters.
- Callback timing diagnostics are opt-in and default OFF. When enabled, the
  monitor lease remains held through timestamp conversion and publication.

## Boundary and Lifetime Gates

- `transport_controller_test`, `routing_matrix_test`, and `audio_input_test`
  cover malformed no-touch boundaries, bounded observations, saturating
  counters, and checked input-capacity arithmetic.
- `realtime_borrowed_target_test` covers admission, replacement, and lease
  draining without a retry loop.
- The WASAPI fake runtime covers terminal worker outcomes, rollback, stop/join,
  exact output-channel negotiation, and explicit reinitialization.


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

- macOS: CoreAudio is the only shipped production device backend and must
  remain the strictest callback-safety gate.
- Windows: WASAPI remains source/fake-test capable but unpromoted until package,
  ABI, and physical-device evidence exists. ASIO is optional source-only
  integration requiring an external SDK and is excluded from install/export.
- Linux: Dummy is the only advertised driver. ALSA, JACK, and PipeWire remain
  distinct future providers rather than one generic Linux backend.
- iOS: RemoteIO remains a future provider and should share the same callback and
  preparation contracts, with platform-specific capability limits exposed
  through `AudioDriverCapabilities`.
