// SPDX-License-Identifier: MIT
// SessionManager Tests (Session confidence/package/recovery slice)

#include "../Source/Session/SessionManager.h"
#include <cmath>
#include <filesystem>
#include <gtest/gtest.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>

namespace {

juce::File makeTempDirectory() {
  auto baseDirectory = juce::File(std::filesystem::temp_directory_path().string());
  auto uniqueName = juce::String("clip_composer_session_manager_") + juce::Uuid().toString();
  auto tempDirectory = baseDirectory.getChildFile(uniqueName);
  tempDirectory.createDirectory();
  return tempDirectory;
}

void writeTestWavFile(const juce::File& file) {
  file.getParentDirectory().createDirectory();
  file.deleteFile();

  juce::WavAudioFormat format;
  auto stream = file.createOutputStream();
  ASSERT_NE(stream, nullptr);

  std::unique_ptr<juce::AudioFormatWriter> writer(
      format.createWriterFor(stream.release(), 48000.0, 2, 16, {}, 0));
  ASSERT_NE(writer, nullptr);

  constexpr int numSamples = 48000;
  juce::AudioBuffer<float> buffer(2, numSamples);
  for (int sample = 0; sample < numSamples; ++sample) {
    auto value = 0.2f * std::sin(2.0 * juce::MathConstants<double>::pi * 440.0 * sample / 48000.0);
    buffer.setSample(0, sample, value);
    buffer.setSample(1, sample, value);
  }

  ASSERT_TRUE(writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()));
}

} // namespace

class SessionManagerTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_tempDirectory = makeTempDirectory();
    m_audioPath = m_tempDirectory.getChildFile("reference.wav");
    m_sessionPath = m_tempDirectory.getChildFile("session.json");
    m_packageDirectory = m_tempDirectory.getChildFile("package");
    writeTestWavFile(m_audioPath);
    m_sessionManager = std::make_unique<SessionManager>();
  }

  void TearDown() override {
    m_sessionManager.reset();
    if (m_tempDirectory.exists()) {
      m_tempDirectory.deleteRecursively();
    }
  }

  std::unique_ptr<SessionManager> m_sessionManager;
  juce::File m_tempDirectory;
  juce::File m_audioPath;
  juce::File m_sessionPath;
  juce::File m_packageDirectory;
};

TEST_F(SessionManagerTest, CreateNewSession) {
  m_sessionManager->clearSession();
  EXPECT_EQ(m_sessionManager->getClipCount(), 0);
}

TEST_F(SessionManagerTest, SaveSessionToFile) {
  m_sessionManager->clearSession();

  bool success = m_sessionManager->saveSession(m_sessionPath);
  EXPECT_TRUE(success);

  EXPECT_TRUE(m_sessionPath.existsAsFile());
}

TEST_F(SessionManagerTest, LoadNonExistentSession) {
  bool success = m_sessionManager->loadSession(juce::File("/tmp/nonexistent_session.json"));
  EXPECT_FALSE(success);
}

TEST_F(SessionManagerTest, SaveAndLoadSession) {
  m_sessionManager->clearSession();
  ASSERT_TRUE(m_sessionManager->loadClip(0, m_audioPath.getFullPathName()));
  m_sessionManager->setSessionName("Round Trip Session");

  bool saved = m_sessionManager->saveSession(m_sessionPath);
  EXPECT_TRUE(saved);

  auto newManager = std::make_unique<SessionManager>();
  bool loaded = newManager->loadSession(m_sessionPath);
  EXPECT_TRUE(loaded);
  EXPECT_EQ(newManager->getClipCount(), 1);
  EXPECT_EQ(newManager->getSessionName(), "Round Trip Session");
}

