// SPDX-License-Identifier: MIT
#include <orpheus/streaming_sample_rate_converter.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <numeric>

namespace orpheus {
namespace {

constexpr double kPi = 3.14159265358979323846;

double blackman(double position) noexcept {
  return 0.42 - 0.5 * std::cos(2.0 * kPi * position) +
         0.08 * std::cos(4.0 * kPi * position);
}

bool checkedMultiply(size_t a, size_t b, size_t& result) noexcept {
  if (a != 0 && b > std::numeric_limits<size_t>::max() / a) {
    return false;
  }
  result = a * b;
  return true;
}

class StreamingSampleRateConverter final : public IStreamingSampleRateConverter {
public:
  StreamingSampleRateConverter(const StreamingSampleRateConfig& config, uint32_t interpolation,
                               uint32_t decimation, uint32_t fir_half, uint32_t fir_length)
      : config_(config), interpolation_(interpolation), decimation_(decimation),
        fir_half_(fir_half), fir_length_(fir_length), identity_(interpolation == decimation),
        phase_taps_(new double[static_cast<size_t>(interpolation_) * fir_length_]),
        history_(new float[static_cast<size_t>(fir_length_) * config_.channel_count]),
        window_(new float[(static_cast<size_t>(fir_length_) + config_.max_input_frames) *
                          config_.channel_count]) {
    const double ratio = static_cast<double>(config_.output_sample_rate) /
                         static_cast<double>(config_.input_sample_rate);
    const double cutoff = 0.5 * std::min(1.0, ratio);
    for (uint32_t phase = 0; phase < interpolation_; ++phase) {
      const double fraction = static_cast<double>(phase) / interpolation_;
      double* taps = phase_taps_.get() + static_cast<size_t>(phase) * fir_length_;
      double normalization = 0.0;
      for (uint32_t tap_index = 0; tap_index < fir_length_; ++tap_index) {
        const double distance =
            (static_cast<double>(tap_index) - static_cast<double>(fir_half_)) - fraction;
        const double x = 2.0 * cutoff * distance;
        const double sinc = std::abs(x) < 1e-12 ? 1.0 : std::sin(kPi * x) / (kPi * x);
        double window_position =
            (distance + static_cast<double>(fir_half_)) / static_cast<double>(fir_length_ - 1);
        window_position = std::clamp(window_position, 0.0, 1.0);
        taps[tap_index] = 2.0 * cutoff * sinc * blackman(window_position);
        normalization += taps[tap_index];
      }
      if (std::abs(normalization) > 1e-12) {
        for (uint32_t tap_index = 0; tap_index < fir_length_; ++tap_index) {
          taps[tap_index] /= normalization;
        }
      }
    }
    reset();
  }

  void reset() noexcept override {
    std::fill_n(history_.get(), static_cast<size_t>(fir_length_) * config_.channel_count, 0.0f);
    input_position_ = 0;
    next_center_ = 0;
    next_phase_ = 0;
  }

  StreamingSampleRateResult process(const float* input, uint32_t input_frames, float* output,
                                    uint32_t output_capacity_frames) noexcept override {
    if (input == nullptr || output == nullptr || input_frames == 0 ||
        input_frames > config_.max_input_frames || output_capacity_frames == 0 ||
        output_capacity_frames > config_.max_output_frames) {
      return {SessionGraphError::InvalidParameter, 0, 0};
    }
    if (identity_) {
      if (input_frames > output_capacity_frames) {
        return {SessionGraphError::InvalidParameter, 0, 0};
      }
      std::memcpy(output, input,
                  static_cast<size_t>(input_frames) * config_.channel_count * sizeof(float));
      input_position_ += input_frames;
      next_center_ += input_frames;
      return {SessionGraphError::OK, input_frames, input_frames};
    }

    const uint64_t block_end = input_position_ + input_frames;
    uint64_t center = next_center_;
    uint32_t phase = next_phase_;
    uint32_t needed = 0;
    while (center + fir_half_ < block_end) {
      if (++needed > config_.max_output_frames || needed > output_capacity_frames) {
        return {SessionGraphError::InvalidParameter, 0, 0};
      }
      advance(center, phase);
    }

    const size_t history_samples = static_cast<size_t>(fir_length_) * config_.channel_count;
    std::memcpy(window_.get(), history_.get(), history_samples * sizeof(float));
    std::memcpy(window_.get() + history_samples, input,
                static_cast<size_t>(input_frames) * config_.channel_count * sizeof(float));
    const int64_t window_base =
        static_cast<int64_t>(input_position_) - static_cast<int64_t>(fir_length_);

    center = next_center_;
    phase = next_phase_;
    for (uint32_t output_frame = 0; output_frame < needed; ++output_frame) {
      const int64_t first =
          static_cast<int64_t>(center) - static_cast<int64_t>(fir_half_);
      const double* taps = phase_taps_.get() + static_cast<size_t>(phase) * fir_length_;
      for (uint16_t channel = 0; channel < config_.channel_count; ++channel) {
        double accumulator = 0.0;
        for (uint32_t tap_index = 0; tap_index < fir_length_; ++tap_index) {
          const size_t window_frame =
              static_cast<size_t>(first + static_cast<int64_t>(tap_index) - window_base);
          accumulator += taps[tap_index] *
                         static_cast<double>(window_[window_frame * config_.channel_count + channel]);
        }
        output[static_cast<size_t>(output_frame) * config_.channel_count + channel] =
            static_cast<float>(accumulator);
      }
      advance(center, phase);
    }

    const size_t total_frames = static_cast<size_t>(fir_length_) + input_frames;
    const float* tail = window_.get() + (total_frames - fir_length_) * config_.channel_count;
    std::memcpy(history_.get(), tail, history_samples * sizeof(float));
    input_position_ = block_end;
    next_center_ = center;
    next_phase_ = phase;
    return {SessionGraphError::OK, input_frames, needed};
  }

private:
  void advance(uint64_t& center, uint32_t& phase) const noexcept {
    const uint64_t phase_total = static_cast<uint64_t>(phase) + decimation_;
    center += phase_total / interpolation_;
    phase = static_cast<uint32_t>(phase_total % interpolation_);
  }

