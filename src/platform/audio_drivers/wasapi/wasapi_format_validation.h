// SPDX-License-Identifier: MIT
#pragma once

#include <bit>
#include <cstdint>
#include <limits>

namespace orpheus::detail {

enum class WASAPISampleType { Unsupported, Pcm16, Float32 };

/// Native descriptor facts; zero channel mask means unspecified speaker placement.
struct WASAPIFormatShape {
  uint32_t sample_rate{0};
  uint16_t channels{0};
  uint16_t bits_per_sample{0};
  uint16_t block_align{0};
  uint32_t average_bytes_per_second{0};
  uint32_t channel_mask{0};
  uint16_t valid_bits_per_sample{0};
  bool extensible{false};
  WASAPISampleType encoding{WASAPISampleType::Unsupported};
};

/// Allocation-free validation shared by the native descriptor bridge and driver.
inline constexpr bool validateWASAPIFormatShape(const WASAPIFormatShape& shape,
                                                uint16_t requested_outputs) noexcept {
  if (shape.sample_rate == 0 || requested_outputs == 0 || shape.channels != requested_outputs) {
    return false;
  }
  const uint16_t supported_bits = shape.encoding == WASAPISampleType::Pcm16
                                      ? 16
                                      : (shape.encoding == WASAPISampleType::Float32 ? 32 : 0);
  if (supported_bits == 0 || shape.bits_per_sample != supported_bits ||
      (shape.extensible && shape.valid_bits_per_sample != supported_bits)) {
    return false;
  }
  const uint32_t block_align = static_cast<uint32_t>(shape.channels) * (supported_bits / 8u);
  const uint64_t average_bytes = static_cast<uint64_t>(shape.sample_rate) * block_align;
  if (block_align > std::numeric_limits<uint16_t>::max() || shape.block_align != block_align ||
      average_bytes > std::numeric_limits<uint32_t>::max() ||
      shape.average_bytes_per_second != average_bytes) {
    return false;
  }
  return shape.channel_mask == 0 || std::popcount(shape.channel_mask) == shape.channels;
}

} // namespace orpheus::detail
