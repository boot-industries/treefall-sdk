// SPDX-License-Identifier: MIT
// ORP134 G8: scene manager <-> routing matrix wiring.
//
// Before this sprint, SceneManager's routing capture/recall existed but was
// UNREACHABLE: setRoutingMatrix() lived only on the hidden concrete class, so
// no consumer of createSceneManager() could ever wire it. These tests drive
// the now-public ISceneManager::setRoutingMatrix() end to end, at Clip
// Composer scale (multi-scene "tab" switching over group assignments/gains).

#include <orpheus/routing_matrix.h>
#include <orpheus/scene_manager.h>

#include <orpheus/session_graph.h>

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

using namespace orpheus;

namespace {

class SceneRoutingTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_graph = std::make_unique<core::SessionGraph>();
    m_scenes = createSceneManager(m_graph.get());
    ASSERT_NE(m_scenes, nullptr);
    m_transport = createTransportController(m_graph.get(), TransportConfig{.sampleRate = static_cast<uint32_t>(48000)});
    ASSERT_NE(m_transport, nullptr);
    m_scenes->setTransportController(m_transport.get());

    m_routing = createRoutingMatrix();
    ASSERT_NE(m_routing, nullptr);

    RoutingConfig config;
    config.num_channels = 16;
    config.num_groups = 4;
    config.num_outputs = 2;
    ASSERT_EQ(m_routing->initialize(config), SessionGraphError::OK);

    // Wire the matrix through the PUBLIC interface (the G8 fix).
    m_scenes->setRoutingMatrix(m_routing.get());
  }

  void assignTopology(uint8_t groupForEvenChannels, float gainDbGroup0) {
    for (uint8_t ch = 0; ch < 16; ++ch) {
      m_routing->setChannelGroup(ch,
                                 (ch % 2 == 0) ? groupForEvenChannels : static_cast<uint8_t>(3));
    }
    GroupConfig group;
    group.gain_db = gainDbGroup0;
    m_routing->configureGroup(0, group);
  }

  std::unique_ptr<core::SessionGraph> m_graph;
  std::unique_ptr<ISceneManager> m_scenes;
  std::unique_ptr<ITransportController> m_transport;
  std::unique_ptr<IRoutingMatrix> m_routing;
};

} // namespace

TEST_F(SceneRoutingTest, CaptureRecordsRoutingState) {
  assignTopology(1, -6.0f);

  const std::string sceneId = m_scenes->captureScene("Act 1");
  ASSERT_FALSE(sceneId.empty());

  const SceneSnapshot* scene = m_scenes->getScene(sceneId);
  ASSERT_NE(scene, nullptr);
  ASSERT_EQ(scene->clipGroups.size(), 16u);
  EXPECT_EQ(scene->clipGroups[0], 1);
  EXPECT_EQ(scene->clipGroups[1], 3);
  ASSERT_EQ(scene->groupGains.size(), 4u);
  EXPECT_FLOAT_EQ(scene->groupGains[0], -6.0f);
}

TEST_F(SceneRoutingTest, RecallRestoresRoutingState) {
  assignTopology(1, -6.0f);
  const std::string sceneA = m_scenes->captureScene("Act 1");

  // Mutate the live matrix, then recall the captured scene.
  assignTopology(2, +3.0f);
  ASSERT_EQ(m_scenes->recallScene(sceneA), SessionGraphError::OK);

  // Verify through the matrix's own snapshot API.
  RoutingSnapshot restored = m_routing->saveSnapshot("verify");
  ASSERT_GE(restored.channels.size(), 16u);
  EXPECT_EQ(restored.channels[0].group_index, 1);
  EXPECT_EQ(restored.channels[1].group_index, 3);
  ASSERT_GE(restored.groups.size(), 1u);
  EXPECT_FLOAT_EQ(restored.groups[0].gain_db, -6.0f);
}

TEST_F(SceneRoutingTest, EightTabPageSwitchingRoundTrip) {
  // Clip Composer models 8 tabs; capture 8 scenes with distinct routing and
  // switch between them repeatedly — every recall must restore ITS state.
  std::vector<std::string> tabs;
  for (int tab = 0; tab < 8; ++tab) {
    assignTopology(static_cast<uint8_t>(tab % 4), static_cast<float>(tab) - 4.0f);
    tabs.push_back(m_scenes->captureScene("Tab " + std::to_string(tab)));
    ASSERT_FALSE(tabs.back().empty());
  }

  // Rapid page switching, out of order, twice around.
  const int order[] = {5, 0, 7, 2, 6, 1, 4, 3, 3, 7, 0, 5};
  for (int tab : order) {
    ASSERT_EQ(m_scenes->recallScene(tabs[static_cast<size_t>(tab)]), SessionGraphError::OK);
    RoutingSnapshot state = m_routing->saveSnapshot("verify");
    ASSERT_GE(state.channels.size(), 16u);
    EXPECT_EQ(state.channels[0].group_index, static_cast<uint8_t>(tab % 4))
        << "tab " << tab << " restored the wrong group topology";
    ASSERT_GE(state.groups.size(), 1u);
    EXPECT_FLOAT_EQ(state.groups[0].gain_db, static_cast<float>(tab) - 4.0f)
        << "tab " << tab << " restored the wrong group gain";
  }
}

TEST_F(SceneRoutingTest, DetachedMatrixStillCapturesMetadata) {
  m_scenes->setRoutingMatrix(nullptr);
  const std::string sceneId = m_scenes->captureScene("no routing");
  ASSERT_FALSE(sceneId.empty());
  const SceneSnapshot* scene = m_scenes->getScene(sceneId);
  ASSERT_NE(scene, nullptr);
  EXPECT_TRUE(scene->clipGroups.empty());
  EXPECT_TRUE(scene->groupGains.empty());
  EXPECT_EQ(m_scenes->recallScene(sceneId), SessionGraphError::OK);
}

TEST_F(SceneRoutingTest, RecallRestoresAssignmentsAtomically) {
  m_graph->set_clip_assignments({11, 22, 33});
  const std::string sceneId = m_scenes->captureScene("Assignments");
  ASSERT_FALSE(sceneId.empty());

  m_graph->set_clip_assignments({99});
  ASSERT_EQ(m_scenes->recallScene(sceneId), SessionGraphError::OK);
  EXPECT_EQ(m_graph->clip_assignments(), (std::vector<uint64_t>{11, 22, 33}));
}

TEST_F(SceneRoutingTest, RecallWithoutTransportDoesNotMutateAssignments) {
  m_graph->set_clip_assignments({1, 2});
  const std::string sceneId = m_scenes->captureScene("Rollback");
  m_graph->set_clip_assignments({7, 8});
  m_scenes->setTransportController(nullptr);

  EXPECT_EQ(m_scenes->recallScene(sceneId), SessionGraphError::NotInitialized);
  EXPECT_EQ(m_graph->clip_assignments(), (std::vector<uint64_t>{7, 8}));
}
