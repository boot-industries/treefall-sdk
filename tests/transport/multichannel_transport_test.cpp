// SPDX-License-Identifier: MIT
#include "../../src/core/routing/gain_smoother.h"
#include "../../src/core/routing/routing_matrix.h"
#include "../../src/core/transport/transport_controller.h"
#include <orpheus/channel_format.h>

#include <gtest/gtest.h>

#include "../support/unique_temp_dir.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace orpheus;
namespace orpheus {
class TransportControllerTestAccess {
public:
  static void replaceRoutingMatrix(TransportController& controller,
                                   std::unique_ptr<RoutingMatrix> replacement) {
    replacement->claimChannelRoutesForTransport();
    controller.m_routingMatrix = std::move(replacement);
  }

  // Publishes a route through the transport's internal writer. Tests call it
  // between blocks on the thread that drives processAudio().
  static SessionGraphError routeLane(TransportController& controller, RoutingChannelIndex lane,
                                     RoutingGroupIndex group, RoutingOutputIndex output) {
    return controller.m_routingMatrix->setTransportChannelRoute(lane, group, output);
  }
};
} // namespace orpheus

namespace {

constexpr uint32_t kSampleRate = 48000;
RoutingConfig routingConfigForTransport(const TransportConfig& config) {
  RoutingConfig routing;
  routing.num_channels =
      static_cast<RoutingChannelIndex>(config.maxActiveVoices * config.maxSourceChannels);
  routing.num_groups = static_cast<RoutingGroupIndex>(config.numGroups);
  routing.num_outputs = static_cast<RoutingOutputIndex>(config.outputChannels);
  routing.sample_rate = config.sampleRate;
  routing.gain_smoothing_ms = 0.0f;
  routing.enable_metering = true;
  routing.enable_clipping_protection = false;
  routing.source_channel_policy = config.sourceChannelPolicy;
  routing.downmix_policy = config.sourceChannelPolicy == SourceChannelPolicy::Discrete
                               ? DownmixPolicy::None
                               : DownmixPolicy::ITU_BS775_3;
  return routing;
}

class FailingRoutingMatrix final : public RoutingMatrix {
public:
  SessionGraphError processRouting(const float* const*, float* const*, uint32_t) override {
    return SessionGraphError::InternalError;
  }
};

class LegacyRoutingMatrix final : public RoutingMatrix {
public:
  void copyGroupOutputMeterSnapshot(GroupOutputMeterSnapshot& destination) const noexcept override {
    destination = {};
  }
};
constexpr uint16_t kChannels = 8;
constexpr size_t kFrames = 64;

class MultichannelTransportTest : public ::testing::Test {
protected:
  void SetUp() override {
    filePath =
        orpheus::tests::support::UniqueTempPath("orpheus_multichannel_transport_test", ".wav")
            .string();
    stereoPath =
        orpheus::tests::support::UniqueTempPath("orpheus_stereo_route_test", ".wav").string();
    writeFixture(filePath, kChannels);
    writeFixture(stereoPath, 2);
  }

  void TearDown() override {
    std::error_code error;
    std::filesystem::remove(filePath, error);
    std::filesystem::remove(stereoPath, error);
  }

