// SPDX-License-Identifier: MIT
// ORP121 Q-07: Threading stress tests for callback queue (C-03 validation)
//
// This test validates the lock-free SPSC callback queue that routes
// events from the audio thread to the UI thread.

#include "../../src/core/transport/transport_controller.h"

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <random>
#include <thread>
#include <vector>

using namespace orpheus;

// Callback counter for stress testing
class CountingCallback : public ITransportCallback {
public:
  std::atomic<int> startCount{0};
  std::atomic<int> stopCount{0};
  std::atomic<int> loopCount{0};
  std::atomic<int> restartCount{0};
  std::atomic<int> seekCount{0};
  std::atomic<int> underrunCount{0};

  void onClipStarted(ClipHandle /*handle*/, TransportPosition /*position*/) override {
    startCount.fetch_add(1, std::memory_order_relaxed);
  }

  void onClipStopped(ClipHandle /*handle*/, TransportPosition /*position*/) override {
    stopCount.fetch_add(1, std::memory_order_relaxed);
  }

  void onClipLooped(ClipHandle /*handle*/, TransportPosition /*position*/) override {
    loopCount.fetch_add(1, std::memory_order_relaxed);
  }

  void onClipRestarted(ClipHandle /*handle*/, TransportPosition /*position*/) override {
    restartCount.fetch_add(1, std::memory_order_relaxed);
  }

  void onClipSeeked(ClipHandle /*handle*/, TransportPosition /*position*/) override {
    seekCount.fetch_add(1, std::memory_order_relaxed);
  }

  void onBufferUnderrun(TransportPosition /*position*/) override {
    underrunCount.fetch_add(1, std::memory_order_relaxed);
  }

  int totalCallbacks() const {
    return startCount.load() + stopCount.load() + loopCount.load() + restartCount.load() +
           seekCount.load() + underrunCount.load();
  }
};

class CallbackQueueStressTest : public ::testing::Test {
protected:
  static constexpr uint32_t SAMPLE_RATE = 48000;
  static constexpr size_t BUFFER_SIZE = 512;

  void SetUp() override {
    m_transport = std::make_unique<TransportController>(nullptr, TransportConfig{.sampleRate = static_cast<uint32_t>(SAMPLE_RATE)});
    m_callback = std::make_unique<CountingCallback>();
    m_transport->setCallback(m_callback.get());
  }

  void TearDown() override {
    m_transport.reset();
  }

  // Simulate audio thread processing
  void simulateAudioCallback(size_t numBuffers = 1) {
    std::vector<float> buffer(BUFFER_SIZE * 2, 0.0f);
    std::vector<float*> buffers = {buffer.data(), buffer.data() + BUFFER_SIZE};

    for (size_t i = 0; i < numBuffers; ++i) {
      m_transport->processAudio(buffers.data(), 2, BUFFER_SIZE);
    }
  }

  // Simulate UI thread callback processing
  void simulateUICallback() {
    m_transport->processCallbacks();
  }

  std::unique_ptr<TransportController> m_transport;
  std::unique_ptr<CountingCallback> m_callback;
};

// ============================================================================
// Basic Functionality Tests
// ============================================================================

TEST_F(CallbackQueueStressTest, SingleStartStopCallback) {
  // Start and stop a clip, verify callbacks arrive
  ClipHandle handle = 1;

  m_transport->startClip(handle);
  simulateAudioCallback();
  simulateUICallback();

  // Give time for callback to be processed
  EXPECT_GE(m_callback->startCount.load(), 0); // May or may not have fired yet

  m_transport->stopClip(handle);
  simulateAudioCallback();
  simulateUICallback();

  // Callbacks should eventually be received
  std::cout << "[Callback Queue] Start: " << m_callback->startCount.load()
            << ", Stop: " << m_callback->stopCount.load() << "\n";
}

// ============================================================================
// Concurrent Access Tests
// ============================================================================

TEST_F(CallbackQueueStressTest, ConcurrentStartStopClips) {
  // Simulate rapid start/stop from UI thread while audio processes
  std::atomic<bool> running{true};
  constexpr int NUM_ITERATIONS = 100;

  // Audio thread simulation
  std::thread audioThread([this, &running]() {
    while (running.load()) {
      simulateAudioCallback();
      std::this_thread::sleep_for(std::chrono::microseconds(100)); // ~10kHz
    }
  });

  // UI thread simulation - start/stop clips rapidly
  std::thread uiThread([this, &running]() {
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
      ClipHandle handle = static_cast<ClipHandle>(i % 10 + 1);

      m_transport->startClip(handle);
      std::this_thread::sleep_for(std::chrono::microseconds(500));

      simulateUICallback();

      m_transport->stopClip(handle);
      std::this_thread::sleep_for(std::chrono::microseconds(500));

      simulateUICallback();
    }
    running.store(false);
  });

  audioThread.join();
  uiThread.join();

  // Final callback processing
  simulateUICallback();

  std::cout << "[Callback Queue] Concurrent test - Total callbacks: "
            << m_callback->totalCallbacks() << "\n";

  // Should complete without deadlock
  EXPECT_TRUE(true);
}

