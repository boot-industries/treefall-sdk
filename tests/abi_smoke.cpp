// SPDX-License-Identifier: MIT
#include "orpheus/abi.h"

#include <gtest/gtest.h>

namespace orpheus::tests {

TEST(AbiNegotiationTest, ReturnsTableForMatchingMajor) {
  uint32_t got_major = 0;
  uint32_t got_minor = 0;
  const auto* session = orpheus_session_abi_v1(ORPHEUS_ABI_MAJOR, &got_major, &got_minor);
  ASSERT_NE(session, nullptr);
  EXPECT_EQ(got_major, ORPHEUS_ABI_MAJOR);
  EXPECT_EQ(got_minor, ORPHEUS_ABI_MINOR);
}

TEST(AbiNegotiationTest, ReturnsNullForFutureMajorRequests) {
  uint32_t got_major = 0;
  uint32_t got_minor = 0;
  const auto* session = orpheus_session_abi_v1(ORPHEUS_ABI_MAJOR + 1, &got_major, &got_minor);
  EXPECT_EQ(session, nullptr);
  EXPECT_EQ(got_major, ORPHEUS_ABI_MAJOR);
  EXPECT_EQ(got_minor, ORPHEUS_ABI_MINOR);
}

TEST(AbiNegotiationTest, ReturnsNullForOlderMajorRequests) {
  uint32_t got_major = 0;
  uint32_t got_minor = 0;
  const auto* session = orpheus_session_abi_v1(ORPHEUS_ABI_MAJOR - 1, &got_major, &got_minor);
  EXPECT_EQ(session, nullptr);
  EXPECT_EQ(got_major, ORPHEUS_ABI_MAJOR);
  EXPECT_EQ(got_minor, ORPHEUS_ABI_MINOR);
}

TEST(AbiTablesTest, SessionTableProvidesCreateDestroy) {
  uint32_t got_major = 0;
  uint32_t got_minor = 0;
  const auto* session = orpheus_session_abi_v1(ORPHEUS_ABI_MAJOR, &got_major, &got_minor);
  ASSERT_NE(session, nullptr);
  EXPECT_EQ(got_major, ORPHEUS_ABI_MAJOR);
  EXPECT_EQ(got_minor, ORPHEUS_ABI_MINOR);

  orpheus_session_handle handle{};
  ASSERT_EQ(session->create(&handle), ORPHEUS_STATUS_OK);
  ASSERT_NE(handle, nullptr);
  session->destroy(handle);
}

TEST(AbiTablesTest, CapBitsAdvertised) {
  uint32_t session_major = 0;
  uint32_t session_minor = 0;
  const auto* session = orpheus_session_abi_v1(ORPHEUS_ABI_MAJOR, &session_major, &session_minor);
  ASSERT_NE(session, nullptr);
  EXPECT_NE(session->caps & ORPHEUS_SESSION_CAP_V1_CORE, 0ull);

  uint32_t clip_major = 0;
  uint32_t clip_minor = 0;
  const auto* clipgrid = orpheus_clipgrid_abi_v1(ORPHEUS_ABI_MAJOR, &clip_major, &clip_minor);
  ASSERT_NE(clipgrid, nullptr);
  EXPECT_NE(clipgrid->caps & ORPHEUS_CLIPGRID_CAP_V1_CORE, 0ull);
  EXPECT_NE(clipgrid->caps & ORPHEUS_CLIPGRID_CAP_V1_SCENES, 0ull);
  EXPECT_NE(clipgrid->set_clip_scene, nullptr);
  EXPECT_NE(clipgrid->trigger_scene, nullptr);
  EXPECT_NE(clipgrid->end_scene, nullptr);
  EXPECT_NE(clipgrid->commit_arrangement, nullptr);

  uint32_t render_major = 0;
  uint32_t render_minor = 0;
  const auto* render = orpheus_render_abi_v1(ORPHEUS_ABI_MAJOR, &render_major, &render_minor);
  ASSERT_NE(render, nullptr);
  EXPECT_NE(render->caps & ORPHEUS_RENDER_CAP_V1_CORE, 0ull);
}

TEST(AbiChildHandleTest, RejectsFabricatedStaleAndForeignChildHandles) {
  const auto* session_api = orpheus_session_abi_v1(ORPHEUS_ABI_MAJOR, nullptr, nullptr);
  const auto* clipgrid = orpheus_clipgrid_abi_v1(ORPHEUS_ABI_MAJOR, nullptr, nullptr);
  ASSERT_NE(session_api, nullptr);
  ASSERT_NE(clipgrid, nullptr);

  orpheus_session_handle session_a{};
  orpheus_session_handle session_b{};
  ASSERT_EQ(session_api->create(&session_a), ORPHEUS_STATUS_OK);
  ASSERT_EQ(session_api->create(&session_b), ORPHEUS_STATUS_OK);

  const orpheus_track_desc track_desc{"track"};
  orpheus_track_handle track_a{};
  orpheus_track_handle track_b{};
  ASSERT_EQ(session_api->add_track(session_a, &track_desc, &track_a), ORPHEUS_STATUS_OK);
  ASSERT_EQ(session_api->add_track(session_b, &track_desc, &track_b), ORPHEUS_STATUS_OK);

  const orpheus_clip_desc clip_desc{"clip", 0.0, 1.0, 0};
  const orpheus_clip_desc stale_desc{"stale", 2.0, 1.0, 0};
  orpheus_clip_handle valid_clip{};
  orpheus_clip_handle stale_clip{};
  orpheus_clip_handle foreign_clip{};
  ASSERT_EQ(clipgrid->add_clip(session_a, track_a, &clip_desc, &valid_clip), ORPHEUS_STATUS_OK);
  ASSERT_EQ(clipgrid->add_clip(session_a, track_a, &stale_desc, &stale_clip), ORPHEUS_STATUS_OK);
  ASSERT_EQ(clipgrid->add_clip(session_b, track_b, &clip_desc, &foreign_clip), ORPHEUS_STATUS_OK);
  ASSERT_EQ(clipgrid->remove_clip(session_a, stale_clip), ORPHEUS_STATUS_OK);

  const auto fabricated_track = reinterpret_cast<orpheus_track_handle>(static_cast<uintptr_t>(0x1));
  const auto fabricated_clip = reinterpret_cast<orpheus_clip_handle>(static_cast<uintptr_t>(0x1));
  orpheus_clip_handle out_clip{};
  EXPECT_EQ(clipgrid->add_clip(session_a, fabricated_track, &clip_desc, &out_clip),
            ORPHEUS_STATUS_NOT_FOUND);
  EXPECT_EQ(clipgrid->add_clip(session_a, track_b, &clip_desc, &out_clip),
            ORPHEUS_STATUS_NOT_FOUND);

  for (const auto invalid_clip : {fabricated_clip, stale_clip, foreign_clip}) {
    EXPECT_EQ(clipgrid->set_clip_start(session_a, invalid_clip, 2.0), ORPHEUS_STATUS_NOT_FOUND);
    EXPECT_EQ(clipgrid->set_clip_length(session_a, invalid_clip, 2.0), ORPHEUS_STATUS_NOT_FOUND);
    EXPECT_EQ(clipgrid->set_clip_scene(session_a, invalid_clip, 1), ORPHEUS_STATUS_NOT_FOUND);
  }

  EXPECT_EQ(clipgrid->set_clip_start(session_a, valid_clip, 2.0), ORPHEUS_STATUS_OK);
  EXPECT_EQ(clipgrid->set_clip_length(session_a, valid_clip, 2.0), ORPHEUS_STATUS_OK);
  EXPECT_EQ(clipgrid->set_clip_scene(session_a, valid_clip, 1), ORPHEUS_STATUS_OK);

  session_api->destroy(session_b);
  session_api->destroy(session_a);
}

} // namespace orpheus::tests
