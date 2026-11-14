// SPDX-License-Identifier: MIT
#include <orpheus/parametric_eq.h>

#include <array>
#include <cmath>

int main() {
  orpheus::ParametricEqSettings settings;
  settings.enabled = true;
  settings.bands[0] = {
      true, orpheus::EqBandType::LowShelf, orpheus::EqBandOrder::Second, 160.0F, 12.0F, 0.1F};

  // A Q 0.1 low shelf still lifts 1 kHz by about 4.25 dB.
  const std::array<float, 1> frequencies{1000.0F};
  std::array<float, 1> magnitudes{};
  if (orpheus::parametricEqMagnitudeDb(settings, 48000.0, frequencies, magnitudes) !=
      orpheus::ParametricEqValidationError::OK) {
    return 1;
  }
  if (std::abs(magnitudes[0] - 4.25F) > 0.1F) {
    return 2;
  }

  orpheus::ParametricEqProcessor processor;
  if (processor.prepare(settings, 48000.0, 1) != orpheus::ParametricEqValidationError::OK) {
    return 3;
  }
  float sample = 0.5F;
  processor.processFrame(&sample, 1);
  return std::isfinite(sample) && processor.active() ? 0 : 4;
}
