// SPDX-License-Identifier: MIT
//
// ORP127 T2 — ThreadSanitizer stress harness for per-voice transport state.
//
// This test deliberately hammers the UI-thread mutation sites
// (startClip / stopClip / restartClip / seekClip) while the audio thread runs
// processAudio() in a tight loop. Before ORP127 T3, several ActiveClip fields
// (currentSample, isStopping, fadeOutGain, hasLoopedOnce, reader) are mutated
// from BOTH threads without synchronization; running this under
// ThreadSanitizer surfaces those data races.
//
// Build with TSan (separate build dir, TSan is incompatible with ASan):
//   cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug \
//         -DORP_ENABLE_ASAN=OFF -DORP_ENABLE_UBSAN=OFF \
//         -DCMAKE_CXX_FLAGS="-fsanitize=thread" \
//         -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
//   cmake --build build-tsan --target voice_state_tsan_test
//   ./build-tsan/tests/transport/voice_state_tsan_test
//
// The harness itself always passes (it asserts liveness, not correctness);
// the *verdict* is whether TSan reports races. Post-T3 it must be clean.

#define ORPHEUS_TEST_DEFINE_RT_ALLOC_HOOKS
#include "../support/rt_guard.hpp"
#include <gtest/gtest.h>
#include <orpheus/audio_file_reader.h>
#include <orpheus/transport_controller.h>

// Access implementation directly for processAudio() + registerClipAudio()
#include "../../src/core/transport/transport_controller.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

// MSVC's <cmath> does not define M_PI without _USE_MATH_DEFINES; guard it so
// the Windows build resolves the constant. Matches the codebase's M_PI_2 guard.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using orpheus::tests::support::RtGuardState;
using orpheus::tests::support::RtSection;

using namespace orpheus;

namespace {

// Minimal WAV writer (16-bit PCM stereo) — mirrors multi_clip_stress_test.
std::string writeSineWav(const std::filesystem::path& dir, const std::string& name, float freq,
                         float durationSeconds, uint32_t sampleRate = 48000) {
  const uint16_t numChannels = 2;
  const int64_t numFrames = static_cast<int64_t>(durationSeconds * sampleRate);

  std::string filepath = (dir / name).string();
  std::ofstream file(filepath, std::ios::binary);

  const uint32_t dataSize = static_cast<uint32_t>(numFrames * numChannels * sizeof(int16_t));
  const uint32_t fileSize = 36 + dataSize;
  const uint32_t fmtSize = 16;
  const uint16_t audioFormat = 1; // PCM
  const uint16_t blockAlign = numChannels * 2;
  const uint32_t byteRate = sampleRate * blockAlign;
  const uint16_t bitsPerSample = 16;

  file.write("RIFF", 4);
  file.write(reinterpret_cast<const char*>(&fileSize), 4);
  file.write("WAVE", 4);
  file.write("fmt ", 4);
  file.write(reinterpret_cast<const char*>(&fmtSize), 4);
  file.write(reinterpret_cast<const char*>(&audioFormat), 2);
  file.write(reinterpret_cast<const char*>(&numChannels), 2);
  file.write(reinterpret_cast<const char*>(&sampleRate), 4);
  file.write(reinterpret_cast<const char*>(&byteRate), 4);
  file.write(reinterpret_cast<const char*>(&blockAlign), 2);
  file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
  file.write("data", 4);
  file.write(reinterpret_cast<const char*>(&dataSize), 4);

  for (int64_t i = 0; i < numFrames; ++i) {
    float s = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * freq * static_cast<float>(i) /
                              static_cast<float>(sampleRate));
    int16_t v = static_cast<int16_t>(s * 32767.0f);
    file.write(reinterpret_cast<const char*>(&v), 2); // L
    file.write(reinterpret_cast<const char*>(&v), 2); // R
  }
  file.close();
  return filepath;
}

} // namespace

class VoiceStateTsanTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_tempDir = std::filesystem::temp_directory_path() / "orp127_tsan";
    std::filesystem::create_directories(m_tempDir);
    m_transport = std::make_unique<TransportController>(
        nullptr, TransportConfig{.sampleRate = static_cast<uint32_t>(48000)});
  }

  void TearDown() override {
    m_transport.reset();
    std::error_code ec;
    std::filesystem::remove_all(m_tempDir, ec);
  }

  std::filesystem::path m_tempDir;
  std::unique_ptr<TransportController> m_transport;
};

