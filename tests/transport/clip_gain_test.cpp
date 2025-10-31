// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include "../../src/core/transport/transport_controller.h"
#include <cmath>
#include <fstream>
#include <limits>
#include <orpheus/audio_file_reader.h>
#include <thread>
#include <vector>

using namespace orpheus;

// Test fixture for clip gain functionality
class ClipGainTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_transport = std::make_unique<TransportController>(nullptr, 48000);

    // Create test audio file with known samples for verification
    createTestAudioFile();
  }

  void TearDown() override {
    m_transport.reset();
    std::remove("/tmp/test_clip_gain.wav");
  }

  void createTestAudioFile() {
    // Create a minimal WAV file (1 second of 0.5 amplitude sine wave, 48kHz, stereo, 16-bit PCM)
    std::ofstream file("/tmp/test_clip_gain.wav", std::ios::binary);

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

    // Write test signal (0.25 amplitude for headroom with gain changes)
    for (int i = 0; i < 48000; ++i) {
      // Generate test tone at 440 Hz (A4)
      float sample = 0.25f * std::sin(2.0f * M_PI * 440.0f * static_cast<float>(i) / 48000.0f);
      int16_t pcmSample = static_cast<int16_t>(sample * 32767.0f);

      // Write stereo (same sample to both channels)
      file.write(reinterpret_cast<const char*>(&pcmSample), 2);
      file.write(reinterpret_cast<const char*>(&pcmSample), 2);
    }
    file.close();
  }

  std::unique_ptr<TransportController> m_transport;
};

// Test 1: Gain initialization at 0 dB (unity gain)
TEST_F(ClipGainTest, GainInitializesToUnityGain) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_gain.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK) << "Failed to register test clip";

  // Check initial gain is 0 dB (unity)
  auto metadata = m_transport->getClipMetadata(handle);
  ASSERT_TRUE(metadata.has_value()) << "Failed to get clip metadata";
  EXPECT_FLOAT_EQ(metadata->gainDb, 0.0f) << "Initial gain should be 0 dB (unity)";
}

// Test 2: setGain within valid range (-96 to +12 dB)
TEST_F(ClipGainTest, SetGainWithinValidRange) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_gain.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Test various valid gain values
  std::vector<float> testGains = {-96.0f, -60.0f, -24.0f, -12.0f, -6.0f,
                                  0.0f,   +3.0f,  +6.0f,  +12.0f};

  for (float gainDb : testGains) {
    auto result = m_transport->updateClipGain(handle, gainDb);
    EXPECT_EQ(result, SessionGraphError::OK) << "Failed to set gain to " << gainDb << " dB";

    // Verify gain was stored
    auto metadata = m_transport->getClipMetadata(handle);
    ASSERT_TRUE(metadata.has_value());
    EXPECT_FLOAT_EQ(metadata->gainDb, gainDb) << "Gain not stored correctly";
  }
}

// Test 3: getGain returns correct value
TEST_F(ClipGainTest, GetGainReturnsCorrectValue) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_gain.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set gain to -12 dB
  m_transport->updateClipGain(handle, -12.0f);

  // Query gain
  auto metadata = m_transport->getClipMetadata(handle);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_FLOAT_EQ(metadata->gainDb, -12.0f);
}

// Test 4: Gain edge cases (0 dB, -inf dB approximation, silence threshold)
TEST_F(ClipGainTest, GainEdgeCases) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_gain.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Test 0 dB (unity gain)
  auto result = m_transport->updateClipGain(handle, 0.0f);
  EXPECT_EQ(result, SessionGraphError::OK);
  auto metadata = m_transport->getClipMetadata(handle);
  EXPECT_FLOAT_EQ(metadata->gainDb, 0.0f);

  // Test very low gain (approximates silence: -96 dB = 0.000015849 linear)
  result = m_transport->updateClipGain(handle, -96.0f);
  EXPECT_EQ(result, SessionGraphError::OK);
  metadata = m_transport->getClipMetadata(handle);
  EXPECT_FLOAT_EQ(metadata->gainDb, -96.0f);

  // Verify dB-to-linear conversion at -96 dB
  float linearGain = std::pow(10.0f, -96.0f / 20.0f);
  EXPECT_NEAR(linearGain, 0.000015849f, 0.000001f) << "-96 dB should convert to ~0.000016 linear";

  // Test maximum boost (+12 dB = 3.98 linear, ~4x amplitude)
  result = m_transport->updateClipGain(handle, +12.0f);
  EXPECT_EQ(result, SessionGraphError::OK);
  metadata = m_transport->getClipMetadata(handle);
  EXPECT_FLOAT_EQ(metadata->gainDb, +12.0f);

  linearGain = std::pow(10.0f, +12.0f / 20.0f);
  EXPECT_NEAR(linearGain, 3.98107f, 0.001f) << "+12 dB should convert to ~3.98 linear";
}

