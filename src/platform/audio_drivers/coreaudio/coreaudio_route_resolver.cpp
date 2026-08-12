// SPDX-License-Identifier: MIT
#include "coreaudio_route_resolver.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace orpheus::detail {
namespace {

constexpr double kSampleRateEpsilonHz = 0.001;
bool normalizeCurrentSampleRate(double value, uint32_t& normalized);

bool isPermissionDenied(OSStatus status) {
  return status == kAudioDevicePermissionsError;
}

template <typename T> CoreAudioRouteQueryResult<T> queryResultForStatus(OSStatus status) {
  CoreAudioRouteQueryResult<T> result;
  if (status == noErr) {
    result.status = CoreAudioRouteQueryStatus::Success;
  } else if (isPermissionDenied(status)) {
    result.status = CoreAudioRouteQueryStatus::PermissionDenied;
  } else {
    result.status = CoreAudioRouteQueryStatus::Failure;
  }
  return result;
}

template <typename T> CoreAudioRouteQueryResult<T> missingQueryResult() {
  CoreAudioRouteQueryResult<T> result;
  result.status = CoreAudioRouteQueryStatus::Missing;
  return result;
}

template <typename T> CoreAudioRouteQueryResult<T> successQueryResult(T value) {
  CoreAudioRouteQueryResult<T> result;
  result.status = CoreAudioRouteQueryStatus::Success;
  result.value = std::move(value);
  return result;
}

class ProductionCoreAudioRouteQuery final : public ICoreAudioRouteQuery {
public:
  CoreAudioRouteQueryResult<ResolvedCoreAudioEndpoint> resolveDefault(bool output) const override {
    const AudioObjectPropertySelector selector = output ? kAudioHardwarePropertyDefaultOutputDevice
                                                        : kAudioHardwarePropertyDefaultInputDevice;
    const AudioObjectPropertyAddress address = {selector, kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};
    AudioDeviceID device_id = 0;
    UInt32 data_size = sizeof(device_id);
    const OSStatus status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0,
                                                       nullptr, &data_size, &device_id);
    if (status != noErr) {
      return queryResultForStatus<ResolvedCoreAudioEndpoint>(status);
    }
    if (device_id == 0 || device_id == kAudioObjectUnknown) {
      return missingQueryResult<ResolvedCoreAudioEndpoint>();
    }

    const auto uid = readDeviceUID(device_id);
    if (uid.status != CoreAudioRouteQueryStatus::Success) {
      CoreAudioRouteQueryResult<ResolvedCoreAudioEndpoint> result;
      result.status = uid.status;
      return result;
    }
    return successQueryResult(ResolvedCoreAudioEndpoint{device_id, uid.value});
  }

  CoreAudioRouteQueryResult<ResolvedCoreAudioEndpoint>
  resolveDeviceUID(std::string_view device_uid) const override {
    if (device_uid.empty()) {
      return missingQueryResult<ResolvedCoreAudioEndpoint>();
    }

    CFStringRef requested = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(device_uid.data()),
        static_cast<CFIndex>(device_uid.size()), kCFStringEncodingUTF8, false);
    if (requested == nullptr) {
      CoreAudioRouteQueryResult<ResolvedCoreAudioEndpoint> result;
      result.status = CoreAudioRouteQueryStatus::Failure;
      return result;
    }

