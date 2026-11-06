// SPDX-License-Identifier: MIT
#include "coreaudio_endpoint_monitor.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
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
  // A worker runs only while this matches the generation it was started with.
  // Every start and stop advances it, so a retired worker exits even if the
  // callback it was running restarted monitoring.
  std::atomic<std::uint64_t> generation{0};
  std::condition_variable changed;
  std::mutex changed_mutex;
  std::mutex callback_mutex;
  std::function<void()> callback;
  // Serializes start and stop. A stop joins its worker after releasing this
  // mutex, because the callback on that worker may itself start or stop
  // monitoring. `retiring` counts such joins; other threads wait on `retired`
  // for them, so stop() still returns only after the worker it retired exits.
  std::mutex lifecycle_mutex;
  std::condition_variable retired;
  size_t retiring{0};
};

namespace {

// Set on each monitor worker thread. A lifecycle call from a worker's own
// callback must not wait for retiring workers: it may be one of them.
thread_local const void* tls_worker_state = nullptr;

} // namespace

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

  std::unique_lock<std::mutex> lifecycle_lock(state_->lifecycle_mutex);
  awaitRetiredWorkers(lifecycle_lock);
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

  const std::uint64_t generation = state_->generation.fetch_add(1, std::memory_order_acq_rel) + 1;
  try {
    worker_ = std::thread(&CoreAudioEndpointMonitor::monitorLoop, state_, generation);
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
  // The local reference keeps the lifecycle mutex alive after the join even if
  // the joined callback destroyed this monitor.
  const std::shared_ptr<State> state = state_;
  std::unique_lock<std::mutex> lifecycle_lock(state->lifecycle_mutex);
  awaitRetiredWorkers(lifecycle_lock);
  if (!state->active.exchange(false, std::memory_order_acq_rel) && !worker_.joinable()) {
    return;
  }

  for (size_t index = 0; index < listener_count_; ++index) {
    const AudioObjectPropertyAddress& address = endpointPropertyAddresses()[index];
    property_api_.removePropertyListener(kAudioObjectSystemObject, &address, &propertyChanged,
                                         state.get());
  }
  listener_count_ = 0;
  state->generation.fetch_add(1, std::memory_order_acq_rel);
  // Passing through the wait mutex orders this wakeup after any predicate check
  // a worker made, so the notify cannot fall between its check and its wait.
  state->changed_mutex.lock();
  state->changed_mutex.unlock();
  state->changed.notify_all();

  std::thread worker = std::move(worker_);
  if (!worker.joinable()) {
    return;
  }
  if (worker.get_id() == std::this_thread::get_id()) {
    worker.detach();
    return;
  }

  // Join outside the lifecycle mutex: the callback in flight on this worker may
  // start or stop monitoring, which needs that mutex.
  ++state->retiring;
  lifecycle_lock.unlock();
  worker.join();
  lifecycle_lock.lock();
  --state->retiring;
  lifecycle_lock.unlock();
  state->retired.notify_all();
}

void CoreAudioEndpointMonitor::awaitRetiredWorkers(std::unique_lock<std::mutex>& lifecycle_lock) {
  if (tls_worker_state == state_.get()) {
    return;
  }
  state_->retired.wait(lifecycle_lock, [this] { return state_->retiring == 0; });
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
  state->changed_mutex.lock();
  state->changed_mutex.unlock();
  state->changed.notify_one();
  return noErr;
}

void CoreAudioEndpointMonitor::monitorLoop(std::shared_ptr<State> state, std::uint64_t generation) {
  tls_worker_state = state.get();
  const auto current = [&state, generation] {
    return state->generation.load(std::memory_order_acquire) == generation;
  };
  while (true) {
    std::unique_lock<std::mutex> lock(state->changed_mutex);
    state->changed.wait(lock, [&state, &current] {
      return state->pending.load(std::memory_order_acquire) || !current();
    });
    if (!current()) {
      return;
    }
    state->pending.store(false, std::memory_order_release);

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
