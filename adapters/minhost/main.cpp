// SPDX-License-Identifier: MIT
#include "json_io.h"
#include "orpheus/abi.h"
#include "orpheus/errors.h"
#include "orpheus/json.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
namespace session_json = orpheus::core::session_json;
namespace json = orpheus::json;
using orpheus::core::SessionGraph;

namespace minhost {

struct ErrorInfo {
  std::string code;
  std::string message;
  std::vector<std::string> details;
};

struct TimelineRange {
  std::optional<double> start_beats;
  std::optional<double> end_beats;
  bool specified = false;
};

struct CliGlobalOptions {
  bool json_output = false;
  std::optional<std::string> session_path;
  std::optional<fs::path> spec_path;
  std::vector<std::string> track_filters;
  bool track_filters_specified = false;
  TimelineRange range;
  std::optional<std::uint32_t> sample_rate_override;
  std::optional<std::uint16_t> bit_depth_override;
};

std::string JsonEscape(std::string_view value);

void PrintJsonError(const ErrorInfo &error) {
  std::cout << "{\n";
  std::cout << "  \"error\": {\n";
  std::cout << "    \"code\": \"" << JsonEscape(error.code) << "\",\n";
  std::cout << "    \"message\": \"" << JsonEscape(error.message) << "\"";
  if (!error.details.empty()) {
    std::cout << ",\n    \"details\": [\n";
    for (std::size_t i = 0; i < error.details.size(); ++i) {
      std::cout << "      \"" << JsonEscape(error.details[i]) << "\"";
      if (i + 1 != error.details.size()) {
        std::cout << ",";
      }
      std::cout << "\n";
    }
    std::cout << "    ]\n";
  } else {
    std::cout << "\n";
  }
  std::cout << "  }\n";
  std::cout << "}" << std::endl;
}

void PrintError(const CliGlobalOptions &global, const ErrorInfo &error) {
  if (global.json_output) {
    PrintJsonError(error);
  } else {
    std::cerr << error.message << std::endl;
    for (const auto &detail : error.details) {
      std::cerr << "  " << detail << std::endl;
    }
  }
}

bool ParseDouble(const std::string &text, double &value) {
  char *end_ptr = nullptr;
  errno = 0;
  value = std::strtod(text.c_str(), &end_ptr);
  if (errno == ERANGE || end_ptr == text.c_str() || end_ptr != text.c_str() + text.size()) {
    return false;
  }
  return true;
}

bool ParseRangeArgument(const std::string &argument, TimelineRange &range, std::string &error) {
  auto colon = argument.find(':');
  double start_value = 0.0;
  double end_value = 0.0;
  TimelineRange candidate = range;
  candidate.specified = true;
  if (colon == std::string::npos) {
    if (!ParseDouble(argument, end_value) || end_value < 0.0) {
      error = "range expects non-negative numeric value or start:end";
      return false;
    }
    candidate.start_beats = 0.0;
    candidate.end_beats = end_value;
  } else {
    const std::string start_text = argument.substr(0, colon);
    const std::string end_text = argument.substr(colon + 1);
    if (!start_text.empty()) {
      if (!ParseDouble(start_text, start_value) || start_value < 0.0) {
        error = "range start must be non-negative";
        return false;
      }
      candidate.start_beats = start_value;
    }
    if (!end_text.empty()) {
      if (!ParseDouble(end_text, end_value) || end_value < 0.0) {
        error = "range end must be non-negative";
        return false;
      }
      candidate.end_beats = end_value;
    }
    if (!candidate.start_beats && !candidate.end_beats) {
      error = "range requires at least one of start or end";
      return false;
    }
    if (candidate.start_beats && candidate.end_beats &&
        *candidate.end_beats <= *candidate.start_beats) {
      error = "range end must be greater than start";
      return false;
    }
  }
  range = candidate;
  return true;
}

std::vector<std::string> SplitCommaSeparated(const std::string &value) {
  std::vector<std::string> result;
  std::stringstream stream(value);
  std::string item;
  while (std::getline(stream, item, ',')) {
    auto begin = item.find_first_not_of(" \t");
    auto end = item.find_last_not_of(" \t");
    if (begin != std::string::npos && end != std::string::npos) {
      result.emplace_back(item.substr(begin, end - begin + 1));
    }
  }
  return result;
}

std::string JsonEscape(std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (char c : value) {
    switch (c) {
      case '"':
        escaped += "\\\"";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          std::ostringstream hex;
          hex << "\\u" << std::uppercase << std::hex << std::setw(4)
              << std::setfill('0')
              << static_cast<int>(static_cast<unsigned char>(c));
          escaped += hex.str();
        } else {
          escaped += c;
        }
        break;
    }
  }
  return escaped;
}

std::string FormatNumber(double value, int precision = 6) {
  std::ostringstream stream;
  stream.setf(std::ios::fixed);
  stream.precision(precision);
  stream << value;
  return stream.str();
}

struct AbiContext {
  const orpheus_session_api_v1 *session_api = nullptr;
  const orpheus_clipgrid_api_v1 *clipgrid_api = nullptr;
  const orpheus_render_api_v1 *render_api = nullptr;
  uint32_t session_major = 0;
  uint32_t session_minor = 0;
  uint32_t clip_major = 0;
  uint32_t clip_minor = 0;
  uint32_t render_major = 0;
  uint32_t render_minor = 0;
};

void PrintNegotiationSummary(const AbiContext &abi, bool verbose) {
  if (!verbose) {
    return;
  }
  std::cout << "ABI negotiation" << std::endl;
  auto print_entry = [](const char *label, uint32_t major, uint32_t minor,
                        bool ok) {
    std::cout << "  " << std::left << std::setw(10) << label << " v" << major << "."
              << minor << (ok ? " ✅" : " ❌") << std::endl;
  };
  print_entry("session", abi.session_major, abi.session_minor,
              abi.session_api != nullptr && abi.session_major == ORPHEUS_ABI_MAJOR &&
                  abi.session_minor == ORPHEUS_ABI_MINOR);
  print_entry("clipgrid", abi.clip_major, abi.clip_minor,
              abi.clipgrid_api != nullptr && abi.clip_major == ORPHEUS_ABI_MAJOR &&
                  abi.clip_minor == ORPHEUS_ABI_MINOR);
  print_entry("render", abi.render_major, abi.render_minor,
              abi.render_api != nullptr && abi.render_major == ORPHEUS_ABI_MAJOR &&
                  abi.render_minor == ORPHEUS_ABI_MINOR);
}

