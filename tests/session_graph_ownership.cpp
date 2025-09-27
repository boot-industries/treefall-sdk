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
using orpheus::core::SessionGraph;
using orpheus::core::Track;

static_assert(!std::is_copy_constructible_v<Track>);
static_assert(!std::is_copy_assignable_v<Track>);
static_assert(!std::is_copy_constructible_v<SessionGraph>);
static_assert(!std::is_copy_assignable_v<SessionGraph>);

TEST(SessionGraphOwnershipTest, TracksReturnConstReferences) {
  using TracksReturn = decltype(std::declval<const SessionGraph &>().tracks());
  using ClipsReturn = decltype(std::declval<const Track &>().clips());

  static_assert(std::is_same_v<TracksReturn,
                               const std::vector<std::unique_ptr<Track>> &>);
  static_assert(std::is_same_v<ClipsReturn,
                               const std::vector<std::unique_ptr<Clip>> &>);

  SUCCEED();
}

TEST(SessionGraphOwnershipTest, IteratorsTraverseWithoutCopying) {
  SessionGraph graph;
  Track *track = graph.add_track("drums");
  ASSERT_NE(track, nullptr);

  Clip *clip = graph.add_clip(*track, "intro", 0.0, 4.0);
  ASSERT_NE(clip, nullptr);

  EXPECT_EQ(std::distance(graph.tracks_begin(), graph.tracks_end()), 1);
  EXPECT_EQ(std::distance(track->clips_begin(), track->clips_end()), 1);
}

}  // namespace
}  // namespace orpheus::tests
