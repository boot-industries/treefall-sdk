// SPDX-License-Identifier: MIT
#include "json_io.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace orpheus::core::session_json {
namespace {

struct JsonValue {
  enum class Type { kNull, kObject, kArray, kString, kNumber, kBoolean };

  Type type = Type::kNull;
  double number{};
  bool boolean{};
  std::string string;
  std::map<std::string, JsonValue> object;
  std::vector<JsonValue> array;
};

class JsonParser {
 public:
  explicit JsonParser(std::string_view input) : input_(input) {}

  JsonValue Parse() {
    SkipWhitespace();
    JsonValue value = ParseValue();
    SkipWhitespace();
    if (!AtEnd()) {
      throw std::runtime_error("Unexpected trailing data in JSON");
    }
    return value;
  }

 private:
  bool AtEnd() const { return index_ >= input_.size(); }

  char Peek() const {
    if (AtEnd()) {
      return '\0';
    }
    return input_[index_];
  }

  char Consume() {
    if (AtEnd()) {
      throw std::runtime_error("Unexpected end of input");
    }
    return input_[index_++];
  }

  void SkipWhitespace() {
    while (!AtEnd() && std::isspace(static_cast<unsigned char>(Peek()))) {
      ++index_;
    }
  }

  JsonValue ParseValue() {
    SkipWhitespace();
    const char c = Peek();
    switch (c) {
      case '{':
        return ParseObject();
      case '[':
        return ParseArray();
      case '"': {
        JsonValue value;
        value.type = JsonValue::Type::kString;
        value.string = ParseString();
        return value;
      }
      case 't':
      case 'f': {
        JsonValue value;
        value.type = JsonValue::Type::kBoolean;
        value.boolean = ParseBoolean();
        return value;
      }
      case 'n':
        ParseNull();
        return {};
      default:
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
          JsonValue value;
          value.type = JsonValue::Type::kNumber;
          value.number = ParseNumber();
          return value;
        }
        break;
    }
    throw std::runtime_error("Unsupported JSON token");
  }

  JsonValue ParseObject() {
    JsonValue value;
    value.type = JsonValue::Type::kObject;
    Consume();  // '{'
    SkipWhitespace();
    if (Peek() == '}') {
      Consume();
      return value;
    }
    while (true) {
      SkipWhitespace();
      if (Peek() != '"') {
        throw std::runtime_error("JSON object keys must be strings");
      }
      const std::string key = ParseString();
      SkipWhitespace();
      if (Consume() != ':') {
        throw std::runtime_error("Expected ':' after object key");
      }
      JsonValue element = ParseValue();
      value.object.emplace(key, std::move(element));
      SkipWhitespace();
      const char next = Consume();
      if (next == '}') {
        break;
      }
      if (next != ',') {
        throw std::runtime_error("Expected ',' between object elements");
      }
    }
    return value;
  }

  JsonValue ParseArray() {
    JsonValue value;
    value.type = JsonValue::Type::kArray;
    Consume();  // '['
    SkipWhitespace();
    if (Peek() == ']') {
      Consume();
      return value;
    }
    while (true) {
      JsonValue element = ParseValue();
      value.array.push_back(std::move(element));
      SkipWhitespace();
      const char next = Consume();
      if (next == ']') {
        break;
      }
      if (next != ',') {
        throw std::runtime_error("Expected ',' between array elements");
      }
      SkipWhitespace();
    }
    return value;
  }

  std::string ParseString() {
    if (Consume() != '"') {
      throw std::runtime_error("Expected string opening quote");
    }
    std::string result;
    while (true) {
      if (AtEnd()) {
        throw std::runtime_error("Unterminated string literal");
      }
      const char c = Consume();
      if (c == '"') {
        break;
      }
      if (c == '\\') {
        if (AtEnd()) {
          throw std::runtime_error("Unterminated escape sequence");
        }
        const char esc = Consume();
        switch (esc) {
          case '"':
          case '\\':
          case '/':
            result.push_back(esc);
            break;
          case 'b':
            result.push_back('\b');
            break;
          case 'f':
            result.push_back('\f');
            break;
          case 'n':
            result.push_back('\n');
            break;
          case 'r':
            result.push_back('\r');
            break;
          case 't':
            result.push_back('\t');
            break;
          case 'u': {
            unsigned int codepoint = 0;
            for (int i = 0; i < 4; ++i) {
              if (AtEnd()) {
                throw std::runtime_error("Invalid unicode escape");
              }
              const char hex = Consume();
              codepoint <<= 4;
              if (hex >= '0' && hex <= '9') {
                codepoint |= static_cast<unsigned int>(hex - '0');
              } else if (hex >= 'a' && hex <= 'f') {
                codepoint |= static_cast<unsigned int>(hex - 'a' + 10);
              } else if (hex >= 'A' && hex <= 'F') {
                codepoint |= static_cast<unsigned int>(hex - 'A' + 10);
              } else {
                throw std::runtime_error("Invalid unicode escape");
              }
            }
            if (codepoint <= 0x7F) {
              result.push_back(static_cast<char>(codepoint));
            } else {
              throw std::runtime_error("Only ASCII unicode escapes supported");
            }
            break;
          }
          default:
            throw std::runtime_error("Unsupported escape sequence");
        }
      } else {
        result.push_back(c);
      }
    }
    return result;
  }

