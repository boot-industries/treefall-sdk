// SPDX-License-Identifier: MIT
// AudioEngine Playback Control Tests (Sprint A4)

#include "../Source/Audio/AudioEngine.h"
#include <gtest/gtest.h>

/**
 * Test Suite: Clip Playback Control
 *
 * Tests clip triggering, stopping, state queries, and panic stop
 */

class AudioEnginePlaybackTest : public ::testing::Test {
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

TEST_F(AudioEnginePlaybackTest, StartClipNotLoaded) {
  // Attempt to start clip that was never loaded
  bool success = m_engine->startClip(0);
  EXPECT_FALSE(success) << "Starting unloaded clip should fail";
}

TEST_F(AudioEnginePlaybackTest, StopClipNotPlaying) {
  // Attempt to stop clip that is not playing
  bool success = m_engine->stopClip(0);
  // Should return gracefully (implementation-defined whether true/false)
  SUCCEED();
}

TEST_F(AudioEnginePlaybackTest, IsClipPlayingForUnloadedClip) {
  // Query playing state for unloaded clip
  bool playing = m_engine->isClipPlaying(0);
  EXPECT_FALSE(playing) << "Unloaded clip should not be playing";
}

TEST_F(AudioEnginePlaybackTest, StartClipInvalidIndex) {
  // Attempt to start clip with invalid index
  bool success = m_engine->startClip(-1);
  EXPECT_FALSE(success);

  success = m_engine->startClip(384);
  EXPECT_FALSE(success);
}

TEST_F(AudioEnginePlaybackTest, StopAllClipsWhenNonePlaying) {
  // Call stopAllClips when no clips are playing (should not crash)
  EXPECT_NO_THROW(m_engine->stopAllClips());
}

TEST_F(AudioEnginePlaybackTest, PanicStopWhenNonePlaying) {
  // Call panicStop when no clips are playing (should not crash)
  EXPECT_NO_THROW(m_engine->panicStop());
}

TEST_F(AudioEnginePlaybackTest, GetClipPositionForUnloadedClip) {
  // Query playback position for unloaded clip
  int64_t position = m_engine->getClipPosition(0);
  EXPECT_EQ(position, -1) << "Unloaded clip should return -1 for position";
}

TEST_F(AudioEnginePlaybackTest, SetLoopModeForUnloadedClip) {
  // Attempt to set loop mode for unloaded clip
  bool success = m_engine->setClipLoopMode(0, true);
  EXPECT_FALSE(success) << "Setting loop mode for unloaded clip should fail";
}

TEST_F(AudioEnginePlaybackTest, SeekUnloadedClip) {
  // Attempt to seek unloaded clip
  bool success = m_engine->seekClip(0, 48000);
  EXPECT_FALSE(success) << "Seeking unloaded clip should fail";
}

TEST_F(AudioEnginePlaybackTest, GetCurrentPosition) {
  // Get current transport position (should always succeed)
  auto position = m_engine->getCurrentPosition();
  // Position should have non-negative sample count
  EXPECT_GE(position.samples, 0);
}
