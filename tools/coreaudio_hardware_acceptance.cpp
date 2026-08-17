// SPDX-License-Identifier: MIT
#if defined(__APPLE__)

#include <orpheus/audio_driver.h>
#include <orpheus/errors.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Options {
  std::string output_uid;
  std::optional<std::string> input_uid;
  uint32_t sample_rate = 48000;
  uint32_t seconds = 10;
  bool allow_mono_fallback = false;
  std::optional<double> expected_input_tone_hz;
  orpheus::AudioRouteRuntimeOutcome expected_route_outcome =
      orpheus::AudioRouteRuntimeOutcome::Healthy;
};

std::string json_escape(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size() + 2);
  for (const char character : value) {
    switch (character) {
    case '\\':
      escaped += "\\\\";
      break;
    case '"':
      escaped += "\\\"";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\r':
      escaped += "\\r";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      if (static_cast<unsigned char>(character) < 0x20) {
        escaped += '?';
      } else {
        escaped += character;
      }
      break;
    }
  }
  return escaped;
}
[[noreturn]] void print_error(const std::string& reason, int exit_code) {
  std::cout << "{\"status\":\"failed\",\"reason\":\"" << json_escape(reason) << "\"}\n";
  std::exit(exit_code);
}
orpheus::AudioRouteRuntimeOutcome parse_route_outcome(const std::string& value) {
  if (value == "healthy") {
    return orpheus::AudioRouteRuntimeOutcome::Healthy;
  }
  if (value == "sample-rate-changed") {
    return orpheus::AudioRouteRuntimeOutcome::SampleRateChanged;
  }
  if (value == "buffer-size-changed") {
    return orpheus::AudioRouteRuntimeOutcome::BufferSizeChanged;
  }
  if (value == "format-changed") {
    return orpheus::AudioRouteRuntimeOutcome::FormatChanged;
  }
  if (value == "input-route-unavailable") {
    return orpheus::AudioRouteRuntimeOutcome::InputRouteUnavailable;
  }
  if (value == "output-route-unavailable") {
    return orpheus::AudioRouteRuntimeOutcome::OutputRouteUnavailable;
  }
  print_error("invalid-expect-route-outcome", 2);
}

uint32_t parse_uint32(const std::string& value, const char* option) {
  try {
    size_t consumed = 0;
    const unsigned long parsed = std::stoul(value, &consumed, 10);
    if (consumed != value.size() || parsed > UINT32_MAX) {
      throw std::invalid_argument("range");
    }
    return static_cast<uint32_t>(parsed);
  } catch (const std::exception&) {
    print_error(std::string("invalid-") + option, 2);
  }
}

Options parse_options(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    auto require_value = [&](const char* option) -> std::string {
      if (index + 1 >= argc) {
        print_error(std::string("missing-") + option, 2);
      }
      return argv[++index];
    };
    if (argument == "--help") {
      std::cout << "usage: orpheus_coreaudio_hardware_acceptance --output-uid UID "
                   "[--input-uid UID] [--sample-rate 44100|48000] [--seconds N] "
                   "[--output-policy strict|mono-fallback] "
                   "[--expect-input-tone-hz HZ] "
                   "[--expect-route-outcome healthy|sample-rate-changed|buffer-size-changed|"
                   "format-changed|input-route-unavailable|output-route-unavailable]\n";
      std::exit(0);
    }
    if (argument == "--output-uid") {
      options.output_uid = require_value("output-uid");
    } else if (argument == "--input-uid") {
      options.input_uid = require_value("input-uid");
    } else if (argument == "--sample-rate") {
      options.sample_rate = parse_uint32(require_value("sample-rate"), "sample-rate");
    } else if (argument == "--seconds") {
      options.seconds = parse_uint32(require_value("seconds"), "seconds");
    } else if (argument == "--output-policy") {
      const auto policy = require_value("output-policy");
      if (policy == "strict") {
        options.allow_mono_fallback = false;
      } else if (policy == "mono-fallback") {
        options.allow_mono_fallback = true;
      } else {
        print_error("invalid-output-policy", 2);
      }
    } else if (argument == "--expect-input-tone-hz") {
      try {
        size_t consumed = 0;
        const double expected = std::stod(require_value("expect-input-tone-hz"), &consumed);
        if (!std::isfinite(expected) || expected <= 0.0 || consumed == 0) {
          throw std::invalid_argument("frequency");
        }
        options.expected_input_tone_hz = expected;
      } catch (const std::exception&) {
        print_error("invalid-expect-input-tone-hz", 2);
      }
    } else if (argument == "--expect-route-outcome") {
      options.expected_route_outcome = parse_route_outcome(require_value("expect-route-outcome"));
    } else {
      print_error("unknown-option", 2);
    }
  }
  if (options.output_uid.empty()) {
    print_error("missing-output-uid", 2);
  }
  if (options.sample_rate != 44100 && options.sample_rate != 48000) {
    print_error("sample-rate-must-be-44100-or-48000", 2);
  }
  if (options.seconds == 0) {
    print_error("seconds-must-be-positive", 2);
  }
  return options;
}

