// SPDX-License-Identifier: MIT
#include <orpheus/polyphase_resampler.h>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace orpheus {

namespace {
constexpr double kPi = 3.14159265358979323846;

// Blackman window over [-1, 1] (arg normalized to the FIR half-width).
double blackman(double n01) {
  // n01 in [0,1]; standard Blackman coefficients.
  return 0.42 - 0.5 * std::cos(2.0 * kPi * n01) + 0.08 * std::cos(4.0 * kPi * n01);
}
} // namespace

PolyphaseResampler::PolyphaseResampler(uint32_t inputRate, uint32_t outputRate,
                                       uint16_t numChannels, uint32_t tapsPerPhase)
    : m_inputRate(inputRate == 0 ? 1 : inputRate), m_outputRate(outputRate == 0 ? 1 : outputRate),
      m_numChannels(numChannels == 0 ? 1 : numChannels),
      m_tapsPerPhase(tapsPerPhase == 0 ? 1 : tapsPerPhase), m_passthrough(false), m_L(1), m_M(1),
      m_firHalf(0), m_firLen(0), m_inputPos(0), m_outputCount(0) {
  m_passthrough = (m_inputRate == m_outputRate);

  // Reduce output/input to L/M.
  int64_t g = std::gcd<int64_t>(m_outputRate, m_inputRate);
  m_L = m_outputRate / g; // interpolation
  m_M = m_inputRate / g;  // decimation

  // Anti-alias cutoff (in cycles/input-sample) = 0.5 * min(1, outRate/inRate).
  // When downsampling (outRate < inRate) the cutoff drops to prevent aliasing.
  double ratio = static_cast<double>(m_outputRate) / static_cast<double>(m_inputRate);
  double cutoff = 0.5 * std::min(1.0, ratio); // normalized to input Nyquist

  // FIR half-length in INPUT samples. When downsampling, the transition band
  // widens (kernel spans more input samples), so scale the half-width by the
  // reciprocal cutoff to keep the tap count meaningful.
  double halfWidthInput = static_cast<double>(m_tapsPerPhase) * (0.5 / cutoff);
  m_firHalf = static_cast<uint32_t>(std::ceil(halfWidthInput));
  if (m_firHalf < 1)
    m_firHalf = 1;
  m_firLen = 2 * m_firHalf + 1;

  // Precompute per-phase taps. For output sample n, the continuous input
  // position is t = n * M / L. Its integer part selects the history window; the
  // fractional part is one of L discrete phases (n*M mod L)/L. We precompute the
  // windowed-sinc weights for each phase across the m_firLen input taps.
  m_phaseTaps.assign(static_cast<size_t>(m_L) * m_firLen, 0.0);
  for (int64_t phase = 0; phase < m_L; ++phase) {
    double frac = static_cast<double>(phase) / static_cast<double>(m_L); // in [0,1)
    double* taps = &m_phaseTaps[static_cast<size_t>(phase) * m_firLen];
    double norm = 0.0;
    for (uint32_t k = 0; k < m_firLen; ++k) {
      // Tap k corresponds to input offset (k - m_firHalf) relative to floor(t).
      // Distance from the exact (fractional) sample position, in input samples:
      double dist = (static_cast<double>(k) - static_cast<double>(m_firHalf)) - frac;
      // Windowed sinc at 2*cutoff bandwidth.
      double x = 2.0 * cutoff * dist;
      double sinc = (std::abs(x) < 1e-12) ? 1.0 : std::sin(kPi * x) / (kPi * x);
      // Window argument mapped to [0,1] across the FIR span.
      double winPos = (dist + static_cast<double>(m_firHalf)) / static_cast<double>(m_firLen - 1);
      winPos = std::clamp(winPos, 0.0, 1.0);
      double w = blackman(winPos);
      double tap = 2.0 * cutoff * sinc * w;
      taps[k] = tap;
      norm += tap;
    }
    // Normalize to unity DC gain per phase (prevents level drift).
    if (norm > 1e-12) {
      for (uint32_t k = 0; k < m_firLen; ++k)
        taps[k] /= norm;
    }
  }

  reset();
}

void PolyphaseResampler::reset() {
  m_history.assign(static_cast<size_t>(m_firLen) * m_numChannels, 0.0f);
  m_inputPos = 0;
  m_outputCount = 0;
}

double PolyphaseResampler::sincKernel(double x) const {
  if (std::abs(x) < 1e-12)
    return 1.0;
  return std::sin(kPi * x) / (kPi * x);
}

