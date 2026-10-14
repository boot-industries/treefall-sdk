// SPDX-License-Identifier: MIT

#include "../../src/core/transport/transport_controller.h"
#include <orpheus/transport_controller.h>

#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cmath>
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

std::string writeSilentWav(const std::filesystem::path& path, bool steppedSignal = false) {
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
  if (steppedSignal) {
    // Distinct block amplitudes make stale/torn meter payloads observable.
    for (size_t frame = 0; frame < durationFrames; ++frame) {
      const auto sample = static_cast<int16_t>(((frame / kBlockFrames) % 31 + 1) * 128);
      silence[frame * channels] = sample;
      silence[frame * channels + 1] = sample;
    }
  }
  file.write(reinterpret_cast<const char*>(silence.data()),
             static_cast<std::streamsize>(silence.size() * sizeof(int16_t)));
  file.close();
  return path.string();
}

class CountingCallback final : public orpheus::ITransportCallback {
public:
  void onClipStarted(orpheus::ClipHandle handle, orpheus::StartRequestTag requestTag,
                     uint32_t voiceId, orpheus::TransportPosition) override {
    ++count;
    started.push_back({requestTag, voiceId, handle});
  }
  void onClipStopped(orpheus::ClipHandle handle, orpheus::StartRequestTag requestTag,
                     uint32_t voiceId, orpheus::TransportPosition) override {
    ++count;
    stopped.push_back({requestTag, voiceId, handle});
  }
  void onClipLooped(orpheus::ClipHandle, orpheus::StartRequestTag, uint32_t,
                    orpheus::TransportPosition) override {
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
  void onActiveClipLimitReached(orpheus::ClipHandle, orpheus::StartRequestTag requestTag,
                                orpheus::TransportPosition) override {
    rejectedTags.push_back(requestTag);
  }

  struct TaggedVoice {
    orpheus::StartRequestTag requestTag = 0;
    uint32_t voiceId = 0;
    orpheus::ClipHandle handle = 0;
  };

  std::vector<TaggedVoice> started;
  std::vector<TaggedVoice> stopped;
  std::vector<orpheus::StartRequestTag> rejectedTags;

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

TEST_F(CallbackLossTelemetryTest, ConcurrentRealtimeRingSaturatesPreservesPayloadAndRecovers) {
  constexpr uint64_t kStalledAttempts = orpheus::kRealtimeTelemetryCapacity + 7;
  constexpr uint64_t kAttempts = 4096;
  constexpr orpheus::ClipHandle kSignalHandle = 33;
  const auto signalPath = writeSilentWav(directory / "stepped.wav", true);
  ASSERT_EQ(transport->registerClipAudio(kSignalHandle, signalPath),
            orpheus::SessionGraphError::OK);
  ASSERT_EQ(transport->prepareClipAudio(kSignalHandle), orpheus::SessionGraphError::OK);
  ASSERT_EQ(transport->setClipLoopMode(kSignalHandle, true), orpheus::SessionGraphError::OK);
  ASSERT_EQ(transport->setClipLoopMode(kFirstHandle, true), orpheus::SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(kSignalHandle), orpheus::SessionGraphError::OK);
  auto* bridge = transport->getRealtimeTelemetry();
  ASSERT_NE(bridge, nullptr);
  bridge->setDecimationBlocks(1);
  ASSERT_EQ(bridge->pendingSnapshotCount(), 0u);

  struct RenderedBlock {
    std::array<float, 2> rmsDb{};
    uint32_t voices = 0;
  };
  // Producer output is an independent oracle for the meters from that exact
  // transport publication, not an unrelated control-side routing/voice query.
  std::vector<RenderedBlock> rendered(kAttempts + 1);
  std::vector<uint64_t> received;
  received.reserve(kAttempts);
  std::atomic<uint64_t> renderedThrough{0};
  std::atomic<bool> finished{false};
  std::atomic<bool> bounded{true};
  bool startSucceeded = false;
  bool coherent = true;
  size_t saturatedPending = 0;
  uint64_t saturatedDrops = 0;
  std::barrier phases(2);

  std::thread producer([&] {
    const auto render = [&](uint64_t attempt, uint32_t voices) {
      renderBlock();
      auto& expected = rendered[attempt];
      expected.voices = voices;
      for (size_t lane = 0; lane < 2; ++lane) {
        const auto& output = lane == 0 ? left : right;
        double sumSquares = 0.0;
        for (const float sample : output) {
          sumSquares += static_cast<double>(sample) * sample;
        }
        const float rms = static_cast<float>(std::sqrt(sumSquares / kBlockFrames));
        expected.rmsDb[lane] = rms > 0.0f ? 20.0f * std::log10(rms) : orpheus::kAudioMeterSilenceDb;
      }
      renderedThrough.store(attempt, std::memory_order_release);
      if (bridge->pendingSnapshotCount() > orpheus::kRealtimeTelemetryCapacity) {
        bounded.store(false, std::memory_order_relaxed);
      }
    };
    // The consumer is deliberately stalled: all 64 unread slots must survive
    // the next seven attempts unchanged (drop newest, not oldest).
    for (uint64_t attempt = 1; attempt <= kStalledAttempts; ++attempt) {
      render(attempt, 1);
    }
    phases.arrive_and_wait(); // Permit the consumer to inspect and drain.
    phases.arrive_and_wait(); // All retained slots have been consumed.
    render(kStalledAttempts + 1, 1);
    phases.arrive_and_wait(); // Publish the first recovered, gapped sequence.
    phases.arrive_and_wait(); // Consumer has checked recovery before stress.
    // Change voice count only after the gapped recovery is consumed: changing
    // routing topology during a dropped peak window legitimately resets it.
    startSucceeded = transport->startClip(kFirstHandle) == orpheus::SessionGraphError::OK;
    for (uint64_t attempt = kStalledAttempts + 2; attempt <= kAttempts; ++attempt) {
      render(attempt, 2);
    }
    finished.store(true, std::memory_order_release);
  });

  std::thread consumer([&] {
    orpheus::RealtimeTelemetrySnapshot snapshot;
    uint64_t previousSequence = 0;
    const auto consume = [&](uint64_t maximumSequence) {
      while (bridge->tryRead(snapshot)) {
        const uint64_t sequence = snapshot.sequence;
        if (sequence == 0 || sequence > maximumSequence || sequence <= previousSequence) {
          coherent = false;
          continue;
        }
        // processAudio publishes before the producer computes the independent
        // output oracle; this release/acquire handoff protects that table only.
        while (renderedThrough.load(std::memory_order_acquire) < sequence) {
          std::this_thread::yield();
        }
        const auto& expected = rendered[sequence];
        coherent &= snapshot.schema_version == orpheus::kRealtimeTelemetrySchemaVersion;
        coherent &= snapshot.position.samples() == static_cast<int64_t>(sequence * kBlockFrames);
        coherent &= snapshot.active_voice_count == expected.voices;
        coherent &= snapshot.diagnostics.callback_count == sequence;
        coherent &= snapshot.diagnostics.samples_processed == sequence * kBlockFrames;
        coherent &= snapshot.diagnostics.last_buffer_frames == kBlockFrames;
        coherent &= snapshot.diagnostics.last_sample_rate == kSampleRate;
        coherent &= snapshot.diagnostics.underrun_count == 0;
        coherent &= snapshot.diagnostics.callback_over_budget_count == 0;
        coherent &= snapshot.diagnostics.callback_p99_over_budget_count == 0;
        coherent &= snapshot.output_count == 2;
        const auto& meters = snapshot.routing_meters;
        coherent &= meters.schema_version == orpheus::kRoutingMeterTelemetrySchemaVersion;
        coherent &= meters.availability == orpheus::MeterAvailability::Measured;
        coherent &= meters.post_master_output_count == 2;
        coherent &= meters.rms_window_frames == kBlockFrames;
        coherent &= meters.peak_window_frames == (sequence - previousSequence) * kBlockFrames;
        for (size_t lane = 0; lane < 2; ++lane) {
          coherent &=
              meters.post_master_output_availability[lane] == orpheus::MeterAvailability::Measured;
          coherent &= std::abs(meters.post_master_output_lane_meters[lane].rms_db -
                               expected.rmsDb[lane]) < 0.0001f;
          coherent &=
              std::abs(snapshot.output_meters[lane].rms_db - expected.rmsDb[lane]) < 0.0001f;
        }
        received.push_back(sequence);
        previousSequence = sequence;
        if (bridge->pendingSnapshotCount() > orpheus::kRealtimeTelemetryCapacity) {
          bounded.store(false, std::memory_order_relaxed);
        }
      }
    };
    phases.arrive_and_wait();
    saturatedPending = bridge->pendingSnapshotCount();
    saturatedDrops = bridge->droppedSnapshotCount();
    consume(kStalledAttempts);
    phases.arrive_and_wait();
    phases.arrive_and_wait();
    consume(kStalledAttempts + 1);
    phases.arrive_and_wait();
    while (!finished.load(std::memory_order_acquire)) {
      consume(kAttempts);
      std::this_thread::yield();
    }
    consume(kAttempts);
  });
  producer.join();
  consumer.join();

  EXPECT_TRUE(startSucceeded);
  EXPECT_TRUE(coherent) << "A consumed payload must match its own rendered block";
  EXPECT_TRUE(bounded.load());
  EXPECT_EQ(saturatedPending, orpheus::kRealtimeTelemetryCapacity);
  EXPECT_EQ(saturatedDrops, 7u);
  ASSERT_GE(received.size(), orpheus::kRealtimeTelemetryCapacity + 1);
  for (size_t index = 0; index < orpheus::kRealtimeTelemetryCapacity; ++index) {
    EXPECT_EQ(received[index], index + 1) << "Unread slots must not be overwritten";
  }
  EXPECT_EQ(received[orpheus::kRealtimeTelemetryCapacity], kStalledAttempts + 1);
  EXPECT_EQ(received.size() + bridge->droppedSnapshotCount(), kAttempts);
  EXPECT_EQ(bridge->pendingSnapshotCount(), 0u);
}

TEST_F(CallbackLossTelemetryTest, SameFileHandlesKeepIndependentVoicesPoliciesAndRenderedAudio) {
  // Both handles decode the same stepped PCM, but use different source offsets
  // and opposite stereo balances. Each output channel therefore proves its
  // handle's actual source cursors and envelopes, not merely its snapshot.
  const auto signalPath = writeSilentWav(directory / "shared-signal.wav", true);
  ASSERT_EQ(transport->unregisterClipAudio(kFirstHandle), orpheus::SessionGraphError::OK);
  ASSERT_EQ(transport->unregisterClipAudio(kSecondHandle), orpheus::SessionGraphError::OK);
  constexpr int64_t kSecondTrim = kBlockFrames * 8;
  constexpr size_t kStopFadeFrames = 480; // 10 ms at 48 kHz, including a partial final block.
  for (const auto handle : {kFirstHandle, kSecondHandle}) {
    ASSERT_EQ(transport->registerClipAudio(handle, signalPath), orpheus::SessionGraphError::OK);
  }
  auto firstMetadata = transport->getClipMetadata(kFirstHandle).value();
  firstMetadata.pan = -1.0f;
  firstMetadata.stopFadeOutSeconds = 0.01;
  auto secondMetadata = transport->getClipMetadata(kSecondHandle).value();
  secondMetadata.pan = 1.0f;
  secondMetadata.trimInSamples = kSecondTrim;
  for (const auto handle : {kFirstHandle, kSecondHandle}) {
    ASSERT_EQ(transport->updateClipMetadata(handle, handle == kFirstHandle ? firstMetadata
                                                                           : secondMetadata),
              orpheus::SessionGraphError::OK);
    ASSERT_EQ(transport->prepareClipAudio(handle), orpheus::SessionGraphError::OK);
    ASSERT_EQ(transport->getClipVoiceMode(handle), orpheus::VoiceMode::Polyphonic);
  }

  const auto sourceSample = [](size_t sourceFrame) {
    return static_cast<float>(((sourceFrame / kBlockFrames) % 31 + 1) * 128) / 32768.0f;
  };
  const auto stopGain = [](size_t elapsedFrame) {
    return elapsedFrame < kStopFadeFrames
               ? 1.0f - static_cast<float>(elapsedFrame) / static_cast<float>(kStopFadeFrames)
               : 0.0f;
  };
  // Frames already rendered since B's first start; its second start is one
  // block later. Never derive the output oracle from a transport snapshot.
  size_t renderedFrames = 0;
  const auto expectSecondOutput = [&](bool secondVoiceStarted) {
    for (size_t frame = 0; frame < kBlockFrames; ++frame) {
      float expected = sourceSample(kSecondTrim + renderedFrames + frame);
      if (secondVoiceStarted) {
        expected += sourceSample(kSecondTrim + renderedFrames - kBlockFrames + frame);
      }
      EXPECT_NEAR(right[frame], expected, 1.0e-6f)
          << "B output at rendered frame " << renderedFrames + frame;
    }
    renderedFrames += kBlockFrames;
  };
  const auto expectFirstOutput = [&](const auto& expectedSample) {
    for (size_t frame = 0; frame < kBlockFrames; ++frame) {
      EXPECT_NEAR(left[frame], expectedSample(frame), 1.0e-6f) << "A frame " << frame;
    }
  };

  ASSERT_EQ(transport->startClip(kFirstHandle, 101), orpheus::SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(kSecondHandle, 201), orpheus::SessionGraphError::OK);
  renderBlock();
  expectFirstOutput([&](size_t frame) { return sourceSample(frame); });
  expectSecondOutput(false);
  ASSERT_EQ(transport->startClip(kFirstHandle, 102), orpheus::SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(kSecondHandle, 202), orpheus::SessionGraphError::OK);
  renderBlock();
  expectFirstOutput(
      [&](size_t frame) { return sourceSample(kBlockFrames + frame) + sourceSample(frame); });
  expectSecondOutput(true);
  transport->processCallbacks();
  ASSERT_EQ(callback.started.size(), 4u);
  constexpr std::array<orpheus::ClipHandle, 4> expectedHandles{kFirstHandle, kSecondHandle,
                                                               kFirstHandle, kSecondHandle};
  constexpr std::array<orpheus::StartRequestTag, 4> expectedTags{101, 201, 102, 202};
  for (size_t index = 0; index < callback.started.size(); ++index) {
    EXPECT_EQ(callback.started[index].handle, expectedHandles[index]);
    EXPECT_EQ(callback.started[index].requestTag, expectedTags[index]);
    EXPECT_NE(callback.started[index].voiceId, 0u);
    for (size_t previous = 0; previous < index; ++previous) {
      EXPECT_NE(callback.started[index].voiceId, callback.started[previous].voiceId);
    }
  }
  const uint32_t secondVoiceId = callback.started[3].voiceId;
  const uint32_t survivingFirstTailId = callback.started[2].voiceId;
  auto snapshot = transport->getActiveVoiceSnapshot();
  ASSERT_NE(findEntry(snapshot, kFirstHandle), nullptr);
  ASSERT_NE(findEntry(snapshot, kSecondHandle), nullptr);
  EXPECT_EQ(findEntry(snapshot, kFirstHandle)->activeVoiceCount, 2u);
  EXPECT_EQ(findEntry(snapshot, kSecondHandle)->activeVoiceCount, 2u);

  ASSERT_EQ(transport->setClipVoiceMode(kFirstHandle, orpheus::VoiceMode::MonoWithFadeOverlap),
            orpheus::SessionGraphError::OK);
  ASSERT_EQ(transport->setMaxVoicesPerClip(2), orpheus::SessionGraphError::OK);
  EXPECT_EQ(transport->getMaxVoicesPerClip(), 2u);
  EXPECT_EQ(transport->getClipVoiceMode(kSecondHandle), orpheus::VoiceMode::Polyphonic);
  ASSERT_EQ(transport->stopClip(kFirstHandle), orpheus::SessionGraphError::OK);
  renderBlock();
  expectFirstOutput([&](size_t frame) {
    return (sourceSample(2 * kBlockFrames + frame) + sourceSample(kBlockFrames + frame)) *
           stopGain(frame);
  });
  expectSecondOutput(true);
  snapshot = transport->getActiveVoiceSnapshot();
  ASSERT_NE(findEntry(snapshot, kFirstHandle), nullptr);
  EXPECT_EQ(findEntry(snapshot, kFirstHandle)->state, orpheus::PlaybackState::Stopping);
  EXPECT_EQ(findEntry(snapshot, kFirstHandle)->activeVoiceCount, 2u);

  // With both A voices fading, refire allocates a new primary and the cap
  // replaces only A's oldest tail. B's two voices keep their original cursors.
  ASSERT_EQ(transport->startClip(kFirstHandle, 103), orpheus::SessionGraphError::OK);
  renderBlock();
  expectFirstOutput([&](size_t frame) {
    return sourceSample(2 * kBlockFrames + frame) * stopGain(kBlockFrames + frame) +
           sourceSample(frame);
  });
  expectSecondOutput(true);
  transport->processCallbacks();
  ASSERT_EQ(callback.started.size(), 5u);
  EXPECT_EQ(callback.started[4].handle, kFirstHandle);
  EXPECT_EQ(callback.started[4].requestTag, 103u);
  const uint32_t refiredVoiceId = callback.started[4].voiceId;
  for (size_t index = 0; index < 4; ++index) {
    EXPECT_NE(refiredVoiceId, callback.started[index].voiceId);
  }
  snapshot = transport->getActiveVoiceSnapshot();
  ASSERT_NE(findEntry(snapshot, kFirstHandle), nullptr);
  ASSERT_NE(findEntry(snapshot, kSecondHandle), nullptr);
  EXPECT_EQ(findEntry(snapshot, kFirstHandle)->activeVoiceCount, 2u);
  EXPECT_EQ(findEntry(snapshot, kFirstHandle)->state, orpheus::PlaybackState::Playing);
  EXPECT_EQ(findEntry(snapshot, kFirstHandle)->newestVoiceId, refiredVoiceId);
  EXPECT_EQ(findEntry(snapshot, kSecondHandle)->activeVoiceCount, 2u);
  EXPECT_EQ(findEntry(snapshot, kSecondHandle)->newestVoiceId, secondVoiceId);
  EXPECT_EQ(findEntry(snapshot, kSecondHandle)->newestStartRequestTag, 202u);
  const auto secondBeforeRejectedUnregister = *findEntry(snapshot, kSecondHandle);
  ASSERT_EQ(transport->unregisterClipAudio(kFirstHandle), orpheus::SessionGraphError::NotReady);
  snapshot = transport->getActiveVoiceSnapshot();
  ASSERT_NE(findEntry(snapshot, kSecondHandle), nullptr);
  EXPECT_EQ(findEntry(snapshot, kSecondHandle)->newestPosition.samples,
            secondBeforeRejectedUnregister.newestPosition.samples);
  EXPECT_EQ(findEntry(snapshot, kSecondHandle)->newestVoiceId, secondVoiceId);
  EXPECT_EQ(findEntry(snapshot, kSecondHandle)->activeVoiceCount, 2u);
  renderBlock();
  expectFirstOutput([&](size_t frame) {
    return sourceSample(3 * kBlockFrames + frame) * stopGain(2 * kBlockFrames + frame) +
           sourceSample(kBlockFrames + frame);
  });
  expectSecondOutput(true);

  ASSERT_EQ(transport->stopClip(kFirstHandle), orpheus::SessionGraphError::OK);
  constexpr size_t kFadeBlocks = (kStopFadeFrames + kBlockFrames - 1) / kBlockFrames;
  for (size_t block = 0; block < kFadeBlocks; ++block) {
    renderBlock();
    expectFirstOutput([&](size_t frame) {
      const size_t elapsed = block * kBlockFrames + frame;
      // A's existing tail keeps its original stop clock; only the new
      // primary starts a fresh envelope on this second stop.
      return sourceSample(4 * kBlockFrames + elapsed) * stopGain(3 * kBlockFrames + elapsed) +
             sourceSample(2 * kBlockFrames + elapsed) * stopGain(elapsed);
    });
    expectSecondOutput(true);
  }
  transport->processCallbacks();
  ASSERT_EQ(callback.stopped.size(), 2u);
  for (const auto& stopped : callback.stopped) {
    EXPECT_EQ(stopped.handle, kFirstHandle);
    EXPECT_TRUE((stopped.requestTag == 102 && stopped.voiceId == survivingFirstTailId) ||
                (stopped.requestTag == 103 && stopped.voiceId == refiredVoiceId));
  }
  EXPECT_NE(callback.stopped[0].voiceId, callback.stopped[1].voiceId);
  snapshot = transport->getActiveVoiceSnapshot();
  EXPECT_EQ(findEntry(snapshot, kFirstHandle), nullptr);
  ASSERT_EQ(snapshot.totalActiveVoiceCount, 2u);
  ASSERT_EQ(transport->unregisterClipAudio(kFirstHandle), orpheus::SessionGraphError::OK);
  for (size_t block = 0; block < 2; ++block) {
    renderBlock();
    expectFirstOutput([](size_t) { return 0.0f; });
    expectSecondOutput(true);
    snapshot = transport->getActiveVoiceSnapshot();
    EXPECT_EQ(findEntry(snapshot, kFirstHandle), nullptr);
    ASSERT_NE(findEntry(snapshot, kSecondHandle), nullptr);
    EXPECT_EQ(findEntry(snapshot, kSecondHandle)->state, orpheus::PlaybackState::Playing);
    EXPECT_EQ(findEntry(snapshot, kSecondHandle)->activeVoiceCount, 2u);
    EXPECT_EQ(findEntry(snapshot, kSecondHandle)->newestVoiceId, secondVoiceId);
    EXPECT_EQ(findEntry(snapshot, kSecondHandle)->newestStartRequestTag, 202u);
    EXPECT_EQ(findEntry(snapshot, kSecondHandle)->newestPosition.samples,
              kSecondTrim + static_cast<int64_t>(renderedFrames - kBlockFrames));
  }
  transport->processCallbacks();
  EXPECT_EQ(callback.started.size(), 5u);
  EXPECT_EQ(callback.stopped.size(), 2u);
  EXPECT_TRUE(callback.rejectedTags.empty());
  EXPECT_EQ(transport->getCallbackDeliveryTelemetry().cumulativeDroppedCount, 0u);
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

TEST_F(CallbackLossTelemetryTest, StartSettlementSnapshotBeginsEmptyAndIgnoresUntaggedStarts) {
  auto snapshot = transport->getStartSettlementSnapshot();
  EXPECT_EQ(snapshot.schemaVersion, orpheus::kStartSettlementSnapshotSchemaVersion);
  EXPECT_EQ(snapshot.entryCount, 0u);
  EXPECT_EQ(snapshot.sequenceExhausted, 0u);
  EXPECT_EQ(snapshot.oldestSequence, 0u);
  EXPECT_EQ(snapshot.latestSequence, 0u);
  EXPECT_EQ(snapshot.overwrittenCount, 0u);

  ASSERT_EQ(transport->startClip(kFirstHandle), orpheus::SessionGraphError::OK);
  renderBlock();

  snapshot = transport->getStartSettlementSnapshot();
  EXPECT_EQ(snapshot.entryCount, 0u);
  EXPECT_EQ(snapshot.latestSequence, 0u);
  const auto active = transport->getActiveVoiceSnapshot();
  EXPECT_EQ(active.schemaVersion, orpheus::kActiveVoiceSnapshotSchemaVersion);
  const auto* entry = findEntry(active, kFirstHandle);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->newestStartRequestTag, 0u);
}

TEST_F(CallbackLossTelemetryTest, TaggedStartsPublishOrderedPositionsCallbacksAndNewestTags) {
  ASSERT_EQ(transport->startClip(kFirstHandle, 41), orpheus::SessionGraphError::OK);
  renderBlock();
  ASSERT_EQ(transport->startClip(kSecondHandle, 42), orpheus::SessionGraphError::OK);
  renderBlock();

  const auto settlements = transport->getStartSettlementSnapshot();
  ASSERT_EQ(settlements.entryCount, 2u);
  EXPECT_EQ(settlements.oldestSequence, 1u);
  EXPECT_EQ(settlements.latestSequence, 2u);
  EXPECT_EQ(settlements.overwrittenCount, 0u);
  EXPECT_EQ(settlements.entries[0].sequence, 1u);
  EXPECT_EQ(settlements.entries[0].requestTag, 41u);
  EXPECT_EQ(settlements.entries[0].handle, kFirstHandle);
  EXPECT_NE(settlements.entries[0].voiceId, 0u);
  EXPECT_EQ(settlements.entries[0].position.samples, 0);
  EXPECT_EQ(settlements.entries[0].outcome, orpheus::StartSettlementOutcome::Started);
  EXPECT_EQ(settlements.entries[1].sequence, 2u);
  EXPECT_EQ(settlements.entries[1].requestTag, 42u);
  EXPECT_EQ(settlements.entries[1].handle, kSecondHandle);
  EXPECT_NE(settlements.entries[1].voiceId, 0u);
  EXPECT_EQ(settlements.entries[1].position.samples, static_cast<int64_t>(kBlockFrames));
  EXPECT_EQ(settlements.entries[1].outcome, orpheus::StartSettlementOutcome::Started);

  const auto voices = transport->getActiveVoiceSnapshot();
  ASSERT_NE(findEntry(voices, kFirstHandle), nullptr);
  ASSERT_NE(findEntry(voices, kSecondHandle), nullptr);
  EXPECT_EQ(findEntry(voices, kFirstHandle)->newestStartRequestTag, 41u);
  EXPECT_EQ(findEntry(voices, kSecondHandle)->newestStartRequestTag, 42u);

  transport->processCallbacks();
  ASSERT_EQ(callback.started.size(), 2u);
  EXPECT_EQ(callback.started[0].requestTag, 41u);
  EXPECT_EQ(callback.started[0].voiceId, settlements.entries[0].voiceId);
  EXPECT_EQ(callback.started[1].requestTag, 42u);
  EXPECT_EQ(callback.started[1].voiceId, settlements.entries[1].voiceId);
}

TEST_F(CallbackLossTelemetryTest, TaggedVoiceLimitRejectionPublishesOutcomeAndCallbackIdentity) {
  orpheus::TransportConfig config;
  config.sampleRate = kSampleRate;
  config.outputChannels = 2;
  config.maxBlockFrames = static_cast<uint32_t>(kBlockFrames);
  config.maxActiveVoices = 1;
  auto limited = std::make_unique<orpheus::TransportController>(nullptr, config);
  ASSERT_EQ(limited->registerClipAudio(kFirstHandle, audioPath), orpheus::SessionGraphError::OK);
  ASSERT_EQ(limited->registerClipAudio(kSecondHandle, audioPath), orpheus::SessionGraphError::OK);
  ASSERT_EQ(limited->prepareClipAudio(kFirstHandle), orpheus::SessionGraphError::OK);
  ASSERT_EQ(limited->prepareClipAudio(kSecondHandle), orpheus::SessionGraphError::OK);
  CountingCallback limitedCallback;
  limited->setCallback(&limitedCallback);

  ASSERT_EQ(limited->startClip(kFirstHandle, 51), orpheus::SessionGraphError::OK);
  ASSERT_EQ(limited->startClip(kSecondHandle, 52), orpheus::SessionGraphError::OK);
  std::array<float, kBlockFrames> limitedLeft{};
  std::array<float, kBlockFrames> limitedRight{};
  float* outputs[2] = {limitedLeft.data(), limitedRight.data()};
  limited->processAudio(outputs, 2, kBlockFrames);

  const auto settlements = limited->getStartSettlementSnapshot();
  ASSERT_EQ(settlements.entryCount, 2u);
  EXPECT_EQ(settlements.entries[0].requestTag, 51u);
  EXPECT_EQ(settlements.entries[0].outcome, orpheus::StartSettlementOutcome::Started);
  EXPECT_NE(settlements.entries[0].voiceId, 0u);
  EXPECT_EQ(settlements.entries[1].requestTag, 52u);
  EXPECT_EQ(settlements.entries[1].outcome,
            orpheus::StartSettlementOutcome::ActiveVoiceLimitRejected);
  EXPECT_EQ(settlements.entries[1].voiceId, 0u);
  EXPECT_EQ(settlements.entries[1].position.samples, 0);

  limited->processCallbacks();
  ASSERT_EQ(limitedCallback.started.size(), 1u);
  ASSERT_EQ(limitedCallback.rejectedTags.size(), 1u);
  EXPECT_EQ(limitedCallback.started[0].requestTag, 51u);
  EXPECT_EQ(limitedCallback.rejectedTags[0], 52u);
}

TEST_F(CallbackLossTelemetryTest, StartSettlementRetentionOverwritesOldestRecordsInOrder) {
  ASSERT_EQ(transport->setMaxVoicesPerClip(2), orpheus::SessionGraphError::OK);
  constexpr uint64_t kFirstTag = 1000;
  constexpr uint64_t kStartCount = 66;
  for (uint64_t index = 0; index < kStartCount; ++index) {
    ASSERT_EQ(transport->startClip(kFirstHandle, kFirstTag + index),
              orpheus::SessionGraphError::OK);
  }
  renderBlock();

  const auto snapshot = transport->getStartSettlementSnapshot();
  ASSERT_EQ(snapshot.entryCount, orpheus::kStartSettlementSnapshotCapacity);
  EXPECT_EQ(snapshot.oldestSequence, 3u);
  EXPECT_EQ(snapshot.latestSequence, kStartCount);
  EXPECT_EQ(snapshot.overwrittenCount, 2u);
  for (size_t index = 0; index < orpheus::kStartSettlementSnapshotCapacity; ++index) {
    EXPECT_EQ(snapshot.entries[index].sequence, index + 3);
    EXPECT_EQ(snapshot.entries[index].requestTag, kFirstTag + index + 2);
    EXPECT_EQ(snapshot.entries[index].outcome, orpheus::StartSettlementOutcome::Started);
  }

  const auto voices = transport->getActiveVoiceSnapshot();
  const auto* entry = findEntry(voices, kFirstHandle);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->newestStartRequestTag, kFirstTag + kStartCount - 1);
}

TEST_F(CallbackLossTelemetryTest, StartSettlementSequenceSaturatesAndBlocksOnlyLaterTaggedStarts) {
  auto* concrete = dynamic_cast<orpheus::TransportController*>(transport.get());
  ASSERT_NE(concrete, nullptr);
  concrete->setStartSettlementSequenceForTesting(std::numeric_limits<uint64_t>::max() - 1);

  ASSERT_EQ(transport->startClip(kFirstHandle, 71), orpheus::SessionGraphError::OK);
  renderBlock();
  auto snapshot = transport->getStartSettlementSnapshot();
  ASSERT_EQ(snapshot.entryCount, 1u);
  EXPECT_EQ(snapshot.entries[0].sequence, std::numeric_limits<uint64_t>::max());
  EXPECT_EQ(snapshot.sequenceExhausted, 1u);
  const size_t taggedVoiceCount = transport->getActiveVoiceSnapshot().totalActiveVoiceCount;

  ASSERT_EQ(transport->startClip(kFirstHandle, 72), orpheus::SessionGraphError::OK);
  renderBlock();
  snapshot = transport->getStartSettlementSnapshot();
  EXPECT_EQ(snapshot.entryCount, 1u);
  EXPECT_EQ(snapshot.latestSequence, std::numeric_limits<uint64_t>::max());
  EXPECT_EQ(transport->getActiveVoiceSnapshot().totalActiveVoiceCount, taggedVoiceCount);

  ASSERT_EQ(transport->startClip(kFirstHandle), orpheus::SessionGraphError::OK);
  renderBlock();
  EXPECT_GT(transport->getActiveVoiceSnapshot().totalActiveVoiceCount, taggedVoiceCount);
  EXPECT_EQ(transport->getStartSettlementSnapshot().entryCount, 1u);

  transport->processCallbacks();
  ASSERT_EQ(callback.started.size(), 2u);
  EXPECT_EQ(callback.started[0].requestTag, 71u);
  EXPECT_EQ(callback.started[1].requestTag, 0u);
}

TEST_F(CallbackLossTelemetryTest, ReusedVoiceIdRetainsExactStartTagThroughStopAndRefire) {
  auto* concrete = dynamic_cast<orpheus::TransportController*>(transport.get());
  ASSERT_NE(concrete, nullptr);

  ASSERT_EQ(transport->startClip(kFirstHandle, 81), orpheus::SessionGraphError::OK);
  renderBlock();
  transport->processCallbacks();
  ASSERT_EQ(callback.started.size(), 1u);
  const uint32_t recycledVoiceId = callback.started[0].voiceId;
  ASSERT_NE(recycledVoiceId, 0u);

  ASSERT_EQ(transport->stopClip(kFirstHandle), orpheus::SessionGraphError::OK);
  for (size_t block = 0; block < 10; ++block) {
    renderBlock();
  }
  transport->processCallbacks();
  ASSERT_EQ(callback.stopped.size(), 1u);
  EXPECT_EQ(callback.stopped[0].requestTag, 81u);
  EXPECT_EQ(callback.stopped[0].voiceId, recycledVoiceId);

  concrete->setNextVoiceIdForTesting(recycledVoiceId);
  ASSERT_EQ(transport->startClip(kFirstHandle, 82), orpheus::SessionGraphError::OK);
  renderBlock();
  transport->processCallbacks();
  ASSERT_EQ(callback.started.size(), 2u);
  EXPECT_EQ(callback.started[1].requestTag, 82u);
  EXPECT_EQ(callback.started[1].voiceId, recycledVoiceId);
  const auto activeSnapshot = transport->getActiveVoiceSnapshot();
  const auto* active = findEntry(activeSnapshot, kFirstHandle);
  ASSERT_NE(active, nullptr);
  EXPECT_EQ(active->newestStartRequestTag, 82u);
  EXPECT_EQ(active->newestVoiceId, recycledVoiceId);
}

TEST_F(CallbackLossTelemetryTest, StartSettlementSnapshotIsCoherentDuringConcurrentPublication) {
  ASSERT_EQ(transport->setMaxVoicesPerClip(2), orpheus::SessionGraphError::OK);
  std::atomic<bool> finished{false};
  std::atomic<bool> coherent{true};

  const auto validate = [](const orpheus::StartSettlementSnapshot& snapshot) {
    if (snapshot.schemaVersion != orpheus::kStartSettlementSnapshotSchemaVersion ||
        snapshot.entryCount > orpheus::kStartSettlementSnapshotCapacity) {
      return false;
    }
    if (snapshot.entryCount == 0) {
      return snapshot.oldestSequence == 0 && snapshot.latestSequence == 0;
    }
    if (snapshot.oldestSequence != snapshot.entries[0].sequence ||
        snapshot.latestSequence != snapshot.entries[snapshot.entryCount - 1].sequence) {
      return false;
    }
    for (uint32_t index = 0; index < snapshot.entryCount; ++index) {
      const auto& record = snapshot.entries[index];
      if (record.requestTag == 0 || record.handle != kFirstHandle ||
          record.outcome != orpheus::StartSettlementOutcome::Started || record.voiceId == 0) {
        return false;
      }
      if (index != 0 && record.sequence != snapshot.entries[index - 1].sequence + 1) {
        return false;
      }
    }
    return true;
  };

  std::thread writer([&] {
    for (uint64_t tag = 1; tag <= 256; ++tag) {
      if (transport->startClip(kFirstHandle, tag) != orpheus::SessionGraphError::OK) {
        coherent.store(false, std::memory_order_release);
        break;
      }
      renderBlock();
    }
    finished.store(true, std::memory_order_release);
  });

  while (!finished.load(std::memory_order_acquire)) {
    if (!validate(transport->getStartSettlementSnapshot())) {
      coherent.store(false, std::memory_order_release);
      break;
    }
  }
  writer.join();

  EXPECT_TRUE(coherent.load(std::memory_order_acquire));
  EXPECT_TRUE(validate(transport->getStartSettlementSnapshot()));
}

} // namespace
