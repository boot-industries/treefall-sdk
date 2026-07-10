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
//  * FileBackedRenderDoesNoFileIO is the STRICT gate (flipped when the
//    ORP134 G1 prepared/streaming sources landed): file-backed clips render
//    from memory-resident PCM; the audio thread performs no file I/O at all.
//    Before G1 this test asserted the opposite (the KNOWN_DEBT era observed
//    ~2,400 read syscalls / ~4.9 MB per 300 buffers).
//
// Lock/race coverage is deliberately delegated to the ThreadSanitizer suite
// (voice_state_tsan_test + this file built under -fsanitize=thread); a
// dlsym-interpose lock detector would fight the sanitizers' own interceptors.

#define ORPHEUS_TEST_DEFINE_RT_ALLOC_HOOKS
#include "../support/rt_guard.hpp"

#include "../../src/core/transport/transport_controller.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
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
// STRICT gate (ORP134 G1 flip point reached): file-backed clips perform NO
// file I/O inside the callback. Decode happens in prepareClipAudio()/
// startClip() on the control thread (prepared memory) or on the stream
// worker (page ring); the audio thread memcpy-reads published PCM only.
//
// The /proc/self/io sampling itself costs a couple of tiny reads (observed
// 2 syscalls / 112 bytes), so the bound is "far below one page", not zero.
// The pre-G1 debt measured ~2,400 syscalls / ~4.9 MB in the same window.
// ============================================================================

TEST_F(RealtimeHarnessTest, FileBackedRenderDoesNoFileIO) {
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

  // Strict gate: no media I/O on the audio thread. Any regression that
  // reintroduces callback-time decoding blows straight through this bound
  // (the debt era measured ~4.9 MB here).
  EXPECT_LT(readCalls, 8u) << "file I/O observed inside the audio callback";
  EXPECT_LT(readBytes, 4096u) << "file I/O observed inside the audio callback";

  // And the file-backed render path must stay allocation-free too.
  EXPECT_EQ(RtGuardState::allocViolations(), 0u);
  EXPECT_EQ(RtGuardState::deallocViolations(), 0u);
}

// ============================================================================
// Streaming sources (ORP134 G1): long files play from a worker-fed page ring.
// ============================================================================

namespace {

// Counts underrun events and remembers whether any samples were audible.
class UnderrunCountingCallback : public ITransportCallback {
public:
  std::atomic<int> underruns{0};
  void onClipStarted(ClipHandle, TransportPosition) override {}
  void onClipStopped(ClipHandle, TransportPosition) override {}
  void onClipLooped(ClipHandle, TransportPosition) override {}
  void onBufferUnderrun(TransportPosition) override {
    underruns.fetch_add(1, std::memory_order_relaxed);
  }
};

bool bufferHasSignal(const std::vector<float>& left) {
  for (float sample : left) {
    if (sample != 0.0f) {
      return true;
    }
  }
  return false;
}

} // namespace

TEST_F(RealtimeHarnessTest, StreamingSourcePlaysPrefilledWindowWithoutUnderrun) {
  // Force the streaming path: anything longer than 1s streams.
  m_transport->setPreparedSourceMaxFrames(48000);

  UnderrunCountingCallback callback;
  m_transport->setCallback(&callback);

  // 10s file >> the 4-page resident window (~5.46s @ 48k).
  std::string path = writeSineWav(m_tempDir, "stream_long.wav", 220.0f, 10.0f);
  ASSERT_EQ(m_transport->registerClipAudio(1, path), SessionGraphError::OK);
  ASSERT_EQ(m_transport->prepareClipAudio(1), SessionGraphError::OK);
  ASSERT_EQ(m_transport->startClip(1), SessionGraphError::OK);

  // Render 4s of audio — fully inside the synchronously prefilled window, so
  // playback must be gap-free even without giving the worker any wall time.
  constexpr int kBuffers = (4 * 48000) / static_cast<int>(kBufferFrames);
  RtGuardState::reset();
  int buffersWithSignal = 0;
  for (int i = 0; i < kBuffers; ++i) {
    guardedCallback();
    if (bufferHasSignal(m_left)) {
      ++buffersWithSignal;
    }
  }
  m_transport->processCallbacks();

  EXPECT_EQ(callback.underruns.load(), 0) << "prefilled window underran";
  EXPECT_EQ(buffersWithSignal, kBuffers) << "silent buffers inside the prefilled window";
  EXPECT_EQ(RtGuardState::allocViolations(), 0u) << "streaming read path allocated";

  m_transport->setCallback(nullptr);
}

