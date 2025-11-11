// SPDX-License-Identifier: MIT
#include "session/session_graph.h"
#include <gtest/gtest.h>
#include <orpheus/performance_monitor.h>

#include <chrono>
#include <thread>

using namespace orpheus;

// Mock session graph for testing
class MockSessionGraph : public core::SessionGraph {
public:
  MockSessionGraph() : core::SessionGraph() {}
};

// Test fixture
class PerformanceMonitorTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_sessionGraph = std::make_unique<MockSessionGraph>();
    m_monitor = createPerformanceMonitor(m_sessionGraph.get());
  }

  void TearDown() override {
    m_monitor.reset();
    m_sessionGraph.reset();
  }

  std::unique_ptr<MockSessionGraph> m_sessionGraph;
  std::unique_ptr<IPerformanceMonitor> m_monitor;
};

// Helper to access private updateMetrics method for testing
class TestablePerformanceMonitor {
public:
  static void updateMetrics(IPerformanceMonitor* monitor, uint64_t callbackDurationUs,
                            uint64_t bufferDurationUs, uint32_t activeClips, uint32_t sampleRate,
                            uint32_t bufferSize) {
    // This is a workaround for testing - in production, the audio thread calls updateMetrics
    // For now, we'll test via the public API only
  }
};

// Basic Tests

TEST_F(PerformanceMonitorTest, InitialState) {
  auto metrics = m_monitor->getMetrics();

  // Initially, all metrics should be zero or default values
  EXPECT_EQ(metrics.cpuUsagePercent, 0.0f);
  EXPECT_EQ(metrics.latencyMs, 0.0f);
  EXPECT_EQ(metrics.bufferUnderrunCount, 0u);
  EXPECT_EQ(metrics.activeClipCount, 0u);
  EXPECT_EQ(metrics.totalSamplesProcessed, 0u);
  EXPECT_GE(metrics.uptimeSeconds, 0.0);
}

TEST_F(PerformanceMonitorTest, UptimeIncreases) {
  auto metrics1 = m_monitor->getMetrics();
  double uptime1 = metrics1.uptimeSeconds;

  // Wait a short time
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  auto metrics2 = m_monitor->getMetrics();
  double uptime2 = metrics2.uptimeSeconds;

  // Uptime should increase
  EXPECT_GT(uptime2, uptime1);
  EXPECT_GE(uptime2 - uptime1, 0.05); // At least 50ms elapsed
}

TEST_F(PerformanceMonitorTest, ResetUnderrunCount) {
  // Initially zero
  auto metrics1 = m_monitor->getMetrics();
  EXPECT_EQ(metrics1.bufferUnderrunCount, 0u);

  // Reset (should still be zero)
  m_monitor->resetUnderrunCount();

  auto metrics2 = m_monitor->getMetrics();
  EXPECT_EQ(metrics2.bufferUnderrunCount, 0u);

  // Note: We can't easily increment underrun count from tests without
  // exposing internal methods, so we're limited in testing this
}

TEST_F(PerformanceMonitorTest, PeakCpuUsageInitiallyZero) {
  float peakCpu = m_monitor->getPeakCpuUsage();
  EXPECT_EQ(peakCpu, 0.0f);
}

TEST_F(PerformanceMonitorTest, ResetPeakCpuUsage) {
  // Reset peak (should set to current CPU usage, which is 0)
  m_monitor->resetPeakCpuUsage();

  float peakCpu = m_monitor->getPeakCpuUsage();
  EXPECT_EQ(peakCpu, 0.0f);
}

TEST_F(PerformanceMonitorTest, CallbackTimingHistogramInitiallyEmpty) {
  auto histogram = m_monitor->getCallbackTimingHistogram();

  // Histogram should have 7 buckets
  EXPECT_EQ(histogram.size(), 7u);

  // All buckets should be zero initially
  for (const auto& [bucketMs, count] : histogram) {
    EXPECT_EQ(count, 0u);
    EXPECT_GT(bucketMs, 0.0f); // Bucket boundary should be positive
  }
}

TEST_F(PerformanceMonitorTest, HistogramBucketBoundaries) {
  auto histogram = m_monitor->getCallbackTimingHistogram();

  // Verify bucket boundaries match expected values
  ASSERT_EQ(histogram.size(), 7u);
  EXPECT_FLOAT_EQ(histogram[0].first, 0.5f);
  EXPECT_FLOAT_EQ(histogram[1].first, 1.0f);
  EXPECT_FLOAT_EQ(histogram[2].first, 2.0f);
  EXPECT_FLOAT_EQ(histogram[3].first, 5.0f);
  EXPECT_FLOAT_EQ(histogram[4].first, 10.0f);
  EXPECT_FLOAT_EQ(histogram[5].first, 20.0f);
  EXPECT_FLOAT_EQ(histogram[6].first, 50.0f);
}

