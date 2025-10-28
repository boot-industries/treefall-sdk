// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include "../../src/core/transport/transport_controller.h"
#include <fstream>
#include <orpheus/audio_file_reader.h>
#include <vector>

using namespace orpheus;

// Test fixture for clip restart functionality
class ClipRestartTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_transport = std::make_unique<TransportController>(nullptr, 48000);

    // Create test audio file
    createTestAudioFile();
  }

  void TearDown() override {
    m_transport.reset();
    std::remove("/tmp/test_clip_restart.wav");
  }

  void createTestAudioFile() {
    // Create a minimal WAV file (1 second of silence, 48kHz, stereo, 16-bit PCM)
    std::ofstream file("/tmp/test_clip_restart.wav", std::ios::binary);

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

TEST_F(ClipRestartTest, RestartNotPlayingStartsClip) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip with audio file
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_restart.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK) << "Failed to register test clip";

  // Restart when not playing should just start it
  auto result = m_transport->restartClip(handle);

  // Should succeed (starts clip if not playing)
  EXPECT_EQ(result, SessionGraphError::OK);

  // Process audio to transition to Playing state
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();
  m_transport->processAudio(buffers, 2, 512);

  // Clip should now be playing
  auto state = m_transport->getClipState(handle);
  EXPECT_EQ(state, PlaybackState::Playing);
}

TEST_F(ClipRestartTest, RestartInvalidHandle) {
  auto result = m_transport->restartClip(0); // Invalid handle

  EXPECT_EQ(result, SessionGraphError::InvalidHandle);
}

TEST_F(ClipRestartTest, RestartUnregisteredClip) {
  auto handle = static_cast<ClipHandle>(999);

  auto result = m_transport->restartClip(handle);

  EXPECT_EQ(result, SessionGraphError::ClipNotRegistered);
}

TEST_F(ClipRestartTest, RestartIsIdempotent) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip with audio file
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_restart.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK) << "Failed to register test clip";

  // Start clip
  m_transport->startClip(handle);

  // Process audio to transition to Playing state
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();
  m_transport->processAudio(buffers, 2, 512);

  // Restart multiple times should not fail
  EXPECT_EQ(m_transport->restartClip(handle), SessionGraphError::OK);
  EXPECT_EQ(m_transport->restartClip(handle), SessionGraphError::OK);
  EXPECT_EQ(m_transport->restartClip(handle), SessionGraphError::OK);

  // Clip should still be playing
  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Playing);
}

// Callback test fixture
class ClipRestartCallbackTest : public ::testing::Test {
protected:
  class TestCallback : public ITransportCallback {
  public:
    void onClipStarted(ClipHandle handle, TransportPosition position) override {
      startedHandle = handle;
      startedPosition = position;
    }

    void onClipStopped(ClipHandle handle, TransportPosition position) override {
      stoppedHandle = handle;
      stoppedPosition = position;
    }

    void onClipLooped(ClipHandle handle, TransportPosition position) override {
      loopedHandle = handle;
      loopedPosition = position;
    }

    void onClipRestarted(ClipHandle handle, TransportPosition position) override {
      restartedHandle = handle;
      restartedPosition = position;
      restartCount++;
    }

    void onBufferUnderrun(TransportPosition position) override {
      // Not tested here
    }

    ClipHandle startedHandle = 0;
    TransportPosition startedPosition = {0, 0.0, 0.0};
    ClipHandle stoppedHandle = 0;
    TransportPosition stoppedPosition = {0, 0.0, 0.0};
    ClipHandle loopedHandle = 0;
    TransportPosition loopedPosition = {0, 0.0, 0.0};
    ClipHandle restartedHandle = 0;
    TransportPosition restartedPosition = {0, 0.0, 0.0};
    int restartCount = 0;
  };

  void SetUp() override {
    m_transport = std::make_unique<TransportController>(nullptr, 48000);
    m_callback = std::make_unique<TestCallback>();
    m_transport->setCallback(m_callback.get());

    // Create test audio file
    createTestAudioFile();
  }

  void TearDown() override {
    m_transport->setCallback(nullptr);
    m_callback.reset();
    m_transport.reset();
    std::remove("/tmp/test_clip_restart_callback.wav");
  }

  void createTestAudioFile() {
    // Create a minimal WAV file (1 second of silence, 48kHz, stereo, 16-bit PCM)
    std::ofstream file("/tmp/test_clip_restart_callback.wav", std::ios::binary);

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
  std::unique_ptr<TestCallback> m_callback;
};

TEST_F(ClipRestartCallbackTest, RestartCallbackFired) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip with audio file
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_restart_callback.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK) << "Failed to register test clip";

  // Start clip
  m_transport->startClip(handle);

  // Process audio to trigger start callback
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();
  m_transport->processAudio(buffers, 2, 512);
  m_transport->processCallbacks(); // Process start callback

  EXPECT_EQ(m_callback->startedHandle, handle);

  // Restart clip
  m_transport->restartClip(handle);
  m_transport->processCallbacks(); // Process restart callback

  EXPECT_EQ(m_callback->restartedHandle, handle);
  EXPECT_EQ(m_callback->restartCount, 1);
  EXPECT_EQ(m_callback->restartedPosition.samples, 0); // Should be at trim IN (0)
}

TEST_F(ClipRestartCallbackTest, RestartCallbackNotFiredForStart) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip with audio file
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_restart_callback.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK) << "Failed to register test clip";

  // Restart when not playing (should just start)
  m_transport->restartClip(handle);

  // Process audio to trigger start callback
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();
  m_transport->processAudio(buffers, 2, 512);
  m_transport->processCallbacks();

  // Should fire start callback, not restart callback
  EXPECT_EQ(m_callback->startedHandle, handle);
  EXPECT_EQ(m_callback->restartedHandle, 0); // No restart callback
  EXPECT_EQ(m_callback->restartCount, 0);
}