    const AudioObjectPropertyAddress address = {kAudioHardwarePropertyDevices,
                                                kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};
    UInt32 data_size = 0;
    OSStatus status =
        AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, nullptr, &data_size);
    if (status != noErr) {
      CFRelease(requested);
      return queryResultForStatus<ResolvedCoreAudioEndpoint>(status);
    }
    if (data_size == 0 || data_size % sizeof(AudioDeviceID) != 0) {
      CFRelease(requested);
      return data_size == 0 ? missingQueryResult<ResolvedCoreAudioEndpoint>()
                            : queryResultForStatus<ResolvedCoreAudioEndpoint>(
                                  kAudioHardwareBadPropertySizeError);
    }

    std::vector<AudioDeviceID> devices(data_size / sizeof(AudioDeviceID));
    status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &data_size,
                                        devices.data());
    if (status != noErr) {
      CFRelease(requested);
      return queryResultForStatus<ResolvedCoreAudioEndpoint>(status);
    }

    for (const AudioDeviceID device_id : devices) {
      const auto uid = readDeviceUID(device_id);
      if (uid.status != CoreAudioRouteQueryStatus::Success) {
        CFRelease(requested);
        CoreAudioRouteQueryResult<ResolvedCoreAudioEndpoint> result;
        result.status = uid.status;
        return result;
      }
      CFStringRef candidate = CFStringCreateWithBytes(
          kCFAllocatorDefault, reinterpret_cast<const UInt8*>(uid.value.data()),
          static_cast<CFIndex>(uid.value.size()), kCFStringEncodingUTF8, false);
      if (candidate == nullptr) {
        CFRelease(requested);
        CoreAudioRouteQueryResult<ResolvedCoreAudioEndpoint> result;
        result.status = CoreAudioRouteQueryStatus::Failure;
        return result;
      }
      const bool equal = CFStringCompare(candidate, requested, 0) == kCFCompareEqualTo;
      CFRelease(candidate);
      if (equal) {
        CFRelease(requested);
        return successQueryResult(ResolvedCoreAudioEndpoint{device_id, uid.value});
      }
    }

    CFRelease(requested);
    return missingQueryResult<ResolvedCoreAudioEndpoint>();
  }

  CoreAudioRouteQueryResult<uint32_t> channelCount(AudioDeviceID device_id,
                                                   AudioObjectPropertyScope scope) const override {
    if (device_id == 0) {
      return missingQueryResult<uint32_t>();
    }
    const AudioObjectPropertyAddress address = {kAudioDevicePropertyStreamConfiguration, scope,
                                                kAudioObjectPropertyElementMain};
    UInt32 data_size = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(device_id, &address, 0, nullptr, &data_size);
    if (status != noErr) {
      return queryResultForStatus<uint32_t>(status);
    }
    if (data_size < sizeof(AudioBufferList)) {
      return queryResultForStatus<uint32_t>(kAudioHardwareBadPropertySizeError);
    }

    std::vector<uint8_t> storage(data_size);
    auto* buffers = reinterpret_cast<AudioBufferList*>(storage.data());
    status = AudioObjectGetPropertyData(device_id, &address, 0, nullptr, &data_size, buffers);
    if (status != noErr) {
      return queryResultForStatus<uint32_t>(status);
    }

    uint64_t channels = 0;
    for (UInt32 index = 0; index < buffers->mNumberBuffers; ++index) {
      channels += buffers->mBuffers[index].mNumberChannels;
    }
    if (channels > std::numeric_limits<uint32_t>::max()) {
      return queryResultForStatus<uint32_t>(kAudioHardwareBadPropertySizeError);
    }
    return successQueryResult(static_cast<uint32_t>(channels));
  }

  CoreAudioRouteQueryResult<std::vector<CoreAudioEndpointRange>>
  advertisedSampleRateRanges(AudioDeviceID device_id) const override {
    if (device_id == 0) {
      return missingQueryResult<std::vector<CoreAudioEndpointRange>>();
    }
    const AudioObjectPropertyAddress address = {kAudioDevicePropertyAvailableNominalSampleRates,
                                                kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};
    UInt32 data_size = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(device_id, &address, 0, nullptr, &data_size);
    if (status != noErr) {
      return queryResultForStatus<std::vector<CoreAudioEndpointRange>>(status);
    }
    if (data_size == 0) {
      return successQueryResult(std::vector<CoreAudioEndpointRange>{});
    }
    if (data_size % sizeof(AudioValueRange) != 0) {
      return queryResultForStatus<std::vector<CoreAudioEndpointRange>>(
          kAudioHardwareBadPropertySizeError);
    }

    std::vector<AudioValueRange> ranges(data_size / sizeof(AudioValueRange));
    status = AudioObjectGetPropertyData(device_id, &address, 0, nullptr, &data_size, ranges.data());
    if (status != noErr) {
      return queryResultForStatus<std::vector<CoreAudioEndpointRange>>(status);
    }

    std::vector<CoreAudioEndpointRange> normalized;
    normalized.reserve(ranges.size());
    for (const AudioValueRange& range : ranges) {
      normalized.push_back(
          {static_cast<double>(range.mMinimum), static_cast<double>(range.mMaximum)});
    }
    return successQueryResult(std::move(normalized));
  }

  CoreAudioRouteQueryResult<double> currentSampleRate(AudioDeviceID device_id) const override {
    if (device_id == 0) {
      return missingQueryResult<double>();
    }
    const AudioObjectPropertyAddress address = {kAudioDevicePropertyNominalSampleRate,
                                                kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};
    Float64 value = 0.0;
    UInt32 data_size = sizeof(value);
    const OSStatus status =
        AudioObjectGetPropertyData(device_id, &address, 0, nullptr, &data_size, &value);
    if (status != noErr) {
      return queryResultForStatus<double>(status);
    }
    return successQueryResult(static_cast<double>(value));
  }

  CoreAudioRouteQueryResult<bool> isRunningSomewhere(AudioDeviceID device_id) const override {
    if (device_id == 0) {
      return missingQueryResult<bool>();
    }
    const AudioObjectPropertyAddress address = {kAudioDevicePropertyDeviceIsRunningSomewhere,
                                                kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};
    UInt32 value = 0;
    UInt32 data_size = sizeof(value);
    const OSStatus status =
        AudioObjectGetPropertyData(device_id, &address, 0, nullptr, &data_size, &value);
    if (status != noErr) {
      return queryResultForStatus<bool>(status);
    }
    return successQueryResult(value != 0);
  }
  CoreAudioRouteQueryResult<uint32_t> transportType(AudioDeviceID device_id) const override {
    if (device_id == 0) {
      return missingQueryResult<uint32_t>();
    }
    const AudioObjectPropertyAddress address = {kAudioDevicePropertyTransportType,
                                                kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};
    UInt32 value = 0;
    UInt32 data_size = sizeof(value);
    const OSStatus status =
        AudioObjectGetPropertyData(device_id, &address, 0, nullptr, &data_size, &value);
    if (status != noErr) {
      return queryResultForStatus<uint32_t>(status);
    }
    return successQueryResult(static_cast<uint32_t>(value));
  }

  CoreAudioRouteQueryResult<std::vector<AudioDeviceID>>
  relatedDeviceIDs(AudioDeviceID device_id) const override {
    if (device_id == 0) {
      return missingQueryResult<std::vector<AudioDeviceID>>();
    }
    const AudioObjectPropertyAddress address = {kAudioDevicePropertyRelatedDevices,
                                                kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};
    UInt32 data_size = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(device_id, &address, 0, nullptr, &data_size);
    if (status != noErr) {
      return queryResultForStatus<std::vector<AudioDeviceID>>(status);
    }
    if (data_size == 0) {
      return successQueryResult(std::vector<AudioDeviceID>{});
    }
    if (data_size % sizeof(AudioDeviceID) != 0) {
      return queryResultForStatus<std::vector<AudioDeviceID>>(kAudioHardwareBadPropertySizeError);
    }
    std::vector<AudioDeviceID> related(data_size / sizeof(AudioDeviceID));
    status =
        AudioObjectGetPropertyData(device_id, &address, 0, nullptr, &data_size, related.data());
    if (status != noErr) {
      return queryResultForStatus<std::vector<AudioDeviceID>>(status);
    }
    return successQueryResult(std::move(related));
  }

  CoreAudioRouteQueryResult<CoreAudioStreamFormat>
  physicalStreamFormat(AudioDeviceID device_id, AudioObjectPropertyScope scope) const override {
    return streamFormat(device_id, scope, kAudioStreamPropertyPhysicalFormat);
  }

  CoreAudioRouteQueryResult<CoreAudioStreamFormat>
  virtualStreamFormat(AudioDeviceID device_id, AudioObjectPropertyScope scope) const override {
    return streamFormat(device_id, scope, kAudioStreamPropertyVirtualFormat);
  }

  CoreAudioRouteQueryResult<bool>
  nominalSampleRateSettable(AudioDeviceID device_id) const override {
    if (device_id == 0) {
      return missingQueryResult<bool>();
    }
    const AudioObjectPropertyAddress address = {kAudioDevicePropertyNominalSampleRate,
                                                kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};
    Boolean settable = 0;
    const OSStatus status = AudioObjectIsPropertySettable(device_id, &address, &settable);
    if (status != noErr) {
      return queryResultForStatus<bool>(status);
    }
    return successQueryResult(settable != 0);
  }

  CoreAudioRouteQueryResult<uint32_t>
  physicalChannelCount(AudioDeviceID device_id, AudioObjectPropertyScope scope) const override {
    return channelCount(device_id, scope);
  }

