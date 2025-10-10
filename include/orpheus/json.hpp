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

class ORPHEUS_API JsonParser {
public:
  explicit JsonParser(std::string_view input);

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
