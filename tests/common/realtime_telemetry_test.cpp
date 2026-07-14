// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>
#include <orpheus/realtime_telemetry.h>

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
