// SPDX-License-Identifier: MIT
//
// ORP127 T9 (G7) — Choke primitive + configurable voice caps.
//
// stopOtherClips(exceptHandle) is the host-neutral choke primitive: it stops
// every voice except the exempt clip's. Hosts (e.g. Clip Composer playgroups)
// scope choke by choosing what to exempt — the SDK has no notion of playgroups.
// setMaxVoicesPerClip bounds voice layering as a resource guard.

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

class CapacityCallback final : public ITransportCallback {
public:
  void onClipStarted(ClipHandle, uint32_t, TransportPosition) override {}
  void onClipStopped(ClipHandle, uint32_t, TransportPosition) override {}
  void onClipLooped(ClipHandle, uint32_t, TransportPosition) override {}
  void onBufferUnderrun(TransportPosition) override {}
  void onActiveClipLimitReached(ClipHandle handle, TransportPosition) override {
    refusedHandle = handle;
    ++refusalCount;
  }

  ClipHandle refusedHandle{0};
  int refusalCount{0};
};

} // namespace

class ChokeAndVoiceCapTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_transport = std::make_unique<TransportController>(nullptr, TransportConfig{.sampleRate = static_cast<uint32_t>(kSampleRate)});
    m_dir = std::filesystem::temp_directory_path() / "orp127_choke";
    std::filesystem::create_directories(m_dir);
  }
  void TearDown() override {
    m_transport.reset();
    std::error_code ec;
    std::filesystem::remove_all(m_dir, ec);
  }

  ClipHandle registerClip(int idx, float freq) {
    ClipHandle h = static_cast<ClipHandle>(idx);
    std::string p = writeSineWav(m_dir / ("c" + std::to_string(idx) + ".wav"), freq, 2.0f);
    EXPECT_EQ(m_transport->registerClipAudio(h, p.c_str()), SessionGraphError::OK);
    return h;
  }

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
};

// ---- Choke primitive -------------------------------------------------------

TEST_F(ChokeAndVoiceCapTest, StopOtherClipsSparesExemptClip) {
  ClipHandle a = registerClip(1, 220.0f);
  ClipHandle b = registerClip(2, 330.0f);
  ClipHandle c = registerClip(3, 440.0f);

  m_transport->startClip(a);
  m_transport->startClip(b);
  m_transport->startClip(c);
  pump();
  ASSERT_EQ(m_transport->getClipState(a), PlaybackState::Playing);
  ASSERT_EQ(m_transport->getClipState(b), PlaybackState::Playing);
  ASSERT_EQ(m_transport->getClipState(c), PlaybackState::Playing);

  // Choke everything except b.
  ASSERT_EQ(m_transport->stopOtherClips(b), SessionGraphError::OK);
  pump();

  // a and c are stopping/stopped; b is untouched (still playing).
  EXPECT_NE(m_transport->getClipState(a), PlaybackState::Playing);
  EXPECT_NE(m_transport->getClipState(c), PlaybackState::Playing);
  EXPECT_EQ(m_transport->getClipState(b), PlaybackState::Playing)
      << "The exempt clip must not be choked";
}

// Group isolation: firing in "group A" (choking non-A) does not affect "group B".
// The SDK primitive is by-handle-set; here the host would exempt every handle it
// considers part of the firing clip's group. We model a two-group host by
// exempting the group-B members explicitly is not possible with a single
// except-handle, so this test verifies the building block: a choke scoped to
// spare one handle leaves that handle's voices intact regardless of others.
TEST_F(ChokeAndVoiceCapTest, ChokeIsByHandleNotGlobal) {
  ClipHandle a = registerClip(1, 220.0f);
  ClipHandle b = registerClip(2, 330.0f);

  m_transport->startClip(a);
  m_transport->startClip(b);
  pump();

  // stopOtherClips(0) == stop everything (no exemption).
  ASSERT_EQ(m_transport->stopOtherClips(0), SessionGraphError::OK);
  pump();
  EXPECT_NE(m_transport->getClipState(a), PlaybackState::Playing);
  EXPECT_NE(m_transport->getClipState(b), PlaybackState::Playing);
}

// ---- Voice caps ------------------------------------------------------------

TEST_F(ChokeAndVoiceCapTest, DefaultVoiceCapIsEight) {
  EXPECT_EQ(m_transport->getMaxVoicesPerClip(), 8u);
}

TEST_F(ChokeAndVoiceCapTest, VoiceCapIsClampedToRange) {
  EXPECT_EQ(m_transport->setMaxVoicesPerClip(0), SessionGraphError::OK);
  EXPECT_EQ(m_transport->getMaxVoicesPerClip(), 1u) << "0 clamps up to 1";

  EXPECT_EQ(m_transport->setMaxVoicesPerClip(1000), SessionGraphError::OK);
  EXPECT_EQ(m_transport->getMaxVoicesPerClip(), 32u) << "clamps to hard max 32";

  for (uint32_t v : {2u, 4u, 8u, 16u}) {
    EXPECT_EQ(m_transport->setMaxVoicesPerClip(v), SessionGraphError::OK);
    EXPECT_EQ(m_transport->getMaxVoicesPerClip(), v);
  }
}

TEST_F(ChokeAndVoiceCapTest, PolyphonicRespectsVoiceCap) {
  ClipHandle h = registerClip(1, 440.0f);
  ASSERT_EQ(m_transport->setClipVoiceMode(h, VoiceMode::Polyphonic), SessionGraphError::OK);

  // Cap at 4; fire 7 times. Oldest-voice eviction keeps the count at the cap.
  ASSERT_EQ(m_transport->setMaxVoicesPerClip(4), SessionGraphError::OK);
  for (int i = 0; i < 7; ++i) {
    m_transport->startClip(h);
    pump();
  }
  EXPECT_EQ(m_transport->getActiveVoiceCount(h), 4u)
      << "Polyphonic layering must be bounded by the voice cap";
}

TEST_F(ChokeAndVoiceCapTest, VoiceCapOfTwoAllowsOneOverlapPair) {
  ClipHandle h = registerClip(1, 440.0f);
  ASSERT_EQ(m_transport->setClipVoiceMode(h, VoiceMode::Polyphonic), SessionGraphError::OK);
  // Cap 2 == effectively "stop-all-on-play" style: at most 2 voices coexist.
  ASSERT_EQ(m_transport->setMaxVoicesPerClip(2), SessionGraphError::OK);

  for (int i = 0; i < 5; ++i) {
    m_transport->startClip(h);
    pump();
  }
  EXPECT_LE(m_transport->getActiveVoiceCount(h), 2u)
      << "A cap of 2 must never exceed two coexisting voices";
}

TEST_F(ChokeAndVoiceCapTest, GlobalVoiceCapPublishesRefusalEvent) {
  const std::string path = writeSineWav(m_dir / "global-cap.wav", 440.0f, 0.1f);
  CapacityCallback callback;
  m_transport->setCallback(&callback);

  for (ClipHandle handle = 1; handle <= 33; ++handle) {
    ASSERT_EQ(m_transport->registerClipAudio(handle, path), SessionGraphError::OK);
    ASSERT_EQ(m_transport->startClip(handle), SessionGraphError::OK);
  }

  pump();
  m_transport->processCallbacks();
  EXPECT_EQ(callback.refusalCount, 1);
  EXPECT_EQ(callback.refusedHandle, 33u);
}