private:
  CoreAudioRouteQueryResult<CoreAudioStreamFormat>
  streamFormat(AudioDeviceID device_id, AudioObjectPropertyScope scope,
               AudioObjectPropertySelector selector) const {
    if (device_id == 0) {
      return missingQueryResult<CoreAudioStreamFormat>();
    }
    const AudioObjectPropertyAddress streams_address = {kAudioDevicePropertyStreams, scope,
                                                        kAudioObjectPropertyElementMain};
    UInt32 data_size = 0;
    OSStatus status =
        AudioObjectGetPropertyDataSize(device_id, &streams_address, 0, nullptr, &data_size);
    if (status != noErr) {
      return queryResultForStatus<CoreAudioStreamFormat>(status);
    }
    if (data_size == 0) {
      return missingQueryResult<CoreAudioStreamFormat>();
    }
    if (data_size % sizeof(AudioStreamID) != 0) {
      return queryResultForStatus<CoreAudioStreamFormat>(kAudioHardwareBadPropertySizeError);
    }
    std::vector<AudioStreamID> streams(data_size / sizeof(AudioStreamID));
    status = AudioObjectGetPropertyData(device_id, &streams_address, 0, nullptr, &data_size,
                                        streams.data());
    if (status != noErr || streams.empty()) {
      return status == noErr ? missingQueryResult<CoreAudioStreamFormat>()
                             : queryResultForStatus<CoreAudioStreamFormat>(status);
    }

    const AudioObjectPropertyAddress format_address = {selector, kAudioObjectPropertyScopeGlobal,
                                                       kAudioObjectPropertyElementMain};
    AudioStreamBasicDescription asbd{};
    UInt32 format_size = sizeof(asbd);
    status = AudioObjectGetPropertyData(streams.front(), &format_address, 0, nullptr, &format_size,
                                        &asbd);
    if (status != noErr) {
      return queryResultForStatus<CoreAudioStreamFormat>(status);
    }
    if (!std::isfinite(asbd.mSampleRate) || asbd.mSampleRate <= 0.0 ||
        asbd.mChannelsPerFrame == 0 ||
        asbd.mChannelsPerFrame > std::numeric_limits<uint16_t>::max()) {
      return queryResultForStatus<CoreAudioStreamFormat>(kAudioHardwareBadPropertySizeError);
    }
    uint32_t sample_rate = 0;
    if (!normalizeCurrentSampleRate(asbd.mSampleRate, sample_rate)) {
      return queryResultForStatus<CoreAudioStreamFormat>(kAudioHardwareBadPropertySizeError);
    }
    return successQueryResult(
        CoreAudioStreamFormat{sample_rate, static_cast<uint16_t>(asbd.mChannelsPerFrame)});
  }

  CoreAudioRouteQueryResult<std::string> readDeviceUID(AudioDeviceID device_id) const {
    const AudioObjectPropertyAddress address = {kAudioDevicePropertyDeviceUID,
                                                kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};
    CFStringRef uid = nullptr;
    UInt32 data_size = sizeof(uid);
    const OSStatus status =
        AudioObjectGetPropertyData(device_id, &address, 0, nullptr, &data_size, &uid);
    if (status != noErr) {
      return queryResultForStatus<std::string>(status);
    }
    if (uid == nullptr) {
      CoreAudioRouteQueryResult<std::string> result;
      result.status = CoreAudioRouteQueryStatus::Failure;
      return result;
    }
    char buffer[1024] = {};
    const Boolean converted =
        CFStringGetCString(uid, buffer, sizeof(buffer), kCFStringEncodingUTF8);
    CFRelease(uid);
    if (!converted) {
      CoreAudioRouteQueryResult<std::string> result;
      result.status = CoreAudioRouteQueryStatus::Failure;
      return result;
    }
    return successQueryResult(std::string(buffer));
  }
};

