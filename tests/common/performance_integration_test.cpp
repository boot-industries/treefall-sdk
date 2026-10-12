// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>
#include <orpheus/performance_monitor.h>
#include <orpheus/session_graph.h>
#include <orpheus/transport_controller.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <future>
#include <thread>

using namespace orpheus;

// Integration test fixture with real SessionGraph
class PerformanceIntegrationTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_sessionGraph = std::make_unique<core::SessionGraph>();
    m_monitor = createPerformanceMonitor(m_sessionGraph.get());
    m_transport = createTransportController(
        m_sessionGraph.get(), TransportConfig{.sampleRate = static_cast<uint32_t>(48000)});
  }

  void TearDown() override {
    m_transport.reset();
    m_monitor.reset();
    m_sessionGraph.reset();
  }

  std::unique_ptr<core::SessionGraph> m_sessionGraph;
  std::unique_ptr<IPerformanceMonitor> m_monitor;
  std::unique_ptr<ITransportController> m_transport;
};

// Integration Tests

TEST_F(PerformanceIntegrationTest, MonitorWithRealSessionGraph) {
  // Create a real session graph
  m_sessionGraph->set_tempo(120.0);
  auto* track = m_sessionGraph->add_track("TestTrack");
  ASSERT_NE(track, nullptr);

  // Add a clip to the track
  auto* clip = m_sessionGraph->add_clip(*track, "TestClip", 0.0, 4.0, 0);
  ASSERT_NE(clip, nullptr);

  // Get metrics
  auto metrics = m_monitor->getMetrics();

  // Metrics should be valid
  EXPECT_GE(metrics.uptimeSeconds, 0.0);
  EXPECT_GE(metrics.cpuUsagePercent, 0.0f);
  EXPECT_GE(metrics.latencyMs, 0.0f);
}

TEST_F(PerformanceIntegrationTest, MonitorWithTransportController) {
  // Create monitor and transport controller on same session graph
  auto metrics = m_monitor->getMetrics();

  // Initial state
  EXPECT_EQ(metrics.activeClipCount, 0u);
  EXPECT_EQ(metrics.totalSamplesProcessed, 0u);

  // Note: Without actually processing audio, we can't test much more
  // In a real application, the audio thread would call updateMetrics()
}

TEST_F(PerformanceIntegrationTest, MetricsConsistencyOverTime) {
  // Sample metrics multiple times and verify consistency
  constexpr int kSamples = 100;
  constexpr int kDelayMs = 10;

  std::vector<PerformanceMetrics> samples;
  samples.reserve(kSamples);

  for (int i = 0; i < kSamples; ++i) {
    samples.push_back(m_monitor->getMetrics());
    std::this_thread::sleep_for(std::chrono::milliseconds(kDelayMs));
  }

  // Verify uptime increases monotonically
  for (size_t i = 1; i < samples.size(); ++i) {
    EXPECT_GE(samples[i].uptimeSeconds, samples[i - 1].uptimeSeconds);
  }

  // Verify other metrics remain stable (no random values)
  for (const auto& metrics : samples) {
    EXPECT_FALSE(std::isnan(metrics.cpuUsagePercent));
    EXPECT_FALSE(std::isinf(metrics.cpuUsagePercent));
    EXPECT_GE(metrics.cpuUsagePercent, 0.0f);
  }
}

TEST_F(PerformanceIntegrationTest, ResetOperationsDoNotAffectOtherMetrics) {
  m_monitor->recordAudioCallback(500, 1000, 7, 48000, 512);
  m_monitor->reportUnderrun();
  const auto before = m_monitor->getMetrics();
  const auto histogramBefore = m_monitor->getCallbackTimingHistogram();
  m_monitor->resetUnderrunCount();
  m_monitor->resetPeakCpuUsage();
  const auto pending = m_monitor->getMetrics();
  EXPECT_EQ(pending.bufferUnderrunCount, 0u);
  EXPECT_FLOAT_EQ(pending.cpuUsagePercent, before.cpuUsagePercent);
  EXPECT_FLOAT_EQ(pending.latencyMs, before.latencyMs);
  EXPECT_EQ(pending.activeClipCount, before.activeClipCount);
  EXPECT_EQ(pending.totalSamplesProcessed, before.totalSamplesProcessed);
  EXPECT_GE(pending.uptimeSeconds, before.uptimeSeconds);
  EXPECT_EQ(m_monitor->getCallbackTimingHistogram(), histogramBefore);
  EXPECT_FLOAT_EQ(m_monitor->getPeakCpuUsage(), 50.0f); // No callback has consumed reset.

  m_monitor->recordAudioCallback(0, 1000, 3, 0, 1024);
  const auto after = m_monitor->getMetrics();
  EXPECT_EQ(after.activeClipCount, 3u);
  EXPECT_EQ(after.totalSamplesProcessed, 1536u);
  EXPECT_FLOAT_EQ(after.latencyMs, before.latencyMs);
  EXPECT_FLOAT_EQ(m_monitor->getPeakCpuUsage(), before.cpuUsagePercent);
  const auto histogramAfter = m_monitor->getCallbackTimingHistogram();
  EXPECT_EQ(histogramAfter[0].second, histogramBefore[0].second + 1);
}

