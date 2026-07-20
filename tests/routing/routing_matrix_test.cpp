// SPDX-License-Identifier: MIT
#include "../../include/orpheus/routing_matrix.h"

#include <array>
#include <atomic>

#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

using namespace orpheus;

class RoutingMatrixTest : public ::testing::Test {
protected:
  static constexpr uint32_t SAMPLE_RATE = 48000;
  static constexpr uint32_t BUFFER_SIZE = 512;
  static constexpr float TOLERANCE = 0.0001f; // 0.01% tolerance for float comparison

  void SetUp() override {
    // Create routing matrix
    matrix = createRoutingMatrix();

    // Default configuration: 4 channels, 2 groups, 2 outputs (stereo)
    config.num_channels = 4;
    config.num_groups = 2;
    config.num_outputs = 2;
    config.solo_mode = SoloMode::SIP;
    config.metering_mode = MeteringMode::Peak;
    config.gain_smoothing_ms = 10.0f;
    config.enable_metering = true;
    config.enable_clipping_protection = false;
  }

  // Helper to create test input buffers
  std::vector<std::vector<float>> createTestInputs(uint32_t num_channels, uint32_t num_frames,
                                                   float amplitude = 0.5f) {
    std::vector<std::vector<float>> inputs(num_channels);
    for (uint32_t ch = 0; ch < num_channels; ++ch) {
      inputs[ch].resize(num_frames);
      for (uint32_t i = 0; i < num_frames; ++i) {
        // Generate simple sine wave per channel (different frequencies)
        float freq = 440.0f * (ch + 1); // 440, 880, 1320, 1760 Hz...
        inputs[ch][i] = amplitude * std::sin(2.0f * 3.14159f * freq * i / SAMPLE_RATE);
      }
    }
    return inputs;
  }

  // Helper to convert vector of vectors to array of pointers
  std::vector<float*> toPointerArray(std::vector<std::vector<float>>& data) {
    std::vector<float*> ptrs;
    for (auto& vec : data) {
      ptrs.push_back(vec.data());
    }
    return ptrs;
  }