AudioRouteCompatibilityStatus unavailableStatus(bool output) {
  return output ? AudioRouteCompatibilityStatus::OutputUnavailable
                : AudioRouteCompatibilityStatus::InputUnavailable;
}

void setFailure(ResolvedCoreAudioRoute& route, AudioRouteCompatibilityStatus status,
                const char* operation, bool output) {
  route.resolved = false;
  route.compatibility.status = status;
  route.compatibility.detail = operation;
  route.compatibility.detail += output ? ":output" : ":input";
  if (route.compatibility.detail.size() > 96) {
    route.compatibility.detail.resize(96);
  }
}

bool setQueryFailure(ResolvedCoreAudioRoute& route, CoreAudioRouteQueryStatus status,
                     const char* operation, bool output, bool missing_is_unavailable = true) {
  if (status == CoreAudioRouteQueryStatus::Success) {
    return false;
  }
  const AudioRouteCompatibilityStatus compatibility_status =
      status == CoreAudioRouteQueryStatus::PermissionDenied
          ? AudioRouteCompatibilityStatus::PermissionDenied
      : status == CoreAudioRouteQueryStatus::Missing && missing_is_unavailable
          ? unavailableStatus(output)
          : AudioRouteCompatibilityStatus::BackendFailure;
  setFailure(route, compatibility_status, operation, output);
  return true;
}

bool normalizeCurrentSampleRate(double value, uint32_t& normalized) {
  if (!std::isfinite(value) || value <= 0.0 ||
      value > static_cast<double>(std::numeric_limits<uint32_t>::max())) {
    return false;
  }
  const double rounded = std::round(value);
  if (std::abs(value - rounded) > kSampleRateEpsilonHz ||
      rounded > static_cast<double>(std::numeric_limits<uint32_t>::max())) {
    return false;
  }
  normalized = static_cast<uint32_t>(rounded);
  return true;
}

