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
#include "../support/sanitizer_guard.hpp"

#include "../../src/core/routing/gain_smoother.h"
#include "../../src/core/routing/routing_matrix.h"
#include "../../src/core/transport/transport_controller.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/thread_info.h>
#elif defined(__linux__)
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#endif

// MSVC's <cmath> does not define M_PI without _USE_MATH_DEFINES; guard it so
// the Windows build resolves the constant. Matches the codebase's M_PI_2 guard.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace orpheus;
using orpheus::tests::support::ProcIoCounters;
using orpheus::tests::support::readProcIoCounters;
using orpheus::tests::support::RtGuardState;
using orpheus::tests::support::RtSection;

namespace {

constexpr uint32_t kSampleRate = 48000;
constexpr size_t kBufferFrames = 512;

#if defined(__APPLE__)
double currentThreadCpuMicros() noexcept {
  thread_basic_info_data_t info{};
  mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
  const thread_t thread = mach_thread_self();
  const kern_return_t result =
      thread_info(thread, THREAD_BASIC_INFO, reinterpret_cast<thread_info_t>(&info), &count);
  mach_port_deallocate(mach_task_self(), thread);
  if (result != KERN_SUCCESS) {
    return -1.0;
  }
  return static_cast<double>(info.user_time.seconds + info.system_time.seconds) * 1'000'000.0 +
         static_cast<double>(info.user_time.microseconds + info.system_time.microseconds);
}
#elif defined(__linux__)
double currentThreadCpuMicros() noexcept {
  timespec value{};
  if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &value) != 0) {
    return -1.0;
  }
  return static_cast<double>(value.tv_sec) * 1'000'000.0 +
         static_cast<double>(value.tv_nsec) / 1'000.0;
}
#endif

// Minimal WAV writer; optional exact topology parameters preserve existing callers.
std::string writeSineWav(const std::filesystem::path& dir, const std::string& name, float freq,
                         float durationSeconds, uint32_t sampleRate = kSampleRate,
                         uint16_t numChannels = 2, int64_t exactFrames = -1) {
  const int64_t numFrames =
      exactFrames >= 0 ? exactFrames : static_cast<int64_t>(durationSeconds * sampleRate);

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
    for (uint16_t channel = 0; channel < numChannels; ++channel) {
      file.write(reinterpret_cast<const char*>(&v), 2);
    }
  }
  file.close();
  return filepath;
}

class RealtimeHarnessTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_tempDir = std::filesystem::temp_directory_path() / "orp136_rt_harness";
    std::filesystem::create_directories(m_tempDir);
    m_transport = std::make_unique<TransportController>(
        nullptr, TransportConfig{.sampleRate = static_cast<uint32_t>(kSampleRate)});
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
    // Use explicit replaceable allocation-function calls rather than a
    // new-expression: C++ permits new/delete elision in optimized builds even
    // when the replacement functions have observable side effects.
    void* allocation = ::operator new(sizeof(int)); // deliberate violation
    *static_cast<int*>(allocation) = 42;
    ::operator delete(allocation); // second deliberate violation
  }

  EXPECT_EQ(RtGuardState::allocViolations(), 1u) << "operator new inside an RtSection must red";
  EXPECT_EQ(RtGuardState::deallocViolations(), 1u)
      << "operator delete inside an RtSection must red";

  // Outside a section, allocations are not violations.
  RtGuardState::reset();
  void* allocation = ::operator new(sizeof(int));
  ::operator delete(allocation);
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
    auto metadata = m_transport->getClipMetadata(handle);
    ASSERT_TRUE(metadata.has_value());
    metadata->dsp.gate.enabled = true;
    metadata->dsp.gate.thresholdDb = -80.0f;
    for (size_t band = 0; band < metadata->dsp.eq.size(); ++band) {
      metadata->dsp.eq[band].enabled = true;
      metadata->dsp.eq[band].frequencyHz = 120.0f * static_cast<float>(band + 1);
      metadata->dsp.eq[band].gainDb = static_cast<float>(band) - 1.5f;
    }
    metadata->dsp.compressor.enabled = true;
    metadata->dsp.width.enabled = true;
    metadata->dsp.limiter.enabled = true;
    ASSERT_EQ(m_transport->updateClipMetadata(handle, *metadata), SessionGraphError::OK);
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
// D3/H prepared-only ingress boundary. The Linux syscall gate traces from
// process start, then validates only the consumer region between explicit
// markers; thread startup and teardown remain outside the measured window.
TEST_F(RealtimeHarnessTest, ConcurrentIngressAt96k64DoesNoIo) {
  constexpr size_t kProducers = 8;
  constexpr size_t kFrames = 64;
  constexpr size_t kRounds = 8;
  constexpr size_t kPerRound = 128;
  auto transport = std::make_unique<TransportController>(
      nullptr, TransportConfig{.sampleRate = 96000, .outputChannels = 2, .maxBlockFrames = 64});
  std::array<ClipHandle, kProducers> handles{};
  std::array<ClipMetadata, kProducers> metadata{};
  for (size_t p = 0; p < kProducers; ++p) {
    handles[p] = static_cast<ClipHandle>(p + 1);
    const auto path = writeSineWav(m_tempDir, "mpsc_rt_" + std::to_string(p) + ".wav",
                                   180.0f + static_cast<float>(p) * 23.0f, 2.0f, 96000);
    ASSERT_EQ(transport->registerClipAudio(handles[p], path), SessionGraphError::OK);
    ASSERT_EQ(transport->prepareClipAudio(handles[p]), SessionGraphError::OK);
    const auto registered = transport->getClipMetadata(handles[p]);
    ASSERT_TRUE(registered.has_value());
    metadata[p] = *registered;
  }
  std::array<float, kFrames> left{}, right{};
  float* output[]{left.data(), right.data()};
  std::array<std::array<SessionGraphError, kRounds * kPerRound>, kProducers> results{};
  std::atomic<size_t> ready{0};
  std::atomic<size_t> completed{0};
  std::atomic<size_t> roundStart{0};
  std::atomic<bool> start{false};
  std::atomic<bool> done{false};
#if defined(__linux__)
  std::atomic<long> consumerTid{0};
#endif
  RtGuardState::reset();
  std::thread pump([&] {
    while (!done.load(std::memory_order_acquire)) {
      transport->processCallbacks();
      (void)transport->getActiveVoiceSnapshot();
    }
  });
  std::thread consumer([&] {
#if defined(__linux__)
    consumerTid.store(static_cast<long>(::syscall(SYS_gettid)), std::memory_order_release);
#endif
    ready.fetch_add(1, std::memory_order_release);
    while (!start.load(std::memory_order_acquire)) {
    }
#if defined(__linux__)
    constexpr char kTraceBegin[] = "TREEFALL_MPSC_TRACE_BEGIN\n";
    (void)::syscall(SYS_write, STDERR_FILENO, kTraceBegin, sizeof(kTraceBegin) - 1);
#endif
    for (size_t round = 0; round < kRounds; ++round) {
      while (roundStart.load(std::memory_order_acquire) != round) {
      }
      {
        RtSection section;
        transport->processAudio(output, 2, kFrames);
      }
      while (completed.load(std::memory_order_acquire) != (round + 1) * kProducers) {
      }
      {
        // All publishers returned: drain the rest before allowing node reuse.
        RtSection section;
        transport->processAudio(output, 2, kFrames);
      }
      roundStart.store(round + 1, std::memory_order_release);
    }
#if defined(__linux__)
    constexpr char kTraceEnd[] = "TREEFALL_MPSC_TRACE_END\n";
    (void)::syscall(SYS_write, STDERR_FILENO, kTraceEnd, sizeof(kTraceEnd) - 1);
#endif
  });
  std::array<std::thread, kProducers> producers;
  for (size_t p = 0; p < kProducers; ++p)
    producers[p] = std::thread([&, p] {
      ready.fetch_add(1, std::memory_order_release);
      while (!start.load(std::memory_order_acquire)) {
      }
      for (size_t round = 0; round < kRounds; ++round) {
        while (roundStart.load(std::memory_order_acquire) != round) {
        }
        for (size_t step = 0; step < kPerRound; ++step) {
          const size_t i = round * kPerRound + step;
          const size_t h = (p + i) % kProducers;
          const auto handle = handles[h];
          const StartRequestTag tag = (static_cast<uint64_t>(p + 1) << 48) | (i + 1);
          auto& result = results[p][i];
          switch (i % 13) {
          case 0:
            result = transport->startClip(handle, tag);
            break;
          case 1:
            result = transport->startClipWithGroupChoke(handle, tag);
            break;
          case 2:
            result = transport->stopClip(handle);
            break;
          case 3:
            result = transport->stopAllClips();
            break;
          case 4:
            result = transport->panic();
            break;
          case 5:
            result = transport->stopOtherClips(handle);
            break;
          case 6:
            result = transport->updateClipTrimPoints(handle, 0, 96000);
            break;
          case 7:
            result = transport->updateClipFades(handle, 0, 0, FadeCurve::Linear, FadeCurve::Linear);
            break;
          case 8:
            result = transport->updateClipGain(handle, -3);
            break;
          case 9:
            result = transport->setClipLoopMode(handle, (i & 1) != 0);
            break;
          case 10:
            result = transport->updateClipMetadata(handle, metadata[h]);
            break;
          case 11:
            result = transport->restartClip(handle);
            break;
          case 12:
            result = transport->seekClip(handle, static_cast<int64_t>(i * 5));
            break;
          }
        }
        completed.fetch_add(1, std::memory_order_release);
        while (roundStart.load(std::memory_order_acquire) == round) {
        }
      }
    });
  while (ready.load(std::memory_order_acquire) != kProducers + 1) {
  }
  start.store(true, std::memory_order_release);
  for (auto& producer : producers)
    producer.join();
  consumer.join();
  done.store(true, std::memory_order_release);
  pump.join();
  transport->processCallbacks();
  for (const auto& row : results)
    for (auto result : row)
      EXPECT_TRUE(result == SessionGraphError::OK || result == SessionGraphError::NotReady);
  const auto telemetry = transport->getCommandIngressTelemetry();
  EXPECT_EQ(telemetry.processedCount, telemetry.admittedCount);
  EXPECT_EQ(telemetry.attemptedCount, telemetry.admittedCount + telemetry.slotUnavailableCount +
                                          telemetry.publicationContentionCount +
                                          telemetry.preparationRejectedCount);
  EXPECT_GT(telemetry.admittedCount, kTransportCommandIngressCapacity);
  EXPECT_EQ(RtGuardState::allocViolations(), 0u);
  EXPECT_EQ(RtGuardState::deallocViolations(), 0u);
#if defined(__linux__)
  std::cout << "MPSC_CONSUMER_TID=" << consumerTid.load(std::memory_order_acquire) << '\n';
#endif
}