// Core race harness: audio thread renders continuously while the UI thread
// fires the full mutation surface (start/stop/restart/seek) at maximum rate.
TEST_F(VoiceStateTsanTest, HammerVoiceMutationsUnderConcurrentRender) {
  constexpr int kNumClips = 8;
  constexpr size_t kNumFrames = 512;
  constexpr size_t kNumChannels = 2;

  // Register clips backed by real audio files so readers are live (exercises
  // the shared_ptr reader swap race in restartClip/seekClip/OUT handling).
  std::vector<ClipHandle> clips;
  for (int i = 0; i < kNumClips; ++i) {
    ClipHandle handle = static_cast<ClipHandle>(i + 1);
    std::string path = writeSineWav(m_tempDir, "tsan_clip_" + std::to_string(i) + ".wav",
                                    220.0f + static_cast<float>(i) * 55.0f, 2.0f);
    ASSERT_EQ(m_transport->registerClipAudio(handle, path), SessionGraphError::OK);
    clips.push_back(handle);
  }

  std::atomic<bool> stop{false};

  std::vector<std::vector<float>> outBufs(kNumChannels, std::vector<float>(kNumFrames, 0.0f));
  std::vector<float*> outPtrs;
  for (auto& b : outBufs)
    outPtrs.push_back(b.data());

  // Materialize voices before the concurrent phase so restartClip/seekClip
  // have live ActiveClip instances to mutate (they early-out when no voice is
  // active). Start all clips, then pump the render to process Start commands.
  for (ClipHandle h : clips)
    ASSERT_EQ(m_transport->startClip(h), SessionGraphError::OK);
  for (int i = 0; i < 4; ++i)
    m_transport->processAudio(outPtrs.data(), kNumChannels, kNumFrames);
  ASSERT_EQ(m_transport->getTotalActiveVoiceCount(), clips.size());

  // Audio thread: render loop, no sleep — maximize overlap with UI mutations.
  std::thread audioThread([&]() {
    while (!stop.load(std::memory_order_relaxed)) {
      m_transport->processAudio(outPtrs.data(), kNumChannels, kNumFrames);
    }
  });

  // UI thread (this thread): fire the full mutation surface repeatedly.
  // Deterministic index rotation (no RNG — keeps the race window reproducible
  // and avoids Math.random-style nondeterminism across runs).
  constexpr int kIterations = 4000;
  const auto expectAdmission = [](SessionGraphError result) {
    EXPECT_TRUE(result == SessionGraphError::OK || result == SessionGraphError::NotReady);
  };
  for (int iter = 0; iter < kIterations; ++iter) {
    ClipHandle h = clips[static_cast<size_t>(iter) % clips.size()];
    // restartClip + seekClip run every iteration to cover the
    // formerly direct UI-thread mutation sites (currentSample, reader, isStopping,
    // hasLoopedOnce) that F-SDK-1 flags. start/stop/callback rotate in to keep
    // the voice population churning.
    expectAdmission(m_transport->restartClip(h));
    expectAdmission(m_transport->seekClip(h, (iter * 137) % (48000 * 2)));
    switch (iter % 3) {
    case 0:
      expectAdmission(m_transport->startClip(h));
      break;
    case 1:
      expectAdmission(m_transport->stopClip(h));
      break;
    case 2:
      m_transport->processCallbacks(); // drain callback ring on UI thread
      break;
    }
  }

  stop.store(true, std::memory_order_relaxed);
  audioThread.join();

  m_transport->processAudio(outPtrs.data(), kNumChannels, kNumFrames);
  ASSERT_EQ(m_transport->panic(), SessionGraphError::OK);
  m_transport->processAudio(outPtrs.data(), kNumChannels, kNumFrames);
  m_transport->processCallbacks();
  EXPECT_EQ(m_transport->getTotalActiveVoiceCount(), 0u);
  const auto telemetry = m_transport->getCommandIngressTelemetry();
  EXPECT_EQ(telemetry.processedCount, telemetry.admittedCount);
  EXPECT_EQ(telemetry.attemptedCount,
            telemetry.admittedCount + telemetry.slotUnavailableCount +
                telemetry.publicationContentionCount + telemetry.preparationRejectedCount);
}

