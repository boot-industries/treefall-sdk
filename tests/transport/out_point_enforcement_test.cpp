// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include "../../src/core/transport/transport_controller.h"
#include <fstream>
#include <orpheus/audio_file_reader.h>
#include <vector>

using namespace orpheus;

// Test fixture for OUT point enforcement
class OutPointEnforcementTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_transport = std::make_unique<TransportController>(nullptr, 48000);

    // Create test audio file
    createTestAudioFile();
  }

  void TearDown() override {
    m_transport.reset();
    std::remove("/tmp/test_out_point.wav");
  }

  void createTestAudioFile() {
    // Create a minimal WAV file (1 second of silence, 48kHz, stereo, 16-bit PCM)
    std::ofstream file("/tmp/test_out_point.wav", std::ios::binary);

    // WAV header (RIFF)
    file << "RIFF";
    uint32_t fileSize = 36 + (48000 * 2 * 2);
    file.write(reinterpret_cast<const char*>(&fileSize), 4);
    file << "WAVE";

    // fmt chunk
    file << "fmt ";
    uint32_t fmtSize = 16;
    file.write(reinterpret_cast<const char*>(&fmtSize), 4);
    uint16_t audioFormat = 1; // PCM
    file.write(reinterpret_cast<const char*>(&audioFormat), 2);
    uint16_t numChannels = 2;
    file.write(reinterpret_cast<const char*>(&numChannels), 2);
    uint32_t sampleRate = 48000;
    file.write(reinterpret_cast<const char*>(&sampleRate), 4);
    uint32_t byteRate = 48000 * 2 * 2;
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    uint16_t blockAlign = 4;
    file.write(reinterpret_cast<const char*>(&blockAlign), 2);
    uint16_t bitsPerSample = 16;
    file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    // data chunk
    file << "data";
    uint32_t dataSize = 48000 * 2 * 2;
    file.write(reinterpret_cast<const char*>(&dataSize), 4);

    // Write silence
    std::vector<uint8_t> silence(dataSize, 0);
    file.write(reinterpret_cast<const char*>(silence.data()), dataSize);
    file.close();
  }

  std::unique_ptr<TransportController> m_transport;
};

TEST_F(OutPointEnforcementTest, StopsAtOutPointWhenLoopDisabled) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip with audio file
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_out_point.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK) << "Failed to register test clip";

  // Set trim points: 0.5 second clip (24000 samples)
  m_transport->updateClipTrimPoints(handle, 0, 24000);

  // Disable loop mode
  m_transport->setClipLoopMode(handle, false);

  // Disable fade-out for immediate stop
  m_transport->updateClipFades(handle, 0.0, 0.0, FadeCurve::Linear, FadeCurve::Linear);

  // Start clip
  m_transport->startClip(handle);

  // Process audio to start playback
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();
  m_transport->processAudio(buffers, 2, 512);

  // Verify clip is playing
  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Playing);

  // Process enough buffers to reach OUT point (24000 samples / 512 = 47 buffers)
  for (int i = 0; i < 50; ++i) {
    m_transport->processAudio(buffers, 2, 512);
  }

  // Clip should be stopped (OUT point enforcement triggered)
  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Stopped);
}

TEST_F(OutPointEnforcementTest, OutPointWithFadeOut) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_out_point.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set trim points: 0.5 second clip (24000 samples)
  m_transport->updateClipTrimPoints(handle, 0, 24000);

  // Disable loop mode
  m_transport->setClipLoopMode(handle, false);

  // Set 100ms fade-out (4800 samples @ 48kHz)
  m_transport->updateClipFades(handle, 0.0, 0.1, FadeCurve::Linear, FadeCurve::Linear);

  // Start clip
  m_transport->startClip(handle);

  // Process audio buffers
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();

  // Process to just before OUT point
  for (int i = 0; i < 47; ++i) {
    m_transport->processAudio(buffers, 2, 512);
  }

  // Clip should still be playing (just reached OUT point, fade-out starting)
  auto state = m_transport->getClipState(handle);
  EXPECT_TRUE(state == PlaybackState::Playing || state == PlaybackState::Stopping);

  // Process fade-out buffers (4800 samples / 512 = ~10 buffers)
  // Need to process enough buffers for fade-out to complete
  for (int i = 0; i < 20; ++i) {
    m_transport->processAudio(buffers, 2, 512);
  }

  // Clip should now be stopped (fade-out complete)
  auto finalState = m_transport->getClipState(handle);
  EXPECT_TRUE(finalState == PlaybackState::Stopped || finalState == PlaybackState::Stopping)
      << "Expected Stopped or Stopping, got " << static_cast<int>(finalState);
}

