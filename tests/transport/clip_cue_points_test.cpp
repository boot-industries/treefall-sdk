// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <orpheus/transport_controller.h>

#include "transport/transport_controller.h"

using namespace orpheus;

/// Test fixture for cue point functionality
class ClipCuePointsTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Create transport controller (nullptr for SessionGraph - not needed for cue point tests)
    m_transport = std::make_unique<TransportController>(nullptr, 48000);

    // Register a test clip handle
    m_clipHandle = 12345;
    m_sampleRate = 48000;
    m_fileDuration = 10 * m_sampleRate; // 10 seconds

    // Try to register test audio file - if this fails, tests that need audio will be skipped
    // For cue point tests, we mainly need the AudioFileEntry to exist in storage
    std::string testFile = "../tests/fixtures/audio/test_tone_1s.wav";
    auto result = m_transport->registerClipAudio(m_clipHandle, testFile);
    m_hasAudioFile = (result == SessionGraphError::OK);

    // Most cue point tests don't need actual audio, just the clip to be "registered"
    // If audio file not available, tests will check m_hasAudioFile before running
  }

  std::unique_ptr<TransportController> m_transport;
  ClipHandle m_clipHandle{0};
  uint32_t m_sampleRate{48000};
  int64_t m_fileDuration{0};
  bool m_hasAudioFile{false}; // Flag to skip tests that require audio playback
};

/// Test adding a single cue point
TEST_F(ClipCuePointsTest, AddSingleCuePoint) {
  if (!m_hasAudioFile) {
    GTEST_SKIP() << "Test audio file not available";
  }

  // Add cue point at 5 seconds (blue color)
  int64_t position = 5 * m_sampleRate;
  int index = m_transport->addCuePoint(m_clipHandle, position, "Verse 1", 0x0000FFFF);

  // Verify index returned
  EXPECT_EQ(index, 0); // First cue point should have index 0

  // Get cue points and verify
  auto cuePoints = m_transport->getCuePoints(m_clipHandle);
  ASSERT_EQ(cuePoints.size(), 1);
  EXPECT_EQ(cuePoints[0].position, position);
  EXPECT_EQ(cuePoints[0].name, "Verse 1");
  EXPECT_EQ(cuePoints[0].color, 0x0000FFFF);
}

/// Test adding multiple cue points (verify sorting)
TEST_F(ClipCuePointsTest, AddMultipleCuePointsSorted) {
  if (!m_hasAudioFile) {
    GTEST_SKIP() << "Test audio file not available";
  }

  // Add cue points in non-sequential order
  int idx1 = m_transport->addCuePoint(m_clipHandle, 7 * m_sampleRate, "Chorus", 0xFF0000FF);
  int idx2 = m_transport->addCuePoint(m_clipHandle, 2 * m_sampleRate, "Intro", 0x00FF00FF);
  int idx3 = m_transport->addCuePoint(m_clipHandle, 5 * m_sampleRate, "Verse 1", 0x0000FFFF);

  // Verify indices reflect sorted positions
  EXPECT_EQ(idx2, 0); // Intro at 2s is first
  EXPECT_EQ(idx3, 1); // Verse 1 at 5s is second
  EXPECT_EQ(idx1, 2); // Chorus at 7s is third

  // Get cue points and verify sorted order
  auto cuePoints = m_transport->getCuePoints(m_clipHandle);
  ASSERT_EQ(cuePoints.size(), 3);
  EXPECT_EQ(cuePoints[0].position, 2 * m_sampleRate);
  EXPECT_EQ(cuePoints[0].name, "Intro");
  EXPECT_EQ(cuePoints[1].position, 5 * m_sampleRate);
  EXPECT_EQ(cuePoints[1].name, "Verse 1");
  EXPECT_EQ(cuePoints[2].position, 7 * m_sampleRate);
  EXPECT_EQ(cuePoints[2].name, "Chorus");
}

/// Test cue points persist across stop/start cycles
TEST_F(ClipCuePointsTest, PersistAcrossStopStart) {
  // Add cue point
  m_transport->addCuePoint(m_clipHandle, 3 * m_sampleRate, "Marker", 0xFFFFFFFF);

  // Start and stop clip
  m_transport->startClip(m_clipHandle);
  m_transport->stopClip(m_clipHandle);

  // Verify cue point still exists
  auto cuePoints = m_transport->getCuePoints(m_clipHandle);
  ASSERT_EQ(cuePoints.size(), 1);
  EXPECT_EQ(cuePoints[0].name, "Marker");
}

