// SPDX-License-Identifier: MIT

#include "../../src/core/common/realtime_borrowed_target.h"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>

namespace {

struct Target {
  int value;
};

std::atomic<bool> close_hook_called{false};

void markClosed() noexcept { close_hook_called.store(true, std::memory_order_release); }

} // namespace

TEST(RealtimeBorrowedTargetTest, LeasePinsTargetUntilReleased) {
  Target first{1};
  Target second{2};
  orpheus::detail::RealtimeBorrowedTarget<Target> target(&first);

  auto lease = target.tryAcquire();
  ASSERT_TRUE(lease);
  ASSERT_EQ(lease.get(), &first);
  EXPECT_EQ(target.get(), &first);

  close_hook_called.store(false, std::memory_order_release);
  std::atomic<bool> replacement_done{false};
  std::thread replacement([&]() {
    target.replaceAndDrain(&second, &markClosed);
    replacement_done.store(true, std::memory_order_release);
  });

  while (!close_hook_called.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
  EXPECT_FALSE(replacement_done.load(std::memory_order_acquire));
  EXPECT_EQ(target.get(), &first);

  lease = {};
  replacement.join();
  EXPECT_TRUE(replacement_done.load(std::memory_order_acquire));
  EXPECT_EQ(target.get(), &second);
}

TEST(RealtimeBorrowedTargetTest, ClosedTargetRejectsAdmissionAndNullReplacementDetaches) {
  Target first{1};
  orpheus::detail::RealtimeBorrowedTarget<Target> target(&first);

  close_hook_called.store(false, std::memory_order_release);
  target.replaceAndDrain(nullptr, &markClosed);
  EXPECT_TRUE(close_hook_called.load(std::memory_order_acquire));
  EXPECT_EQ(target.get(), nullptr);
  EXPECT_FALSE(target.tryAcquire());
}