TEST_F(OutPointEnforcementTest, OutPointWithZeroLengthFade) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_out_point.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set trim points
  m_transport->updateClipTrimPoints(handle, 0, 24000);

  // Disable loop mode
  m_transport->setClipLoopMode(handle, false);

  // Set 0-length fade (immediate stop)
  m_transport->updateClipFades(handle, 0.0, 0.0, FadeCurve::Linear, FadeCurve::Linear);

  // Start clip
  m_transport->startClip(handle);

  // Process audio buffers
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();

  // Process to OUT point
  for (int i = 0; i < 50; ++i) {
    m_transport->processAudio(buffers, 2, 512);
  }

  // Clip should be stopped immediately (no fade-out)
  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Stopped);
}

TEST_F(OutPointEnforcementTest, InvalidHandleReturnsError) {
  auto result = m_transport->setClipLoopMode(0, false);
  EXPECT_EQ(result, SessionGraphError::InvalidHandle);
}

// Callback test fixture
class OutPointCallbackTest : public ::testing::Test {
protected:
  class TestCallback : public ITransportCallback {
  public:
    void onClipStarted(ClipHandle handle, TransportPosition position) override {
      startedHandle = handle;
      startedCount++;
    }

    void onClipStopped(ClipHandle handle, TransportPosition position) override {
      stoppedHandle = handle;
      stoppedPosition = position;
      stoppedCount++;
    }

    void onClipLooped(ClipHandle handle, TransportPosition position) override {
      loopedHandle = handle;
      loopedCount++;
    }

    void onBufferUnderrun(TransportPosition position) override {
      // Not tested here
    }

    ClipHandle startedHandle = 0;
    ClipHandle stoppedHandle = 0;
    ClipHandle loopedHandle = 0;
    TransportPosition stoppedPosition = {0, 0.0, 0.0};
    int startedCount = 0;
    int stoppedCount = 0;
    int loopedCount = 0;
  };

  void SetUp() override {
    m_transport = std::make_unique<TransportController>(nullptr, 48000);
    m_callback = std::make_unique<TestCallback>();
    m_transport->setCallback(m_callback.get());

    createTestAudioFile();
  }

  void TearDown() override {
    m_transport->setCallback(nullptr);
    m_callback.reset();
    m_transport.reset();
    std::remove("/tmp/test_out_point_callback.wav");
  }

  void createTestAudioFile() {
    // Create a minimal WAV file
    std::ofstream file("/tmp/test_out_point_callback.wav", std::ios::binary);

    file << "RIFF";
    uint32_t fileSize = 36 + (48000 * 2 * 2);
    file.write(reinterpret_cast<const char*>(&fileSize), 4);
    file << "WAVE";

    file << "fmt ";
    uint32_t fmtSize = 16;
    file.write(reinterpret_cast<const char*>(&fmtSize), 4);
    uint16_t audioFormat = 1;
    file.write(reinterpret_cast<const char*>(&audioFormat), 2);
    uint16_t numChannels = 2;
    file.write(reinterpret_cast<const char*>(&numChannels), 2);
    uint32_t sampleRate = 48000;
    file.write(reinterpret_cast<const char*>(&sampleRate), 4);
    uint32_t byteRate = 48000 * 2 * 2;
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    uint16_t blockAlign = 4;
    file.write(reinterpret_cast<const char*>(&blockAlign), 2);
    uint16_t bitsPerSample = 16;
    file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    file << "data";
    uint32_t dataSize = 48000 * 2 * 2;
    file.write(reinterpret_cast<const char*>(&dataSize), 4);

    std::vector<uint8_t> silence(dataSize, 0);
    file.write(reinterpret_cast<const char*>(silence.data()), dataSize);
    file.close();
  }

  std::unique_ptr<TransportController> m_transport;
  std::unique_ptr<TestCallback> m_callback;
};

TEST_F(OutPointCallbackTest, CallbackFiredOnOutPoint) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_out_point_callback.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set trim points
  m_transport->updateClipTrimPoints(handle, 0, 24000);
  m_transport->setClipLoopMode(handle, false);
  m_transport->updateClipFades(handle, 0.0, 0.0, FadeCurve::Linear, FadeCurve::Linear);

  // Start clip
  m_transport->startClip(handle);

  // Process audio buffers
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();

  // Process first buffer to trigger start callback
  m_transport->processAudio(buffers, 2, 512);
  m_transport->processCallbacks();

  EXPECT_EQ(m_callback->startedCount, 1);
  EXPECT_EQ(m_callback->startedHandle, handle);

  // Process to OUT point
  for (int i = 0; i < 50; ++i) {
    m_transport->processAudio(buffers, 2, 512);
  }

  // Process callbacks
  m_transport->processCallbacks();

  // Stopped callback should have been fired
  EXPECT_EQ(m_callback->stoppedCount, 1);
  EXPECT_EQ(m_callback->stoppedHandle, handle);
}