TEST_F(SessionManagerTest, SupportsHundredLogicalSlotsPerTab) {
  SessionManager::ClipData clip;
  clip.filePath = m_audioPath.getFullPathName().toStdString();
  clip.displayName = "Slot 100";
  clip.mediaAvailable = true;

  m_sessionManager->setClip(99, clip, 0);
  EXPECT_TRUE(m_sessionManager->hasClip(99, 0));
  EXPECT_EQ(m_sessionManager->getClip(99, 0).displayName, "Slot 100");

  EXPECT_FALSE(m_sessionManager->hasClip(100, 0));
  m_sessionManager->setClip(100, clip, 0);
  EXPECT_FALSE(m_sessionManager->hasClip(100, 0));
}

TEST_F(SessionManagerTest, GlobalIndexUsesHundredSlotPages) {
  SessionManager::ClipData clip;
  clip.filePath = m_audioPath.getFullPathName().toStdString();
  clip.displayName = "Second Page First Slot";
  clip.mediaAvailable = true;

  m_sessionManager->setClip(0, clip, 1);
  EXPECT_TRUE(m_sessionManager->hasClipByGlobalIndex(100));
  EXPECT_EQ(m_sessionManager->getClipByGlobalIndex(100).displayName, "Second Page First Slot");
  EXPECT_FALSE(m_sessionManager->hasClipByGlobalIndex(99));
}

TEST_F(SessionManagerTest, LoadsLegacyFortyEightVisibleSlotSessionWithoutRemapping) {
  juce::var sessionJson = juce::var(new juce::DynamicObject());
  auto* sessionObject = sessionJson.getDynamicObject();
  ASSERT_NE(sessionObject, nullptr);

  sessionObject->setProperty("version", "0.2.0-alpha");
  sessionObject->setProperty("name", "Legacy 48 Slot Session");

  juce::Array<juce::var> clips;
  auto clipJson = juce::var(new juce::DynamicObject());
  auto* clipObject = clipJson.getDynamicObject();
  ASSERT_NE(clipObject, nullptr);
  clipObject->setProperty("tabIndex", 1);
  clipObject->setProperty("buttonIndex", 47);
  clipObject->setProperty("filePath", m_audioPath.getFullPathName());
  clipObject->setProperty("displayName", "Legacy Last Visible Slot");
  clips.add(clipJson);
  sessionObject->setProperty("clips", juce::var(clips));

  ASSERT_TRUE(m_sessionPath.replaceWithText(juce::JSON::toString(sessionJson, true)));

  SessionManager reloaded;
  ASSERT_TRUE(reloaded.loadSession(m_sessionPath));

  EXPECT_TRUE(reloaded.hasClip(47, 1));
  EXPECT_EQ(reloaded.getClip(47, 1).displayName, "Legacy Last Visible Slot");
  EXPECT_FALSE(reloaded.hasClipByGlobalIndex(95));
  EXPECT_TRUE(reloaded.hasClipByGlobalIndex(147));
}

TEST_F(SessionManagerTest, GetSessionName) {
  m_sessionManager->clearSession();
  std::string name = m_sessionManager->getSessionName();
  EXPECT_FALSE(name.empty());
}

TEST_F(SessionManagerTest, SaveAndReloadMissingMediaState) {
  SessionManager::ClipData clip;
  clip.filePath = m_tempDirectory.getChildFile("missing.wav").getFullPathName().toStdString();
  clip.displayName = "Missing Cue";
  clip.clipGroup = 2;
  clip.mediaAvailable = false;
  clip.mediaStatus = "Missing media";
  clip.trimOutSamples = 24000;

  m_sessionManager->setClip(0, clip, 0);

  ASSERT_TRUE(m_sessionManager->saveSession(m_sessionPath));

  SessionManager reloaded;
  ASSERT_TRUE(reloaded.loadSession(m_sessionPath));

  EXPECT_TRUE(reloaded.hasMissingMedia());
  ASSERT_EQ(reloaded.getMissingMediaResolutions().size(), 1u);

  const auto& missing = reloaded.getMissingMediaResolutions().front();
  EXPECT_EQ(missing.tabIndex, 0);
  EXPECT_EQ(missing.buttonIndex, 0);
  EXPECT_EQ(missing.originalPath, clip.filePath);

  auto loadedClip = reloaded.getClip(0, 0);
  EXPECT_FALSE(loadedClip.mediaAvailable);
  EXPECT_EQ(loadedClip.mediaStatus, "Missing media");
  EXPECT_EQ(loadedClip.displayName, "Missing Cue");
  EXPECT_EQ(loadedClip.clipGroup, 2);
}

