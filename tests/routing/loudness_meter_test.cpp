// SPDX-License-Identifier: MIT
#include <orpheus/loudness_meter.h>

#include <cmath>
#include <vector>

#include <gtest/gtest.h>

namespace {

constexpr double kSampleRate = 48000.0;
constexpr std::size_t kBlockSize = 480;
constexpr double kFrequencyHz = 440.0;

std::vector<float> makeSineBlock(float amplitude, double& phase) {
  std::vector<float> block(kBlockSize, 0.0f);
  const double phaseIncrement = 2.0 * 3.14159265358979323846 * kFrequencyHz / kSampleRate;

  for (std::size_t index = 0; index < block.size(); ++index) {
    block[index] = amplitude * static_cast<float>(std::sin(phase));
    phase += phaseIncrement;
  }

  return block;
}

void processSeconds(orpheus::LoudnessMeter& meter, float amplitude, double seconds) {
  double phase = 0.0;
  const int totalBlocks =
      static_cast<int>(std::llround((seconds * kSampleRate) / static_cast<double>(kBlockSize)));

  for (int block = 0; block < totalBlocks; ++block) {
    auto left = makeSineBlock(amplitude, phase);
    meter.processBuffer(left.data(), left.data(), left.size());
  }
}

} // namespace

TEST(LoudnessMeterTest, ResetClearsShortTermAndIntegratedHistory) {
  orpheus::LoudnessMeter meter(kSampleRate);
  processSeconds(meter, 0.7f, 4.0);

  EXPECT_GT(meter.shortTermLufs(), orpheus::LoudnessMeter::kSilenceLufs);
  EXPECT_GT(meter.integratedLufs(), orpheus::LoudnessMeter::kSilenceLufs);

  meter.reset();

  EXPECT_FLOAT_EQ(meter.shortTermLufs(), orpheus::LoudnessMeter::kSilenceLufs);
  EXPECT_FLOAT_EQ(meter.integratedLufs(), orpheus::LoudnessMeter::kSilenceLufs);
}

TEST(LoudnessMeterTest, LouderInputProducesHigherShortTermLoudness) {
  orpheus::LoudnessMeter quietMeter(kSampleRate);
  orpheus::LoudnessMeter loudMeter(kSampleRate);

  processSeconds(quietMeter, 0.15f, 4.0);
  processSeconds(loudMeter, 0.65f, 4.0);

  EXPECT_GT(loudMeter.shortTermLufs(), quietMeter.shortTermLufs());
  EXPECT_GT(loudMeter.integratedLufs(), quietMeter.integratedLufs());
}

TEST(LoudnessMeterTest, SustainedSignalPopulatesIntegratedAndShortTermLoudness) {
  orpheus::LoudnessMeter meter(kSampleRate);
  processSeconds(meter, 0.5f, 4.0);

  EXPECT_GT(meter.shortTermLufs(), -80.0f);
  EXPECT_GT(meter.integratedLufs(), -80.0f);
  EXPECT_LE(std::abs(meter.shortTermLufs() - meter.integratedLufs()), 6.0f);
}
