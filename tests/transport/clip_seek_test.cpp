// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include "../../src/core/transport/transport_controller.h"
#include <fstream>
#include <orpheus/audio_file_reader.h>
#include <vector>

using namespace orpheus;

// Test fixture for clip seek functionality
class ClipSeekTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_transport = std::make_unique<TransportController>(nullptr, 48000);

    // Create test audio file
    createTestAudioFile();
  }

  void TearDown() override {
    m_transport.reset();
    std::remove("/tmp/test_clip_seek.wav");
  }

  void createTestAudioFile() {
    // Create a minimal WAV file (1 second of silence, 48kHz, stereo, 16-bit PCM)
    std::ofstream file("/tmp/test_clip_seek.wav", std::ios::binary);

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

TEST_F(ClipSeekTest, SeekToMiddleOfClip) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip with audio file
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_seek.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK) << "Failed to register test clip";

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

  // Seek to middle of file (24000 samples = 0.5 seconds)
  auto result = m_transport->seekClip(handle, 24000);
  EXPECT_EQ(result, SessionGraphError::OK);

  // Position should be updated (might not be exact due to audio thread timing)
  int64_t position = m_transport->getClipPosition(handle);
  EXPECT_GE(position, 23000); // Should be close to 24000
  EXPECT_LE(position, 25000);
}

TEST_F(ClipSeekTest, SeekToBeginning) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_seek.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Start clip
  m_transport->startClip(handle);

  // Process audio buffers
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();

  // Let it play for a bit
  for (int i = 0; i < 10; ++i) {
    m_transport->processAudio(buffers, 2, 512);
  }

  // Seek to beginning (position 0)
  auto result = m_transport->seekClip(handle, 0);
  EXPECT_EQ(result, SessionGraphError::OK);

  // Position should be back at start
  int64_t position = m_transport->getClipPosition(handle);
  EXPECT_LE(position, 1000); // Should be near 0
}

TEST_F(ClipSeekTest, SeekBeyondFileLength) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_seek.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Start clip
  m_transport->startClip(handle);

  // Process audio
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();
  m_transport->processAudio(buffers, 2, 512);

  // Seek beyond file length (should clamp to file duration)
  auto result = m_transport->seekClip(handle, 100000); // Beyond 48000 samples
  EXPECT_EQ(result, SessionGraphError::OK);

  // Position should be clamped to file length (48000 samples)
  int64_t position = m_transport->getClipPosition(handle);
  EXPECT_LE(position, 48000);
}

TEST_F(ClipSeekTest, SeekNegativePosition) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_seek.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Start clip
  m_transport->startClip(handle);

  // Process audio
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();
  m_transport->processAudio(buffers, 2, 512);

  // Seek to negative position (should clamp to 0)
  auto result = m_transport->seekClip(handle, -1000);
  EXPECT_EQ(result, SessionGraphError::OK);

  // Position should be clamped to 0
  int64_t position = m_transport->getClipPosition(handle);
  EXPECT_GE(position, 0);
  EXPECT_LE(position, 1000);
}

TEST_F(ClipSeekTest, SeekWhenNotPlaying) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_seek.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Try to seek when not playing
  auto result = m_transport->seekClip(handle, 24000);

  // Should return error (clip not playing)
  EXPECT_EQ(result, SessionGraphError::NotReady);
}

TEST_F(ClipSeekTest, SeekInvalidHandle) {
  auto result = m_transport->seekClip(0, 24000); // Invalid handle

  EXPECT_EQ(result, SessionGraphError::InvalidHandle);
}

TEST_F(ClipSeekTest, SeekUnregisteredClip) {
  auto handle = static_cast<ClipHandle>(999);

  auto result = m_transport->seekClip(handle, 24000);

  EXPECT_EQ(result, SessionGraphError::ClipNotRegistered);
}

// Callback test fixture
class ClipSeekCallbackTest : public ::testing::Test {
protected:
  class TestCallback : public ITransportCallback {
  public:
    void onClipStarted(ClipHandle handle, TransportPosition position) override {
      startedHandle = handle;
    }

    void onClipStopped(ClipHandle handle, TransportPosition position) override {
      stoppedHandle = handle;
    }

    void onClipLooped(ClipHandle handle, TransportPosition position) override {
      loopedHandle = handle;
    }

    void onClipSeeked(ClipHandle handle, TransportPosition position) override {
      seekedHandle = handle;
      seekedPosition = position;
      seekCount++;
    }

    void onBufferUnderrun(TransportPosition position) override {
      // Not tested here
    }

    ClipHandle startedHandle = 0;
    ClipHandle stoppedHandle = 0;
    ClipHandle loopedHandle = 0;
    ClipHandle seekedHandle = 0;
    TransportPosition seekedPosition = {0, 0.0, 0.0};
    int seekCount = 0;
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
    std::remove("/tmp/test_clip_seek_callback.wav");
  }

  void createTestAudioFile() {
    // Create a minimal WAV file
    std::ofstream file("/tmp/test_clip_seek_callback.wav", std::ios::binary);

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

TEST_F(ClipSeekCallbackTest, SeekCallbackFired) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_seek_callback.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Start clip
  m_transport->startClip(handle);

  // Process audio
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();
  m_transport->processAudio(buffers, 2, 512);
  m_transport->processCallbacks();

  EXPECT_EQ(m_callback->startedHandle, handle);

  // Seek clip
  m_transport->seekClip(handle, 24000);
  m_transport->processCallbacks();

  // Callback should have been fired
  EXPECT_EQ(m_callback->seekedHandle, handle);
  EXPECT_EQ(m_callback->seekCount, 1);
  EXPECT_EQ(m_callback->seekedPosition.samples, 24000);
}

TEST_F(ClipSeekCallbackTest, SeekRespectsOutPointEnforcement) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_seek_callback.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set trim OUT point (0.5 seconds)
  m_transport->updateClipTrimPoints(handle, 0, 24000);

  // Disable loop mode
  m_transport->setClipLoopMode(handle, false);

  // Disable fade-out for immediate stop
  m_transport->updateClipFades(handle, 0.0, 0.0, FadeCurve::Linear, FadeCurve::Linear);

  // Start clip
  m_transport->startClip(handle);

  // Process audio
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();
  m_transport->processAudio(buffers, 2, 512);

  // Seek to position past OUT point
  m_transport->seekClip(handle, 30000); // Beyond 24000 OUT point

  // Process audio buffers (OUT point enforcement should trigger)
  for (int i = 0; i < 5; ++i) {
    m_transport->processAudio(buffers, 2, 512);
  }

  m_transport->processCallbacks();

  // Clip should be stopped (OUT point enforcement)
  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Stopped);
  EXPECT_EQ(m_callback->stoppedHandle, handle);
}
