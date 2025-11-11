// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace orpheus {

// Forward declarations
namespace core {
class SessionGraph;
} // namespace core

/// Performance metrics for audio processing
///
/// This structure provides a snapshot of real-time audio processing performance.
/// All values are thread-safe and can be queried from the UI thread without
/// blocking the audio thread.
struct PerformanceMetrics {
  float cpuUsagePercent;          ///< CPU usage (0-100%)
  float latencyMs;                ///< Round-trip latency in milliseconds
  uint32_t bufferUnderrunCount;   ///< Total dropout count since start
  uint32_t activeClipCount;       ///< Currently playing clips
  uint64_t totalSamplesProcessed; ///< Lifetime sample count
  double uptimeSeconds;           ///< Time since audio thread started
};

/// Performance monitor for diagnostics and metering
///
/// This interface provides real-time performance monitoring for the audio engine.
/// All methods are thread-safe and designed to be called from the UI thread.
///
/// Thread Safety:
/// - All query methods are thread-safe (atomic reads from audio thread state)
/// - Reset methods are thread-safe (atomic writes)
/// - No locks are held in the audio callback path
///
/// Performance:
/// - getMetrics() completes in <100 CPU cycles (atomic reads only)
/// - Audio thread overhead: <1% CPU (single timestamp + atomic increments)
class IPerformanceMonitor {
public:
  virtual ~IPerformanceMonitor() = default;

  /// Get current performance metrics
  ///
  /// This function returns a snapshot of current performance metrics.
  /// All values are atomically consistent (captured at the same moment).
  ///
  /// @return Metrics structure (atomic snapshot)
  ///
  /// @note Thread-safe: Can be called from any thread
  /// @note Performance: <100 CPU cycles (no locks, atomic operations only)
  /// @note Typical use: Poll at 30 Hz from UI thread for real-time display
  virtual PerformanceMetrics getMetrics() const = 0;

  /// Reset buffer underrun counter
  ///
  /// This resets the dropout counter to zero. Useful after resolving
  /// performance issues to verify the fix.
  ///
  /// @note Thread-safe: Can be called from any thread
  virtual void resetUnderrunCount() = 0;

  /// Get peak CPU usage since last reset
  ///
  /// This returns the maximum CPU usage percentage observed since the
  /// monitor was created or since resetPeakCpuUsage() was last called.
  ///
  /// @return Peak CPU % (0-100%, can exceed 100% if callback takes longer than buffer duration)
  ///
  /// @note Thread-safe: Can be called from any thread
  /// @note Useful for profiling worst-case scenarios (e.g., plugin scan spikes)
  virtual float getPeakCpuUsage() const = 0;

  /// Reset peak CPU usage tracker
  ///
  /// This resets the peak CPU tracker to the current CPU usage value.
  /// Useful for measuring peak load during specific operations.
  ///
  /// @note Thread-safe: Can be called from any thread
  virtual void resetPeakCpuUsage() = 0;

  /// Get audio callback timing histogram
  ///
  /// This returns a histogram of audio callback execution times, useful for
  /// profiling jitter and diagnosing performance issues.
  ///
  /// @return Vector of {bucketMs, count} pairs
  ///         - bucketMs: Upper bound of timing bucket in milliseconds
  ///         - count: Number of callbacks that fell within this bucket
  ///
  /// @note Thread-safe: Can be called from any thread
  /// @note Example buckets: 0.5ms, 1ms, 2ms, 5ms, 10ms, 20ms, 50ms+
  /// @note Histogram is accumulated over the lifetime of the monitor
  virtual std::vector<std::pair<float, uint32_t>> getCallbackTimingHistogram() const = 0;

  /// Record audio callback performance (called by audio driver)
  ///
  /// This method should be called by the audio driver after each audio callback
  /// to update performance metrics. It measures CPU usage as a percentage of
  /// available time (callbackDuration / bufferDuration * 100).
  ///
  /// @param callbackDurationUs Audio callback execution time in microseconds
  /// @param bufferDurationUs Audio buffer duration in microseconds
  /// @param activeClips Number of currently active clips
  /// @param sampleRate Sample rate in Hz
  /// @param bufferSize Buffer size in samples
  ///
  /// @note Thread-safe: Should be called from audio thread
  /// @note Performance: <100 CPU cycles (atomic operations only, no locks)
  /// @note This is the preferred way to measure audio CPU (deadline-based, not wall-clock)
  ///
  /// @code
  /// auto start = std::chrono::high_resolution_clock::now();
  /// callback->processAudio(inputs, outputs, channels, frames);
  /// auto end = std::chrono::high_resolution_clock::now();
  /// uint64_t durationUs = std::chrono::duration_cast<std::chrono::microseconds>(end -
  /// start).count(); uint64_t bufferUs = (frames * 1'000'000) / sampleRate;
  /// monitor->recordAudioCallback(durationUs, bufferUs, activeClips, sampleRate, frames);
  /// @endcode
  virtual void recordAudioCallback(uint64_t callbackDurationUs, uint64_t bufferDurationUs,
                                   uint32_t activeClips, uint32_t sampleRate,
                                   uint32_t bufferSize) = 0;

  /// Report audio buffer underrun (called by audio driver)
  ///
  /// This method should be called whenever the audio driver detects a buffer
  /// underrun (dropout). This increments the underrun counter returned by getMetrics().
  ///
  /// @note Thread-safe: Should be called from audio thread
  /// @note Performance: <10 CPU cycles (single atomic increment)
  virtual void reportUnderrun() = 0;
};

/// Create performance monitor instance
///
/// This factory function creates a performance monitor that tracks the
/// performance of the audio processing in the provided SessionGraph.
///
/// @param sessionGraph Pointer to session graph (must remain valid for monitor lifetime)
///
/// @return Unique pointer to performance monitor instance
///
/// @note The session graph pointer must remain valid for the lifetime of the monitor
/// @note The monitor automatically hooks into the audio callback to measure performance
std::unique_ptr<IPerformanceMonitor> createPerformanceMonitor(core::SessionGraph* sessionGraph);

} // namespace orpheus
