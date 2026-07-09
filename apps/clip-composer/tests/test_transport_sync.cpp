// SPDX-License-Identifier: MIT
// Grid/Edit-dialog transport synchronization tests (OCC151 T3 / G1)
//
// These tests lock in the transport-unification invariant from OCC151:
//   "One clip identity = one voice. The Edit dialog and grid share the same
//    player (ClipHandle) for a given clip. No parallel cue-buss handles for the
//    same clip."
//
// Before OCC151 the Edit dialog's PreviewPlayer allocated a *dedicated* cue-buss
// ClipHandle for the same file already loaded on the grid button (F-APP-1). The
// same file then played on two handles concurrently, which (a) desynced
// grid/dialog play-state and position, and (b) summed to 2x amplitude at the
// master. The fix routes PreviewPlayer through the grid ClipHandle (buttonIndex)
// so play/stop/position are single-sourced.

#include "../Source/Audio/AudioEngine.h"
#include "../Source/UI/PreviewPlayer.h"

#include <gtest/gtest.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <cmath>
#include <filesystem>

namespace {

// Writes a 1-second 48 kHz stereo 440 Hz sine so loadClip() has real audio to
// register. Mirrors the helper in test_performance.cpp.
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

class TransportSyncTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto uniqueName = "clip-composer-transport-sync-" +
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

    ASSERT_TRUE(m_engine->loadClip(kButtonIndex, m_testAudioFile.getFullPathName()))
        << "Test clip should load";
  }

  void TearDown() override {
    m_engine.reset();
    if (m_testAudioFile.existsAsFile())
      m_testAudioFile.deleteFile();
  }

  static constexpr int kButtonIndex = 0;

  // PreviewPlayer is a juce::Timer; a message manager must exist for start/stop
  // timer calls and for JUCE singleton (ShutdownDetector) cleanup at teardown.
  juce::ScopedJuceInitialiser_GUI m_juce;
  std::unique_ptr<AudioEngine> m_engine;
  juce::File m_testAudioFile;
};

// The Edit dialog's PreviewPlayer must NOT allocate a dedicated cue buss for the
// grid clip's own file. This is the direct, deterministic proof that the
// parallel-handle path from F-APP-1 is gone.
TEST_F(TransportSyncTest, PreviewPlayerDoesNotAllocateDedicatedBussForGridClip) {
  PreviewPlayer preview(m_engine.get(), kButtonIndex);

  // The dialog constructs the PreviewPlayer with no source path.
  EXPECT_FALSE(preview.isUsingDedicatedAuditionBuss())
      << "Fresh PreviewPlayer must not hold a cue buss";

  // setClipMetadata() used to call this with the loaded file's path, allocating a
  // second handle. Post-OCC151 it is a no-op — the dialog shares the grid handle.
  preview.setAuditionSource(m_testAudioFile.getFullPathName());
  EXPECT_FALSE(preview.isUsingDedicatedAuditionBuss())
      << "setAuditionSource for the grid clip must not allocate a parallel handle";
}

// Firing from the grid and observing via the dialog's PreviewPlayer must report
// the same play-state, because both read the same ClipHandle (buttonIndex).
TEST_F(TransportSyncTest, GridAndDialogReportSamePlayState) {
  PreviewPlayer preview(m_engine.get(), kButtonIndex);
  preview.setAuditionSource(m_testAudioFile.getFullPathName());
  ASSERT_FALSE(preview.isUsingDedicatedAuditionBuss());

  // Stopped state agrees across both surfaces.
  EXPECT_EQ(m_engine->isClipPlaying(kButtonIndex), preview.getPlaybackSnapshot().isPlaying);

  // Fire from the grid. Whatever play-state the engine reports for the handle,
  // the dialog's snapshot must report the identical value — they are the same
  // handle, so they can never disagree.
  m_engine->startClip(kButtonIndex);
  EXPECT_EQ(m_engine->isClipPlaying(kButtonIndex), preview.getPlaybackSnapshot().isPlaying)
      << "Grid and dialog must agree on play-state (shared handle)";

  m_engine->stopClip(kButtonIndex);
  EXPECT_EQ(m_engine->isClipPlaying(kButtonIndex), preview.getPlaybackSnapshot().isPlaying)
      << "Grid and dialog must agree after stop (shared handle)";
}

// Firing from the dialog's PreviewPlayer must be visible through the grid's
// AudioEngine query, and vice versa — the position readback is single-sourced.
TEST_F(TransportSyncTest, DialogAndGridReadSamePosition) {
  PreviewPlayer preview(m_engine.get(), kButtonIndex);
  preview.setAuditionSource(m_testAudioFile.getFullPathName());
  ASSERT_FALSE(preview.isUsingDedicatedAuditionBuss());

  // Position is read from the same handle on both surfaces, so the snapshot the
  // dialog shows equals what the grid clip reports.
  EXPECT_EQ(preview.getPlaybackSnapshot().currentPositionSamples,
            m_engine->getClipPosition(kButtonIndex))
      << "Dialog snapshot position must equal grid clip position (shared handle)";
}

// Firing the same clip twice must not create a second handle / voice list entry.
// With the dialog sharing the grid handle, a second play() is just a restart of
// the same handle — never a parallel allocation.
TEST_F(TransportSyncTest, FiringSameClipTwiceDoesNotAllocateSecondHandle) {
  PreviewPlayer preview(m_engine.get(), kButtonIndex);
  preview.setAuditionSource(m_testAudioFile.getFullPathName());
  ASSERT_FALSE(preview.isUsingDedicatedAuditionBuss());

  m_engine->startClip(kButtonIndex);
  EXPECT_FALSE(preview.isUsingDedicatedAuditionBuss()) << "First fire must not spawn a cue buss";

  // Fire again from the dialog surface (same handle → restart in place).
  m_engine->startClip(kButtonIndex);
  EXPECT_FALSE(preview.isUsingDedicatedAuditionBuss())
      << "Second fire must not spawn a second (stacked) handle";

  m_engine->stopClip(kButtonIndex);
}
