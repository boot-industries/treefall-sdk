// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <bit>
#include <cstdint>

#include "../../src/platform/audio_drivers/wasapi/wasapi_format_validation.h"

namespace orpheus {
namespace {

// The speaker-mask reconciliation a self-authored native request uses. The
// decision is host-neutral, so it is verified on every platform; the
// descriptor transformation that applies it is covered by the Windows driver
// test, which exercises the real mix-format copy path.

TEST(WASAPIChannelMaskTest, CanonicalMasksCoverSupportedChannelCounts) {
  EXPECT_EQ(detail::canonicalWASAPISpeakerMask(1), 0x4u);
  EXPECT_EQ(detail::canonicalWASAPISpeakerMask(2), 0x3u);
  EXPECT_EQ(detail::canonicalWASAPISpeakerMask(4), 0x33u);
  EXPECT_EQ(detail::canonicalWASAPISpeakerMask(6), 0x3fu);
  EXPECT_EQ(detail::canonicalWASAPISpeakerMask(8), 0x63fu);
}

TEST(WASAPIChannelMaskTest, CountsWithoutCanonicalLayoutStayUnspecified) {
  EXPECT_EQ(detail::canonicalWASAPISpeakerMask(0), 0u);
  EXPECT_EQ(detail::canonicalWASAPISpeakerMask(3), 0u);
  EXPECT_EQ(detail::canonicalWASAPISpeakerMask(5), 0u);
  EXPECT_EQ(detail::canonicalWASAPISpeakerMask(7), 0u);
  EXPECT_EQ(detail::canonicalWASAPISpeakerMask(9), 0u);
}

TEST(WASAPIChannelMaskTest, MatchingInheritedMaskIsKept) {
  EXPECT_EQ(detail::reconcileWASAPIChannelMask(0x3u, 2), 0x3u);
  EXPECT_EQ(detail::reconcileWASAPIChannelMask(0x3fu, 6), 0x3fu);
  EXPECT_EQ(detail::reconcileWASAPIChannelMask(0x63fu, 8), 0x63fu);
}

TEST(WASAPIChannelMaskTest, UnspecifiedInheritedMaskStaysUnspecified) {
  EXPECT_EQ(detail::reconcileWASAPIChannelMask(0u, 2), 0u);
  EXPECT_EQ(detail::reconcileWASAPIChannelMask(0u, 6), 0u);
}

TEST(WASAPIChannelMaskTest, ReducedCountReplacesTheDeviceLayout) {
  EXPECT_EQ(detail::reconcileWASAPIChannelMask(0x3fu, 2), 0x3u);
  EXPECT_EQ(detail::reconcileWASAPIChannelMask(0x63fu, 6), 0x3fu);
  EXPECT_EQ(detail::reconcileWASAPIChannelMask(0x3fu, 1), 0x4u);
  EXPECT_EQ(detail::reconcileWASAPIChannelMask(0x3fu, 4), 0x33u);
}

TEST(WASAPIChannelMaskTest, CountWithoutCanonicalLayoutBecomesUnspecified) {
  EXPECT_EQ(detail::reconcileWASAPIChannelMask(0x3fu, 3), 0u);
  EXPECT_EQ(detail::reconcileWASAPIChannelMask(0x3fu, 5), 0u);
  EXPECT_EQ(detail::reconcileWASAPIChannelMask(0x3fu, 7), 0u);
}

TEST(WASAPIChannelMaskTest, ReconciledMaskAlwaysMatchesTheRequestedCount) {
  for (uint32_t inherited : {0u, 0x3u, 0x4u, 0x33u, 0x3fu, 0x63fu}) {
    for (uint16_t channels = 1; channels <= 8; ++channels) {
      const uint32_t mask = detail::reconcileWASAPIChannelMask(inherited, channels);
      EXPECT_TRUE(mask == 0 || std::popcount(mask) == channels)
          << "inherited=0x" << std::hex << inherited << " channels=" << std::dec << channels
          << " mask=0x" << std::hex << mask;
    }
  }
}

} // namespace
} // namespace orpheus
