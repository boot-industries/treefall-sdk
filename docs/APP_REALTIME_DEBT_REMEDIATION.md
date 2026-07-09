# App Realtime Debt Remediation

This note is for amending adjacent Orpheus applications after the SDK
realtime-audit hardening branch lands. It does not require SDK code changes in
those repos beyond consuming the public preparation/capability contracts added
here.

Run from `~/dev/orpheus-sdk`:

```sh
python3 tools/realtime_audit.py --root ~/dev/orpheus-sdk --include-adjacent
```

The current app-specific debt is:

- `~/dev/clip-composer/Source/Audio/AudioEngine.cpp`: callback-local analyzer
  wrapping/processing around `juce::AudioBuffer<float> tempBuffer`.
- `~/dev/fourtrack/src/fourtrack/engine/engine.cpp`: file-backed
  `readSamples()` calls from `Engine::processAudio`.

## Clip Composer

### Problem

`AudioEngine::processAudio()` renders transport output and then runs shmui
analysis on the audio callback. The current pattern constructs a JUCE buffer
wrapper and calls `AudioAnalyzer::processBlock()` before updating meter arrays.
Even if the wrapper itself is cheap, analyzer work belongs off the realtime
thread because it can grow over time, hide locks/allocation, and compete with
small-buffer playback.

### Target Architecture

- Keep `AudioEngine::processAudio()` limited to transport rendering, silence
  fallback, and fixed-size telemetry copies.
- Add a preallocated single-producer/single-consumer telemetry ring owned by
  `AudioEngine`.
- In the audio callback, copy or downsample only the minimum samples needed for
  UI meters into the ring. If the ring is full, drop the telemetry block.
- Drain the ring from the existing message-thread/UI timer and run
  `m_audioAnalyzer->processBlock()` there.
- Update `m_rmsLevels` and `m_peakLevels` only from the message thread, or
  publish them through atomics if audio-thread reads are required.

### Implementation Steps

1. Add fixed-capacity telemetry storage sized for the maximum expected callback:
   channel count, frame count, and a small number of blocks.
2. Replace callback-side `juce::AudioBuffer<float> tempBuffer(...)` and
   `m_audioAnalyzer->processBlock(tempBuffer)` with a nonblocking copy into the
   telemetry ring.
3. Add `AudioEngine::drainAnalyzerTelemetry()` and call it from the same
   message-thread path that already drains transport callbacks.
4. In `drainAnalyzerTelemetry()`, wrap the copied telemetry block in a JUCE
   buffer and call the shmui analyzer outside the audio callback.
5. Keep overflow observable with an atomic dropped-telemetry counter, but do not
   log from the callback.

### Acceptance Checks

- `AudioEngine::processAudio()` contains no `AudioAnalyzer::processBlock`, no
  callback-local analyzer wrapper, no heap allocation, and no logging.
- UI meters continue to update at timer cadence; occasional telemetry drops are
  acceptable under load.
- `python3 ~/dev/orpheus-sdk/tools/realtime_audit.py --root ~/dev/orpheus-sdk --include-adjacent`
  no longer reports Clip Composer app debt.

## FourTrack

### Problem

`Engine::processAudio()` reads track files directly through
`readers_[i]->readSamples(...)`. Those readers are backed by the SDK audio-file
reader, which can decode and perform blocking file I/O. FourTrack already moved
recording writes to a worker queue; playback needs the same separation.

### Target Architecture

- Keep realtime playback reading only from preallocated per-track PCM ring
  buffers.
- Move file reader ownership, `readSamples()`, seeking, and EOF/loop fill logic
  to a playback-stream worker.
- Maintain a per-track stream state:
  playhead position, desired seek position, EOF state, available frames, and
  underrun counter.
- On `play`, `seek`, or track reload, pause or reset the stream worker, seek the
  reader on the worker thread, prefill each active track ring, then let the audio
  callback consume from the rings.
- If a playback ring underruns, the callback emits silence for that track,
  increments an atomic underrun counter, and continues.

### Implementation Steps

1. Introduce a `PlaybackStream` per track with a fixed-size float ring buffer
   and atomics for read/write cursors.
2. Move `open_reader()`, reader `seek()`, and reader `readSamples()` calls into
   a worker thread or existing non-realtime transition section.
3. During `prime_readers()` or `play`, prefill enough frames for the lowest
   supported buffer size target. Start with at least 4 audio callbacks worth of
   ring capacity per track; tune upward after stress tests.
4. In `Engine::processAudio()`, replace `readers_[i]->readSamples(...)` with a
   nonblocking pop from the track ring into `channel_scratch_[i]`.
5. Ensure `with_audio_paused()` coordinates stream reset/seek operations so the
   callback never touches reader objects.
6. Keep offline bounce using direct file readers; that path is not realtime.

### Acceptance Checks

- `Engine::processAudio()` contains no `readSamples()`, no reader `seek()`, and
  no reader-object access except ring-buffer state owned for realtime use.
- Recording still uses the existing writer queue; playback underruns are counted
  and rendered as silence, not blocking recovery.
- Latency transition tests still pass, and a new playback-stream stress test
  covers seek, play/stop, EOF, and simultaneous four-track playback.
- The adjacent realtime audit no longer reports FourTrack app debt.

## SDK Hooks To Use

- Use `ITransportController::registerClipAudio()` instead of casting to the
  concrete SDK transport where possible.
- Call `ITransportController::prepareClipAudio()` after registration or metadata
  changes and before latency-critical playback.
- Query `IAudioDriver::getCapabilities()` instead of parsing driver names when
  choosing channel counts, buffer sizes, or platform-specific options.
- Use `RealtimeDiagnostics` or app-local atomics for callback counters; run
  heavier timing and histogram work outside production callbacks.
