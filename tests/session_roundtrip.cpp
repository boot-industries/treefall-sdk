// SPDX-License-Identifier: MIT
#include <orpheus/json.hpp>
#include <orpheus/session_io.h>

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

TEST(SessionRoundTrip, MigratesLegacySchemaAndRejectsFutureSchema) {
  const std::string original = LoadFixtureText(FixturesRoot() / "solo_click.json");
  const std::string schemaLine = "  \"schema_version\": 1,\n";
  std::string legacy = original;
  const size_t schemaPosition = legacy.find(schemaLine);
  ASSERT_NE(schemaPosition, std::string::npos);
  legacy.erase(schemaPosition, schemaLine.size());

  const SessionGraph migrated = ParseSession(legacy);
  EXPECT_NE(SerializeSession(migrated).find(schemaLine), std::string::npos);

  std::string future = original;
  const size_t versionPosition = future.find("\"schema_version\": 1");
  ASSERT_NE(versionPosition, std::string::npos);
  future.replace(versionPosition, std::string("\"schema_version\": 1").size(),
                 "\"schema_version\": 999");
  EXPECT_THROW(ParseSession(future), std::runtime_error);
}

TEST(SessionRoundTrip, AtomicSaveKeepsBackupAndRecoversCorruptPrimary) {
  const fs::path root = fs::temp_directory_path() / "orpheus_session_atomic_test";
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path path = root / "session.json";

  SessionGraph session;
  session.set_name("First");
  session.set_session_range(0.0, 8.0);
  SaveSessionToFile(session, path.string());
  EXPECT_TRUE(fs::is_regular_file(path));
  EXPECT_FALSE(fs::exists(path.string() + ".tmp"));

  session.set_name("Second");
  SaveSessionToFile(session, path.string());
  EXPECT_EQ(LoadSessionFromFile(path.string()).name(), "Second");
  EXPECT_EQ(LoadSessionFromFile(path.string() + ".bak").name(), "First");
  EXPECT_FALSE(fs::exists(path.string() + ".bak.tmp"));

  {
    std::ofstream corrupt(path, std::ios::trunc);
    corrupt << "{not valid json";
  }
  SessionLoadResult recovered = LoadSessionWithRecovery(path.string());
  EXPECT_TRUE(recovered.recoveredFromBackup);
  EXPECT_EQ(recovered.sourcePath, path.string() + ".bak");
  EXPECT_EQ(recovered.session.name(), "First");
  fs::remove_all(root);
}

TEST(SessionRoundTrip, FutureSchemaPrimaryNeverFallsBackToBackup) {
  const fs::path root = fs::temp_directory_path() / "orpheus_session_future_schema_test";
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path path = root / "session.json";

  SessionGraph session;
  session.set_name("First");
  session.set_session_range(0.0, 8.0);
  SaveSessionToFile(session, path.string());
  session.set_name("Second");
  SaveSessionToFile(session, path.string());
  ASSERT_EQ(LoadSessionFromFile(path.string()).name(), "Second");

  std::string primary;
  {
    std::ifstream read(path);
    ASSERT_TRUE(read.is_open());
    std::ostringstream buffer;
    buffer << read.rdbuf();
    primary = buffer.str();
  }
  const size_t marker = primary.find("\"schema_version\": 1");
  ASSERT_NE(marker, std::string::npos) << "primary no longer advertises schema_version 1";
  primary.replace(marker, std::string("\"schema_version\": 1").size(), "\"schema_version\": 999");
  {
    std::ofstream write(path, std::ios::trunc);
    write << primary;
  }

  // A future schema is a hard failure and must never resolve to the backup.
  EXPECT_THROW(LoadSessionWithRecovery(path.string()), std::runtime_error);
  EXPECT_THROW(LoadSessionFromFile(path.string()), std::runtime_error);

  // Neither document may be rewritten as a side effect of the rejection.
  {
    std::ifstream after(path);
    std::ostringstream buffer;
    buffer << after.rdbuf();
    EXPECT_NE(buffer.str().find("\"schema_version\": 999"), std::string::npos);
  }
  {
    std::ifstream after(path.string() + ".bak");
    std::ostringstream buffer;
    buffer << after.rdbuf();
    EXPECT_NE(buffer.str().find("\"schema_version\": 1"), std::string::npos);
  }

  // A missing primary is ordinary I/O loss and must still recover.
  fs::remove(path);
  const SessionLoadResult recovered = LoadSessionWithRecovery(path.string());
  EXPECT_TRUE(recovered.recoveredFromBackup);
  EXPECT_EQ(recovered.sourcePath, path.string() + ".bak");
  EXPECT_EQ(recovered.session.name(), "First");

  fs::remove_all(root);
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

TEST(SessionRoundTrip, RejectsDuplicateKeysAtEveryObjectDepth) {
  for (
      const std::string text :
      {R"({"name":"A","name":"B"})",
       R"({"name":"A","tempo_bpm":120,"start_beats":0,"end_beats":1,"render":{"bit_depth":16,"bit_depth":24},"tracks":[]})",
       R"({"name":"A","tempo_bpm":120,"start_beats":0,"end_beats":1,"tracks":[{"name":"T","clips":[{"name":"C","start_beats":0,"length_beats":1,"length_beats":2}]}]})"}) {
    try {
      ParseSession(text);
      FAIL() << "Accepted duplicate";
    } catch (const std::runtime_error& error) {
      EXPECT_EQ(std::string(error.what()).find("Duplicate JSON object key: "), 0u);
    }
  }
}

TEST(SessionRoundTrip, CheckedRenderSettingsRejectFractionalNegativeAndExtremeNumbers) {
  for (const std::string field : {"sample_rate_hz", "bit_depth"}) {
    for (const std::string value : {"1.5", "-1", "4294967296", "18446744073709551616"}) {
      const std::string text =
          R"({"name":"A","tempo_bpm":120,"start_beats":0,"end_beats":1,"tracks":[],"render":{")" +
          field + "\":" + value + "}}";
      try {
        ParseSession(text);
        FAIL() << text;
      } catch (const std::invalid_argument& error) {
        EXPECT_EQ(std::string(error.what()),
                  "render." + field + (value == "1.5" ? " must be integral" : " out of range"));
      }
    }
  }
}

