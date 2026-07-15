// SPDX-License-Identifier: MIT
#include "../../src/core/transport/transport_controller.h"
#include <orpheus/channel_format.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace orpheus;

namespace {

constexpr uint32_t kSampleRate = 48000;
constexpr uint16_t kChannels = 8;
constexpr size_t kFrames = 64;

class MultichannelTransportTest : public ::testing::Test {
protected:
  void SetUp() override {
    filePath = (std::filesystem::temp_directory_path() /
                "orpheus_multichannel_transport_test.wav")
                   .string();
    stereoPath = (std::filesystem::temp_directory_path() /
                  "orpheus_stereo_route_test.wav")
                     .string();
    writeFixture(filePath, kChannels);
    writeFixture(stereoPath, 2);
  }

  void TearDown() override {
    std::error_code error;
    std::filesystem::remove(filePath, error);
    std::filesystem::remove(stereoPath, error);
  }

  void writeFixture(const std::string& path, uint16_t channels) const {
    constexpr uint16_t bitsPerSample = 16;
    const uint16_t blockAlign = channels * (bitsPerSample / 8);
    const uint32_t dataSize = static_cast<uint32_t>(kFrames * blockAlign);
    const uint32_t riffSize = 36 + dataSize;
    const uint32_t byteRate = kSampleRate * blockAlign;
    constexpr uint32_t fmtSize = 16;
    constexpr uint16_t pcmFormat = 1;

    std::ofstream file(path, std::ios::binary);
    ASSERT_TRUE(file.is_open());
    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&riffSize), sizeof(riffSize));
    file.write("WAVEfmt ", 8);
    file.write(reinterpret_cast<const char*>(&fmtSize), sizeof(fmtSize));
    file.write(reinterpret_cast<const char*>(&pcmFormat), sizeof(pcmFormat));
    file.write(reinterpret_cast<const char*>(&channels), sizeof(channels));
    file.write(reinterpret_cast<const char*>(&kSampleRate), sizeof(kSampleRate));
    file.write(reinterpret_cast<const char*>(&byteRate), sizeof(byteRate));
    file.write(reinterpret_cast<const char*>(&blockAlign), sizeof(blockAlign));
    file.write(reinterpret_cast<const char*>(&bitsPerSample), sizeof(bitsPerSample));
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&dataSize), sizeof(dataSize));

    for (size_t frame = 0; frame < kFrames; ++frame) {
      for (uint16_t channel = 0; channel < channels; ++channel) {
        const int16_t sample = static_cast<int16_t>((channel + 1) * 2048);
        file.write(reinterpret_cast<const char*>(&sample), sizeof(sample));
      }
    }
    ASSERT_TRUE(file.good());
  }

  std::string filePath;
  std::string stereoPath;
};

TEST(ChannelFormatTest, DistinguishesDiscreteAndMatrixStereoCompoundPairs) {
  const auto discrete = ChannelFormat::SMPTE51Stereo();
  const auto matrix = ChannelFormat::SMPTE51MatrixStereo();

  EXPECT_EQ(discrete.layout, ChannelLayout::SMPTE_51_ST);
  EXPECT_EQ(discrete.channel_map[6], Speaker::Lo);
  EXPECT_EQ(discrete.channel_map[7], Speaker::Ro);
  EXPECT_EQ(matrix.layout, ChannelLayout::SMPTE_51_LTRT);
  EXPECT_EQ(matrix.channel_map[6], Speaker::Lt);
  EXPECT_EQ(matrix.channel_map[7], Speaker::Rt);
  EXPECT_NE(discrete.channel_map[6], matrix.channel_map[6]);
  EXPECT_NE(discrete.channel_map[7], matrix.channel_map[7]);
}

TEST_F(MultichannelTransportTest, RequiresExplicitSMPTEBundleAndPreservesEightChannels) {
  TransportConfig config;
  config.sampleRate = kSampleRate;
  config.outputChannels = kChannels;
  config.maxBlockFrames = static_cast<uint32_t>(kFrames);
  config.maxActiveVoices = 2;
  config.maxSourceChannels = kChannels;
  config.sourceChannelPolicy = SourceChannelPolicy::Discrete;

  auto transport = createTransportController(nullptr, config);
  ASSERT_NE(transport, nullptr);
  ASSERT_EQ(transport->registerClipAudio(1, filePath), SessionGraphError::OK);
  EXPECT_EQ(transport->startClip(1), SessionGraphError::InvalidParameter);

  auto metadata = transport->getClipMetadata(1);
  ASSERT_TRUE(metadata.has_value());
  const auto format = ChannelFormat::SMPTE51Stereo();
  metadata->sourceLayout = format.layout;
  metadata->speakerPatchSize = format.num_channels;
  const auto wrongBed = ChannelFormat::Surround71();
  std::copy_n(wrongBed.channel_map.begin(), wrongBed.num_channels,
              metadata->speakerPatch.begin());
  EXPECT_EQ(transport->updateClipMetadata(1, *metadata),
            SessionGraphError::InvalidParameter);
  std::copy_n(format.channel_map.begin(), format.num_channels,
              metadata->speakerPatch.begin());
  ASSERT_EQ(transport->updateClipMetadata(1, *metadata), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);

  std::array<std::array<float, kFrames>, kChannels> storage{};
  std::array<float*, kChannels> outputs{};
  for (size_t channel = 0; channel < kChannels; ++channel) {
    outputs[channel] = storage[channel].data();
  }

  transport->processAudio(outputs.data(), outputs.size(), kFrames);

  for (size_t channel = 0; channel < kChannels; ++channel) {
    const float expected = static_cast<float>((channel + 1) * 2048) / 32768.0f;
    EXPECT_NEAR(storage[channel][0], expected, 1.0e-4f) << "channel " << channel;
    EXPECT_NEAR(storage[channel][kFrames - 1], expected, 1.0e-4f) << "channel " << channel;
  }
}

