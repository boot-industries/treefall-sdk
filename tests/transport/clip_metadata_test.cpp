// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include "../../src/core/transport/transport_controller.h"
#include <fstream>
#include <orpheus/audio_file_reader.h>
#include <vector>

using namespace orpheus;

// Test fixture for persistent clip metadata functionality
class ClipMetadataTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_transport = std::make_unique<TransportController>(nullptr, 48000);

    // Create test audio file
    createTestAudioFile();
  }

  void TearDown() override {
    m_transport.reset();
    std::remove("/tmp/test_clip_metadata.wav");
  }

  void createTestAudioFile() {
    // Create a WAV file (1 second, 48kHz, stereo, 16-bit PCM)
    std::ofstream file("/tmp/test_clip_metadata.wav", std::ios::binary);

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

// Test 1: Metadata survives stopClip() → startClip() cycle
TEST_F(ClipMetadataTest, MetadataSurvivesStopStartCycle) {
  auto handle = static_cast<ClipHandle>(1);

  // Register clip
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_metadata.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set comprehensive metadata
  ClipMetadata metadata;
  metadata.trimInSamples = 1000;
  metadata.trimOutSamples = 40000;
  metadata.fadeInSeconds = 0.05;
  metadata.fadeOutSeconds = 0.1;
  metadata.fadeInCurve = FadeCurve::EqualPower;
  metadata.fadeOutCurve = FadeCurve::Exponential;
  metadata.gainDb = -6.0f;
  metadata.loopEnabled = true;
  metadata.stopOthersOnPlay = true;

  m_transport->updateClipMetadata(handle, metadata);

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

  // Process until stopped
  for (int i = 0; i < 10; ++i) {
    m_transport->processAudio(buffers, 2, 512);
    m_transport->processCallbacks();
  }

  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Stopped);

  // Verify metadata persisted
  auto retrieved = m_transport->getClipMetadata(handle);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->trimInSamples, 1000);
  EXPECT_EQ(retrieved->trimOutSamples, 40000);
  EXPECT_DOUBLE_EQ(retrieved->fadeInSeconds, 0.05);
  EXPECT_DOUBLE_EQ(retrieved->fadeOutSeconds, 0.1);
  EXPECT_EQ(retrieved->fadeInCurve, FadeCurve::EqualPower);
  EXPECT_EQ(retrieved->fadeOutCurve, FadeCurve::Exponential);
  EXPECT_FLOAT_EQ(retrieved->gainDb, -6.0f);
  EXPECT_TRUE(retrieved->loopEnabled);
  EXPECT_TRUE(retrieved->stopOthersOnPlay);

  // Start again
  m_transport->startClip(handle);
  m_transport->processAudio(buffers, 2, 512);

  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Playing);

  // Metadata should still match
  retrieved = m_transport->getClipMetadata(handle);
  EXPECT_EQ(retrieved->trimInSamples, 1000);
  EXPECT_FLOAT_EQ(retrieved->gainDb, -6.0f);
  EXPECT_TRUE(retrieved->loopEnabled);
}

// Test 2: Trim points persist
TEST_F(ClipMetadataTest, TrimPointsPersist) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_metadata.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set trim points
  int64_t trimIn = 5000;
  int64_t trimOut = 30000;
  m_transport->updateClipTrimPoints(handle, trimIn, trimOut);

  // Query trim points
  int64_t retrievedIn = 0;
  int64_t retrievedOut = 0;
  auto result = m_transport->getClipTrimPoints(handle, retrievedIn, retrievedOut);

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_EQ(retrievedIn, trimIn);
  EXPECT_EQ(retrievedOut, trimOut);

  // Start and stop clip
  m_transport->startClip(handle);
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();
  m_transport->processAudio(buffers, 2, 512);

  m_transport->stopClip(handle);
  for (int i = 0; i < 10; ++i) {
    m_transport->processAudio(buffers, 2, 512);
    m_transport->processCallbacks();
  }

  // Trim points should still be there
  result = m_transport->getClipTrimPoints(handle, retrievedIn, retrievedOut);
  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_EQ(retrievedIn, trimIn);
  EXPECT_EQ(retrievedOut, trimOut);
}

