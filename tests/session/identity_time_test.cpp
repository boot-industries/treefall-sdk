// SPDX-License-Identifier: MIT
// ORP134 G2: identity & time-domain primitive tests.
//
// Covers: strong-ID type safety and hashing, deterministic allocation,
// JSON serialization round-trips BY ID (the acceptance gate), sample-domain
// canonical time with derived seconds/beats/timecode views, and the
// media-model aggregates.

#include <orpheus/identity.h>
#include <orpheus/json.hpp>
#include <orpheus/media_model.h>
#include <orpheus/time_domain.h>

#include <cstdlib>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>

namespace {

// IDs are 64-bit; the SDK's JSON value stores numbers as double (53-bit
// mantissa), so IDs serialize as DECIMAL STRINGS. This helper pair is the
// pattern hosts should copy.
template <typename Id> std::string idToJsonString(Id id) {
  return std::to_string(id.raw());
}

template <typename Id> Id idFromJsonString(const std::string& text) {
  return Id::fromRaw(std::strtoull(text.c_str(), nullptr, 10));
}

} // namespace

using namespace orpheus;

// ---------------------------------------------------------------------------
// Identity
// ---------------------------------------------------------------------------

TEST(IdentityTest, DefaultIsInvalidAndValidIdsCompare) {
  ClipId none;
  EXPECT_FALSE(none.isValid());
  EXPECT_EQ(none, ClipId::invalid());
  EXPECT_EQ(none.raw(), 0u);

  ClipId a = ClipId::fromRaw(1);
  ClipId b = ClipId::fromRaw(2);
  EXPECT_TRUE(a.isValid());
  EXPECT_NE(a, b);
  EXPECT_LT(a, b);
  EXPECT_GE(b, a);
}

TEST(IdentityTest, StrongTypesDoNotMix) {
  // Different tag → different type. (Compile-time property, asserted here.)
  static_assert(!std::is_same_v<ClipId, TrackId>);
  static_assert(!std::is_same_v<SessionId, MediaId>);
  static_assert(!std::is_convertible_v<ClipId, TrackId>);
  static_assert(!std::is_convertible_v<ClipId, uint64_t>);
  static_assert(!std::is_convertible_v<uint64_t, ClipId>);
  // Trivially copyable → safe as POD message payload on realtime paths.
  static_assert(std::is_trivially_copyable_v<ClipId>);
  static_assert(std::is_trivially_copyable_v<AutomationLaneId>);
  SUCCEED();
}

TEST(IdentityTest, HashSupportsUnorderedContainers) {
  std::unordered_map<ClipId, int> byClip;
  byClip[ClipId::fromRaw(7)] = 70;
  byClip[ClipId::fromRaw(9)] = 90;
  EXPECT_EQ(byClip.at(ClipId::fromRaw(7)), 70);
  EXPECT_EQ(byClip.at(ClipId::fromRaw(9)), 90);
}

TEST(IdentityTest, AllocatorIsDeterministicAndMonotonic) {
  IdAllocator<ClipId> alloc;
  ClipId first = alloc.allocate();
  ClipId second = alloc.allocate();
  EXPECT_EQ(first.raw(), 1u);
  EXPECT_EQ(second.raw(), 2u);

  // Replay from the same seed → same IDs (determinism rule).
  IdAllocator<ClipId> replay;
  EXPECT_EQ(replay.allocate(), first);
  EXPECT_EQ(replay.allocate(), second);

  // Watermark resume: continue above persisted IDs after a reload.
  IdAllocator<ClipId> resumed(alloc.nextRaw());
  EXPECT_EQ(resumed.allocate().raw(), 3u);

  // reserveThrough bumps past foreign IDs seen during deserialization.
  IdAllocator<ClipId> merged;
  merged.reserveThrough(ClipId::fromRaw(41));
  EXPECT_EQ(merged.allocate().raw(), 42u);
}

