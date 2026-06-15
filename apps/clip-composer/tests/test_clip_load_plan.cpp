// SPDX-License-Identifier: MIT
// Clip load path planning tests (portable session/audio-engine path alignment)

#include "../Source/Core/ClipLoadPlan.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <juce_core/juce_core.h>

namespace {

juce::File makeTempDirectory() {
  auto baseDirectory = juce::File(std::filesystem::temp_directory_path().string());
  auto uniqueName = juce::String("clip_composer_load_plan_") + juce::Uuid().toString();
  auto tempDirectory = baseDirectory.getChildFile(uniqueName);
  tempDirectory.createDirectory();
  return tempDirectory;
}

} // namespace

class ClipLoadPlanTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_tempDirectory = makeTempDirectory();
    m_sourceDirectory = m_tempDirectory.getChildFile("source");
    m_projectAudioDirectory = m_tempDirectory.getChildFile("project-audio");
    m_sourceDirectory.createDirectory();
    m_projectAudioDirectory.createDirectory();
    m_sourceFile = m_sourceDirectory.getChildFile("stinger.wav");
    ASSERT_TRUE(m_sourceFile.replaceWithText("fake wav bytes"));
  }

  void TearDown() override {
    if (m_tempDirectory.exists())
      m_tempDirectory.deleteRecursively();
  }

  juce::File m_tempDirectory;
  juce::File m_sourceDirectory;
  juce::File m_projectAudioDirectory;
  juce::File m_sourceFile;
};

TEST_F(ClipLoadPlanTest, LinkedImportUsesOriginalPathForSessionAndAudioEngine) {
  const auto plan = occ::makeLinkedClipLoadPlan(m_sourceFile);

  EXPECT_EQ(plan.sessionPath, m_sourceFile.getFullPathName());
  EXPECT_EQ(plan.audioEnginePath, m_sourceFile.getFullPathName());
  EXPECT_FALSE(plan.copiedToProject);
}

TEST_F(ClipLoadPlanTest, CopiedImportUsesCopiedProjectPathForSessionAndAudioEngine) {
  const auto plan = occ::copyClipIntoProjectAudioDirectory(m_sourceFile, m_projectAudioDirectory);

  ASSERT_TRUE(plan.has_value());
  EXPECT_TRUE(plan->copiedToProject);
  EXPECT_TRUE(juce::File(plan->sessionPath).isAChildOf(m_projectAudioDirectory));
  EXPECT_EQ(plan->sessionPath, plan->audioEnginePath)
      << "portable copied imports must not let SessionManager and AudioEngine diverge";
  EXPECT_TRUE(juce::File(plan->audioEnginePath).existsAsFile());
}

TEST_F(ClipLoadPlanTest, CopiedImportCreatesUniqueDestinationWithoutOverwritingExistingMedia) {
  auto existingProjectFile = m_projectAudioDirectory.getChildFile(m_sourceFile.getFileName());
  ASSERT_TRUE(existingProjectFile.replaceWithText("already in project"));

  const auto plan = occ::copyClipIntoProjectAudioDirectory(m_sourceFile, m_projectAudioDirectory);

  ASSERT_TRUE(plan.has_value());
  EXPECT_NE(plan->sessionPath, existingProjectFile.getFullPathName());
  EXPECT_TRUE(plan->sessionPath.endsWith("stinger_1.wav"));
  EXPECT_EQ(existingProjectFile.loadFileAsString(), "already in project");
}
