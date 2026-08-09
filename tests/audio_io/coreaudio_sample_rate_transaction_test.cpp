// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include "coreaudio/coreaudio_sample_rate_transaction.h"
#include "coreaudio_property_api_test_support.h"

#include <chrono>
#include <thread>
#include <vector>

#if defined(ORPHEUS_ENABLE_COREAUDIO)
namespace orpheus {
namespace {

constexpr AudioDeviceID kOutputDevice = 11;
constexpr AudioDeviceID kInputDevice = 22;

AudioObjectPropertyAddress nominalRateAddress() {
  return {kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
          kAudioObjectPropertyElementMain};
}

detail::ResolvedCoreAudioRoute distinctRoute() {
  detail::ResolvedCoreAudioRoute route;
  route.resolved = true;
  route.output_device_id = kOutputDevice;
  route.input_device_id = kInputDevice;
  return route;
}

detail::ResolvedCoreAudioRoute sameDeviceRoute() {
  detail::ResolvedCoreAudioRoute route;
  route.resolved = true;
  route.output_device_id = kOutputDevice;
  route.input_device_id = kOutputDevice;
  return route;
}

void seedRates(test_support::FakeCoreAudioPropertyApi& api, Float64 output_rate = 44100.0,
               Float64 input_rate = 44100.0) {
  api.setRate(kOutputDevice, output_rate);
  api.setRate(kInputDevice, input_rate);
}

class CoreAudioSampleRateTransactionTest : public ::testing::Test {
protected:
  test_support::FakeCoreAudioPropertyApi api;
};

TEST_F(CoreAudioSampleRateTransactionTest, SameRateHasNoListenersOrWrites) {
  seedRates(api, 48000.0, 48000.0);

  CoreAudioSampleRateTransaction transaction(api, distinctRoute(), 48000,
                                             std::chrono::milliseconds(20));
  EXPECT_EQ(transaction.begin(), AudioRouteRuntimeOutcome::Healthy);
  EXPECT_TRUE(api.listenerLedger().empty());
  EXPECT_TRUE(api.writeLedger().empty());
}

TEST_F(CoreAudioSampleRateTransactionTest, WritesOutputThenDistinctInputAndConfirmsEvents) {
  seedRates(api);

  CoreAudioSampleRateTransaction transaction(api, distinctRoute(), 48000,
                                             std::chrono::milliseconds(100));
  ASSERT_EQ(transaction.begin(), AudioRouteRuntimeOutcome::Healthy);

  const auto listeners = api.listenerLedger();
  ASSERT_EQ(listeners.size(), 2u);
  EXPECT_EQ(listeners[0].object_id, kOutputDevice);
  EXPECT_EQ(listeners[1].object_id, kInputDevice);

  const auto writes = api.writeLedger();
  ASSERT_EQ(writes.size(), 2u);
  EXPECT_EQ(writes[0].object_id, kOutputDevice);
  EXPECT_EQ(writes[1].object_id, kInputDevice);
  EXPECT_EQ(api.listenerCount(), 0u);

  transaction.commit();
  EXPECT_EQ(api.listenerCount(), 0u);
}

TEST_F(CoreAudioSampleRateTransactionTest, SameDeviceDuplexUsesOneEndpoint) {
  seedRates(api, 44100.0, 44100.0);

  CoreAudioSampleRateTransaction transaction(api, sameDeviceRoute(), 48000,
                                             std::chrono::milliseconds(100));
  ASSERT_EQ(transaction.begin(), AudioRouteRuntimeOutcome::Healthy);

  const auto listeners = api.listenerLedger();
  const auto writes = api.writeLedger();
  ASSERT_EQ(listeners.size(), 1u);
  ASSERT_EQ(writes.size(), 1u);
  EXPECT_EQ(listeners[0].object_id, kOutputDevice);
  EXPECT_EQ(writes[0].object_id, kOutputDevice);
}

TEST_F(CoreAudioSampleRateTransactionTest, SettableFalseFailsBeforeListenersOrWrites) {
  seedRates(api);
  api.setRateSettable(kOutputDevice, false);

  CoreAudioSampleRateTransaction transaction(api, distinctRoute(), 48000,
                                             std::chrono::milliseconds(20));
  EXPECT_EQ(transaction.begin(), AudioRouteRuntimeOutcome::SampleRateChangeFailed);
  EXPECT_TRUE(api.listenerLedger().empty());
  EXPECT_TRUE(api.writeLedger().empty());
}

TEST_F(CoreAudioSampleRateTransactionTest, SettableQueryErrorIsBackendFailure) {
  seedRates(api);
  api.failSettable(-50);

  CoreAudioSampleRateTransaction transaction(api, distinctRoute(), 48000,
                                             std::chrono::milliseconds(20));
  EXPECT_EQ(transaction.begin(), AudioRouteRuntimeOutcome::BackendFailure);
  EXPECT_TRUE(api.listenerLedger().empty());
  EXPECT_TRUE(api.writeLedger().empty());
}

TEST_F(CoreAudioSampleRateTransactionTest, TimeoutRollsBackInReverseOrder) {
  seedRates(api);
  api.suppressListenerDelivery();

  CoreAudioSampleRateTransaction transaction(api, distinctRoute(), 48000,
                                             std::chrono::milliseconds(5));
  EXPECT_EQ(transaction.begin(), AudioRouteRuntimeOutcome::SampleRateChangeFailed);

  const auto writes = api.writeLedger();
  ASSERT_EQ(writes.size(), 4u);
  EXPECT_EQ(writes[0].object_id, kOutputDevice);
  EXPECT_EQ(writes[1].object_id, kInputDevice);
  EXPECT_EQ(writes[2].object_id, kInputDevice);
  EXPECT_EQ(writes[3].object_id, kOutputDevice);
  EXPECT_EQ(api.listenerCount(), 0u);
}

TEST_F(CoreAudioSampleRateTransactionTest, ThirdPartyRateChangeIsNotOverwrittenDuringRollback) {
  seedRates(api);
  api.suppressListenerDelivery();

  CoreAudioSampleRateTransaction transaction(api, distinctRoute(), 48000,
                                             std::chrono::milliseconds(30));
  std::thread mutator([this] {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    api.thirdPartyRateMutation(kOutputDevice, 96000.0);
  });
  EXPECT_EQ(transaction.begin(), AudioRouteRuntimeOutcome::SampleRateChangeFailed);
  mutator.join();

  const auto writes = api.writeLedger();
  ASSERT_EQ(writes.size(), 3u);
  EXPECT_EQ(writes[0].object_id, kOutputDevice);
  EXPECT_EQ(writes[1].object_id, kInputDevice);
  EXPECT_EQ(writes[2].object_id, kInputDevice);
}

TEST_F(CoreAudioSampleRateTransactionTest, SuppressedEventsCanBeDeliveredToCompleteTransaction) {
  seedRates(api);
  api.suppressListenerDelivery();

  CoreAudioSampleRateTransaction transaction(api, distinctRoute(), 48000,
                                             std::chrono::milliseconds(200));
  std::thread delivery([this] {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    api.deliverPendingListeners();
  });
  EXPECT_EQ(transaction.begin(), AudioRouteRuntimeOutcome::Healthy);
  delivery.join();
  EXPECT_EQ(api.writeLedger().size(), 2u);
}

TEST_F(CoreAudioSampleRateTransactionTest, WriteRefusalIsSampleRateChangeFailure) {
  seedRates(api);
  api.failSet(-1);

  CoreAudioSampleRateTransaction transaction(api, distinctRoute(), 48000,
                                             std::chrono::milliseconds(20));
  EXPECT_EQ(transaction.begin(), AudioRouteRuntimeOutcome::SampleRateChangeFailed);
  EXPECT_EQ(api.listenerCount(), 0u);
  EXPECT_TRUE(api.writeLedger().empty());
}

} // namespace
} // namespace orpheus
#endif
