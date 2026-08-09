// SPDX-License-Identifier: MIT
#pragma once

#include "../../../core/common/realtime_borrowed_target.h"
#include "coreaudio_property_api.h"
#include "coreaudio_route_monitor.h"
#include "coreaudio_route_resolver.h"

#include <orpheus/audio_driver.h>

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>

#include <array>
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

namespace detail {

/// Internal test-only observation point for input-direction operations.
struct CoreAudioDriverDirectionAudit {
  enum class InputDirectionOperation : uint8_t {
    RefreshUid,
    Latency,
    StreamEnumeration,
    Capacity,
    Monitor,
    Aggregate,
    AudioUnitEnable,
    AudioUnitConfigure,
    RateTransaction,
    InputRender,
  };

  virtual ~CoreAudioDriverDirectionAudit() = default;
  virtual void beforeInputOperation(InputDirectionOperation operation) noexcept = 0;
};

} // namespace detail

/// CoreAudio HAL output driver with stable UID routing and control-thread
/// route-state monitoring.
class CoreAudioDriver : public IAudioDriver {
public:
  CoreAudioDriver();
  explicit CoreAudioDriver(std::shared_ptr<const detail::ICoreAudioRouteQuery> query);
  CoreAudioDriver(std::shared_ptr<const detail::ICoreAudioRouteQuery> query,
                  std::shared_ptr<ICoreAudioPropertyApi> property_api,
                  detail::CoreAudioDriverDirectionAudit* direction_audit = nullptr);
  CoreAudioDriver(std::shared_ptr<const detail::ICoreAudioRouteQuery> query,
                  ICoreAudioPropertyApi& property_api,
                  detail::CoreAudioDriverDirectionAudit* direction_audit = nullptr);

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
  SessionGraphError initializeAudioOutput(const AudioOutputRouteRequest& request) override;
  AudioIoRouteState getAudioIoRouteState() const override;
  AudioRouteCompatibility probeRoute(const AudioDriverConfig& config) const override;

  void setPerformanceMonitor(IPerformanceMonitor* monitor) override;

  void setInputRenderFailuresForTesting(uint64_t count) noexcept;
  void incrementInputRenderFailuresForTesting() noexcept;

private:
  static OSStatus renderCallback(void* inRefCon, AudioUnitRenderActionFlags* ioActionFlags,
                                 const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber,
                                 UInt32 inNumberFrames, AudioBufferList* ioData);

  void recordInputRenderFailure() noexcept;
  void notifyInputOperation(
      detail::CoreAudioDriverDirectionAudit::InputDirectionOperation) const noexcept;

  uint32_t getChannelCount(AudioDeviceID device_id, AudioObjectPropertyScope scope) const;
  std::optional<std::string> getDeviceUID(AudioDeviceID device_id) const;
  CFStringRef copyDeviceUID(AudioDeviceID device_id) const;
  UInt32 getClockDomain(AudioDeviceID device_id) const;
  UInt32 getUInt32Property(AudioDeviceID device_id, AudioObjectPropertySelector selector,
                           AudioObjectPropertyScope scope) const;
  std::optional<UInt32> getOptionalUInt32Property(AudioDeviceID device_id,
                                                  AudioObjectPropertySelector selector,
                                                  AudioObjectPropertyScope scope) const;
  std::optional<Float64> getOptionalFloat64Property(AudioDeviceID device_id,
                                                    AudioObjectPropertySelector selector,
                                                    AudioObjectPropertyScope scope) const;
  std::optional<UInt32> getOptionalStreamLatency(AudioDeviceID device_id,
                                                 AudioObjectPropertyScope scope) const;
  std::vector<AudioStreamID> enumerateStreams(AudioDeviceID device_id,
                                              AudioObjectPropertyScope scope) const;
  std::optional<AudioStreamBasicDescription>
  getStreamFormat(AudioStreamID stream_id, AudioObjectPropertySelector selector) const;

  AudioDeviceID createAggregateDevice(AudioDeviceID input_device_id,
                                      AudioDeviceID output_device_id);
  SessionGraphError setupAudioUnit(AudioDeviceID device_id);
  bool createRouteMonitor();
  bool startRouteMonitorLocked();
  void stopRouteMonitor();
  void routeMonitorLoop();
  void publishTerminalRouteOutcome(AudioRouteRuntimeOutcome outcome) noexcept;
  void publishRoutePollResult(CoreAudioRoutePollResult result) noexcept;
  void refreshActiveRouteLocked();

  AudioRouteLatency queryLatencyBreakdown() const;
  AudioLatencyBreakdown queryDetailedLatencyBreakdown() const;
  uint32_t queryDeviceLatency() const;
  std::optional<uint32_t> queryMaximumBufferFrames(AudioDeviceID device_id,
                                                   AudioObjectPropertyScope scope) const;
  bool allocateRenderBuffers(uint32_t maximum_frames) noexcept;

  AudioRouteRuntimeOutcome disableAutomaticHogModeLocked() noexcept;
  bool restoreAutomaticHogModeLocked() noexcept;
  void cleanupAudioUnit();
  void stopRenderingLocked();

  detail::CoreAudioRouteResolver route_resolver_;
  CoreAudioPropertyApi production_property_api_;
  ICoreAudioPropertyApi* property_api_{nullptr};
  std::shared_ptr<ICoreAudioPropertyApi> injected_property_api_;
  detail::CoreAudioDriverDirectionAudit* direction_audit_{nullptr};

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
  uint32_t render_capacity_frames_{0};
  std::atomic<AudioRouteRuntimeOutcome> route_outcome_{AudioRouteRuntimeOutcome::Healthy};
  std::atomic<AudioRouteState> unavailable_route_state_{AudioRouteState::Failed};
  std::atomic<uint32_t> render_sample_rate_{0};
  std::atomic<uint32_t> render_max_callback_frames_{0};
  std::atomic<uint32_t> render_chunk_frames_{0};

  std::unique_ptr<CoreAudioRouteMonitor> route_monitor_;
  std::atomic<bool> route_monitor_active_{false};
  std::mutex route_monitor_mutex_;
  std::thread route_monitor_thread_;

  detail::RealtimeBorrowedTarget<IAudioCallback> callback_target_;
  detail::RealtimeBorrowedTarget<IPerformanceMonitor> performance_monitor_target_;
  uint64_t expected_stream_sample_{0};
  bool stream_timeline_initialized_{false};

  std::vector<float*> input_buffers_;
  std::vector<float*> output_buffers_;
  std::vector<const float*> input_chunk_buffers_;
  std::vector<float*> output_chunk_buffers_;
  std::vector<float> input_storage_;
  std::vector<float> output_storage_;
  std::vector<uint8_t> input_abl_storage_;

  bool automatic_hog_mode_changed_{false};
  UInt32 previous_hog_mode_allowed_{0};
  ActiveAudioRoute active_route_;
  mutable std::mutex mutex_;
};

} // namespace orpheus
