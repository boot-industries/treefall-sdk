// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>
#include <orpheus/transport_controller.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace orpheus;

constexpr uint32_t kSampleRate = 48000;
constexpr size_t kBlockFrames = 64;
constexpr double kPi = 3.14159265358979323846;

std::string writeStereoSineWav(const std::filesystem::path& path, float frequency) {
  constexpr uint16_t channels = 2;
  constexpr uint16_t bitsPerSample = 16;
  constexpr uint32_t durationFrames = kSampleRate * 2;
  const uint16_t blockAlign = channels * (bitsPerSample / 8);
  const uint32_t byteRate = kSampleRate * blockAlign;
  const uint32_t dataSize = durationFrames * blockAlign;
  const uint32_t riffSize = 36 + dataSize;
  const uint32_t fmtSize = 16;
  const uint16_t pcmFormat = 1;

  std::ofstream file(path, std::ios::binary);
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

  for (uint32_t frame = 0; frame < durationFrames; ++frame) {
    const float phase = static_cast<float>(2.0 * kPi * frequency * frame / kSampleRate);
    const int16_t sample = static_cast<int16_t>(std::sin(phase) * 12000.0f);
    file.write(reinterpret_cast<const char*>(&sample), sizeof(sample));
    file.write(reinterpret_cast<const char*>(&sample), sizeof(sample));
  }
  return path.string();
}

struct CallbackEvent {
  enum class Type { Started, Stopped, Refused };
  Type type;
  ClipHandle handle;
};

class RecordingCallback final : public ITransportCallback {
public:
  void onClipStarted(ClipHandle handle, uint32_t, TransportPosition) override {
    events.push_back({CallbackEvent::Type::Started, handle});
  }
  void onClipStopped(ClipHandle handle, uint32_t, TransportPosition) override {
    events.push_back({CallbackEvent::Type::Stopped, handle});
  }
  void onClipLooped(ClipHandle, uint32_t, TransportPosition) override {}
  void onBufferUnderrun(TransportPosition) override {}
  void onActiveClipLimitReached(ClipHandle handle, TransportPosition) override {
    events.push_back({CallbackEvent::Type::Refused, handle});
  }

  std::vector<CallbackEvent> events;
};

class AtomicGroupChokeTest : public ::testing::Test {
protected:
  void SetUp() override {
    directory = std::filesystem::temp_directory_path() / "orp150_atomic_group_choke";
    std::filesystem::create_directories(directory);
    createTransport(8);
  }

  void TearDown() override {
    transport.reset();
    std::error_code error;
    std::filesystem::remove_all(directory, error);
  }

  void createTransport(uint32_t maxActiveVoices) {
    TransportConfig config;
    config.sampleRate = kSampleRate;
    config.outputChannels = 4;
    config.maxBlockFrames = 256;
    config.maxActiveVoices = maxActiveVoices;
    config.numGroups = 2;
    config.maxSourceChannels = 2;
    transport = createTransportController(nullptr, config);
    ASSERT_NE(transport, nullptr);
    ASSERT_EQ(transport->setGroupOutputBus(0, OutputBusRoute{0, 2}), SessionGraphError::OK);
    ASSERT_EQ(transport->setGroupOutputBus(1, OutputBusRoute{2, 2}), SessionGraphError::OK);
    transport->setCallback(&callback);
  }

  void registerClip(ClipHandle handle, RoutingGroupIndex group,
                    VoiceMode mode = VoiceMode::Polyphonic, double stopFadeSeconds = 0.02) {
    const std::string file =
        writeStereoSineWav(directory / ("clip-" + std::to_string(handle) + ".wav"),
                           180.0f + static_cast<float>(handle) * 37.0f);
    ASSERT_EQ(transport->registerClipAudio(handle, file), SessionGraphError::OK);
    auto metadata = transport->getClipMetadata(handle);
    ASSERT_TRUE(metadata.has_value());
    metadata->routingGroup = group;
    metadata->voiceMode = mode;
    metadata->fadeOutSeconds = stopFadeSeconds;
    ASSERT_EQ(transport->updateClipMetadata(handle, *metadata), SessionGraphError::OK);
  }

  std::array<double, 4> render(size_t blocks = 1) {
    std::array<double, 4> energy{};
    std::array<std::vector<float>, 4> channels;
    std::array<float*, 4> pointers{};
    for (size_t channel = 0; channel < channels.size(); ++channel) {
      channels[channel].resize(kBlockFrames);
      pointers[channel] = channels[channel].data();
    }
    for (size_t block = 0; block < blocks; ++block) {
      for (auto& channel : channels) {
        std::fill(channel.begin(), channel.end(), 0.0f);
      }
      transport->processAudio(pointers.data(), pointers.size(), kBlockFrames);
      for (size_t channel = 0; channel < channels.size(); ++channel) {
        for (float sample : channels[channel]) {
          energy[channel] += std::abs(sample);
        }
      }
    }
    return energy;
  }