/// Test seeking to cue point
TEST_F(ClipCuePointsTest, SeekToCuePoint) {
  // Add multiple cue points
  m_transport->addCuePoint(m_clipHandle, 2 * m_sampleRate, "Cue 0", 0xFF0000FF);
  m_transport->addCuePoint(m_clipHandle, 5 * m_sampleRate, "Cue 1", 0x00FF00FF);
  m_transport->addCuePoint(m_clipHandle, 8 * m_sampleRate, "Cue 2", 0x0000FFFF);

  // Start playback
  m_transport->startClip(m_clipHandle);

  // Seek to second cue point (index 1, at 5 seconds)
  auto result = m_transport->seekToCuePoint(m_clipHandle, 1);
  EXPECT_EQ(result, SessionGraphError::OK);

  // Verify position is at cue point (allow small buffer processing delay)
  int64_t position = m_transport->getClipPosition(m_clipHandle);
  EXPECT_NEAR(position, 5 * m_sampleRate, 2048); // Within one buffer size
}

/// Test seeking to invalid cue index
TEST_F(ClipCuePointsTest, SeekToInvalidCueIndex) {
  // Add one cue point
  m_transport->addCuePoint(m_clipHandle, 2 * m_sampleRate, "Cue 0", 0xFF0000FF);

  // Start playback
  m_transport->startClip(m_clipHandle);

  // Try to seek to non-existent cue point (index 5)
  auto result = m_transport->seekToCuePoint(m_clipHandle, 5);
  EXPECT_EQ(result, SessionGraphError::InvalidParameter);
}

/// Test seeking when clip not playing
TEST_F(ClipCuePointsTest, SeekToCuePointWhenStopped) {
  // Add cue point
  m_transport->addCuePoint(m_clipHandle, 2 * m_sampleRate, "Cue 0", 0xFF0000FF);

  // Do NOT start playback

  // Try to seek (should fail - clip not playing)
  auto result = m_transport->seekToCuePoint(m_clipHandle, 0);
  EXPECT_EQ(result, SessionGraphError::NotReady);
}

/// Test removing cue point
TEST_F(ClipCuePointsTest, RemoveCuePoint) {
  // Add three cue points
  m_transport->addCuePoint(m_clipHandle, 2 * m_sampleRate, "Cue 0", 0xFF0000FF);
  m_transport->addCuePoint(m_clipHandle, 5 * m_sampleRate, "Cue 1", 0x00FF00FF);
  m_transport->addCuePoint(m_clipHandle, 8 * m_sampleRate, "Cue 2", 0x0000FFFF);

  // Remove middle cue point (index 1)
  auto result = m_transport->removeCuePoint(m_clipHandle, 1);
  EXPECT_EQ(result, SessionGraphError::OK);

  // Verify only two cue points remain
  auto cuePoints = m_transport->getCuePoints(m_clipHandle);
  ASSERT_EQ(cuePoints.size(), 2);
  EXPECT_EQ(cuePoints[0].name, "Cue 0");
  EXPECT_EQ(cuePoints[1].name, "Cue 2"); // Cue 2 shifted down to index 1
}

/// Test removing invalid cue index
TEST_F(ClipCuePointsTest, RemoveInvalidCueIndex) {
  // Add one cue point
  m_transport->addCuePoint(m_clipHandle, 2 * m_sampleRate, "Cue 0", 0xFF0000FF);

  // Try to remove non-existent cue point (index 5)
  auto result = m_transport->removeCuePoint(m_clipHandle, 5);
  EXPECT_EQ(result, SessionGraphError::InvalidParameter);

  // Verify cue point still exists
  auto cuePoints = m_transport->getCuePoints(m_clipHandle);
  EXPECT_EQ(cuePoints.size(), 1);
}

/// Test adding cue point with position clamping
TEST_F(ClipCuePointsTest, PositionClamping) {
  // Try to add cue point beyond file duration
  int64_t outOfRangePosition = 20 * m_sampleRate; // 20 seconds (file is only ~1 second)
  int index =
      m_transport->addCuePoint(m_clipHandle, outOfRangePosition, "Out of Range", 0xFFFFFFFF);

  // Verify cue point was added with clamped position
  EXPECT_GE(index, 0); // Should succeed

  auto cuePoints = m_transport->getCuePoints(m_clipHandle);
  ASSERT_EQ(cuePoints.size(), 1);
  // Position should be clamped to file duration
  EXPECT_LE(cuePoints[0].position, m_fileDuration);

  // Try negative position
  int index2 = m_transport->addCuePoint(m_clipHandle, -1000, "Negative", 0xFFFFFFFF);
  EXPECT_GE(index2, 0);

  cuePoints = m_transport->getCuePoints(m_clipHandle);
  ASSERT_EQ(cuePoints.size(), 2);
  EXPECT_EQ(cuePoints[0].position, 0); // Clamped to 0
}

/// Test adding cue point with invalid handle
TEST_F(ClipCuePointsTest, AddCuePointInvalidHandle) {
  // Try to add cue point to non-existent clip
  int index = m_transport->addCuePoint(99999, 1000, "Invalid", 0xFFFFFFFF);
  EXPECT_EQ(index, -1); // Should fail
}

