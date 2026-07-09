// SPDX-License-Identifier: MIT
#include <orpheus/audio_analysis.h>
#include <orpheus/loudness_meter.h>

#include <algorithm>
#include <cmath>

namespace orpheus::analysis {

namespace {

constexpr float kPi = 3.14159265358979323846f;

void applyWindow(std::vector<std::complex<float>>& data, size_t count, WindowType window) {
  if (window != WindowType::Hann || count < 2) {
    return;
  }
  for (size_t i = 0; i < count; ++i) {
    const float w =
        0.5f *
        (1.0f - std::cos(2.0f * kPi * static_cast<float>(i) / static_cast<float>(count - 1)));
    data[i] *= w;
  }
}

std::vector<float> magnitudesFromComplex(const std::vector<std::complex<float>>& data) {
  const size_t bins = data.size() / 2 + 1;
  std::vector<float> magnitudes(bins, 0.0f);
  for (size_t i = 0; i < bins; ++i) {
    magnitudes[i] = std::abs(data[i]);
  }
  return magnitudes;
}

} // namespace

size_t nextPowerOfTwo(size_t value) {
  size_t result = 1;
  while (result < value) {
    result <<= 1;
  }
  return result;
}

void fftInPlace(std::vector<std::complex<float>>& data) {
  const size_t n = data.size();
  if (n < 2 || (n & (n - 1)) != 0) {
    return; // power-of-two only; callers guarantee this
  }

  // Bit-reversal permutation.
  for (size_t i = 1, j = 0; i < n; ++i) {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1) {
      j ^= bit;
    }
    j ^= bit;
    if (i < j) {
      std::swap(data[i], data[j]);
    }
  }

  // Iterative Danielson-Lanczos butterflies.
  for (size_t len = 2; len <= n; len <<= 1) {
    const float angle = -2.0f * kPi / static_cast<float>(len);
    const std::complex<float> wlen(std::cos(angle), std::sin(angle));
    for (size_t i = 0; i < n; i += len) {
      std::complex<float> w(1.0f, 0.0f);
      for (size_t k = 0; k < len / 2; ++k) {
        const std::complex<float> u = data[i + k];
        const std::complex<float> v = data[i + k + len / 2] * w;
        data[i + k] = u + v;
        data[i + k + len / 2] = u - v;
        w *= wlen;
      }
    }
  }
}

Spectrum magnitudeSpectrum(const float* samples, size_t count, uint32_t sampleRate,
                           WindowType window, size_t fftSize) {
  Spectrum result;
  result.sampleRate = sampleRate;
  if (samples == nullptr || count == 0 || sampleRate == 0) {
    return result;
  }

  size_t n = fftSize != 0 ? fftSize : nextPowerOfTwo(count);
  n = std::max<size_t>(n, 2);
  if ((n & (n - 1)) != 0 || n < count) {
    n = nextPowerOfTwo(std::max(n, count));
  }

  std::vector<std::complex<float>> data(n, std::complex<float>(0.0f, 0.0f));
  for (size_t i = 0; i < count; ++i) {
    data[i] = std::complex<float>(samples[i], 0.0f);
  }
  applyWindow(data, count, window);
  fftInPlace(data);

  result.magnitudes = magnitudesFromComplex(data);
  result.fftSize = n;
  result.binHz = static_cast<float>(sampleRate) / static_cast<float>(n);
  return result;
}

StftResult stft(const float* samples, size_t count, uint32_t sampleRate, size_t frameSize,
                size_t hopSize, WindowType window) {
  StftResult result;
  result.sampleRate = sampleRate;
  if (samples == nullptr || count == 0 || sampleRate == 0 || frameSize < 2 || hopSize == 0) {
    return result;
  }
  if ((frameSize & (frameSize - 1)) != 0) {
    frameSize = nextPowerOfTwo(frameSize);
  }
  result.frameSize = frameSize;
  result.hopSize = hopSize;
  result.binHz = static_cast<float>(sampleRate) / static_cast<float>(frameSize);

  std::vector<std::complex<float>> data(frameSize);
  for (size_t start = 0; start + 1 <= count; start += hopSize) {
    const size_t remaining = count - start;
    const size_t take = std::min(frameSize, remaining);
    for (size_t i = 0; i < take; ++i) {
      data[i] = std::complex<float>(samples[start + i], 0.0f);
    }
    for (size_t i = take; i < frameSize; ++i) {
      data[i] = std::complex<float>(0.0f, 0.0f);
    }
    applyWindow(data, frameSize, window);
    fftInPlace(data);
    result.frames.push_back(magnitudesFromComplex(data));
    if (remaining <= frameSize) {
      break; // last (possibly zero-padded) frame emitted
    }
  }
  return result;
}

float rms(const float* samples, size_t count) {
  if (samples == nullptr || count == 0) {
    return 0.0f;
  }
  double sum = 0.0;
  for (size_t i = 0; i < count; ++i) {
    sum += static_cast<double>(samples[i]) * static_cast<double>(samples[i]);
  }
  return static_cast<float>(std::sqrt(sum / static_cast<double>(count)));
}

