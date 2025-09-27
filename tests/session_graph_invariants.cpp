// SPDX-License-Identifier: MIT
#include "session/session_graph.h"

#include <gtest/gtest.h>

namespace orpheus::core::tests {
namespace {

double ClipEnd(const Clip &clip) { return clip.start() + clip.length(); }

}  // namespace

TEST(SessionGraphInvariants, CommitSortsTracksAndClipsAndUpdatesRange) {
  SessionGraph session;
  Track *second = session.add_track("Beta");
  Track *first = session.add_track("Alpha");

  ASSERT_NE(second, nullptr);
  ASSERT_NE(first, nullptr);

  Clip *late = session.add_clip(*second, "zzz", 8.0, 2.0);
  Clip *early = session.add_clip(*second, "aaa", 2.0, 1.0);
  Clip *tied = session.add_clip(*second, "mmm", 2.0, 0.5);
  ASSERT_NE(late, nullptr);
  ASSERT_NE(early, nullptr);
  ASSERT_NE(tied, nullptr);

  session.commit_clip_grid();

  const auto &tracks = session.tracks();
  ASSERT_EQ(tracks.size(), 2u);
  EXPECT_EQ(tracks[0]->name(), "Alpha");
  EXPECT_EQ(tracks[1]->name(), "Beta");

  const auto &clips = tracks[1]->clips();
  ASSERT_EQ(clips.size(), 3u);
  EXPECT_EQ(clips[0]->name(), "aaa");
  EXPECT_EQ(clips[1]->name(), "mmm");
  EXPECT_EQ(clips[2]->name(), "zzz");

  EXPECT_DOUBLE_EQ(session.session_start_beats(), 2.0);
  EXPECT_DOUBLE_EQ(session.session_end_beats(), ClipEnd(*late));
}

TEST(SessionGraphInvariants, CommitResetsRangeWhenEmpty) {
  SessionGraph session;
  Track *track = session.add_track("Track");
  ASSERT_NE(track, nullptr);

  Clip *clip = session.add_clip(*track, "short", 1.0, 1.5);
  ASSERT_NE(clip, nullptr);

  session.commit_clip_grid();
  EXPECT_DOUBLE_EQ(session.session_start_beats(), 1.0);
  EXPECT_DOUBLE_EQ(session.session_end_beats(), ClipEnd(*clip));

  EXPECT_TRUE(session.remove_clip(clip));
  session.commit_clip_grid();
  EXPECT_DOUBLE_EQ(session.session_start_beats(), 0.0);
  EXPECT_DOUBLE_EQ(session.session_end_beats(), 0.0);
}

TEST(SessionGraphInvariants, ClipLengthIsClampedToMinimum) {
  SessionGraph session;
  Track *track = session.add_track("Track");
  ASSERT_NE(track, nullptr);

  Clip *clip = session.add_clip(*track, "Clip", 0.0, 0.0);
  ASSERT_NE(clip, nullptr);
  EXPECT_GT(clip->length(), 0.0);

  session.set_clip_length(*clip, -10.0);
  EXPECT_GT(clip->length(), 0.0);
}

}  // namespace orpheus::core::tests
