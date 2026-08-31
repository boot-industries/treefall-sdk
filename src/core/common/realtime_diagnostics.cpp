// SPDX-License-Identifier: MIT
#include "realtime_counter.h"
#include <orpheus/realtime_diagnostics.h>
namespace orpheus {

void RealtimeDiagnostics::recordCallback(uint32_t bufferFrames, uint32_t sampleRate,
                                         uint64_t callbackDurationNs) {
  detail::publishSaturatingIncrement(callback_count_);
  detail::publishSaturatingAdd(samples_processed_, bufferFrames);
  last_buffer_frames_.store(bufferFrames, std::memory_order_relaxed);
  last_sample_rate_.store(sampleRate, std::memory_order_relaxed);

  if (callbackDurationNs == 0 || sampleRate == 0) {
    return;
  }

  const uint64_t bufferDurationNs =
      (static_cast<uint64_t>(bufferFrames) * 1'000'000'000ull) / sampleRate;
  const uint64_t quarterBudgetNs = bufferDurationNs / 4u;
  const uint64_t halfBudgetNs = bufferDurationNs / 2u;

  if (callbackDurationNs > quarterBudgetNs) {
    detail::publishSaturatingIncrement(callback_over_budget_count_);
  }
  if (callbackDurationNs > halfBudgetNs) {
    detail::publishSaturatingIncrement(callback_p99_over_budget_count_);
  }
}

void RealtimeDiagnostics::reportUnderrun() {
  detail::publishSaturatingIncrement(underrun_count_);
}

void RealtimeDiagnostics::reset() {
  callback_count_.store(0, std::memory_order_relaxed);
  samples_processed_.store(0, std::memory_order_relaxed);
  underrun_count_.store(0, std::memory_order_relaxed);
  callback_over_budget_count_.store(0, std::memory_order_relaxed);
  callback_p99_over_budget_count_.store(0, std::memory_order_relaxed);
  last_buffer_frames_.store(0, std::memory_order_relaxed);
  last_sample_rate_.store(0, std::memory_order_relaxed);
}

RealtimeDiagnosticsSnapshot RealtimeDiagnostics::snapshot() const {
  RealtimeDiagnosticsSnapshot snap;
  snap.callback_count = callback_count_.load(std::memory_order_relaxed);
  snap.samples_processed = samples_processed_.load(std::memory_order_relaxed);
  snap.underrun_count = underrun_count_.load(std::memory_order_relaxed);
  snap.callback_over_budget_count = callback_over_budget_count_.load(std::memory_order_relaxed);
  snap.callback_p99_over_budget_count =
      callback_p99_over_budget_count_.load(std::memory_order_relaxed);
  snap.last_buffer_frames = last_buffer_frames_.load(std::memory_order_relaxed);
  snap.last_sample_rate = last_sample_rate_.load(std::memory_order_relaxed);
  return snap;
}

} // namespace orpheus
