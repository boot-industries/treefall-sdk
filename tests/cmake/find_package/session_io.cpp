// SPDX-License-Identifier: MIT
// clang-format off: include order is the contract exercised by this fixture.
#if defined(TREEFALL_SESSION_HEADER_FIRST)
#include <treefall/session_io.h>
#include <orpheus/session_io.h>
#else
#include <orpheus/session_io.h>
#include <treefall/session_io.h>
#endif
// clang-format on

#include <orpheus/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace session_json = orpheus::core::session_json;

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::string read_file(const fs::path& path) {
  std::ifstream input(path);
  require(input.is_open(), "Cannot read saved session");
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

int main() {
#if defined(TREEFALL_SESSION_HEADER_FIRST)
  const fs::path root = fs::current_path() / "treefall-session-io-data";
#else
  const fs::path root = fs::current_path() / "orpheus-session-io-data";
#endif
  try {
    fs::remove_all(root);
    fs::create_directories(root);
    const auto parser_rejects_with = [](const std::string& text,
                                        ::orpheus::json::JsonParseLimits limits,
                                        const char* expected_error) {
      try {
        ::orpheus::json::JsonParser parser(text, limits);
        static_cast<void>(parser.Parse());
      } catch (const std::runtime_error& error) {
        return std::string(error.what()) == expected_error;
      }
      return false;
    };
    ::orpheus::json::JsonParseLimits object_limits;
    object_limits.max_object_members = 1u;
    object_limits.max_nodes = 2u;
    require(parser_rejects_with(R"({"a":0,"b":[0]})", object_limits,
                                "JSON object member budget exceeded"),
            "Installed JSON parser did not enforce object limits before parsing the next value");
    ::orpheus::json::JsonParseLimits array_limits;
    array_limits.max_array_elements = 1u;
    array_limits.max_nodes = 2u;
    require(parser_rejects_with("[0,[0]]", array_limits, "JSON array element budget exceeded"),
            "Installed JSON parser did not enforce array limits before parsing the next value");
    ::orpheus::json::JsonParseLimits node_limits;
    node_limits.max_nodes = 2u;
    require(parser_rejects_with("[0,1]", node_limits, "JSON node budget exceeded"),
            "Installed JSON parser did not enforce its node budget");
    ::orpheus::json::JsonParseLimits depth_limits;
    depth_limits.max_depth = 1u;
    require(parser_rejects_with("[[0]]", depth_limits, "JSON nesting depth limit exceeded"),
            "Installed JSON parser did not enforce its nesting-depth limit");

    orpheus::core::SessionGraph bulk_graph;
    auto* bulk_track = bulk_graph.add_track("Installed bulk track");
    require(bulk_track != nullptr, "Cannot create bulk fixture track");
    const auto first_clip = bulk_graph.add_clip(*bulk_track, "First", 0.0, 1.0);
    require(first_clip != nullptr, "Cannot create first bulk fixture clip");
    const std::vector<orpheus::core::SessionGraph::ClipSpec> clip_specs{
        {"Later", 4.0, 2.0, 0u},
        {"Earlier", 2.0, 1.0, 0u},
    };
    const auto bulk_revision = bulk_graph.revision();
    const auto created_clips = bulk_graph.add_clips(*bulk_track, clip_specs);
    require(created_clips.size() == 2u && bulk_graph.revision() == bulk_revision + 1u &&
                created_clips[0]->name() == "Earlier" && created_clips[1]->name() == "Later" &&
                bulk_track->clips().size() == 3u,
            "Installed bulk clip construction did not append and return sorted clips");

    const auto failed_batch_revision = bulk_graph.revision();
    const std::vector<orpheus::core::SessionGraph::ClipSpec> invalid_specs{
        {"Valid before overlap", 8.0, 1.0, 0u},
        {"Overlapping", 0.5, 1.0, 0u},
    };
    bool batch_rejected = false;
    try {
      static_cast<void>(bulk_graph.add_clips(*bulk_track, invalid_specs));
    } catch (const std::invalid_argument&) {
      batch_rejected = true;
    }
    require(batch_rejected && bulk_graph.revision() == failed_batch_revision &&
                bulk_graph.tracks().front()->clips().size() == 3u,
            "Rejected installed bulk insertion changed the session");
    const auto next_clip =
        bulk_graph.add_clip(*bulk_graph.tracks().front(), "After rejection", 6.0, 1.0);
    require(next_clip != nullptr && next_clip->id().raw() == first_clip->id().raw() + 3u,
            "Rejected installed bulk insertion consumed clip IDs");

    const std::string original_name = bulk_graph.name();
    orpheus::TrackId rolled_back_track_id;
    {
      auto transaction = bulk_graph.begin_transaction();
      rolled_back_track_id = bulk_graph.create_track("Rolled back track");
      bulk_graph.set_name("Temporary name");
    }
    require(bulk_graph.name() == original_name && bulk_graph.tracks().size() == 1u &&
                bulk_graph.tracks().front()->clips().size() == 4u,
            "Installed transaction rollback did not restore session state");
    const auto after_rollback_track = bulk_graph.create_track("After rollback");
    require(after_rollback_track.raw() > rolled_back_track_id.raw(),
            "Installed transaction rollback regressed the track-ID allocator");
    bulk_graph.commit_clip_grid();
    require(bulk_graph.session_start_beats() == 0.0 && bulk_graph.session_end_beats() == 7.0,
            "Installed transaction rollback lost a pending clip-grid refresh");

    const auto path = root / "session.json";
    orpheus::core::SessionGraph original;
    original.set_name("First");
    original.set_tempo(100.0);
    original.set_session_range(0.0, 8.0);
    auto* track = original.add_track("Installed public track");
    require(original.add_clip(*track, "Installed public clip", 1.0, 2.0) != nullptr,
            "Cannot create fixture clip");
    original.commit_clip_grid();
    const auto text = session_json::SerializeSession(original);
    auto parsed = treefall::core::session_json::ParseSession(text);
    require(parsed.name() == "First" && parsed.tempo() == 100.0 && parsed.tracks().size() == 1 &&
                parsed.tracks().front()->clips().size() == 1,
            "Public parser did not preserve the session");
    require(parsed.tracks().front()->clips().front()->name() == "Installed public clip" &&
                parsed.tracks().front()->clips().front()->start() == 1.0 &&
                parsed.tracks().front()->clips().front()->length() == 2.0,
            "Public parser changed the clip's canonical timeline");

    session_json::SaveSessionToFile(parsed, path.string());
    auto primary = session_json::LoadSessionWithRecovery(path.string());
    require(!primary.recoveredFromBackup && primary.sourcePath == path.string() &&
                primary.session.name() == "First",
            "Valid primary was not loaded directly");
    parsed.set_name("Second");
    treefall::core::session_json::SaveSessionToFile(parsed, path.string());
    const auto backup = fs::path(path.string() + ".bak");
    require(session_json::LoadSessionFromFile(path.string()).name() == "Second" &&
                session_json::LoadSessionFromFile(backup.string()).name() == "First",
            "Atomic save did not preserve the previous session as backup");
    const auto backup_before = read_file(backup);

    auto future = read_file(path);
    const auto schema = future.find("\"schema_version\": 1");
    require(schema != std::string::npos, "Saved session is missing its schema");
    future.replace(schema, std::string("\"schema_version\": 1").size(), "\"schema_version\": 999");
    {
      std::ofstream output(path, std::ios::trunc);
      output << future;
      require(static_cast<bool>(output), "Cannot write future-schema fixture");
    }
    bool future_rejected = false;
    try {
      (void)session_json::LoadSessionWithRecovery(path.string());
    } catch (const std::runtime_error&) {
      future_rejected = true;
    }
    require(future_rejected, "Future schema incorrectly fell back to backup");
    require(read_file(path) == future && read_file(backup) == backup_before,
            "Future-schema rejection mutated persisted documents");
    {
      std::ofstream corrupt(path, std::ios::trunc);
      corrupt << "{not valid json";
      require(static_cast<bool>(corrupt), "Cannot write corrupt-primary fixture");
    }
    auto recovered = treefall::core::session_json::LoadSessionWithRecovery(path.string());
    require(recovered.recoveredFromBackup && recovered.sourcePath == backup.string() &&
                recovered.session.name() == "First",
            "Corrupt primary was not recovered from backup");
    fs::remove(path);
    auto missing = session_json::LoadSessionWithRecovery(path.string());
    require(missing.recoveredFromBackup && missing.session.name() == "First",
            "Missing primary was not recovered from backup");
    fs::remove_all(root);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    std::error_code ignored;
    fs::remove_all(root, ignored);
    return 1;
  }
}
