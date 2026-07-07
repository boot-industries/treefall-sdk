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

#include <gtest/gtest.h>
#include <orpheus/audio_file_reader.h>
#include <orpheus/transport_controller.h>

// Access implementation directly for processAudio() + registerClipAudio()
#include "../../src/core/transport/transport_controller.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

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
    m_transport = std::make_unique<TransportController>(nullptr, 48000);
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
    m_transport->startClip(h);
  for (int i = 0; i < 4; ++i)
    m_transport->processAudio(outPtrs.data(), kNumChannels, kNumFrames);

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
  for (int iter = 0; iter < kIterations; ++iter) {
    ClipHandle h = clips[static_cast<size_t>(iter) % clips.size()];
    // restartClip + seekClip run every iteration — these are the direct
    // UI-thread mutation sites (currentSample, reader, isStopping,
    // hasLoopedOnce) that F-SDK-1 flags. start/stop/callback rotate in to keep
    // the voice population churning.
    m_transport->restartClip(h);
    m_transport->seekClip(h, (iter * 137) % (48000 * 2));
    switch (iter % 3) {
    case 0:
      m_transport->startClip(h);
      break;
    case 1:
      m_transport->stopClip(h);
      break;
    case 2:
      m_transport->processCallbacks(); // drain callback ring on UI thread
      break;
    }
  }

  stop.store(true, std::memory_order_relaxed);
  audioThread.join();

  // Liveness only: if we got here without deadlock/crash the harness ran.
  // TSan (if enabled) is the real judge of correctness.
  SUCCEED();
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
    m_transport->startClip(handle);
  }

  std::atomic<bool> stop{false};

  std::vector<std::vector<float>> outBufs(kNumChannels, std::vector<float>(kNumFrames, 0.0f));
  std::vector<float*> outPtrs;
  for (auto& b : outBufs)
    outPtrs.push_back(b.data());

  std::thread audioThread([&]() {
    int n = 0;
    while (!stop.load(std::memory_order_relaxed)) {
      m_transport->processAudio(outPtrs.data(), kNumChannels, kNumFrames);
      // Occasionally toggle voice population from the audio side via commands.
      if ((n++ & 0x3F) == 0) {
        // no-op; render is the main audio-thread mutation via processCommands
      }
    }
  });

  volatile int sink = 0; // prevent the queries being optimized away
  constexpr int kIterations = 6000;
  for (int iter = 0; iter < kIterations; ++iter) {
    ClipHandle h = clips[static_cast<size_t>(iter) % clips.size()];
    sink += static_cast<int>(m_transport->isClipPlaying(h));
    sink += static_cast<int>(m_transport->getClipState(h));
    sink += static_cast<int>(m_transport->getClipPosition(h) & 0xFF);
    if ((iter % 3) == 0)
      m_transport->stopClip(h);
    if ((iter % 7) == 0)
      m_transport->startClip(h);
  }

  stop.store(true, std::memory_order_relaxed);
  audioThread.join();
  (void)sink;
  SUCCEED();
}
