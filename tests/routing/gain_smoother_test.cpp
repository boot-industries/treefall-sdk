// SPDX-License-Identifier: MIT
#include "../../src/core/routing/gain_smoother.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <gtest/gtest.h>
#include <thread>

using namespace orpheus;

class GainSmootherTest : public ::testing::Test {
protected:
  static constexpr uint32_t SAMPLE_RATE = 48000;
  static constexpr float TOLERANCE = 0.0001f; // 0.01% tolerance for float comparison
};

// ============================================================================
// Basic Functionality Tests
// ============================================================================

TEST_F(GainSmootherTest, InitialState) {
  GainSmoother smoother(SAMPLE_RATE, 10.0f);

  EXPECT_FLOAT_EQ(smoother.getCurrent(), 1.0f);
  EXPECT_FLOAT_EQ(smoother.getTarget(), 1.0f);
  EXPECT_FALSE(smoother.isRamping());
}

TEST_F(GainSmootherTest, SetTargetUpdatesTarget) {
  GainSmoother smoother(SAMPLE_RATE, 10.0f);

  smoother.setTarget(0.5f);

  // Give pending target time to be picked up
  smoother.process();

  EXPECT_FLOAT_EQ(smoother.getTarget(), 0.5f);
  EXPECT_TRUE(smoother.isRamping());
}

TEST_F(GainSmootherTest, ResetChangesImmediately) {
  GainSmoother smoother(SAMPLE_RATE, 10.0f);

  smoother.reset(0.25f);

  EXPECT_FLOAT_EQ(smoother.getCurrent(), 0.25f);
  EXPECT_FLOAT_EQ(smoother.getTarget(), 0.25f);
  EXPECT_FALSE(smoother.isRamping());
}

// ============================================================================
// Linear Ramping Tests
// ============================================================================

TEST_F(GainSmootherTest, LinearRampUp) {
  // 10ms smoothing = 480 samples @ 48kHz
  GainSmoother smoother(SAMPLE_RATE, 10.0f);

  smoother.reset(0.0f);
  smoother.setTarget(1.0f);

  // Process samples and verify linear ramp
  float prev_gain = smoother.process();
  EXPECT_FLOAT_EQ(prev_gain, 0.0f);

  for (int i = 1; i < 480; ++i) {
    float gain = smoother.process();

    // Verify increasing
    EXPECT_GT(gain, prev_gain);

    // Verify linear increment (approximately 1.0 / 480 per sample)
    float expected_increment = 1.0f / 480.0f;
    float actual_increment = gain - prev_gain;
    EXPECT_NEAR(actual_increment, expected_increment, TOLERANCE);

    prev_gain = gain;
  }

  // Final sample should reach exactly 1.0 (no overshoot)
  float final_gain = smoother.process();
  EXPECT_FLOAT_EQ(final_gain, 1.0f);
  EXPECT_FALSE(smoother.isRamping());
}

TEST_F(GainSmootherTest, LinearRampDown) {
  // 10ms smoothing = 480 samples @ 48kHz
  GainSmoother smoother(SAMPLE_RATE, 10.0f);

  smoother.reset(1.0f);
  smoother.setTarget(0.0f);

  // Process samples and verify linear ramp
  float prev_gain = smoother.process();
  EXPECT_FLOAT_EQ(prev_gain, 1.0f);

  for (int i = 1; i < 480; ++i) {
    float gain = smoother.process();

    // Verify decreasing
    EXPECT_LT(gain, prev_gain);

    // Verify linear decrement
    float expected_decrement = -1.0f / 480.0f;
    float actual_decrement = gain - prev_gain;
    EXPECT_NEAR(actual_decrement, expected_decrement, TOLERANCE);

    prev_gain = gain;
  }

  // Final sample should reach exactly 0.0 (no overshoot)
  float final_gain = smoother.process();
  EXPECT_FLOAT_EQ(final_gain, 0.0f);
  EXPECT_FALSE(smoother.isRamping());
}

TEST_F(GainSmootherTest, NoOvershoot) {
  GainSmoother smoother(SAMPLE_RATE, 10.0f);

  smoother.reset(0.0f);
  smoother.setTarget(0.5f);

  // Process more samples than needed (should clamp at target)
  for (int i = 0; i < 1000; ++i) {
    float gain = smoother.process();

    // Should never exceed target
    EXPECT_LE(gain, 0.5f);
  }

  // Should be exactly at target
  EXPECT_FLOAT_EQ(smoother.getCurrent(), 0.5f);
  EXPECT_FALSE(smoother.isRamping());
}

// ============================================================================
// Smoothing Time Tests
// ============================================================================

TEST_F(GainSmootherTest, ConfigurableSmoothingTime1ms) {
  // 1ms @ 48kHz = 48 samples
  GainSmoother smoother(SAMPLE_RATE, 1.0f);

  smoother.reset(0.0f);
  smoother.setTarget(1.0f);

  // Process samples
  for (int i = 0; i < 48; ++i) {
    smoother.process();
  }

  // Should reach target in approximately 48 samples
  EXPECT_NEAR(smoother.getCurrent(), 1.0f, 0.1f);
}

