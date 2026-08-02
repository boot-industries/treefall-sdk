// SPDX-License-Identifier: MIT
#pragma once

#include <atomic>
#include <cstdint>
#include <limits>

namespace orpheus::detail {

static_assert(std::atomic<uint64_t>::is_always_lock_free,
              "realtime counters require lock-free uint64_t atomics");

/// Saturating arithmetic for producer-owned cumulative realtime counters.
/// The caller owns serialization; this helper deliberately performs no CAS loop.
constexpr uint64_t saturatingAdd(uint64_t value, uint64_t amount) noexcept {
  constexpr uint64_t maximum = std::numeric_limits<uint64_t>::max();
  return amount > maximum - value ? maximum : value + amount;
}

constexpr uint64_t saturatingIncrement(uint64_t value) noexcept {
  return saturatingAdd(value, 1);
}

/// Publish one producer-owned cumulative counter with one relaxed load/store.
inline void publishSaturatingAdd(std::atomic<uint64_t>& counter, uint64_t amount) noexcept {
  const uint64_t value = counter.load(std::memory_order_relaxed);
  counter.store(saturatingAdd(value, amount), std::memory_order_relaxed);
}

inline void publishSaturatingIncrement(std::atomic<uint64_t>& counter) noexcept {
  publishSaturatingAdd(counter, 1);
}

} // namespace orpheus::detail
