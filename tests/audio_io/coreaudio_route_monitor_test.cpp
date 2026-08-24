// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include "coreaudio/coreaudio_route_monitor.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <vector>

#ifdef ORPHEUS_ENABLE_COREAUDIO

namespace orpheus {
namespace {

struct PropertyKey {
  AudioObjectID object_id;
  AudioObjectPropertySelector selector;

  bool operator<(const PropertyKey& other) const {
    return std::tie(object_id, selector) < std::tie(other.object_id, other.selector);
  }
};

class FakeCoreAudioRoutePropertyApi final : public ICoreAudioSampleRatePropertyApi {
public:
  struct Listener {
    AudioObjectID object_id;
    AudioObjectPropertyAddress address;
    AudioObjectPropertyListenerProc callback;
    void* context;
  };

  void setAlive(AudioObjectID object_id, UInt32 alive) {
    uint32_values_[{object_id, kAudioDevicePropertyDeviceIsAlive}] = alive;
  }

  void setRate(AudioObjectID object_id, Float64 rate) {
    float_values_[{object_id, kAudioDevicePropertyNominalSampleRate}] = rate;
  }

  void setBuffer(AudioObjectID object_id, UInt32 frames) {
    uint32_values_[{object_id, kAudioDevicePropertyBufferFrameSize}] = frames;
  }

  void setFormat(AudioStreamID stream_id, AudioObjectPropertySelector selector,
                 const AudioStreamBasicDescription& format) {
    formats_[{stream_id, selector}] = format;
  }

  void allowRateWrites(bool allowed) {
    allow_rate_writes_ = allowed;
  }

  void failQueries(bool failed) {
    fail_queries_ = failed;
  }

  void notify(AudioObjectID object_id, AudioObjectPropertySelector selector) {
    const AudioObjectPropertyAddress address = {selector, kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};
    for (const Listener& listener : listeners_) {
      if (listener.object_id == object_id && listener.address.mSelector == selector) {
        ++callback_deliveries_;
        listener.callback(object_id, 1, &address, listener.context);
      }
    }
  }

  size_t listenerCount() const {
    return listeners_.size();
  }

  size_t callbackDeliveries() const {
    return callback_deliveries_;
  }

  OSStatus addPropertyListener(AudioObjectID object_id, const AudioObjectPropertyAddress* address,
                               AudioObjectPropertyListenerProc callback,
                               void* context) noexcept override {
    listeners_.push_back({object_id, *address, callback, context});
    return noErr;
  }

  OSStatus removePropertyListener(AudioObjectID object_id,
                                  const AudioObjectPropertyAddress* address,
                                  AudioObjectPropertyListenerProc callback,
                                  void* context) noexcept override {
    const auto it =
        std::find_if(listeners_.begin(), listeners_.end(), [&](const Listener& listener) {
          return listener.object_id == object_id && listener.callback == callback &&
                 listener.context == context && listener.address.mSelector == address->mSelector;
        });
    if (it == listeners_.end()) {
      return -1;
    }
    listeners_.erase(it);
    return noErr;
  }

  OSStatus getPropertyData(AudioObjectID object_id, const AudioObjectPropertyAddress* address,
                           UInt32* size, void* data) noexcept override {
    if (fail_queries_) {
      return -1;
    }
    const PropertyKey key{object_id, address->mSelector};
    if (address->mSelector == kAudioDevicePropertyDeviceIsAlive ||
        address->mSelector == kAudioDevicePropertyBufferFrameSize) {
      const auto it = uint32_values_.find(key);
      if (it == uint32_values_.end() || *size < sizeof(UInt32)) {
        return -1;
      }
      *size = sizeof(UInt32);
      std::memcpy(data, &it->second, sizeof(UInt32));
      return noErr;
    }
    if (address->mSelector == kAudioDevicePropertyNominalSampleRate) {
      const auto it = float_values_.find(key);
      if (it == float_values_.end() || *size < sizeof(Float64)) {
        return -1;
      }
      *size = sizeof(Float64);
      std::memcpy(data, &it->second, sizeof(Float64));
      return noErr;
    }

    const auto it = formats_.find(key);
    if (it == formats_.end() || *size < sizeof(AudioStreamBasicDescription)) {
      return -1;
    }
    *size = sizeof(AudioStreamBasicDescription);
    std::memcpy(data, &it->second, sizeof(AudioStreamBasicDescription));
    return noErr;
  }

