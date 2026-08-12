// SPDX-License-Identifier: MIT
#include <orpheus/directional_sample_rate_converter.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <utility>
#include <vector>

namespace orpheus::audio_utils {
namespace {

constexpr uint32_t kTaps = 255;
constexpr uint32_t kPhaseIntervals = 1024;
constexpr uint32_t kHistoryFrames = (kTaps - 1) / 2;
constexpr uint64_t kPpmScale = 1'000'000;
constexpr int32_t kMaximumCorrectionPpm = 1'000;
constexpr double kPi = 3.141592653589793238462643383279502884;

bool isCanonicalRate(uint32_t rate) noexcept {
  return rate == 16'000 || rate == 24'000 || rate == 44'100 || rate == 48'000;
}

bool isSessionRate(uint32_t rate) noexcept {
  return rate == 44'100 || rate == 48'000;
}

bool isPowerOfTwo(uint32_t value) noexcept {
  return value != 0 && (value & (value - 1)) == 0;
}

uint64_t ceilDivide(uint64_t numerator, uint64_t denominator) noexcept {
  const uint64_t quotient = numerator / denominator;
  return quotient + (numerator % denominator != 0 ? 1 : 0);
}

bool checkedMultiply(uint64_t lhs, uint64_t rhs, uint64_t& result) noexcept {
  if (lhs != 0 && rhs > std::numeric_limits<uint64_t>::max() / lhs) {
    return false;
  }
  result = lhs * rhs;
  return true;
}

bool checkedAdd(uint64_t lhs, uint64_t rhs, uint64_t& result) noexcept {
  if (rhs > std::numeric_limits<uint64_t>::max() - lhs) {
    return false;
  }
  result = lhs + rhs;
  return true;
}

bool checkedPhaseAdvance(uint64_t phase_remainder, uint64_t phase_increment, uint32_t output_frames,
                         uint64_t denominator, uint64_t& input_frames) noexcept {
  uint64_t product = 0;
  uint64_t phase = 0;
  if (!checkedMultiply(phase_increment, output_frames, product) ||
      !checkedAdd(phase_remainder, product, phase)) {
    return false;
  }
  input_frames = ceilDivide(phase, denominator);
  return true;
}

bool checkedRequiredInput(uint64_t phase_remainder, uint64_t phase_increment,
                          uint32_t output_frames, uint64_t denominator,
                          uint64_t& required_frames) noexcept {
  uint64_t input_advance = 0;
  if (!checkedPhaseAdvance(phase_remainder, phase_increment, output_frames, denominator,
                           input_advance) ||
      !checkedAdd(input_advance, kHistoryFrames, required_frames)) {
    return false;
  }
  return true;
}

} // namespace

struct DirectionalSampleRateConverter::Impl {
  explicit Impl(const DirectionalSrcConfig& source_config)
      : config(source_config),
        denominator(static_cast<uint64_t>(source_config.output_rate) * kPpmScale),
        nominal_phase_increment(static_cast<uint64_t>(source_config.input_rate) * kPpmScale),
        phase_increment(nominal_phase_increment),
        phase_kernels(static_cast<size_t>(kPhaseIntervals + 1) * kTaps),
        fifo(static_cast<size_t>(source_config.fifo_capacity_frames) * source_config.channels),
        history(static_cast<size_t>(kHistoryFrames) * source_config.channels) {
    buildKernels();
  }

  void buildKernels() {
    const double rate_ratio =
        static_cast<double>(config.output_rate) / static_cast<double>(config.input_rate);
    const double cutoff = 0.45 * std::min(1.0, rate_ratio);

    for (uint32_t phase = 0; phase <= kPhaseIntervals; ++phase) {
      const double fraction = static_cast<double>(phase) / static_cast<double>(kPhaseIntervals);
      double sum = 0.0;
      const size_t base = static_cast<size_t>(phase) * kTaps;
      for (uint32_t tap = 0; tap < kTaps; ++tap) {
        const double x = static_cast<double>(tap) - static_cast<double>(kHistoryFrames) - fraction;
        const double sinc_argument = 2.0 * cutoff * x;
        const double sinc =
            sinc_argument == 0.0 ? 1.0 : std::sin(kPi * sinc_argument) / (kPi * sinc_argument);
        const double window = 0.42 - 0.5 * std::cos(2.0 * kPi * static_cast<double>(tap) / 254.0) +
                              0.08 * std::cos(4.0 * kPi * static_cast<double>(tap) / 254.0);
        const double coefficient = 2.0 * cutoff * sinc * window;
        phase_kernels[base + tap] = coefficient;
        sum += coefficient;
      }
      if (sum != 0.0 && std::isfinite(sum)) {
        for (uint32_t tap = 0; tap < kTaps; ++tap) {
          phase_kernels[base + tap] /= sum;
        }
      }
    }
  }

