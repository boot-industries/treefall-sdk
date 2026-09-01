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
inline constexpr uint32_t kRealtimeTelemetrySchemaVersion = 3;

/// Schema version for the canonical routing-meter payload.
inline constexpr uint32_t kRoutingMeterTelemetrySchemaVersion = 1;

/// Default cadence: retain one snapshot after every eight audio callbacks.
inline constexpr uint32_t kRealtimeTelemetryDefaultDecimationBlocks = 8;
/// Maximum logical groups carried by the legacy fields in one telemetry snapshot.
inline constexpr size_t kRealtimeTelemetryMaxGroups = 16;
/// Maximum routing output lanes carried by one telemetry snapshot.
inline constexpr size_t kRealtimeTelemetryMaxOutputs = kRoutingMaxOutputs;
/// Number of snapshots retained by RealtimeTelemetry before new captures drop.
inline constexpr size_t kRealtimeTelemetryCapacity = 64;

/// Canonical routing-level metering captured at one transport publication.
///
/// After an OK routing render with RoutingConfig::enable_metering, availability
/// is Measured. Standard aggregate domains use the active routing configuration:
/// group_aggregate_meters is bounded by num_groups and
/// post_master_output_lane_meters by num_outputs, independently of the
/// optional nested logical-group-output extension. Indices beyond those counts
/// are Unconfigured. If metering is disabled or routing fails, the outer and
/// standard domains are Unmeasured and both window fields are zero. An
/// individual unsupported/unmeasured domain has ignored meter fields while
/// measured domains retain the frame windows.
///
/// peak_window_frames is the saturating sum of successful transport-callback
/// frame counts since the prior successful enqueue. rms_window_frames is the
/// final successful callback's numFrames. Canonical peak is the maximum across
/// the peak window; canonical RMS is from the final callback. A zero-frame
/// callback extends neither window. Logical-lane raw_block_frames remains the
/// independent routing-slice length.
struct RoutingMeterTelemetry {
  uint32_t schema_version{kRoutingMeterTelemetrySchemaVersion};
  MeterAvailability availability{MeterAvailability::Unsupported};
  MeterPeakDefinition aggregate_peak_definition{MeterPeakDefinition::SamplePeak};
  MeterPeakDefinition post_master_output_peak_definition{MeterPeakDefinition::TruePeak4x};
  uint8_t reserved0{0};
  uint16_t post_master_output_count{0};
  uint64_t peak_window_frames{0};
  uint32_t rms_window_frames{0};
  uint32_t reserved1{0};
  std::array<MeterAvailability, kRoutingControlMaxGroups> group_aggregate_availability{};
  std::array<AudioMeter, kRoutingControlMaxGroups> group_aggregate_meters{};
  GroupOutputMeterSnapshot group_output_meters{};
  MeterAvailability master_aggregate_availability{MeterAvailability::Unsupported};
  std::array<uint8_t, 3> reserved2{};
  AudioMeter master_aggregate_meter{};
  std::array<MeterAvailability, kRoutingMaxOutputs> post_master_output_availability{};
  std::array<AudioMeter, kRoutingMaxOutputs> post_master_output_lane_meters{};
};

static_assert(std::is_trivially_copyable_v<RoutingMeterTelemetry>);
static_assert(std::is_standard_layout_v<RoutingMeterTelemetry>);

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
  RoutingMeterTelemetry routing_meters{};
};

static_assert(std::is_trivially_copyable_v<RealtimeTelemetrySnapshot>);
static_assert(std::is_standard_layout_v<RealtimeTelemetrySnapshot>);

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

  /// Publish a due snapshot by copying into the fixed ring slot. Returns false
  /// when full; attempt sequence advances before that capacity check. The
  /// bridge stamps schema_version, sequence, diagnostics, and nested schemas.
  [[nodiscard]] bool publishFromRealtime(const RealtimeTelemetrySnapshot& snapshot) noexcept;
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
