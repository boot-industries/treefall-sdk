// SPDX-License-Identifier: MIT
#include "orpheus/abi.h"

#include <gtest/gtest.h>

namespace orpheus::tests {

TEST(AbiNegotiationTest, DowngradesToSupportedMajor) {
  uint32_t got_major = 0;
  uint32_t got_minor = 0;
  const auto *session = orpheus_session_abi_v1(2, &got_major, &got_minor);
  ASSERT_NE(session, nullptr);
  EXPECT_EQ(got_major, ORPHEUS_ABI_V1_MAJOR);
  EXPECT_EQ(got_minor, ORPHEUS_ABI_V1_MINOR);
}

TEST(AbiNegotiationTest, UpgradesOlderMajorRequests) {
  uint32_t got_major = 0;
  uint32_t got_minor = 0;
  const auto *session = orpheus_session_abi_v1(0, &got_major, &got_minor);
  ASSERT_NE(session, nullptr);
  EXPECT_EQ(got_major, ORPHEUS_ABI_V1_MAJOR);
  EXPECT_EQ(got_minor, ORPHEUS_ABI_V1_MINOR);
}

TEST(AbiTablesTest, SessionTableProvidesCreateDestroy) {
  uint32_t got_major = 0;
  uint32_t got_minor = 0;
  const auto *session =
      orpheus_session_abi_v1(ORPHEUS_ABI_V1_MAJOR, &got_major, &got_minor);
  ASSERT_NE(session, nullptr);
  EXPECT_EQ(got_major, ORPHEUS_ABI_V1_MAJOR);
  EXPECT_EQ(got_minor, ORPHEUS_ABI_V1_MINOR);

  orpheus_session_handle handle{};
  ASSERT_EQ(session->create(&handle), ORPHEUS_STATUS_OK);
  ASSERT_NE(handle, nullptr);
  session->destroy(handle);
}

TEST(AbiTablesTest, CapBitsAdvertised) {
  uint32_t session_major = 0;
  uint32_t session_minor = 0;
  const auto *session = orpheus_session_abi_v1(ORPHEUS_ABI_V1_MAJOR,
                                               &session_major, &session_minor);
  ASSERT_NE(session, nullptr);
  EXPECT_NE(session->caps & ORPHEUS_SESSION_CAP_V1_CORE, 0ull);

  uint32_t clip_major = 0;
  uint32_t clip_minor = 0;
  const auto *clipgrid =
      orpheus_clipgrid_abi_v1(ORPHEUS_ABI_V1_MAJOR, &clip_major, &clip_minor);
  ASSERT_NE(clipgrid, nullptr);
  EXPECT_NE(clipgrid->caps & ORPHEUS_CLIPGRID_CAP_V1_CORE, 0ull);
  EXPECT_NE(clipgrid->caps & ORPHEUS_CLIPGRID_CAP_V1_SCENES, 0ull);
  EXPECT_NE(clipgrid->set_clip_scene, nullptr);
  EXPECT_NE(clipgrid->trigger_scene, nullptr);
  EXPECT_NE(clipgrid->end_scene, nullptr);
  EXPECT_NE(clipgrid->commit_arrangement, nullptr);

  uint32_t render_major = 0;
  uint32_t render_minor = 0;
  const auto *render =
      orpheus_render_abi_v1(ORPHEUS_ABI_V1_MAJOR, &render_major, &render_minor);
  ASSERT_NE(render, nullptr);
  EXPECT_NE(render->caps & ORPHEUS_RENDER_CAP_V1_CORE, 0ull);
}

}  // namespace orpheus::tests