bool supportsRequestedRate(const std::vector<CoreAudioEndpointRange>& ranges, uint32_t requested) {
  const double requested_rate = static_cast<double>(requested);
  return std::any_of(ranges.begin(), ranges.end(), [requested_rate](const auto& range) {
    if (!std::isfinite(range.minimum) || !std::isfinite(range.maximum) || range.minimum <= 0.0 ||
        range.maximum <= 0.0 || range.minimum > range.maximum) {
      return false;
    }
    return requested_rate + kSampleRateEpsilonHz >= range.minimum &&
           requested_rate <= range.maximum + kSampleRateEpsilonHz;
  });
}
bool isBluetoothTransport(uint32_t transport) {
  return transport == kAudioDeviceTransportTypeBluetooth ||
         transport == kAudioDeviceTransportTypeBluetoothLE;
}

bool isConverterRate(uint32_t rate) {
  return rate == 16'000 || rate == 24'000 || rate == 44'100 || rate == 48'000;
}

bool containsDevice(const std::vector<AudioDeviceID>& devices, AudioDeviceID device_id) {
  return std::find(devices.begin(), devices.end(), device_id) != devices.end();
}

bool requestsExplicitStereoMap(const AudioDriverConfig& config) {
  return config.num_outputs == 2 &&
         config.channel_map.output_channels == std::vector<uint16_t>{0, 1};
}

} // namespace

bool resolveCoreAudioChannelMap(const std::vector<uint16_t>& requested, uint16_t logical_count,
                                uint32_t physical_count, std::vector<uint16_t>& resolved) {
  resolved.clear();
  if (logical_count > physical_count) {
    return false;
  }
  if (requested.empty()) {
    resolved.resize(logical_count);
    for (uint16_t channel = 0; channel < logical_count; ++channel) {
      resolved[channel] = channel;
    }
    return true;
  }
  if (requested.size() != logical_count) {
    return false;
  }

  resolved.reserve(requested.size());
  for (const uint16_t channel : requested) {
    if (channel >= physical_count ||
        std::find(resolved.begin(), resolved.end(), channel) != resolved.end()) {
      resolved.clear();
      return false;
    }
    resolved.push_back(channel);
  }
  return true;
}

CoreAudioRouteResolver::CoreAudioRouteResolver(std::shared_ptr<const ICoreAudioRouteQuery> query)
    : query_(std::move(query)) {}