bool NegotiateApis(AbiContext &abi, bool verbose, ErrorInfo &error) {
  abi.session_api = orpheus_session_abi_v1(ORPHEUS_ABI_MAJOR, &abi.session_major,
                                           &abi.session_minor);
  abi.clipgrid_api = orpheus_clipgrid_abi_v1(ORPHEUS_ABI_MAJOR, &abi.clip_major,
                                             &abi.clip_minor);
  abi.render_api = orpheus_render_abi_v1(ORPHEUS_ABI_MAJOR, &abi.render_major,
                                         &abi.render_minor);
  PrintNegotiationSummary(abi, verbose);
  if (!abi.session_api || !abi.clipgrid_api || !abi.render_api ||
      abi.session_major != ORPHEUS_ABI_MAJOR ||
      abi.clip_major != ORPHEUS_ABI_MAJOR ||
      abi.render_major != ORPHEUS_ABI_MAJOR ||
      abi.session_minor != ORPHEUS_ABI_MINOR ||
      abi.clip_minor != ORPHEUS_ABI_MINOR ||
      abi.render_minor != ORPHEUS_ABI_MINOR) {
    error.code = "abi.negotiation";
    error.message = "ABI negotiation failed";
    return false;
  }
  return true;
}

struct SessionGuard {
  const orpheus_session_api_v1 *api = nullptr;
  orpheus_session_handle handle = nullptr;

  ~SessionGuard() {
    if (api && handle) {
      api->destroy(handle);
    }
  }
};

struct SessionLoadOptions {
  std::string session_path;
  std::vector<std::string> track_filters;
  TimelineRange range;
  std::optional<std::uint32_t> render_sample_rate_override;
  std::optional<std::uint16_t> render_bit_depth_override;
  std::optional<bool> render_dither_override;
  bool require_tracks = true;
};

struct SessionContext {
  AbiContext abi;
  SessionGraph graph;
  SessionGuard guard;
  std::size_t loaded_tracks = 0;
  std::size_t loaded_clips = 0;
  std::vector<std::string> loaded_track_names;
  double tempo_bpm = 0.0;
  double range_start_beats = 0.0;
  double range_end_beats = 0.0;

  SessionGraph *session_impl() {
    return guard.handle
               ? reinterpret_cast<SessionGraph *>(guard.handle)
               : nullptr;
  }
  const SessionGraph *session_impl() const {
    return guard.handle
               ? reinterpret_cast<const SessionGraph *>(guard.handle)
               : nullptr;
  }
};

bool ClipIntersectsRange(double clip_start, double clip_length, double start_beats,
                         double end_beats) {
  const double clip_end = clip_start + clip_length;
  return clip_end > start_beats && clip_start < end_beats;
}

bool PrepareSession(const SessionLoadOptions &options, SessionContext &context,
                    bool verbose, ErrorInfo &error) {
  if (options.session_path.empty()) {
    error.code = "cli.session";
    error.message = "--session is required";
    return false;
  }

  if (!NegotiateApis(context.abi, verbose, error)) {
    return false;
  }

  try {
    context.graph = session_json::LoadSessionFromFile(options.session_path);
  } catch (const std::exception &ex) {
    error.code = "session.load";
    error.message = "Failed to load session JSON";
    error.details = {ex.what()};
    return false;
  }

  double start_beats = context.graph.session_start_beats();
  double end_beats = context.graph.session_end_beats();
  if (options.range.specified) {
    if (options.range.start_beats) {
      start_beats = *options.range.start_beats;
    }
    if (options.range.end_beats) {
      end_beats = *options.range.end_beats;
    }
    if (end_beats <= start_beats) {
      error.code = "session.range";
      error.message = "Invalid session range";
      error.details = {"end must be greater than start"};
      return false;
    }
    context.graph.set_session_range(start_beats, end_beats);
  }

  orpheus_session_handle handle = nullptr;
  auto status = context.abi.session_api->create(&handle);
  if (status != ORPHEUS_STATUS_OK) {
    error.code = "session.create";
    error.message = "Failed to create session";
    error.details = {std::string{orpheus_status_to_string(status)}};
    return false;
  }
  context.guard = SessionGuard{context.abi.session_api, handle};

  SessionGraph *session_impl = context.session_impl();
  session_impl->set_name(context.graph.name());
  session_impl->set_render_sample_rate(context.graph.render_sample_rate());
  session_impl->set_render_bit_depth(context.graph.render_bit_depth());
  session_impl->set_render_dither(context.graph.render_dither());
  session_impl->set_session_range(start_beats, end_beats);

  if (options.render_sample_rate_override) {
    try {
      session_impl->set_render_sample_rate(*options.render_sample_rate_override);
    } catch (const std::exception &ex) {
      error.code = "session.render";
      error.message = "Invalid render sample rate";
      error.details = {ex.what()};
      return false;
    }
  }
  if (options.render_bit_depth_override) {
    try {
      session_impl->set_render_bit_depth(*options.render_bit_depth_override);
    } catch (const std::exception &ex) {
      error.code = "session.render";
      error.message = "Invalid render bit depth";
      error.details = {ex.what()};
      return false;
    }
  }
  if (options.render_dither_override) {
    session_impl->set_render_dither(*options.render_dither_override);
  }

  context.tempo_bpm = context.graph.tempo();
  status = context.abi.session_api->set_tempo(context.guard.handle, context.tempo_bpm);
  if (status != ORPHEUS_STATUS_OK) {
    error.code = "session.tempo";
    error.message = "Failed to set tempo";
    error.details = {std::string{orpheus_status_to_string(status)}};
    return false;
  }

  std::unordered_set<std::string> selected_tracks;
  if (!options.track_filters.empty()) {
    selected_tracks.insert(options.track_filters.begin(), options.track_filters.end());
  }

  for (const auto &track_ptr : context.graph.tracks()) {
    if (!selected_tracks.empty() && !selected_tracks.count(track_ptr->name())) {
      continue;
    }

    const orpheus_track_desc track_desc{track_ptr->name().c_str()};
    orpheus_track_handle track_handle = nullptr;
    status =
        context.abi.session_api->add_track(context.guard.handle, &track_desc, &track_handle);
    if (status != ORPHEUS_STATUS_OK) {
      error.code = "session.track";
      error.message = "Failed to add track";
      error.details = {track_ptr->name(),
                       std::string{orpheus_status_to_string(status)}};
      return false;
    }
    ++context.loaded_tracks;
    context.loaded_track_names.push_back(track_ptr->name());

    for (const auto &clip_ptr : track_ptr->clips()) {
      if (!ClipIntersectsRange(clip_ptr->start(), clip_ptr->length(), start_beats,
                               end_beats)) {
        continue;
      }
      const orpheus_clip_desc clip_desc{clip_ptr->name().c_str(), clip_ptr->start(),
                                        clip_ptr->length(), 0u};
      orpheus_clip_handle clip_handle = nullptr;
      status = context.abi.clipgrid_api->add_clip(context.guard.handle, track_handle,
                                                  &clip_desc, &clip_handle);
      if (status != ORPHEUS_STATUS_OK) {
        error.code = "session.clip";
        error.message = "Failed to add clip";
        error.details = {clip_ptr->name(),
                         std::string{orpheus_status_to_string(status)}};
        return false;
      }
      ++context.loaded_clips;
    }
  }

  status = context.abi.clipgrid_api->commit(context.guard.handle);
  if (status != ORPHEUS_STATUS_OK) {
    error.code = "session.commit";
    error.message = "Failed to commit clip grid";
    error.details = {std::string{orpheus_status_to_string(status)}};
    return false;
  }

  orpheus_transport_state state{};
  status = context.abi.session_api->get_transport_state(context.guard.handle, &state);
  if (status != ORPHEUS_STATUS_OK) {
    error.code = "session.state";
    error.message = "Failed to query transport state";
    error.details = {std::string{orpheus_status_to_string(status)}};
    return false;
  }
  context.tempo_bpm = state.tempo_bpm;
  context.range_start_beats = start_beats;
  context.range_end_beats = end_beats;

  if (options.require_tracks && !selected_tracks.empty() && context.loaded_tracks == 0) {
    error.code = "session.tracks";
    error.message = "No tracks matched selection";
    return false;
  }
  if (options.require_tracks && context.graph.tracks().empty()) {
    error.code = "session.tracks";
    error.message = "Session does not contain any tracks";
    return false;
  }
  if (options.require_tracks && context.loaded_tracks == 0) {
    error.code = "session.tracks";
    error.message = "No tracks available in the selected range";
    return false;
  }

  return true;
}

