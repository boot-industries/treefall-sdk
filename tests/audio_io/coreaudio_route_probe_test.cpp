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
using detail::CoreAudioRouteResolver;
using detail::CoreAudioStreamFormat;
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
  int transport_reads = 0;
  int related_reads = 0;
  int physical_format_reads = 0;
  int virtual_format_reads = 0;
  int settable_reads = 0;
  int physical_channel_reads = 0;
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
  uint32_t transport_type = kAudioDeviceTransportTypeBuiltIn;
  std::vector<AudioDeviceID> related_device_ids;
  CoreAudioStreamFormat input_physical_format{48000, 0};
  CoreAudioStreamFormat output_physical_format{48000, 0};
  CoreAudioStreamFormat input_virtual_format{48000, 0};
  CoreAudioStreamFormat output_virtual_format{48000, 0};
  CoreAudioRouteQueryStatus transport_status = CoreAudioRouteQueryStatus::Success;
  CoreAudioRouteQueryStatus related_status = CoreAudioRouteQueryStatus::Success;
  CoreAudioRouteQueryStatus physical_format_status = CoreAudioRouteQueryStatus::Success;
  CoreAudioRouteQueryStatus virtual_format_status = CoreAudioRouteQueryStatus::Success;
  CoreAudioRouteQueryStatus settable_status = CoreAudioRouteQueryStatus::Success;
  bool nominal_sample_rate_settable = true;
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
  CoreAudioRouteQueryResult<uint32_t> transportType(AudioDeviceID device_id) const override {
    ++ledger.transport_reads;
    const auto endpoint = endpoints.find(device_id);
    if (endpoint == endpoints.end()) {
      return fakeResult<uint32_t>(CoreAudioRouteQueryStatus::Missing);
    }
    return fakeResult(endpoint->second.transport_status, endpoint->second.transport_type);
  }

  CoreAudioRouteQueryResult<std::vector<AudioDeviceID>>
  relatedDeviceIDs(AudioDeviceID device_id) const override {
    ++ledger.related_reads;
    const auto endpoint = endpoints.find(device_id);
    if (endpoint == endpoints.end()) {
      return fakeResult<std::vector<AudioDeviceID>>(CoreAudioRouteQueryStatus::Missing);
    }
    return fakeResult(endpoint->second.related_status, endpoint->second.related_device_ids);
  }

  CoreAudioRouteQueryResult<CoreAudioStreamFormat>
  physicalStreamFormat(AudioDeviceID device_id, AudioObjectPropertyScope scope) const override {
    ++ledger.physical_format_reads;
    const auto endpoint = endpoints.find(device_id);
    if (endpoint == endpoints.end()) {
      return fakeResult<CoreAudioStreamFormat>(CoreAudioRouteQueryStatus::Missing);
    }
    return fakeResult(endpoint->second.physical_format_status,
                      scope == kAudioObjectPropertyScopeOutput
                          ? endpoint->second.output_physical_format
                          : endpoint->second.input_physical_format);
  }

  CoreAudioRouteQueryResult<CoreAudioStreamFormat>
  virtualStreamFormat(AudioDeviceID device_id, AudioObjectPropertyScope scope) const override {
    ++ledger.virtual_format_reads;
    const auto endpoint = endpoints.find(device_id);
    if (endpoint == endpoints.end()) {
      return fakeResult<CoreAudioStreamFormat>(CoreAudioRouteQueryStatus::Missing);
    }
    return fakeResult(endpoint->second.virtual_format_status,
                      scope == kAudioObjectPropertyScopeOutput
                          ? endpoint->second.output_virtual_format
                          : endpoint->second.input_virtual_format);
  }

  CoreAudioRouteQueryResult<bool>
  nominalSampleRateSettable(AudioDeviceID device_id) const override {
    ++ledger.settable_reads;
    const auto endpoint = endpoints.find(device_id);
    if (endpoint == endpoints.end()) {
      return fakeResult<bool>(CoreAudioRouteQueryStatus::Missing);
    }
    return fakeResult(endpoint->second.settable_status,
                      endpoint->second.nominal_sample_rate_settable);
  }

  CoreAudioRouteQueryResult<uint32_t>
  physicalChannelCount(AudioDeviceID device_id, AudioObjectPropertyScope scope) const override {
    ++ledger.physical_channel_reads;
    if (scope == kAudioObjectPropertyScopeOutput) {
      ++ledger.output_channel_reads;
    } else {
      ++ledger.input_channel_reads;
    }
    const auto endpoint = endpoints.find(device_id);
    if (endpoint == endpoints.end()) {
      return fakeResult<uint32_t>(CoreAudioRouteQueryStatus::Missing);
    }
    if (scope == kAudioObjectPropertyScopeOutput) {
      return fakeResult(endpoint->second.output_channel_status, endpoint->second.output_channels);
    }
    return fakeResult(endpoint->second.input_channel_status, endpoint->second.input_channels);
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
  endpoint.related_device_ids = {endpoint.device_id};
  endpoint.input_physical_format = {48000, 2};
  endpoint.output_physical_format = {48000, 2};
  endpoint.input_virtual_format = {48000, 2};
  endpoint.output_virtual_format = {48000, 2};
  fake.endpoints.emplace(endpoint.device_id, endpoint);
  fake.default_input = endpoint.device_id;
  fake.default_output = endpoint.device_id;
}

