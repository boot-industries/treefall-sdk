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
  OutputOnlyRouteQuery(AudioDeviceID output_id, uint32_t output_channels, double output_rate,
                       uint32_t output_transport = kAudioDeviceTransportTypeBuiltIn,
                       AudioDeviceID input_id = 0, uint32_t input_channels = 0,
                       double input_rate = 0.0,
                       uint32_t input_transport = kAudioDeviceTransportTypeBuiltIn)
      : output_id_(output_id), output_channels_(output_channels), output_rate_(output_rate),
        output_transport_(output_transport), input_id_(input_id), input_channels_(input_channels),
        input_rate_(input_rate), input_transport_(input_transport) {}

  detail::CoreAudioRouteQueryResult<detail::ResolvedCoreAudioEndpoint>
  resolveDefault(bool output) const override {
    if (output) {
      return {detail::CoreAudioRouteQueryStatus::Success, {output_id_, "injected.output"}};
    }
    if (input_id_ != 0) {
      return {detail::CoreAudioRouteQueryStatus::Success, {input_id_, "injected.input"}};
    }
    return {detail::CoreAudioRouteQueryStatus::PermissionDenied, {}};
  }

  detail::CoreAudioRouteQueryResult<detail::ResolvedCoreAudioEndpoint>
  resolveDeviceUID(std::string_view device_uid) const override {
    if (device_uid == "stale.input.uid") {
      stale_input_uid_requests_.fetch_add(1, std::memory_order_relaxed);
      return {detail::CoreAudioRouteQueryStatus::PermissionDenied, {}};
    }
    if (device_uid == "injected.output") {
      return {detail::CoreAudioRouteQueryStatus::Success, {output_id_, "injected.output"}};
    }
    if (device_uid == "injected.input" && input_id_ != 0) {
      return {detail::CoreAudioRouteQueryStatus::Success, {input_id_, "injected.input"}};
    }
    return {detail::CoreAudioRouteQueryStatus::Missing, {}};
  }

  detail::CoreAudioRouteQueryResult<uint32_t>
  channelCount(AudioDeviceID device_id, AudioObjectPropertyScope scope) const override {
    if (device_id == output_id_ && scope == kAudioObjectPropertyScopeOutput) {
      return {detail::CoreAudioRouteQueryStatus::Success, output_channels_};
    }
    if (device_id == input_id_ && scope == kAudioObjectPropertyScopeInput) {
      return {detail::CoreAudioRouteQueryStatus::Success, input_channels_};
    }
    return {detail::CoreAudioRouteQueryStatus::PermissionDenied, 0};
  }

  detail::CoreAudioRouteQueryResult<std::vector<detail::CoreAudioEndpointRange>>
  advertisedSampleRateRanges(AudioDeviceID device_id) const override {
    if (device_id == output_id_) {
      return {detail::CoreAudioRouteQueryStatus::Success, {{44100.0, 48000.0}}};
    }
    if (device_id == input_id_) {
      return {detail::CoreAudioRouteQueryStatus::Success, {{input_rate_, input_rate_}}};
    }
    return {detail::CoreAudioRouteQueryStatus::Missing, {}};
  }

  detail::CoreAudioRouteQueryResult<double>
  currentSampleRate(AudioDeviceID device_id) const override {
    if (device_id == output_id_) {
      return {detail::CoreAudioRouteQueryStatus::Success, output_rate_};
    }
    if (device_id == input_id_) {
      return {detail::CoreAudioRouteQueryStatus::Success, input_rate_};
    }
    return {detail::CoreAudioRouteQueryStatus::Missing, 0.0};
  }

  detail::CoreAudioRouteQueryResult<bool>
  isRunningSomewhere(AudioDeviceID device_id) const override {
    if (device_id == output_id_ || device_id == input_id_) {
      return {detail::CoreAudioRouteQueryStatus::Success, false};
    }
    return {detail::CoreAudioRouteQueryStatus::Missing, false};
  }

  detail::CoreAudioRouteQueryResult<uint32_t>
  transportType(AudioDeviceID device_id) const override {
    if (device_id == output_id_) {
      return {detail::CoreAudioRouteQueryStatus::Success, output_transport_};
    }
    if (device_id == input_id_) {
      return {detail::CoreAudioRouteQueryStatus::Success, input_transport_};
    }
    return {detail::CoreAudioRouteQueryStatus::Missing, 0};
  }

  detail::CoreAudioRouteQueryResult<std::vector<AudioDeviceID>>
  relatedDeviceIDs(AudioDeviceID device_id) const override {
    if (device_id == output_id_) {
      return {detail::CoreAudioRouteQueryStatus::Success, {output_id_}};
    }
    if (device_id == input_id_) {
      return {detail::CoreAudioRouteQueryStatus::Success, {input_id_}};
    }
    return {detail::CoreAudioRouteQueryStatus::Missing, {}};
  }

  detail::CoreAudioRouteQueryResult<detail::CoreAudioStreamFormat>
  physicalStreamFormat(AudioDeviceID device_id, AudioObjectPropertyScope scope) const override {
    if (device_id == output_id_ && scope == kAudioObjectPropertyScopeOutput) {
      return {detail::CoreAudioRouteQueryStatus::Success,
              {static_cast<uint32_t>(output_rate_), static_cast<uint16_t>(output_channels_)}};
    }
    if (device_id == input_id_ && scope == kAudioObjectPropertyScopeInput) {
      return {detail::CoreAudioRouteQueryStatus::Success,
              {static_cast<uint32_t>(input_rate_), static_cast<uint16_t>(input_channels_)}};
    }
    return {detail::CoreAudioRouteQueryStatus::Missing, {}};
  }

  detail::CoreAudioRouteQueryResult<detail::CoreAudioStreamFormat>
  virtualStreamFormat(AudioDeviceID device_id, AudioObjectPropertyScope scope) const override {
    return physicalStreamFormat(device_id, scope);
  }

  detail::CoreAudioRouteQueryResult<bool>
  nominalSampleRateSettable(AudioDeviceID device_id) const override {
    if (device_id == output_id_ || device_id == input_id_) {
      return {detail::CoreAudioRouteQueryStatus::Success, true};
    }
    return {detail::CoreAudioRouteQueryStatus::Missing, false};
  }

  detail::CoreAudioRouteQueryResult<uint32_t>
  physicalChannelCount(AudioDeviceID device_id, AudioObjectPropertyScope scope) const override {
    return channelCount(device_id, scope);
  }

  uint32_t staleInputUidRequests() const noexcept {
    return stale_input_uid_requests_.load(std::memory_order_relaxed);
  }