std::string FormatBeats(double beats) {
  std::ostringstream stream;
  stream.setf(std::ios::fixed);
  stream.precision(2);
  stream << beats;
  return stream.str();
}

void PrintSessionSummary(const SessionContext &context) {
  const SessionGraph *session_impl = context.session_impl();
  std::cout << "Session: '" << context.graph.name() << "'" << std::endl;
  std::cout << "  tempo       : " << std::fixed << std::setprecision(2) << context.tempo_bpm
            << " bpm" << std::endl;
  std::cout << "  range       : " << FormatBeats(context.range_start_beats) << " → "
            << FormatBeats(context.range_end_beats) << " beats" << std::endl;
  std::cout << "  tracks      : " << context.loaded_tracks << " loaded";
  if (context.loaded_tracks < context.graph.tracks().size()) {
    std::cout << " (of " << context.graph.tracks().size() << ")";
  }
  std::cout << std::endl;
  std::cout << "  clips       : " << context.loaded_clips << std::endl;
  if (session_impl) {
    std::cout << "  render spec : " << session_impl->render_sample_rate() << " Hz, "
              << session_impl->render_bit_depth() << "-bit, dither "
              << (session_impl->render_dither() ? "on" : "off") << std::endl;
  }
}

void PrintAbiJson(const AbiContext &abi, int indent) {
  const std::string base(indent, ' ');
  const std::string inner(indent + 2, ' ');
  const bool session_ok = abi.session_api != nullptr &&
                          abi.session_major == ORPHEUS_ABI_MAJOR &&
                          abi.session_minor == ORPHEUS_ABI_MINOR;
  const bool clip_ok = abi.clipgrid_api != nullptr &&
                       abi.clip_major == ORPHEUS_ABI_MAJOR &&
                       abi.clip_minor == ORPHEUS_ABI_MINOR;
  const bool render_ok = abi.render_api != nullptr &&
                         abi.render_major == ORPHEUS_ABI_MAJOR &&
                         abi.render_minor == ORPHEUS_ABI_MINOR;
  auto print_entry = [&](const char *label, uint32_t major, uint32_t minor,
                         bool ok, bool last) {
    std::cout << inner << "\"" << label << "\": {\"major\": " << major
              << ", \"minor\": " << minor << ", \"ok\": "
              << (ok ? "true" : "false") << "}";
    if (!last) {
      std::cout << ",";
    }
    std::cout << "\n";
  };
  std::cout << base << "\"abi\": {\n";
  print_entry("session", abi.session_major, abi.session_minor, session_ok, false);
  print_entry("clipgrid", abi.clip_major, abi.clip_minor, clip_ok, false);
  print_entry("render", abi.render_major, abi.render_minor, render_ok, true);
  std::cout << base << "}";
}

SessionLoadOptions MergeSessionOptions(const CliGlobalOptions &global,
                                       const SessionLoadOptions &local) {
  SessionLoadOptions merged = local;
  if (merged.session_path.empty() && global.session_path) {
    merged.session_path = *global.session_path;
  }
  if (merged.track_filters.empty() && global.track_filters_specified) {
    merged.track_filters = global.track_filters;
  }
  if (!merged.range.specified && global.range.specified) {
    merged.range = global.range;
  }
  if (!merged.render_sample_rate_override && global.sample_rate_override) {
    merged.render_sample_rate_override = global.sample_rate_override;
  }
  if (!merged.render_bit_depth_override && global.bit_depth_override) {
    merged.render_bit_depth_override = global.bit_depth_override;
  }
  return merged;
}

void PrintGlobalHelp() {
  std::cout << "Orpheus Minhost (session ABI " << orpheus::ToString(orpheus::kSessionAbi)
            << ")" << std::endl;
  std::cout << "Usage: orpheus_minhost [global options] <command> [options]\n";
  std::cout << "Global options:\n";
  std::cout << "  --json             Emit structured JSON summaries\n";
  std::cout << "  --session <file>   Default session JSON to load\n";
  std::cout << "  --spec <file>      Default click-spec override JSON\n";
  std::cout << "  --tracks <a,b,c>   Restrict commands to the named tracks\n";
  std::cout << "  --range <start:end>Limit session range in beats\n";
  std::cout << "  --sr <hz>          Override render/click sample rate\n";
  std::cout << "  --bd <bits>        Override render bit depth (16/24/32)\n";
  std::cout << "Commands:\n";
  std::cout << "  load                 Load a session and print metadata\n";
  std::cout << "  render-click         Render a metronome click track\n";
  std::cout << "  render-tracks        Render track stems to disk\n";
  std::cout << "  simulate-transport   Run a transport simulation\n";
  std::cout << "\nUse 'orpheus_minhost <command> --help' for command options." << std::endl;
}