// Test 3: Fade curves persist (IN/OUT seconds + curve type)
TEST_F(ClipMetadataTest, FadeCurvesPersist) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_metadata.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set fade curves
  m_transport->updateClipFades(handle, 0.02, 0.05, FadeCurve::Exponential, FadeCurve::EqualPower);

  // Query metadata
  auto metadata = m_transport->getClipMetadata(handle);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_DOUBLE_EQ(metadata->fadeInSeconds, 0.02);
  EXPECT_DOUBLE_EQ(metadata->fadeOutSeconds, 0.05);
  EXPECT_EQ(metadata->fadeInCurve, FadeCurve::Exponential);
  EXPECT_EQ(metadata->fadeOutCurve, FadeCurve::EqualPower);

  // Cycle through stop/start
  m_transport->startClip(handle);
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();
  m_transport->processAudio(buffers, 2, 512);

  m_transport->stopClip(handle);
  for (int i = 0; i < 10; ++i) {
    m_transport->processAudio(buffers, 2, 512);
    m_transport->processCallbacks();
  }

  // Fade curves should persist
  metadata = m_transport->getClipMetadata(handle);
  EXPECT_DOUBLE_EQ(metadata->fadeInSeconds, 0.02);
  EXPECT_DOUBLE_EQ(metadata->fadeOutSeconds, 0.05);
  EXPECT_EQ(metadata->fadeInCurve, FadeCurve::Exponential);
  EXPECT_EQ(metadata->fadeOutCurve, FadeCurve::EqualPower);
}

// Test 4: Gain persists (dB value)
TEST_F(ClipMetadataTest, GainPersists) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_metadata.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set gain
  float gainDb = -12.0f;
  m_transport->updateClipGain(handle, gainDb);

  // Verify gain is stored
  auto metadata = m_transport->getClipMetadata(handle);
  EXPECT_FLOAT_EQ(metadata->gainDb, gainDb);

  // Cycle through multiple stop/start cycles
  for (int cycle = 0; cycle < 3; ++cycle) {
    m_transport->startClip(handle);

    float* buffers[2] = {nullptr, nullptr};
    std::vector<float> leftBuffer(512, 0.0f);
    std::vector<float> rightBuffer(512, 0.0f);
    buffers[0] = leftBuffer.data();
    buffers[1] = rightBuffer.data();
    m_transport->processAudio(buffers, 2, 512);

    m_transport->stopClip(handle);
    for (int i = 0; i < 10; ++i) {
      m_transport->processAudio(buffers, 2, 512);
      m_transport->processCallbacks();
    }

    // Gain should persist across all cycles
    metadata = m_transport->getClipMetadata(handle);
    EXPECT_FLOAT_EQ(metadata->gainDb, gainDb) << "Gain should persist in cycle " << cycle;
  }
}

// Test 5: Loop mode persists (bool flag)
TEST_F(ClipMetadataTest, LoopModePersists) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_metadata.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Enable loop mode
  m_transport->setClipLoopMode(handle, true);

  // Verify loop is enabled
  auto metadata = m_transport->getClipMetadata(handle);
  EXPECT_TRUE(metadata->loopEnabled);

  // Stop/start cycle
  m_transport->startClip(handle);
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();
  m_transport->processAudio(buffers, 2, 512);

  m_transport->stopClip(handle);
  for (int i = 0; i < 10; ++i) {
    m_transport->processAudio(buffers, 2, 512);
    m_transport->processCallbacks();
  }

  // Loop mode should persist
  metadata = m_transport->getClipMetadata(handle);
  EXPECT_TRUE(metadata->loopEnabled);

  // Disable loop mode
  m_transport->setClipLoopMode(handle, false);

  // Stop/start cycle again
  m_transport->startClip(handle);
  m_transport->processAudio(buffers, 2, 512);
  m_transport->stopClip(handle);
  for (int i = 0; i < 10; ++i) {
    m_transport->processAudio(buffers, 2, 512);
    m_transport->processCallbacks();
  }

  // Loop mode should still be disabled
  metadata = m_transport->getClipMetadata(handle);
  EXPECT_FALSE(metadata->loopEnabled);
}

