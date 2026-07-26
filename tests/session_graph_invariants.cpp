// SPDX-License-Identifier: MIT
#include <orpheus/session_graph.h>

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

namespace orpheus::core::tests {
namespace {

double ClipEnd(const Clip& clip) {
  return clip.start() + clip.length();
}

} // namespace

TEST(SessionGraphInvariants, CommitSortsTracksAndClipsAndUpdatesRange) {
  SessionGraph session;
  Track* second = session.add_track("Beta");
  Track* first = session.add_track("Alpha");

  ASSERT_NE(second, nullptr);
  ASSERT_NE(first, nullptr);

  Clip* late = session.add_clip(*second, "zzz", 8.0, 2.0);
  Clip* early = session.add_clip(*second, "aaa", 2.0, 1.0);
  Clip* tied = session.add_clip(*second, "mmm", 3.0, 0.5);
  ASSERT_NE(late, nullptr);
  ASSERT_NE(early, nullptr);
  ASSERT_NE(tied, nullptr);

  session.commit_clip_grid();

  const auto& tracks = session.tracks();
  ASSERT_EQ(tracks.size(), 2u);
  EXPECT_EQ(tracks[0]->name(), "Alpha");
  EXPECT_EQ(tracks[1]->name(), "Beta");

  const auto& clips = tracks[1]->clips();
  ASSERT_EQ(clips.size(), 3u);
  EXPECT_EQ(clips[0]->name(), "aaa");
  EXPECT_EQ(clips[1]->name(), "mmm");
  EXPECT_EQ(clips[2]->name(), "zzz");

  EXPECT_DOUBLE_EQ(session.session_start_beats(), 2.0);
  EXPECT_DOUBLE_EQ(session.session_end_beats(), ClipEnd(*late));
}

TEST(SessionGraphInvariants, CommitResetsRangeWhenEmpty) {
  SessionGraph session;
  Track* track = session.add_track("Track");
  ASSERT_NE(track, nullptr);

  Clip* clip = session.add_clip(*track, "short", 1.0, 1.5);
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
  Track* track = session.add_track("Track");
  ASSERT_NE(track, nullptr);

  Clip* clip = session.add_clip(*track, "Clip", 0.0, 0.0);
  ASSERT_NE(clip, nullptr);
  EXPECT_GT(clip->length(), 0.0);

  session.set_clip_length(*clip, -10.0);
  EXPECT_GT(clip->length(), 0.0);
}

TEST(SessionGraphInvariants, RejectsOverlappingClips) {
  SessionGraph session;
  Track* track = session.add_track("Track");
  ASSERT_NE(track, nullptr);

  Clip* first = session.add_clip(*track, "one", 0.0, 4.0);
  ASSERT_NE(first, nullptr);

  EXPECT_THROW(static_cast<void>(session.add_clip(*track, "two", 2.0, 4.0)), std::invalid_argument);

  Clip* second = session.add_clip(*track, "two", 4.0, 4.0);
  ASSERT_NE(second, nullptr);

  EXPECT_THROW(session.set_clip_start(*second, 2.0), std::invalid_argument);
  EXPECT_DOUBLE_EQ(second->start(), 4.0);

  Clip* third = session.add_clip(*track, "three", 12.0, 4.0);
  ASSERT_NE(third, nullptr);

  EXPECT_THROW(session.set_clip_length(*second, 10.0), std::invalid_argument);
  EXPECT_DOUBLE_EQ(second->length(), 4.0);
}

TEST(SessionGraphInvariants, RejectsNonFiniteTimelineValuesWithoutMutation) {
  SessionGraph session;
  Track* track = session.add_track("Timeline");
  ASSERT_NE(track, nullptr);
  Clip* clip = session.add_clip(*track, "clip", 0.0, 1.0, 1);
  ASSERT_NE(clip, nullptr);

  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double infinity = std::numeric_limits<double>::infinity();
  const double original_start = clip->start();
  const double original_length = clip->length();
  const std::size_t original_clip_count = track->clips().size();

  EXPECT_THROW(static_cast<void>(session.add_clip(*track, "nan", nan, 1.0)), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(session.add_clip(*track, "infinity", 2.0, infinity)),
               std::invalid_argument);
  EXPECT_THROW(session.set_clip_start(*clip, nan), std::invalid_argument);
  EXPECT_THROW(session.set_clip_length(*clip, infinity), std::invalid_argument);
  EXPECT_EQ(track->clips().size(), original_clip_count);
  EXPECT_DOUBLE_EQ(clip->start(), original_start);
  EXPECT_DOUBLE_EQ(clip->length(), original_length);

  session.set_session_range(3.0, 8.0);
  EXPECT_THROW(session.set_session_range(nan, 8.0), std::invalid_argument);
  EXPECT_THROW(session.set_session_range(3.0, infinity), std::invalid_argument);
  EXPECT_DOUBLE_EQ(session.session_start_beats(), 3.0);
  EXPECT_DOUBLE_EQ(session.session_end_beats(), 8.0);

  const QuantizationWindow valid_quantization{1.0, 0.1};
  session.trigger_scene(1, 0.0, valid_quantization);
  EXPECT_THROW(session.trigger_scene(2, nan, valid_quantization), std::invalid_argument);
  EXPECT_THROW(session.trigger_scene(2, 0.0, QuantizationWindow{infinity, 0.1}),
               std::invalid_argument);
  EXPECT_THROW(session.trigger_scene(2, 0.0, QuantizationWindow{1.0, nan}), std::invalid_argument);
  EXPECT_THROW(session.end_scene(1, infinity, valid_quantization), std::invalid_argument);
  EXPECT_THROW(session.end_scene(1, 1.0, QuantizationWindow{1.0, infinity}), std::invalid_argument);
  EXPECT_THROW(session.commit_arrangement(nan), std::invalid_argument);
  EXPECT_THROW(session.commit_arrangement(infinity), std::invalid_argument);

  session.end_scene(1, 1.0, valid_quantization);
  session.commit_arrangement(1.0);
  ASSERT_EQ(session.committed_clips().size(), 1u);
  EXPECT_DOUBLE_EQ(session.committed_clips().front().arranged_start_beats, 0.0);
  EXPECT_DOUBLE_EQ(session.committed_clips().front().arranged_length_beats, 1.0);
}

TEST(SessionGraphTransactions, CoalescesStableIdEditsIntoOneRevision) {
  SessionGraph session;
  const std::uint64_t base_revision = session.revision();

  auto transaction = session.begin_transaction();
  const TrackId track_id = session.create_track("Music");
  const TimeRange range = TimeRange::fromStartLength(TimePoint::fromSamples(48000), 96000);
  const ClipId clip_id = session.create_clip(track_id, "Intro", range, 3u);
  session.set_name("Edited");
  session.set_clip_assignment_ids({clip_id});
  const SessionGraphChangeSet change = transaction.commit();

  EXPECT_EQ(change.base_revision, base_revision);
  EXPECT_EQ(change.revision, base_revision + 1u);
  EXPECT_EQ(session.revision(), change.revision);
  EXPECT_TRUE(has_change(change.changes, SessionChangeFlags::Metadata));
  EXPECT_TRUE(has_change(change.changes, SessionChangeFlags::Structure));
  EXPECT_TRUE(has_change(change.changes, SessionChangeFlags::ClipAssignments));

  const SessionGraphSnapshot snapshot = session.snapshot();
  ASSERT_EQ(snapshot.tracks.size(), 1u);
  ASSERT_EQ(snapshot.tracks[0].clips.size(), 1u);
  EXPECT_EQ(snapshot.tracks[0].id, track_id);
  EXPECT_EQ(snapshot.tracks[0].clips[0].id, clip_id);
  EXPECT_EQ(snapshot.tracks[0].clips[0].track_id, track_id);
  EXPECT_EQ(snapshot.tracks[0].clips[0].range, range);
  EXPECT_EQ(snapshot.clip_assignments, (std::vector<ClipId>{clip_id}));
}

TEST(SessionGraphTransactions, DestructionRollsBackStateAndIdWatermarks) {
  SessionGraph session;
  const TrackId first_id = session.create_track("Existing");
  const std::uint64_t base_revision = session.revision();
  TrackId rolled_back_id;

  {
    auto transaction = session.begin_transaction();
    rolled_back_id = session.create_track("Temporary");
    session.set_name("Temporary name");
  }

  EXPECT_EQ(session.revision(), base_revision);
  EXPECT_EQ(session.name(), "Session");
  ASSERT_EQ(session.tracks().size(), 1u);
  EXPECT_EQ(session.tracks()[0]->id(), first_id);
  EXPECT_EQ(session.create_track("Replacement"), rolled_back_id);
}

TEST(SessionGraphTransactions, RestoreProvidesUndoRedoWithoutReusingOldRevision) {
  SessionGraph session;
  const TrackId track_id = session.create_track("Track");
  const ClipId clip_id = session.create_clip(
      track_id, "Clip", TimeRange::fromStartLength(TimePoint::fromSamples(0), 48000));
  const SessionGraphSnapshot before = session.snapshot();

  {
    auto transaction = session.begin_transaction();
    session.set_name("After");
    session.set_clip_range(clip_id,
                           TimeRange::fromStartLength(TimePoint::fromSamples(96000), 24000));
    static_cast<void>(transaction.commit());
  }
  const SessionGraphSnapshot after = session.snapshot();
  const std::uint64_t edited_revision = session.revision();

  session.restore(before);
  EXPECT_EQ(session.revision(), edited_revision + 1u);
  EXPECT_EQ(session.name(), "Session");
  ASSERT_EQ(session.snapshot().tracks[0].clips.size(), 1u);
  EXPECT_EQ(session.snapshot().tracks[0].clips[0].id, clip_id);
  EXPECT_EQ(session.snapshot().tracks[0].clips[0].range,
            TimeRange::fromStartLength(TimePoint::fromSamples(0), 48000));

  session.restore(after);
  EXPECT_EQ(session.revision(), edited_revision + 2u);
  EXPECT_EQ(session.name(), "After");
  EXPECT_EQ(session.snapshot().tracks[0].clips[0].range,
            TimeRange::fromStartLength(TimePoint::fromSamples(96000), 24000));
}

TEST(SessionGraphTransactions, RejectsNestedTransactions) {
  SessionGraph session;
  auto transaction = session.begin_transaction();
  EXPECT_THROW(static_cast<void>(session.begin_transaction()), std::logic_error);
  transaction.rollback();
}

} // namespace orpheus::core::tests
