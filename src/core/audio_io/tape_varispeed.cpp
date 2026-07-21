// SPDX-License-Identifier: MIT

#include "orpheus/tape_varispeed.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace orpheus {
namespace {

constexpr size_t kTapCount = 128;
constexpr size_t kHalfTaps = kTapCount / 2;
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kKaiserBeta = 10.0;

bool valid_rate(double rate, double minimum, double maximum) {
  return std::isfinite(rate) && rate >= minimum && rate <= maximum;
}

double bessel_i0(double value) {
  double term = 1.0;
  double sum = 1.0;
  const double square = value * value / 4.0;
  for (int index = 1; index < 40; ++index) {
    term *= square / static_cast<double>(index * index);
    sum += term;
    if (term < sum * 1.0e-16) {
      break;
    }
  }
  return sum;
}

double sinc(double value) {
  if (std::abs(value) < 1.0e-12) {
    return 1.0;
  }
  const double radians = kPi * value;
  return std::sin(radians) / radians;
}

} // namespace

struct PreparedTapeVarispeed::Impl {
  TapeVarispeedConfig config{};
  std::vector<float> history;
  std::vector<double> window;
  double phase = 0.0;
  double applied = 1.0;
  double target = 1.0;
  double rate_step = 0.0;
  uint32_t slew_remaining = 0;
  bool prepared = false;

  float source_sample(const float* input, size_t input_frames, int64_t frame,
                      uint16_t channel) const noexcept {
    if (frame >= 0 && static_cast<size_t>(frame) < input_frames) {
      return input[static_cast<size_t>(frame) * config.channels + channel];
    }
    if (frame < 0) {
      const int64_t history_frame = static_cast<int64_t>(kTapCount) + frame;
      if (history_frame >= 0) {
        return history[static_cast<size_t>(history_frame) * config.channels + channel];
      }
    }
    return 0.0f;
  }

  void append_history(const float* input, size_t consumed) noexcept {
    if (consumed == 0) {
      return;
    }
    const size_t channels = config.channels;
    if (consumed >= kTapCount) {
      std::memcpy(history.data(), input + (consumed - kTapCount) * channels,
                  kTapCount * channels * sizeof(float));
      return;
    }
    const size_t retained = kTapCount - consumed;
    std::memmove(history.data(), history.data() + consumed * channels,
                 retained * channels * sizeof(float));
    std::memcpy(history.data() + retained * channels, input, consumed * channels * sizeof(float));
  }

  void start_slew(double next_target) noexcept {
    if (next_target == target) {
      return;
    }
    target = next_target;
    slew_remaining = config.rate_slew_frames;
    if (slew_remaining == 0) {
      applied = target;
      rate_step = 0.0;
      return;
    }
    rate_step = (target - applied) / static_cast<double>(slew_remaining);
  }

  void advance_rate() noexcept {
    if (slew_remaining == 0) {
      return;
    }
    applied += rate_step;
    --slew_remaining;
    if (slew_remaining == 0) {
      applied = target;
      rate_step = 0.0;
    }
  }
};

PreparedTapeVarispeed::PreparedTapeVarispeed() : impl_(std::make_unique<Impl>()) {}
PreparedTapeVarispeed::~PreparedTapeVarispeed() = default;
PreparedTapeVarispeed::PreparedTapeVarispeed(PreparedTapeVarispeed&&) noexcept = default;
PreparedTapeVarispeed& PreparedTapeVarispeed::operator=(PreparedTapeVarispeed&&) noexcept = default;

