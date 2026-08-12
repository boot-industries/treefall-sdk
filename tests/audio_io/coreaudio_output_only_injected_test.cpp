// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include "coreaudio/coreaudio_driver.h"
#include "coreaudio_property_api_test_support.h"

#include <CoreAudio/CoreAudio.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#if defined(ORPHEUS_ENABLE_COREAUDIO)
namespace orpheus {
namespace {

constexpr AudioDeviceID kSyntheticStream = 7001;

class OutputOnlyRouteQuery final : public detail::ICoreAudioRouteQuery {
public:
  OutputOnlyRouteQuery(AudioDeviceID output_id, uint32_t channels, double rate)
      : output_id_(output_id), channels_(channels), rate_(rate) {}

  detail::CoreAudioRouteQueryResult<detail::ResolvedCoreAudioEndpoint>
  resolveDefault(bool output) const override {
    if (!output) {
      return {detail::CoreAudioRouteQueryStatus::PermissionDenied, {}};
    }
    return {detail::CoreAudioRouteQueryStatus::Success, {output_id_, "injected.output"}};
  }

  detail::CoreAudioRouteQueryResult<detail::ResolvedCoreAudioEndpoint>
  resolveDeviceUID(std::string_view device_uid) const override {
    if (device_uid == "injected.output") {
      return {detail::CoreAudioRouteQueryStatus::Success, {output_id_, "injected.output"}};
    }
    return {detail::CoreAudioRouteQueryStatus::Missing, {}};
  }

  detail::CoreAudioRouteQueryResult<uint32_t>
  channelCount(AudioDeviceID device_id, AudioObjectPropertyScope scope) const override {
    if (device_id != output_id_ || scope != kAudioObjectPropertyScopeOutput) {
      return {detail::CoreAudioRouteQueryStatus::PermissionDenied, 0};
    }
    return {detail::CoreAudioRouteQueryStatus::Success, channels_};
  }

  detail::CoreAudioRouteQueryResult<std::vector<detail::CoreAudioEndpointRange>>
  advertisedSampleRateRanges(AudioDeviceID device_id) const override {
    if (device_id != output_id_) {
      return {detail::CoreAudioRouteQueryStatus::Missing, {}};
    }
    return {detail::CoreAudioRouteQueryStatus::Success, {{44100.0, 48000.0}}};
  }

  detail::CoreAudioRouteQueryResult<double>
  currentSampleRate(AudioDeviceID device_id) const override {
    if (device_id != output_id_) {
      return {detail::CoreAudioRouteQueryStatus::Missing, 0.0};
    }
    return {detail::CoreAudioRouteQueryStatus::Success, rate_};
  }

  detail::CoreAudioRouteQueryResult<bool>
  isRunningSomewhere(AudioDeviceID device_id) const override {
    if (device_id != output_id_) {
      return {detail::CoreAudioRouteQueryStatus::Missing, false};
    }
    return {detail::CoreAudioRouteQueryStatus::Success, false};
  }
  detail::CoreAudioRouteQueryResult<uint32_t>
  transportType(AudioDeviceID device_id) const override {
    if (device_id != output_id_) {
      return {detail::CoreAudioRouteQueryStatus::Missing, 0};
    }
    return {detail::CoreAudioRouteQueryStatus::Success, kAudioDeviceTransportTypeBuiltIn};
  }

  detail::CoreAudioRouteQueryResult<std::vector<AudioDeviceID>>
  relatedDeviceIDs(AudioDeviceID device_id) const override {
    if (device_id != output_id_) {
      return {detail::CoreAudioRouteQueryStatus::Missing, {}};
    }
    return {detail::CoreAudioRouteQueryStatus::Success, {output_id_}};
  }

  detail::CoreAudioRouteQueryResult<detail::CoreAudioStreamFormat>
  physicalStreamFormat(AudioDeviceID device_id, AudioObjectPropertyScope scope) const override {
    if (device_id != output_id_ || scope != kAudioObjectPropertyScopeOutput) {
      return {detail::CoreAudioRouteQueryStatus::Missing, {}};
    }
    return {detail::CoreAudioRouteQueryStatus::Success,
            {static_cast<uint32_t>(rate_), static_cast<uint16_t>(channels_)}};
  }

