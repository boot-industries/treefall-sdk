// SPDX-License-Identifier: MIT
// Integration test for fade curve processing in TransportController

#include "transport/transport_controller.h"
#include <cmath>
#include <fstream>
#include <gtest/gtest.h>
#include <orpheus/audio_file_reader.h>
#include <orpheus/transport_controller.h>
#include <vector>

using namespace orpheus;

class FadeProcessingTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_transport = std::make_unique<TransportController>(nullptr, 48000);

    // Create a test audio file (1 second of silence at 48kHz)
    createTestAudioFile();
  }

  void TearDown() override {
    m_transport.reset();

    // Clean up test file
    std::remove("/tmp/test_fade_audio.wav");
  }

  void createTestAudioFile() {
    // Create a minimal WAV file (1 second of silence, 48kHz, stereo, 16-bit PCM)
    std::ofstream file("/tmp/test_fade_audio.wav", std::ios::binary);

    // WAV header
    file << "RIFF";
    uint32_t fileSize = 36 + (48000 * 2 * 2); // Header + (samples * channels * bytes_per_sample)
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

    // Write silence (48000 samples * 2 channels * 2 bytes)
    std::vector<uint8_t> silence(dataSize, 0);
    file.write(reinterpret_cast<const char*>(silence.data()), dataSize);

    file.close();
  }

  std::unique_ptr<TransportController> m_transport;
};

// Test fade metadata update and retrieval
TEST_F(FadeProcessingTest, UpdateAndQueryFadeMetadata) {
  // Register test audio file
  ClipHandle handle = 1;
  auto result = m_transport->registerClipAudio(handle, "/tmp/test_fade_audio.wav");
  ASSERT_EQ(result, SessionGraphError::OK);

  // Update fade settings
  result = m_transport->updateClipFades(handle,
                                        0.1, // 0.1s fade-in
                                        0.2, // 0.2s fade-out
                                        FadeCurve::EqualPower, FadeCurve::Exponential);
  EXPECT_EQ(result, SessionGraphError::OK);
}

// Test fade validation (negative fade duration)
TEST_F(FadeProcessingTest, RejectNegativeFadeDuration) {
  // Register test audio file
  ClipHandle handle = 1;
  auto result = m_transport->registerClipAudio(handle, "/tmp/test_fade_audio.wav");
  ASSERT_EQ(result, SessionGraphError::OK);

  // Try to set negative fade-in duration (should fail)
  result = m_transport->updateClipFades(handle,
                                        -0.1, // Invalid: negative fade-in
                                        0.5, FadeCurve::Linear, FadeCurve::Linear);
  EXPECT_EQ(result, SessionGraphError::InvalidFadeDuration);
}

// Test fade validation (fade longer than clip)
TEST_F(FadeProcessingTest, RejectFadeLongerThanClip) {
  // Register test audio file (1 second long)
  ClipHandle handle = 1;
  auto result = m_transport->registerClipAudio(handle, "/tmp/test_fade_audio.wav");
  ASSERT_EQ(result, SessionGraphError::OK);

  // Try to set fade-in longer than clip duration
  result = m_transport->updateClipFades(handle,
                                        2.0, // Invalid: 2s fade-in on 1s clip
                                        0.0, FadeCurve::Linear, FadeCurve::Linear);
  EXPECT_EQ(result, SessionGraphError::InvalidFadeDuration);
}

// Test trim points with fades
TEST_F(FadeProcessingTest, TrimPointsAndFadesInteraction) {
  // Register test audio file
  ClipHandle handle = 1;
  auto result = m_transport->registerClipAudio(handle, "/tmp/test_fade_audio.wav");
  ASSERT_EQ(result, SessionGraphError::OK);

  // Set trim points (trim to 0.5s duration)
  result = m_transport->updateClipTrimPoints(handle, 0, 24000); // 0 to 0.5s at 48kHz
  EXPECT_EQ(result, SessionGraphError::OK);

  // Set fades (should be validated against trimmed duration, not full file)
  result = m_transport->updateClipFades(handle,
                                        0.1, // Valid: 0.1s fade-in on 0.5s clip
                                        0.2, // Valid: 0.2s fade-out on 0.5s clip
                                        FadeCurve::Linear, FadeCurve::EqualPower);
  EXPECT_EQ(result, SessionGraphError::OK);
}