void PrintLoadHelp() {
  std::cout << "Usage: orpheus_minhost load --session <file.json> [options]\n";
  std::cout << "Options:\n";
  std::cout << "  --session <file>       Session JSON to load\n";
  std::cout << "  --tracks  <a,b,c>      Only load the named tracks\n";
  std::cout << "  --range   <start:end>  Limit session range in beats\n";
  std::cout << "  --sr      <hz>         Override render sample rate\n";
  std::cout << "  --bd      <bits>       Override render bit depth (16/24/32)" << std::endl;
}

void PrintRenderTracksHelp() {
  std::cout << "Usage: orpheus_minhost render-tracks --session <file.json> --out <dir> [options]\n";
  std::cout << "Options:\n";
  std::cout << "  --session <file>       Session JSON to load\n";
  std::cout << "  --out     <dir>        Directory to write rendered stems\n";
  std::cout << "  --tracks  <a,b,c>      Only render the named tracks\n";
  std::cout << "  --range   <start:end>  Limit session range in beats\n";
  std::cout << "  --sr      <hz>         Override render sample rate\n";
  std::cout << "  --bd      <bits>       Override render bit depth (16/24/32)\n";
  std::cout << "  --no-dither            Disable render dither" << std::endl;
}

void PrintRenderClickHelp() {
  std::cout << "Usage: orpheus_minhost render-click --session <file.json> [options]\n";
  std::cout << "Options:\n";
  std::cout << "  --session <file>       Session JSON to load\n";
  std::cout << "  --out     <file.wav>   Output path for rendered click\n";
  std::cout << "  --spec    <file.json>  Click render spec overrides\n";
  std::cout << "  --range   <start:end>  Override click length in beats\n";
  std::cout << "  --sr      <hz>         Override click sample rate\n";
  std::cout << "  --bd      <bits>       Hint bit depth for suggested name\n";
  std::cout << "  --tracks  <a,b,c>      Restrict session load to tracks" << std::endl;
}

void PrintSimulateTransportHelp() {
  std::cout << "Usage: orpheus_minhost simulate-transport --session <file.json> [options]\n";
  std::cout << "Options:\n";
  std::cout << "  --session <file>       Session JSON to load\n";
  std::cout << "  --range   <start:end>  Duration in beats to simulate" << std::endl;
}

bool ParseUint32(const std::string &text, std::uint32_t &value) {
  char *end_ptr = nullptr;
  errno = 0;
  unsigned long parsed = std::strtoul(text.c_str(), &end_ptr, 10);
  if (errno == ERANGE || end_ptr == text.c_str() || end_ptr != text.c_str() + text.size()) {
    return false;
  }
  value = static_cast<std::uint32_t>(parsed);
  return true;
}

bool ParseUint16(const std::string &text, std::uint16_t &value) {
  std::uint32_t temp = 0;
  if (!ParseUint32(text, temp) || temp > std::numeric_limits<std::uint16_t>::max()) {
    return false;
  }
  value = static_cast<std::uint16_t>(temp);
  return true;
}

bool ParseSessionCommonArg(const std::vector<std::string> &args, std::size_t &index,
                           SessionLoadOptions &options, bool &show_help,
                           ErrorInfo &error) {
  const std::string &arg = args[index];
  if (arg == "--session") {
    if (index + 1 >= args.size()) {
      error.code = "cli.args";
      error.message = "--session requires a path";
      return false;
    }
    options.session_path = args[++index];
    return true;
  }
  if (arg == "--tracks") {
    if (index + 1 >= args.size()) {
      error.code = "cli.args";
      error.message = "--tracks requires a comma separated list";
      return false;
    }
    options.track_filters = SplitCommaSeparated(args[++index]);
    return true;
  }
  if (arg == "--range") {
    if (index + 1 >= args.size()) {
      error.code = "cli.args";
      error.message = "--range requires a value";
      return false;
    }
    std::string parse_error;
    if (!ParseRangeArgument(args[++index], options.range, parse_error)) {
      error.code = "cli.range";
      error.message = parse_error;
      return false;
    }
    return true;
  }
  if (arg == "--help") {
    show_help = true;
    return true;
  }
  return false;
}

struct LoadCommandOptions {
  SessionLoadOptions session;
};

bool ParseLoadCommand(const std::vector<std::string> &args, LoadCommandOptions &options,
                      bool &show_help, ErrorInfo &error) {
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string &arg = args[i];
    if (ParseSessionCommonArg(args, i, options.session, show_help, error)) {
      if (!error.message.empty()) {
        return false;
      }
      continue;
    }
    if (arg == "--sr") {
      if (i + 1 >= args.size()) {
        error.code = "cli.args";
        error.message = "--sr requires a value";
        return false;
      }
      std::uint32_t sr = 0;
      if (!ParseUint32(args[++i], sr) || sr == 0u) {
        error.code = "cli.args";
        error.message = "--sr expects a positive integer";
        return false;
      }
      options.session.render_sample_rate_override = sr;
      continue;
    }
    if (arg == "--bd") {
      if (i + 1 >= args.size()) {
        error.code = "cli.args";
        error.message = "--bd requires a value";
        return false;
      }
      std::uint16_t bd = 0;
      if (!ParseUint16(args[++i], bd) ||
          (bd != 16u && bd != 24u && bd != 32u)) {
        error.code = "cli.args";
        error.message = "--bd must be 16, 24, or 32";
        return false;
      }
      options.session.render_bit_depth_override = bd;
      continue;
    }
    error.code = "cli.args";
    error.message = "Unknown argument: " + arg;
    return false;
  }
  return true;
}

