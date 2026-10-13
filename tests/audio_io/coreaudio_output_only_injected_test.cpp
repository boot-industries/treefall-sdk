// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include "coreaudio/coreaudio_driver.h"
#include "coreaudio_property_api_test_support.h"

#include <CoreAudio/CoreAudio.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <sys/utsname.h>
#include <thread>
#include <vector>

#if defined(ORPHEUS_ENABLE_COREAUDIO)
namespace orpheus {

// This friend prepares only synthetic startup state. AudioUnit creation,
// initialization and physical playback are deliberately not part of these tests.
struct CoreAudioDriverTestAccess {
  static SessionGraphError prepareConvertedInput(CoreAudioDriver& driver, AudioUnit input,
                                                 AudioUnit output,
                                                 detail::CoreAudioDriverControlTestHooks hooks,
                                                 detail::AudioOutputUnitStartFunction start_unit) {
    driver.control_test_hooks_ = hooks;
    driver.audio_unit_start_ = start_unit;
    driver.config_.sample_rate = 48000;
    driver.config_.buffer_size = 128;
    driver.config_.num_inputs = 1;
    driver.config_.num_outputs = 1;
    driver.input_device_id_ = 7202;
    driver.output_device_id_ = 7201;
    driver.device_id_ = 7201;
    driver.input_audio_unit_ = input;
    driver.audio_unit_ = output;
    driver.input_conversion_active_ = true;
    driver.input_physical_rate_ = 44100;
    driver.output_physical_rate_ = 48000;
    driver.input_client_channels_ = 1;
    driver.output_client_channels_ = 1;
    driver.available_input_channels_ = 1;
    driver.available_output_channels_ = 1;
    driver.input_channel_map_ = {0};
    driver.output_channel_map_ = {0};
    driver.render_capacity_frames_ = 128;
    driver.input_render_capacity_frames_ = 256;
    audio_utils::DirectionalSrcConfig converter;
    converter.input_rate = 44100;
    converter.output_rate = 48000;
    converter.channels = 1;
    converter.max_input_frames = 256;
    converter.max_output_frames = 128;
    converter.fifo_capacity_frames = 2048;
    converter.prime_input_frames = 64;
    const auto result = driver.input_converter_.prepare(converter);
    if (result != SessionGraphError::OK) {
      return result;
    }
    driver.route_monitor_ = std::make_unique<CoreAudioRouteMonitor>(
        *driver.property_api_, 48000, 128,
        std::vector<CoreAudioRouteDevice>{{7202, true, false, false, 44100},
                                          {7201, false, true, false, 48000}},
        std::vector<CoreAudioRouteStream>{});
    driver.refreshActiveRouteLocked();
    return SessionGraphError::OK;
  }

  static bool mutexBusy(CoreAudioDriver& driver) {
    std::unique_lock<std::mutex> lock(driver.mutex_, std::try_to_lock);
    return !lock.owns_lock();
  }

  static audio_utils::DirectionalSrcTransfer supplyInput(CoreAudioDriver& driver) {
    std::array<float, 256> input{};
    input.fill(0.25f);
    const float* channels[] = {input.data()};
    return driver.input_converter_.pushInput(channels, 1, input.size());
  }

  static bool inputAdmitted(CoreAudioDriver& driver) {
    return static_cast<bool>(driver.input_callback_target_.tryAcquire());
  }

  static bool outputAdmitted(CoreAudioDriver& driver) {
    return static_cast<bool>(driver.output_callback_target_.tryAcquire());
  }

  static bool clientAdmitted(CoreAudioDriver& driver) {
    return static_cast<bool>(driver.callback_target_.tryAcquire());
  }

