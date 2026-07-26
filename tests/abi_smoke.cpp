// SPDX-License-Identifier: MIT
#include "orpheus/abi.h"

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>
#include <limits>

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

TEST(AbiTimelineTest, RejectsNonFiniteValuesWithoutChangingValidTimeline) {
  const auto* session_api = orpheus_session_abi_v1(ORPHEUS_ABI_MAJOR, nullptr, nullptr);
  const auto* clipgrid = orpheus_clipgrid_abi_v1(ORPHEUS_ABI_MAJOR, nullptr, nullptr);
  ASSERT_NE(session_api, nullptr);
  ASSERT_NE(clipgrid, nullptr);

  orpheus_session_handle session{};
  orpheus_track_handle track{};
  ASSERT_EQ(session_api->create(&session), ORPHEUS_STATUS_OK);
  const orpheus_track_desc track_desc{"track"};
  ASSERT_EQ(session_api->add_track(session, &track_desc, &track), ORPHEUS_STATUS_OK);

  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double infinity = std::numeric_limits<double>::infinity();
  const orpheus_clip_desc valid_desc{"clip", 0.0, 1.0, 1};
  const orpheus_clip_desc nan_start_desc{"nan", nan, 1.0, 1};
  const orpheus_clip_desc infinity_length_desc{"infinity", 2.0, infinity, 1};
  orpheus_clip_handle clip{};
  orpheus_clip_handle rejected_clip{};
  ASSERT_EQ(clipgrid->add_clip(session, track, &valid_desc, &clip), ORPHEUS_STATUS_OK);
  EXPECT_EQ(clipgrid->add_clip(session, track, &nan_start_desc, &rejected_clip),
            ORPHEUS_STATUS_INVALID_ARGUMENT);
  EXPECT_EQ(clipgrid->add_clip(session, track, &infinity_length_desc, &rejected_clip),
            ORPHEUS_STATUS_INVALID_ARGUMENT);
  EXPECT_EQ(clipgrid->set_clip_start(session, clip, nan), ORPHEUS_STATUS_INVALID_ARGUMENT);
  EXPECT_EQ(clipgrid->set_clip_length(session, clip, infinity), ORPHEUS_STATUS_INVALID_ARGUMENT);

  orpheus_scene_trigger_desc trigger{1, 0.0, {1.0, 0.1}};
  ASSERT_EQ(clipgrid->trigger_scene(session, &trigger), ORPHEUS_STATUS_OK);
  trigger.position_beats = nan;
  EXPECT_EQ(clipgrid->trigger_scene(session, &trigger), ORPHEUS_STATUS_INVALID_ARGUMENT);
  trigger.position_beats = 0.0;
  trigger.quant.grid_beats = infinity;
  EXPECT_EQ(clipgrid->trigger_scene(session, &trigger), ORPHEUS_STATUS_INVALID_ARGUMENT);
  trigger.quant.grid_beats = 1.0;
  trigger.quant.tolerance_beats = nan;
  EXPECT_EQ(clipgrid->trigger_scene(session, &trigger), ORPHEUS_STATUS_INVALID_ARGUMENT);

  orpheus_scene_end_desc end{1, infinity, {1.0, 0.1}};
  EXPECT_EQ(clipgrid->end_scene(session, &end), ORPHEUS_STATUS_INVALID_ARGUMENT);
  end.position_beats = 1.0;
  end.quant.tolerance_beats = infinity;
  EXPECT_EQ(clipgrid->end_scene(session, &end), ORPHEUS_STATUS_INVALID_ARGUMENT);

  orpheus_arrangement_commit_desc arrangement{nan};
  EXPECT_EQ(clipgrid->commit_arrangement(session, &arrangement), ORPHEUS_STATUS_INVALID_ARGUMENT);
  arrangement.fallback_scene_length_beats = infinity;
  EXPECT_EQ(clipgrid->commit_arrangement(session, &arrangement), ORPHEUS_STATUS_INVALID_ARGUMENT);

  end.quant.tolerance_beats = 0.1;
  ASSERT_EQ(clipgrid->end_scene(session, &end), ORPHEUS_STATUS_OK);
  arrangement.fallback_scene_length_beats = 1.0;
  EXPECT_EQ(clipgrid->commit_arrangement(session, &arrangement), ORPHEUS_STATUS_OK);
  EXPECT_EQ(clipgrid->set_clip_start(session, clip, 2.0), ORPHEUS_STATUS_OK);

  session_api->destroy(session);
}