  detail::CoreAudioRouteQueryResult<detail::CoreAudioStreamFormat>
  virtualStreamFormat(AudioDeviceID device_id, AudioObjectPropertyScope scope) const override {
    return physicalStreamFormat(device_id, scope);
  }

  detail::CoreAudioRouteQueryResult<bool>
  nominalSampleRateSettable(AudioDeviceID device_id) const override {
    if (device_id != output_id_) {
      return {detail::CoreAudioRouteQueryStatus::Missing, false};
    }
    return {detail::CoreAudioRouteQueryStatus::Success, true};
  }

  detail::CoreAudioRouteQueryResult<uint32_t>
  physicalChannelCount(AudioDeviceID device_id, AudioObjectPropertyScope scope) const override {
    return channelCount(device_id, scope);
  }

private:
  AudioDeviceID output_id_;
  uint32_t channels_;
  double rate_;
};

class DirectionAudit final : public detail::CoreAudioDriverDirectionAudit {
public:
  void beforeInputOperation(InputDirectionOperation operation) noexcept override {
    operations.push_back(operation);
  }

  std::vector<InputDirectionOperation> operations;
};

class OutputCallback final : public IAudioCallback {
public:
  void processAudio(const AudioProcessBlock& block) noexcept override {
    calls.fetch_add(1, std::memory_order_relaxed);
    for (uint16_t channel = 0; channel < block.num_output_channels; ++channel) {
      for (uint32_t frame = 0; frame < block.num_frames; ++frame) {
        block.output_buffers[channel][frame] = 0.0f;
      }
    }
  }

  std::atomic<uint32_t> calls{0};
};

AudioObjectPropertyAddress
address(AudioObjectPropertySelector selector,
        AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal) {
  return {selector, scope, kAudioObjectPropertyElementMain};
}

bool readUInt32(AudioDeviceID device_id, AudioObjectPropertySelector selector, UInt32& value) {
  auto property = address(selector);
  UInt32 size = sizeof(value);
  return AudioObjectGetPropertyData(device_id, &property, 0, nullptr, &size, &value) == noErr &&
         size == sizeof(value);
}

bool readRate(AudioDeviceID device_id, Float64& value) {
  auto property = address(kAudioDevicePropertyNominalSampleRate);
  UInt32 size = sizeof(value);
  return AudioObjectGetPropertyData(device_id, &property, 0, nullptr, &size, &value) == noErr &&
         size == sizeof(value) && value > 0.0;
}

uint32_t outputChannels(AudioDeviceID device_id) {
  auto property = address(kAudioDevicePropertyStreamConfiguration, kAudioObjectPropertyScopeOutput);
  UInt32 size = 0;
  if (AudioObjectGetPropertyDataSize(device_id, &property, 0, nullptr, &size) != noErr ||
      size < sizeof(AudioBufferList)) {
    return 0;
  }
  std::vector<uint8_t> storage(size);
  auto* buffers = reinterpret_cast<AudioBufferList*>(storage.data());
  if (AudioObjectGetPropertyData(device_id, &property, 0, nullptr, &size, buffers) != noErr) {
    return 0;
  }
  uint32_t channels = 0;
  for (UInt32 index = 0; index < buffers->mNumberBuffers; ++index) {
    channels += buffers->mBuffers[index].mNumberChannels;
  }
  return channels;
}

TEST(CoreAudioOutputOnlyInjectedTest, NeverTouchesStaleInputDirection) {
  AudioObjectPropertyAddress default_output = {kAudioHardwarePropertyDefaultOutputDevice,
                                               kAudioObjectPropertyScopeGlobal,
                                               kAudioObjectPropertyElementMain};
  AudioDeviceID output_id = 0;
  UInt32 default_size = sizeof(output_id);
  if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &default_output, 0, nullptr,
                                 &default_size, &output_id) != noErr ||
      output_id == 0) {
    GTEST_SKIP() << "No readable CoreAudio default output";
  }

  UInt32 channels = outputChannels(output_id);
  Float64 rate = 0.0;
  UInt32 buffer = 0;
  if (channels < 2 || !readRate(output_id, rate) ||
      !readUInt32(output_id, kAudioDevicePropertyBufferFrameSize, buffer) || buffer == 0 ||
      buffer > 0xffffu || rate > 0xffffu) {
    GTEST_SKIP() << "Default output lacks a usable stereo rate/buffer route";
  }

