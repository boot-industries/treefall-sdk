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
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

struct Options {
  std::string output_uid;
  std::optional<std::string> input_uid;
  uint32_t sample_rate = 48000;
  uint32_t seconds = 10;
  uint32_t grace_ms = 5000;
  bool allow_mono_fallback = false;
  std::optional<double> expected_input_tone_hz;
  std::optional<orpheus::AudioRouteRuntimeOutcome> expected_initialize_outcome;
  std::optional<orpheus::AudioRouteRuntimeOutcome> expected_terminal_outcome;
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

std::optional<orpheus::AudioRouteRuntimeOutcome> parse_route_outcome(std::string_view value) {
  using Outcome = orpheus::AudioRouteRuntimeOutcome;
  constexpr std::pair<std::string_view, Outcome> names[] = {
      {"Healthy", Outcome::Healthy},
      {"SampleRateUnsupported", Outcome::SampleRateUnsupported},
      {"SampleRateChangeFailed", Outcome::SampleRateChangeFailed},
      {"SampleRateChanged", Outcome::SampleRateChanged},
      {"BufferSizeChanged", Outcome::BufferSizeChanged},
      {"FormatChanged", Outcome::FormatChanged},
      {"ChannelMapInvalid", Outcome::ChannelMapInvalid},
      {"InputRouteUnavailable", Outcome::InputRouteUnavailable},
      {"OutputRouteUnavailable", Outcome::OutputRouteUnavailable},
      {"PermissionDenied", Outcome::PermissionDenied},
      {"BackendFailure", Outcome::BackendFailure},
      {"ProfileConflict", Outcome::ProfileConflict},
      {"InputConversionFailed", Outcome::InputConversionFailed},
      {"OutputConversionFailed", Outcome::OutputConversionFailed},
  };
  for (const auto& [name, outcome] : names) {
    if (name == value) {
      return outcome;
    }
  }
  return std::nullopt;
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
    auto parse_expected_outcome = [&](const char* option) -> orpheus::AudioRouteRuntimeOutcome {
      const auto value = require_value(option);
      const auto outcome = parse_route_outcome(value);
      if (!outcome.has_value()) {
        print_error(std::string("invalid-") + option, 2);
      }
      return *outcome;
    };
    if (argument == "--help") {
      std::cout << "usage: orpheus_coreaudio_hardware_acceptance --output-uid UID "
                   "[--input-uid UID] [--sample-rate 44100|48000] [--seconds N] "
                   "[--grace-ms N] [--output-policy strict|mono-fallback] "
                   "[--expect-input-tone-hz HZ] "
                   "[--expect-initialize-outcome OUTCOME] "
                   "[--expect-terminal-outcome OUTCOME]\n";
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
    } else if (argument == "--grace-ms") {
      options.grace_ms = parse_uint32(require_value("grace-ms"), "grace-ms");
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
    } else if (argument == "--expect-initialize-outcome") {
      options.expected_initialize_outcome = parse_expected_outcome("expect-initialize-outcome");
    } else if (argument == "--expect-terminal-outcome") {
      options.expected_terminal_outcome = parse_expected_outcome("expect-terminal-outcome");
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
  if (options.grace_ms == 0) {
    print_error("grace-ms-must-be-positive", 2);
  }
  if (options.expected_initialize_outcome.has_value() &&
      options.expected_terminal_outcome.has_value()) {
    print_error("initialize-and-terminal-expectations-are-exclusive", 2);
  }
  if (options.expected_terminal_outcome == orpheus::AudioRouteRuntimeOutcome::Healthy) {
    print_error("terminal-outcome-must-not-be-healthy", 2);
  }
  return options;
}

class AcceptanceCallback final : public orpheus::IAudioCallback {
public:
  AcceptanceCallback(float* capture, uint64_t capture_capacity)
      : capture_(capture), capture_capacity_(capture_capacity) {}

  void processAudio(const orpheus::AudioProcessBlock& block) noexcept override {
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

    const uint64_t capture_start = input_frames_.load(std::memory_order_relaxed);
    input_callbacks_.fetch_add(1, std::memory_order_relaxed);
    uint64_t copied = 0;
    if (capture_ != nullptr && capture_start < capture_capacity_) {
      copied = std::min<uint64_t>(block.num_frames, capture_capacity_ - capture_start);
      std::copy_n(block.input_buffers[0], static_cast<size_t>(copied), capture_ + capture_start);
    }
    if (copied < block.num_frames) {
      capture_overflow_frames_.fetch_add(block.num_frames - copied, std::memory_order_relaxed);
    }
    input_frames_.store(capture_start + block.num_frames, std::memory_order_release);
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
  uint64_t capture_overflow_frames() const noexcept {
    return capture_overflow_frames_.load(std::memory_order_relaxed);
  }

private:
  float* capture_ = nullptr;
  uint64_t capture_capacity_ = 0;
  std::atomic<uint64_t> callbacks_{0};
  std::atomic<uint64_t> callback_frames_{0};
  std::atomic<uint64_t> input_callbacks_{0};
  std::atomic<uint64_t> input_frames_{0};
  std::atomic<uint64_t> capture_overflow_frames_{0};
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
void append_optional_number(std::ostringstream& json, const char* key,
                            const std::optional<T>& value, bool& first) {
  if (!first) {
    json << ',';
  }
  first = false;
  json << '"' << key << "\":";
  if (value.has_value()) {
    json << *value;
  } else {
    json << "null";
  }
}

struct CaptureEvidence {
  std::optional<float> first_sample;
  std::optional<float> last_sample;
  double peak = 0.0;
  uint64_t zero_crossings = 0;
  uint64_t sequence_hash = 1469598103934665603ULL;
  bool finite = true;
};

CaptureEvidence inspect_capture(const float* samples, uint64_t frames) {
  CaptureEvidence evidence;
  if (samples == nullptr || frames == 0) {
    return evidence;
  }
  evidence.first_sample = samples[0];
  evidence.last_sample = samples[frames - 1];
  float previous = 0.0F;
  for (uint64_t frame = 0; frame < frames; ++frame) {
    const float sample = samples[frame];
    evidence.finite = evidence.finite && std::isfinite(sample);
    evidence.peak = std::max(evidence.peak, std::fabs(static_cast<double>(sample)));
    if ((previous < 0.0F && sample >= 0.0F) || (previous >= 0.0F && sample < 0.0F)) {
      ++evidence.zero_crossings;
    }
    previous = sample;
    uint32_t bits = 0;
    std::memcpy(&bits, &sample, sizeof(bits));
    for (uint32_t shift = 0; shift < 32; shift += 8) {
      evidence.sequence_hash ^= static_cast<uint8_t>((bits >> shift) & 0xffU);
      evidence.sequence_hash *= 1099511628211ULL;
    }
  }
  return evidence;
}

double capture_frequency_hz(const CaptureEvidence& evidence, uint64_t frames,
                            uint32_t sample_rate) {
  return frames == 0 ? 0.0
                     : static_cast<double>(evidence.zero_crossings) * sample_rate /
                           (2.0 * static_cast<double>(frames));
}

void append_route_facts(std::ostringstream& json, const orpheus::ActiveAudioRoute& active,
                        const orpheus::AudioIoRouteState& io_route,
                        const orpheus::AudioIoTelemetry& telemetry, bool& first) {
  append_string(json, "selected_input_uid", io_route.selected_input_device_id, first);
  append_string(json, "selected_output_uid", io_route.selected_output_device_id, first);
  append_string(json, "resolved_input_uid", active.input_device_id, first);
  append_string(json, "resolved_output_uid", active.output_device_id, first);
  append_map(json, "active_input_map", active.input_channels, first);
  append_map(json, "active_output_map", active.output_channels, first);
  append_number(json, "requested_sample_rate", active.requested_sample_rate, first);
  append_number(json, "actual_sample_rate", active.actual_sample_rate, first);
  append_number(json, "requested_buffer_frames", io_route.requested_buffer_size, first);
  append_number(json, "actual_buffer_frames", active.actual_buffer_frames, first);
  append_number(json, "route_state_actual_buffer_frames", io_route.actual_buffer_size, first);
  append_number(json, "available_input_channels", active.available_input_channels, first);
  append_number(json, "available_output_channels", active.available_output_channels, first);
  append_number(json, "requested_output_channels", active.requested_output_channels, first);
  append_number(json, "resolved_output_channels", active.resolved_output_channels, first);
  append_number(json, "input_virtual_format_channels", active.input_virtual_format_channels, first);
  append_number(json, "output_virtual_format_channels", active.output_virtual_format_channels,
                first);
  append_number(json, "input_client_format_channels", active.input_client_format_channels, first);
  append_number(json, "output_client_format_channels", active.output_client_format_channels, first);
  append_number(json, "input_physical_sample_rate", active.input_physical_sample_rate, first);
  append_number(json, "output_physical_sample_rate", active.output_physical_sample_rate, first);
  append_number(json, "input_client_sample_rate", active.input_client_sample_rate, first);
  append_number(json, "output_client_sample_rate", active.output_client_sample_rate, first);
  append_bool(json, "input_conversion_active", active.input_conversion_active, first);
  append_bool(json, "output_conversion_active", active.output_conversion_active, first);
  append_bool(json, "input_is_bluetooth", active.input_is_bluetooth, first);
  append_bool(json, "output_is_bluetooth", active.output_is_bluetooth, first);
  append_bool(json, "endpoints_related", active.endpoints_related, first);
  append_bool(json, "output_mono_fallback", active.output_mono_fallback, first);
  append_number(json, "capture_latency_frames", active.latency.capture_frames, first);
  append_number(json, "playback_latency_frames", active.latency.playback_frames, first);
  append_number(json, "processing_latency_frames", active.latency.processing_frames, first);
  append_bool(json, "latency_complete", active.latency.complete, first);
  append_optional_number(json, "input_device_latency_frames", io_route.latency.input_device_frames,
                         first);
  append_optional_number(json, "input_safety_offset_frames",
                         io_route.latency.input_safety_offset_frames, first);
  append_optional_number(json, "input_stream_latency_frames", io_route.latency.input_stream_frames,
                         first);
  append_optional_number(json, "input_converter_latency_frames",
                         io_route.latency.input_converter_frames, first);
  append_optional_number(json, "output_device_latency_frames",
                         io_route.latency.output_device_frames, first);
  append_optional_number(json, "output_safety_offset_frames",
                         io_route.latency.output_safety_offset_frames, first);
  append_optional_number(json, "output_stream_latency_frames",
                         io_route.latency.output_stream_frames, first);
  append_optional_number(json, "output_converter_latency_frames",
                         io_route.latency.output_converter_frames, first);
  append_optional_number(json, "callback_buffer_frames", io_route.latency.callback_buffer_frames,
                         first);
  append_optional_number(json, "aggregate_or_audio_unit_frames",
                         io_route.latency.aggregate_or_audio_unit_frames, first);
  append_optional_number(json, "input_audio_unit_frames", io_route.latency.input_audio_unit_frames,
                         first);
  append_optional_number(json, "output_audio_unit_frames",
                         io_route.latency.output_audio_unit_frames, first);
  append_bool(json, "detailed_latency_complete", io_route.latency.complete, first);
  append_number(json, "input_render_failures", telemetry.input_render_failures, first);
  append_number(json, "input_fifo_overruns", telemetry.input_fifo_overruns, first);
  append_number(json, "input_fifo_underruns", telemetry.input_fifo_underruns, first);
  append_number(json, "input_conversion_failures", telemetry.input_conversion_failures, first);
  append_number(json, "output_conversion_failures", telemetry.output_conversion_failures, first);
  append_string(json, "route_outcome", route_outcome_name(telemetry.route_outcome), first);
}

} // namespace

int main(int argc, char** argv) {
  const Options options = parse_options(argc, argv);
  auto driver = orpheus::createCoreAudioDriver();
  if (!driver) {
    print_error("coreaudio-driver-unavailable", 3);
  }

  const uint64_t expected_frames = static_cast<uint64_t>(options.sample_rate) * options.seconds;
  std::vector<float> capture;
  if (options.input_uid.has_value()) {
    try {
      capture.resize(static_cast<size_t>(expected_frames));
    } catch (const std::exception&) {
      print_error("capture-buffer-allocation-failed", 3);
    }
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
  const auto initialized_route = driver->getActiveRoute();
  const auto initialized_io_route = driver->getAudioIoRouteState();
  const auto initialized_telemetry = driver->getTelemetry();
  if (initialize_result != orpheus::SessionGraphError::OK ||
      options.expected_initialize_outcome.has_value()) {
    const bool expected =
        initialize_result != orpheus::SessionGraphError::OK &&
        options.expected_initialize_outcome.has_value() &&
        initialized_telemetry.route_outcome == *options.expected_initialize_outcome;
    std::ostringstream json;
    json << std::setprecision(10) << '{';
    bool first = true;
    append_string(json, "status", expected ? "passed" : "failed", first);
    append_string(json, "backend", "CoreAudio", first);
    append_string(json, "reason", "initialize", first);
    append_bool(json, "initialize_result_ok", initialize_result == orpheus::SessionGraphError::OK,
                first);
    append_string(json, "input_uid", options.input_uid.value_or(""), first);
    append_string(json, "output_uid", options.output_uid, first);
    append_number(json, "sample_rate", options.sample_rate, first);
    append_number(json, "seconds", options.seconds, first);
    append_number(json, "requested_frames", expected_frames, first);
    append_string(json, "output_policy", output_policy_name(options.allow_mono_fallback), first);
    append_route_facts(json, initialized_route, initialized_io_route, initialized_telemetry, first);
    append_string(json, "expected_initialize_outcome",
                  options.expected_initialize_outcome.has_value()
                      ? route_outcome_name(*options.expected_initialize_outcome)
                      : "",
                  first);
    append_bool(json, "initialize_outcome_expected", expected, first);
    append_string(json, "detail", initialized_io_route.detail, first);
    json << "}\n";
    std::cout << json.str();
    return expected ? 0 : 4;
  }

  AcceptanceCallback callback(capture.empty() ? nullptr : capture.data(), expected_frames);
  const auto start_result = driver->start(&callback);
  if (start_result != orpheus::SessionGraphError::OK) {
    const auto telemetry = driver->getTelemetry();
    std::ostringstream json;
    json << "{\"status\":\"failed\",\"reason\":\"start\",\"route_outcome\":\""
         << route_outcome_name(telemetry.route_outcome) << "\",\"detail\":\""
         << json_escape(driver->getAudioIoRouteState().detail) << "\"}\n";
    std::cout << json.str();
    return 5;
  }

  std::optional<orpheus::AudioRouteRuntimeOutcome> first_terminal_outcome;
  uint64_t callbacks_at_terminal = 0;
  bool terminal_stable = true;
  bool no_callbacks_after_terminal = true;
  bool wait_ok = true;
  if (options.expected_terminal_outcome.has_value()) {
    const auto search_deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(options.grace_ms);
    std::optional<std::chrono::steady_clock::time_point> stability_deadline;
    for (;;) {
      const auto now = std::chrono::steady_clock::now();
      if (now >= search_deadline && !stability_deadline.has_value()) {
        break;
      }
      if (stability_deadline.has_value() && now >= *stability_deadline) {
        break;
      }

      const auto outcome = driver->getTelemetry().route_outcome;
      if (!first_terminal_outcome.has_value()) {
        if (outcome != orpheus::AudioRouteRuntimeOutcome::Healthy) {
          first_terminal_outcome = outcome;
          callbacks_at_terminal = callback.callbacks();
          stability_deadline = now + std::chrono::milliseconds(options.grace_ms);
        }
      } else {
        terminal_stable = terminal_stable && outcome == *first_terminal_outcome;
        no_callbacks_after_terminal =
            no_callbacks_after_terminal && callback.callbacks() == callbacks_at_terminal;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (first_terminal_outcome.has_value()) {
      no_callbacks_after_terminal =
          no_callbacks_after_terminal && callback.callbacks() == callbacks_at_terminal;
    }
    wait_ok = first_terminal_outcome.has_value() &&
              *first_terminal_outcome == *options.expected_terminal_outcome && terminal_stable &&
              no_callbacks_after_terminal;
  } else if (options.input_uid.has_value()) {
    const auto capture_deadline = std::chrono::steady_clock::now() +
                                  std::chrono::seconds(options.seconds) +
                                  std::chrono::milliseconds(options.grace_ms);
    while (callback.input_frames() < expected_frames &&
           std::chrono::steady_clock::now() < capture_deadline) {
      const auto outcome = driver->getTelemetry().route_outcome;
      if (outcome != orpheus::AudioRouteRuntimeOutcome::Healthy) {
        first_terminal_outcome = outcome;
        wait_ok = false;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (callback.input_frames() < expected_frames) {
      wait_ok = false;
    }
  } else {
    std::this_thread::sleep_for(std::chrono::seconds(options.seconds));
  }

  const uint64_t callbacks_before_stop = callback.callbacks();
  const auto stop_result = driver->stop();
  const uint64_t callbacks_after_stop = callback.callbacks();
  const auto negotiated = driver->getActiveRoute();
  const auto final_io_route = driver->getAudioIoRouteState();
  const auto telemetry = driver->getTelemetry();
  const auto route_outcome = telemetry.route_outcome;
  const uint64_t captured_frames = std::min<uint64_t>(callback.input_frames(), expected_frames);
  const auto capture_evidence =
      inspect_capture(capture.empty() ? nullptr : capture.data(), captured_frames);
  const double zero_crossing_frequency =
      capture_frequency_hz(capture_evidence, captured_frames, options.sample_rate);
  const bool counters_clear =
      telemetry.input_render_failures == 0 && telemetry.input_fifo_overruns == 0 &&
      telemetry.input_fifo_underruns == 0 && telemetry.input_conversion_failures == 0 &&
      telemetry.output_conversion_failures == 0;
  const bool capture_capacity_exact =
      !options.input_uid.has_value() || capture.size() == expected_frames;
  const bool input_activity_matches =
      options.input_uid.has_value()
          ? callback.input_callbacks() > 0 && captured_frames == expected_frames
          : callback.input_callbacks() == 0;
  const bool tone_in_tolerance =
      !options.expected_input_tone_hz.has_value() ||
      (options.input_uid.has_value() && captured_frames == expected_frames &&
       capture_evidence.finite && capture_evidence.peak > 0.0 &&
       std::abs(zero_crossing_frequency - *options.expected_input_tone_hz) <= 20.0);
  const bool expected_terminal_ok = !options.expected_terminal_outcome.has_value() ||
                                    (first_terminal_outcome.has_value() &&
                                     *first_terminal_outcome == *options.expected_terminal_outcome);
  const bool healthy_run = options.expected_terminal_outcome.has_value() ||
                           route_outcome == orpheus::AudioRouteRuntimeOutcome::Healthy;
  const bool passed = stop_result == orpheus::SessionGraphError::OK && callback.callbacks() > 0 &&
                      wait_ok && healthy_run && counters_clear && capture_capacity_exact &&
                      input_activity_matches && tone_in_tolerance && expected_terminal_ok &&
                      callbacks_after_stop == callbacks_before_stop;

  std::ostringstream json;
  json << std::setprecision(10) << '{';
  bool first = true;
  append_string(json, "status", passed ? "passed" : "failed", first);
  append_string(json, "backend", "CoreAudio", first);
  append_string(json, "input_uid", options.input_uid.value_or(""), first);
  append_string(json, "output_uid", options.output_uid, first);
  append_number(json, "sample_rate", options.sample_rate, first);
  append_number(json, "seconds", options.seconds, first);
  append_number(json, "requested_frames", expected_frames, first);
  append_string(json, "output_policy", output_policy_name(options.allow_mono_fallback), first);
  append_route_facts(json, negotiated, final_io_route, telemetry, first);
  append_number(json, "callbacks", callback.callbacks(), first);
  append_number(json, "callback_frames", callback.callback_frames(), first);
  append_number(json, "input_callbacks", callback.input_callbacks(), first);
  append_number(json, "input_frames", callback.input_frames(), first);
  append_number(json, "capture_capacity_frames",
                options.input_uid.has_value() ? expected_frames : 0, first);
  append_number(json, "captured_frames", captured_frames, first);
  append_bool(json, "capture_capacity_exact", capture_capacity_exact, first);
  append_number(json, "uncaptured_input_frames", callback.capture_overflow_frames(), first);
  append_number(json, "captured_duration_seconds",
                static_cast<double>(captured_frames) / options.sample_rate, first);
  append_optional_number(json, "capture_first_sample", capture_evidence.first_sample, first);
  append_optional_number(json, "capture_last_sample", capture_evidence.last_sample, first);
  append_number(json, "input_peak", capture_evidence.peak, first);
  append_bool(json, "capture_finite", capture_evidence.finite, first);
  append_number(json, "capture_zero_crossings", capture_evidence.zero_crossings, first);
  append_number(json, "capture_sequence_hash", capture_evidence.sequence_hash, first);
  append_number(json, "zero_crossing_frequency_hz", zero_crossing_frequency, first);
  append_bool(json, "capture_complete",
              !options.input_uid.has_value() || captured_frames == expected_frames, first);
  append_bool(json, "stop_result_ok", stop_result == orpheus::SessionGraphError::OK, first);
  append_number(json, "callbacks_before_stop", callbacks_before_stop, first);
  append_number(json, "callbacks_after_stop", callbacks_after_stop, first);
  append_bool(json, "callbacks_stopped_after_terminal", no_callbacks_after_terminal, first);
  append_string(
      json, "first_terminal_outcome",
      first_terminal_outcome.has_value() ? route_outcome_name(*first_terminal_outcome) : "", first);
  append_bool(json, "terminal_outcome_stable", terminal_stable, first);
  append_bool(json, "wait_complete", wait_ok, first);
  if (options.expected_input_tone_hz.has_value()) {
    append_number(json, "expected_input_tone_hz", *options.expected_input_tone_hz, first);
    append_bool(json, "input_tone_in_tolerance", tone_in_tolerance, first);
  }
  if (options.expected_terminal_outcome.has_value()) {
    append_string(json, "expected_terminal_outcome",
                  route_outcome_name(*options.expected_terminal_outcome), first);
    append_bool(json, "terminal_outcome_expected", expected_terminal_ok, first);
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