  bool ParseBoolean() {
    if (input_.substr(index_, 4) == "true") {
      index_ += 4;
      return true;
    }
    if (input_.substr(index_, 5) == "false") {
      index_ += 5;
      return false;
    }
    throw std::runtime_error("Invalid boolean literal");
  }

  void ParseNull() {
    if (input_.substr(index_, 4) != "null") {
      throw std::runtime_error("Invalid null literal");
    }
    index_ += 4;
  }

  double ParseNumber() {
    const std::size_t start = index_;
    if (Peek() == '-') {
      ++index_;
    }
    if (!std::isdigit(static_cast<unsigned char>(Peek()))) {
      throw std::runtime_error("Invalid number");
    }
    if (Peek() == '0') {
      ++index_;
    } else {
      while (std::isdigit(static_cast<unsigned char>(Peek()))) {
        ++index_;
      }
    }
    if (Peek() == '.') {
      ++index_;
      if (!std::isdigit(static_cast<unsigned char>(Peek()))) {
        throw std::runtime_error("Invalid fractional part");
      }
      while (std::isdigit(static_cast<unsigned char>(Peek()))) {
        ++index_;
      }
    }
    if (Peek() == 'e' || Peek() == 'E') {
      ++index_;
      if (Peek() == '+' || Peek() == '-') {
        ++index_;
      }
      if (!std::isdigit(static_cast<unsigned char>(Peek()))) {
        throw std::runtime_error("Invalid exponent");
      }
      while (std::isdigit(static_cast<unsigned char>(Peek()))) {
        ++index_;
      }
    }
    const std::string_view number_view = input_.substr(start, index_ - start);
    const std::string number_string(number_view);
    errno = 0;
    char *end_ptr = nullptr;
    const double value = std::strtod(number_string.c_str(), &end_ptr);
    if (end_ptr != number_string.c_str() + number_string.size() || errno == ERANGE ||
        end_ptr == number_string.c_str()) {
      throw std::runtime_error("Failed to parse number");
    }
    return value;
  }

  std::string_view input_;
  std::size_t index_ = 0;
};

const JsonValue &ExpectObject(const JsonValue &value, const char *context) {
  if (value.type != JsonValue::Type::kObject) {
    throw std::runtime_error(std::string("Expected object for ") + context);
  }
  return value;
}

const JsonValue &ExpectArray(const JsonValue &value, const char *context) {
  if (value.type != JsonValue::Type::kArray) {
    throw std::runtime_error(std::string("Expected array for ") + context);
  }
  return value;
}

const JsonValue *RequireField(const JsonValue &object, const std::string &key) {
  const auto it = object.object.find(key);
  if (it == object.object.end()) {
    throw std::runtime_error("Missing field: " + key);
  }
  return &it->second;
}

double RequireNumber(const JsonValue &value, const std::string &key) {
  if (value.type != JsonValue::Type::kNumber) {
    throw std::runtime_error("Expected numeric field: " + key);
  }
  return value.number;
}

std::string RequireString(const JsonValue &value, const std::string &key) {
  if (value.type != JsonValue::Type::kString) {
    throw std::runtime_error("Expected string field: " + key);
  }
  return value.string;
}

std::string FormatDouble(double value) {
  std::ostringstream stream;
  stream.setf(std::ios::fixed);
  stream.precision(6);
  stream << value;
  std::string text = stream.str();
  if (auto dot = text.find('.'); dot != std::string::npos) {
    while (!text.empty() && text.back() == '0') {
      text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
      text.pop_back();
    }
  }
  if (text.empty()) {
    return "0";
  }
  return text;
}

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

void WriteIndent(std::ostringstream &stream, int indent) {
  stream << std::string(indent, ' ');
}

std::string EscapeString(const std::string &value) {
  std::string result;
  result.reserve(value.size());
  for (char c : value) {
    switch (c) {
      case '"':
        result += "\\\"";
        break;
      case '\\':
        result += "\\\\";
        break;
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      default:
        result.push_back(c);
        break;
    }
  }
  return result;
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

std::string MakeRenderClickFilename(const std::string &session_name,
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
  return "out/" + sanitized_project + "_" + sanitized_stem + "_" + rate_tag +
         "_" + std::to_string(bit_depth_bits) + "b.wav";
}

}  // namespace orpheus::core::session_json