int RunLoadCommand(const CliGlobalOptions &global, const LoadCommandOptions &options) {
  SessionLoadOptions session_options = MergeSessionOptions(global, options.session);
  SessionContext context;
  ErrorInfo error;
  if (!PrepareSession(session_options, context, !global.json_output, error)) {
    PrintError(global, error);
    return 1;
  }
  if (global.json_output) {
    const SessionGraph *session_impl = context.session_impl();
    std::cout << "{\n";
    std::cout << "  \"command\": \"load\",\n";
    PrintAbiJson(context.abi, 2);
    std::cout << ",\n";
    std::cout << "  \"session\": {\n";
    std::cout << "    \"name\": \"" << JsonEscape(context.graph.name()) << "\",\n";
    std::cout << "    \"tempo_bpm\": " << FormatNumber(context.tempo_bpm) << ",\n";
    std::cout << "    \"range_beats\": {\n";
    std::cout << "      \"start\": " << FormatNumber(context.range_start_beats) << ",\n";
    std::cout << "      \"end\": " << FormatNumber(context.range_end_beats) << "\n";
    std::cout << "    },\n";
    std::cout << "    \"tracks\": {\n";
    std::cout << "      \"loaded\": " << context.loaded_tracks << ",\n";
    std::cout << "      \"available\": " << context.graph.tracks().size() << ",\n";
    std::cout << "      \"names\": [";
    if (!context.loaded_track_names.empty()) {
      std::cout << "\n";
      for (std::size_t i = 0; i < context.loaded_track_names.size(); ++i) {
        std::cout << "        \"" << JsonEscape(context.loaded_track_names[i]) << "\"";
        if (i + 1 != context.loaded_track_names.size()) {
          std::cout << ",\n";
        } else {
          std::cout << "\n";
        }
      }
      std::cout << "      ";
    }
    std::cout << "]\n";
    std::cout << "    },\n";
    std::cout << "    \"clips\": " << context.loaded_clips << ",\n";
    if (session_impl) {
      std::cout << "    \"render_spec\": {\n";
      std::cout << "      \"sample_rate\": " << session_impl->render_sample_rate()
                << ",\n";
      std::cout << "      \"bit_depth\": " << session_impl->render_bit_depth() << ",\n";
      std::cout << "      \"dither\": "
                << (session_impl->render_dither() ? "true" : "false") << "\n";
      std::cout << "    }\n";
    } else {
      std::cout << "    \"render_spec\": null\n";
    }
    std::cout << "  }\n";
    std::cout << "}" << std::endl;
  } else {
    PrintSessionSummary(context);
  }
  return 0;
}

struct RenderTracksCommandOptions {
  SessionLoadOptions session;
  fs::path output_directory;
};

bool ParseRenderTracksCommand(const std::vector<std::string> &args,
                              RenderTracksCommandOptions &options, bool &show_help,
                              ErrorInfo &error) {
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string &arg = args[i];
    if (ParseSessionCommonArg(args, i, options.session, show_help, error)) {
      if (!error.message.empty()) {
        return false;
      }
      continue;
    }
    if (arg == "--out") {
      if (i + 1 >= args.size()) {
        error.code = "cli.args";
        error.message = "--out requires a directory";
        return false;
      }
      options.output_directory = args[++i];
      continue;
    }
    if (arg == "--sr") {
      if (i + 1 >= args.size()) {
        error.code = "cli.args";
        error.message = "--sr requires a value";
        return false;
      }
      std::uint32_t sr = 0;
      if (!ParseUint32(args[++i], sr) || sr == 0u) {
        error.code = "cli.args";
        error.message = "--sr expects a positive integer";
        return false;
      }
      options.session.render_sample_rate_override = sr;
      continue;
    }
    if (arg == "--bd") {
      if (i + 1 >= args.size()) {
        error.code = "cli.args";
        error.message = "--bd requires a value";
        return false;
      }
      std::uint16_t bd = 0;
      if (!ParseUint16(args[++i], bd) ||
          (bd != 16u && bd != 24u && bd != 32u)) {
        error.code = "cli.args";
        error.message = "--bd must be 16, 24, or 32";
        return false;
      }
      options.session.render_bit_depth_override = bd;
      continue;
    }
    if (arg == "--no-dither") {
      options.session.render_dither_override = false;
      continue;
    }
    error.code = "cli.args";
    error.message = "Unknown argument: " + arg;
    return false;
  }
  return true;
}

int RunRenderTracksCommand(const CliGlobalOptions &global,
                           const RenderTracksCommandOptions &options) {
  if (options.output_directory.empty()) {
    ErrorInfo error{"cli.args", "--out is required", {}};
    PrintError(global, error);
    return 1;
  }

  SessionLoadOptions session_options = MergeSessionOptions(global, options.session);
  session_options.require_tracks = true;

  SessionContext context;
  ErrorInfo error;
  if (!PrepareSession(session_options, context, !global.json_output, error)) {
    PrintError(global, error);
    return 1;
  }
  if (context.loaded_tracks == 0) {
    ErrorInfo no_tracks{"session.tracks", "No tracks available for rendering", {}};
    PrintError(global, no_tracks);
    return 1;
  }

  const auto status = context.abi.render_api->render_tracks(
      context.guard.handle, options.output_directory.string().c_str());
  if (status != ORPHEUS_STATUS_OK) {
    ErrorInfo render_error{"render.tracks", "Track render failed",
                           {std::string{orpheus_status_to_string(status)}}};
    PrintError(global, render_error);
    return 1;
  }

  std::vector<std::pair<std::string, fs::path>> stems;
  const SessionGraph *session_impl = context.session_impl();
  const std::uint32_t sample_rate =
      session_impl ? session_impl->render_sample_rate() : 0u;
  const std::uint16_t bit_depth =
      session_impl ? session_impl->render_bit_depth() : 0u;
  const bool dither = session_impl ? session_impl->render_dither() : false;
  for (const auto &track_name : context.loaded_track_names) {
    const std::string filename = session_json::MakeRenderStemFilename(
        context.graph.name(), track_name, sample_rate, bit_depth);
    stems.emplace_back(track_name, options.output_directory / filename);
  }

  if (!global.json_output) {
    std::cout << "Rendered stems to " << options.output_directory << std::endl;
    for (const auto &entry : stems) {
      std::cout << "  - " << entry.second << std::endl;
    }
  } else {
    std::cout << "{\n";
    std::cout << "  \"command\": \"render-tracks\",\n";
    PrintAbiJson(context.abi, 2);
    std::cout << ",\n";
    std::cout << "  \"output_directory\": \""
              << JsonEscape(options.output_directory.string()) << "\",\n";
    if (session_impl) {
      std::cout << "  \"render_spec\": {\n";
      std::cout << "    \"sample_rate\": " << sample_rate << ",\n";
      std::cout << "    \"bit_depth\": " << bit_depth << ",\n";
      std::cout << "    \"dither\": " << (dither ? "true" : "false") << "\n";
      std::cout << "  },\n";
    } else {
      std::cout << "  \"render_spec\": null,\n";
    }
    std::cout << "  \"stems\": [";
    if (!stems.empty()) {
      std::cout << "\n";
      for (std::size_t i = 0; i < stems.size(); ++i) {
        std::cout << "    {\"track\": \"" << JsonEscape(stems[i].first)
                  << "\", \"path\": \"" << JsonEscape(stems[i].second.string())
                  << "\"}";
        if (i + 1 != stems.size()) {
          std::cout << ",\n";
        } else {
          std::cout << "\n";
        }
      }
      std::cout << "  ";
    }
    std::cout << "]\n";
    std::cout << "}" << std::endl;
  }
  return 0;
}




