// SPDX-License-Identifier: MIT
#pragma once

#include <atomic>
#include <cstdint>
#include <thread>
#include <utility>

namespace orpheus::detail {

/// A control-thread-replaced, non-owning target with callback admission leases.
///
/// The target owner remains responsible for pointee destruction. A successful
/// Lease pins callback use until destruction. Replacement closes admission,
/// drains admitted leases on the control thread, publishes the new pointer, and
/// reopens admission only for a non-null target.
template <typename T> class RealtimeBorrowedTarget final {
private:
  static constexpr uint64_t kAccepting = uint64_t{1} << 63;
  static constexpr uint64_t kCountMask = ~kAccepting;

  static_assert(std::atomic<uint64_t>::is_always_lock_free,
                "borrowed target admission must be lock-free");
  static_assert(std::atomic<T*>::is_always_lock_free,
                "borrowed target pointer publication must be lock-free");

public:
  class Lease final {
  public:
    Lease() noexcept = default;
    Lease(const Lease&) = delete;
    Lease& operator=(const Lease&) = delete;

    Lease(Lease&& other) noexcept
        : owner_(std::exchange(other.owner_, nullptr)),
          target_(std::exchange(other.target_, nullptr)) {}

    Lease& operator=(Lease&& other) noexcept {
      if (this != &other) {
        release();
        owner_ = std::exchange(other.owner_, nullptr);
        target_ = std::exchange(other.target_, nullptr);
      }
      return *this;
    }

    ~Lease() {
      release();
    }

    [[nodiscard]] explicit operator bool() const noexcept {
      return target_ != nullptr;
    }
    [[nodiscard]] T* get() const noexcept {
      return target_;
    }

  private:
    friend class RealtimeBorrowedTarget;
    Lease(RealtimeBorrowedTarget* owner, T* target) noexcept : owner_(owner), target_(target) {}

    void release() noexcept {
      if (owner_ != nullptr) {
        owner_->admission_.fetch_sub(1, std::memory_order_release);
        owner_ = nullptr;
        target_ = nullptr;
      }
    }

    RealtimeBorrowedTarget* owner_{nullptr};
    T* target_{nullptr};
  };

  explicit RealtimeBorrowedTarget(T* target = nullptr) noexcept : target_(target) {
    if (target != nullptr) {
      admission_.store(kAccepting, std::memory_order_release);
    }
  }

  RealtimeBorrowedTarget(const RealtimeBorrowedTarget&) = delete;
  RealtimeBorrowedTarget& operator=(const RealtimeBorrowedTarget&) = delete;

  [[nodiscard]] T* get() const noexcept {
    return target_.load(std::memory_order_acquire);
  }

  /// Attempt one admission. Contention is an empty lease, never a retry loop.
  [[nodiscard]] Lease tryAcquire() noexcept {
    uint64_t observed = admission_.load(std::memory_order_acquire);
    if ((observed & kAccepting) == 0 || (observed & kCountMask) == kCountMask) {
      return {};
    }

    if (!admission_.compare_exchange_strong(observed, observed + 1, std::memory_order_acq_rel,
                                            std::memory_order_acquire)) {
      return {};
    }

    T* target = target_.load(std::memory_order_acquire);
    if (target == nullptr) {
      admission_.fetch_sub(1, std::memory_order_release);
      return {};
    }
    return Lease(this, target);
  }

  /// Control-thread-only replacement. Return is a lifetime barrier for old target use.
  void replaceAndDrain(T* replacement) noexcept {
    replaceAndDrain(replacement, nullptr);
  }

  /// Test hook runs after admission closes and before the drain completes.
  void replaceAndDrain(T* replacement, void (*after_close)() noexcept) noexcept {
    admission_.fetch_and(kCountMask, std::memory_order_acq_rel);
    if (after_close != nullptr) {
      after_close();
    }

    while ((admission_.load(std::memory_order_acquire) & kCountMask) != 0) {
      std::this_thread::yield();
    }

    target_.store(replacement, std::memory_order_release);
    admission_.store(replacement != nullptr ? kAccepting : 0, std::memory_order_release);
  }

  ~RealtimeBorrowedTarget() {
    replaceAndDrain(nullptr);
  }

private:
  std::atomic<T*> target_{nullptr};
  std::atomic<uint64_t> admission_{0};
};

} // namespace orpheus::detail