SessionGraphError PreparedTapeVarispeed::prepare(const TapeVarispeedConfig& config) {
  if (config.sample_rate == 0 || config.channels == 0 || config.max_input_frames == 0 ||
      config.max_output_frames == 0 || !std::isfinite(config.min_rate) ||
      !std::isfinite(config.max_rate) || config.min_rate <= 0.0 ||
      config.min_rate > config.max_rate ||
      !valid_rate(config.initial_rate, config.min_rate, config.max_rate)) {
    return SessionGraphError::InvalidParameter;
  }

  try {
    Impl next;
    next.config = config;
    next.history.assign(kTapCount * config.channels, 0.0f);
    next.window.resize(kTapCount);
    const double denominator = bessel_i0(kKaiserBeta);
    for (size_t tap = 0; tap < kTapCount; ++tap) {
      const double position =
          (2.0 * static_cast<double>(tap) / static_cast<double>(kTapCount - 1)) - 1.0;
      next.window[tap] =
          bessel_i0(kKaiserBeta * std::sqrt(std::max(0.0, 1.0 - position * position))) /
          denominator;
    }
    next.applied = config.initial_rate;
    next.target = config.initial_rate;
    next.prepared = true;
    *impl_ = std::move(next);
  } catch (const std::bad_alloc&) {
    return SessionGraphError::NotSupported;
  }
  return SessionGraphError::OK;
}

size_t PreparedTapeVarispeed::required_input_frames(size_t output_frames) const noexcept {
  if (!impl_->prepared || output_frames > impl_->config.max_output_frames) {
    return 0;
  }
  const double worst_rate = impl_->config.max_rate;
  return static_cast<size_t>(std::ceil(static_cast<double>(output_frames) * worst_rate)) +
         kHalfTaps + 1;
}

size_t PreparedTapeVarispeed::required_output_capacity(size_t input_frames) const noexcept {
  if (!impl_->prepared || input_frames > impl_->config.max_input_frames) {
    return 0;
  }
  return static_cast<size_t>(
             std::ceil(static_cast<double>(input_frames) / impl_->config.min_rate)) +
         1;
}

size_t PreparedTapeVarispeed::max_drain_frames() const noexcept {
  return 0;
}
size_t PreparedTapeVarispeed::latency_frames() const noexcept {
  return kHalfTaps;
}
double PreparedTapeVarispeed::applied_rate() const noexcept {
  return impl_->applied;
}

TapeVarispeedProcessResult PreparedTapeVarispeed::process(const float* input, size_t input_frames,
                                                          float* output, size_t output_capacity,
                                                          double target_rate) noexcept {
  TapeVarispeedProcessResult result;
  if (!impl_->prepared) {
    result.error = SessionGraphError::NotReady;
    return result;
  }
  if (!valid_rate(target_rate, impl_->config.min_rate, impl_->config.max_rate) ||
      input_frames > impl_->config.max_input_frames ||
      output_capacity > impl_->config.max_output_frames ||
      (input_frames != 0 && input == nullptr) || (output_capacity != 0 && output == nullptr)) {
    result.error = SessionGraphError::InvalidParameter;
    return result;
  }
  if (input_frames == 0 || output_capacity == 0) {
    return result;
  }

  impl_->start_slew(target_rate);
  if (impl_->applied == 1.0 && impl_->target == 1.0 && impl_->slew_remaining == 0) {
    const size_t frames = std::min(input_frames, output_capacity);
    std::memcpy(output, input, frames * impl_->config.channels * sizeof(float));
    impl_->append_history(input, frames);
    result.input_frames_consumed = frames;
    result.output_frames_produced = frames;
    return result;
  }

  while (result.output_frames_produced < output_capacity) {
    const int64_t centre = static_cast<int64_t>(std::floor(impl_->phase));
    if (centre + static_cast<int64_t>(kHalfTaps) >= static_cast<int64_t>(input_frames)) {
      break;
    }
    const double fraction = impl_->phase - static_cast<double>(centre);
    const double cutoff = 0.5 / std::max(1.0, impl_->applied);
    for (uint16_t channel = 0; channel < impl_->config.channels; ++channel) {
      double sum = 0.0;
      double normalization = 0.0;
      for (size_t tap = 0; tap < kTapCount; ++tap) {
        const int64_t source =
            centre + static_cast<int64_t>(tap) - static_cast<int64_t>(kHalfTaps - 1);
        const double distance =
            static_cast<double>(tap) - static_cast<double>(kHalfTaps - 1) - fraction;
        const double coefficient =
            2.0 * cutoff * sinc(2.0 * cutoff * distance) * impl_->window[tap];
        sum += static_cast<double>(impl_->source_sample(input, input_frames, source, channel)) *
               coefficient;
        normalization += coefficient;
      }
      output[result.output_frames_produced * impl_->config.channels + channel] =
          static_cast<float>(sum / normalization);
    }
    ++result.output_frames_produced;
    impl_->phase += impl_->applied;
    impl_->advance_rate();
  }

  result.input_frames_consumed =
      std::min(input_frames, static_cast<size_t>(std::floor(impl_->phase)));
  impl_->append_history(input, result.input_frames_consumed);
  impl_->phase -= static_cast<double>(result.input_frames_consumed);
  return result;
}

