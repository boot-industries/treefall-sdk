// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "orpheus/export.h"

namespace orpheus::core::json {

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

  JsonValue Parse();

 private:
  bool AtEnd() const;
  char Peek() const;
  char Consume();
  void SkipWhitespace();

  JsonValue ParseValue();
  JsonValue ParseObject();
  JsonValue ParseArray();
  std::string ParseString();
  bool ParseBoolean();
  void ParseNull();
  double ParseNumber();

  std::string_view input_;
  std::size_t index_ = 0;
};

ORPHEUS_API const JsonValue &ExpectObject(const JsonValue &value,
                                          const char *context);
ORPHEUS_API const JsonValue &ExpectArray(const JsonValue &value,
                                         const char *context);
ORPHEUS_API const JsonValue *RequireField(const JsonValue &object,
                                          const std::string &key);
ORPHEUS_API double RequireNumber(const JsonValue &value,
                                 const std::string &key);
ORPHEUS_API std::string RequireString(const JsonValue &value,
                                      const std::string &key);

ORPHEUS_API std::string FormatDouble(double value);
ORPHEUS_API void WriteIndent(std::ostringstream &stream, int indent);
ORPHEUS_API std::string EscapeString(const std::string &value);

}  // namespace orpheus::core::json
