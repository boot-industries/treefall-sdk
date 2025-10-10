// SPDX-License-Identifier: MIT
#include "reconform_plan.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "common/json_parser.h"

namespace orpheus::core::otio {
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

ReconformTimeRange ParseTimeRange(const JsonValue& value, const std::string& context) {
  const JsonValue& object = ExpectObject(value, context.c_str());
  const JsonValue* start_field = RequireField(object, "start_seconds");
  const JsonValue* duration_field = RequireField(object, "duration_seconds");
  return ReconformTimeRange{RequireNumber(*start_field, context + ".start_seconds"),
                            RequireNumber(*duration_field, context + ".duration_seconds")};
}

void WriteTimeRange(std::ostringstream& stream, std::size_t indent, const std::string& name,
                    const ReconformTimeRange& range) {
  WriteIndent(stream, indent);
  stream << '"' << name << '"' << ": {\n";
  WriteIndent(stream, indent + 2);
  stream << "\"start_seconds\": " << FormatDouble(range.start_seconds) << ",\n";
  WriteIndent(stream, indent + 2);
  stream << "\"duration_seconds\": " << FormatDouble(range.duration_seconds) << "\n";
  WriteIndent(stream, indent);
  stream << '}';
}

std::string OperationKindString(const ReconformOperationData& data) {
  if (std::holds_alternative<ReconformInsert>(data)) {
    return "insert";
  }
  if (std::holds_alternative<ReconformDelete>(data)) {
    return "delete";
  }
  return "retime";
}

ReconformInsert ParseInsertOperation(const JsonValue& object, const std::string& context) {
  const JsonValue* target_field = RequireField(object, "target");
  const JsonValue* source_field = RequireField(object, "source");
  return ReconformInsert{ParseTimeRange(*target_field, context + ".target"),
                         ParseTimeRange(*source_field, context + ".source")};
}

ReconformDelete ParseDeleteOperation(const JsonValue& object, const std::string& context) {
  const JsonValue* target_field = RequireField(object, "target");
  return ReconformDelete{ParseTimeRange(*target_field, context + ".target")};
}

ReconformRetime ParseRetimeOperation(const JsonValue& object, const std::string& context) {
  const JsonValue* target_field = RequireField(object, "target");
  const JsonValue* duration_field = RequireField(object, "retimed_duration_seconds");
  ReconformRetime result{};
  result.target = ParseTimeRange(*target_field, context + ".target");
  result.retimed_duration_seconds =
      RequireNumber(*duration_field, context + ".retimed_duration_seconds");
  return result;
}

} // namespace

ReconformPlan ParseReconformPlan(const std::string& json_text) {
  JsonParser parser(json_text);
  const JsonValue root = parser.Parse();
  const JsonValue& object = ExpectObject(root, "reconform_plan");

  ReconformPlan plan;
  if (auto version_it = object.object.find("version"); version_it != object.object.end()) {
    const double version_value = RequireNumber(version_it->second, "plan.version");
    if (version_value < 0.0 ||
        version_value > static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
      throw std::runtime_error("plan.version out of range");
    }
    const double rounded = std::round(version_value);
    if (std::fabs(version_value - rounded) > 1e-6) {
      throw std::runtime_error("plan.version must be integral");
    }
    plan.version = static_cast<std::uint32_t>(rounded);
  }

  const JsonValue* timeline_field = RequireField(object, "timeline");
  plan.timeline_name = RequireString(*timeline_field, "plan.timeline");

  const JsonValue* operations_field = RequireField(object, "operations");
  const JsonValue& operations_array = ExpectArray(*operations_field, "plan.operations");
  plan.operations.reserve(operations_array.array.size());
  for (std::size_t index = 0; index < operations_array.array.size(); ++index) {
    const JsonValue& op_value = operations_array.array[index];
    const JsonValue& op_object = ExpectObject(op_value, "plan.operation");
    const JsonValue* kind_field = RequireField(op_object, "kind");
    const std::string kind = RequireString(*kind_field, "plan.operation.kind");

    ReconformOperation operation;
    if (auto note_it = op_object.object.find("note"); note_it != op_object.object.end()) {
      operation.note = RequireString(note_it->second, "plan.operation.note");
    }

    const std::string context = "plan.operations[" + std::to_string(index) + "]";
    if (kind == "insert") {
      operation.data = ParseInsertOperation(op_object, context);
    } else if (kind == "delete") {
      operation.data = ParseDeleteOperation(op_object, context);
    } else if (kind == "retime") {
      operation.data = ParseRetimeOperation(op_object, context);
    } else {
      throw std::runtime_error("Unknown reconform operation kind: " + kind);
    }
    plan.operations.push_back(std::move(operation));
  }

  return plan;
}

