// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include "coreaudio/coreaudio_sample_rate_monitor.h"

#include <functional>
#include <map>
#include <utility>
#include <vector>

#ifdef ORPHEUS_ENABLE_COREAUDIO

namespace orpheus {
namespace {

class FakeCoreAudioSampleRatePropertyApi final : public ICoreAudioSampleRatePropertyApi {
public:
  struct Listener {
    AudioObjectID device_id;
    AudioObjectPropertyListenerProc callback;
    void* context;
  };

  void setRate(AudioObjectID device_id, Float64 rate) {
    rates_[device_id] = rate;
  }

  void rejectRateChanges() {
    accepts_rate_changes_ = false;
  }

  void failQueries() {
    queries_fail_ = true;
  }


  void notifyDuringNextPropertyRead(AudioObjectID device_id) {
    on_next_property_read_ = [this, device_id] { notifyRateChange(device_id); };
  }
  void notifyRateChange(AudioObjectID device_id) {
    const AudioObjectPropertyAddress address = {kAudioDevicePropertyNominalSampleRate,
                                                kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};
    for (const Listener& listener : listeners_) {
      if (listener.device_id == device_id) {
        ++callback_deliveries_;
        listener.callback(device_id, 1, &address, listener.context);
      }
    }
  }

  size_t listenerCount() const {
    return listeners_.size();
  }

  size_t callbackDeliveries() const {
    return callback_deliveries_;
  }

  OSStatus addPropertyListener(AudioObjectID device_id, const AudioObjectPropertyAddress*,
                               AudioObjectPropertyListenerProc listener,
                               void* context) noexcept override {
    listeners_.push_back({device_id, listener, context});
    return noErr;
  }

  OSStatus removePropertyListener(AudioObjectID device_id, const AudioObjectPropertyAddress*,
                                  AudioObjectPropertyListenerProc listener,
                                  void* context) noexcept override {
    for (auto it = listeners_.begin(); it != listeners_.end(); ++it) {
      if (it->device_id == device_id && it->callback == listener && it->context == context) {
        listeners_.erase(it);
        return noErr;
      }
    }
    return kAudioHardwareBadObjectError;
  }

  OSStatus getPropertyData(AudioObjectID device_id, const AudioObjectPropertyAddress*, UInt32* size,
                           void* data) noexcept override {
    if (queries_fail_ || rates_.find(device_id) == rates_.end() || *size != sizeof(Float64)) {
      return kAudioHardwareUnspecifiedError;
    }
    *static_cast<Float64*>(data) = rates_[device_id];
    if (on_next_property_read_) {
      std::function<void()> callback = std::exchange(on_next_property_read_, {});
      callback();
    }
    return noErr;
  }

