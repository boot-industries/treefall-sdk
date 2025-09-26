#include "orpheus/abi.h"

#include <gtest/gtest.h>

namespace orpheus::tests {

TEST(AbiNegotiationTest, PrefersCurrentMajor) {
  AbiVersion requested{1, 5};
  const auto negotiated = NegotiateAbi(requested);
  EXPECT_EQ(negotiated.major, kCurrentAbi.major);
  EXPECT_LE(negotiated.minor, requested.minor);
}

TEST(AbiNegotiationTest, FallsBackOnMajorMismatch) {
  AbiVersion requested{2, 0};
  const auto negotiated = NegotiateAbi(requested);
  EXPECT_EQ(negotiated.major, kCurrentAbi.major);
  EXPECT_EQ(negotiated.minor, kCurrentAbi.minor);
}

}  // namespace orpheus::tests
