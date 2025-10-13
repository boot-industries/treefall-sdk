// SPDX-License-Identifier: MIT
#include "session/session_graph.h"

#include <gtest/gtest.h>

namespace orpheus::core::tests {
namespace {

QuantizationWindow MakeQuant(double grid, double tolerance) {
  QuantizationWindow window;
  window.grid_beats = grid;
  window.tolerance_beats = tolerance;
  return window;
}

} // namespace

TEST(SessionGraphScenes, TriggerQuantizesWithinTolerance) {
  SessionGraph session;
  Track* track = session.add_track("Track");
  ASSERT_NE(track, nullptr);

  Clip* clip = session.add_clip(*track, "Clip", 0.0, 2.0);
  ASSERT_NE(clip, nullptr);
  session.set_clip_scene(*clip, 1u);

  session.trigger_scene(1u, 3.05, MakeQuant(1.0, 0.1));
  session.end_scene(1u, 5.95, MakeQuant(1.0, 0.1));
  session.commit_arrangement();

  const auto& committed = session.committed_clips();
  ASSERT_EQ(committed.size(), 1u);
  EXPECT_EQ(committed[0].scene_index, 1u);
  EXPECT_DOUBLE_EQ(committed[0].arranged_start_beats, 3.0);
  EXPECT_DOUBLE_EQ(committed[0].arranged_length_beats, 2.0);
  EXPECT_DOUBLE_EQ(session.session_start_beats(), 3.0);
  EXPECT_DOUBLE_EQ(session.session_end_beats(), 5.0);
}

TEST(SessionGraphScenes, TriggerOutsideToleranceMovesForward) {
  SessionGraph session;
  Track* track = session.add_track("Track");
  ASSERT_NE(track, nullptr);

  Clip* clip = session.add_clip(*track, "Clip", 0.0, 4.0);
  ASSERT_NE(clip, nullptr);
  session.set_clip_scene(*clip, 2u);

  session.trigger_scene(2u, 3.21, MakeQuant(1.0, 0.05));
  session.commit_arrangement(1.0);

  const auto& committed = session.committed_clips();
  ASSERT_EQ(committed.size(), 1u);
  EXPECT_EQ(committed[0].scene_index, 2u);
  EXPECT_DOUBLE_EQ(committed[0].arranged_start_beats, 4.0);
  EXPECT_DOUBLE_EQ(committed[0].arranged_length_beats, 1.0);
  EXPECT_DOUBLE_EQ(session.session_start_beats(), 4.0);
  EXPECT_DOUBLE_EQ(session.session_end_beats(), 5.0);
}

TEST(SessionGraphScenes, ArrangementCommitOrdersScenesAndTracks) {
  SessionGraph session;
  Track* a = session.add_track("A");
  Track* b = session.add_track("B");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);

  Clip* a1 = session.add_clip(*a, "A1", 0.0, 1.5);
  Clip* b1 = session.add_clip(*b, "B1", 0.0, 1.0);
  Clip* a2 = session.add_clip(*a, "A2", 2.0, 1.0);
  Clip* b2 = session.add_clip(*b, "B2", 2.0, 0.5);

  ASSERT_NE(a1, nullptr);
  ASSERT_NE(b1, nullptr);
  ASSERT_NE(a2, nullptr);
  ASSERT_NE(b2, nullptr);

  session.set_clip_scene(*a1, 10u);
  session.set_clip_scene(*b1, 10u);
  session.set_clip_scene(*a2, 20u);
  session.set_clip_scene(*b2, 20u);

  session.trigger_scene(10u, 0.01, MakeQuant(2.0, 0.1));
  session.end_scene(10u, 1.95, MakeQuant(2.0, 0.1));

  session.trigger_scene(20u, 2.05, MakeQuant(2.0, 0.1));
  session.end_scene(20u, 3.95, MakeQuant(2.0, 0.1));

  session.commit_arrangement();

  const auto& committed = session.committed_clips();
  ASSERT_EQ(committed.size(), 4u);

  EXPECT_EQ(committed[0].scene_index, 10u);
  EXPECT_DOUBLE_EQ(committed[0].arranged_start_beats, 0.0);
  EXPECT_DOUBLE_EQ(committed[0].arranged_length_beats, 1.5);

  EXPECT_EQ(committed[1].scene_index, 10u);
  EXPECT_DOUBLE_EQ(committed[1].arranged_start_beats, 0.0);
  EXPECT_DOUBLE_EQ(committed[1].arranged_length_beats, 1.0);

  EXPECT_EQ(committed[2].scene_index, 20u);
  EXPECT_DOUBLE_EQ(committed[2].arranged_start_beats, 2.0);
  EXPECT_DOUBLE_EQ(committed[2].arranged_length_beats, 1.0);

  EXPECT_EQ(committed[3].scene_index, 20u);
  EXPECT_DOUBLE_EQ(committed[3].arranged_start_beats, 2.0);
  EXPECT_DOUBLE_EQ(committed[3].arranged_length_beats, 0.5);

  EXPECT_DOUBLE_EQ(session.session_start_beats(), 0.0);
  EXPECT_DOUBLE_EQ(session.session_end_beats(), 3.0);
}

} // namespace orpheus::core::tests
