// SPDX-License-Identifier: MIT
// Multi-Tab Isolation Tests (Sprint A4)

#include "../Source/Audio/AudioEngine.h"
#include <gtest/gtest.h>

/**
 * Test Suite: Multi-Tab Isolation
 *
 * Tests that clips on different tabs are properly isolated
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

TEST_F(AudioEngineMultiTabTest, LogicalTabsIndependent) {
  for (int tab = 0; tab < occ::NUM_TABS; ++tab) {
    const int startIndex = tab * occ::BUTTONS_PER_TAB;
    const int endIndex = startIndex + occ::BUTTONS_PER_TAB;
    for (int i = startIndex; i < endIndex; ++i) {
      EXPECT_FALSE(m_engine->isClipPlaying(i))
          << "Tab " << (tab + 1) << " clip " << i << " should not be playing";
    }
  }
}
