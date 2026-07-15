// SPDX-License-Identifier: MIT
//
// ORP127 T6 (G4) — Clip gain smoothing (anti-zipper).
//
// Set-Gain previously wrote clipGainLinear with no ramp, so a fader drag applied
// as a step on the next buffer boundary — zipper noise. Now each voice ramps its
// gain toward the target over ~5ms. This test plays a constant full-scale source,
// applies an abrupt gain change, and asserts the output has no large single-
// sample jump (a step would jump by |Δgain|*steady in one sample).

#include "../../src/core/transport/transport_controller.h"
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <orpheus/audio_file_reader.h>
#include <orpheus/transport_controller.h>
#include <string>
#include <vector>

using namespace orpheus;

namespace {

std::string writeConstantWav(const std::filesystem::path& path, float durationSeconds,
                             uint32_t sampleRate = 48000) {
  const uint16_t numChannels = 2;
  const int64_t numFrames = static_cast<int64_t>(durationSeconds * sampleRate);
  const int16_t kConst = 30000;

  std::ofstream file(path, std::ios::binary);
  const uint32_t dataSize = static_cast<uint32_t>(numFrames * numChannels * sizeof(int16_t));
  const uint32_t fileSize = 36 + dataSize;
  const uint32_t fmtSize = 16;
  const uint16_t audioFormat = 1;
  const uint16_t blockAlign = numChannels * 2;
  const uint32_t byteRate = sampleRate * blockAlign;
  const uint16_t bitsPerSample = 16;

  file.write("RIFF", 4);
  file.write(reinterpret_cast<const char*>(&fileSize), 4);
  file.write("WAVE", 4);
  file.write("fmt ", 4);
  file.write(reinterpret_cast<const char*>(&fmtSize), 4);
  file.write(reinterpret_cast<const char*>(&audioFormat), 2);
  file.write(reinterpret_cast<const char*>(&numChannels), 2);
  file.write(reinterpret_cast<const char*>(&sampleRate), 4);
  file.write(reinterpret_cast<const char*>(&byteRate), 4);
  file.write(reinterpret_cast<const char*>(&blockAlign), 2);
  file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
  file.write("data", 4);
  file.write(reinterpret_cast<const char*>(&dataSize), 4);
  for (int64_t i = 0; i < numFrames; ++i) {
    file.write(reinterpret_cast<const char*>(&kConst), 2);
    file.write(reinterpret_cast<const char*>(&kConst), 2);
  }
  file.close();
  return path.string();
}

} // namespace

class ClipGainSmoothingTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_transport = std::make_unique<TransportController>(nullptr, TransportConfig{.sampleRate = static_cast<uint32_t>(kSampleRate)});
    m_path = (std::filesystem::temp_directory_path() / "orp127_gain_smooth.wav").string();
    writeConstantWav(m_path, 2.0f, kSampleRate);
  }
  void TearDown() override {
    m_transport.reset();
    std::error_code ec;
    std::filesystem::remove(m_path, ec);
  }

  static constexpr uint32_t kSampleRate = 48000;
  static constexpr size_t kBuffer = 512;
  std::unique_ptr<TransportController> m_transport;
  std::string m_path;
};

// A big abrupt gain drop must arrive as a short ramp, not a single-sample step.
TEST_F(ClipGainSmoothingTest, AbruptGainChangeRampsWithoutZipper) {
  ClipHandle handle = 1;
  ASSERT_EQ(m_transport->registerClipAudio(handle, m_path.c_str()), SessionGraphError::OK);

  std::vector<float> left(kBuffer, 0.0f);
  std::vector<float> right(kBuffer, 0.0f);
  float* buffers[2] = {left.data(), right.data()};

  // Start at unity and settle.
  m_transport->startClip(handle);
  m_transport->processAudio(buffers, 2, kBuffer);
  float steady = std::fabs(left[100]);
  ASSERT_GT(steady, 0.01f);

  // Drop gain hard: 0 dB -> -20 dB (linear ~0.1). A step would jump ~0.9*steady
  // in a single sample.
  ASSERT_EQ(m_transport->updateClipGain(handle, -20.0f), SessionGraphError::OK);

  // Capture the transition. 5ms ramp = 240 samples, so 1-2 buffers cover it.
  std::vector<float> env;
  for (int b = 0; b < 3; ++b) {
    m_transport->processAudio(buffers, 2, kBuffer);
    for (size_t i = 0; i < kBuffer; ++i)
      env.push_back(std::fabs(left[i]));
  }

  float maxJump = 0.0f;
  for (size_t i = 1; i < env.size(); ++i) {
    maxJump = std::max(maxJump, std::fabs(env[i] - env[i - 1]));
  }

  // With a 5ms ramp the per-sample change is ~0.9*steady/240 ≈ 0.004*steady.
  // Require the worst single-sample jump to be well under 10% of full scale.
  EXPECT_LT(maxJump, steady * 0.1f)
      << "Single-sample gain jump (" << maxJump << ") indicates a zipper step; steady = " << steady;

  // And the gain must actually reach the new target (~0.1 * steady).
  EXPECT_NEAR(env.back(), steady * 0.1f, steady * 0.03f) << "Gain did not settle at -20 dB target";
}

// Rapid successive gain changes (automation) must stay smooth.
TEST_F(ClipGainSmoothingTest, RapidGainAutomationStaysSmooth) {
  ClipHandle handle = 1;
  ASSERT_EQ(m_transport->registerClipAudio(handle, m_path.c_str()), SessionGraphError::OK);

  std::vector<float> left(kBuffer, 0.0f);
  std::vector<float> right(kBuffer, 0.0f);
  float* buffers[2] = {left.data(), right.data()};

  m_transport->startClip(handle);
  m_transport->processAudio(buffers, 2, kBuffer);
  float steady = std::fabs(left[100]);
  ASSERT_GT(steady, 0.01f);

  // Sweep gain across a range, one change per buffer, capturing output.
  const float gains[] = {-6.0f, -12.0f, -3.0f, -18.0f, 0.0f, -9.0f};
  std::vector<float> env;
  for (float g : gains) {
    ASSERT_EQ(m_transport->updateClipGain(handle, g), SessionGraphError::OK);
    m_transport->processAudio(buffers, 2, kBuffer);
    for (size_t i = 0; i < kBuffer; ++i)
      env.push_back(std::fabs(left[i]));
  }

  // No single-sample jump should exceed 10% of full scale anywhere in the sweep.
  float maxJump = 0.0f;
  for (size_t i = 1; i < env.size(); ++i) {
    maxJump = std::max(maxJump, std::fabs(env[i] - env[i - 1]));
  }
  EXPECT_LT(maxJump, steady * 0.1f)
      << "Zipper detected during rapid automation: max jump " << maxJump;
}
