// SPDX-License-Identifier: MIT
// tools/cli/inspect_session.cpp
//
// Session Inspection CLI Tool
// Load and validate Orpheus session JSON files, print human-readable summary

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Minimal JSON parsing (no dependencies on nlohmann/json for CLI tool)
namespace json {

struct Value {
  enum Type { Null, Bool, Number, String, Array, Object };
  Type type = Null;

  bool bool_value = false;
  double num_value = 0.0;
  std::string str_value;
  std::vector<Value> array_value;
  std::vector<std::pair<std::string, Value>> object_value;

  bool isObject() const {
    return type == Object;
  }
  bool isArray() const {
    return type == Array;
  }
  bool isString() const {
    return type == String;
  }
  bool isNumber() const {
    return type == Number;
  }
  bool isBool() const {
    return type == Bool;
  }

  const Value& operator[](const std::string& key) const {
    static Value null_value;
    for (const auto& kv : object_value) {
      if (kv.first == key) {
        return kv.second;
      }
    }
    return null_value;
  }

  const Value& operator[](size_t index) const {
    static Value null_value;
    if (index < array_value.size()) {
      return array_value[index];
    }
    return null_value;
  }

  size_t size() const {
    if (type == Array) {
      return array_value.size();
    }
    if (type == Object) {
      return object_value.size();
    }
    return 0;
  }
};

// Minimal JSON parser (very basic, no error handling)
Value parse(const std::string& json_str);

} // namespace json

// Session metadata structure
struct SessionMetadata {
  std::string name;
  std::string version;
  std::string created_date;
  uint32_t sample_rate = 0;
  size_t num_clips = 0;
  size_t num_tracks = 0;
  double tempo = 0.0;
};

// Print usage information
void printUsage(const char* program_name) {
  std::cout << "Orpheus Session Inspection CLI\n\n";
  std::cout << "Usage: " << program_name << " <session.json> [options]\n\n";
  std::cout << "Options:\n";
  std::cout << "  --summary    Print summary only (default)\n";
  std::cout << "  --verbose    Print detailed clip information\n";
  std::cout << "  --validate   Validate session schema\n";
  std::cout << "  --csv        Export as CSV\n";
  std::cout << "  --help       Show this help message\n\n";
  std::cout << "Examples:\n";
  std::cout << "  " << program_name << " my_session.json\n";
  std::cout << "  " << program_name << " my_session.json --verbose\n";
  std::cout << "  " << program_name << " my_session.json --csv > output.csv\n";
}

