// SPDX-License-Identifier: MIT
#pragma once

#include <atomic>

namespace orpheus {

// Standalone fences for the seqlock protocols in the routing matrix. GCC
// rejects std::atomic_thread_fence under -fsanitize=thread (-Wtsan, an error
// with -Werror) because ThreadSanitizer does not model standalone fences. The
// fence code is still emitted, and every field the seqlocks guard is an
// atomic, so ThreadSanitizer reports no race; only the diagnostic is
// suppressed here. Clang accepts the fences and needs no suppression.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtsan"
#endif

/// Seqlock writer fence: orders the odd sequence store before the payload
/// stores that follow it.
inline void seqlockReleaseFence() noexcept {
  std::atomic_thread_fence(std::memory_order_release);
}

/// Seqlock reader fence: orders the payload loads of a read window before the
/// closing sequence load.
inline void seqlockAcquireFence() noexcept {
  std::atomic_thread_fence(std::memory_order_acquire);
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

} // namespace orpheus
