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

/// Canonical speaker mask for a channel count, or 0 for unspecified placement.
inline constexpr uint32_t canonicalWASAPISpeakerMask(uint16_t channels) noexcept {
  switch (channels) {
  case 1:
    return 0x4; // SPEAKER_FRONT_CENTER
  case 2:
    return 0x3; // SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT
  case 4:
    return 0x33; // SPEAKER_QUAD
  case 6:
    return 0x3F; // SPEAKER_SURROUND
  case 8:
    return 0x63F; // SPEAKER_7POINT1
  default:
    return 0; // unspecified speaker placement
  }
}

/// Speaker mask a self-authored request descriptor has to carry.
///
/// A device mix format that describes more channels than the request keeps a
/// speaker mask that no longer matches the request, so the request would be
/// rejected by its own validation before the device is ever asked whether it
/// supports the format. The request is self-authored, so its mask has to
/// describe its own channel count: an inherited mask that does not match is
/// replaced by the canonical layout for the requested count, or by the
/// unspecified value when that count has no canonical layout. An inherited
/// mask that already matches is kept, so a device layout the engine agrees
/// with survives unchanged.
inline constexpr uint32_t reconcileWASAPIChannelMask(uint32_t inherited_mask,
                                                     uint16_t requested_channels) noexcept {
  if (inherited_mask == 0 || std::popcount(inherited_mask) == requested_channels) {
    return inherited_mask;
  }
  return canonicalWASAPISpeakerMask(requested_channels);
}

} // namespace orpheus::detail
