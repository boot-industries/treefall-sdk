// SPDX-License-Identifier: MIT
// ORP121 Q-04: True-Peak Metering Estimator
#pragma once

#include <array>
#include <cmath>
#include <cstring>

namespace orpheus {

/// SDK true-peak estimator modeled against ITU-R BS.1770-4 terminology.
///
/// This estimator uses 4x oversampling to expose inter-sample peaks that can
/// cause clipping in downstream D/A converters. It is not a standalone claim
/// of standards conformance.
///
/// Configuration:
/// - 4x oversampling factor
/// - 12-tap polyphase filter per phase (48 total coefficients)
/// - Zero allocation and bounded work per input sample
class TruePeakMeter {
public:
  static constexpr int OVERSAMPLE_FACTOR = 4;
  static constexpr int TAPS_PER_PHASE = 12;
  static constexpr int TOTAL_TAPS = OVERSAMPLE_FACTOR * TAPS_PER_PHASE; // 48

  TruePeakMeter() {
    reset();
  }

  /// Reset filter history
  void reset() {
    m_history.fill(0.0f);
  }

  /// Process a single sample and return true-peak value
  /// @param sample Input sample
  /// @return Maximum interpolated peak value (absolute)
  float process(float sample) {
    // Shift history and insert new sample
    std::memmove(&m_history[1], &m_history[0], (TAPS_PER_PHASE - 1) * sizeof(float));
    m_history[0] = sample;

    // Calculate 4 interpolated samples (polyphase filter)
    float peak = std::abs(sample); // Include original sample

    for (size_t phase = 0; phase < static_cast<size_t>(OVERSAMPLE_FACTOR); ++phase) {
      float interpolated = 0.0f;
      for (size_t tap = 0; tap < static_cast<size_t>(TAPS_PER_PHASE); ++tap) {
        interpolated += m_history[tap] * s_filterCoeffs[phase][tap];
      }
      peak = std::max(peak, std::abs(interpolated));
    }

    return peak;
  }

  /// Process a buffer and return the maximum true-peak
  /// @param buffer Input buffer
  /// @param num_frames Number of samples
  /// @return Maximum true-peak value in the buffer
  float processBuffer(const float* buffer, size_t num_frames) {
    float max_peak = 0.0f;
    for (size_t i = 0; i < num_frames; ++i) {
      float peak = process(buffer[i]);
      max_peak = std::max(max_peak, peak);
    }
    return max_peak;
  }

private:
  std::array<float, TAPS_PER_PHASE> m_history{};

  // SDK polyphase FIR filter coefficients.
  // 4 phases × 12 taps = 48 coefficients.
  // These windowed-sinc coefficients are selected for the SDK's 4x estimator.
  //
  // Phase 0: Samples at original positions (identity + filtering)
  // Phase 1: Samples at 1/4 offset
  // Phase 2: Samples at 2/4 offset (midpoint)
  // Phase 3: Samples at 3/4 offset
  static constexpr std::array<std::array<float, TAPS_PER_PHASE>, OVERSAMPLE_FACTOR> s_filterCoeffs =
      {{
          // Phase 0 (original sample positions with anti-aliasing)
          {0.0017089843750f, -0.0291748046875f, -0.0189208984375f, 0.1109619140625f,
           0.2817382812500f, 0.3876953125000f, 0.2817382812500f, 0.1109619140625f,
           -0.0189208984375f, -0.0291748046875f, 0.0017089843750f, 0.0000000000000f},
          // Phase 1 (1/4 sample offset)
          {0.0030517578125f, -0.0133056640625f, -0.0482177734375f, 0.0476074218750f,
           0.2919921875000f, 0.4438476562500f, 0.2220458984375f, 0.0476074218750f,
           -0.0448608398438f, -0.0166015625000f, 0.0073242187500f, -0.0024414062500f},
          // Phase 2 (1/2 sample offset - midpoint)
          {0.0024414062500f, 0.0073242187500f, -0.0598144531250f, -0.0166015625000f,
           0.2324218750000f, 0.4638671875000f, 0.2324218750000f, -0.0166015625000f,
           -0.0598144531250f, 0.0073242187500f, 0.0024414062500f, 0.0000000000000f},
          // Phase 3 (3/4 sample offset)
          {-0.0024414062500f, 0.0073242187500f, -0.0166015625000f, -0.0448608398438f,
           0.0476074218750f, 0.2220458984375f, 0.4438476562500f, 0.2919921875000f, 0.0476074218750f,
           -0.0482177734375f, -0.0133056640625f, 0.0030517578125f},
      }};
};

// Static member definition
constexpr std::array<std::array<float, TruePeakMeter::TAPS_PER_PHASE>,
                     TruePeakMeter::OVERSAMPLE_FACTOR>
    TruePeakMeter::s_filterCoeffs;

} // namespace orpheus
