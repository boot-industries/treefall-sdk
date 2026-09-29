// SPDX-License-Identifier: MIT
#pragma once

#include "coreaudio_property_api.h"

#include <CoreAudio/CoreAudio.h>

#include <atomic>
#include <cstdint>
#include <vector>

namespace orpheus {

/// Control-thread result from servicing a CoreAudio route notification.
enum class CoreAudioRoutePollResult : uint8_t {
  NoChange,
  SampleRateChanged,
  BufferSizeChanged,
  FormatChanged,
  InputUnavailable,
  OutputUnavailable,
  BackendFailure,
};

/// Physical endpoint facts observed when the route becomes active.
struct CoreAudioRouteDevice {
  AudioDeviceID device_id = 0;
  bool monitors_input = false;
  bool monitors_output = false;
  bool is_private_aggregate = false;
  /// Captured from the active endpoint during start().
  uint32_t expected_sample_rate = 0;
  /// Captured from the active endpoint during start().
  uint32_t expected_buffer_frames = 0;
};

/// One stream whose virtual and physical formats are part of the active route.
struct CoreAudioRouteStream {
  AudioStreamID stream_id = 0;
  /// Captured from the active stream during start().
  AudioStreamBasicDescription expected_virtual_format{};
  AudioStreamBasicDescription expected_physical_format{};
};

/// Watches the physical route outside the render callback.
///
/// Property listeners perform only atomic notification. The control worker
/// calls poll(), which performs passive HAL reads and never writes a property,
/// rebuilds a route, or reinitializes an AudioUnit.
class CoreAudioRouteMonitor final {
public:
  CoreAudioRouteMonitor(ICoreAudioPropertyApi& property_api, uint32_t expected_sample_rate,
                        uint32_t expected_buffer_frames, std::vector<CoreAudioRouteDevice> devices,
                        std::vector<CoreAudioRouteStream> streams);
  ~CoreAudioRouteMonitor();

  CoreAudioRouteMonitor(const CoreAudioRouteMonitor&) = delete;
  CoreAudioRouteMonitor& operator=(const CoreAudioRouteMonitor&) = delete;

  bool start() noexcept;
  void stop() noexcept;
  void requestCheck() noexcept;
  void closeAdmission() noexcept;
  void waitForChange() noexcept;
  CoreAudioRoutePollResult poll() noexcept;
  bool permitsRendering() const noexcept;
  bool isTerminal() const noexcept;

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
  static bool readUInt32(ICoreAudioPropertyApi& property_api, AudioObjectID object_id,
                         const AudioObjectPropertyAddress& address, UInt32& value) noexcept;
  static bool readFloat64(ICoreAudioPropertyApi& property_api, AudioObjectID object_id,
                          const AudioObjectPropertyAddress& address, Float64& value) noexcept;
  static bool readFormat(ICoreAudioPropertyApi& property_api, AudioObjectID object_id,
                         const AudioObjectPropertyAddress& address,
                         AudioStreamBasicDescription& value) noexcept;

  enum class State : uint8_t { Stopped, Active, Terminal };
  void signalStateChange() noexcept;

  ICoreAudioPropertyApi& property_api_;
  const Float64 expected_sample_rate_;
  const UInt32 expected_buffer_frames_;
  std::vector<CoreAudioRouteDevice> devices_;
  std::vector<CoreAudioRouteStream> streams_;
  std::vector<ListenerRegistration> registrations_;
  std::atomic<State> state_{State::Stopped};
  std::atomic<uint64_t> generation_{0};
  std::atomic<bool> pending_{false};
  size_t registered_count_{0};
};

} // namespace orpheus