  auto property_api = std::make_shared<test_support::FakeCoreAudioPropertyApi>();
  property_api->setAlive(output_id, 1);
  property_api->setDeviceUID(output_id, "injected.output");
  property_api->setRate(output_id, rate);
  property_api->setBuffer(output_id, buffer);
  property_api->setBufferRange(output_id, buffer, buffer, kAudioObjectPropertyScopeOutput);
  property_api->setBufferRange(output_id, buffer, buffer, kAudioObjectPropertyScopeGlobal);
  property_api->setStreams(output_id, kAudioObjectPropertyScopeOutput, {kSyntheticStream});

  AudioStreamBasicDescription format{};
  format.mSampleRate = rate;
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags =
      kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
  format.mBytesPerPacket = sizeof(float);
  format.mFramesPerPacket = 1;
  format.mBytesPerFrame = sizeof(float);
  format.mChannelsPerFrame = channels;
  format.mBitsPerChannel = 32;
  property_api->setFormat(kSyntheticStream, kAudioStreamPropertyVirtualFormat, format);
  property_api->setFormat(kSyntheticStream, kAudioStreamPropertyPhysicalFormat, format);

  auto query = std::make_shared<OutputOnlyRouteQuery>(output_id, channels, rate);
  DirectionAudit audit;
  CoreAudioDriver driver(query, property_api, &audit);

  AudioDriverConfig config;
  config.sample_rate = static_cast<uint32_t>(rate);
  config.buffer_size = static_cast<uint16_t>(buffer);
  config.num_inputs = 0;
  config.num_outputs = 2;
  config.input_device_id = "stale.input.uid";
  config.channel_map.input_channels = {7, 8, 9};

  ASSERT_EQ(driver.initialize(config), SessionGraphError::OK);
  OutputCallback callback;
  ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  ASSERT_EQ(driver.stop(), SessionGraphError::OK);

  EXPECT_TRUE(audit.operations.empty());
  EXPECT_TRUE(property_api->writeLedger().empty());

  property_api->clearLedgers();
  property_api->setHogModeAllowed(1);
  property_api->setHogModeSettable(true);
  config.sample_rate_policy = AudioSampleRatePolicy::RequestExactRate;
  ASSERT_EQ(driver.initialize(config), SessionGraphError::OK);
  auto writes = property_api->writeLedger();
  ASSERT_EQ(writes.size(), 1u);
  EXPECT_EQ(writes[0].address.mSelector, kAudioHardwarePropertyHogModeIsAllowed);
  UInt32 disabled_hog_mode = 99;
  ASSERT_EQ(writes[0].bytes.size(), sizeof(disabled_hog_mode));
  std::memcpy(&disabled_hog_mode, writes[0].bytes.data(), sizeof(disabled_hog_mode));
  EXPECT_EQ(disabled_hog_mode, 0u);

  ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
  ASSERT_EQ(driver.stop(), SessionGraphError::OK);
  writes = property_api->writeLedger();
  ASSERT_EQ(writes.size(), 2u);
  UInt32 restored_hog_mode = 0;
  ASSERT_EQ(writes[1].bytes.size(), sizeof(restored_hog_mode));
  std::memcpy(&restored_hog_mode, writes[1].bytes.data(), sizeof(restored_hog_mode));
  EXPECT_EQ(restored_hog_mode, 1u);
}

TEST(CoreAudioOutputOnlyInjectedTest, PreserveRateMismatchIsRejectedWithoutPropertyWrites) {
  auto property_api = std::make_shared<test_support::FakeCoreAudioPropertyApi>();
  auto query = std::make_shared<OutputOnlyRouteQuery>(11, 2, 48000.0);
  DirectionAudit audit;
  CoreAudioDriver driver(query, property_api, &audit);

  AudioDriverConfig config;
  config.sample_rate = 44100;
  config.buffer_size = 256;
  config.num_inputs = 0;
  config.num_outputs = 2;
  config.output_device_id = "injected.output";
  config.sample_rate_policy = AudioSampleRatePolicy::PreserveDeviceRate;

  EXPECT_EQ(driver.initialize(config), SessionGraphError::InvalidParameter);
  EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::SampleRateChanged);
  EXPECT_TRUE(property_api->writeLedger().empty());
  EXPECT_TRUE(audit.operations.empty());
}
} // namespace
} // namespace orpheus
#endif