private:
  AudioDeviceID output_id_;
  uint32_t output_channels_;
  double output_rate_;
  uint32_t output_transport_;
  AudioDeviceID input_id_;
  uint32_t input_channels_;
  double input_rate_;
  uint32_t input_transport_;
  mutable std::atomic<uint32_t> stale_input_uid_requests_{0};
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
    const uint32_t encoded_width = static_cast<uint32_t>(block.num_output_channels) + 1;
    uint32_t expected_width = 0;
    if (!first_output_width.compare_exchange_strong(
            expected_width, encoded_width, std::memory_order_relaxed, std::memory_order_relaxed) &&
        expected_width != encoded_width) {
      output_width_changed.store(true, std::memory_order_relaxed);
    }
    calls.fetch_add(1, std::memory_order_relaxed);
    for (uint16_t channel = 0; channel < block.num_output_channels; ++channel) {
      if (block.output_buffers == nullptr || block.output_buffers[channel] == nullptr) {
        continue;
      }
      for (uint32_t frame = 0; frame < block.num_frames; ++frame) {
        block.output_buffers[channel][frame] = 0.0f;
      }
    }
  }

  uint16_t firstOutputChannels() const noexcept {
    const uint32_t encoded = first_output_width.load(std::memory_order_relaxed);
    return encoded == 0 ? 0 : static_cast<uint16_t>(encoded - 1);
  }

  bool outputWidthChanged() const noexcept {
    return output_width_changed.load(std::memory_order_relaxed);
  }

  std::atomic<uint32_t> calls{0};

private:
  std::atomic<uint32_t> first_output_width{0};
  std::atomic<bool> output_width_changed{false};
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

constexpr AudioDeviceID kSyntheticInputStreamDevice = 7002;
constexpr AudioStreamID kSyntheticInputStream = 7003;

void configureFakeEndpoint(test_support::FakeCoreAudioPropertyApi& property_api,
                           AudioDeviceID device_id, AudioStreamID stream_id, const char* uid,
                           double rate, UInt32 buffer, AudioObjectPropertyScope scope,
                           UInt32 channels) {
  property_api.setAlive(device_id, 1);
  property_api.setDeviceUID(device_id, uid);
  property_api.setRate(device_id, rate);
  property_api.setBuffer(device_id, buffer);
  property_api.setBufferRange(device_id, buffer, buffer, scope);
  property_api.setBufferRange(device_id, buffer, buffer, kAudioObjectPropertyScopeGlobal);
  property_api.setStreams(device_id, scope, {stream_id});
  property_api.setChannelCount(device_id, scope, channels);

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
  property_api.setFormat(stream_id, kAudioStreamPropertyVirtualFormat, format);
  property_api.setFormat(stream_id, kAudioStreamPropertyPhysicalFormat, format);
}

