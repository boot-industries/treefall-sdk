// SPDX-License-Identifier: MIT
// Performance Regression Tests (Sprint A4)

#include "../Source/Audio/AudioEngine.h"
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

#if defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__linux__)
#include <sys/resource.h>
#include <unistd.h>
#endif

/**
 * Test Suite: Performance Regression
 *
 * Tests performance baselines to catch regressions in CI/CD
 * - CPU usage (idle, under load)
 * - Memory usage (with varying clip counts)
 * - Latency measurements
 */

// Helper: Get current process memory usage in MB
size_t getProcessMemoryMB() {
#if defined(__APPLE__)
  struct task_basic_info info;
  mach_msg_type_number_t size = sizeof(info);
  kern_return_t kerr = task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &size);
  if (kerr == KERN_SUCCESS) {
    return info.resident_size / (1024 * 1024);
  }
#elif defined(__linux__)
  long rss = 0;
  FILE* fp = fopen("/proc/self/statm", "r");
  if (fp && fscanf(fp, "%*s%ld", &rss) == 1) {
    fclose(fp);
    return rss * sysconf(_SC_PAGESIZE) / (1024 * 1024);
  }
  if (fp)
    fclose(fp);
#endif
  return 0;
}

class PerformanceTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_engine = std::make_unique<AudioEngine>();
    if (!m_engine->initialize(48000)) {
      GTEST_SKIP() << "Audio device not available";
    }
  }

  void TearDown() override {
    m_engine.reset();
  }

  std::unique_ptr<AudioEngine> m_engine;
};

TEST_F(PerformanceTest, MemoryUsageIdle) {
  // Measure idle memory usage (no clips loaded)
  size_t memoryMB = getProcessMemoryMB();

  // Idle memory should be reasonable (<100MB)
  EXPECT_LT(memoryMB, 100) << "Idle memory usage: " << memoryMB << "MB";
}

TEST_F(PerformanceTest, MemoryUsageWith48Clips) {
  // Simulate loading 48 clips (1 full tab)
  // Note: Will fail to load since files don't exist, but metadata structures allocated
  for (int i = 0; i < 48; ++i) {
    m_engine->loadClip(i, "/tmp/dummy.wav");
  }

  size_t memoryMB = getProcessMemoryMB();

  // Memory with 48 clip slots allocated should be reasonable (<150MB)
  EXPECT_LT(memoryMB, 150) << "Memory with 48 clips: " << memoryMB << "MB";
}

TEST_F(PerformanceTest, MemoryUsageWith384Clips) {
  // Simulate loading all 384 clips (8 tabs full)
  for (int i = 0; i < 384; ++i) {
    m_engine->loadClip(i, "/tmp/dummy.wav");
  }

  size_t memoryMB = getProcessMemoryMB();

  // Memory with 384 clip slots should be <200MB (OCC100 target)
  EXPECT_LT(memoryMB, 200) << "Memory with 384 clips: " << memoryMB << "MB";
}

TEST_F(PerformanceTest, EngineStartLatency) {
  // Measure time to start audio engine
  auto start = std::chrono::high_resolution_clock::now();
  m_engine->start();
  auto end = std::chrono::high_resolution_clock::now();

  auto latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  // Engine start should be fast (<500ms)
  EXPECT_LT(latencyMs, 500) << "Engine start latency: " << latencyMs << "ms";

  m_engine->stop();
}

TEST_F(PerformanceTest, EngineStopLatency) {
  m_engine->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Measure time to stop audio engine
  auto start = std::chrono::high_resolution_clock::now();
  m_engine->stop();
  auto end = std::chrono::high_resolution_clock::now();

  auto latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  // Engine stop should be fast (<500ms)
  EXPECT_LT(latencyMs, 500) << "Engine stop latency: " << latencyMs << "ms";
}

TEST_F(PerformanceTest, GetLatencySamplesPerformance) {
  // Measure performance of latency query (should be instant)
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < 1000; ++i) {
    m_engine->getLatencySamples();
  }
  auto end = std::chrono::high_resolution_clock::now();

  auto totalUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  double avgUs = totalUs / 1000.0;

  // Average latency query should be <10 microseconds
  EXPECT_LT(avgUs, 10.0) << "Avg latency query time: " << avgUs << "µs";
}

TEST_F(PerformanceTest, IsClipPlayingPerformance) {
  // Measure performance of playing state query
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < 1000; ++i) {
    m_engine->isClipPlaying(0);
  }
  auto end = std::chrono::high_resolution_clock::now();

  auto totalUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  double avgUs = totalUs / 1000.0;

  // Average query should be <5 microseconds (critical for UI responsiveness)
  EXPECT_LT(avgUs, 5.0) << "Avg isClipPlaying query time: " << avgUs << "µs";
}

TEST_F(PerformanceTest, MultipleClipStatusQueries) {
  // Measure performance of querying all 384 clips
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < 384; ++i) {
    m_engine->isClipPlaying(i);
  }
  auto end = std::chrono::high_resolution_clock::now();

  auto totalUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

  // Querying all 384 clips should be <2ms (critical for UI refresh rate)
  EXPECT_LT(totalUs, 2000) << "Query 384 clips time: " << totalUs << "µs";
}
