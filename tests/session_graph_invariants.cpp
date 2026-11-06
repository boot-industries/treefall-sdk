// SPDX-License-Identifier: MIT
#include "support/rt_guard.hpp"

#include <orpheus/session_graph.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace orpheus::core::tests {
namespace {

double ClipEnd(const Clip& clip) {
  return clip.start() + clip.length();
}

// Test-local model of the original track behaviour: append, stable_sort by
// (start, name), then validate every adjacent pair.
struct ReferenceClip {
  std::uint64_t id = 0;
  std::string name;
  double start = 0.0;
  double length = 0.0;
};

constexpr double kReferenceTolerance = 1e-9;

void SortReference(std::vector<ReferenceClip>& clips) {
  std::stable_sort(clips.begin(), clips.end(),
                   [](const ReferenceClip& lhs, const ReferenceClip& rhs) {
                     if (lhs.start < rhs.start) {
                       return true;
                     }
                     if (rhs.start < lhs.start) {
                       return false;
                     }
                     return lhs.name < rhs.name;
                   });
}

/// Returns the error the full layout validation raises, if any.
std::optional<std::string> ValidateReference(const std::vector<ReferenceClip>& clips,
                                             const std::string& track_name) {
  for (std::size_t index = 1; index < clips.size(); ++index) {
    const ReferenceClip& previous = clips[index - 1];
    const ReferenceClip& current = clips[index];
    if (current.start + kReferenceTolerance < previous.start) {
      return "Clips on track \"" + track_name + "\" must be sorted by start time";
    }
    if (previous.start + previous.length > current.start + kReferenceTolerance) {
      return "Clips on track \"" + track_name + "\" must not overlap";
    }
  }
  return std::nullopt;
}

/// Applies an edit to a copy of the reference and keeps it only when the
/// full validation accepts it. Returns the rejection message, if any.
template <typename Edit>
std::optional<std::string> ApplyReferenceEdit(std::vector<ReferenceClip>& clips,
                                              const std::string& track_name, Edit edit) {
  std::vector<ReferenceClip> candidate = clips;
  edit(candidate);
  SortReference(candidate);
  if (auto error = ValidateReference(candidate, track_name)) {
    return error;
  }
  clips = std::move(candidate);
  return std::nullopt;
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

TEST(SessionGraphTransactions, DestructionRollsBackStateWithoutRegressingIdWatermarks) {
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
  const TrackId replacement_id = session.create_track("Replacement");
  EXPECT_GT(replacement_id.raw(), rolled_back_id.raw());
}

TEST(SessionGraphTransactions, RemovedIdsAreNeverReallocatedAfterRollback) {
  SessionGraph session;
  const TrackId removed_track = session.create_track("Removed");
  const TrackId survivor = session.create_track("Survivor");
  ASSERT_TRUE(session.remove_track(removed_track));
  ASSERT_EQ(session.tracks().size(), 1u);

  TrackId provisional_track;
  {
    auto transaction = session.begin_transaction();
    provisional_track = session.create_track("Provisional");
  }
  const TrackId after_rollback_track = session.create_track("After rollback");
  EXPECT_GT(after_rollback_track.raw(), provisional_track.raw());

  const ClipId removed_clip = session.create_clip(
      survivor, "Removed clip", TimeRange::fromStartLength(TimePoint::fromSamples(0), 48000));
  ASSERT_TRUE(session.remove_clip(removed_clip));

  ClipId provisional_clip;
  {
    auto transaction = session.begin_transaction();
    provisional_clip =
        session.create_clip(survivor, "Provisional clip",
                            TimeRange::fromStartLength(TimePoint::fromSamples(48000), 48000));
  }
  const ClipId after_rollback_clip =
      session.create_clip(survivor, "After rollback clip",
                          TimeRange::fromStartLength(TimePoint::fromSamples(96000), 48000));
  EXPECT_GT(after_rollback_clip.raw(), provisional_clip.raw());
}

TEST(SessionGraphTransactions, RestoreOfOlderSnapshotDoesNotRegressAllocator) {
  SessionGraph session;
  const TrackId first_track = session.create_track("First");
  const ClipId first_clip = session.create_clip(
      first_track, "First clip", TimeRange::fromStartLength(TimePoint::fromSamples(0), 48000));
  const SessionGraphSnapshot before = session.snapshot();

  const TrackId second_track = session.create_track("Second");
  const ClipId second_clip = session.create_clip(
      first_track, "Second clip", TimeRange::fromStartLength(TimePoint::fromSamples(48000), 48000));
  EXPECT_GT(second_track.raw(), first_track.raw());
  EXPECT_GT(second_clip.raw(), first_clip.raw());

  session.restore(before);
  ASSERT_EQ(session.tracks().size(), 1u);
  EXPECT_EQ(session.tracks()[0]->id(), first_track);
  EXPECT_GT(session.create_track("After restore").raw(), second_track.raw());
  EXPECT_GT(session
                .create_clip(first_track, "Clip after restore",
                             TimeRange::fromStartLength(TimePoint::fromSamples(96000), 48000))
                .raw(),
            second_clip.raw());
}

TEST(SessionGraphTransactions, PersistedSnapshotRestoresAboveItsWatermark) {
  SessionGraph source;
  const TrackId track = source.create_track("Track");
  static_cast<void>(source.create_clip(
      track, "Clip", TimeRange::fromStartLength(TimePoint::fromSamples(0), 48000)));
  static_cast<void>(source.create_track("Second"));
  const SessionGraphSnapshot before = source.snapshot();

  // A fresh graph's own watermark is 1. Restoring must raise it to the
  // persisted one, so the first allocation the restored graph hands out is the
  // persisted next ID rather than a reuse of a lower value.
  SessionGraph destination;
  destination.restore(before);
  EXPECT_EQ(destination.create_track("Restored").raw(), before.next_track_id_raw);
  EXPECT_EQ(destination
                .create_clip(track, "Restored clip",
                             TimeRange::fromStartLength(TimePoint::fromSamples(48000), 48000))
                .raw(),
            before.next_clip_id_raw);
  EXPECT_GT(before.next_track_id_raw, 1u);
  EXPECT_GT(before.next_clip_id_raw, 1u);
}

TEST(SessionGraphTransactions, LegacySnapshotRestoresUsingExtantIds) {
  SessionGraph source;
  const TrackId track = source.create_track("Track");
  const ClipId clip = source.create_clip(
      track, "Clip", TimeRange::fromStartLength(TimePoint::fromSamples(0), 48000));
  SessionGraphSnapshot legacy = source.snapshot();
  legacy.next_track_id_raw = 50u;
  legacy.next_clip_id_raw = 60u;
  legacy.schema_version = 1u;
  legacy.next_track_id_raw = 0u;
  legacy.next_clip_id_raw = 0u;

  SessionGraph destination;
  destination.restore(legacy);
  // The version-1 path must ignore the persisted watermark for both allocators
  // and derive them from the IDs the snapshot actually carries.
  EXPECT_EQ(destination.create_track("Restored").raw(), track.raw() + 1u);
  EXPECT_EQ(destination
                .create_clip(track, "Restored clip",
                             TimeRange::fromStartLength(TimePoint::fromSamples(48000), 48000))
                .raw(),
            clip.raw() + 1u);

  SessionGraphSnapshot too_old = source.snapshot();
  too_old.schema_version = 0u;
  EXPECT_THROW(destination.restore(too_old), std::invalid_argument);
  SessionGraphSnapshot too_new = source.snapshot();
  too_new.schema_version = SessionGraphSnapshot::kSchemaVersion + 1u;
  EXPECT_THROW(destination.restore(too_new), std::invalid_argument);
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

TEST(SessionGraphInvariants, SceneNumericRejectionDoesNotConsumePendingTimeline) {
  SessionGraph session;
  auto* track = session.add_track("Track");
  ASSERT_NE(session.add_clip(*track, "Clip", 0.0, 1.0, 1), nullptr);
  const QuantizationWindow quantization{1.0, 0.1};
  session.trigger_scene(1, 0.0, quantization);
  EXPECT_THROW(session.trigger_scene(2, 0x1p63, quantization), std::invalid_argument);
  EXPECT_THROW(session.trigger_scene(
                   2, 1.0, QuantizationWindow{std::numeric_limits<double>::denorm_min(), 0.0}),
               std::invalid_argument);
  EXPECT_THROW(session.end_scene(1, 0x1p63, quantization), std::invalid_argument);
  EXPECT_THROW(session.commit_arrangement(0x1p63), std::invalid_argument);
  EXPECT_TRUE(session.committed_clips().empty());
  session.end_scene(1, 1.0, quantization);
  session.commit_arrangement();
  ASSERT_EQ(session.committed_clips().size(), 1u);
  EXPECT_DOUBLE_EQ(session.committed_clips()[0].arranged_start_beats, 0.0);
  EXPECT_DOUBLE_EQ(session.committed_clips()[0].arranged_length_beats, 1.0);
}

TEST(SessionGraphInvariants, AssignmentValidationIsAtomicAndAllowsRepeatedMembers) {
  SessionGraph session;
  auto* track = session.add_track("Track");
  const auto id = session.add_clip(*track, "Clip", 0.0, 1.0)->id();
  session.set_clip_assignment_ids({id, id});
  EXPECT_EQ(session.clip_assignments(), (std::vector<uint64_t>{id.raw(), id.raw()}));
  const auto revision = session.revision();
  const auto before = session.clip_assignments();
  for (const uint64_t invalid : {uint64_t{0}, id.raw() + 1}) {
    EXPECT_THROW(session.set_clip_assignments({id.raw(), invalid}), std::invalid_argument);
    EXPECT_THROW(session.set_clip_assignment_ids({id, ClipId::fromRaw(invalid)}),
                 std::invalid_argument);
    EXPECT_EQ(session.clip_assignments(), before);
    EXPECT_EQ(session.revision(), revision);
  }
  auto snapshot = session.snapshot();
  snapshot.name = "Must not publish";
  snapshot.clip_assignments = {ClipId::fromRaw(id.raw() + 1)};
  EXPECT_THROW(session.restore(snapshot), std::invalid_argument);
  EXPECT_EQ(session.name(), "Session");
  EXPECT_EQ(session.tracks().front()->clips().front()->id(), id);
  EXPECT_EQ(session.tracks().front()->clips().front()->name(), "Clip");
  EXPECT_EQ(session.clip_assignments(), before);
  EXPECT_EQ(session.revision(), revision);
  snapshot.clip_assignments = {id, id};
  session.restore(snapshot);
  EXPECT_EQ(session.clip_assignments(), before);
}

TEST(SessionGraphInvariants, ExtremeTimingRejectionPreservesGraphAndRevision) {
  SessionGraph session;
  auto* track = session.add_track("Track");
  auto* clip = session.add_clip(*track, "Clip", 0.0, 1.0);
  const auto revision = session.revision();
  for (double value :
       {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::max(), 0x1p63}) {
    EXPECT_THROW(session.set_session_range(0.0, value), std::invalid_argument);
    EXPECT_THROW(session.set_clip_start(*clip, value), std::invalid_argument);
    EXPECT_THROW(session.set_clip_length(*clip, value), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(session.add_clip(*track, "Rejected", value, 1.0)),
                 std::invalid_argument);
    EXPECT_EQ(session.revision(), revision);
    EXPECT_DOUBLE_EQ(clip->start(), 0.0);
    EXPECT_DOUBLE_EQ(clip->length(), 1.0);
    EXPECT_EQ(track->clips().size(), 1u);
  }
  EXPECT_THROW(session.set_tempo(std::numeric_limits<double>::min()), std::invalid_argument);
  EXPECT_EQ(session.revision(), revision);
  auto snapshot = session.snapshot();
  snapshot.tracks[0].clips[0].range =
      TimeRange::fromStartLength(TimePoint::fromSamples(std::numeric_limits<int64_t>::max()), 1);
  EXPECT_THROW(session.restore(snapshot), std::invalid_argument);
  EXPECT_EQ(session.revision(), revision);
}

TEST(SessionGraphInvariants, BulkClipConstructionIsOneEditAndSortsTheTrack) {
  SessionGraph session;
  auto* track = session.add_track("Music");
  ASSERT_NE(track, nullptr);
  const std::uint64_t revision = session.revision();

  std::vector<SessionGraph::ClipSpec> specs;
  specs.push_back(SessionGraph::ClipSpec{"Second", 4.0, 2.0, 0u});
  specs.push_back(SessionGraph::ClipSpec{"First", 0.0, 2.0, 0u});
  specs.push_back(SessionGraph::ClipSpec{"Third", 8.0, 2.0, 0u});

  const std::vector<Clip*> clips = session.add_clips(*track, specs);
  ASSERT_EQ(clips.size(), 3u);
  EXPECT_EQ(session.revision(), revision + 1u) << "a bulk add is one logical edit";
  ASSERT_EQ(track->clips().size(), 3u);
  EXPECT_EQ(track->clips()[0]->name(), "First");
  EXPECT_EQ(track->clips()[1]->name(), "Second");
  EXPECT_EQ(track->clips()[2]->name(), "Third");
  for (const Clip* clip : clips) {
    EXPECT_NE(clip, nullptr);
  }
}

TEST(SessionGraphInvariants, BulkClipConstructionRejectsOverlapsWithoutSideEffects) {
  SessionGraph session;
  auto* track = session.add_track("Music");
  ASSERT_NE(track, nullptr);
  ASSERT_NE(session.add_clip(*track, "Existing", 0.0, 2.0), nullptr);
  const std::uint64_t revision = session.revision();
  const size_t clip_count = track->clips().size();

  std::vector<SessionGraph::ClipSpec> specs;
  specs.push_back(SessionGraph::ClipSpec{"Overlap", 1.0, 2.0, 0u});
  EXPECT_THROW(static_cast<void>(session.add_clips(*track, specs)), std::invalid_argument);
  EXPECT_EQ(track->clips().size(), clip_count) << "a rejected batch leaves the track unchanged";
  EXPECT_EQ(session.revision(), revision);
}

TEST(SessionGraphInvariants, EmptyBulkClipConstructionIsNotAnEdit) {
  SessionGraph session;
  auto* track = session.add_track("Music");
  ASSERT_NE(track, nullptr);
  const std::uint64_t revision = session.revision();
  EXPECT_TRUE(session.add_clips(*track, {}).empty());
  EXPECT_EQ(session.revision(), revision);
}

TEST(SessionGraphInvariants, TransactionRollbackRestoresTheGraphWithoutAllocating) {
  SessionGraph session;
  auto* track = session.add_track("Music");
  ASSERT_NE(track, nullptr);
  ASSERT_NE(session.add_clip(*track, "Clip", 0.0, 1.0), nullptr);

  auto transaction = session.begin_transaction();
  session.set_name("Renamed");
  ASSERT_NE(session.add_clip(*track, "Added", 4.0, 1.0), nullptr);

  orpheus::tests::support::RtGuardState::reset();
  {
    orpheus::tests::support::RtSection section;
    transaction.rollback();
  }
  EXPECT_EQ(orpheus::tests::support::RtGuardState::allocViolations(), 0u)
      << "the rollback must not allocate";
  EXPECT_EQ(session.name(), "Session");
  // The rollback replaces the graph's tracks, so the borrowed track pointer
  // from before the transaction is stale and the track is re-resolved.
  ASSERT_EQ(session.tracks().size(), 1u);
  ASSERT_EQ(session.tracks()[0]->clips().size(), 1u);
  EXPECT_EQ(session.tracks()[0]->clips()[0]->name(), "Clip");
}

TEST(SessionGraphInvariants, IncrementalEditsMatchSortedReference) {
  SessionGraph session;
  Track* track = session.add_track("Lane");
  ASSERT_NE(track, nullptr);
  const double tempo = session.tempo();
  const std::uint32_t sample_rate = session.render_sample_rate();
  // Quarter-beat grid in samples, so beat values derived from samples are exact.
  const auto quarter_beat_samples =
      static_cast<std::int64_t>(static_cast<double>(sample_rate) * 60.0 / tempo / 4.0);
  ASSERT_GT(quarter_beat_samples, 0);

  std::vector<ReferenceClip> reference;
  std::optional<std::uint64_t> next_clip_id;
  std::mt19937 rng(0x0282u);
  const auto pick = [&rng](std::uint32_t bound) {
    return static_cast<std::uint32_t>(rng() % bound);
  };
  const auto find_live = [&](std::uint64_t id) -> Clip* {
    for (const auto& clip : track->clips()) {
      if (clip->id().raw() == id) {
        return clip.get();
      }
    }
    return nullptr;
  };

  std::size_t accepted = 0;
  std::size_t rejected = 0;
  for (int step = 0; step < 2000; ++step) {
    const std::uint64_t revision_before = session.revision();
    const std::uint32_t op = reference.empty() ? 0u : pick(3);
    std::optional<std::string> expected_error;
    std::optional<std::string> actual_error;

    if (op == 0u) {
      const std::string name = "c" + std::to_string(pick(6));
      const double start = static_cast<double>(pick(1024)) * 0.25;
      const double length = static_cast<double>(1u + pick(16)) * 0.25;
      expected_error =
          ApplyReferenceEdit(reference, track->name(), [&](std::vector<ReferenceClip>& clips) {
            clips.push_back(ReferenceClip{next_clip_id.value_or(0), name, start, length});
          });
      try {
        Clip* clip = session.add_clip(*track, name, start, length);
        ASSERT_NE(clip, nullptr);
        if (!next_clip_id.has_value()) {
          next_clip_id = clip->id().raw();
          for (ReferenceClip& entry : reference) {
            if (entry.id == 0) {
              entry.id = *next_clip_id;
            }
          }
        }
        EXPECT_EQ(clip->id().raw(), *next_clip_id) << "step " << step;
        ++*next_clip_id;
      } catch (const std::invalid_argument& error) {
        actual_error = error.what();
      }
    } else {
      const ReferenceClip chosen = reference[pick(static_cast<std::uint32_t>(reference.size()))];
      const std::uint32_t start_quarter = pick(1024);
      if (op == 1u) {
        const double start = static_cast<double>(start_quarter) * 0.25;
        expected_error =
            ApplyReferenceEdit(reference, track->name(), [&](std::vector<ReferenceClip>& clips) {
              for (ReferenceClip& entry : clips) {
                if (entry.id == chosen.id) {
                  entry.start = start;
                }
              }
            });
        Clip* live = find_live(chosen.id);
        ASSERT_NE(live, nullptr);
        try {
          session.set_clip_start(*live, start);
        } catch (const std::invalid_argument& error) {
          actual_error = error.what();
        }
      } else {
        const auto start_samples = static_cast<std::int64_t>(start_quarter) * quarter_beat_samples;
        const auto length_samples = static_cast<std::int64_t>(1u + pick(16)) * quarter_beat_samples;
        const TimePoint start_point = TimePoint::fromSamples(start_samples);
        const TimePoint end_point = TimePoint::fromSamples(start_samples + length_samples);
        const double start = start_point.toBeats(tempo, sample_rate);
        const double length = end_point.toBeats(tempo, sample_rate) - start;
        expected_error =
            ApplyReferenceEdit(reference, track->name(), [&](std::vector<ReferenceClip>& clips) {
              for (ReferenceClip& entry : clips) {
                if (entry.id == chosen.id) {
                  entry.start = start;
                  entry.length = length;
                }
              }
            });
        try {
          session.set_clip_range(ClipId::fromRaw(chosen.id),
                                 TimeRange::fromStartEnd(start_point, end_point));
        } catch (const std::invalid_argument& error) {
          actual_error = error.what();
        }
      }
    }

    ASSERT_EQ(actual_error, expected_error) << "step " << step << " op " << op;
    if (expected_error.has_value()) {
      ++rejected;
      EXPECT_EQ(session.revision(), revision_before) << "step " << step;
    } else {
      ++accepted;
      EXPECT_EQ(session.revision(), revision_before + 1u) << "step " << step;
    }

    const auto& live = track->clips();
    ASSERT_EQ(live.size(), reference.size()) << "step " << step;
    for (std::size_t index = 0; index < live.size(); ++index) {
      ASSERT_EQ(live[index]->id().raw(), reference[index].id) << "step " << step;
      ASSERT_EQ(live[index]->name(), reference[index].name) << "step " << step;
      ASSERT_EQ(live[index]->start(), reference[index].start) << "step " << step;
      ASSERT_EQ(live[index]->length(), reference[index].length) << "step " << step;
    }
  }
  // The walk must exercise both outcomes to mean anything.
  EXPECT_GT(accepted, 100u);
  EXPECT_GT(rejected, 100u);
}
} // namespace orpheus::core::tests
