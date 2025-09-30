// SPDX-License-Identifier: MIT
#include "orpheus/json.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace orpheus::json {

JsonParser::JsonParser(std::string_view input) : input_(input) {}

JsonValue JsonParser::Parse() {
  SkipWhitespace();
  JsonValue value = ParseValue();
  SkipWhitespace();
  if (!AtEnd()) {
    throw std::runtime_error("Unexpected trailing data in JSON");
  }
  return value;
}

bool JsonParser::AtEnd() const { return index_ >= input_.size(); }

char JsonParser::Peek() const {
  if (AtEnd()) {
    return '\0';
  }
  return input_[index_];
}

char JsonParser::Consume() {
  if (AtEnd()) {
    throw std::runtime_error("Unexpected end of input");
  }
  return input_[index_++];
}

void JsonParser::SkipWhitespace() {
  while (!AtEnd() && std::isspace(static_cast<unsigned char>(Peek()))) {
    ++index_;
  }
}

JsonValue JsonParser::ParseValue() {
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

JsonValue JsonParser::ParseObject() {
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

JsonValue JsonParser::ParseArray() {
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

std::string JsonParser::ParseString() {
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

bool JsonParser::ParseBoolean() {
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

void JsonParser::ParseNull() {
  if (input_.substr(index_, 4) != "null") {
    throw std::runtime_error("Invalid null literal");
  }
  index_ += 4;
}

double JsonParser::ParseNumber() {
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

}  // namespace orpheus::json
