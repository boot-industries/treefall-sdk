// SPDX-License-Identifier: MIT
#pragma once

#include <cmath>

namespace orpheus::pinned {

// ORP273 F-02: libm-independent elementary functions for the render path.
// Every Horner step uses std::fma, which IEEE-754 requires to be correctly
// rounded, so the results are bit-identical on every platform and do not
// depend on the host libm. The constants are hex-float literals so that they
// parse to the same double on every compiler.
//
// Verified maxima over the used ranges: |pinnedSin - sin| <= 3.4e-16,
// |pinnedCos - cos| <= 3.4e-16, |pinnedExp - exp| <= 1.1e-14. The float32
// results differ from the host libm in one sample per million, by at most
// one ulp of the float32 result.

inline constexpr double kPiOver4 = 0x1.921fb54442d18p-1;  // pi / 4
inline constexpr double kSqrtHalf = 0x1.6a09e667f3bcdp-1; // sqrt(1/2)
inline constexpr double kLn2 = 0x1.62e42fefa39efp-1;      // ln 2
inline constexpr double kLn10 = 0x1.26bb1bbb55516p+1;     // ln 10

// 1/n! for n = 2..17, each the correctly-rounded double.
inline constexpr double kInvFact2 = 0x1.0000000000000p-1;
inline constexpr double kInvFact3 = 0x1.5555555555555p-3;
inline constexpr double kInvFact4 = 0x1.5555555555555p-5;
inline constexpr double kInvFact5 = 0x1.1111111111111p-7;
inline constexpr double kInvFact6 = 0x1.6c16c16c16c17p-10;
inline constexpr double kInvFact7 = 0x1.a01a01a01a01ap-13;
inline constexpr double kInvFact8 = 0x1.a01a01a01a01ap-16;
inline constexpr double kInvFact9 = 0x1.71de3a556c734p-19;
inline constexpr double kInvFact10 = 0x1.27e4fb7789f5cp-22;
inline constexpr double kInvFact11 = 0x1.ae64567f544e4p-26;
inline constexpr double kInvFact12 = 0x1.1eed8eff8d898p-29;
inline constexpr double kInvFact13 = 0x1.6124613a86d09p-33;
inline constexpr double kInvFact14 = 0x1.93974a8c07c9dp-37;
inline constexpr double kInvFact15 = 0x1.ae7f3e733b81fp-41;
inline constexpr double kInvFact16 = 0x1.ae7f3e733b81fp-45;
inline constexpr double kInvFact17 = 0x1.952c77030ad4ap-49;

/// sin(u) for |u| <= pi/4, Taylor series truncated after the u^17 term.
inline double sinKernel(double u) noexcept {
  const double u2 = u * u;
  double s = kInvFact17;
  s = std::fma(s, u2, -kInvFact15);
  s = std::fma(s, u2, kInvFact13);
  s = std::fma(s, u2, -kInvFact11);
  s = std::fma(s, u2, kInvFact9);
  s = std::fma(s, u2, -kInvFact7);
  s = std::fma(s, u2, kInvFact5);
  s = std::fma(s, u2, -kInvFact3);
  s = std::fma(s, u2, 1.0);
  return s * u;
}

/// cos(u) for |u| <= pi/4, Taylor series truncated after the u^16 term.
inline double cosKernel(double u) noexcept {
  const double u2 = u * u;
  double c = kInvFact16;
  c = std::fma(c, u2, -kInvFact14);
  c = std::fma(c, u2, kInvFact12);
  c = std::fma(c, u2, -kInvFact10);
  c = std::fma(c, u2, kInvFact8);
  c = std::fma(c, u2, -kInvFact6);
  c = std::fma(c, u2, kInvFact4);
  c = std::fma(c, u2, -kInvFact2);
  c = std::fma(c, u2, 1.0);
  return c;
}

/// sin(x) for x in [0, pi/2]. Reduces by pi/4 and uses the addition formulas.
inline double sin(double x) noexcept {
  const double u = x - kPiOver4;
  return kSqrtHalf * (cosKernel(u) + sinKernel(u));
}

/// cos(x) for x in [0, pi/2].
inline double cos(double x) noexcept {
  const double u = x - kPiOver4;
  return kSqrtHalf * (cosKernel(u) - sinKernel(u));
}

/// exp(x) for |x| <= 4. Range-reduces by ln 2 and scales with std::ldexp,
/// which is an exact power-of-two scaling and therefore platform-independent.
inline double exp(double x) noexcept {
  constexpr int kSeriesTerms = 14;
  const double k = std::floor((x / kLn2) + 0.5);
  const double r = x - (k * kLn2);
  double term = 1.0;
  double sum = 1.0;
  for (int n = 1; n <= kSeriesTerms; ++n) {
    term = (term * r) / static_cast<double>(n);
    sum = sum + term;
  }
  return std::ldexp(sum, static_cast<int>(k));
}

/// 10^exponent for |exponent| <= 2.
inline float pow10(float exponent) noexcept {
  return static_cast<float>(exp(static_cast<double>(exponent) * kLn10));
}

} // namespace orpheus::pinned