uint32_t deviceChannels(AudioDeviceID device_id, AudioObjectPropertyScope scope) {
  auto property = address(kAudioDevicePropertyStreamConfiguration, scope);
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

uint32_t outputChannels(AudioDeviceID device_id) {
  return deviceChannels(device_id, kAudioObjectPropertyScopeOutput);
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
  EXPECT_GT(callback.calls.load(std::memory_order_relaxed), 0u);
  EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::Healthy);
  EXPECT_EQ(query->staleInputUidRequests(), 0u);
  const auto listeners = property_api->listenerLedger();
  ASSERT_FALSE(listeners.empty());
  for (const auto& listener : listeners) {
    EXPECT_TRUE(listener.object_id == output_id || listener.object_id == kSyntheticStream);
  }

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
  for (const auto& write : writes) {
    EXPECT_EQ(write.address.mSelector, kAudioHardwarePropertyHogModeIsAllowed);
  }
  ASSERT_EQ(writes.size(), 2u);
  UInt32 restored_hog_mode = 0;
  ASSERT_EQ(writes[1].bytes.size(), sizeof(restored_hog_mode));
  std::memcpy(&restored_hog_mode, writes[1].bytes.data(), sizeof(restored_hog_mode));
  EXPECT_EQ(restored_hog_mode, 1u);
}

TEST(CoreAudioOutputOnlyInjectedTest, ReinitializeAfterDuplexRemovesInputStateBeforeStaleOutput) {
  AudioObjectPropertyAddress default_input = {kAudioHardwarePropertyDefaultInputDevice,
                                              kAudioObjectPropertyScopeGlobal,
                                              kAudioObjectPropertyElementMain};
  AudioObjectPropertyAddress default_output = {kAudioHardwarePropertyDefaultOutputDevice,
                                               kAudioObjectPropertyScopeGlobal,
                                               kAudioObjectPropertyElementMain};
  AudioDeviceID input_id = 0;
  AudioDeviceID output_id = 0;
  UInt32 input_size = sizeof(input_id);
  UInt32 output_size = sizeof(output_id);
  if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &default_input, 0, nullptr, &input_size,
                                 &input_id) != noErr ||
      AudioObjectGetPropertyData(kAudioObjectSystemObject, &default_output, 0, nullptr,
                                 &output_size, &output_id) != noErr ||
      input_id == 0 || output_id == 0 || input_id == output_id) {
    GTEST_SKIP() << "Distinct readable CoreAudio input and output routes are unavailable";
  }

  const UInt32 output_channels = outputChannels(output_id);
  const UInt32 input_channels = deviceChannels(input_id, kAudioObjectPropertyScopeInput);
  Float64 input_rate = 0.0;
  Float64 output_rate = 0.0;
  UInt32 input_buffer = 0;
  UInt32 output_buffer = 0;
  if (input_channels == 0 || output_channels < 2 || !readRate(input_id, input_rate) ||
      !readRate(output_id, output_rate) ||
      !readUInt32(input_id, kAudioDevicePropertyBufferFrameSize, input_buffer) ||
      !readUInt32(output_id, kAudioDevicePropertyBufferFrameSize, output_buffer) ||
      input_buffer == 0 || output_buffer == 0 || input_buffer > 0xffffu ||
      output_buffer > 0xffffu || (input_rate != 44100.0 && input_rate != 48000.0) ||
      (output_rate != 44100.0 && output_rate != 48000.0)) {
    GTEST_SKIP() << "Default CoreAudio routes lack deterministic 44.1/48 kHz duplex facts";
  }

  const uint32_t session_rate = input_rate == 44100.0 ? 48000u : 44100u;
  auto property_api = std::make_shared<test_support::FakeCoreAudioPropertyApi>();
  configureFakeEndpoint(*property_api, output_id, kSyntheticStream, "injected.output", output_rate,
                        output_buffer, kAudioObjectPropertyScopeOutput, output_channels);
  configureFakeEndpoint(*property_api, input_id, kSyntheticInputStream, "injected.input",
                        input_rate, input_buffer, kAudioObjectPropertyScopeInput, 1);

  auto query = std::make_shared<OutputOnlyRouteQuery>(output_id, output_channels, output_rate,
                                                      kAudioDeviceTransportTypeBuiltIn, input_id, 1,
                                                      input_rate, kAudioDeviceTransportTypeBuiltIn);
  DirectionAudit audit;
  CoreAudioDriver driver(query, property_api, &audit);

  AudioDriverConfig duplex;
  duplex.sample_rate = session_rate;
  duplex.buffer_size = static_cast<uint16_t>(output_buffer);
  duplex.num_inputs = 1;
  duplex.num_outputs = 2;
  duplex.input_device_id = "injected.input";
  duplex.output_device_id = "injected.output";
  duplex.channel_map.input_channels = {0};
  duplex.channel_map.output_channels = {0, 1};
  duplex.sample_rate_policy = AudioSampleRatePolicy::RequestExactRateOrConvert;
  ASSERT_EQ(driver.initialize(duplex), SessionGraphError::OK);
  OutputCallback duplex_callback;
  ASSERT_EQ(driver.start(&duplex_callback), SessionGraphError::OK);
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  ASSERT_EQ(driver.stop(), SessionGraphError::OK);
  ASSERT_GT(duplex_callback.calls.load(std::memory_order_relaxed), 0u);
  const size_t duplex_audit_count = audit.operations.size();
  ASSERT_GT(duplex_audit_count, 0u);

  property_api->clearLedgers();
  ASSERT_EQ(property_api->listenerCount(), 0u);

  duplex.num_inputs = 0;
  duplex.input_device_id = "stale.input.uid";
  duplex.channel_map.input_channels = {7, 8};
  ASSERT_EQ(driver.initialize(duplex), SessionGraphError::OK);
  EXPECT_EQ(query->staleInputUidRequests(), 0u);
  EXPECT_EQ(property_api->listenerCount(), 0u);
  EXPECT_EQ(audit.operations.size(), duplex_audit_count);

  OutputCallback output_callback;
  ASSERT_EQ(driver.start(&output_callback), SessionGraphError::OK);
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  ASSERT_EQ(driver.stop(), SessionGraphError::OK);
  EXPECT_GT(output_callback.calls.load(std::memory_order_relaxed), 0u);
  for (const auto& listener : property_api->listenerLedger()) {
    EXPECT_TRUE(listener.object_id == output_id || listener.object_id == kSyntheticStream);
    EXPECT_NE(listener.object_id, kSyntheticInputStream);
  }
}