TEST_F(OutPointCallbackTest, MultipleClipsDifferentOutPoints) {
  auto handle1 = static_cast<ClipHandle>(1);
  auto handle2 = static_cast<ClipHandle>(2);

  // Register both clips
  m_transport->registerClipAudio(handle1, "/tmp/test_out_point_callback.wav");
  m_transport->registerClipAudio(handle2, "/tmp/test_out_point_callback.wav");

  // Set different trim points
  m_transport->updateClipTrimPoints(handle1, 0, 24000); // 0.5 sec
  m_transport->updateClipTrimPoints(handle2, 0, 48000); // 1.0 sec

  // Disable loop for both
  m_transport->setClipLoopMode(handle1, false);
  m_transport->setClipLoopMode(handle2, false);

  // No fade-out
  m_transport->updateClipFades(handle1, 0.0, 0.0, FadeCurve::Linear, FadeCurve::Linear);
  m_transport->updateClipFades(handle2, 0.0, 0.0, FadeCurve::Linear, FadeCurve::Linear);

  // Start both clips
  m_transport->startClip(handle1);
  m_transport->startClip(handle2);

  // Process audio buffers
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();

  // Process to first OUT point (24000 samples / 512 = 47 buffers)
  for (int i = 0; i < 50; ++i) {
    m_transport->processAudio(buffers, 2, 512);
  }

  // Clip 1 should be stopped, clip 2 still playing
  EXPECT_EQ(m_transport->getClipState(handle1), PlaybackState::Stopped);
  EXPECT_EQ(m_transport->getClipState(handle2), PlaybackState::Playing);

  // Process to second OUT point (another 47 buffers)
  for (int i = 0; i < 50; ++i) {
    m_transport->processAudio(buffers, 2, 512);
  }

  // Both clips should be stopped
  EXPECT_EQ(m_transport->getClipState(handle1), PlaybackState::Stopped);
  EXPECT_EQ(m_transport->getClipState(handle2), PlaybackState::Stopped);
}

// Loop mode OUT point tests
TEST_F(OutPointEnforcementTest, LoopModeRestartsAtInPoint) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip with audio file
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_out_point.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK) << "Failed to register test clip";

  // Set trim points: 0.5 second clip (24000 samples)
  m_transport->updateClipTrimPoints(handle, 0, 24000);

  // ENABLE loop mode
  m_transport->setClipLoopMode(handle, true);

  // Disable fade for clear position tracking
  m_transport->updateClipFades(handle, 0.0, 0.0, FadeCurve::Linear, FadeCurve::Linear);

  // Start clip
  m_transport->startClip(handle);

  // Process audio to start playback
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();
  m_transport->processAudio(buffers, 2, 512);

  // Verify clip is playing
  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Playing);

  // Process enough buffers to reach OUT point (24000 samples / 512 = 47 buffers)
  for (int i = 0; i < 50; ++i) {
    m_transport->processAudio(buffers, 2, 512);
  }

  // Clip should STILL be playing (loop mode, not stopped)
  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Playing);

  // Position should have looped back to near IN point (0)
  int64_t position = m_transport->getClipPosition(handle);
  EXPECT_GE(position, 0);
  EXPECT_LT(position, 5000); // Should be near beginning after loop
}

TEST_F(OutPointEnforcementTest, LoopModeWithNonZeroInPoint) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_out_point.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set trim points: Start at 0.1 sec (4800 samples), end at 0.5 sec (24000 samples)
  int64_t trimIn = 4800;
  int64_t trimOut = 24000;
  m_transport->updateClipTrimPoints(handle, trimIn, trimOut);

  // Enable loop mode
  m_transport->setClipLoopMode(handle, true);

  // Disable fade
  m_transport->updateClipFades(handle, 0.0, 0.0, FadeCurve::Linear, FadeCurve::Linear);

  // Start clip
  m_transport->startClip(handle);

  // Process audio buffers
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();

  // Process enough to reach OUT point
  // Duration = trimOut - trimIn = 19200 samples / 512 = 38 buffers
  for (int i = 0; i < 50; ++i) {
    m_transport->processAudio(buffers, 2, 512);
  }

  // Clip should still be playing
  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Playing);

  // Position should have looped back to trimIn (4800), not 0
  int64_t position = m_transport->getClipPosition(handle);
  EXPECT_GE(position, trimIn);
  EXPECT_LT(position, trimIn + 10000); // Should be within ~10K samples of trimIn after loop (allow
                                       // for buffer granularity)
}