std::string SerializeReconformPlan(const ReconformPlan& plan) {
  std::ostringstream stream;
  stream << "{\n";
  WriteIndent(stream, 2);
  stream << "\"version\": " << plan.version << ",\n";
  WriteIndent(stream, 2);
  stream << "\"timeline\": \"" << EscapeString(plan.timeline_name) << "\",\n";
  WriteIndent(stream, 2);
  stream << "\"operations\": [\n";

  for (std::size_t index = 0; index < plan.operations.size(); ++index) {
    const ReconformOperation& operation = plan.operations[index];
    WriteIndent(stream, 4);
    stream << "{\n";

    std::vector<std::string> field_blocks;

    {
      std::ostringstream field;
      WriteIndent(field, 6);
      field << "\"kind\": \"" << OperationKindString(operation.data) << "\"";
      field_blocks.push_back(field.str());
    }

    if (!operation.note.empty()) {
      std::ostringstream field;
      WriteIndent(field, 6);
      field << "\"note\": \"" << EscapeString(operation.note) << "\"";
      field_blocks.push_back(field.str());
    }

    std::visit(
        [&](const auto& op_data) {
          using T = std::decay_t<decltype(op_data)>;
          if constexpr (std::is_same_v<T, ReconformInsert>) {
            std::ostringstream target_field;
            WriteTimeRange(target_field, 6, "target", op_data.target);
            field_blocks.push_back(target_field.str());

            std::ostringstream source_field;
            WriteTimeRange(source_field, 6, "source", op_data.source);
            field_blocks.push_back(source_field.str());
          } else if constexpr (std::is_same_v<T, ReconformDelete>) {
            std::ostringstream target_field;
            WriteTimeRange(target_field, 6, "target", op_data.target);
            field_blocks.push_back(target_field.str());
          } else if constexpr (std::is_same_v<T, ReconformRetime>) {
            std::ostringstream target_field;
            WriteTimeRange(target_field, 6, "target", op_data.target);
            field_blocks.push_back(target_field.str());

            std::ostringstream duration_field;
            WriteIndent(duration_field, 6);
            duration_field << "\"retimed_duration_seconds\": "
                           << FormatDouble(op_data.retimed_duration_seconds);
            field_blocks.push_back(duration_field.str());
          }
        },
        operation.data);

    for (std::size_t field_index = 0; field_index < field_blocks.size(); ++field_index) {
      stream << field_blocks[field_index];
      if (field_index + 1 < field_blocks.size()) {
        stream << ",\n";
      } else {
        stream << "\n";
      }
    }

    WriteIndent(stream, 4);
    stream << "}";
    if (index + 1 < plan.operations.size()) {
      stream << ",";
    }
    stream << "\n";
  }

  WriteIndent(stream, 2);
  stream << "]\n";
  stream << "}\n";
  return stream.str();
}

ReconformPlan LoadReconformPlanFromFile(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("Unable to open reconform plan: " + path);
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return ParseReconformPlan(buffer.str());
}

void SaveReconformPlanToFile(const ReconformPlan& plan, const std::string& path) {
  std::ofstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("Unable to write reconform plan: " + path);
  }
  file << SerializeReconformPlan(plan);
  if (!file) {
    throw std::runtime_error("Failed to write reconform plan: " + path);
  }
}

ReconformPlan ImportTimelineReconformPlan(std::string_view /*otio_json_text*/) {
  return {};
}

ReconformPlan DiffReconformTimelines(std::string_view /*reference_otio*/,
                                     std::string_view /*revised_otio*/) {
  return {};
}

} // namespace orpheus::core::otio