TapeVarispeedProcessResult PreparedTapeVarispeed::drain(float* output,
                                                        size_t output_capacity) noexcept {
  TapeVarispeedProcessResult result;
  if (!impl_->prepared) {
    result.error = SessionGraphError::NotReady;
  } else if (output_capacity > impl_->config.max_output_frames ||
             (output_capacity != 0 && output == nullptr)) {
    result.error = SessionGraphError::InvalidParameter;
  }
  return result;
}

SessionGraphError PreparedTapeVarispeed::reset(double initial_rate) noexcept {
  if (!impl_->prepared) {
    return SessionGraphError::NotReady;
  }
  if (!valid_rate(initial_rate, impl_->config.min_rate, impl_->config.max_rate)) {
    return SessionGraphError::InvalidParameter;
  }
  std::fill(impl_->history.begin(), impl_->history.end(), 0.0f);
  impl_->phase = 0.0;
  impl_->applied = initial_rate;
  impl_->target = initial_rate;
  impl_->rate_step = 0.0;
  impl_->slew_remaining = 0;
  return SessionGraphError::OK;
}

struct OfflineTapeVarispeed::Impl {
  PreparedTapeVarispeed processor;
  double rate = 1.0;
  bool configured = false;
};

OfflineTapeVarispeed::OfflineTapeVarispeed() : impl_(std::make_unique<Impl>()) {}
OfflineTapeVarispeed::~OfflineTapeVarispeed() = default;
OfflineTapeVarispeed::OfflineTapeVarispeed(OfflineTapeVarispeed&&) noexcept = default;
OfflineTapeVarispeed& OfflineTapeVarispeed::operator=(OfflineTapeVarispeed&&) noexcept = default;

SessionGraphError OfflineTapeVarispeed::configure(uint32_t sample_rate, uint16_t channels,
                                                  double rate) {
  if (sample_rate == 0 || channels == 0 || !valid_rate(rate, 0.5, 2.0)) {
    return SessionGraphError::InvalidParameter;
  }
  TapeVarispeedConfig config;
  config.sample_rate = sample_rate;
  config.channels = channels;
  config.max_input_frames = 4096;
  config.max_output_frames = 8192;
  config.initial_rate = rate;
  const auto error = impl_->processor.prepare(config);
  if (error == SessionGraphError::OK) {
    impl_->rate = rate;
    impl_->configured = true;
  }
  return error;
}

size_t OfflineTapeVarispeed::required_output_capacity(size_t input_frames) const noexcept {
  return impl_->configured ? impl_->processor.required_output_capacity(input_frames) : 0;
}
size_t OfflineTapeVarispeed::max_drain_frames() const noexcept {
  return impl_->configured ? impl_->processor.max_drain_frames() : 0;
}
TapeVarispeedProcessResult OfflineTapeVarispeed::process(const float* input, size_t input_frames,
                                                         float* output, size_t output_capacity) {
  if (!impl_->configured) {
    return {SessionGraphError::NotReady, 0, 0};
  }
  return impl_->processor.process(input, input_frames, output, output_capacity, impl_->rate);
}
TapeVarispeedProcessResult OfflineTapeVarispeed::drain(float* output, size_t output_capacity) {
  if (!impl_->configured) {
    return {SessionGraphError::NotReady, 0, 0};
  }
  return impl_->processor.drain(output, output_capacity);
}
void OfflineTapeVarispeed::reset() noexcept {
  if (impl_->configured) {
    (void)impl_->processor.reset(impl_->rate);
  }
}

} // namespace orpheus
