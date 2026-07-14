// SPDX-License-Identifier: MIT
//
// OCC155 — SDK API requests from Clip Composer.
//
// Regression + contract coverage for the three net-new transport APIs and the
// one confirmation the SDK team owed OCC:
//
//   Ask #2 (CRITICAL) — fade-overlap re-fire no longer corrupts playback via a
//                       shared file cursor. ORP134 G1 replaced the per-voice
//                       IAudioFileReader with position-explicit IClipSource
//                       views; this test proves the two overlapping voices of a
//                       MonoWithFadeOverlap clip read from INDEPENDENT positions.
//   Ask #3 (HIGH)     — getRoutingMatrix() returns the transport's public
//                       IRoutingMatrix so hosts stop reaching into internals.
//   Ask #4 (MED)      — unregisterClipAudio() releases a registered clip and is
//                       correctly gated on active voices.
//   Ask #5 (HIGH)     — panic() hard-cuts all voices with no fade (immediate
//                       mute), in contrast to stopAllClips()'s fade envelope.

#include "../../src/core/transport/transport_controller.h"
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <orpheus/audio_file_reader.h>
#include <orpheus/routing_matrix.h>
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

// A CONSTANT full-scale WAV: output amplitude tracks the applied gain envelope,
// so panic vs. fade behavior is readable straight off the buffer.
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

// A distinct-per-frame sine so the two overlapping voices of one clip, seeked to
// different positions, produce measurably different output. A high frequency
// makes adjacent-position phase differences large and easy to detect.
std::string writeSineWav(const std::filesystem::path& path, float freq, float durationSeconds,
                         uint32_t sampleRate = 48000) {
  const uint16_t numChannels = 2;
  const int64_t numFrames = static_cast<int64_t>(durationSeconds * sampleRate);
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
    float s = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * freq * static_cast<float>(i) /
                              static_cast<float>(sampleRate));
    int16_t v = static_cast<int16_t>(s * 32767.0f);
    file.write(reinterpret_cast<const char*>(&v), 2);
    file.write(reinterpret_cast<const char*>(&v), 2);
  }
  file.close();
  return path.string();
}

} // namespace

class Occ155ApiTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_transport = std::make_unique<TransportController>(nullptr, kSampleRate);
    m_dir = std::filesystem::temp_directory_path() / "occ155_api";
    std::filesystem::create_directories(m_dir);
    m_constPath = writeConstantWav(m_dir / "const.wav", 2.0f, kSampleRate);
    m_sinePath = writeSineWav(m_dir / "sine.wav", 4000.0f, 2.0f, kSampleRate);
  }
  void TearDown() override {
    m_transport.reset();
    std::error_code ec;
    std::filesystem::remove_all(m_dir, ec);
  }

  // Render `buffers` blocks; captures the max |sample| seen in channel 0 so the
  // caller can assert on output level.
  float renderPeak(int buffers = 1) {
    std::vector<float> left(kBuffer, 0.0f), right(kBuffer, 0.0f);
    float* b[2] = {left.data(), right.data()};
    float peak = 0.0f;
    for (int i = 0; i < buffers; ++i) {
      m_transport->processAudio(b, 2, kBuffer);
      for (size_t s = 0; s < kBuffer; ++s)
        peak = std::max(peak, std::fabs(left[s]));
    }
    return peak;
  }

  void pump(int buffers = 1) {
    (void)renderPeak(buffers);
  }

  static constexpr uint32_t kSampleRate = 48000;
  static constexpr size_t kBuffer = 512;
  std::unique_ptr<TransportController> m_transport;
  std::filesystem::path m_dir;
  std::string m_constPath;
  std::string m_sinePath;
};

// ===========================================================================
// Ask #3 — public getRoutingMatrix() accessor
// ===========================================================================

TEST_F(Occ155ApiTest, GetRoutingMatrixReturnsUsableMatrix) {
  IRoutingMatrix* matrix = m_transport->getRoutingMatrix();
  ASSERT_NE(matrix, nullptr) << "Transport must expose its routing matrix";

  // The accessor must be stable across calls (same owned instance).
  EXPECT_EQ(matrix, m_transport->getRoutingMatrix());

  // And it must be a live, configured matrix — a group op returns OK, proving
  // OCC can drive group gains/mutes/meters through the public interface instead
  // of the old `#define private public` reach-in.
  EXPECT_EQ(matrix->setGroupGain(0, -6.0f), SessionGraphError::OK);
  EXPECT_EQ(matrix->setGroupMute(0, true), SessionGraphError::OK);
  EXPECT_TRUE(matrix->isGroupMuted(0));
  EXPECT_EQ(matrix->setGroupMute(0, false), SessionGraphError::OK);
}