// ============================================================================
// Streaming sources (ORP134 G1): long files play from a worker-fed page ring.
// ============================================================================

namespace {

// Counts underrun events and remembers whether any samples were audible.
class UnderrunCountingCallback : public ITransportCallback {
public:
  std::atomic<int> underruns{0};
  std::atomic<int> firstStarts{0};
  std::atomic<int> refireStarts{0};
  void onClipStarted(ClipHandle, orpheus::StartRequestTag requestTag, uint32_t,
                     TransportPosition) override {
    if (requestTag == 1) {
      firstStarts.fetch_add(1, std::memory_order_relaxed);
    } else if (requestTag == 2) {
      refireStarts.fetch_add(1, std::memory_order_relaxed);
    }
  }
  void onClipStopped(ClipHandle, orpheus::StartRequestTag, uint32_t, TransportPosition) override {}
  void onClipLooped(ClipHandle, orpheus::StartRequestTag, uint32_t, TransportPosition) override {}
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

  // 10s file >> the 6-page resident window (~8.2s @ 48k).
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

TEST_F(RealtimeHarnessTest, StreamingGroupChokeRefirePrimesTrimInWithoutUnderrun) {
  m_transport->setPreparedSourceMaxFrames(48000);

  UnderrunCountingCallback callback;
  m_transport->setCallback(&callback);

  const std::string path = writeSineWav(m_tempDir, "stream_group_refire.wav", 220.0f, 10.0f);
  ASSERT_EQ(m_transport->registerClipAudio(1, path), SessionGraphError::OK);
  ASSERT_EQ(m_transport->setClipVoiceMode(1, VoiceMode::MonoWithFadeOverlap),
            SessionGraphError::OK);
  ASSERT_EQ(m_transport->prepareClipAudio(1), SessionGraphError::OK);
  ASSERT_EQ(m_transport->startClip(1), SessionGraphError::OK);
  guardedCallback();

  // Move beyond the first page while staying inside the initial resident
  // window. The audio thread retires page zero, reproducing a long clip whose
  // streaming cache has advanced away from its trim-in point.
  constexpr int64_t advancedPosition = 2 * static_cast<int64_t>(StreamingClipSource::kPageFrames);
  ASSERT_EQ(m_transport->seekClip(1, advancedPosition), SessionGraphError::OK);
  guardedCallback();
  m_transport->processCallbacks();
  ASSERT_EQ(callback.underruns.load(), 0);

  ASSERT_EQ(m_transport->startClipWithGroupChoke(1), SessionGraphError::OK);
  guardedCallback();
  m_transport->processCallbacks();

  EXPECT_EQ(callback.underruns.load(), 0) << "group-choke refire missed the trim-in streaming page";
  EXPECT_TRUE(bufferHasSignal(m_left));
  m_transport->setCallback(nullptr);
}

TEST_F(RealtimeHarnessTest, StreamingFadeOverlapKeepsBothCursorsAllocationFree) {
  m_transport->setPreparedSourceMaxFrames(48000);
  ASSERT_EQ(m_transport->setMaxVoicesPerClip(2), SessionGraphError::OK);
  UnderrunCountingCallback callback;
  m_transport->setCallback(&callback);

  const std::string path = writeSineWav(m_tempDir, "stream_overlap.wav", 220.0f, 10.0f);
  ASSERT_EQ(m_transport->registerClipAudio(1, path), SessionGraphError::OK);
  ASSERT_EQ(m_transport->setClipVoiceMode(1, VoiceMode::MonoWithFadeOverlap),
            SessionGraphError::OK);
  auto metadata = m_transport->getClipMetadata(1);
  ASSERT_TRUE(metadata.has_value());
  metadata->stopFadeOutSeconds = 4.0;
  ASSERT_EQ(m_transport->updateClipMetadata(1, *metadata), SessionGraphError::OK);
  ASSERT_EQ(m_transport->setClipLoopMode(1, true), SessionGraphError::OK);
  ASSERT_EQ(m_transport->prepareClipAudio(1), SessionGraphError::OK);

  constexpr int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  RtGuardState::reset();
  ASSERT_EQ(m_transport->startClip(1, 1), SessionGraphError::OK);
  guardedCallback();
  ASSERT_EQ(m_transport->seekClip(1, 6 * page + 100), SessionGraphError::OK);
  guardedCallback();
  ASSERT_EQ(m_transport->stopClip(1), SessionGraphError::OK);
  guardedCallback();
  ASSERT_EQ(m_transport->startClip(1, 2), SessionGraphError::OK);
  for (size_t block = 0; block < 32; ++block) {
    guardedCallback();
  }
  m_transport->processCallbacks();

  EXPECT_EQ(m_transport->getActiveVoiceCount(1), 2u);
  EXPECT_EQ(callback.firstStarts.load(std::memory_order_relaxed), 1);
  EXPECT_EQ(callback.refireStarts.load(std::memory_order_relaxed), 1);
  EXPECT_EQ(callback.underruns.load(std::memory_order_relaxed), 0);
  EXPECT_TRUE(bufferHasSignal(m_left));
  EXPECT_EQ(RtGuardState::allocViolations(), 0u);
  EXPECT_EQ(RtGuardState::deallocViolations(), 0u);

  ASSERT_EQ(m_transport->panic(), SessionGraphError::OK);
  guardedCallback();
  m_transport->processCallbacks();
  EXPECT_EQ(m_transport->getTotalActiveVoiceCount(), 0u);
  m_transport->setCallback(nullptr);
}

TEST_F(RealtimeHarnessTest, StreamingSeekBeyondWindowIsPrimedBeforeFirstRender) {
  m_transport->setPreparedSourceMaxFrames(48000);

  UnderrunCountingCallback callback;
  m_transport->setCallback(&callback);

  std::string path = writeSineWav(m_tempDir, "stream_seek.wav", 220.0f, 10.0f);
  ASSERT_EQ(m_transport->registerClipAudio(1, path), SessionGraphError::OK);
  ASSERT_EQ(m_transport->prepareClipAudio(1), SessionGraphError::OK);
  ASSERT_EQ(m_transport->startClip(1), SessionGraphError::OK);
  guardedCallback(); // materialize the voice

  // Jump far beyond the resident worker window. Accepted seeks synchronously
  // pin their first-render pages before this command reaches processAudio().
  ASSERT_EQ(m_transport->seekClip(1, 8 * 48000), SessionGraphError::OK);
  RtGuardState::reset();
  guardedCallback();
  m_transport->processCallbacks();

  EXPECT_TRUE(bufferHasSignal(m_left)) << "accepted seek rendered a silent first block";
  EXPECT_EQ(callback.underruns.load(), 0) << "primed seek synthesized BufferUnderrun";
  EXPECT_EQ(RtGuardState::allocViolations(), 0u);
  EXPECT_EQ(RtGuardState::deallocViolations(), 0u);
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
  ASSERT_EQ(source->prefill(0), SessionGraphError::OK);

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
  ASSERT_EQ(source->prefill(start), SessionGraphError::OK);

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
  if (orpheus::test::timingAssertionsEnabled()) {
    EXPECT_LT(maxUs, budgetUs) << "a single reader-less callback exceeded the realtime budget";
  } else {
    std::cout << "  - duration bound skipped under sanitizer\n";
  }
}

TEST_F(RealtimeHarnessTest, MaxTopologyTelemetryMeteringIsAllocationFree) {
  constexpr RoutingChannelIndex kRoutingChannels = 256;
  constexpr RoutingGroupIndex kRoutingGroups = 32;
  constexpr RoutingOutputIndex kRoutingOutputs = 32;

  RoutingConfig config;
  config.num_channels = kRoutingChannels;
  config.num_groups = kRoutingGroups;
  config.num_outputs = kRoutingOutputs;
  config.sample_rate = kSampleRate;
  config.gain_smoothing_ms = 0.0f;
  config.enable_metering = true;
  config.source_channel_policy = SourceChannelPolicy::Discrete;
  config.metering_mode = MeteringMode::TruePeak;

  RoutingMatrix matrix;
  ASSERT_EQ(matrix.initialize(config), SessionGraphError::OK);
  for (RoutingChannelIndex channel = 0; channel < kRoutingChannels; ++channel) {
    ASSERT_EQ(matrix.setChannelRoute(channel,
                                     static_cast<RoutingGroupIndex>(channel % kRoutingGroups),
                                     static_cast<RoutingOutputIndex>(channel % kRoutingOutputs)),
              SessionGraphError::OK);
  }

  std::vector<std::vector<float>> inputs(kRoutingChannels,
                                         std::vector<float>(kBufferFrames, 0.01f));
  std::vector<const float*> inputPointers;
  inputPointers.reserve(kRoutingChannels);
  for (const auto& lane : inputs) {
    inputPointers.push_back(lane.data());
  }
  std::vector<std::vector<float>> outputs(kRoutingOutputs, std::vector<float>(kBufferFrames, 0.0f));
  std::vector<float*> outputPointers;
  outputPointers.reserve(kRoutingOutputs);
  for (auto& lane : outputs) {
    outputPointers.push_back(lane.data());
  }

  for (int warmup = 0; warmup < 4; ++warmup) {
    ASSERT_EQ(matrix.processRouting(inputPointers.data(), outputPointers.data(), kBufferFrames),
              SessionGraphError::OK);
  }
  GroupOutputMeterSnapshot snapshot;
  matrix.copyGroupOutputMeterSnapshot(snapshot);
  ASSERT_EQ(snapshot.coherent, 1);
  ASSERT_EQ(snapshot.availability, MeterAvailability::Measured);
  ASSERT_EQ(snapshot.groups[0].availability, MeterAvailability::Measured);

  // The separate deadline gate covers 300 callbacks per mode. A repeated
  // non-silent callback sample is sufficient to expose allocation in this path.
  constexpr int kAllocationCallbacks = 16;

  RtGuardState::reset();
  for (int callback = 0; callback < kAllocationCallbacks; ++callback) {
    RtSection section;
    ASSERT_EQ(matrix.processRouting(inputPointers.data(), outputPointers.data(), kBufferFrames),
              SessionGraphError::OK);
  }

  EXPECT_EQ(RtGuardState::allocViolations(), 0u)
      << "maximum-topology non-silent metering allocated on the callback";
  EXPECT_EQ(RtGuardState::deallocViolations(), 0u)
      << "maximum-topology non-silent metering deallocated on the callback";
}

TEST_F(RealtimeHarnessTest, MaxTopologySamplePeakAndTruePeakMeetDeadline) {
  constexpr RoutingChannelIndex kRoutingChannels = 256;
  constexpr RoutingGroupIndex kRoutingGroups = 32;
  constexpr RoutingOutputIndex kRoutingOutputs = 32;
  constexpr int kCallbacks = 300;

  RoutingConfig config;
  config.num_channels = kRoutingChannels;
  config.num_groups = kRoutingGroups;
  config.num_outputs = kRoutingOutputs;
  config.sample_rate = kSampleRate;
  config.gain_smoothing_ms = 0.0f;
  config.enable_metering = true;
  config.enable_clipping_protection = false;
  config.source_channel_policy = SourceChannelPolicy::Discrete;

  std::vector<std::vector<float>> inputs(kRoutingChannels,
                                         std::vector<float>(kBufferFrames, 0.01f));
  std::vector<const float*> inputPointers;
  inputPointers.reserve(kRoutingChannels);
  for (const auto& lane : inputs) {
    inputPointers.push_back(lane.data());
  }
  std::vector<std::vector<float>> outputs(kRoutingOutputs, std::vector<float>(kBufferFrames, 0.0f));
  std::vector<float*> outputPointers;
  outputPointers.reserve(kRoutingOutputs);
  for (auto& lane : outputs) {
    outputPointers.push_back(lane.data());
  }

  const bool timingAssertionsEnabled = orpheus::test::timingAssertionsEnabled();

  const auto runMode = [&](MeteringMode mode, const char* label) {
    config.metering_mode = mode;
    RoutingMatrix matrix;
    ASSERT_EQ(matrix.initialize(config), SessionGraphError::OK);
    for (RoutingChannelIndex channel = 0; channel < kRoutingChannels; ++channel) {
      ASSERT_EQ(matrix.setChannelRoute(channel, static_cast<RoutingGroupIndex>(channel / 8),
                                       static_cast<RoutingOutputIndex>(channel % 8)),
                SessionGraphError::OK);
    }
    for (int warmup = 0; warmup < 4; ++warmup) {
      ASSERT_EQ(matrix.processRouting(inputPointers.data(), outputPointers.data(), kBufferFrames),
                SessionGraphError::OK);
    }

    std::vector<double> durations;
    durations.reserve(kCallbacks);
#if defined(__APPLE__) || defined(__linux__)
    std::vector<double> cpuDurations;
    cpuDurations.reserve(kCallbacks);
#endif
    RtGuardState::reset();
    for (int callback = 0; callback < kCallbacks; ++callback) {
#if defined(__APPLE__) || defined(__linux__)
      const double cpuStart = currentThreadCpuMicros();
      ASSERT_GE(cpuStart, 0.0);
#endif
      const auto start = std::chrono::steady_clock::now();
      {
        RtSection section;
        ASSERT_EQ(matrix.processRouting(inputPointers.data(), outputPointers.data(), kBufferFrames),
                  SessionGraphError::OK);
      }
      const auto end = std::chrono::steady_clock::now();
      durations.push_back(std::chrono::duration<double, std::micro>(end - start).count());
#if defined(__APPLE__) || defined(__linux__)
      const double cpuEnd = currentThreadCpuMicros();
      ASSERT_GE(cpuEnd, cpuStart);
      cpuDurations.push_back(cpuEnd - cpuStart);
#endif
    }

    std::sort(durations.begin(), durations.end());
    double total = 0.0;
    for (double duration : durations) {
      total += duration;
    }
    const double average = total / static_cast<double>(durations.size());
    const double p99 = durations[(durations.size() * 99) / 100];
    const double maximum = durations.back();
    const double budget = (static_cast<double>(kBufferFrames) * 1'000'000.0) / kSampleRate;
    std::cout << "[RT Harness] max topology " << label << ": wall avg " << average << " us, p99 "
              << p99 << " us, max " << maximum << " us, budget " << budget << " us\n";
    EXPECT_EQ(RtGuardState::allocViolations(), 0u);
    EXPECT_EQ(RtGuardState::deallocViolations(), 0u);
#if defined(NDEBUG)
    if (timingAssertionsEnabled) {
#if defined(__APPLE__) || defined(__linux__)
      std::sort(cpuDurations.begin(), cpuDurations.end());
      const double cpuMaximum = cpuDurations.back();
      std::cout << "[RT Harness] max topology " << label << ": thread CPU max " << cpuMaximum
                << " us\n";
      EXPECT_LT(cpuMaximum, budget)
          << label << " maximum thread CPU exceeded the 512-frame/48 kHz callback budget";
      EXPECT_LT(p99, budget) << label
                             << " wall-clock p99 exceeded the 512-frame/48 kHz callback budget";
#else
      EXPECT_LT(maximum, budget) << label << " exceeded the 512-frame/48 kHz callback budget";
#endif
    }
#endif
  };

  runMode(MeteringMode::Peak, "sample-peak");
  runMode(MeteringMode::TruePeak, "true-peak");
}

TEST_F(RealtimeHarnessTest, WorstCaseTopologyDeadlineDistribution) {
#if !defined(NDEBUG)
  GTEST_SKIP() << "Requires unsanitized Release and ORPHEUS_RUN_WORST_CASE_BENCHMARK=1";
#endif
  if (orpheus::test::underSanitizer()) {
    GTEST_SKIP() << "Sanitizer execution is not performance acceptance";
  }
  const char* enabled = std::getenv("ORPHEUS_RUN_WORST_CASE_BENCHMARK");
  if (enabled == nullptr || std::string_view(enabled) != "1") {
    GTEST_SKIP() << "Opt-in measurement disabled; R-01 is not qualified by this skip";
  }
  constexpr size_t frames = 2048;
  // ORP273 F-18: the measurement's shape is environment-overridable so the
  // registered CTest gate can run a reduced observation count, while the full
  // 10,000-observation qualification stays available by setting the variables
  // when invoking the binary directly. The p99 ratio is clamped so that no
  // configuration can raise the bar above the documented 0.5 ceiling.
  const auto envUnsigned = [](const char* name, size_t fallback) -> size_t {
    const char* raw = std::getenv(name);
    if (raw == nullptr) {
      return fallback;
    }
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(raw, &end, 10);
    if (end == raw || parsed == 0ull) {
      return fallback;
    }
    return static_cast<size_t>(parsed);
  };
  const auto envRatio = [](const char* name, double fallback, double ceiling) -> double {
    const char* raw = std::getenv(name);
    if (raw == nullptr) {
      return fallback;
    }
    char* end = nullptr;
    const double parsed = std::strtod(raw, &end);
    if (end == raw || !(parsed > 0.0)) {
      return fallback;
    }
    return std::min(parsed, ceiling);
  };
  const size_t warmups = envUnsigned("ORPHEUS_DEADLINE_WARMUPS", 100u);
  const size_t observations = envUnsigned("ORPHEUS_DEADLINE_OBSERVATIONS", 10000u);
  const double p99_ratio = envRatio("ORPHEUS_DEADLINE_P99_RATIO", 0.5, 0.5);

  std::vector<uint32_t> rates;
  if (const char* raw = std::getenv("ORPHEUS_DEADLINE_RATES")) {
    const std::string text(raw);
    size_t start = 0;
    while (start <= text.size()) {
      const size_t comma = text.find(',', start);
      const std::string token =
          text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
      if (!token.empty()) {
        char* end = nullptr;
        const unsigned long long parsed = std::strtoull(token.c_str(), &end, 10);
        if (end != token.c_str() && parsed > 0ull) {
          rates.push_back(static_cast<uint32_t>(parsed));
        }
      }
      if (comma == std::string::npos) {
        break;
      }
      start = comma + 1;
    }
  }
  if (rates.empty()) {
    rates = {44100u, 48000u, 96000u, 192000u};
  }
#if defined(_MSC_VER)
  std::cout << "[WorstCase] compiler=MSVC-" << _MSC_VER;
#else
  std::cout << "[WorstCase] compiler=" << __VERSION__;
#endif
  std::cout << " build=Release-unsanitized rates=synthetic-not-negotiated"
            << " frames=" << frames << " warmup=" << warmups << " blocks=" << observations
            << " p99_ratio=" << p99_ratio << " outliers=retained\n";
  const auto measure = [&](uint32_t rate, const char* mix, auto&& prepare, auto&& render,
                           auto&& verify) {
    std::vector<double> wall(observations);
#if defined(__APPLE__) || defined(__linux__)
    std::vector<double> cpu(observations, -1.0);
    bool cpuAvailable = true;
#endif
    for (size_t block = 0; block < warmups; ++block) {
      prepare(block);
      if (::testing::Test::HasFatalFailure()) {
        return;
      }
      render();
      verify();
    }
    uint64_t ioReadCalls = 0;
    uint64_t ioReadBytes = 0;
    bool ioAvailable = true;
    RtGuardState::reset();
    for (size_t block = 0; block < observations; ++block) {
      prepare(block + warmups);
      if (::testing::Test::HasFatalFailure()) {
        return;
      }
      const auto before = readProcIoCounters();
#if defined(__APPLE__) || defined(__linux__)
      const double cpuStart = currentThreadCpuMicros();
#endif
      const auto begin = std::chrono::steady_clock::now();
      {
        RtSection section;
        render();
      }
      const auto end = std::chrono::steady_clock::now();
#if defined(__APPLE__) || defined(__linux__)
      const double cpuEnd = currentThreadCpuMicros();
      if (cpuStart >= 0.0 && cpuEnd >= cpuStart) {
        cpu[block] = cpuEnd - cpuStart;
      } else {
        cpuAvailable = false;
      }
#endif
      wall[block] = std::chrono::duration<double, std::micro>(end - begin).count();
      const auto after = readProcIoCounters();
      if (before && after) {
        ioReadCalls += after->syscr - before->syscr;
        ioReadBytes += after->rchar - before->rchar;
      } else {
        ioAvailable = false;
      }
      verify();
    }
    const double deadline = static_cast<double>(frames) * 1000000.0 / rate;
    for (size_t block = 0; block < observations; ++block) {
      if (wall[block] >= deadline) {
        std::cout << "[WorstCaseMiss] rate=" << rate << " mix=" << mix << " block=" << block
                  << " wall_us=" << wall[block];
#if defined(__APPLE__) || defined(__linux__)
        if (cpu[block] >= 0.0) {
          std::cout << " cpu_us=" << cpu[block];
        }
#endif
        std::cout << " deadline_us=" << deadline << '\n';
      }
    }
    std::sort(wall.begin(), wall.end());
    const auto report = [&](const char* clock, const std::vector<double>& values) {
      std::cout << "[WorstCase] rate=" << rate << " mix=" << mix << " clock=" << clock
                << " p50_us=" << values[observations * 50 / 100]
                << " p95_us=" << values[observations * 95 / 100]
                << " p99_us=" << values[observations * 99 / 100] << " max_us=" << values.back()
                << " deadline_us=" << deadline << '\n';
    };
    report("wall", wall);
#if defined(__APPLE__) || defined(__linux__)
    if (cpuAvailable) {
      std::sort(cpu.begin(), cpu.end());
      report("thread-cpu", cpu);
    }
#endif
    std::cout << "[WorstCase] mix=" << mix << " allocations=" << RtGuardState::allocViolations()
              << " deallocations=" << RtGuardState::deallocViolations()
              << " io_sampler=" << (ioAvailable ? "proc-self-includes-observer" : "unavailable")
              << " sampled_read_calls=" << ioReadCalls << " sampled_read_bytes=" << ioReadBytes
              << '\n';
    EXPECT_EQ(RtGuardState::allocViolations(), 0u);
    EXPECT_EQ(RtGuardState::deallocViolations(), 0u);
    EXPECT_LT(wall[observations * 99 / 100], deadline);
    EXPECT_LT(wall.back(), deadline);
    EXPECT_LT(wall[observations * 99 / 100], deadline * p99_ratio)
        << mix << " wall-clock p99 exceeded " << p99_ratio << " of the frame deadline";
    // /proc sampling itself performs reads; exact callback syscall attribution
    // remains a separate Linux trace gate, never a zero-I/O claim from this aggregate.
  };
  for (uint32_t rate : rates) {
    {
      TransportConfig config;
      config.sampleRate = rate;
      config.maxBlockFrames = frames;
      config.maxActiveVoices = 32;
      config.outputChannels = 8;
      auto transport = std::make_unique<TransportController>(nullptr, config);
      const auto path = writeSineWav(m_tempDir, "worst-" + std::to_string(rate) + ".wav", 220.0f,
                                     0.0f, rate, 8, 8192);
      std::array<ClipMetadata, 32> metadata;
      const auto sourceFormat = ChannelFormat::Surround71();
      ASSERT_EQ(transport->setGroupOutputBus(0, OutputBusRoute{0, 8}), SessionGraphError::OK);
      ASSERT_EQ(transport->setGroupOutputBus(1, OutputBusRoute{0, 8}), SessionGraphError::OK);
      for (ClipHandle handle = 1; handle <= 32; ++handle) {
        ASSERT_EQ(transport->registerClipAudio(handle, path), SessionGraphError::OK);
        auto& value = metadata[handle - 1];
        value = *transport->getClipMetadata(handle);
        value.loopEnabled = true;
        value.voiceMode = VoiceMode::MonoStrict;
        value.routingGroup = handle == 1 ? 0 : 1;
        value.sourceLayout = sourceFormat.layout;
        value.speakerPatchSize = sourceFormat.num_channels;
        std::copy_n(sourceFormat.channel_map.begin(), sourceFormat.num_channels,
                    value.speakerPatch.begin());
        ASSERT_EQ(transport->updateClipMetadata(handle, value), SessionGraphError::OK);
        ASSERT_EQ(transport->prepareClipAudio(handle), SessionGraphError::OK);
        ASSERT_EQ(transport->startClip(handle), SessionGraphError::OK);
      }
      std::array<std::array<float, frames>, 8> output{};
      std::array<float*, 8> lanes{};
      for (size_t lane = 0; lane < lanes.size(); ++lane) {
        lanes[lane] = output[lane].data();
      }
      transport->processAudio(lanes.data(), lanes.size(), frames);
      ASSERT_EQ(transport->getTotalActiveVoiceCount(), 32u);
      const auto prepare = [&](size_t block) {
        for (size_t command = 0; command < 128; ++command) {
          const ClipHandle handle = static_cast<ClipHandle>(command % 32 + 1);
          ASSERT_EQ(transport->updateClipMetadata(handle, metadata[handle - 1]),
                    SessionGraphError::OK);
        }
        for (size_t command = 0; command < 126; ++command) {
          ASSERT_EQ(transport->updateClipGain(static_cast<ClipHandle>(command % 32 + 1),
                                              (block & 1) ? -3.0f : -6.0f),
                    SessionGraphError::OK);
        }
        ASSERT_EQ(transport->startClipWithGroupChoke(1), SessionGraphError::OK);
      };
      measure(
          rate, "transport-32voices-8source-255commands", prepare,
          [&] { transport->processAudio(lanes.data(), lanes.size(), frames); },
          [&] {
            ASSERT_EQ(transport->getTotalActiveVoiceCount(), 32u);
            transport->processCallbacks();
          });
    }
    for (MeteringMode mode : {MeteringMode::Peak, MeteringMode::TruePeak}) {
      RoutingConfig config;
      config.num_channels = 256;
      config.num_groups = 32;
      config.num_outputs = 32;
      config.sample_rate = rate;
      config.gain_smoothing_ms = 0.0f;
      config.enable_metering = true;
      config.enable_clipping_protection = false;
      config.source_channel_policy = SourceChannelPolicy::Discrete;
      config.metering_mode = mode;
      RoutingMatrix matrix;
      ASSERT_EQ(matrix.initialize(config), SessionGraphError::OK);
      for (RoutingChannelIndex channel = 0; channel < 256; ++channel) {
        ASSERT_EQ(matrix.setChannelRoute(channel, static_cast<RoutingGroupIndex>(channel / 8),
                                         static_cast<RoutingOutputIndex>(channel % 8)),
                  SessionGraphError::OK);
      }
      std::vector<std::vector<float>> input(256, std::vector<float>(frames, 0.01f));
      std::vector<std::vector<float>> output(32, std::vector<float>(frames, 0.0f));
      std::array<const float*, 256> inputs{};
      std::array<float*, 32> outputs{};
      for (size_t lane = 0; lane < inputs.size(); ++lane) {
        inputs[lane] = input[lane].data();
      }
      for (size_t lane = 0; lane < outputs.size(); ++lane) {
        outputs[lane] = output[lane].data();
      }
      SessionGraphError result = SessionGraphError::OK;
      measure(
          rate,
          mode == MeteringMode::Peak ? "routing-256-32-32-Peak" : "routing-256-32-32-TruePeak",
          [](size_t) {},
          [&] { result = matrix.processRouting(inputs.data(), outputs.data(), frames); },
          [&] { ASSERT_EQ(result, SessionGraphError::OK); });
    }
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