size_t PolyphaseResampler::estimateOutputFrames(size_t inFrames) const {
  if (m_passthrough)
    return inFrames;
  // Output frames whose input center falls within the frames consumed so far.
  // n * M / L <= totalInput - firHalf  =>  n <= (totalInput - firHalf) * L / M
  long double totalInput =
      static_cast<long double>(m_inputPos) + static_cast<long double>(inFrames);
  long double maxCenter = totalInput - static_cast<long double>(m_firHalf);
  if (maxCenter < 0)
    return 0;
  long double maxN = maxCenter * static_cast<long double>(m_L) / static_cast<long double>(m_M);
  int64_t frames = static_cast<int64_t>(std::floor(maxN)) - m_outputCount;
  return frames > 0 ? static_cast<size_t>(frames) : 0;
}

size_t PolyphaseResampler::process(const float* input, size_t inFrames,
                                   std::vector<float>& output) {
  const uint16_t ch = m_numChannels;

  if (m_passthrough) {
    output.assign(input, input + inFrames * ch);
    m_inputPos += static_cast<int64_t>(inFrames);
    m_outputCount += static_cast<int64_t>(inFrames);
    return inFrames;
  }

  // Append new input to history so the FIR window can reach back across calls.
  // history holds the most recent (m_firLen-1) frames before this block, then
  // the block itself; we index globally via m_inputPos.
  const int64_t blockStart = m_inputPos;                                // global index of input[0]
  const int64_t blockEnd = blockStart + static_cast<int64_t>(inFrames); // one past last

  // Extend history buffer with this block (kept as a rolling tail).
  // We store the last (m_firLen) frames globally in m_history; but to convolve
  // across the block we need direct access to block samples too. Simplest exact
  // approach: build a contiguous view [historyTail | block] and index into it.
  std::vector<float> window;
  const size_t tail = static_cast<size_t>(m_firLen);
  window.resize((tail + inFrames) * ch);
  // copy history tail
  std::copy(m_history.begin(), m_history.end(), window.begin());
  // copy block
  std::copy(input, input + inFrames * ch, window.begin() + static_cast<long>(tail) * ch);
  // The global index of window frame 0 is (blockStart - m_firLen).
  const int64_t windowBase = blockStart - static_cast<int64_t>(m_firLen);

  output.clear();

  // Produce every output frame whose center lies within the fully-covered range.
  // Center of output n is inputPos = n*M/L. We can compute it once we know the
  // FIR reaches [center - firHalf, center + firHalf] within available samples.
  // Available samples: global indices [windowBase, blockEnd). We require the FIR
  // window [center - firHalf, center + firHalf] to be inside available data.
  int64_t n = m_outputCount;
  while (true) {
    // center (integer floor) and phase from n*M/L
    int64_t num = n * m_M;
    int64_t centerFloor = num / m_L;
    int64_t phase = num % m_L;
    if (phase < 0) { // guard (num >= 0 here, but keep robust)
      phase += m_L;
      centerFloor -= 1;
    }

    int64_t firFirst = centerFloor - static_cast<int64_t>(m_firHalf);
    int64_t firLast = centerFloor + static_cast<int64_t>(m_firHalf);
    // Need firLast < blockEnd (all taps available) and firFirst >= windowBase.
    if (firLast >= blockEnd)
      break; // not enough forward data yet — wait for the next block
    if (firFirst < windowBase) {
      // Startup transient before enough history exists: skip (zeros implied).
      ++n;
      continue;
    }

    const double* taps = &m_phaseTaps[static_cast<size_t>(phase) * m_firLen];
    // Convolve for each channel.
    for (uint16_t c = 0; c < ch; ++c) {
      double acc = 0.0;
      for (uint32_t k = 0; k < m_firLen; ++k) {
        int64_t gi = firFirst + static_cast<int64_t>(k); // global input frame index
        size_t wi = static_cast<size_t>(gi - windowBase);
        acc += taps[k] * static_cast<double>(window[wi * ch + c]);
      }
      output.push_back(static_cast<float>(acc));
    }
    ++n;
  }

  size_t produced = static_cast<size_t>(n - m_outputCount);
  m_outputCount = n;

  // Advance input position and retain the last (m_firLen) frames as history.
  m_inputPos = blockEnd;
  const size_t totalWindowFrames = tail + inFrames;
  const size_t keep = std::min<size_t>(tail, totalWindowFrames);
  m_history.assign(window.end() - static_cast<long>(keep) * ch, window.end());
  if (m_history.size() < static_cast<size_t>(m_firLen) * ch) {
    // pad front with zeros if we don't yet have a full tail
    std::vector<float> padded(static_cast<size_t>(m_firLen) * ch, 0.0f);
    std::copy(m_history.begin(), m_history.end(),
              padded.end() - static_cast<long>(m_history.size()));
    m_history.swap(padded);
  }

  return produced;
}

} // namespace orpheus
