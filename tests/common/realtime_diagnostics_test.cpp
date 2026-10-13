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

TEST(RealtimeDiagnosticsTest, LegacyBudgetCountersUseStrictQuarterAndHalfThresholds) {
  RealtimeDiagnostics diagnostics;
  // Four frames at 1 kHz: exactly 4 ms, quarter = 1 ms, half = 2 ms.
  diagnostics.recordCallback(4, 1000, 1'000'000);
  auto snapshot = diagnostics.snapshot();
  EXPECT_EQ(snapshot.callback_over_budget_count, 0u);
  EXPECT_EQ(snapshot.callback_p99_over_budget_count, 0u);

  diagnostics.recordCallback(4, 1000, 1'000'001);
  snapshot = diagnostics.snapshot();
  EXPECT_EQ(snapshot.callback_over_budget_count, 1u);
  EXPECT_EQ(snapshot.callback_p99_over_budget_count, 0u);

  diagnostics.recordCallback(4, 1000, 2'000'000);
  snapshot = diagnostics.snapshot();
  EXPECT_EQ(snapshot.callback_over_budget_count, 2u);
  EXPECT_EQ(snapshot.callback_p99_over_budget_count, 0u);

  diagnostics.recordCallback(4, 1000, 2'000'001);
  snapshot = diagnostics.snapshot();
  EXPECT_EQ(snapshot.callback_over_budget_count, 3u);
  // One half-budget exceedance, not a percentile calculation over four samples.
  EXPECT_EQ(snapshot.callback_p99_over_budget_count, 1u);
  EXPECT_EQ(snapshot.callback_count, 4u);
}

TEST(RealtimeDiagnosticsTest, UnobservedDurationAndUnknownRateDoNotEvaluateBudgets) {
  RealtimeDiagnostics diagnostics;
  diagnostics.recordCallback(4, 1000, 2'000'001);
  diagnostics.recordCallback(4, 1000, 0);
  diagnostics.recordCallback(4, 0, 100'000'000);
  const auto snapshot = diagnostics.snapshot();
  EXPECT_EQ(snapshot.callback_count, 3u);
  EXPECT_EQ(snapshot.samples_processed, 12u);
  EXPECT_EQ(snapshot.last_sample_rate, 0u);
  EXPECT_EQ(snapshot.callback_over_budget_count, 1u);
  EXPECT_EQ(snapshot.callback_p99_over_budget_count, 1u);
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
