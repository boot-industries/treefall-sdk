// SPDX-License-Identifier: MIT
#include "render/pcm.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace orpheus::core::render {
namespace {
constexpr std::uint64_t kLcgMultiplier = 6364136223846793005ull;
constexpr std::uint64_t kLcgIncrement = 1ull;
constexpr std::uint64_t kMantissaMask = (1ull << 53u) - 1ull;
constexpr int kMantissaShift = 11;
}  // namespace

double TpdfDitherGenerator::Uniform() {
  state_ = state_ * kLcgMultiplier + kLcgIncrement;
  const std::uint64_t mantissa = (state_ >> kMantissaShift) & kMantissaMask;
  return static_cast<double>(mantissa) / static_cast<double>(1ull << 53u);
}

std::vector<std::uint8_t> QuantizeInterleaved(
    const std::vector<double> &samples, std::uint16_t bit_depth_bits,
    bool dither, std::uint64_t seed) {
  if (bit_depth_bits != 16u && bit_depth_bits != 24u && bit_depth_bits != 32u) {
    throw std::invalid_argument("Unsupported bit depth");
  }

  const auto RoundTiesToZero = [](double value) -> std::int64_t {
    const double floored = std::floor(value);
    const double fraction = value - floored;
    if (fraction > 0.5) {
      return static_cast<std::int64_t>(floored + 1.0);
    }
    if (fraction < 0.5) {
      return static_cast<std::int64_t>(floored);
    }
    if (value >= 0.0) {
      return static_cast<std::int64_t>(floored);
    }
    return static_cast<std::int64_t>(floored + 1.0);
  };

  if (bit_depth_bits == 32u) {
    std::vector<std::uint8_t> pcm(samples.size() * sizeof(float));
    for (std::size_t index = 0; index < samples.size(); ++index) {
      const double clamped = std::clamp(samples[index], -1.0, 1.0);
      std::int64_t quantized =
          RoundTiesToZero(clamped * static_cast<double>(1 << 15));
      quantized = std::clamp<std::int64_t>(quantized, -32768, 32767);
      const float value = static_cast<float>(
          static_cast<double>(quantized) / static_cast<double>(1 << 15));
      std::uint32_t raw = 0;
      std::memcpy(&raw, &value, sizeof(float));
      const std::size_t offset = index * sizeof(float);
      pcm[offset] = static_cast<std::uint8_t>(raw & 0xFF);
      pcm[offset + 1] = static_cast<std::uint8_t>((raw >> 8) & 0xFF);
      pcm[offset + 2] = static_cast<std::uint8_t>((raw >> 16) & 0xFF);
      pcm[offset + 3] = static_cast<std::uint8_t>((raw >> 24) & 0xFF);
    }
    return pcm;
  }

  const std::size_t bytes_per_sample = bit_depth_bits == 24u ? 3u : 2u;
  const std::int64_t min_value = -(1ll << (bit_depth_bits - 1));
  const std::int64_t max_value = (1ll << (bit_depth_bits - 1)) - 1ll;
  const double max_amplitude = static_cast<double>(max_value);
  const double lsb =
      bit_depth_bits == 0u
          ? 0.0
          : 1.0 / static_cast<double>(1ull << (bit_depth_bits - 1));

  std::vector<std::uint8_t> pcm(samples.size() * bytes_per_sample);
  TpdfDitherGenerator generator(seed);

  for (std::size_t index = 0; index < samples.size(); ++index) {
    double sample = std::clamp(samples[index], -1.0, 1.0);
    if (dither && lsb > 0.0) {
      sample += generator.Next(lsb);
    }
    sample = std::clamp(sample, -1.0, 1.0);

    std::int64_t quantized = RoundTiesToZero(sample * max_amplitude);
    quantized = std::clamp(quantized, min_value, max_value);

    const std::size_t offset = index * bytes_per_sample;
    if (bytes_per_sample == 2u) {
      const std::int16_t value = static_cast<std::int16_t>(quantized);
      pcm[offset] = static_cast<std::uint8_t>(value & 0xFF);
      pcm[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    } else {
      const std::int32_t value = static_cast<std::int32_t>(quantized);
      pcm[offset] = static_cast<std::uint8_t>(value & 0xFF);
      pcm[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
      pcm[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    }
  }

  return pcm;
}

}  // namespace orpheus::core::render
