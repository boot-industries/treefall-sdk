# ORP129 Variable-Rate and Reverse Scrub Reader SDK Implementation

**Status:** Implemented — branch `feat/orp-scrub-reader`, PR open against `main`
**Author:** ORP129 scrub-reader sprint, 2026-07-09
**Scope:** SDK audio I/O (`include/orpheus/audio_file_reader.h`, `src/core/audio_io/`) and DSP (`include/orpheus/dsp/`)
**Related:** FTR018 (inbound handoff spec, FourTrack), FTR017 (jog-wheel consumer), ORP127 (voice/SRC sprint — PolyphaseResampler), ORP128 (CoreAudio SR resilience)

---

## Summary

Implements the two primitives requested by the FourTrack FTR018 handoff so the
SDK can serve an audible, time-varying, forward-**and-backward** scrub (tape/CDJ
jog), lifting the capability out of FourTrack's local stopgap
(`fourtrack::dsp::ScrubResampler`) and into the shared SDK per the "fix at the
source" convention:

1. **§1 — backward/windowed read** (`IAudioFileReader::readSamplesEndingAt`): the
   primary ask. A reader can now return a window of frames *ending at* an
   arbitrary position, in forward order, for the caller to play out in reverse —
   removing the reverse-depth bound of a fixed local history ring.
2. **§2 — variable-rate + reverse resampler** (`orpheus::ScrubResampler`): a
   per-track, RT-safe, linear-interpolating resampler at an arbitrary, mutable,
   **signed** rate, with persistent fractional phase for click-free rate changes.

Both are host-neutral, dependency-free, and built into `Orpheus::audio_utils`
(alongside `PolyphaseResampler`). No existing consumer changed behavior.

## §1 — `readSamplesEndingAt` (backward/windowed read)

```cpp
// IAudioFileReader — DEFAULTED virtual (not = 0).
virtual Result<size_t> readSamplesEndingAt(int64_t end_sample, float* buffer,
                                           size_t num_frames);
```

- Fills `buffer` with the frame window `[end_sample - num_frames + 1, end_sample]`
  in **forward** (ascending-index) order; the caller emits it in reverse.
- Underflow below index 0 clamps to the in-range trailing frames; the returned
  count reflects only the frames actually read.
- **Restores** the read position observed on entry, so it composes with ordinary
  forward `readSamples()` streaming.

**Defaulted, not pure — a hard constraint.** A pure-virtual `= 0` would
transitively break every existing `IAudioFileReader` implementation
(`AudioFileReaderLibsndfile`, `AudioFileReaderExtended`,
`ResamplingAudioFileReader`) and Clip Composer, plus their tests. The base class
therefore ships a portable default (`src/core/audio_io/audio_file_reader.cpp`,
always built) implemented purely on the existing `seek` + `readSamples`
primitives, so every reader gets reverse for free.
`AudioFileReaderLibsndfile` overrides it with a single locked
seek/read/restore directly on the sndfile handle.

## §2 — `ScrubResampler` (variable-rate + reverse)

`include/orpheus/dsp/scrub_resampler.h` + `src/core/audio_io/scrub_resampler.cpp`.
Ported from `fourtrack::dsp::ScrubResampler` (the FTR017 reference) with a
namespace change. Two ways to drive it, sharing one ring and one continuity
guarantee:

- **Ring model** (`push_forward` + `render`) — the reference/tested API. `rate < 0`
  walks a persistent fractional cursor **down** through buffered history; reverse
  replays forward-decoded samples. Reverse depth is ring-bounded; pair with §1 to
  remove the bound.
- **Per-call model** (`processVariable`) — the FTR018 §2 shape:

  ```cpp
  size_t processVariable(const float* input, size_t in_frames,
                         float* output, size_t out_n, double rate);
  ```

  Feeds `input` as fresh forward history then renders `out_n` frames at `rate`,
  carrying the fractional phase across calls. `input` may be null / `in_frames` 0
  to render from existing history while reversing or holding.

**RT-safety:** all storage is allocated in `prepare()`; no other method
allocates, locks, or does I/O.