  static bool completelyCleaned(CoreAudioDriver& driver) {
    return !driver.audio_unit_ && !driver.input_audio_unit_ && !driver.route_monitor_ &&
           driver.render_capacity_frames_ == 0 && driver.input_render_capacity_frames_ == 0 &&
           driver.render_sample_rate_.load() == 0 &&
           driver.input_converter_.bufferedInputFrames() == 0 && !inputAdmitted(driver) &&
           !outputAdmitted(driver) && !clientAdmitted(driver);
  }
};

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

struct HardwareOutputOnlyRoute {
  std::shared_ptr<test_support::FakeCoreAudioPropertyApi> property_api;
  std::shared_ptr<OutputOnlyRouteQuery> query;
  uint32_t sample_rate;
  uint16_t buffer_size;
};

std::optional<HardwareOutputOnlyRoute> makeHardwareOutputOnlyRoute(std::string& skip_reason) {
  AudioObjectPropertyAddress default_output = {kAudioHardwarePropertyDefaultOutputDevice,
                                               kAudioObjectPropertyScopeGlobal,
                                               kAudioObjectPropertyElementMain};
  AudioDeviceID output_id = 0;
  UInt32 default_size = sizeof(output_id);
  if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &default_output, 0, nullptr,
                                 &default_size, &output_id) != noErr ||
      output_id == 0) {
    skip_reason = "No readable CoreAudio default output";
    return std::nullopt;
  }

  const uint32_t channels = outputChannels(output_id);
  Float64 rate = 0.0;
  UInt32 buffer = 0;
  if (channels < 2 || !readRate(output_id, rate) ||
      !readUInt32(output_id, kAudioDevicePropertyBufferFrameSize, buffer) || buffer == 0 ||
      buffer > 0xffffu || rate > 0xffffu) {
    skip_reason = "Default output lacks a usable stereo rate/buffer route";
    return std::nullopt;
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
  return HardwareOutputOnlyRoute{std::move(property_api), std::move(query),
                                 static_cast<uint32_t>(rate), static_cast<uint16_t>(buffer)};
}
constexpr OSStatus kInjectedOutputStartStatus = static_cast<OSStatus>(-50'001);

OSStatus injectedOutputUnitStart(AudioUnit) {
  return kInjectedOutputStartStatus;
}

TEST(CoreAudioOutputOnlyInjectedTest, NeverTouchesStaleInputDirection) {
  std::string skip_reason;
  const auto setup = makeHardwareOutputOnlyRoute(skip_reason);
  if (!setup.has_value()) {
    GTEST_SKIP() << skip_reason;
  }

  DirectionAudit audit;
  CoreAudioDriver driver(setup->query, setup->property_api, &audit);

  AudioDriverConfig config;
  config.sample_rate = setup->sample_rate;
  config.buffer_size = setup->buffer_size;
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
  EXPECT_TRUE(setup->property_api->writeLedger().empty());

  setup->property_api->clearLedgers();
  setup->property_api->setHogModeAllowed(1);
  setup->property_api->setHogModeSettable(true);
  config.sample_rate_policy = AudioSampleRatePolicy::RequestExactRate;
  ASSERT_EQ(driver.initialize(config), SessionGraphError::OK);
  auto writes = setup->property_api->writeLedger();
  ASSERT_EQ(writes.size(), 1u);
  EXPECT_EQ(writes[0].address.mSelector, kAudioHardwarePropertyHogModeIsAllowed);
  UInt32 disabled_hog_mode = 99;
  ASSERT_EQ(writes[0].bytes.size(), sizeof(disabled_hog_mode));
  std::memcpy(&disabled_hog_mode, writes[0].bytes.data(), sizeof(disabled_hog_mode));
  EXPECT_EQ(disabled_hog_mode, 0u);

  ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
  ASSERT_EQ(driver.stop(), SessionGraphError::OK);
  writes = setup->property_api->writeLedger();
  ASSERT_EQ(writes.size(), 2u);
  UInt32 restored_hog_mode = 0;
  ASSERT_EQ(writes[1].bytes.size(), sizeof(restored_hog_mode));
  std::memcpy(&restored_hog_mode, writes[1].bytes.data(), sizeof(restored_hog_mode));
  EXPECT_EQ(restored_hog_mode, 1u);
}

TEST(CoreAudioOutputOnlyInjectedTest, OutputStartFailurePublishesNativeStatus) {
  std::string skip_reason;
  const auto setup = makeHardwareOutputOnlyRoute(skip_reason);
  if (!setup.has_value()) {
    GTEST_SKIP() << skip_reason;
  }

  DirectionAudit audit;
  CoreAudioDriver driver(setup->query, setup->property_api, &audit, &injectedOutputUnitStart);

  AudioDriverConfig config;
  config.sample_rate = setup->sample_rate;
  config.buffer_size = setup->buffer_size;
  config.num_inputs = 0;
  config.num_outputs = 2;
  config.output_device_id = "injected.output";

  ASSERT_EQ(driver.initialize(config), SessionGraphError::OK);
  OutputCallback callback;
  EXPECT_EQ(driver.start(&callback), SessionGraphError::InternalError);
  EXPECT_FALSE(driver.isRunning());

  const auto telemetry = driver.getTelemetry();
  EXPECT_EQ(telemetry.route_outcome, AudioRouteRuntimeOutcome::BackendFailure);
  EXPECT_EQ(telemetry.route_backend_error, static_cast<int32_t>(kInjectedOutputStartStatus));
  EXPECT_EQ(callback.calls.load(std::memory_order_relaxed), 0u);

  ASSERT_EQ(driver.initialize(config), SessionGraphError::OK);
  EXPECT_EQ(driver.getTelemetry().route_backend_error, 0);
}

TEST(CoreAudioOutputOnlyInjectedTest, OutputRouteRequestUsesCooperativeRatePolicy) {
  std::string skip_reason;
  const auto setup = makeHardwareOutputOnlyRoute(skip_reason);
  if (!setup.has_value()) {
    GTEST_SKIP() << skip_reason;
  }

  DirectionAudit audit;
  CoreAudioDriver driver(setup->query, setup->property_api, &audit);

  AudioOutputRouteRequest request;
  request.output_device_id = "injected.output";
  request.output_channel_map = {0, 1};
  request.requested_sample_rate = setup->sample_rate;
  request.requested_buffer_size = setup->buffer_size;

  ASSERT_EQ(driver.initializeAudioOutput(request), SessionGraphError::OK);
  OutputCallback callback;
  ASSERT_EQ(driver.start(&callback), SessionGraphError::OK);
  ASSERT_EQ(driver.stop(), SessionGraphError::OK);

  const auto& config = driver.getConfig();
  EXPECT_EQ(config.num_inputs, 0);
  EXPECT_TRUE(config.input_device_id.empty());
  EXPECT_TRUE(config.channel_map.input_channels.empty());
  EXPECT_EQ(config.output_device_id, "injected.output");
  EXPECT_EQ(config.channel_map.output_channels, (std::vector<uint16_t>{0, 1}));
  EXPECT_EQ(config.num_outputs, request.output_channel_map.size());
  EXPECT_EQ(config.sample_rate, request.requested_sample_rate);
  EXPECT_EQ(config.buffer_size, request.requested_buffer_size);
  EXPECT_EQ(config.sample_rate_policy, AudioSampleRatePolicy::RequestExactRateOrConvert);
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
  EXPECT_EQ(driver.getTelemetry().route_backend_error, 0);
  EXPECT_TRUE(property_api->writeLedger().empty());
  EXPECT_TRUE(audit.operations.empty());
}

using ControlOperation = detail::CoreAudioDriverControlTestHooks::Operation;
using ControlClock = std::chrono::steady_clock;

struct SyntheticPrimeState {
  CoreAudioDriver* driver{nullptr};
  std::mutex mutex;
  std::condition_variable changed;
  bool paused{false};
  bool release_poll{false};
  size_t arrivals{0};
  std::atomic<bool> observe_controls{false};
  std::array<std::atomic<bool>, 3> saw_busy_mutex{};
  std::atomic<int64_t> elapsed_ns{0};
  std::atomic<uint32_t> polls{0};
  std::atomic<int64_t> cadence_ms{0};

  static ControlClock::time_point now(void* context) {
    auto& state = *static_cast<SyntheticPrimeState*>(context);
    return ControlClock::time_point(std::chrono::nanoseconds(state.elapsed_ns.load()));
  }

  static void pollWait(void* context, std::chrono::milliseconds cadence) {
    auto& state = *static_cast<SyntheticPrimeState*>(context);
    state.polls.fetch_add(1);
    state.cadence_ms.store(cadence.count());
    {
      std::unique_lock<std::mutex> lock(state.mutex);
      state.paused = true;
      state.changed.notify_all();
      state.changed.wait(lock, [&] { return state.release_poll; });
    }
    // Preserve cadence after the deterministic gate opens. This is not the
    // success condition: only actual converter input can satisfy readiness.
    std::this_thread::sleep_for(cadence);
  }

  static void beforeLock(void* context, ControlOperation operation) {
    auto& state = *static_cast<SyntheticPrimeState*>(context);
    if (!state.observe_controls.load() || operation == ControlOperation::Start) {
      return;
    }
    const size_t index = operation == ControlOperation::ActiveRoute ? 0
                         : operation == ControlOperation::Latency   ? 1
                                                                    : 2;
    state.saw_busy_mutex[index].store(CoreAudioDriverTestAccess::mutexBusy(*state.driver));
    std::lock_guard<std::mutex> lock(state.mutex);
    ++state.arrivals;
    state.changed.notify_all();
  }

  void release() {
    std::lock_guard<std::mutex> lock(mutex);
    release_poll = true;
    changed.notify_all();
  }
};

// These are opaque synthetic handles, not AudioComponent instances. Every
// native operation reachable from start/status/stop/destruction is intercepted.
struct SyntheticUnit {
  std::atomic<uint32_t> starts{0};
  std::atomic<uint32_t> stops{0};
  std::atomic<uint32_t> uninitializes{0};
  std::atomic<uint32_t> disposals{0};
  OSStatus start_status{noErr};

  AudioUnit handle() {
    return reinterpret_cast<AudioUnit>(this);
  }
  static SyntheticUnit& from(AudioUnit unit) {
    return *reinterpret_cast<SyntheticUnit*>(unit);
  }
  static OSStatus start(AudioUnit unit) {
    auto& fake = from(unit);
    fake.starts.fetch_add(1);
    return fake.start_status;
  }
  static OSStatus stop(AudioUnit unit) {
    from(unit).stops.fetch_add(1);
    return noErr;
  }
  static OSStatus uninitialize(AudioUnit unit) {
    from(unit).uninitializes.fetch_add(1);
    return noErr;
  }
  static OSStatus dispose(AudioUnit unit) {
    from(unit).disposals.fetch_add(1);
    return noErr;
  }
  static OSStatus getProperty(AudioUnit, AudioUnitPropertyID property, AudioUnitScope,
                              AudioUnitElement, void* data, UInt32* size) {
    if (property != kAudioUnitProperty_Latency || *size != sizeof(Float64)) {
      return kAudioUnitErr_InvalidProperty;
    }
    *static_cast<Float64*>(data) = 0.0;
    return noErr;
  }
};

struct SyntheticStartup {
  SyntheticPrimeState state;
  SyntheticUnit input;
  SyntheticUnit output;
  std::shared_ptr<test_support::FakeCoreAudioPropertyApi> properties =
      std::make_shared<test_support::FakeCoreAudioPropertyApi>();
  CoreAudioDriver driver{std::make_shared<OutputOnlyRouteQuery>(7201, 1, 48000.0), properties};
  OutputCallback callback;

  SessionGraphError prepare(bool controlled_clock) {
    state.driver = &driver;
    for (const AudioDeviceID device : {7201u, 7202u}) {
      const bool input_device = device == 7202;
      const auto scope =
          input_device ? kAudioObjectPropertyScopeInput : kAudioObjectPropertyScopeOutput;
      properties->setAlive(device, 1);
      properties->setRate(device, input_device ? 44100.0 : 48000.0);
      properties->setBuffer(device, 128);
      properties->setDeviceUID(device, input_device ? "synthetic.input" : "synthetic.output");
      properties->setLatency(device, scope, 0);
      properties->setSafetyOffset(device, scope, 0);
      properties->setStreams(device, scope, {device + 100});
      properties->setStreamLatency(device + 100, 0);
    }
    detail::CoreAudioDriverControlTestHooks hooks;
    hooks.context = &state;
    hooks.stopUnit = &SyntheticUnit::stop;
    hooks.uninitializeUnit = &SyntheticUnit::uninitialize;
    hooks.disposeUnit = &SyntheticUnit::dispose;
    hooks.getUnitProperty = &SyntheticUnit::getProperty;
    hooks.primeNow = controlled_clock ? &SyntheticPrimeState::now : nullptr;
    hooks.primePollWait = &SyntheticPrimeState::pollWait;
    hooks.beforeControlLock = &SyntheticPrimeState::beforeLock;
    return CoreAudioDriverTestAccess::prepareConvertedInput(driver, input.handle(), output.handle(),
                                                            hooks, &SyntheticUnit::start);
  }
};

using ControlMeasurements = std::array<std::vector<double>, 5>;

double millisecondsSince(ControlClock::time_point begin) {
  return std::chrono::duration<double, std::milli>(ControlClock::now() - begin).count();
}

void reportControlMeasurements(const char* scenario, ControlMeasurements measurements) {
  struct utsname host{};
  const bool host_available = uname(&host) == 0;
#ifdef NDEBUG
  constexpr const char* build_mode = "Release";
#else
  constexpr const char* build_mode = "Debug";
#endif
  std::cout << "CoreAudio synthetic control distribution scenario=" << scenario
            << " clock=steady_clock rates=input44100/session48000 frames=128"
            << " physical_playback=false end_to_end_bound=false"
            << " host=" << (host_available ? host.nodename : "unavailable")
            << " arch=" << (host_available ? host.machine : "unavailable")
            << " os=" << (host_available ? host.release : "unavailable")
            << " compiler=" << __VERSION__ << " build=" << build_mode;
  std::cout << '\n';
  const std::array<const char*, 5> names = {"start", "getActiveRoute", "getLatencySamples", "stop",
                                            "imposed_prime_pause"};
  for (size_t index = 0; index < measurements.size(); ++index) {
    auto& values = measurements[index];
    if (values.empty()) {
      continue;
    }
    std::sort(values.begin(), values.end());
    const auto percentile = [&](double quantile) {
      return values[static_cast<size_t>(std::ceil(quantile * values.size())) - 1];
    };
    std::cout << "  operation=" << names[index] << " n=" << values.size()
              << " p50_ms=" << percentile(0.50) << " p95_ms=" << percentile(0.95)
              << " p99_ms=" << percentile(0.99) << " max_ms=" << values.back() << '\n';
    const std::string key = std::string(scenario) + "_" + names[index];
    testing::Test::RecordProperty(key + "_sample_count", static_cast<int>(values.size()));
    testing::Test::RecordProperty(key + "_p50_ms", std::to_string(percentile(0.50)));
    testing::Test::RecordProperty(key + "_p95_ms", std::to_string(percentile(0.95)));
    testing::Test::RecordProperty(key + "_p99_ms", std::to_string(percentile(0.99)));
    testing::Test::RecordProperty(key + "_max_ms", std::to_string(values.back()));
  }
}

void measureContendedPrime(bool timeout, bool real_deadline, size_t count,
                           ControlMeasurements& measurements) {
  for (size_t sample = 0; sample < count; ++sample) {
    SCOPED_TRACE(sample);
    SyntheticStartup fixture;
    ASSERT_EQ(fixture.prepare(!real_deadline), SessionGraphError::OK);
    SessionGraphError start_result = SessionGraphError::InternalError;
    SessionGraphError stop_result = SessionGraphError::InternalError;
    ActiveAudioRoute observed_route;
    uint32_t observed_latency = 0;
    double start_ms = 0;
    std::array<double, 3> control_ms{};
    std::atomic<bool> start_done{false};
    std::array<std::atomic<bool>, 3> control_done{};
    std::thread starter([&] {
      const auto begin = ControlClock::now();
      start_result = fixture.driver.start(&fixture.callback);
      start_ms = millisecondsSince(begin);
      start_done.store(true);
    });
    bool paused = false;
    {
      std::unique_lock<std::mutex> lock(fixture.state.mutex);
      paused = fixture.state.changed.wait_for(lock, std::chrono::seconds(5),
                                              [&] { return fixture.state.paused; });
    }
    EXPECT_TRUE(paused);
    if (!paused) {
      fixture.state.release();
      starter.join();
      return;
    }
    EXPECT_FALSE(start_done.load());
    EXPECT_EQ(fixture.input.starts.load(), 1u);
    EXPECT_EQ(fixture.output.starts.load(), 0u);
    EXPECT_TRUE(CoreAudioDriverTestAccess::inputAdmitted(fixture.driver));
    EXPECT_FALSE(CoreAudioDriverTestAccess::outputAdmitted(fixture.driver));
    EXPECT_FALSE(CoreAudioDriverTestAccess::clientAdmitted(fixture.driver));
    fixture.state.observe_controls.store(true);
    std::array<std::thread, 3> controls = {std::thread([&] {
                                             const auto begin = ControlClock::now();
                                             observed_route = fixture.driver.getActiveRoute();
                                             control_ms[0] = millisecondsSince(begin);
                                             control_done[0].store(true);
                                           }),
                                           std::thread([&] {
                                             const auto begin = ControlClock::now();
                                             observed_latency = fixture.driver.getLatencySamples();
                                             control_ms[1] = millisecondsSince(begin);
                                             control_done[1].store(true);
                                           }),
                                           std::thread([&] {
                                             const auto begin = ControlClock::now();
                                             stop_result = fixture.driver.stop();
                                             control_ms[2] = millisecondsSince(begin);
                                             control_done[2].store(true);
                                           })};
    bool arrived = false;
    double pause_ms = 0;
    {
      std::unique_lock<std::mutex> lock(fixture.state.mutex);
      arrived = fixture.state.changed.wait_for(lock, std::chrono::seconds(5),
                                               [&] { return fixture.state.arrivals == 3; });
      // This imposed delay is measurement workload only. Readiness is released
      // explicitly below, after all callers observe the held production mutex.
      const auto begin = ControlClock::now();
      const auto hold_until = begin + std::chrono::milliseconds(sample % 2 == 0 ? 5 : 20);
      fixture.state.changed.wait_until(lock, hold_until, [] { return false; });
      pause_ms = millisecondsSince(begin);
    }
    EXPECT_TRUE(arrived);
    for (size_t index = 0; index < controls.size(); ++index) {
      EXPECT_TRUE(fixture.state.saw_busy_mutex[index].load());
      EXPECT_FALSE(control_done[index].load());
    }
    EXPECT_FALSE(start_done.load());
    EXPECT_EQ(fixture.callback.calls.load(), 0u);
    if (timeout) {
      if (!real_deadline) {
        // Exercise the production two-second comparison without making every
        // distribution sample sleep two seconds. Real wall-deadline evidence
        // is collected separately, not inferred from this synthetic clock.
        fixture.state.elapsed_ns.store(2'000'000'000);
      }
    } else {
      const auto transfer = CoreAudioDriverTestAccess::supplyInput(fixture.driver);
      EXPECT_EQ(transfer.status, audio_utils::DirectionalSrcStatus::Ok);
      EXPECT_EQ(transfer.input_frames_consumed, 256u);
    }
    fixture.state.release();
    starter.join();
    for (auto& control : controls) {
      control.join();
    }
    fixture.state.observe_controls.store(false);
    EXPECT_EQ(start_result, timeout ? SessionGraphError::NotReady : SessionGraphError::OK);
    EXPECT_EQ(stop_result, SessionGraphError::OK);
    EXPECT_FALSE(fixture.driver.isRunning());
    EXPECT_EQ(fixture.callback.calls.load(), 0u);
    EXPECT_EQ(fixture.state.cadence_ms.load(), 1);
    EXPECT_GE(fixture.state.polls.load(), 1u);
    EXPECT_EQ(fixture.properties->listenerCount(), 0u);
    EXPECT_FALSE(CoreAudioDriverTestAccess::inputAdmitted(fixture.driver));
    EXPECT_FALSE(CoreAudioDriverTestAccess::outputAdmitted(fixture.driver));
    EXPECT_FALSE(CoreAudioDriverTestAccess::clientAdmitted(fixture.driver));
    const auto telemetry = fixture.driver.getTelemetry();
    if (timeout) {
      EXPECT_EQ(telemetry.route_outcome, AudioRouteRuntimeOutcome::InputConversionFailed);
      EXPECT_EQ(telemetry.input_conversion_failures, 1u);
      EXPECT_EQ(telemetry.route_backend_error, 0);
      EXPECT_EQ(fixture.output.starts.load(), 0u);
      EXPECT_TRUE(CoreAudioDriverTestAccess::completelyCleaned(fixture.driver));
      EXPECT_TRUE(observed_route.output_device_id.empty());
      EXPECT_EQ(observed_latency, 0u);
      EXPECT_EQ(fixture.input.uninitializes.load(), 1u);
      EXPECT_EQ(fixture.output.uninitializes.load(), 1u);
      EXPECT_EQ(fixture.input.disposals.load(), 1u);
      EXPECT_EQ(fixture.output.disposals.load(), 1u);
      EXPECT_GE(fixture.input.stops.load(), 1u);
      EXPECT_EQ(fixture.driver.start(&fixture.callback), SessionGraphError::NotReady);
      EXPECT_EQ(fixture.driver.getAudioIoRouteState().state, AudioRouteState::Failed);
      if (real_deadline) {
        EXPECT_GE(start_ms, 2000.0);
      } else {
        EXPECT_EQ(fixture.state.polls.load(), 1u);
      }
    } else {
      EXPECT_EQ(telemetry.route_outcome, AudioRouteRuntimeOutcome::Healthy);
      EXPECT_EQ(telemetry.input_conversion_failures, 0u);
      EXPECT_EQ(fixture.output.starts.load(), 1u);
      EXPECT_EQ(observed_route.output_device_id, "synthetic.output");
      EXPECT_GT(observed_latency, 0u);
      EXPECT_EQ(fixture.input.disposals.load(), 0u);
      EXPECT_EQ(fixture.output.disposals.load(), 0u);
      EXPECT_EQ(fixture.state.polls.load(), 1u);
    }
    measurements[0].push_back(start_ms);
    for (size_t index = 0; index < control_ms.size(); ++index) {
      measurements[index + 1].push_back(control_ms[index]);
    }
    measurements[4].push_back(pause_ms);
  }
}

TEST(CoreAudioSyntheticPrimeTest, DelayedInputSerializesControlWaitDistribution) {
  ControlMeasurements measurements;
  measureContendedPrime(false, false, 24, measurements);
  reportControlMeasurements("delayed_input", std::move(measurements));
}

TEST(CoreAudioSyntheticPrimeTest, ControlledTimeoutCleansUpAndSerializesControlWaitDistribution) {
  ControlMeasurements measurements;
  measureContendedPrime(true, false, 24, measurements);
  reportControlMeasurements("controlled_timeout", std::move(measurements));
}

TEST(CoreAudioSyntheticPrimeTest, RealPrimePollDeadlineIsNotAnEndToEndBound) {
  ControlMeasurements measurements;
  measureContendedPrime(true, true, 1, measurements);
  reportControlMeasurements("real_two_second_poll", std::move(measurements));
}

TEST(CoreAudioSyntheticPrimeTest, InputAndOutputStartFailuresPublishBackendStatusWithoutPlayback) {
  for (const bool fail_input : {true, false}) {
    SCOPED_TRACE(fail_input);
    SyntheticStartup fixture;
    ASSERT_EQ(fixture.prepare(true), SessionGraphError::OK);
    (fail_input ? fixture.input : fixture.output).start_status = kInjectedOutputStartStatus;
    if (!fail_input) {
      ASSERT_EQ(CoreAudioDriverTestAccess::supplyInput(fixture.driver).status,
                audio_utils::DirectionalSrcStatus::Ok);
    }
    EXPECT_EQ(fixture.driver.start(&fixture.callback), SessionGraphError::InternalError);
    EXPECT_FALSE(fixture.driver.isRunning());
    const auto telemetry = fixture.driver.getTelemetry();
    EXPECT_EQ(telemetry.route_outcome, AudioRouteRuntimeOutcome::BackendFailure);
    EXPECT_EQ(telemetry.route_backend_error, kInjectedOutputStartStatus);
    EXPECT_EQ(telemetry.input_conversion_failures, 0u);
    EXPECT_EQ(fixture.callback.calls.load(), 0u);
    EXPECT_EQ(fixture.properties->listenerCount(), 0u);
    EXPECT_FALSE(CoreAudioDriverTestAccess::inputAdmitted(fixture.driver));
    EXPECT_FALSE(CoreAudioDriverTestAccess::outputAdmitted(fixture.driver));
    EXPECT_FALSE(CoreAudioDriverTestAccess::clientAdmitted(fixture.driver));
    EXPECT_EQ(fixture.input.starts.load(), 1u);
    EXPECT_EQ(fixture.output.starts.load(), fail_input ? 0u : 1u);
    EXPECT_EQ(fixture.driver.start(&fixture.callback), SessionGraphError::NotReady);
  }
}

} // namespace
} // namespace orpheus
#endif