TEST(CoreAudioOutputOnlyInjectedTest, BluetoothMonoRequiresExplicitFallbackAndPublishesOneChannel) {
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

  Float64 rate = 0.0;
  UInt32 buffer = 0;
  if (!readRate(output_id, rate) ||
      !readUInt32(output_id, kAudioDevicePropertyBufferFrameSize, buffer) || buffer == 0 ||
      buffer > 0xffffu || rate > 0xffffu) {
    GTEST_SKIP() << "Default output lacks a usable mono fixture rate/buffer route";
  }

  auto property_api = std::make_shared<test_support::FakeCoreAudioPropertyApi>();
  configureFakeEndpoint(*property_api, output_id, kSyntheticStream, "injected.output", rate, buffer,
                        kAudioObjectPropertyScopeOutput, 1);
  auto query = std::make_shared<OutputOnlyRouteQuery>(output_id, 1, rate,
                                                      kAudioDeviceTransportTypeBluetooth);
  DirectionAudit audit;
  CoreAudioDriver driver(query, property_api, &audit);

  AudioDriverConfig config;
  config.sample_rate = static_cast<uint32_t>(rate);
  config.buffer_size = static_cast<uint16_t>(buffer);
  config.num_inputs = 0;
  config.num_outputs = 2;
  config.output_device_id = "injected.output";
  config.channel_map.output_channels = {0, 1};
  config.sample_rate_policy = AudioSampleRatePolicy::RequestExactRateOrConvert;
  config.output_channel_policy = AudioOutputChannelPolicy::RequireRequestedChannels;

  EXPECT_EQ(driver.initialize(config), SessionGraphError::InvalidParameter);
  EXPECT_EQ(driver.getTelemetry().route_outcome, AudioRouteRuntimeOutcome::ProfileConflict);

  config.output_channel_policy = AudioOutputChannelPolicy::AllowMonoFallback;
  ASSERT_EQ(driver.initialize(config), SessionGraphError::OK);
  const auto active = driver.getActiveRoute();
  EXPECT_EQ(active.output_channels, (std::vector<uint16_t>{0}));
  EXPECT_EQ(active.resolved_output_channels, 1u);
  EXPECT_TRUE(active.output_mono_fallback);

  OutputCallback callback;
  ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  ASSERT_EQ(driver.stop(), SessionGraphError::OK);
  EXPECT_GT(callback.calls.load(std::memory_order_relaxed), 0u);
  EXPECT_EQ(callback.firstOutputChannels(), 1u);
  EXPECT_FALSE(callback.outputWidthChanged());
  EXPECT_TRUE(audit.operations.empty());
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
