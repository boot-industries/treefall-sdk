// SPDX-License-Identifier: MIT
#include "../../src/core/transport/transport_controller.h"
#include "../../src/core/routing/gain_smoother.h"
#include "../../src/core/routing/routing_matrix.h"
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
namespace orpheus {
class TransportControllerTestAccess {
public:
  static void replaceRoutingMatrix(TransportController& controller,
                                   std::unique_ptr<IRoutingMatrix> replacement) {
    controller.m_routingMatrix = std::move(replacement);
  }
};
} // namespace orpheus

namespace {

constexpr uint32_t kSampleRate = 48000;
RoutingConfig routingConfigForTransport(const TransportConfig& config) {
  RoutingConfig routing;
  routing.num_channels = static_cast<RoutingChannelIndex>(
      config.maxActiveVoices * config.maxSourceChannels);
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
  SessionGraphError processRouting(const float* const*, float* const*,
                                   uint32_t) override {
    return SessionGraphError::InternalError;
  }
};

class LegacyRoutingMatrix final : public RoutingMatrix {
public:
  void copyGroupOutputMeterSnapshot(
      GroupOutputMeterSnapshot& destination) const noexcept override {
    destination = {};
  }
};
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

  void writeFixture(const std::string& path, uint16_t channels,
                    int16_t firstSample = 2048) const {
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
        const int16_t sample =
            static_cast<int16_t>((channel + 1) * firstSample);
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
TEST_F(MultichannelTransportTest, PublishesIndependentLogicalGroupOutputTelemetry) {
  const auto group0Path =
      (std::filesystem::temp_directory_path() / "orpheus_meter_group0.wav").string();
  const auto group1Path =
      (std::filesystem::temp_directory_path() / "orpheus_meter_group1.wav").string();
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
  ASSERT_EQ(transport->setGroupOutputBus(0, OutputBusRoute{0, 1}),
            SessionGraphError::OK);
  ASSERT_EQ(transport->setGroupOutputBus(1, OutputBusRoute{0, 1}),
            SessionGraphError::OK);
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
  ASSERT_EQ(transport->updateClipMetadata(10, *group0Metadata),
            SessionGraphError::OK);
  ASSERT_EQ(transport->updateClipMetadata(11, *group1Metadata),
            SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(10), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(11), SessionGraphError::OK);

  auto* telemetry = transport->getRealtimeTelemetry();
  ASSERT_NE(telemetry, nullptr);
  telemetry->setDecimationBlocks(1);
  std::vector<std::vector<float>> outputStorage(
      config.outputChannels, std::vector<float>(kFrames, 0.0f));
  std::vector<float*> outputs;
  for (auto& lane : outputStorage) {
    outputs.push_back(lane.data());
  }
  transport->processAudio(outputs.data(), outputs.size(), kFrames);

  RealtimeTelemetrySnapshot snapshot;
  ASSERT_TRUE(telemetry->tryRead(snapshot));
  ASSERT_EQ(snapshot.routing_meters.availability, MeterAvailability::Measured);
  ASSERT_EQ(snapshot.routing_meters.group_output_meters.availability,
            MeterAvailability::Measured);
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
  std::vector<std::vector<float>> outputStorage(
      config.outputChannels, std::vector<float>(kFrames, 0.0f));
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
  EXPECT_GT(recovered.routing_meters.master_aggregate_meter.peak_db,
            kAudioMeterSilenceDb);
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
  ASSERT_EQ(failing->initialize(routingConfigForTransport(config)),
            SessionGraphError::OK);
  TransportControllerTestAccess::replaceRoutingMatrix(*controller, std::move(failing));

  auto* telemetry = transport->getRealtimeTelemetry();
  telemetry->setDecimationBlocks(1);
  std::vector<std::vector<float>> outputStorage(
      config.outputChannels, std::vector<float>(kFrames, 0.0f));
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
  EXPECT_EQ(snapshot.routing_meters.group_aggregate_availability[0],
            MeterAvailability::Unmeasured);
  EXPECT_EQ(snapshot.routing_meters.master_aggregate_availability,
            MeterAvailability::Unmeasured);
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
  ASSERT_EQ(legacy->initialize(routingConfigForTransport(config)),
            SessionGraphError::OK);
  TransportControllerTestAccess::replaceRoutingMatrix(*controller, std::move(legacy));

  auto* telemetry = transport->getRealtimeTelemetry();
  telemetry->setDecimationBlocks(1);
  std::vector<std::vector<float>> outputStorage(
      config.outputChannels, std::vector<float>(kFrames, 0.0f));
  std::vector<float*> outputs;
  for (auto& lane : outputStorage) {
    outputs.push_back(lane.data());
  }
  transport->processAudio(outputs.data(), outputs.size(), kFrames);

  RealtimeTelemetrySnapshot snapshot;
  ASSERT_TRUE(telemetry->tryRead(snapshot));
  EXPECT_EQ(snapshot.routing_meters.availability, MeterAvailability::Measured);
  EXPECT_EQ(snapshot.routing_meters.group_aggregate_availability[0],
            MeterAvailability::Measured);
  EXPECT_EQ(snapshot.routing_meters.master_aggregate_availability,
            MeterAvailability::Measured);
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
  EXPECT_EQ(snapshot.routing_meters.schema_version,
            kRoutingMeterTelemetrySchemaVersion);
  EXPECT_EQ(snapshot.routing_meters.group_output_meters.schema_version,
            kGroupOutputMeterSnapshotSchemaVersion);
}
} // namespace