float peak(const float* samples, size_t count) {
  float maxAbs = 0.0f;
  for (size_t i = 0; i < count; ++i) {
    maxAbs = std::max(maxAbs, std::abs(samples[i]));
  }
  return maxAbs;
}

float integratedLufs(const float* interleaved, size_t frames, uint16_t numChannels,
                     double sampleRate) {
  if (interleaved == nullptr || frames == 0 || numChannels == 0) {
    return LoudnessMeter::kSilenceLufs;
  }

  // De-interleave into the meter's mono/stereo model (extra channels ignored,
  // matching LoudnessMeter's two-filter design).
  std::vector<float> left(frames, 0.0f);
  std::vector<float> right(frames, 0.0f);
  const bool stereo = numChannels >= 2;
  for (size_t i = 0; i < frames; ++i) {
    left[i] = interleaved[i * numChannels];
    right[i] = stereo ? interleaved[i * numChannels + 1] : 0.0f;
  }

  LoudnessMeter meter(sampleRate);
  meter.processBuffer(left.data(), stereo ? right.data() : nullptr, frames);
  return meter.integratedLufs();
}

float spectralCentroidHz(const Spectrum& spectrum) {
  double weighted = 0.0;
  double total = 0.0;
  for (size_t i = 0; i < spectrum.magnitudes.size(); ++i) {
    const double magnitude = spectrum.magnitudes[i];
    weighted += magnitude * static_cast<double>(i) * spectrum.binHz;
    total += magnitude;
  }
  return total > 0.0 ? static_cast<float>(weighted / total) : 0.0f;
}

float spectralRolloffHz(const Spectrum& spectrum, float fraction) {
  fraction = std::clamp(fraction, 0.0f, 1.0f);
  double total = 0.0;
  for (float magnitude : spectrum.magnitudes) {
    total += static_cast<double>(magnitude) * magnitude;
  }
  if (total <= 0.0) {
    return 0.0f;
  }
  const double target = total * fraction;
  double running = 0.0;
  for (size_t i = 0; i < spectrum.magnitudes.size(); ++i) {
    running += static_cast<double>(spectrum.magnitudes[i]) * spectrum.magnitudes[i];
    if (running >= target) {
      return static_cast<float>(i) * spectrum.binHz;
    }
  }
  return static_cast<float>(spectrum.magnitudes.size() - 1) * spectrum.binHz;
}

std::vector<int64_t> detectOnsets(const float* samples, size_t count, uint32_t sampleRate,
                                  size_t frameSize, size_t hopSize, float sensitivity) {
  std::vector<int64_t> onsets;
  const StftResult analysis = stft(samples, count, sampleRate, frameSize, hopSize);
  if (analysis.frames.size() < 3) {
    return onsets;
  }

  // Spectral flux: positive magnitude increases per frame.
  std::vector<double> flux(analysis.frames.size(), 0.0);
  for (size_t frame = 1; frame < analysis.frames.size(); ++frame) {
    double sum = 0.0;
    const auto& current = analysis.frames[frame];
    const auto& previous = analysis.frames[frame - 1];
    for (size_t bin = 0; bin < current.size(); ++bin) {
      const double diff = static_cast<double>(current[bin]) - previous[bin];
      if (diff > 0.0) {
        sum += diff;
      }
    }
    flux[frame] = sum;
  }

  const double mean = [&]() {
    double sum = 0.0;
    for (double f : flux) {
      sum += f;
    }
    return sum / static_cast<double>(flux.size());
  }();
  const double threshold = mean * static_cast<double>(std::max(0.1f, sensitivity));

  // Local-maximum peak picking above the adaptive threshold.
  for (size_t frame = 1; frame + 1 < flux.size(); ++frame) {
    if (flux[frame] > threshold && flux[frame] > flux[frame - 1] &&
        flux[frame] >= flux[frame + 1]) {
      onsets.push_back(static_cast<int64_t>(frame * analysis.hopSize));
    }
  }
  return onsets;
}

WaveformPeaks waveformPeaks(const float* interleaved, size_t frames, uint16_t numChannels,
                            uint32_t pixelWidth) {
  WaveformPeaks result;
  if (interleaved == nullptr || frames == 0 || numChannels == 0 || pixelWidth == 0) {
    return result;
  }
  result.minPeaks.assign(pixelWidth, 0.0f);
  result.maxPeaks.assign(pixelWidth, 0.0f);

  const double framesPerPixel = static_cast<double>(frames) / static_cast<double>(pixelWidth);
  for (uint32_t pixel = 0; pixel < pixelWidth; ++pixel) {
    const size_t begin = static_cast<size_t>(static_cast<double>(pixel) * framesPerPixel);
    size_t end = static_cast<size_t>(static_cast<double>(pixel + 1) * framesPerPixel);
    end = std::min(std::max(end, begin + 1), frames);
    float lo = interleaved[begin * numChannels];
    float hi = lo;
    for (size_t frame = begin; frame < end; ++frame) {
      const float sample = interleaved[frame * numChannels];
      lo = std::min(lo, sample);
      hi = std::max(hi, sample);
    }
    result.minPeaks[pixel] = lo;
    result.maxPeaks[pixel] = hi;
  }
  return result;
}

} // namespace orpheus::analysis