TEST(SessionRoundTrip, FailedLoadLeavesExistingGraphAndRevisionUntouched) {
  SessionGraph graph;
  graph.set_name("Retained");
  const auto revision = graph.revision();
  const auto path = fs::temp_directory_path() / "orpheus-session-invalid-numeric.json";
  {
    std::ofstream file(path);
    file
        << R"({"name":"Replacement","tempo_bpm":120,"start_beats":0,"end_beats":1,"tracks":[],"render":{"sample_rate_hz":48000.5}})";
  }
  EXPECT_ANY_THROW(graph = LoadSessionFromFile(path.string()));
  EXPECT_EQ(graph.name(), "Retained");
  EXPECT_EQ(graph.revision(), revision);
  fs::remove(path);
}

TEST(SessionJsonLimits, StructuralLimitsAreExplicitErrors) {
  using ::orpheus::json::JsonParseLimits;
  using ::orpheus::json::JsonParser;

  const auto parseMessage = [](const std::string& text, JsonParseLimits limits) {
    try {
      JsonParser parser(text, limits);
      static_cast<void>(parser.Parse());
    } catch (const std::runtime_error& error) {
      return std::string(error.what());
    }
    return std::string();
  };

  // Nesting depth: the 64th nested container is accepted, the 65th is not.
  const auto nested = [](int levels) {
    std::string text;
    for (int level = 0; level < levels; ++level) {
      text += "[";
    }
    text += "1";
    for (int level = 0; level < levels; ++level) {
      text += "]";
    }
    return text;
  };
  EXPECT_EQ(parseMessage(nested(64), JsonParseLimits{}), "");
  EXPECT_EQ(parseMessage(nested(65), JsonParseLimits{}), "JSON nesting depth limit exceeded");

  // Node budget: one node per parsed value, including the root.
  JsonParseLimits nodeLimits;
  nodeLimits.max_nodes = 3u;
  EXPECT_EQ(parseMessage("[1,2]", nodeLimits), "");
  EXPECT_EQ(parseMessage("[1,2,3]", nodeLimits), "JSON node budget exceeded");

  // Object member budget.
  JsonParseLimits memberLimits;
  memberLimits.max_object_members = 2u;
  EXPECT_EQ(parseMessage(R"({"a":1,"b":2})", memberLimits), "");
  EXPECT_EQ(parseMessage(R"({"a":1,"b":2,"c":3})", memberLimits),
            "JSON object member budget exceeded");

  // Array element budget.
  JsonParseLimits arrayLimits;
  arrayLimits.max_array_elements = 2u;
  EXPECT_EQ(parseMessage("[1,2]", arrayLimits), "");
  EXPECT_EQ(parseMessage("[1,2,3]", arrayLimits), "JSON array element budget exceeded");

  // A container budget is enforced before parsing (and allocating) the next
  // value, even when the node budget would otherwise fail first.
  JsonParseLimits memberBeforeNodeLimit;
  memberBeforeNodeLimit.max_object_members = 1u;
  memberBeforeNodeLimit.max_nodes = 2u;
  EXPECT_EQ(parseMessage(R"({"a":0,"b":[0]})", memberBeforeNodeLimit),
            "JSON object member budget exceeded");

  JsonParseLimits arrayBeforeNodeLimit;
  arrayBeforeNodeLimit.max_array_elements = 1u;
  arrayBeforeNodeLimit.max_nodes = 2u;
  EXPECT_EQ(parseMessage("[0,[0]]", arrayBeforeNodeLimit), "JSON array element budget exceeded");
}

TEST(SessionJsonLimits, SessionDocumentByteLimitIsAnExplicitError) {
  const std::string oversized(67108865u, ' ');
  try {
    static_cast<void>(ParseSession(oversized));
    FAIL() << "an oversized session document must be rejected";
  } catch (const std::runtime_error& error) {
    EXPECT_EQ(std::string(error.what()), "Session document exceeds maximum size of 67108864 bytes");
  }
}

