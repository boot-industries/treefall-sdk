// SPDX-License-Identifier: MIT
#pragma once

#include "coreaudio_endpoint_catalog.h"
#include <orpheus/audio_driver.h>

#include <CoreAudio/CoreAudio.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace orpheus::detail {

enum class CoreAudioRouteQueryStatus : uint8_t {
  Success,
  Missing,
  PermissionDenied,
  Failure,
};

template <typename T> struct CoreAudioRouteQueryResult {
  CoreAudioRouteQueryStatus status = CoreAudioRouteQueryStatus::Failure;
  T value{};
};

struct ResolvedCoreAudioEndpoint {
  AudioDeviceID device_id = 0;
  std::string device_uid;
};

struct CoreAudioStreamFormat {
  uint32_t sample_rate = 0;
  uint16_t channels = 0;
};

struct CoreAudioDeviceRatePlan {
  AudioDeviceID device_id = 0;
  uint32_t observed_rate = 0;
  std::optional<uint32_t> requested_write_rate;
  bool input_uses_external_src = false;
  bool output_uses_external_src = false;
};

class ICoreAudioRouteQuery {
public:
  virtual ~ICoreAudioRouteQuery() = default;

  virtual CoreAudioRouteQueryResult<ResolvedCoreAudioEndpoint>
  resolveDefault(bool output) const = 0;
  virtual CoreAudioRouteQueryResult<ResolvedCoreAudioEndpoint>
  resolveDeviceUID(std::string_view device_uid) const = 0;
  virtual CoreAudioRouteQueryResult<uint32_t>
  channelCount(AudioDeviceID device_id, AudioObjectPropertyScope scope) const = 0;
  virtual CoreAudioRouteQueryResult<std::vector<CoreAudioEndpointRange>>
  advertisedSampleRateRanges(AudioDeviceID device_id) const = 0;
  virtual CoreAudioRouteQueryResult<double> currentSampleRate(AudioDeviceID device_id) const = 0;
  virtual CoreAudioRouteQueryResult<bool> isRunningSomewhere(AudioDeviceID device_id) const = 0;

  virtual CoreAudioRouteQueryResult<uint32_t> transportType(AudioDeviceID device_id) const = 0;
  virtual CoreAudioRouteQueryResult<std::vector<AudioDeviceID>>
  relatedDeviceIDs(AudioDeviceID device_id) const = 0;
  virtual CoreAudioRouteQueryResult<CoreAudioStreamFormat>
  physicalStreamFormat(AudioDeviceID device_id, AudioObjectPropertyScope scope) const = 0;
  virtual CoreAudioRouteQueryResult<CoreAudioStreamFormat>
  virtualStreamFormat(AudioDeviceID device_id, AudioObjectPropertyScope scope) const = 0;
  virtual CoreAudioRouteQueryResult<bool>
  nominalSampleRateSettable(AudioDeviceID device_id) const = 0;
  virtual CoreAudioRouteQueryResult<uint32_t>
  physicalChannelCount(AudioDeviceID device_id, AudioObjectPropertyScope scope) const = 0;
};

struct ResolvedCoreAudioRoute {
  bool resolved = false;
  AudioRouteCompatibility compatibility;
  AudioDeviceID input_device_id = 0;
  AudioDeviceID output_device_id = 0;
  uint32_t input_channel_count = 0;
  uint32_t output_channel_count = 0;
  std::vector<uint16_t> input_channel_map;
  std::vector<uint16_t> output_channel_map;
  std::vector<CoreAudioDeviceRatePlan> device_rate_plans;
  CoreAudioStreamFormat input_physical_format;
  CoreAudioStreamFormat output_physical_format;
  CoreAudioStreamFormat input_virtual_format;
  CoreAudioStreamFormat output_virtual_format;
  uint32_t input_transport_type = 0;
  uint32_t output_transport_type = 0;
  bool input_is_bluetooth = false;
  bool output_is_bluetooth = false;
  bool endpoints_related = false;
  bool output_mono_fallback = false;
  bool requires_private_aggregate = false;
};

bool resolveCoreAudioChannelMap(const std::vector<uint16_t>& requested, uint16_t logical_count,
                                uint32_t physical_count, std::vector<uint16_t>& resolved);

class CoreAudioRouteResolver {
public:
  explicit CoreAudioRouteResolver(std::shared_ptr<const ICoreAudioRouteQuery> query);
  ResolvedCoreAudioRoute resolve(const AudioDriverConfig& config,
                                 bool allow_rate_writes = true) const;

private:
  std::shared_ptr<const ICoreAudioRouteQuery> query_;
};

std::shared_ptr<const ICoreAudioRouteQuery> makeCoreAudioRouteQuery();

} // namespace orpheus::detail
