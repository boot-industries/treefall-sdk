// SPDX-License-Identifier: MIT
#pragma once

#include "coreaudio_property_api.h"
#include "coreaudio_route_resolver.h"

#include <orpheus/audio_driver.h>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace orpheus {

/// Bounded, listener-confirmed activation transaction for physical endpoints.
class CoreAudioSampleRateTransaction final {
public:
  CoreAudioSampleRateTransaction(ICoreAudioPropertyApi&, const detail::ResolvedCoreAudioRoute&,
                                 uint32_t requested_rate,
                                 std::chrono::milliseconds timeout = std::chrono::seconds{
                                     2}) noexcept;
  ~CoreAudioSampleRateTransaction();

  CoreAudioSampleRateTransaction(const CoreAudioSampleRateTransaction&) = delete;
  CoreAudioSampleRateTransaction& operator=(const CoreAudioSampleRateTransaction&) = delete;

  AudioRouteRuntimeOutcome begin() noexcept;
  void commit() noexcept;

private:
  struct Endpoint {
    AudioDeviceID device_id = 0;
    Float64 previous_rate = 0.0;
    bool needs_change = false;
    bool write_started = false;
    bool registered = false;
    bool confirmed = false;
  };

  static OSStatus propertyChanged(AudioObjectID, UInt32, const AudioObjectPropertyAddress*,
                                  void* context) noexcept;
  static AudioObjectPropertyAddress nominalRateAddress() noexcept;

  bool readRate(const Endpoint& endpoint, Float64& rate) noexcept;
  bool allConfirmed() const noexcept;
  void removeListeners() noexcept;
  void rollback() noexcept;
  AudioRouteRuntimeOutcome fail(AudioRouteRuntimeOutcome outcome) noexcept;

  ICoreAudioPropertyApi& property_api_;
  std::array<Endpoint, 2> endpoints_{};
  size_t endpoint_count_{0};
  const Float64 requested_rate_;
  const std::chrono::milliseconds timeout_;
  std::atomic<uint32_t> notification_bits_{0};
  std::condition_variable notification_changed_;
  std::mutex notification_mutex_;
  bool begun_{false};
  bool rollback_done_{false};
  bool committed_{false};
};

} // namespace orpheus
