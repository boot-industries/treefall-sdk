// SPDX-License-Identifier: MIT
#if defined(TREEFALL_SESSION_HEADER_FIRST)
#include <orpheus/session_io.h>
#include <treefall/session_io.h>
#else
#include <orpheus/session_io.h>
#include <treefall/session_io.h>
#endif

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

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