// Second harness: concurrent queries (isClipPlaying / getClipState /
// getClipPosition) from the UI thread while render + mutations run. This
// targets the m_activeClipCount iteration race called out in F-SDK-1.
TEST_F(VoiceStateTsanTest, HammerQueriesUnderConcurrentRender) {
  constexpr int kNumClips = 6;
  constexpr size_t kNumFrames = 256;
  constexpr size_t kNumChannels = 2;

  std::vector<ClipHandle> clips;
  for (int i = 0; i < kNumClips; ++i) {
    ClipHandle handle = static_cast<ClipHandle>(i + 1);
    std::string path = writeSineWav(m_tempDir, "tsan_q_" + std::to_string(i) + ".wav",
                                    330.0f + static_cast<float>(i) * 40.0f, 1.5f);
    ASSERT_EQ(m_transport->registerClipAudio(handle, path), SessionGraphError::OK);
    clips.push_back(handle);
    ASSERT_EQ(m_transport->startClip(handle), SessionGraphError::OK);
  }

  std::atomic<bool> stop{false};

  std::vector<std::vector<float>> outBufs(kNumChannels, std::vector<float>(kNumFrames, 0.0f));
  std::vector<float*> outPtrs;
  for (auto& b : outBufs)
    outPtrs.push_back(b.data());
  IRoutingMatrix* routing = m_transport->getRoutingMatrix();
  ASSERT_NE(routing, nullptr);
  const RoutingConfig routingConfig = routing->getConfig();

  std::thread audioThread([&]() {
    while (!stop.load(std::memory_order_relaxed)) {
      m_transport->processAudio(outPtrs.data(), kNumChannels, kNumFrames);
    }
  });

  volatile int sink = 0; // prevent the queries being optimized away
  uint32_t invalidMeterSnapshots = 0;

  constexpr int kIterations = 6000;
  GroupOutputMeterSnapshot meterSnapshot;
  const auto expectAdmission = [](SessionGraphError result) {
    EXPECT_TRUE(result == SessionGraphError::OK || result == SessionGraphError::NotReady);
  };
  for (int iter = 0; iter < kIterations; ++iter) {
    ClipHandle h = clips[static_cast<size_t>(iter) % clips.size()];
    sink += static_cast<int>(m_transport->isClipPlaying(h));
    sink += static_cast<int>(m_transport->getClipState(h));
    sink += static_cast<int>(m_transport->getClipPosition(h) & 0xFF);

    routing->copyGroupOutputMeterSnapshot(meterSnapshot);
    if (meterSnapshot.coherent != 0) {
      const bool groupCountValid = meterSnapshot.group_count <= kRoutingControlMaxGroups &&
                                   meterSnapshot.group_count <= routingConfig.num_groups;
      if (!groupCountValid) {
        ++invalidMeterSnapshots;
      } else {
        for (RoutingGroupIndex group = 0; group < meterSnapshot.group_count; ++group) {
          const auto& frame = meterSnapshot.groups[group];
          if (frame.logical_lane_count > kRoutingMaxOutputs ||
              static_cast<uint32_t>(frame.routing_output_start) + frame.logical_lane_count >
                  routingConfig.num_outputs) {
            ++invalidMeterSnapshots;
          }
        }
      }
    }

    if ((iter % 5) == 0) {
      const auto channel = static_cast<RoutingChannelIndex>(
          static_cast<size_t>(iter) % (static_cast<size_t>(routingConfig.num_channels)));
      const auto group =
          static_cast<RoutingGroupIndex>(static_cast<size_t>(iter) % routingConfig.num_groups);
      const auto lane =
          static_cast<RoutingOutputIndex>(static_cast<size_t>(iter) % routingConfig.num_outputs);
      routing->setChannelRoute(channel, group, lane);
    }
    if ((iter % 11) == 0) {
      const auto group =
          static_cast<RoutingGroupIndex>(static_cast<size_t>(iter) % routingConfig.num_groups);
      const auto output =
          static_cast<RoutingOutputIndex>(static_cast<size_t>(iter) % routingConfig.num_outputs);
      routing->setGroupOutputRoute(group, output, 1);
    }
    if ((iter % 3) == 0)
      expectAdmission(m_transport->stopClip(h));
    if ((iter % 7) == 0)
      expectAdmission(m_transport->startClip(h));
  }

  stop.store(true, std::memory_order_relaxed);
  audioThread.join();
  m_transport->processAudio(outPtrs.data(), kNumChannels, kNumFrames);
  ASSERT_EQ(m_transport->panic(), SessionGraphError::OK);
  m_transport->processAudio(outPtrs.data(), kNumChannels, kNumFrames);
  m_transport->processCallbacks();
  EXPECT_EQ(m_transport->getTotalActiveVoiceCount(), 0u);
  EXPECT_EQ(invalidMeterSnapshots, 0u)
      << "coherent group-output meter snapshot exceeded configured bounds";
  (void)sink;
}

