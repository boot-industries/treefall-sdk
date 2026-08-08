// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/audio_driver_manager.h>

#include <cstdint>
#include <string>
#include <vector>

namespace orpheus::detail {

/// CoreAudio properties normalized for capability projection and deterministic tests.
/// This private seam keeps CoreAudio property-query types out of the public API.
struct CoreAudioEndpointRange {
  double minimum = 0.0;
  double maximum = 0.0;
};

struct CoreAudioEndpointFacts {
  std::string device_id;
  std::string display_name;
  bool is_default_input = false;
  bool is_default_output = false;
  uint32_t input_channel_count = 0;
  uint32_t output_channel_count = 0;
  std::vector<CoreAudioEndpointRange> sample_rate_ranges;
  std::vector<CoreAudioEndpointRange> buffer_size_ranges;
  uint32_t nominal_sample_rate = 0;
  uint32_t current_buffer_size = 0;
  bool is_running_somewhere = false;
};

AudioEndpointCapabilities makeCoreAudioEndpointCapabilities(const CoreAudioEndpointFacts& facts);

} // namespace orpheus::detail
