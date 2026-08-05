// SPDX-License-Identifier: MIT
#include "coreaudio_endpoint_monitor.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>

namespace orpheus {
namespace {

class NativeCoreAudioEndpointPropertyApi final : public ICoreAudioEndpointPropertyApi {
public:
  OSStatus addPropertyListener(AudioObjectID object_id, const AudioObjectPropertyAddress* address,
                               AudioObjectPropertyListenerProc callback,
                               void* context) noexcept override {
    return AudioObjectAddPropertyListener(object_id, address, callback, context);
  }

  OSStatus removePropertyListener(AudioObjectID object_id,
                                  const AudioObjectPropertyAddress* address,
                                  AudioObjectPropertyListenerProc callback,
                                  void* context) noexcept override {
    return AudioObjectRemovePropertyListener(object_id, address, callback, context);
  }
};

const std::array<AudioObjectPropertyAddress, 3>& endpointPropertyAddresses() {
  static const std::array<AudioObjectPropertyAddress, 3> addresses = {
      AudioObjectPropertyAddress{kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal,
                                 kAudioObjectPropertyElementMain},
      AudioObjectPropertyAddress{kAudioHardwarePropertyDefaultInputDevice,
                                 kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain},
      AudioObjectPropertyAddress{kAudioHardwarePropertyDefaultOutputDevice,
                                 kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain},
  };
  return addresses;
}

} // namespace

struct CoreAudioEndpointMonitor::State {
  std::atomic<bool> active{false};
  std::atomic<bool> pending{false};
  std::condition_variable changed;
  std::mutex changed_mutex;
  std::mutex callback_mutex;
  std::function<void()> callback;
};

CoreAudioEndpointMonitor::CoreAudioEndpointMonitor(ICoreAudioEndpointPropertyApi& property_api)
    : property_api_(property_api), state_(std::make_shared<State>()) {}

CoreAudioEndpointMonitor::~CoreAudioEndpointMonitor() {
  stop();
}

void CoreAudioEndpointMonitor::setCallback(std::function<void()> callback) {
  bool should_start = false;
  {
    std::lock_guard<std::mutex> lock(state_->callback_mutex);
    state_->callback = std::move(callback);
    should_start = static_cast<bool>(state_->callback);
  }

  if (!should_start) {
    stop();
    return;
  }

  std::lock_guard<std::mutex> lifecycle_lock(lifecycle_mutex_);
  if (state_->active.exchange(true, std::memory_order_acq_rel)) {
    return;
  }

  for (const AudioObjectPropertyAddress& address : endpointPropertyAddresses()) {
    if (property_api_.addPropertyListener(kAudioObjectSystemObject, &address, &propertyChanged,
                                          state_.get()) != noErr) {
      for (size_t index = 0; index < listener_count_; ++index) {
        const AudioObjectPropertyAddress& registered = endpointPropertyAddresses()[index];
        property_api_.removePropertyListener(kAudioObjectSystemObject, &registered,
                                             &propertyChanged, state_.get());
      }
      listener_count_ = 0;
      state_->active.store(false, std::memory_order_release);
      return;
    }
    ++listener_count_;
  }

  try {
    worker_ = std::thread(&CoreAudioEndpointMonitor::monitorLoop, state_);
  } catch (...) {
    for (size_t index = 0; index < listener_count_; ++index) {
      const AudioObjectPropertyAddress& registered = endpointPropertyAddresses()[index];
      property_api_.removePropertyListener(kAudioObjectSystemObject, &registered, &propertyChanged,
                                           state_.get());
    }
    listener_count_ = 0;
    state_->active.store(false, std::memory_order_release);
  }
}

void CoreAudioEndpointMonitor::stop() noexcept {
  std::lock_guard<std::mutex> lifecycle_lock(lifecycle_mutex_);
  if (!state_->active.exchange(false, std::memory_order_acq_rel) && !worker_.joinable()) {
    return;
  }

  for (size_t index = 0; index < listener_count_; ++index) {
    const AudioObjectPropertyAddress& address = endpointPropertyAddresses()[index];
    property_api_.removePropertyListener(kAudioObjectSystemObject, &address, &propertyChanged,
                                         state_.get());
  }
  listener_count_ = 0;
  state_->pending.store(true, std::memory_order_release);
  state_->changed.notify_one();

  if (worker_.joinable()) {
    if (worker_.get_id() == std::this_thread::get_id()) {
      worker_.detach();
    } else {
      worker_.join();
    }
  }
}

OSStatus CoreAudioEndpointMonitor::propertyChanged(AudioObjectID object_id, UInt32 address_count,
                                                   const AudioObjectPropertyAddress* addresses,
                                                   void* context) noexcept {
  (void)object_id;
  (void)address_count;
  (void)addresses;
  auto* state = static_cast<State*>(context);
  if (state == nullptr || !state->active.load(std::memory_order_acquire)) {
    return noErr;
  }
  state->pending.store(true, std::memory_order_release);
  state->changed.notify_one();
  return noErr;
}

void CoreAudioEndpointMonitor::monitorLoop(std::shared_ptr<State> state) {
  while (state->active.load(std::memory_order_acquire)) {
    std::unique_lock<std::mutex> lock(state->changed_mutex);
    state->changed.wait(lock, [&state] {
      return state->pending.load(std::memory_order_acquire) ||
             !state->active.load(std::memory_order_acquire);
    });
    state->pending.store(false, std::memory_order_release);
    if (!state->active.load(std::memory_order_acquire)) {
      return;
    }

    std::function<void()> callback;
    {
      std::lock_guard<std::mutex> callback_lock(state->callback_mutex);
      callback = state->callback;
    }
    lock.unlock();
    if (callback) {
      callback();
    }
  }
}

std::unique_ptr<CoreAudioEndpointMonitor> createCoreAudioEndpointMonitor() {
  static NativeCoreAudioEndpointPropertyApi property_api;
  return std::make_unique<CoreAudioEndpointMonitor>(property_api);
}

} // namespace orpheus
