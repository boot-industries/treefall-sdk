#include "json_io.h"
#include "orpheus/abi.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
namespace session_json = orpheus::core::session_json;
using orpheus::core::SessionGraph;

namespace {

struct Options {
  std::string session_path;
  std::optional<std::string> render_path;
  std::optional<double> bpm_override;
  std::uint32_t bars = 4;
};

void PrintUsage() {
  std::cout << "Usage: orpheus_minhost --session <session.json> [--render <out.wav>]"
            << " [--bars <count>] [--bpm <tempo>]" << std::endl;
}

std::optional<Options> ParseOptions(int argc, char **argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--session") {
      if (i + 1 >= argc) {
        std::cerr << "--session requires a path" << std::endl;
        return std::nullopt;
      }
      options.session_path = argv[++i];
    } else if (arg == "--render") {
      if (i + 1 >= argc) {
        std::cerr << "--render requires a path" << std::endl;
        return std::nullopt;
      }
      options.render_path = argv[++i];
    } else if (arg == "--bars") {
      if (i + 1 >= argc) {
        std::cerr << "--bars requires a value" << std::endl;
        return std::nullopt;
      }
      options.bars = static_cast<std::uint32_t>(std::stoul(argv[++i]));
      if (options.bars == 0) {
        std::cerr << "--bars must be greater than zero" << std::endl;
        return std::nullopt;
      }
    } else if (arg == "--bpm") {
      if (i + 1 >= argc) {
        std::cerr << "--bpm requires a value" << std::endl;
        return std::nullopt;
      }
      options.bpm_override = std::stod(argv[++i]);
      if (*options.bpm_override <= 0.0) {
        std::cerr << "--bpm must be greater than zero" << std::endl;
        return std::nullopt;
      }
    } else {
      std::cerr << "Unknown argument: " << arg << std::endl;
      return std::nullopt;
    }
  }

  if (options.session_path.empty()) {
    std::cerr << "Missing required --session argument" << std::endl;
    return std::nullopt;
  }

  return options;
}

std::string StatusToString(orpheus_status status) {
  switch (status) {
    case ORPHEUS_STATUS_OK:
      return "ok";
    case ORPHEUS_STATUS_INVALID_ARGUMENT:
      return "invalid argument";
    case ORPHEUS_STATUS_NOT_FOUND:
      return "not found";
    case ORPHEUS_STATUS_OUT_OF_MEMORY:
      return "out of memory";
    case ORPHEUS_STATUS_INTERNAL_ERROR:
      return "internal error";
    case ORPHEUS_STATUS_NOT_IMPLEMENTED:
      return "not implemented";
    case ORPHEUS_STATUS_IO_ERROR:
      return "io error";
  }
  return "unknown";
}

void RunTransportSimulation(double tempo_bpm, std::chrono::seconds duration) {
  if (tempo_bpm <= 0.0) {
    std::cout << "Transport simulation skipped: invalid tempo" << std::endl;
    return;
  }

  const double beat_duration_seconds = 60.0 / tempo_bpm;
  const int total_beats = static_cast<int>(std::ceil(duration.count() /
                                                     beat_duration_seconds));
  auto start = std::chrono::steady_clock::now();

  std::cout << "Simulating transport for " << duration.count() << " seconds"
            << std::endl;
  for (int beat = 0; beat < total_beats; ++beat) {
    const double elapsed = beat * beat_duration_seconds;
    const auto delta = std::chrono::duration<double>(elapsed);
    const auto next_tick =
        start + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                     delta);
    std::this_thread::sleep_until(next_tick);
    std::cout << "[transport] beat " << (beat + 1) << " at " << std::fixed
              << std::setprecision(2) << elapsed << "s" << std::endl;
  }

  std::this_thread::sleep_until(start + duration);
  std::cout << "[transport] simulation complete" << std::endl;
}

}  // namespace

