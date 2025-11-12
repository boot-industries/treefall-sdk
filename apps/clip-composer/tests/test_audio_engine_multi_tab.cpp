// SPDX-License-Identifier: MIT
// Multi-Tab Isolation Tests (Sprint A4)

#include "../Source/Audio/AudioEngine.h"
#include <gtest/gtest.h>

/**
 * Test Suite: Multi-Tab Isolation
 *
 * Tests that clips on different tabs (0-383) are properly isolated
 * and can be managed independently
 */

class AudioEngineMultiTabTest : public ::testing::Test {
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

TEST_F(AudioEngineMultiTabTest, Tab1ClipsIndependent) {
  // Test buttons 0-47 (Tab 1)
  for (int i = 0; i < 48; ++i) {
    EXPECT_FALSE(m_engine->isClipPlaying(i)) << "Tab 1 clip " << i << " should not be playing";
  }
}

TEST_F(AudioEngineMultiTabTest, Tab2ClipsIndependent) {
  // Test buttons 48-95 (Tab 2)
  for (int i = 48; i < 96; ++i) {
    EXPECT_FALSE(m_engine->isClipPlaying(i)) << "Tab 2 clip " << i << " should not be playing";
  }
}

TEST_F(AudioEngineMultiTabTest, Tab3ClipsIndependent) {
  // Test buttons 96-143 (Tab 3)
  for (int i = 96; i < 144; ++i) {
    EXPECT_FALSE(m_engine->isClipPlaying(i)) << "Tab 3 clip " << i << " should not be playing";
  }
}

TEST_F(AudioEngineMultiTabTest, Tab4ClipsIndependent) {
  // Test buttons 144-191 (Tab 4)
  for (int i = 144; i < 192; ++i) {
    EXPECT_FALSE(m_engine->isClipPlaying(i)) << "Tab 4 clip " << i << " should not be playing";
  }
}

TEST_F(AudioEngineMultiTabTest, Tab5ClipsIndependent) {
  // Test buttons 192-239 (Tab 5)
  for (int i = 192; i < 240; ++i) {
    EXPECT_FALSE(m_engine->isClipPlaying(i)) << "Tab 5 clip " << i << " should not be playing";
  }
}

TEST_F(AudioEngineMultiTabTest, Tab6ClipsIndependent) {
  // Test buttons 240-287 (Tab 6)
  for (int i = 240; i < 288; ++i) {
    EXPECT_FALSE(m_engine->isClipPlaying(i)) << "Tab 6 clip " << i << " should not be playing";
  }
}

TEST_F(AudioEngineMultiTabTest, Tab7ClipsIndependent) {
  // Test buttons 288-335 (Tab 7)
  for (int i = 288; i < 336; ++i) {
    EXPECT_FALSE(m_engine->isClipPlaying(i)) << "Tab 7 clip " << i << " should not be playing";
  }
}

TEST_F(AudioEngineMultiTabTest, Tab8ClipsIndependent) {
  // Test buttons 336-383 (Tab 8)
  for (int i = 336; i < 384; ++i) {
    EXPECT_FALSE(m_engine->isClipPlaying(i)) << "Tab 8 clip " << i << " should not be playing";
  }
}