  StreamingSampleRateConfig config_;
  uint32_t interpolation_;
  uint32_t decimation_;
  uint32_t fir_half_;
  uint32_t fir_length_;
  bool identity_;
  std::unique_ptr<double[]> phase_taps_;
  std::unique_ptr<float[]> history_;
  std::unique_ptr<float[]> window_;
  uint64_t input_position_{0};
  uint64_t next_center_{0};
  uint32_t next_phase_{0};
};

} // namespace

Result<std::unique_ptr<IStreamingSampleRateConverter>>
createStreamingSampleRateConverter(const StreamingSampleRateConfig& config) {
  if (config.channel_count == 0 || config.input_sample_rate == 0 ||
      config.output_sample_rate == 0 || config.max_input_frames == 0 ||
      config.max_output_frames == 0 || config.taps_per_phase < 8) {
    return {nullptr, SessionGraphError::InvalidParameter, "Invalid streaming sample-rate config"};
  }

  const uint32_t divisor = std::gcd(config.input_sample_rate, config.output_sample_rate);
  const uint32_t interpolation = config.output_sample_rate / divisor;
  const uint32_t decimation = config.input_sample_rate / divisor;
  const double ratio = static_cast<double>(config.output_sample_rate) /
                       static_cast<double>(config.input_sample_rate);
  const double cutoff = 0.5 * std::min(1.0, ratio);
  const double half_width = static_cast<double>(config.taps_per_phase) * (0.5 / cutoff);
  if (!std::isfinite(half_width) || half_width > (std::numeric_limits<uint32_t>::max() - 1) / 2) {
    return {nullptr, SessionGraphError::InvalidParameter, "Streaming FIR size overflow"};
  }
  const uint32_t fir_half = std::max(1u, static_cast<uint32_t>(std::ceil(half_width)));
  const uint32_t fir_length = fir_half * 2 + 1;

  size_t coefficient_count = 0;
  size_t history_count = 0;
  size_t window_frames = 0;
  size_t window_count = 0;
  if (!checkedMultiply(interpolation, fir_length, coefficient_count) ||
      coefficient_count > std::numeric_limits<size_t>::max() / sizeof(double) ||
      !checkedMultiply(fir_length, config.channel_count, history_count) ||
      history_count > std::numeric_limits<size_t>::max() / sizeof(float) ||
      config.max_input_frames > std::numeric_limits<size_t>::max() - fir_length) {
    return {nullptr, SessionGraphError::InvalidParameter, "Streaming converter storage overflow"};
  }
  window_frames = static_cast<size_t>(fir_length) + config.max_input_frames;
  if (!checkedMultiply(window_frames, config.channel_count, window_count) ||
      window_count > std::numeric_limits<size_t>::max() / sizeof(float)) {
    return {nullptr, SessionGraphError::InvalidParameter, "Streaming converter storage overflow"};
  }

  try {
    return {std::make_unique<StreamingSampleRateConverter>(config, interpolation, decimation,
                                                           fir_half, fir_length),
            SessionGraphError::OK, {}};
  } catch (const std::bad_alloc&) {
    return {nullptr, SessionGraphError::InternalError,
            "Unable to allocate streaming sample-rate storage"};
  }
}

} // namespace orpheus