TEST(IdentityTest, JsonRoundTripById) {
  // The acceptance gate: serialization round-trips BY ID, through the SDK's
  // own JSON parser. IDs travel as decimal strings (64-bit safe; the JSON
  // number type is a double).
  IdAllocator<ClipId> clips;
  IdAllocator<TrackId> tracks;

  ClipId clip = clips.allocate();
  TrackId track = tracks.allocate();

  std::ostringstream wire;
  wire << "{\"clip\":\"" << idToJsonString(clip) << "\","
       << "\"track\":\"" << idToJsonString(track) << "\","
       << "\"clip_watermark\":\"" << clips.nextRaw() << "\"}";

  const std::string wireText = wire.str();
  orpheus::json::JsonParser parser(wireText);
  const orpheus::json::JsonValue doc = parser.Parse();
  const auto& obj = orpheus::json::ExpectObject(doc, "identity round-trip");

  ClipId clipBack = idFromJsonString<ClipId>(obj.object.at("clip").string);
  TrackId trackBack = idFromJsonString<TrackId>(obj.object.at("track").string);
  IdAllocator<ClipId> clipsBack(
      std::strtoull(obj.object.at("clip_watermark").string.c_str(), nullptr, 10));

  EXPECT_EQ(clipBack, clip);
  EXPECT_EQ(trackBack, track);
  // Post-reload allocation continues above every persisted ID.
  EXPECT_GT(clipsBack.allocate(), clipBack);
}

TEST(IdentityTest, JsonStringPathPreserves64BitIds) {
  // Above 2^53 a double-typed JSON number would corrupt the ID; the string
  // path must not.
  const uint64_t big = (1ull << 53) + 12345ull;
  MediaId media = MediaId::fromRaw(big);

  std::ostringstream wire;
  wire << "{\"media\":\"" << idToJsonString(media) << "\"}";

  const std::string wireText = wire.str();
  orpheus::json::JsonParser parser(wireText);
  const auto doc = parser.Parse();
  MediaId back = idFromJsonString<MediaId>(doc.object.at("media").string);
  EXPECT_EQ(back, media);
  EXPECT_EQ(back.raw(), big);
}

// ---------------------------------------------------------------------------
// Time domain
// ---------------------------------------------------------------------------

TEST(TimeDomainTest, SamplesAreCanonical) {
  constexpr uint32_t kRate = 48000;
  TimePoint p = TimePoint::fromSamples(96000);
  EXPECT_EQ(p.samples(), 96000);
  EXPECT_DOUBLE_EQ(p.toSeconds(kRate), 2.0);
  EXPECT_DOUBLE_EQ(p.toBeats(120.0, kRate), 4.0);

  // Derived-domain constructors round to the nearest sample.
  EXPECT_EQ(TimePoint::fromSeconds(2.0, kRate).samples(), 96000);
  EXPECT_EQ(TimePoint::fromBeats(4.0, 120.0, kRate).samples(), 96000);
  EXPECT_EQ(TimePoint::fromSeconds(1.0 / 48000.0 * 0.6, kRate).samples(), 1); // rounds up
}

TEST(TimeDomainTest, ConversionsRoundTripWithinOneSample) {
  constexpr uint32_t kRate = 48000;
  constexpr double kTempo = 133.7;
  for (int64_t samples : {0ll, 1ll, 479ll, 48000ll, 123456789ll}) {
    TimePoint p = TimePoint::fromSamples(samples);
    TimePoint viaSeconds = TimePoint::fromSeconds(p.toSeconds(kRate), kRate);
    TimePoint viaBeats = TimePoint::fromBeats(p.toBeats(kTempo, kRate), kTempo, kRate);
    EXPECT_LE(std::abs(viaSeconds - p), 1) << "seconds round-trip drifted at " << samples;
    EXPECT_LE(std::abs(viaBeats - p), 1) << "beats round-trip drifted at " << samples;
  }
}

TEST(TimeDomainTest, TimecodeIsIntegerExact) {
  constexpr uint32_t kRate = 48000;
  // 1h 02m 03s + 12 frames @ 25fps = 12/25s = 0.48s = 23040 samples
  const int64_t samples = ((3600 + 120 + 3) * static_cast<int64_t>(kRate)) + 23040;
  Timecode tc = TimePoint::fromSamples(samples).toTimecode(kRate, 25);
  EXPECT_EQ(tc.hours, 1);
  EXPECT_EQ(tc.minutes, 2);
  EXPECT_EQ(tc.seconds, 3);
  EXPECT_EQ(tc.frames, 12);

  // Negative time clamps to zero rather than producing negative fields.
  Timecode neg = TimePoint::fromSamples(-100).toTimecode(kRate, 25);
  EXPECT_EQ(neg, Timecode{});
}

TEST(TimeDomainTest, RangesAreHalfOpen) {
  TimeRange range = TimeRange::fromStartLength(TimePoint::fromSamples(1000), 500);
  EXPECT_EQ(range.start().samples(), 1000);
  EXPECT_EQ(range.end().samples(), 1500);
  EXPECT_EQ(range.length(), 500);
  EXPECT_FALSE(range.empty());

  EXPECT_TRUE(range.contains(TimePoint::fromSamples(1000))); // inclusive start
  EXPECT_TRUE(range.contains(TimePoint::fromSamples(1499)));
  EXPECT_FALSE(range.contains(TimePoint::fromSamples(1500))); // exclusive end
  EXPECT_FALSE(range.contains(TimePoint::fromSamples(999)));
}