// Test 6: Metadata for multiple clips (no cross-contamination)
TEST_F(ClipMetadataTest, MultipleClipsNoContamination) {
  std::vector<ClipHandle> handles = {1, 2, 3, 4};

  // Register all clips
  for (auto handle : handles) {
    auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_metadata.wav");
    ASSERT_EQ(regResult, SessionGraphError::OK);
  }

  // Set unique metadata for each clip
  m_transport->updateClipGain(handles[0], -12.0f);
  m_transport->updateClipGain(handles[1], -6.0f);
  m_transport->updateClipGain(handles[2], 0.0f);
  m_transport->updateClipGain(handles[3], +6.0f);

  m_transport->setClipLoopMode(handles[0], true);
  m_transport->setClipLoopMode(handles[1], false);
  m_transport->setClipLoopMode(handles[2], true);
  m_transport->setClipLoopMode(handles[3], false);

  m_transport->updateClipTrimPoints(handles[0], 0, 10000);
  m_transport->updateClipTrimPoints(handles[1], 1000, 20000);
  m_transport->updateClipTrimPoints(handles[2], 2000, 30000);
  m_transport->updateClipTrimPoints(handles[3], 3000, 40000);

  // Verify each clip has unique metadata
  auto meta0 = m_transport->getClipMetadata(handles[0]);
  auto meta1 = m_transport->getClipMetadata(handles[1]);
  auto meta2 = m_transport->getClipMetadata(handles[2]);
  auto meta3 = m_transport->getClipMetadata(handles[3]);

  EXPECT_FLOAT_EQ(meta0->gainDb, -12.0f);
  EXPECT_FLOAT_EQ(meta1->gainDb, -6.0f);
  EXPECT_FLOAT_EQ(meta2->gainDb, 0.0f);
  EXPECT_FLOAT_EQ(meta3->gainDb, +6.0f);

  EXPECT_TRUE(meta0->loopEnabled);
  EXPECT_FALSE(meta1->loopEnabled);
  EXPECT_TRUE(meta2->loopEnabled);
  EXPECT_FALSE(meta3->loopEnabled);

  EXPECT_EQ(meta0->trimInSamples, 0);
  EXPECT_EQ(meta1->trimInSamples, 1000);
  EXPECT_EQ(meta2->trimInSamples, 2000);
  EXPECT_EQ(meta3->trimInSamples, 3000);

  // Start and stop all clips
  for (auto handle : handles) {
    m_transport->startClip(handle);
  }

  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();

  for (int i = 0; i < 5; ++i) {
    m_transport->processAudio(buffers, 2, 512);
  }

  for (auto handle : handles) {
    m_transport->stopClip(handle);
  }

  for (int i = 0; i < 10; ++i) {
    m_transport->processAudio(buffers, 2, 512);
    m_transport->processCallbacks();
  }

  // Verify metadata still unique (no cross-contamination)
  meta0 = m_transport->getClipMetadata(handles[0]);
  meta1 = m_transport->getClipMetadata(handles[1]);
  meta2 = m_transport->getClipMetadata(handles[2]);
  meta3 = m_transport->getClipMetadata(handles[3]);

  EXPECT_FLOAT_EQ(meta0->gainDb, -12.0f);
  EXPECT_FLOAT_EQ(meta1->gainDb, -6.0f);
  EXPECT_FLOAT_EQ(meta2->gainDb, 0.0f);
  EXPECT_FLOAT_EQ(meta3->gainDb, +6.0f);
}

// Test 7: updateClipMetadata batch update works correctly
TEST_F(ClipMetadataTest, BatchUpdateMetadata) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_metadata.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Set all metadata in one call
  ClipMetadata metadata;
  metadata.trimInSamples = 2000;
  metadata.trimOutSamples = 35000;
  metadata.fadeInSeconds = 0.03;
  metadata.fadeOutSeconds = 0.07;
  metadata.fadeInCurve = FadeCurve::EqualPower;
  metadata.fadeOutCurve = FadeCurve::Linear;
  metadata.gainDb = -9.0f;
  metadata.loopEnabled = true;
  metadata.stopOthersOnPlay = false;

  auto result = m_transport->updateClipMetadata(handle, metadata);
  EXPECT_EQ(result, SessionGraphError::OK);

  // Verify all fields were set
  auto retrieved = m_transport->getClipMetadata(handle);
  ASSERT_TRUE(retrieved.has_value());

  EXPECT_EQ(retrieved->trimInSamples, 2000);
  EXPECT_EQ(retrieved->trimOutSamples, 35000);
  EXPECT_DOUBLE_EQ(retrieved->fadeInSeconds, 0.03);
  EXPECT_DOUBLE_EQ(retrieved->fadeOutSeconds, 0.07);
  EXPECT_EQ(retrieved->fadeInCurve, FadeCurve::EqualPower);
  EXPECT_EQ(retrieved->fadeOutCurve, FadeCurve::Linear);
  EXPECT_FLOAT_EQ(retrieved->gainDb, -9.0f);
  EXPECT_TRUE(retrieved->loopEnabled);
  EXPECT_FALSE(retrieved->stopOthersOnPlay);
}

