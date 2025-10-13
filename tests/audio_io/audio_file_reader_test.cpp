// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>
#include <orpheus/audio_file_reader.h>

#include <fstream>
#include <vector>

using namespace orpheus;

// Test fixture
class AudioFileReaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_reader = createAudioFileReader();
    }

    void TearDown() override {
        m_reader->close();
        m_reader.reset();
    }

    std::unique_ptr<IAudioFileReader> m_reader;
};

// Basic Tests

TEST_F(AudioFileReaderTest, InitialState) {
    // Initially, no file should be open
    EXPECT_FALSE(m_reader->isOpen());
    EXPECT_EQ(m_reader->getCurrentPosition(), 0);
}

TEST_F(AudioFileReaderTest, OpenNonExistentFile) {
    auto result = m_reader->open("/nonexistent/file.wav");

    EXPECT_FALSE(result.isOk());
    EXPECT_EQ(result.error, SessionGraphError::InternalError);
    EXPECT_FALSE(m_reader->isOpen());
}

TEST_F(AudioFileReaderTest, ReadWithoutOpening) {
    std::vector<float> buffer(1024);
    auto result = m_reader->readSamples(buffer.data(), 1024);

    EXPECT_FALSE(result.isOk());
    EXPECT_EQ(result.error, SessionGraphError::NotReady);
    EXPECT_EQ(result.value, 0);
}

TEST_F(AudioFileReaderTest, SeekWithoutOpening) {
    auto error = m_reader->seek(0);
    EXPECT_EQ(error, SessionGraphError::NotReady);
}

TEST_F(AudioFileReaderTest, CloseWhenNotOpen) {
    // Should not crash
    m_reader->close();
    EXPECT_FALSE(m_reader->isOpen());
}

// TODO: Add tests with actual audio files
// These require test fixtures (WAV/AIFF/FLAC files)
//
// TEST_F(AudioFileReaderTest, OpenWAVFile) {
//     auto result = m_reader->open("tests/fixtures/audio/test_48k_stereo.wav");
//     ASSERT_TRUE(result.isOk());
//
//     auto& metadata = result.value;
//     EXPECT_EQ(metadata.format, AudioFileFormat::WAV);
//     EXPECT_EQ(metadata.sample_rate, 48000);
//     EXPECT_EQ(metadata.num_channels, 2);
//     EXPECT_GT(metadata.duration_samples, 0);
//     EXPECT_TRUE(m_reader->isOpen());
// }
//
// TEST_F(AudioFileReaderTest, ReadSamples) {
//     m_reader->open("tests/fixtures/audio/test_48k_stereo.wav");
//
//     std::vector<float> buffer(1024 * 2);  // 1024 frames * 2 channels
//     auto result = m_reader->readSamples(buffer.data(), 1024);
//
//     ASSERT_TRUE(result.isOk());
//     EXPECT_EQ(result.value, 1024);  // Should read 1024 frames
//     EXPECT_EQ(m_reader->getCurrentPosition(), 1024);
// }
//
// TEST_F(AudioFileReaderTest, SeekToPosition) {
//     m_reader->open("tests/fixtures/audio/test_48k_stereo.wav");
//
//     auto error = m_reader->seek(48000);  // Seek to 1 second @ 48kHz
//     EXPECT_EQ(error, SessionGraphError::OK);
//     EXPECT_EQ(m_reader->getCurrentPosition(), 48000);
//
//     // Read after seek
//     std::vector<float> buffer(1024 * 2);
//     auto result = m_reader->readSamples(buffer.data(), 1024);
//     EXPECT_TRUE(result.isOk());
//     EXPECT_EQ(m_reader->getCurrentPosition(), 48000 + 1024);
// }
//
// TEST_F(AudioFileReaderTest, ReadUntilEOF) {
//     auto open_result = m_reader->open("tests/fixtures/audio/test_48k_stereo.wav");
//     ASSERT_TRUE(open_result.isOk());
//
//     int64_t total_duration = open_result.value.duration_samples;
//     int64_t total_read = 0;
//
//     std::vector<float> buffer(1024 * 2);
//     while (total_read < total_duration) {
//         auto result = m_reader->readSamples(buffer.data(), 1024);
//         ASSERT_TRUE(result.isOk());
//
//         if (result.value == 0) {
//             break;  // EOF
//         }
//
//         total_read += result.value;
//     }
//
//     EXPECT_EQ(total_read, total_duration);
// }
//
// TEST_F(AudioFileReaderTest, OpenMultipleFormats) {
//     // Test WAV
//     auto wav_result = m_reader->open("tests/fixtures/audio/test.wav");
//     EXPECT_TRUE(wav_result.isOk());
//     EXPECT_EQ(wav_result.value.format, AudioFileFormat::WAV);
//     m_reader->close();
//
//     // Test AIFF
//     auto aiff_result = m_reader->open("tests/fixtures/audio/test.aiff");
//     EXPECT_TRUE(aiff_result.isOk());
//     EXPECT_EQ(aiff_result.value.format, AudioFileFormat::AIFF);
//     m_reader->close();
//
//     // Test FLAC
//     auto flac_result = m_reader->open("tests/fixtures/audio/test.flac");
//     EXPECT_TRUE(flac_result.isOk());
//     EXPECT_EQ(flac_result.value.format, AudioFileFormat::FLAC);
//     m_reader->close();
// }
