// SPDX-License-Identifier: MIT
//
// ORP127 T7 (G5) — Voice allocation policy (VoiceMode).
//
// Covers the three first-class modes and their fire scenarios:
//   Polyphonic          — every fire layers a new voice (up to the cap).
//   MonoWithFadeOverlap — fire while playing restarts in place; fire during a
//                         fading tail adds a fresh voice alongside the tail.
//   MonoStrict          — fire while playing restarts from zero, no fade tail;
//                         a single voice at all times.

#include "../../src/core/transport/transport_controller.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <orpheus/audio_file_reader.h>
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

class HandleTransitionCallback : public ITransportCallback {
public:
  void onClipStarted(ClipHandle handle, orpheus::StartRequestTag, uint32_t voiceId,
                     TransportPosition) override {
    started.push_back(handle);
    startedVoiceIds.push_back(voiceId);
  }
  void onClipStopped(ClipHandle handle, orpheus::StartRequestTag, uint32_t voiceId,
                     TransportPosition) override {
    stopped.push_back(handle);
    stoppedVoiceIds.push_back(voiceId);
  }
  void onClipLooped(ClipHandle, orpheus::StartRequestTag, uint32_t, TransportPosition) override {}
  void onBufferUnderrun(TransportPosition) override {}

  std::vector<uint32_t> startedVoiceIds;
  std::vector<uint32_t> stoppedVoiceIds;

  std::vector<ClipHandle> started;
  std::vector<ClipHandle> stopped;
};

} // namespace

class VoiceModeTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_transport = std::make_unique<TransportController>(
        nullptr, TransportConfig{.sampleRate = static_cast<uint32_t>(kSampleRate)});
    m_dir = std::filesystem::temp_directory_path() / "orp127_voicemode";
    std::filesystem::create_directories(m_dir);
    m_path = writeSineWav(m_dir / "vm.wav", 440.0f, 2.0f);
  }
  void TearDown() override {
    m_transport.reset();
    std::error_code ec;
    std::filesystem::remove_all(m_dir, ec);
  }

  // Render one buffer to let the audio thread process pending commands.
  void pump(int buffers = 1) {
    std::vector<float> left(kBuffer, 0.0f), right(kBuffer, 0.0f);
    float* b[2] = {left.data(), right.data()};
    for (int i = 0; i < buffers; ++i)
      m_transport->processAudio(b, 2, kBuffer);
  }

  static constexpr uint32_t kSampleRate = 48000;
  static constexpr size_t kBuffer = 512;
  std::unique_ptr<TransportController> m_transport;
  std::filesystem::path m_dir;
  std::string m_path;
};

// ---- API round-trip --------------------------------------------------------

TEST_F(VoiceModeTest, SetAndGetVoiceMode) {
  ClipHandle h = 1;
  ASSERT_EQ(m_transport->registerClipAudio(h, m_path.c_str()), SessionGraphError::OK);

  // Default is Polyphonic.
  EXPECT_EQ(m_transport->getClipVoiceMode(h), VoiceMode::Polyphonic);

  EXPECT_EQ(m_transport->setClipVoiceMode(h, VoiceMode::MonoWithFadeOverlap),
            SessionGraphError::OK);
  EXPECT_EQ(m_transport->getClipVoiceMode(h), VoiceMode::MonoWithFadeOverlap);

  EXPECT_EQ(m_transport->setClipVoiceMode(h, VoiceMode::MonoStrict), SessionGraphError::OK);
  EXPECT_EQ(m_transport->getClipVoiceMode(h), VoiceMode::MonoStrict);

  EXPECT_EQ(m_transport->setClipVoiceMode(0, VoiceMode::Polyphonic),
            SessionGraphError::InvalidHandle);
  EXPECT_EQ(m_transport->setClipVoiceMode(999, VoiceMode::Polyphonic),
            SessionGraphError::ClipNotRegistered);
}

// ---- Polyphonic ------------------------------------------------------------