TEST_F(OutPointCallbackTest, LoopCallbackFired) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_out_point_callback.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set trim points
  m_transport->updateClipTrimPoints(handle, 0, 24000);

  // Enable loop mode
  m_transport->setClipLoopMode(handle, true);

  // Disable fade
  m_transport->updateClipFades(handle, 0.0, 0.0, FadeCurve::Linear, FadeCurve::Linear);

  // Start clip
  m_transport->startClip(handle);

  // Process audio buffers
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();

  // Process first buffer to trigger start callback
  m_transport->processAudio(buffers, 2, 512);
  m_transport->processCallbacks();

  EXPECT_EQ(m_callback->startedCount, 1);
  EXPECT_EQ(m_callback->startedHandle, handle);

  // Process to OUT point (should trigger loop)
  for (int i = 0; i < 50; ++i) {
    m_transport->processAudio(buffers, 2, 512);
  }

  // Process callbacks
  m_transport->processCallbacks();

  // Loop callback should have been fired (NOT stopped callback)
  EXPECT_GE(m_callback->loopedCount, 1); // At least 1 loop
  EXPECT_EQ(m_callback->loopedHandle, handle);
  EXPECT_EQ(m_callback->stoppedCount, 0); // Should NOT stop in loop mode
}

// ORP091 Regression Test: Non-loop clips must never have position below IN point
TEST_F(OutPointEnforcementTest, NonLoopNeverGoesbelowInPoint) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_out_point.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set trim points with non-zero IN point (matches ORP091 log: IN=444836)
  int64_t trimIn = 10000;  // Start at 10000 samples
  int64_t trimOut = 24000; // End at 24000 samples (14000 sample duration)
  m_transport->updateClipTrimPoints(handle, trimIn, trimOut);

  // DISABLE loop mode (critical: non-loop behavior)
  m_transport->setClipLoopMode(handle, false);

  // Set fade-out to verify position during fade-out period
  m_transport->updateClipFades(handle, 0.0, 0.1, FadeCurve::Linear, FadeCurve::Linear);

  // Start clip
  m_transport->startClip(handle);

  // Process audio buffers
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();

  // Process to OUT point and beyond (enough to complete fade-out)
  // 14000 samples / 512 = ~28 buffers to reach OUT
  // + 4800 samples fade-out / 512 = ~10 buffers
  // Total: 50 buffers to be safe
  for (int i = 0; i < 50; ++i) {
    m_transport->processAudio(buffers, 2, 512);

    // CRITICAL TEST: Position must NEVER go below trimIn
    // This is Edit Law #1: "IN ≤ Playhead"
    // Bug ORP091: Position was dropping to 0 (violating this law)
    int64_t position = m_transport->getClipPosition(handle);
    if (position >= 0) { // -1 means stopped (valid)
      EXPECT_GE(position, trimIn) << "Clip position " << position << " dropped below trimIn "
                                  << trimIn << " at buffer " << i
                                  << " (ORP091 regression - illegal loop to zero)";
    }
  }

  // Final state: clip should be stopped or stopping (fade-out may still be active)
  auto finalState = m_transport->getClipState(handle);
  EXPECT_TRUE(finalState == PlaybackState::Stopped || finalState == PlaybackState::Stopping)
      << "Clip should be stopped or stopping after reaching OUT point in non-loop mode, got "
      << static_cast<int>(finalState);
}

// ORP093 Regression Test #1: Position never escapes OUT point during playback
TEST_F(OutPointEnforcementTest, PositionNeverEscapesOutDuringPlayback) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_out_point.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set trim points: 0.5 second clip (24000 samples)
  int64_t trimIn = 0;
  int64_t trimOut = 24000;
  m_transport->updateClipTrimPoints(handle, trimIn, trimOut);

  // Disable loop mode
  m_transport->setClipLoopMode(handle, false);

  // No fade-out for immediate position tracking
  m_transport->updateClipFades(handle, 0.0, 0.0, FadeCurve::Linear, FadeCurve::Linear);

  // Start clip
  m_transport->startClip(handle);

  // Process audio buffers
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();

  // Process to OUT point and beyond
  for (int i = 0; i < 60; ++i) {
    m_transport->processAudio(buffers, 2, 512);

    // CRITICAL TEST: Position must NEVER exceed trimOut
    // This is Edit Law #2: "Playhead < OUT"
    // Bug ORP093: Position was exceeding OUT (e.g., 52000 when OUT=50000)
    int64_t position = m_transport->getClipPosition(handle);
    if (position >= 0) { // -1 means stopped (valid)
      EXPECT_LT(position, trimOut)
          << "Clip position " << position << " exceeded trimOut " << trimOut << " at buffer " << i
          << " (ORP093 regression - position escape bug)";
    }
  }

  // Clip should be stopped
  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Stopped);
}

