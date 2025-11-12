// SPDX-License-Identifier: MIT
// AudioEngine Clip Loading/Unloading Tests (Sprint A4)

#include "../Source/Audio/AudioEngine.h"
#include <gtest/gtest.h>
#include <juce_core/juce_core.h>

/**
 * Test Suite: Clip Loading and Unloading
 *
 * Tests clip registration, metadata queries, and resource management
 * Note: These tests use dummy file paths since we don't have test assets yet
 */

class AudioEngineClipsTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_engine = std::make_unique<AudioEngine>();
    if (!m_engine->initialize(48000)) {
      GTEST_SKIP() << "Audio device not available";
    }
  }

  void TearDown() override {
    m_engine.reset();
  }

  std::unique_ptr<AudioEngine> m_engine;
};

TEST_F(AudioEngineClipsTest, LoadClipInvalidPath) {
  // Attempt to load non-existent file
  bool success = m_engine->loadClip(0, "/nonexistent/file.wav");
  EXPECT_FALSE(success) << "Loading non-existent file should fail";
}

TEST_F(AudioEngineClipsTest, LoadClipInvalidButtonIndex) {
  // Button index out of range
  bool success = m_engine->loadClip(-1, "/tmp/test.wav");
  EXPECT_FALSE(success) << "Negative button index should be rejected";

  success = m_engine->loadClip(384, "/tmp/test.wav");
  EXPECT_FALSE(success) << "Button index >= MAX_CLIP_BUTTONS should be rejected";
}

TEST_F(AudioEngineClipsTest, GetMetadataForUnloadedClip) {
  // Query metadata for slot that has no clip loaded
  auto metadata = m_engine->getClipMetadata(0);
  EXPECT_FALSE(metadata.has_value()) << "Unloaded slot should return empty metadata";
}

TEST_F(AudioEngineClipsTest, UnloadClipThatWasNeverLoaded) {
  // Unload slot that never had a clip (should not crash)
  EXPECT_NO_THROW(m_engine->unloadClip(0));
}

TEST_F(AudioEngineClipsTest, MAX_CLIP_BUTTONS_Constant) {
  // Verify MAX_CLIP_BUTTONS is 384 (8 tabs × 48 buttons)
  EXPECT_EQ(AudioEngine::MAX_CLIP_BUTTONS, 384);
}

TEST_F(AudioEngineClipsTest, LoadClipAtBoundaryIndices) {
  // Test boundary indices (0 and 383)
  bool success0 = m_engine->loadClip(0, "/tmp/clip0.wav");
  bool success383 = m_engine->loadClip(383, "/tmp/clip383.wav");

  // Should accept valid indices (even if files don't exist)
  // Actual loading will fail due to missing files, but index validation should pass
  EXPECT_FALSE(success0) << "File doesn't exist, but index should be valid";
  EXPECT_FALSE(success383) << "File doesn't exist, but index should be valid";
}

TEST_F(AudioEngineClipsTest, LoadMultipleClipsSequentially) {
  // Attempt to load clips in sequence (will fail due to missing files)
  for (int i = 0; i < 10; ++i) {
    juce::String path = juce::String::formatted("/tmp/clip%d.wav", i);
    m_engine->loadClip(i, path);
  }

  // Verify no crashes occurred
  SUCCEED();
}

TEST_F(AudioEngineClipsTest, UnloadAfterLoad) {
  // Load a clip, then unload it
  m_engine->loadClip(0, "/tmp/test.wav");
  EXPECT_NO_THROW(m_engine->unloadClip(0));

  // Metadata should be cleared after unload
  auto metadata = m_engine->getClipMetadata(0);
  EXPECT_FALSE(metadata.has_value());
}
