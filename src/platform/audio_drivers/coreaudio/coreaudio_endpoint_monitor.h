// SPDX-License-Identifier: MIT
#pragma once

#include <CoreAudio/CoreAudio.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace orpheus {

/// Minimal CoreAudio property-listener surface used by endpoint discovery.
class ICoreAudioEndpointPropertyApi {
public:
  virtual ~ICoreAudioEndpointPropertyApi() = default;

  virtual OSStatus addPropertyListener(AudioObjectID object_id,
                                       const AudioObjectPropertyAddress* address,
                                       AudioObjectPropertyListenerProc callback,
                                       void* context) noexcept = 0;
  virtual OSStatus removePropertyListener(AudioObjectID object_id,
                                          const AudioObjectPropertyAddress* address,
                                          AudioObjectPropertyListenerProc callback,
                                          void* context) noexcept = 0;
};

/// Coalesces CoreAudio endpoint-catalog changes onto one control worker.
///
/// The callback runs off the CoreAudio listener thread. It may unregister
/// itself, replace itself, or destroy this monitor; its worker state remains
/// alive until the in-flight callback returns. Starting and stopping are
/// serialized, and stop() returns only after the retired worker has exited,
/// except when called from that worker's own callback.
class CoreAudioEndpointMonitor final {
public:
  explicit CoreAudioEndpointMonitor(ICoreAudioEndpointPropertyApi& property_api);
  ~CoreAudioEndpointMonitor();

  CoreAudioEndpointMonitor(const CoreAudioEndpointMonitor&) = delete;
  CoreAudioEndpointMonitor& operator=(const CoreAudioEndpointMonitor&) = delete;

  /// Replaces the notification callback. An empty callback stops monitoring.
  void setCallback(std::function<void()> callback);
  /// Stops monitoring. Safe from the notification callback and safe to repeat.
  void stop() noexcept;

private:
  struct State;

  static OSStatus propertyChanged(AudioObjectID, UInt32, const AudioObjectPropertyAddress*,
                                  void* context) noexcept;
  static void monitorLoop(std::shared_ptr<State> state, std::uint64_t generation);
  void awaitRetiredWorkers(std::unique_lock<std::mutex>& lifecycle_lock);

  ICoreAudioEndpointPropertyApi& property_api_;
  std::shared_ptr<State> state_;
  std::thread worker_;
  size_t listener_count_{0};
};

/// Builds an endpoint monitor backed by CoreAudio's native property API.
std::unique_ptr<CoreAudioEndpointMonitor> createCoreAudioEndpointMonitor();

} // namespace orpheus