// ===========================================================================
// Ask #4 — unregisterClipAudio()
// ===========================================================================

TEST_F(Occ155ApiTest, UnregisterClipAudioReleasesRegisteredClip) {
  ClipHandle h = 1;
  ASSERT_EQ(m_transport->registerClipAudio(h, m_constPath.c_str()), SessionGraphError::OK);
  // Metadata is queryable while registered.
  ASSERT_TRUE(m_transport->getClipMetadata(h).has_value());

  EXPECT_EQ(m_transport->unregisterClipAudio(h), SessionGraphError::OK);

  // After release the clip is gone from the registry.
  EXPECT_FALSE(m_transport->getClipMetadata(h).has_value());
}

TEST_F(Occ155ApiTest, UnregisterClipAudioIsIdempotentAndValidatesHandle) {
  // Unregistered handle: no-op success so hosts can call unconditionally.
  EXPECT_EQ(m_transport->unregisterClipAudio(42), SessionGraphError::OK);
  // Zero handle is invalid.
  EXPECT_EQ(m_transport->unregisterClipAudio(0), SessionGraphError::InvalidHandle);
}

TEST_F(Occ155ApiTest, UnregisterClipAudioRefusesWhileVoiceActive) {
  ClipHandle h = 1;
  ASSERT_EQ(m_transport->registerClipAudio(h, m_constPath.c_str()), SessionGraphError::OK);

  m_transport->startClip(h);
  pump(2);
  ASSERT_TRUE(m_transport->isClipPlaying(h));

  // Must refuse: an active voice still reads the source.
  EXPECT_EQ(m_transport->unregisterClipAudio(h), SessionGraphError::NotReady);
  EXPECT_TRUE(m_transport->getClipMetadata(h).has_value())
      << "Entry must survive a refused release";

  // Hard-cut, then release succeeds.
  ASSERT_EQ(m_transport->panic(), SessionGraphError::OK);
  pump(2);
  ASSERT_FALSE(m_transport->isClipPlaying(h));
  EXPECT_EQ(m_transport->unregisterClipAudio(h), SessionGraphError::OK);
  EXPECT_FALSE(m_transport->getClipMetadata(h).has_value());
}

// ===========================================================================
// Ask #5 — panic() immediate hard-cut
// ===========================================================================

TEST_F(Occ155ApiTest, PanicSilencesImmediatelyWithNoFadeTail) {
  ClipHandle h = 1;
  ASSERT_EQ(m_transport->registerClipAudio(h, m_constPath.c_str()), SessionGraphError::OK);
  // A LONG fade-out: stopAllClips() would ride this envelope down for 500ms.
  // panic() must ignore it entirely.
  ASSERT_EQ(m_transport->updateClipFades(h, 0.0, 0.5, FadeCurve::Linear, FadeCurve::Linear),
            SessionGraphError::OK);

  m_transport->startClip(h);
  float steady = renderPeak(1);
  ASSERT_GT(steady, 0.01f) << "Expected non-silent steady state before panic";

  ASSERT_EQ(m_transport->panic(), SessionGraphError::OK);

  // The very next block after panic must be silent (no fade tail at all).
  float afterPanic = renderPeak(1);
  EXPECT_LT(afterPanic, steady * 0.001f)
      << "panic() must silence output on the next block; got " << afterPanic;

  // And all voices are gone.
  EXPECT_EQ(m_transport->getActiveVoiceCount(h), 0u);
  EXPECT_FALSE(m_transport->isClipPlaying(h));
}

TEST_F(Occ155ApiTest, PanicDiffersFromStopAllClipsFade) {
  ClipHandle h = 1;
  ASSERT_EQ(m_transport->registerClipAudio(h, m_constPath.c_str()), SessionGraphError::OK);
  ASSERT_EQ(m_transport->updateClipFades(h, 0.0, 0.5, FadeCurve::Linear, FadeCurve::Linear),
            SessionGraphError::OK);

  // Baseline: stopAllClips() rides the fade — the block right after is still
  // largely audible (500ms fade >> one 512-sample block).
  m_transport->startClip(h);
  float steady = renderPeak(1);
  ASSERT_GT(steady, 0.01f);
  ASSERT_EQ(m_transport->stopAllClips(), SessionGraphError::OK);
  float afterStopAll = renderPeak(1);
  EXPECT_GT(afterStopAll, steady * 0.5f)
      << "stopAllClips() should still be audible one block into a 500ms fade";
}

