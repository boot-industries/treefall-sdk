// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include "../../src/core/transport/transport_controller.h"
#include <fstream>
#include <orpheus/audio_file_reader.h>
#include <vector>

using namespace orpheus;

// Test fixture for clip loop mode functionality
class ClipLoopTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_transport = std::make_unique<TransportController>(nullptr, 48000);

    // Create test audio file (short clip for faster loop testing)
    createTestAudioFile();
  }

  void TearDown() override {
    m_transport.reset();
    std::remove("/tmp/test_clip_loop.wav");
  }

  void createTestAudioFile() {
    // Create a short WAV file (0.1 seconds = 4800 samples @ 48kHz, stereo, 16-bit PCM)
    // Short duration makes loop testing faster
    std::ofstream file("/tmp/test_clip_loop.wav", std::ios::binary);

    const uint32_t sampleRate = 48000;
    const uint32_t duration = 4800; // 0.1 seconds
    const uint16_t numChannels = 2;

    // WAV header (RIFF)
    file << "RIFF";
    uint32_t fileSize = 36 + (duration * numChannels * 2);
    file.write(reinterpret_cast<const char*>(&fileSize), 4);
    file << "WAVE";

    // fmt chunk
    file << "fmt ";
    uint32_t fmtSize = 16;
    file.write(reinterpret_cast<const char*>(&fmtSize), 4);
    uint16_t audioFormat = 1; // PCM
    file.write(reinterpret_cast<const char*>(&audioFormat), 2);
    file.write(reinterpret_cast<const char*>(&numChannels), 2);
    file.write(reinterpret_cast<const char*>(&sampleRate), 4);
    uint32_t byteRate = sampleRate * numChannels * 2;
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    uint16_t blockAlign = numChannels * 2;
    file.write(reinterpret_cast<const char*>(&blockAlign), 2);
    uint16_t bitsPerSample = 16;
    file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    // data chunk
    file << "data";
    uint32_t dataSize = duration * numChannels * 2;
    file.write(reinterpret_cast<const char*>(&dataSize), 4);

    // Write ramp signal (makes loop boundary audible for debugging)
    for (uint32_t i = 0; i < duration; ++i) {
      float ramp = static_cast<float>(i) / static_cast<float>(duration);
      float sample = 0.3f * ramp; // Ramp from 0 to 0.3
      int16_t pcmSample = static_cast<int16_t>(sample * 32767.0f);

      // Write stereo
      file.write(reinterpret_cast<const char*>(&pcmSample), 2);
      file.write(reinterpret_cast<const char*>(&pcmSample), 2);
    }
    file.close();
  }

  std::unique_ptr<TransportController> m_transport;
};

// Test 1: setClipLoopMode enables looping
TEST_F(ClipLoopTest, SetLoopModeEnablesLooping) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_loop.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Enable loop mode
  auto result = m_transport->setClipLoopMode(handle, true);
  EXPECT_EQ(result, SessionGraphError::OK);

  // Verify loop mode is enabled
  auto metadata = m_transport->getClipMetadata(handle);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_TRUE(metadata->loopEnabled) << "Loop mode should be enabled";
}

// Test 2: setClipLoopMode disables looping
TEST_F(ClipLoopTest, SetLoopModeDisablesLooping) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_loop.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Enable loop mode first
  m_transport->setClipLoopMode(handle, true);

  // Now disable it
  auto result = m_transport->setClipLoopMode(handle, false);
  EXPECT_EQ(result, SessionGraphError::OK);

  // Verify loop mode is disabled
  auto metadata = m_transport->getClipMetadata(handle);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_FALSE(metadata->loopEnabled) << "Loop mode should be disabled";
}

// Test 3: Loop boundary behavior (trim OUT → trim IN seek)
TEST_F(ClipLoopTest, LoopBoundarySeeksToTrimIn) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_loop.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set trim points (loop from 1000 to 3000 samples)
  int64_t trimIn = 1000;
  int64_t trimOut = 3000;
  m_transport->updateClipTrimPoints(handle, trimIn, trimOut);

  // Enable loop mode
  m_transport->setClipLoopMode(handle, true);

  // Start clip
  m_transport->startClip(handle);

  // Process audio
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();

  // Process enough audio to reach trim OUT point
  // Clip duration: 3000 - 1000 = 2000 samples
  // Process 2500 samples (past trim OUT) to trigger loop
  for (int i = 0; i < 6; ++i) { // 6 * 512 = 3072 samples
    m_transport->processAudio(buffers, 2, 512);
  }

  // Verify clip is still playing (looped back)
  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Playing);

  // Verify position is within trim bounds (should have looped back to trim IN)
  int64_t position = m_transport->getClipPosition(handle);
  EXPECT_GE(position, trimIn) << "Position should be >= trim IN after loop";
  EXPECT_LT(position, trimOut) << "Position should be < trim OUT after loop";
}

