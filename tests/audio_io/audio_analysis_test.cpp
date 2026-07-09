// SPDX-License-Identifier: MIT
// ORP134 G6: analysis facade tests — FFT/STFT correctness on known signals,
// LUFS parity with the wrapped LoudnessMeter (facade must not diverge from
// the primitive it wraps), spectral features, onsets, and waveform proxy.

#include <orpheus/audio_analysis.h>
#include <orpheus/loudness_meter.h>

#include "../support/synth.hpp"

#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

using namespace orpheus;
using namespace orpheus::analysis;
using orpheus::tests::support::GenerateSine;

namespace {
constexpr uint32_t kRate = 48000;
}

TEST(AnalysisFftTest, SinePeaksAtItsFrequencyBin) {
  // 750 Hz = bin 64 exactly at fftSize 4096 @ 48k (48000/4096 = 11.71875 Hz;
  // 750 / 11.71875 = 64) — no spectral leakage even rectangular.
  auto sine = GenerateSine(4096, kRate, 750, 0.5f);
  Spectrum spectrum =
      magnitudeSpectrum(sine.data(), sine.size(), kRate, WindowType::Rectangular, 4096);

  ASSERT_EQ(spectrum.fftSize, 4096u);
  ASSERT_EQ(spectrum.magnitudes.size(), 2049u);
  size_t peakBin = 0;
  for (size_t i = 1; i < spectrum.magnitudes.size(); ++i) {
    if (spectrum.magnitudes[i] > spectrum.magnitudes[peakBin]) {
      peakBin = i;
    }
  }
  EXPECT_EQ(peakBin, 64u);
  EXPECT_NEAR(static_cast<float>(peakBin) * spectrum.binHz, 750.0f, spectrum.binHz);
}

TEST(AnalysisFftTest, ZeroPadsNonPowerOfTwoInput) {
  auto sine = GenerateSine(3000, kRate, 750, 0.5f);
  Spectrum spectrum = magnitudeSpectrum(sine.data(), sine.size(), kRate);
  EXPECT_EQ(spectrum.fftSize, 4096u); // next power of two
  EXPECT_GT(spectrum.magnitudes[64], 0.0f);
}

TEST(AnalysisFftTest, DcSignalConcentratesInBinZero) {
  std::vector<float> dc(1024, 0.5f);
  Spectrum spectrum = magnitudeSpectrum(dc.data(), dc.size(), kRate, WindowType::Rectangular, 1024);
  size_t peakBin = 1;
  for (size_t i = 1; i < spectrum.magnitudes.size(); ++i) {
    if (spectrum.magnitudes[i] > spectrum.magnitudes[peakBin]) {
      peakBin = i;
    }
  }
  EXPECT_GT(spectrum.magnitudes[0], spectrum.magnitudes[peakBin] * 100.0f);
}

TEST(AnalysisStftTest, FrameGeometryAndContent) {
  auto sine = GenerateSine(48000, kRate, 750, 0.5f);
  StftResult result = stft(sine.data(), sine.size(), kRate, 1024, 512);

  EXPECT_EQ(result.frameSize, 1024u);
  EXPECT_EQ(result.hopSize, 512u);
  EXPECT_GT(result.frames.size(), 90u); // ~ (48000-1024)/512 frames
  // Every steady-state frame peaks at the 750 Hz bin (bin 16 at 1024).
  const size_t expectedBin = 16;
  const auto& mid = result.frames[result.frames.size() / 2];
  size_t peakBin = 0;
  for (size_t i = 1; i < mid.size(); ++i) {
    if (mid[i] > mid[peakBin]) {
      peakBin = i;
    }
  }
  EXPECT_EQ(peakBin, expectedBin);
}

TEST(AnalysisStatsTest, RmsAndPeakOfKnownSignals) {
  std::vector<float> dc(1000, 0.5f);
  EXPECT_FLOAT_EQ(rms(dc.data(), dc.size()), 0.5f);
  EXPECT_FLOAT_EQ(peak(dc.data(), dc.size()), 0.5f);

  auto sine = GenerateSine(48000, kRate, 1000, 0.5f);
  EXPECT_NEAR(rms(sine.data(), sine.size()), 0.5f / std::sqrt(2.0f), 0.005f);
  EXPECT_NEAR(peak(sine.data(), sine.size()), 0.5f, 0.01f);

  EXPECT_FLOAT_EQ(rms(nullptr, 0), 0.0f);
}