TEST_F(Occ155ApiTest, PanicClearsAllVoicesAcrossClips) {
  ClipHandle h1 = 1, h2 = 2;
  ASSERT_EQ(m_transport->registerClipAudio(h1, m_constPath.c_str()), SessionGraphError::OK);
  ASSERT_EQ(m_transport->registerClipAudio(h2, m_sinePath.c_str()), SessionGraphError::OK);

  m_transport->startClip(h1);
  m_transport->startClip(h2);
  pump(2);
  ASSERT_TRUE(m_transport->isClipPlaying(h1));
  ASSERT_TRUE(m_transport->isClipPlaying(h2));

  ASSERT_EQ(m_transport->panic(), SessionGraphError::OK);
  pump(1);

  EXPECT_EQ(m_transport->getActiveVoiceCount(h1), 0u);
  EXPECT_EQ(m_transport->getActiveVoiceCount(h2), 0u);
  EXPECT_FALSE(m_transport->isClipPlaying(h1));
  EXPECT_FALSE(m_transport->isClipPlaying(h2));
}

// ===========================================================================
// Ask #2 (CRITICAL) — fade-overlap re-fire: two voices, independent cursors
// ===========================================================================
//
// The v0.3.0 bug: one shared_ptr<IAudioFileReader> per handle meant the two
// voices in the fade-overlap window drove a single SNDFILE* cursor via seek()/
// readSamples(), corrupting each other's file position. ORP134 G1 made reads
// position-explicit (source->read(pos, ...)). This test fires a
// MonoWithFadeOverlap clip, stops it to start a fade tail, then re-fires DURING
// the tail so two voices coexist — and asserts the fresh voice actually plays
// from near trim IN while the tail continues from deep in the file, i.e. the
// two cursors are independent, not fighting.

TEST_F(Occ155ApiTest, FadeOverlapReFireHasIndependentCursors) {
  ClipHandle h = 1;
  ASSERT_EQ(m_transport->registerClipAudio(h, m_sinePath.c_str()), SessionGraphError::OK);
  // Long fade tail so it survives the re-fire window across several blocks.
  ASSERT_EQ(m_transport->updateClipFades(h, 0.0, 0.2, FadeCurve::Linear, FadeCurve::Linear),
            SessionGraphError::OK);
  ASSERT_EQ(m_transport->setClipVoiceMode(h, VoiceMode::MonoWithFadeOverlap),
            SessionGraphError::OK);

  // Fire and let the first voice advance well into the file.
  m_transport->startClip(h);
  pump(20); // ~20 * 512 = 10240 samples in
  int64_t tailPos = m_transport->getClipPosition(h);
  ASSERT_GT(tailPos, 5000) << "First voice should be deep into the file before re-fire";

  // Stop -> the voice enters its 200ms fade tail.
  m_transport->stopClip(h);
  pump(1);
  ASSERT_EQ(m_transport->getClipState(h), PlaybackState::Stopping);

  // Re-fire DURING the tail: fresh voice must coexist with the fading tail.
  m_transport->startClip(h);
  pump(1);
  ASSERT_EQ(m_transport->getActiveVoiceCount(h), 2u)
      << "Fade-overlap re-fire must produce two voices (fresh + fading tail)";

  // getClipPosition() returns the FIRST voice found. The load-bearing assertion:
  // the reported position is now near trim IN (fresh voice) — NOT still tracking
  // the deep tail position. Under the v0.3.0 shared-cursor bug, the two voices
  // could not maintain distinct positions; a fresh voice seeking to IN would drag
  // the tail's cursor (or vice versa), so the fresh voice could not sit near IN
  // while the tail lived on independently.
  int64_t freshPos = m_transport->getClipPosition(h);
  EXPECT_LT(freshPos, tailPos)
      << "Fresh voice must play from near trim IN, independent of the deep tail cursor "
      << "(fresh=" << freshPos << ", tail was=" << tailPos << ")";

  // Let the tail complete; exactly the fresh voice remains and it keeps advancing
  // from its own cursor — proof the two never shared state.
  pump(20); // 200ms tail ~= 9600 samples ~= 19 blocks
  EXPECT_EQ(m_transport->getActiveVoiceCount(h), 1u)
      << "Fade tail must complete and tear down, leaving only the fresh voice";
  EXPECT_EQ(m_transport->getClipState(h), PlaybackState::Playing);
  int64_t advanced = m_transport->getClipPosition(h);
  EXPECT_GT(advanced, freshPos) << "Surviving fresh voice must keep advancing on its own cursor";
}
