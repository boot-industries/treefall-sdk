// SPDX-License-Identifier: MIT
// Sample-rate mismatch handling tests (OCC151 T7 / G5)
//
// ORP127 G6 gave the SDK a deterministic polyphase ResamplingAudioFileReader,
// and registerClipAudio wraps mismatched-rate files in it automatically, so they
// play at correct pitch and the transport reports positions in the ENGINE-rate
// timeline. OCC151 T7 makes OCC present the clip's UI metadata in that same
// engine-rate timeline so the Edit dialog's duration / trim / fade math stays
// sample-accurate against playback — no more native-vs-engine-frame skew, and no
// more misleading "audio will sound distorted" warning.

#include "../Source/Audio/AudioEngine.h"

#include <gtest/gtest.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <cmath>
#include <filesystem>

namespace {

// Writes a 1-second stereo 440 Hz sine at an arbitrary sample rate.
void writeSineWav(const juce::File& file, double sampleRate) {
  juce::WavAudioFormat format;
  auto stream = file.createOutputStream();
  ASSERT_NE(stream, nullptr);

  std::unique_ptr<juce::AudioFormatWriter> writer(
      format.createWriterFor(stream.release(), sampleRate, 2, 16, {}, 0));
  ASSERT_NE(writer, nullptr);

  const int numSamples = static_cast<int>(sampleRate); // 1 second
  juce::AudioBuffer<float> buffer(2, numSamples);
  for (int sample = 0; sample < numSamples; ++sample) {
    const auto value =
        0.2f * std::sin(2.0 * juce::MathConstants<double>::pi * 440.0 * sample / sampleRate);
    buffer.setSample(0, sample, value);
    buffer.setSample(1, sample, value);
  }
  ASSERT_TRUE(writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()));
}

} // namespace

class SampleRateTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto tempDirectory = juce::File(std::filesystem::temp_directory_path().string());
    auto suffix = juce::String(juce::Time::getMillisecondCounterHiRes(), 0) + "-" +
                  juce::String(juce::Random::getSystemRandom().nextInt());
    m_file441 = tempDirectory.getChildFile("occ-sr-441-" + suffix + ".wav");
    m_file48 = tempDirectory.getChildFile("occ-sr-48-" + suffix + ".wav");
    writeSineWav(m_file441, 44100.0);
    writeSineWav(m_file48, 48000.0);

    m_engine = std::make_unique<AudioEngine>();
    if (!m_engine->initialize(48000)) {
      GTEST_SKIP() << "Audio device not available";
    }
  }

  void TearDown() override {
    m_engine.reset();
    if (m_file441.existsAsFile())
      m_file441.deleteFile();
    if (m_file48.existsAsFile())
      m_file48.deleteFile();
  }

  std::unique_ptr<AudioEngine> m_engine;
  juce::File m_file441;
  juce::File m_file48;
};

// A matched-rate (48 kHz) file loads with its native metadata unchanged.
TEST_F(SampleRateTest, MatchedRateMetadataIsUnchanged) {
  ASSERT_TRUE(m_engine->loadClip(0, m_file48.getFullPathName()));
  auto meta = m_engine->getClipMetadata(0);
  ASSERT_TRUE(meta.has_value());
  EXPECT_EQ(meta->sample_rate, 48000u);
  EXPECT_EQ(meta->duration_samples, 48000); // 1 second at 48 kHz
}

// A 44.1 kHz file loaded in a 48 kHz engine must present its UI metadata in the
// engine-rate timeline (so trims/fades line up with the resampled playback).
TEST_F(SampleRateTest, MismatchedRateMetadataIsPresentedAtEngineRate) {
  ASSERT_TRUE(m_engine->loadClip(0, m_file441.getFullPathName()));
  auto meta = m_engine->getClipMetadata(0);
  ASSERT_TRUE(meta.has_value());

  // Reported rate is the engine rate, not the native 44.1 kHz.
  EXPECT_EQ(meta->sample_rate, 48000u)
      << "UI metadata must be in the engine-rate timeline for resampled clips";

  // Duration scales by 48000/44100. 1 s of 44.1 kHz audio -> 48000 engine frames.
  // Allow a tiny tolerance for the integer conversion of the source frame count.
  const int64_t expected = 48000;
  EXPECT_NEAR(static_cast<double>(meta->duration_samples), static_cast<double>(expected), 2.0)
      << "Engine-rate duration must equal native_duration * engineRate/nativeRate";
}

// The engine-rate duration must be strictly longer than the native frame count
// when upsampling (44.1 -> 48), proving the conversion actually happened.
TEST_F(SampleRateTest, UpsampledDurationExceedsNativeFrameCount) {
  ASSERT_TRUE(m_engine->loadClip(0, m_file441.getFullPathName()));
  auto meta = m_engine->getClipMetadata(0);
  ASSERT_TRUE(meta.has_value());
  EXPECT_GT(meta->duration_samples, 44100)
      << "48 kHz engine frames for a 1 s 44.1 kHz file must exceed 44100";
}
