// SPDX-License-Identifier: MIT
#include <orpheus/audio_file_capabilities.h>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace {
using namespace orpheus;

class AudioFileCapabilitiesTest : public ::testing::Test {
protected:
  void SetUp() override {
    directory = std::filesystem::temp_directory_path() /
                ("orpheus-capabilities-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
  }
  void TearDown() override {
    std::error_code error;
    std::filesystem::remove_all(directory, error);
  }
  std::filesystem::path directory;
};

TEST_F(AudioFileCapabilitiesTest, RegistrySeparatesPolicyFromProviderAvailability) {
  const auto capabilities = getAudioFileCapabilities();
  ASSERT_EQ(capabilities.codecs.size(), 6u);
  constexpr std::array formats{AudioFileFormat::Unknown, AudioFileFormat::WAV,
                               AudioFileFormat::AIFF,    AudioFileFormat::FLAC,
                               AudioFileFormat::MP3,     AudioFileFormat::OGG};
  for (size_t i = 0; i < formats.size(); ++i) {
    const auto& codec = capabilities.codecs[i];
    EXPECT_EQ(codec.format, formats[i]);
    EXPECT_EQ(codec.documented_read_support, i >= 1 && i <= 3);
    if (i >= 1 && i <= 3) {
      ASSERT_EQ(codec.write_sample_formats.size(), i == 3 ? 2u : 3u);
      EXPECT_EQ(codec.write_sample_formats[0], AudioSampleFormat::Int16);
      EXPECT_EQ(codec.write_sample_formats[1], AudioSampleFormat::Int24);
      if (i != 3)
        EXPECT_EQ(codec.write_sample_formats[2], AudioSampleFormat::Float32);
    } else {
      EXPECT_TRUE(codec.write_sample_formats.empty());
    }
  }
  AudioFileWriterConfig valid{AudioFileFormat::WAV, 48000, 2, AudioSampleFormat::Int16};
#if defined(ORPHEUS_AUDIO_FILE_CAPABILITIES_HAVE_SNDFILE)
  EXPECT_TRUE(capabilities.file_io_available);
  EXPECT_EQ(preflightAudioFileWrite(valid), SessionGraphError::OK);
#else
  EXPECT_FALSE(capabilities.file_io_available);
  EXPECT_EQ(preflightAudioFileWrite(valid), SessionGraphError::NotReady);
  EXPECT_EQ(probeAudioFile((directory / "missing.wav").string()).error,
            SessionGraphError::NotReady);
#endif
}

TEST_F(AudioFileCapabilitiesTest, RefusedConfigurationsNeverCreateDestination) {
  auto writer = createAudioFileWriter();
  const auto destination = directory / "refused.wav";
  const auto check = [&](AudioFileWriterConfig config, SessionGraphError expected) {
    EXPECT_EQ(preflightAudioFileWrite(config), expected);
    if (writer)
      EXPECT_EQ(writer->open(destination.string(), config), expected);
    EXPECT_FALSE(std::filesystem::exists(destination));
  };
  AudioFileWriterConfig config{AudioFileFormat::MP3, 0, 2, AudioSampleFormat::Int16};
  check(config, SessionGraphError::InvalidParameter); // Rate precedes unsupported container.
  config.sample_rate = 48000;
  check(config, SessionGraphError::NotSupported);
  config.format = AudioFileFormat::OGG;
  check(config, SessionGraphError::NotSupported);
  config.format = AudioFileFormat::WAV;
  config.sample_rate = UINT32_MAX;
  check(config, SessionGraphError::InvalidParameter);
  config.sample_rate = 48000;
  config.num_channels = 0;
  check(config, SessionGraphError::InvalidParameter);
  config.num_channels = 2;
  config.sample_format = static_cast<AudioSampleFormat>(255);
  check(config, SessionGraphError::InvalidParameter);
  config.format = AudioFileFormat::FLAC;
  config.sample_format = AudioSampleFormat::Float32;
  check(config, SessionGraphError::InvalidParameter);
}

TEST_F(AudioFileCapabilitiesTest, EmptyProbeIsInvalidEvenWithoutProvider) {
  EXPECT_EQ(probeAudioFile("").error, SessionGraphError::InvalidParameter);
}

#if defined(ORPHEUS_AUDIO_FILE_CAPABILITIES_HAVE_SNDFILE)
TEST_F(AudioFileCapabilitiesTest, BackendRefusesNineChannelFlacBeforeCreatingFile) {
  AudioFileWriterConfig config{AudioFileFormat::FLAC, 48000, 9, AudioSampleFormat::Int16};
  EXPECT_EQ(preflightAudioFileWrite(config), SessionGraphError::InvalidParameter);
  auto writer = createAudioFileWriter();
  ASSERT_NE(writer, nullptr);
  const auto destination = directory / "nine-channel.flac";
  EXPECT_EQ(writer->open(destination.string(), config), SessionGraphError::InvalidParameter);
  EXPECT_FALSE(std::filesystem::exists(destination));
}

TEST_F(AudioFileCapabilitiesTest, RuntimeFileFailuresAreNotCapabilityRefusals) {
  const auto missing = probeAudioFile((directory / "missing.wav").string());
  EXPECT_EQ(missing.error, SessionGraphError::InternalError);
  EXPECT_FALSE(missing.errorMessage.empty());
  const auto corrupt = directory / "corrupt.wav";
  {
    std::ofstream stream(corrupt, std::ios::binary);
    stream << "not an audio header";
    ASSERT_TRUE(stream.good());
  }
  EXPECT_EQ(probeAudioFile(corrupt.string()).error, SessionGraphError::InternalError);
  AudioFileWriterConfig config{AudioFileFormat::WAV, 48000, 2, AudioSampleFormat::Int16};
  ASSERT_EQ(preflightAudioFileWrite(config), SessionGraphError::OK);
  auto writer = createAudioFileWriter();
  ASSERT_NE(writer, nullptr);
  EXPECT_EQ(writer->open((directory / "absent" / "output.wav").string(), config),
            SessionGraphError::InternalError);
}

TEST_F(AudioFileCapabilitiesTest, ProbeReportsHeaderWithoutHashingUnlikeReaderOpen) {
  const auto path = directory / "source.wav";
  AudioFileWriterConfig config{AudioFileFormat::WAV, 44100, 2, AudioSampleFormat::Int16};
  auto writer = createAudioFileWriter();
  ASSERT_NE(writer, nullptr);
  ASSERT_EQ(writer->open(path.string(), config), SessionGraphError::OK);
  const std::array<float, 6> samples{0.25f, -0.25f, 0.5f, -0.5f, 0.0f, 0.0f};
  const auto written = writer->writeSamples(samples.data(), 3);
  ASSERT_EQ(written.error, SessionGraphError::OK);
  ASSERT_EQ(written.value, 3u);
  ASSERT_EQ(writer->close(), SessionGraphError::OK);
  const auto probe = probeAudioFile(path.string());
  ASSERT_EQ(probe.error, SessionGraphError::OK) << probe.errorMessage;
  EXPECT_EQ(probe.value.format, AudioFileFormat::WAV);
  EXPECT_EQ(probe.value.sample_rate, 44100u);
  EXPECT_EQ(probe.value.num_channels, 2u);
  EXPECT_EQ(probe.value.duration_samples, 3);
  EXPECT_EQ(probe.value.bit_depth, 16u);
  EXPECT_EQ(probe.value.codec, "PCM_16");
  EXPECT_TRUE(probe.value.file_hash_sha256.empty());
  auto reader = createAudioFileReader();
  ASSERT_NE(reader, nullptr);
  const auto opened = reader->open(path.string());
  ASSERT_EQ(opened.error, SessionGraphError::OK);
  EXPECT_EQ(opened.value.file_hash_sha256.size(), 64u);
  EXPECT_EQ(opened.value.duration_samples, probe.value.duration_samples);
  EXPECT_EQ(opened.value.bit_depth, probe.value.bit_depth);
}

TEST_F(AudioFileCapabilitiesTest, IncidentalAuDecodingPreservesUnknownMetadata) {
  // Sun AU, four signed PCM8 samples. Neither AU nor PCM8 is represented by the SDK enums.
  const std::array<unsigned char, 28> bytes{0x2e, 0x73, 0x6e, 0x64, 0, 0,  0,   24, 0,    0,
                                            0,    4,    0,    0,    0, 2,  0,   0,  0xbb, 0x80,
                                            0,    0,    0,    1,    0, 32, 224, 0};
  const auto path = directory / "incidental.au";
  {
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    ASSERT_TRUE(stream.good());
  }
  const auto probe = probeAudioFile(path.string());
  ASSERT_EQ(probe.error, SessionGraphError::OK) << probe.errorMessage;
  EXPECT_EQ(probe.value.format, AudioFileFormat::Unknown);
  EXPECT_EQ(probe.value.bit_depth, 0u);
  EXPECT_EQ(probe.value.codec, "UNKNOWN");
  auto reader = createAudioFileReader();
  ASSERT_NE(reader, nullptr);
  const auto opened = reader->open(path.string());
  ASSERT_EQ(opened.error, SessionGraphError::OK);
  EXPECT_EQ(opened.value.format, AudioFileFormat::Unknown);
  EXPECT_EQ(opened.value.bit_depth, 0u);
  std::array<float, 4> decoded{};
  const auto result = reader->readSamples(decoded.data(), decoded.size());
  ASSERT_EQ(result.error, SessionGraphError::OK);
  ASSERT_EQ(result.value, decoded.size());
  EXPECT_EQ(decoded, (std::array<float, 4>{0, 0.25f, -0.25f, 0}));
}
#endif
} // namespace
