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

std::vector<std::uint8_t> QuantizeInterleaved(const std::vector<double>& samples,
                                              std::uint16_t bit_depth_bits, bool dither,
                                              std::uint64_t seed);

} // namespace orpheus::core::render
