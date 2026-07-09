// SPDX-License-Identifier: MIT
// ORP136 §2.2: runtime realtime-safety harness for the transport callback.
//
// tools/realtime_audit.py is a static gate; this test is the RUNTIME gate the
// ORP134 streaming-reader sprint depends on ("do not start G1 without it").
// It must be able to red/green the claim "no allocations / no file I/O on the
// audio thread":
//
//  * GuardDetectsViolations proves the harness REDS on a real violation
//    (self-test of the detector, not of the SDK).
//  * ReaderlessMultiClipRenderIsAllocationFree proves the pure render path
//    (voices, fades, gain smoothing, routing, snapshot publish, event ring)
//    performs ZERO C++ allocations/deallocations on the callback thread.
//  * FileBackedRenderStillDoesFileIO_KnownDebt pins the KNOWN architecture
//    debt (docs/REALTIME_AUDIT.md, KNOWN_DEBT_PATTERNS): file-backed clips
//    still read from disk inside processAudio(). It asserts the I/O is
//    OBSERVED — when the ORP134 G1 streaming reader lands, this test will
//    fail, and must be flipped to assert ZERO I/O (the strict gate).
//
// Lock/race coverage is deliberately delegated to the ThreadSanitizer suite
// (voice_state_tsan_test + this file built under -fsanitize=thread); a
// dlsym-interpose lock detector would fight the sanitizers' own interceptors.

#define ORPHEUS_TEST_DEFINE_RT_ALLOC_HOOKS
#include "../support/rt_guard.hpp"

#include "../../src/core/transport/transport_controller.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

using namespace orpheus;
using orpheus::tests::support::ProcIoCounters;
using orpheus::tests::support::readProcIoCounters;
using orpheus::tests::support::RtGuardState;
using orpheus::tests::support::RtSection;

namespace {

constexpr uint32_t kSampleRate = 48000;
constexpr size_t kBufferFrames = 512;

// Minimal WAV writer (16-bit PCM stereo) — mirrors multi_clip_stress_test.
std::string writeSineWav(const std::filesystem::path& dir, const std::string& name, float freq,
                         float durationSeconds, uint32_t sampleRate = kSampleRate) {
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
                              static_cast<float>(kSampleRate));
    int16_t v = static_cast<int16_t>(s * 32767.0f);
    file.write(reinterpret_cast<const char*>(&v), 2); // L
    file.write(reinterpret_cast<const char*>(&v), 2); // R
  }
  file.close();
  return filepath;
}

class RealtimeHarnessTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_tempDir = std::filesystem::temp_directory_path() / "orp136_rt_harness";
    std::filesystem::create_directories(m_tempDir);
    m_transport = std::make_unique<TransportController>(nullptr, kSampleRate);
    m_left.assign(kBufferFrames, 0.0f);
    m_right.assign(kBufferFrames, 0.0f);
    m_buffers[0] = m_left.data();
    m_buffers[1] = m_right.data();
    RtGuardState::reset();
  }

  void TearDown() override {
    m_transport.reset();
    std::error_code ec;
    std::filesystem::remove_all(m_tempDir, ec);
  }

  // One simulated audio callback, guarded as a realtime section.
  void guardedCallback() {
    RtSection section;
    m_transport->processAudio(m_buffers, 2, kBufferFrames);
  }

  std::filesystem::path m_tempDir;
  std::unique_ptr<TransportController> m_transport;
  std::vector<float> m_left;
  std::vector<float> m_right;
  float* m_buffers[2] = {nullptr, nullptr};
};

} // namespace

// ============================================================================
// Detector self-test: the harness must RED on a real violation.
// ============================================================================

TEST_F(RealtimeHarnessTest, GuardDetectsViolations) {
  RtGuardState::reset();
  ASSERT_EQ(RtGuardState::totalViolations(), 0u);

  {
    RtSection section;
    volatile int* leak = new int(42); // deliberate violation
    delete leak;                      // second deliberate violation
  }

  EXPECT_EQ(RtGuardState::allocViolations(), 1u) << "operator new inside an RtSection must red";
  EXPECT_EQ(RtGuardState::deallocViolations(), 1u)
      << "operator delete inside an RtSection must red";

  // Outside a section, allocations are not violations.
  RtGuardState::reset();
  volatile int* fine = new int(7);
  delete fine;
  EXPECT_EQ(RtGuardState::totalViolations(), 0u);
}

// ============================================================================
// GREEN gate: the pure render path performs zero allocations.
// ============================================================================

TEST_F(RealtimeHarnessTest, ReaderlessMultiClipRenderIsAllocationFree) {
  constexpr int kNumClips = 16;
  constexpr int kNumBuffers = 500;

  // Reader-less clips exercise the voice/fade/gain/routing/snapshot/event
  // machinery without touching the (known-debt) file-read path.
  for (int i = 0; i < kNumClips; ++i) {
    ASSERT_EQ(m_transport->startClip(static_cast<ClipHandle>(i + 1)), SessionGraphError::OK);
  }

  RtGuardState::reset();
  for (int buffer = 0; buffer < kNumBuffers; ++buffer) {
    guardedCallback();

    // UI-thread work between callbacks (unguarded): churn voices so the
    // guarded path also covers voice add/remove/restart and event posting.
    if (buffer % 50 == 10) {
      ClipHandle handle = static_cast<ClipHandle>(buffer % kNumClips + 1);
      m_transport->stopClip(handle);
    }
    if (buffer % 50 == 30) {
      ClipHandle handle = static_cast<ClipHandle>(buffer % kNumClips + 1);
      m_transport->startClip(handle);
    }
    if (buffer % 97 == 0) {
      m_transport->stopOtherClips(static_cast<ClipHandle>(1));
    }
    if (buffer % 100 == 60) {
      m_transport->processCallbacks(); // drain event ring on the "UI thread"
    }
  }

  EXPECT_EQ(RtGuardState::allocViolations(), 0u)
      << "processAudio() allocated on the callback thread (" << RtGuardState::allocViolationBytes()
      << " bytes)";
  EXPECT_EQ(RtGuardState::deallocViolations(), 0u)
      << "processAudio() deallocated on the callback thread";

  std::cout << "[RT Harness] reader-less stress: " << kNumBuffers << " buffers x " << kNumClips
            << " clips, alloc violations = " << RtGuardState::allocViolations()
            << ", dealloc violations = " << RtGuardState::deallocViolations() << "\n";
}

