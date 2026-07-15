// SPDX-License-Identifier: MIT

#include "../../src/core/transport/transport_controller.h"
#include <orpheus/transport_controller.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr uint32_t kSampleRate = 48000;
constexpr size_t kBlockFrames = 64;
constexpr orpheus::ClipHandle kFirstHandle = 11;
constexpr orpheus::ClipHandle kSecondHandle = 22;

std::string writeSilentWav(const std::filesystem::path& path) {
  constexpr uint16_t channels = 2;
  constexpr uint16_t bitsPerSample = 16;
  constexpr uint16_t audioFormat = 1;
  constexpr uint32_t durationFrames = kSampleRate * 2;
  constexpr uint32_t bytesPerSample = bitsPerSample / 8;
  constexpr uint32_t dataSize = durationFrames * channels * bytesPerSample;
  constexpr uint32_t riffSize = 36 + dataSize;
  constexpr uint32_t fmtSize = 16;
  constexpr uint32_t byteRate = kSampleRate * channels * bytesPerSample;
  constexpr uint16_t blockAlign = channels * bytesPerSample;

  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write("RIFF", 4);
  file.write(reinterpret_cast<const char*>(&riffSize), sizeof(riffSize));
  file.write("WAVEfmt ", 8);
  file.write(reinterpret_cast<const char*>(&fmtSize), sizeof(fmtSize));
  file.write(reinterpret_cast<const char*>(&audioFormat), sizeof(audioFormat));
  file.write(reinterpret_cast<const char*>(&channels), sizeof(channels));
  file.write(reinterpret_cast<const char*>(&kSampleRate), sizeof(kSampleRate));
  file.write(reinterpret_cast<const char*>(&byteRate), sizeof(byteRate));
  file.write(reinterpret_cast<const char*>(&blockAlign), sizeof(blockAlign));
  file.write(reinterpret_cast<const char*>(&bitsPerSample), sizeof(bitsPerSample));
  file.write("data", 4);
  file.write(reinterpret_cast<const char*>(&dataSize), sizeof(dataSize));
  std::vector<int16_t> silence(static_cast<size_t>(durationFrames) * channels, 0);
  file.write(reinterpret_cast<const char*>(silence.data()),
             static_cast<std::streamsize>(silence.size() * sizeof(int16_t)));
  file.close();
  return path.string();
}

class CountingCallback final : public orpheus::ITransportCallback {
public:
  void onClipStarted(orpheus::ClipHandle, orpheus::TransportPosition) override {
    ++count;
  }
  void onClipStopped(orpheus::ClipHandle, orpheus::TransportPosition) override {
    ++count;
  }
  void onClipLooped(orpheus::ClipHandle, orpheus::TransportPosition) override {
    ++count;
  }
  void onClipRestarted(orpheus::ClipHandle, orpheus::TransportPosition) override {
    ++count;
  }
  void onClipSeeked(orpheus::ClipHandle, orpheus::TransportPosition) override {
    ++count;
  }
  void onBufferUnderrun(orpheus::TransportPosition) override {
    ++count;
  }

  uint64_t count = 0;
};

const orpheus::ActiveVoiceSnapshotEntry* findEntry(const orpheus::ActiveVoiceSnapshot& snapshot,
                                                   orpheus::ClipHandle handle) {
  for (uint32_t index = 0; index < snapshot.entryCount; ++index) {
    if (snapshot.entries[index].handle == handle) {
      return &snapshot.entries[index];
    }
  }
  return nullptr;
}

class CallbackLossTelemetryTest : public ::testing::Test {
protected:
  void SetUp() override {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    directory =
        std::filesystem::temp_directory_path() / ("orp151-callback-loss-" + std::to_string(suffix));
    std::filesystem::create_directories(directory);
    audioPath = writeSilentWav(directory / "voices.wav");

    orpheus::TransportConfig config;
    config.sampleRate = kSampleRate;
    config.outputChannels = 2;
    config.maxBlockFrames = static_cast<uint32_t>(kBlockFrames);
    config.maxActiveVoices = 4;
    transport = orpheus::createTransportController(nullptr, config);
    ASSERT_NE(transport, nullptr);
    ASSERT_EQ(transport->registerClipAudio(kFirstHandle, audioPath),
              orpheus::SessionGraphError::OK);
    ASSERT_EQ(transport->registerClipAudio(kSecondHandle, audioPath),
              orpheus::SessionGraphError::OK);
    ASSERT_EQ(transport->prepareClipAudio(kFirstHandle), orpheus::SessionGraphError::OK);
    ASSERT_EQ(transport->prepareClipAudio(kSecondHandle), orpheus::SessionGraphError::OK);
    transport->setCallback(&callback);
  }

