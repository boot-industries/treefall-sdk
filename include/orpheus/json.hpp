// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "orpheus/export.h"

namespace orpheus::json {

struct JsonValue {
  enum class Type { kNull, kObject, kArray, kString, kNumber, kBoolean };

  Type type = Type::kNull;
  double number{};
  bool boolean{};
  std::string string;
  std::map<std::string, JsonValue> object;
  std::vector<JsonValue> array;
};

/// Structural limits applied while parsing. Exceeding any limit is an
/// explicit error, not a truncation.
struct JsonParseLimits {
  std::size_t max_depth{64u};
  std::size_t max_nodes{1000000u};
  std::size_t max_object_members{1024u};
  std::size_t max_array_elements{65536u};
};

class ORPHEUS_API JsonParser {
public:
  explicit JsonParser(std::string_view input);
  explicit JsonParser(std::string_view input, JsonParseLimits limits);

  [[nodiscard]] JsonValue Parse();

private:
  [[nodiscard]] bool AtEnd() const;
  [[nodiscard]] char Peek() const;
  char Consume();
  void SkipWhitespace();

  JsonValue ParseValue();
  JsonValue ParseObject();
  JsonValue ParseArray();
  [[nodiscard]] std::string ParseString();
  [[nodiscard]] bool ParseBoolean();
  void ParseNull();
  [[nodiscard]] double ParseNumber();

  [[nodiscard]] std::string_view Slice(std::size_t start, std::size_t length) const;

  const char* data_ = nullptr;
  std::size_t size_ = 0;

  std::size_t index_ = 0;

  JsonParseLimits limits_{};
  std::size_t depth_{0u};
  std::size_t nodeCount_{0u};
};

[[nodiscard]] ORPHEUS_API const JsonValue& ExpectObject(const JsonValue& value,
                                                        const char* context);
[[nodiscard]] ORPHEUS_API const JsonValue& ExpectArray(const JsonValue& value, const char* context);
[[nodiscard]] ORPHEUS_API const JsonValue* RequireField(const JsonValue& object,
                                                        const std::string& key);
[[nodiscard]] ORPHEUS_API double RequireNumber(const JsonValue& value, const std::string& key);
[[nodiscard]] ORPHEUS_API std::string RequireString(const JsonValue& value, const std::string& key);

ORPHEUS_API std::string FormatDouble(double value);
ORPHEUS_API void WriteIndent(std::ostringstream& stream, std::size_t indent);
ORPHEUS_API std::string EscapeString(const std::string& value);

} // namespace orpheus::json
