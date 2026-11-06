// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#if defined(__APPLE__) && defined(ORPHEUS_ENABLE_COREAUDIO)
#include "coreaudio/coreaudio_endpoint_monitor.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace orpheus {
namespace {

class FakeCoreAudioEndpointPropertyApi final : public ICoreAudioEndpointPropertyApi {
public:
  struct Listener {
    AudioObjectPropertyAddress address;
    AudioObjectPropertyListenerProc callback;
    void* context;
  };

  OSStatus addPropertyListener(AudioObjectID, const AudioObjectPropertyAddress* address,
                               AudioObjectPropertyListenerProc callback,
                               void* context) noexcept override {
    std::lock_guard<std::mutex> lock(mutex_);
    listeners_.push_back({*address, callback, context});
    return noErr;
  }

  OSStatus removePropertyListener(AudioObjectID, const AudioObjectPropertyAddress* address,
                                  AudioObjectPropertyListenerProc callback,
                                  void* context) noexcept override {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto listener =
        std::find_if(listeners_.begin(), listeners_.end(), [&](const Listener& candidate) {
          return candidate.address.mSelector == address->mSelector &&
                 candidate.callback == callback && candidate.context == context;
        });
    if (listener == listeners_.end()) {
      return -1;
    }
    listeners_.erase(listener);
    return noErr;
  }

  void notify(AudioObjectPropertySelector selector) {
    std::vector<Listener> listeners;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      listeners = listeners_;
    }
    const AudioObjectPropertyAddress address = {selector, kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};
    for (const Listener& listener : listeners) {
      if (listener.address.mSelector == selector) {
        listener.callback(kAudioObjectSystemObject, 1, &address, listener.context);
      }
    }
  }

  size_t listenerCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return listeners_.size();
  }

private:
  mutable std::mutex mutex_;
  std::vector<Listener> listeners_;
};

TEST(CoreAudioEndpointMonitorTest, MonitorsDeviceAndDefaultEndpointChanges) {
  FakeCoreAudioEndpointPropertyApi api;
  CoreAudioEndpointMonitor monitor(api);
  std::promise<void> callback_called;
  auto callback_complete = callback_called.get_future();

  monitor.setCallback([&callback_called] { callback_called.set_value(); });
  ASSERT_EQ(api.listenerCount(), 3u);

  api.notify(kAudioHardwarePropertyDefaultOutputDevice);
  EXPECT_EQ(callback_complete.wait_for(std::chrono::seconds(1)), std::future_status::ready);

  monitor.setCallback({});
  EXPECT_EQ(api.listenerCount(), 0u);
}

TEST(CoreAudioEndpointMonitorTest, CallbackMayUnregisterItself) {
  FakeCoreAudioEndpointPropertyApi api;
  CoreAudioEndpointMonitor monitor(api);
  std::promise<void> callback_finished;
  auto completion = callback_finished.get_future();

  monitor.setCallback([&] {
    monitor.setCallback({});
    callback_finished.set_value();
  });
  api.notify(kAudioHardwarePropertyDevices);

  EXPECT_EQ(completion.wait_for(std::chrono::seconds(1)), std::future_status::ready);
  EXPECT_EQ(api.listenerCount(), 0u);
}

TEST(CoreAudioEndpointMonitorTest, CallbackMayDestroyMonitor) {
  FakeCoreAudioEndpointPropertyApi api;
  auto monitor = std::make_unique<CoreAudioEndpointMonitor>(api);
  std::promise<void> callback_finished;
  auto completion = callback_finished.get_future();

  monitor->setCallback([&] {
    monitor.reset();
    callback_finished.set_value();
  });
  api.notify(kAudioHardwarePropertyDevices);

  EXPECT_EQ(completion.wait_for(std::chrono::seconds(1)), std::future_status::ready);
  EXPECT_EQ(api.listenerCount(), 0u);
}

TEST(CoreAudioEndpointMonitorTest, StopNeverLosesTheWakeup) {
  // Each iteration races a listener wakeup against stop(). A wakeup published
  // between the worker's predicate check and its wait is lost, the worker never
  // wakes, and stop() joins it forever. Oversubscribed racers make preemption
  // inside that window likely. They run detached and own their state, so a hang
  // fails this test instead of blocking the suite.
  struct Racer {
    FakeCoreAudioEndpointPropertyApi api;
  };
  struct Shared {
    std::vector<Racer> racers;
    std::atomic<size_t> remaining{0};
    std::promise<void> finished;
  };
  const size_t racer_count = std::max(4u, 2u * std::thread::hardware_concurrency());
  auto shared = std::make_shared<Shared>();
  shared->racers = std::vector<Racer>(racer_count);
  shared->remaining.store(racer_count);
  auto completion = shared->finished.get_future();

  for (size_t index = 0; index < racer_count; ++index) {
    std::thread([shared, index] {
      FakeCoreAudioEndpointPropertyApi& api = shared->racers[index].api;
      CoreAudioEndpointMonitor monitor(api);
      for (int iteration = 0; iteration < 2000; ++iteration) {
        monitor.setCallback([] {});
        std::thread notifier([&api] { api.notify(kAudioHardwarePropertyDefaultOutputDevice); });
        monitor.setCallback({});
        notifier.join();
      }
      if (shared->remaining.fetch_sub(1) == 1) {
        shared->finished.set_value();
      }
    }).detach();
  }

  ASSERT_EQ(completion.wait_for(std::chrono::seconds(10)), std::future_status::ready)
      << "stop() did not return: a worker missed its stop wakeup";
  for (const Racer& racer : shared->racers) {
    EXPECT_EQ(racer.api.listenerCount(), 0u);
  }
}

} // namespace
} // namespace orpheus
#endif
