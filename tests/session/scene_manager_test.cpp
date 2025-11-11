// SPDX-License-Identifier: MIT
#include "../../include/orpheus/scene_manager.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

#include "../../src/core/session/session_graph.h"

using namespace orpheus;

class SceneManagerTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Create session graph
    sessionGraph = std::make_unique<core::SessionGraph>();
    sessionGraph->set_name("Test Session");

    // Create scene manager
    sceneManager = createSceneManager(sessionGraph.get());

    // Use /tmp directly (always exists on Unix systems)
    tempDir = "/tmp";
  }

  void TearDown() override {
    // Clean up temporary files
    cleanupTempFiles();
  }

  void cleanupTempFiles() {
    // Remove any temp files created during tests
    // Note: This is basic cleanup, production code would use a proper temp dir library
  }

  std::string getTempFilePath(const std::string& filename) {
    return tempDir + "/orpheus_test_" + filename;
  }

  std::unique_ptr<core::SessionGraph> sessionGraph;
  std::unique_ptr<ISceneManager> sceneManager;
  std::string tempDir;
};

// ============================================================================
// Scene Capture Tests
// ============================================================================

TEST_F(SceneManagerTest, CaptureSceneGeneratesUUID) {
  std::string sceneId = sceneManager->captureScene("Test Scene");

  EXPECT_FALSE(sceneId.empty());
  EXPECT_TRUE(sceneId.find("scene-") == 0); // Starts with "scene-"
}

TEST_F(SceneManagerTest, CaptureSceneStoresMetadata) {
  std::string sceneId = sceneManager->captureScene("My Scene");

  const SceneSnapshot* scene = sceneManager->getScene(sceneId);
  ASSERT_NE(scene, nullptr);

  EXPECT_EQ(scene->sceneId, sceneId);
  EXPECT_EQ(scene->name, "My Scene");
  EXPECT_GT(scene->timestamp, 0);
}

TEST_F(SceneManagerTest, CaptureMultipleScenesWithUniqueIds) {
  std::string id1 = sceneManager->captureScene("Scene 1");
  std::string id2 = sceneManager->captureScene("Scene 2");
  std::string id3 = sceneManager->captureScene("Scene 3");

  EXPECT_NE(id1, id2);
  EXPECT_NE(id2, id3);
  EXPECT_NE(id1, id3);
}

TEST_F(SceneManagerTest, CaptureSceneWithEmptyName) {
  std::string sceneId = sceneManager->captureScene("");

  EXPECT_FALSE(sceneId.empty());

  const SceneSnapshot* scene = sceneManager->getScene(sceneId);
  ASSERT_NE(scene, nullptr);
  EXPECT_EQ(scene->name, "");
}

TEST_F(SceneManagerTest, CaptureSceneWithLongName) {
  std::string longName(1000, 'x'); // 1000 characters
  std::string sceneId = sceneManager->captureScene(longName);

  EXPECT_FALSE(sceneId.empty());

  const SceneSnapshot* scene = sceneManager->getScene(sceneId);
  ASSERT_NE(scene, nullptr);
  EXPECT_EQ(scene->name, longName);
}

TEST_F(SceneManagerTest, CaptureSceneTimestampIncreases) {
  std::string id1 = sceneManager->captureScene("Scene 1");
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  std::string id2 = sceneManager->captureScene("Scene 2");

  const SceneSnapshot* scene1 = sceneManager->getScene(id1);
  const SceneSnapshot* scene2 = sceneManager->getScene(id2);

  ASSERT_NE(scene1, nullptr);
  ASSERT_NE(scene2, nullptr);

  // Timestamps should be close (within 5 seconds)
  EXPECT_LE(
      std::abs(static_cast<int64_t>(scene2->timestamp) - static_cast<int64_t>(scene1->timestamp)),
      5);
}

// ============================================================================
// Scene Recall Tests
// ============================================================================

TEST_F(SceneManagerTest, RecallSceneSucceeds) {
  std::string sceneId = sceneManager->captureScene("Test Scene");

  auto result = sceneManager->recallScene(sceneId);

  EXPECT_EQ(result, SessionGraphError::OK);
}

TEST_F(SceneManagerTest, RecallNonExistentSceneFails) {
  auto result = sceneManager->recallScene("non-existent-scene-id");

  EXPECT_EQ(result, SessionGraphError::InvalidHandle);
}

TEST_F(SceneManagerTest, RecallSceneWithEmptyIdFails) {
  auto result = sceneManager->recallScene("");

  EXPECT_EQ(result, SessionGraphError::InvalidHandle);
}

