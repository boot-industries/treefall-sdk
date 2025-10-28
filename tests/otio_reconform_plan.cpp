// SPDX-License-Identifier: MIT
#include "otio/reconform_plan.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <variant>

#include <gtest/gtest.h>

namespace orpheus::tests {
namespace {

namespace fs = std::filesystem;
namespace otio = orpheus::core::otio;

fs::path FixtureRoot() {
  static const fs::path root = fs::path(__FILE__).parent_path() / "fixtures" / "reconform";
  return root;
}

std::string LoadFixture(const std::string& name) {
  const fs::path path = FixtureRoot() / name;
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("Unable to open fixture: " + path.string());
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

} // namespace

TEST(ReconformPlanJsonTest, InsertFixtureRoundTrips) {
  const std::string text = LoadFixture("insert.json");
  const otio::ReconformPlan plan = otio::ParseReconformPlan(text);

  EXPECT_EQ(plan.version, 1u);
  EXPECT_EQ(plan.timeline_name, "Demo Sequence");
  ASSERT_EQ(plan.operations.size(), 1u);

  const otio::ReconformOperation& operation = plan.operations.front();
  EXPECT_TRUE(std::holds_alternative<otio::ReconformInsert>(operation.data));
  EXPECT_EQ(operation.note, "Insert establishing shot");

  const auto& insert = std::get<otio::ReconformInsert>(operation.data);
  EXPECT_DOUBLE_EQ(insert.target.start_seconds, 5.0);
  EXPECT_DOUBLE_EQ(insert.target.duration_seconds, 3.5);
  EXPECT_DOUBLE_EQ(insert.source.start_seconds, 42.5);
  EXPECT_DOUBLE_EQ(insert.source.duration_seconds, 3.5);

  const std::string serialized = otio::SerializeReconformPlan(plan);
  EXPECT_EQ(serialized, text);

  const otio::ReconformPlan roundtrip = otio::ParseReconformPlan(serialized);
  EXPECT_EQ(roundtrip, plan);
}

TEST(ReconformPlanJsonTest, DeleteFixtureRoundTrips) {
  const std::string text = LoadFixture("delete.json");
  const otio::ReconformPlan plan = otio::ParseReconformPlan(text);

  EXPECT_EQ(plan.version, 1u);
  EXPECT_EQ(plan.timeline_name, "Demo Sequence");
  ASSERT_EQ(plan.operations.size(), 1u);

  const otio::ReconformOperation& operation = plan.operations.front();
  EXPECT_TRUE(std::holds_alternative<otio::ReconformDelete>(operation.data));
  EXPECT_TRUE(operation.note.empty());

  const auto& del = std::get<otio::ReconformDelete>(operation.data);
  EXPECT_DOUBLE_EQ(del.target.start_seconds, 12.0);
  EXPECT_DOUBLE_EQ(del.target.duration_seconds, 4.0);

  const std::string serialized = otio::SerializeReconformPlan(plan);
  EXPECT_EQ(serialized, text);
  EXPECT_EQ(otio::ParseReconformPlan(serialized), plan);
}

TEST(ReconformPlanJsonTest, RetimeFixtureRoundTrips) {
  const std::string text = LoadFixture("retime.json");
  const otio::ReconformPlan plan = otio::ParseReconformPlan(text);

  EXPECT_EQ(plan.version, 1u);
  EXPECT_EQ(plan.timeline_name, "Demo Sequence");
  ASSERT_EQ(plan.operations.size(), 1u);

  const otio::ReconformOperation& operation = plan.operations.front();
  EXPECT_TRUE(std::holds_alternative<otio::ReconformRetime>(operation.data));
  EXPECT_EQ(operation.note, "Slow motion bridge");

  const auto& retime = std::get<otio::ReconformRetime>(operation.data);
  EXPECT_DOUBLE_EQ(retime.target.start_seconds, 30.0);
  EXPECT_DOUBLE_EQ(retime.target.duration_seconds, 5.0);
  EXPECT_DOUBLE_EQ(retime.retimed_duration_seconds, 7.5);

  const std::string serialized = otio::SerializeReconformPlan(plan);
  EXPECT_EQ(serialized, text);
  EXPECT_EQ(otio::ParseReconformPlan(serialized), plan);
}

} // namespace orpheus::tests