class AcceptanceCallback final : public orpheus::IAudioCallback {
public:
  void processAudio(const orpheus::AudioProcessBlock& block) noexcept override {
    observeWidth(first_input_width_, input_width_changed_, block.num_input_channels);
    observeWidth(first_output_width_, output_width_changed_, block.num_output_channels);
    for (uint16_t channel = 0; channel < block.num_output_channels; ++channel) {
      if (block.output_buffers != nullptr && block.output_buffers[channel] != nullptr) {
        std::fill_n(block.output_buffers[channel], block.num_frames, 0.0f);
      }
    }
    callbacks_.fetch_add(1, std::memory_order_relaxed);
    callback_frames_.fetch_add(block.num_frames, std::memory_order_relaxed);
    if (block.input_buffers == nullptr || block.num_input_channels == 0 || block.num_frames == 0 ||
        block.input_buffers[0] == nullptr) {
      return;
    }
    const float* input = block.input_buffers[0];
    float previous = previous_input_sample_;
    for (uint32_t frame = 0; frame < block.num_frames; ++frame) {
      const float sample = input[frame];
      peak_ = std::max(peak_, std::fabs(sample));
      if ((previous < 0.0f && sample >= 0.0f) || (previous >= 0.0f && sample < 0.0f)) {
        zero_crossings_.fetch_add(1, std::memory_order_relaxed);
      }
      previous = sample;
    }
    previous_input_sample_ = previous;
    input_callbacks_.fetch_add(1, std::memory_order_relaxed);
    input_frames_.fetch_add(block.num_frames, std::memory_order_relaxed);
  }

  uint64_t callbacks() const noexcept {
    return callbacks_.load(std::memory_order_relaxed);
  }
  uint64_t callback_frames() const noexcept {
    return callback_frames_.load(std::memory_order_relaxed);
  }
  uint64_t input_callbacks() const noexcept {
    return input_callbacks_.load(std::memory_order_relaxed);
  }
  uint64_t input_frames() const noexcept {
    return input_frames_.load(std::memory_order_relaxed);
  }
  uint64_t zero_crossings() const noexcept {
    return zero_crossings_.load(std::memory_order_relaxed);
  }
  uint16_t first_input_width() const noexcept {
    return decodeWidth(first_input_width_);
  }
  uint16_t first_output_width() const noexcept {
    return decodeWidth(first_output_width_);
  }
  bool callback_width_changed() const noexcept {
    return input_width_changed_.load(std::memory_order_relaxed) ||
           output_width_changed_.load(std::memory_order_relaxed);
  }
  float peak() const noexcept {
    return peak_;
  }

private:
  static void observeWidth(std::atomic<uint32_t>& first_width, std::atomic<bool>& changed,
                           uint16_t width) noexcept {
    const uint32_t encoded = static_cast<uint32_t>(width) + 1;
    uint32_t expected = 0;
    if (first_width.compare_exchange_strong(expected, encoded, std::memory_order_relaxed,
                                            std::memory_order_relaxed)) {
      return;
    }
    if (expected != encoded) {
      changed.store(true, std::memory_order_relaxed);
    }
  }

