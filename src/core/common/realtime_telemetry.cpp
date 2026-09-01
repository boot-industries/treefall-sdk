// SPDX-License-Identifier: MIT
#include "realtime_counter.h"
#include "spsc_observation.h"
#include <orpheus/realtime_telemetry.h>

#include <algorithm>

namespace orpheus {

RealtimeTelemetry::RealtimeTelemetry(uint32_t decimationBlocks) noexcept {
  setDecimationBlocks(decimationBlocks);
}

bool RealtimeTelemetry::beginRealtimeBlock(uint32_t bufferFrames, uint32_t sampleRate) noexcept {
  diagnostics_.recordCallback(bufferFrames, sampleRate);

  ++blocks_since_snapshot_;
  const uint32_t blocks = decimation_blocks_.load(std::memory_order_relaxed);
  if (blocks_since_snapshot_ < blocks) {
    return false;
  }

  blocks_since_snapshot_ = 0;
  return true;
}

void RealtimeTelemetry::reportUnderrunFromRealtime() noexcept {
  diagnostics_.reportUnderrun();
}

bool RealtimeTelemetry::publishFromRealtime(
    const RealtimeTelemetrySnapshot& snapshot) noexcept {
  const uint64_t writeIndex = write_index_.load(std::memory_order_relaxed);
  const uint64_t readIndex = read_index_.load(std::memory_order_acquire);
  const uint64_t sequence = next_sequence_;
  next_sequence_ = detail::saturatingIncrement(next_sequence_);
  if ((writeIndex - readIndex) >= kRealtimeTelemetryCapacity) {
    detail::publishSaturatingIncrement(dropped_snapshot_count_);
    return false;
  }

  RealtimeTelemetrySnapshot& slot = snapshots_[writeIndex % kRealtimeTelemetryCapacity];
  slot = snapshot;
  slot.sequence = sequence;
  slot.diagnostics = diagnostics_.snapshot();
  slot.schema_version = kRealtimeTelemetrySchemaVersion;
  slot.routing_meters.schema_version = kRoutingMeterTelemetrySchemaVersion;
  slot.routing_meters.group_output_meters.schema_version =
      kGroupOutputMeterSnapshotSchemaVersion;
  write_index_.store(writeIndex + 1, std::memory_order_release);
  return true;
}

bool RealtimeTelemetry::tryRead(RealtimeTelemetrySnapshot& snapshot) noexcept {
  const uint64_t readIndex = read_index_.load(std::memory_order_relaxed);
  const uint64_t writeIndex = write_index_.load(std::memory_order_acquire);
  if (readIndex == writeIndex) {
    return false;
  }

  snapshot = snapshots_[readIndex % kRealtimeTelemetryCapacity];
  read_index_.store(readIndex + 1, std::memory_order_release);
  return true;
}

void RealtimeTelemetry::setDecimationBlocks(uint32_t blocks) noexcept {
  decimation_blocks_.store(std::max(blocks, uint32_t{1}), std::memory_order_relaxed);
}

uint32_t RealtimeTelemetry::decimationBlocks() const noexcept {
  return decimation_blocks_.load(std::memory_order_relaxed);
}

uint64_t RealtimeTelemetry::droppedSnapshotCount() const noexcept {
  return dropped_snapshot_count_.load(std::memory_order_relaxed);
}

size_t RealtimeTelemetry::pendingSnapshotCount() const noexcept {
  return detail::observeBoundedPending(read_index_, write_index_, kRealtimeTelemetryCapacity);
}

} // namespace orpheus