struct ClickSpecOverrides {
  std::optional<double> tempo_bpm;
  std::optional<std::uint32_t> bars;
  std::optional<std::uint32_t> sample_rate;
  std::optional<std::uint32_t> channels;
  std::optional<double> gain;
  std::optional<double> click_frequency_hz;
  std::optional<double> click_duration_seconds;
  std::optional<std::string> output_path;
};

bool ParseClickSpecOverrides(const fs::path &spec_path, ClickSpecOverrides &overrides,
                             ErrorInfo &error) {
  std::ifstream stream(spec_path);
  if (!stream) {
    error.code = "spec.open";
    error.message = "Failed to open spec file";
    error.details = {spec_path.string()};
    return false;
  }
  std::stringstream buffer;
  buffer << stream.rdbuf();
  const std::string text = buffer.str();
  try {
    json::JsonParser parser(text);
    const json::JsonValue root = parser.Parse();
    const json::JsonValue &object = json::ExpectObject(root, "click spec");
    if (auto it = object.object.find("tempo_bpm"); it != object.object.end() &&
                                       it->second.type == json::JsonValue::Type::kNumber) {
      overrides.tempo_bpm = it->second.number;
    }
    if (auto it = object.object.find("bars"); it != object.object.end() &&
                                    it->second.type == json::JsonValue::Type::kNumber) {
      if (it->second.number < 0.0) {
        throw std::runtime_error("bars must be non-negative");
      }
      overrides.bars = static_cast<std::uint32_t>(std::llround(it->second.number));
    }
    if (auto it = object.object.find("sample_rate"); it != object.object.end() &&
                                           it->second.type == json::JsonValue::Type::kNumber) {
      if (it->second.number <= 0.0) {
        throw std::runtime_error("sample_rate must be positive");
      }
      overrides.sample_rate = static_cast<std::uint32_t>(std::llround(it->second.number));
    }
    if (auto it = object.object.find("channels"); it != object.object.end() &&
                                         it->second.type == json::JsonValue::Type::kNumber) {
      if (it->second.number <= 0.0) {
        throw std::runtime_error("channels must be positive");
      }
      overrides.channels = static_cast<std::uint32_t>(std::llround(it->second.number));
    }
    if (auto it = object.object.find("gain"); it != object.object.end() &&
                                      it->second.type == json::JsonValue::Type::kNumber) {
      overrides.gain = it->second.number;
    }
    if (auto it = object.object.find("click_frequency_hz");
        it != object.object.end() && it->second.type == json::JsonValue::Type::kNumber) {
      overrides.click_frequency_hz = it->second.number;
    }
    if (auto it = object.object.find("click_duration_seconds");
        it != object.object.end() && it->second.type == json::JsonValue::Type::kNumber) {
      overrides.click_duration_seconds = it->second.number;
    }
    if (auto it = object.object.find("output_path"); it != object.object.end() &&
                                            it->second.type == json::JsonValue::Type::kString) {
      overrides.output_path = it->second.string;
    }
  } catch (const std::exception &ex) {
    error.code = "spec.parse";
    error.message = "Failed to parse click spec";
    error.details = {ex.what()};
    return false;
  }
  return true;
}

struct RenderClickCommandOptions {
  SessionLoadOptions session;
  std::optional<fs::path> output_path;
  std::optional<fs::path> spec_path;
  std::optional<std::uint32_t> sample_rate_override;
  std::optional<std::uint16_t> bit_depth_hint;
};

RenderClickCommandOptions MergeRenderClickOptions(
    const CliGlobalOptions &global, const RenderClickCommandOptions &local) {
  RenderClickCommandOptions merged = local;
  merged.session = MergeSessionOptions(global, local.session);
  if (!merged.spec_path && global.spec_path) {
    merged.spec_path = global.spec_path;
  }
  if (!merged.sample_rate_override && global.sample_rate_override) {
    merged.sample_rate_override = global.sample_rate_override;
  }
  if (!merged.bit_depth_hint && global.bit_depth_override) {
    const std::uint16_t bd = *global.bit_depth_override;
    if (bd == 16u || bd == 24u) {
      merged.bit_depth_hint = bd;
    }
  }
  return merged;
}

bool ParseRenderClickCommand(const std::vector<std::string> &args,
                             RenderClickCommandOptions &options, bool &show_help,
                             ErrorInfo &error) {
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string &arg = args[i];
    if (ParseSessionCommonArg(args, i, options.session, show_help, error)) {
      if (!error.message.empty()) {
        return false;
      }
      continue;
    }
    if (arg == "--out") {
      if (i + 1 >= args.size()) {
        error.code = "cli.args";
        error.message = "--out requires a path";
        return false;
      }
      options.output_path = fs::path(args[++i]);
      continue;
    }
    if (arg == "--spec") {
      if (i + 1 >= args.size()) {
        error.code = "cli.args";
        error.message = "--spec requires a path";
        return false;
      }
      options.spec_path = fs::path(args[++i]);
      continue;
    }
    if (arg == "--sr") {
      if (i + 1 >= args.size()) {
        error.code = "cli.args";
        error.message = "--sr requires a value";
        return false;
      }
      std::uint32_t sr = 0;
      if (!ParseUint32(args[++i], sr) || sr == 0u) {
        error.code = "cli.args";
        error.message = "--sr expects a positive integer";
        return false;
      }
      options.sample_rate_override = sr;
      continue;
    }
    if (arg == "--bd") {
      if (i + 1 >= args.size()) {
        error.code = "cli.args";
        error.message = "--bd requires a value";
        return false;
      }
      std::uint16_t bd = 0;
      if (!ParseUint16(args[++i], bd) || (bd != 16u && bd != 24u)) {
        error.code = "cli.args";
        error.message = "--bd must be 16 or 24";
        return false;
      }
      options.bit_depth_hint = bd;
      continue;
    }
    error.code = "cli.args";
    error.message = "Unknown argument: " + arg;
    return false;
  }
  options.session.require_tracks = false;
  return true;
}

double BeatsToBars(double beats) {
  if (beats <= 0.0) {
    return 0.0;
  }
  return beats / 4.0;
}

std::uint32_t ComputeBarsFromRange(const SessionContext &context,
                                   const TimelineRange &range) {
  double start = context.range_start_beats;
  double end = context.range_end_beats;
  if (range.specified) {
    if (range.start_beats) {
      start = *range.start_beats;
    }
    if (range.end_beats) {
      end = *range.end_beats;
    }
  }
  const double beats = std::max(0.0, end - start);
  const double bars = BeatsToBars(beats);
  if (bars <= 0.0) {
    return 1u;
  }
  return static_cast<std::uint32_t>(std::ceil(bars));
}

