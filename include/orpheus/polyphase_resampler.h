// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace orpheus {

/// Deterministic rational polyphase sample-rate converter (ORP127 G6).
///
/// Converts interleaved multi-channel audio from an input rate to an output
/// rate using a windowed-sinc (Blackman-windowed) FIR low-pass, evaluated per
/// polyphase branch. The ratio is reduced to L/M by gcd; the anti-alias cutoff
/// tracks the smaller of the two rates.
///
/// Design goals (host-neutral SDK contract):
/// - No third-party dependency (hand-rolled).
/// - Deterministic: same input -> same output, bit-identical across platforms
///   (uses double accumulation and a fixed kernel; no RNG, no fast-math).
/// - Streaming: process() can be called repeatedly; internal history persists.
/// - Offline-capable: works outside any real-time constraint.
///
/// This is a quality/simplicity-balanced converter intended for playback of
/// mismatched-rate files, not a mastering-grade SRC. It is exact for L==M
/// (pass-through) and correct in pitch for arbitrary ratios.
class PolyphaseResampler {
public:
  /// @param inputRate  Source sample rate in Hz (> 0)
  /// @param outputRate Target sample rate in Hz (> 0)
  /// @param numChannels Interleaved channel count (>= 1)
  /// @param tapsPerPhase Half-length of the sinc per phase (quality knob).
  ///        Total FIR length ~= 2*tapsPerPhase*L. 16 is a good default.
  PolyphaseResampler(uint32_t inputRate, uint32_t outputRate, uint16_t numChannels,
                     uint32_t tapsPerPhase = 16);

  /// True if input and output rates are equal (pass-through, no filtering).
  bool isPassthrough() const {
    return m_passthrough;
  }

  uint32_t inputRate() const {
    return m_inputRate;
  }
  uint32_t outputRate() const {
    return m_outputRate;
  }
  uint16_t numChannels() const {
    return m_numChannels;
  }

  /// Convert an interleaved input block to interleaved output.
  ///
  /// @param input  Interleaved input frames (inFrames * numChannels floats).
  /// @param inFrames Number of input frames provided.
  /// @param output Destination vector; resized to hold the produced frames
  ///        (producedFrames * numChannels). Cleared/overwritten each call.
  /// @return Number of output frames produced this call.
  ///
  /// Streaming contract: call repeatedly with successive input blocks. The
  /// converter keeps the filter history and fractional phase between calls, so
  /// the concatenation of outputs equals a single-shot conversion of the
  /// concatenated inputs (up to the startup transient of the FIR).
  size_t process(const float* input, size_t inFrames, std::vector<float>& output);

  /// Estimate the number of output frames for a given input frame count
  /// (useful for preallocation). Approximate near block boundaries.
  size_t estimateOutputFrames(size_t inFrames) const;

  /// Reset streaming state (history + phase) to a clean start.
  void reset();

private:
  double sincKernel(double x) const; // windowed sinc, x in samples (kernel space)

  uint32_t m_inputRate;
  uint32_t m_outputRate;
  uint16_t m_numChannels;
  uint32_t m_tapsPerPhase;
  bool m_passthrough;

  // Reduced ratio outputRate/inputRate = L / M.
  int64_t m_L; // interpolation factor
  int64_t m_M; // decimation factor

  // Precomputed polyphase FIR: for phase p in [0, L), the taps applied to the
  // input history. m_firLen taps per phase, centered.
  uint32_t m_firHalf;              // half-length in INPUT samples
  uint32_t m_firLen;               // full length in INPUT samples (2*m_firHalf+1)
  std::vector<double> m_phaseTaps; // [L][m_firLen] flattened

  // Streaming state: ring of recent input frames (interleaved) large enough to
  // cover the FIR history, plus the current output-sample index t (in output
  // samples since stream start) mapped back to input position.
  std::vector<float> m_history; // interleaved, length = m_firLen * numChannels
  int64_t m_inputPos;           // index of the newest input frame consumed (global)
  int64_t m_outputCount;        // output frames produced so far (global)
};

} // namespace orpheus