**Scope boundary (honored):** `ScrubResampler` is a **pure varispeed** resampler
— rate changes pitch and duration together, exactly as jogging a reel does.
Independent pitch-shift / time-stretch is deliberately **out of scope**, so a
pitch stage can compose downstream later. Linear interpolation ships; a
cubic/Hermite kernel over the same ring is a drop-in fidelity upgrade that does
not change the interface.

## Tests

Ported the FTR018 contract suite into the SDK, all green:

| Case | Where |
|------|-------|
| pass-through 1.0×, fractional 0.5× / 2.0× | `scrub_resampler_test` |
| reverse from mid-ring −1.0× (descending ramp) | `scrub_resampler_test` |
| hold at 0.0 (cursor frozen) | `scrub_resampler_test` |
| reverse/forward past buffered edge → silence (never OOB) | `scrub_resampler_test` |
| ring-wrap keeps absolute indexing correct | `scrub_resampler_test` |
| `processVariable` pass-through + cross-call phase + signed reverse | `scrub_resampler_test` |
| `readSamplesEndingAt` forward-order / position-restore / underflow-clamp / guards / reverse-emit (base default) | `reverse_read_test` |
| `readSamplesEndingAt` libsndfile override over a temp WAV | `reverse_read_test` (gated on `ORPHEUS_HAVE_SNDFILE`) |

`scrub_resampler_test` links only `orpheus_audio_utils` (host-neutral, no
libsndfile), matching `polyphase_resampler_test`.

**Full suite:** 122 tests, all functional tests pass. The lone red on the run
machine — `WaveformProcessorTest.PerformanceTest10MinuteWav` — is a hardcoded
2000 ms wall-clock assertion that flakes on either side of the threshold on
pristine `main` (measured 1985–2049 ms across repeated runs under Debug+ASan),
touches no code in this change, and is not a regression.

## Consumer impact

None. The defaulted virtual and the new DSP class are additive; the full SDK and
its 122-test suite build and pass unchanged. Per FTR018's migration plan,
FourTrack can later swap its local `ScrubResampler` for `orpheus::ScrubResampler`
behind the same engine seam and drive reverse via §1 — **not done here** (no pin
bump, no FourTrack edits).

## Spec friction / deviations from FTR018

- **§1 signature:** the handoff sketched `readSamplesEndingAt(...) = 0`. Shipped
  as a **defaulted** virtual (constraint: pure-virtual breaks existing
  consumers). Behavior is otherwise as specified.
- **§2 shape:** the handoff named the `processVariable(input, output, rate)`
  convenience form *and* referenced the ScrubResampler push/render/ring reference.
  Both are exposed: the ring API is preserved (it is what the ring-wrap /
  reverse-from-history contract tests exercise), and `processVariable` layers on
  it so the two paths share one continuity guarantee rather than duplicating the
  interpolation.
- **Ownership note (deferred):** FTR018 suggests the ring could migrate to a
  reader decorator paired with §1. This sprint keeps the ring caller-owned (as in
  the reference) and ships §1 as the on-demand backward primitive; wiring a
  decorator that internally drives §1 is a natural follow-up, not required to
  unblock the consumer.

## Files

```
 include/orpheus/audio_file_reader.h                |  35 +   (§1 defaulted virtual)
 include/orpheus/dsp/scrub_resampler.h              | 141 +   (§2 class + factory)
 src/core/audio_io/audio_file_reader.cpp            |  72 +   (§1 base default)
 src/core/audio_io/audio_file_reader_libsndfile.*   |  59 +   (§1 override)
 src/core/audio_io/scrub_resampler.cpp              | 122 +   (§2 impl)
 src/core/audio_io/CMakeLists.txt                   |   8 +
 tests/audio_io/reverse_read_test.cpp               | 249 +   (§1 suite)
 tests/audio_io/scrub_resampler_test.cpp            | 281 +   (§2 suite)
 tests/audio_io/CMakeLists.txt                      |  51 +
```

## Navigation

- **Repo:** orpheus-sdk
- **Docs:** `docs/orp/`
- **Branch:** `feat/orp-scrub-reader`
