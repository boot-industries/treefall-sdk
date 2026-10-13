// SPDX-License-Identifier: MIT
#pragma once

#include <atomic>
#include <cstdint>

namespace orpheus {

/// Value copy of independently sampled, eventually consistent callback counters.
struct RealtimeDiagnosticsSnapshot {
  uint64_t callback_count = 0;
  uint64_t samples_processed = 0;
  uint64_t underrun_count = 0;
  /// Observed durations strictly greater than one quarter of the buffer budget.
  uint64_t callback_over_budget_count = 0;
  /// Legacy name: observed durations strictly greater than half the buffer budget.
  /// This is an exceedance count, not a p99 percentile or percentile estimate.
  uint64_t callback_p99_over_budget_count = 0;
  uint32_t last_buffer_frames = 0;
  uint32_t last_sample_rate = 0;
};

/// Allocation-free counters for audio callbacks.
///
/// recordCallback() only performs relaxed atomic operations and arithmetic, so
/// it can be called from realtime threads. Percentile distributions belong in a
/// non-realtime profiler; these counters record threshold exceedances without
/// heap allocation, locks, file I/O, or std::function ownership on the callback.
/// Exactly one realtime producer may record/report at a time.
class RealtimeDiagnostics {
public:
  /// A zero duration means no timing observation, not an instantaneous callback.
  /// Thresholds use the integer buffer duration (frames * 1e9 / sampleRate),
  /// divided by four/two, with strict > comparisons. A zero sample rate records
  /// callback/frame counters but cannot evaluate a timing budget.
  void recordCallback(uint32_t bufferFrames, uint32_t sampleRate, uint64_t callbackDurationNs = 0);
  void reportUnderrun();
  /// Reset only after the realtime producer has stopped.
  void reset();

  /// Fields are independent atomic observations, not a same-moment publication.
  /// During production their callback counts, totals, and last-buffer fields may
  /// describe different instants. After the producer stops, all counters settle.
  [[nodiscard]] RealtimeDiagnosticsSnapshot snapshot() const;

private:
  std::atomic<uint64_t> callback_count_{0};
  std::atomic<uint64_t> samples_processed_{0};
  std::atomic<uint64_t> underrun_count_{0};
  std::atomic<uint64_t> callback_over_budget_count_{0};
  std::atomic<uint64_t> callback_p99_over_budget_count_{0};
  std::atomic<uint32_t> last_buffer_frames_{0};
  std::atomic<uint32_t> last_sample_rate_{0};
};

} // namespace orpheus