TEST_F(RealtimeHarnessTest, StreamingSeekBeyondWindowUnderrunsSilentlyThenRecovers) {
  m_transport->setPreparedSourceMaxFrames(48000);

  UnderrunCountingCallback callback;
  m_transport->setCallback(&callback);

  std::string path = writeSineWav(m_tempDir, "stream_seek.wav", 220.0f, 10.0f);
  ASSERT_EQ(m_transport->registerClipAudio(1, path), SessionGraphError::OK);
  ASSERT_EQ(m_transport->prepareClipAudio(1), SessionGraphError::OK);
  ASSERT_EQ(m_transport->startClip(1), SessionGraphError::OK);
  guardedCallback(); // materialize the voice

  // Jump far beyond the resident window: the ring holds ~[0, 5.46s); seek 8s.
  ASSERT_EQ(m_transport->seekClip(1, 8 * 48000), SessionGraphError::OK);

  // The seek lands at the next callback; the target page is normally not
  // resident yet, so the clip renders SILENCE + reports BufferUnderrun — and
  // never blocks. Under heavy instrumentation (TSan) the 10ms worker poll can
  // win the race and refill before this buffer renders; both outcomes honor
  // the contract, so the strict miss assertions apply only when the miss
  // actually happened. (The miss path itself is covered deterministically by
  // StreamingClipSourceMissIsNonBlockingAndRecovers below.)
  guardedCallback();
  const bool firstPostSeekSilent = !bufferHasSignal(m_left);
  m_transport->processCallbacks();
  if (firstPostSeekSilent) {
    EXPECT_GT(callback.underruns.load(), 0)
        << "silent post-seek buffer must report a BufferUnderrun";
  } else {
    std::cout << "  - worker refilled before the first post-seek buffer "
                 "(instrumentation slowdown); miss path covered by the unit test\n";
  }

  // Give the stream worker wall time to refill at the new position, then
  // playback must recover with real audio.
  bool recovered = false;
  for (int attempt = 0; attempt < 200 && !recovered; ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    guardedCallback();
    recovered = bufferHasSignal(m_left);
  }
  m_transport->processCallbacks();
  EXPECT_TRUE(recovered) << "stream worker never refilled after the seek";

  m_transport->setCallback(nullptr);
}

TEST_F(RealtimeHarnessTest, StreamingClipSourceMissIsNonBlockingAndRecovers) {
  // Deterministic miss coverage: drive a StreamingClipSource directly with no
  // worker thread, so nothing can refill behind the test's back.
  std::string path = writeSineWav(m_tempDir, "stream_unit.wav", 220.0f, 10.0f);
  auto reader = createAudioFileReader();
  ASSERT_NE(reader, nullptr);
  auto opened = reader->open(path);
  ASSERT_TRUE(opened.isOk());

  auto source = std::make_shared<StreamingClipSource>(
      std::shared_ptr<IAudioFileReader>(std::move(reader)), opened.value.num_channels,
      opened.value.duration_samples);
  source->prefill(0);

  std::vector<float> buffer(kBufferFrames * opened.value.num_channels, 0.0f);
  size_t framesRead = 0;

  // Resident read at the prefilled head succeeds with real audio.
  ASSERT_TRUE(source->read(0, buffer.data(), kBufferFrames, framesRead));
  EXPECT_EQ(framesRead, kBufferFrames);

  // 8s is far outside the prefilled window: a guaranteed miss. read() must
  // return false immediately (framesRead 0, dest untouched) — never block.
  const int64_t farPos = 8 * 48000;
  framesRead = 123;
  EXPECT_FALSE(source->read(farPos, buffer.data(), kBufferFrames, framesRead));
  EXPECT_EQ(framesRead, 0u);

  // One worker pass (called synchronously here) refills at the demand the
  // missed read published; the same read then succeeds.
  source->service();
  ASSERT_TRUE(source->read(farPos, buffer.data(), kBufferFrames, framesRead));
  EXPECT_EQ(framesRead, kBufferFrames);
}

TEST_F(RealtimeHarnessTest, StreamingSourceServesDescendingReadsAcrossPageBoundaries) {
  // FTR025 T3b: reverse scrub reads DESCENDING positions. The demand window
  // keeps one page behind the demand page, so backward page crossings land on
  // resident pages (given ordinary worker passes) all the way down to frame 0
  // — no miss-per-page-boundary, no silence cliff.
  std::string path = writeSineWav(m_tempDir, "stream_rev.wav", 220.0f, 10.0f);
  auto reader = createAudioFileReader();
  ASSERT_NE(reader, nullptr);
  auto opened = reader->open(path);
  ASSERT_TRUE(opened.isOk());
  const uint16_t channels = opened.value.num_channels;

  auto source =
      std::make_shared<StreamingClipSource>(std::shared_ptr<IAudioFileReader>(std::move(reader)),
                                            channels, opened.value.duration_samples);

  // Start deep in the file (inside page 3, ~4.3s) and walk backward to 0.
  const int64_t start = 3 * static_cast<int64_t>(StreamingClipSource::kPageFrames) + 1000;
  source->prefill(start);

  std::vector<float> buffer(kBufferFrames * channels, 0.0f);
  int64_t pos = start;
  int misses = 0;
  int reads = 0;
  while (pos >= 0) {
    size_t framesRead = 0;
    if (!source->read(pos, buffer.data(), kBufferFrames, framesRead)) {
      ++misses;
    } else {
      EXPECT_GT(framesRead, 0u) << "descending read at " << pos;
      float peak = 0.0f;
      for (size_t i = 0; i < framesRead * channels; ++i) {
        peak = std::max(peak, std::abs(buffer[i]));
      }
      EXPECT_GT(peak, 0.01f) << "silent descending read at " << pos;
    }
    ++reads;
    // One synchronous worker pass per buffer — far LESS worker time than the
    // production 10ms poll provides (~9 buffers per pass at 48k/512).
    source->service();
    if (pos == 0) {
      break;
    }
    pos = std::max<int64_t>(0, pos - static_cast<int64_t>(kBufferFrames));
  }

  EXPECT_EQ(misses, 0) << "backward page crossings missed (" << misses << "/" << reads
                       << " reads) — the behind-page window is not being kept resident";
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
