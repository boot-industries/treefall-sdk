// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/export.h>
#include <orpheus/realtime_diagnostics.h>
#include <orpheus/routing_matrix.h>
#include <orpheus/time_domain.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace orpheus {

/// Stable schema version for RealtimeTelemetrySnapshot.
inline constexpr uint32_t kRealtimeTelemetrySchemaVersion = 2;

/// Default cadence: retain one snapshot after every eight audio callbacks.
inline constexpr uint32_t kRealtimeTelemetryDefaultDecimationBlocks = 8;
/// Maximum routing groups carried by one telemetry snapshot.
inline constexpr size_t kRealtimeTelemetryMaxGroups = 16;
/// Maximum physical output lanes carried by one telemetry snapshot.
inline constexpr size_t kRealtimeTelemetryMaxOutputs = kRoutingMaxOutputs;

/// Number of snapshots retained by RealtimeTelemetry before new captures drop.
inline constexpr size_t kRealtimeTelemetryCapacity = 64;

/// Fixed-size transport, routing, and callback-health observation.
///
/// This is deliberately presentation-neutral. Hosts may build meter histories,
/// FFT analyzers, view models, and smoothing on the message thread after
/// tryRead(); none of that application state belongs in the realtime bridge.
struct RealtimeTelemetrySnapshot {
  uint32_t schema_version{kRealtimeTelemetrySchemaVersion};
  uint64_t sequence{0}; ///< Monotonic capture attempt; gaps identify dropped snapshots.
  TimePoint position{}; ///< Canonical post-block transport position.
  RealtimeDiagnosticsSnapshot diagnostics{};
  uint32_t active_voice_count{0};
  uint8_t group_count{0};
  std::array<AudioMeter, kRealtimeTelemetryMaxGroups> group_meters{};
  uint8_t output_count{0};
  std::array<AudioMeter, kRealtimeTelemetryMaxOutputs> output_meters{};
  AudioMeter master_meter{};
};

static_assert(std::is_trivially_copyable_v<RealtimeTelemetrySnapshot>);

/// Fixed-capacity single-producer/single-consumer realtime telemetry bridge.
///
/// Thread and lifetime contract:
/// - Exactly one realtime producer calls beginRealtimeBlock(),
///   reportUnderrunFromRealtime(), and publishFromRealtime().
/// - Exactly one message-thread consumer calls tryRead().
/// - setDecimationBlocks() and the const queries may be called from any thread.
/// - The bridge must outlive both producer and consumer; callers must stop both
///   threads before destroying it.
///
/// The producer never allocates, locks, blocks, performs I/O, or overwrites an
/// unread slot. A full ring drops the new snapshot and increments the drop
/// count. The consumer owns all work performed after tryRead().
class ORPHEUS_API RealtimeTelemetry {
public:
  explicit RealtimeTelemetry(
      uint32_t decimationBlocks = kRealtimeTelemetryDefaultDecimationBlocks) noexcept;

  RealtimeTelemetry(const RealtimeTelemetry&) = delete;
  RealtimeTelemetry& operator=(const RealtimeTelemetry&) = delete;

  /// Record one callback and return true when this block should be captured.
  /// Call once per realtime callback, before publishFromRealtime().
  [[nodiscard]] bool beginRealtimeBlock(uint32_t bufferFrames, uint32_t sampleRate) noexcept;

  /// Add one source/cache underrun to the next diagnostics snapshot.
  void reportUnderrunFromRealtime() noexcept;

  /// Publish a due snapshot. Returns false when the fixed ring is full.
  /// The bridge stamps schema_version, sequence, and diagnostics.
  [[nodiscard]] bool publishFromRealtime(RealtimeTelemetrySnapshot snapshot) noexcept;

  /// Read the oldest retained snapshot on the single consumer thread.
  [[nodiscard]] bool tryRead(RealtimeTelemetrySnapshot& snapshot) noexcept;

  /// Capture one snapshot after each N callbacks. Zero is clamped to one.
  void setDecimationBlocks(uint32_t blocks) noexcept;
  [[nodiscard]] uint32_t decimationBlocks() const noexcept;

  [[nodiscard]] uint64_t droppedSnapshotCount() const noexcept;
  [[nodiscard]] size_t pendingSnapshotCount() const noexcept;

private:
  std::array<RealtimeTelemetrySnapshot, kRealtimeTelemetryCapacity> snapshots_{};
  std::atomic<uint64_t> write_index_{0};
  std::atomic<uint64_t> read_index_{0};
  std::atomic<uint64_t> dropped_snapshot_count_{0};
  std::atomic<uint32_t> decimation_blocks_{kRealtimeTelemetryDefaultDecimationBlocks};

  RealtimeDiagnostics diagnostics_{};
  uint32_t blocks_since_snapshot_{0}; // Producer thread only.
  uint64_t next_sequence_{1};         // Producer thread only.
};

} // namespace orpheus
