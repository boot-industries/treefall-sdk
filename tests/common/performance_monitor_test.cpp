// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>
#include <orpheus/performance_monitor.h>
#include <orpheus/session_graph.h>

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>
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

// Reusable rendezvous for exact producer/control ordering, without sleeps.
class MonitorTestBarrier {
public:
  void arriveAndWait() {
    std::unique_lock<std::mutex> lock(m_mutex);
    const auto generation = m_generation;
    if (++m_arrivals == 2) {
      m_arrivals = 0;
      ++m_generation;
      m_changed.notify_all();
    } else {
      m_changed.wait(lock, [&] { return m_generation != generation; });
    }
  }

private:
  std::mutex m_mutex;
  std::condition_variable m_changed;
  unsigned m_arrivals = 0;
  unsigned m_generation = 0;
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

TEST_F(PerformanceMonitorTest, UnderrunResetClearsBeforeAndRetainsAfterExchange) {
  MonitorTestBarrier boundary;
  std::thread producer([&] {
    for (unsigned i = 0; i < 17; ++i) {
      m_monitor->reportUnderrun();
    }
    boundary.arriveAndWait(); // All preceding increments have completed.
    boundary.arriveAndWait(); // Reset must complete before the next increments.
    for (unsigned i = 0; i < 23; ++i) {
      m_monitor->reportUnderrun();
    }
    boundary.arriveAndWait();
  });

  boundary.arriveAndWait();
  EXPECT_EQ(m_monitor->getMetrics().bufferUnderrunCount, 17u);
  m_monitor->resetUnderrunCount();
  EXPECT_EQ(m_monitor->getMetrics().bufferUnderrunCount, 0u);
  boundary.arriveAndWait();
  boundary.arriveAndWait();
  EXPECT_EQ(m_monitor->getMetrics().bufferUnderrunCount, 23u);
  producer.join();
}

TEST_F(PerformanceMonitorTest, PeakCpuUsageInitiallyZero) {
  float peakCpu = m_monitor->getPeakCpuUsage();
  EXPECT_EQ(peakCpu, 0.0f);
}

TEST_F(PerformanceMonitorTest, PeakResetUsesProducerBoundaryAndPreviousSmoothedCpu) {
  MonitorTestBarrier boundary;
  std::thread producer([&] {
    m_monitor->recordAudioCallback(1000, 1000, 1, 48000, 512); // Instant 100, EMA 10.
    boundary.arriveAndWait();
    boundary.arriveAndWait();
    m_monitor->recordAudioCallback(20, 1000, 1, 48000, 512); // Instant 2, EMA 9.2.
    boundary.arriveAndWait();
    boundary.arriveAndWait();
    m_monitor->recordAudioCallback(250, 1000, 1, 48000, 512); // Instant 25 > baseline.
    boundary.arriveAndWait();
    boundary.arriveAndWait();
    m_monitor->recordAudioCallback(0, 1000, 1, 48000, 512);
    boundary.arriveAndWait();
  });

  boundary.arriveAndWait();
  EXPECT_FLOAT_EQ(m_monitor->getMetrics().cpuUsagePercent, 10.0f);
  EXPECT_FLOAT_EQ(m_monitor->getPeakCpuUsage(), 100.0f);
  m_monitor->resetPeakCpuUsage();
  EXPECT_FLOAT_EQ(m_monitor->getPeakCpuUsage(), 100.0f); // Idle request does not write peak.
  boundary.arriveAndWait();
  boundary.arriveAndWait();
  EXPECT_FLOAT_EQ(m_monitor->getPeakCpuUsage(), 10.0f); // Prior EMA, not new EMA 9.2.
  EXPECT_NEAR(m_monitor->getMetrics().cpuUsagePercent, 9.2f, 0.0001f);
  m_monitor->resetPeakCpuUsage(); // After the previous boundary: wait for another callback.
  EXPECT_FLOAT_EQ(m_monitor->getPeakCpuUsage(), 10.0f);
  boundary.arriveAndWait();
  boundary.arriveAndWait();
  EXPECT_FLOAT_EQ(m_monitor->getPeakCpuUsage(), 25.0f); // Includes reset callback's instant CPU.
  boundary.arriveAndWait();
  boundary.arriveAndWait();
  EXPECT_FLOAT_EQ(m_monitor->getPeakCpuUsage(), 25.0f); // No reset request remains.
  producer.join();
}

TEST_F(PerformanceMonitorTest, BufferDurationEstimateRetainsLastValueWithoutSampleRate) {
  m_monitor->recordAudioCallback(100, 1000, 0, 0, 512);
  EXPECT_FLOAT_EQ(m_monitor->getMetrics().latencyMs, 0.0f);
  m_monitor->recordAudioCallback(100, 1000, 0, 48000, 512);
  EXPECT_NEAR(m_monitor->getMetrics().latencyMs, 10.666667f, 0.00001f);
  m_monitor->recordAudioCallback(100, 1000, 0, 0, 2048);
  EXPECT_NEAR(m_monitor->getMetrics().latencyMs, 10.666667f, 0.00001f);
  m_monitor->recordAudioCallback(100, 1000, 0, 96000, 512);
  EXPECT_NEAR(m_monitor->getMetrics().latencyMs, 5.333333f, 0.00001f);
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

TEST_F(PerformanceMonitorTest, ConcurrentPeakResetRequestsCoalesce) {
  MonitorTestBarrier producerBoundary;
  std::thread producer([&] {
    m_monitor->recordAudioCallback(1000, 1000, 1, 48000, 512);
    producerBoundary.arriveAndWait();
    producerBoundary.arriveAndWait();
    m_monitor->recordAudioCallback(250, 1000, 1, 48000, 512);
    producerBoundary.arriveAndWait();
    producerBoundary.arriveAndWait();
    m_monitor->recordAudioCallback(0, 1000, 1, 48000, 512);
    producerBoundary.arriveAndWait();
  });

  producerBoundary.arriveAndWait();
  MonitorTestBarrier resetBoundary;
  std::thread resetter([&] {
    resetBoundary.arriveAndWait();
    for (unsigned i = 0; i < 100; ++i) {
      m_monitor->resetPeakCpuUsage();
    }
  });
  resetBoundary.arriveAndWait();
  for (unsigned i = 0; i < 100; ++i) {
    m_monitor->resetPeakCpuUsage();
  }
  resetter.join();
  EXPECT_FLOAT_EQ(m_monitor->getPeakCpuUsage(), 100.0f);
  producerBoundary.arriveAndWait();
  producerBoundary.arriveAndWait();
  EXPECT_FLOAT_EQ(m_monitor->getPeakCpuUsage(), 25.0f);
  producerBoundary.arriveAndWait();
  producerBoundary.arriveAndWait();
  EXPECT_FLOAT_EQ(m_monitor->getPeakCpuUsage(), 25.0f); // Requests consumed once, not replayed.
  producer.join();
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

TEST(PerformanceMonitorStandaloneTest, StandaloneFactory) {
  auto monitor = createStandalonePerformanceMonitor();
  ASSERT_NE(monitor, nullptr);

  monitor->recordAudioCallback(500, 1000, 2, 48000, 512);
  auto metrics = monitor->getMetrics();

  EXPECT_GE(metrics.cpuUsagePercent, 0.0f);
  EXPECT_EQ(metrics.activeClipCount, 2u);
}

TEST_F(PerformanceMonitorTest, LongRunningUptime) {
  auto metrics1 = m_monitor->getMetrics();
  EXPECT_GE(metrics1.uptimeSeconds, 0.0);

  // Simulate long running time (100ms)
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  auto metrics2 = m_monitor->getMetrics();
  EXPECT_GE(metrics2.uptimeSeconds - metrics1.uptimeSeconds, 0.1);
}