  static uint16_t decodeWidth(const std::atomic<uint32_t>& first_width) noexcept {
    const uint32_t encoded = first_width.load(std::memory_order_relaxed);
    return encoded == 0 ? 0 : static_cast<uint16_t>(encoded - 1);
  }

  std::atomic<uint64_t> callbacks_{0};
  std::atomic<uint64_t> callback_frames_{0};
  std::atomic<uint64_t> input_callbacks_{0};
  std::atomic<uint64_t> input_frames_{0};
  std::atomic<uint64_t> zero_crossings_{0};
  std::atomic<uint32_t> first_input_width_{0};
  std::atomic<uint32_t> first_output_width_{0};
  std::atomic<bool> input_width_changed_{false};
  std::atomic<bool> output_width_changed_{false};
  float previous_input_sample_ = 0.0f;
  float peak_ = 0.0f;
};

const char* route_outcome_name(orpheus::AudioRouteRuntimeOutcome outcome) {
  switch (outcome) {
  case orpheus::AudioRouteRuntimeOutcome::Healthy:
    return "Healthy";
  case orpheus::AudioRouteRuntimeOutcome::SampleRateUnsupported:
    return "SampleRateUnsupported";
  case orpheus::AudioRouteRuntimeOutcome::SampleRateChangeFailed:
    return "SampleRateChangeFailed";
  case orpheus::AudioRouteRuntimeOutcome::SampleRateChanged:
    return "SampleRateChanged";
  case orpheus::AudioRouteRuntimeOutcome::BufferSizeChanged:
    return "BufferSizeChanged";
  case orpheus::AudioRouteRuntimeOutcome::FormatChanged:
    return "FormatChanged";
  case orpheus::AudioRouteRuntimeOutcome::ChannelMapInvalid:
    return "ChannelMapInvalid";
  case orpheus::AudioRouteRuntimeOutcome::InputRouteUnavailable:
    return "InputRouteUnavailable";
  case orpheus::AudioRouteRuntimeOutcome::OutputRouteUnavailable:
    return "OutputRouteUnavailable";
  case orpheus::AudioRouteRuntimeOutcome::PermissionDenied:
    return "PermissionDenied";
  case orpheus::AudioRouteRuntimeOutcome::ProfileConflict:
    return "ProfileConflict";
  case orpheus::AudioRouteRuntimeOutcome::InputConversionFailed:
    return "InputConversionFailed";
  case orpheus::AudioRouteRuntimeOutcome::OutputConversionFailed:
    return "OutputConversionFailed";
  case orpheus::AudioRouteRuntimeOutcome::BackendFailure:
    return "BackendFailure";
  }
  return "Unknown";
}

std::string output_policy_name(bool allow_mono_fallback) {
  return allow_mono_fallback ? "mono-fallback" : "strict";
}

void append_string(std::ostringstream& json, const char* key, const std::string& value,
                   bool& first) {
  if (!first) {
    json << ',';
  }
  first = false;
  json << '"' << key << "\":\"" << json_escape(value) << '"';
}

void append_string(std::ostringstream& json, const char* key, const char* value, bool& first) {
  append_string(json, key, std::string(value), first);
}

template <typename T>
void append_number(std::ostringstream& json, const char* key, T value, bool& first) {
  if (!first) {
    json << ',';
  }
  first = false;
  json << '"' << key << "\":" << value;
}

void append_bool(std::ostringstream& json, const char* key, bool value, bool& first) {
  if (!first) {
    json << ',';
  }
  first = false;
  json << '"' << key << "\":" << (value ? "true" : "false");
}

void append_map(std::ostringstream& json, const char* key, const std::vector<uint16_t>& map,
                bool& first) {
  if (!first) {
    json << ',';
  }
  first = false;
  json << '"' << key << "\":[";
  for (size_t index = 0; index < map.size(); ++index) {
    if (index != 0) {
      json << ',';
    }
    json << map[index];
  }
  json << ']';
}

template <typename T>
void append_prefixed_number(std::ostringstream& json, const std::string& prefix, const char* suffix,
                            T value, bool& first) {
  const std::string key = prefix + suffix;
  append_number(json, key.c_str(), value, first);
}