  OSStatus setPropertyData(AudioObjectID device_id, const AudioObjectPropertyAddress*, UInt32 size,
                           const void* data) noexcept override {
    if (!accepts_rate_changes_ || size != sizeof(Float64)) {
      return kAudioHardwareUnsupportedOperationError;
    }
    rates_[device_id] = *static_cast<const Float64*>(data);
    return noErr;
  }

private:
  std::map<AudioObjectID, Float64> rates_;
  std::vector<Listener> listeners_;
  bool accepts_rate_changes_{true};
  bool queries_fail_{false};
  size_t callback_deliveries_{0};
  std::function<void()> on_next_property_read_;
};

TEST(CoreAudioSampleRateMonitorTest, RegistersEachUniqueRouteAndCleansUpListeners) {
  FakeCoreAudioSampleRatePropertyApi api;
  api.setRate(11, 48000.0);
  api.setRate(22, 48000.0);
  CoreAudioSampleRateMonitor monitor(api, 48000, {11, 22, 11});

  ASSERT_TRUE(monitor.start());
  EXPECT_EQ(api.listenerCount(), 2u);
  monitor.requestCheck();
  EXPECT_EQ(monitor.poll(), CoreAudioSampleRatePollResult::NoChange);
  EXPECT_TRUE(monitor.permitsRendering());

  monitor.stop();
  EXPECT_EQ(api.listenerCount(), 0u);
  api.notifyRateChange(11);
  EXPECT_EQ(api.callbackDeliveries(), 0u);
  EXPECT_FALSE(monitor.isPending());
}

TEST(CoreAudioSampleRateMonitorTest, ReassertsChangedRateBeforeRenderingResumes) {
  FakeCoreAudioSampleRatePropertyApi api;
  api.setRate(11, 48000.0);
  CoreAudioSampleRateMonitor monitor(api, 48000, {11});

  ASSERT_TRUE(monitor.start());
  monitor.requestCheck();
  ASSERT_EQ(monitor.poll(), CoreAudioSampleRatePollResult::NoChange);
  ASSERT_TRUE(monitor.permitsRendering());

  api.setRate(11, 44100.0);
  api.notifyRateChange(11);
  EXPECT_TRUE(monitor.isPending());
  EXPECT_FALSE(monitor.permitsRendering());
  EXPECT_EQ(monitor.poll(), CoreAudioSampleRatePollResult::RateRestored);
  EXPECT_TRUE(monitor.permitsRendering());

  monitor.requestCheck();
  EXPECT_EQ(monitor.poll(), CoreAudioSampleRatePollResult::NoChange);
}

TEST(CoreAudioSampleRateMonitorTest, RetainsNotificationArrivingDuringFinalPropertyRead) {
  FakeCoreAudioSampleRatePropertyApi api;
  api.setRate(11, 48000.0);
  CoreAudioSampleRateMonitor monitor(api, 48000, {11});

  ASSERT_TRUE(monitor.start());
  monitor.requestCheck();
  ASSERT_EQ(monitor.poll(), CoreAudioSampleRatePollResult::NoChange);
  ASSERT_TRUE(monitor.permitsRendering());

  api.notifyDuringNextPropertyRead(11);
  monitor.requestCheck();
  EXPECT_EQ(monitor.poll(), CoreAudioSampleRatePollResult::NoChange);
  EXPECT_TRUE(monitor.isPending());
  EXPECT_FALSE(monitor.permitsRendering());

  EXPECT_EQ(monitor.poll(), CoreAudioSampleRatePollResult::NoChange);
  EXPECT_FALSE(monitor.isPending());
  EXPECT_TRUE(monitor.permitsRendering());
}

TEST(CoreAudioSampleRateMonitorTest, RefusedReassertionKeepsRenderingBlocked) {
  FakeCoreAudioSampleRatePropertyApi api;
  api.setRate(11, 48000.0);
  CoreAudioSampleRateMonitor monitor(api, 48000, {11});

  ASSERT_TRUE(monitor.start());
  monitor.requestCheck();
  ASSERT_EQ(monitor.poll(), CoreAudioSampleRatePollResult::NoChange);

  api.setRate(11, 44100.0);
  api.rejectRateChanges();
  api.notifyRateChange(11);
  EXPECT_EQ(monitor.poll(), CoreAudioSampleRatePollResult::ReinitializationRequired);
  EXPECT_FALSE(monitor.permitsRendering());
}

TEST(CoreAudioSampleRateMonitorTest, QueryFailureKeepsRenderingBlocked) {
  FakeCoreAudioSampleRatePropertyApi api;
  api.setRate(11, 48000.0);
  CoreAudioSampleRateMonitor monitor(api, 48000, {11});

  ASSERT_TRUE(monitor.start());
  api.failQueries();
  monitor.requestCheck();
  EXPECT_EQ(monitor.poll(), CoreAudioSampleRatePollResult::QueryFailed);
  EXPECT_FALSE(monitor.permitsRendering());
}

} // namespace
} // namespace orpheus

#endif // ORPHEUS_ENABLE_COREAUDIO