TEST_F(VoiceModeTest, PolyphonicLayersVoices) {
  ClipHandle h = 1;
  ASSERT_EQ(m_transport->registerClipAudio(h, m_path.c_str()), SessionGraphError::OK);
  ASSERT_EQ(m_transport->setClipVoiceMode(h, VoiceMode::Polyphonic), SessionGraphError::OK);

  // Fire three times; each fire should layer a new voice.
  m_transport->startClip(h);
  pump();
  m_transport->startClip(h);
  pump();
  m_transport->startClip(h);
  pump();

  EXPECT_EQ(m_transport->getActiveVoiceCount(h), 3u)
      << "Polyphonic mode should layer three simultaneous voices";
}

TEST_F(VoiceModeTest, VoiceAwareCallbacksPublishAcceptedSdkIdentity) {
  constexpr ClipHandle handle = 1;
  ASSERT_EQ(m_transport->registerClipAudio(handle, m_path.c_str()), SessionGraphError::OK);
  ASSERT_EQ(m_transport->setClipVoiceMode(handle, VoiceMode::MonoWithFadeOverlap),
            SessionGraphError::OK);

  HandleTransitionCallback callback;
  m_transport->setCallback(&callback);

  ASSERT_EQ(m_transport->startClip(handle), SessionGraphError::OK);
  pump();
  m_transport->processCallbacks();
  ASSERT_EQ(callback.startedVoiceIds.size(), 1u);
  const uint32_t initialVoiceId = callback.startedVoiceIds.front();
  EXPECT_NE(initialVoiceId, 0u);

  ASSERT_EQ(m_transport->startClip(handle), SessionGraphError::OK);
  pump();
  m_transport->processCallbacks();
  ASSERT_EQ(callback.startedVoiceIds.size(), 2u);
  EXPECT_EQ(callback.startedVoiceIds.back(), initialVoiceId)
      << "an in-place refire must retain the SDK voice identity";

  ASSERT_EQ(m_transport->stopClip(handle), SessionGraphError::OK);
  pump(4);
  m_transport->processCallbacks();
  ASSERT_EQ(callback.stoppedVoiceIds.size(), 1u);
  EXPECT_EQ(callback.stoppedVoiceIds.front(), initialVoiceId);
}

// ---- MonoWithFadeOverlap ---------------------------------------------------

TEST_F(VoiceModeTest, MonoFadeOverlapRestartsInPlaceWhilePlaying) {
  ClipHandle h = 1;
  ASSERT_EQ(m_transport->registerClipAudio(h, m_path.c_str()), SessionGraphError::OK);
  ASSERT_EQ(m_transport->setClipVoiceMode(h, VoiceMode::MonoWithFadeOverlap),
            SessionGraphError::OK);

  m_transport->startClip(h);
  pump(4); // advance a bit so position > trim IN

  int64_t posBefore = m_transport->getClipPosition(h);
  EXPECT_GT(posBefore, 0);

  // Fire again while playing: must NOT stack — one voice, restarted near IN.
  m_transport->startClip(h);
  pump();

  EXPECT_EQ(m_transport->getActiveVoiceCount(h), 1u)
      << "Fire-while-playing must not stack in MonoWithFadeOverlap";
  EXPECT_LT(m_transport->getClipPosition(h), posBefore)
      << "Fire-while-playing should restart the voice near trim IN";
}