void append_prefixed_string(std::ostringstream& json, const std::string& prefix, const char* suffix,
                            const std::string& value, bool& first) {
  const std::string key = prefix + suffix;
  append_string(json, key.c_str(), value, first);
}

void append_prefixed_bool(std::ostringstream& json, const std::string& prefix, const char* suffix,
                          bool value, bool& first) {
  const std::string key = prefix + suffix;
  append_bool(json, key.c_str(), value, first);
}

void append_prefixed_map(std::ostringstream& json, const std::string& prefix, const char* suffix,
                         const std::vector<uint16_t>& value, bool& first) {
  const std::string key = prefix + suffix;
  append_map(json, key.c_str(), value, first);
}

void append_route_facts(std::ostringstream& json, const std::string& prefix,
                        const orpheus::ActiveAudioRoute& active,
                        const orpheus::AudioIoRouteState& io_route, bool& first) {
  append_prefixed_string(json, prefix, "active_input_uid", active.input_device_id, first);
  append_prefixed_string(json, prefix, "active_output_uid", active.output_device_id, first);
  append_prefixed_number(json, prefix, "requested_session_host_callback_rate",
                         active.requested_sample_rate, first);
  append_prefixed_number(json, prefix, "session_host_callback_rate", active.actual_sample_rate,
                         first);
  append_prefixed_number(json, prefix, "session_host_callback_buffer_frames",
                         active.actual_buffer_frames, first);
  append_prefixed_number(json, prefix, "physical_input_rate", active.input_physical_sample_rate,
                         first);
  append_prefixed_number(json, prefix, "physical_output_rate", active.output_physical_sample_rate,
                         first);
  append_prefixed_number(json, prefix, "input_auhal_client_rate", active.input_client_sample_rate,
                         first);
  append_prefixed_number(json, prefix, "output_auhal_client_rate", active.output_client_sample_rate,
                         first);
  append_prefixed_number(json, prefix, "available_input_channels", active.available_input_channels,
                         first);
  append_prefixed_number(json, prefix, "available_output_channels",
                         active.available_output_channels, first);
  append_prefixed_number(json, prefix, "requested_output_channels",
                         active.requested_output_channels, first);
  append_prefixed_number(json, prefix, "resolved_output_channels", active.resolved_output_channels,
                         first);
  append_prefixed_number(json, prefix, "input_auhal_client_format_channels",
                         active.input_client_format_channels, first);
  append_prefixed_number(json, prefix, "output_auhal_client_format_channels",
                         active.output_client_format_channels, first);
  append_prefixed_bool(json, prefix, "input_sample_rate_conversion_active",
                       active.input_conversion_active, first);
  append_prefixed_bool(json, prefix, "output_sample_rate_conversion_active",
                       active.output_conversion_active, first);
  append_prefixed_bool(json, prefix, "input_is_bluetooth", active.input_is_bluetooth, first);
  append_prefixed_bool(json, prefix, "output_is_bluetooth", active.output_is_bluetooth, first);
  append_prefixed_bool(json, prefix, "endpoints_related", active.endpoints_related, first);
  append_prefixed_bool(json, prefix, "output_mono_fallback", active.output_mono_fallback, first);
  append_prefixed_map(json, prefix, "active_output_map", active.output_channels, first);
  append_prefixed_number(json, prefix, "capture_latency_frames", active.latency.capture_frames,
                         first);
  append_prefixed_number(json, prefix, "playback_latency_frames", active.latency.playback_frames,
                         first);
  append_prefixed_number(json, prefix, "processing_latency_frames",
                         active.latency.processing_frames, first);
  append_prefixed_bool(json, prefix, "latency_complete", active.latency.complete, first);
  append_prefixed_number(json, prefix, "input_converter_latency_frames",
                         io_route.latency.input_converter_frames.value_or(0), first);
  append_prefixed_number(json, prefix, "output_converter_latency_frames",
                         io_route.latency.output_converter_frames.value_or(0), first);
  append_prefixed_string(json, prefix, "route_detail", io_route.detail, first);
}