// Test 8: Session defaults are applied to new clips
TEST_F(ClipMetadataTest, SessionDefaultsApplied) {
  // Set session defaults
  SessionDefaults defaults;
  defaults.fadeInSeconds = 0.05;
  defaults.fadeOutSeconds = 0.1;
  defaults.fadeInCurve = FadeCurve::Exponential;
  defaults.fadeOutCurve = FadeCurve::EqualPower;
  defaults.loopEnabled = true;
  defaults.stopOthersOnPlay = false;
  defaults.gainDb = -3.0f;

  m_transport->setSessionDefaults(defaults);

  // Register new clip (should inherit defaults)
  auto handle = static_cast<ClipHandle>(1);
  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_metadata.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Verify clip inherited defaults
  auto metadata = m_transport->getClipMetadata(handle);
  ASSERT_TRUE(metadata.has_value());

  EXPECT_DOUBLE_EQ(metadata->fadeInSeconds, 0.05);
  EXPECT_DOUBLE_EQ(metadata->fadeOutSeconds, 0.1);
  EXPECT_EQ(metadata->fadeInCurve, FadeCurve::Exponential);
  EXPECT_EQ(metadata->fadeOutCurve, FadeCurve::EqualPower);
  EXPECT_TRUE(metadata->loopEnabled);
  EXPECT_FALSE(metadata->stopOthersOnPlay);
  EXPECT_FLOAT_EQ(metadata->gainDb, -3.0f);
}

// Test 9: getSessionDefaults returns correct values
TEST_F(ClipMetadataTest, GetSessionDefaults) {
  // Set session defaults
  SessionDefaults defaults;
  defaults.fadeInSeconds = 0.02;
  defaults.fadeOutSeconds = 0.08;
  defaults.fadeInCurve = FadeCurve::Linear;
  defaults.fadeOutCurve = FadeCurve::Exponential;
  defaults.loopEnabled = false;
  defaults.stopOthersOnPlay = true;
  defaults.gainDb = +3.0f;

  m_transport->setSessionDefaults(defaults);

  // Retrieve defaults
  auto retrieved = m_transport->getSessionDefaults();

  EXPECT_DOUBLE_EQ(retrieved.fadeInSeconds, 0.02);
  EXPECT_DOUBLE_EQ(retrieved.fadeOutSeconds, 0.08);
  EXPECT_EQ(retrieved.fadeInCurve, FadeCurve::Linear);
  EXPECT_EQ(retrieved.fadeOutCurve, FadeCurve::Exponential);
  EXPECT_FALSE(retrieved.loopEnabled);
  EXPECT_TRUE(retrieved.stopOthersOnPlay);
  EXPECT_FLOAT_EQ(retrieved.gainDb, +3.0f);
}

// Test 10: stopOthersOnPlay metadata persists
TEST_F(ClipMetadataTest, StopOthersOnPlayPersists) {
  auto handle = static_cast<ClipHandle>(1);

  auto regResult = m_transport->registerClipAudio(handle, "/tmp/test_clip_metadata.wav");
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Enable stopOthersOnPlay
  m_transport->setClipStopOthersMode(handle, true);

  // Verify it's enabled
  EXPECT_TRUE(m_transport->getClipStopOthersMode(handle));

  // Stop/start cycle
  m_transport->startClip(handle);
  float* buffers[2] = {nullptr, nullptr};
  std::vector<float> leftBuffer(512, 0.0f);
  std::vector<float> rightBuffer(512, 0.0f);
  buffers[0] = leftBuffer.data();
  buffers[1] = rightBuffer.data();
  m_transport->processAudio(buffers, 2, 512);

  m_transport->stopClip(handle);
  for (int i = 0; i < 10; ++i) {
    m_transport->processAudio(buffers, 2, 512);
    m_transport->processCallbacks();
  }

  // Should still be enabled
  EXPECT_TRUE(m_transport->getClipStopOthersMode(handle));

  // Disable it
  m_transport->setClipStopOthersMode(handle, false);

  // Verify disabled
  EXPECT_FALSE(m_transport->getClipStopOthersMode(handle));
}
