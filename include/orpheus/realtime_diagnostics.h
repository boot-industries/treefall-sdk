// SPDX-License-Identifier: MIT
#pragma once

#include <atomic>
#include <cstdint>

namespace orpheus {

/// Immutable snapshot of realtime callback health.
struct RealtimeDiagnosticsSnapshot {
  uint64_t callback_count = 0;
  uint64_t samples_processed = 0;
  uint64_t underrun_count = 0;
  uint64_t callback_over_budget_count = 0;
  uint64_t callback_p99_over_budget_count = 0;
  uint32_t last_buffer_frames = 0;
  uint32_t last_sample_rate = 0;
};

/// Allocation-free counters for audio callbacks.
///
/// recordCallback() only performs relaxed atomic operations and arithmetic, so
/// it can be called from realtime threads. Percentile distribution still belongs
/// in a non-realtime profiler; these counters enforce budget guardrails without
/// heap allocation, locks, file I/O, or std::function ownership on the callback.
class RealtimeDiagnostics {
public:
  void recordCallback(uint32_t bufferFrames, uint32_t sampleRate, uint64_t callbackDurationNs = 0);
  void reportUnderrun();
  void reset();

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
