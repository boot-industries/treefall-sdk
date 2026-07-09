// SPDX-License-Identifier: MIT
// ORP133 G3: Debug-build enforcement of the transport command queue's
// single-producer contract.
//
// The UI→audio command queue is a lock-free SPSC ring: exactly ONE control
// thread may post commands (startClip/stopClip/updateClip*/...). postCommand()
// captures the first producer thread in debug builds and asserts if a command
// is later posted from a different thread. This test proves the assertion
// fires (death test) and that the legal single-thread pattern does not.
//
// Release builds compile the check out entirely; the death test is skipped
// when NDEBUG is defined.

#include "../../src/core/transport/transport_controller.h"

#include <gtest/gtest.h>
#include <memory>
#include <thread>

using namespace orpheus;

namespace {

constexpr uint32_t kSampleRate = 48000;

} // namespace

TEST(CommandProducerAssertTest, SingleThreadProducerIsAccepted) {
  // The legal pattern: every control-mutating call from one thread.
  TransportController transport(nullptr, kSampleRate);
  for (int i = 0; i < 64; ++i) {
    EXPECT_EQ(transport.startClip(static_cast<ClipHandle>(i % 8 + 1)), SessionGraphError::OK);
    EXPECT_EQ(transport.stopClip(static_cast<ClipHandle>(i % 8 + 1)), SessionGraphError::OK);
  }
  SUCCEED();
}

TEST(CommandProducerAssertTest, DedicatedControlThreadIsAccepted) {
  // A single non-main control thread is equally legal — the contract is ONE
  // producer thread, not specifically the thread that constructed the object.
  TransportController transport(nullptr, kSampleRate);
  std::thread control([&transport]() {
    for (int i = 0; i < 64; ++i) {
      EXPECT_EQ(transport.startClip(static_cast<ClipHandle>(i % 8 + 1)), SessionGraphError::OK);
    }
  });
  control.join();
  SUCCEED();
}

TEST(CommandProducerAssertTest, SecondProducerThreadAsserts) {
#ifdef NDEBUG
  GTEST_SKIP() << "Producer assertion is compiled out in release builds";
#else
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  EXPECT_DEATH(
      {
        TransportController transport(nullptr, kSampleRate);
        // Main thread claims the producer role...
        transport.startClip(1);
        // ...so a post from any other thread violates the SPSC contract.
        std::thread other([&transport]() { transport.startClip(2); });
        other.join();
      },
      "single");
#endif
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