int RunRenderClickCommand(const CliGlobalOptions &global,
                          const RenderClickCommandOptions &options) {
  RenderClickCommandOptions merged = MergeRenderClickOptions(global, options);
  SessionContext context;
  ErrorInfo error;
  if (!PrepareSession(merged.session, context, !global.json_output, error)) {
    PrintError(global, error);
    return 1;
  }

  orpheus_render_click_spec spec{};
  spec.tempo_bpm = context.tempo_bpm;
  spec.bars = ComputeBarsFromRange(context, merged.session.range);
  spec.sample_rate = merged.sample_rate_override.value_or(44100u);
  spec.channels = 2;
  spec.gain = 0.3;
  spec.click_frequency_hz = 1000.0;
  spec.click_duration_seconds = 0.05;

  std::optional<std::string> override_output;
  if (merged.spec_path) {
    ClickSpecOverrides overrides;
    if (!ParseClickSpecOverrides(*merged.spec_path, overrides, error)) {
      PrintError(global, error);
      return 1;
    }
    if (overrides.tempo_bpm) {
      spec.tempo_bpm = *overrides.tempo_bpm;
    }
    if (overrides.bars) {
      spec.bars = *overrides.bars;
    }
    if (overrides.sample_rate) {
      spec.sample_rate = *overrides.sample_rate;
    }
    if (overrides.channels) {
      spec.channels = *overrides.channels;
    }
    if (overrides.gain) {
      spec.gain = *overrides.gain;
    }
    if (overrides.click_frequency_hz) {
      spec.click_frequency_hz = *overrides.click_frequency_hz;
    }
    if (overrides.click_duration_seconds) {
      spec.click_duration_seconds = *overrides.click_duration_seconds;
    }
    if (overrides.output_path) {
      override_output = overrides.output_path;
    }
  }

  if (merged.sample_rate_override) {
    spec.sample_rate = *merged.sample_rate_override;
  }

  const std::uint16_t bit_depth_hint = merged.bit_depth_hint.value_or(16u);

  const auto output_path =
      merged.output_path.value_or(fs::path(override_output.value_or("")));

  if (!output_path.empty()) {
    const auto status =
        context.abi.render_api->render_click(&spec, output_path.string().c_str());
    if (status != ORPHEUS_STATUS_OK) {
      ErrorInfo render_error{"render.click", "Render failed",
                             {std::string{orpheus_status_to_string(status)}}};
      PrintError(global, render_error);
      return 1;
    }
    if (!global.json_output) {
      std::cout << "Rendered click track to " << output_path << std::endl;
    }
  } else if (!global.json_output) {
    std::cout << "Click render spec ready (no output path provided)." << std::endl;
  }

  const std::string suggested = session_json::MakeRenderClickFilename(
      context.graph.name(), "click", spec.sample_rate, bit_depth_hint);
  if (!global.json_output) {
    std::cout << "Suggested render path: " << suggested << std::endl;
  } else {
    std::cout << "{\n";
    std::cout << "  \"command\": \"render-click\",\n";
    PrintAbiJson(context.abi, 2);
    std::cout << ",\n";
    std::cout << "  \"spec\": {\n";
    std::cout << "    \"tempo_bpm\": " << FormatNumber(spec.tempo_bpm) << ",\n";
    std::cout << "    \"bars\": " << spec.bars << ",\n";
    std::cout << "    \"sample_rate\": " << spec.sample_rate << ",\n";
    std::cout << "    \"channels\": " << spec.channels << ",\n";
    std::cout << "    \"gain\": " << FormatNumber(spec.gain) << ",\n";
    std::cout << "    \"click_frequency_hz\": " << FormatNumber(spec.click_frequency_hz)
              << ",\n";
    std::cout << "    \"click_duration_seconds\": "
              << FormatNumber(spec.click_duration_seconds) << "\n";
    std::cout << "  },\n";
    if (!output_path.empty()) {
      std::cout << "  \"output_path\": \"" << JsonEscape(output_path.string())
                << "\",\n";
    } else {
      std::cout << "  \"output_path\": null,\n";
    }
    std::cout << "  \"suggested_path\": \"" << JsonEscape(suggested) << "\"\n";
    std::cout << "}" << std::endl;
  }
  return 0;
}

struct SimulateTransportCommandOptions {
  SessionLoadOptions session;
};

bool ParseSimulateTransportCommand(const std::vector<std::string> &args,
                                   SimulateTransportCommandOptions &options,
                                   bool &show_help, ErrorInfo &error) {
  for (std::size_t i = 0; i < args.size(); ++i) {
    if (ParseSessionCommonArg(args, i, options.session, show_help, error)) {
      if (!error.message.empty()) {
        return false;
      }
      continue;
    }
    error.code = "cli.args";
    error.message = "Unknown argument: " + args[i];
    return false;
  }
  options.session.require_tracks = false;
  return true;
}

void RunTransportSimulation(double tempo_bpm, std::chrono::duration<double> duration,
                            bool verbose) {
  if (tempo_bpm <= 0.0) {
    if (verbose) {
      std::cout << "Transport simulation skipped: invalid tempo" << std::endl;
    }
    return;
  }

  if (!verbose) {
    return;
  }

  const double beat_duration_seconds = 60.0 / tempo_bpm;
  const int total_beats =
      static_cast<int>(std::ceil(duration.count() / beat_duration_seconds));
  auto start = std::chrono::steady_clock::now();

  std::cout << "Simulating transport for " << std::fixed << std::setprecision(2)
            << duration.count() << " seconds" << std::endl;
  for (int beat = 0; beat < total_beats; ++beat) {
    const double elapsed = beat * beat_duration_seconds;
    const auto delta = std::chrono::duration<double>(elapsed);
    const auto next_tick =
        start + std::chrono::duration_cast<std::chrono::steady_clock::duration>(delta);
    std::this_thread::sleep_until(next_tick);
    std::cout << "[transport] beat " << (beat + 1) << " at " << std::fixed
              << std::setprecision(2) << elapsed << "s" << std::endl;
  }

  std::this_thread::sleep_until(start + std::chrono::duration_cast<std::chrono::steady_clock::duration>(duration));
  std::cout << "[transport] simulation complete" << std::endl;
}