  void TearDown() override {
    transport.reset();
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
  }

  void renderBlock() {
    float* outputs[2] = {left.data(), right.data()};
    transport->processAudio(outputs, 2, kBlockFrames);
  }

  std::filesystem::path directory;
  std::string audioPath;
  std::unique_ptr<orpheus::ITransportController> transport;
  CountingCallback callback;
  std::array<float, kBlockFrames> left{};
  std::array<float, kBlockFrames> right{};
};

TEST_F(CallbackLossTelemetryTest, OverflowIsCumulativeAndSnapshotReconcilesSurvivingVoices) {
  const auto initialTelemetry = transport->getCallbackDeliveryTelemetry();
  EXPECT_EQ(initialTelemetry.lastAttemptedSequence, 0u);
  EXPECT_EQ(initialTelemetry.lastPostedSequence, 0u);
  EXPECT_EQ(initialTelemetry.cumulativeDroppedCount, 0u);
  EXPECT_EQ(initialTelemetry.lastDroppedSequence, 0u);
  EXPECT_EQ(initialTelemetry.activeVoiceSnapshotSequence, 0u);
  EXPECT_EQ(transport->getActiveVoiceSnapshot().publicationSequence, 0u);

  ASSERT_EQ(transport->startClip(kFirstHandle), orpheus::SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(kSecondHandle), orpheus::SessionGraphError::OK);
  renderBlock();

  constexpr uint64_t restartAttempts = 300;
  for (uint64_t attempt = 0; attempt < restartAttempts; ++attempt) {
    ASSERT_EQ(transport->restartClip(kFirstHandle), orpheus::SessionGraphError::OK);
    renderBlock();
  }

  const auto overflowTelemetry = transport->getCallbackDeliveryTelemetry();
  EXPECT_EQ(overflowTelemetry.lastAttemptedSequence, 302u);
  EXPECT_EQ(overflowTelemetry.lastPostedSequence, 255u);
  EXPECT_EQ(overflowTelemetry.cumulativeDroppedCount, 47u);
  EXPECT_EQ(overflowTelemetry.lastDroppedSequence, 302u);
  EXPECT_EQ(overflowTelemetry.activeVoiceSnapshotSequence, 301u);

  const auto snapshot = transport->getActiveVoiceSnapshot();
  EXPECT_EQ(snapshot.publicationSequence, 301u);
  EXPECT_GE(snapshot.publicationSequence, overflowTelemetry.activeVoiceSnapshotSequence);
  EXPECT_EQ(snapshot.entryCount, 2u);
  EXPECT_EQ(snapshot.totalActiveVoiceCount, 2u);

  const auto* first = findEntry(snapshot, kFirstHandle);
  const auto* second = findEntry(snapshot, kSecondHandle);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(first->activeVoiceCount, 1u);
  EXPECT_EQ(first->state, orpheus::PlaybackState::Playing);
  EXPECT_EQ(first->newestPosition.samples, static_cast<int64_t>(kBlockFrames));
  EXPECT_EQ(second->activeVoiceCount, 1u);
  EXPECT_EQ(second->state, orpheus::PlaybackState::Playing);
  EXPECT_EQ(second->newestPosition.samples, static_cast<int64_t>(kBlockFrames * 301));

  transport->processCallbacks();
  EXPECT_EQ(callback.count, 255u);
  EXPECT_EQ(transport->getCallbackDeliveryTelemetry().cumulativeDroppedCount, 47u);

  ASSERT_EQ(transport->restartClip(kFirstHandle), orpheus::SessionGraphError::OK);
  renderBlock();
  const auto recoveredTelemetry = transport->getCallbackDeliveryTelemetry();
  EXPECT_EQ(recoveredTelemetry.lastAttemptedSequence, 303u);
  EXPECT_EQ(recoveredTelemetry.lastPostedSequence, 303u);
  EXPECT_EQ(recoveredTelemetry.cumulativeDroppedCount, 47u);
  EXPECT_EQ(recoveredTelemetry.lastDroppedSequence, 302u);
  EXPECT_EQ(recoveredTelemetry.activeVoiceSnapshotSequence, 302u);
  transport->processCallbacks();
  EXPECT_EQ(callback.count, 256u);
}