TEST(SessionJsonLimits, SessionFileReadEnforcesTheDocumentByteLimit) {
  const auto path = fs::temp_directory_path() / "orpheus-session-oversized-file.json";
  {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(file.is_open());
    file.seekp(static_cast<std::streamoff>(67108864u));
    file.put('x');
    ASSERT_TRUE(file.good());
  }
  ASSERT_EQ(fs::file_size(path), 67108865u);

  try {
    static_cast<void>(LoadSessionFromFile(path.string()));
    FAIL() << "an oversized session file must be rejected";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("Session document exceeds maximum size"),
              std::string::npos);
  }
  fs::remove(path);
}

TEST(SessionJsonLimits, FailedOversizedSavePreservesPreviousDocument) {
  const auto path = fs::temp_directory_path() / "orpheus-session-save-limit.json";
  SessionGraph previous;
  previous.set_name("Preserved");
  SaveSessionToFile(previous, path.string());
  const std::string saved_document = LoadFixtureText(path);

  SessionGraph oversized;
  oversized.set_name(std::string(11184811u, '\x01'));
  EXPECT_THROW(SaveSessionToFile(oversized, path.string()), std::runtime_error);
  EXPECT_EQ(LoadFixtureText(path), saved_document);
  EXPECT_FALSE(fs::exists(path.string() + ".tmp"));
  fs::remove(path);
}

TEST(SessionJsonSemantics, RejectsUnescapedControlCharacters) {
  std::string invalid_json{"\"a"};
  invalid_json.push_back('\x01');
  invalid_json += "b\"";
  ::orpheus::json::JsonParser parser(invalid_json);
  EXPECT_THROW(static_cast<void>(parser.Parse()), std::runtime_error);
}

TEST(SessionJsonSemantics, SerializesControlCharactersAsValidEscapes) {
  SessionGraph graph;
  std::string name{"Control"};
  name.push_back('\0');
  name.push_back('\b');
  name.push_back('\f');
  name.push_back('\x01');
  name.push_back('\x1f');
  graph.set_name(name);

  const std::string json = SerializeSession(graph);
  EXPECT_NE(json.find("\\u0000"), std::string::npos);
  EXPECT_NE(json.find("\\b"), std::string::npos);
  EXPECT_NE(json.find("\\f"), std::string::npos);
  EXPECT_NE(json.find("\\u0001"), std::string::npos);
  EXPECT_NE(json.find("\\u001f"), std::string::npos);
  EXPECT_EQ(ParseSession(json).name(), name);
}

TEST(SessionRoundTrip, FailedBulkInsertDoesNotConsumeClipIds) {
  SessionGraph graph;
  Track* track = graph.add_track("Track");
  ASSERT_NE(track, nullptr);
  const ClipId existing_id = graph.add_clip(*track, "Existing", 0.0, 2.0)->id();
  const std::uint64_t revision = graph.revision();

  const std::vector<SessionGraph::ClipSpec> specs{
      {"Valid before overlap", 4.0, 1.0, 0u},
      {"Overlapping", 1.0, 2.0, 0u},
  };
  EXPECT_THROW(static_cast<void>(graph.add_clips(*track, specs)), std::invalid_argument);
  EXPECT_EQ(graph.revision(), revision);
  ASSERT_EQ(graph.tracks().front()->clips().size(), 1u);

  const ClipId next_id = graph.add_clip(*graph.tracks().front(), "Next", 2.0, 1.0)->id();
  EXPECT_EQ(next_id.raw(), existing_id.raw() + 1u);
}

TEST(SessionRoundTrip, RollbackPreservesPendingClipGridRefresh) {
  SessionGraph graph;
  Track* track = graph.add_track("Track");
  ASSERT_NE(track, nullptr);
  ASSERT_NE(graph.add_clip(*track, "Clip", 4.0, 2.0), nullptr);

  auto transaction = graph.begin_transaction();
  graph.set_name("Temporary");
  transaction.rollback();

  graph.commit_clip_grid();
  EXPECT_DOUBLE_EQ(graph.session_start_beats(), 4.0);
  EXPECT_DOUBLE_EQ(graph.session_end_beats(), 6.0);
}

TEST(SessionRoundTrip, RollbackPreservesExactBeatValues) {
  SessionGraph graph;
  Track* track = graph.add_track("Track");
  ASSERT_NE(track, nullptr);
  const double start = 0.123456789;
  const double length = 1.234567891;
  ASSERT_NE(graph.add_clip(*track, "Clip", start, length), nullptr);

  auto transaction = graph.begin_transaction();
  graph.set_name("Temporary");
  transaction.rollback();

  ASSERT_EQ(graph.tracks().size(), 1u);
  ASSERT_EQ(graph.tracks().front()->clips().size(), 1u);
  EXPECT_DOUBLE_EQ(graph.tracks().front()->clips().front()->start(), start);
  EXPECT_DOUBLE_EQ(graph.tracks().front()->clips().front()->length(), length);
}

} // namespace orpheus::core::session_json::tests
