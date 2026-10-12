// SPDX-License-Identifier: MIT
#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace orpheus::core::detail {

inline double RequireFinite(double value, const char* field) {
  if (!std::isfinite(value)) {
    throw std::invalid_argument(std::string(field) + " must be finite");
  }
  return value;
}

template <typename Integer> Integer CheckedInteger(double value, const char* field) {
  static_assert(std::is_integral_v<Integer>);
  RequireFinite(value, field);
  if (std::trunc(value) != value) {
    throw std::invalid_argument(std::string(field) + " must be integral");
  }
  // Exclusive power-of-two upper bounds stay exact even for 64-bit integers;
  // converting max() to double would admit 2^63 or 2^64.
  constexpr double upper = static_cast<double>(std::numeric_limits<Integer>::max() / 2 + 1) * 2.0;
  constexpr double lower = std::is_signed_v<Integer> ? -upper : 0.0;
  if (value < lower || value >= upper) {
    throw std::invalid_argument(std::string(field) + " out of range");
  }
  return static_cast<Integer>(value);
}

inline std::int64_t RoundedSamples(double value, const char* field) {
  RequireFinite(value, field);
  const double rounded = std::round(value);
  constexpr double upper = 0x1p63;
  if (rounded < -upper || rounded >= upper) {
    throw std::invalid_argument(std::string(field) + " out of range");
  }
  return static_cast<std::int64_t>(rounded);
}

} // namespace orpheus::core::detail
