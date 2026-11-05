// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include "coreaudio/coreaudio_sample_rate_transaction.h"
#include "coreaudio_property_api_test_support.h"

#include <algorithm>
#include <atomic>
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

std::vector<detail::CoreAudioDeviceRatePlan> writePlans(bool distinct) {
  detail::CoreAudioDeviceRatePlan output;
  output.device_id = kOutputDevice;
  output.requested_write_rate = 48000u;
  std::vector<detail::CoreAudioDeviceRatePlan> plans{output};
  if (distinct) {
    detail::CoreAudioDeviceRatePlan input;
    input.device_id = kInputDevice;
    input.requested_write_rate = 48000u;
    plans.push_back(input);
  }
  return plans;
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

  CoreAudioSampleRateTransaction transaction(api, writePlans(true), std::chrono::milliseconds(20));
  EXPECT_EQ(transaction.begin(), AudioRouteRuntimeOutcome::Healthy);
  EXPECT_TRUE(api.listenerLedger().empty());
  EXPECT_TRUE(api.writeLedger().empty());
}

TEST_F(CoreAudioSampleRateTransactionTest, WritesOutputThenDistinctInputAndConfirmsEvents) {
  seedRates(api);

  CoreAudioSampleRateTransaction transaction(api, writePlans(true), std::chrono::milliseconds(100));
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

  CoreAudioSampleRateTransaction transaction(api, writePlans(false),
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

  CoreAudioSampleRateTransaction transaction(api, writePlans(true), std::chrono::milliseconds(20));
  EXPECT_EQ(transaction.begin(), AudioRouteRuntimeOutcome::SampleRateChangeFailed);
  EXPECT_TRUE(api.listenerLedger().empty());
  EXPECT_TRUE(api.writeLedger().empty());
}

TEST_F(CoreAudioSampleRateTransactionTest, SettableQueryErrorIsBackendFailure) {
  seedRates(api);
  api.failSettable(-50);

  CoreAudioSampleRateTransaction transaction(api, writePlans(true), std::chrono::milliseconds(20));
  EXPECT_EQ(transaction.begin(), AudioRouteRuntimeOutcome::BackendFailure);
  EXPECT_TRUE(api.listenerLedger().empty());
  EXPECT_TRUE(api.writeLedger().empty());
}

TEST_F(CoreAudioSampleRateTransactionTest, TimeoutRollsBackInReverseOrder) {
  seedRates(api);
  api.suppressListenerDelivery();

  CoreAudioSampleRateTransaction transaction(api, writePlans(true), std::chrono::milliseconds(5));
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

  CoreAudioSampleRateTransaction transaction(api, writePlans(true), std::chrono::milliseconds(100));
  AudioRouteRuntimeOutcome result = AudioRouteRuntimeOutcome::BackendFailure;
  std::thread transaction_thread([&] { result = transaction.begin(); });

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (api.writeLedger().size() < 2u && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  api.thirdPartyRateMutation(kOutputDevice, 96000.0);
  transaction_thread.join();

  ASSERT_EQ(result, AudioRouteRuntimeOutcome::SampleRateChangeFailed);
  const auto writes = api.writeLedger();
  ASSERT_EQ(writes.size(), 3u);
  EXPECT_EQ(writes[0].object_id, kOutputDevice);
  EXPECT_EQ(writes[1].object_id, kInputDevice);
  EXPECT_EQ(writes[2].object_id, kInputDevice);
}

TEST_F(CoreAudioSampleRateTransactionTest, SuppressedEventsCanBeDeliveredToCompleteTransaction) {
  seedRates(api);
  api.suppressListenerDelivery();

  CoreAudioSampleRateTransaction transaction(api, writePlans(true), std::chrono::milliseconds(200));
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

  CoreAudioSampleRateTransaction transaction(api, writePlans(true), std::chrono::milliseconds(20));
  EXPECT_EQ(transaction.begin(), AudioRouteRuntimeOutcome::SampleRateChangeFailed);
  EXPECT_EQ(api.listenerCount(), 0u);
  EXPECT_TRUE(api.writeLedger().empty());
}

TEST_F(CoreAudioSampleRateTransactionTest, ConcurrentListenerDeliveryIsNeverLost) {
  // A listener event delivered between the waiter's predicate check and its
  // wait is lost, and begin() then sleeps to its deadline and rolls back.
  // Oversubscribed racers make preemption inside that window likely.
  constexpr int kIterations = 300;
  constexpr auto kBound = std::chrono::milliseconds(500);
  const unsigned racer_count = std::max(4u, std::thread::hardware_concurrency());
  std::atomic<int> lost{0};
  std::atomic<int> completed{0};

  std::vector<std::thread> racers;
  racers.reserve(racer_count);
  for (unsigned racer = 0; racer < racer_count; ++racer) {
    racers.emplace_back([&] {
      for (int iteration = 0; iteration < kIterations; ++iteration) {
        test_support::FakeCoreAudioPropertyApi racer_api;
        seedRates(racer_api);
        racer_api.suppressListenerDelivery();
        CoreAudioSampleRateTransaction transaction(racer_api, writePlans(true), kBound);
        std::atomic<bool> delivering{true};
        std::thread delivery([&] {
          while (delivering.load(std::memory_order_acquire)) {
            racer_api.deliverPendingListeners();
            std::this_thread::yield();
          }
        });
        const auto started = std::chrono::steady_clock::now();
        const AudioRouteRuntimeOutcome result = transaction.begin();
        const auto elapsed = std::chrono::steady_clock::now() - started;
        delivering.store(false, std::memory_order_release);
        delivery.join();
        if (result != AudioRouteRuntimeOutcome::Healthy || elapsed >= kBound) {
          lost.fetch_add(1);
        } else {
          completed.fetch_add(1);
          transaction.commit();
        }
      }
    });
  }
  for (std::thread& racer : racers) {
    racer.join();
  }

  EXPECT_EQ(lost.load(), 0) << "begin() missed a delivered listener event";
  EXPECT_EQ(completed.load(), static_cast<int>(racer_count) * kIterations);
}

} // namespace
} // namespace orpheus
#endif