TEST(AbiRenderClickTest, StreamsBoundedPcmAndRejectsImpossiblePlansBeforeFileCreation) {
  const auto* render = orpheus_render_abi_v1(ORPHEUS_ABI_MAJOR, nullptr, nullptr);
  ASSERT_NE(render, nullptr);

  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "orpheus-abi-render-click";
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path output = root / "valid.wav";
  const std::string output_string = output.string();
  const orpheus_render_click_spec valid{120.0, 1u, 8000u, 1u, 0.25, 1000.0, 0.01};
  ASSERT_EQ(render->render_click(&valid, output_string.c_str()), ORPHEUS_STATUS_OK);

  std::array<char, 44> header{};
  std::ifstream stream(output, std::ios::binary);
  ASSERT_TRUE(stream);
  stream.read(header.data(), static_cast<std::streamsize>(header.size()));
  ASSERT_EQ(stream.gcount(), static_cast<std::streamsize>(header.size()));
  const auto read_u16 = [&header](std::size_t offset) {
    std::uint16_t value = 0;
    std::memcpy(&value, header.data() + offset, sizeof(value));
    return value;
  };
  const auto read_u32 = [&header](std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, header.data() + offset, sizeof(value));
    return value;
  };
  EXPECT_EQ(std::string(header.data(), 4), "RIFF");
  EXPECT_EQ(std::string(header.data() + 8, 4), "WAVE");
  EXPECT_EQ(std::string(header.data() + 12, 4), "fmt ");
  EXPECT_EQ(std::string(header.data() + 36, 4), "data");
  EXPECT_EQ(read_u16(20), 1u);
  EXPECT_EQ(read_u16(22), 1u);
  EXPECT_EQ(read_u32(24), 8000u);
  EXPECT_EQ(read_u16(34), 16u);
  EXPECT_EQ(read_u32(40), 32000u);

  const fs::path excessive_channels = root / "excessive-channels.wav";
  const fs::path infinite_duration = root / "infinite-duration.wav";
  const fs::path excessive_bars = root / "excessive-bars.wav";
  const std::string excessive_channels_string = excessive_channels.string();
  const std::string infinite_duration_string = infinite_duration.string();
  const std::string excessive_bars_string = excessive_bars.string();
  auto invalid_channels = valid;
  invalid_channels.channels = std::numeric_limits<std::uint32_t>::max();
  EXPECT_EQ(render->render_click(&invalid_channels, excessive_channels_string.c_str()),
            ORPHEUS_STATUS_INVALID_ARGUMENT);
  auto invalid_duration = valid;
  invalid_duration.click_duration_seconds = std::numeric_limits<double>::infinity();
  EXPECT_EQ(render->render_click(&invalid_duration, infinite_duration_string.c_str()),
            ORPHEUS_STATUS_INVALID_ARGUMENT);
  auto invalid_bars = valid;
  invalid_bars.bars = 8389u;
  EXPECT_EQ(render->render_click(&invalid_bars, excessive_bars_string.c_str()),
            ORPHEUS_STATUS_INVALID_ARGUMENT);
  EXPECT_FALSE(fs::exists(excessive_channels));
  EXPECT_FALSE(fs::exists(infinite_duration));
  EXPECT_FALSE(fs::exists(excessive_bars));

  fs::remove_all(root);
}

} // namespace orpheus::tests