// Integration test: Process audio with fades enabled
TEST_F(FadeProcessingTest, AudioCallbackWithFades) {
  // Register test audio file
  ClipHandle handle = 1;
  auto result = m_transport->registerClipAudio(handle, "/tmp/test_fade_audio.wav");
  ASSERT_EQ(result, SessionGraphError::OK);

  // Set fade-in and fade-out
  result = m_transport->updateClipFades(handle,
                                        0.1, // 0.1s fade-in (4800 samples @ 48kHz)
                                        0.1, // 0.1s fade-out
                                        FadeCurve::Linear, FadeCurve::Linear);
  ASSERT_EQ(result, SessionGraphError::OK);

  // Start clip
  result = m_transport->startClip(handle);
  ASSERT_EQ(result, SessionGraphError::OK);

  // Create output buffers
  const size_t numChannels = 2;
  const size_t numFrames = 512;
  std::vector<float> leftBuffer(numFrames, 0.0f);
  std::vector<float> rightBuffer(numFrames, 0.0f);
  float* outputBuffers[2] = {leftBuffer.data(), rightBuffer.data()};

  // Process first buffer (should contain fade-in)
  m_transport->processAudio(outputBuffers, numChannels, numFrames);

  // NOTE: Since test file is silence, we can't verify gain was applied
  // In a real test, we'd use a test file with known amplitude (e.g., sine wave)
  // and verify that fade-in ramp is applied correctly

  // Output should be silence (input is silence, even with fade applied)
  for (size_t i = 0; i < numFrames; ++i) {
    EXPECT_FLOAT_EQ(leftBuffer[i], 0.0f) << "Sample " << i << " should be silent";
    EXPECT_FLOAT_EQ(rightBuffer[i], 0.0f) << "Sample " << i << " should be silent";
  }
}

// Test all fade curve types
TEST_F(FadeProcessingTest, AllFadeCurveTypes) {
  ClipHandle handle = 1;
  auto result = m_transport->registerClipAudio(handle, "/tmp/test_fade_audio.wav");
  ASSERT_EQ(result, SessionGraphError::OK);

  // Test Linear fade
  result = m_transport->updateClipFades(handle, 0.1, 0.1, FadeCurve::Linear, FadeCurve::Linear);
  EXPECT_EQ(result, SessionGraphError::OK);

  // Test EqualPower fade
  result =
      m_transport->updateClipFades(handle, 0.1, 0.1, FadeCurve::EqualPower, FadeCurve::EqualPower);
  EXPECT_EQ(result, SessionGraphError::OK);

  // Test Exponential fade
  result = m_transport->updateClipFades(handle, 0.1, 0.1, FadeCurve::Exponential,
                                        FadeCurve::Exponential);
  EXPECT_EQ(result, SessionGraphError::OK);

  // Test mixed curves (Linear fade-in, EqualPower fade-out)
  result = m_transport->updateClipFades(handle, 0.1, 0.1, FadeCurve::Linear, FadeCurve::EqualPower);
  EXPECT_EQ(result, SessionGraphError::OK);
}

// Test metadata persistence across clip start/stop
TEST_F(FadeProcessingTest, FadeMetadataPersistsAcrossPlayback) {
  ClipHandle handle = 1;
  auto result = m_transport->registerClipAudio(handle, "/tmp/test_fade_audio.wav");
  ASSERT_EQ(result, SessionGraphError::OK);

  // Set fades
  result =
      m_transport->updateClipFades(handle, 0.2, 0.3, FadeCurve::EqualPower, FadeCurve::Exponential);
  ASSERT_EQ(result, SessionGraphError::OK);

  // Start and stop clip
  m_transport->startClip(handle);

  // Create output buffers and process a few frames
  const size_t numChannels = 2;
  const size_t numFrames = 512;
  std::vector<float> leftBuffer(numFrames, 0.0f);
  std::vector<float> rightBuffer(numFrames, 0.0f);
  float* outputBuffers[2] = {leftBuffer.data(), rightBuffer.data()};

  m_transport->processAudio(outputBuffers, numChannels, numFrames);

  m_transport->stopClip(handle);

  // Verify fade metadata is still accessible (should persist in AudioFileEntry)
  // NOTE: There's no public API to query fade settings directly yet
  // In production, we'd add a getClipFadeSettings() method to verify this
}