int RunSimulateTransportCommand(const CliGlobalOptions &global,
                                const SimulateTransportCommandOptions &options) {
  SessionLoadOptions session_options = MergeSessionOptions(global, options.session);
  session_options.require_tracks = false;
  SessionContext context;
  ErrorInfo error;
  if (!PrepareSession(session_options, context, !global.json_output, error)) {
    PrintError(global, error);
    return 1;
  }

  double start = context.range_start_beats;
  double end = context.range_end_beats;
  if (session_options.range.specified) {
    if (session_options.range.start_beats) {
      start = *session_options.range.start_beats;
    }
    if (session_options.range.end_beats) {
      end = *session_options.range.end_beats;
    }
  }
  double beats = end - start;
  if (beats <= 0.0) {
    beats = 16.0;  // default 4 bars
  }
  double seconds = 0.0;
  if (context.tempo_bpm > 0.0) {
    seconds = beats * (60.0 / context.tempo_bpm);
  }
  RunTransportSimulation(context.tempo_bpm, std::chrono::duration<double>(seconds),
                         !global.json_output);
  if (global.json_output) {
    std::cout << "{\n";
    std::cout << "  \"command\": \"simulate-transport\",\n";
    PrintAbiJson(context.abi, 2);
    std::cout << ",\n";
    std::cout << "  \"tempo_bpm\": " << FormatNumber(context.tempo_bpm) << ",\n";
    std::cout << "  \"beats\": " << FormatNumber(beats) << ",\n";
    std::cout << "  \"seconds\": " << FormatNumber(seconds) << "\n";
    std::cout << "}" << std::endl;
  }
  return 0;
}

struct ParsedCommand {
  std::string name;
  std::vector<std::string> args;
  bool show_help = false;
};

bool ParseArguments(int argc, char **argv, CliGlobalOptions &global,
                    ParsedCommand &command, ErrorInfo &error) {
  std::vector<std::string> positional;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--json") {
      global.json_output = true;
      continue;
    }
    if (arg == "--session") {
      if (i + 1 >= argc) {
        error.code = "cli.args";
        error.message = "--session requires a path";
        return false;
      }
      global.session_path = argv[++i];
      continue;
    }
    if (arg == "--spec") {
      if (i + 1 >= argc) {
        error.code = "cli.args";
        error.message = "--spec requires a path";
        return false;
      }
      global.spec_path = fs::path(argv[++i]);
      continue;
    }
    if (arg == "--tracks") {
      if (i + 1 >= argc) {
        error.code = "cli.args";
        error.message = "--tracks requires a comma separated list";
        return false;
      }
      global.track_filters = SplitCommaSeparated(argv[++i]);
      global.track_filters_specified = true;
      continue;
    }
    if (arg == "--range") {
      if (i + 1 >= argc) {
        error.code = "cli.args";
        error.message = "--range requires a value";
        return false;
      }
      std::string parse_error;
      if (!ParseRangeArgument(argv[++i], global.range, parse_error)) {
        error.code = "cli.range";
        error.message = parse_error;
        return false;
      }
      continue;
    }
    if (arg == "--sr") {
      if (i + 1 >= argc) {
        error.code = "cli.args";
        error.message = "--sr requires a value";
        return false;
      }
      std::uint32_t sr = 0;
      if (!ParseUint32(argv[++i], sr) || sr == 0u) {
        error.code = "cli.args";
        error.message = "--sr expects a positive integer";
        return false;
      }
      global.sample_rate_override = sr;
      continue;
    }
    if (arg == "--bd") {
      if (i + 1 >= argc) {
        error.code = "cli.args";
        error.message = "--bd requires a value";
        return false;
      }
      std::uint16_t bd = 0;
      if (!ParseUint16(argv[++i], bd) ||
          (bd != 16u && bd != 24u && bd != 32u)) {
        error.code = "cli.args";
        error.message = "--bd must be 16, 24, or 32";
        return false;
      }
      global.bit_depth_override = bd;
      continue;
    }
    if (arg == "--help") {
      if (!command.name.empty()) {
        command.show_help = true;
      } else {
        positional.push_back(arg);
      }
      continue;
    }
    if (arg == "--") {
      for (int j = i + 1; j < argc; ++j) {
        positional.emplace_back(argv[j]);
      }
      break;
    }
    positional.push_back(std::move(arg));
  }

  if (positional.empty()) {
    error.code = "cli.command";
    error.message = "No command specified";
    return false;
  }
  command.name = positional.front();
  command.args.assign(positional.begin() + 1, positional.end());
  return true;
}

int Run(int argc, char **argv) {
  CliGlobalOptions global;
  ParsedCommand command;
  ErrorInfo parse_error;
  if (!ParseArguments(argc, argv, global, command, parse_error)) {
    if (!parse_error.message.empty()) {
      PrintError(global, parse_error);
    }
    if (!global.json_output) {
      PrintGlobalHelp();
    }
    return 1;
  }

  if (command.name == "--help" || command.name == "help") {
    PrintGlobalHelp();
    return 0;
  }

  ErrorInfo error;
  if (command.name == "load") {
    LoadCommandOptions options;
    if (!ParseLoadCommand(command.args, options, command.show_help, error)) {
      if (command.show_help) {
        PrintLoadHelp();
        return 0;
      }
      PrintError(global, error);
      return 1;
    }
    if (command.show_help) {
      PrintLoadHelp();
      return 0;
    }
    return RunLoadCommand(global, options);
  }
  if (command.name == "render-tracks") {
    RenderTracksCommandOptions options;
    if (!ParseRenderTracksCommand(command.args, options, command.show_help, error)) {
      if (command.show_help) {
        PrintRenderTracksHelp();
        return 0;
      }
      PrintError(global, error);
      return 1;
    }
    if (command.show_help) {
      PrintRenderTracksHelp();
      return 0;
    }
    return RunRenderTracksCommand(global, options);
  }
  if (command.name == "render-click") {
    RenderClickCommandOptions options;
    if (!ParseRenderClickCommand(command.args, options, command.show_help, error)) {
      if (command.show_help) {
        PrintRenderClickHelp();
        return 0;
      }
      PrintError(global, error);
      return 1;
    }
    if (command.show_help) {
      PrintRenderClickHelp();
      return 0;
    }
    return RunRenderClickCommand(global, options);
  }
  if (command.name == "simulate-transport") {
    SimulateTransportCommandOptions options;
    if (!ParseSimulateTransportCommand(command.args, options, command.show_help,
                                       error)) {
      if (command.show_help) {
        PrintSimulateTransportHelp();
        return 0;
      }
      PrintError(global, error);
      return 1;
    }
    if (command.show_help) {
      PrintSimulateTransportHelp();
      return 0;
    }
    return RunSimulateTransportCommand(global, options);
  }

  ErrorInfo unknown{"cli.command", "Unknown command: " + command.name, {}};
  PrintError(global, unknown);
  PrintGlobalHelp();
  return 1;
}

}  // namespace minhost

#ifndef ORPHEUS_MINHOST_NO_ENTRYPOINT
int main(int argc, char **argv) { return minhost::Run(argc, argv); }
#endif