TEST_F(PerformanceIntegrationTest, HistogramRemainsStable) {
  // Get histogram multiple times
  auto hist1 = m_monitor->getCallbackTimingHistogram();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  auto hist2 = m_monitor->getCallbackTimingHistogram();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  auto hist3 = m_monitor->getCallbackTimingHistogram();

  // Histogram should have 7 buckets consistently
  EXPECT_EQ(hist1.size(), 7u);
  EXPECT_EQ(hist2.size(), 7u);
  EXPECT_EQ(hist3.size(), 7u);

  // Bucket boundaries should remain the same
  for (size_t i = 0; i < 7; ++i) {
    EXPECT_FLOAT_EQ(hist1[i].first, hist2[i].first);
    EXPECT_FLOAT_EQ(hist2[i].first, hist3[i].first);
  }

  // Counts should be non-decreasing (or stay at zero if no audio processing)
  for (size_t i = 0; i < 7; ++i) {
    EXPECT_GE(hist2[i].second, hist1[i].second);
    EXPECT_GE(hist3[i].second, hist2[i].second);
  }
}

TEST_F(PerformanceIntegrationTest, OverheadMeasurement) {
  // Measure overhead of performance monitoring
  // This test verifies that getMetrics() is fast enough for real-time use

  constexpr int kWarmupIterations = 1000;
  constexpr int kMeasureIterations = 100000;

  // Warmup
  for (int i = 0; i < kWarmupIterations; ++i) {
    auto metrics = m_monitor->getMetrics();
    (void)metrics;
  }

  // Measure
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < kMeasureIterations; ++i) {
    auto metrics = m_monitor->getMetrics();
    (void)metrics;
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

  double avgNanoseconds = static_cast<double>(duration.count()) / kMeasureIterations;
  double avgMicroseconds = avgNanoseconds / 1000.0;

  // Print results
  std::cout << "Performance Monitor Overhead:" << std::endl;
  std::cout << "  Average getMetrics() time: " << avgNanoseconds << " ns (" << avgMicroseconds
            << " µs)" << std::endl;

  // Target: <100 CPU cycles
  // On a 3 GHz CPU: 100 cycles = ~33 ns
  // We'll use a more generous target for CI: 500 ns
  EXPECT_LT(avgNanoseconds, 500.0) << "getMetrics() is too slow for real-time use";

  // Calculate overhead as percentage of 48kHz audio buffer (1024 samples = ~21ms)
  double bufferDurationUs = (1024.0 / 48000.0) * 1'000'000.0; // ~21,333 µs
  double overheadPercent = (avgMicroseconds / bufferDurationUs) * 100.0;

  std::cout << "  Overhead per 1024-sample buffer: " << overheadPercent << "%" << std::endl;

  // Target: <0.01% overhead (much less than the 2% target for overall monitoring)
  EXPECT_LT(overheadPercent, 0.01) << "getMetrics() overhead is too high";
}

TEST_F(PerformanceIntegrationTest, StressTestWithMultipleClips) {
  // Simulate stress test with 16 clips
  constexpr int kNumClips = 16;

  // Add 16 clips to session graph
  auto* track = m_sessionGraph->add_track("StressTrack");
  ASSERT_NE(track, nullptr);

  for (int i = 0; i < kNumClips; ++i) {
    auto* clip = m_sessionGraph->add_clip(*track, "Clip" + std::to_string(i),
                                          static_cast<double>(i * 4), 4.0, 0);
    ASSERT_NE(clip, nullptr);
  }

  // Get metrics
  auto metrics = m_monitor->getMetrics();

  // Metrics should still be valid even with many clips
  EXPECT_FALSE(std::isnan(metrics.cpuUsagePercent));
  EXPECT_FALSE(std::isinf(metrics.cpuUsagePercent));
  EXPECT_GE(metrics.uptimeSeconds, 0.0);
}