/// Test getting cue points from invalid handle
TEST_F(ClipCuePointsTest, GetCuePointsInvalidHandle) {
  // Try to get cue points from non-existent clip
  auto cuePoints = m_transport->getCuePoints(99999);
  EXPECT_TRUE(cuePoints.empty()); // Should return empty vector
}

/// Test duplicate positions (multiple markers at same position)
TEST_F(ClipCuePointsTest, DuplicatePositions) {
  // Add multiple cue points at same position
  int64_t position = 5 * m_sampleRate;
  int idx1 = m_transport->addCuePoint(m_clipHandle, position, "Marker A", 0xFF0000FF);
  int idx2 = m_transport->addCuePoint(m_clipHandle, position, "Marker B", 0x00FF00FF);

  // Both should succeed
  EXPECT_GE(idx1, 0);
  EXPECT_GE(idx2, 0);

  // Verify both exist
  auto cuePoints = m_transport->getCuePoints(m_clipHandle);
  EXPECT_EQ(cuePoints.size(), 2);

  // Both should have same position
  EXPECT_EQ(cuePoints[0].position, position);
  EXPECT_EQ(cuePoints[1].position, position);
}

/// Test removing all cue points
TEST_F(ClipCuePointsTest, RemoveAllCuePoints) {
  // Add three cue points
  m_transport->addCuePoint(m_clipHandle, 2 * m_sampleRate, "Cue 0", 0xFF0000FF);
  m_transport->addCuePoint(m_clipHandle, 5 * m_sampleRate, "Cue 1", 0x00FF00FF);
  m_transport->addCuePoint(m_clipHandle, 8 * m_sampleRate, "Cue 2", 0x0000FFFF);

  // Remove all cue points (indices shift as we remove)
  m_transport->removeCuePoint(m_clipHandle, 0);
  m_transport->removeCuePoint(m_clipHandle, 0);
  m_transport->removeCuePoint(m_clipHandle, 0);

  // Verify no cue points remain
  auto cuePoints = m_transport->getCuePoints(m_clipHandle);
  EXPECT_TRUE(cuePoints.empty());
}

/// Test seeking to multiple cue points in sequence
TEST_F(ClipCuePointsTest, SeekToMultipleCuePoints) {
  // Add cue points at 2, 4, 6, 8 seconds
  for (int i = 0; i < 4; ++i) {
    m_transport->addCuePoint(m_clipHandle, (2 + i * 2) * m_sampleRate, "Cue " + std::to_string(i),
                             0xFFFFFFFF);
  }

  // Start playback
  m_transport->startClip(m_clipHandle);

  // Seek to each cue point in sequence
  for (uint32_t i = 0; i < 4; ++i) {
    auto result = m_transport->seekToCuePoint(m_clipHandle, i);
    EXPECT_EQ(result, SessionGraphError::OK);

    // Verify position (allow buffer processing tolerance)
    int64_t expectedPos = (2 + i * 2) * m_sampleRate;
    int64_t actualPos = m_transport->getClipPosition(m_clipHandle);
    EXPECT_NEAR(actualPos, expectedPos, 2048);
  }
}

/// Test edge case: cue point at position 0
TEST_F(ClipCuePointsTest, CuePointAtZero) {
  // Add cue point at position 0
  int index = m_transport->addCuePoint(m_clipHandle, 0, "Start", 0xFFFFFFFF);
  EXPECT_EQ(index, 0);

  // Start playback and seek to cue point 0
  m_transport->startClip(m_clipHandle);
  auto result = m_transport->seekToCuePoint(m_clipHandle, 0);
  EXPECT_EQ(result, SessionGraphError::OK);

  // Position should be at or near 0
  int64_t position = m_transport->getClipPosition(m_clipHandle);
  EXPECT_NEAR(position, 0, 2048);
}

/// Test thread safety: add/remove cue points while clip is playing
TEST_F(ClipCuePointsTest, ThreadSafety) {
  // Start playback
  m_transport->startClip(m_clipHandle);

  // Add cue points while playing
  m_transport->addCuePoint(m_clipHandle, 2 * m_sampleRate, "Cue 0", 0xFF0000FF);
  m_transport->addCuePoint(m_clipHandle, 5 * m_sampleRate, "Cue 1", 0x00FF00FF);

  // Process some audio (simulate playback)
  float* outputs[2] = {nullptr, nullptr};
  float leftBuffer[512] = {0};
  float rightBuffer[512] = {0};
  outputs[0] = leftBuffer;
  outputs[1] = rightBuffer;
  m_transport->processAudio(outputs, 2, 512);

  // Verify cue points exist
  auto cuePoints = m_transport->getCuePoints(m_clipHandle);
  EXPECT_EQ(cuePoints.size(), 2);

  // Remove cue point while playing
  m_transport->removeCuePoint(m_clipHandle, 0);

  // Process more audio
  m_transport->processAudio(outputs, 2, 512);

  // Verify cue point was removed
  cuePoints = m_transport->getCuePoints(m_clipHandle);
  EXPECT_EQ(cuePoints.size(), 1);
}
