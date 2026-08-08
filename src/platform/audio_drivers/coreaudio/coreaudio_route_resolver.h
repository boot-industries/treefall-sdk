// SPDX-License-Identifier: MIT
#pragma once

#include "coreaudio_endpoint_catalog.h"
#include <orpheus/audio_driver.h>

#include <CoreAudio/CoreAudio.h>

#include <cstdint>
#include <memory>
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
  bool requires_private_aggregate = false;
};

bool resolveCoreAudioChannelMap(const std::vector<uint16_t>& requested, uint16_t logical_count,
                                uint32_t physical_count, std::vector<uint16_t>& resolved);

class CoreAudioRouteResolver {
public:
  explicit CoreAudioRouteResolver(std::shared_ptr<const ICoreAudioRouteQuery> query);
  ResolvedCoreAudioRoute resolve(const AudioDriverConfig& config) const;

private:
  std::shared_ptr<const ICoreAudioRouteQuery> query_;
};

std::shared_ptr<const ICoreAudioRouteQuery> makeCoreAudioRouteQuery();

} // namespace orpheus::detail
