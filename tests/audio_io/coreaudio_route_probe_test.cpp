// SPDX-License-Identifier: MIT
#include "coreaudio/coreaudio_driver.h"
#include "coreaudio/coreaudio_route_resolver.h"

#include <gtest/gtest.h>

#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace orpheus {
namespace {

using detail::CoreAudioEndpointRange;
using detail::CoreAudioRouteQueryResult;
using detail::CoreAudioRouteQueryStatus;
using detail::ICoreAudioRouteQuery;
using detail::ResolvedCoreAudioEndpoint;

struct FakeLedger {
  int default_input_reads = 0;
  int default_output_reads = 0;
  int uid_reads = 0;
  int input_channel_reads = 0;
  int output_channel_reads = 0;
  int sample_rate_range_reads = 0;
  int current_rate_reads = 0;
  int running_reads = 0;
  int property_writes = 0;
  int aggregate_requests = 0;
  int auhal_actions = 0;
  int listener_registrations = 0;
  int io_starts = 0;
  int tcc_requests = 0;
  int driver_state_mutations = 0;
};

struct FakeEndpoint {
  AudioDeviceID device_id = 0;
  std::string device_uid;
  uint32_t input_channels = 0;
  uint32_t output_channels = 0;
  CoreAudioRouteQueryStatus input_channel_status = CoreAudioRouteQueryStatus::Success;
  CoreAudioRouteQueryStatus output_channel_status = CoreAudioRouteQueryStatus::Success;
  std::vector<CoreAudioEndpointRange> sample_rate_ranges;
  CoreAudioRouteQueryStatus sample_rate_status = CoreAudioRouteQueryStatus::Success;
  double current_sample_rate = 48000.0;
  CoreAudioRouteQueryStatus current_rate_status = CoreAudioRouteQueryStatus::Success;
  bool is_running_somewhere = false;
  CoreAudioRouteQueryStatus running_status = CoreAudioRouteQueryStatus::Success;
};

template <typename T>
CoreAudioRouteQueryResult<T> fakeResult(CoreAudioRouteQueryStatus status, T value = {}) {
  return {status, std::move(value)};
}

class FakeCoreAudioRouteQuery final : public ICoreAudioRouteQuery {
public:
  CoreAudioRouteQueryResult<ResolvedCoreAudioEndpoint> resolveDefault(bool output) const override {
    if (output) {
      ++ledger.default_output_reads;
      if (default_output_status != CoreAudioRouteQueryStatus::Success) {
        return fakeResult<ResolvedCoreAudioEndpoint>(default_output_status);
      }
      return endpointFor(default_output);
    }
    ++ledger.default_input_reads;
    if (default_input_status != CoreAudioRouteQueryStatus::Success) {
      return fakeResult<ResolvedCoreAudioEndpoint>(default_input_status);
    }
    return endpointFor(default_input);
  }

  CoreAudioRouteQueryResult<ResolvedCoreAudioEndpoint>
  resolveDeviceUID(std::string_view device_uid) const override {
    ++ledger.uid_reads;
    const auto status = uid_status.find(std::string(device_uid));
    if (status != uid_status.end() && status->second != CoreAudioRouteQueryStatus::Success) {
      return fakeResult<ResolvedCoreAudioEndpoint>(status->second);
    }
    for (const auto& [id, endpoint] : endpoints) {
      if (endpoint.device_uid == device_uid) {
        return fakeResult(CoreAudioRouteQueryStatus::Success,
                          ResolvedCoreAudioEndpoint{id, endpoint.device_uid});
      }
    }
    return fakeResult<ResolvedCoreAudioEndpoint>(CoreAudioRouteQueryStatus::Missing);
  }

  CoreAudioRouteQueryResult<uint32_t> channelCount(AudioDeviceID device_id,
                                                   AudioObjectPropertyScope scope) const override {
    const auto endpoint = endpoints.find(device_id);
    if (scope == kAudioObjectPropertyScopeOutput) {
      ++ledger.output_channel_reads;
    } else {
      ++ledger.input_channel_reads;
    }
    if (endpoint == endpoints.end()) {
      return fakeResult<uint32_t>(CoreAudioRouteQueryStatus::Missing);
    }
    if (scope == kAudioObjectPropertyScopeOutput) {
      return fakeResult(endpoint->second.output_channel_status, endpoint->second.output_channels);
    }
    return fakeResult(endpoint->second.input_channel_status, endpoint->second.input_channels);
  }