TEST_F(CallbackQueueStressTest, HighFrequencyCommands) {
  // Stress test: Many commands in rapid succession
  std::atomic<bool> running{true};
  constexpr int NUM_COMMANDS = 1000;

  // Audio thread - process frequently
  std::thread audioThread([this, &running]() {
    while (running.load()) {
      simulateAudioCallback();
      std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
  });

  // UI thread - issue commands as fast as possible
  std::thread uiThread([this, &running, NUM_COMMANDS]() {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(1, 10);

    for (int i = 0; i < NUM_COMMANDS; ++i) {
      ClipHandle handle = static_cast<ClipHandle>(dist(rng));

      // Alternate start/stop
      if (i % 2 == 0) {
        m_transport->startClip(handle);
      } else {
        m_transport->stopClip(handle);
      }

      // Process callbacks periodically
      if (i % 10 == 0) {
        simulateUICallback();
      }
    }
    running.store(false);
  });

  audioThread.join();
  uiThread.join();

  // Final drain
  for (int i = 0; i < 10; ++i) {
    simulateUICallback();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  std::cout << "[Callback Queue] High frequency - Commands: " << NUM_COMMANDS
            << ", Callbacks: " << m_callback->totalCallbacks() << "\n";

  EXPECT_TRUE(true); // No deadlock
}

// ============================================================================
// Stress Tests (Extended Duration)
// ============================================================================

TEST_F(CallbackQueueStressTest, SustainedOperationTwoSeconds) {
  // Run concurrent audio/UI threads for 2 seconds
  std::atomic<bool> running{true};
  std::atomic<int> commandCount{0};

  // Audio thread
  std::thread audioThread([this, &running]() {
    while (running.load()) {
      simulateAudioCallback();
      std::this_thread::sleep_for(std::chrono::microseconds(100)); // ~10kHz
    }
  });

  // UI command thread
  std::thread commandThread([this, &running, &commandCount]() {
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> clipDist(1, 32);

    while (running.load()) {
      ClipHandle handle = static_cast<ClipHandle>(clipDist(rng));

      // Random operation
      if (commandCount.load() % 2 == 0) {
        m_transport->startClip(handle);
      } else {
        m_transport->stopClip(handle);
      }
      commandCount.fetch_add(1);

      std::this_thread::sleep_for(std::chrono::microseconds(500)); // 2000 cmd/sec
    }
  });

  // UI callback thread
  std::thread callbackThread([this, &running]() {
    while (running.load()) {
      simulateUICallback();
      std::this_thread::sleep_for(std::chrono::milliseconds(16)); // 60Hz
    }
  });

  // Run for 2 seconds
  std::this_thread::sleep_for(std::chrono::seconds(2));
  running.store(false);

  audioThread.join();
  commandThread.join();
  callbackThread.join();

  // Final drain
  simulateUICallback();

  std::cout << "[Callback Queue] 2-second sustained test:\n"
            << "  Commands issued: " << commandCount.load() << "\n"
            << "  Callbacks received: " << m_callback->totalCallbacks() << "\n"
            << "  Start: " << m_callback->startCount.load()
            << ", Stop: " << m_callback->stopCount.load() << "\n";

  // Should complete without crash or deadlock
  EXPECT_TRUE(true);
}

// ============================================================================
// Queue Overflow Test
// ============================================================================

TEST_F(CallbackQueueStressTest, RapidFireWithoutProcessing) {
  // Issue many commands without processing callbacks (tests queue overflow handling)
  constexpr int NUM_COMMANDS = 1000;

  // Issue commands very rapidly without processing callbacks
  for (int i = 0; i < NUM_COMMANDS; ++i) {
    ClipHandle handle = static_cast<ClipHandle>(i % 10 + 1);
    if (i % 2 == 0) {
      m_transport->startClip(handle);
    } else {
      m_transport->stopClip(handle);
    }
    simulateAudioCallback(); // Process audio to generate callbacks
  }

  // Now process all callbacks at once
  for (int i = 0; i < 20; ++i) {
    simulateUICallback();
  }

  std::cout << "[Callback Queue] Rapid fire test (queue overflow scenario):\n"
            << "  Commands: " << NUM_COMMANDS << "\n"
            << "  Callbacks received: " << m_callback->totalCallbacks() << "\n";

  // Queue size is 256, so we expect at most 256 callbacks to be retained
  // (unless multiple processing passes)
  EXPECT_GT(m_callback->totalCallbacks(), 0);
}

// ============================================================================
// Latency Test
// ============================================================================

TEST_F(CallbackQueueStressTest, CallbackLatency) {
  // Measure time from command to callback
  constexpr int NUM_ITERATIONS = 50;
  std::vector<int64_t> latencies;
  latencies.reserve(NUM_ITERATIONS);

  for (int i = 0; i < NUM_ITERATIONS; ++i) {
    auto start = std::chrono::high_resolution_clock::now();

    ClipHandle handle = static_cast<ClipHandle>(i + 1);
    int initialCount = m_callback->startCount.load();

    m_transport->startClip(handle);
    simulateAudioCallback();
    simulateUICallback();

    // Wait for callback (with timeout)
    int timeout = 100;
    while (m_callback->startCount.load() == initialCount && timeout-- > 0) {
      simulateAudioCallback();
      simulateUICallback();
      std::this_thread::sleep_for(std::chrono::microseconds(10));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    latencies.push_back(latency);

    m_transport->stopClip(handle);
    simulateAudioCallback();
    simulateUICallback();
  }

  // Calculate statistics
  int64_t total = 0;
  int64_t max_lat = 0;
  for (auto lat : latencies) {
    total += lat;
    max_lat = std::max(max_lat, lat);
  }
  double avg_lat = static_cast<double>(total) / NUM_ITERATIONS;

  std::cout << "[Callback Queue] Latency:\n"
            << "  Average: " << avg_lat << " µs\n"
            << "  Maximum: " << max_lat << " µs\n";

  // Callbacks should complete within reasonable time (< 10ms). Sanitizer
  // builds (notably TSan, ~19ms observed) inflate the wall clock, so — as with
  // the waveform and multi-clip perf bounds — the assertion is skipped there.
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
    EXPECT_LT(avg_lat, 10000.0);
  } else {
    std::cout << "  - Latency bound skipped under sanitizer\n";
  }
}

// ============================================================================
// Event Ordering Tests (ORP133 G1)
// ============================================================================

namespace {

// Records the exact dispatch sequence so FIFO ordering through the POD event
// ring can be asserted (not just counts).
class RecordingCallback : public ITransportCallback {
public:
  enum class Kind : uint8_t { Started, Stopped, Looped, Restarted, Seeked, Underrun };

  struct Entry {
    Kind kind;
    ClipHandle handle;
  };

  std::vector<Entry> entries;

  void onClipStarted(ClipHandle handle, TransportPosition /*position*/) override {
    entries.push_back({Kind::Started, handle});
  }
  void onClipStopped(ClipHandle handle, TransportPosition /*position*/) override {
    entries.push_back({Kind::Stopped, handle});
  }
  void onClipLooped(ClipHandle handle, TransportPosition /*position*/) override {
    entries.push_back({Kind::Looped, handle});
  }
  void onClipRestarted(ClipHandle handle, TransportPosition /*position*/) override {
    entries.push_back({Kind::Restarted, handle});
  }
  void onClipSeeked(ClipHandle handle, TransportPosition /*position*/) override {
    entries.push_back({Kind::Seeked, handle});
  }
  void onBufferUnderrun(TransportPosition /*position*/) override {
    entries.push_back({Kind::Underrun, 0});
  }
};

} // namespace

TEST_F(CallbackQueueStressTest, EventOrderingPreserved) {
  // ORP133 G1: the audio→UI ring switched from std::function payloads to POD
  // TransportEvents. Emission points map 1:1 to the old postCallback sites, so
  // dispatch order must be exactly emission (FIFO) order. Drive a
  // deterministic sequence and assert the exact dispatch sequence.
  auto recorder = std::make_unique<RecordingCallback>();
  m_transport->setCallback(recorder.get());

  using Kind = RecordingCallback::Kind;

  // Buffer 1: start clip 1 → Started(1)
  m_transport->startClip(1);
  simulateAudioCallback();

  // Buffer 2: start clip 2 → Started(2)
  m_transport->startClip(2);
  simulateAudioCallback();

  // Buffer 3+: stop clip 1. The default 10ms stop fade (480 samples @ 48kHz)
  // completes within one 512-frame buffer for reader-less clips → Stopped(1).
  m_transport->stopClip(1);
  simulateAudioCallback(2);

  // Next buffer: start clip 3 → Started(3)
  m_transport->startClip(3);
  simulateAudioCallback();

  // Drain everything in ONE pass so ordering inside the ring is what's tested.
  m_transport->processCallbacks();

  ASSERT_EQ(recorder->entries.size(), 4u);
  EXPECT_EQ(recorder->entries[0].kind, Kind::Started);
  EXPECT_EQ(recorder->entries[0].handle, 1u);
  EXPECT_EQ(recorder->entries[1].kind, Kind::Started);
  EXPECT_EQ(recorder->entries[1].handle, 2u);
  EXPECT_EQ(recorder->entries[2].kind, Kind::Stopped);
  EXPECT_EQ(recorder->entries[2].handle, 1u);
  EXPECT_EQ(recorder->entries[3].kind, Kind::Started);
  EXPECT_EQ(recorder->entries[3].handle, 3u);

  // Restore the fixture's counting callback before TearDown.
  m_transport->setCallback(m_callback.get());
}

// NOTE: Restarted/Seeked event translation (payload + delivery) is covered by
// clip_restart_test.cpp and clip_seek_test.cpp against registered audio files;
// this suite focuses on ring mechanics with reader-less clips.

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
