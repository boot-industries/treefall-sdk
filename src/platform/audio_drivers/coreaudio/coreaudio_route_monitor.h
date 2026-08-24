// SPDX-License-Identifier: MIT
#pragma once

#include "coreaudio_sample_rate_monitor.h"

#include <CoreAudio/CoreAudio.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <vector>

namespace orpheus {

/// Control-thread result from servicing a CoreAudio route notification.
enum class CoreAudioRoutePollResult : uint8_t {
  NoChange,
  RateRestored,
  RouteUnavailable,
  InputUnavailable,
  OutputUnavailable,
  FormatChanged,
  ReinitializationRequired,
  BackendFailure,
};

/// A physical device contributing one or both directions of an active route.
struct CoreAudioRouteDevice {
  AudioDeviceID device_id = 0;
  bool monitors_input = false;
  bool monitors_output = false;
};

/// One stream whose virtual and physical formats are part of the active route.
struct CoreAudioRouteStream {
  AudioStreamID stream_id = 0;
  AudioStreamBasicDescription expected_virtual_format{};
  AudioStreamBasicDescription expected_physical_format{};
};

/// Watches the physical route outside the render callback.
///
/// Property listeners perform only an atomic notification. The control worker
/// calls poll(), which performs all HAL reads and any safe nominal-rate restore.
/// A pending notification closes the render gate until the worker acknowledges
/// it or reaches a terminal route outcome.
class CoreAudioRouteMonitor final {
public:
  CoreAudioRouteMonitor(ICoreAudioSampleRatePropertyApi& property_api,
                        uint32_t expected_sample_rate, uint32_t expected_buffer_frames,
                        std::vector<CoreAudioRouteDevice> devices,
                        std::vector<CoreAudioRouteStream> streams);
  ~CoreAudioRouteMonitor();

  CoreAudioRouteMonitor(const CoreAudioRouteMonitor&) = delete;
  CoreAudioRouteMonitor& operator=(const CoreAudioRouteMonitor&) = delete;

  /// Register alive/rate/buffer and stream-format listeners.
  bool start() noexcept;
  /// Remove every listener and close the render gate. Safe to repeat.
  void stop() noexcept;

  /// Close the render gate and request a control-thread verification.
  void requestCheck() noexcept;
  /// Wait for a property notification or requestCheck().
  void waitForChange() noexcept;
  /// Read and service route properties on the control thread.
  CoreAudioRoutePollResult poll() noexcept;

  /// Whether the render callback may pass audio to the host.
  bool permitsRendering() const noexcept;

private:
  struct ListenerRegistration {
    AudioObjectID object_id = 0;
    AudioObjectPropertyAddress address{};
  };

  static OSStatus propertyChanged(AudioObjectID, UInt32, const AudioObjectPropertyAddress*,
                                  void* context) noexcept;
  static AudioObjectPropertyAddress deviceAddress(AudioObjectPropertySelector selector) noexcept;
  static AudioObjectPropertyAddress streamAddress(AudioObjectPropertySelector selector) noexcept;
  static bool streamLayoutsEqual(const AudioStreamBasicDescription& lhs,
                                 const AudioStreamBasicDescription& rhs) noexcept;
  static bool readUInt32(ICoreAudioSampleRatePropertyApi& property_api, AudioObjectID object_id,
                         const AudioObjectPropertyAddress& address, UInt32& value) noexcept;
  static bool readFloat64(ICoreAudioSampleRatePropertyApi& property_api, AudioObjectID object_id,
                          const AudioObjectPropertyAddress& address, Float64& value) noexcept;
  static bool readFormat(ICoreAudioSampleRatePropertyApi& property_api, AudioObjectID object_id,
                         const AudioObjectPropertyAddress& address,
                         AudioStreamBasicDescription& value) noexcept;

  static constexpr uint64_t kPendingBit = 1;
  static constexpr uint64_t kStoppedBit = 2;
  static constexpr uint64_t kGenerationIncrement = 4;

  ICoreAudioSampleRatePropertyApi& property_api_;
  const Float64 expected_sample_rate_;
  const UInt32 expected_buffer_frames_;
  std::vector<CoreAudioRouteDevice> devices_;
  std::vector<CoreAudioRouteStream> streams_;
  std::vector<ListenerRegistration> registrations_;
  std::atomic<uint64_t> state_{kStoppedBit};
  std::condition_variable pending_changed_;
  std::mutex pending_mutex_;
  size_t registered_count_{0};
};

} // namespace orpheus