// ============================================================================
// KNOWN DEBT pin: file-backed clips still do file I/O inside the callback.
//
// >>> ORP134 G1 FLIP POINT <<<
// When the streaming reader lands and decode moves off the audio thread,
// this test WILL FAIL (the I/O delta drops to ~0). That failure is the
// signal to flip the assertions below to EXPECT_EQ(0, ...) — turning this
// harness into the strict runtime gate that ORP134's acceptance requires —
// and to flip tools/realtime_audit.py --fail-known-debt on in CI.
// ============================================================================

TEST_F(RealtimeHarnessTest, FileBackedRenderStillDoesFileIO_KnownDebt) {
  auto before = readProcIoCounters();
  if (!before.has_value()) {
    GTEST_SKIP() << "/proc/self/io not available on this platform";
  }

  constexpr int kNumClips = 8;
  constexpr int kNumBuffers = 300; // ~3.2s of audio -> far beyond any prefetch

  for (int i = 0; i < kNumClips; ++i) {
    ClipHandle handle = static_cast<ClipHandle>(i + 1);
    std::string path = writeSineWav(m_tempDir, "debt_" + std::to_string(i) + ".wav",
                                    220.0f + 55.0f * static_cast<float>(i), 4.0f);
    ASSERT_EQ(m_transport->registerClipAudio(handle, path), SessionGraphError::OK);
    ASSERT_EQ(m_transport->prepareClipAudio(handle), SessionGraphError::OK);
    ASSERT_EQ(m_transport->startClip(handle), SessionGraphError::OK);
  }

  // Sample I/O counters around ONLY the guarded render loop.
  before = readProcIoCounters();
  ASSERT_TRUE(before.has_value());
  RtGuardState::reset();

  for (int buffer = 0; buffer < kNumBuffers; ++buffer) {
    guardedCallback();
  }

  auto after = readProcIoCounters();
  ASSERT_TRUE(after.has_value());

  const std::uint64_t readCalls = after->syscr - before->syscr;
  const std::uint64_t readBytes = after->rchar - before->rchar;

  std::cout << "[RT Harness] file-backed render: " << readCalls << " read syscalls, " << readBytes
            << " bytes read, alloc violations = " << RtGuardState::allocViolations()
            << " (bytes = " << RtGuardState::allocViolationBytes() << ")\n";

  // The debt is real and observable: libsndfile decodes on the audio thread
  // (transport_controller.cpp render loop -> readSamples -> sf_readf_float).
  EXPECT_GT(readCalls, 0u)
      << "No file I/O observed on the audio thread. If the ORP134 streaming reader has "
         "landed, FLIP this test: assert readCalls == 0 / readBytes == 0 and enable "
         "realtime_audit.py --fail-known-debt in CI. Do not leave this inverted.";
  EXPECT_GT(readBytes, 0u);
}

// ============================================================================
// Callback duration accounting (report always; bound only unsanitized).
// ============================================================================

TEST_F(RealtimeHarnessTest, CallbackDurationWithinBudget) {
  constexpr int kNumClips = 16;
  constexpr int kNumBuffers = 300;

  for (int i = 0; i < kNumClips; ++i) {
    ASSERT_EQ(m_transport->startClip(static_cast<ClipHandle>(i + 1)), SessionGraphError::OK);
  }
  guardedCallback(); // warm-up: materialize voices

  double maxUs = 0.0;
  double totalUs = 0.0;
  for (int buffer = 0; buffer < kNumBuffers; ++buffer) {
    auto start = std::chrono::steady_clock::now();
    guardedCallback();
    auto end = std::chrono::steady_clock::now();
    double us = std::chrono::duration<double, std::micro>(end - start).count();
    maxUs = std::max(maxUs, us);
    totalUs += us;
  }

  const double budgetUs = (static_cast<double>(kBufferFrames) * 1e6) / kSampleRate; // ~10667us
  std::cout << "[RT Harness] duration: avg " << (totalUs / kNumBuffers) << " us, max " << maxUs
            << " us, budget " << budgetUs << " us\n";

  // Wall-clock bounds are skipped under sanitizers (same policy as the
  // waveform and multi-clip perf bounds).
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
  constexpr bool kUnderSanitizer = true;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) ||                         \
    __has_feature(memory_sanitizer) || __has_feature(undefined_behavior_sanitizer)
  constexpr bool kUnderSanitizer = true;
#else
  constexpr bool kUnderSanitizer = false;
#endif
#else
  constexpr bool kUnderSanitizer = false;
#endif

  if (!kUnderSanitizer) {
    EXPECT_LT(maxUs, budgetUs) << "a single reader-less callback exceeded the realtime budget";
  } else {
    std::cout << "  - duration bound skipped under sanitizer\n";
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