TEST_F(CallbackLossTelemetryTest, HealthyDrainHasNoDropsAndStaysMonotonic) {
  ASSERT_EQ(transport->startClip(kFirstHandle), orpheus::SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(kSecondHandle), orpheus::SessionGraphError::OK);
  renderBlock();

  auto telemetry = transport->getCallbackDeliveryTelemetry();
  EXPECT_EQ(telemetry.lastAttemptedSequence, 2u);
  EXPECT_EQ(telemetry.lastPostedSequence, 2u);
  EXPECT_EQ(telemetry.cumulativeDroppedCount, 0u);
  EXPECT_EQ(telemetry.lastDroppedSequence, 0u);
  EXPECT_EQ(telemetry.activeVoiceSnapshotSequence, 1u);
  transport->processCallbacks();
  EXPECT_EQ(callback.count, 2u);

  ASSERT_EQ(transport->restartClip(kFirstHandle), orpheus::SessionGraphError::OK);
  renderBlock();
  telemetry = transport->getCallbackDeliveryTelemetry();
  EXPECT_EQ(telemetry.lastAttemptedSequence, 3u);
  EXPECT_EQ(telemetry.lastPostedSequence, 3u);
  EXPECT_EQ(telemetry.cumulativeDroppedCount, 0u);
  EXPECT_EQ(telemetry.lastDroppedSequence, 0u);
  EXPECT_EQ(telemetry.activeVoiceSnapshotSequence, 2u);
  transport->processCallbacks();
  EXPECT_EQ(callback.count, 3u);

  const auto snapshot = transport->getActiveVoiceSnapshot();
  EXPECT_EQ(snapshot.entryCount, 2u);
  EXPECT_EQ(snapshot.totalActiveVoiceCount, 2u);
  EXPECT_EQ(snapshot.publicationSequence, 2u);
}

TEST_F(CallbackLossTelemetryTest, ConcurrentSnapshotsRemainCoherentDuringPublication) {
  ASSERT_EQ(transport->setClipLoopMode(kFirstHandle, true), orpheus::SessionGraphError::OK);
  ASSERT_EQ(transport->setClipLoopMode(kSecondHandle, true), orpheus::SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(kFirstHandle), orpheus::SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(kSecondHandle), orpheus::SessionGraphError::OK);
  renderBlock();

  std::atomic<bool> stop{false};
  std::thread audioThread([this, &stop] {
    while (!stop.load(std::memory_order_relaxed)) {
      renderBlock();
    }
  });

  uint64_t previousPublication = 0;
  for (size_t iteration = 0; iteration < 2000; ++iteration) {
    const auto telemetry = transport->getCallbackDeliveryTelemetry();
    const auto snapshot = transport->getActiveVoiceSnapshot();
    EXPECT_LE(snapshot.entryCount, orpheus::kActiveVoiceSnapshotCapacity);
    EXPECT_GE(snapshot.publicationSequence, previousPublication);
    previousPublication = snapshot.publicationSequence;

    uint32_t summedVoices = 0;
    for (uint32_t index = 0; index < snapshot.entryCount; ++index) {
      const auto& entry = snapshot.entries[index];
      EXPECT_NE(entry.handle, 0u);
      EXPECT_NE(entry.newestVoiceId, 0u);
      EXPECT_GT(entry.activeVoiceCount, 0u);
      EXPECT_TRUE(entry.state == orpheus::PlaybackState::Playing ||
                  entry.state == orpheus::PlaybackState::Stopping);
      EXPECT_GE(entry.newestPosition.samples, 0);
      const double expectedSeconds =
          static_cast<double>(entry.newestPosition.samples) / static_cast<double>(kSampleRate);
      EXPECT_DOUBLE_EQ(entry.newestPosition.seconds, expectedSeconds);
      EXPECT_DOUBLE_EQ(entry.newestPosition.beats, expectedSeconds * 2.0);
      summedVoices += entry.activeVoiceCount;
      for (uint32_t other = index + 1; other < snapshot.entryCount; ++other) {
        EXPECT_NE(entry.handle, snapshot.entries[other].handle);
      }
    }
    EXPECT_EQ(snapshot.totalActiveVoiceCount, summedVoices);
    EXPECT_GE(snapshot.publicationSequence, telemetry.activeVoiceSnapshotSequence);
  }

  stop.store(true, std::memory_order_relaxed);
  audioThread.join();
}

