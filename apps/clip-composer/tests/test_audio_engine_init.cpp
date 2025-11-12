// SPDX-License-Identifier: MIT
// AudioEngine Initialization and Shutdown Tests (Sprint A4)

#include "../Source/Audio/AudioEngine.h"
#include <gtest/gtest.h>

/**
 * Test Suite: AudioEngine Initialization
 *
 * Tests basic engine lifecycle: initialize, start, stop, shutdown
 * Verifies resource management and state transitions
 */

TEST(AudioEngineInitTest, ConstructorDoesNotCrash) {
  // Test that AudioEngine can be constructed without crashing
  AudioEngine engine;
  EXPECT_FALSE(engine.isRunning());
}

TEST(AudioEngineInitTest, InitializeWithDefaultSampleRate) {
  AudioEngine engine;
  bool success = engine.initialize(48000);

  // Note: May fail if no audio device available (headless CI)
  if (success) {
    EXPECT_EQ(engine.getSampleRate(), 48000u);
    EXPECT_FALSE(engine.isRunning()); // Not started yet
  }
}

TEST(AudioEngineInitTest, InitializeWith44100SampleRate) {
  AudioEngine engine;
  bool success = engine.initialize(44100);

  if (success) {
    EXPECT_EQ(engine.getSampleRate(), 44100u);
  }
}

TEST(AudioEngineInitTest, StartAndStopEngine) {
  AudioEngine engine;
  if (!engine.initialize(48000)) {
    GTEST_SKIP() << "Audio device not available (headless CI?)";
  }

  bool started = engine.start();
  EXPECT_TRUE(started);
  EXPECT_TRUE(engine.isRunning());

  engine.stop();
  EXPECT_FALSE(engine.isRunning());
}

TEST(AudioEngineInitTest, MultipleStartStopCycles) {
  AudioEngine engine;
  if (!engine.initialize(48000)) {
    GTEST_SKIP() << "Audio device not available";
  }

  // Test multiple start/stop cycles (stress test)
  for (int i = 0; i < 5; ++i) {
    EXPECT_TRUE(engine.start());
    EXPECT_TRUE(engine.isRunning());

    engine.stop();
    EXPECT_FALSE(engine.isRunning());
  }
}

TEST(AudioEngineInitTest, GetBufferSizeAfterInitialization) {
  AudioEngine engine;
  if (!engine.initialize(48000)) {
    GTEST_SKIP() << "Audio device not available";
  }

  uint32_t bufferSize = engine.getBufferSize();
  // Buffer size should be reasonable (64-2048 samples)
  EXPECT_GE(bufferSize, 64u);
  EXPECT_LE(bufferSize, 2048u);
}

TEST(AudioEngineInitTest, GetLatencySamplesAfterInitialization) {
  AudioEngine engine;
  if (!engine.initialize(48000)) {
    GTEST_SKIP() << "Audio device not available";
  }

  uint32_t latency = engine.getLatencySamples();
  // Latency should be non-zero and reasonable (<10000 samples = ~200ms @ 48kHz)
  EXPECT_GT(latency, 0u);
  EXPECT_LT(latency, 10000u);
}

TEST(AudioEngineInitTest, CleanShutdownWithoutCrash) {
  AudioEngine engine;
  if (engine.initialize(48000)) {
    engine.start();
    engine.stop();
  }
  // Destructor should clean up without crashing
}