// Test 4: onClipLooped callback fires when clip loops
TEST_F(ClipLoopTest, OnClipLoopedCallbackFires) {
  // Callback test fixture
  class TestCallback : public ITransportCallback {
  public:
    void onClipStarted(ClipHandle handle, TransportPosition position) override {
      startedHandle = handle;
    }

    void onClipStopped(ClipHandle /*handle*/, TransportPosition /*position*/) override {}

    void onClipLooped(ClipHandle handle, TransportPosition position) override {
      loopedHandle = handle;
      loopedPosition = position;
      loopCount++;
    }

    void onBufferUnderrun(TransportPosition /*position*/) override {}

    ClipHandle startedHandle = 0;
    ClipHandle loopedHandle = 0;
    TransportPosition loopedPosition = {0, 0.0, 0.0};
    int loopCount = 0;
  };

  auto callback = std::make_unique<TestCallback>();
  m_transport->setCallback(callback.get());

  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_loop.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set short trim for faster loop
  m_transport->updateClipTrimPoints(handle, 0, 2000);

  // Enable loop mode
  m_transport->setClipLoopMode(handle, true);

  // Start clip
  m_transport->startClip(handle);

  // Process audio
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();

  // Process enough audio to trigger loop (2000 samples / 512 = ~4 buffers)
  for (int i = 0; i < 6; ++i) {
    m_transport->processAudio(buffers, 2, 512);
    m_transport->processCallbacks(); // Process callbacks
  }

  // Verify loop callback was fired
  EXPECT_EQ(callback->loopedHandle, handle) << "onClipLooped should be called with correct handle";
  EXPECT_GE(callback->loopCount, 1) << "Loop callback should fire at least once";

  m_transport->setCallback(nullptr);
}

// Test 5: Loop mode with trim points respects IN/OUT boundaries
TEST_F(ClipLoopTest, LoopModeRespectsTrims) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_loop.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set trim points
  int64_t trimIn = 500;
  int64_t trimOut = 2500;
  m_transport->updateClipTrimPoints(handle, trimIn, trimOut);

  // Enable loop mode
  m_transport->setClipLoopMode(handle, true);

  // Start clip
  m_transport->startClip(handle);

  // Process audio
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();

  // Process multiple loops
  for (int loop = 0; loop < 3; ++loop) {
    // Process enough audio for one loop iteration
    for (int i = 0; i < 5; ++i) {
      m_transport->processAudio(buffers, 2, 512);
    }

    // Verify position stays within trim bounds
    int64_t position = m_transport->getClipPosition(handle);
    EXPECT_GE(position, trimIn) << "Position should never be below trim IN (loop " << loop << ")";
    EXPECT_LT(position, trimOut) << "Position should never exceed trim OUT (loop " << loop << ")";
  }
}

// Test 6: Loop mode without fade-out at loop boundary
TEST_F(ClipLoopTest, LoopModeNoFadeAtBoundary) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_loop.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set fade-out (should NOT be applied at loop boundary)
  m_transport->updateClipFades(handle, 0.0, 0.01, FadeCurve::Linear, FadeCurve::Linear);

  // Enable loop mode
  m_transport->setClipLoopMode(handle, true);

  // Start clip
  m_transport->startClip(handle);

  // Process audio
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();

  // Process through multiple loops
  for (int i = 0; i < 20; ++i) {
    m_transport->processAudio(buffers, 2, 512);
  }

  // Clip should still be playing (no fade-out at loop boundary)
  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Playing)
      << "Clip should keep playing (no fade-out at loop boundary)";
}

// Test 7: Loop mode persists across stop/start cycle
TEST_F(ClipLoopTest, LoopModePersists) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_loop.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Enable loop mode
  m_transport->setClipLoopMode(handle, true);

  // Start clip
  m_transport->startClip(handle);

  // Process audio
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();
  m_transport->processAudio(buffers, 2, 512);

  // Stop clip
  m_transport->stopClip(handle);

  // Process audio until stopped
  for (int i = 0; i < 10; ++i) {
    m_transport->processAudio(buffers, 2, 512);
    m_transport->processCallbacks();
  }

  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Stopped);

  // Verify loop mode persisted
  auto metadata = m_transport->getClipMetadata(handle);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_TRUE(metadata->loopEnabled) << "Loop mode should persist after stop";

  // Start again and verify still looping
  m_transport->startClip(handle);
  m_transport->processAudio(buffers, 2, 512);

  EXPECT_TRUE(m_transport->isClipLooping(handle)) << "Clip should still be looping after restart";
}

