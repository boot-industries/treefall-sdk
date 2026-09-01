// SPDX-License-Identifier: MIT
#pragma once

// ORP134 G6: unified offline analysis facade.
//
// One entry point for the analysis primitives downstream apps kept
// re-implementing (FreqFinder is the primary consumer; Clip Composer and
// FourTrack reuse the metering/waveform pieces):
//
//  * FFT / STFT           — new in the SDK (iterative radix-2, power-of-two)
//  * RMS / peak           — buffer statistics
//  * Integrated LUFS      — wraps the existing LoudnessMeter (K-weighted
//                           analysis modeled against BS.1770); the facade does
//                           not re-implement it
//  * Spectral centroid / rolloff — derived from the magnitude spectrum
//  * Onset detection      — spectral-flux peak picking over the STFT
//  * Waveform proxy       — min/max peaks for an IN-MEMORY buffer. For
//                           file-backed proxies keep using
//                           IAudioFileReaderExtended::getWaveformData()
//                           (waveform_processor.cpp) — NOT duplicated here.
//
// All functions are OFFLINE/background-thread utilities: they allocate and
// are NOT realtime-safe. Determinism: within one platform+toolchain results
// are bit-stable (fixed iteration order, no wall clock, no RNG); the window/
// twiddle tables use libm (std::sin/std::cos), so cross-platform bit
// identity follows the same policy as the transport fade math (ORP136 §2.3 —
// pinning libm is future work; drift is a bug to file, not mask).

#include <complex>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace orpheus::analysis {

/// Analysis window applied before the FFT.
enum class WindowType : uint8_t {
  Rectangular = 0,
  Hann = 1,
};

/// Single-frame magnitude spectrum.
struct Spectrum {
  std::vector<float> magnitudes; ///< fftSize/2 + 1 bins (DC .. Nyquist)
  float binHz = 0.0f;            ///< Frequency step between bins
  uint32_t sampleRate = 0;       ///< Rate the source samples were in
  size_t fftSize = 0;            ///< Transform length actually used (power of two)
};

/// Short-time Fourier transform result: one Spectrum-shaped magnitude row per
/// hop, plus the geometry needed to map rows back to sample positions.
struct StftResult {
  std::vector<std::vector<float>> frames; ///< [numFrames][fftSize/2+1]
  size_t frameSize = 0;                   ///< Analysis frame length (power of two)
  size_t hopSize = 0;                     ///< Samples advanced per row
  float binHz = 0.0f;
  uint32_t sampleRate = 0;
};

/// Min/max waveform proxy for an in-memory buffer (one entry per pixel).
struct WaveformPeaks {
  std::vector<float> minPeaks;
  std::vector<float> maxPeaks;
};

/// Smallest power of two >= value (>= 1).
size_t nextPowerOfTwo(size_t value);

/// In-place iterative radix-2 FFT. data.size() must be a power of two.
void fftInPlace(std::vector<std::complex<float>>& data);

/// Magnitude spectrum of a mono buffer. The input is windowed and zero-padded
/// up to the next power of two (or `fftSize` if nonzero — must be pow2 and
/// >= count).
Spectrum magnitudeSpectrum(const float* samples, size_t count, uint32_t sampleRate,
                           WindowType window = WindowType::Hann, size_t fftSize = 0);

/// STFT over a mono buffer. frameSize must be a power of two; hopSize >= 1.
StftResult stft(const float* samples, size_t count, uint32_t sampleRate, size_t frameSize = 1024,
                size_t hopSize = 512, WindowType window = WindowType::Hann);

/// Root-mean-square of a buffer (interleaved or mono — plain sample RMS).
float rms(const float* samples, size_t count);

/// Absolute peak of a buffer.
float peak(const float* samples, size_t count);

/// Integrated loudness (LUFS) of an interleaved buffer. Wraps the existing
/// LoudnessMeter (K-weighted); channels beyond the first two are ignored,
/// matching the meter's stereo model. Returns LoudnessMeter::kSilenceLufs
/// for empty/silent input.
float integratedLufs(const float* interleaved, size_t frames, uint16_t numChannels,
                     double sampleRate);

/// Amplitude-weighted mean frequency of a spectrum (Hz); 0 for silence.
float spectralCentroidHz(const Spectrum& spectrum);

/// Frequency below which `fraction` of the spectral energy lies (Hz).
float spectralRolloffHz(const Spectrum& spectrum, float fraction = 0.85f);

/// Spectral-flux onset detection over a mono buffer. Returns onset positions
/// in SAMPLES (frame-quantized to hopSize). `sensitivity` scales the adaptive
/// threshold: lower = more onsets (typical range 1.0 .. 3.0).
std::vector<int64_t> detectOnsets(const float* samples, size_t count, uint32_t sampleRate,
                                  size_t frameSize = 1024, size_t hopSize = 512,
                                  float sensitivity = 1.5f);

/// Min/max waveform proxy for an in-memory interleaved buffer (channel 0).
/// For FILE-backed proxies use IAudioFileReaderExtended::getWaveformData().
WaveformPeaks waveformPeaks(const float* interleaved, size_t frames, uint16_t numChannels,
                            uint32_t pixelWidth);

} // namespace orpheus::analysis