  OSStatus setPropertyData(AudioObjectID object_id, const AudioObjectPropertyAddress* address,
                           UInt32 size, const void* data) noexcept override {
    if (address->mSelector != kAudioDevicePropertyNominalSampleRate || !allow_rate_writes_ ||
        size != sizeof(Float64)) {
      return -1;
    }
    Float64 rate = 0.0;
    std::memcpy(&rate, data, sizeof(rate));
    float_values_[{object_id, address->mSelector}] = rate;
    return noErr;
  }

private:
  std::map<PropertyKey, UInt32> uint32_values_;
  std::map<PropertyKey, Float64> float_values_;
  std::map<PropertyKey, AudioStreamBasicDescription> formats_;
  std::vector<Listener> listeners_;
  bool allow_rate_writes_{true};
  bool fail_queries_{false};
  size_t callback_deliveries_{0};
};

AudioStreamBasicDescription testFormat() {
  AudioStreamBasicDescription format{};
  format.mSampleRate = 48000.0;
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
  format.mBytesPerPacket = sizeof(float) * 2;
  format.mFramesPerPacket = 1;
  format.mBytesPerFrame = sizeof(float) * 2;
  format.mChannelsPerFrame = 2;
  format.mBitsPerChannel = 32;
  return format;
}

class RouteMonitorFixture {
public:
  RouteMonitorFixture() : format(testFormat()), stream{100, format, format} {
    api.setAlive(1, 1);
    api.setRate(1, 48000.0);
    api.setBuffer(1, 512);
    api.setFormat(stream.stream_id, kAudioStreamPropertyVirtualFormat,
                  stream.expected_virtual_format);
    api.setFormat(stream.stream_id, kAudioStreamPropertyPhysicalFormat,
                  stream.expected_physical_format);
    monitor = std::make_unique<CoreAudioRouteMonitor>(
        api, 48000, 512, std::vector<CoreAudioRouteDevice>{{1, false, false}},
        std::vector<CoreAudioRouteStream>{stream});
    initialization_ok = monitor->start();
    if (initialization_ok) {
      monitor->requestCheck();
      initialization_ok =
          monitor->poll() == CoreAudioRoutePollResult::NoChange && monitor->permitsRendering();
    }
  }

  ~RouteMonitorFixture() {
    if (monitor) {
      monitor->stop();
    }
  }