TEST(AnalysisLufsTest, FacadeMatchesWrappedLoudnessMeter) {
  // The facade must WRAP LoudnessMeter, not re-implement it: identical input
  // must produce the identical integrated LUFS.
  auto left = GenerateSine(96000, kRate, 440, 0.25f);
  auto right = GenerateSine(96000, kRate, 554, 0.25f);
  std::vector<float> interleaved(left.size() * 2);
  for (size_t i = 0; i < left.size(); ++i) {
    interleaved[i * 2] = left[i];
    interleaved[i * 2 + 1] = right[i];
  }

  LoudnessMeter reference(kRate);
  reference.processBuffer(left.data(), right.data(), left.size());

  const float facade = integratedLufs(interleaved.data(), left.size(), 2, kRate);
  EXPECT_FLOAT_EQ(facade, reference.integratedLufs());
  EXPECT_LT(facade, 0.0f);
  EXPECT_GT(facade, -40.0f);

  EXPECT_FLOAT_EQ(integratedLufs(nullptr, 0, 2, kRate), LoudnessMeter::kSilenceLufs);
}

TEST(AnalysisSpectralTest, CentroidTracksFrequencyAndRolloffBoundsIt) {
  auto low = GenerateSine(8192, kRate, 200, 0.5f);
  auto high = GenerateSine(8192, kRate, 4000, 0.5f);

  Spectrum lowSpec = magnitudeSpectrum(low.data(), low.size(), kRate);
  Spectrum highSpec = magnitudeSpectrum(high.data(), high.size(), kRate);

  const float lowCentroid = spectralCentroidHz(lowSpec);
  const float highCentroid = spectralCentroidHz(highSpec);
  EXPECT_LT(lowCentroid, highCentroid);
  EXPECT_NEAR(highCentroid, 4000.0f, 800.0f); // leakage skews slightly

  // Rolloff of a pure tone sits at/just above the tone.
  const float rolloff = spectralRolloffHz(highSpec, 0.85f);
  EXPECT_NEAR(rolloff, 4000.0f, 200.0f);

  EXPECT_FLOAT_EQ(spectralCentroidHz(Spectrum{}), 0.0f);
}

TEST(AnalysisOnsetTest, DetectsToneBurstsNearTheirStarts) {
  // Silence, then a burst at 1.0s, silence, burst at 2.0s.
  std::vector<float> signal(3 * kRate, 0.0f);
  auto burst = GenerateSine(kRate / 4, kRate, 880, 0.8f);
  std::copy(burst.begin(), burst.end(), signal.begin() + 1 * kRate);
  std::copy(burst.begin(), burst.end(), signal.begin() + 2 * kRate);

  auto onsets = detectOnsets(signal.data(), signal.size(), kRate);
  ASSERT_GE(onsets.size(), 2u);

  auto nearest = [&](int64_t target) {
    int64_t best = onsets.front();
    for (int64_t onset : onsets) {
      if (std::llabs(onset - target) < std::llabs(best - target)) {
        best = onset;
      }
    }
    return best;
  };
  EXPECT_LT(std::llabs(nearest(1 * kRate) - 1 * kRate), 2048);
  EXPECT_LT(std::llabs(nearest(2 * kRate) - 2 * kRate), 2048);
}

TEST(AnalysisWaveformTest, PeaksProxyBoundsTheSignal) {
  auto sine = GenerateSine(48000, kRate, 100, 0.5f);
  WaveformPeaks proxy = waveformPeaks(sine.data(), sine.size(), 1, 100);

  ASSERT_EQ(proxy.minPeaks.size(), 100u);
  ASSERT_EQ(proxy.maxPeaks.size(), 100u);
  for (size_t i = 0; i < 100; ++i) {
    EXPECT_LE(proxy.minPeaks[i], proxy.maxPeaks[i]);
    EXPECT_GE(proxy.minPeaks[i], -0.51f);
    EXPECT_LE(proxy.maxPeaks[i], 0.51f);
  }
  // 100 Hz over 1s = 100 cycles; ~1 cycle per pixel → every pixel spans
  // nearly the full amplitude.
  EXPECT_LT(proxy.minPeaks[50], -0.4f);
  EXPECT_GT(proxy.maxPeaks[50], 0.4f);
}