// ORP093 Regression Test #2: Position never escapes below IN point
TEST_F(OutPointEnforcementTest, PositionNeverEscapesBelowInPoint) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_out_point.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set trim points with non-zero IN point
  int64_t trimIn = 10000;
  int64_t trimOut = 30000;
  m_transport->updateClipTrimPoints(handle, trimIn, trimOut);

  // Enable loop mode (tests loop restart boundary enforcement)
  m_transport->setClipLoopMode(handle, true);

  // No fade for clear position tracking
  m_transport->updateClipFades(handle, 0.0, 0.0, FadeCurve::Linear, FadeCurve::Linear);

  // Start clip
  m_transport->startClip(handle);

  // Process audio buffers
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();

  // Process multiple loops (enough to reach OUT and loop several times)
  for (int i = 0; i < 100; ++i) {
    m_transport->processAudio(buffers, 2, 512);

    // CRITICAL TEST: Position must NEVER go below trimIn
    // This is Edit Law #1: "IN ≤ Playhead"
    // ORP093: Ensures position clamping before rendering
    int64_t position = m_transport->getClipPosition(handle);
    if (position >= 0) { // -1 means stopped (valid)
      EXPECT_GE(position, trimIn) << "Clip position " << position << " dropped below trimIn "
                                  << trimIn << " at buffer " << i
                                  << " (ORP093 regression - IN point escape)";

      // Also verify position doesn't exceed OUT
      EXPECT_LT(position, trimOut)
          << "Clip position " << position << " exceeded trimOut " << trimOut << " at buffer " << i
          << " (ORP093 regression - OUT point escape)";
    }
  }

  // Clip should still be playing (loop mode)
  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Playing);
}

// ORP093 Regression Test #3: Metadata update clamps position to new boundaries
TEST_F(OutPointEnforcementTest, MetadataUpdateClampsPosition) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_out_point.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set initial trim points: large range
  int64_t initialTrimIn = 0;
  int64_t initialTrimOut = 48000;
  m_transport->updateClipTrimPoints(handle, initialTrimIn, initialTrimOut);

  // Disable loop mode
  m_transport->setClipLoopMode(handle, false);

  // No fade
  m_transport->updateClipFades(handle, 0.0, 0.0, FadeCurve::Linear, FadeCurve::Linear);

  // Start clip
  m_transport->startClip(handle);

  // Process audio buffers
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();

  // Process to mid-point (around 15000 samples)
  for (int i = 0; i < 30; ++i) {
    m_transport->processAudio(buffers, 2, 512);
  }

  // Verify position is somewhere in the middle
  int64_t positionBeforeUpdate = m_transport->getClipPosition(handle);
  EXPECT_GT(positionBeforeUpdate, 10000);
  EXPECT_LT(positionBeforeUpdate, 20000);

  // Now shrink trim range to be BEFORE current position
  // This simulates user moving OUT point to left of playhead
  int64_t newTrimIn = 0;
  int64_t newTrimOut = 5000; // OUT is now BEFORE current position!
  m_transport->updateClipTrimPoints(handle, newTrimIn, newTrimOut);

  // Process one more buffer (ORP093 fix should clamp position)
  m_transport->processAudio(buffers, 2, 512);

  // CRITICAL TEST: Position must be clamped to new OUT point
  // ORP093 fix: Position clamping happens BEFORE rendering
  int64_t positionAfterUpdate = m_transport->getClipPosition(handle);
  if (positionAfterUpdate >= 0) { // -1 means stopped (valid behavior)
    EXPECT_LT(positionAfterUpdate, newTrimOut)
        << "Clip position " << positionAfterUpdate << " exceeded new trimOut " << newTrimOut
        << " after metadata update (ORP093 regression - metadata update doesn't clamp position)";
  }

  // Clip should be stopped or stopping (position exceeded new OUT point)
  auto finalState = m_transport->getClipState(handle);
  EXPECT_TRUE(finalState == PlaybackState::Stopped || finalState == PlaybackState::Stopping)
      << "Clip should stop when metadata update moves OUT point before playhead, got "
      << static_cast<int>(finalState);
}