void append_telemetry(std::ostringstream& json, const std::string& prefix,
                      const orpheus::AudioIoTelemetry& telemetry, bool& first) {
  append_prefixed_number(json, prefix, "input_render_failures", telemetry.input_render_failures,
                         first);
  append_prefixed_number(json, prefix, "input_fifo_overruns", telemetry.input_fifo_overruns, first);
  append_prefixed_number(json, prefix, "input_fifo_underruns", telemetry.input_fifo_underruns,
                         first);
  append_prefixed_number(json, prefix, "input_conversion_failures",
                         telemetry.input_conversion_failures, first);
  append_prefixed_number(json, prefix, "output_conversion_failures",
                         telemetry.output_conversion_failures, first);
  append_prefixed_string(json, prefix, "route_outcome", route_outcome_name(telemetry.route_outcome),
                         first);
}

} // namespace

int main(int argc, char** argv) {
  const Options options = parse_options(argc, argv);
  auto driver = orpheus::createCoreAudioDriver();
  if (!driver) {
    print_error("coreaudio-driver-unavailable", 3);
  }

  orpheus::AudioDriverConfig config;
  config.sample_rate = options.sample_rate;
  config.buffer_size = 512;
  config.num_inputs = options.input_uid.has_value() ? 1 : 0;
  config.input_device_id = options.input_uid.value_or("");
  config.num_outputs = 2;
  config.output_device_id = options.output_uid;
  config.channel_map.input_channels =
      options.input_uid.has_value() ? std::vector<uint16_t>{0} : std::vector<uint16_t>{};
  config.channel_map.output_channels = {0, 1};
  config.sample_rate_policy = orpheus::AudioSampleRatePolicy::RequestExactRateOrConvert;
  config.output_channel_policy = options.allow_mono_fallback
                                     ? orpheus::AudioOutputChannelPolicy::AllowMonoFallback
                                     : orpheus::AudioOutputChannelPolicy::RequireRequestedChannels;

  const auto initialize_result = driver->initialize(config);
  const auto initialize_io_route = driver->getAudioIoRouteState();
  const auto initialize_telemetry = driver->getTelemetry();
  if (initialize_result != orpheus::SessionGraphError::OK) {
    std::ostringstream json;
    json << "{\"status\":\"failed\",\"reason\":\"initialize\",\"expected_route_outcome\":\""
         << route_outcome_name(options.expected_route_outcome) << "\",\"route_outcome\":\""
         << route_outcome_name(initialize_telemetry.route_outcome) << "\",\"detail\":\""
         << json_escape(initialize_io_route.detail) << "\"}\n";
    std::cout << json.str();
    return 4;
  }

  AcceptanceCallback callback;
  const auto start_result = driver->start(&callback);
  if (start_result != orpheus::SessionGraphError::OK) {
    const auto telemetry = driver->getTelemetry();
    std::ostringstream json;
    json << "{\"status\":\"failed\",\"reason\":\"start\",\"expected_route_outcome\":\""
         << route_outcome_name(options.expected_route_outcome) << "\",\"route_outcome\":\""
         << route_outcome_name(telemetry.route_outcome) << "\",\"detail\":\""
         << json_escape(driver->getAudioIoRouteState().detail) << "\"}\n";
    std::cout << json.str();
    return 5;
  }

  // Capture the post-start settled route before the acceptance window mutates anything.
  const auto settled_active = driver->getActiveRoute();
  const auto settled_io_route = driver->getAudioIoRouteState();
  const auto settled_telemetry = driver->getTelemetry();

  std::this_thread::sleep_for(std::chrono::seconds(options.seconds));
  const auto stop_result = driver->stop();
  const auto negotiated = driver->getActiveRoute();
  const auto final_io_route = driver->getAudioIoRouteState();
  const auto telemetry = driver->getTelemetry();
  const auto route_outcome = telemetry.route_outcome;
  const uint64_t expected_frames = static_cast<uint64_t>(options.sample_rate) * options.seconds;
  const int64_t callback_frame_delta =
      static_cast<int64_t>(callback.callback_frames()) - static_cast<int64_t>(expected_frames);
  const int64_t input_frame_delta =
      static_cast<int64_t>(callback.input_frames()) - static_cast<int64_t>(expected_frames);
  const double zero_crossing_frequency =
      callback.input_frames() == 0 ? 0.0
                                   : static_cast<double>(callback.zero_crossings()) *
                                         static_cast<double>(options.sample_rate) /
                                         (2.0 * static_cast<double>(callback.input_frames()));
  const bool startup_counters_clear = settled_telemetry.input_render_failures == 0 &&
                                      settled_telemetry.input_fifo_overruns == 0 &&
                                      settled_telemetry.input_fifo_underruns == 0 &&
                                      settled_telemetry.input_conversion_failures == 0 &&
                                      settled_telemetry.output_conversion_failures == 0;
  const bool final_counters_clear =
      telemetry.input_render_failures == 0 && telemetry.input_fifo_overruns == 0 &&
      telemetry.input_fifo_underruns == 0 && telemetry.input_conversion_failures == 0 &&
      telemetry.output_conversion_failures == 0;
  const bool input_activity_matches = options.input_uid.has_value()
                                          ? callback.input_callbacks() > 0
                                          : callback.input_callbacks() == 0;
  const bool tone_in_tolerance =
      !options.expected_input_tone_hz.has_value() ||
      (callback.input_frames() != 0 &&
       std::abs(zero_crossing_frequency - *options.expected_input_tone_hz) <= 20.0);
  const bool route_matches = route_outcome == options.expected_route_outcome;
  const bool widths_stable = !callback.callback_width_changed();
  const bool passed =
      stop_result == orpheus::SessionGraphError::OK && callback.callbacks() > 0 && route_matches &&
      startup_counters_clear && final_counters_clear && input_activity_matches &&
      tone_in_tolerance &&
      (options.expected_route_outcome != orpheus::AudioRouteRuntimeOutcome::Healthy ||
       widths_stable);

  std::ostringstream json;
  json << std::setprecision(10) << '{';
  bool first = true;
  append_string(json, "status", passed ? "passed" : "failed", first);
  append_string(json, "backend", "CoreAudio", first);
  append_string(json, "input_uid", options.input_uid.value_or(""), first);
  append_string(json, "output_uid", options.output_uid, first);
  append_string(json, "expected_route_outcome", route_outcome_name(options.expected_route_outcome),
                first);
  append_number(json, "seconds", options.seconds, first);
  append_number(json, "requested_frames", expected_frames, first);
  append_string(json, "output_policy", output_policy_name(options.allow_mono_fallback), first);
  append_route_facts(json, "settled_", settled_active, settled_io_route, first);
  append_telemetry(json, "startup_", settled_telemetry, first);
  append_route_facts(json, "", negotiated, final_io_route, first);
  append_number(json, "callbacks", callback.callbacks(), first);
  append_number(json, "callback_frames", callback.callback_frames(), first);
  append_number(json, "input_callbacks", callback.input_callbacks(), first);
  append_number(json, "input_frames", callback.input_frames(), first);
  append_number(json, "callback_frame_delta", callback_frame_delta, first);
  append_number(json, "input_frame_delta", input_frame_delta, first);
  append_number(json, "first_input_callback_width", callback.first_input_width(), first);
  append_number(json, "first_output_callback_width", callback.first_output_width(), first);
  append_bool(json, "callback_width_changed", callback.callback_width_changed(), first);
  append_bool(json, "callback_width_stable", widths_stable, first);
  append_bool(json, "input_activity_matches", input_activity_matches, first);
  append_bool(json, "startup_counters_clear", startup_counters_clear, first);
  append_bool(json, "final_counters_clear", final_counters_clear, first);
  append_telemetry(json, "", telemetry, first);
  append_number(json, "input_peak", callback.peak(), first);
  append_number(json, "zero_crossing_frequency_hz", zero_crossing_frequency, first);
  if (options.expected_input_tone_hz.has_value()) {
    append_number(json, "expected_input_tone_hz", *options.expected_input_tone_hz, first);
    append_bool(json, "input_tone_in_tolerance", tone_in_tolerance, first);
  }
  append_bool(json, "passed", passed, first);
  json << "}\n";
  std::cout << json.str();
  return passed ? 0 : 6;
}

#else
int main() {
  return 1;
}
#endif