TEST_F(MultichannelTransportTest, RoutesStereoThroughLogicalGroupToSelectedOutputPair) {
  TransportConfig config;
  config.sampleRate = kSampleRate;
  config.outputChannels = kChannels;
  config.maxBlockFrames = static_cast<uint32_t>(kFrames);
  config.maxActiveVoices = 2;
  config.maxSourceChannels = kChannels;

  auto transport = createTransportController(nullptr, config);
  ASSERT_NE(transport, nullptr);
  ASSERT_EQ(transport->registerClipAudio(2, stereoPath), SessionGraphError::OK);
  ASSERT_EQ(transport->setGroupOutputBus(3, OutputBusRoute{6, 2}),
            SessionGraphError::OK);
  EXPECT_EQ(transport->getGroupOutputBus(3),
            std::optional<OutputBusRoute>(OutputBusRoute{6, 2}));

  auto metadata = transport->getClipMetadata(2);
  ASSERT_TRUE(metadata.has_value());
  metadata->routingGroup = 3;
  metadata->loopEnabled = true;
  ASSERT_EQ(transport->updateClipMetadata(2, *metadata), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(2), SessionGraphError::OK);

  std::array<std::array<float, kFrames>, kChannels> storage{};
  std::array<float*, kChannels> outputs{};
  for (size_t channel = 0; channel < kChannels; ++channel) {
    outputs[channel] = storage[channel].data();
  }

  transport->processAudio(outputs.data(), outputs.size(), kFrames);
  for (size_t channel = 0; channel < 6; ++channel) {
    EXPECT_FLOAT_EQ(storage[channel][0], 0.0f) << "channel " << channel;
  }
  EXPECT_NEAR(storage[6][0], 2048.0f / 32768.0f, 1.0e-4f);
  EXPECT_NEAR(storage[7][0], 4096.0f / 32768.0f, 1.0e-4f);

  ASSERT_EQ(transport->getRoutingMatrix()->setGroupMute(3, true), SessionGraphError::OK);
  transport->processAudio(outputs.data(), outputs.size(), kFrames);
  for (const auto& channel : storage) {
    EXPECT_FLOAT_EQ(channel[0], 0.0f);
  }
}

TEST(MultichannelTransportConfigTest, RejectsUnsupportedRealtimeBounds) {
  TransportConfig config;
  config.sampleRate = kSampleRate;

  config.outputChannels = 33;
  EXPECT_EQ(createTransportController(nullptr, config), nullptr);

  config.outputChannels = 2;
  config.maxBlockFrames = 2049;
  EXPECT_EQ(createTransportController(nullptr, config), nullptr);

  config.maxBlockFrames = 512;
  config.maxActiveVoices = 32;
  config.maxSourceChannels = 9;
  EXPECT_EQ(createTransportController(nullptr, config), nullptr);

  config.maxActiveVoices = 33;
  config.maxSourceChannels = 8;
  EXPECT_EQ(createTransportController(nullptr, config), nullptr);
}


TEST(ChannelFormatTest, DistinguishesSMPTE51StereoBundleFromSurround71Bed) {
  const auto programme = ChannelFormat::SMPTE51Stereo();
  const std::array<Speaker, 8> expectedProgramme = {
      Speaker::L,  Speaker::R,  Speaker::C,  Speaker::LFE,
      Speaker::Ls, Speaker::Rs, Speaker::Lo, Speaker::Ro,
  };
  EXPECT_EQ(programme.layout, ChannelLayout::SMPTE_51_ST);
  EXPECT_EQ(programme.num_channels, expectedProgramme.size());
  EXPECT_TRUE(std::equal(expectedProgramme.begin(), expectedProgramme.end(),
                         programme.channel_map.begin()));

  const auto surround = ChannelFormat::Surround71();
  EXPECT_EQ(surround.channel_map[6], Speaker::Lb);
  EXPECT_EQ(surround.channel_map[7], Speaker::Rb);
  EXPECT_NE(programme.channel_map[6], surround.channel_map[6]);
  EXPECT_NE(programme.channel_map[7], surround.channel_map[7]);
}
} // namespace
