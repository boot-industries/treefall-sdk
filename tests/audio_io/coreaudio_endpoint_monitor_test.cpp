// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#if defined(__APPLE__) && defined(ORPHEUS_ENABLE_COREAUDIO)
#include "coreaudio/coreaudio_endpoint_monitor.h"

#include <orpheus/audio_driver.h>
#include <orpheus/audio_driver_manager.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace orpheus {
namespace detail {
std::unique_ptr<IAudioDriverManager> createAudioDriverManagerForTesting(
    std::function<std::unique_ptr<IAudioDriver>(const std::string&)> factory,
    std::function<std::unique_ptr<CoreAudioEndpointMonitor>()> endpoint_monitor_factory);
} // namespace detail

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
    if (on_remove_) {
      on_remove_();
    }
    return noErr;
  }

  /// Runs inside every successful removal. A monitor removes its listeners
  /// while it serializes stop(), before it retires the worker.
  void setOnRemove(std::function<void()> on_remove) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_remove_ = std::move(on_remove);
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
  std::function<void()> on_remove_;
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

enum class ReentrantCall { Clear, Replace };

class CoreAudioEndpointMonitorReentrantStopTest : public ::testing::TestWithParam<ReentrantCall> {};

TEST_P(CoreAudioEndpointMonitorReentrantStopTest, CallbackLifecycleCallDuringExternalStopReturns) {
  // An external stop() retires the worker while its callback is in flight, and
  // the callback then clears or replaces itself. If stop() held its lifecycle
  // lock across the join, that call would wait for stop() while stop() waits
  // for the callback. The scenario runs detached and owns its state, so a hang
  // fails this test instead of blocking the suite.
  struct Scenario {
    FakeCoreAudioEndpointPropertyApi api;
    std::unique_ptr<CoreAudioEndpointMonitor> monitor;
    std::promise<void> callback_entered;
    std::promise<void> stop_removed_listeners;
    std::atomic<bool> callback_ran{false};
    std::atomic<bool> removal_seen{false};
    std::atomic<int> replacement_calls{0};
    size_t listeners_after_stop{0};
    size_t listeners_after_cleanup{0};
    std::promise<void> finished;
  };
  const ReentrantCall call = GetParam();
  auto scenario = std::make_shared<Scenario>();
  auto completion = scenario->finished.get_future();

  std::thread([scenario, call] {
    Scenario& s = *scenario;
    s.monitor = std::make_unique<CoreAudioEndpointMonitor>(s.api);
    std::shared_future<void> stop_in_progress = s.stop_removed_listeners.get_future().share();
    s.monitor->setCallback([&s, call, stop_in_progress] {
      if (s.callback_ran.exchange(true)) {
        return;
      }
      s.callback_entered.set_value();
      stop_in_progress.wait();
      if (call == ReentrantCall::Clear) {
        s.monitor->setCallback({});
      } else {
        s.monitor->setCallback([&s] { s.replacement_calls.fetch_add(1); });
      }
    });
    s.api.setOnRemove([&s] {
      if (!s.removal_seen.exchange(true)) {
        s.stop_removed_listeners.set_value();
      }
    });
    s.api.notify(kAudioHardwarePropertyDevices);
    s.callback_entered.get_future().wait();

    s.monitor->stop();
    s.listeners_after_stop = s.api.listenerCount();
    s.monitor->setCallback({});
    s.listeners_after_cleanup = s.api.listenerCount();
    s.finished.set_value();
  }).detach();

  ASSERT_EQ(completion.wait_for(std::chrono::seconds(5)), std::future_status::ready)
      << "stop() deadlocked against a lifecycle call from the callback it was joining";
  EXPECT_EQ(scenario->listeners_after_stop, call == ReentrantCall::Replace ? 3u : 0u)
      << "the callback's own lifecycle call must take effect, not be lost";
  EXPECT_EQ(scenario->listeners_after_cleanup, 0u);
}

