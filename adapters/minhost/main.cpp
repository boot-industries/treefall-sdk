// SPDX-License-Identifier: MIT
#include "json_io.h"
#include "orpheus/abi.h"

#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;
namespace session_json = orpheus::core::session_json;
using orpheus::core::SessionGraph;

namespace {

struct ClickOptions {
  std::string session_path;
  std::optional<std::string> render_path;
  std::optional<double> bpm_override;
  std::uint32_t bars = 4;
};

struct RenderTracksOptions {
  std::string session_path;
  std::vector<std::string> track_names;
  fs::path output_directory;
  std::optional<double> bpm_override;
  std::optional<std::uint32_t> sample_rate_override;
  std::optional<std::uint16_t> bit_depth_override;
  std::optional<bool> dither_override;
};

void PrintUsage() {
  std::cout << "Usage:" << std::endl;
  std::cout << "  orpheus_minhost --session <session.json> [--render <out.wav>]"
            << " [--bars <count>] [--bpm <tempo>]" << std::endl;
  std::cout <<
      "  orpheus_minhost render --session <session.json> --out <dir> [--tracks"
      << " <name,name,...>] [--sample-rate <hz>] [--bit-depth <bits>]"
      << " [--dither <on|off>] [--bpm <tempo>]" << std::endl;
}

std::optional<ClickOptions> ParseClickOptions(int argc, char **argv) {
  ClickOptions options;
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

std::optional<RenderTracksOptions> ParseRenderOptions(int argc, char **argv) {
  RenderTracksOptions options;
  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--session") {
      if (i + 1 >= argc) {
        std::cerr << "--session requires a path" << std::endl;
        return std::nullopt;
      }
      options.session_path = argv[++i];
    } else if (arg == "--out") {
      if (i + 1 >= argc) {
        std::cerr << "--out requires a directory" << std::endl;
        return std::nullopt;
      }
      options.output_directory = argv[++i];
    } else if (arg == "--tracks") {
      if (i + 1 >= argc) {
        std::cerr << "--tracks requires a comma separated list" << std::endl;
        return std::nullopt;
      }
      std::stringstream stream(argv[++i]);
      std::string item;
      while (std::getline(stream, item, ',')) {
        auto begin = item.find_first_not_of(" \t");
        auto end = item.find_last_not_of(" \t");
        if (begin != std::string::npos && end != std::string::npos) {
          options.track_names.push_back(item.substr(begin, end - begin + 1));
        }
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
    } else if (arg == "--sample-rate") {
      if (i + 1 >= argc) {
        std::cerr << "--sample-rate requires a value" << std::endl;
        return std::nullopt;
      }
      const auto parsed = std::stoul(argv[++i]);
      if (parsed == 0u) {
        std::cerr << "--sample-rate must be greater than zero" << std::endl;
        return std::nullopt;
      }
      options.sample_rate_override = static_cast<std::uint32_t>(parsed);
    } else if (arg == "--bit-depth") {
      if (i + 1 >= argc) {
        std::cerr << "--bit-depth requires a value" << std::endl;
        return std::nullopt;
      }
      const auto parsed = static_cast<std::uint16_t>(std::stoul(argv[++i]));
      if (parsed != 16u && parsed != 24u) {
        std::cerr << "--bit-depth must be 16 or 24" << std::endl;
        return std::nullopt;
      }
      options.bit_depth_override = parsed;
    } else if (arg == "--dither") {
      if (i + 1 >= argc) {
        std::cerr << "--dither requires a value" << std::endl;
        return std::nullopt;
      }
      std::string value = argv[++i];
      for (char &c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      }
      if (value == "on" || value == "true" || value == "1") {
        options.dither_override = true;
      } else if (value == "off" || value == "false" || value == "0") {
        options.dither_override = false;
      } else {
        std::cerr << "--dither expects on/off" << std::endl;
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
  if (options.output_directory.empty()) {
    std::cerr << "Missing required --out argument" << std::endl;
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
  std::cout << "Orpheus Minhost (session ABI "
            << orpheus::ToString(orpheus::kSessionAbi) << ")" << std::endl;

  const bool render_tracks_mode =
      argc > 1 && std::string_view(argv[1]) == "render";

  std::optional<ClickOptions> click_options;
  std::optional<RenderTracksOptions> render_options;
  if (render_tracks_mode) {
    render_options = ParseRenderOptions(argc, argv);
    if (!render_options) {
      PrintUsage();
      return 1;
    }
  } else {
    click_options = ParseClickOptions(argc, argv);
    if (!click_options) {
      PrintUsage();
      return 1;
    }
  }

  const std::string &session_path =
      render_tracks_mode ? render_options->session_path
                         : click_options->session_path;
  const std::optional<double> bpm_override =
      render_tracks_mode ? render_options->bpm_override
                         : click_options->bpm_override;

  uint32_t session_major = 0;
  uint32_t session_minor = 0;
  const auto *session_api =
      orpheus_session_abi_v1(ORPHEUS_ABI_V1_MAJOR, &session_major,
                             &session_minor);
  uint32_t clip_major = 0;
  uint32_t clip_minor = 0;
  const auto *clipgrid_api =
      orpheus_clipgrid_abi_v1(ORPHEUS_ABI_V1_MAJOR, &clip_major, &clip_minor);
  uint32_t render_major = 0;
  uint32_t render_minor = 0;
  const auto *render_api =
      orpheus_render_abi_v1(ORPHEUS_ABI_V1_MAJOR, &render_major,
                            &render_minor);
  if (!session_api || !clipgrid_api || !render_api ||
      session_major != ORPHEUS_ABI_V1_MAJOR ||
      clip_major != ORPHEUS_ABI_V1_MAJOR ||
      render_major != ORPHEUS_ABI_V1_MAJOR ||
      session_minor != ORPHEUS_ABI_V1_MINOR ||
      clip_minor != ORPHEUS_ABI_V1_MINOR ||
      render_minor != ORPHEUS_ABI_V1_MINOR) {
    std::cerr << "ABI negotiation failed" << std::endl;
    return 1;
  }

  SessionGraph session_graph;
  try {
    session_graph = session_json::LoadSessionFromFile(session_path);
  } catch (const std::exception &ex) {
    std::cerr << "Failed to load session JSON: " << ex.what() << std::endl;
    return 1;
  }

  const double tempo = bpm_override.value_or(session_graph.tempo());

  orpheus_session_handle session_handle{};
  if (session_api->create(&session_handle) != ORPHEUS_STATUS_OK) {
    std::cerr << "Failed to create session" << std::endl;
    return 1;
  }

  struct SessionGuard {
    const orpheus_session_api_v1 *abi{};
    orpheus_session_handle handle{};
    ~SessionGuard() {
      if (abi && handle) {
        abi->destroy(handle);
      }
    }
  } guard{session_api, session_handle};

  auto *session_impl = reinterpret_cast<SessionGraph *>(session_handle);
  session_impl->set_name(session_graph.name());
  session_impl->set_render_sample_rate(session_graph.render_sample_rate());
  session_impl->set_render_bit_depth(session_graph.render_bit_depth());
  session_impl->set_render_dither(session_graph.render_dither());

  auto status = session_api->set_tempo(session_handle, tempo);
  if (status != ORPHEUS_STATUS_OK) {
    std::cerr << "Failed to set tempo: " << StatusToString(status)
              << std::endl;
    return 1;
  }

  std::unordered_set<std::string> selected_tracks;
  if (render_tracks_mode && !render_options->track_names.empty()) {
    selected_tracks.insert(render_options->track_names.begin(),
                           render_options->track_names.end());
  }

  std::size_t clip_count = 0;
  std::size_t loaded_tracks = 0;
  for (const auto &track_ptr : session_graph.tracks()) {
    if (!selected_tracks.empty() &&
        !selected_tracks.count(track_ptr->name())) {
      continue;
    }
    orpheus_track_handle track_handle{};
    const orpheus_track_desc track_desc{track_ptr->name().c_str()};
    status = session_api->add_track(session_handle, &track_desc, &track_handle);
    if (status != ORPHEUS_STATUS_OK) {
      std::cerr << "Failed to add track: " << StatusToString(status)
                << std::endl;
      return 1;
    }
    ++loaded_tracks;

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
            << loaded_tracks << " track(s) and " << clip_count
            << " clip(s)" << std::endl;

  if (render_tracks_mode) {
    if (loaded_tracks == 0) {
      std::cerr << "No tracks matched selection for rendering" << std::endl;
      return 1;
    }

    try {
      if (render_options->sample_rate_override) {
        session_impl->set_render_sample_rate(*render_options->sample_rate_override);
      }
      if (render_options->bit_depth_override) {
        session_impl->set_render_bit_depth(*render_options->bit_depth_override);
      }
      if (render_options->dither_override.has_value()) {
        session_impl->set_render_dither(*render_options->dither_override);
      }
    } catch (const std::exception &ex) {
      std::cerr << "Invalid render configuration: " << ex.what() << std::endl;
      return 1;
    }

    status = render_api->render_tracks(session_handle,
                                       render_options->output_directory.string().c_str());
    if (status != ORPHEUS_STATUS_OK) {
      std::cerr << "Track render failed: " << StatusToString(status)
                << std::endl;
      return 1;
    }

    std::cout << "Rendered stems to " << render_options->output_directory
              << std::endl;
    for (const auto &track_ptr : session_graph.tracks()) {
      if (!selected_tracks.empty() &&
          !selected_tracks.count(track_ptr->name())) {
        continue;
      }
      const std::string filename = session_json::MakeRenderStemFilename(
          session_graph.name(), track_ptr->name(),
          session_impl->render_sample_rate(),
          session_impl->render_bit_depth());
      std::cout << "  - " << (render_options->output_directory / filename)
                << std::endl;
    }
    return 0;
  }

  constexpr std::uint32_t kClickSampleRate = 44100;
  constexpr std::uint32_t kClickBitDepth = 16;

  if (click_options->render_path) {
    orpheus_render_click_spec spec{};
    spec.tempo_bpm = tempo;
    spec.bars = click_options->bars;
    spec.sample_rate = kClickSampleRate;
    spec.channels = 2;
    spec.gain = 0.3;
    spec.click_frequency_hz = 1000.0;
    spec.click_duration_seconds = 0.05;
    status = render_api->render_click(&spec, click_options->render_path->c_str());
    if (status != ORPHEUS_STATUS_OK) {
      std::cerr << "Render failed: " << StatusToString(status) << std::endl;
      return 1;
    }
    std::cout << "Rendered click track to " << *click_options->render_path
              << std::endl;
  } else {
    RunTransportSimulation(state.tempo_bpm, std::chrono::seconds(5));
    const std::string suggested = session_json::MakeRenderClickFilename(
        session_graph.name(), "click", kClickSampleRate, kClickBitDepth);
    std::cout << "Suggested render path: " << suggested << std::endl;
  }

  return 0;
}
