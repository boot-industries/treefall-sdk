// SPDX-License-Identifier: MIT
#include "orpheus/adm/entity_graph.h"

#include <gtest/gtest.h>

#include "common/json_parser.h"

namespace adm = orpheus::core::adm;
namespace json = orpheus::core::json;

TEST(AdmEntityGraphTest, BuildsGraphAndAppliesThinning) {
  adm::EntityGraph graph;

  auto& programme =
      graph.add_programme({"APR_0001", "Immersive Programme", adm::EntityKind::kProgramme});
  auto& content = graph.add_content({"ACO_0001", "Narrative", adm::EntityKind::kContent});
  graph.link_programme_to_content(programme, content);

  auto& bed = graph.add_bed({"AB_0001", "Main 5.1", adm::EntityKind::kBed});
  bed.add_channel({"ACH_0001", "Left"});
  bed.add_channel({"ACH_0002", "Right"});
  bed.add_channel({"ACH_0003", "Centre"});
  bed.add_channel({"ACH_0004", "LFE"});
  bed.add_channel({"ACH_0005", "Ls"});
  bed.add_channel({"ACH_0006", "Rs"});
  graph.link_content_to_bed(content, bed);

  ASSERT_EQ(bed.channels().size(), 6u);

  auto& object = graph.add_object({"AO_0001", "Spot FX", adm::EntityKind::kObject});
  object.add_point({0.0, 0.0, 0.0, 0.0});
  object.add_point({1.0, 0.5, 0.0, 0.0});
  object.add_point({2.0, 1.0, 0.0, 0.0});
  object.add_point({3.0, 1.0, 0.5, 0.0});
  graph.link_content_to_object(content, object);

  const auto dense = object.trajectory(adm::ThinningPolicy::kDisabled);
  ASSERT_EQ(dense.size(), 4u);
  EXPECT_DOUBLE_EQ(dense[1].time_seconds, 1.0);

  const auto thinned = object.trajectory(adm::ThinningPolicy::kEnabled);
  ASSERT_EQ(thinned.size(), 3u);
  EXPECT_DOUBLE_EQ(thinned[1].time_seconds, 2.0);

  const std::string json_dump = graph.DebugDumpJson(adm::ThinningPolicy::kEnabled);
  json::JsonParser parser(json_dump);
  const json::JsonValue root_value = parser.Parse();
  const auto& root_object = json::ExpectObject(root_value, "root");

  const auto programmes_it = root_object.object.find("programmes");
  ASSERT_NE(programmes_it, root_object.object.end());
  const auto& programmes = json::ExpectArray(programmes_it->second, "programmes");
  ASSERT_EQ(programmes.array.size(), 1u);
  const auto& programme_json = json::ExpectObject(programmes.array[0], "programme element");
  const auto* envelope_field = json::RequireField(programme_json, "envelope");
  const auto& envelope_json = json::ExpectObject(*envelope_field, "programme envelope");
  const auto* programme_id = json::RequireField(envelope_json, "id");
  ASSERT_EQ(programme_id->type, json::JsonValue::Type::kString);
  EXPECT_EQ(programme_id->string, "APR_0001");

  const auto objects_it = root_object.object.find("objects");
  ASSERT_NE(objects_it, root_object.object.end());
  const auto& objects = json::ExpectArray(objects_it->second, "objects");
  ASSERT_EQ(objects.array.size(), 1u);
  const auto& object_json = json::ExpectObject(objects.array[0], "object element");
  const auto* trajectory_field = json::RequireField(object_json, "trajectory");
  const auto& trajectory_json = json::ExpectArray(*trajectory_field, "trajectory");
  ASSERT_EQ(trajectory_json.array.size(), 3u);
  const auto* time_field = json::RequireField(trajectory_json.array[1], "time");
  ASSERT_EQ(time_field->type, json::JsonValue::Type::kNumber);
  EXPECT_DOUBLE_EQ(time_field->number, 2.0);
}
