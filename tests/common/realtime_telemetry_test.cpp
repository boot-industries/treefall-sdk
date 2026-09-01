// SPDX-License-Identifier: MIT
#include "../../src/core/common/spsc_observation.h"
#include "../../src/core/common/realtime_counter.h"
#include <gtest/gtest.h>
#include <orpheus/realtime_telemetry.h>

#include <atomic>
#include <limits>
#include <type_traits>
using namespace orpheus;

TEST(RealtimeTelemetryTest, CapturesAtExactBlockCadence) {
  RealtimeTelemetry telemetry(3);

  EXPECT_FALSE(telemetry.beginRealtimeBlock(128, 48000));
  EXPECT_FALSE(telemetry.beginRealtimeBlock(128, 48000));
  EXPECT_TRUE(telemetry.beginRealtimeBlock(128, 48000));

  RealtimeTelemetrySnapshot input;
  input.position = TimePoint::fromSamples(384);
  ASSERT_TRUE(telemetry.publishFromRealtime(input));

  RealtimeTelemetrySnapshot output;
  ASSERT_TRUE(telemetry.tryRead(output));
  EXPECT_EQ(output.sequence, 1u);
  EXPECT_EQ(output.position.samples(), 384);
  EXPECT_EQ(output.diagnostics.callback_count, 3u);
  EXPECT_EQ(output.diagnostics.samples_processed, 384u);
  EXPECT_EQ(output.diagnostics.last_buffer_frames, 128u);
  EXPECT_EQ(output.diagnostics.last_sample_rate, 48000u);
  EXPECT_FALSE(telemetry.tryRead(output));
}

TEST(RealtimeTelemetryTest, RetainsFixedCapacityAndDropsNewestSnapshots) {
  RealtimeTelemetry telemetry(1);

  for (size_t i = 0; i < kRealtimeTelemetryCapacity; ++i) {
    ASSERT_TRUE(telemetry.beginRealtimeBlock(64, 48000));
    RealtimeTelemetrySnapshot snapshot;
    snapshot.position = TimePoint::fromSamples(static_cast<int64_t>(i * 64));
    ASSERT_TRUE(telemetry.publishFromRealtime(snapshot));
  }

  ASSERT_TRUE(telemetry.beginRealtimeBlock(64, 48000));
  RealtimeTelemetrySnapshot dropped;
  dropped.position = TimePoint::fromSamples(9999);
  EXPECT_FALSE(telemetry.publishFromRealtime(dropped));
  EXPECT_EQ(telemetry.pendingSnapshotCount(), kRealtimeTelemetryCapacity);
  EXPECT_EQ(telemetry.droppedSnapshotCount(), 1u);

  for (size_t i = 0; i < kRealtimeTelemetryCapacity; ++i) {
    RealtimeTelemetrySnapshot output;
    ASSERT_TRUE(telemetry.tryRead(output));
    EXPECT_EQ(output.sequence, i + 1);
    EXPECT_EQ(output.position.samples(), static_cast<int64_t>(i * 64));
  }

  RealtimeTelemetrySnapshot output;
  EXPECT_FALSE(telemetry.tryRead(output));
}

TEST(RealtimeTelemetryTest, ReusesDrainedSlotsWithoutReordering) {
  RealtimeTelemetry telemetry(1);

  for (size_t cycle = 0; cycle < 3; ++cycle) {
    for (size_t i = 0; i < kRealtimeTelemetryCapacity; ++i) {
      ASSERT_TRUE(telemetry.beginRealtimeBlock(32, 44100));
      RealtimeTelemetrySnapshot input;
      input.active_voice_count = static_cast<uint32_t>(cycle * kRealtimeTelemetryCapacity + i);
      ASSERT_TRUE(telemetry.publishFromRealtime(input));
    }

    for (size_t i = 0; i < kRealtimeTelemetryCapacity; ++i) {
      RealtimeTelemetrySnapshot output;
      ASSERT_TRUE(telemetry.tryRead(output));
      EXPECT_EQ(output.active_voice_count, cycle * kRealtimeTelemetryCapacity + i);
    }
  }

  EXPECT_EQ(telemetry.droppedSnapshotCount(), 0u);
}

TEST(RealtimeTelemetryTest, ClampsDecimationAndPublishesUnderrunDiagnostics) {
  RealtimeTelemetry telemetry(0);
  EXPECT_EQ(telemetry.decimationBlocks(), 1u);

  telemetry.reportUnderrunFromRealtime();
  ASSERT_TRUE(telemetry.beginRealtimeBlock(256, 96000));
  ASSERT_TRUE(telemetry.publishFromRealtime({}));

  RealtimeTelemetrySnapshot output;
  ASSERT_TRUE(telemetry.tryRead(output));
  EXPECT_EQ(output.schema_version, kRealtimeTelemetrySchemaVersion);
  EXPECT_EQ(output.diagnostics.underrun_count, 1u);
}

TEST(RealtimeTelemetryTest, SlotSideSchemaStampingAndCanonicalAvailability) {
  static_assert(std::is_invocable_r_v<
                bool, decltype(&RealtimeTelemetry::publishFromRealtime),
                RealtimeTelemetry*, const RealtimeTelemetrySnapshot&>);

  RealtimeTelemetry telemetry(1);
  ASSERT_TRUE(telemetry.beginRealtimeBlock(64, 48000));
  const RealtimeTelemetrySnapshot input{};
  ASSERT_TRUE(telemetry.publishFromRealtime(input));

  RealtimeTelemetrySnapshot output;
  ASSERT_TRUE(telemetry.tryRead(output));
  EXPECT_EQ(output.schema_version, kRealtimeTelemetrySchemaVersion);
  EXPECT_EQ(output.routing_meters.schema_version,
            kRoutingMeterTelemetrySchemaVersion);
  EXPECT_EQ(output.routing_meters.group_output_meters.schema_version,
            kGroupOutputMeterSnapshotSchemaVersion);
  EXPECT_EQ(output.routing_meters.availability, MeterAvailability::Unsupported);
  EXPECT_FALSE(telemetry.tryRead(output));
}


TEST(RealtimeTelemetryTest, ConcurrentPendingObservationNeverUnderflows) {
  std::atomic<uint64_t> read{0};
  std::atomic<uint64_t> write{1};
  bool advanced = false;
  const size_t pending = detail::observeBoundedPending(
      read, write, kRealtimeTelemetryCapacity,
      [&]() noexcept {
        if (!advanced) {
          read.store(2, std::memory_order_release);
          write.store(2, std::memory_order_release);
          advanced = true;
        }
      });
  EXPECT_EQ(pending, 2u);
  EXPECT_LE(pending, kRealtimeTelemetryCapacity);
}

TEST(RealtimeTelemetryTest, RealtimeCountersSaturate) {
  EXPECT_EQ(detail::saturatingAdd(std::numeric_limits<uint64_t>::max() - 1, 2),
            std::numeric_limits<uint64_t>::max());
  std::atomic<uint64_t> counter{std::numeric_limits<uint64_t>::max() - 1};
  detail::publishSaturatingIncrement(counter);
  detail::publishSaturatingIncrement(counter);
  EXPECT_EQ(counter.load(std::memory_order_relaxed), std::numeric_limits<uint64_t>::max());
}