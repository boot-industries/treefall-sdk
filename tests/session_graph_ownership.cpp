// SPDX-License-Identifier: MIT
#include "session/session_graph.h"

#include <gtest/gtest.h>

#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

namespace orpheus::tests {
namespace {

using orpheus::core::Clip;
using orpheus::core::MarkerSet;
using orpheus::core::PlaylistLane;
using orpheus::core::SessionGraph;
using orpheus::core::Track;

static_assert(!std::is_copy_constructible_v<Track>);
static_assert(!std::is_copy_assignable_v<Track>);
static_assert(!std::is_copy_constructible_v<SessionGraph>);
static_assert(!std::is_copy_assignable_v<SessionGraph>);

TEST(SessionGraphOwnershipTest, TracksReturnConstReferences) {
  using TracksReturn = decltype(std::declval<const SessionGraph&>().tracks());
  using ClipsReturn = decltype(std::declval<const Track&>().clips());
  using MarkerSetsReturn = decltype(std::declval<const SessionGraph&>().marker_sets());
  using PlaylistLanesReturn = decltype(std::declval<const SessionGraph&>().playlist_lanes());
  using MarkersReturn = decltype(std::declval<const MarkerSet&>().markers());

  static_assert(std::is_same_v<TracksReturn, const std::vector<std::unique_ptr<Track>>&>);
  static_assert(std::is_same_v<ClipsReturn, const std::vector<std::unique_ptr<Clip>>&>);
  static_assert(std::is_same_v<MarkerSetsReturn, const std::vector<std::unique_ptr<MarkerSet>>&>);
  static_assert(
      std::is_same_v<PlaylistLanesReturn, const std::vector<std::unique_ptr<PlaylistLane>>&>);
  static_assert(std::is_same_v<MarkersReturn, const std::vector<MarkerSet::Marker>&>);

  SUCCEED();
}

TEST(SessionGraphOwnershipTest, IteratorsTraverseWithoutCopying) {
  SessionGraph graph;
  Track* track = graph.add_track("drums");
  ASSERT_NE(track, nullptr);

  Clip* clip = graph.add_clip(*track, "intro", 0.0, 4.0);
  ASSERT_NE(clip, nullptr);

  EXPECT_EQ(std::distance(graph.tracks_begin(), graph.tracks_end()), 1);
  EXPECT_EQ(std::distance(track->clips_begin(), track->clips_end()), 1);
}

TEST(SessionGraphOwnershipTest, MarkerSetsAndPlaylistLanesAccessible) {
  SessionGraph graph;
  MarkerSet* marker_set = graph.add_marker_set("Navigation");
  ASSERT_NE(marker_set, nullptr);
  MarkerSet::Marker* marker = marker_set->add_marker("Intro", 0.0);
  ASSERT_NE(marker, nullptr);
  EXPECT_EQ(std::distance(marker_set->markers_begin(), marker_set->markers_end()), 1);
  EXPECT_EQ(std::distance(graph.marker_sets_begin(), graph.marker_sets_end()), 1);

  PlaylistLane* lane = graph.add_playlist_lane("Main", true);
  ASSERT_NE(lane, nullptr);
  EXPECT_TRUE(lane->is_active());
  EXPECT_EQ(std::distance(graph.playlist_lanes_begin(), graph.playlist_lanes_end()), 1);
}

} // namespace
} // namespace orpheus::tests