// Load JSON file
std::string loadFile(const std::string& file_path) {
  std::ifstream file(file_path);
  if (!file.is_open()) {
    std::cerr << "Error: Could not open file: " << file_path << "\n";
    return "";
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

// Parse session metadata
SessionMetadata parseSessionMetadata(const json::Value& root) {
  SessionMetadata meta;

  if (root.isObject()) {
    const auto& session_meta = root["sessionMetadata"];
    if (session_meta.isObject()) {
      const auto& name = session_meta["name"];
      if (name.isString()) {
        meta.name = name.str_value;
      }

      const auto& version = session_meta["version"];
      if (version.isString()) {
        meta.version = version.str_value;
      }

      const auto& created = session_meta["createdDate"];
      if (created.isString()) {
        meta.created_date = created.str_value;
      }

      const auto& sample_rate = session_meta["sampleRate"];
      if (sample_rate.isNumber()) {
        meta.sample_rate = static_cast<uint32_t>(sample_rate.num_value);
      }
    }

    const auto& clips = root["clips"];
    if (clips.isArray()) {
      meta.num_clips = clips.size();
    }

    const auto& tracks = root["tracks"];
    if (tracks.isArray()) {
      meta.num_tracks = tracks.size();
    }

    const auto& tempo_map = root["tempoMap"];
    if (tempo_map.isObject()) {
      const auto& tempo = tempo_map["tempo"];
      if (tempo.isNumber()) {
        meta.tempo = tempo.num_value;
      }
    }
  }

  return meta;
}

// Print summary
void printSummary(const SessionMetadata& meta) {
  std::cout << "\n";
  std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
  std::cout << "║           ORPHEUS SESSION INSPECTION REPORT                   ║\n";
  std::cout << "╚═══════════════════════════════════════════════════════════════╝\n\n";

  std::cout << "Session Name:    " << (meta.name.empty() ? "(Unnamed)" : meta.name) << "\n";
  std::cout << "Version:         " << (meta.version.empty() ? "N/A" : meta.version) << "\n";
  std::cout << "Created:         " << (meta.created_date.empty() ? "N/A" : meta.created_date)
            << "\n";
  std::cout << "Sample Rate:     " << meta.sample_rate << " Hz\n";
  std::cout << "Tempo:           " << std::fixed << std::setprecision(2) << meta.tempo << " BPM\n";
  std::cout << "\n";
  std::cout << "Tracks:          " << meta.num_tracks << "\n";
  std::cout << "Clips:           " << meta.num_clips << "\n";
  std::cout << "\n";
}

// Print detailed clip information
void printVerbose(const json::Value& root) {
  const auto& clips = root["clips"];
  if (!clips.isArray()) {
    return;
  }

  std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
  std::cout << "║                    CLIP DETAILS                               ║\n";
  std::cout << "╚═══════════════════════════════════════════════════════════════╝\n\n";

  for (size_t i = 0; i < clips.size(); ++i) {
    const auto& clip = clips[i];

    std::cout << "Clip #" << (i + 1) << ":\n";

    const auto& name = clip["name"];
    if (name.isString()) {
      std::cout << "  Name:         " << name.str_value << "\n";
    }

    const auto& file_path = clip["filePath"];
    if (file_path.isString()) {
      std::cout << "  File:         " << file_path.str_value << "\n";
    }

    const auto& trim_in = clip["trimInSamples"];
    const auto& trim_out = clip["trimOutSamples"];
    if (trim_in.isNumber() && trim_out.isNumber()) {
      int64_t duration_samples = static_cast<int64_t>(trim_out.num_value - trim_in.num_value);
      std::cout << "  Trim IN:      " << static_cast<int64_t>(trim_in.num_value) << " samples\n";
      std::cout << "  Trim OUT:     " << static_cast<int64_t>(trim_out.num_value) << " samples\n";
      std::cout << "  Duration:     " << duration_samples << " samples\n";
    }

    const auto& gain = clip["gain"];
    if (gain.isNumber()) {
      std::cout << "  Gain:         " << std::fixed << std::setprecision(2) << gain.num_value
                << " dB\n";
    }

    const auto& fade_in = clip["fadeInSeconds"];
    const auto& fade_out = clip["fadeOutSeconds"];
    if (fade_in.isNumber() && fade_out.isNumber()) {
      std::cout << "  Fade IN:      " << std::fixed << std::setprecision(3) << fade_in.num_value
                << " s\n";
      std::cout << "  Fade OUT:     " << std::fixed << std::setprecision(3) << fade_out.num_value
                << " s\n";
    }

    std::cout << "\n";
  }
}

// Export as CSV
void exportCSV(const json::Value& root) {
  const auto& clips = root["clips"];
  if (!clips.isArray()) {
    return;
  }

  // CSV header
  std::cout << "Index,Name,FilePath,TrimInSamples,TrimOutSamples,DurationSamples,GainDB,"
               "FadeInSeconds,FadeOutSeconds\n";

  for (size_t i = 0; i < clips.size(); ++i) {
    const auto& clip = clips[i];

    std::cout << (i + 1) << ",";

    const auto& name = clip["name"];
    std::cout << (name.isString() ? name.str_value : "") << ",";

    const auto& file_path = clip["filePath"];
    std::cout << (file_path.isString() ? file_path.str_value : "") << ",";

    const auto& trim_in = clip["trimInSamples"];
    const auto& trim_out = clip["trimOutSamples"];
    int64_t trim_in_val = trim_in.isNumber() ? static_cast<int64_t>(trim_in.num_value) : 0;
    int64_t trim_out_val = trim_out.isNumber() ? static_cast<int64_t>(trim_out.num_value) : 0;
    int64_t duration = trim_out_val - trim_in_val;

    std::cout << trim_in_val << "," << trim_out_val << "," << duration << ",";

    const auto& gain = clip["gain"];
    std::cout << (gain.isNumber() ? gain.num_value : 0.0) << ",";

    const auto& fade_in = clip["fadeInSeconds"];
    std::cout << (fade_in.isNumber() ? fade_in.num_value : 0.0) << ",";

    const auto& fade_out = clip["fadeOutSeconds"];
    std::cout << (fade_out.isNumber() ? fade_out.num_value : 0.0) << "\n";
  }
}

// Validate session schema
bool validateSession(const json::Value& root) {
  bool valid = true;

  // Check required fields
  if (!root.isObject()) {
    std::cerr << "✗ Root is not an object\n";
    return false;
  }

  const auto& session_meta = root["sessionMetadata"];
  if (!session_meta.isObject()) {
    std::cerr << "✗ Missing 'sessionMetadata' object\n";
    valid = false;
  }

  const auto& clips = root["clips"];
  if (!clips.isArray()) {
    std::cerr << "✗ Missing 'clips' array\n";
    valid = false;
  } else {
    // Validate each clip
    for (size_t i = 0; i < clips.size(); ++i) {
      const auto& clip = clips[i];
      if (!clip.isObject()) {
        std::cerr << "✗ Clip #" << (i + 1) << " is not an object\n";
        valid = false;
        continue;
      }

      // Check required clip fields
      if (!clip["name"].isString()) {
        std::cerr << "✗ Clip #" << (i + 1) << " missing 'name' field\n";
        valid = false;
      }

      if (!clip["filePath"].isString()) {
        std::cerr << "✗ Clip #" << (i + 1) << " missing 'filePath' field\n";
        valid = false;
      }
    }
  }

  if (valid) {
    std::cout << "✓ Session schema is valid\n";
  } else {
    std::cout << "✗ Session schema validation failed\n";
  }

  return valid;
}

// Minimal JSON parser implementation
namespace json {

class Parser {
public:
  Parser(const std::string& str) : input(str), pos(0) {}

  Value parse() {
    skipWhitespace();
    return parseValue();
  }

private:
  const std::string& input;
  size_t pos;

  void skipWhitespace() {
    while (pos < input.size() && std::isspace(input[pos])) {
      ++pos;
    }
  }

  char peek() const {
    return pos < input.size() ? input[pos] : '\0';
  }

  char consume() {
    return pos < input.size() ? input[pos++] : '\0';
  }

  Value parseValue() {
    skipWhitespace();
    char c = peek();

    if (c == '{') {
      return parseObject();
    }
    if (c == '[') {
      return parseArray();
    }
    if (c == '"') {
      return parseString();
    }
    if (c == 't' || c == 'f') {
      return parseBool();
    }
    if (c == 'n') {
      return parseNull();
    }
    if (c == '-' || std::isdigit(c)) {
      return parseNumber();
    }

    return Value(); // Null
  }

  Value parseObject() {
    Value obj;
    obj.type = Value::Object;

    consume(); // '{'
    skipWhitespace();

    while (peek() != '}' && peek() != '\0') {
      skipWhitespace();

      // Parse key
      Value key = parseString();
      skipWhitespace();

      if (consume() != ':') {
        break; // Error
      }

      skipWhitespace();

      // Parse value
      Value value = parseValue();

      obj.object_value.push_back({key.str_value, value});

      skipWhitespace();
      if (peek() == ',') {
        consume();
      }
    }

    consume(); // '}'
    return obj;
  }

  Value parseArray() {
    Value arr;
    arr.type = Value::Array;

    consume(); // '['
    skipWhitespace();

    while (peek() != ']' && peek() != '\0') {
      arr.array_value.push_back(parseValue());

      skipWhitespace();
      if (peek() == ',') {
        consume();
      }
    }

    consume(); // ']'
    return arr;
  }

  Value parseString() {
    Value str;
    str.type = Value::String;

    consume(); // '"'

    while (peek() != '"' && peek() != '\0') {
      char c = consume();
      if (c == '\\') {
        char escaped = consume();
        switch (escaped) {
        case 'n':
          str.str_value += '\n';
          break;
        case 't':
          str.str_value += '\t';
          break;
        case '\\':
          str.str_value += '\\';
          break;
        case '"':
          str.str_value += '"';
          break;
        default:
          str.str_value += escaped;
          break;
        }
      } else {
        str.str_value += c;
      }
    }

    consume(); // '"'
    return str;
  }

  Value parseNumber() {
    Value num;
    num.type = Value::Number;

    std::string num_str;
    while (peek() == '-' || peek() == '+' || peek() == '.' || peek() == 'e' || peek() == 'E' ||
           std::isdigit(peek())) {
      num_str += consume();
    }

    num.num_value = std::stod(num_str);
    return num;
  }

  Value parseBool() {
    Value b;
    b.type = Value::Bool;

    if (peek() == 't') {
      consume();
      consume();
      consume();
      consume(); // "true"
      b.bool_value = true;
    } else {
      consume();
      consume();
      consume();
      consume();
      consume(); // "false"
      b.bool_value = false;
    }

    return b;
  }

  Value parseNull() {
    consume();
    consume();
    consume();
    consume(); // "null"
    return Value();
  }
};

Value parse(const std::string& json_str) {
  Parser parser(json_str);
  return parser.parse();
}

} // namespace json

int main(int argc, char** argv) {
  if (argc < 2) {
    printUsage(argv[0]);
    return 1;
  }

  std::string file_path = argv[1];

  // Parse command-line options
  bool verbose = false;
  bool validate = false;
  bool csv = false;

  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--help") {
      printUsage(argv[0]);
      return 0;
    } else if (arg == "--verbose") {
      verbose = true;
    } else if (arg == "--validate") {
      validate = true;
    } else if (arg == "--csv") {
      csv = true;
    } else if (arg == "--summary") {
      // Default behavior
    } else {
      std::cerr << "Unknown option: " << arg << "\n";
      printUsage(argv[0]);
      return 1;
    }
  }

  // Load JSON file
  std::string json_content = loadFile(file_path);
  if (json_content.empty()) {
    return 1;
  }

  // Parse JSON
  json::Value root = json::parse(json_content);

  // Validate if requested
  if (validate) {
    return validateSession(root) ? 0 : 1;
  }

  // Export as CSV if requested
  if (csv) {
    exportCSV(root);
    return 0;
  }

  // Parse metadata
  SessionMetadata meta = parseSessionMetadata(root);

  // Print summary
  printSummary(meta);

  // Print verbose details if requested
  if (verbose) {
    printVerbose(root);
  }

  return 0;
}