TEST_F(CallbackLossTelemetryTest, SingleHandleAggregatesTailsStateTieBreakAndCompaction) {
  ASSERT_EQ(transport->startClip(kFirstHandle), orpheus::SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(kFirstHandle), orpheus::SessionGraphError::OK);
  renderBlock();

  auto snapshot = transport->getActiveVoiceSnapshot();
  const auto* aggregate = findEntry(snapshot, kFirstHandle);
  ASSERT_NE(aggregate, nullptr);
  EXPECT_EQ(aggregate->activeVoiceCount, 2u);
  EXPECT_EQ(aggregate->state, orpheus::PlaybackState::Playing);
  EXPECT_EQ(aggregate->newestVoiceId, 2u);
  EXPECT_EQ(aggregate->newestStartSample, 0);
  EXPECT_EQ(aggregate->newestPosition.samples, static_cast<int64_t>(kBlockFrames));

  ASSERT_EQ(transport->stopClip(kFirstHandle), orpheus::SessionGraphError::OK);
  renderBlock();
  snapshot = transport->getActiveVoiceSnapshot();
  aggregate = findEntry(snapshot, kFirstHandle);
  ASSERT_NE(aggregate, nullptr);
  EXPECT_EQ(aggregate->activeVoiceCount, 2u);
  EXPECT_EQ(aggregate->state, orpheus::PlaybackState::Stopping);
  EXPECT_EQ(aggregate->newestVoiceStopping, 1u);

  ASSERT_EQ(transport->setClipVoiceMode(kFirstHandle, orpheus::VoiceMode::MonoWithFadeOverlap),
            orpheus::SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(kFirstHandle), orpheus::SessionGraphError::OK);
  renderBlock();
  snapshot = transport->getActiveVoiceSnapshot();
  aggregate = findEntry(snapshot, kFirstHandle);
  ASSERT_NE(aggregate, nullptr);
  EXPECT_EQ(aggregate->activeVoiceCount, 3u);
  EXPECT_EQ(aggregate->state, orpheus::PlaybackState::Playing);
  EXPECT_EQ(aggregate->newestVoiceId, 3u);
  EXPECT_EQ(aggregate->newestVoiceStopping, 0u);
  EXPECT_EQ(aggregate->newestVoiceLoopEnabled, 0u);
  EXPECT_EQ(aggregate->newestStartSample, static_cast<int64_t>(kBlockFrames * 2));
  EXPECT_EQ(aggregate->newestPosition.samples, static_cast<int64_t>(kBlockFrames));
  EXPECT_EQ(aggregate->newestTrimInSamples, 0);
  EXPECT_EQ(aggregate->newestTrimOutSamples, static_cast<int64_t>(kSampleRate * 2));

  ASSERT_EQ(transport->stopClip(kFirstHandle), orpheus::SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(kSecondHandle), orpheus::SessionGraphError::OK);
  renderBlock();
  snapshot = transport->getActiveVoiceSnapshot();
  aggregate = findEntry(snapshot, kFirstHandle);
  ASSERT_NE(aggregate, nullptr);
  EXPECT_EQ(aggregate->activeVoiceCount, 3u);
  EXPECT_EQ(aggregate->state, orpheus::PlaybackState::Stopping);
  ASSERT_NE(findEntry(snapshot, kSecondHandle), nullptr);

  for (size_t block = 0; block < 8; ++block) {
    renderBlock();
  }
  snapshot = transport->getActiveVoiceSnapshot();
  EXPECT_EQ(findEntry(snapshot, kFirstHandle), nullptr);
  const auto* survivor = findEntry(snapshot, kSecondHandle);
  ASSERT_NE(survivor, nullptr);
  EXPECT_EQ(snapshot.entryCount, 1u);
  EXPECT_EQ(snapshot.totalActiveVoiceCount, 1u);
  EXPECT_EQ(survivor->activeVoiceCount, 1u);
  EXPECT_EQ(survivor->state, orpheus::PlaybackState::Playing);
}