  uint64_t correctedPhaseIncrement(int32_t ppm) const noexcept {
    const int64_t scaled = static_cast<int64_t>(kPpmScale) + ppm;
    return static_cast<uint64_t>(static_cast<uint64_t>(config.input_rate) *
                                 static_cast<uint64_t>(scaled));
  }

  uint32_t bufferedFrames() const noexcept {
    const uint64_t write = write_index.load(std::memory_order_acquire);
    const uint64_t read = read_index.load(std::memory_order_acquire);
    const uint64_t available = write - read;
    return static_cast<uint32_t>(std::min<uint64_t>(available, config.fifo_capacity_frames));
  }

  float fifoSample(uint64_t read, uint32_t offset, uint16_t channel) const noexcept {
    const uint32_t frame = static_cast<uint32_t>(read + offset) & (config.fifo_capacity_frames - 1);
    return fifo[static_cast<size_t>(frame) * config.channels + channel];
  }

  float historySample(int32_t offset, uint16_t channel) const noexcept {
    const uint32_t distance = static_cast<uint32_t>(-offset);
    const uint32_t frame = (history_head + kHistoryFrames - distance) % kHistoryFrames;
    return history[static_cast<size_t>(frame) * config.channels + channel];
  }

  float sampleAt(uint64_t read, int32_t offset, uint16_t channel) const noexcept {
    if (offset < 0) {
      return historySample(offset, channel);
    }
    return fifoSample(read, static_cast<uint32_t>(offset), channel);
  }

  void rememberConsumedFrame(uint64_t read) noexcept {
    const uint32_t source_frame = static_cast<uint32_t>(read) & (config.fifo_capacity_frames - 1);
    const size_t source_base = static_cast<size_t>(source_frame) * config.channels;
    const size_t history_base = static_cast<size_t>(history_head) * config.channels;
    for (uint16_t channel = 0; channel < config.channels; ++channel) {
      history[history_base + channel] = fifo[source_base + channel];
    }
    history_head = (history_head + 1) % kHistoryFrames;
  }

