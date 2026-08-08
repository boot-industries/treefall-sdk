// SPDX-License-Identifier: MIT
#include "coreaudio_endpoint_catalog.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace orpheus::detail {
namespace {

constexpr std::array<uint32_t, 6> kCommonSampleRates = {44100, 48000, 88200, 96000, 176400, 192000};
constexpr std::array<uint32_t, 14> kCommonBufferSizes = {32,  64,  96,  128,  160,  192,  256,
                                                         384, 512, 768, 1024, 1536, 2048, 4096};

bool isInRange(double value, const CoreAudioEndpointRange& range) {
  return value >= range.minimum && value <= range.maximum;
}

void appendUnique(std::vector<uint32_t>& values, uint32_t value) {
  if (value != 0 && std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

void appendChannels(std::vector<AudioChannelDescriptor>& channels, uint32_t count,
                    const std::string& deviceId, const std::string& displayName,
                    const char* stableDirection, const char* displayDirection) {
  if (count > std::numeric_limits<uint16_t>::max()) {
    return;
  }
  channels.reserve(count);
  for (uint32_t channel = 0; channel < count; ++channel) {
    channels.push_back({static_cast<uint16_t>(channel),
                        deviceId + ":" + stableDirection + ":" + std::to_string(channel),
                        displayName + " " + displayDirection + " " + std::to_string(channel + 1)});
  }
}

} // namespace

AudioEndpointCapabilities makeCoreAudioEndpointCapabilities(const CoreAudioEndpointFacts& facts) {
  AudioEndpointCapabilities endpoint;
  endpoint.device_id = facts.device_id;
  endpoint.display_name = facts.display_name.empty() ? facts.device_id : facts.display_name;
  endpoint.is_default_input = facts.is_default_input;
  endpoint.is_default_output = facts.is_default_output;

  const bool channel_counts_representable =
      facts.input_channel_count <= std::numeric_limits<uint16_t>::max() &&
      facts.output_channel_count <= std::numeric_limits<uint16_t>::max();
  endpoint.supports_input = facts.input_channel_count != 0 && channel_counts_representable;
  endpoint.supports_output = facts.output_channel_count != 0 && channel_counts_representable;
  endpoint.availability = !channel_counts_representable
                              ? AudioEndpointAvailability::FormatUnavailable
                              : (endpoint.supports_input || endpoint.supports_output
                                     ? AudioEndpointAvailability::Available
                                     : AudioEndpointAvailability::FormatUnavailable);

  if (endpoint.supports_input) {
    appendChannels(endpoint.input_channels, facts.input_channel_count, facts.device_id,
                   endpoint.display_name, "input", "Input");
  }
  if (endpoint.supports_output) {
    appendChannels(endpoint.output_channels, facts.output_channel_count, facts.device_id,
                   endpoint.display_name, "output", "Output");
  }

  for (const uint32_t rate : kCommonSampleRates) {
    if (std::any_of(facts.sample_rate_ranges.begin(), facts.sample_rate_ranges.end(),
                    [rate](const CoreAudioEndpointRange& range) {
                      return isInRange(static_cast<double>(rate), range);
                    })) {
      endpoint.supported_sample_rates.push_back(rate);
    }
  }
  endpoint.nominal_sample_rate = facts.nominal_sample_rate;
  appendUnique(endpoint.supported_sample_rates, endpoint.nominal_sample_rate);
  std::sort(endpoint.supported_sample_rates.begin(), endpoint.supported_sample_rates.end());

  for (const uint32_t buffer : kCommonBufferSizes) {
    if (std::any_of(facts.buffer_size_ranges.begin(), facts.buffer_size_ranges.end(),
                    [buffer](const CoreAudioEndpointRange& range) {
                      return isInRange(static_cast<double>(buffer), range);
                    })) {
      endpoint.supported_buffer_sizes.push_back(buffer);
    }
  }
  endpoint.current_buffer_size = facts.current_buffer_size;
  endpoint.is_running_somewhere = facts.is_running_somewhere;

  appendUnique(endpoint.supported_buffer_sizes, endpoint.current_buffer_size);
  std::sort(endpoint.supported_buffer_sizes.begin(), endpoint.supported_buffer_sizes.end());

  return endpoint;
}

} // namespace orpheus::detail
