// SPDX-License-Identifier: MIT
#include "orpheus/performance_monitor.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>

#include "session/session_graph.h"

namespace orpheus {

namespace {

/// Exponential moving average alpha (0.1 = 10% new value, 90% old value)
constexpr float kEmaAlpha = 0.1f;

/// Histogram bucket boundaries in milliseconds
constexpr std::array<float, 7> kHistogramBuckets = {0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 50.0f};

} // anonymous namespace

/// Implementation of IPerformanceMonitor
class PerformanceMonitorImpl : public IPerformanceMonitor {
public:
  explicit PerformanceMonitorImpl(core::SessionGraph* sessionGraph)
      : m_sessionGraph(sessionGraph), m_startTime(std::chrono::steady_clock::now()) {
    // Initialize histogram buckets
    for (size_t i = 0; i < kHistogramBuckets.size(); ++i) {
      m_histogramCounts[i].store(0, std::memory_order_relaxed);
    }
  }

  ~PerformanceMonitorImpl() override = default;

  // Prevent copying and moving
  PerformanceMonitorImpl(const PerformanceMonitorImpl&) = delete;
  PerformanceMonitorImpl& operator=(const PerformanceMonitorImpl&) = delete;
  PerformanceMonitorImpl(PerformanceMonitorImpl&&) = delete;
  PerformanceMonitorImpl& operator=(PerformanceMonitorImpl&&) = delete;

  PerformanceMetrics getMetrics() const override {
    PerformanceMetrics metrics{};

    // Read atomically (memory_order_relaxed is safe for metrics display)
    metrics.cpuUsagePercent = m_cpuUsagePercent.load(std::memory_order_relaxed);
    metrics.latencyMs = m_latencyMs.load(std::memory_order_relaxed);
    metrics.bufferUnderrunCount = m_underrunCount.load(std::memory_order_relaxed);
    metrics.activeClipCount = m_activeClipCount.load(std::memory_order_relaxed);
    metrics.totalSamplesProcessed = m_totalSamplesProcessed.load(std::memory_order_relaxed);

    // Calculate uptime
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - m_startTime);
    metrics.uptimeSeconds = static_cast<double>(duration.count()) / 1'000'000.0;

    return metrics;
  }

  void resetUnderrunCount() override {
    m_underrunCount.store(0, std::memory_order_relaxed);
  }

  float getPeakCpuUsage() const override {
    return m_peakCpuUsage.load(std::memory_order_relaxed);
  }

  void resetPeakCpuUsage() override {
    float currentCpu = m_cpuUsagePercent.load(std::memory_order_relaxed);
    m_peakCpuUsage.store(currentCpu, std::memory_order_relaxed);
  }

  std::vector<std::pair<float, uint32_t>> getCallbackTimingHistogram() const override {
    std::vector<std::pair<float, uint32_t>> histogram;
    histogram.reserve(kHistogramBuckets.size());

    for (size_t i = 0; i < kHistogramBuckets.size(); ++i) {
      uint32_t count = m_histogramCounts[i].load(std::memory_order_relaxed);
      histogram.emplace_back(kHistogramBuckets[i], count);
    }

    return histogram;
  }

  /// Called by audio thread to update metrics
  /// @param callbackDurationUs Audio callback duration in microseconds
  /// @param bufferDurationUs Buffer duration in microseconds
  /// @param activeClips Number of currently active clips
  /// @param sampleRate Sample rate in Hz
  /// @param bufferSize Buffer size in samples
  void updateMetrics(uint64_t callbackDurationUs, uint64_t bufferDurationUs, uint32_t activeClips,
                     uint32_t sampleRate, uint32_t bufferSize) {
    // Update total samples processed
    m_totalSamplesProcessed.fetch_add(bufferSize, std::memory_order_relaxed);

    // Update active clip count
    m_activeClipCount.store(activeClips, std::memory_order_relaxed);

    // Calculate CPU usage: (callbackDuration / bufferDuration) * 100
    float instantCpu = 0.0f;
    if (bufferDurationUs > 0) {
      instantCpu =
          (static_cast<float>(callbackDurationUs) / static_cast<float>(bufferDurationUs)) * 100.0f;
    }

    // Apply exponential moving average to smooth readings
    float currentCpu = m_cpuUsagePercent.load(std::memory_order_relaxed);
    float smoothedCpu = (kEmaAlpha * instantCpu) + ((1.0f - kEmaAlpha) * currentCpu);
    m_cpuUsagePercent.store(smoothedCpu, std::memory_order_relaxed);

    // Update peak CPU if higher
    float peakCpu = m_peakCpuUsage.load(std::memory_order_relaxed);
    if (instantCpu > peakCpu) {
      m_peakCpuUsage.store(instantCpu, std::memory_order_relaxed);
    }

    // Calculate latency (for now, use buffer size as approximation)
    // Total latency = (inputBuffer + processingBuffer + outputBuffer) / sampleRate * 1000
    // Simplified: latency ≈ bufferSize / sampleRate * 1000 ms
    if (sampleRate > 0) {
      float latency = (static_cast<float>(bufferSize) / static_cast<float>(sampleRate)) * 1000.0f;
      m_latencyMs.store(latency, std::memory_order_relaxed);
    }

    // Update timing histogram
    float callbackMs = static_cast<float>(callbackDurationUs) / 1000.0f;
    for (size_t i = 0; i < kHistogramBuckets.size(); ++i) {
      if (callbackMs <= kHistogramBuckets[i]) {
        m_histogramCounts[i].fetch_add(1, std::memory_order_relaxed);
        break;
      }
      // If we're past the last bucket, increment the last bucket (50ms+)
      if (i == kHistogramBuckets.size() - 1) {
        m_histogramCounts[i].fetch_add(1, std::memory_order_relaxed);
      }
    }
  }

  /// Called by audio thread when an underrun occurs
  void reportUnderrun() {
    m_underrunCount.fetch_add(1, std::memory_order_relaxed);
  }

private:
  [[maybe_unused]] core::SessionGraph* m_sessionGraph;
  std::chrono::steady_clock::time_point m_startTime;

  // Atomic metrics (updated by audio thread, read by UI thread)
  std::atomic<float> m_cpuUsagePercent{0.0f};
  std::atomic<float> m_latencyMs{0.0f};
  std::atomic<uint32_t> m_underrunCount{0};
  std::atomic<uint32_t> m_activeClipCount{0};
  std::atomic<uint64_t> m_totalSamplesProcessed{0};

  // Peak tracking
  std::atomic<float> m_peakCpuUsage{0.0f};

  // Histogram (7 buckets: 0.5ms, 1ms, 2ms, 5ms, 10ms, 20ms, 50ms+)
  std::array<std::atomic<uint32_t>, 7> m_histogramCounts;
};

// Factory function implementation
std::unique_ptr<IPerformanceMonitor> createPerformanceMonitor(core::SessionGraph* sessionGraph) {
  return std::make_unique<PerformanceMonitorImpl>(sessionGraph);
}

} // namespace orpheus