TEST_F(PerformanceMonitorTest, MetricsStructureValidity) {
  auto metrics = m_monitor->getMetrics();

  // All numeric values should be valid (not NaN or Inf)
  EXPECT_FALSE(std::isnan(metrics.cpuUsagePercent));
  EXPECT_FALSE(std::isinf(metrics.cpuUsagePercent));
  EXPECT_FALSE(std::isnan(metrics.latencyMs));
  EXPECT_FALSE(std::isinf(metrics.latencyMs));
  EXPECT_FALSE(std::isnan(metrics.uptimeSeconds));
  EXPECT_FALSE(std::isinf(metrics.uptimeSeconds));

  // Percentages should be in reasonable ranges
  EXPECT_GE(metrics.cpuUsagePercent, 0.0f);
  EXPECT_GE(metrics.latencyMs, 0.0f);
  EXPECT_GE(metrics.uptimeSeconds, 0.0);
}

TEST_F(PerformanceMonitorTest, ThreadSafety_ConcurrentReads) {
  // Test that concurrent reads don't crash or produce inconsistent data
  constexpr int kNumThreads = 4;
  constexpr int kIterationsPerThread = 1000;

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([this]() {
      for (int j = 0; j < kIterationsPerThread; ++j) {
        auto metrics = m_monitor->getMetrics();
        auto peak = m_monitor->getPeakCpuUsage();
        auto histogram = m_monitor->getCallbackTimingHistogram();

        // Basic sanity checks
        EXPECT_GE(metrics.cpuUsagePercent, 0.0f);
        EXPECT_GE(peak, 0.0f);
        EXPECT_EQ(histogram.size(), 7u);
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }
}

TEST_F(PerformanceMonitorTest, ThreadSafety_ConcurrentResets) {
  // Test that concurrent resets don't crash
  constexpr int kNumThreads = 4;
  constexpr int kIterationsPerThread = 100;

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([this]() {
      for (int j = 0; j < kIterationsPerThread; ++j) {
        m_monitor->resetUnderrunCount();
        m_monitor->resetPeakCpuUsage();

        // Also do some reads
        auto metrics = m_monitor->getMetrics();
        (void)metrics; // Suppress unused warning
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }
}

TEST_F(PerformanceMonitorTest, MultipleMonitorInstances) {
  // Test that multiple monitor instances can coexist
  auto monitor2 = createPerformanceMonitor(m_sessionGraph.get());

  auto metrics1 = m_monitor->getMetrics();
  auto metrics2 = monitor2->getMetrics();

  // Both should return valid metrics
  EXPECT_GE(metrics1.uptimeSeconds, 0.0);
  EXPECT_GE(metrics2.uptimeSeconds, 0.0);
}

TEST_F(PerformanceMonitorTest, PerformanceOfGetMetrics) {
  // Measure performance of getMetrics() - should be <100 CPU cycles
  // On a 3 GHz CPU, 100 cycles = ~33 nanoseconds
  constexpr int kIterations = 10000;

  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < kIterations; ++i) {
    auto metrics = m_monitor->getMetrics();
    (void)metrics; // Suppress unused warning
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

  double avgNanoseconds = static_cast<double>(duration.count()) / kIterations;

  // Should average less than 1000 nanoseconds (generous target for CI)
  EXPECT_LT(avgNanoseconds, 1000.0);

  // Print for informational purposes
  std::cout << "Average getMetrics() time: " << avgNanoseconds << " ns" << std::endl;
}

// Edge Cases

TEST_F(PerformanceMonitorTest, NullSessionGraph) {
  // createPerformanceMonitor should accept nullptr (doesn't crash)
  auto monitor = createPerformanceMonitor(nullptr);
  EXPECT_NE(monitor, nullptr);

  auto metrics = monitor->getMetrics();
  EXPECT_GE(metrics.uptimeSeconds, 0.0);
}

TEST_F(PerformanceMonitorTest, LongRunningUptime) {
  auto metrics1 = m_monitor->getMetrics();
  EXPECT_GE(metrics1.uptimeSeconds, 0.0);

  // Simulate long running time (100ms)
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  auto metrics2 = m_monitor->getMetrics();
  EXPECT_GE(metrics2.uptimeSeconds - metrics1.uptimeSeconds, 0.1);
}

// Note: We cannot easily test the internal updateMetrics() method without
// exposing it or creating a test-only subclass. In production, the audio
// thread will call this method. Integration tests will verify end-to-end behavior.