FakeEndpoint makeEndpoint(AudioDeviceID device_id, std::string uid, uint32_t input_channels,
                          uint32_t output_channels, uint32_t current_rate, uint32_t transport,
                          std::vector<CoreAudioEndpointRange> ranges,
                          std::vector<AudioDeviceID> related = {}) {
  FakeEndpoint endpoint;
  endpoint.device_id = device_id;
  endpoint.device_uid = std::move(uid);
  endpoint.input_channels = input_channels;
  endpoint.output_channels = output_channels;
  endpoint.sample_rate_ranges = std::move(ranges);
  endpoint.current_sample_rate = static_cast<double>(current_rate);
  endpoint.transport_type = transport;
  endpoint.related_device_ids =
      related.empty() ? std::vector<AudioDeviceID>{device_id} : std::move(related);
  endpoint.input_physical_format = {current_rate, static_cast<uint16_t>(input_channels)};
  endpoint.output_physical_format = {current_rate, static_cast<uint16_t>(output_channels)};
  endpoint.input_virtual_format = {current_rate, static_cast<uint16_t>(input_channels)};
  endpoint.output_virtual_format = {current_rate, static_cast<uint16_t>(output_channels)};
  return endpoint;
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

TEST(CoreAudioRouteProbeTest, OutputOnlyBuiltInSafeWritePlansRequestedRate) {
  auto fake = std::make_shared<FakeCoreAudioRouteQuery>();
  fake->endpoints.emplace(40, makeEndpoint(40, "built-in.output", 0, 2, 44100,
                                           kAudioDeviceTransportTypeBuiltIn,
                                           {{44100.0, 44100.0}, {48000.0, 48000.0}}));
  CoreAudioRouteResolver resolver(fake);

  auto config = duplexConfig();
  config.num_inputs = 0;
  config.input_device_id = "stale-input";
  config.channel_map.input_channels = {99, 99};
  config.output_device_id = "built-in.output";
  config.channel_map.output_channels = {0, 1};
  config.sample_rate_policy = AudioSampleRatePolicy::RequestExactRateOrConvert;

  const auto route = resolver.resolve(config, true);

  ASSERT_TRUE(route.resolved);
  ASSERT_EQ(route.compatibility.status, AudioRouteCompatibilityStatus::Compatible);
  ASSERT_EQ(route.device_rate_plans.size(), 1u);
  const auto& output_plan = route.device_rate_plans.front();
  ASSERT_TRUE(output_plan.requested_write_rate.has_value());
  EXPECT_EQ(*output_plan.requested_write_rate, 48000u);
  EXPECT_FALSE(route.compatibility.output_conversion_required);
  EXPECT_FALSE(output_plan.output_uses_external_src);
  EXPECT_EQ(fake->ledger.default_input_reads, 0);
  EXPECT_EQ(fake->ledger.input_channel_reads, 0);
}

TEST(CoreAudioRouteProbeTest, OutputOnlyBusyBuiltInUsesExternalSrc) {
  auto fake = std::make_shared<FakeCoreAudioRouteQuery>();
  auto endpoint = makeEndpoint(40, "built-in.output", 0, 2, 44100, kAudioDeviceTransportTypeBuiltIn,
                               {{44100.0, 44100.0}, {48000.0, 48000.0}});
  endpoint.is_running_somewhere = true;
  fake->endpoints.emplace(endpoint.device_id, std::move(endpoint));
  CoreAudioRouteResolver resolver(fake);

  auto config = duplexConfig();
  config.num_inputs = 0;
  config.input_device_id = "stale-input";
  config.channel_map.input_channels = {99, 99};
  config.output_device_id = "built-in.output";
  config.channel_map.output_channels = {0, 1};
  config.sample_rate_policy = AudioSampleRatePolicy::RequestExactRateOrConvert;

  const auto route = resolver.resolve(config, true);

  ASSERT_TRUE(route.resolved);
  ASSERT_EQ(route.compatibility.status, AudioRouteCompatibilityStatus::Compatible);
  ASSERT_EQ(route.device_rate_plans.size(), 1u);
  const auto& output_plan = route.device_rate_plans.front();
  EXPECT_FALSE(output_plan.requested_write_rate.has_value());
  EXPECT_TRUE(route.compatibility.output_conversion_required);
  EXPECT_TRUE(output_plan.output_uses_external_src);
  EXPECT_EQ(route.compatibility.planned_output_client_rate, 44100u);
  EXPECT_EQ(fake->ledger.default_input_reads, 0);
  EXPECT_EQ(fake->ledger.input_channel_reads, 0);
}

TEST(CoreAudioRouteProbeTest, OutputOnlyNonSettableBuiltInUsesExternalSrc) {
  auto fake = std::make_shared<FakeCoreAudioRouteQuery>();
  auto endpoint = makeEndpoint(40, "built-in.output", 0, 2, 44100, kAudioDeviceTransportTypeBuiltIn,
                               {{44100.0, 44100.0}, {48000.0, 48000.0}});
  endpoint.nominal_sample_rate_settable = false;
  fake->endpoints.emplace(endpoint.device_id, std::move(endpoint));
  CoreAudioRouteResolver resolver(fake);

  auto config = duplexConfig();
  config.num_inputs = 0;
  config.input_device_id = "stale-input";
  config.channel_map.input_channels = {99, 99};
  config.output_device_id = "built-in.output";
  config.channel_map.output_channels = {0, 1};
  config.sample_rate_policy = AudioSampleRatePolicy::RequestExactRateOrConvert;

  const auto route = resolver.resolve(config, true);

  ASSERT_TRUE(route.resolved);
  ASSERT_EQ(route.compatibility.status, AudioRouteCompatibilityStatus::Compatible);
  ASSERT_EQ(route.device_rate_plans.size(), 1u);
  const auto& output_plan = route.device_rate_plans.front();
  EXPECT_FALSE(output_plan.requested_write_rate.has_value());
  EXPECT_TRUE(route.compatibility.output_conversion_required);
  EXPECT_TRUE(output_plan.output_uses_external_src);
  EXPECT_EQ(route.compatibility.planned_output_client_rate, 44100u);
  EXPECT_EQ(fake->ledger.default_input_reads, 0);
  EXPECT_EQ(fake->ledger.input_channel_reads, 0);
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
TEST(CoreAudioRouteProbeTest, ChannelMapDefaultsOrderAndBoundsAreDeterministic) {
  std::vector<uint16_t> resolved;

  EXPECT_TRUE(detail::resolveCoreAudioChannelMap({}, 2, 2, resolved));
  EXPECT_EQ(resolved, (std::vector<uint16_t>{0, 1}));

  EXPECT_TRUE(detail::resolveCoreAudioChannelMap({1, 0}, 2, 2, resolved));
  EXPECT_EQ(resolved, (std::vector<uint16_t>{1, 0}));
  EXPECT_TRUE(detail::resolveCoreAudioChannelMap({2, 0}, 2, 3, resolved));
  EXPECT_EQ(resolved, (std::vector<uint16_t>{2, 0}));

  EXPECT_FALSE(detail::resolveCoreAudioChannelMap({0, 0}, 2, 2, resolved));
  EXPECT_FALSE(detail::resolveCoreAudioChannelMap({0, 2}, 2, 2, resolved));
  EXPECT_FALSE(detail::resolveCoreAudioChannelMap({0}, 2, 2, resolved));
  EXPECT_FALSE(detail::resolveCoreAudioChannelMap({0, 1, 2}, 2, 3, resolved));
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

TEST(CoreAudioRouteProbeTest, PassiveFactQueryFailuresAreBackendFailures) {
  auto fake = std::make_shared<FakeCoreAudioRouteQuery>();
  installSameDevice(*fake);
  fake->endpoints.at(11).sample_rate_status = CoreAudioRouteQueryStatus::Missing;
  CoreAudioDriver driver(fake);

  auto compatibility = driver.probeRoute(duplexConfig());
  EXPECT_EQ(compatibility.status, AudioRouteCompatibilityStatus::BackendFailure);
  EXPECT_EQ(compatibility.detail, "ranges:output");
  EXPECT_EQ(fake->ledger.current_rate_reads, 0);
  expectFailureDetail(compatibility);

  fake->endpoints.at(11).sample_rate_status = CoreAudioRouteQueryStatus::Success;
  fake->endpoints.at(11).current_rate_status = CoreAudioRouteQueryStatus::Missing;
  fake->resetLedger();
  compatibility = driver.probeRoute(duplexConfig());
  EXPECT_EQ(compatibility.status, AudioRouteCompatibilityStatus::BackendFailure);
  EXPECT_EQ(compatibility.detail, "current_rate:output");
  EXPECT_EQ(fake->ledger.running_reads, 0);

  fake->endpoints.at(11).current_rate_status = CoreAudioRouteQueryStatus::Success;
  fake->endpoints.at(11).running_status = CoreAudioRouteQueryStatus::Missing;
  fake->resetLedger();
  compatibility = driver.probeRoute(duplexConfig());
  EXPECT_EQ(compatibility.status, AudioRouteCompatibilityStatus::BackendFailure);
  EXPECT_EQ(compatibility.detail, "running:output");
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
TEST(CoreAudioRouteProbeTest, BluetoothInputUsesNativeRateAndWritesOnlyDistinctMacOutput) {
  auto fake = std::make_shared<FakeCoreAudioRouteQuery>();
  fake->endpoints.emplace(12, makeEndpoint(12, "bt-input", 1, 0, 16000,
                                           kAudioDeviceTransportTypeBluetooth, {{16000.0, 16000.0}},
                                           {12}));
  fake->endpoints.emplace(11, makeEndpoint(11, "mac-output", 0, 2, 44100,
                                           kAudioDeviceTransportTypeBuiltIn,
                                           {{44100.0, 44100.0}, {48000.0, 48000.0}}, {11}));
  CoreAudioRouteResolver resolver(fake);

  auto config = duplexConfig();
  config.num_inputs = 1;
  config.input_device_id = "bt-input";
  config.output_device_id = "mac-output";
  config.channel_map.output_channels = {0, 1};
  config.sample_rate_policy = AudioSampleRatePolicy::RequestExactRateOrConvert;

  const auto route = resolver.resolve(config);

  ASSERT_TRUE(route.resolved);
  ASSERT_EQ(route.compatibility.status, AudioRouteCompatibilityStatus::Compatible);
  EXPECT_TRUE(route.compatibility.input_conversion_required);
  EXPECT_FALSE(route.compatibility.output_conversion_required);
  EXPECT_EQ(route.compatibility.planned_input_client_rate, 16000u);
  EXPECT_EQ(route.compatibility.planned_output_client_rate, 48000u);
  EXPECT_TRUE(route.compatibility.input_is_bluetooth);
  EXPECT_FALSE(route.compatibility.output_is_bluetooth);
  EXPECT_FALSE(route.compatibility.endpoints_related);
  EXPECT_TRUE(route.compatibility.requires_post_bind_reprobe);
  ASSERT_EQ(route.device_rate_plans.size(), 2u);
  const auto input_plan =
      std::find_if(route.device_rate_plans.begin(), route.device_rate_plans.end(),
                   [](const auto& plan) { return plan.device_id == 12; });
  const auto output_plan =
      std::find_if(route.device_rate_plans.begin(), route.device_rate_plans.end(),
                   [](const auto& plan) { return plan.device_id == 11; });
  ASSERT_NE(input_plan, route.device_rate_plans.end());
  ASSERT_NE(output_plan, route.device_rate_plans.end());
  EXPECT_FALSE(input_plan->requested_write_rate.has_value());
  EXPECT_TRUE(input_plan->input_uses_external_src);
  ASSERT_TRUE(output_plan->requested_write_rate.has_value());
  EXPECT_EQ(*output_plan->requested_write_rate, 48000u);
  EXPECT_FALSE(output_plan->output_uses_external_src);
}

TEST(CoreAudioRouteProbeTest, RelatedBluetoothDuplexConvertsBothAndStrictPolicyConflicts) {
  auto fake = std::make_shared<FakeCoreAudioRouteQuery>();
  fake->endpoints.emplace(20, makeEndpoint(20, "headset", 2, 2, 16000,
                                           kAudioDeviceTransportTypeBluetooth,
                                           {{16000.0, 16000.0}, {48000.0, 48000.0}}, {20}));
  CoreAudioRouteResolver resolver(fake);

  auto conversion = duplexConfig();
  conversion.input_device_id = "headset";
  conversion.output_device_id = "headset";
  conversion.channel_map.input_channels = {0, 1};
  conversion.channel_map.output_channels = {0, 1};
  conversion.sample_rate_policy = AudioSampleRatePolicy::RequestExactRateOrConvert;

  const auto converted = resolver.resolve(conversion);

  ASSERT_TRUE(converted.resolved);
  ASSERT_EQ(converted.compatibility.status, AudioRouteCompatibilityStatus::Compatible);
  EXPECT_TRUE(converted.compatibility.input_conversion_required);
  EXPECT_TRUE(converted.compatibility.output_conversion_required);
  EXPECT_EQ(converted.compatibility.planned_input_client_rate, 16000u);
  EXPECT_EQ(converted.compatibility.planned_output_client_rate, 16000u);
  EXPECT_TRUE(converted.compatibility.input_is_bluetooth);
  EXPECT_TRUE(converted.compatibility.output_is_bluetooth);
  EXPECT_TRUE(converted.compatibility.endpoints_related);
  ASSERT_EQ(converted.device_rate_plans.size(), 1u);
  EXPECT_FALSE(converted.device_rate_plans.front().requested_write_rate.has_value());
  EXPECT_TRUE(converted.device_rate_plans.front().input_uses_external_src);
  EXPECT_TRUE(converted.device_rate_plans.front().output_uses_external_src);

  conversion.sample_rate_policy = AudioSampleRatePolicy::RequestExactRate;
  const auto strict = resolver.resolve(conversion);
  EXPECT_FALSE(strict.resolved);
  EXPECT_EQ(strict.compatibility.status, AudioRouteCompatibilityStatus::ProfileConflict);
  EXPECT_TRUE(strict.compatibility.input_is_bluetooth);
  EXPECT_TRUE(strict.compatibility.output_is_bluetooth);
}

TEST(CoreAudioRouteProbeTest, UnrelatedBluetoothDuplexPreservesBothNativeRatesWithoutWrites) {
  auto fake = std::make_shared<FakeCoreAudioRouteQuery>();
  fake->endpoints.emplace(21, makeEndpoint(21, "bt-input", 2, 0, 16000,
                                           kAudioDeviceTransportTypeBluetooth, {{16000.0, 16000.0}},
                                           {21}));
  fake->endpoints.emplace(22, makeEndpoint(22, "bt-output", 0, 2, 24000,
                                           kAudioDeviceTransportTypeBluetooth, {{24000.0, 24000.0}},
                                           {22}));
  CoreAudioRouteResolver resolver(fake);

  auto config = duplexConfig();
  config.input_device_id = "bt-input";
  config.output_device_id = "bt-output";
  config.channel_map.input_channels = {0, 1};
  config.channel_map.output_channels = {0, 1};
  config.sample_rate_policy = AudioSampleRatePolicy::RequestExactRateOrConvert;

  const auto route = resolver.resolve(config);

  ASSERT_TRUE(route.resolved);
  EXPECT_FALSE(route.compatibility.endpoints_related);
  EXPECT_TRUE(route.compatibility.input_conversion_required);
  EXPECT_TRUE(route.compatibility.output_conversion_required);
  EXPECT_EQ(route.compatibility.planned_input_client_rate, 16000u);
  EXPECT_EQ(route.compatibility.planned_output_client_rate, 24000u);
  EXPECT_FALSE(route.requires_private_aggregate);
  ASSERT_EQ(route.device_rate_plans.size(), 2u);
  for (const auto& plan : route.device_rate_plans) {
    EXPECT_FALSE(plan.requested_write_rate.has_value());
  }
}

TEST(CoreAudioRouteProbeTest, BluetoothOutputOnlyNeverQueriesInputAndWritesWhenSafe) {
  auto fake = std::make_shared<FakeCoreAudioRouteQuery>();
  fake->default_input_status = CoreAudioRouteQueryStatus::PermissionDenied;
  fake->endpoints.emplace(30, makeEndpoint(30, "bt-output", 0, 2, 16000,
                                           kAudioDeviceTransportTypeBluetooth,
                                           {{16000.0, 16000.0}, {48000.0, 48000.0}}, {30}));
  CoreAudioRouteResolver resolver(fake);

  auto config = duplexConfig();
  config.num_inputs = 0;
  config.input_device_id = "stale-input";
  config.output_device_id = "bt-output";
  config.channel_map.input_channels = {99, 99};
  config.channel_map.output_channels = {0, 1};
  config.sample_rate_policy = AudioSampleRatePolicy::RequestExactRateOrConvert;

  const auto route = resolver.resolve(config);

  ASSERT_TRUE(route.resolved);
  EXPECT_EQ(route.compatibility.current_input_sample_rate, 0u);
  EXPECT_FALSE(route.compatibility.input_is_bluetooth);
  EXPECT_TRUE(route.compatibility.output_is_bluetooth);
  EXPECT_FALSE(route.compatibility.output_conversion_required);
  ASSERT_EQ(route.device_rate_plans.size(), 1u);
  ASSERT_TRUE(route.device_rate_plans.front().requested_write_rate.has_value());
  EXPECT_EQ(*route.device_rate_plans.front().requested_write_rate, 48000u);
  EXPECT_EQ(fake->ledger.default_input_reads, 0);
  EXPECT_EQ(fake->ledger.uid_reads, 1);
  EXPECT_EQ(fake->ledger.input_channel_reads, 0);
  EXPECT_EQ(fake->ledger.physical_format_reads, 1);
}

TEST(CoreAudioRouteProbeTest, ExplicitBluetoothMonoFallbackIsTheOnlyAcceptedWidthReduction) {
  auto fake = std::make_shared<FakeCoreAudioRouteQuery>();
  fake->endpoints.emplace(31, makeEndpoint(31, "mono-output", 0, 1, 48000,
                                           kAudioDeviceTransportTypeBluetooth, {{48000.0, 48000.0}},
                                           {31}));
  CoreAudioRouteResolver resolver(fake);

  auto fallback = duplexConfig();
  fallback.num_inputs = 0;
  fallback.output_device_id = "mono-output";
  fallback.channel_map.output_channels = {0, 1};
  fallback.output_channel_policy = AudioOutputChannelPolicy::AllowMonoFallback;
  fallback.sample_rate_policy = AudioSampleRatePolicy::RequestExactRateOrConvert;
  const auto accepted = resolver.resolve(fallback);

  ASSERT_TRUE(accepted.resolved);
  EXPECT_TRUE(accepted.output_mono_fallback);
  EXPECT_EQ(accepted.output_channel_map, (std::vector<uint16_t>{0}));
  EXPECT_EQ(accepted.compatibility.requested_output_channels, 2u);
  EXPECT_EQ(accepted.compatibility.resolved_output_channels, 1u);
  EXPECT_TRUE(accepted.compatibility.output_mono_fallback_planned);

  fallback.output_channel_policy = AudioOutputChannelPolicy::RequireRequestedChannels;
  const auto rejected = resolver.resolve(fallback);
  EXPECT_FALSE(rejected.resolved);
  EXPECT_EQ(rejected.compatibility.status, AudioRouteCompatibilityStatus::ProfileConflict);
}

} // namespace
} // namespace orpheus
