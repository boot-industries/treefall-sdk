// SPDX-License-Identifier: MIT
#include <ShmUI.h>
#include <orpheus/audio_analysis.h>
#include <orpheus/version.h>

#include <array>

int main() {
  static_assert(orpheus::kSdkVersionMajor == ORPHEUS_SDK_VERSION_MAJOR);

  constexpr std::array<float, 8> impulse{1.0f, 0.0f, 0.0f, 0.0f,
                                         0.0f, 0.0f, 0.0f, 0.0f};
  const auto spectrum = orpheus::analysis::magnitudeSpectrum(
      impulse.data(), impulse.size(), 48000, orpheus::analysis::WindowType::Rectangular);

  const auto& theme = shmui::defaultTheme();

  return spectrum.fftSize == impulse.size() && !spectrum.magnitudes.empty() &&
      shmui::Version::major == 2 && !theme.accent.isTransparent() ? 0 : 1;
}
