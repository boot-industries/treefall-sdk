// SPDX-License-Identifier: MIT
#include "coreaudio_route_resolver.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace orpheus::detail {
namespace {

constexpr double kSampleRateEpsilonHz = 0.001;

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

private:
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

ResolvedCoreAudioRoute CoreAudioRouteResolver::resolve(const AudioDriverConfig& config) const {
  ResolvedCoreAudioRoute route;
  route.compatibility.requested_sample_rate = config.sample_rate;

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

  const auto resolve_endpoint = [&](bool output, const std::string& requested,
                                    ResolvedCoreAudioEndpoint& endpoint) -> bool {
    const auto resolved =
        requested.empty() ? query_->resolveDefault(output) : query_->resolveDeviceUID(requested);
    if (setQueryFailure(route, resolved.status, "resolve", output)) {
      return false;
    }
    endpoint = resolved.value;
    if (endpoint.device_id == 0) {
      setFailure(route, unavailableStatus(output), "resolve", output);
      return false;
    }
    if (endpoint.device_uid.empty()) {
      if (requested.empty()) {
        setFailure(route, AudioRouteCompatibilityStatus::BackendFailure, "resolve", output);
        return false;
      }
      endpoint.device_uid = requested;
    }
    const auto channels =
        query_->channelCount(endpoint.device_id, output ? kAudioObjectPropertyScopeOutput
                                                        : kAudioObjectPropertyScopeInput);
    if (setQueryFailure(route, channels.status, "channels", output)) {
      return false;
    }
    if (channels.value == 0) {
      setFailure(route, unavailableStatus(output), "channels", output);
      return false;
    }
    if (output) {
      route.output_device_id = endpoint.device_id;
      route.output_channel_count = channels.value;
      route.compatibility.resolved_output_device_id = endpoint.device_uid;
    } else {
      route.input_device_id = endpoint.device_id;
      route.input_channel_count = channels.value;
      route.compatibility.resolved_input_device_id = endpoint.device_uid;
    }
    return true;
  };

  ResolvedCoreAudioEndpoint output_endpoint;
  if (!resolve_endpoint(true, config.output_device_id, output_endpoint)) {
    return route;
  }

  ResolvedCoreAudioEndpoint input_endpoint;
  if (config.num_inputs > 0 && !resolve_endpoint(false, config.input_device_id, input_endpoint)) {
    return route;
  }

  if (!resolveCoreAudioChannelMap(config.channel_map.output_channels, config.num_outputs,
                                  route.output_channel_count, route.output_channel_map)) {
    setFailure(route, AudioRouteCompatibilityStatus::InvalidChannelMap, "map", true);
    return route;
  }
  if (config.num_inputs > 0 &&
      !resolveCoreAudioChannelMap(config.channel_map.input_channels, config.num_inputs,
                                  route.input_channel_count, route.input_channel_map)) {
    setFailure(route, AudioRouteCompatibilityStatus::InvalidChannelMap, "map", false);
    return route;
  }

  bool output_rate_supported = false;
  const auto output_ranges = query_->advertisedSampleRateRanges(route.output_device_id);
  if (setQueryFailure(route, output_ranges.status, "ranges", true, false)) {
    return route;
  }
  output_rate_supported = supportsRequestedRate(output_ranges.value, config.sample_rate);
  if (!output_rate_supported) {
    setFailure(route, AudioRouteCompatibilityStatus::SampleRateUnsupported, "ranges", true);
    return route;
  }

  bool input_rate_supported = true;
  if (config.num_inputs > 0 && route.input_device_id != route.output_device_id) {
    const auto input_ranges = query_->advertisedSampleRateRanges(route.input_device_id);
    if (setQueryFailure(route, input_ranges.status, "ranges", false, false)) {
      return route;
    }
    input_rate_supported = supportsRequestedRate(input_ranges.value, config.sample_rate);
    if (!input_rate_supported) {
      setFailure(route, AudioRouteCompatibilityStatus::SampleRateUnsupported, "ranges", false);
      return route;
    }
  }

  const auto query_global_facts = [&](AudioDeviceID device_id, bool output, uint32_t& rate,
                                      bool& running) -> bool {
    const auto current = query_->currentSampleRate(device_id);
    if (setQueryFailure(route, current.status, "current_rate", output, false)) {
      return false;
    }
    if (!normalizeCurrentSampleRate(current.value, rate)) {
      setFailure(route, AudioRouteCompatibilityStatus::BackendFailure, "current_rate", output);
      return false;
    }
    const auto is_running = query_->isRunningSomewhere(device_id);
    if (setQueryFailure(route, is_running.status, "running", output, false)) {
      return false;
    }
    running = is_running.value;
    return true;
  };

  uint32_t output_current_rate = 0;
  bool output_running = false;
  if (!query_global_facts(route.output_device_id, true, output_current_rate, output_running)) {
    return route;
  }

  uint32_t input_current_rate = 0;
  bool input_running = false;
  if (config.num_inputs > 0) {
    if (route.input_device_id == route.output_device_id) {
      input_current_rate = output_current_rate;
      input_running = output_running;
    } else if (!query_global_facts(route.input_device_id, false, input_current_rate,
                                   input_running)) {
      return route;
    }
  }

  route.compatibility.current_output_sample_rate = output_current_rate;
  route.compatibility.output_is_running_somewhere = output_running;
  route.compatibility.output_rate_change_required = output_current_rate != config.sample_rate;
  if (config.num_inputs > 0) {
    route.compatibility.current_input_sample_rate = input_current_rate;
    route.compatibility.input_is_running_somewhere = input_running;
    route.compatibility.input_rate_change_required = input_current_rate != config.sample_rate;
  }

  const bool rate_change_required = route.compatibility.output_rate_change_required ||
                                    route.compatibility.input_rate_change_required;
  route.compatibility.status =
      rate_change_required && config.sample_rate_policy == AudioSampleRatePolicy::RequestExactRate
          ? AudioRouteCompatibilityStatus::RequiresSampleRateChange
          : AudioRouteCompatibilityStatus::Compatible;
  route.compatibility.detail.clear();
  route.resolved = true;
  route.requires_private_aggregate =
      config.num_inputs > 0 && route.input_device_id != route.output_device_id;
  (void)input_rate_supported;
  return route;
}

std::shared_ptr<const ICoreAudioRouteQuery> makeCoreAudioRouteQuery() {
  return std::make_shared<ProductionCoreAudioRouteQuery>();
}

} // namespace orpheus::detail