  CoreAudioRouteQueryResult<std::vector<CoreAudioEndpointRange>>
  advertisedSampleRateRanges(AudioDeviceID device_id) const override {
    ++ledger.sample_rate_range_reads;
    const auto endpoint = endpoints.find(device_id);
    if (endpoint == endpoints.end()) {
      return fakeResult<std::vector<CoreAudioEndpointRange>>(CoreAudioRouteQueryStatus::Missing);
    }
    return fakeResult(endpoint->second.sample_rate_status, endpoint->second.sample_rate_ranges);
  }

  CoreAudioRouteQueryResult<double> currentSampleRate(AudioDeviceID device_id) const override {
    ++ledger.current_rate_reads;
    const auto endpoint = endpoints.find(device_id);
    if (endpoint == endpoints.end()) {
      return fakeResult<double>(CoreAudioRouteQueryStatus::Missing);
    }
    return fakeResult(endpoint->second.current_rate_status, endpoint->second.current_sample_rate);
  }

  CoreAudioRouteQueryResult<bool> isRunningSomewhere(AudioDeviceID device_id) const override {
    ++ledger.running_reads;
    const auto endpoint = endpoints.find(device_id);
    if (endpoint == endpoints.end()) {
      return fakeResult<bool>(CoreAudioRouteQueryStatus::Missing);
    }
    return fakeResult(endpoint->second.running_status, endpoint->second.is_running_somewhere);
  }

  void resetLedger() const {
    ledger = {};
  }

