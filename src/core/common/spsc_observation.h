// SPDX-License-Identifier: MIT
#pragma once

#include <atomic>
#include <algorithm>
#include <cstddef>
#include <utility>

namespace orpheus::detail {

struct NoopObservationHook {
  constexpr void operator()() const noexcept {}
};

/// Observe an SPSC pending count without exposing a cross-thread underflow.
/// Reading the consumer index first permits a producer/consumer publication to
/// be observed between the two loads; the capacity clamp preserves the public
/// bounded-diagnostic contract in that permitted overestimate.
template <typename ReadAtomic, typename WriteAtomic,
          typename BetweenLoads = NoopObservationHook>
size_t observeBoundedPending(const ReadAtomic& read_index, const WriteAtomic& write_index,
                             size_t capacity,
                             BetweenLoads between_loads = BetweenLoads{}) noexcept {
  const auto read = read_index.load(std::memory_order_acquire);
  between_loads();
  const auto write = write_index.load(std::memory_order_acquire);
  const auto pending = write - read;
  return std::min<size_t>(static_cast<size_t>(pending), capacity);
}

} // namespace orpheus::detail
