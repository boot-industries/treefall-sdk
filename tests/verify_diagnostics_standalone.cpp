// SPDX-License-Identifier: MIT
#include <orpheus/audio_driver.h>
#include <orpheus/loudness_meter.h>
#include <orpheus/performance_monitor.h>

#include <cmath>

// An installed-header-only backend fixture: identical callback buffers, but
// independent measured route terms (or explicitly unavailable capture terms).
class DiagnosticRouteDriver final : public orpheus::IAudioDriver {
public:
  DiagnosticRouteDriver(uint32_t playbackFrames, bool complete)
      : m_playbackFrames(playbackFrames), m_complete(complete) {}

  orpheus::SessionGraphError initialize(const orpheus::AudioDriverConfig& config) override {
    m_config = config;
    return orpheus::SessionGraphError::OK;
  }
  orpheus::SessionGraphError start(orpheus::IAudioCallback* callback) override {
    if (callback == nullptr) {
      return orpheus::SessionGraphError::InvalidParameter;
    }
    m_running = true;
    return orpheus::SessionGraphError::OK;
  }
  orpheus::SessionGraphError stop() override {
    m_running = false;
    return orpheus::SessionGraphError::OK;
  }
  bool isRunning() const override {
    return m_running;
  }
  const orpheus::AudioDriverConfig& getConfig() const override {
    return m_config;
  }
  std::string getDriverName() const override {
    return "Diagnostic route fixture";
  }
  uint32_t getLatencySamples() const override {
    return m_complete ? 64 + m_playbackFrames + m_config.buffer_size : 0;
  }
  orpheus::ActiveAudioRoute getActiveRoute() const override {
    orpheus::ActiveAudioRoute route;
    route.actual_sample_rate = m_config.sample_rate;
    route.actual_buffer_frames = m_config.buffer_size;
    route.latency.capture_frames = m_complete ? 64 : 0;
    route.latency.playback_frames = m_playbackFrames;
    route.latency.processing_frames = m_config.buffer_size;
    route.latency.complete = m_complete;
    return route;
  }
  orpheus::AudioIoRouteState getAudioIoRouteState() const override {
    orpheus::AudioIoRouteState state;
    state.actual_sample_rate = m_config.sample_rate;
    state.actual_buffer_size = m_config.buffer_size;
    auto& latency = state.latency;
    if (m_complete) {
      latency.input_device_frames = 64;
    }
    latency.input_safety_offset_frames = 0;
    latency.input_stream_frames = 0;
    latency.input_converter_frames = 0;
    latency.output_device_frames = m_playbackFrames;
    latency.output_safety_offset_frames = 0;
    latency.output_stream_frames = 0;
    latency.output_converter_frames = 0;
    latency.callback_buffer_frames = m_config.buffer_size;
    latency.aggregate_or_audio_unit_frames = 0;
    latency.input_audio_unit_frames = 0;
    latency.output_audio_unit_frames = 0;
    latency.complete = m_complete;
    return state;
  }

private:
  orpheus::AudioDriverConfig m_config;
  uint32_t m_playbackFrames;
  bool m_complete;
  bool m_running = false;
};

int main() {
  auto monitor = orpheus::createStandalonePerformanceMonitor();
  if (monitor == nullptr) {
    return 1;
  }

  monitor->recordAudioCallback(500, 1000, 0, 48000, 512);
  const auto metrics = monitor->getMetrics();
  if (std::fabs(metrics.latencyMs - 10.666667f) > 0.00001f) {
    return 2;
  }
  monitor->recordAudioCallback(20, 1000, 0, 0, 2048);
  if (monitor->getMetrics().latencyMs != metrics.latencyMs) {
    return 3; // No sample rate: retain estimate, not a measured/unknown sentinel.
  }

  DiagnosticRouteDriver first(128, true);
  DiagnosticRouteDriver second(256, true);
  DiagnosticRouteDriver incomplete(128, false);
  const auto firstRoute = first.getActiveRoute();
  const auto secondRoute = second.getActiveRoute();
  const auto unknownRoute = incomplete.getActiveRoute();
  const auto firstTerms = first.getAudioIoRouteState().latency;
  const auto secondTerms = second.getAudioIoRouteState().latency;
  const auto unknownTerms = incomplete.getAudioIoRouteState().latency;
  auto secondMonitor = orpheus::createStandalonePerformanceMonitor();
  secondMonitor->recordAudioCallback(500, 1000, 0, secondRoute.actual_sample_rate,
                                     secondRoute.actual_buffer_frames);
  auto unknownMonitor = orpheus::createStandalonePerformanceMonitor();
  unknownMonitor->recordAudioCallback(500, 1000, 0, unknownRoute.actual_sample_rate,
                                      unknownRoute.actual_buffer_frames);
  if (firstRoute.actual_buffer_frames != 512 || secondRoute.actual_buffer_frames != 512 ||
      firstRoute.actual_sample_rate != 48000 || secondRoute.actual_sample_rate != 48000 ||
      !firstRoute.latency.complete || !secondRoute.latency.complete ||
      firstRoute.latency.playback_frames != 128 || secondRoute.latency.playback_frames != 256 ||
      first.getLatencySamples() != 704 || second.getLatencySamples() != 832 ||
      !firstTerms.complete || !secondTerms.complete || firstTerms.output_device_frames != 128 ||
      secondTerms.output_device_frames != 256 ||
      secondMonitor->getMetrics().latencyMs != metrics.latencyMs ||
      unknownMonitor->getMetrics().latencyMs != metrics.latencyMs ||
      unknownRoute.latency.complete || unknownTerms.complete ||
      unknownTerms.input_device_frames.has_value() || unknownTerms.callback_buffer_frames != 512 ||
      incomplete.getLatencySamples() != 0) {
    return 4; // Equal buffer estimates must not overwrite measured or unknown terms.
  }

  monitor->reportUnderrun();
  monitor->reportUnderrun();
  monitor->resetUnderrunCount();
  monitor->reportUnderrun();
  if (monitor->getMetrics().bufferUnderrunCount != 1) {
    return 5;
  }
  const auto baseline = monitor->getMetrics().cpuUsagePercent;
  monitor->resetPeakCpuUsage();
  monitor->resetPeakCpuUsage();
  if (monitor->getPeakCpuUsage() != 50.0f) {
    return 6; // Idle peak reset is a request, not another peak writer.
  }
  monitor->recordAudioCallback(0, 1000, 0, 48000, 512);
  if (monitor->getPeakCpuUsage() != baseline) {
    return 7;
  }
  monitor->resetPeakCpuUsage();
  monitor->recordAudioCallback(250, 1000, 0, 48000, 512);
  monitor->recordAudioCallback(0, 1000, 0, 48000, 512);
  if (monitor->getPeakCpuUsage() != 25.0f) {
    return 8; // Reset callback's instant CPU is included; consumed requests do not replay.
  }

  orpheus::LoudnessMeter meter(48000.0);
  float silence = 0.0f;
  meter.processBuffer(&silence, 1);
  (void)meter.integratedLufs();
  return 0;
}
