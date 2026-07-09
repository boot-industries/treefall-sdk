// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>
#include <orpheus/realtime_diagnostics.h>

using namespace orpheus;

TEST(RealtimeDiagnosticsTest, RecordsCallbackCountersWithoutTiming) {
  RealtimeDiagnostics diagnostics;

  diagnostics.recordCallback(128, 48000);
  diagnostics.recordCallback(256, 48000);

  const auto snap = diagnostics.snapshot();
  EXPECT_EQ(snap.callback_count, 2u);
  EXPECT_EQ(snap.samples_processed, 384u);
  EXPECT_EQ(snap.last_buffer_frames, 256u);
  EXPECT_EQ(snap.last_sample_rate, 48000u);
  EXPECT_EQ(snap.callback_over_budget_count, 0u);
  EXPECT_EQ(snap.callback_p99_over_budget_count, 0u);
}

TEST(RealtimeDiagnosticsTest, TracksBudgetAndUnderrunCounters) {
  RealtimeDiagnostics diagnostics;

  // 128 frames at 48 kHz is about 2.666 ms; 2 ms exceeds the 50% budget.
  diagnostics.recordCallback(128, 48000, 2'000'000u);
  diagnostics.reportUnderrun();

  const auto snap = diagnostics.snapshot();
  EXPECT_EQ(snap.callback_count, 1u);
  EXPECT_EQ(snap.callback_over_budget_count, 1u);
  EXPECT_EQ(snap.callback_p99_over_budget_count, 1u);
  EXPECT_EQ(snap.underrun_count, 1u);
}

TEST(RealtimeDiagnosticsTest, ResetClearsCounters) {
  RealtimeDiagnostics diagnostics;
  diagnostics.recordCallback(128, 48000, 2'000'000u);
  diagnostics.reportUnderrun();

  diagnostics.reset();

  const auto snap = diagnostics.snapshot();
  EXPECT_EQ(snap.callback_count, 0u);
  EXPECT_EQ(snap.samples_processed, 0u);
  EXPECT_EQ(snap.underrun_count, 0u);
  EXPECT_EQ(snap.callback_over_budget_count, 0u);
  EXPECT_EQ(snap.callback_p99_over_budget_count, 0u);
}
