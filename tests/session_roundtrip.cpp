// SPDX-License-Identifier: MIT
#include "session/json_io.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace fs = std::filesystem;

namespace orpheus::core::session_json::tests {
namespace {

constexpr double kDoubleTolerance = 1e-6;

bool NearlyEqual(double lhs, double rhs) {
  return std::abs(lhs - rhs) <= kDoubleTolerance;
}

bool SessionsEqual(const SessionGraph& lhs, const SessionGraph& rhs) {
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
  const auto& lhs_marker_sets = lhs.marker_sets();
  const auto& rhs_marker_sets = rhs.marker_sets();
  if (lhs_marker_sets.size() != rhs_marker_sets.size()) {
    return false;
  }
  for (std::size_t set_index = 0; set_index < lhs_marker_sets.size(); ++set_index) {
    const MarkerSet& lhs_set = *lhs_marker_sets[set_index];
    const MarkerSet& rhs_set = *rhs_marker_sets[set_index];
    if (lhs_set.name() != rhs_set.name()) {
      return false;
    }
    const auto& lhs_markers = lhs_set.markers();
    const auto& rhs_markers = rhs_set.markers();
    if (lhs_markers.size() != rhs_markers.size()) {
      return false;
    }
    for (std::size_t marker_index = 0; marker_index < lhs_markers.size(); ++marker_index) {
      const auto& lhs_marker = lhs_markers[marker_index];
      const auto& rhs_marker = rhs_markers[marker_index];
      if (lhs_marker.name != rhs_marker.name) {
        return false;
      }
      if (!NearlyEqual(lhs_marker.position_beats, rhs_marker.position_beats)) {
        return false;
      }
    }
  }
  const auto& lhs_lanes = lhs.playlist_lanes();
  const auto& rhs_lanes = rhs.playlist_lanes();
  if (lhs_lanes.size() != rhs_lanes.size()) {
    return false;
  }
  for (std::size_t lane_index = 0; lane_index < lhs_lanes.size(); ++lane_index) {
    const PlaylistLane& lhs_lane = *lhs_lanes[lane_index];
    const PlaylistLane& rhs_lane = *rhs_lanes[lane_index];
    if (lhs_lane.name() != rhs_lane.name()) {
      return false;
    }
    if (lhs_lane.is_active() != rhs_lane.is_active()) {
      return false;
    }
  }
  const auto& lhs_tracks = lhs.tracks();
  const auto& rhs_tracks = rhs.tracks();
  if (lhs_tracks.size() != rhs_tracks.size()) {
    return false;
  }
  for (std::size_t track_index = 0; track_index < lhs_tracks.size(); ++track_index) {
    const Track& lhs_track = *lhs_tracks[track_index];
    const Track& rhs_track = *rhs_tracks[track_index];
    if (lhs_track.name() != rhs_track.name()) {
      return false;
    }
    const auto& lhs_clips = lhs_track.clips();
    const auto& rhs_clips = rhs_track.clips();
    if (lhs_clips.size() != rhs_clips.size()) {
      return false;
    }
    for (std::size_t clip_index = 0; clip_index < lhs_clips.size(); ++clip_index) {
      const Clip& lhs_clip = *lhs_clips[clip_index];
      const Clip& rhs_clip = *rhs_clips[clip_index];
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

std::string LoadFixtureText(const fs::path& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("Unable to open fixture: " + path.string());
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

fs::path FixturesRoot() {
  return fs::path(ORPHEUS_SESSION_FIXTURES_DIR);
}

} // namespace

TEST(SessionRoundTrip, GoldenFixturesAreStableAndDeterministic) {
  const std::vector<std::string> fixtures = {"solo_click.json", "two_tracks.json",
                                             "loop_grid.json"};

  for (const auto& fixture : fixtures) {
    const fs::path path = FixturesRoot() / fixture;
    const std::string original = LoadFixtureText(path);
    const SessionGraph session = LoadSessionFromFile(path.string());
    const std::string serialized = SerializeSession(session);
    EXPECT_EQ(serialized, original) << "Fixture serialization drifted: " << fixture;

    const SessionGraph reparsed = ParseSession(serialized);
    ASSERT_TRUE(SessionsEqual(session, reparsed)) << "Round-trip mismatch for fixture: " << fixture;

    const std::string reserialized = SerializeSession(reparsed);
    EXPECT_EQ(reserialized, serialized)
        << "Repeated serialization is not deterministic for: " << fixture;
  }
}

TEST(SessionRoundTrip, RejectsOverlappingClips) {
  const std::string invalid = R"({
    "name": "Invalid",
    "tempo_bpm": 120,
    "start_beats": 0,
    "end_beats": 8,
    "render": {
      "sample_rate_hz": 48000,
      "bit_depth": 24,
      "dither": true
    },
    "marker_sets": [],
    "playlist_lanes": [],
    "tracks": [
      {
        "name": "Track",
        "clips": [
          {"name": "one", "start_beats": 0, "length_beats": 4},
          {"name": "two", "start_beats": 2, "length_beats": 4}
        ]
      }
    ]
  })";

  EXPECT_THROW(ParseSession(invalid), std::invalid_argument);
}

} // namespace orpheus::core::session_json::tests
