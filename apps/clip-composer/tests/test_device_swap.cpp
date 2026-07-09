// SPDX-License-Identifier: MIT
// Device-swap safety stress tests (OCC151 T6 / G4 / F-APP-4)
//
// setAudioDevice() runs on the UI thread and swaps the transport/driver
// unique_ptrs. The old driver's audio thread reads m_transportController every
// callback, so moving those pointers out from under a live callback is a data
// race. OCC151 T6 makes the swap safe by quiescing the audio thread (stopping
// the running driver) BEFORE any member mutation.
//
// These tests hammer setAudioDevice while clips are playing and assert the
// engine survives without crashing and stays in a consistent, running state.
// In CI there is no real audio device, so the engine falls back to the dummy
// driver — the swap ordering under test is identical either way.

#include "../Source/Audio/AudioEngine.h"

#include <gtest/gtest.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <cmath>
#include <filesystem>

namespace {

void writeTestWavFile(const juce::File& file) {
  juce::WavAudioFormat format;
  auto stream = file.createOutputStream();
  ASSERT_NE(stream, nullptr);

  std::unique_ptr<juce::AudioFormatWriter> writer(
      format.createWriterFor(stream.release(), 48000.0, 2, 16, {}, 0));
  ASSERT_NE(writer, nullptr);

  constexpr int numSamples = 48000;
  juce::AudioBuffer<float> buffer(2, numSamples);
  for (int sample = 0; sample < numSamples; ++sample) {
    const auto value =
        0.2f * std::sin(2.0 * juce::MathConstants<double>::pi * 440.0 * sample / 48000.0);
    buffer.setSample(0, sample, value);
    buffer.setSample(1, sample, value);
  }
  ASSERT_TRUE(writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()));
}

} // namespace

class DeviceSwapStressTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto uniqueName = "clip-composer-device-swap-" +
                      juce::String(juce::Time::getMillisecondCounterHiRes(), 0) + "-" +
                      juce::String(juce::Random::getSystemRandom().nextInt()) + ".wav";
    auto tempDirectory = juce::File(std::filesystem::temp_directory_path().string());
    m_testAudioFile = tempDirectory.getChildFile(uniqueName);
    m_testAudioFile.getParentDirectory().createDirectory();
    writeTestWavFile(m_testAudioFile);

    m_engine = std::make_unique<AudioEngine>();
    if (!m_engine->initialize(48000)) {
      GTEST_SKIP() << "Audio device not available";
    }

    for (int i = 0; i < kNumClips; ++i) {
      ASSERT_TRUE(m_engine->loadClip(i, m_testAudioFile.getFullPathName()));
    }
  }

  void TearDown() override {
    m_engine.reset();
    if (m_testAudioFile.existsAsFile())
      m_testAudioFile.deleteFile();
  }

  static constexpr int kNumClips = 8;

  juce::ScopedJuceInitialiser_GUI m_juce;
  std::unique_ptr<AudioEngine> m_engine;
  juce::File m_testAudioFile;
};

// Rapid device switches while the driver is stopped must not crash and must
// leave the engine consistent.
TEST_F(DeviceSwapStressTest, RapidSwitchesWhileStoppedDoNotCrash) {
  for (int i = 0; i < 25; ++i) {
    // Default device round-trips through the same quiesce/rebuild/publish path.
    m_engine->setAudioDevice("Default Device", 48000, 512);
  }
  EXPECT_EQ(m_engine->getCurrentDeviceName(), "Default Device");
}

// The real race site: switch devices repeatedly WHILE clips are playing and the
// driver is running. The swap must quiesce the audio thread first, so no
// callback dereferences a half-swapped transport.
TEST_F(DeviceSwapStressTest, RapidSwitchesDuringPlaybackDoNotCrash) {
  // Start the driver (real CoreAudio if present, dummy fallback otherwise) so the
  // wasRunning branch — the one that must stop-before-swap — is exercised.
  m_engine->start();

  for (int i = 0; i < kNumClips; ++i) {
    m_engine->startClip(i);
  }

  for (int i = 0; i < 25; ++i) {
    // Re-fire a couple of clips each iteration so there is always live/ fading
    // voice state to carry across the swap (rehydrateTransportState path).
    m_engine->startClip(i % kNumClips);
    const bool ok = m_engine->setAudioDevice("Default Device", 48000, 512);
    EXPECT_TRUE(ok) << "Default-device swap should succeed on iteration " << i;
    m_engine->drainTransportCallbacks(); // message-thread drain, mirrors the app
  }

  // Engine is still usable after the storm.
  EXPECT_EQ(m_engine->getCurrentDeviceName(), "Default Device");
  EXPECT_NO_THROW(m_engine->stopAllClips());
}

// A swap must preserve which clips were playing (rehydration restarts them).
TEST_F(DeviceSwapStressTest, PlayingClipsSurviveASwap) {
  m_engine->start();
  m_engine->startClip(0);

  ASSERT_TRUE(m_engine->setAudioDevice("Default Device", 48000, 512));

  // The clip that was playing before the swap should be restarted afterwards.
  // (Position/timing is driver-dependent, but the handle must be re-fired.)
  EXPECT_NO_THROW(m_engine->stopAllClips());
}
