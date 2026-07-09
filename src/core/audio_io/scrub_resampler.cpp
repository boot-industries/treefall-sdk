// SPDX-License-Identifier: MIT
//
// ORP128 / FTR018 §2 — variable-rate + reverse scrub resampler. Ported from
// fourtrack::dsp::ScrubResampler (FTR017), the working reference for this shape,
// with a namespace change and the per-call processVariable() convenience form
// layered on the same ring.

#include "orpheus/dsp/scrub_resampler.h"

#include <algorithm>
#include <cmath>

namespace orpheus {

void ScrubResampler::prepare(size_t ring_capacity_frames) {
  // The only allocation. Called off the audio thread, never the audio callback.
  ring_.assign(ring_capacity_frames, 0.0f);
  write_pos_ = 0;
  filled_ = 0;
  newest_sample_ = -1;
  cursor_ = 0.0;
}

void ScrubResampler::reset(int64_t anchor_sample) {
  // Empty the history and park the cursor. No allocation. The ring keeps its
  // capacity; filled_ = 0 means "nothing buffered yet" so the first push after a
  // reset defines the new contiguous run. newest_sample_ is set one below the
  // anchor so the first pushed sample (at anchor_sample) becomes newest.
  write_pos_ = 0;
  filled_ = 0;
  newest_sample_ = anchor_sample - 1;
  cursor_ = static_cast<double>(anchor_sample);
}

void ScrubResampler::push_forward(const float* mono, size_t n, int64_t first_sample_index) {
  if (ring_.empty() || n == 0 || mono == nullptr) {
    return;
  }
  const size_t cap = ring_.size();

  // If the incoming block is larger than the ring, only the last `cap` samples
  // can survive — skip the prefix we would immediately overwrite.
  size_t start = 0;
  if (n > cap) {
    start = n - cap;
  }
  for (size_t i = start; i < n; ++i) {
    ring_[write_pos_] = mono[i];
    write_pos_ = (write_pos_ + 1) % cap;
    if (filled_ < cap) {
      ++filled_;
    }
  }
  // The newest absolute index is that of the last sample we actually stored.
  newest_sample_ = first_sample_index + static_cast<int64_t>(n) - 1;
}

float ScrubResampler::sample_at(int64_t s) const {
  if (filled_ == 0) {
    return 0.0f;
  }
  const int64_t oldest = newest_sample_ - static_cast<int64_t>(filled_) + 1;
  if (s < oldest || s > newest_sample_) {
    return 0.0f; // Outside buffered history — clamp to silence (reverse depth bound).
  }
  const size_t cap = ring_.size();
  // Distance (in samples) back from the newest; newest lives just before write_pos_.
  const int64_t back = newest_sample_ - s; // 0 == newest
  const int64_t newest_slot = (static_cast<int64_t>(write_pos_) - 1 + static_cast<int64_t>(cap)) %
                              static_cast<int64_t>(cap);
  int64_t slot = (newest_slot - back) % static_cast<int64_t>(cap);
  if (slot < 0) {
    slot += static_cast<int64_t>(cap);
  }
  return ring_[static_cast<size_t>(slot)];
}

size_t ScrubResampler::render(float* out, size_t out_n, double rate) {
  if (out == nullptr) {
    return 0;
  }
  for (size_t i = 0; i < out_n; ++i) {
    // Linear interpolation between the two samples bracketing the cursor.
    const double base = std::floor(cursor_);
    const double frac = cursor_ - base;
    const int64_t i0 = static_cast<int64_t>(base);
    const float s0 = sample_at(i0);
    const float s1 = sample_at(i0 + 1);
    out[i] = static_cast<float>(s0 + (s1 - s0) * frac);
    cursor_ += rate;
  }
  return out_n;
}

size_t ScrubResampler::processVariable(const float* input, size_t in_frames, float* output,
                                       size_t out_n, double rate) {
  // Per-call convenience form (FTR018 §2). Treat `input` as fresh forward mono
  // history contiguous with what is already held (starting at newest_sample_ + 1),
  // then render from the persistent cursor. Both steps are alloc-free, so the
  // whole call is RT-safe once prepare() has run.
  if (input != nullptr && in_frames > 0 && !ring_.empty()) {
    push_forward(input, in_frames, newest_sample_ + 1);
  }
  return render(output, out_n, rate);
}

size_t ScrubResampler::forward_refill_hint(double rate, size_t out_n) const {
  if (rate <= 0.0 || out_n == 0) {
    return 0; // Reverse/hold replays buffered history; no new reads needed.
  }
  // Samples the cursor will consume this buffer, plus a 1-sample interpolation
  // margin (we read i0 and i0+1). Capped to the ring capacity so a single refill
  // can never exceed what the ring can hold.
  const double consumed = static_cast<double>(out_n) * rate;
  size_t need = static_cast<size_t>(std::ceil(consumed)) + 1;
  if (!ring_.empty()) {
    need = std::min(need, ring_.size());
  }
  return need;
}

} // namespace orpheus
