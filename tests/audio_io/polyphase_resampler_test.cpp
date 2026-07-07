// SPDX-License-Identifier: MIT
//
// ORP127 T8 (G6) — Polyphase sample-rate converter.
//
// Verifies pitch correctness (a sine keeps its frequency across a rate change),
// pass-through behavior for equal rates, and determinism (bit-identical output
// for identical input). Frequency is measured by zero-crossing counting — no
// FFT dependency, fully deterministic.

#include <orpheus/polyphase_resampler.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

using namespace orpheus;

namespace {

std::vector<float> makeSine(uint32_t rate, double freq, double seconds, uint16_t ch = 1) {
  const int64_t n = static_cast<int64_t>(seconds * rate);
  std::vector<float> out(static_cast<size_t>(n) * ch);
  for (int64_t i = 0; i < n; ++i) {
    float s = static_cast<float>(
        std::sin(2.0 * M_PI * freq * static_cast<double>(i) / static_cast<double>(rate)));
    for (uint16_t c = 0; c < ch; ++c)
      out[static_cast<size_t>(i) * ch + c] = s;
  }
  return out;
}

// Measure fundamental frequency via zero-crossing rate on channel 0. Skips a
// startup guard region to avoid the FIR transient.
double measureFreq(const std::vector<float>& interleaved, uint16_t ch, uint32_t rate,
                   size_t guardFrames) {
  const size_t frames = interleaved.size() / ch;
  if (frames <= guardFrames + 2)
    return 0.0;
  int crossings = 0;
  size_t firstCross = 0, lastCross = 0;
  bool haveFirst = false;
  for (size_t i = guardFrames + 1; i < frames; ++i) {
    float prev = interleaved[(i - 1) * ch];
    float cur = interleaved[i * ch];
    if (prev < 0.0f && cur >= 0.0f) { // rising zero crossing
      if (!haveFirst) {
        firstCross = i;
        haveFirst = true;
      }
      lastCross = i;
      ++crossings;
    }
  }
  if (crossings < 2)
    return 0.0;
  // (crossings - 1) full periods span (lastCross - firstCross) frames.
  double periods = static_cast<double>(crossings - 1);
  double spanFrames = static_cast<double>(lastCross - firstCross);
  return periods * static_cast<double>(rate) / spanFrames;
}

} // namespace

// 44.1 kHz sine in a 48 kHz engine must keep its frequency (correct pitch).
TEST(PolyphaseResamplerTest, UpsampleKeepsFrequency) {
  const uint32_t inRate = 44100, outRate = 48000;
  const double freq = 1000.0;
  auto in = makeSine(inRate, freq, 1.0, 1);

  PolyphaseResampler rs(inRate, outRate, 1);
  std::vector<float> out, chunk;
  // Feed in blocks to exercise streaming.
  const size_t block = 1024;
  for (size_t i = 0; i < in.size(); i += block) {
    size_t n = std::min(block, in.size() - i);
    rs.process(in.data() + i, n, chunk);
    out.insert(out.end(), chunk.begin(), chunk.end());
  }

  double measured = measureFreq(out, 1, outRate, /*guard*/ 2000);
  EXPECT_NEAR(measured, freq, 2.0) << "Upsampled sine should keep 1000 Hz, got " << measured;

  // Output length should be ~ inFrames * outRate/inRate.
  double expectedFrames = static_cast<double>(in.size()) * outRate / inRate;
  EXPECT_NEAR(static_cast<double>(out.size()), expectedFrames, block * 2);
}

// 48 kHz sine downsampled to 44.1 kHz must keep its frequency.
TEST(PolyphaseResamplerTest, DownsampleKeepsFrequency) {
  const uint32_t inRate = 48000, outRate = 44100;
  const double freq = 1000.0;
  auto in = makeSine(inRate, freq, 1.0, 1);

  PolyphaseResampler rs(inRate, outRate, 1);
  std::vector<float> out, chunk;
  const size_t block = 1024;
  for (size_t i = 0; i < in.size(); i += block) {
    size_t n = std::min(block, in.size() - i);
    rs.process(in.data() + i, n, chunk);
    out.insert(out.end(), chunk.begin(), chunk.end());
  }

  double measured = measureFreq(out, 1, outRate, 2000);
  EXPECT_NEAR(measured, freq, 2.0) << "Downsampled sine should keep 1000 Hz, got " << measured;
}

// Equal rates => exact pass-through.
TEST(PolyphaseResamplerTest, EqualRatesArePassthrough) {
  PolyphaseResampler rs(48000, 48000, 2);
  EXPECT_TRUE(rs.isPassthrough());

  std::vector<float> in = {0.1f, -0.1f, 0.2f, -0.2f, 0.3f, -0.3f};
  std::vector<float> out;
  size_t frames = rs.process(in.data(), 3, out);
  EXPECT_EQ(frames, 3u);
  ASSERT_EQ(out.size(), in.size());
  for (size_t i = 0; i < in.size(); ++i)
    EXPECT_FLOAT_EQ(out[i], in[i]);
}

// Determinism: identical input -> bit-identical output across two instances.
TEST(PolyphaseResamplerTest, DeterministicOutput) {
  const uint32_t inRate = 44100, outRate = 48000;
  auto in = makeSine(inRate, 440.0, 0.5, 2);

  auto run = [&]() {
    PolyphaseResampler rs(inRate, outRate, 2);
    std::vector<float> out, chunk;
    const size_t block = 777; // odd block size to stress phase continuity
    for (size_t i = 0; i < in.size() / 2; i += block) {
      size_t n = std::min(block, in.size() / 2 - i);
      rs.process(in.data() + i * 2, n, chunk);
      out.insert(out.end(), chunk.begin(), chunk.end());
    }
    return out;
  };

  std::vector<float> a = run();
  std::vector<float> b = run();
  ASSERT_EQ(a.size(), b.size());
  for (size_t i = 0; i < a.size(); ++i) {
    ASSERT_EQ(std::bit_cast<uint32_t>(a[i]), std::bit_cast<uint32_t>(b[i]))
        << "Non-deterministic output at index " << i;
  }
}

// Stereo channels stay independent (a hard-panned signal doesn't bleed).
TEST(PolyphaseResamplerTest, StereoChannelsIndependent) {
  const uint32_t inRate = 44100, outRate = 48000;
  const int64_t n = inRate / 2;
  std::vector<float> in(static_cast<size_t>(n) * 2);
  for (int64_t i = 0; i < n; ++i) {
    in[static_cast<size_t>(i) * 2 + 0] =
        static_cast<float>(std::sin(2.0 * M_PI * 500.0 * i / inRate)); // L: 500 Hz
    in[static_cast<size_t>(i) * 2 + 1] = 0.0f;                         // R: silent
  }

  PolyphaseResampler rs(inRate, outRate, 2);
  std::vector<float> out;
  rs.process(in.data(), static_cast<size_t>(n), out);

  // R channel must remain (near) silent.
  double rEnergy = 0.0;
  const size_t frames = out.size() / 2;
  for (size_t i = 0; i < frames; ++i)
    rEnergy += std::abs(static_cast<double>(out[i * 2 + 1]));
  EXPECT_LT(rEnergy / std::max<size_t>(1, frames), 1e-3)
      << "Silent right channel leaked energy from the left";
}
