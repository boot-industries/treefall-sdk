// SPDX-License-Identifier: MIT
// ORP121 Q-04: True-Peak Metering Estimator
#pragma once

#include <array>
#include <cmath>
#if defined(__aarch64__) || defined(__arm64__)
#include <arm_neon.h>
#elif defined(_M_X64) || defined(__SSE2__)
#include <emmintrin.h>
#endif

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
  friend struct TruePeakMeterTestAccess;

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
    m_history_head = 0;
  }

  /// Process a single sample and return true-peak value
  /// @param sample Input sample
  /// @return Maximum interpolated peak value (absolute)
  float process(float sample) {
    // A duplicated circular history keeps the newest-to-oldest tap order
    // contiguous without shifting eleven samples for every input sample.
    m_history_head =
        m_history_head == 0 ? static_cast<size_t>(TAPS_PER_PHASE - 1) : m_history_head - 1;
    m_history[m_history_head] = sample;
    m_history[m_history_head + TAPS_PER_PHASE] = sample;

    float peak = std::abs(sample); // Include original sample
    const float* history = &m_history[m_history_head];
#if defined(_M_X64) || defined(__SSE2__)
    // Accumulate all four FIR phases in parallel. Each lane keeps its
    // canonical tap order; transposed coefficients make each tap contiguous.
    __m128 phaseSums = _mm_setzero_ps();
    for (size_t tap = 0; tap < static_cast<size_t>(TAPS_PER_PHASE); ++tap) {
      const __m128 coefficients = _mm_loadu_ps(s_transposedFilterCoeffs[tap].data());
      phaseSums = _mm_add_ps(phaseSums, _mm_mul_ps(_mm_set1_ps(history[tap]), coefficients));
    }
    // Keep the current peak as MAXPS's second operand so a NaN phase retains
    // the earlier peak, matching the sequential std::max updates below.
    const __m128 phaseAbs = _mm_andnot_ps(_mm_set1_ps(-0.0f), phaseSums);
    const __m128 withSamplePeak = _mm_max_ps(phaseAbs, _mm_set1_ps(peak));
    const __m128 pairPeaks =
        _mm_max_ps(withSamplePeak, _mm_movehl_ps(withSamplePeak, withSamplePeak));
    peak = _mm_cvtss_f32(_mm_max_ss(pairPeaks, _mm_shuffle_ps(pairPeaks, pairPeaks, 1)));
#elif defined(__aarch64__) || defined(__arm64__)
    float32x4_t phaseSums = vdupq_n_f32(0.0f);
    for (size_t tap = 0; tap < static_cast<size_t>(TAPS_PER_PHASE); ++tap) {
      const float32x4_t coefficients = vld1q_f32(s_transposedFilterCoeffs[tap].data());
      const float32x4_t input = vdupq_n_f32(history[tap]);
      phaseSums = vaddq_f32(phaseSums, vmulq_f32(input, coefficients));
    }
    peak = std::max(peak, std::abs(vgetq_lane_f32(phaseSums, 0)));
    peak = std::max(peak, std::abs(vgetq_lane_f32(phaseSums, 1)));
    peak = std::max(peak, std::abs(vgetq_lane_f32(phaseSums, 2)));
    peak = std::max(peak, std::abs(vgetq_lane_f32(phaseSums, 3)));
#else
    for (size_t phase = 0; phase < static_cast<size_t>(OVERSAMPLE_FACTOR); ++phase) {
      const auto& coefficients = s_filterCoeffs[phase];
      float interpolated = history[0] * coefficients[0];
      interpolated += history[1] * coefficients[1];
      interpolated += history[2] * coefficients[2];
      interpolated += history[3] * coefficients[3];
      interpolated += history[4] * coefficients[4];
      interpolated += history[5] * coefficients[5];
      interpolated += history[6] * coefficients[6];
      interpolated += history[7] * coefficients[7];
      interpolated += history[8] * coefficients[8];
      interpolated += history[9] * coefficients[9];
      interpolated += history[10] * coefficients[10];
      interpolated += history[11] * coefficients[11];
      peak = std::max(peak, std::abs(interpolated));
    }
#endif

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
  std::array<float, TAPS_PER_PHASE * 2> m_history{};
  size_t m_history_head{0};

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

  // Generated from the canonical filter table for contiguous four-phase loads.
  static constexpr std::array<std::array<float, OVERSAMPLE_FACTOR>, TAPS_PER_PHASE>
      s_transposedFilterCoeffs = [] {
        std::array<std::array<float, OVERSAMPLE_FACTOR>, TAPS_PER_PHASE> transposed{};
        for (size_t tap = 0; tap < static_cast<size_t>(TAPS_PER_PHASE); ++tap) {
          for (size_t phase = 0; phase < static_cast<size_t>(OVERSAMPLE_FACTOR); ++phase) {
            transposed[tap][phase] = s_filterCoeffs[phase][tap];
          }
        }
        return transposed;
      }();
};

// Static member definition
constexpr std::array<std::array<float, TruePeakMeter::TAPS_PER_PHASE>,
                     TruePeakMeter::OVERSAMPLE_FACTOR>
    TruePeakMeter::s_filterCoeffs;

constexpr std::array<std::array<float, TruePeakMeter::OVERSAMPLE_FACTOR>,
                     TruePeakMeter::TAPS_PER_PHASE>
    TruePeakMeter::s_transposedFilterCoeffs;

} // namespace orpheus
