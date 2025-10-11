// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <vector>

namespace orpheus::core::render {

class TpdfDitherGenerator {
public:
  explicit TpdfDitherGenerator(std::uint64_t seed) : state_(seed) {}

  double Next(double lsb) {
    if (lsb == 0.0) {
      return 0.0;
    }
    return (Uniform() - Uniform()) * lsb;
  }

private:
  double Uniform();

  std::uint64_t state_;
};

// Quantize a set of interleaved samples into PCM bytes.
//
// When `bit_depth_bits` is 32 the renderer emits IEEE-754 float samples. The
// dithering flag is ignored for this mode because the samples are written
// directly after clamping to [-1.0, 1.0]. Integer depths (16 and 24) continue
// to use TPDF dithering when requested.
std::vector<std::uint8_t> QuantizeInterleaved(const std::vector<double>& samples,
                                              std::uint16_t bit_depth_bits, bool dither,
                                              std::uint64_t seed);

} // namespace orpheus::core::render
