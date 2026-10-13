// SPDX-License-Identifier: MIT
#include <treefall/performance_monitor.h>
#include <treefall/realtime_diagnostics.h>

#include <cmath>
#include <type_traits>

int main() {
  static_assert(std::is_trivially_copyable_v<treefall::RealtimeDiagnosticsSnapshot>);
  treefall::RealtimeDiagnostics diagnostics;
  diagnostics.recordCallback(64, 48000);
  const auto snapshot = diagnostics.snapshot();
  if (snapshot.callback_count != 1 || snapshot.last_buffer_frames != 64 ||
      snapshot.last_sample_rate != 48000 || snapshot.callback_over_budget_count != 0 ||
      snapshot.callback_p99_over_budget_count != 0) {
    return 1;
  }
  // Omitted timing is unobserved, not proof that the callback met its budget.
  // The legacy "p99" field counts half-budget exceedances, never a percentile.
  diagnostics.recordCallback(64, 48000, 333334);
  diagnostics.recordCallback(64, 48000, 666667);
  const auto timed = diagnostics.snapshot();
  if (timed.callback_over_budget_count != 2 || timed.callback_p99_over_budget_count != 1) {
    return 2;
  }

  auto monitor = treefall::createStandalonePerformanceMonitor();
  monitor->recordAudioCallback(1000, 1000, 2, 48000, 512);
  if (std::fabs(monitor->getMetrics().latencyMs - 10.666667f) > 0.00001f) {
    return 3;
  }
  monitor->resetPeakCpuUsage();
  if (monitor->getPeakCpuUsage() != 100.0f) {
    return 4;
  }
  monitor->recordAudioCallback(0, 1000, 2, 0, 2048);
  if (monitor->getPeakCpuUsage() != 10.0f ||
      std::fabs(monitor->getMetrics().latencyMs - 10.666667f) > 0.00001f) {
    return 5;
  }
  return 0;
}