ResolvedCoreAudioRoute CoreAudioRouteResolver::resolve(const AudioDriverConfig& config,
                                                       bool allow_rate_writes) const {
  ResolvedCoreAudioRoute route;
  route.compatibility.requested_sample_rate = config.sample_rate;
  route.compatibility.requested_output_channels = config.num_outputs;

  if (config.num_outputs == 0) {
    setFailure(route, AudioRouteCompatibilityStatus::OutputUnavailable, "config", true);
    return route;
  }
  if (config.sample_rate == 0 || config.buffer_size == 0) {
    setFailure(route, AudioRouteCompatibilityStatus::BackendFailure, "config", true);
    return route;
  }
  if (!query_) {
    setFailure(route, AudioRouteCompatibilityStatus::BackendFailure, "query", true);
    return route;
  }

  struct EndpointFacts {
    AudioDeviceID device_id = 0;
    uint32_t physical_channels = 0;
    uint32_t transport = 0;
    uint32_t observed_rate = 0;
    bool running = false;
    bool settable = false;
    bool is_bluetooth = false;
    std::vector<CoreAudioEndpointRange> ranges;
    CoreAudioStreamFormat physical_format;
    CoreAudioStreamFormat virtual_format;
  };

  const auto resolve_endpoint = [&](bool output, const std::string& requested,
                                    EndpointFacts& facts) -> bool {
    const auto resolved =
        requested.empty() ? query_->resolveDefault(output) : query_->resolveDeviceUID(requested);
    if (setQueryFailure(route, resolved.status, "resolve", output)) {
      return false;
    }
    if (resolved.value.device_id == 0) {
      setFailure(route, unavailableStatus(output), "resolve", output);
      return false;
    }
    const auto channels = query_->physicalChannelCount(resolved.value.device_id,
                                                       output ? kAudioObjectPropertyScopeOutput
                                                              : kAudioObjectPropertyScopeInput);
    if (setQueryFailure(route, channels.status, "channels", output)) {
      return false;
    }
    if (channels.value == 0) {
      setFailure(route, unavailableStatus(output), "channels", output);
      return false;
    }

    facts.device_id = resolved.value.device_id;
    facts.physical_channels = channels.value;
    if (output) {
      route.output_device_id = facts.device_id;
      route.output_channel_count = facts.physical_channels;
      route.compatibility.resolved_output_device_id =
          resolved.value.device_uid.empty() ? requested : resolved.value.device_uid;
    } else {
      route.input_device_id = facts.device_id;
      route.input_channel_count = facts.physical_channels;
      route.compatibility.resolved_input_device_id =
          resolved.value.device_uid.empty() ? requested : resolved.value.device_uid;
    }
    return true;
  };

  EndpointFacts output_facts;
  if (!resolve_endpoint(true, config.output_device_id, output_facts)) {
    return route;
  }

  EndpointFacts input_facts;
  const bool has_input = config.num_inputs > 0;
  if (has_input && !resolve_endpoint(false, config.input_device_id, input_facts)) {
    return route;
  }

  const auto read_transport = [&](EndpointFacts& facts, bool output) -> bool {
    const auto transport = query_->transportType(facts.device_id);
    if (setQueryFailure(route, transport.status, "transport", output, false)) {
      return false;
    }
    facts.transport = transport.value;
    facts.is_bluetooth = isBluetoothTransport(facts.transport);
    return true;
  };
  if (!read_transport(output_facts, true)) {
    return route;
  }
  if (has_input && input_facts.device_id != output_facts.device_id &&
      !read_transport(input_facts, false)) {
    return route;
  }
  if (has_input && input_facts.device_id == output_facts.device_id) {
    input_facts.transport = output_facts.transport;
    input_facts.is_bluetooth = output_facts.is_bluetooth;
  }
  route.output_transport_type = output_facts.transport;
  route.output_is_bluetooth = output_facts.is_bluetooth;
  route.compatibility.output_is_bluetooth = output_facts.is_bluetooth;
  if (has_input) {
    route.input_transport_type = input_facts.transport;
    route.input_is_bluetooth = input_facts.is_bluetooth;
    route.compatibility.input_is_bluetooth = input_facts.is_bluetooth;
  }

  if (has_input && input_facts.device_id != output_facts.device_id) {
    const auto output_related = query_->relatedDeviceIDs(output_facts.device_id);
    if (setQueryFailure(route, output_related.status, "related", true, false)) {
      return route;
    }
    const auto input_related = query_->relatedDeviceIDs(input_facts.device_id);
    if (setQueryFailure(route, input_related.status, "related", false, false)) {
      return route;
    }
    route.endpoints_related = containsDevice(output_related.value, input_facts.device_id) ||
                              containsDevice(input_related.value, output_facts.device_id);
  } else {
    route.endpoints_related = has_input;
  }
  route.compatibility.endpoints_related = route.endpoints_related;

  if (requestsExplicitStereoMap(config) && route.output_channel_count == 1 &&
      output_facts.is_bluetooth) {
    if (config.output_channel_policy == AudioOutputChannelPolicy::AllowMonoFallback) {
      route.output_channel_map = {0};
      route.output_mono_fallback = true;
      route.compatibility.output_mono_fallback_planned = true;
    } else {
      setFailure(route, AudioRouteCompatibilityStatus::ProfileConflict, "channels", true);
      return route;
    }
  } else if (!resolveCoreAudioChannelMap(config.channel_map.output_channels, config.num_outputs,
                                         route.output_channel_count, route.output_channel_map)) {
    setFailure(route, AudioRouteCompatibilityStatus::InvalidChannelMap, "map", true);
    return route;
  }
  if (has_input &&
      !resolveCoreAudioChannelMap(config.channel_map.input_channels, config.num_inputs,
                                  route.input_channel_count, route.input_channel_map)) {
    setFailure(route, AudioRouteCompatibilityStatus::InvalidChannelMap, "map", false);
    return route;
  }
  route.compatibility.resolved_output_channels =
      static_cast<uint16_t>(route.output_channel_map.size());

  const auto read_global_facts = [&](EndpointFacts& facts, bool output) -> bool {
    const auto ranges = query_->advertisedSampleRateRanges(facts.device_id);
    if (setQueryFailure(route, ranges.status, "ranges", output, false)) {
      return false;
    }
    facts.ranges = ranges.value;
    const bool session_supported = supportsRequestedRate(facts.ranges, config.sample_rate);
    if (config.sample_rate_policy != AudioSampleRatePolicy::RequestExactRateOrConvert &&
        !session_supported) {
      setFailure(route, AudioRouteCompatibilityStatus::SampleRateUnsupported, "ranges", output);
      return false;
    }

    const auto current = query_->currentSampleRate(facts.device_id);
    if (setQueryFailure(route, current.status, "current_rate", output, false)) {
      return false;
    }
    if (!normalizeCurrentSampleRate(current.value, facts.observed_rate)) {
      setFailure(route, AudioRouteCompatibilityStatus::BackendFailure, "current_rate", output);
      return false;
    }
    if (!isConverterRate(facts.observed_rate) ||
        !supportsRequestedRate(facts.ranges, facts.observed_rate)) {
      setFailure(route, AudioRouteCompatibilityStatus::SampleRateUnsupported, "ranges", output);
      return false;
    }

    const auto running = query_->isRunningSomewhere(facts.device_id);
    if (setQueryFailure(route, running.status, "running", output, false)) {
      return false;
    }
    facts.running = running.value;

    const auto settable = query_->nominalSampleRateSettable(facts.device_id);
    if (setQueryFailure(route, settable.status, "settable", output, false)) {
      return false;
    }
    facts.settable = settable.value;
    return true;
  };

  const auto read_stream_facts = [&](EndpointFacts& facts, bool output) -> bool {
    const auto physical = query_->physicalStreamFormat(
        facts.device_id, output ? kAudioObjectPropertyScopeOutput : kAudioObjectPropertyScopeInput);
    if (setQueryFailure(route, physical.status, "physical_format", output, false)) {
      return false;
    }
    facts.physical_format = physical.value;
    const auto virtual_format = query_->virtualStreamFormat(
        facts.device_id, output ? kAudioObjectPropertyScopeOutput : kAudioObjectPropertyScopeInput);
    if (setQueryFailure(route, virtual_format.status, "virtual_format", output, false)) {
      return false;
    }
    facts.virtual_format = virtual_format.value;
    return true;
  };

  if (!read_global_facts(output_facts, true) || !read_stream_facts(output_facts, true)) {
    return route;
  }
  if (has_input) {
    if (input_facts.device_id == output_facts.device_id) {
      input_facts.observed_rate = output_facts.observed_rate;
      input_facts.running = output_facts.running;
      input_facts.settable = output_facts.settable;
      input_facts.ranges = output_facts.ranges;
    } else if (!read_global_facts(input_facts, false)) {
      return route;
    }
    if (!read_stream_facts(input_facts, false)) {
      return route;
    }
  }

  route.output_physical_format = output_facts.physical_format;
  route.output_virtual_format = output_facts.virtual_format;
  route.compatibility.output_virtual_format_channels = output_facts.virtual_format.channels;
  route.compatibility.current_output_sample_rate = output_facts.observed_rate;
  route.compatibility.output_is_running_somewhere = output_facts.running;
  route.compatibility.output_rate_change_required =
      output_facts.observed_rate != config.sample_rate;
  if (has_input) {
    route.input_physical_format = input_facts.physical_format;
    route.input_virtual_format = input_facts.virtual_format;
    route.compatibility.input_virtual_format_channels = input_facts.virtual_format.channels;
    route.compatibility.current_input_sample_rate = input_facts.observed_rate;
    route.compatibility.input_is_running_somewhere = input_facts.running;
    route.compatibility.input_rate_change_required =
        input_facts.observed_rate != config.sample_rate;
  }

  route.device_rate_plans.reserve(2);
  const auto add_plan = [&](AudioDeviceID device_id,
                            uint32_t observed_rate) -> CoreAudioDeviceRatePlan& {
    for (auto& plan : route.device_rate_plans) {
      if (plan.device_id == device_id) {
        return plan;
      }
    }
    route.device_rate_plans.push_back(CoreAudioDeviceRatePlan{});
    auto& plan = route.device_rate_plans.back();
    plan.device_id = device_id;
    plan.observed_rate = observed_rate;
    return plan;
  };
  auto& output_plan = add_plan(output_facts.device_id, output_facts.observed_rate);
  CoreAudioDeviceRatePlan* input_plan = nullptr;
  if (has_input) {
    input_plan = &add_plan(input_facts.device_id, input_facts.observed_rate);
  }

  const bool exact_policy = config.sample_rate_policy == AudioSampleRatePolicy::RequestExactRate;
  const bool conversion_policy =
      config.sample_rate_policy == AudioSampleRatePolicy::RequestExactRateOrConvert;
  const bool related_bluetooth_duplex =
      has_input && route.endpoints_related && input_facts.is_bluetooth && output_facts.is_bluetooth;
  const bool unrelated_bluetooth_duplex = has_input && !route.endpoints_related &&
                                          input_facts.is_bluetooth && output_facts.is_bluetooth;

  const auto can_write_rate = [&](const EndpointFacts& facts) {
    return !facts.running && facts.settable &&
           supportsRequestedRate(facts.ranges, config.sample_rate);
  };
  const auto write_if_safe = [&](const EndpointFacts& facts, CoreAudioDeviceRatePlan& plan,
                                 bool allow_write) {
    if (allow_write && allow_rate_writes && facts.observed_rate != config.sample_rate &&
        can_write_rate(facts)) {
      plan.requested_write_rate = config.sample_rate;
      return true;
    }
    return false;
  };

  bool input_converts = has_input && input_facts.observed_rate != config.sample_rate;
  bool output_converts = output_facts.observed_rate != config.sample_rate;
  if (conversion_policy) {
    const bool output_is_unrelated_mac = !output_facts.is_bluetooth && has_input &&
                                         input_facts.is_bluetooth && !route.endpoints_related;
    const bool output_write_allowed = !related_bluetooth_duplex && !unrelated_bluetooth_duplex &&
                                              (!output_facts.is_bluetooth || !has_input) &&
                                              !output_is_unrelated_mac
                                          ? true
                                          : output_is_unrelated_mac;
    const bool input_write_allowed =
        !input_facts.is_bluetooth && !related_bluetooth_duplex && !unrelated_bluetooth_duplex;
    if (has_input && input_plan != &output_plan) {
      (void)write_if_safe(input_facts, *input_plan, input_write_allowed);
    } else if (has_input) {
      (void)write_if_safe(input_facts, output_plan, input_write_allowed);
    }
    (void)write_if_safe(output_facts, output_plan, output_write_allowed);
    input_converts = has_input && input_facts.observed_rate != config.sample_rate &&
                     (!input_plan || !input_plan->requested_write_rate.has_value());
    output_converts = output_facts.observed_rate != config.sample_rate &&
                      !output_plan.requested_write_rate.has_value();
  } else if (exact_policy) {
    if (has_input && input_plan != &output_plan) {
      if (!write_if_safe(input_facts, *input_plan,
                         !input_facts.is_bluetooth && !related_bluetooth_duplex)) {
        input_converts = false;
      } else {
        input_converts = false;
      }
    } else if (has_input && input_facts.observed_rate != config.sample_rate) {
      (void)write_if_safe(input_facts, output_plan, !input_facts.is_bluetooth);
      input_converts = false;
    }
    (void)write_if_safe(output_facts, output_plan,
                        !related_bluetooth_duplex && !unrelated_bluetooth_duplex);
  }

  if (input_plan != nullptr) {
    input_plan->input_uses_external_src = input_converts;
  }
  output_plan.output_uses_external_src = output_converts;
  if (input_plan == &output_plan) {
    output_plan.output_uses_external_src = output_converts;
    output_plan.input_uses_external_src = input_converts;
  }

  if (exact_policy) {
    const bool exact_input_failure = has_input && input_facts.observed_rate != config.sample_rate &&
                                     (!input_plan || !input_plan->requested_write_rate.has_value());
    const bool exact_output_failure = output_facts.observed_rate != config.sample_rate &&
                                      !output_plan.requested_write_rate.has_value();
    if (exact_input_failure || exact_output_failure) {
      const bool bluetooth_conflict = (exact_input_failure && input_facts.is_bluetooth) ||
                                      (exact_output_failure && output_facts.is_bluetooth) ||
                                      related_bluetooth_duplex;
      route.resolved = false;
      route.compatibility.status = bluetooth_conflict
                                       ? AudioRouteCompatibilityStatus::ProfileConflict
                                       : AudioRouteCompatibilityStatus::RequiresSampleRateChange;
      route.compatibility.detail.clear();
      return route;
    }
    input_converts = false;
    output_converts = false;
  }

  route.compatibility.planned_input_client_rate =
      has_input ? (input_converts ? input_facts.observed_rate : config.sample_rate) : 0;
  route.compatibility.planned_output_client_rate =
      output_converts ? output_facts.observed_rate : config.sample_rate;
  route.compatibility.input_conversion_required = input_converts;
  route.compatibility.output_conversion_required = output_converts;
  route.compatibility.requires_post_bind_reprobe =
      has_input && (input_facts.is_bluetooth || related_bluetooth_duplex);
  route.requires_private_aggregate =
      has_input && input_facts.device_id != output_facts.device_id && !input_converts;
  route.compatibility.detail.clear();
  route.resolved = true;
  route.compatibility.status = AudioRouteCompatibilityStatus::Compatible;
  return route;
}

std::shared_ptr<const ICoreAudioRouteQuery> makeCoreAudioRouteQuery() {
  return std::make_shared<ProductionCoreAudioRouteQuery>();
}

} // namespace orpheus::detail
