// SPDX-License-Identifier: MIT
#include "json_io.h"

#include <cmath>
#include <filesystem>
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

}  // namespace

TEST(JsonConformance, RoundTripFixtures) {
  const std::vector<std::string> fixtures = {
      "solo_click.json", "two_tracks.json", "loop_grid.json"};
  for (const auto &fixture : fixtures) {
    const fs::path path = FixturesRoot() / fixture;
    const SessionGraph session = LoadSessionFromFile(path.string());
    const std::string serialized = SerializeSession(session);
    const SessionGraph reparsed = ParseSession(serialized);
    ASSERT_TRUE(SessionsEqual(session, reparsed))
        << "Fixture round trip mismatch: " << fixture;
  }
}

TEST(JsonConformance, DeterministicClickFilename) {
  const SessionGraph session =
      LoadSessionFromFile((FixturesRoot() / "solo_click.json").string());
  const std::string filename =
      MakeRenderClickFilename(session.name(), session.tempo(), 4);
  EXPECT_EQ(filename, "out/solo_click__120__4.wav");

  const SessionGraph loop =
      LoadSessionFromFile((FixturesRoot() / "loop_grid.json").string());
  const std::string loop_filename =
      MakeRenderClickFilename(loop.name(), loop.tempo(), 16);
  EXPECT_EQ(loop_filename, "out/loop_grid__128__16.wav");
}

}  // namespace orpheus::core::session_json::tests