TEST(TimeDomainTest, OverlapAndIntersection) {
  TimeRange a = TimeRange::fromStartLength(TimePoint::fromSamples(0), 1000);
  TimeRange b = TimeRange::fromStartLength(TimePoint::fromSamples(500), 1000);
  TimeRange c = TimeRange::fromStartLength(TimePoint::fromSamples(2000), 100);

  EXPECT_TRUE(a.overlaps(b));
  EXPECT_TRUE(b.overlaps(a));
  EXPECT_FALSE(a.overlaps(c));

  TimeRange ab = a.intersect(b);
  EXPECT_EQ(ab.start().samples(), 500);
  EXPECT_EQ(ab.end().samples(), 1000);

  EXPECT_TRUE(a.intersect(c).empty());

  // Adjacent half-open ranges do not overlap.
  TimeRange d = TimeRange::fromStartEnd(TimePoint::fromSamples(1000), TimePoint::fromSamples(2000));
  EXPECT_FALSE(a.overlaps(d));

  // Negative length clamps to empty.
  EXPECT_TRUE(TimeRange::fromStartLength(TimePoint::fromSamples(10), -5).empty());
}

// ---------------------------------------------------------------------------
// Media model aggregates
// ---------------------------------------------------------------------------

TEST(MediaModelTest, TakeSerializationRoundTripsById) {
  Take take;
  take.id = ClipId::fromRaw(11);
  take.track = TrackId::fromRaw(3);
  take.region.media = MediaId::fromRaw(77);
  take.region.sourceRange = TimeRange::fromStartLength(TimePoint::fromSamples(480), 96000);
  take.placedAt = TimePoint::fromSamples(24000);
  take.index = 2;
  take.name = "vocal pass 2";

  std::ostringstream wire;
  wire << "{\"id\":\"" << idToJsonString(take.id) << "\","
       << "\"track\":\"" << idToJsonString(take.track) << "\","
       << "\"media\":\"" << idToJsonString(take.region.media) << "\","
       << "\"src_start\":" << take.region.sourceRange.start().samples() << ","
       << "\"src_length\":" << take.region.sourceRange.length() << ","
       << "\"placed_at\":" << take.placedAt.samples() << ","
       << "\"index\":" << take.index << ","
       << "\"name\":\"" << take.name << "\"}";

  const std::string wireText = wire.str();
  orpheus::json::JsonParser parser(wireText);
  const auto parsed = parser.Parse();
  const auto& obj = orpheus::json::ExpectObject(parsed, "take round-trip");

  Take back;
  back.id = idFromJsonString<ClipId>(obj.object.at("id").string);
  back.track = idFromJsonString<TrackId>(obj.object.at("track").string);
  back.region.media = idFromJsonString<MediaId>(obj.object.at("media").string);
  back.region.sourceRange = TimeRange::fromStartLength(
      TimePoint::fromSamples(static_cast<int64_t>(obj.object.at("src_start").number)),
      static_cast<int64_t>(obj.object.at("src_length").number));
  back.placedAt = TimePoint::fromSamples(static_cast<int64_t>(obj.object.at("placed_at").number));
  back.index = static_cast<uint32_t>(obj.object.at("index").number);
  back.name = obj.object.at("name").string;

  EXPECT_EQ(back.id, take.id);
  EXPECT_EQ(back.track, take.track);
  EXPECT_EQ(back.region, take.region);
  EXPECT_EQ(back.placedAt, take.placedAt);
  EXPECT_EQ(back.index, take.index);
  EXPECT_EQ(back.name, take.name);
}

TEST(MediaModelTest, LauncherSceneHoldsSlots) {
  LauncherScene scene;
  scene.name = "Act 1";
  scene.slots.push_back({ClipId::fromRaw(5), 0, 2, 3});
  scene.slots.push_back({ClipId::invalid(), 0, 2, 4}); // empty slot

  EXPECT_EQ(scene.slots.size(), 2u);
  EXPECT_TRUE(scene.slots[0].clip.isValid());
  EXPECT_FALSE(scene.slots[1].clip.isValid());
  EXPECT_EQ(scene.slots[0].row, 2);
  EXPECT_EQ(scene.slots[0].column, 3);
}
