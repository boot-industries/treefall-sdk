// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include "coreaudio/coreaudio_route_monitor.h"
#include "coreaudio_property_api_test_support.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#ifdef ORPHEUS_ENABLE_COREAUDIO

namespace orpheus {
namespace {
using test_support::FakeCoreAudioPropertyApi;

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
        api, 48000, 512, std::vector<CoreAudioRouteDevice>{{1, false, true, false}},
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

  FakeCoreAudioPropertyApi api;
  AudioStreamBasicDescription format;
  CoreAudioRouteStream stream;
  std::unique_ptr<CoreAudioRouteMonitor> monitor;
  bool initialization_ok{false};
};

TEST(CoreAudioRouteMonitorTest, RegistersInSuppliedOrderAndCleansUp) {
  RouteMonitorFixture fixture;
  ASSERT_TRUE(fixture.initialization_ok);
  EXPECT_EQ(fixture.api.listenerCount(), 5u);
  EXPECT_EQ(fixture.api.callbackDeliveries(), 0u);
  fixture.monitor->stop();
  EXPECT_EQ(fixture.api.listenerCount(), 0u);
}

TEST(CoreAudioRouteMonitorTest, DeduplicatesSharedDeviceAndStreamRegistrations) {
  FakeCoreAudioPropertyApi api;
  api.setAlive(1, 1);
  api.setRate(1, 48000.0);
  api.setBuffer(1, 512);
  const AudioStreamBasicDescription format = testFormat();
  api.setFormat(100, kAudioStreamPropertyVirtualFormat, format);
  api.setFormat(100, kAudioStreamPropertyPhysicalFormat, format);

  CoreAudioRouteMonitor monitor(
      api, 48000, 512,
      std::vector<CoreAudioRouteDevice>{{1, true, false, false, 48000},
                                        {1, false, true, false, 48000}},
      std::vector<CoreAudioRouteStream>{{100, format, format}, {100, format, format}});

  ASSERT_TRUE(monitor.start());
  EXPECT_EQ(api.listenerCount(), 5u);
  monitor.requestCheck();
  EXPECT_EQ(monitor.poll(), CoreAudioRoutePollResult::NoChange);
  monitor.stop();
  EXPECT_EQ(api.listenerCount(), 0u);
}

TEST(CoreAudioRouteMonitorTest, AliveLossClosesAdmissionAndReportsOutputUnavailable) {
  RouteMonitorFixture fixture;
  ASSERT_TRUE(fixture.initialization_ok);
  fixture.api.setAlive(1, 0);
  fixture.api.notify(1, kAudioDevicePropertyDeviceIsAlive);

  EXPECT_TRUE(fixture.monitor->permitsRendering())
      << "Passive monitoring keeps admission open until the control worker reports terminal loss";
  EXPECT_EQ(fixture.monitor->poll(), CoreAudioRoutePollResult::OutputUnavailable);
  EXPECT_TRUE(fixture.monitor->isTerminal());
  EXPECT_TRUE(fixture.api.writeLedger().empty());
}

TEST(CoreAudioRouteMonitorTest, RateChangeIsReportedWithoutWriteBack) {
  RouteMonitorFixture fixture;
  ASSERT_TRUE(fixture.initialization_ok);
  fixture.api.thirdPartyRateMutation(1, 44100.0);
  fixture.api.notify(1, kAudioDevicePropertyNominalSampleRate);

  EXPECT_EQ(fixture.monitor->poll(), CoreAudioRoutePollResult::SampleRateChanged);
  EXPECT_FALSE(fixture.monitor->permitsRendering());
  EXPECT_TRUE(fixture.api.writeLedger().empty());
}

TEST(CoreAudioRouteMonitorTest, RateOnlyStreamConvergenceUsesVerifiedDeviceRate) {
  FakeCoreAudioPropertyApi api;
  api.setAlive(1, 1);
  api.setRate(1, 48000.0);
  api.setBuffer(1, 512);

  const AudioStreamID stream_id = 100;
  auto initial_format = testFormat();
  initial_format.mSampleRate = 44100.0;
  const CoreAudioRouteStream stream{stream_id, initial_format, initial_format};
  api.setFormat(stream_id, kAudioStreamPropertyVirtualFormat, initial_format);
  api.setFormat(stream_id, kAudioStreamPropertyPhysicalFormat, initial_format);

  CoreAudioRouteMonitor monitor(api, 48000, 512,
                                std::vector<CoreAudioRouteDevice>{{1, false, true, false}},
                                std::vector<CoreAudioRouteStream>{stream});
  ASSERT_TRUE(monitor.start());
  monitor.requestCheck();
  ASSERT_EQ(monitor.poll(), CoreAudioRoutePollResult::NoChange);
  ASSERT_TRUE(monitor.permitsRendering());

  auto converged_format = initial_format;
  converged_format.mSampleRate = 48000.0;
  api.setFormat(stream_id, kAudioStreamPropertyVirtualFormat, converged_format);
  api.setFormat(stream_id, kAudioStreamPropertyPhysicalFormat, converged_format);
  api.notify(stream_id, kAudioStreamPropertyVirtualFormat);
  api.notify(stream_id, kAudioStreamPropertyPhysicalFormat);

  EXPECT_EQ(monitor.poll(), CoreAudioRoutePollResult::NoChange);
  EXPECT_TRUE(monitor.permitsRendering());
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

TEST(CoreAudioRouteMonitorTest, BufferChangeReportsBufferSizeChanged) {
  RouteMonitorFixture fixture;
  ASSERT_TRUE(fixture.initialization_ok);
  fixture.api.setBuffer(1, 256);
  fixture.api.notify(1, kAudioDevicePropertyBufferFrameSize);

  EXPECT_EQ(fixture.monitor->poll(), CoreAudioRoutePollResult::BufferSizeChanged);
  EXPECT_FALSE(fixture.monitor->permitsRendering());
}

TEST(CoreAudioRouteMonitorTest, PropertyReadFailureReportsBackendFailure) {
  RouteMonitorFixture fixture;
  ASSERT_TRUE(fixture.initialization_ok);
  fixture.api.failGet(-1);
  fixture.api.notify(1, kAudioDevicePropertyDeviceIsAlive);

  EXPECT_EQ(fixture.monitor->poll(), CoreAudioRoutePollResult::BackendFailure);
  EXPECT_FALSE(fixture.monitor->permitsRendering());
}

TEST(CoreAudioRouteMonitorTest, PhysicalOutputPrecedesInputAndAggregateLoss) {
  FakeCoreAudioPropertyApi api;
  for (const AudioDeviceID device_id : {AudioDeviceID{1}, AudioDeviceID{2}, AudioDeviceID{3}}) {
    api.setAlive(device_id, 1);
    api.setRate(device_id, 48000.0);
    api.setBuffer(device_id, 512);
  }

  CoreAudioRouteMonitor monitor(api, 48000, 512,
                                std::vector<CoreAudioRouteDevice>{{3, false, false, true},
                                                                  {2, true, false, false},
                                                                  {1, false, true, false}},
                                {});
  ASSERT_TRUE(monitor.start());
  monitor.requestCheck();
  ASSERT_EQ(monitor.poll(), CoreAudioRoutePollResult::NoChange);

  api.setAlive(2, 0);
  api.setAlive(3, 0);
  api.notify(2, kAudioDevicePropertyDeviceIsAlive);
  EXPECT_EQ(monitor.poll(), CoreAudioRoutePollResult::InputUnavailable);

  monitor.stop();
  api.setAlive(2, 1);
  api.setAlive(3, 1);
  ASSERT_TRUE(monitor.start());
  monitor.requestCheck();
  ASSERT_EQ(monitor.poll(), CoreAudioRoutePollResult::NoChange);
  api.setAlive(1, 0);
  api.setAlive(3, 0);
  api.notify(3, kAudioDevicePropertyDeviceIsAlive);
  EXPECT_EQ(monitor.poll(), CoreAudioRoutePollResult::OutputUnavailable);
}

TEST(CoreAudioRouteMonitorTest, SameDeviceDuplexLossUsesOutputPrecedence) {
  FakeCoreAudioPropertyApi api;
  api.setAlive(1, 1);
  api.setRate(1, 48000.0);
  api.setBuffer(1, 512);
  CoreAudioRouteMonitor monitor(
      api, 48000, 512,
      std::vector<CoreAudioRouteDevice>{{1, true, false, false}, {1, false, true, false}}, {});
  ASSERT_TRUE(monitor.start());
  monitor.requestCheck();
  ASSERT_EQ(monitor.poll(), CoreAudioRoutePollResult::NoChange);
  api.setAlive(1, 0);
  api.notify(1, kAudioDevicePropertyDeviceIsAlive);
  EXPECT_EQ(monitor.poll(), CoreAudioRoutePollResult::OutputUnavailable);
}

TEST(CoreAudioRouteMonitorTest, StopWakesAtomicWorkerWait) {
  RouteMonitorFixture fixture;
  ASSERT_TRUE(fixture.initialization_ok);
  std::atomic<bool> returned{false};
  std::thread worker([&] {
    fixture.monitor->waitForChange();
    returned.store(true, std::memory_order_release);
  });
  fixture.monitor->stop();
  worker.join();
  EXPECT_TRUE(returned.load(std::memory_order_acquire));
}

TEST(CoreAudioRouteMonitorTest, DeviceCallbackTerminalTransitionDoesNotNotify) {
  RouteMonitorFixture fixture;
  ASSERT_TRUE(fixture.initialization_ok);
  const uint64_t notifications_before = fixture.monitor->notificationCount();

  // The device (realtime) callback publishes the terminal state and returns.
  // Notifying a waiter there is a futex wake per callback, and the audio
  // thread must not perform system calls.
  fixture.monitor->closeAdmissionFromCallback();
  EXPECT_TRUE(fixture.monitor->isTerminal());
  EXPECT_EQ(fixture.monitor->notificationCount(), notifications_before)
      << "the device-callback terminal transition must not notify a waiter";

  // The control path still notifies, so the worker is woken promptly.
  RouteMonitorFixture control;
  ASSERT_TRUE(control.initialization_ok);
  const uint64_t control_before = control.monitor->notificationCount();
  control.monitor->closeAdmission();
  EXPECT_TRUE(control.monitor->isTerminal());
  EXPECT_GT(control.monitor->notificationCount(), control_before)
      << "the control terminal transition must notify a waiting route worker";
}

TEST(CoreAudioRouteMonitorTest, WorkerObservesCallbackPublishedTerminalState) {
  RouteMonitorFixture fixture;
  ASSERT_TRUE(fixture.initialization_ok);
  std::atomic<bool> returned{false};
  std::thread worker([&] {
    fixture.monitor->waitForChange();
    returned.store(true, std::memory_order_release);
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  fixture.monitor->closeAdmissionFromCallback();
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!returned.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  worker.join();
  EXPECT_TRUE(returned.load(std::memory_order_acquire))
      << "the route worker must observe a callback-published terminal state";
}

} // namespace
} // namespace orpheus

#endif // ORPHEUS_ENABLE_COREAUDIO
