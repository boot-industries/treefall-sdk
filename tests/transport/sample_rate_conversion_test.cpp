// SPDX-License-Identifier: MIT
//
// ORP127 T8 (G6) — End-to-end sample-rate conversion in the transport.
//
// Loads a 44.1 kHz sine into a 48 kHz transport and verifies the rendered
// output keeps its frequency (correct pitch, no aliasing artifact big enough to
// shift the fundamental), and that the clip's reported duration is expressed in
// engine-rate samples so trims stay sample-accurate.

#include "../../src/core/transport/transport_controller.h"
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <orpheus/transport_controller.h>
#include <string>
#include <vector>

// MSVC's <cmath> does not define M_PI without _USE_MATH_DEFINES; guard it so
// the Windows build resolves the constant. Matches the codebase's M_PI_2 guard.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace orpheus;

namespace {

std::string writeSineWav(const std::filesystem::path& path, uint32_t sampleRate, double freq,
                         double seconds) {
  const uint16_t numChannels = 2;
  const int64_t numFrames = static_cast<int64_t>(seconds * sampleRate);
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
    float s = 0.5f * static_cast<float>(std::sin(2.0 * M_PI * freq * static_cast<double>(i) /
                                                 static_cast<double>(sampleRate)));
    int16_t v = static_cast<int16_t>(s * 32767.0f);
    file.write(reinterpret_cast<const char*>(&v), 2);
    file.write(reinterpret_cast<const char*>(&v), 2);
  }
  file.close();
  return path.string();
}

double measureFreqFromChannel(const std::vector<float>& mono, uint32_t rate, size_t guard) {
  int crossings = 0;
  size_t first = 0, last = 0;
  bool have = false;
  for (size_t i = guard + 1; i < mono.size(); ++i) {
    if (mono[i - 1] < 0.0f && mono[i] >= 0.0f) {
      if (!have) {
        first = i;
        have = true;
      }
      last = i;
      ++crossings;
    }
  }
  if (crossings < 2)
    return 0.0;
  return static_cast<double>(crossings - 1) * rate / static_cast<double>(last - first);
}

} // namespace

class SampleRateConversionTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_dir = std::filesystem::temp_directory_path() / "orp127_src";
    std::filesystem::create_directories(m_dir);
    m_transport = std::make_unique<TransportController>(nullptr, kEngineRate);
  }
  void TearDown() override {
    m_transport.reset();
    std::error_code ec;
    std::filesystem::remove_all(m_dir, ec);
  }

  static constexpr uint32_t kEngineRate = 48000;
  static constexpr size_t kBuffer = 512;
  std::filesystem::path m_dir;
  std::unique_ptr<TransportController> m_transport;
};

TEST_F(SampleRateConversionTest, MismatchedFilePlaysAtCorrectPitch) {
  // 1 kHz sine sampled at 44.1 kHz, played by a 48 kHz engine.
  const double freq = 1000.0;
  std::string path = writeSineWav(m_dir / "src.wav", 44100, freq, 1.5);

  ClipHandle h = 1;
  ASSERT_EQ(m_transport->registerClipAudio(h, path.c_str()), SessionGraphError::OK);

  // Duration must be reported in ENGINE-rate samples (~1.5s * 48000).
  auto meta = m_transport->getClipMetadata(h);
  ASSERT_TRUE(meta.has_value());
  double expectedEngineFrames = 1.5 * kEngineRate;
  EXPECT_NEAR(static_cast<double>(meta->trimOutSamples), expectedEngineFrames, kEngineRate * 0.02)
      << "Clip duration should be in engine-rate frames";

  // Render ~1 second of output and measure the fundamental on the left channel.
  m_transport->startClip(h);

  std::vector<float> left(kBuffer, 0.0f), right(kBuffer, 0.0f);
  float* buffers[2] = {left.data(), right.data()};
  std::vector<float> captured;

  const int buffersToRender = static_cast<int>(kEngineRate / kBuffer); // ~1s
  for (int b = 0; b < buffersToRender; ++b) {
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    m_transport->processAudio(buffers, 2, kBuffer);
    for (size_t i = 0; i < kBuffer; ++i)
      captured.push_back(left[i]);
  }

  double measured = measureFreqFromChannel(captured, kEngineRate, /*guard*/ 4000);
  EXPECT_NEAR(measured, freq, 5.0)
      << "44.1kHz file in a 48kHz engine should still sound at " << freq << " Hz, got " << measured;
}

TEST_F(SampleRateConversionTest, MatchedFileIsUntouched) {
  // A 48 kHz file in a 48 kHz engine is not resampled: duration exact.
  std::string path = writeSineWav(m_dir / "match.wav", 48000, 440.0, 1.0);
  ClipHandle h = 1;
  ASSERT_EQ(m_transport->registerClipAudio(h, path.c_str()), SessionGraphError::OK);

  auto meta = m_transport->getClipMetadata(h);
  ASSERT_TRUE(meta.has_value());
  EXPECT_EQ(meta->trimOutSamples, 48000) << "Matched-rate file duration must be exact";
}