TEST_F(SceneManagerTest, RecallSceneMultipleTimes) {
  std::string sceneId = sceneManager->captureScene("Test Scene");

  // Recall multiple times
  EXPECT_EQ(sceneManager->recallScene(sceneId), SessionGraphError::OK);
  EXPECT_EQ(sceneManager->recallScene(sceneId), SessionGraphError::OK);
  EXPECT_EQ(sceneManager->recallScene(sceneId), SessionGraphError::OK);
}

// ============================================================================
// Scene Listing Tests
// ============================================================================

TEST_F(SceneManagerTest, ListScenesReturnsEmptyByDefault) {
  auto scenes = sceneManager->listScenes();

  EXPECT_TRUE(scenes.empty());
}

TEST_F(SceneManagerTest, ListScenesReturnsAllScenes) {
  sceneManager->captureScene("Scene 1");
  sceneManager->captureScene("Scene 2");
  sceneManager->captureScene("Scene 3");

  auto scenes = sceneManager->listScenes();

  EXPECT_EQ(scenes.size(), 3);
}

TEST_F(SceneManagerTest, ListScenesSortedByTimestampNewestFirst) {
  std::string id1 = sceneManager->captureScene("Scene 1");
  std::this_thread::sleep_for(
      std::chrono::seconds(1)); // Need at least 1 second for timestamp to change
  std::string id2 = sceneManager->captureScene("Scene 2");
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::string id3 = sceneManager->captureScene("Scene 3");

  auto scenes = sceneManager->listScenes();

  ASSERT_EQ(scenes.size(), 3);

  // Verify scenes are sorted by timestamp (newest first)
  // Check that timestamps are in descending order
  EXPECT_GE(scenes[0].timestamp, scenes[1].timestamp);
  EXPECT_GE(scenes[1].timestamp, scenes[2].timestamp);

  // Most recent scene should be id3
  EXPECT_EQ(scenes[0].name, "Scene 3");
  EXPECT_EQ(scenes[2].name, "Scene 1");
}

TEST_F(SceneManagerTest, ListScenesAfterDelete) {
  std::string id1 = sceneManager->captureScene("Scene 1");
  std::string id2 = sceneManager->captureScene("Scene 2");

  sceneManager->deleteScene(id1);

  auto scenes = sceneManager->listScenes();

  EXPECT_EQ(scenes.size(), 1);
  EXPECT_EQ(scenes[0].sceneId, id2);
}

// ============================================================================
// Scene Deletion Tests
// ============================================================================

TEST_F(SceneManagerTest, DeleteSceneSucceeds) {
  std::string sceneId = sceneManager->captureScene("Test Scene");

  auto result = sceneManager->deleteScene(sceneId);

  EXPECT_EQ(result, SessionGraphError::OK);
}

TEST_F(SceneManagerTest, DeleteNonExistentSceneFails) {
  auto result = sceneManager->deleteScene("non-existent-scene-id");

  EXPECT_EQ(result, SessionGraphError::InvalidHandle);
}

TEST_F(SceneManagerTest, DeleteSceneRemovesFromList) {
  std::string sceneId = sceneManager->captureScene("Test Scene");

  sceneManager->deleteScene(sceneId);

  EXPECT_FALSE(sceneManager->hasScene(sceneId));
  EXPECT_EQ(sceneManager->getScene(sceneId), nullptr);
}

TEST_F(SceneManagerTest, DeleteSceneTwiceFails) {
  std::string sceneId = sceneManager->captureScene("Test Scene");

  EXPECT_EQ(sceneManager->deleteScene(sceneId), SessionGraphError::OK);
  EXPECT_EQ(sceneManager->deleteScene(sceneId), SessionGraphError::InvalidHandle);
}

// ============================================================================
// Scene Export Tests
// ============================================================================

TEST_F(SceneManagerTest, ExportSceneCreatesFile) {
  std::string sceneId = sceneManager->captureScene("Export Test");
  std::string filePath = getTempFilePath("export_test.json");

  auto result = sceneManager->exportScene(sceneId, filePath);

  EXPECT_EQ(result, SessionGraphError::OK);

  // Check file exists
  std::ifstream file(filePath);
  EXPECT_TRUE(file.good());
  file.close();

  // Clean up
  std::remove(filePath.c_str());
}

TEST_F(SceneManagerTest, ExportNonExistentSceneFails) {
  std::string filePath = getTempFilePath("non_existent.json");

  auto result = sceneManager->exportScene("non-existent-scene-id", filePath);

  EXPECT_EQ(result, SessionGraphError::InvalidHandle);
}

TEST_F(SceneManagerTest, ExportSceneWithInvalidPathFails) {
  std::string sceneId = sceneManager->captureScene("Test Scene");
  std::string invalidPath = "/invalid/path/that/does/not/exist/scene.json";

  auto result = sceneManager->exportScene(sceneId, invalidPath);

  EXPECT_EQ(result, SessionGraphError::InternalError);
}