TEST_F(GainSmootherTest, ConfigurableSmoothingTime10ms) {
  // 10ms @ 48kHz = 480 samples
  GainSmoother smoother(SAMPLE_RATE, 10.0f);

  smoother.reset(0.0f);
  smoother.setTarget(1.0f);

  // Process samples
  for (int i = 0; i < 480; ++i) {
    smoother.process();
  }

  // Should reach target in exactly 480 samples
  EXPECT_FLOAT_EQ(smoother.getCurrent(), 1.0f);
}

TEST_F(GainSmootherTest, ConfigurableSmoothingTime100ms) {
  // 100ms @ 48kHz = 4800 samples
  GainSmoother smoother(SAMPLE_RATE, 100.0f);

  smoother.reset(0.0f);
  smoother.setTarget(1.0f);

  // Process samples (need 4801 calls with "return before increment" semantics)
  for (int i = 0; i <= 4800; ++i) {
    smoother.process();
  }

  // Should reach target after 4801 calls
  EXPECT_FLOAT_EQ(smoother.getCurrent(), 1.0f);
}

// ============================================================================
// Target Update Tests
// ============================================================================

TEST_F(GainSmootherTest, MultipleTargetUpdates) {
  GainSmoother smoother(SAMPLE_RATE, 10.0f);

  smoother.reset(0.0f);

  // Update target multiple times before processing
  smoother.setTarget(0.3f);
  smoother.setTarget(0.5f);
  smoother.setTarget(0.7f);

  // Process should use latest target (0.7)
  smoother.process();
  EXPECT_FLOAT_EQ(smoother.getTarget(), 0.7f);
}