TEST_F(CallbackLossTelemetryTest, VoiceIdsWrapWithoutZeroCollisionAndSurviveCompaction) {
  orpheus::TransportConfig config;
  config.sampleRate = kSampleRate;
  config.outputChannels = 2;
  config.maxBlockFrames = static_cast<uint32_t>(kBlockFrames);
  config.maxActiveVoices = 4;
  auto wrapped = std::make_unique<orpheus::TransportController>(nullptr, config);

  constexpr std::array<orpheus::ClipHandle, 4> handles = {101, 102, 103, 104};
  for (const auto handle : handles) {
    ASSERT_EQ(wrapped->registerClipAudio(handle, audioPath), orpheus::SessionGraphError::OK);
    ASSERT_EQ(wrapped->prepareClipAudio(handle), orpheus::SessionGraphError::OK);
  }

  std::array<float, kBlockFrames> wrapLeft{};
  std::array<float, kBlockFrames> wrapRight{};
  auto renderWrapped = [&] {
    float* outputs[2] = {wrapLeft.data(), wrapRight.data()};
    wrapped->processAudio(outputs, 2, kBlockFrames);
  };

  wrapped->setNextVoiceIdForTesting(std::numeric_limits<uint32_t>::max() - 1);
  ASSERT_EQ(wrapped->startClip(handles[0]), orpheus::SessionGraphError::OK);
  ASSERT_EQ(wrapped->startClip(handles[1]), orpheus::SessionGraphError::OK);
  ASSERT_EQ(wrapped->startClip(handles[2]), orpheus::SessionGraphError::OK);
  renderWrapped();

  auto snapshot = wrapped->getActiveVoiceSnapshot();
  ASSERT_NE(findEntry(snapshot, handles[0]), nullptr);
  ASSERT_NE(findEntry(snapshot, handles[1]), nullptr);
  ASSERT_NE(findEntry(snapshot, handles[2]), nullptr);
  EXPECT_EQ(findEntry(snapshot, handles[0])->newestVoiceId,
            std::numeric_limits<uint32_t>::max() - 1);
  EXPECT_EQ(findEntry(snapshot, handles[1])->newestVoiceId, std::numeric_limits<uint32_t>::max());
  EXPECT_EQ(findEntry(snapshot, handles[2])->newestVoiceId, 1u);

  wrapped->setNextVoiceIdForTesting(std::numeric_limits<uint32_t>::max());
  ASSERT_EQ(wrapped->startClip(handles[3]), orpheus::SessionGraphError::OK);
  renderWrapped();
  snapshot = wrapped->getActiveVoiceSnapshot();
  ASSERT_NE(findEntry(snapshot, handles[3]), nullptr);
  EXPECT_EQ(findEntry(snapshot, handles[3])->newestVoiceId, 2u);
  for (uint32_t index = 0; index < snapshot.entryCount; ++index) {
    EXPECT_NE(snapshot.entries[index].newestVoiceId, 0u);
    for (uint32_t other = index + 1; other < snapshot.entryCount; ++other) {
      EXPECT_NE(snapshot.entries[index].newestVoiceId, snapshot.entries[other].newestVoiceId);
    }
  }

  ASSERT_EQ(wrapped->stopClip(handles[1]), orpheus::SessionGraphError::OK);
  for (size_t block = 0; block < 8; ++block) {
    renderWrapped();
  }
  snapshot = wrapped->getActiveVoiceSnapshot();
  EXPECT_EQ(findEntry(snapshot, handles[1]), nullptr);
  EXPECT_EQ(snapshot.totalActiveVoiceCount, 3u);
  ASSERT_NE(findEntry(snapshot, handles[0]), nullptr);
  ASSERT_NE(findEntry(snapshot, handles[2]), nullptr);
  ASSERT_NE(findEntry(snapshot, handles[3]), nullptr);
  EXPECT_EQ(findEntry(snapshot, handles[0])->newestVoiceId,
            std::numeric_limits<uint32_t>::max() - 1);
  EXPECT_EQ(findEntry(snapshot, handles[2])->newestVoiceId, 1u);
  EXPECT_EQ(findEntry(snapshot, handles[3])->newestVoiceId, 2u);

  ASSERT_EQ(wrapped->startClip(handles[1]), orpheus::SessionGraphError::OK);
  renderWrapped();
  snapshot = wrapped->getActiveVoiceSnapshot();
  const auto* restarted = findEntry(snapshot, handles[1]);
  ASSERT_NE(restarted, nullptr);
  EXPECT_EQ(restarted->newestVoiceId, 3u);
  EXPECT_EQ(restarted->state, orpheus::PlaybackState::Playing);
  EXPECT_EQ(snapshot.totalActiveVoiceCount, 4u);
}