int main(int argc, char **argv) {
  std::cout << "Orpheus Minhost (ABI " << orpheus::ToString(orpheus::kCurrentAbi)
            << ")" << std::endl;

  auto parsed = ParseOptions(argc, argv);
  if (!parsed) {
    PrintUsage();
    return 1;
  }
  const Options &options = *parsed;

  const auto *session_api = orpheus_session_abi_v1();
  const auto *clipgrid_api = orpheus_clipgrid_abi_v1();
  const auto *render_api = orpheus_render_abi_v1();
  if (!session_api || !clipgrid_api || !render_api) {
    std::cerr << "ABI tables are unavailable" << std::endl;
    return 1;
  }

  SessionGraph session_graph;
  try {
    session_graph = session_json::LoadSessionFromFile(options.session_path);
  } catch (const std::exception &ex) {
    std::cerr << "Failed to load session JSON: " << ex.what() << std::endl;
    return 1;
  }

  const double tempo =
      options.bpm_override.value_or(session_graph.tempo());

  orpheus_session_handle session_handle{};
  if (session_api->create(&session_handle) != ORPHEUS_STATUS_OK) {
    std::cerr << "Failed to create session" << std::endl;
    return 1;
  }

  struct SessionGuard {
    const orpheus_session_v1 *abi{};
    orpheus_session_handle handle{};
    ~SessionGuard() {
      if (abi && handle) {
        abi->destroy(handle);
      }
    }
  } guard{session_api, session_handle};

  auto status = session_api->set_tempo(session_handle, tempo);
  if (status != ORPHEUS_STATUS_OK) {
    std::cerr << "Failed to set tempo: " << StatusToString(status)
              << std::endl;
    return 1;
  }

  std::size_t clip_count = 0;
  for (const auto &track_ptr : session_graph.tracks()) {
    orpheus_track_handle track_handle{};
    const orpheus_track_desc track_desc{track_ptr->name().c_str()};
    status = session_api->add_track(session_handle, &track_desc, &track_handle);
    if (status != ORPHEUS_STATUS_OK) {
      std::cerr << "Failed to add track: " << StatusToString(status)
                << std::endl;
      return 1;
    }

    for (const auto &clip_ptr : track_ptr->clips()) {
      const orpheus_clip_desc clip_desc{clip_ptr->name().c_str(),
                                        clip_ptr->start(),
                                        clip_ptr->length()};
      orpheus_clip_handle clip_handle{};
      status =
          clipgrid_api->add_clip(session_handle, track_handle, &clip_desc,
                                 &clip_handle);
      if (status != ORPHEUS_STATUS_OK) {
        std::cerr << "Failed to add clip: " << StatusToString(status)
                  << std::endl;
        return 1;
      }
      ++clip_count;
    }
  }

  status = clipgrid_api->commit(session_handle);
  if (status != ORPHEUS_STATUS_OK) {
    std::cerr << "Failed to commit clip grid: " << StatusToString(status)
              << std::endl;
    return 1;
  }

  orpheus_transport_state state{};
  status = session_api->get_transport_state(session_handle, &state);
  if (status != ORPHEUS_STATUS_OK) {
    std::cerr << "Failed to query transport state: "
              << StatusToString(status) << std::endl;
    return 1;
  }

  std::cout << "Loaded session '" << session_graph.name() << "' with "
            << session_graph.tracks().size() << " track(s) and " << clip_count
            << " clip(s)" << std::endl;

  if (options.render_path) {
    orpheus_render_click_spec spec{};
    spec.tempo_bpm = tempo;
    spec.bars = options.bars;
    spec.sample_rate = 44100;
    spec.channels = 2;
    spec.gain = 0.3;
    spec.click_frequency_hz = 1000.0;
    spec.click_duration_seconds = 0.05;
    status = render_api->render_click(&spec, options.render_path->c_str());
    if (status != ORPHEUS_STATUS_OK) {
      std::cerr << "Render failed: " << StatusToString(status) << std::endl;
      return 1;
    }
    std::cout << "Rendered click track to " << *options.render_path
              << std::endl;
  } else {
    RunTransportSimulation(state.tempo_bpm, std::chrono::seconds(5));
    const std::string suggested = session_json::MakeRenderClickFilename(
        session_graph.name(), tempo, options.bars);
    std::cout << "Suggested render path: " << suggested << std::endl;
  }

  return 0;
}