TEST_F(GainSmootherTest, TargetUpdateDuringRamp) {
  GainSmoother smoother(SAMPLE_RATE, 10.0f);

  smoother.reset(0.0f);
  smoother.setTarget(1.0f);

  // Ramp halfway (240 samples)
  for (int i = 0; i < 240; ++i) {
    smoother.process();
  }

  float halfway_gain = smoother.getCurrent();
  EXPECT_NEAR(halfway_gain, 0.5f, 0.1f);

  // Change target mid-ramp
  smoother.setTarget(0.25f);
  smoother.process(); // Pick up new target

  // Should now ramp down toward 0.25
  for (int i = 0; i < 100; ++i) {
    smoother.process();
  }

  EXPECT_LT(smoother.getCurrent(), halfway_gain);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(GainSmootherTest, TargetEqualsCurrentNoRamp) {
  GainSmoother smoother(SAMPLE_RATE, 10.0f);

  smoother.reset(0.5f);
  smoother.setTarget(0.5f);

  // Pending target is set, so isRamping() returns true initially
  EXPECT_TRUE(smoother.isRamping());

  // Process once to pick up the pending target
  float gain = smoother.process();
  EXPECT_FLOAT_EQ(gain, 0.5f);

  // After processing, current == target, so no longer ramping
  EXPECT_FALSE(smoother.isRamping());
}

TEST_F(GainSmootherTest, ClampToZeroAndOne) {
  GainSmoother smoother(SAMPLE_RATE, 10.0f);

  // Test clamping to valid range [0.0, 1.0]
  smoother.setTarget(-0.5f); // Should clamp to 0.0
  smoother.process();
  EXPECT_GE(smoother.getTarget(), 0.0f);

  smoother.setTarget(1.5f); // Should clamp to 1.0
  smoother.process();
  EXPECT_LE(smoother.getTarget(), 1.0f);
}

TEST_F(GainSmootherTest, VerySmallIncrement) {
  // Large smoothing time = very small increment per sample
  GainSmoother smoother(SAMPLE_RATE, 100.0f);

  smoother.reset(0.0f);
  smoother.setTarget(1.0f);

  // Increment should be 1.0 / 4800 ≈ 0.00020833
  float gain1 = smoother.process();
  float gain2 = smoother.process();

  float increment = gain2 - gain1;
  EXPECT_NEAR(increment, 1.0f / 4800.0f, TOLERANCE);
}

// ============================================================================
// Thread Safety Tests (Lock-Free)
// ============================================================================

TEST_F(GainSmootherTest, ConcurrentTargetUpdates) {
  GainSmoother smoother(SAMPLE_RATE, 10.0f);
  std::atomic<bool> running{true};

  // UI thread: Rapidly update target
  std::thread ui_thread([&]() {
    for (int i = 0; i < 1000; ++i) {
      float target = (i % 100) / 100.0f;
      smoother.setTarget(target);
      std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
    running.store(false);
  });

  // Audio thread: Process samples
  std::thread audio_thread([&]() {
    while (running.load()) {
      smoother.process();
    }
  });

  ui_thread.join();
  audio_thread.join();

  // Should complete without deadlock or crash
  EXPECT_TRUE(true);
}

TEST_F(GainSmootherTest, ConcurrentGetTarget) {
  GainSmoother smoother(SAMPLE_RATE, 10.0f);
  std::atomic<bool> running{true};

  // UI thread: Update target
  std::thread ui_thread([&]() {
    for (int i = 0; i < 100; ++i) {
      smoother.setTarget(0.5f);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    running.store(false);
  });

  // Another thread: Read target
  std::thread reader_thread([&]() {
    while (running.load()) {
      float target = smoother.getTarget();
      (void)target; // Suppress unused warning
    }
  });

  // Audio thread: Process
  std::thread audio_thread([&]() {
    while (running.load()) {
      smoother.process();
    }
  });

  ui_thread.join();
  reader_thread.join();
  audio_thread.join();

  // Should complete without race conditions
  EXPECT_TRUE(true);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(GainSmootherTest, ProcessingPerformance) {
  GainSmoother smoother(SAMPLE_RATE, 10.0f);

  smoother.reset(0.0f);
  smoother.setTarget(1.0f);

  // Time processing 1 million samples
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < 1'000'000; ++i) {
    smoother.process();
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  // 1 million samples @ 48kHz = 20.8 seconds of audio
  // Should process in < 100ms on modern hardware
  std::cout << "[Gain Smoother] Processed 1M samples in " << duration.count() << " µs\n";
  EXPECT_LT(duration.count(), 100'000); // < 100ms
}

// ============================================================================
// Accuracy Tests
// ============================================================================

TEST_F(GainSmootherTest, RampAccuracyOver100Steps) {
  GainSmoother smoother(SAMPLE_RATE, 10.0f);

  smoother.reset(0.0f);
  smoother.setTarget(1.0f);

  // Sample at 100 points along the ramp
  for (int i = 0; i < 100; ++i) {
    // Process 4.8 samples between measurements
    for (int j = 0; j < 4; ++j) {
      smoother.process();
    }

    float gain = smoother.process();          // 5th sample is measurement point
    float expected = (i + 1) * 5.0f / 480.0f; // Expected progress
    expected = std::min(expected, 1.0f);      // Clamp at target

    EXPECT_NEAR(gain, expected, 0.01f) << "At step " << i;
  }
}

TEST_F(GainSmootherTest, SymmetricRampUpDown) {
  GainSmoother smoother(SAMPLE_RATE, 10.0f);

  // Ramp up
  smoother.reset(0.0f);
  smoother.setTarget(1.0f);

  std::vector<float> ramp_up_values;
  for (int i = 0; i < 480; ++i) {
    ramp_up_values.push_back(smoother.process());
  }

  // Ramp down
  smoother.reset(1.0f);
  smoother.setTarget(0.0f);

  std::vector<float> ramp_down_values;
  for (int i = 0; i < 480; ++i) {
    ramp_down_values.push_back(smoother.process());
  }

  // Ramp down should be symmetric (inverted) to ramp up
  for (size_t i = 0; i < ramp_up_values.size(); ++i) {
    float expected = 1.0f - ramp_up_values[i];
    EXPECT_NEAR(ramp_down_values[i], expected, TOLERANCE) << "At sample " << i;
  }
}

// ============================================================================
// Integration Tests (Real-World Scenarios)
// ============================================================================

TEST_F(GainSmootherTest, FadeOutScenario) {
  // Simulate fade-out during clip stop (like transport controller)
  GainSmoother smoother(SAMPLE_RATE, 10.0f);

  smoother.reset(1.0f);     // Playing at full volume
  smoother.setTarget(0.0f); // Fade out

  // Process 10ms (480 samples)
  for (int i = 0; i < 480; ++i) {
    float gain = smoother.process();
    // Verify smooth decrease
    EXPECT_LE(gain, 1.0f);
    EXPECT_GE(gain, 0.0f);
  }

  // Should be silent after 10ms
  EXPECT_FLOAT_EQ(smoother.getCurrent(), 0.0f);
}

TEST_F(GainSmootherTest, ChannelFaderMovement) {
  // Simulate user moving a fader in UI
  GainSmoother smoother(SAMPLE_RATE, 10.0f);

  // Start at unity (0dB)
  smoother.reset(1.0f);

  // User drags fader to -6dB (0.5 linear)
  smoother.setTarget(0.5f);

  // Process until reached
  while (smoother.isRamping()) {
    smoother.process();
  }
  EXPECT_FLOAT_EQ(smoother.getCurrent(), 0.5f);

  // User drags fader to -12dB (0.25 linear)
  smoother.setTarget(0.25f);

  while (smoother.isRamping()) {
    smoother.process();
  }
  EXPECT_FLOAT_EQ(smoother.getCurrent(), 0.25f);
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
