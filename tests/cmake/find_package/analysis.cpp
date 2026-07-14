// SPDX-License-Identifier: MIT
#include <orpheus/audio_analysis.h>

#include <array>

int main() {
  constexpr std::array<float, 8> impulse{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  const auto spectrum = orpheus::analysis::magnitudeSpectrum(
      impulse.data(), impulse.size(), 48000, orpheus::analysis::WindowType::Rectangular);
  return spectrum.fftSize == impulse.size() && !spectrum.magnitudes.empty() ? 0 : 1;
}