// Test 8: isClipLooping query returns correct state
TEST_F(ClipLoopTest, IsClipLoopingQuery) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_loop.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Initially not looping
  EXPECT_FALSE(m_transport->isClipLooping(handle)) << "Clip should not be looping (not started)";

  // Enable loop mode
  m_transport->setClipLoopMode(handle, true);

  // Start clip
  m_transport->startClip(handle);

  // Process audio
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();
  m_transport->processAudio(buffers, 2, 512);

  // Should report as looping
  EXPECT_TRUE(m_transport->isClipLooping(handle))
      << "Clip should be looping (playing + loop enabled)";

  // Disable loop mode while playing
  m_transport->setClipLoopMode(handle, false);

  // Should no longer report as looping
  EXPECT_FALSE(m_transport->isClipLooping(handle))
      << "Clip should not be looping (playing but loop disabled)";
}

// Test 9: Invalid inputs rejected
TEST_F(ClipLoopTest, InvalidInputsRejected) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_loop.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Test invalid handle (0)
  auto result = m_transport->setClipLoopMode(0, true);
  EXPECT_EQ(result, SessionGraphError::InvalidHandle) << "Handle 0 should be rejected";

  // Test unregistered clip
  auto unregisteredHandle = static_cast<ClipHandle>(999);
  result = m_transport->setClipLoopMode(unregisteredHandle, true);
  EXPECT_EQ(result, SessionGraphError::ClipNotRegistered) << "Unregistered clip should be rejected";
}

// Test 10: Multiple loops execute correctly
TEST_F(ClipLoopTest, MultipleLoopsExecuteCorrectly) {
  // Callback to count loops
  class LoopCountCallback : public ITransportCallback {
  public:
    void onClipStarted(ClipHandle /*handle*/, TransportPosition /*position*/) override {}
    void onClipStopped(ClipHandle /*handle*/, TransportPosition /*position*/) override {}
    void onClipLooped(ClipHandle /*handle*/, TransportPosition /*position*/) override {
      loopCount++;
    }
    void onBufferUnderrun(TransportPosition /*position*/) override {}

    int loopCount = 0;
  };

  auto callback = std::make_unique<LoopCountCallback>();
  m_transport->setCallback(callback.get());

  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_loop.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set very short trim for fast loops (1000 samples = ~2 buffers)
  m_transport->updateClipTrimPoints(handle, 0, 1000);

  // Enable loop mode
  m_transport->setClipLoopMode(handle, true);

  // Start clip
  m_transport->startClip(handle);

  // Process audio
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();

  // Process enough buffers to trigger multiple loops
  // 1000 samples per loop / 512 samples per buffer ≈ 2 buffers per loop
  // 20 buffers ≈ 10 loops
  for (int i = 0; i < 20; ++i) {
    m_transport->processAudio(buffers, 2, 512);
    m_transport->processCallbacks();
  }

  // Verify multiple loops occurred
  EXPECT_GE(callback->loopCount, 5) << "Should have looped at least 5 times";

  // Clip should still be playing
  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Playing);

  m_transport->setCallback(nullptr);
}

// Test 11: Concurrent loop mode changes across multiple clips
TEST_F(ClipLoopTest, ConcurrentLoopModeChanges) {
  std::vector<ClipHandle> handles = {1, 2, 3, 4};

  for (auto handle : handles) {
    auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_loop.wav");
    ASSERT_EQ(regResult, SessionGraphError::OK);
  }

  // Set different loop modes
  m_transport->setClipLoopMode(handles[0], true);
  m_transport->setClipLoopMode(handles[1], false);
  m_transport->setClipLoopMode(handles[2], true);
  m_transport->setClipLoopMode(handles[3], false);

  // Verify each clip has correct loop mode
  EXPECT_TRUE(m_transport->getClipMetadata(handles[0])->loopEnabled);
  EXPECT_FALSE(m_transport->getClipMetadata(handles[1])->loopEnabled);
  EXPECT_TRUE(m_transport->getClipMetadata(handles[2])->loopEnabled);
  EXPECT_FALSE(m_transport->getClipMetadata(handles[3])->loopEnabled);

  // Start all clips
  for (auto handle : handles) {
    m_transport->startClip(handle);
  }

  // Process audio
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();

  // Process multiple buffers
  for (int i = 0; i < 10; ++i) {
    m_transport->processAudio(buffers, 2, 512);
  }

  // Verify loop modes are independent
  EXPECT_TRUE(m_transport->isClipLooping(handles[0]));
  EXPECT_FALSE(m_transport->isClipLooping(handles[1]));
  EXPECT_TRUE(m_transport->isClipLooping(handles[2]));
  EXPECT_FALSE(m_transport->isClipLooping(handles[3]));
}