  DirectionalSrcConfig config;
  uint64_t denominator = 0;
  uint64_t nominal_phase_increment = 0;
  uint64_t phase_increment = 0;
  int32_t correction_ppm = 0;
  uint64_t phase_remainder = 0;
  uint64_t output_frames_produced = 0;
  uint32_t history_head = 0;
  std::vector<double> phase_kernels;
  std::vector<float> fifo;
  std::vector<float> history;
  std::atomic<uint64_t> write_index{0};
  std::atomic<uint64_t> read_index{0};
  std::atomic<bool> primed{false};
};

DirectionalSampleRateConverter::DirectionalSampleRateConverter() noexcept = default;

DirectionalSampleRateConverter::~DirectionalSampleRateConverter() = default;

DirectionalSampleRateConverter::DirectionalSampleRateConverter(
    DirectionalSampleRateConverter&& other) noexcept = default;

DirectionalSampleRateConverter& DirectionalSampleRateConverter::operator=(
    DirectionalSampleRateConverter&& other) noexcept = default;

SessionGraphError
DirectionalSampleRateConverter::prepare(const DirectionalSrcConfig& config) noexcept {
  if (!isCanonicalRate(config.input_rate) || !isCanonicalRate(config.output_rate) ||
      (!isSessionRate(config.input_rate) && !isSessionRate(config.output_rate))) {
    return SessionGraphError::NotSupported;
  }
  if (config.channels < 1 || config.channels > 2 || config.max_input_frames < 1 ||
      config.max_input_frames > 4096 || config.max_output_frames < 1 ||
      config.max_output_frames > 4096 || config.fifo_capacity_frames < 1024 ||
      config.fifo_capacity_frames > 32768 || !isPowerOfTwo(config.fifo_capacity_frames)) {
    return SessionGraphError::InvalidParameter;
  }

  const uint64_t denominator = static_cast<uint64_t>(config.output_rate) * kPpmScale;
  const uint64_t correction_scale = kPpmScale + kMaximumCorrectionPpm;
  const uint64_t worst_phase_increment =
      static_cast<uint64_t>(config.input_rate) * correction_scale;
  uint64_t worst_required = 0;
  if (!checkedRequiredInput(denominator - 1, worst_phase_increment, config.max_output_frames,
                            denominator, worst_required)) {
    return SessionGraphError::InvalidParameter;
  }
  if (!checkedAdd(worst_required, kTaps, worst_required)) {
    return SessionGraphError::InvalidParameter;
  }

  uint64_t callback_storage = 0;
  if (!checkedMultiply(config.max_input_frames, 8, callback_storage)) {
    return SessionGraphError::InvalidParameter;
  }
  if (config.fifo_capacity_frames < callback_storage ||
      config.fifo_capacity_frames < worst_required ||
      config.prime_input_frames > config.fifo_capacity_frames / 2) {
    return SessionGraphError::InvalidParameter;
  }
  uint64_t prime_with_history = 0;
  if (!checkedAdd(config.prime_input_frames, kHistoryFrames + 1, prime_with_history) ||
      prime_with_history > config.fifo_capacity_frames) {
    return SessionGraphError::InvalidParameter;
  }

  try {
    auto replacement = std::make_unique<Impl>(config);
    impl_ = std::move(replacement);
  } catch (const std::bad_alloc&) {
    return SessionGraphError::InternalError;
  }
  return SessionGraphError::OK;
}

DirectionalSrcStatus
DirectionalSampleRateConverter::requiredInputFrames(uint32_t output_frames,
                                                    uint32_t& input_frames) const noexcept {
  input_frames = 0;
  if (!impl_) {
    return DirectionalSrcStatus::NotPrepared;
  }
  if (output_frames > impl_->config.max_output_frames) {
    return DirectionalSrcStatus::InvalidArgument;
  }
  if (output_frames == 0) {
    return DirectionalSrcStatus::Ok;
  }

  uint64_t required = 0;
  if (!checkedRequiredInput(impl_->phase_remainder, impl_->phase_increment, output_frames,
                            impl_->denominator, required)) {
    return DirectionalSrcStatus::InvalidArgument;
  }
  const uint64_t buffered = impl_->bufferedFrames();
  if (required <= buffered) {
    return DirectionalSrcStatus::Ok;
  }
  const uint64_t additional = required - buffered;
  if (additional > std::numeric_limits<uint32_t>::max()) {
    return DirectionalSrcStatus::InvalidArgument;
  }
  input_frames = static_cast<uint32_t>(additional);
  return DirectionalSrcStatus::Ok;
}

DirectionalSrcTransfer DirectionalSampleRateConverter::pushInput(const float* const* input,
                                                                 uint16_t channels,
                                                                 uint32_t frames) noexcept {
  DirectionalSrcTransfer result;
  if (!impl_) {
    return result;
  }
  if (frames == 0) {
    result.status = DirectionalSrcStatus::Ok;
    return result;
  }
  if (input == nullptr || channels != impl_->config.channels ||
      frames > impl_->config.max_input_frames) {
    result.status = DirectionalSrcStatus::InvalidArgument;
    return result;
  }
  for (uint16_t channel = 0; channel < channels; ++channel) {
    if (input[channel] == nullptr) {
      result.status = DirectionalSrcStatus::InvalidArgument;
      return result;
    }
  }

  const uint64_t read = impl_->read_index.load(std::memory_order_acquire);
  const uint64_t write = impl_->write_index.load(std::memory_order_relaxed);
  const uint64_t buffered = write - read;
  if (buffered > impl_->config.fifo_capacity_frames ||
      frames > impl_->config.fifo_capacity_frames - buffered) {
    result.status = DirectionalSrcStatus::InputOverflow;
    return result;
  }

  for (uint32_t frame = 0; frame < frames; ++frame) {
    const uint32_t destination_frame =
        static_cast<uint32_t>(write + frame) & (impl_->config.fifo_capacity_frames - 1);
    const size_t destination_base = static_cast<size_t>(destination_frame) * impl_->config.channels;
    for (uint16_t channel = 0; channel < channels; ++channel) {
      impl_->fifo[destination_base + channel] = input[channel][frame];
    }
  }
  impl_->write_index.store(write + frames, std::memory_order_release);
  if (write + frames - read >=
      static_cast<uint64_t>(kHistoryFrames + 1 + impl_->config.prime_input_frames)) {
    impl_->primed.store(true, std::memory_order_release);
  }
  result.status = DirectionalSrcStatus::Ok;
  result.input_frames_consumed = frames;
  return result;
}

DirectionalSrcTransfer DirectionalSampleRateConverter::renderOutput(float* const* output,
                                                                    uint16_t channels,
                                                                    uint32_t frames) noexcept {
  DirectionalSrcTransfer result;
  if (!impl_) {
    return result;
  }
  if (frames > impl_->config.max_output_frames || output == nullptr ||
      channels != impl_->config.channels) {
    result.status = DirectionalSrcStatus::InvalidArgument;
    return result;
  }
  if (frames == 0) {
    result.status = DirectionalSrcStatus::Ok;
    return result;
  }
  if (!impl_->primed.load(std::memory_order_acquire)) {
    result.status = DirectionalSrcStatus::InputUnderflow;
    return result;
  }

  uint64_t required = 0;
  if (!checkedRequiredInput(impl_->phase_remainder, impl_->phase_increment, frames,
                            impl_->denominator, required)) {
    result.status = DirectionalSrcStatus::InvalidArgument;
    return result;
  }
  const uint64_t read = impl_->read_index.load(std::memory_order_relaxed);
  const uint64_t write = impl_->write_index.load(std::memory_order_acquire);
  const uint64_t buffered = write - read;
  if (buffered < required) {
    result.status = DirectionalSrcStatus::InputUnderflow;
    return result;
  }

  uint64_t cursor = read;
  for (uint32_t frame = 0; frame < frames; ++frame) {
    const uint64_t scaled_phase = impl_->phase_remainder * kPhaseIntervals;
    const uint32_t phase_index = static_cast<uint32_t>(scaled_phase / impl_->denominator);
    const double phase_fraction = static_cast<double>(scaled_phase % impl_->denominator) /
                                  static_cast<double>(impl_->denominator);
    const size_t kernel_base = static_cast<size_t>(phase_index) * kTaps;
    const size_t next_kernel_base = static_cast<size_t>(phase_index + 1) * kTaps;

    for (uint16_t channel = 0; channel < channels; ++channel) {
      double sum = 0.0;
      for (uint32_t tap = 0; tap < kTaps; ++tap) {
        const int32_t offset = static_cast<int32_t>(tap) - static_cast<int32_t>(kHistoryFrames);
        const float sample = impl_->sampleAt(cursor, offset, channel);
        const double coefficient = impl_->phase_kernels[kernel_base + tap] +
                                   (impl_->phase_kernels[next_kernel_base + tap] -
                                    impl_->phase_kernels[kernel_base + tap]) *
                                       phase_fraction;
        sum += coefficient * static_cast<double>(sample);
      }
      output[channel][frame] = static_cast<float>(sum);
    }

    const uint64_t phase_total = impl_->phase_remainder + impl_->phase_increment;
    const uint64_t consumed = phase_total / impl_->denominator;
    impl_->phase_remainder = phase_total % impl_->denominator;
    for (uint64_t input_frame = 0; input_frame < consumed; ++input_frame) {
      impl_->rememberConsumedFrame(cursor);
      ++cursor;
    }
  }

  impl_->read_index.store(cursor, std::memory_order_release);
  impl_->output_frames_produced += frames;
  result.status = DirectionalSrcStatus::Ok;
  result.input_frames_consumed = static_cast<uint32_t>(cursor - read);
  result.output_frames_produced = frames;
  return result;
}

bool DirectionalSampleRateConverter::setInputRateCorrectionPpm(int32_t ppm) noexcept {
  if (!impl_ || !impl_->config.allow_input_rate_correction || ppm < -kMaximumCorrectionPpm ||
      ppm > kMaximumCorrectionPpm) {
    return false;
  }
  impl_->correction_ppm = ppm;
  impl_->phase_increment = impl_->correctedPhaseIncrement(ppm);
  return true;
}

bool DirectionalSampleRateConverter::isPrimed() const noexcept {
  return impl_ != nullptr && impl_->primed.load(std::memory_order_acquire);
}
uint32_t DirectionalSampleRateConverter::bufferedInputFrames() const noexcept {
  return impl_ ? impl_->bufferedFrames() : 0;
}

uint32_t DirectionalSampleRateConverter::latencyOutputFrames() const noexcept {
  if (!impl_) {
    return 0;
  }
  const uint64_t numerator =
      static_cast<uint64_t>(kHistoryFrames + impl_->config.prime_input_frames) *
      impl_->config.output_rate;
  return static_cast<uint32_t>(ceilDivide(numerator, impl_->config.input_rate));
}

void DirectionalSampleRateConverter::reset() noexcept {
  if (!impl_) {
    return;
  }
  impl_->write_index.store(0, std::memory_order_relaxed);
  impl_->read_index.store(0, std::memory_order_relaxed);
  impl_->phase_remainder = 0;
  impl_->primed.store(false, std::memory_order_release);
  impl_->phase_increment = impl_->nominal_phase_increment;
  impl_->correction_ppm = 0;
  impl_->output_frames_produced = 0;
  impl_->history_head = 0;
  std::fill(impl_->fifo.begin(), impl_->fifo.end(), 0.0F);
  std::fill(impl_->history.begin(), impl_->history.end(), 0.0F);
}

} // namespace orpheus::audio_utils