TEST_F(SessionManagerTest, RelinkMissingMediaClearsRecoveryState) {
  SessionManager::ClipData clip;
  clip.filePath = m_tempDirectory.getChildFile("missing.wav").getFullPathName().toStdString();
  clip.displayName = "Missing Cue";
  clip.mediaAvailable = false;
  clip.mediaStatus = "Missing media";

  m_sessionManager->setClip(0, clip, 0);
  ASSERT_TRUE(m_sessionManager->saveSession(m_sessionPath));

  SessionManager reloaded;
  ASSERT_TRUE(reloaded.loadSession(m_sessionPath));
  ASSERT_TRUE(reloaded.hasMissingMedia());

  SessionManager::MissingMediaResolution resolution;
  ASSERT_TRUE(reloaded.relinkMissingMedia(0, 0, m_audioPath, &resolution));

  EXPECT_EQ(resolution.tabIndex, 0);
  EXPECT_EQ(resolution.buttonIndex, 0);
  EXPECT_EQ(resolution.resolvedPath, m_audioPath.getFullPathName().toStdString());
  EXPECT_FALSE(reloaded.hasMissingMedia());

  auto relinkedClip = reloaded.getClip(0, 0);
  EXPECT_TRUE(relinkedClip.mediaAvailable);
  EXPECT_TRUE(relinkedClip.mediaStatus.empty());
  EXPECT_EQ(relinkedClip.filePath, m_audioPath.getFullPathName().toStdString());
}

TEST_F(SessionManagerTest, ExportAndImportSessionPackage) {
  m_sessionManager->clearSession();
  ASSERT_TRUE(m_sessionManager->loadClip(0, m_audioPath.getFullPathName()));
  m_sessionManager->setSessionName("Package Session");
  ASSERT_TRUE(m_sessionManager->saveSession(m_sessionPath));

  SessionManager::SessionPackageManifest manifest;
  ASSERT_TRUE(m_sessionManager->exportSessionPackage(m_packageDirectory, &manifest));

  EXPECT_FALSE(manifest.packageId.empty());
  EXPECT_EQ(manifest.sessionName, "Package Session");
  EXPECT_GT(manifest.mediaCount, 0);
  EXPECT_TRUE(manifest.copiedMedia);

  EXPECT_TRUE(m_packageDirectory.getChildFile("session.json").existsAsFile());
  EXPECT_TRUE(m_packageDirectory.getChildFile("manifest.json").existsAsFile());
  EXPECT_TRUE(m_packageDirectory.getChildFile("media").exists());

  SessionManager imported;
  ASSERT_TRUE(imported.importSessionPackage(m_packageDirectory));

  EXPECT_EQ(imported.getClipCount(), 1);
  EXPECT_EQ(imported.getSessionName(), "Package Session");
  EXPECT_FALSE(imported.hasMissingMedia());
  EXPECT_EQ(imported.getSessionLineage().packageId, manifest.packageId);
  EXPECT_EQ(imported.getLastPackageManifest().packageId, manifest.packageId);
  EXPECT_EQ(imported.getCurrentFile().getFileName(), "session.json");
  EXPECT_EQ(imported.getCurrentFile().getParentDirectory().getFullPathName(),
            m_packageDirectory.getFullPathName());

  auto importedClip = imported.getClip(0, 0);
  EXPECT_TRUE(importedClip.mediaAvailable);
  EXPECT_NE(importedClip.filePath.find("media"), std::string::npos);
}
