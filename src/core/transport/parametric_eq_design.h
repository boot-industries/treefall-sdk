// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/parametric_eq.h>

#include <cstdint>

namespace orpheus::detail {

/// Trapezoidal SVF prototype H(s) = (m0 s^2 + m1 s + m2) / (s^2 + k s + 1),
/// with s = (1/g)(1 - z^-1)/(1 + z^-1).
struct EqSvfDesign {
  double g;
  double k;
  double m0;
  double m1;
  double m2;
};

/// Precondition: band passes validateParametricEq at sampleRate.
EqSvfDesign designEqBand(const EqBandSettings& band, double sampleRate) noexcept;

/// Transition length in frames: max(1, round(sampleRate * kParametricEqTransitionSeconds)).
uint32_t parametricEqTransitionFrames(double sampleRate) noexcept;

} // namespace orpheus::detail
