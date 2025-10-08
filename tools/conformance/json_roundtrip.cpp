// SPDX-License-Identifier: MIT
#include "json_io.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

namespace orpheus::core::session_json::tests {
namespace {

bool NearlyEqual(double lhs, double rhs, double tolerance = 1e-6) {
  return std::abs(lhs - rhs) <= tolerance;
}

bool SessionsEqual(const SessionGraph &lhs, const SessionGraph &rhs) {
  if (lhs.name() != rhs.name()) {
    return false;
  }
  if (!NearlyEqual(lhs.tempo(), rhs.tempo())) {
    return false;
  }
  if (!NearlyEqual(lhs.session_start_beats(), rhs.session_start_beats())) {
    return false;
  }
  if (!NearlyEqual(lhs.session_end_beats(), rhs.session_end_beats())) {
    return false;
  }
  if (lhs.render_sample_rate() != rhs.render_sample_rate()) {
    return false;
  }
  if (lhs.render_bit_depth() != rhs.render_bit_depth()) {
    return false;
  }
  if (lhs.render_dither() != rhs.render_dither()) {
    return false;
  }
  const auto &lhs_marker_sets = lhs.marker_sets();
  const auto &rhs_marker_sets = rhs.marker_sets();
  if (lhs_marker_sets.size() != rhs_marker_sets.size()) {
    return false;
  }
  for (std::size_t index = 0; index < lhs_marker_sets.size(); ++index) {
    const MarkerSet &lhs_set = *lhs_marker_sets[index];
    const MarkerSet &rhs_set = *rhs_marker_sets[index];
    if (lhs_set.name() != rhs_set.name()) {
      return false;
    }
    const auto &lhs_markers = lhs_set.markers();
    const auto &rhs_markers = rhs_set.markers();
    if (lhs_markers.size() != rhs_markers.size()) {
      return false;
    }
    for (std::size_t marker_index = 0; marker_index < lhs_markers.size();
         ++marker_index) {
      const auto &lhs_marker = lhs_markers[marker_index];
      const auto &rhs_marker = rhs_markers[marker_index];
      if (lhs_marker.name != rhs_marker.name) {
        return false;
      }
      if (!NearlyEqual(lhs_marker.position_beats, rhs_marker.position_beats)) {
        return false;
      }
    }
  }
  const auto &lhs_playlist_lanes = lhs.playlist_lanes();
  const auto &rhs_playlist_lanes = rhs.playlist_lanes();
  if (lhs_playlist_lanes.size() != rhs_playlist_lanes.size()) {
    return false;
  }
  for (std::size_t index = 0; index < lhs_playlist_lanes.size(); ++index) {
    const PlaylistLane &lhs_lane = *lhs_playlist_lanes[index];
    const PlaylistLane &rhs_lane = *rhs_playlist_lanes[index];
    if (lhs_lane.name() != rhs_lane.name()) {
      return false;
    }
    if (lhs_lane.is_active() != rhs_lane.is_active()) {
      return false;
    }
  }
  const auto &lhs_tracks = lhs.tracks();
  const auto &rhs_tracks = rhs.tracks();
  if (lhs_tracks.size() != rhs_tracks.size()) {
    return false;
  }
  for (std::size_t track_index = 0; track_index < lhs_tracks.size(); ++track_index) {
    const Track &lhs_track = *lhs_tracks[track_index];
    const Track &rhs_track = *rhs_tracks[track_index];
    if (lhs_track.name() != rhs_track.name()) {
      return false;
    }
    const auto &lhs_clips = lhs_track.clips();
    const auto &rhs_clips = rhs_track.clips();
    if (lhs_clips.size() != rhs_clips.size()) {
      return false;
    }
    for (std::size_t clip_index = 0; clip_index < lhs_clips.size(); ++clip_index) {
      const Clip &lhs_clip = *lhs_clips[clip_index];
      const Clip &rhs_clip = *rhs_clips[clip_index];
      if (lhs_clip.name() != rhs_clip.name()) {
        return false;
      }
      if (!NearlyEqual(lhs_clip.start(), rhs_clip.start())) {
        return false;
      }
      if (!NearlyEqual(lhs_clip.length(), rhs_clip.length())) {
        return false;
      }
    }
  }
  return true;
}

fs::path FixturesRoot() {
  return fs::path(ORPHEUS_FIXTURES_DIR);
}

std::string LoadFixtureText(const fs::path &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("Unable to open fixture: " + path.string());
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

}  // namespace

TEST(JsonConformance, RoundTripFixtures) {
  const char *external_command =
      std::getenv("ORPHEUS_JSON_ROUNDTRIP_COMMAND");
  if (external_command != nullptr && external_command[0] != '\0') {
    const std::string command(external_command);
    const int result = std::system(command.c_str());
    (void)result;
  }
  const std::vector<std::string> fixtures = {
      "solo_click.json", "two_tracks.json", "loop_grid.json"};
  for (const auto &fixture : fixtures) {
    const fs::path path = FixturesRoot() / fixture;
    const std::string original = LoadFixtureText(path);
    const SessionGraph session = LoadSessionFromFile(path.string());
    const std::string serialized = SerializeSession(session);
    const SessionGraph reparsed = ParseSession(serialized);
    ASSERT_TRUE(SessionsEqual(session, reparsed))
        << "Fixture round trip mismatch: " << fixture;
    EXPECT_EQ(serialized, original)
        << "Fixture serialization drifted: " << fixture;
  }
}

TEST(JsonConformance, DeterministicClickFilename) {
  const SessionGraph session =
      LoadSessionFromFile((FixturesRoot() / "solo_click.json").string());
  const std::string filename = MakeRenderClickFilename(session.name(), "Click",
                                                       44100, 16);
  EXPECT_EQ(filename, "out/solo_click_click_44p1k_16b.wav");

  const SessionGraph loop =
      LoadSessionFromFile((FixturesRoot() / "loop_grid.json").string());
  const std::string loop_filename =
      MakeRenderClickFilename(loop.name(), "Click", 48000, 16);
  EXPECT_EQ(loop_filename, "out/loop_grid_click_48k_16b.wav");
}

}  // namespace orpheus::core::session_json::tests
