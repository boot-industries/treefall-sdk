// SPDX-License-Identifier: MIT
#include "coreaudio/coreaudio_endpoint_catalog.h"

#include <gtest/gtest.h>

#include <vector>

namespace orpheus {
namespace {

using detail::CoreAudioEndpointFacts;
using detail::CoreAudioEndpointRange;
using detail::makeCoreAudioEndpointCapabilities;

TEST(CoreAudioEndpointCatalogTest, PreservesOutputOnlyEndpointsAndDuplicateNames) {
  const std::vector<CoreAudioEndpointFacts> facts = {
      {.device_id = "uid-output-a",
       .display_name = "Duplicate Interface",
       .is_default_output = true,
       .output_channel_count = 2,
       .sample_rate_ranges = {{44100.0, 48000.0}},
       .buffer_size_ranges = {{64.0, 512.0}},
       .nominal_sample_rate = 48000,
       .current_buffer_size = 256},
      {.device_id = "uid-output-b",
       .display_name = "Duplicate Interface",
       .output_channel_count = 1,
       .sample_rate_ranges = {{96000.0, 192000.0}},
       .buffer_size_ranges = {{128.0, 1024.0}},
       .nominal_sample_rate = 96000,
       .current_buffer_size = 1024},
  };

  std::vector<AudioEndpointCapabilities> endpoints;
  endpoints.reserve(facts.size());
  for (const auto& endpoint : facts) {
    endpoints.push_back(makeCoreAudioEndpointCapabilities(endpoint));
  }

  ASSERT_EQ(endpoints.size(), 2u);
  EXPECT_EQ(endpoints[0].device_id, "uid-output-a");
  EXPECT_EQ(endpoints[1].device_id, "uid-output-b");
  EXPECT_EQ(endpoints[0].display_name, endpoints[1].display_name);
  EXPECT_FALSE(endpoints[0].supports_input);
  EXPECT_TRUE(endpoints[0].supports_output);
  EXPECT_TRUE(endpoints[0].is_default_output);
  ASSERT_EQ(endpoints[0].output_channels.size(), 2u);
  EXPECT_EQ(endpoints[0].output_channels[0].stable_id, "uid-output-a:output:0");
  EXPECT_EQ(endpoints[0].output_channels[1].device_index, 1u);
  EXPECT_EQ(endpoints[0].supported_buffer_sizes,
            (std::vector<uint32_t>{64, 96, 128, 160, 192, 256, 384, 512}));
  EXPECT_EQ(endpoints[0].current_buffer_size, 256u);
  EXPECT_EQ(endpoints[1].output_channels[0].stable_id, "uid-output-b:output:0");
  EXPECT_EQ(endpoints[1].supported_sample_rates, (std::vector<uint32_t>{96000, 176400, 192000}));
  EXPECT_EQ(endpoints[1].supported_buffer_sizes,
            (std::vector<uint32_t>{128, 160, 192, 256, 384, 512, 768, 1024}));
}

TEST(CoreAudioEndpointCatalogTest, ReportsFormatUnavailableWithoutUsableChannels) {
  const CoreAudioEndpointFacts facts{
      .device_id = "uid-no-format",
      .display_name = "No Format",
      .sample_rate_ranges = {{48000.0, 48000.0}},
      .buffer_size_ranges = {{512.0, 512.0}},
      .nominal_sample_rate = 48000,
      .current_buffer_size = 512,
  };

  const auto endpoint = makeCoreAudioEndpointCapabilities(facts);
  EXPECT_EQ(endpoint.availability, AudioEndpointAvailability::FormatUnavailable);
  EXPECT_FALSE(endpoint.supports_input);
  EXPECT_FALSE(endpoint.supports_output);
  EXPECT_TRUE(endpoint.input_channels.empty());
  EXPECT_TRUE(endpoint.output_channels.empty());
  EXPECT_EQ(endpoint.supported_sample_rates, (std::vector<uint32_t>{48000}));
  EXPECT_EQ(endpoint.supported_buffer_sizes, (std::vector<uint32_t>{512}));
}
TEST(CoreAudioEndpointCatalogTest, ProjectsRunningSomewhereWithoutChangingAvailability) {
  CoreAudioEndpointFacts running;
  running.device_id = "running";
  running.display_name = "Running";
  running.output_channel_count = 2;
  running.is_running_somewhere = true;

  CoreAudioEndpointFacts stopped = running;
  stopped.device_id = "stopped";
  stopped.is_running_somewhere = false;

  const auto running_endpoint = makeCoreAudioEndpointCapabilities(running);
  const auto stopped_endpoint = makeCoreAudioEndpointCapabilities(stopped);

  EXPECT_TRUE(running_endpoint.is_running_somewhere);
  EXPECT_FALSE(stopped_endpoint.is_running_somewhere);
  EXPECT_EQ(running_endpoint.availability, AudioEndpointAvailability::Available);
  EXPECT_EQ(stopped_endpoint.availability, AudioEndpointAvailability::Available);
}

} // namespace
} // namespace orpheus
