// SPDX-License-Identifier: MIT
#include <orpheus/loudness_meter.h>
#include <orpheus/performance_monitor.h>

int main() {
  auto monitor = orpheus::createStandalonePerformanceMonitor();
  if (monitor == nullptr) {
    return 1;
  }

  monitor->recordAudioCallback(500, 1000, 0, 48000, 512);
  const auto metrics = monitor->getMetrics();
  (void)metrics.cpuUsagePercent;

  orpheus::LoudnessMeter meter(48000.0);
  float silence = 0.0f;
  meter.processBuffer(&silence, 1);
  (void)meter.integratedLufs();
  return 0;
}
