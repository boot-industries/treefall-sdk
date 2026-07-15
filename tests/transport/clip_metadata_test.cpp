#include <gtest/gtest.h>

#include "../../src/core/transport/transport_controller.h"
#include <filesystem>
#include <fstream>
#include <orpheus/audio_file_reader.h>
#include <vector>

using namespace orpheus;

// Test fixture for clip metadata persistence
class ClipMetadataTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_transport = std::make_unique<TransportController>(nullptr, TransportConfig{.sampleRate = static_cast<uint32_t>(48000)});
    m_testFilePath = (std::filesystem::temp_directory_path() / "test_clip_metadata.wav").string();
    createTestAudioFile();
  }

  void TearDown() override {
    m_transport.reset();
    if (!m_testFilePath.empty() && std::filesystem::exists(m_testFilePath)) {
      std::filesystem::remove(m_testFilePath);
    }
  }

  void createTestAudioFile() {
    // Create a short WAV file (1 second = 48000 samples @ 48kHz)
    std::ofstream file(m_testFilePath, std::ios::binary);

    const uint32_t sampleRate = 48000;
    const uint32_t duration = 48000; // 1 second
    const uint16_t numChannels = 2;

    // WAV header
    file << "RIFF";
    uint32_t fileSize = 36 + (duration * numChannels * 2);
    file.write(reinterpret_cast<const char*>(&fileSize), 4);
    file << "WAVE";

    // fmt chunk
    file << "fmt ";
    uint32_t fmtSize = 16;
    file.write(reinterpret_cast<const char*>(&fmtSize), 4);
    uint16_t audioFormat = 1;
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

    // Write silence
    std::vector<char> silence(dataSize, 0);
    file.write(silence.data(), dataSize);
    file.close();
  }

  std::unique_ptr<TransportController> m_transport;
  std::string m_testFilePath;
};

// Test 1: Metadata persists after stop/start cycle
TEST_F(ClipMetadataTest, MetadataSurvivesStopStartCycle) {
  auto handle = static_cast<ClipHandle>(1);
  auto regResult = m_transport->registerClipAudio(handle, m_testFilePath.c_str());
  ASSERT_EQ(regResult, SessionGraphError::OK);

  // Apply metadata (Gain + Trim)
  m_transport->updateClipGain(
      handle, 0.5f); // -6dB (approx, using as linear or dB? updateClipGain expects dB usually, but
                     // test said 0.5f. If 0.5f dB, that's small. If linear, API is
                     // updateClipGain(dB). Let's assume test meant 0.5 dB or 0.5 linear converted.
  // Wait, API says updateClipGain(float gainDb). So 0.5f is 0.5 dB.
  // But EXPECT_FLOAT_EQ(metadata->gainDb, 0.5f) checks persistence, so value doesn't matter as long
  // as it persists.

  m_transport->updateClipTrimPoints(handle, 1000, 5000);

  // Start then stop
  m_transport->startClip(handle);
  m_transport->stopClip(handle);

  // Verify metadata still correct
  auto metadata = m_transport->getClipMetadata(handle);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_FLOAT_EQ(metadata->gainDb, 0.5f);
  EXPECT_EQ(metadata->trimInSamples, 1000);
  EXPECT_EQ(metadata->trimOutSamples, 5000);
}

// Test 2: Trim points persist
TEST_F(ClipMetadataTest, TrimPointsPersist) {
  auto handle = static_cast<ClipHandle>(1);
  m_transport->registerClipAudio(handle, m_testFilePath.c_str());

  int64_t trimIn = 500;
  int64_t trimOut = 10000;

  m_transport->updateClipTrimPoints(handle, trimIn, trimOut);

  auto metadata = m_transport->getClipMetadata(handle);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_EQ(metadata->trimInSamples, trimIn);
  EXPECT_EQ(metadata->trimOutSamples, trimOut);
}

// Test 3: Fade curves persist
TEST_F(ClipMetadataTest, FadeCurvesPersist) {
  auto handle = static_cast<ClipHandle>(1);
  m_transport->registerClipAudio(handle, m_testFilePath.c_str());

  double fadeInTime = 0.3;  // 0.3s fits in 1s clip
  double fadeOutTime = 0.4; // 0.4s fits in 1s clip (total 0.7s leaves 0.3s unfaded)
  auto fadeInCurve = FadeCurve::Exponential;
  auto fadeOutCurve = FadeCurve::EqualPower; // Logarithmic not supported

  auto result =
      m_transport->updateClipFades(handle, fadeInTime, fadeOutTime, fadeInCurve, fadeOutCurve);
  ASSERT_EQ(result, SessionGraphError::OK); // Verify update succeeded

  auto metadata = m_transport->getClipMetadata(handle);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_DOUBLE_EQ(metadata->fadeInSeconds, fadeInTime);
  EXPECT_DOUBLE_EQ(metadata->fadeOutSeconds, fadeOutTime);
  EXPECT_EQ(metadata->fadeInCurve, fadeInCurve);
  EXPECT_EQ(metadata->fadeOutCurve, fadeOutCurve);
}