TEST_F(SceneManagerTest, ExportedSceneContainsCorrectData) {
  std::string sceneId = sceneManager->captureScene("Export Data Test");
  std::string filePath = getTempFilePath("export_data_test.json");

  sceneManager->exportScene(sceneId, filePath);

  // Read file contents
  std::ifstream file(filePath);
  std::ostringstream oss;
  oss << file.rdbuf();
  std::string jsonStr = oss.str();
  file.close();

  // Verify JSON contains scene metadata
  EXPECT_TRUE(jsonStr.find("sceneId") != std::string::npos);
  EXPECT_TRUE(jsonStr.find("name") != std::string::npos);
  EXPECT_TRUE(jsonStr.find("timestamp") != std::string::npos);
  EXPECT_TRUE(jsonStr.find("Export Data Test") != std::string::npos);

  // Clean up
  std::remove(filePath.c_str());
}

// ============================================================================
// Scene Import Tests
// ============================================================================

TEST_F(SceneManagerTest, ImportSceneFromValidFile) {
  // Export a scene first
  std::string originalId = sceneManager->captureScene("Import Test");
  std::string filePath = getTempFilePath("import_test.json");
  sceneManager->exportScene(originalId, filePath);

  // Clear all scenes
  sceneManager->clearAllScenes();

  // Import the scene
  std::string importedId = sceneManager->importScene(filePath);

  EXPECT_FALSE(importedId.empty());
  EXPECT_NE(importedId, originalId); // New UUID generated

  // Verify scene exists
  EXPECT_TRUE(sceneManager->hasScene(importedId));

  const SceneSnapshot* scene = sceneManager->getScene(importedId);
  ASSERT_NE(scene, nullptr);
  EXPECT_EQ(scene->name, "Import Test");

  // Clean up
  std::remove(filePath.c_str());
}

TEST_F(SceneManagerTest, ImportSceneFromNonExistentFileFails) {
  std::string importedId = sceneManager->importScene("/non/existent/file.json");

  EXPECT_TRUE(importedId.empty());
}

TEST_F(SceneManagerTest, ImportSceneFromInvalidJsonFails) {
  std::string filePath = getTempFilePath("invalid.json");

  // Write invalid JSON
  std::ofstream file(filePath);
  file << "{ invalid json }";
  file.close();

  std::string importedId = sceneManager->importScene(filePath);

  EXPECT_TRUE(importedId.empty());

  // Clean up
  std::remove(filePath.c_str());
}

TEST_F(SceneManagerTest, ImportSceneGeneratesNewUUID) {
  // Export a scene
  std::string originalId = sceneManager->captureScene("UUID Test");
  std::string filePath = getTempFilePath("uuid_test.json");
  sceneManager->exportScene(originalId, filePath);

  // Import the scene
  std::string importedId = sceneManager->importScene(filePath);

  EXPECT_FALSE(importedId.empty());
  EXPECT_NE(importedId, originalId); // New UUID

  // Clean up
  std::remove(filePath.c_str());
}

TEST_F(SceneManagerTest, ImportScenePreservesName) {
  // Export a scene
  std::string originalId = sceneManager->captureScene("Original Scene Name");
  std::string filePath = getTempFilePath("name_test.json");
  sceneManager->exportScene(originalId, filePath);

  // Clear and import
  sceneManager->clearAllScenes();
  std::string importedId = sceneManager->importScene(filePath);

  const SceneSnapshot* scene = sceneManager->getScene(importedId);
  ASSERT_NE(scene, nullptr);
  EXPECT_EQ(scene->name, "Original Scene Name");

  // Clean up
  std::remove(filePath.c_str());
}

// ============================================================================
// Round-Trip Export/Import Tests
// ============================================================================

TEST_F(SceneManagerTest, ExportImportRoundTrip) {
  // Create scene with metadata
  std::string originalId = sceneManager->captureScene("Round Trip Test");
  std::string filePath = getTempFilePath("round_trip.json");

  // Export
  auto exportResult = sceneManager->exportScene(originalId, filePath);
  ASSERT_EQ(exportResult, SessionGraphError::OK);

  // Clear scenes
  sceneManager->clearAllScenes();
  EXPECT_FALSE(sceneManager->hasScene(originalId));

  // Import
  std::string importedId = sceneManager->importScene(filePath);
  EXPECT_FALSE(importedId.empty());

  // Verify scene restored
  const SceneSnapshot* scene = sceneManager->getScene(importedId);
  ASSERT_NE(scene, nullptr);
  EXPECT_EQ(scene->name, "Round Trip Test");

  // Clean up
  std::remove(filePath.c_str());
}

