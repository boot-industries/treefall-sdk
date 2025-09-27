// SPDX-License-Identifier: MIT
#include "json_io.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include "common/json_parser.h"

namespace orpheus::core::session_json {
namespace {

using ::orpheus::core::json::EscapeString;
using ::orpheus::core::json::ExpectArray;
using ::orpheus::core::json::ExpectObject;
using ::orpheus::core::json::FormatDouble;
using ::orpheus::core::json::JsonParser;
using ::orpheus::core::json::JsonValue;
using ::orpheus::core::json::RequireField;
using ::orpheus::core::json::RequireNumber;
using ::orpheus::core::json::RequireString;
using ::orpheus::core::json::WriteIndent;

std::string FormatSampleRateTag(std::uint32_t sample_rate_hz) {
  if (sample_rate_hz == 0u) {
    return "0k";
  }
  std::ostringstream builder;
  builder << (sample_rate_hz / 1000u);
  std::uint32_t remainder = sample_rate_hz % 1000u;
  if (remainder != 0u) {
    while (remainder % 10u == 0u) {
      remainder /= 10u;
    }
    if (remainder != 0u) {
      builder << 'p' << remainder;
    }
  }
  builder << 'k';
  return builder.str();
}

std::string SanitizeSessionName(const std::string &session_name) {
  std::string sanitized;
  sanitized.reserve(session_name.size());
  for (char c : session_name) {
    if (std::isalnum(static_cast<unsigned char>(c))) {
      sanitized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    } else if (c == '_' || c == '-' || c == ' ') {
      sanitized.push_back('_');
    }
  }
  while (sanitized.find("__") != std::string::npos) {
    const auto pos = sanitized.find("__");
    sanitized.replace(pos, 2, "_");
  }
  sanitized.erase(std::remove(sanitized.begin(), sanitized.end(), ' '),
                  sanitized.end());
  sanitized.erase(std::unique(sanitized.begin(), sanitized.end(),
                              [](char lhs, char rhs) { return lhs == '_' && rhs == '_'; }),
                  sanitized.end());
  if (sanitized.empty()) {
    sanitized = "session";
  }
  return sanitized;
}

}  // namespace

SessionGraph ParseSession(const std::string &json_text) {
  JsonParser parser(json_text);
  const JsonValue root = parser.Parse();
  const JsonValue &object = ExpectObject(root, "session root");

  SessionGraph session;
  const JsonValue *name_field = RequireField(object, "name");
  session.set_name(RequireString(*name_field, "name"));
  const JsonValue *tempo_field = RequireField(object, "tempo_bpm");
  session.set_tempo(RequireNumber(*tempo_field, "tempo_bpm"));

  const JsonValue *start_field = RequireField(object, "start_beats");
  const JsonValue *end_field = RequireField(object, "end_beats");
  const double start = RequireNumber(*start_field, "start_beats");
  const double end = RequireNumber(*end_field, "end_beats");
  session.set_session_range(start, end);

  if (auto render_it = object.object.find("render");
      render_it != object.object.end()) {
    const JsonValue &render_object =
        ExpectObject(render_it->second, "render settings");
    if (auto rate_it = render_object.object.find("sample_rate_hz");
        rate_it != render_object.object.end()) {
      const double value =
          RequireNumber(rate_it->second, "render.sample_rate_hz");
      if (value < 0.0 || value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("render.sample_rate_hz out of range");
      }
      session.set_render_sample_rate(
          static_cast<std::uint32_t>(std::llround(value)));
    }
    if (auto depth_it = render_object.object.find("bit_depth");
        depth_it != render_object.object.end()) {
      const double value = RequireNumber(depth_it->second, "render.bit_depth");
      if (value < 0.0 || value > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error("render.bit_depth out of range");
      }
      session.set_render_bit_depth(
          static_cast<std::uint16_t>(std::llround(value)));
    }
    if (auto dither_it = render_object.object.find("dither");
        dither_it != render_object.object.end()) {
      if (dither_it->second.type != JsonValue::Type::kBoolean) {
        throw std::runtime_error("render.dither must be boolean");
      }
      session.set_render_dither(dither_it->second.boolean);
    }
  }

  if (auto marker_sets_it = object.object.find("marker_sets");
      marker_sets_it != object.object.end()) {
    const JsonValue &marker_sets_value =
        ExpectArray(marker_sets_it->second, "marker_sets");
    for (const auto &marker_set_value : marker_sets_value.array) {
      const JsonValue &marker_set_object =
          ExpectObject(marker_set_value, "marker_set");
      const JsonValue *name_field = RequireField(marker_set_object, "name");
      const std::string marker_set_name =
          RequireString(*name_field, "marker_set.name");
      MarkerSet *marker_set = session.add_marker_set(marker_set_name);
      const JsonValue *markers_field =
          RequireField(marker_set_object, "markers");
      const JsonValue &markers_array =
          ExpectArray(*markers_field, "marker_set.markers");
      for (const auto &marker_value : markers_array.array) {
        const JsonValue &marker_object = ExpectObject(marker_value, "marker");
        const JsonValue *marker_name_field =
            RequireField(marker_object, "name");
        const std::string marker_name =
            RequireString(*marker_name_field, "marker.name");
        const JsonValue *marker_position_field =
            RequireField(marker_object, "position_beats");
        const double position =
            RequireNumber(*marker_position_field, "marker.position_beats");
        marker_set->add_marker(marker_name, position);
      }
    }
  }

  if (auto playlist_lanes_it = object.object.find("playlist_lanes");
      playlist_lanes_it != object.object.end()) {
    const JsonValue &playlist_lanes_array =
        ExpectArray(playlist_lanes_it->second, "playlist_lanes");
    for (const auto &lane_value : playlist_lanes_array.array) {
      const JsonValue &lane_object = ExpectObject(lane_value, "playlist_lane");
      const JsonValue *name_field = RequireField(lane_object, "name");
      const std::string lane_name =
          RequireString(*name_field, "playlist_lane.name");
      bool is_active = false;
      if (auto active_it = lane_object.object.find("is_active");
          active_it != lane_object.object.end()) {
        if (active_it->second.type != JsonValue::Type::kBoolean) {
          throw std::runtime_error(
              "playlist_lane.is_active must be boolean");
        }
        is_active = active_it->second.boolean;
      }
      session.add_playlist_lane(lane_name, is_active);
    }
  }

  const JsonValue *tracks_field = RequireField(object, "tracks");
  const JsonValue &tracks_value = ExpectArray(*tracks_field, "tracks array");
  for (const auto &track_value : tracks_value.array) {
    const JsonValue &track_object = ExpectObject(track_value, "track");
    const JsonValue *track_name_field = RequireField(track_object, "name");
    const std::string track_name =
        RequireString(*track_name_field, "track.name");
    Track *track = session.add_track(track_name);
    const JsonValue *clips_field = RequireField(track_object, "clips");
    const JsonValue &clips_value =
        ExpectArray(*clips_field, "track.clips");
    for (const auto &clip_value : clips_value.array) {
      const JsonValue &clip_object = ExpectObject(clip_value, "clip");
      const JsonValue *clip_name_field = RequireField(clip_object, "name");
      const std::string clip_name =
          RequireString(*clip_name_field, "clip.name");
      const JsonValue *clip_start_field =
          RequireField(clip_object, "start_beats");
      const JsonValue *clip_length_field =
          RequireField(clip_object, "length_beats");
      const double clip_start =
          RequireNumber(*clip_start_field, "clip.start_beats");
      const double clip_length =
          RequireNumber(*clip_length_field, "clip.length_beats");
      session.add_clip(*track, clip_name, clip_start, clip_length);
    }
  }

  session.commit_clip_grid();
  return session;
}

std::string SerializeSession(const SessionGraph &session) {
  std::ostringstream stream;
  stream << "{\n";
  WriteIndent(stream, 2);
  stream << "\"name\": \"" << EscapeString(session.name()) << "\",\n";
  WriteIndent(stream, 2);
  stream << "\"tempo_bpm\": " << FormatDouble(session.tempo()) << ",\n";
  WriteIndent(stream, 2);
  stream << "\"start_beats\": " << FormatDouble(session.session_start_beats())
         << ",\n";
  WriteIndent(stream, 2);
  stream << "\"end_beats\": " << FormatDouble(session.session_end_beats())
         << ",\n";
  WriteIndent(stream, 2);
  stream << "\"render\": {\n";
  WriteIndent(stream, 4);
  stream << "\"sample_rate_hz\": " << session.render_sample_rate() << ",\n";
  WriteIndent(stream, 4);
  stream << "\"bit_depth\": " << session.render_bit_depth() << ",\n";
  WriteIndent(stream, 4);
  stream << "\"dither\": " << (session.render_dither() ? "true" : "false")
         << "\n";
  WriteIndent(stream, 2);
  stream << "},\n";
  WriteIndent(stream, 2);
  stream << "\"marker_sets\": [\n";
  const auto &marker_sets = session.marker_sets();
  for (std::size_t marker_set_index = 0;
       marker_set_index < marker_sets.size(); ++marker_set_index) {
    const MarkerSet &marker_set = *marker_sets[marker_set_index];
    WriteIndent(stream, 4);
    stream << "{\n";
    WriteIndent(stream, 6);
    stream << "\"name\": \"" << EscapeString(marker_set.name()) << "\",\n";
    WriteIndent(stream, 6);
    stream << "\"markers\": [\n";
    const auto &markers = marker_set.markers();
    for (std::size_t marker_index = 0; marker_index < markers.size();
         ++marker_index) {
      const MarkerSet::Marker &marker = markers[marker_index];
      WriteIndent(stream, 8);
      stream << "{\n";
      WriteIndent(stream, 10);
      stream << "\"name\": \"" << EscapeString(marker.name) << "\",\n";
      WriteIndent(stream, 10);
      stream << "\"position_beats\": "
             << FormatDouble(marker.position_beats) << "\n";
      WriteIndent(stream, 8);
      stream << "}";
      if (marker_index + 1 < markers.size()) {
        stream << ",";
      }
      stream << "\n";
    }
    WriteIndent(stream, 6);
    stream << "]\n";
    WriteIndent(stream, 4);
    stream << "}";
    if (marker_set_index + 1 < marker_sets.size()) {
      stream << ",";
    }
    stream << "\n";
  }
  WriteIndent(stream, 2);
  stream << "],\n";
  WriteIndent(stream, 2);
  stream << "\"playlist_lanes\": [\n";
  const auto &playlist_lanes = session.playlist_lanes();
  for (std::size_t lane_index = 0; lane_index < playlist_lanes.size();
       ++lane_index) {
    const PlaylistLane &lane = *playlist_lanes[lane_index];
    WriteIndent(stream, 4);
    stream << "{\n";
    WriteIndent(stream, 6);
    stream << "\"name\": \"" << EscapeString(lane.name()) << "\",\n";
    WriteIndent(stream, 6);
    stream << "\"is_active\": " << (lane.is_active() ? "true" : "false")
           << "\n";
    WriteIndent(stream, 4);
    stream << "}";
    if (lane_index + 1 < playlist_lanes.size()) {
      stream << ",";
    }
    stream << "\n";
  }
  WriteIndent(stream, 2);
  stream << "],\n";
  WriteIndent(stream, 2);
  stream << "\"tracks\": [\n";

  const auto &tracks = session.tracks();
  for (std::size_t track_index = 0; track_index < tracks.size(); ++track_index) {
    const Track &track = *tracks[track_index];
    WriteIndent(stream, 4);
    stream << "{\n";
    WriteIndent(stream, 6);
    stream << "\"name\": \"" << EscapeString(track.name()) << "\",\n";
    WriteIndent(stream, 6);
    stream << "\"clips\": [\n";
    const auto &clips = track.clips();
    for (std::size_t clip_index = 0; clip_index < clips.size(); ++clip_index) {
      const Clip &clip = *clips[clip_index];
      WriteIndent(stream, 8);
      stream << "{\n";
      WriteIndent(stream, 10);
      stream << "\"name\": \"" << EscapeString(clip.name()) << "\",\n";
      WriteIndent(stream, 10);
      stream << "\"start_beats\": " << FormatDouble(clip.start()) << ",\n";
      WriteIndent(stream, 10);
      stream << "\"length_beats\": " << FormatDouble(clip.length()) << "\n";
      WriteIndent(stream, 8);
      stream << "}";
      if (clip_index + 1 < clips.size()) {
        stream << ",";
      }
      stream << "\n";
    }
    WriteIndent(stream, 6);
    stream << "]\n";
    WriteIndent(stream, 4);
    stream << "}";
    if (track_index + 1 < tracks.size()) {
      stream << ",";
    }
    stream << "\n";
  }

  WriteIndent(stream, 2);
  stream << "]\n";
  stream << "}\n";
  return stream.str();
}

SessionGraph LoadSessionFromFile(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("Unable to open session fixture: " + path);
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return ParseSession(buffer.str());
}

void SaveSessionToFile(const SessionGraph &session, const std::string &path) {
  std::ofstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("Unable to write session: " + path);
  }
  file << SerializeSession(session);
  if (!file) {
    throw std::runtime_error("Failed to write session: " + path);
  }
}