TEST(CoreAudioEndpointMonitorTest, CallbackRestartRetiresTheOldWorker) {
  // A callback that stops and restarts monitoring leaves its own worker
  // detached. That retired worker must exit once the callback returns instead
  // of serving notifications beside the new worker. A thread-local probe on the
  // old worker reports when that thread exits.
  struct ExitProbe {
    std::shared_ptr<std::promise<void>> exited;
    ~ExitProbe() {
      if (exited) {
        exited->set_value();
      }
    }
  };
  FakeCoreAudioEndpointPropertyApi api;
  CoreAudioEndpointMonitor monitor(api);
  auto old_worker_exited = std::make_shared<std::promise<void>>();
  auto old_worker_exit = old_worker_exited->get_future();
  std::atomic<int> replacement_calls{0};
  std::promise<void> restarted;
  auto restart_complete = restarted.get_future();

  monitor.setCallback([&] {
    thread_local ExitProbe probe;
    probe.exited = old_worker_exited;
    monitor.setCallback({});
    monitor.setCallback([&replacement_calls] { replacement_calls.fetch_add(1); });
    restarted.set_value();
  });
  api.notify(kAudioHardwarePropertyDevices);
  ASSERT_EQ(restart_complete.wait_for(std::chrono::seconds(1)), std::future_status::ready);
  EXPECT_EQ(api.listenerCount(), 3u);
  EXPECT_EQ(old_worker_exit.wait_for(std::chrono::seconds(2)), std::future_status::ready)
      << "the worker retired by its own callback kept running after a restart";

  api.notify(kAudioHardwarePropertyDefaultOutputDevice);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (replacement_calls.load() == 0 && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_GE(replacement_calls.load(), 1);
  monitor.setCallback({});
  EXPECT_EQ(api.listenerCount(), 0u);
}

INSTANTIATE_TEST_SUITE_P(LifecycleCalls, CoreAudioEndpointMonitorReentrantStopTest,
                         ::testing::Values(ReentrantCall::Clear, ReentrantCall::Replace),
                         [](const ::testing::TestParamInfo<ReentrantCall>& info) {
                           return info.param == ReentrantCall::Clear ? "Clear" : "Replace";
                         });

TEST(AudioDriverManagerEndpointMonitorTest, CallbackGetterWhileAnotherThreadClearsDoesNotDeadlock) {
  // Clearing the endpoint callback joins the monitor worker. A callback in
  // flight on that worker calls a manager getter that takes the manager state
  // mutex, so the clear must not hold that mutex across the join. The scenario
  // runs detached and owns its state, so a hang fails this test instead of
  // blocking the suite.
  struct Scenario {
    FakeCoreAudioEndpointPropertyApi api;
    std::unique_ptr<IAudioDriverManager> manager;
    std::promise<void> callback_entered;
    std::promise<void> clear_removed_listeners;
    std::atomic<bool> callback_ran{false};
    std::atomic<bool> removal_seen{false};
    std::atomic<bool> getters_returned{false};
    size_t listeners_after_clear{0};
    std::promise<void> finished;
  };
  auto scenario = std::make_shared<Scenario>();
  auto completion = scenario->finished.get_future();

  std::thread([scenario] {
    Scenario& s = *scenario;
    s.manager = detail::createAudioDriverManagerForTesting(
        [](const std::string&) -> std::unique_ptr<IAudioDriver> { return nullptr; },
        [&s] { return std::make_unique<CoreAudioEndpointMonitor>(s.api); });
    std::shared_future<void> clear_in_progress = s.clear_removed_listeners.get_future().share();
    s.manager->setEndpointChangeCallback([&s, clear_in_progress] {
      if (s.callback_ran.exchange(true)) {
        return;
      }
      s.callback_entered.set_value();
      clear_in_progress.wait();
      const bool idle =
          !s.manager->getCurrentDevice().has_value() && s.manager->getActiveDriver() == nullptr;
      s.getters_returned.store(idle);
    });
    s.api.setOnRemove([&s] {
      if (!s.removal_seen.exchange(true)) {
        s.clear_removed_listeners.set_value();
      }
    });
    s.api.notify(kAudioHardwarePropertyDefaultOutputDevice);
    s.callback_entered.get_future().wait();

    s.manager->setEndpointChangeCallback({});
    s.listeners_after_clear = s.api.listenerCount();
    s.manager.reset();
    s.finished.set_value();
  }).detach();

  ASSERT_EQ(completion.wait_for(std::chrono::seconds(5)), std::future_status::ready)
      << "clearing the endpoint callback deadlocked against a getter in that callback";
  EXPECT_TRUE(scenario->getters_returned.load());
  EXPECT_EQ(scenario->listeners_after_clear, 0u);
}

} // namespace
} // namespace orpheus
#endif
