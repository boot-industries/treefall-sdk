#include "orpheus/abi.h"

#include <gtest/gtest.h>

namespace orpheus::tests {

TEST(AbiNegotiationTest, PrefersCurrentMajor) {
  AbiVersion requested{1, 5};
  const auto negotiated = NegotiateAbi(requested);
  EXPECT_EQ(negotiated.major, 1u);
  EXPECT_LE(negotiated.minor, requested.minor);
}

TEST(AbiNegotiationTest, FallsBackOnMajorMismatch) {
  AbiVersion requested{2, 0};
  const auto negotiated = NegotiateAbi(requested);
  EXPECT_EQ(negotiated.major, 1u);
  EXPECT_EQ(negotiated.minor, 0u);
}

TEST(AbiTablesTest, SessionTableProvidesCreateDestroy) {
  const auto *session = orpheus_session_abi_v1();
  ASSERT_NE(session, nullptr);

  orpheus_session_handle handle{};
  ASSERT_EQ(session->create(&handle), ORPHEUS_STATUS_OK);
  ASSERT_NE(handle, nullptr);
  session->destroy(handle);
}

}  // namespace orpheus::tests