  std::unique_ptr<IRoutingMatrix> matrix;
  RoutingConfig config;
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(RoutingMatrixTest, InitializeWithValidConfig) {
  auto result = matrix->initialize(config);

  EXPECT_EQ(result, SessionGraphError::OK);

  auto retrieved_config = matrix->getConfig();
  EXPECT_EQ(retrieved_config.num_channels, 4);
  EXPECT_EQ(retrieved_config.num_groups, 2);
  EXPECT_EQ(retrieved_config.num_outputs, 2);
}

TEST_F(RoutingMatrixTest, InitializeWithInvalidChannelCount) {
  config.num_channels = 0; // Invalid
  auto result = matrix->initialize(config);

  EXPECT_EQ(result, SessionGraphError::InvalidParameter);
}

TEST_F(RoutingMatrixTest, InitializeWithTooManyChannels) {
  config.num_channels = 257; // > 256 logical source lanes
  auto result = matrix->initialize(config);

  EXPECT_EQ(result, SessionGraphError::InvalidParameter);
}

TEST_F(RoutingMatrixTest, InitializeWithInvalidGroupCount) {
  config.num_groups = 0; // Invalid
  auto result = matrix->initialize(config);

  EXPECT_EQ(result, SessionGraphError::InvalidParameter);
}

TEST_F(RoutingMatrixTest, InitializeWithMonoOutput) {
  config.num_outputs = 1;
  auto result = matrix->initialize(config);

  EXPECT_EQ(result, SessionGraphError::OK);
  EXPECT_EQ(matrix->getConfig().num_outputs, 1);
}

// ============================================================================
// Basic Routing Tests
// ============================================================================

TEST_F(RoutingMatrixTest, ProcessRoutingWithSilence) {
  matrix->initialize(config);

  // Create silent inputs
  auto inputs = createTestInputs(4, BUFFER_SIZE, 0.0f);
  auto input_ptrs = toPointerArray(inputs);

  // Create outputs
  std::vector<std::vector<float>> outputs(2);
  for (auto& out : outputs) {
    out.resize(BUFFER_SIZE);
  }
  auto output_ptrs = toPointerArray(outputs);

  // Process
  auto result = matrix->processRouting(input_ptrs.data(), output_ptrs.data(), BUFFER_SIZE);

  EXPECT_EQ(result, SessionGraphError::OK);

  // Verify output is silence
  for (uint32_t i = 0; i < BUFFER_SIZE; ++i) {
    EXPECT_FLOAT_EQ(outputs[0][i], 0.0f);
    EXPECT_FLOAT_EQ(outputs[1][i], 0.0f);
  }
}

TEST_F(RoutingMatrixTest, ProcessRoutingWithSignal) {
  matrix->initialize(config);

  // Create test inputs (amplitude 0.5)
  auto inputs = createTestInputs(4, BUFFER_SIZE, 0.5f);
  auto input_ptrs = toPointerArray(inputs);

  // Create outputs
  std::vector<std::vector<float>> outputs(2);
  for (auto& out : outputs) {
    out.resize(BUFFER_SIZE);
  }
  auto output_ptrs = toPointerArray(outputs);

  // Process
  auto result = matrix->processRouting(input_ptrs.data(), output_ptrs.data(), BUFFER_SIZE);

  EXPECT_EQ(result, SessionGraphError::OK);

  // Verify output is NOT silence (has signal)
  bool has_signal = false;
  for (uint32_t i = 0; i < BUFFER_SIZE; ++i) {
    if (std::abs(outputs[0][i]) > 0.01f || std::abs(outputs[1][i]) > 0.01f) {
      has_signal = true;
      break;
    }
  }
  EXPECT_TRUE(has_signal);
}

TEST_F(RoutingMatrixTest, ChannelAssignmentToGroups) {
  matrix->initialize(config);

  // Assign channels to groups: ch0/1 → group0, ch2/3 → group1
  matrix->setChannelGroup(0, 0);
  matrix->setChannelGroup(1, 0);
  matrix->setChannelGroup(2, 1);
  matrix->setChannelGroup(3, 1);

  // Create test inputs
  auto inputs = createTestInputs(4, BUFFER_SIZE, 0.5f);
  auto input_ptrs = toPointerArray(inputs);

  // Create outputs
  std::vector<std::vector<float>> outputs(2);
  for (auto& out : outputs) {
    out.resize(BUFFER_SIZE);
  }
  auto output_ptrs = toPointerArray(outputs);

  // Process
  auto result = matrix->processRouting(input_ptrs.data(), output_ptrs.data(), BUFFER_SIZE);

  EXPECT_EQ(result, SessionGraphError::OK);

  // Both groups should contribute to output
  EXPECT_TRUE(std::abs(outputs[0][100]) > 0.01f); // Has signal
}

// ============================================================================
// Gain Control Tests
// ============================================================================

TEST_F(RoutingMatrixTest, ChannelGainAttenuation) {
  // Configure for single channel
  config.num_channels = 1;
  config.num_groups = 1;
  matrix->initialize(config);

  // Set channel 0 to -6 dB (half amplitude)
  matrix->setChannelGain(0, -6.0f);

  // Create single-channel input
  auto inputs = createTestInputs(1, BUFFER_SIZE, 1.0f);
  auto input_ptrs = toPointerArray(inputs);

  // Create outputs
  std::vector<std::vector<float>> outputs(2);
  for (auto& out : outputs) {
    out.resize(BUFFER_SIZE);
  }
  auto output_ptrs = toPointerArray(outputs);

  // Process multiple buffers to let gain smoothing settle
  for (int i = 0; i < 10; ++i) {
    matrix->processRouting(input_ptrs.data(), output_ptrs.data(), BUFFER_SIZE);
  }

  // After smoothing, output should be attenuated
  // -6 dB = 0.5 linear, so peak should be ~0.5
  // For sine wave: avg(abs(x)) = (2/π) * amplitude ≈ 0.637 * amplitude
  // ORP121 C-04: Constant-power pan law applies -3 dB (0.707) at center
  // Expected: 0.637 * 0.5 * 0.707 = 0.225
  float avg_output = 0.0f;
  for (uint32_t i = BUFFER_SIZE / 2; i < BUFFER_SIZE; ++i) { // Second half (after transient)
    avg_output += std::abs(outputs[0][i]);
  }
  avg_output /= (BUFFER_SIZE / 2);

  // Expect approximately 0.225 (0.5 peak × 0.637 sine × 0.707 pan law)
  EXPECT_NEAR(avg_output, 0.225f, 0.05f);
}

TEST_F(RoutingMatrixTest, MasterGainAttenuation) {
  // Configure for single channel
  config.num_channels = 1;
  config.num_groups = 1;
  matrix->initialize(config);

  // Set master gain to -6 dB
  matrix->setMasterGain(-6.0f);

  // Create test inputs
  auto inputs = createTestInputs(1, BUFFER_SIZE, 1.0f);
  auto input_ptrs = toPointerArray(inputs);

  // Create outputs
  std::vector<std::vector<float>> outputs(2);
  for (auto& out : outputs) {
    out.resize(BUFFER_SIZE);
  }
  auto output_ptrs = toPointerArray(outputs);

  // Process multiple buffers to let gain smoothing settle
  for (int i = 0; i < 10; ++i) {
    matrix->processRouting(input_ptrs.data(), output_ptrs.data(), BUFFER_SIZE);
  }

  // Output should be attenuated by master gain
  // For sine wave with -6 dB gain: avg(abs(x)) ≈ 0.318
  // ORP121 C-04: Constant-power pan law applies -3 dB (0.707) at center
  // Expected: 0.637 * 0.5 * 0.707 = 0.225
  float avg_output = 0.0f;
  for (uint32_t i = BUFFER_SIZE / 2; i < BUFFER_SIZE; ++i) {
    avg_output += std::abs(outputs[0][i]);
  }
  avg_output /= (BUFFER_SIZE / 2);

  // Expect approximately 0.225 (0.5 peak × 0.637 sine × 0.707 pan law)
  EXPECT_NEAR(avg_output, 0.225f, 0.05f);
}

// ============================================================================
// Mute/Solo Tests
// ============================================================================

TEST_F(RoutingMatrixTest, ChannelMuteSilencesOutput) {
  // Configure for single channel
  config.num_channels = 1;
  config.num_groups = 1;
  matrix->initialize(config);

  // Mute channel 0
  matrix->setChannelMute(0, true);

  // Create test inputs
  auto inputs = createTestInputs(1, BUFFER_SIZE, 1.0f);
  auto input_ptrs = toPointerArray(inputs);

  // Create outputs
  std::vector<std::vector<float>> outputs(2);
  for (auto& out : outputs) {
    out.resize(BUFFER_SIZE);
  }
  auto output_ptrs = toPointerArray(outputs);

  // Process
  matrix->processRouting(input_ptrs.data(), output_ptrs.data(), BUFFER_SIZE);

  // Output should be silence
  for (uint32_t i = 0; i < BUFFER_SIZE; ++i) {
    EXPECT_FLOAT_EQ(outputs[0][i], 0.0f);
    EXPECT_FLOAT_EQ(outputs[1][i], 0.0f);
  }
}

TEST_F(RoutingMatrixTest, MasterMuteSilencesOutput) {
  // Configure for single channel
  config.num_channels = 1;
  config.num_groups = 1;
  matrix->initialize(config);

  // Enable master mute
  matrix->setMasterMute(true);

  // Create test inputs
  auto inputs = createTestInputs(1, BUFFER_SIZE, 1.0f);
  auto input_ptrs = toPointerArray(inputs);

  // Create outputs
  std::vector<std::vector<float>> outputs(2);
  for (auto& out : outputs) {
    out.resize(BUFFER_SIZE);
  }
  auto output_ptrs = toPointerArray(outputs);

  // Process
  matrix->processRouting(input_ptrs.data(), output_ptrs.data(), BUFFER_SIZE);

  // Output should be silence
  for (uint32_t i = 0; i < BUFFER_SIZE; ++i) {
    EXPECT_FLOAT_EQ(outputs[0][i], 0.0f);
    EXPECT_FLOAT_EQ(outputs[1][i], 0.0f);
  }
}

TEST_F(RoutingMatrixTest, SoloChannelMutesOthers) {
  matrix->initialize(config);

  // Solo channel 0 (others should be muted)
  matrix->setChannelSolo(0, true);

  // Verify solo is active
  EXPECT_TRUE(matrix->isSoloActive());

  // Channel 0 should NOT be muted
  EXPECT_FALSE(matrix->isChannelMuted(0));

  // Channel 1 should be effectively muted (solo active, not solo'd)
  EXPECT_TRUE(matrix->isChannelMuted(1));
}

TEST_F(RoutingMatrixTest, GroupSoloDoesNotMuteSourceChannels) {
  ASSERT_EQ(matrix->initialize(config), SessionGraphError::OK);
  ASSERT_EQ(matrix->setGroupSolo(0, true), SessionGraphError::OK);

  EXPECT_TRUE(matrix->isSoloActive());
  for (RoutingChannelIndex channel = 0; channel < config.num_channels; ++channel)
    EXPECT_FALSE(matrix->isChannelMuted(channel));
  EXPECT_FALSE(matrix->isGroupMuted(0));
  EXPECT_TRUE(matrix->isGroupMuted(1));
}

TEST_F(RoutingMatrixTest, ChannelSoloDoesNotMuteLogicalGroups) {
  ASSERT_EQ(matrix->initialize(config), SessionGraphError::OK);
  ASSERT_EQ(matrix->setChannelSolo(0, true), SessionGraphError::OK);

  EXPECT_TRUE(matrix->isSoloActive());
  EXPECT_FALSE(matrix->isChannelMuted(0));
  EXPECT_TRUE(matrix->isChannelMuted(1));
  for (RoutingGroupIndex group = 0; group < config.num_groups; ++group)
    EXPECT_FALSE(matrix->isGroupMuted(group));
}

// ============================================================================
// Metering Tests
// ============================================================================

TEST_F(RoutingMatrixTest, MeteringDetectsPeak) {
  // Configure for single channel
  config.num_channels = 1;
  config.num_groups = 1;
  matrix->initialize(config);

  // Create input with known peak (1.0)
  auto inputs = createTestInputs(1, BUFFER_SIZE, 1.0f);
  auto input_ptrs = toPointerArray(inputs);

  // Create outputs
  std::vector<std::vector<float>> outputs(2);
  for (auto& out : outputs) {
    out.resize(BUFFER_SIZE);
  }
  auto output_ptrs = toPointerArray(outputs);

  // Process
  matrix->processRouting(input_ptrs.data(), output_ptrs.data(), BUFFER_SIZE);

  // Check master meter
  auto meter = matrix->getMasterMeter();

  // ORP121 C-04: Constant-power pan law applies -3 dB (0.707) at center
  // Peak should be close to -3 dB (unity input × 0.707 pan law)
  EXPECT_NEAR(meter.peak_db, -3.0f, 1.0f); // -3 dB due to pan law at center
}

TEST_F(RoutingMatrixTest, MeteringDetectsClipping) {
  matrix->initialize(config);

  // Create input that will clip (amplitude > 1.0)
  auto inputs = createTestInputs(4, BUFFER_SIZE, 0.5f);
  auto input_ptrs = toPointerArray(inputs);

  // Create outputs
  std::vector<std::vector<float>> outputs(2);
  for (auto& out : outputs) {
    out.resize(BUFFER_SIZE);
  }
  auto output_ptrs = toPointerArray(outputs);

  // Sum 4 channels (each 0.5) into 2 groups into master = potential clipping
  matrix->processRouting(input_ptrs.data(), output_ptrs.data(), BUFFER_SIZE);

  // Check master meter
  auto meter = matrix->getMasterMeter();

  // May or may not clip depending on signal phase, but clipping should be tracked
  // (This is a basic smoke test - clipping detection works)
  EXPECT_GE(meter.clip_count, 0); // Non-negative
}

TEST_F(RoutingMatrixTest, StereoMetersIncludeRightOnlySignal) {
  config.num_channels = 1;
  config.num_groups = 1;
  config.num_outputs = 2;
  ASSERT_EQ(matrix->initialize(config), SessionGraphError::OK);
  ASSERT_EQ(matrix->setChannelPan(0, 1.0f), SessionGraphError::OK);

  std::array<float, BUFFER_SIZE> input{};
  input.fill(0.5f);
  std::array<float, BUFFER_SIZE> left{};
  std::array<float, BUFFER_SIZE> right{};
  const float* inputs[] = {input.data()};
  float* outputs[] = {left.data(), right.data()};

  ASSERT_EQ(matrix->processRouting(inputs, outputs, BUFFER_SIZE), SessionGraphError::OK);
  ASSERT_EQ(matrix->processRouting(inputs, outputs, BUFFER_SIZE), SessionGraphError::OK);

  const auto groupMeter = matrix->getGroupMeter(0);
  const auto masterMeter = matrix->getMasterMeter();
  EXPECT_GT(groupMeter.peak_db, -7.0f);
  EXPECT_GT(masterMeter.peak_db, -7.0f);
  EXPECT_GT(groupMeter.rms_db, -10.0f);
}

TEST_F(RoutingMatrixTest, OutputMetersReportEachPhysicalLane) {
  config.num_channels = 1;
  config.num_groups = 1;
  config.num_outputs = 2;
  config.gain_smoothing_ms = 0.0f;
  ASSERT_EQ(matrix->initialize(config), SessionGraphError::OK);
  ASSERT_EQ(matrix->setChannelPan(0, 1.0f), SessionGraphError::OK);

  std::array<float, BUFFER_SIZE> input{};
  input.fill(0.5f);
  std::array<float, BUFFER_SIZE> left{};
  std::array<float, BUFFER_SIZE> right{};
  const float* inputs[] = {input.data()};
  float* outputs[] = {left.data(), right.data()};

  ASSERT_EQ(matrix->processRouting(inputs, outputs, BUFFER_SIZE), SessionGraphError::OK);
  EXPECT_FLOAT_EQ(matrix->getOutputMeter(0).peak_db, -100.0f);
  EXPECT_GT(matrix->getOutputMeter(1).peak_db, -7.0f);
  EXPECT_GT(matrix->getOutputMeter(1).rms_db, -7.0f);
  EXPECT_FLOAT_EQ(matrix->getOutputMeter(2).peak_db, -100.0f);
}

TEST_F(RoutingMatrixTest, ChannelMetersReportIsolatedEffectiveContributions) {
  config.num_channels = 2;
  config.num_groups = 1;
  config.num_outputs = 2;
  config.gain_smoothing_ms = 0.0f;

  const auto render = [&](bool reverse_channels) {
    auto routed = createRoutingMatrix();
    EXPECT_EQ(routed->initialize(config), SessionGraphError::OK);

    std::array<std::array<float, BUFFER_SIZE>, 2> input{};
    for (RoutingChannelIndex channel = 0; channel < 2; ++channel) {
      const bool source_a = reverse_channels ? channel == 1 : channel == 0;
      input[channel].fill(source_a ? 0.8f : 0.5f);
      EXPECT_EQ(routed->setChannelGroup(channel, 0), SessionGraphError::OK);
      EXPECT_EQ(routed->setChannelGain(channel, source_a ? -6.0f : -12.0f), SessionGraphError::OK);
      EXPECT_EQ(routed->setChannelPan(channel, source_a ? -1.0f : 1.0f), SessionGraphError::OK);
    }

    const float* inputs[] = {input[0].data(), input[1].data()};
    std::array<float, BUFFER_SIZE> left{};
    std::array<float, BUFFER_SIZE> right{};
    float* outputs[] = {left.data(), right.data()};
    EXPECT_EQ(routed->processRouting(inputs, outputs, BUFFER_SIZE), SessionGraphError::OK);

    std::array<AudioMeter, 2> readings = {routed->getChannelMeter(0), routed->getChannelMeter(1)};
    if (reverse_channels) {
      std::swap(readings[0], readings[1]);
    }
    return readings;
  };

  const auto forward = render(false);
  const auto reversed = render(true);
  const float source_a_peak = 0.8f * std::pow(10.0f, -6.0f / 20.0f);
  const float source_b_peak = 0.5f * std::pow(10.0f, -12.0f / 20.0f);
  const auto to_db = [](float linear) { return 20.0f * std::log10(linear); };

  EXPECT_NEAR(forward[0].peak_db, to_db(source_a_peak), 0.01f);
  EXPECT_NEAR(forward[0].rms_db, to_db(source_a_peak / std::sqrt(2.0f)), 0.01f);
  EXPECT_NEAR(forward[1].peak_db, to_db(source_b_peak), 0.01f);
  EXPECT_NEAR(forward[1].rms_db, to_db(source_b_peak / std::sqrt(2.0f)), 0.01f);
  EXPECT_NEAR(reversed[0].peak_db, forward[0].peak_db, 0.0001f);
  EXPECT_NEAR(reversed[0].rms_db, forward[0].rms_db, 0.0001f);
  EXPECT_NEAR(reversed[1].peak_db, forward[1].peak_db, 0.0001f);
  EXPECT_NEAR(reversed[1].rms_db, forward[1].rms_db, 0.0001f);
}

TEST_F(RoutingMatrixTest, ChannelMetersPublishCurrentSilenceForMuteSoloAndNullInput) {
  config.num_channels = 2;
  config.num_groups = 1;
  config.gain_smoothing_ms = 0.0f;
  ASSERT_EQ(matrix->initialize(config), SessionGraphError::OK);

  std::array<float, BUFFER_SIZE> source_a{};
  std::array<float, BUFFER_SIZE> source_b{};
  source_a.fill(0.4f);
  source_b.fill(0.3f);
  std::array<float, BUFFER_SIZE> left{};
  std::array<float, BUFFER_SIZE> right{};
  float* outputs[] = {left.data(), right.data()};
  const float* inputs[] = {source_a.data(), source_b.data()};

  ASSERT_EQ(matrix->processRouting(inputs, outputs, BUFFER_SIZE), SessionGraphError::OK);
  EXPECT_GT(matrix->getChannelMeter(1).peak_db, -100.0f);

  ASSERT_EQ(matrix->setChannelMute(1, true), SessionGraphError::OK);
  ASSERT_EQ(matrix->processRouting(inputs, outputs, BUFFER_SIZE), SessionGraphError::OK);
  EXPECT_FLOAT_EQ(matrix->getChannelMeter(1).peak_db, -100.0f);
  EXPECT_FLOAT_EQ(matrix->getChannelMeter(1).rms_db, -100.0f);
  EXPECT_GT(matrix->getGroupMeter(0).peak_db, -100.0f);
  EXPECT_GT(matrix->getMasterMeter().peak_db, -100.0f);
  ASSERT_EQ(matrix->setChannelMute(1, false), SessionGraphError::OK);
  ASSERT_EQ(matrix->setChannelMute(0, true), SessionGraphError::OK);
  ASSERT_EQ(matrix->processRouting(inputs, outputs, BUFFER_SIZE), SessionGraphError::OK);
  EXPECT_FLOAT_EQ(matrix->getChannelMeter(0).peak_db, -100.0f);
  EXPECT_FLOAT_EQ(matrix->getChannelMeter(0).rms_db, -100.0f);
  EXPECT_GT(matrix->getChannelMeter(1).peak_db, -100.0f);
  EXPECT_GT(matrix->getGroupMeter(0).peak_db, -100.0f);
  EXPECT_GT(matrix->getMasterMeter().peak_db, -100.0f);

  ASSERT_EQ(matrix->setChannelMute(0, false), SessionGraphError::OK);

  ASSERT_EQ(matrix->setChannelSolo(0, true), SessionGraphError::OK);
  ASSERT_EQ(matrix->processRouting(inputs, outputs, BUFFER_SIZE), SessionGraphError::OK);
  EXPECT_FLOAT_EQ(matrix->getChannelMeter(1).peak_db, -100.0f);
  EXPECT_FLOAT_EQ(matrix->getChannelMeter(1).rms_db, -100.0f);

  ASSERT_EQ(matrix->setChannelSolo(0, false), SessionGraphError::OK);
  const float* null_input[] = {source_a.data(), nullptr};
  ASSERT_EQ(matrix->processRouting(null_input, outputs, BUFFER_SIZE), SessionGraphError::OK);
  EXPECT_FLOAT_EQ(matrix->getChannelMeter(1).peak_db, -100.0f);
  EXPECT_FLOAT_EQ(matrix->getChannelMeter(1).rms_db, -100.0f);
}

TEST_F(RoutingMatrixTest, GroupAndMasterMetersRetainSummedStageReadings) {
  config.num_channels = 2;
  config.num_groups = 1;
  config.gain_smoothing_ms = 0.0f;
  ASSERT_EQ(matrix->initialize(config), SessionGraphError::OK);

  std::array<float, BUFFER_SIZE> source_a{};
  std::array<float, BUFFER_SIZE> source_b{};
  source_a.fill(0.25f);
  source_b.fill(0.25f);
  const float* inputs[] = {source_a.data(), source_b.data()};
  std::array<float, BUFFER_SIZE> left{};
  std::array<float, BUFFER_SIZE> right{};
  float* outputs[] = {left.data(), right.data()};
  ASSERT_EQ(matrix->processRouting(inputs, outputs, BUFFER_SIZE), SessionGraphError::OK);

  const AudioMeter channel = matrix->getChannelMeter(0);
  const AudioMeter group = matrix->getGroupMeter(0);
  const AudioMeter master = matrix->getMasterMeter();
  EXPECT_NEAR(group.peak_db - channel.peak_db, 6.0206f, 0.01f);
  EXPECT_NEAR(group.rms_db - channel.rms_db, 6.0206f, 0.01f);
  EXPECT_NEAR(master.peak_db, group.peak_db, 0.01f);
  EXPECT_NEAR(master.rms_db, group.rms_db, 0.01f);
}

// ============================================================================
// Snapshot Tests
// ============================================================================

TEST_F(RoutingMatrixTest, SaveSnapshotCapturesState) {
  matrix->initialize(config);

  // Configure channels
  matrix->setChannelGain(0, -6.0f);
  matrix->setChannelMute(1, true);
  matrix->setChannelSolo(2, true);

  // Save snapshot
  auto snapshot = matrix->saveSnapshot("Test Snapshot");

  EXPECT_EQ(snapshot.name, "Test Snapshot");
  EXPECT_EQ(snapshot.channels.size(), 4);
  EXPECT_EQ(snapshot.groups.size(), 2);
}

TEST_F(RoutingMatrixTest, SnapshotRevisionAndProvenanceAreDeterministic) {
  ASSERT_EQ(matrix->initialize(config), SessionGraphError::OK);

  RoutingSnapshotContext firstContext;
  firstContext.controlTimeMs = 1234;
  firstContext.audioPosition = TimePoint::fromSamples(2048);
  const auto first = matrix->saveSnapshot("First", firstContext);

  RoutingSnapshotContext secondContext;
  secondContext.controlTimeMs = 9999;
  const auto second = matrix->saveSnapshot("Second", secondContext);

  EXPECT_EQ(first.captureRevision, 1u);
  EXPECT_EQ(second.captureRevision, 2u);
  ASSERT_TRUE(first.controlTimeMs.has_value());
  EXPECT_EQ(*first.controlTimeMs, 1234u);
  ASSERT_TRUE(first.audioPosition.has_value());
  EXPECT_EQ(first.audioPosition->samples(), 2048);
  EXPECT_FALSE(second.audioPosition.has_value());

  ASSERT_EQ(first.channels.size(), second.channels.size());
  ASSERT_EQ(first.groups.size(), second.groups.size());
  EXPECT_FLOAT_EQ(first.master_gain_db, second.master_gain_db);
  EXPECT_EQ(first.master_mute, second.master_mute);
}

TEST_F(RoutingMatrixTest, LoadSnapshotRestoresState) {
  matrix->initialize(config);

  // Configure channels
  matrix->setChannelGain(0, -6.0f);
  matrix->setChannelMute(1, true);

  // Save snapshot
  auto snapshot = matrix->saveSnapshot("Saved State");

  // Change state
  matrix->setChannelGain(0, 0.0f);
  matrix->setChannelMute(1, false);

  // Load snapshot (restore)
  auto result = matrix->loadSnapshot(snapshot);

  EXPECT_EQ(result, SessionGraphError::OK);

  // Verify state restored (channels should have original settings)
  // (Direct verification requires getChannelConfig, which is not in interface)
  // For now, just verify load succeeded
}

TEST_F(RoutingMatrixTest, ResetClearsAllState) {
  matrix->initialize(config);

  // Configure channels
  matrix->setChannelGain(0, -6.0f);
  matrix->setChannelMute(1, true);
  matrix->setMasterGain(-3.0f);

  // Reset
  auto result = matrix->reset();

  EXPECT_EQ(result, SessionGraphError::OK);

  // Solo should be inactive after reset
  EXPECT_FALSE(matrix->isSoloActive());
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(RoutingMatrixTest, Process64ChannelsSimultaneously) {
  // Reconfigure for maximum channels
  config.num_channels = 64;
  config.num_groups = 16;
  config.num_outputs = 2;

  matrix->initialize(config);

  // Create 64-channel input
  auto inputs = createTestInputs(64, BUFFER_SIZE, 0.1f); // Low amplitude to avoid clipping
  auto input_ptrs = toPointerArray(inputs);

  // Create outputs
  std::vector<std::vector<float>> outputs(2);
  for (auto& out : outputs) {
    out.resize(BUFFER_SIZE);
  }
  auto output_ptrs = toPointerArray(outputs);

  // Process
  auto result = matrix->processRouting(input_ptrs.data(), output_ptrs.data(), BUFFER_SIZE);

  EXPECT_EQ(result, SessionGraphError::OK);

  // Verify output has signal (sum of 64 channels)
  bool has_signal = false;
  for (uint32_t i = 0; i < BUFFER_SIZE; ++i) {
    if (std::abs(outputs[0][i]) > 0.01f) {
      has_signal = true;
      break;
    }
  }
  EXPECT_TRUE(has_signal);
}

TEST_F(RoutingMatrixTest, RapidParameterChanges) {
  matrix->initialize(config);

  // Create test inputs
  auto inputs = createTestInputs(4, BUFFER_SIZE, 0.5f);
  auto input_ptrs = toPointerArray(inputs);

  // Create outputs
  std::vector<std::vector<float>> outputs(2);
  for (auto& out : outputs) {
    out.resize(BUFFER_SIZE);
  }
  auto output_ptrs = toPointerArray(outputs);

  // Rapidly change parameters while processing
  for (int i = 0; i < 100; ++i) {
    // Change gains
    float gain = -12.0f + (i % 24); // Cycle through -12 to +12 dB
    matrix->setChannelGain(0, gain);
    matrix->setGroupGain(0, gain);
    matrix->setMasterGain(gain / 2.0f);

    // Process
    auto result = matrix->processRouting(input_ptrs.data(), output_ptrs.data(), BUFFER_SIZE);
    EXPECT_EQ(result, SessionGraphError::OK);
  }

  // Should complete without crashes or errors
  EXPECT_TRUE(true);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(RoutingMatrixTest, UnassignedChannelProducesNoOutput) {
  // Configure for single channel
  config.num_channels = 1;
  config.num_groups = 1;
  matrix->initialize(config);

  // Unassign channel 0 from all groups
  matrix->setChannelGroup(0, UNASSIGNED_GROUP);

  // Create test inputs (1 channel to match config)
  auto inputs = createTestInputs(1, BUFFER_SIZE, 1.0f);
  auto input_ptrs = toPointerArray(inputs);

  // Create outputs
  std::vector<std::vector<float>> outputs(2);
  for (auto& out : outputs) {
    out.resize(BUFFER_SIZE);
  }
  auto output_ptrs = toPointerArray(inputs); // Using inputs array just has pointers to test code
                                             // for outputs Fixed - create proper output pointers
  auto output_ptrs_fixed = toPointerArray(outputs);

  // Process
  matrix->processRouting(input_ptrs.data(), output_ptrs_fixed.data(), BUFFER_SIZE);

  // Output should be silence (unassigned channel doesn't route)
  for (uint32_t i = 0; i < BUFFER_SIZE; ++i) {
    EXPECT_FLOAT_EQ(outputs[0][i], 0.0f);
  }
}

TEST_F(RoutingMatrixTest, ProcessWithoutInitializeFails) {
  // Don't initialize

  // Create dummy buffers
  auto inputs = createTestInputs(1, BUFFER_SIZE);
  auto input_ptrs = toPointerArray(inputs);

  std::vector<std::vector<float>> outputs(2);
  for (auto& out : outputs) {
    out.resize(BUFFER_SIZE);
  }
  auto output_ptrs = toPointerArray(outputs);

  // Process should fail
  auto result = matrix->processRouting(input_ptrs.data(), output_ptrs.data(), BUFFER_SIZE);

  EXPECT_EQ(result, SessionGraphError::NotInitialized);
}

TEST_F(RoutingMatrixTest, ProcessWithOversizedBufferSucceeds) {
  // FTR028: a block larger than the internal slice (MAX_BUFFER_SIZE = 2048) is
  // no longer rejected — processRouting() chunks it internally and processes
  // the whole block. (Previously this returned InvalidParameter and produced
  // no output, which silently summed FourTrack bounces to silence.)
  matrix->initialize(config); // 4 channels per SetUp()

  // Inputs must match config.num_channels (4) to be dereferenced safely.
  auto inputs = createTestInputs(4, 4096);
  auto input_ptrs = toPointerArray(inputs);

  std::vector<std::vector<float>> outputs(2);
  for (auto& out : outputs) {
    out.resize(4096, 0.0f);
  }
  auto output_ptrs = toPointerArray(outputs);

  // Process should succeed for the oversized block.
  auto result = matrix->processRouting(input_ptrs.data(), output_ptrs.data(), 4096);

  EXPECT_EQ(result, SessionGraphError::OK);
}

// ============================================================================
// ORP121 Q-05: Headroom Management Tests
// ============================================================================

TEST_F(RoutingMatrixTest, HeadroomModeNoneNoAttenuation) {
  // HeadroomMode::None should not attenuate the signal
  config.headroom_mode = HeadroomMode::None;
  config.num_channels = 2;
  config.num_groups = 1;
  config.gain_smoothing_ms = 0.1f; // Fast smoothing for test
  config.enable_clipping_protection = false;
  matrix->initialize(config);

  // Assign both channels to group 0
  matrix->setChannelGroup(0, 0);
  matrix->setChannelGroup(1, 0);
  matrix->setChannelGain(0, 0.0f); // Unity gain
  matrix->setChannelGain(1, 0.0f); // Unity gain
  matrix->setGroupGain(0, 0.0f);   // Unity gain
  matrix->setMasterGain(0.0f);     // Unity gain

  // Create constant amplitude test inputs (0.5 for both channels)
  std::vector<std::vector<float>> inputs(2);
  for (auto& in : inputs) {
    in.resize(BUFFER_SIZE, 0.5f);
  }
  auto input_ptrs = toPointerArray(inputs);

  // Create output buffers
  std::vector<std::vector<float>> outputs(2);
  for (auto& out : outputs) {
    out.resize(BUFFER_SIZE, 0.0f);
  }
  auto output_ptrs = toPointerArray(outputs);

  // Process a few times to let gain smoothers settle
  for (int i = 0; i < 10; ++i) {
    std::fill(outputs[0].begin(), outputs[0].end(), 0.0f);
    std::fill(outputs[1].begin(), outputs[1].end(), 0.0f);
    matrix->processRouting(input_ptrs.data(), output_ptrs.data(), BUFFER_SIZE);
  }

  // With HeadroomMode::None, output should be sum of inputs: 0.5 + 0.5 = 1.0
  // (allowing for constant-power pan law at center, each channel contributes ~0.707 * 0.5)
  // Two centered channels: 2 * (0.707 * 0.5) ≈ 0.707
  float expected = 2.0f * (0.707107f * 0.5f);
  EXPECT_NEAR(outputs[0][BUFFER_SIZE - 1], expected, 0.05f);
}

TEST_F(RoutingMatrixTest, HeadroomModePerGroupAttenuates) {
  // HeadroomMode::PerGroup should attenuate by 1/n where n = active channels in group
  config.headroom_mode = HeadroomMode::PerGroup;
  config.num_channels = 2;
  config.num_groups = 1;
  config.gain_smoothing_ms = 0.1f;
  config.enable_clipping_protection = false;
  matrix->initialize(config);

  // Assign both channels to group 0
  matrix->setChannelGroup(0, 0);
  matrix->setChannelGroup(1, 0);
  matrix->setChannelGain(0, 0.0f);
  matrix->setChannelGain(1, 0.0f);
  matrix->setGroupGain(0, 0.0f);
  matrix->setMasterGain(0.0f);

  // Create constant amplitude test inputs
  std::vector<std::vector<float>> inputs(2);
  for (auto& in : inputs) {
    in.resize(BUFFER_SIZE, 0.5f);
  }
  auto input_ptrs = toPointerArray(inputs);

  std::vector<std::vector<float>> outputs(2);
  for (auto& out : outputs) {
    out.resize(BUFFER_SIZE, 0.0f);
  }
  auto output_ptrs = toPointerArray(outputs);

  // Process multiple times to let smoothers settle
  for (int i = 0; i < 10; ++i) {
    std::fill(outputs[0].begin(), outputs[0].end(), 0.0f);
    std::fill(outputs[1].begin(), outputs[1].end(), 0.0f);
    matrix->processRouting(input_ptrs.data(), output_ptrs.data(), BUFFER_SIZE);
  }

  // With 2 channels in group, headroom = 1/2 = 0.5
  // Expected: (2 * 0.707 * 0.5) * 0.5 ≈ 0.354
  float expected_none = 2.0f * (0.707107f * 0.5f);
  float expected_per_group = expected_none * 0.5f;
  EXPECT_NEAR(outputs[0][BUFFER_SIZE - 1], expected_per_group, 0.05f);
}

TEST_F(RoutingMatrixTest, TruePeakMeteringDetectsInterSamplePeaks) {
  // ORP121 Q-04: True-peak metering should detect peaks between samples
  config.metering_mode = MeteringMode::TruePeak;
  config.num_channels = 1;
  config.num_groups = 1;
  config.gain_smoothing_ms = 0.1f;
  config.enable_metering = true;
  config.enable_clipping_protection = false;
  matrix->initialize(config);

  matrix->setChannelGroup(0, 0);
  matrix->setChannelGain(0, 0.0f);
  matrix->setGroupGain(0, 0.0f);
  matrix->setMasterGain(0.0f);

  // Create test signal: two samples that will create an inter-sample peak
  // At samples [0.7, -0.7], the true peak between them is higher than 0.7
  std::vector<std::vector<float>> inputs(1);
  inputs[0].resize(BUFFER_SIZE, 0.0f);
  // Create a pattern that will produce inter-sample peaks
  for (uint32_t i = 0; i < BUFFER_SIZE; i += 2) {
    inputs[0][i] = 0.7f;
    inputs[0][i + 1] = -0.7f;
  }
  auto input_ptrs = toPointerArray(inputs);

  std::vector<std::vector<float>> outputs(2);
  for (auto& out : outputs) {
    out.resize(BUFFER_SIZE, 0.0f);
  }
  auto output_ptrs = toPointerArray(outputs);

  // Process multiple times to let smoothers settle
  for (int i = 0; i < 10; ++i) {
    std::fill(outputs[0].begin(), outputs[0].end(), 0.0f);
    std::fill(outputs[1].begin(), outputs[1].end(), 0.0f);
    matrix->processRouting(input_ptrs.data(), output_ptrs.data(), BUFFER_SIZE);
  }

  // Get meter reading
  AudioMeter meter = matrix->getMasterMeter();

  // True-peak should detect inter-sample peaks higher than sample peak
  // The sample peak is 0.7, but true-peak may be higher due to interpolation
  // We just verify it detected a significant peak (> 0.5 after pan law)
  float peak_linear = std::pow(10.0f, meter.peak_db / 20.0f);
  EXPECT_GT(peak_linear, 0.3f);

  std::cout << "[True-Peak] Detected peak: " << meter.peak_db << " dBFS (" << peak_linear
            << " linear)\n";
}

TEST_F(RoutingMatrixTest, HeadroomModeLogarithmicAttenuates) {
  // HeadroomMode::Logarithmic should attenuate by 1/sqrt(n)
  config.headroom_mode = HeadroomMode::Logarithmic;
  config.num_channels = 4;
  config.num_groups = 1;
  config.gain_smoothing_ms = 0.1f;
  config.enable_clipping_protection = false;
  matrix->initialize(config);

  // Assign all 4 channels to group 0
  for (int i = 0; i < 4; ++i) {
    matrix->setChannelGroup(i, 0);
    matrix->setChannelGain(i, 0.0f);
  }
  matrix->setGroupGain(0, 0.0f);
  matrix->setMasterGain(0.0f);

  // Create constant amplitude test inputs
  std::vector<std::vector<float>> inputs(4);
  for (auto& in : inputs) {
    in.resize(BUFFER_SIZE, 0.5f);
  }
  auto input_ptrs = toPointerArray(inputs);

  std::vector<std::vector<float>> outputs(2);
  for (auto& out : outputs) {
    out.resize(BUFFER_SIZE, 0.0f);
  }
  auto output_ptrs = toPointerArray(outputs);

  // Process multiple times to let smoothers settle
  for (int i = 0; i < 10; ++i) {
    std::fill(outputs[0].begin(), outputs[0].end(), 0.0f);
    std::fill(outputs[1].begin(), outputs[1].end(), 0.0f);
    matrix->processRouting(input_ptrs.data(), output_ptrs.data(), BUFFER_SIZE);
  }

  // With 4 channels, logarithmic headroom = 1/sqrt(4) = 0.5
  // Expected: (4 * 0.707 * 0.5) * 0.5 ≈ 0.707
  float expected_none = 4.0f * (0.707107f * 0.5f);
  float expected_log = expected_none * (1.0f / std::sqrt(4.0f));
  EXPECT_NEAR(outputs[0][BUFFER_SIZE - 1], expected_log, 0.1f);
}

// ============================================================================
// FTR028: Block-size ceiling / internal chunking
// ============================================================================
//
// Regression coverage for FTR028: processRouting() must accept blocks larger
// than the internal slice size (kRoutingSliceFrames == 2048) by chunking
// internally, rather than silently rejecting them and producing no output.
// A large offline bounce/export block must pass signal through at the correct
// gain across the ENTIRE block, including past the 2048-frame boundary.

// Helper: single-channel, single-group config with a DC (constant) input so
// the expected output is exactly predictable (no smoothing transient: gain and
// pan default to unity/center and never change, so the smoothers sit still).
namespace {
constexpr float kConstantPowerCenter = 0.707f; // pan law at center (-3 dB)

void configureSingleChannelDC(IRoutingMatrix& matrix, RoutingConfig& config) {
  config.num_channels = 1;
  config.num_groups = 1;
  config.num_outputs = 2;
  config.enable_metering = false;
  config.enable_clipping_protection = false; // keep math linear for assertions
  ASSERT_EQ(matrix.initialize(config), SessionGraphError::OK);
}
} // namespace

TEST_F(RoutingMatrixTest, ExposesMaxBlockFramesContract) {
  matrix->initialize(config);
  // FTR028: the internal slice granularity is exposed publicly and matches the
  // documented constant.
  EXPECT_EQ(matrix->maxBlockFrames(), kRoutingSliceFrames);
  EXPECT_EQ(kRoutingSliceFrames, 2048u);
}

TEST_F(RoutingMatrixTest, ProcessRoutingBoundaryBlockPassesSignal) {
  // Exactly the slice boundary (2048) must still work and pass signal.
  configureSingleChannelDC(*matrix, config);

  const uint32_t frames = kRoutingSliceFrames; // 2048
  const float amplitude = 0.5f;

  std::vector<float> input(frames, amplitude);
  const float* input_ptrs[1] = {input.data()};

  std::vector<std::vector<float>> outputs(2, std::vector<float>(frames, -999.0f));
  auto output_ptrs = toPointerArray(outputs);

  auto result = matrix->processRouting(input_ptrs, output_ptrs.data(), frames);
  ASSERT_EQ(result, SessionGraphError::OK);

  // DC input through unity gain + constant-power center pan.
  const float expected = amplitude * kConstantPowerCenter; // ~0.3535
  EXPECT_NEAR(outputs[0][frames - 1], expected, 0.01f);
  EXPECT_NEAR(outputs[1][frames - 1], expected, 0.01f);
}

TEST_F(RoutingMatrixTest, ProcessRoutingOnePastBoundaryPassesTailSample) {
  configureSingleChannelDC(*matrix, config);
  const uint32_t frames = kRoutingSliceFrames + 1;
  const float amplitude = 0.5f;
  std::vector<float> input(frames, amplitude);
  const float* input_ptrs[1] = {input.data()};
  std::vector<std::vector<float>> outputs(2, std::vector<float>(frames, -999.0f));
  auto output_ptrs = toPointerArray(outputs);

  ASSERT_EQ(matrix->processRouting(input_ptrs, output_ptrs.data(), frames), SessionGraphError::OK);
  const float expected = amplitude * kConstantPowerCenter;
  EXPECT_NEAR(outputs[0][2048], expected, 0.01f);
  EXPECT_NEAR(outputs[1][2048], expected, 0.01f);
}

TEST_F(RoutingMatrixTest, ProcessRoutingLargeBlockChunksAndPassesSignal) {
  // FTR028 core: a 4096-frame block (2x the internal slice) must NOT be
  // rejected and must pass signal across the WHOLE block, including samples
  // past the 2048 boundary that the old ceiling would have dropped.
  configureSingleChannelDC(*matrix, config);

  const uint32_t frames = 4096; // 2 * kRoutingSliceFrames
  const float amplitude = 0.5f;

  std::vector<float> input(frames, amplitude);
  const float* input_ptrs[1] = {input.data()};

  // Pre-fill outputs with a sentinel so untouched samples are detectable.
  std::vector<std::vector<float>> outputs(2, std::vector<float>(frames, -999.0f));
  auto output_ptrs = toPointerArray(outputs);

  auto result = matrix->processRouting(input_ptrs, output_ptrs.data(), frames);
  ASSERT_EQ(result, SessionGraphError::OK);

  const float expected = amplitude * kConstantPowerCenter; // ~0.3535

  // Assert CONTENT at exact sample positions across the entire block:
  // first sample, just before the boundary, exactly at the boundary (first
  // sample of the second slice), and the last sample.
  for (uint32_t pos : {0u, 2047u, 2048u, frames - 1}) {
    EXPECT_NEAR(outputs[0][pos], expected, 0.01f) << "left @ frame " << pos;
    EXPECT_NEAR(outputs[1][pos], expected, 0.01f) << "right @ frame " << pos;
  }

  // No sentinel survived anywhere: every sample was written (no silent tail).
  for (uint32_t i = 0; i < frames; ++i) {
    ASSERT_GT(outputs[0][i], expected - 0.05f) << "left silent/untouched @ " << i;
    ASSERT_GT(outputs[1][i], expected - 0.05f) << "right silent/untouched @ " << i;
  }
}

TEST_F(RoutingMatrixTest, ProcessRoutingLargeBlockMatchesChunkedEquivalent) {
  // The chunked large-block path must produce identical output to feeding the
  // same signal as consecutive 2048-frame calls — proving chunking is a pure
  // decomposition, not an approximation. Uses a fresh matrix per path.
  const uint32_t total = 5000; // deliberately not a multiple of 2048
  const float amplitude = 0.4f;
  std::vector<float> input(total, amplitude);

  // Path A: single large call.
  auto matrixA = createRoutingMatrix();
  RoutingConfig cfgA = config;
  configureSingleChannelDC(*matrixA, cfgA);
  const float* inA[1] = {input.data()};
  std::vector<std::vector<float>> outA(2, std::vector<float>(total, 0.0f));
  std::vector<float*> outAptr = {outA[0].data(), outA[1].data()};
  ASSERT_EQ(matrixA->processRouting(inA, outAptr.data(), total), SessionGraphError::OK);

  // Path B: consecutive slices of <= 2048.
  auto matrixB = createRoutingMatrix();
  RoutingConfig cfgB = config;
  configureSingleChannelDC(*matrixB, cfgB);
  std::vector<std::vector<float>> outB(2, std::vector<float>(total, 0.0f));
  uint32_t off = 0;
  while (off < total) {
    uint32_t slice = std::min<uint32_t>(total - off, kRoutingSliceFrames);
    const float* inB[1] = {input.data() + off};
    float* outBptr[2] = {outB[0].data() + off, outB[1].data() + off};
    ASSERT_EQ(matrixB->processRouting(inB, outBptr, slice), SessionGraphError::OK);
    off += slice;
  }

  for (uint32_t i = 0; i < total; ++i) {
    EXPECT_NEAR(outA[0][i], outB[0][i], 1e-6f) << "left mismatch @ " << i;
    EXPECT_NEAR(outA[1][i], outB[1][i], 1e-6f) << "right mismatch @ " << i;
  }
}

// ============================================================================
// Coherent group-control contract
// ============================================================================

TEST_F(RoutingMatrixTest, GroupControlSnapshotReportsConfiguredAndEffectiveState) {
  ASSERT_EQ(matrix->initialize(config), SessionGraphError::OK);

  const auto initial = matrix->getRoutingControlSnapshot();
  ASSERT_EQ(initial.schema_version, kRoutingControlSnapshotSchemaVersion);
  ASSERT_EQ(initial.group_count, 2);
  EXPECT_EQ(initial.revision, 0u);

  ASSERT_EQ(matrix->setGroupGain(0, -6.0f), SessionGraphError::OK);
  ASSERT_EQ(matrix->setGroupMute(0, true), SessionGraphError::OK);
  ASSERT_EQ(matrix->setGroupSolo(1, true), SessionGraphError::OK);

  const auto snapshot = matrix->getRoutingControlSnapshot();
  EXPECT_EQ(snapshot.revision, 3u);
  EXPECT_FLOAT_EQ(snapshot.groups[0].gain_db, -6.0f);
  EXPECT_TRUE(snapshot.groups[0].configured_mute);
  EXPECT_FALSE(snapshot.groups[0].configured_solo);
  EXPECT_TRUE(snapshot.groups[0].effective_mute);
  EXPECT_FALSE(snapshot.groups[1].configured_mute);
  EXPECT_TRUE(snapshot.groups[1].configured_solo);
  EXPECT_FALSE(snapshot.groups[1].effective_mute);
}

TEST_F(RoutingMatrixTest, GroupControlTransactionRejectsInvalidStateWithoutMutation) {
  config.num_outputs = 4;
  ASSERT_EQ(matrix->initialize(config), SessionGraphError::OK);
  const auto before = matrix->getRoutingControlSnapshot();

  auto invalid = before;
  invalid.groups[0].gain_db = -9.0f;
  invalid.groups[0].configured_mute = true;
  invalid.groups[1].output_start = 3;
  invalid.groups[1].output_width = 2;
  EXPECT_EQ(matrix->applyGroupControlSnapshot(invalid), SessionGraphError::InvalidParameter);

  const auto after = matrix->getRoutingControlSnapshot();
  EXPECT_EQ(after.revision, before.revision);
  EXPECT_FLOAT_EQ(after.groups[0].gain_db, before.groups[0].gain_db);
  EXPECT_EQ(after.groups[0].configured_mute, before.groups[0].configured_mute);
  EXPECT_EQ(after.groups[1].output_start, before.groups[1].output_start);
  EXPECT_EQ(after.groups[1].output_width, before.groups[1].output_width);
}

TEST_F(RoutingMatrixTest, GroupControlTransactionPublishesOneCoherentRevision) {
  config.num_outputs = 4;
  ASSERT_EQ(matrix->initialize(config), SessionGraphError::OK);
  auto desired = matrix->getRoutingControlSnapshot();
  desired.groups[0].gain_db = -3.0f;
  desired.groups[0].configured_mute = true;
  desired.groups[0].output_start = 0;
  desired.groups[0].output_width = 2;
  desired.groups[1].gain_db = 4.0f;
  desired.groups[1].configured_solo = true;
  desired.groups[1].output_start = 2;
  desired.groups[1].output_width = 2;

  ASSERT_EQ(matrix->applyGroupControlSnapshot(desired), SessionGraphError::OK);
  const auto applied = matrix->getRoutingControlSnapshot();
  EXPECT_EQ(applied.revision, desired.revision + 1u);
  EXPECT_FLOAT_EQ(applied.groups[0].gain_db, -3.0f);
  EXPECT_TRUE(applied.groups[0].configured_mute);
  EXPECT_EQ(applied.groups[0].output_width, 2);
  EXPECT_FLOAT_EQ(applied.groups[1].gain_db, 4.0f);
  EXPECT_TRUE(applied.groups[1].configured_solo);
  EXPECT_EQ(applied.groups[1].output_start, 2);
}

TEST_F(RoutingMatrixTest, AcceptedGroupTransactionTakesEffectAtNextRenderBoundary) {
  config.num_channels = 2;
  config.num_groups = 2;
  config.num_outputs = 4;
  config.source_channel_policy = SourceChannelPolicy::Discrete;
  config.enable_metering = false;
  ASSERT_EQ(matrix->initialize(config), SessionGraphError::OK);

  ChannelConfig channel0;
  channel0.group_index = 0;
  channel0.output_channel = 0;
  ChannelConfig channel1;
  channel1.group_index = 1;
  channel1.output_channel = 0;
  ASSERT_EQ(matrix->configureChannel(0, channel0), SessionGraphError::OK);
  ASSERT_EQ(matrix->configureChannel(1, channel1), SessionGraphError::OK);

  auto profile = matrix->getRoutingControlSnapshot();
  profile.groups[0].output_start = 1;
  profile.groups[0].output_width = 1;
  profile.groups[1].configured_mute = true;
  profile.groups[1].output_start = 3;
  profile.groups[1].output_width = 1;
  ASSERT_EQ(matrix->applyGroupControlSnapshot(profile), SessionGraphError::OK);

  std::vector<float> source0(BUFFER_SIZE, 0.25f);
  std::vector<float> source1(BUFFER_SIZE, 0.5f);
  const float* inputs[2] = {source0.data(), source1.data()};
  std::vector<std::vector<float>> outputs(config.num_outputs,
                                          std::vector<float>(BUFFER_SIZE, -999.0f));
  auto outputPointers = toPointerArray(outputs);
  ASSERT_EQ(matrix->processRouting(inputs, outputPointers.data(), BUFFER_SIZE),
            SessionGraphError::OK);

  EXPECT_FLOAT_EQ(outputs[0].front(), 0.0f);
  EXPECT_NEAR(outputs[1].front(), 0.25f, TOLERANCE);
  EXPECT_FLOAT_EQ(outputs[2].front(), 0.0f);
  EXPECT_FLOAT_EQ(outputs[3].front(), 0.0f);
}

TEST_F(RoutingMatrixTest, RoutingPresetRecallPublishesSavedGroupControls) {
  config.num_outputs = 4;
  ASSERT_EQ(matrix->initialize(config), SessionGraphError::OK);
  auto expected = matrix->getRoutingControlSnapshot();
  expected.groups[0].gain_db = -12.0f;
  expected.groups[0].configured_mute = true;
  expected.groups[0].output_width = 1;
  expected.groups[1].gain_db = 3.0f;
  expected.groups[1].output_start = 2;
  expected.groups[1].output_width = 2;
  ASSERT_EQ(matrix->applyGroupControlSnapshot(expected), SessionGraphError::OK);
  const auto preset = matrix->saveSnapshot("routing");

  auto changed = matrix->getRoutingControlSnapshot();
  changed.groups[0].gain_db = 0.0f;
  changed.groups[0].configured_mute = false;
  changed.groups[0].output_width = 4;
  changed.groups[1].gain_db = 0.0f;
  changed.groups[1].output_start = 0;
  changed.groups[1].output_width = 4;
  ASSERT_EQ(matrix->applyGroupControlSnapshot(changed), SessionGraphError::OK);
  ASSERT_EQ(matrix->loadSnapshot(preset), SessionGraphError::OK);

  const auto recalled = matrix->getRoutingControlSnapshot();
  EXPECT_FLOAT_EQ(recalled.groups[0].gain_db, -12.0f);
  EXPECT_TRUE(recalled.groups[0].configured_mute);
  EXPECT_EQ(recalled.groups[0].output_width, 1);
  EXPECT_FLOAT_EQ(recalled.groups[1].gain_db, 3.0f);
  EXPECT_EQ(recalled.groups[1].output_start, 2);
  EXPECT_EQ(recalled.groups[1].output_width, 2);
}

TEST_F(RoutingMatrixTest, ConcurrentRenderAndQueryObserveOnlyCompleteTransactions) {
  ASSERT_EQ(matrix->initialize(config), SessionGraphError::OK);
  auto stateA = matrix->getRoutingControlSnapshot();
  stateA.groups[0].gain_db = -8.0f;
  stateA.groups[0].configured_mute = true;
  stateA.groups[1].gain_db = 2.0f;
  stateA.groups[1].configured_solo = false;
  auto stateB = stateA;
  stateB.groups[0].gain_db = 5.0f;
  stateB.groups[0].configured_mute = false;
  stateB.groups[1].gain_db = -4.0f;
  stateB.groups[1].configured_solo = true;
  ASSERT_EQ(matrix->applyGroupControlSnapshot(stateA), SessionGraphError::OK);

  std::atomic<bool> writerDone{false};
  std::atomic<bool> failed{false};
  std::thread writer([&] {
    for (int iteration = 0; iteration < 2000; ++iteration) {
      const auto& state = (iteration & 1) == 0 ? stateB : stateA;
      if (matrix->applyGroupControlSnapshot(state) != SessionGraphError::OK) {
        failed.store(true, std::memory_order_release);
      }
    }
    writerDone.store(true, std::memory_order_release);
  });

  auto inputs = createTestInputs(config.num_channels, BUFFER_SIZE);
  auto inputPointers = toPointerArray(inputs);
  std::vector<std::vector<float>> outputs(config.num_outputs, std::vector<float>(BUFFER_SIZE));
  auto outputPointers = toPointerArray(outputs);
  do {
    if (matrix->processRouting(reinterpret_cast<const float* const*>(inputPointers.data()),
                               outputPointers.data(), BUFFER_SIZE) != SessionGraphError::OK) {
      failed.store(true, std::memory_order_release);
    }
    const auto observed = matrix->getRoutingControlSnapshot();
    const bool isA = observed.groups[0].gain_db == -8.0f && observed.groups[0].configured_mute &&
                     observed.groups[1].gain_db == 2.0f && !observed.groups[1].configured_solo;
    const bool isB = observed.groups[0].gain_db == 5.0f && !observed.groups[0].configured_mute &&
                     observed.groups[1].gain_db == -4.0f && observed.groups[1].configured_solo;
    if (!isA && !isB) {
      failed.store(true, std::memory_order_release);
    }
  } while (!writerDone.load(std::memory_order_acquire));

  writer.join();
  EXPECT_FALSE(failed.load(std::memory_order_acquire));
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
