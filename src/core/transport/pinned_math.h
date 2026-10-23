// SPDX-License-Identifier: MIT
#pragma once

#include <cmath>

namespace orpheus::pinned {

// ORP273 F-02: libm-independent elementary functions for the render path.
// Horner steps explicitly round the multiply before the add, avoiding both
// baseline x86 libm fma calls and target-dependent implicit contraction.
// Transport/routing targets also disable implicit contraction in surrounding
// render arithmetic. Hex-float constants parse identically on each compiler.
//
// Accuracy smoke over 1,000,001 positions: sin/cos absolute error <= 3.34e-16
// on [0, pi/2]; pow10 matches float-rounded libm on [-4.8, 1.2] (the clip
// gain and DSP threshold range). This is accuracy evidence, not proof of every
// DSP program's cross-platform determinism.

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

/// Multiply and add with exactly two IEEE roundings, never fused into a
/// single-rounding FMA. The volatile product forces the multiply to be
/// rounded before the add.
inline double mulAdd(double a, double b, double c) noexcept {
  volatile double product = a * b;
  return product + c;
}

/// sin(u) for |u| <= pi/4, Taylor series truncated after the u^17 term.
inline double sinKernel(double u) noexcept {
  const double u2 = u * u;
  double s = kInvFact17;
  s = mulAdd(s, u2, -kInvFact15);
  s = mulAdd(s, u2, kInvFact13);
  s = mulAdd(s, u2, -kInvFact11);
  s = mulAdd(s, u2, kInvFact9);
  s = mulAdd(s, u2, -kInvFact7);
  s = mulAdd(s, u2, kInvFact5);
  s = mulAdd(s, u2, -kInvFact3);
  s = mulAdd(s, u2, 1.0);
  return s * u;
}

/// cos(u) for |u| <= pi/4, Taylor series truncated after the u^16 term.
inline double cosKernel(double u) noexcept {
  const double u2 = u * u;
  double c = kInvFact16;
  c = mulAdd(c, u2, -kInvFact14);
  c = mulAdd(c, u2, kInvFact12);
  c = mulAdd(c, u2, -kInvFact10);
  c = mulAdd(c, u2, kInvFact8);
  c = mulAdd(c, u2, -kInvFact6);
  c = mulAdd(c, u2, kInvFact4);
  c = mulAdd(c, u2, -kInvFact2);
  c = mulAdd(c, u2, 1.0);
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

/// exp(x) over the gain/threshold range [-12, 4]. Reduces by ln 2 and
/// scales with std::ldexp (an exact power-of-two scaling).
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

/// 10^exponent for the gain/threshold range [-4.8, 1.2].
inline float pow10(float exponent) noexcept {
  return static_cast<float>(exp(static_cast<double>(exponent) * kLn10));
}

} // namespace orpheus::pinned