TEST_F(CallbackLossTelemetryTest, SameSampleOrdinalWrapSelectsChronologicallyNewestVoice) {
  orpheus::TransportConfig config;
  config.sampleRate = kSampleRate;
  config.outputChannels = 2;
  config.maxBlockFrames = static_cast<uint32_t>(kBlockFrames);
  config.maxActiveVoices = 2;
  auto wrapped = std::make_unique<orpheus::TransportController>(nullptr, config);

  constexpr orpheus::ClipHandle handle = 501;
  ASSERT_EQ(wrapped->registerClipAudio(handle, audioPath), orpheus::SessionGraphError::OK);
  ASSERT_EQ(wrapped->prepareClipAudio(handle), orpheus::SessionGraphError::OK);

  std::array<float, kBlockFrames> wrapLeft{};
  std::array<float, kBlockFrames> wrapRight{};
  auto renderWrapped = [&] {
    float* outputs[2] = {wrapLeft.data(), wrapRight.data()};
    wrapped->processAudio(outputs, 2, kBlockFrames);
  };

  wrapped->setNextVoiceIdForTesting(std::numeric_limits<uint32_t>::max());
  wrapped->setNextVoiceStartOrdinalForTesting(std::numeric_limits<uint64_t>::max());
  ASSERT_EQ(wrapped->startClip(handle), orpheus::SessionGraphError::OK);
  ASSERT_EQ(wrapped->startClip(handle), orpheus::SessionGraphError::OK);
  renderWrapped();

  auto snapshot = wrapped->getActiveVoiceSnapshot();
  const auto* aggregate = findEntry(snapshot, handle);
  ASSERT_NE(aggregate, nullptr);
  EXPECT_EQ(aggregate->activeVoiceCount, 2u);
  EXPECT_EQ(aggregate->newestStartSample, 0);
  EXPECT_EQ(aggregate->newestVoiceId, 1u);

  ASSERT_TRUE(wrapped->setVoiceSnapshotFieldsForTesting(std::numeric_limits<uint32_t>::max(), true,
                                                        false, 10, 1000, 20));
  ASSERT_TRUE(wrapped->setVoiceSnapshotFieldsForTesting(1, false, true, 30, 2000, 40));
  renderWrapped();

  snapshot = wrapped->getActiveVoiceSnapshot();
  aggregate = findEntry(snapshot, handle);
  ASSERT_NE(aggregate, nullptr);
  EXPECT_EQ(aggregate->activeVoiceCount, 2u);
  EXPECT_EQ(aggregate->state, orpheus::PlaybackState::Playing);
  EXPECT_EQ(aggregate->newestVoiceId, 1u);
  EXPECT_EQ(aggregate->newestVoiceStopping, 0u);
  EXPECT_EQ(aggregate->newestVoiceLoopEnabled, 1u);
  EXPECT_EQ(aggregate->newestTrimInSamples, 30);
  EXPECT_EQ(aggregate->newestTrimOutSamples, 2000);
  EXPECT_EQ(aggregate->newestPosition.samples, 104);
  const double expectedSeconds = 104.0 / static_cast<double>(kSampleRate);
  EXPECT_DOUBLE_EQ(aggregate->newestPosition.seconds, expectedSeconds);
  EXPECT_DOUBLE_EQ(aggregate->newestPosition.beats, expectedSeconds * 2.0);
}

} // namespace
