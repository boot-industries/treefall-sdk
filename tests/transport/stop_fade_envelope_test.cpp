// SPDX-License-Identifier: MIT
//
// ORP127 T4 (G2) — Measure the stop fade-out envelope.
//
// Before ORP127, the stop fade computed a single gain scalar per buffer, so a
// short fade (e.g. 10ms @ 512-sample buffers @ 48kHz) rendered as a 1-2 step
// staircase — audible as bitcrush. This test plays a constant full-scale signal,
// triggers stopClip(), captures the per-sample output during the fade, and
// asserts the envelope is smooth: monotonically non-increasing with no long
// constant plateaus (which a per-buffer staircase would produce).

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

// Write a stereo 16-bit PCM WAV holding a CONSTANT full-scale positive DC value.
// A constant source makes the output amplitude equal to the applied gain
// envelope, so we can read the fade shape straight off the output buffer.
std::string writeConstantWav(const std::filesystem::path& path, float durationSeconds,
                             uint32_t sampleRate = 48000) {
  const uint16_t numChannels = 2;
  const int64_t numFrames = static_cast<int64_t>(durationSeconds * sampleRate);
  const int16_t kConst = 30000; // near full scale, positive

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

class StopFadeEnvelopeTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_transport = std::make_unique<TransportController>(nullptr, TransportConfig{.sampleRate = static_cast<uint32_t>(kSampleRate)});
    m_path = (std::filesystem::temp_directory_path() / "orp127_stop_fade.wav").string();
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

// Capture the fade envelope across a 10ms stop fade and assert smoothness.
TEST_F(StopFadeEnvelopeTest, TenMillisecondStopFadeIsSmooth) {
  ClipHandle handle = 1;
  ASSERT_EQ(m_transport->registerClipAudio(handle, m_path.c_str()), SessionGraphError::OK);

  // Configure a 10ms linear fade-out. 10ms @ 48kHz = 480 samples — spans ~1
  // buffer, which is exactly where the old per-buffer scalar produced a
  // staircase.
  ASSERT_EQ(m_transport->updateClipFades(handle, 0.0, 0.010, FadeCurve::Linear, FadeCurve::Linear),
            SessionGraphError::OK);

  std::vector<float> left(kBuffer, 0.0f);
  std::vector<float> right(kBuffer, 0.0f);
  float* buffers[2] = {left.data(), right.data()};

  // Start and render one buffer at full level to establish the steady state.
  m_transport->startClip(handle);
  m_transport->processAudio(buffers, 2, kBuffer);

  // Reference full-scale level (the constant source * routing gain).
  float steady = std::fabs(left[100]);
  ASSERT_GT(steady, 0.01f) << "Expected a non-silent steady-state signal";

  // Trigger stop and capture the envelope across the fade. The fade is ~480
  // samples, so 3 buffers (1536 samples) comfortably covers it.
  ASSERT_EQ(m_transport->stopClip(handle), SessionGraphError::OK);

  std::vector<float> envelope;
  for (int b = 0; b < 3; ++b) {
    m_transport->processAudio(buffers, 2, kBuffer);
    for (size_t i = 0; i < kBuffer; ++i) {
      envelope.push_back(std::fabs(left[i]));
    }
  }

  // 1) Monotonic non-increase (small epsilon for float noise). A per-buffer
  //    staircase would still be monotonic, so this alone is necessary but not
  //    sufficient — see the plateau check below.
  for (size_t i = 1; i < envelope.size(); ++i) {
    EXPECT_LE(envelope[i], envelope[i - 1] + 1e-4f)
        << "Envelope rose at sample " << i << " (" << envelope[i - 1] << " -> " << envelope[i]
        << ")";
  }

  // 2) The fade must actually reach (near) silence by the end.
  EXPECT_LT(envelope.back(), steady * 0.02f) << "Fade did not reach silence";

  // 3) No staircase: within the active fade region the gain must change nearly
  //    every sample. A per-buffer scalar holds a constant value for 512 samples
  //    at a time; a per-sample linear fade over 480 samples changes by ~steady/480
  //    each step. Count the longest run of (near-)identical consecutive values
  //    while the signal is still audible; a per-sample fade keeps this tiny.
  size_t longestPlateau = 0;
  size_t currentPlateau = 0;
  const float stepEps = steady / 5000.0f; // far smaller than a real per-sample step
  for (size_t i = 1; i < envelope.size(); ++i) {
    if (envelope[i - 1] < steady * 0.02f) {
      break; // reached silence — plateaus of zero are expected past the fade
    }
    if (std::fabs(envelope[i] - envelope[i - 1]) <= stepEps) {
      ++currentPlateau;
      longestPlateau = std::max(longestPlateau, currentPlateau);
    } else {
      currentPlateau = 0;
    }
  }

  // A per-buffer staircase would hold constant for a full buffer (512) or at
  // least many hundreds of samples. A per-sample fade holds for only a handful.
  EXPECT_LT(longestPlateau, static_cast<size_t>(64))
      << "Envelope has a long constant plateau (" << longestPlateau
      << " samples) — indicates per-buffer staircase, not a per-sample fade";
}

// Same idea but with a longer 50ms fade — verify the per-sample envelope tracks
// a linear ramp closely across multiple buffers.
TEST_F(StopFadeEnvelopeTest, FiftyMillisecondStopFadeTracksLinearRamp) {
  ClipHandle handle = 1;
  ASSERT_EQ(m_transport->registerClipAudio(handle, m_path.c_str()), SessionGraphError::OK);
  ASSERT_EQ(m_transport->updateClipFades(handle, 0.0, 0.050, FadeCurve::Linear, FadeCurve::Linear),
            SessionGraphError::OK);

  std::vector<float> left(kBuffer, 0.0f);
  std::vector<float> right(kBuffer, 0.0f);
  float* buffers[2] = {left.data(), right.data()};

  m_transport->startClip(handle);
  m_transport->processAudio(buffers, 2, kBuffer);
  float steady = std::fabs(left[100]);
  ASSERT_GT(steady, 0.01f);

  ASSERT_EQ(m_transport->stopClip(handle), SessionGraphError::OK);

  const int fadeSamples = static_cast<int>(0.050 * kSampleRate); // 2400
  std::vector<float> envelope;
  const int buffersNeeded = (fadeSamples / static_cast<int>(kBuffer)) + 2;
  for (int b = 0; b < buffersNeeded; ++b) {
    m_transport->processAudio(buffers, 2, kBuffer);
    for (size_t i = 0; i < kBuffer; ++i)
      envelope.push_back(std::fabs(left[i]));
  }

  // Compare the measured envelope against the ideal linear ramp at several
  // checkpoints (25%, 50%, 75% through the fade). A per-buffer staircase would
  // deviate substantially at these interior points.
  for (double frac : {0.25, 0.5, 0.75}) {
    size_t idx = static_cast<size_t>(frac * fadeSamples);
    ASSERT_LT(idx, envelope.size());
    float expected = steady * static_cast<float>(1.0 - frac);
    EXPECT_NEAR(envelope[idx], expected, steady * 0.05f)
        << "Envelope at " << (frac * 100) << "% deviates from the linear ramp";
  }
}

// ORP127 T5 (G3): a non-looped clip with NO configured fade-out that reaches its
// OUT point must NOT hard-cut. Before this fix, the reader was nulled at OUT and
// the trailing OUT-buffer samples rendered at full gain, producing a click. Now
// the default stop fade renders real audio through the boundary. We measure the
// largest sample-to-sample DROP across the OUT transition and require it to be
// small (a hard cut would drop from full-scale to ~0 in a single sample).
TEST_F(StopFadeEnvelopeTest, NonLoopedOutBoundaryHasNoHardCut) {
  ClipHandle handle = 1;
  ASSERT_EQ(m_transport->registerClipAudio(handle, m_path.c_str()), SessionGraphError::OK);

  // Trim OUT well inside the file so there is real audio to read past OUT for
  // the fade tail. No fade configured — this is the bare hard-cut scenario.
  const int64_t trimOut = 20000;
  ASSERT_EQ(m_transport->updateClipTrimPoints(handle, 0, trimOut), SessionGraphError::OK);
  ASSERT_EQ(m_transport->setClipLoopMode(handle, false), SessionGraphError::OK);
  ASSERT_EQ(m_transport->updateClipFades(handle, 0.0, 0.0, FadeCurve::Linear, FadeCurve::Linear),
            SessionGraphError::OK);

  std::vector<float> left(kBuffer, 0.0f);
  std::vector<float> right(kBuffer, 0.0f);
  float* buffers[2] = {left.data(), right.data()};

  m_transport->startClip(handle);

  // Render across OUT (20000 samples ~= 40 buffers) plus the fade tail.
  std::vector<float> envelope;
  for (int b = 0; b < 45; ++b) {
    m_transport->processAudio(buffers, 2, kBuffer);
    for (size_t i = 0; i < kBuffer; ++i) {
      envelope.push_back(std::fabs(left[i]));
    }
  }

  float steady = 0.0f;
  for (size_t i = 0; i < 200 && i < envelope.size(); ++i) {
    steady = std::max(steady, envelope[i]);
  }
  ASSERT_GT(steady, 0.01f) << "Expected a non-silent steady state before OUT";

  // Largest single-sample downward step anywhere in the signal.
  float maxDrop = 0.0f;
  for (size_t i = 1; i < envelope.size(); ++i) {
    float drop = envelope[i - 1] - envelope[i];
    maxDrop = std::max(maxDrop, drop);
  }

  // A hard cut drops by ~steady in one sample. The default 10ms fade (480
  // samples) drops by ~steady/480 per sample. Require the worst step to be a
  // small fraction of full scale — comfortably rules out a cliff.
  EXPECT_LT(maxDrop, steady * 0.1f) << "Largest single-sample drop (" << maxDrop
                                    << ") near OUT indicates a hard cut; steady = " << steady;

  // And the clip must reach silence (fade actually completed).
  EXPECT_LT(envelope.back(), steady * 0.02f) << "Clip did not fade to silence after OUT";
}