TEST_F(VoiceModeTest, MonoFadeOverlapAddsFreshVoiceDuringFadeTail) {
  ClipHandle h = 1;
  ASSERT_EQ(m_transport->registerClipAudio(h, m_path.c_str()), SessionGraphError::OK);
  // Long fade-out so the tail persists across the re-fire window.
  ASSERT_EQ(m_transport->updateClipFades(h, 0.0, 0.2, FadeCurve::Linear, FadeCurve::Linear),
            SessionGraphError::OK);
  ASSERT_EQ(m_transport->setClipVoiceMode(h, VoiceMode::MonoWithFadeOverlap),
            SessionGraphError::OK);

  m_transport->startClip(h);
  pump(4);

  // Stop -> the voice begins a (200ms) fade tail.
  m_transport->stopClip(h);
  pump(); // tail now fading (isStopping = true)
  ASSERT_EQ(m_transport->getClipState(h), PlaybackState::Stopping);

  // Fire again DURING the fade tail: a fresh voice should coexist with the tail.
  m_transport->startClip(h);
  pump();

  EXPECT_EQ(m_transport->getActiveVoiceCount(h), 2u)
      << "Fire during fade tail should add a fresh voice alongside the tail (voices == 2)";
  // The clip is playing again (the fresh voice is not stopping).
  EXPECT_EQ(m_transport->getClipState(h), PlaybackState::Playing);
}

TEST_F(VoiceModeTest, MonoFadeOverlapTailCompletesAndTearsDown) {
  ClipHandle h = 1;
  ASSERT_EQ(m_transport->registerClipAudio(h, m_path.c_str()), SessionGraphError::OK);
  // 100ms fade so the overlap window spans several buffers and is observable
  // before the tail tears down.
  ASSERT_EQ(m_transport->updateClipFades(h, 0.0, 0.1, FadeCurve::Linear, FadeCurve::Linear),
            SessionGraphError::OK);
  ASSERT_EQ(m_transport->setClipVoiceMode(h, VoiceMode::MonoWithFadeOverlap),
            SessionGraphError::OK);

  m_transport->startClip(h);
  pump(4);
  m_transport->stopClip(h);
  pump();                    // tail now fading (100ms ~= 4800 samples ~= 9.4 buffers)
  m_transport->startClip(h); // fresh voice + fading tail
  pump();
  EXPECT_EQ(m_transport->getActiveVoiceCount(h), 2u)
      << "Fresh voice must coexist with the fading tail during the overlap window";

  // Let the ~100ms tail complete (well over 10 buffers from the stop point).
  pump(14);
  EXPECT_EQ(m_transport->getActiveVoiceCount(h), 1u)
      << "The fade tail must complete and be torn down, leaving only the fresh voice";
  EXPECT_EQ(m_transport->getClipState(h), PlaybackState::Playing);
}

TEST_F(VoiceModeTest, StopAllTailCompletionCannotEvictRefiredSiblingVoice) {
  constexpr ClipHandle first = 1;
  constexpr ClipHandle refired = 2;
  for (const auto handle : {first, refired}) {
    ASSERT_EQ(m_transport->registerClipAudio(handle, m_path.c_str()), SessionGraphError::OK);
    ASSERT_EQ(m_transport->updateClipFades(handle, 0.0, 0.1, FadeCurve::Linear, FadeCurve::Linear),
              SessionGraphError::OK);
    ASSERT_EQ(m_transport->setClipVoiceMode(handle, VoiceMode::MonoWithFadeOverlap),
              SessionGraphError::OK);
  }

  HandleTransitionCallback callback;
  m_transport->setCallback(&callback);
  ASSERT_EQ(m_transport->startClip(first), SessionGraphError::OK);
  ASSERT_EQ(m_transport->startClip(refired), SessionGraphError::OK);
  pump();

  ASSERT_EQ(m_transport->stopAllClips(), SessionGraphError::OK);
  pump();
  ASSERT_EQ(m_transport->getClipState(refired), PlaybackState::Stopping);

  ASSERT_EQ(m_transport->startClip(refired), SessionGraphError::OK);
  pump();
  ASSERT_EQ(m_transport->getActiveVoiceCount(refired), 2u);

  pump(14);
  m_transport->processCallbacks();

  EXPECT_EQ(m_transport->getActiveVoiceCount(first), 0u);
  EXPECT_EQ(m_transport->getActiveVoiceCount(refired), 1u);
  EXPECT_EQ(m_transport->getClipState(refired), PlaybackState::Playing);
  ASSERT_EQ(callback.startedVoiceIds.size(), 3u);
  EXPECT_EQ(std::count(callback.stopped.begin(), callback.stopped.end(), first), 1);
  EXPECT_EQ(std::count(callback.stopped.begin(), callback.stopped.end(), refired), 1);
  EXPECT_EQ(std::count(callback.stoppedVoiceIds.begin(), callback.stoppedVoiceIds.end(),
                       callback.startedVoiceIds[1]),
            1)
      << "The retired fade tail must publish its own voice identity";
  EXPECT_EQ(std::count(callback.stoppedVoiceIds.begin(), callback.stoppedVoiceIds.end(),
                       callback.startedVoiceIds[2]),
            0)
      << "The fresh sibling voice must remain live";
}

