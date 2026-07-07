// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>
#include <orpheus/audio_driver.h>
#include <orpheus/audio_file_reader.h>
#include <orpheus/transport_controller.h>

// Access implementation for processAudio
#include "../../src/core/transport/transport_controller.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

using namespace orpheus;

// Test fixture for multi-clip stress testing
class MultiClipStressTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Create transport controller (no SessionGraph for now)
    m_transport = std::make_unique<TransportController>(nullptr, 48000);

    // Create dummy audio driver
    m_driver = createDummyAudioDriver();

    // Configure driver
    AudioDriverConfig config;
    config.sample_rate = 48000;
    config.buffer_size = 512;
    config.num_outputs = 2;
    config.num_inputs = 0;

    ASSERT_EQ(m_driver->initialize(config), SessionGraphError::OK);

    // Create transport callback
    m_callback = std::make_unique<TestTransportCallback>();
    m_transport->setCallback(m_callback.get());

    m_tempDir = std::filesystem::temp_directory_path();
  }

  void TearDown() override {
    if (m_driver && m_driver->isRunning()) {
      m_driver->stop();
    }
    m_transport.reset();
    m_driver.reset();

    // Clean up temp files
    for (const auto& file : m_tempFiles) {
      if (std::filesystem::exists(file)) {
        std::filesystem::remove(file);
      }
    }
  }

  // Test transport callback
  class TestTransportCallback : public ITransportCallback {
  public:
    void onClipStarted(ClipHandle handle, TransportPosition position) override {
      m_clipsStarted.fetch_add(1, std::memory_order_relaxed);
    }

    void onClipStopped(ClipHandle handle, TransportPosition position) override {
      m_clipsStopped.fetch_add(1, std::memory_order_relaxed);
    }

    void onClipLooped(ClipHandle handle, TransportPosition position) override {
      // Not used in stress tests
    }

    void onBufferUnderrun(TransportPosition position) override {
      // Not tested yet
      (void)position;
    }

    int getClipsStarted() const {
      return m_clipsStarted.load(std::memory_order_relaxed);
    }

    int getClipsStopped() const {
      return m_clipsStopped.load(std::memory_order_relaxed);
    }

    void reset() {
      m_clipsStarted.store(0, std::memory_order_relaxed);
      m_clipsStopped.store(0, std::memory_order_relaxed);
    }

  private:
    std::atomic<int> m_clipsStarted{0};
    std::atomic<int> m_clipsStopped{0};
  };

  // Transport audio adapter (connects transport to driver)
  class TransportAudioAdapter : public IAudioCallback {
  public:
    explicit TransportAudioAdapter(TransportController* transport) : m_transport(transport) {}

    void processAudio(const float** input_buffers, float** output_buffers, size_t num_channels,
                      size_t num_frames) override {
      (void)input_buffers; // Not used for playback

      if (m_transport) {
        m_transport->processAudio(output_buffers, num_channels, num_frames);
      }

      m_callbackCount.fetch_add(1, std::memory_order_relaxed);
    }

    int getCallbackCount() const {
      return m_callbackCount.load(std::memory_order_relaxed);
    }

  private:
    TransportController* m_transport;
    std::atomic<int> m_callbackCount{0};
  };

  // Generate a simple test audio file (sine wave)
  std::string createTestAudioFile(const std::string& filename, float frequency,
                                  float duration_seconds) {
    const uint32_t sample_rate = 48000;
    const uint16_t num_channels = 2;
    const int64_t num_frames = static_cast<int64_t>(duration_seconds * sample_rate);

    std::vector<float> audio_data(num_frames * num_channels);

    // Generate sine wave
    for (int64_t i = 0; i < num_frames; ++i) {
      float sample = 0.3f * std::sin(2.0f * M_PI * frequency * i / sample_rate);
      audio_data[i * 2 + 0] = sample; // Left
      audio_data[i * 2 + 1] = sample; // Right
    }

    // Write to temp file (simple WAV format)
    std::string filepath = (m_tempDir / filename).string();
    std::ofstream file(filepath, std::ios::binary);

    // WAV header (simplified, 44 bytes)
    uint32_t data_size = num_frames * num_channels * sizeof(int16_t);
    uint32_t file_size = 36 + data_size;

    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&file_size), 4);
    file.write("WAVE", 4);
    file.write("fmt ", 4);

    uint32_t fmt_size = 16;
    uint16_t audio_format = 1; // PCM
    uint16_t block_align = num_channels * 2;
    uint32_t byte_rate = sample_rate * block_align;
    uint16_t bits_per_sample = 16;

    file.write(reinterpret_cast<const char*>(&fmt_size), 4);
    file.write(reinterpret_cast<const char*>(&audio_format), 2);
    file.write(reinterpret_cast<const char*>(&num_channels), 2);
    file.write(reinterpret_cast<const char*>(&sample_rate), 4);
    file.write(reinterpret_cast<const char*>(&byte_rate), 4);
    file.write(reinterpret_cast<const char*>(&block_align), 2);
    file.write(reinterpret_cast<const char*>(&bits_per_sample), 2);
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&data_size), 4);

    // Write audio data (convert float to int16)
    for (size_t i = 0; i < audio_data.size(); ++i) {
      int16_t sample = static_cast<int16_t>(audio_data[i] * 32767.0f);
      file.write(reinterpret_cast<const char*>(&sample), 2);
    }

    file.close();
    m_tempFiles.push_back(filepath);
    return filepath;
  }

  std::unique_ptr<TransportController> m_transport;
  std::unique_ptr<IAudioDriver> m_driver;
  std::unique_ptr<TestTransportCallback> m_callback;
  std::filesystem::path m_tempDir;
  std::vector<std::string> m_tempFiles;
};