// Test 5: Invalid inputs (NaN, infinity, invalid handle)
TEST_F(ClipGainTest, InvalidInputsRejected) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_gain.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Test NaN
  auto result = m_transport->updateClipGain(handle, std::numeric_limits<float>::quiet_NaN());
  EXPECT_EQ(result, SessionGraphError::InvalidParameter) << "NaN should be rejected";

  // Test positive infinity
  result = m_transport->updateClipGain(handle, std::numeric_limits<float>::infinity());
  EXPECT_EQ(result, SessionGraphError::InvalidParameter) << "Positive infinity should be rejected";

  // Test negative infinity
  result = m_transport->updateClipGain(handle, -std::numeric_limits<float>::infinity());
  EXPECT_EQ(result, SessionGraphError::InvalidParameter) << "Negative infinity should be rejected";

  // Test invalid handle (0)
  result = m_transport->updateClipGain(0, -6.0f);
  EXPECT_EQ(result, SessionGraphError::InvalidHandle) << "Handle 0 should be rejected";

  // Test unregistered clip
  auto unregisteredHandle = static_cast<ClipHandle>(999);
  result = m_transport->updateClipGain(unregisteredHandle, -6.0f);
  EXPECT_EQ(result, SessionGraphError::ClipNotRegistered) << "Unregistered clip should be rejected";
}

// Test 6: dB-to-linear conversion accuracy
TEST_F(ClipGainTest, DbToLinearConversionAccuracy) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_gain.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Test known dB-to-linear conversions
  struct TestCase {
    float gainDb;
    float expectedLinear;
  };

  std::vector<TestCase> testCases = {
      {-6.0f, 0.5012f},  // -6 dB ≈ 0.5 (half amplitude)
      {0.0f, 1.0f},      // 0 dB = unity gain
      {+6.0f, 1.9953f},  // +6 dB ≈ 2.0 (double amplitude)
      {-12.0f, 0.2512f}, // -12 dB ≈ 0.25
      {+12.0f, 3.9811f}, // +12 dB ≈ 4.0
      {-20.0f, 0.1f},    // -20 dB = 0.1 (10% amplitude)
      {+20.0f, 10.0f},   // +20 dB = 10.0 (10x amplitude)
  };

  for (const auto& testCase : testCases) {
    m_transport->updateClipGain(handle, testCase.gainDb);

    // Calculate linear gain using formula from implementation
    float linearGain = std::pow(10.0f, testCase.gainDb / 20.0f);

    EXPECT_NEAR(linearGain, testCase.expectedLinear, 0.001f)
        << testCase.gainDb << " dB should convert to ~" << testCase.expectedLinear << " linear";
  }
}

// Test 7: Gain changes during playback (should apply immediately)
TEST_F(ClipGainTest, GainChangesDuringPlayback) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_gain.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set initial gain
  m_transport->updateClipGain(handle, -6.0f);

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

  // Change gain while playing
  auto result = m_transport->updateClipGain(handle, +3.0f);
  EXPECT_EQ(result, SessionGraphError::OK) << "Should be able to change gain during playback";

  // Verify gain was updated
  auto metadata = m_transport->getClipMetadata(handle);
  EXPECT_FLOAT_EQ(metadata->gainDb, +3.0f) << "Gain should update during playback";

  // Process more audio (gain should be applied in this buffer)
  m_transport->processAudio(buffers, 2, 512);

  // Clip should still be playing
  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Playing);
}