  void writeFixture(const std::string& path, uint16_t channels, int16_t firstSample = 2048) const {
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
        const int16_t sample = static_cast<int16_t>((channel + 1) * firstSample);
        file.write(reinterpret_cast<const char*>(&sample), sizeof(sample));
      }
    }
    ASSERT_TRUE(file.good());
  }

  // Mono 16-bit file holding kFrames frames at each listed level, so a
  // looping voice rendered in kFrames blocks plays one level per block.
  void writeMonoBlockFixture(const std::string& path, const std::vector<int16_t>& levels) const {
    constexpr uint16_t channels = 1;
    constexpr uint16_t bitsPerSample = 16;
    constexpr uint16_t blockAlign = channels * (bitsPerSample / 8);
    const uint32_t dataSize = static_cast<uint32_t>(levels.size() * kFrames * blockAlign);
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
    for (const int16_t level : levels) {
      for (size_t frame = 0; frame < kFrames; ++frame) {
        file.write(reinterpret_cast<const char*>(&level), sizeof(level));
      }
    }
    ASSERT_TRUE(file.good());
  }

  // One looping mono voice panned into both lanes of a two-lane group 0.
  static std::unique_ptr<ITransportController> startLoopingMonoVoice(const std::string& path,
                                                                     uint32_t decimationBlocks) {
    TransportConfig config;
    config.sampleRate = kSampleRate;
    config.outputChannels = 2;
    config.maxBlockFrames = static_cast<uint32_t>(kFrames);
    config.maxActiveVoices = 1;
    config.maxSourceChannels = 2;
    config.numGroups = 1;
    auto transport = createTransportController(nullptr, config);
    EXPECT_NE(transport, nullptr);
    if (!transport) {
      return transport;
    }
    EXPECT_EQ(transport->registerClipAudio(40, path), SessionGraphError::OK);
    auto metadata = transport->getClipMetadata(40);
    EXPECT_TRUE(metadata.has_value());
    if (metadata) {
      metadata->loopEnabled = true;
      metadata->fadeInSeconds = 0.0;
      metadata->fadeOutSeconds = 0.0;
      EXPECT_EQ(transport->updateClipMetadata(40, *metadata), SessionGraphError::OK);
    }
    EXPECT_EQ(transport->startClip(40), SessionGraphError::OK);
    transport->getRealtimeTelemetry()->setDecimationBlocks(decimationBlocks);
    return transport;
  }

  static void renderBlock(ITransportController& transport) {
    std::array<std::array<float, kFrames>, 2> storage{};
    std::array<float*, 2> outputs = {storage[0].data(), storage[1].data()};
    transport.processAudio(outputs.data(), outputs.size(), kFrames);
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
  std::copy_n(wrongBed.channel_map.begin(), wrongBed.num_channels, metadata->speakerPatch.begin());
  EXPECT_EQ(transport->updateClipMetadata(1, *metadata), SessionGraphError::InvalidParameter);
  std::copy_n(format.channel_map.begin(), format.num_channels, metadata->speakerPatch.begin());
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
  ASSERT_EQ(transport->setGroupOutputBus(3, OutputBusRoute{6, 2}), SessionGraphError::OK);
  EXPECT_EQ(transport->getGroupOutputBus(3), std::optional<OutputBusRoute>(OutputBusRoute{6, 2}));

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

TEST_F(MultichannelTransportTest, MonoVoiceLaneStaysSilentAfterGroupWidthChange) {
  const auto monoPath =
      orpheus::tests::support::UniqueTempPath("orpheus_mono_width_change", ".wav").string();
  writeFixture(monoPath, 1);

  TransportConfig config;
  config.sampleRate = kSampleRate;
  config.outputChannels = 2;
  config.maxBlockFrames = static_cast<uint32_t>(kFrames);
  config.maxActiveVoices = 1;
  config.maxSourceChannels = 2;
  config.numGroups = 1;
  config.sourceChannelPolicy = SourceChannelPolicy::Discrete;
  auto transport = createTransportController(nullptr, config);
  ASSERT_NE(transport, nullptr);
  ASSERT_EQ(transport->registerClipAudio(30, monoPath), SessionGraphError::OK);
  auto metadata = transport->getClipMetadata(30);
  ASSERT_TRUE(metadata.has_value());
  metadata->loopEnabled = true;
  metadata->fadeInSeconds = 0.0;
  metadata->fadeOutSeconds = 0.0;
  ASSERT_EQ(transport->updateClipMetadata(30, *metadata), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(30), SessionGraphError::OK);

  std::array<std::array<float, kFrames>, 2> storage{};
  std::array<float*, 2> outputs = {storage[0].data(), storage[1].data()};
  transport->processAudio(outputs.data(), outputs.size(), kFrames);
  // The group starts two lanes wide, so the mono voice pans into both lanes.
  ASSERT_GT(std::abs(storage[0][kFrames - 1]), 0.01f);
  ASSERT_GT(std::abs(storage[1][kFrames - 1]), 0.01f);

  // Narrowing the live group stops the voice writing its second lane. The lane
  // keeps the route it got at voice start, so the routing matrix still meters
  // it: it must read silence, not keep replaying the last block it rendered.
  ASSERT_EQ(transport->setGroupOutputBus(0, OutputBusRoute{0, 1}), SessionGraphError::OK);
  for (int block = 0; block < 3; ++block) {
    storage = {};
    transport->processAudio(outputs.data(), outputs.size(), kFrames);
    EXPECT_GT(std::abs(storage[0][kFrames - 1]), 0.01f) << "block " << block;
    for (size_t frame = 0; frame < kFrames; ++frame) {
      EXPECT_EQ(storage[1][frame], 0.0f) << "block " << block << " frame " << frame;
    }
    EXPECT_EQ(transport->getRoutingMatrix()->getChannelMeter(1).peak_db, kAudioMeterSilenceDb)
        << "block " << block;
  }

  std::error_code ignored;
  std::filesystem::remove(monoPath, ignored);
}

// Whether a mono voice pans into two lanes follows the group width the routing
// matrix renders, whichever public path set it: setGroupOutputBus() on the
// transport, or setGroupOutputRoute()/applyGroupControlSnapshot() on its
// matrix. Matrix-path transports must render bit for bit like the
// setGroupOutputBus() reference at the same width.
TEST_F(MultichannelTransportTest, MonoVoiceFollowsGroupWidthSetThroughRoutingMatrix) {
  const auto monoPath =
      (std::filesystem::temp_directory_path() / "orpheus_mono_matrix_width.wav").string();
  writeFixture(monoPath, 1);

  using Block = std::array<std::array<float, kFrames>, 2>;
  const auto makeTransport = [&] {
    TransportConfig config;
    config.sampleRate = kSampleRate;
    config.outputChannels = 2;
    config.maxBlockFrames = static_cast<uint32_t>(kFrames);
    config.maxActiveVoices = 1;
    config.maxSourceChannels = 2;
    config.numGroups = 1;
    config.sourceChannelPolicy = SourceChannelPolicy::Discrete;
    auto transport = createTransportController(nullptr, config);
    EXPECT_NE(transport, nullptr);
    EXPECT_EQ(transport->registerClipAudio(31, monoPath), SessionGraphError::OK);
    auto metadata = transport->getClipMetadata(31);
    EXPECT_TRUE(metadata.has_value());
    metadata->loopEnabled = true;
    metadata->fadeInSeconds = 0.0;
    metadata->fadeOutSeconds = 0.0;
    EXPECT_EQ(transport->updateClipMetadata(31, *metadata), SessionGraphError::OK);
    return transport;
  };
  const auto firstBlock = [](ITransportController& transport) {
    Block storage{};
    std::array<float*, 2> outputs = {storage[0].data(), storage[1].data()};
    EXPECT_EQ(transport.startClip(31), SessionGraphError::OK);
    transport.processAudio(outputs.data(), outputs.size(), kFrames);
    return storage;
  };
  const auto withSnapshotWidth = [](ITransportController& transport, uint16_t width) {
    auto* routing = transport.getRoutingMatrix();
    auto desired = routing->getRoutingControlSnapshot();
    desired.groups[0].output_start = 0;
    desired.groups[0].output_width = width;
    return routing->applyGroupControlSnapshot(desired);
  };
  const auto expectSameBlock = [](const Block& actual, const Block& expected, const char* path) {
    for (size_t lane = 0; lane < 2; ++lane) {
      for (size_t frame = 0; frame < kFrames; ++frame) {
        ASSERT_EQ(actual[lane][frame], expected[lane][frame])
            << path << " lane " << lane << " frame " << frame;
      }
    }
  };

  // One lane wide: the voice renders a single unpanned lane.
  auto narrowReference = makeTransport();
  ASSERT_EQ(narrowReference->setGroupOutputBus(0, OutputBusRoute{0, 1}), SessionGraphError::OK);
  const Block narrow = firstBlock(*narrowReference);
  ASSERT_GT(std::abs(narrow[0][kFrames - 1]), 0.01f);
  ASSERT_EQ(narrow[1][kFrames - 1], 0.0f);

  auto narrowBySnapshot = makeTransport();
  ASSERT_EQ(withSnapshotWidth(*narrowBySnapshot, 1), SessionGraphError::OK);
  expectSameBlock(firstBlock(*narrowBySnapshot), narrow, "applyGroupControlSnapshot narrow");

  auto narrowByRoute = makeTransport();
  ASSERT_EQ(narrowByRoute->getRoutingMatrix()->setGroupOutputRoute(0, 0, 1), SessionGraphError::OK);
  expectSameBlock(firstBlock(*narrowByRoute), narrow, "setGroupOutputRoute narrow");

  // Two lanes wide: the voice pans into both lanes.
  auto wideReference = makeTransport();
  const Block wide = firstBlock(*wideReference);
  ASSERT_GT(std::abs(wide[0][kFrames - 1]), 0.01f);
  ASSERT_GT(std::abs(wide[1][kFrames - 1]), 0.01f);

  auto wideBySnapshot = makeTransport();
  ASSERT_EQ(wideBySnapshot->setGroupOutputBus(0, OutputBusRoute{0, 1}), SessionGraphError::OK);
  ASSERT_EQ(withSnapshotWidth(*wideBySnapshot, 2), SessionGraphError::OK);
  expectSameBlock(firstBlock(*wideBySnapshot), wide, "applyGroupControlSnapshot wide");

  auto wideByRoute = makeTransport();
  ASSERT_EQ(wideByRoute->setGroupOutputBus(0, OutputBusRoute{0, 1}), SessionGraphError::OK);
  ASSERT_EQ(wideByRoute->getRoutingMatrix()->setGroupOutputRoute(0, 0, 2), SessionGraphError::OK);
  expectSameBlock(firstBlock(*wideByRoute), wide, "setGroupOutputRoute wide");

  std::error_code ignored;
  std::filesystem::remove(monoPath, ignored);
}

// A voice slot exposes all maxSourceChannels lanes to routing, including the
// lanes beyond the voice's own channel count. Those lanes must read silence,
// not the last block a wider voice rendered into the same slot. Hosts can no
// longer route a transport lane, so the test publishes the route through the
// transport's internal writer to make the lane's content audible.
TEST_F(MultichannelTransportTest, LaneBeyondVoiceChannelsStaysSilentAfterSlotReuse) {
  TransportConfig config;
  config.sampleRate = kSampleRate;
  config.outputChannels = kChannels;
  config.maxBlockFrames = static_cast<uint32_t>(kFrames);
  config.maxActiveVoices = 1;
  config.maxSourceChannels = kChannels;
  config.numGroups = 1;
  config.sourceChannelPolicy = SourceChannelPolicy::Discrete;
  auto transport = createTransportController(nullptr, config);
  ASSERT_NE(transport, nullptr);
  std::array<std::array<float, kFrames>, kChannels> storage{};
  std::array<float*, kChannels> outputs{};
  for (size_t channel = 0; channel < kChannels; ++channel) {
    outputs[channel] = storage[channel].data();
  }
  const auto render = [&] {
    storage = {};
    transport->processAudio(outputs.data(), outputs.size(), kFrames);
  };

  // An 8-channel voice writes every lane of the only voice slot, then ends.
  ASSERT_EQ(transport->registerClipAudio(40, filePath), SessionGraphError::OK);
  auto wide = transport->getClipMetadata(40);
  ASSERT_TRUE(wide.has_value());
  wide->loopEnabled = true;
  wide->fadeInSeconds = 0.0;
  wide->fadeOutSeconds = 0.0;
  const auto format = ChannelFormat::SMPTE51Stereo();
  wide->sourceLayout = format.layout;
  wide->speakerPatchSize = format.num_channels;
  std::copy_n(format.channel_map.begin(), format.num_channels, wide->speakerPatch.begin());
  ASSERT_EQ(transport->updateClipMetadata(40, *wide), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(40), SessionGraphError::OK);
  render();
  ASSERT_GT(std::abs(storage[5][kFrames - 1]), 0.01f);
  ASSERT_EQ(transport->panic(), SessionGraphError::OK);
  render();
  render();

  // A stereo voice reuses the slot; lanes 2..7 are beyond its channel count,
  // so it never writes them and voice routing leaves them unassigned.
  ASSERT_EQ(transport->registerClipAudio(41, stereoPath), SessionGraphError::OK);
  auto narrow = transport->getClipMetadata(41);
  ASSERT_TRUE(narrow.has_value());
  narrow->loopEnabled = true;
  narrow->fadeInSeconds = 0.0;
  narrow->fadeOutSeconds = 0.0;
  ASSERT_EQ(transport->updateClipMetadata(41, *narrow), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(41), SessionGraphError::OK);
  render();

  // Hosts may not edit transport-owned channel routes.
  auto* routing = transport->getRoutingMatrix();
  ASSERT_NE(routing, nullptr);
  EXPECT_EQ(routing->setChannelRoute(5, 0, 5), SessionGraphError::NotSupported);

  // Route lane 5 to output 5 so whatever the lane holds becomes audible.
  auto* controller = dynamic_cast<TransportController*>(transport.get());
  ASSERT_NE(controller, nullptr);
  ASSERT_EQ(TransportControllerTestAccess::routeLane(*controller, 5, 0, 5), SessionGraphError::OK);
  for (int block = 0; block < 4; ++block) {
    render();
    EXPECT_GT(std::abs(storage[0][kFrames - 1]), 0.01f) << "block " << block;
    for (size_t frame = 0; frame < kFrames; ++frame) {
      ASSERT_EQ(storage[5][frame], 0.0f) << "block " << block << " frame " << frame;
    }
  }
}

TEST(MultichannelTransportConfigTest, RejectsUnsupportedRealtimeBounds) {
  TransportConfig config;
  config.sampleRate = kSampleRate;

  config.outputChannels = static_cast<uint32_t>(kRoutingMaxOutputs + 1);
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
TEST_F(MultichannelTransportTest, PublishesIndependentLogicalGroupOutputTelemetry) {
  const auto group0Path =
      orpheus::tests::support::UniqueTempPath("orpheus_meter_group0", ".wav").string();
  const auto group1Path =
      orpheus::tests::support::UniqueTempPath("orpheus_meter_group1", ".wav").string();
  writeFixture(group0Path, 1, 2048);
  writeFixture(group1Path, 1, 4096);

  TransportConfig config;
  config.sampleRate = kSampleRate;
  config.outputChannels = 2;
  config.maxBlockFrames = static_cast<uint32_t>(kFrames);
  config.maxActiveVoices = 2;
  config.maxSourceChannels = 1;
  config.numGroups = 2;
  config.sourceChannelPolicy = SourceChannelPolicy::Discrete;
  auto transport = createTransportController(nullptr, config);
  ASSERT_NE(transport, nullptr);
  ASSERT_EQ(transport->setGroupOutputBus(0, OutputBusRoute{0, 1}), SessionGraphError::OK);
  ASSERT_EQ(transport->setGroupOutputBus(1, OutputBusRoute{0, 1}), SessionGraphError::OK);
  ASSERT_EQ(transport->registerClipAudio(10, group0Path), SessionGraphError::OK);
  ASSERT_EQ(transport->registerClipAudio(11, group1Path), SessionGraphError::OK);

  auto group0Metadata = transport->getClipMetadata(10);
  auto group1Metadata = transport->getClipMetadata(11);
  ASSERT_TRUE(group0Metadata.has_value());
  ASSERT_TRUE(group1Metadata.has_value());
  group0Metadata->routingGroup = 0;
  group0Metadata->loopEnabled = true;
  group1Metadata->routingGroup = 1;
  group1Metadata->loopEnabled = true;
  ASSERT_EQ(transport->updateClipMetadata(10, *group0Metadata), SessionGraphError::OK);
  ASSERT_EQ(transport->updateClipMetadata(11, *group1Metadata), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(10), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(11), SessionGraphError::OK);

  auto* telemetry = transport->getRealtimeTelemetry();
  ASSERT_NE(telemetry, nullptr);
  std::vector<std::vector<float>> outputStorage(config.outputChannels,
                                                std::vector<float>(kFrames, 0.0f));
  std::vector<float*> outputs;
  for (auto& lane : outputStorage) {
    outputs.push_back(lane.data());
  }
  transport->processAudio(outputs.data(), outputs.size(), kFrames);

  RealtimeTelemetrySnapshot snapshot;
  ASSERT_TRUE(telemetry->tryRead(snapshot));
  ASSERT_EQ(snapshot.routing_meters.availability, MeterAvailability::Measured);
  ASSERT_EQ(snapshot.routing_meters.group_output_meters.availability, MeterAvailability::Measured);
  ASSERT_EQ(snapshot.routing_meters.group_output_meters.coherent, 1);
  ASSERT_EQ(snapshot.routing_meters.group_output_meters.group_count, 2);
  const auto& group0 = snapshot.routing_meters.group_output_meters.groups[0];
  const auto& group1 = snapshot.routing_meters.group_output_meters.groups[1];
  ASSERT_EQ(group0.logical_lane_count, 1);
  ASSERT_EQ(group1.logical_lane_count, 1);
  ASSERT_EQ(group0.availability, MeterAvailability::Measured);
  ASSERT_EQ(group1.availability, MeterAvailability::Measured);
  EXPECT_LT(group0.lane_meters[0].peak_db, group1.lane_meters[0].peak_db);
  EXPECT_GT(snapshot.routing_meters.post_master_output_lane_meters[0].peak_db,
            group1.lane_meters[0].peak_db);
  EXPECT_EQ(snapshot.routing_meters.peak_window_frames, kFrames);
  EXPECT_EQ(snapshot.routing_meters.rms_window_frames, kFrames);

  std::error_code ignored;
  std::filesystem::remove(group0Path, ignored);
  std::filesystem::remove(group1Path, ignored);
}

TEST_F(MultichannelTransportTest, RetainsPeakAcrossTelemetryDecimationAndDrop) {
  TransportConfig config;
  config.sampleRate = kSampleRate;
  config.outputChannels = 2;
  config.maxBlockFrames = static_cast<uint32_t>(kFrames);
  config.maxActiveVoices = 1;
  config.maxSourceChannels = 2;
  auto transport = createTransportController(nullptr, config);
  ASSERT_NE(transport, nullptr);
  ASSERT_EQ(transport->registerClipAudio(20, stereoPath), SessionGraphError::OK);
  auto metadata = transport->getClipMetadata(20);
  ASSERT_TRUE(metadata.has_value());
  metadata->loopEnabled = true;
  ASSERT_EQ(transport->updateClipMetadata(20, *metadata), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(20), SessionGraphError::OK);

  auto* telemetry = transport->getRealtimeTelemetry();
  ASSERT_NE(telemetry, nullptr);
  telemetry->setDecimationBlocks(1);
  std::vector<std::vector<float>> outputStorage(config.outputChannels,
                                                std::vector<float>(kFrames, 0.0f));
  std::vector<float*> outputs;
  for (auto& lane : outputStorage) {
    outputs.push_back(lane.data());
  }

  for (size_t block = 0; block < kRealtimeTelemetryCapacity; ++block) {
    transport->processAudio(outputs.data(), outputs.size(), kFrames);
  }
  transport->processAudio(outputs.data(), outputs.size(), kFrames);
  transport->processAudio(outputs.data(), outputs.size(), kFrames);
  EXPECT_EQ(telemetry->pendingSnapshotCount(), kRealtimeTelemetryCapacity);
  EXPECT_EQ(telemetry->droppedSnapshotCount(), 2u);

  RealtimeTelemetrySnapshot oldSnapshot;
  uint64_t lastOldSequence = 0;
  size_t drained = 0;
  while (telemetry->tryRead(oldSnapshot)) {
    lastOldSequence = oldSnapshot.sequence;
    ++drained;
  }
  EXPECT_EQ(drained, kRealtimeTelemetryCapacity);
  EXPECT_EQ(lastOldSequence, kRealtimeTelemetryCapacity);

  transport->processAudio(outputs.data(), outputs.size(), kFrames);
  RealtimeTelemetrySnapshot recovered;
  ASSERT_TRUE(telemetry->tryRead(recovered));
  EXPECT_EQ(recovered.sequence, kRealtimeTelemetryCapacity + 3);
  EXPECT_GT(recovered.routing_meters.master_aggregate_meter.peak_db, kAudioMeterSilenceDb);
  EXPECT_EQ(recovered.routing_meters.peak_window_frames, 3 * kFrames);
  EXPECT_FALSE(telemetry->tryRead(recovered));
}

TEST(MultichannelTransportMeteringTest, CanonicalAvailabilityMarksRoutingFailure) {
  TransportConfig config;
  config.sampleRate = kSampleRate;
  config.outputChannels = 2;
  config.maxBlockFrames = static_cast<uint32_t>(kFrames);
  config.maxActiveVoices = 1;
  config.maxSourceChannels = 1;
  config.numGroups = 1;
  auto transport = createTransportController(nullptr, config);
  ASSERT_NE(transport, nullptr);
  auto* controller = dynamic_cast<TransportController*>(transport.get());
  ASSERT_NE(controller, nullptr);
  auto failing = std::make_unique<FailingRoutingMatrix>();
  ASSERT_EQ(failing->initialize(routingConfigForTransport(config)), SessionGraphError::OK);
  TransportControllerTestAccess::replaceRoutingMatrix(*controller, std::move(failing));

  auto* telemetry = transport->getRealtimeTelemetry();
  telemetry->setDecimationBlocks(1);
  std::vector<std::vector<float>> outputStorage(config.outputChannels,
                                                std::vector<float>(kFrames, 0.0f));
  std::vector<float*> outputs;
  for (auto& lane : outputStorage) {
    outputs.push_back(lane.data());
  }
  transport->processAudio(outputs.data(), outputs.size(), kFrames);

  RealtimeTelemetrySnapshot snapshot;
  ASSERT_TRUE(telemetry->tryRead(snapshot));
  EXPECT_EQ(snapshot.routing_meters.availability, MeterAvailability::Unmeasured);
  EXPECT_EQ(snapshot.routing_meters.peak_window_frames, 0u);
  EXPECT_EQ(snapshot.routing_meters.rms_window_frames, 0u);
  EXPECT_EQ(snapshot.routing_meters.group_aggregate_availability[0], MeterAvailability::Unmeasured);
  EXPECT_EQ(snapshot.routing_meters.master_aggregate_availability, MeterAvailability::Unmeasured);
  EXPECT_EQ(snapshot.routing_meters.post_master_output_availability[0],
            MeterAvailability::Unmeasured);
  EXPECT_EQ(snapshot.group_count, 1u);
}

TEST(MultichannelTransportMeteringTest,
     UnsupportedLogicalExtensionKeepsCanonicalAggregatesMeasured) {
  TransportConfig config;
  config.sampleRate = kSampleRate;
  config.outputChannels = 2;
  config.maxBlockFrames = static_cast<uint32_t>(kFrames);
  config.maxActiveVoices = 1;
  config.maxSourceChannels = 1;
  config.numGroups = 1;
  auto transport = createTransportController(nullptr, config);
  ASSERT_NE(transport, nullptr);
  auto* controller = dynamic_cast<TransportController*>(transport.get());
  ASSERT_NE(controller, nullptr);
  auto legacy = std::make_unique<LegacyRoutingMatrix>();
  ASSERT_EQ(legacy->initialize(routingConfigForTransport(config)), SessionGraphError::OK);
  TransportControllerTestAccess::replaceRoutingMatrix(*controller, std::move(legacy));

  auto* telemetry = transport->getRealtimeTelemetry();
  telemetry->setDecimationBlocks(1);
  std::vector<std::vector<float>> outputStorage(config.outputChannels,
                                                std::vector<float>(kFrames, 0.0f));
  std::vector<float*> outputs;
  for (auto& lane : outputStorage) {
    outputs.push_back(lane.data());
  }
  transport->processAudio(outputs.data(), outputs.size(), kFrames);

  RealtimeTelemetrySnapshot snapshot;
  ASSERT_TRUE(telemetry->tryRead(snapshot));
  EXPECT_EQ(snapshot.routing_meters.availability, MeterAvailability::Measured);
  EXPECT_EQ(snapshot.routing_meters.group_aggregate_availability[0], MeterAvailability::Measured);
  EXPECT_EQ(snapshot.routing_meters.master_aggregate_availability, MeterAvailability::Measured);
  EXPECT_EQ(snapshot.routing_meters.post_master_output_availability[0],
            MeterAvailability::Measured);
  EXPECT_EQ(snapshot.routing_meters.group_output_meters.availability,
            MeterAvailability::Unsupported);
  EXPECT_EQ(snapshot.routing_meters.group_output_meters.coherent, 0);
}

TEST_F(MultichannelTransportTest, CanonicalFrameStampsSchemas) {
  TransportConfig config;
  config.sampleRate = kSampleRate;
  config.outputChannels = 2;
  config.maxBlockFrames = static_cast<uint32_t>(kFrames);
  config.maxActiveVoices = 1;
  config.maxSourceChannels = 1;
  auto transport = createTransportController(nullptr, config);
  ASSERT_NE(transport, nullptr);
  auto* telemetry = transport->getRealtimeTelemetry();
  ASSERT_NE(telemetry, nullptr);
  telemetry->setDecimationBlocks(1);
  std::array<std::array<float, kFrames>, 2> storage{};
  std::array<float*, 2> outputs = {storage[0].data(), storage[1].data()};
  transport->processAudio(outputs.data(), outputs.size(), kFrames);

  RealtimeTelemetrySnapshot snapshot;
  ASSERT_TRUE(telemetry->tryRead(snapshot));
  EXPECT_EQ(snapshot.schema_version, kRealtimeTelemetrySchemaVersion);
  EXPECT_EQ(snapshot.routing_meters.schema_version, kRoutingMeterTelemetrySchemaVersion);
  EXPECT_EQ(snapshot.routing_meters.group_output_meters.schema_version,
            kGroupOutputMeterSnapshotSchemaVersion);
}

TEST_F(MultichannelTransportTest, TopologyShrinkClearsLanesBeyondLogicalCount) {
  const auto path =
      orpheus::tests::support::UniqueTempPath("orpheus_meter_topology_shrink", ".wav").string();
  writeMonoBlockFixture(path, {8192});
  auto transport = startLoopingMonoVoice(path, 4);
  ASSERT_NE(transport, nullptr);
  auto* telemetry = transport->getRealtimeTelemetry();

  for (int block = 0; block < 4; ++block) {
    renderBlock(*transport);
  }
  auto wide = std::make_unique<RealtimeTelemetrySnapshot>();
  ASSERT_TRUE(telemetry->tryRead(*wide));
  ASSERT_EQ(wide->routing_meters.group_output_meters.groups[0].logical_lane_count, 2);
  ASSERT_GT(wide->routing_meters.group_output_meters.groups[0].lane_meters[1].peak_db,
            kAudioMeterSilenceDb);

  // Shrink the group in the middle of a decimation window, after two blocks
  // have already measured its second lane.
  renderBlock(*transport);
  renderBlock(*transport);
  ASSERT_EQ(transport->setGroupOutputBus(0, OutputBusRoute{0, 1}), SessionGraphError::OK);
  renderBlock(*transport);
  renderBlock(*transport);

  auto narrow = std::make_unique<RealtimeTelemetrySnapshot>();
  ASSERT_TRUE(telemetry->tryRead(*narrow));
  const auto& frame = narrow->routing_meters.group_output_meters.groups[0];
  EXPECT_EQ(frame.logical_lane_count, 1);
  EXPECT_GT(frame.lane_meters[0].peak_db, kAudioMeterSilenceDb);
  const AudioMeter unconfigured{};
  EXPECT_EQ(frame.lane_meters[1].peak_db, unconfigured.peak_db);
  EXPECT_EQ(frame.lane_meters[1].rms_db, unconfigured.rms_db);
  EXPECT_EQ(frame.lane_meters[1].clipping, unconfigured.clipping);
  EXPECT_EQ(frame.lane_meters[1].clip_count, unconfigured.clip_count);

  std::error_code ignored;
  std::filesystem::remove(path, ignored);
}

TEST_F(MultichannelTransportTest, DecimationMergeMatchesBlockMaxima) {
  const auto path =
      orpheus::tests::support::UniqueTempPath("orpheus_meter_alternating", ".wav").string();
  writeMonoBlockFixture(path, {16384, 2048, 16384, 2048});
  auto reference = startLoopingMonoVoice(path, 1);
  auto decimated = startLoopingMonoVoice(path, 4);
  ASSERT_NE(reference, nullptr);
  ASSERT_NE(decimated, nullptr);

  // Loud, quiet, loud, quiet: one per-block reference snapshot each.
  std::vector<RealtimeTelemetrySnapshot> blocks(4);
  for (auto& block : blocks) {
    renderBlock(*reference);
    ASSERT_TRUE(reference->getRealtimeTelemetry()->tryRead(block));
    renderBlock(*decimated);
  }
  auto window = std::make_unique<RealtimeTelemetrySnapshot>();
  ASSERT_TRUE(decimated->getRealtimeTelemetry()->tryRead(*window));
  EXPECT_FALSE(decimated->getRealtimeTelemetry()->tryRead(*window));
  const RoutingMeterTelemetry& meters = window->routing_meters;
  EXPECT_EQ(meters.availability, MeterAvailability::Measured);
  EXPECT_EQ(meters.peak_window_frames, 4 * kFrames);
  EXPECT_EQ(meters.rms_window_frames, kFrames);

  const auto expectWindow = [&](const AudioMeter& actual, auto&& perBlock, const char* name) {
    float peak = kAudioMeterSilenceDb;
    uint32_t clipCount = 0;
    for (const auto& block : blocks) {
      peak = std::max(peak, perBlock(block).peak_db);
      clipCount = std::max(clipCount, perBlock(block).clip_count);
    }
    EXPECT_GT(peak, perBlock(blocks.back()).peak_db) << name;
    EXPECT_EQ(actual.peak_db, peak) << name;
    EXPECT_EQ(actual.rms_db, perBlock(blocks.back()).rms_db) << name;
    EXPECT_EQ(actual.clip_count, clipCount) << name;
  };
  for (size_t lane = 0; lane < 2; ++lane) {
    EXPECT_EQ(meters.group_output_meters.groups[0].availability, MeterAvailability::Measured);
    expectWindow(
        meters.group_output_meters.groups[0].lane_meters[lane],
        [lane](const RealtimeTelemetrySnapshot& block) -> const AudioMeter& {
          return block.routing_meters.group_output_meters.groups[0].lane_meters[lane];
        },
        lane == 0 ? "group 0 lane 0" : "group 0 lane 1");
  }
  expectWindow(
      meters.master_aggregate_meter,
      [](const RealtimeTelemetrySnapshot& block) -> const AudioMeter& {
        return block.routing_meters.master_aggregate_meter;
      },
      "master aggregate");

  std::error_code ignored;
  std::filesystem::remove(path, ignored);
}

TEST_F(MultichannelTransportTest, PostPublishWindowRestarts) {
  const auto path =
      orpheus::tests::support::UniqueTempPath("orpheus_meter_window_restart", ".wav").string();
  writeMonoBlockFixture(path, {8192});
  auto transport = startLoopingMonoVoice(path, 1);
  ASSERT_NE(transport, nullptr);
  auto* telemetry = transport->getRealtimeTelemetry();
  auto snapshot = std::make_unique<RealtimeTelemetrySnapshot>();

  renderBlock(*transport);
  ASSERT_TRUE(telemetry->tryRead(*snapshot));
  renderBlock(*transport);
  ASSERT_TRUE(telemetry->tryRead(*snapshot));
  EXPECT_EQ(snapshot->routing_meters.availability, MeterAvailability::Measured);
  EXPECT_EQ(snapshot->routing_meters.peak_window_frames, kFrames);
  EXPECT_EQ(snapshot->routing_meters.rms_window_frames, kFrames);
  EXPECT_EQ(snapshot->routing_meters.group_aggregate_availability[0], MeterAvailability::Measured);
  EXPECT_EQ(snapshot->routing_meters.group_output_meters.availability, MeterAvailability::Measured);
  EXPECT_GT(snapshot->routing_meters.group_output_meters.groups[0].lane_meters[1].peak_db,
            kAudioMeterSilenceDb);

  // A zero-frame block measures nothing: the published window is a full reset,
  // not the previous window's payload under Unmeasured marks.
  std::array<std::array<float, kFrames>, 2> storage{};
  std::array<float*, 2> outputs = {storage[0].data(), storage[1].data()};
  transport->processAudio(outputs.data(), outputs.size(), 0);
  ASSERT_TRUE(telemetry->tryRead(*snapshot));
  EXPECT_EQ(snapshot->routing_meters.availability, MeterAvailability::Unmeasured);
  EXPECT_EQ(snapshot->routing_meters.peak_window_frames, 0u);
  EXPECT_EQ(snapshot->routing_meters.rms_window_frames, 0u);
  EXPECT_EQ(snapshot->routing_meters.post_master_output_count, 0u);
  EXPECT_EQ(snapshot->routing_meters.group_output_meters.group_count, 0u);
  EXPECT_EQ(snapshot->routing_meters.group_output_meters.groups[0].availability,
            MeterAvailability::Unconfigured);
  EXPECT_EQ(snapshot->routing_meters.group_output_meters.groups[0].lane_meters[1].peak_db,
            kAudioMeterSilenceDb);

  renderBlock(*transport);
  ASSERT_TRUE(telemetry->tryRead(*snapshot));
  EXPECT_EQ(snapshot->routing_meters.availability, MeterAvailability::Measured);
  EXPECT_EQ(snapshot->routing_meters.peak_window_frames, kFrames);

  auto* controller = dynamic_cast<TransportController*>(transport.get());
  ASSERT_NE(controller, nullptr);
  TransportConfig config;
  config.sampleRate = kSampleRate;
  config.outputChannels = 2;
  config.maxBlockFrames = static_cast<uint32_t>(kFrames);
  config.maxActiveVoices = 1;
  config.maxSourceChannels = 2;
  config.numGroups = 1;
  auto failing = std::make_unique<FailingRoutingMatrix>();
  ASSERT_EQ(failing->initialize(routingConfigForTransport(config)), SessionGraphError::OK);
  TransportControllerTestAccess::replaceRoutingMatrix(*controller, std::move(failing));
  renderBlock(*transport);
  ASSERT_TRUE(telemetry->tryRead(*snapshot));
  EXPECT_EQ(snapshot->routing_meters.availability, MeterAvailability::Unmeasured);
  EXPECT_EQ(snapshot->routing_meters.peak_window_frames, 0u);
  EXPECT_EQ(snapshot->routing_meters.rms_window_frames, 0u);
  EXPECT_EQ(snapshot->routing_meters.group_aggregate_availability[0],
            MeterAvailability::Unmeasured);
  EXPECT_EQ(snapshot->routing_meters.group_output_meters.availability,
            MeterAvailability::Unmeasured);

  std::error_code ignored;
  std::filesystem::remove(path, ignored);
}
} // namespace