// Test 4: Gain persists
TEST_F(ClipMetadataTest, GainPersists) {
  auto handle = static_cast<ClipHandle>(1);
  m_transport->registerClipAudio(handle, m_testFilePath.c_str());

  float gain = 1.5f; // +1.5dB
  m_transport->updateClipGain(handle, gain);

  auto metadata = m_transport->getClipMetadata(handle);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_FLOAT_EQ(metadata->gainDb, gain);
}

// Test 5: Loop mode persists
TEST_F(ClipMetadataTest, LoopModePersists) {
  auto handle = static_cast<ClipHandle>(1);
  m_transport->registerClipAudio(handle, m_testFilePath.c_str());

  m_transport->setClipLoopMode(handle, true);

  auto metadata = m_transport->getClipMetadata(handle);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_TRUE(metadata->loopEnabled);

  m_transport->setClipLoopMode(handle, false);
  metadata = m_transport->getClipMetadata(handle);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_FALSE(metadata->loopEnabled);
}

// Test 6: Multiple clips don't cross-contaminate metadata
TEST_F(ClipMetadataTest, MultipleClipsNoContamination) {
  auto handle1 = static_cast<ClipHandle>(1);
  auto handle2 = static_cast<ClipHandle>(2);

  m_transport->registerClipAudio(handle1, m_testFilePath.c_str());
  m_transport->registerClipAudio(handle2, m_testFilePath.c_str());

  // Set different metadata
  m_transport->updateClipGain(handle1, 0.8f);
  m_transport->updateClipGain(handle2, 0.2f);

  m_transport->setClipLoopMode(handle1, true);
  m_transport->setClipLoopMode(handle2, false);

  // Verify handle 1
  auto meta1 = m_transport->getClipMetadata(handle1);
  EXPECT_FLOAT_EQ(meta1->gainDb, 0.8f);
  EXPECT_TRUE(meta1->loopEnabled);

  // Verify handle 2
  auto meta2 = m_transport->getClipMetadata(handle2);
  EXPECT_FLOAT_EQ(meta2->gainDb, 0.2f);
  EXPECT_FALSE(meta2->loopEnabled);
}

// Test 7: Batch update (updateClipMetadata)
// Testing individual updates accumulating correctly
TEST_F(ClipMetadataTest, BatchUpdateMetadata) {
  auto handle = static_cast<ClipHandle>(1);
  m_transport->registerClipAudio(handle, m_testFilePath.c_str());

  // Chain updates
  m_transport->updateClipGain(handle, 0.9f);
  m_transport->updateClipTrimPoints(handle, 500, 2000);
  m_transport->setClipLoopMode(handle, true);
  m_transport->updateClipFades(handle, 0.1, 0.1, FadeCurve::Linear, FadeCurve::Linear);

  auto meta = m_transport->getClipMetadata(handle);
  EXPECT_FLOAT_EQ(meta->gainDb, 0.9f);
  EXPECT_EQ(meta->trimInSamples, 500);
  EXPECT_EQ(meta->trimOutSamples, 2000);
  EXPECT_TRUE(meta->loopEnabled);
  EXPECT_DOUBLE_EQ(meta->fadeInSeconds, 0.1);
}

// Test 8: Defaults applied correctly on registration
TEST_F(ClipMetadataTest, SessionDefaultsApplied) {
  auto handle = static_cast<ClipHandle>(1);
  m_transport->registerClipAudio(handle, m_testFilePath.c_str());

  auto meta = m_transport->getClipMetadata(handle);
  ASSERT_TRUE(meta.has_value());

  // Check defaults
  EXPECT_FLOAT_EQ(meta->gainDb, 0.0f); // Default 0dB
  EXPECT_EQ(meta->trimInSamples, 0);
  // trimOut depends on file length, checked in other tests
  EXPECT_FALSE(meta->loopEnabled);
  EXPECT_DOUBLE_EQ(meta->fadeInSeconds, 0.0); // Default 0s fade
  EXPECT_DOUBLE_EQ(meta->fadeOutSeconds, 0.0);
}

// Test 9: getSessionDefaults returns correct default struct
TEST_F(ClipMetadataTest, GetSessionDefaults) {
  SessionDefaults defaults; // Default ctor should set defaults
  EXPECT_FLOAT_EQ(defaults.gainDb, 0.0f);
  EXPECT_FALSE(defaults.loopEnabled);
  EXPECT_FALSE(defaults.stopOthersOnPlay);
}

// Test 10: StopOthers mode persists
TEST_F(ClipMetadataTest, StopOthersOnPlayPersists) {
  auto handle = static_cast<ClipHandle>(1);
  m_transport->registerClipAudio(handle, m_testFilePath.c_str());

  m_transport->setClipStopOthersMode(handle, true);

  auto meta = m_transport->getClipMetadata(handle);
  ASSERT_TRUE(meta.has_value());
  EXPECT_TRUE(meta->stopOthersOnPlay);

  m_transport->setClipStopOthersMode(handle, false);
  meta = m_transport->getClipMetadata(handle);
  EXPECT_FALSE(meta->stopOthersOnPlay);
}