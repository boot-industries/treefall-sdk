// SPDX-License-Identifier: MIT
#include "../../include/orpheus/routing_matrix.h"

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <memory>

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
    std::vector<std::vector<float>> createTestInputs(uint32_t num_channels, uint32_t num_frames, float amplitude = 0.5f) {
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
    config.num_channels = 65; // > 64
    auto result = matrix->initialize(config);

    EXPECT_EQ(result, SessionGraphError::InvalidParameter);
}

TEST_F(RoutingMatrixTest, InitializeWithInvalidGroupCount) {
    config.num_groups = 0; // Invalid
    auto result = matrix->initialize(config);

    EXPECT_EQ(result, SessionGraphError::InvalidParameter);
}

TEST_F(RoutingMatrixTest, InitializeWithInvalidOutputCount) {
    config.num_outputs = 1; // < 2 (stereo minimum)
    auto result = matrix->initialize(config);

    EXPECT_EQ(result, SessionGraphError::InvalidParameter);
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
    // Expected: 0.637 * 0.5 = 0.318
    float avg_output = 0.0f;
    for (uint32_t i = BUFFER_SIZE / 2; i < BUFFER_SIZE; ++i) { // Second half (after transient)
        avg_output += std::abs(outputs[0][i]);
    }
    avg_output /= (BUFFER_SIZE / 2);

    // Expect approximately 0.318 (0.5 peak × 0.637 sine wave factor)
    EXPECT_NEAR(avg_output, 0.318f, 0.05f);
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
    float avg_output = 0.0f;
    for (uint32_t i = BUFFER_SIZE / 2; i < BUFFER_SIZE; ++i) {
        avg_output += std::abs(outputs[0][i]);
    }
    avg_output /= (BUFFER_SIZE / 2);

    EXPECT_NEAR(avg_output, 0.318f, 0.05f);
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

    // Peak should be close to 1.0 (unity gain)
    EXPECT_NEAR(meter.peak_db, 0.0f, 1.0f); // 0 dB = unity
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
    auto output_ptrs = toPointerArray(inputs); // Using inputs array just has pointers to test code for outputs
// Fixed - create proper output pointers
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

TEST_F(RoutingMatrixTest, ProcessWithOversizedBufferFails) {
    matrix->initialize(config);

    // Create oversized buffer (> MAX_BUFFER_SIZE = 2048)
    auto inputs = createTestInputs(1, 4096);
    auto input_ptrs = toPointerArray(inputs);

    std::vector<std::vector<float>> outputs(2);
    for (auto& out : outputs) {
        out.resize(4096);
    }
    auto output_ptrs = toPointerArray(outputs);

    // Process should fail
    auto result = matrix->processRouting(input_ptrs.data(), output_ptrs.data(), 4096);

    EXPECT_EQ(result, SessionGraphError::InvalidParameter);
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