TEST_F(VoiceModeTest, StopAllNearLoopBoundaryCompletesFadeInsteadOfLoopingForever) {
  constexpr ClipHandle handle = 1;
  ASSERT_EQ(m_transport->registerClipAudio(handle, m_path.c_str()), SessionGraphError::OK);
  ASSERT_EQ(m_transport->updateClipFades(handle, 0.0, 0.5, FadeCurve::Linear, FadeCurve::Linear),
            SessionGraphError::OK);
  ASSERT_EQ(m_transport->setClipLoopMode(handle, true), SessionGraphError::OK);

  ASSERT_EQ(m_transport->startClip(handle), SessionGraphError::OK);
  pump();
  ASSERT_EQ(m_transport->seekClip(handle, 91200), SessionGraphError::OK);
  pump();
  ASSERT_GT(m_transport->getClipPosition(handle), 91200);

  ASSERT_EQ(m_transport->stopAllClips(), SessionGraphError::OK);
  pump(60);

  EXPECT_EQ(m_transport->getActiveVoiceCount(handle), 0u);
  EXPECT_EQ(m_transport->getClipState(handle), PlaybackState::Stopped)
      << "A stopping loop must not wrap to trim IN and restart its fade clock";
}

// ---- MonoStrict ------------------------------------------------------------

TEST_F(VoiceModeTest, MonoStrictRestartsFromZeroNoStacking) {
  ClipHandle h = 1;
  ASSERT_EQ(m_transport->registerClipAudio(h, m_path.c_str()), SessionGraphError::OK);
  ASSERT_EQ(m_transport->setClipVoiceMode(h, VoiceMode::MonoStrict), SessionGraphError::OK);

  m_transport->startClip(h);
  pump(4);
  int64_t posBefore = m_transport->getClipPosition(h);
  EXPECT_GT(posBefore, 0);

  m_transport->startClip(h);
  pump();

  EXPECT_EQ(m_transport->getActiveVoiceCount(h), 1u) << "MonoStrict must never stack";
  EXPECT_LT(m_transport->getClipPosition(h), posBefore)
      << "MonoStrict fire-while-playing restarts from zero";
}

TEST_F(VoiceModeTest, MonoStrictCutsFadingTail) {
  ClipHandle h = 1;
  ASSERT_EQ(m_transport->registerClipAudio(h, m_path.c_str()), SessionGraphError::OK);
  // Long fade so, under fade-overlap, a tail would linger — MonoStrict must cut it.
  ASSERT_EQ(m_transport->updateClipFades(h, 0.0, 0.2, FadeCurve::Linear, FadeCurve::Linear),
            SessionGraphError::OK);
  ASSERT_EQ(m_transport->setClipVoiceMode(h, VoiceMode::MonoStrict), SessionGraphError::OK);

  m_transport->startClip(h);
  pump(4);
  m_transport->stopClip(h);
  pump();
  ASSERT_EQ(m_transport->getClipState(h), PlaybackState::Stopping);

  // Fire during the tail: MonoStrict replaces it with a single fresh voice — no
  // overlap, exactly one voice.
  m_transport->startClip(h);
  pump();

  EXPECT_EQ(m_transport->getActiveVoiceCount(h), 1u)
      << "MonoStrict must cut the fading tail (single voice, no overlap)";
  EXPECT_EQ(m_transport->getClipState(h), PlaybackState::Playing);
}
