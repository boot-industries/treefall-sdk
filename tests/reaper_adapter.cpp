// SPDX-License-Identifier: MIT
#include "session/json_io.h"

#include "orpheus/adapters/reaper/entry.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

namespace session_json = orpheus::core::session_json;

namespace orpheus::tests {
namespace {

TEST(ReaperAdapterIntegration, ImportsMarkerSetsAndPlaylistLanes) {
  orpheus::core::SessionGraph graph;
  graph.set_name("Adapter Test");
  graph.set_tempo(120.0);
  graph.set_session_range(0.0, 8.0);
  auto* track = graph.add_track("Track");
  ASSERT_NE(track, nullptr);
  graph.add_clip(*track, "Clip", 0.0, 4.0);
  auto* marker_set = graph.add_marker_set("Song");
  ASSERT_NE(marker_set, nullptr);
  marker_set->add_marker("Intro", 0.0);
  marker_set->add_marker("Outro", 7.5);
  graph.add_playlist_lane("Main", true);
  graph.add_playlist_lane("Alternate", false);

  const std::string json = session_json::SerializeSession(graph);
  const std::filesystem::path temp_path =
      std::filesystem::temp_directory_path() / "orpheus_reaper_adapter.json";
  {
    std::ofstream file(temp_path);
    ASSERT_TRUE(file.is_open());
    file << json;
  }

  ASSERT_EQ(OrpheusImportSession(temp_path.string().c_str()), 1);
  const char* panel_text = ReaperExtensionPanelText();
  ASSERT_NE(panel_text, nullptr);
  const std::string panel(panel_text);
  EXPECT_NE(panel.find("Marker Sets: 1"), std::string::npos);
  EXPECT_NE(panel.find("Playlist Lanes: 2"), std::string::npos);
  EXPECT_NE(panel.find("Song (2)"), std::string::npos);
  EXPECT_NE(panel.find("Alternate"), std::string::npos);
}

} // namespace
} // namespace orpheus::tests