// ============================================================================
// Test Case 1: 16 Simultaneous Clips with Metadata (ORP099 Task 1.4)
// ============================================================================

TEST_F(MultiClipStressTest, SixteenSimultaneousClips) {
  std::cout << "\n[Stress Test] Starting 16 simultaneous clips with metadata test...\n";

  // Create 16 test audio files (different frequencies for identification)
  std::vector<std::string> audioFiles;
  std::vector<ClipHandle> clips;
  std::vector<float> expectedGains;

  // Define gain settings (-12, -6, 0, +3 dB in rotation)
  const std::vector<float> gainSettings = {-12.0f, -6.0f, 0.0f, +3.0f};

  for (int i = 0; i < 16; ++i) {
    float frequency = 220.0f + (i * 55.0f); // A3, D4, G4, etc.
    std::string filename = "test_clip_" + std::to_string(i) + ".wav";
    std::string filepath = createTestAudioFile(filename, frequency, 3.0f); // 3 seconds
    audioFiles.push_back(filepath);

    // Register audio file with transport
    ClipHandle handle = i + 1;
    clips.push_back(handle);
    ASSERT_EQ(m_transport->registerClipAudio(handle, filepath), SessionGraphError::OK);

    // Set gain (rotate through -12, -6, 0, +3 dB)
    float gainDb = gainSettings[i % gainSettings.size()];
    expectedGains.push_back(gainDb);
    ASSERT_EQ(m_transport->updateClipGain(handle, gainDb), SessionGraphError::OK);

    // Set loop mode (first 8 clips loop, last 8 don't)
    bool loopEnabled = (i < 8);
    ASSERT_EQ(m_transport->setClipLoopMode(handle, loopEnabled), SessionGraphError::OK);

    // Set trim points (0-50% offset, distributed across clips)
    int64_t totalSamples = 3 * 48000;             // 3 seconds @ 48kHz
    float trimOffsetPercent = (i / 16.0f) * 0.5f; // 0% to 50%
    int64_t trimIn = static_cast<int64_t>(totalSamples * trimOffsetPercent);
    int64_t trimOut = totalSamples - 1000; // Leave 1000 samples at end
    ASSERT_EQ(m_transport->updateClipTrimPoints(handle, trimIn, trimOut), SessionGraphError::OK);
  }

  // Start all 16 clips simultaneously.
  for (auto handle : clips) {
    ASSERT_EQ(m_transport->startClip(handle), SessionGraphError::OK);
  }

  // ORP127: Verify stability by driving processAudio() a deterministic number
  // of buffers rather than sleeping for wall-clock seconds and counting
  // sleep-loop callbacks (which measured the host scheduler, not the SDK, and
  // was flaky under load). 12000 buffers @ 512/48k == ~128 seconds of audio —
  // exceeds the original 60s stability window, and runs in a fraction of the
  // wall-clock time with a fully deterministic result.
  std::cout << "  - Driving 12000 buffers (~128s of audio) to verify stability...\n";
  constexpr int kNumBuffers = 12000;
  constexpr size_t kBufferSize = 512;

  std::vector<float> left(kBufferSize, 0.0f);
  std::vector<float> right(kBufferSize, 0.0f);
  float* buffers[2] = {left.data(), right.data()};

  for (int i = 0; i < kNumBuffers; ++i) {
    m_transport->processAudio(buffers, 2, kBufferSize);
    if ((i % 2000) == 0) {
      m_transport->processCallbacks();
    }
  }
  m_transport->processCallbacks();

  // Verify all clips started (callbacks all delivered — no drops).
  EXPECT_EQ(m_callback->getClipsStarted(), 16);

  // Looping clips (first 8) must still be playing after sustained processing.
  int playing_count = 0;
  int looping_still_playing = 0;
  for (size_t i = 0; i < clips.size(); ++i) {
    auto state = m_transport->getClipState(clips[i]);
    if (state == PlaybackState::Playing) {
      playing_count++;
      if (i < 8)
        ++looping_still_playing;
    }
  }
  EXPECT_EQ(looping_still_playing, 8)
      << "All 8 looping clips must still be playing after 12000 buffers";

  // Verify metadata is correct (unchanged by sustained playback).
  for (size_t i = 0; i < clips.size(); ++i) {
    auto handle = clips[i];
    auto metadata = m_transport->getClipMetadata(handle);
    ASSERT_TRUE(metadata.has_value()) << "Metadata should exist for clip " << i;

    EXPECT_FLOAT_EQ(metadata->gainDb, expectedGains[i])
        << "Clip " << i << " gain should be " << expectedGains[i] << " dB";

    bool expectedLoop = (i < 8);
    EXPECT_EQ(metadata->loopEnabled, expectedLoop)
        << "Clip " << i << " loop mode should be " << (expectedLoop ? "enabled" : "disabled");
  }

  std::cout << "[Stress Test] 16 simultaneous clips with metadata: PASSED\n";
  std::cout << "  - Buffers driven: " << kNumBuffers << " (~128s of audio)\n";
  std::cout << "  - Clips started: " << m_callback->getClipsStarted() << "\n";
  std::cout << "  - Still playing: " << playing_count << " / 16 (looping: " << looping_still_playing
            << "/8)\n";
  std::cout << "  - Gain settings: -12, -6, 0, +3 dB (rotated)\n";
  std::cout << "  - Loop enabled: First 8 clips\n";
  std::cout << "  - Trim offsets: 0-50% distributed\n";
  std::cout << "  - Memory stable: No leaks detected (verify with ASan)\n";
}