  mutable FakeLedger ledger;
  std::map<AudioDeviceID, FakeEndpoint> endpoints;
  std::map<std::string, CoreAudioRouteQueryStatus> uid_status;
  AudioDeviceID default_input = 0;
  AudioDeviceID default_output = 0;
  CoreAudioRouteQueryStatus default_input_status = CoreAudioRouteQueryStatus::Success;
  CoreAudioRouteQueryStatus default_output_status = CoreAudioRouteQueryStatus::Success;

private:
  CoreAudioRouteQueryResult<ResolvedCoreAudioEndpoint> endpointFor(AudioDeviceID device_id) const {
    const auto endpoint = endpoints.find(device_id);
    if (endpoint == endpoints.end()) {
      return fakeResult<ResolvedCoreAudioEndpoint>(CoreAudioRouteQueryStatus::Missing);
    }
    return fakeResult(CoreAudioRouteQueryStatus::Success,
                      ResolvedCoreAudioEndpoint{device_id, endpoint->second.device_uid});
  }
};

AudioDriverConfig duplexConfig() {
  AudioDriverConfig config;
  config.sample_rate = 48000;
  config.buffer_size = 512;
  config.num_inputs = 2;
  config.num_outputs = 2;
  return config;
}

void installSameDevice(FakeCoreAudioRouteQuery& fake) {
  FakeEndpoint endpoint;
  endpoint.device_id = 11;
  endpoint.device_uid = "shared";
  endpoint.input_channels = 2;
  endpoint.output_channels = 2;
  endpoint.sample_rate_ranges = {{44100.0, 96000.0}};
  endpoint.current_sample_rate = 48000.0;
  endpoint.is_running_somewhere = true;
  fake.endpoints.emplace(endpoint.device_id, endpoint);
  fake.default_input = endpoint.device_id;
  fake.default_output = endpoint.device_id;
}

void expectNoProhibitedActions(const FakeLedger& ledger) {
  EXPECT_EQ(ledger.property_writes, 0);
  EXPECT_EQ(ledger.aggregate_requests, 0);
  EXPECT_EQ(ledger.auhal_actions, 0);
  EXPECT_EQ(ledger.listener_registrations, 0);
  EXPECT_EQ(ledger.io_starts, 0);
  EXPECT_EQ(ledger.tcc_requests, 0);
  EXPECT_EQ(ledger.driver_state_mutations, 0);
}

void expectFailureDetail(const AudioRouteCompatibility& compatibility) {
  EXPECT_LE(compatibility.detail.size(), 96u);
  for (const unsigned char character : compatibility.detail) {
    EXPECT_LT(character, 128u);
  }
  EXPECT_TRUE(compatibility.detail.find(':') != std::string::npos);
}

TEST(CoreAudioRouteProbeTest, SameDeviceDefaultsAreCompatibleAndGlobalReadsAreDeduplicated) {
  auto fake = std::make_shared<FakeCoreAudioRouteQuery>();
  installSameDevice(*fake);
  CoreAudioDriver driver(fake);

  const AudioRouteCompatibility compatibility = driver.probeRoute(duplexConfig());

  EXPECT_EQ(compatibility.status, AudioRouteCompatibilityStatus::Compatible);
  EXPECT_EQ(compatibility.resolved_input_device_id, "shared");
  EXPECT_EQ(compatibility.resolved_output_device_id, "shared");
  EXPECT_EQ(compatibility.current_input_sample_rate, 48000u);
  EXPECT_EQ(compatibility.current_output_sample_rate, 48000u);
  EXPECT_FALSE(compatibility.input_rate_change_required);
  EXPECT_FALSE(compatibility.output_rate_change_required);
  EXPECT_TRUE(compatibility.input_is_running_somewhere);
  EXPECT_TRUE(compatibility.output_is_running_somewhere);
  EXPECT_TRUE(compatibility.detail.empty());

  EXPECT_EQ(fake->ledger.default_input_reads, 1);
  EXPECT_EQ(fake->ledger.default_output_reads, 1);
  EXPECT_EQ(fake->ledger.uid_reads, 0);
  EXPECT_EQ(fake->ledger.input_channel_reads, 1);
  EXPECT_EQ(fake->ledger.output_channel_reads, 1);
  EXPECT_EQ(fake->ledger.sample_rate_range_reads, 1);
  EXPECT_EQ(fake->ledger.current_rate_reads, 1);
  EXPECT_EQ(fake->ledger.running_reads, 1);
  expectNoProhibitedActions(fake->ledger);
}

TEST(CoreAudioRouteProbeTest, RatePolicyClassifiesSupportedMismatchWithoutMutatingRate) {
  auto fake = std::make_shared<FakeCoreAudioRouteQuery>();
  installSameDevice(*fake);
  fake->endpoints.at(11).current_sample_rate = 44100.0;
  CoreAudioDriver driver(fake);

  AudioDriverConfig preserve = duplexConfig();
  const auto preserved = driver.probeRoute(preserve);
  EXPECT_EQ(preserved.status, AudioRouteCompatibilityStatus::Compatible);
  EXPECT_EQ(preserved.current_input_sample_rate, 44100u);
  EXPECT_EQ(preserved.current_output_sample_rate, 44100u);
  EXPECT_TRUE(preserved.input_rate_change_required);
  EXPECT_TRUE(preserved.output_rate_change_required);
  EXPECT_TRUE(preserved.detail.empty());

  fake->resetLedger();
  AudioDriverConfig exact = duplexConfig();
  exact.sample_rate_policy = AudioSampleRatePolicy::RequestExactRate;
  const auto requested = driver.probeRoute(exact);
  EXPECT_EQ(requested.status, AudioRouteCompatibilityStatus::RequiresSampleRateChange);
  EXPECT_EQ(requested.current_input_sample_rate, 44100u);
  EXPECT_EQ(requested.current_output_sample_rate, 44100u);
  EXPECT_TRUE(requested.input_rate_change_required);
  EXPECT_TRUE(requested.output_rate_change_required);
  EXPECT_TRUE(requested.detail.empty());
  expectNoProhibitedActions(fake->ledger);
}

TEST(CoreAudioRouteProbeTest, ExplicitUidDoesNotFallBackAndDirectionMismatchIsUnavailable) {
  auto fake = std::make_shared<FakeCoreAudioRouteQuery>();
  installSameDevice(*fake);
  CoreAudioDriver driver(fake);

  AudioDriverConfig missing = duplexConfig();
  missing.num_inputs = 0;
  missing.output_device_id = "missing-output";
  auto compatibility = driver.probeRoute(missing);
  EXPECT_EQ(compatibility.status, AudioRouteCompatibilityStatus::OutputUnavailable);
  EXPECT_EQ(fake->ledger.default_output_reads, 0);
  EXPECT_EQ(fake->ledger.uid_reads, 1);
  EXPECT_EQ(fake->ledger.output_channel_reads, 0);

  fake->resetLedger();
  FakeEndpoint input_only = fake->endpoints.at(11);
  input_only.device_id = 12;
  input_only.device_uid = "input-only";
  input_only.output_channels = 0;
  fake->endpoints.emplace(input_only.device_id, input_only);
  AudioDriverConfig incompatible = duplexConfig();
  incompatible.num_inputs = 0;
  incompatible.output_device_id = "input-only";
  compatibility = driver.probeRoute(incompatible);
  EXPECT_EQ(compatibility.status, AudioRouteCompatibilityStatus::OutputUnavailable);
  EXPECT_EQ(fake->ledger.default_output_reads, 0);
  EXPECT_EQ(fake->ledger.uid_reads, 1);
  EXPECT_EQ(fake->ledger.output_channel_reads, 1);
  expectFailureDetail(compatibility);
}

TEST(CoreAudioRouteProbeTest, OutputOnlyIgnoresStaleInputUidMapAndPermissionState) {
  auto fake = std::make_shared<FakeCoreAudioRouteQuery>();
  installSameDevice(*fake);
  fake->default_input_status = CoreAudioRouteQueryStatus::PermissionDenied;
  CoreAudioDriver driver(fake);

  AudioDriverConfig config = duplexConfig();
  config.num_inputs = 0;
  config.input_device_id = "stale-input";
  config.channel_map.input_channels = {99, 99};
  const auto compatibility = driver.probeRoute(config);

  EXPECT_EQ(compatibility.status, AudioRouteCompatibilityStatus::Compatible);
  EXPECT_EQ(compatibility.resolved_input_device_id, "");
  EXPECT_EQ(compatibility.current_input_sample_rate, 0u);
  EXPECT_FALSE(compatibility.input_rate_change_required);
  EXPECT_FALSE(compatibility.input_is_running_somewhere);
  EXPECT_EQ(fake->ledger.default_input_reads, 0);
  EXPECT_EQ(fake->ledger.input_channel_reads, 0);
  EXPECT_EQ(fake->ledger.sample_rate_range_reads, 1);
  EXPECT_EQ(fake->ledger.current_rate_reads, 1);
  EXPECT_EQ(fake->ledger.running_reads, 1);
  expectNoProhibitedActions(fake->ledger);
}

TEST(CoreAudioRouteProbeTest, InvalidStaticConfigurationPrecedesEveryEndpointQuery) {
  auto fake = std::make_shared<FakeCoreAudioRouteQuery>();
  installSameDevice(*fake);
  CoreAudioDriver driver(fake);

  AudioDriverConfig no_output = duplexConfig();
  no_output.num_outputs = 0;
  no_output.sample_rate = 0;
  no_output.buffer_size = 0;
  auto compatibility = driver.probeRoute(no_output);
  EXPECT_EQ(compatibility.status, AudioRouteCompatibilityStatus::OutputUnavailable);
  expectFailureDetail(compatibility);
  EXPECT_EQ(fake->ledger.default_input_reads, 0);
  EXPECT_EQ(fake->ledger.default_output_reads, 0);
  EXPECT_EQ(fake->ledger.uid_reads, 0);

  fake->resetLedger();
  AudioDriverConfig invalid_rate = duplexConfig();
  invalid_rate.sample_rate = 0;
  compatibility = driver.probeRoute(invalid_rate);
  EXPECT_EQ(compatibility.status, AudioRouteCompatibilityStatus::BackendFailure);
  EXPECT_EQ(fake->ledger.default_input_reads, 0);
  EXPECT_EQ(fake->ledger.default_output_reads, 0);
  EXPECT_EQ(fake->ledger.uid_reads, 0);
  expectFailureDetail(compatibility);
}

TEST(CoreAudioRouteProbeTest, InvalidMapsAreClassifiedAfterBothEndpointsResolve) {
  auto fake = std::make_shared<FakeCoreAudioRouteQuery>();
  installSameDevice(*fake);
  CoreAudioDriver driver(fake);

  AudioDriverConfig output_map = duplexConfig();
  output_map.channel_map.output_channels = {0, 0};
  auto compatibility = driver.probeRoute(output_map);
  EXPECT_EQ(compatibility.status, AudioRouteCompatibilityStatus::InvalidChannelMap);
  EXPECT_EQ(compatibility.detail, "map:output");
  EXPECT_EQ(fake->ledger.input_channel_reads, 1);
  EXPECT_EQ(fake->ledger.output_channel_reads, 1);
  EXPECT_EQ(fake->ledger.sample_rate_range_reads, 0);

  fake->resetLedger();
  AudioDriverConfig input_map = duplexConfig();
  input_map.channel_map.input_channels = {2, 0};
  compatibility = driver.probeRoute(input_map);
  EXPECT_EQ(compatibility.status, AudioRouteCompatibilityStatus::InvalidChannelMap);
  EXPECT_EQ(compatibility.detail, "map:input");
  EXPECT_EQ(fake->ledger.input_channel_reads, 1);
  EXPECT_EQ(fake->ledger.output_channel_reads, 1);
  EXPECT_EQ(fake->ledger.sample_rate_range_reads, 0);
}

TEST(CoreAudioRouteProbeTest, UnsupportedRateSkipsCurrentAndRunningQueries) {
  auto fake = std::make_shared<FakeCoreAudioRouteQuery>();
  installSameDevice(*fake);
  fake->endpoints.at(11).sample_rate_ranges = {{44100.0, 44100.0}};
  CoreAudioDriver driver(fake);

  AudioDriverConfig config = duplexConfig();
  config.sample_rate = 48000;
  const auto compatibility = driver.probeRoute(config);

  EXPECT_EQ(compatibility.status, AudioRouteCompatibilityStatus::SampleRateUnsupported);
  EXPECT_EQ(compatibility.current_input_sample_rate, 0u);
  EXPECT_EQ(compatibility.current_output_sample_rate, 0u);
  EXPECT_EQ(fake->ledger.sample_rate_range_reads, 1);
  EXPECT_EQ(fake->ledger.current_rate_reads, 0);
  EXPECT_EQ(fake->ledger.running_reads, 0);
  expectFailureDetail(compatibility);
}

TEST(CoreAudioRouteProbeTest, PermissionDeniedInputStopsBeforeInputFacts) {
  auto fake = std::make_shared<FakeCoreAudioRouteQuery>();
  installSameDevice(*fake);
  fake->default_input_status = CoreAudioRouteQueryStatus::PermissionDenied;
  CoreAudioDriver driver(fake);

  const auto compatibility = driver.probeRoute(duplexConfig());

  EXPECT_EQ(compatibility.status, AudioRouteCompatibilityStatus::PermissionDenied);
  EXPECT_EQ(compatibility.detail, "resolve:input");
  EXPECT_EQ(fake->ledger.default_output_reads, 1);
  EXPECT_EQ(fake->ledger.default_input_reads, 1);
  EXPECT_EQ(fake->ledger.output_channel_reads, 1);
  EXPECT_EQ(fake->ledger.input_channel_reads, 0);
  EXPECT_EQ(fake->ledger.sample_rate_range_reads, 0);
  expectFailureDetail(compatibility);
}

TEST(CoreAudioRouteProbeTest, RejectsMalformedCurrentRateWithBoundedDetail) {
  auto fake = std::make_shared<FakeCoreAudioRouteQuery>();
  installSameDevice(*fake);
  fake->endpoints.at(11).current_sample_rate = 48000.002;
  CoreAudioDriver driver(fake);

  const auto compatibility = driver.probeRoute(duplexConfig());

  EXPECT_EQ(compatibility.status, AudioRouteCompatibilityStatus::BackendFailure);
  EXPECT_EQ(compatibility.detail, "current_rate:output");
  EXPECT_EQ(fake->ledger.current_rate_reads, 1);
  EXPECT_EQ(fake->ledger.running_reads, 0);
  expectFailureDetail(compatibility);
}

TEST(CoreAudioRouteProbeTest,
     ResolverMarksSupportedRoutesAsResolvedAndAggregatesOnlyDistinctDevices) {
  auto fake = std::make_shared<FakeCoreAudioRouteQuery>();
  installSameDevice(*fake);
  detail::CoreAudioRouteResolver resolver(fake);

  auto same_device = resolver.resolve(duplexConfig());
  EXPECT_TRUE(same_device.resolved);
  EXPECT_FALSE(same_device.requires_private_aggregate);
  EXPECT_EQ(same_device.input_device_id, 11u);
  EXPECT_EQ(same_device.output_device_id, 11u);
  EXPECT_EQ(same_device.input_channel_map, (std::vector<uint16_t>{0, 1}));
  EXPECT_EQ(same_device.output_channel_map, (std::vector<uint16_t>{0, 1}));

  fake->resetLedger();
  FakeEndpoint distinct_input = fake->endpoints.at(11);
  distinct_input.device_id = 12;
  distinct_input.device_uid = "input";
  distinct_input.output_channels = 0;
  fake->endpoints.emplace(12, distinct_input);
  fake->default_input = 12;
  auto distinct = resolver.resolve(duplexConfig());
  EXPECT_TRUE(distinct.resolved);
  EXPECT_TRUE(distinct.requires_private_aggregate);
  EXPECT_EQ(distinct.input_device_id, 12u);
  EXPECT_EQ(distinct.output_device_id, 11u);
  EXPECT_EQ(fake->ledger.sample_rate_range_reads, 2);
  EXPECT_EQ(fake->ledger.current_rate_reads, 2);
  EXPECT_EQ(fake->ledger.running_reads, 2);
}

TEST(CoreAudioRouteProbeTest, ProbeDoesNotMutateCoreAudioDriverState) {
  auto fake = std::make_shared<FakeCoreAudioRouteQuery>();
  installSameDevice(*fake);
  CoreAudioDriver driver(fake);
  const AudioDriverConfig before_config = driver.getConfig();
  const ActiveAudioRoute before_route = driver.getActiveRoute();
  const AudioIoRouteState before_state = driver.getAudioIoRouteState();

  AudioDriverConfig request = duplexConfig();
  request.output_device_id = "shared";
  request.input_device_id = "shared";
  request.channel_map.output_channels = {1, 0};
  const auto compatibility = driver.probeRoute(request);
  ASSERT_EQ(compatibility.status, AudioRouteCompatibilityStatus::Compatible);

  const auto& after_config = driver.getConfig();
  EXPECT_EQ(after_config.sample_rate, before_config.sample_rate);
  EXPECT_EQ(after_config.buffer_size, before_config.buffer_size);
  EXPECT_EQ(after_config.num_inputs, before_config.num_inputs);
  EXPECT_EQ(after_config.num_outputs, before_config.num_outputs);
  EXPECT_EQ(after_config.input_device_id, before_config.input_device_id);
  EXPECT_EQ(after_config.output_device_id, before_config.output_device_id);
  EXPECT_EQ(after_config.channel_map.input_channels, before_config.channel_map.input_channels);
  EXPECT_EQ(after_config.channel_map.output_channels, before_config.channel_map.output_channels);
  EXPECT_EQ(after_config.sample_rate_policy, before_config.sample_rate_policy);

  const auto after_route = driver.getActiveRoute();
  EXPECT_EQ(after_route.input_device_id, before_route.input_device_id);
  EXPECT_EQ(after_route.output_device_id, before_route.output_device_id);
  EXPECT_EQ(after_route.input_channels, before_route.input_channels);
  EXPECT_EQ(after_route.output_channels, before_route.output_channels);
  EXPECT_EQ(after_route.requested_sample_rate, before_route.requested_sample_rate);
  EXPECT_EQ(after_route.actual_sample_rate, before_route.actual_sample_rate);

  const auto after_state = driver.getAudioIoRouteState();
  EXPECT_EQ(after_state.state, before_state.state);
  EXPECT_EQ(after_state.selected_input_device_id, before_state.selected_input_device_id);
  EXPECT_EQ(after_state.selected_output_device_id, before_state.selected_output_device_id);
  EXPECT_EQ(after_state.active_input_device_id, before_state.active_input_device_id);
  EXPECT_EQ(after_state.active_output_device_id, before_state.active_output_device_id);
  EXPECT_EQ(after_state.requested_sample_rate, before_state.requested_sample_rate);
  EXPECT_EQ(after_state.actual_sample_rate, before_state.actual_sample_rate);
  expectNoProhibitedActions(fake->ledger);
}

} // namespace
} // namespace orpheus
