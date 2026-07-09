// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace orpheus {

// -----------------------------------------------------------------------------
// ScrubResampler — a real-time-safe, per-track variable-rate + reverse reader.
//
// ORP128 / FTR018. This is the SDK home for the audible jog-wheel scrub engine
// FourTrack built locally (fourtrack::dsp::ScrubResampler) and handed upstream.
// PolyphaseResampler cannot serve it: its ratio is fixed at construction and its
// process() allocates per call. ScrubResampler drives an arbitrary, time-varying
// rate (which may be negative) with no allocation, lock, or I/O after prepare().
//
// SCOPE. This is a *pure varispeed resampler*: rate changes pitch and duration
// together (tape/CDJ scrub), exactly as jogging a reel does. Independent
// pitch-shift / time-stretch is deliberately OUT OF SCOPE — processVariable()
// stays a plain resampler so a pitch stage can compose downstream later.
//
// TWO WAYS TO DRIVE IT.
//
//   (a) Ring model (push_forward + render) — the reference/tested API. Feed the
//       ring the forward-decoded samples the engine already reads at 1.0x, then
//       render() at a persistent fractional cursor. rate < 0 walks the cursor
//       DOWN through buffered history: reverse *replays* samples decoded forward,
//       because a plain file reader cannot stream backward. This is what the
//       FourTrack jog seam uses today. Its reverse depth is bounded by the ring;
//       lift that bound by pairing it with the backward-capable reader
//       (IAudioFileReader::readSamplesEndingAt, ORP128 §1).
//
//   (b) Per-call model (processVariable) — the convenience shape named by the
//       FTR018 handoff §2: hand a block of input and get out_n output frames at
//       `rate`, with the fractional phase carried across calls so rate changes
//       stay click-free. Implemented on top of the same ring, so both paths share
//       one code path and one continuity guarantee.
//
// REAL-TIME SAFETY. All storage is allocated in prepare() (control thread). No
// other method allocates, locks, or does I/O: render()/processVariable() only
// index a fixed vector and do float math, so they are safe in the audio callback.
// Continuity — and therefore click-free rate changes — comes from the cursor
// being *persistent* across buffers: the caller changes `rate` each buffer but
// never re-seeds the cursor mid-gesture, so the output stays C0-continuous
// (linear interp). The fidelity trade-off is mild high-frequency imaging at
// extreme rates, acceptable for a live monitoring gesture; a cubic/Hermite
// kernel is a drop-in upgrade that does not change this interface.
//
// OWNERSHIP. One instance per track, touched only on the audio thread after
// prepare(). Not thread-safe; a jog rate reaches the audio thread as an atomic
// set by the control thread, never through this object.
// -----------------------------------------------------------------------------

class ScrubResampler {
public:
  ScrubResampler() = default;

  /// Allocate the ring. Call once off the audio thread, never from the audio
  /// callback. `ring_capacity_frames` bounds the reverse depth (ring model).
  void prepare(size_t ring_capacity_frames);

  /// Clear buffered history and place the read cursor at `anchor_sample` (an
  /// absolute sample index). Used on jog begin (prime) and jog end (seek/hold).
  /// Cheap: no allocation. After reset the ring is empty; the first push_forward
  /// must supply the samples at and after `anchor_sample`.
  void reset(int64_t anchor_sample);

  /// Append `n` freshly-decoded forward mono samples whose absolute indices are
  /// [first_sample_index, first_sample_index + n). Samples are expected to be
  /// contiguous with, or overlapping, what is already held; a gap forward simply
  /// advances the newest index. Overruns the ring capacity by overwriting the
  /// oldest samples (that is the reverse-depth bound). No allocation.
  void push_forward(const float* mono, size_t n, int64_t first_sample_index);

  /// Produce `out_n` output samples at `rate` (the cursor advances by `rate` per
  /// output frame). Reads only from the ring: positions outside the buffered
  /// range yield 0.0 (clamped, no out-of-bounds). Returns `out_n` (always fills
  /// the buffer, padding with silence past the buffered edges). No allocation.
  size_t render(float* out, size_t out_n, double rate);

  /// Per-call variable-rate resample (FTR018 §2). Pushes `in_frames` of `input`
  /// as fresh forward mono history starting at the current newest index + 1, then
  /// renders `out_n` output frames at `rate` from the persistent cursor. `rate`
  /// may change every call and may be negative (reverse, replaying buffered
  /// history). No allocation: all scratch is reserved in prepare(). Returns the
  /// number of output frames produced (== out_n; edges beyond the buffered range
  /// are silence).
  ///
  /// This is the plain resampler shape the handoff names; it composes the ring
  /// (a) API rather than duplicating it, so both paths share one continuity
  /// guarantee. `input` may be null / `in_frames` 0 to render from existing
  /// history without feeding new samples (e.g. while reversing or holding).
  size_t processVariable(const float* input, size_t in_frames, float* output, size_t out_n,
                         double rate);

  /// How many forward source frames the engine should decode and push_forward()
  /// next buffer to keep the ring ahead of the cursor for `out_n` output frames
  /// at `rate`. Returns 0 when reversing or holding (rate <= 0) — reverse replays
  /// buffered history and needs no new reads. Forward: enough to cover the cursor
  /// advance plus a one-sample interpolation margin, capped to the ring capacity.
  [[nodiscard]] size_t forward_refill_hint(double rate, size_t out_n) const;

  /// The current fractional read position (absolute sample index). The engine's
  /// integer playhead is floor(position()).
  [[nodiscard]] double position() const {
    return cursor_;
  }

  /// Absolute index of the newest buffered sample (highest index held), or the
  /// last reset/push anchor even when empty.
  [[nodiscard]] int64_t newest_sample() const {
    return newest_sample_;
  }

  [[nodiscard]] size_t capacity() const {
    return ring_.size();
  }

private:
  // Fetch the buffered sample at absolute index `s`, or 0.0 if `s` is outside the
  // currently-held range. Audio-thread only; no bounds surprises.
  [[nodiscard]] float sample_at(int64_t s) const;

  std::vector<float> ring_;    ///< Pre-allocated ring of forward-decoded samples.
  size_t write_pos_ = 0;       ///< Next write slot in the ring (mod capacity).
  size_t filled_ = 0;          ///< Valid samples held (<= capacity()).
  int64_t newest_sample_ = -1; ///< Absolute index of the most recently pushed sample.
  double cursor_ = 0.0;        ///< Absolute fractional read position.
};

/// Factory: an owned, prepared ScrubResampler (FTR018 §2 createScrubResampler
/// shape). `ring_capacity_frames` bounds the ring-model reverse depth.
inline ScrubResampler createScrubResampler(size_t ring_capacity_frames) {
  ScrubResampler r;
  r.prepare(ring_capacity_frames);
  return r;
}

} // namespace orpheus
