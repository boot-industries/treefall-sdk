// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "orpheus/export.h"

namespace orpheus::core::otio {

struct ReconformTimeRange {
  double start_seconds{0.0};
  double duration_seconds{0.0};

  friend bool operator==(const ReconformTimeRange&, const ReconformTimeRange&) = default;
};

struct ReconformInsert {
  ReconformTimeRange target;
  ReconformTimeRange source;

  friend bool operator==(const ReconformInsert&, const ReconformInsert&) = default;
};

struct ReconformDelete {
  ReconformTimeRange target;

  friend bool operator==(const ReconformDelete&, const ReconformDelete&) = default;
};

struct ReconformRetime {
  ReconformTimeRange target;
  double retimed_duration_seconds{0.0};

  friend bool operator==(const ReconformRetime&, const ReconformRetime&) = default;
};

using ReconformOperationData = std::variant<ReconformInsert, ReconformDelete, ReconformRetime>;

struct ReconformOperation {
  ReconformOperationData data;
  std::string note;

  friend bool operator==(const ReconformOperation&, const ReconformOperation&) = default;
};

struct ReconformPlan {
  std::uint32_t version{1};
  std::string timeline_name;
  std::vector<ReconformOperation> operations;

  friend bool operator==(const ReconformPlan&, const ReconformPlan&) = default;
};

ORPHEUS_API ReconformPlan ParseReconformPlan(const std::string& json_text);
ORPHEUS_API std::string SerializeReconformPlan(const ReconformPlan& plan);

ORPHEUS_API ReconformPlan LoadReconformPlanFromFile(const std::string& path);
ORPHEUS_API void SaveReconformPlanToFile(const ReconformPlan& plan, const std::string& path);

// Placeholder entry points for future OTIO integration. These currently return
// empty plans and will be fleshed out in later milestones.
ORPHEUS_API ReconformPlan ImportTimelineReconformPlan(std::string_view otio_json_text);
ORPHEUS_API ReconformPlan DiffReconformTimelines(std::string_view reference_otio,
                                                 std::string_view revised_otio);

} // namespace orpheus::core::otio
