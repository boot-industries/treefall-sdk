// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/export.h>

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
/// Values are independently sampled, eventually consistent aggregates, not a
/// same-moment callback snapshot. Queries do not block the audio thread.
struct PerformanceMetrics {
  float cpuUsagePercent;          ///< CPU usage (0-100%)
  float latencyMs;                ///< Estimated callback-buffer duration in milliseconds;
                                  ///< not measured route latency. Zero sample rate retains
                                  ///< the last estimate (initially zero).
  uint32_t bufferUnderrunCount;   ///< Dropout count since the last ordered reset
  uint32_t activeClipCount;       ///< Last reported active clip count
  uint64_t totalSamplesProcessed; ///< Lifetime sample count
  double uptimeSeconds;           ///< Observation-time elapsed since monitor creation
};

/// Performance monitor for realtime diagnostics.
///
/// This interface provides real-time CPU, callback, and underrun diagnostics
/// for the audio engine. Audio-level metering is owned by the routing contract,
/// not this diagnostics interface.
///
/// Thread Safety:
/// - Query methods independently sample atomic aggregates; uptime is observation-time
/// - Reset methods are thread-safe; peak reset is deferred to the next callback
/// - recordAudioCallback() has one exclusive callback producer per monitor
/// - No locks or allocations are used in the callback recording path
///
/// latencyMs estimates bufferSize / sampleRate only. For measured route latency
/// and unknown terms, use AudioRouteLatency::complete and the directional
/// AudioLatencyBreakdown from the existing audio driver route APIs instead.
class IPerformanceMonitor {
public:
  virtual ~IPerformanceMonitor() = default;

  /// Get current performance metrics
  ///
  /// Fields are sampled independently and are eventually consistent; they need
  /// not describe the same callback. Uptime is computed at query observation time.
  ///
  /// @return Independently aggregated metrics
  ///
  /// @note Thread-safe: Can be called from any thread
  /// @note Typical use: Poll at 30 Hz from UI thread for real-time display
  virtual PerformanceMetrics getMetrics() const = 0;

  /// Reset buffer underrun counter
  ///
  /// The atomic exchange to zero is the reset's linearization point. Underrun
  /// increments ordered before it are cleared; increments ordered after it are
  /// retained. Other metrics and histogram counts are unchanged.
  ///
  /// @note Thread-safe: Can be called from any thread
  virtual void resetUnderrunCount() = 0;

  /// Get peak CPU usage since last reset
  ///
  /// Returns the maximum instantaneous CPU usage since creation or the last
  /// callback that consumed a reset request, including that reset's baseline
  /// of the preceding smoothed CPU usage. Pending resets do not change the peak.
  ///
  /// @return Peak CPU % (0-100%, can exceed 100% if callback takes longer than buffer duration)
  ///
  /// @note Thread-safe: Can be called from any thread
  /// @note Useful for profiling worst-case scenarios (e.g., plugin scan spikes)
  virtual float getPeakCpuUsage() const = 0;

  /// Reset peak CPU usage tracker
  ///
  /// Request a reset, consumed by the sole producer at the start of the next
  /// recordAudioCallback(). The producer initializes peak to its preceding
  /// smoothed CPU usage, then includes that callback's instantaneous CPU usage.
  /// Concurrent requests coalesce. While idle, peak is unchanged until the next
  /// callback; a request after a callback's reset boundary waits for a later one.
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
  /// @note Bucket counts are sampled independently, not as one coherent snapshot
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
  /// @param sampleRate Sample rate in Hz; zero retains the last latencyMs estimate
  /// @param bufferSize Buffer size in samples
  ///
  /// @note Thread-safe: One exclusive audio callback producer per monitor;
  ///       queries, resets, and underrun reports may run concurrently
  /// @note Callback recording uses bounded atomic operations, no locks or allocation
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
ORPHEUS_API std::unique_ptr<IPerformanceMonitor>
createPerformanceMonitor(core::SessionGraph* sessionGraph);

/// Create a performance monitor that is not tied to a SessionGraph.
///
/// This is equivalent to createPerformanceMonitor(nullptr) but names the
/// intent explicitly for lightweight consumers that do not own session state.
ORPHEUS_API std::unique_ptr<IPerformanceMonitor> createStandalonePerformanceMonitor();

} // namespace orpheus
