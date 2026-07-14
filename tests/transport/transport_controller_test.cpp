// SPDX-License-Identifier: MIT
#include "session/session_graph.h"
#include <gtest/gtest.h>
#include <orpheus/transport_controller.h>

#include <memory>
#include <vector>

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

TEST_F(TransportControllerTest, StopAllInGroupReportsNotSupported) {
  // ORP133 G2: stopAllInGroup was a silent no-op in every release (the
  // transport has no clip→group mapping — grouping is a host concern). It now
  // reports that truthfully: NotSupported for every group index, never a
  // silent no-op. Hosts scope group-stops with stopOtherClips() + their own
  // group model.
  EXPECT_EQ(m_transport->stopAllInGroup(0), SessionGraphError::NotSupported);
  EXPECT_EQ(m_transport->stopAllInGroup(3), SessionGraphError::NotSupported);
  EXPECT_EQ(m_transport->stopAllInGroup(4), SessionGraphError::NotSupported);
}

TEST_F(TransportControllerTest, GetCurrentPosition) {
  TransportPosition pos = m_transport->getCurrentPosition();

  // Initially at sample 0
  EXPECT_EQ(pos.samples, 0);
  EXPECT_DOUBLE_EQ(pos.seconds, 0.0);
}

namespace {

// Advance the transport by exactly one second of silence (no active clips —
// the timeline still moves).
void advanceOneSecond(ITransportController& transport, uint32_t sampleRate) {
  constexpr size_t kBlock = 480;
  std::vector<float> left(kBlock, 0.0f);
  std::vector<float> right(kBlock, 0.0f);
  float* buffers[2] = {left.data(), right.data()};
  for (uint32_t rendered = 0; rendered < sampleRate; rendered += kBlock) {
    transport.processAudio(buffers, 2, kBlock);
  }
}

} // namespace

// FTR027 §1: TransportPosition::beats derives from the session graph's real
// tempo, not a hardcoded 120 BPM.
TEST_F(TransportControllerTest, BeatsFollowSessionTempoAtConstruction) {
  m_sessionGraph->set_tempo(90.0);
  auto transport = createTransportController(m_sessionGraph.get(), 48000);

  advanceOneSecond(*transport, 48000);

  TransportPosition pos = transport->getCurrentPosition();
  EXPECT_EQ(pos.samples, 48000);
  EXPECT_DOUBLE_EQ(pos.seconds, 1.0);
  // 90 BPM = 1.5 beats per second. The old hardcoded tempo would report 2.0.
  EXPECT_DOUBLE_EQ(pos.beats, 1.5);
}

// FTR027 §1: a set_tempo() change after construction reaches beats on the
// next processCallbacks() pump.
TEST_F(TransportControllerTest, BeatsTrackLiveTempoChange) {
  auto transport = createTransportController(m_sessionGraph.get(), 48000);

  advanceOneSecond(*transport, 48000);

  // Session default is 120 BPM = 2 beats per second.
  EXPECT_DOUBLE_EQ(transport->getCurrentPosition().beats, 2.0);

  m_sessionGraph->set_tempo(150.0);

  // Not yet published: beats still reflect the previously cached tempo.
  EXPECT_DOUBLE_EQ(transport->getCurrentPosition().beats, 2.0);

  transport->processCallbacks(); // control-thread pump republishes the tempo

  EXPECT_DOUBLE_EQ(transport->getCurrentPosition().beats, 2.5);
}

TEST_F(TransportControllerTest, PublicInterfaceReportsRenderContract) {
  const TransportRenderConfig config = m_transport->getRenderConfig();
  EXPECT_EQ(config.sampleRate, 48000u);
  EXPECT_EQ(config.outputChannels, 2u);
  EXPECT_EQ(config.maxBlockFrames, 2048u);
}

TEST_F(TransportControllerTest, PublicInterfaceRendersAndDrainsCallbacks) {
  const TransportRenderConfig config = m_transport->getRenderConfig();
  ASSERT_GE(config.outputChannels, 2u);
  ASSERT_GE(config.maxBlockFrames, 64u);

  constexpr size_t kFrames = 64;
  std::vector<std::vector<float>> storage(
      config.outputChannels, std::vector<float>(kFrames, 1.0f));
  std::vector<float*> outputs;
  outputs.reserve(config.outputChannels);
  for (auto& channel : storage) {
    outputs.push_back(channel.data());
  }

  TestCallback callback;
  m_transport->setCallback(&callback);
  ASSERT_EQ(m_transport->startClip(42), SessionGraphError::OK);

  m_transport->processAudio(outputs.data(), outputs.size(), kFrames);
  EXPECT_EQ(callback.startCount, 0);
  m_transport->processCallbacks();

  EXPECT_EQ(callback.startCount, 1);
  EXPECT_EQ(callback.lastHandle, 42u);
  EXPECT_EQ(m_transport->getCurrentPosition().samples, static_cast<int64_t>(kFrames));
  for (const auto& channel : storage) {
    for (float sample : channel) {
      EXPECT_FLOAT_EQ(sample, 0.0f);
    }
  }
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
  EXPECT_EQ(m_transport->registerClipAudio(0, "missing.wav"), SessionGraphError::InvalidHandle);
  EXPECT_EQ(m_transport->prepareClipAudio(0), SessionGraphError::InvalidHandle);
}

TEST_F(TransportControllerTest, PrepareUnregisteredClipFails) {
  EXPECT_EQ(m_transport->prepareClipAudio(123), SessionGraphError::ClipNotRegistered);
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
