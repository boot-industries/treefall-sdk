// SPDX-License-Identifier: MIT
// Performance Regression Tests (Sprint A4)

#include "../Source/Audio/AudioEngine.h"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <gtest/gtest.h>
#include <juce_audio_formats/juce_audio_formats.h>
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
  static void writeTestWavFile(const juce::File& file) {
    juce::WavAudioFormat format;
    auto stream = file.createOutputStream();
    ASSERT_NE(stream, nullptr);

    std::unique_ptr<juce::AudioFormatWriter> writer(
        format.createWriterFor(stream.release(), 48000.0, 2, 16, {}, 0));
    ASSERT_NE(writer, nullptr);

    constexpr int numSamples = 48000;
    juce::AudioBuffer<float> buffer(2, numSamples);
    for (int sample = 0; sample < numSamples; ++sample) {
      auto value =
          0.2f * std::sin(2.0 * juce::MathConstants<double>::pi * 440.0 * sample / 48000.0);
      buffer.setSample(0, sample, value);
      buffer.setSample(1, sample, value);
    }

    ASSERT_TRUE(writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()));
  }

  void SetUp() override {
    auto uniqueName = "clip-composer-performance-" +
                      juce::String(juce::Time::getMillisecondCounterHiRes(), 0) + "-" +
                      juce::String(juce::Random::getSystemRandom().nextInt()) + ".wav";
    auto tempDirectory = juce::File(std::filesystem::temp_directory_path().string());
    m_testAudioFile = tempDirectory.getChildFile(uniqueName);
    m_testAudioFile.getParentDirectory().createDirectory();
    writeTestWavFile(m_testAudioFile);

    m_engine = std::make_unique<AudioEngine>();
    if (!m_engine->initialize(48000)) {
      GTEST_SKIP() << "Audio device not available";
    }
  }

  void TearDown() override {
    m_engine.reset();
    if (m_testAudioFile.existsAsFile()) {
      m_testAudioFile.deleteFile();
    }
  }

  std::unique_ptr<AudioEngine> m_engine;
  juce::File m_testAudioFile;
};

TEST_F(PerformanceTest, MemoryUsageIdle) {
  // Measure idle memory usage (no clips loaded)
  size_t memoryMB = getProcessMemoryMB();

  // Idle memory should be reasonable (<100MB)
  EXPECT_LT(memoryMB, 100) << "Idle memory usage: " << memoryMB << "MB";
}

TEST_F(PerformanceTest, MemoryUsageWith48Clips) {
  for (int i = 0; i < 48; ++i) {
    ASSERT_TRUE(m_engine->loadClip(i, m_testAudioFile.getFullPathName()));
  }

  size_t memoryMB = getProcessMemoryMB();

  // Memory with 48 clip slots allocated should be reasonable (<150MB)
  EXPECT_LT(memoryMB, 150) << "Memory with 48 clips: " << memoryMB << "MB";
}

TEST_F(PerformanceTest, MemoryUsageWith384Clips) {
  for (int i = 0; i < 384; ++i) {
    ASSERT_TRUE(m_engine->loadClip(i, m_testAudioFile.getFullPathName()));
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