// D3 stress fixture: the ingress contract is exercised independently from
// TSan's race verdict.  All producer-side ledgers are preallocated; no
// producer retries a refused operation.
TEST_F(VoiceStateTsanTest, EightProducersSaturateIngressAt96k64) {
  constexpr size_t kProducers = 8;
  constexpr size_t kFrames = 64;
  constexpr size_t kCapacity = 255;
  constexpr size_t kIterations = 4096;
  m_transport = std::make_unique<TransportController>(
      nullptr, TransportConfig{.sampleRate = 96000, .outputChannels = 2, .maxBlockFrames = 64});
  std::array<ClipHandle, kProducers> handles{};
  std::array<ClipMetadata, kProducers> metadata{};
  for (size_t i = 0; i < kProducers; ++i) {
    handles[i] = static_cast<ClipHandle>(i + 1);
    const auto path = writeSineWav(m_tempDir, "ingress_" + std::to_string(i) + ".wav",
                                   220.0f + 17.0f * i, 2.0f, 96000);
    ASSERT_EQ(m_transport->registerClipAudio(handles[i], path), SessionGraphError::OK);
    ASSERT_EQ(m_transport->prepareClipAudio(handles[i]), SessionGraphError::OK);
    const auto registered = m_transport->getClipMetadata(handles[i]);
    ASSERT_TRUE(registered.has_value());
    metadata[i] = *registered;
  }
  std::array<float, kFrames> left{}, right{};
  float* output[]{left.data(), right.data()};
  const auto render = [&] {
    RtSection section;
    m_transport->processAudio(output, 2, kFrames);
  };
  const auto drain = [&] {
    render();
    m_transport->processCallbacks();
  };
  const auto partition = [](const TransportCommandIngressTelemetry& value) {
    EXPECT_EQ(value.attemptedCount, value.admittedCount + value.slotUnavailableCount +
                                        value.publicationContentionCount +
                                        value.preparationRejectedCount);
  };
  RtGuardState::reset();

  // Consumer is stopped: reserve all nodes, then exactly eight producers each
  // make 64 additional valid calls without retrying.
  for (size_t i = 0; i < kCapacity; ++i)
    ASSERT_EQ(m_transport->stopClip(handles[i % kProducers]), SessionGraphError::OK);
  std::array<std::array<SessionGraphError, 64>, kProducers> refused{};
  std::array<std::thread, kProducers> producers;
  std::barrier forceStart(static_cast<std::ptrdiff_t>(kProducers));
  for (size_t p = 0; p < kProducers; ++p)
    producers[p] = std::thread([&, p] {
      forceStart.arrive_and_wait();
      for (size_t i = 0; i < 64; ++i)
        refused[p][i] = m_transport->stopClip(handles[p]);
    });
  for (auto& producer : producers)
    producer.join();
  for (const auto& row : refused)
    for (auto result : row)
      EXPECT_EQ(result, SessionGraphError::NotReady);
  auto telemetry = m_transport->getCommandIngressTelemetry();
  EXPECT_EQ(telemetry.attemptedCount, 767u);
  EXPECT_EQ(telemetry.admittedCount, 255u);
  EXPECT_EQ(telemetry.slotUnavailableCount, 512u);
  partition(telemetry);
  drain();

  // Exactly one callback overlaps each 1024-call round. At most 510 nodes can
  // be admitted; the 32-CAS contention budget cannot explain all other calls.
  for (size_t round = 0; round < 4; ++round) {
    const auto before = m_transport->getCommandIngressTelemetry();
    ASSERT_EQ(before.processedCount, before.admittedCount);
    std::barrier begin(static_cast<std::ptrdiff_t>(kProducers + 1));
    std::barrier end(static_cast<std::ptrdiff_t>(kProducers + 1));
    std::array<std::array<SessionGraphError, 128>, kProducers> results{};
    for (size_t p = 0; p < kProducers; ++p)
      producers[p] = std::thread([&, p] {
        begin.arrive_and_wait();
        for (size_t i = 0; i < 128; ++i)
          results[p][i] = m_transport->stopClip(handles[p]);
        end.arrive_and_wait();
      });
    begin.arrive_and_wait();
    render();
    end.arrive_and_wait();
    for (auto& producer : producers)
      producer.join();
    size_t admitted = 0;
    for (const auto& row : results)
      for (auto result : row) {
        EXPECT_TRUE(result == SessionGraphError::OK || result == SessionGraphError::NotReady);
        admitted += result == SessionGraphError::OK;
      }
    const auto after = m_transport->getCommandIngressTelemetry();
    EXPECT_EQ(after.attemptedCount - before.attemptedCount, 1024u);
    EXPECT_EQ(after.admittedCount - before.admittedCount, admitted);
    EXPECT_GT(after.slotUnavailableCount, before.slotUnavailableCount);
    partition(after);
    drain();
  }

  std::array<std::array<SessionGraphError, kIterations>, kProducers> ledger{};
  std::barrier begin(static_cast<std::ptrdiff_t>(kProducers + 2));
  std::atomic<bool> done{false};
  std::thread audio([&] {
    begin.arrive_and_wait();
    while (!done.load(std::memory_order_acquire))
      render();
    render(); // Every publisher has returned before done is published.
  });
  std::thread pump([&] {
    begin.arrive_and_wait();
    while (!done.load(std::memory_order_acquire)) {
      m_transport->processCallbacks();
      (void)m_transport->getActiveVoiceSnapshot();
      (void)m_transport->getCommandIngressTelemetry();
    }
  });
  for (size_t p = 0; p < kProducers; ++p)
    producers[p] = std::thread([&, p] {
      begin.arrive_and_wait();
      for (size_t i = 0; i < kIterations; ++i) {
        const size_t h = (p + i) % kProducers;
        const auto handle = handles[h];
        const StartRequestTag tag = (static_cast<uint64_t>(p + 1) << 48) | (i + 1);
        switch (i % 13) {
        case 0:
          ledger[p][i] = m_transport->startClip(handle, tag);
          break;
        case 1:
          ledger[p][i] = m_transport->startClipWithGroupChoke(handle, tag);
          break;
        case 2:
          ledger[p][i] = m_transport->stopClip(handle);
          break;
        case 3:
          ledger[p][i] = m_transport->stopAllClips();
          break;
        case 4:
          ledger[p][i] = m_transport->panic();
          break;
        case 5:
          ledger[p][i] = m_transport->stopOtherClips(handle);
          break;
        case 6:
          ledger[p][i] = m_transport->updateClipTrimPoints(handle, 0, 96000);
          break;
        case 7:
          ledger[p][i] =
              m_transport->updateClipFades(handle, 0, 0, FadeCurve::Linear, FadeCurve::Linear);
          break;
        case 8:
          ledger[p][i] = m_transport->updateClipGain(handle, -6);
          break;
        case 9:
          ledger[p][i] = m_transport->setClipLoopMode(handle, (i & 1) != 0);
          break;
        case 10:
          ledger[p][i] = m_transport->updateClipMetadata(handle, metadata[h]);
          break;
        case 11:
          ledger[p][i] = m_transport->restartClip(handle);
          break;
        case 12:
          ledger[p][i] = m_transport->seekClip(handle, static_cast<int64_t>(i * 7));
          break;
        }
      }
    });
  for (auto& producer : producers)
    producer.join();
  done.store(true, std::memory_order_release);
  audio.join();
  pump.join();
  m_transport->processCallbacks();
  for (const auto& row : ledger)
    for (auto result : row)
      EXPECT_TRUE(result == SessionGraphError::OK || result == SessionGraphError::NotReady);
  telemetry = m_transport->getCommandIngressTelemetry();
  partition(telemetry);
  EXPECT_EQ(telemetry.processedCount, telemetry.admittedCount);
  const auto retained = m_transport->getStartSettlementSnapshot();
  for (uint32_t i = 0; i < retained.entryCount; ++i) {
    const auto& record = retained.entries[i];
    const size_t producer = static_cast<size_t>(record.requestTag >> 48) - 1;
    const size_t iteration = static_cast<size_t>(record.requestTag & UINT64_C(0xFFFFFFFFFFFF)) - 1;
    ASSERT_LT(producer, kProducers);
    ASSERT_LT(iteration, kIterations);
    EXPECT_EQ(ledger[producer][iteration], SessionGraphError::OK);
    EXPECT_LE(iteration % 13, 1u);
    EXPECT_EQ(record.handle, handles[(producer + iteration) % kProducers]);
  }

  // With all 128 updates admitted, the final persistent gain must be a final
  // update from one producer, never an overwritten earlier update.
  std::array<std::array<SessionGraphError, 16>, kProducers> shared{};
  for (size_t p = 0; p < kProducers; ++p)
    producers[p] = std::thread([&, p] {
      for (size_t i = 0; i < 16; ++i)
        shared[p][i] = m_transport->updateClipGain(handles[0], -static_cast<float>(p * 16 + i));
    });
  for (auto& producer : producers)
    producer.join();
  for (const auto& row : shared)
    for (auto result : row)
      EXPECT_EQ(result, SessionGraphError::OK);
  const auto sharedMetadata = m_transport->getClipMetadata(handles[0]);
  ASSERT_TRUE(sharedMetadata.has_value());
  bool finalProducerValue = false;
  for (size_t p = 0; p < kProducers; ++p)
    finalProducerValue |= sharedMetadata->gainDb == -static_cast<float>(p * 16 + 15);
  EXPECT_TRUE(finalProducerValue);
  drain();

  const auto beforeRefusedTags = m_transport->getStartSettlementSnapshot();
  for (size_t i = 0; i < kCapacity; ++i)
    ASSERT_EQ(m_transport->stopClip(handles[i % kProducers]), SessionGraphError::OK);
  for (size_t i = 0; i < 512; ++i)
    EXPECT_EQ(m_transport->startClip(handles[i % kProducers], (UINT64_C(0x7A) << 56) | i),
              SessionGraphError::NotReady);
  drain();
  EXPECT_EQ(m_transport->getStartSettlementSnapshot().latestSequence,
            beforeRefusedTags.latestSequence);

  // Eight concurrent producers, eight tags each: complete retention allows
  // exact at-most-once and per-producer FIFO settlement checks.
  std::array<StartRequestTag, 64> tags{};
  std::array<SessionGraphError, 64> admittedTags{};
  for (size_t p = 0; p < kProducers; ++p)
    producers[p] = std::thread([&, p] {
      for (size_t i = 0; i < 8; ++i) {
        const size_t n = p * 8 + i;
        tags[n] = (UINT64_C(0x5B) << 56) | n;
        admittedTags[n] = m_transport->startClip(handles[p], tags[n]);
      }
    });
  for (auto& producer : producers)
    producer.join();
  for (auto result : admittedTags)
    ASSERT_EQ(result, SessionGraphError::OK);
  drain();
  const auto settlements = m_transport->getStartSettlementSnapshot();
  ASSERT_EQ(settlements.entryCount, 64u);
  std::array<bool, 64> seen{};
  std::array<uint64_t, kProducers> lastSequence{};
  std::array<int, kProducers> lastOrdinal{};
  lastOrdinal.fill(-1);
  for (uint32_t i = 0; i < settlements.entryCount; ++i) {
    const auto& record = settlements.entries[i];
    ASSERT_EQ(record.requestTag >> 56, UINT64_C(0x5B));
    const size_t n = static_cast<size_t>(record.requestTag & UINT64_C(0x00FFFFFFFFFFFFFF));
    ASSERT_LT(n, tags.size());
    EXPECT_FALSE(seen[n]);
    seen[n] = true;
    const size_t p = n / 8;
    EXPECT_GT(static_cast<int>(n % 8), lastOrdinal[p]);
    EXPECT_GT(record.sequence, lastSequence[p]);
    lastOrdinal[p] = static_cast<int>(n % 8);
    lastSequence[p] = record.sequence;
  }
  for (bool observed : seen)
    EXPECT_TRUE(observed);
  ASSERT_EQ(m_transport->panic(), SessionGraphError::OK);
  drain();
  EXPECT_EQ(m_transport->getTotalActiveVoiceCount(), 0u);
  telemetry = m_transport->getCommandIngressTelemetry();
  partition(telemetry);
  EXPECT_EQ(telemetry.processedCount, telemetry.admittedCount);
  EXPECT_EQ(RtGuardState::allocViolations(), 0u);
  EXPECT_EQ(RtGuardState::deallocViolations(), 0u);
}