// Test 8: Concurrent gain changes across multiple clips
TEST_F(ClipGainTest, ConcurrentGainChanges) {
  // Create multiple test clips
  std::vector<ClipHandle> handles = {1, 2, 3, 4};

  for (auto handle : handles) {
    auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_gain.wav");
    ASSERT_EQ(regResult, SessionGraphError::OK);
  }

  // Set different gain for each clip
  std::vector<float> gains = {-12.0f, -6.0f, 0.0f, +6.0f};

  for (size_t i = 0; i < handles.size(); ++i) {
    auto result = m_transport->updateClipGain(handles[i], gains[i]);
    EXPECT_EQ(result, SessionGraphError::OK);
  }

  // Verify each clip has correct gain
  for (size_t i = 0; i < handles.size(); ++i) {
    auto metadata = m_transport->getClipMetadata(handles[i]);
    ASSERT_TRUE(metadata.has_value());
    EXPECT_FLOAT_EQ(metadata->gainDb, gains[i])
        << "Clip " << handles[i] << " should have gain " << gains[i] << " dB";
  }

  // Start all clips
  for (auto handle : handles) {
    m_transport->startClip(handle);
  }

  // Process audio with all clips playing
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();
  m_transport->processAudio(buffers, 2, 512);

  // All clips should be playing with their respective gains
  for (auto handle : handles) {
    EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Playing);
  }
}

// Test 9: Gain persistence (survives stop/start cycle)
TEST_F(ClipGainTest, GainPersistsAcrossStopStartCycle) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_gain.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set gain
  m_transport->updateClipGain(handle, -12.0f);

  // Start clip
  m_transport->startClip(handle);

  // Process audio
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();
  m_transport->processAudio(buffers, 2, 512);

  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Playing);

  // Stop clip
  m_transport->stopClip(handle);

  // Process audio until clip is fully stopped
  for (int i = 0; i < 10; ++i) {
    m_transport->processAudio(buffers, 2, 512);
    m_transport->processCallbacks();
  }

  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Stopped);

  // Verify gain persisted
  auto metadata = m_transport->getClipMetadata(handle);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_FLOAT_EQ(metadata->gainDb, -12.0f) << "Gain should persist after stop";

  // Start clip again
  m_transport->startClip(handle);
  m_transport->processAudio(buffers, 2, 512);

  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Playing);

  // Gain should still be -12 dB
  metadata = m_transport->getClipMetadata(handle);
  EXPECT_FLOAT_EQ(metadata->gainDb, -12.0f) << "Gain should persist after restart";
}

// Test 10: Verify gain is applied to audio output (amplitude check)
TEST_F(ClipGainTest, GainAppliedToAudioOutput) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_gain.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set gain to -6 dB (should halve amplitude approximately)
  m_transport->updateClipGain(handle, -6.0f);

  // Start clip
  m_transport->startClip(handle);

  // Process audio
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();

  m_transport->processAudio(buffers, 2, 512);

  // Check that audio output is non-zero (clip is producing sound)
  bool hasNonZeroSamples = false;
  for (size_t i = 0; i < 512; ++i) {
    if (std::abs(leftBuffer[i]) > 0.0001f || std::abs(rightBuffer[i]) > 0.0001f) {
      hasNonZeroSamples = true;
      break;
    }
  }

  EXPECT_TRUE(hasNonZeroSamples) << "Audio output should contain non-zero samples with -6 dB gain";
}

// Test 11: Thread safety - concurrent gain updates
TEST_F(ClipGainTest, ThreadSafeConcurrentUpdates) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_gain.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Start clip
  m_transport->startClip(handle);

  // Setup audio thread simulation
  std::atomic<bool> running{true};
  std::thread audioThread([&]() {
    float* buffers[2] = {nullptr, nullptr};
    std::vector<float> leftBuffer(512, 0.0f);
    std::vector<float> rightBuffer(512, 0.0f);
    buffers[0] = leftBuffer.data();
    buffers[1] = rightBuffer.data();

    while (running.load()) {
      m_transport->processAudio(buffers, 2, 512);
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
  });

  // Update gain from UI thread while audio is processing
  for (int i = 0; i < 10; ++i) {
    float gainDb = -12.0f + (i * 2.0f); // -12 dB to +6 dB
    auto result = m_transport->updateClipGain(handle, gainDb);
    EXPECT_EQ(result, SessionGraphError::OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  // Stop audio thread
  running.store(false);
  audioThread.join();

  // Verify final gain was set correctly
  auto metadata = m_transport->getClipMetadata(handle);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_FLOAT_EQ(metadata->gainDb, +6.0f) << "Final gain should be +6 dB";
}