// ============================================================================
// Utility Method Tests
// ============================================================================

TEST_F(SceneManagerTest, GetSceneReturnsValidPointer) {
  std::string sceneId = sceneManager->captureScene("Get Test");

  const SceneSnapshot* scene = sceneManager->getScene(sceneId);

  ASSERT_NE(scene, nullptr);
  EXPECT_EQ(scene->sceneId, sceneId);
}

TEST_F(SceneManagerTest, GetSceneReturnsNullForNonExistent) {
  const SceneSnapshot* scene = sceneManager->getScene("non-existent-id");

  EXPECT_EQ(scene, nullptr);
}

TEST_F(SceneManagerTest, HasSceneReturnsTrueForExisting) {
  std::string sceneId = sceneManager->captureScene("Has Test");

  EXPECT_TRUE(sceneManager->hasScene(sceneId));
}

TEST_F(SceneManagerTest, HasSceneReturnsFalseForNonExistent) {
  EXPECT_FALSE(sceneManager->hasScene("non-existent-id"));
}

TEST_F(SceneManagerTest, ClearAllScenesRemovesAll) {
  sceneManager->captureScene("Scene 1");
  sceneManager->captureScene("Scene 2");
  sceneManager->captureScene("Scene 3");

  auto result = sceneManager->clearAllScenes();

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_TRUE(sceneManager->listScenes().empty());
}

TEST_F(SceneManagerTest, ClearAllScenesOnEmptyManagerSucceeds) {
  auto result = sceneManager->clearAllScenes();

  EXPECT_EQ(result, SessionGraphError::OK);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(SceneManagerTest, CaptureSceneWithSpecialCharactersInName) {
  std::string specialName = "Scene!@#$%^&*()_+-=[]{}|;':\",./<>?";
  std::string sceneId = sceneManager->captureScene(specialName);

  EXPECT_FALSE(sceneId.empty());

  const SceneSnapshot* scene = sceneManager->getScene(sceneId);
  ASSERT_NE(scene, nullptr);
  EXPECT_EQ(scene->name, specialName);
}

TEST_F(SceneManagerTest, CaptureSceneWithUnicodeCharacters) {
  std::string unicodeName = "Scene 测试 Тест テスト";
  std::string sceneId = sceneManager->captureScene(unicodeName);

  EXPECT_FALSE(sceneId.empty());

  const SceneSnapshot* scene = sceneManager->getScene(sceneId);
  ASSERT_NE(scene, nullptr);
  EXPECT_EQ(scene->name, unicodeName);
}

TEST_F(SceneManagerTest, MultipleSceneOperations) {
  // Create multiple scenes
  std::string id1 = sceneManager->captureScene("Scene 1");
  std::string id2 = sceneManager->captureScene("Scene 2");
  std::string id3 = sceneManager->captureScene("Scene 3");

  // Delete one
  sceneManager->deleteScene(id2);

  // List remaining
  auto scenes = sceneManager->listScenes();
  EXPECT_EQ(scenes.size(), 2);

  // Recall one
  EXPECT_EQ(sceneManager->recallScene(id1), SessionGraphError::OK);

  // Export one
  std::string filePath = getTempFilePath("multi_op.json");
  EXPECT_EQ(sceneManager->exportScene(id3, filePath), SessionGraphError::OK);

  // Clean up
  std::remove(filePath.c_str());
}

TEST_F(SceneManagerTest, SceneSnapshotInitialization) {
  // Test default constructor
  SceneSnapshot scene;

  EXPECT_TRUE(scene.sceneId.empty());
  EXPECT_TRUE(scene.name.empty());
  EXPECT_EQ(scene.timestamp, 0);
  EXPECT_TRUE(scene.assignedClips.empty());
  EXPECT_TRUE(scene.clipGroups.empty());
  EXPECT_TRUE(scene.groupGains.empty());
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(SceneManagerTest, Capture100Scenes) {
  for (int i = 0; i < 100; ++i) {
    std::string name = "Scene " + std::to_string(i);
    std::string sceneId = sceneManager->captureScene(name);
    EXPECT_FALSE(sceneId.empty());
  }

  auto scenes = sceneManager->listScenes();
  EXPECT_EQ(scenes.size(), 100);
}

TEST_F(SceneManagerTest, RapidCaptureAndDelete) {
  for (int i = 0; i < 50; ++i) {
    std::string sceneId = sceneManager->captureScene("Temp Scene");
    EXPECT_EQ(sceneManager->deleteScene(sceneId), SessionGraphError::OK);
  }

  EXPECT_TRUE(sceneManager->listScenes().empty());
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
