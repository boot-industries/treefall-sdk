// SPDX-License-Identifier: MIT
#pragma once

#include "../../../core/common/realtime_borrowed_target.h"
#include "coreaudio_route_monitor.h"
#include <orpheus/audio_driver.h>

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace orpheus {

class IPerformanceMonitor;

/// CoreAudio HAL output driver with stable UID routing and control-thread
/// route-state monitoring.
class CoreAudioDriver : public IAudioDriver {
public:
  CoreAudioDriver();
  ~CoreAudioDriver() override;

  SessionGraphError initialize(const AudioDriverConfig& config) override;
  SessionGraphError start(IAudioCallback* callback) override;
  SessionGraphError stop() override;
  bool isRunning() const override;
  const AudioDriverConfig& getConfig() const override;
  std::string getDriverName() const override;
  uint32_t getLatencySamples() const override;
  AudioIoTelemetry getTelemetry() const noexcept override;
  AudioDriverCapabilities getCapabilities() const override;
  ActiveAudioRoute getActiveRoute() const override;

  void setPerformanceMonitor(IPerformanceMonitor* monitor) override;

  // Test-only helpers for deterministic backend failure accounting.
  void setInputRenderFailuresForTesting(uint64_t count) noexcept;
  void incrementInputRenderFailuresForTesting() noexcept;

private:
  static OSStatus renderCallback(void* inRefCon, AudioUnitRenderActionFlags* ioActionFlags,
                                 const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber,
                                 UInt32 inNumberFrames, AudioBufferList* ioData);

  void recordInputRenderFailure() noexcept;

  std::vector<AudioDeviceID> enumerateDevices();
  AudioDeviceID findDeviceByUID(const std::string& device_uid);
  AudioDeviceID getDefaultDevice(AudioObjectPropertySelector selector);
  AudioDeviceID resolveInputOutputDevice();
  AudioDeviceID createAggregateDevice(AudioDeviceID input_device_id,
                                      AudioDeviceID output_device_id);
  bool supportsDirection(AudioDeviceID device_id, AudioObjectPropertyScope scope) const;
  uint32_t getChannelCount(AudioDeviceID device_id, AudioObjectPropertyScope scope) const;
  std::optional<std::string> getDeviceUID(AudioDeviceID device_id) const;

  bool resolveChannelMaps(const AudioDriverConfig& config);
  SessionGraphError setupAudioUnit(AudioDeviceID device_id);
  std::vector<AudioStreamID> enumerateStreams(AudioDeviceID device_id,
                                              AudioObjectPropertyScope scope) const;
  std::optional<AudioStreamBasicDescription>
  getStreamFormat(AudioStreamID stream_id, AudioObjectPropertySelector selector) const;
  bool createRouteMonitor();
  bool startRouteMonitorLocked();
  void stopRouteMonitor();
  void routeMonitorLoop();
  void publishRoutePollResult(CoreAudioRoutePollResult result);
  void refreshActiveRouteLocked();

  AudioRouteLatency queryLatencyBreakdown() const;
  uint32_t queryDeviceLatency() const;
  void cleanupAudioUnit();
  void stopRenderingLocked();

  AudioDriverConfig config_;
  AudioUnit audio_unit_{nullptr};
  AudioDeviceID device_id_{0};
  AudioDeviceID aggregate_device_id_{0};
  AudioDeviceID input_device_id_{0};
  AudioDeviceID output_device_id_{0};
  uint32_t input_channel_offset_{0};
  uint32_t available_input_channels_{0};
  uint32_t available_output_channels_{0};
  std::vector<uint16_t> input_channel_map_;
  std::vector<uint16_t> output_channel_map_;

  std::atomic<bool> is_running_{false};
  std::atomic<uint64_t> input_render_failures_{0};
  std::atomic<AudioDriverRuntimeOutcome> runtime_outcome_{AudioDriverRuntimeOutcome::Healthy};
  std::atomic<AudioRouteRuntimeOutcome> route_outcome_{AudioRouteRuntimeOutcome::Healthy};

  CoreAudioSampleRatePropertyApi route_property_api_;
  std::unique_ptr<CoreAudioRouteMonitor> route_monitor_;
  std::atomic<bool> route_monitor_active_{false};
  std::mutex route_monitor_mutex_;
  std::thread route_monitor_thread_;

  detail::RealtimeBorrowedTarget<IAudioCallback> callback_target_;
  detail::RealtimeBorrowedTarget<IPerformanceMonitor> performance_monitor_target_;
  int64_t expected_stream_sample_{0};
  bool stream_timeline_initialized_{false};

  std::vector<float*> input_buffers_;
  std::vector<float*> output_buffers_;
  std::vector<float> input_storage_;
  std::vector<float> output_storage_;
  std::vector<uint8_t> input_abl_storage_;

  ActiveAudioRoute active_route_;
  mutable std::mutex mutex_;
};

} // namespace orpheus