  FakeCoreAudioRoutePropertyApi api;
  AudioStreamBasicDescription format;
  CoreAudioRouteStream stream;
  std::unique_ptr<CoreAudioRouteMonitor> monitor;
  bool initialization_ok{false};
};

TEST(CoreAudioRouteMonitorTest, RegistersAllRoutePropertiesAndCleansUp) {
  RouteMonitorFixture fixture;
  ASSERT_TRUE(fixture.initialization_ok);
  EXPECT_EQ(fixture.api.listenerCount(), 5u);
  EXPECT_EQ(fixture.api.callbackDeliveries(), 0u);
  fixture.monitor->stop();
  EXPECT_EQ(fixture.api.listenerCount(), 0u);
}

TEST(CoreAudioRouteMonitorTest, AliveLossClosesGateAndReportsUnavailable) {
  RouteMonitorFixture fixture;
  ASSERT_TRUE(fixture.initialization_ok);
  fixture.api.setAlive(1, 0);
  fixture.api.notify(1, kAudioDevicePropertyDeviceIsAlive);

  EXPECT_FALSE(fixture.monitor->permitsRendering());
  EXPECT_EQ(fixture.monitor->poll(), CoreAudioRoutePollResult::RouteUnavailable);
  EXPECT_FALSE(fixture.monitor->permitsRendering());
}

TEST(CoreAudioRouteMonitorTest, RateChangeRestoresBeforeReopeningGate) {
  RouteMonitorFixture fixture;
  ASSERT_TRUE(fixture.initialization_ok);
  fixture.api.setRate(1, 44100.0);
  fixture.api.notify(1, kAudioDevicePropertyNominalSampleRate);

  EXPECT_FALSE(fixture.monitor->permitsRendering());
  EXPECT_EQ(fixture.monitor->poll(), CoreAudioRoutePollResult::RateRestored);
  EXPECT_TRUE(fixture.monitor->permitsRendering());
}

TEST(CoreAudioRouteMonitorTest, RateOnlyStreamConvergenceUsesVerifiedDeviceRate) {
  FakeCoreAudioRoutePropertyApi api;
  auto initial_format = testFormat();
  initial_format.mSampleRate = 44100.0;
  CoreAudioRouteStream stream{100, initial_format, initial_format};
  api.setAlive(1, 1);
  api.setRate(1, 48000.0);
  api.setBuffer(1, 512);
  api.setFormat(stream.stream_id, kAudioStreamPropertyVirtualFormat,
                stream.expected_virtual_format);
  api.setFormat(stream.stream_id, kAudioStreamPropertyPhysicalFormat,
                stream.expected_physical_format);

  CoreAudioRouteMonitor monitor(api, 48000, 512,
                                std::vector<CoreAudioRouteDevice>{{1, false, false}},
                                std::vector<CoreAudioRouteStream>{stream});
  ASSERT_TRUE(monitor.start());
  monitor.requestCheck();
  ASSERT_EQ(monitor.poll(), CoreAudioRoutePollResult::NoChange);
  ASSERT_TRUE(monitor.permitsRendering());

  auto converged_format = initial_format;
  converged_format.mSampleRate = 48000.0;
  api.setFormat(stream.stream_id, kAudioStreamPropertyVirtualFormat, converged_format);
  api.setFormat(stream.stream_id, kAudioStreamPropertyPhysicalFormat, converged_format);
  api.notify(stream.stream_id, kAudioStreamPropertyVirtualFormat);
  api.notify(stream.stream_id, kAudioStreamPropertyPhysicalFormat);

  EXPECT_FALSE(monitor.permitsRendering());
  EXPECT_EQ(monitor.poll(), CoreAudioRoutePollResult::NoChange);
  EXPECT_TRUE(monitor.permitsRendering());
}

TEST(CoreAudioRouteMonitorTest, RefusedRateRestoreRequiresReinitialization) {
  RouteMonitorFixture fixture;
  ASSERT_TRUE(fixture.initialization_ok);
  fixture.api.allowRateWrites(false);
  fixture.api.setRate(1, 44100.0);
  fixture.api.notify(1, kAudioDevicePropertyNominalSampleRate);

  EXPECT_EQ(fixture.monitor->poll(), CoreAudioRoutePollResult::ReinitializationRequired);
  EXPECT_FALSE(fixture.monitor->permitsRendering());
}

TEST(CoreAudioRouteMonitorTest, StreamFormatChangeReportsFormatChanged) {
  RouteMonitorFixture fixture;
  ASSERT_TRUE(fixture.initialization_ok);
  auto changed = fixture.format;
  changed.mChannelsPerFrame = 1;
  fixture.api.setFormat(fixture.stream.stream_id, kAudioStreamPropertyVirtualFormat, changed);
  fixture.api.notify(fixture.stream.stream_id, kAudioStreamPropertyVirtualFormat);

  EXPECT_EQ(fixture.monitor->poll(), CoreAudioRoutePollResult::FormatChanged);
  EXPECT_FALSE(fixture.monitor->permitsRendering());
}

TEST(CoreAudioRouteMonitorTest, BufferChangeRequiresReinitialization) {
  RouteMonitorFixture fixture;
  ASSERT_TRUE(fixture.initialization_ok);
  fixture.api.setBuffer(1, 256);
  fixture.api.notify(1, kAudioDevicePropertyBufferFrameSize);

  EXPECT_EQ(fixture.monitor->poll(), CoreAudioRoutePollResult::ReinitializationRequired);
  EXPECT_FALSE(fixture.monitor->permitsRendering());
}

TEST(CoreAudioRouteMonitorTest, PropertyReadFailureReportsBackendFailure) {
  RouteMonitorFixture fixture;
  ASSERT_TRUE(fixture.initialization_ok);
  fixture.api.failQueries(true);
  fixture.api.notify(1, kAudioDevicePropertyDeviceIsAlive);

  EXPECT_EQ(fixture.monitor->poll(), CoreAudioRoutePollResult::BackendFailure);
  EXPECT_FALSE(fixture.monitor->permitsRendering());
}
TEST(CoreAudioRouteMonitorTest, ReportsUnavailableDirectionForDistinctDuplexEndpoints) {
  FakeCoreAudioRoutePropertyApi api;
  for (const AudioDeviceID device_id : {AudioDeviceID{1}, AudioDeviceID{2}, AudioDeviceID{3}}) {
    api.setAlive(device_id, 1);
    api.setRate(device_id, 48000.0);
    api.setBuffer(device_id, 512);
  }

  {
    CoreAudioRouteMonitor monitor(
        api, 48000, 512,
        std::vector<CoreAudioRouteDevice>{{1, false, false}, {2, true, false}, {3, false, true}},
        {});
    ASSERT_TRUE(monitor.start());
    monitor.requestCheck();
    ASSERT_EQ(monitor.poll(), CoreAudioRoutePollResult::NoChange);
    api.setAlive(2, 0);
    api.notify(2, kAudioDevicePropertyDeviceIsAlive);
    EXPECT_EQ(monitor.poll(), CoreAudioRoutePollResult::InputUnavailable);
  }

  api.setAlive(2, 1);
  {
    CoreAudioRouteMonitor monitor(
        api, 48000, 512,
        std::vector<CoreAudioRouteDevice>{{1, false, false}, {2, true, false}, {3, false, true}},
        {});
    ASSERT_TRUE(monitor.start());
    monitor.requestCheck();
    ASSERT_EQ(monitor.poll(), CoreAudioRoutePollResult::NoChange);
    api.setAlive(1, 0);
    api.setAlive(3, 0);
    api.notify(3, kAudioDevicePropertyDeviceIsAlive);
    EXPECT_EQ(monitor.poll(), CoreAudioRoutePollResult::OutputUnavailable);
  }
}

} // namespace
} // namespace orpheus

#endif // ORPHEUS_ENABLE_COREAUDIO