TEST_F(PerformanceIntegrationTest, ConcurrentAccessFromMultipleThreads) {
  // Simulate concurrent access from UI thread and other threads
  constexpr int kNumThreads = 8;
  constexpr int kIterationsPerThread = 1000;

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  std::atomic<bool> stopFlag{false};

  // Start threads that continuously read metrics
  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([this, &stopFlag]() {
      for (int j = 0; j < kIterationsPerThread && !stopFlag; ++j) {
        auto metrics = m_monitor->getMetrics();
        auto peak = m_monitor->getPeakCpuUsage();
        auto histogram = m_monitor->getCallbackTimingHistogram();

        // Verify data validity
        EXPECT_GE(metrics.cpuUsagePercent, 0.0f);
        EXPECT_GE(peak, 0.0f);
        EXPECT_EQ(histogram.size(), 7u);
      }
    });
  }

  // Wait for threads to complete
  for (auto& thread : threads) {
    thread.join();
  }
}

TEST_F(PerformanceIntegrationTest, ConcurrentProducerQueriesAndResetRequests) {
  constexpr unsigned kCallbacks = 5000;
  std::promise<void> startPromise;
  auto start = startPromise.get_future().share();
  std::atomic<bool> producerDone{false};
  std::atomic<unsigned> observations{0};
  std::promise<void> firstObservation;
  auto observed = firstObservation.get_future();
  std::promise<void> firstCallback;
  auto recorded = firstCallback.get_future();

  std::thread producer([&] {
    start.wait();
    m_monitor->recordAudioCallback(1000, 1000, 1, 48000, 512);
    firstCallback.set_value();
    observed.wait(); // Reader must sample a live producer before it continues.
    for (unsigned i = 1; i < kCallbacks; ++i) {
      m_monitor->recordAudioCallback(i % 1001, 1000, i % 33, 48000, 512);
      m_monitor->reportUnderrun();
    }
    producerDone.store(true, std::memory_order_release);
  });
  std::thread observer([&] {
    start.wait();
    recorded.wait();
    double previousUptime = 0.0;
    do {
      const auto metrics = m_monitor->getMetrics();
      // Independent aggregates: only per-field invariants, no same-callback claims.
      EXPECT_GE(metrics.cpuUsagePercent, 0.0f);
      EXPECT_LE(metrics.cpuUsagePercent, 100.0f);
      EXPECT_LE(metrics.activeClipCount, 32u);
      EXPECT_EQ(metrics.totalSamplesProcessed % 512, 0u);
      EXPECT_LE(metrics.totalSamplesProcessed, uint64_t{kCallbacks} * 512);
      EXPECT_GE(metrics.uptimeSeconds, previousUptime);
      previousUptime = metrics.uptimeSeconds;
      EXPECT_GE(m_monitor->getPeakCpuUsage(), 0.0f);
      uint32_t histogramTotal = 0;
      for (const auto& bucket : m_monitor->getCallbackTimingHistogram()) {
        histogramTotal += bucket.second;
      }
      EXPECT_LE(histogramTotal, kCallbacks);
      if (observations.fetch_add(1, std::memory_order_relaxed) == 0) {
        firstObservation.set_value();
      }
    } while (!producerDone.load(std::memory_order_acquire));
  });
  startPromise.set_value();
  for (unsigned i = 0; i < kCallbacks; ++i) {
    m_monitor->resetUnderrunCount();
    m_monitor->resetPeakCpuUsage();
  }
  producer.join();
  observer.join();

  EXPECT_GT(observations.load(), 0u);
  EXPECT_EQ(m_monitor->getMetrics().totalSamplesProcessed, uint64_t{kCallbacks} * 512);
  uint32_t histogramTotal = 0;
  for (const auto& bucket : m_monitor->getCallbackTimingHistogram()) {
    histogramTotal += bucket.second;
  }
  EXPECT_EQ(histogramTotal, kCallbacks);
  const auto last = m_monitor->getMetrics();
  m_monitor->resetPeakCpuUsage();
  m_monitor->recordAudioCallback(0, 1000, 0, 48000, 512); // Exclusive producer handoff after join.
  EXPECT_FLOAT_EQ(m_monitor->getPeakCpuUsage(), last.cpuUsagePercent);
}

TEST_F(PerformanceIntegrationTest, MonitorLifetimeSafety) {
  // Test that monitor can outlive session graph (should not crash)
  auto monitor = createPerformanceMonitor(m_sessionGraph.get());

  // Get initial metrics
  auto metrics1 = monitor->getMetrics();
  EXPECT_GE(metrics1.uptimeSeconds, 0.0);

  // Destroy session graph
  m_sessionGraph.reset();

  // Monitor should still be queryable (though session graph is gone)
  auto metrics2 = monitor->getMetrics();
  EXPECT_GE(metrics2.uptimeSeconds, metrics1.uptimeSeconds);
}