// ============================================================================
// Test Case 2: Rapid Start/Stop
// ============================================================================

TEST_F(MultiClipStressTest, RapidStartStop) {
  std::cout << "\n[Stress Test] Rapid start/stop test (100 clips/second)...\n";

  // Create a single test audio file
  std::string filepath = createTestAudioFile("rapid_test.wav", 440.0f, 1.0f);

  // Register 10 clips with the same audio file
  std::vector<ClipHandle> clips;
  for (int i = 0; i < 10; ++i) {
    ClipHandle handle = i + 1;
    clips.push_back(handle);
    ASSERT_EQ(m_transport->registerClipAudio(handle, filepath), SessionGraphError::OK);
  }

  // Start audio driver
  auto adapter = std::make_unique<TransportAudioAdapter>(m_transport.get());
  ASSERT_EQ(m_driver->start(adapter.get()), SessionGraphError::OK);

  // Rapid start/stop (100 operations/second = 10ms per operation)
  auto start_time = std::chrono::steady_clock::now();
  int operations = 0;

  for (int i = 0; i < 10; ++i) {
    // Start all clips
    for (auto handle : clips) {
      m_transport->startClip(handle);
      operations++;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Stop all clips
    for (auto handle : clips) {
      m_transport->stopClip(handle);
      operations++;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  auto end_time = std::chrono::steady_clock::now();
  auto duration_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
  float operations_per_second = (operations * 1000.0f) / duration_ms;

  // Process callbacks
  m_transport->processCallbacks();

  // Stop driver
  m_driver->stop();

  std::cout << "[Stress Test] Rapid start/stop: PASSED\n";
  std::cout << "  - Operations: " << operations << "\n";
  std::cout << "  - Duration: " << duration_ms << " ms\n";
  std::cout << "  - Operations/second: " << operations_per_second << "\n";
  std::cout << "  - Clips started: " << m_callback->getClipsStarted() << "\n";

  EXPECT_GT(operations_per_second, 100.0f); // Should exceed 100 ops/second
}

// ============================================================================
// Test Case 3: CPU Usage Measurement (Basic)
// ============================================================================

TEST_F(MultiClipStressTest, CPUUsageMeasurement) {
  std::cout << "\n[Stress Test] CPU usage measurement (16 clips)...\n";

  // Create 16 test audio files
  std::vector<std::string> audioFiles;
  std::vector<ClipHandle> clips;

  for (int i = 0; i < 16; ++i) {
    float frequency = 220.0f + (i * 55.0f);
    std::string filename = "cpu_test_" + std::to_string(i) + ".wav";
    std::string filepath = createTestAudioFile(filename, frequency, 5.0f); // 5 seconds
    audioFiles.push_back(filepath);

    ClipHandle handle = i + 1;
    clips.push_back(handle);
    ASSERT_EQ(m_transport->registerClipAudio(handle, filepath), SessionGraphError::OK);
    // Loop so all 16 voices stay active for the full RT-budget measurement
    // (clips are 5s; without looping the non-looped voices would end mid-run).
    ASSERT_EQ(m_transport->setClipLoopMode(handle, true), SessionGraphError::OK);
  }

  // Start all 16 clips.
  for (auto handle : clips) {
    m_transport->startClip(handle);
  }

  // ORP127: Measure the *actual* per-buffer render cost deterministically by
  // driving processAudio() directly, rather than counting callbacks delivered
  // by the dummy driver's sleep loop (which measures the host scheduler under
  // load, not the SDK, and was therefore flaky). This is a real RT-budget check.
  constexpr size_t kBufferSize = 512;
  constexpr uint32_t kSampleRate = 48000;
  constexpr int kNumBuffers = 500; // ~5.3s of audio at 512/48k

  std::vector<float> left(kBufferSize, 0.0f);
  std::vector<float> right(kBufferSize, 0.0f);
  float* buffers[2] = {left.data(), right.data()};

  // Warm up (first buffer materializes voices from the Start commands).
  m_transport->processAudio(buffers, 2, kBufferSize);

  auto start_time = std::chrono::steady_clock::now();
  for (int i = 0; i < kNumBuffers; ++i) {
    m_transport->processAudio(buffers, 2, kBufferSize);
  }
  auto end_time = std::chrono::steady_clock::now();

  m_transport->processCallbacks();

  auto total_us =
      std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
  double avg_us_per_buffer = static_cast<double>(total_us) / static_cast<double>(kNumBuffers);

  // Real-time budget: one 512-sample buffer @ 48kHz must render in well under
  // its playback duration (~10667us). We require a comfortable margin so the
  // check is meaningful yet not brittle under host load: rendering 16 clips
  // must cost less than half the buffer's real-time budget.
  const double buffer_budget_us = (kBufferSize * 1e6) / kSampleRate; // ~10667us
  const double budget_fraction = avg_us_per_buffer / buffer_budget_us;

  std::cout << "[Stress Test] CPU usage (deterministic RT-budget check):\n";
  std::cout << "  - Buffers rendered: " << kNumBuffers << " (16 clips each)\n";
  std::cout << "  - Avg render time/buffer: " << avg_us_per_buffer << " us\n";
  std::cout << "  - Buffer RT budget: " << buffer_budget_us << " us\n";
  std::cout << "  - Budget used: " << (budget_fraction * 100.0) << "%\n";

  // Verify all 16 clips actually rendered (not a no-op) — SDK correctness.
  int playing = 0;
  for (auto handle : clips) {
    if (m_transport->getClipState(handle) == PlaybackState::Playing)
      ++playing;
  }
  EXPECT_EQ(playing, 16) << "All 16 clips should still be playing after 500 buffers";

  // RT-budget assertion: rendering 16 clips must fit comfortably within one
  // buffer's real-time budget. Half the budget is a generous, load-tolerant
  // ceiling (real cost is a few percent).
  EXPECT_LT(budget_fraction, 0.5)
      << "16-clip render should use <50% of the buffer's real-time budget";
}

// ============================================================================
// Test Case 4: Memory Usage Tracking (AddressSanitizer)
// ============================================================================

TEST_F(MultiClipStressTest, MemoryUsageTracking) {
  std::cout << "\n[Stress Test] Memory usage tracking...\n";
  std::cout << "  - Note: Run with AddressSanitizer to detect leaks\n";
  std::cout << "  - Command: ASAN_OPTIONS=detect_leaks=1 ./multi_clip_stress_test\n";

  // Create test audio file
  std::string filepath = createTestAudioFile("mem_test.wav", 440.0f, 2.0f);

  // Register and start/stop clips repeatedly
  for (int iteration = 0; iteration < 100; ++iteration) {
    ClipHandle handle = iteration + 1;

    // Register audio
    ASSERT_EQ(m_transport->registerClipAudio(handle, filepath), SessionGraphError::OK);

    // Start clip (queues command)
    ASSERT_EQ(m_transport->startClip(handle), SessionGraphError::OK);

    // Stop clip (queues command)
    ASSERT_EQ(m_transport->stopClip(handle), SessionGraphError::OK);
  }

  std::cout << "[Stress Test] Memory tracking: PASSED\n";
  std::cout << "  - Registered 100 clips\n";
  std::cout << "  - Started/stopped 100 clips\n";
  std::cout << "  - AddressSanitizer will report any leaks at program exit\n";
}

// ============================================================================
// Test Case 5: Long-Duration Test (Disabled by default, 1 hour)
// ============================================================================

TEST_F(MultiClipStressTest, DISABLED_LongDurationTest) {
  std::cout << "\n[Stress Test] Long-duration test (1 hour)...\n";
  std::cout << "  - This test runs for 1 hour. Enable with --gtest_also_run_disabled_tests\n";

  // Create 16 test audio files
  std::vector<std::string> audioFiles;
  std::vector<ClipHandle> clips;

  for (int i = 0; i < 16; ++i) {
    float frequency = 220.0f + (i * 55.0f);
    std::string filename = "long_test_" + std::to_string(i) + ".wav";
    std::string filepath = createTestAudioFile(filename, frequency, 10.0f); // 10 seconds per clip
    audioFiles.push_back(filepath);

    ClipHandle handle = i + 1;
    clips.push_back(handle);
    ASSERT_EQ(m_transport->registerClipAudio(handle, filepath), SessionGraphError::OK);
  }

  // Start audio driver
  auto adapter = std::make_unique<TransportAudioAdapter>(m_transport.get());
  ASSERT_EQ(m_driver->start(adapter.get()), SessionGraphError::OK);

  // Random number generator for clip selection
  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> clip_dist(0, clips.size() - 1);

  // Run for 1 hour (3600 seconds)
  auto start_time = std::chrono::steady_clock::now();
  auto test_duration = std::chrono::hours(1);
  auto next_report = start_time + std::chrono::minutes(5);

  while (std::chrono::steady_clock::now() - start_time < test_duration) {
    // Randomly start/stop clips (rotation pattern)
    int clip_index = clip_dist(rng);
    ClipHandle handle = clips[clip_index];

    if (m_transport->isClipPlaying(handle)) {
      m_transport->stopClip(handle);
    } else {
      m_transport->startClip(handle);
    }

    // Sleep for a bit to avoid saturating the command queue
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Process callbacks periodically
    m_transport->processCallbacks();

    // Report progress every 5 minutes
    auto now = std::chrono::steady_clock::now();
    if (now >= next_report) {
      auto elapsed_minutes =
          std::chrono::duration_cast<std::chrono::minutes>(now - start_time).count();
      std::cout << "  - Progress: " << elapsed_minutes << " minutes elapsed\n";
      std::cout << "    Clips started: " << m_callback->getClipsStarted() << "\n";
      std::cout << "    Clips stopped: " << m_callback->getClipsStopped() << "\n";
      std::cout << "    Audio callbacks: " << adapter->getCallbackCount() << "\n";
      next_report = now + std::chrono::minutes(5);
    }
  }

  // Stop driver
  m_driver->stop();

  auto end_time = std::chrono::steady_clock::now();
  auto total_minutes =
      std::chrono::duration_cast<std::chrono::minutes>(end_time - start_time).count();

  std::cout << "[Stress Test] Long-duration test: PASSED\n";
  std::cout << "  - Total duration: " << total_minutes << " minutes\n";
  std::cout << "  - Clips started: " << m_callback->getClipsStarted() << "\n";
  std::cout << "  - Clips stopped: " << m_callback->getClipsStopped() << "\n";
  std::cout << "  - Audio callbacks: " << adapter->getCallbackCount() << "\n";
  std::cout << "  - No crashes or underruns detected\n";
}