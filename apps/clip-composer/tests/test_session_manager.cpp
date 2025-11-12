// SPDX-License-Identifier: MIT
// SessionManager Tests (Sprint A4)

#include "../Source/Session/SessionManager.h"
#include <gtest/gtest.h>
#include <juce_core/juce_core.h>

/**
 * Test Suite: Session Save/Load
 *
 * Tests session persistence and JSON serialization
 */

class SessionManagerTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_sessionManager = std::make_unique<SessionManager>();
    m_tempSessionPath = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getChildFile("clip_composer_test_session.json")
                            .getFullPathName();
  }

  void TearDown() override {
    // Clean up temp session file
    juce::File tempFile(m_tempSessionPath);
    if (tempFile.existsAsFile()) {
      tempFile.deleteFile();
    }
    m_sessionManager.reset();
  }

  std::unique_ptr<SessionManager> m_sessionManager;
  juce::String m_tempSessionPath;
};

TEST_F(SessionManagerTest, CreateNewSession) {
  // Create a new empty session
  bool success = m_sessionManager->newSession();
  EXPECT_TRUE(success);
}

TEST_F(SessionManagerTest, SaveSessionToFile) {
  m_sessionManager->newSession();

  // Save to temp file
  bool success = m_sessionManager->saveSession(m_tempSessionPath);
  EXPECT_TRUE(success);

  // Verify file was created
  juce::File sessionFile(m_tempSessionPath);
  EXPECT_TRUE(sessionFile.existsAsFile());
}

TEST_F(SessionManagerTest, LoadNonExistentSession) {
  // Attempt to load session from non-existent file
  bool success = m_sessionManager->loadSession("/tmp/nonexistent_session.json");
  EXPECT_FALSE(success);
}

TEST_F(SessionManagerTest, SaveAndLoadSession) {
  m_sessionManager->newSession();

  // Save session
  bool saved = m_sessionManager->saveSession(m_tempSessionPath);
  EXPECT_TRUE(saved);

  // Create new SessionManager and load
  auto newManager = std::make_unique<SessionManager>();
  bool loaded = newManager->loadSession(m_tempSessionPath);
  EXPECT_TRUE(loaded);
}

TEST_F(SessionManagerTest, GetSessionName) {
  m_sessionManager->newSession();
  juce::String name = m_sessionManager->getSessionName();
  EXPECT_FALSE(name.isEmpty());
}