  void drainSetupCommands() {
    render();
    transport->processCallbacks();
    callback.events.clear();
  }

  std::filesystem::path directory;
  std::unique_ptr<ITransportController> transport;
  RecordingCallback callback;
};

TEST_F(AtomicGroupChokeTest, AcceptedStartChokesAllAndOnlyRegisteredGroupPeers) {
  registerClip(1, 0);
  registerClip(2, 0);
  registerClip(3, 1);
  registerClip(4, 0, VoiceMode::MonoWithFadeOverlap);
  drainSetupCommands();

  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(2), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(3), SessionGraphError::OK);
  render();
  transport->processCallbacks();
  callback.events.clear();

  ASSERT_EQ(transport->startClipWithGroupChoke(4), SessionGraphError::OK);
  const auto energy = render();
  transport->processCallbacks();

  EXPECT_EQ(transport->getClipState(1), PlaybackState::Stopping);
  EXPECT_EQ(transport->getClipState(2), PlaybackState::Stopping);
  EXPECT_EQ(transport->getClipState(3), PlaybackState::Playing);
  EXPECT_EQ(transport->getClipState(4), PlaybackState::Playing);
  EXPECT_GT(energy[2] + energy[3], 0.0) << "the other group must remain audible";

  ASSERT_FALSE(callback.events.empty());
  EXPECT_EQ(callback.events.front().type, CallbackEvent::Type::Started);
  EXPECT_EQ(callback.events.front().handle, 4u);

  render(20);
  transport->processCallbacks();
  ASSERT_GE(callback.events.size(), 3u);
  std::set<ClipHandle> stopped;
  for (size_t index = 1; index < callback.events.size(); ++index) {
    EXPECT_NE(callback.events[index].type, CallbackEvent::Type::Started);
    if (callback.events[index].type == CallbackEvent::Type::Stopped) {
      stopped.insert(callback.events[index].handle);
    }
  }
  EXPECT_EQ(stopped, (std::set<ClipHandle>{1, 2}));
  EXPECT_EQ(transport->getClipState(3), PlaybackState::Playing);
  EXPECT_EQ(transport->getClipState(4), PlaybackState::Playing);
}

TEST_F(AtomicGroupChokeTest, QueueFullRejectionNeverChangesPeersAndIsRepeatable) {
  registerClip(1, 0);
  registerClip(2, 1);
  registerClip(3, 0);
  drainSetupCommands();

  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(2), SessionGraphError::OK);
  render();
  transport->processCallbacks();

  for (int cycle = 0; cycle < 3; ++cycle) {
    for (size_t command = 0; command < 255; ++command) {
      ASSERT_EQ(transport->stopClip(1000 + command), SessionGraphError::OK)
          << "cycle " << cycle << ", command " << command;
    }
    EXPECT_EQ(transport->startClipWithGroupChoke(3), SessionGraphError::InternalError)
        << "cycle " << cycle;

    render();
    EXPECT_EQ(transport->getClipState(1), PlaybackState::Playing);
    EXPECT_EQ(transport->getClipState(2), PlaybackState::Playing);
    EXPECT_EQ(transport->getClipState(3), PlaybackState::Stopped);
  }
}

TEST_F(AtomicGroupChokeTest, StopOthersModeUsesOneAtomicQueueAdmission) {
  registerClip(1, 0);
  registerClip(2, 1);
  registerClip(3, 0);
  drainSetupCommands();

  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(2), SessionGraphError::OK);
  render();
  transport->processCallbacks();
  callback.events.clear();

  ASSERT_EQ(transport->setClipStopOthersMode(3, true), SessionGraphError::OK);
  for (size_t command = 0; command < 254; ++command) {
    ASSERT_EQ(transport->stopClip(1000 + command), SessionGraphError::OK);
  }
  // One slot remains: the single StartWithStopOthers admission must fit.
  EXPECT_EQ(transport->startClip(3), SessionGraphError::OK);

  render();
  EXPECT_EQ(transport->getClipState(1), PlaybackState::Stopping);
  EXPECT_EQ(transport->getClipState(2), PlaybackState::Stopping);
  EXPECT_EQ(transport->getClipState(3), PlaybackState::Playing);
}

