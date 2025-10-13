// SPDX-License-Identifier: MIT
#include "session/session_graph.h"
#include <gtest/gtest.h>
#include <orpheus/transport_controller.h>

using namespace orpheus;

// Mock session graph for testing
class MockSessionGraph : public core::SessionGraph {
public:
  MockSessionGraph() : core::SessionGraph() {}
};

// Test fixture
class TransportControllerTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_sessionGraph = std::make_unique<MockSessionGraph>();
    m_transport = createTransportController(m_sessionGraph.get(), 48000);
  }

  void TearDown() override {
    m_transport.reset();
    m_sessionGraph.reset();
  }

  std::unique_ptr<MockSessionGraph> m_sessionGraph;
  std::unique_ptr<ITransportController> m_transport;
};

// Test callback implementation
class TestCallback : public ITransportCallback {
public:
  int startCount = 0;
  int stopCount = 0;
  int loopCount = 0;
  int underrunCount = 0;
  ClipHandle lastHandle = 0;

  void onClipStarted(ClipHandle handle, TransportPosition position) override {
    ++startCount;
    lastHandle = handle;
  }

  void onClipStopped(ClipHandle handle, TransportPosition position) override {
    ++stopCount;
    lastHandle = handle;
  }

  void onClipLooped(ClipHandle handle, TransportPosition position) override {
    ++loopCount;
    lastHandle = handle;
  }

  void onBufferUnderrun(TransportPosition position) override {
    ++underrunCount;
  }
};

// Basic Tests

TEST_F(TransportControllerTest, InitialState) {
  // Initially, no clips should be playing
  ClipHandle handle = 1;
  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Stopped);
  EXPECT_FALSE(m_transport->isClipPlaying(handle));
}

TEST_F(TransportControllerTest, StartClip) {
  ClipHandle handle = 1;

  // Start clip
  EXPECT_EQ(m_transport->startClip(handle), SessionGraphError::OK);

  // TODO: Process audio callback to actually start the clip
  // For now, state remains Stopped until processAudio() is called
}

TEST_F(TransportControllerTest, StopClip) {
  ClipHandle handle = 1;

  // Start and then stop
  EXPECT_EQ(m_transport->startClip(handle), SessionGraphError::OK);
  EXPECT_EQ(m_transport->stopClip(handle), SessionGraphError::OK);
}

TEST_F(TransportControllerTest, StopAllClips) {
  // Start multiple clips
  EXPECT_EQ(m_transport->startClip(1), SessionGraphError::OK);
  EXPECT_EQ(m_transport->startClip(2), SessionGraphError::OK);
  EXPECT_EQ(m_transport->startClip(3), SessionGraphError::OK);

  // Stop all
  EXPECT_EQ(m_transport->stopAllClips(), SessionGraphError::OK);
}

TEST_F(TransportControllerTest, StopAllInGroup) {
  // Stop all clips in group 0
  EXPECT_EQ(m_transport->stopAllInGroup(0), SessionGraphError::OK);

  // Invalid group index should fail
  EXPECT_EQ(m_transport->stopAllInGroup(4), SessionGraphError::InvalidParameter);
}

TEST_F(TransportControllerTest, GetCurrentPosition) {
  TransportPosition pos = m_transport->getCurrentPosition();

  // Initially at sample 0
  EXPECT_EQ(pos.samples, 0);
  EXPECT_DOUBLE_EQ(pos.seconds, 0.0);
}

TEST_F(TransportControllerTest, Callback) {
  TestCallback callback;
  m_transport->setCallback(&callback);

  // Initially no callbacks
  EXPECT_EQ(callback.startCount, 0);
  EXPECT_EQ(callback.stopCount, 0);
}

TEST_F(TransportControllerTest, InvalidHandle) {
  // Handle 0 is invalid
  EXPECT_EQ(m_transport->startClip(0), SessionGraphError::InvalidHandle);
  EXPECT_EQ(m_transport->stopClip(0), SessionGraphError::InvalidHandle);
}

TEST_F(TransportControllerTest, StartClipTwice) {
  ClipHandle handle = 1;

  // Starting twice should be idempotent
  EXPECT_EQ(m_transport->startClip(handle), SessionGraphError::OK);
  EXPECT_EQ(m_transport->startClip(handle), SessionGraphError::OK);
}

// TODO: Add more comprehensive tests:
// - Sample-accurate timing (±1 sample)
// - Multi-clip playback (16 simultaneous)
// - Fade-out behavior
// - Callback invocation
// - Command queue overflow handling
// - Integration with actual audio processing