std::string MakeRenderStemFilename(const std::string &session_name,
                                   const std::string &stem_name,
                                   std::uint32_t sample_rate_hz,
                                   std::uint32_t bit_depth_bits) {
  std::string sanitized_project = SanitizeSessionName(session_name);
  std::string sanitized_stem = SanitizeSessionName(stem_name);
  if (sanitized_project.empty()) {
    sanitized_project = "session";
  }
  if (sanitized_stem.empty()) {
    sanitized_stem = "stem";
  }
  if (sample_rate_hz == 0u) {
    sample_rate_hz = 44100u;
  }
  if (bit_depth_bits == 0u) {
    bit_depth_bits = 16u;
  }
  const std::string rate_tag = FormatSampleRateTag(sample_rate_hz);
  return sanitized_project + "_" + sanitized_stem + "_" + rate_tag + "_" +
         std::to_string(bit_depth_bits) + "b.wav";
}

std::string MakeRenderClickFilename(const std::string &session_name,
                                    const std::string &stem_name,
                                    std::uint32_t sample_rate_hz,
                                    std::uint32_t bit_depth_bits) {
  return "out/" +
         MakeRenderStemFilename(session_name, stem_name, sample_rate_hz,
                                bit_depth_bits);
}

}  // namespace orpheus::core::session_json