TEST_F(AtomicGroupChokeTest, MetadataQueueRejectionKeepsPersistentAndActiveGroupsAligned) {
  registerClip(1, 0);
  registerClip(2, 1);
  drainSetupCommands();

  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  render();

  auto rejectedMetadata = transport->getClipMetadata(1);
  ASSERT_TRUE(rejectedMetadata.has_value());
  rejectedMetadata->routingGroup = 1;

  for (size_t command = 0; command < 255; ++command) {
    ASSERT_EQ(transport->stopClip(1000 + command), SessionGraphError::OK);
  }
  EXPECT_EQ(transport->updateClipMetadata(1, *rejectedMetadata), SessionGraphError::InternalError);

  const auto persistentAfterRejection = transport->getClipMetadata(1);
  ASSERT_TRUE(persistentAfterRejection.has_value());
  EXPECT_EQ(persistentAfterRejection->routingGroup, 0u);

  const auto rejectedRender = render();
  EXPECT_GT(rejectedRender[0] + rejectedRender[1], 0.0);
  EXPECT_DOUBLE_EQ(rejectedRender[2] + rejectedRender[3], 0.0);

  auto acceptedMetadata = transport->getClipMetadata(1);
  ASSERT_TRUE(acceptedMetadata.has_value());
  acceptedMetadata->routingGroup = 1;
  ASSERT_EQ(transport->updateClipMetadata(1, *acceptedMetadata), SessionGraphError::OK);

  const auto persistentAfterSuccess = transport->getClipMetadata(1);
  ASSERT_TRUE(persistentAfterSuccess.has_value());
  EXPECT_EQ(persistentAfterSuccess->routingGroup, 1u);

  const auto acceptedRender = render();
  EXPECT_DOUBLE_EQ(acceptedRender[0] + acceptedRender[1], 0.0);
  EXPECT_GT(acceptedRender[2] + acceptedRender[3], 0.0);

  ASSERT_EQ(transport->startClipWithGroupChoke(2), SessionGraphError::OK);
  render();
  EXPECT_EQ(transport->getClipState(1), PlaybackState::Stopping);
  EXPECT_EQ(transport->getClipState(2), PlaybackState::Playing);
}

TEST_F(AtomicGroupChokeTest, VoicePoolRejectionLeavesEveryPeerUntouched) {
  transport.reset();
  callback.events.clear();
  createTransport(3);

  registerClip(1, 0);
  registerClip(2, 0);
  registerClip(3, 1);
  registerClip(4, 0);
  drainSetupCommands();

  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(2), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(3), SessionGraphError::OK);
  render();
  transport->processCallbacks();
  callback.events.clear();

  ASSERT_EQ(transport->startClipWithGroupChoke(4), SessionGraphError::OK);
  render();
  transport->processCallbacks();

  EXPECT_EQ(transport->getClipState(1), PlaybackState::Playing);
  EXPECT_EQ(transport->getClipState(2), PlaybackState::Playing);
  EXPECT_EQ(transport->getClipState(3), PlaybackState::Playing);
  EXPECT_EQ(transport->getClipState(4), PlaybackState::Stopped);
  ASSERT_EQ(callback.events.size(), 1u);
  EXPECT_EQ(callback.events[0].type, CallbackEvent::Type::Refused);
  EXPECT_EQ(callback.events[0].handle, 4u);
}

TEST_F(AtomicGroupChokeTest, MonoRefirePreservesFadeOverlapCapAndSparesOwnTails) {
  registerClip(1, 0);
  registerClip(2, 0, VoiceMode::MonoWithFadeOverlap, 0.1);
  drainSetupCommands();
  ASSERT_EQ(transport->setMaxVoicesPerClip(2), SessionGraphError::OK);

  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(2), SessionGraphError::OK);
  render();
  ASSERT_EQ(transport->stopClip(2), SessionGraphError::OK);
  render();
  ASSERT_EQ(transport->getClipState(2), PlaybackState::Stopping);

  ASSERT_EQ(transport->startClipWithGroupChoke(2), SessionGraphError::OK);
  render();
  EXPECT_EQ(transport->getActiveVoiceCount(2), 2u);
  EXPECT_EQ(transport->getClipState(1), PlaybackState::Stopping);

  for (int refire = 0; refire < 8; ++refire) {
    ASSERT_EQ(transport->startClipWithGroupChoke(2), SessionGraphError::OK);
    render();
    EXPECT_LE(transport->getActiveVoiceCount(2), 2u);
    EXPECT_EQ(transport->getClipState(2), PlaybackState::Playing);
  }
}

TEST_F(AtomicGroupChokeTest, PreAdmissionValidationRejectsWithoutPeerMutation) {
  registerClip(1, 0);
  drainSetupCommands();
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  render();

  EXPECT_EQ(transport->startClipWithGroupChoke(0), SessionGraphError::InvalidHandle);
  EXPECT_EQ(transport->startClipWithGroupChoke(99), SessionGraphError::ClipNotRegistered);
  render();
  EXPECT_EQ(transport->getClipState(1), PlaybackState::Playing);
}

} // namespace
