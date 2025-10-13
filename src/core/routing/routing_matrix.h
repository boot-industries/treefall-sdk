// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/routing_matrix.h>

#include <array>
#include <atomic>
#include <mutex>
#include <vector>

namespace orpheus {

// Forward declaration for gain smoother
class GainSmoother;

/// Internal channel state (audio thread)
struct ChannelState {
    uint8_t group_index;        ///< Assigned group (255 = unassigned)
    GainSmoother* gain_smoother; ///< Gain smoothing
    GainSmoother* pan_left;     ///< Left pan gain
    GainSmoother* pan_right;    ///< Right pan gain
    std::atomic<bool> mute;
    std::atomic<bool> solo;

    // Metering
    std::atomic<float> peak_level;
    std::atomic<float> rms_level;
    std::atomic<uint32_t> clip_count;

    // Configuration (UI thread writes, audio thread reads)
    ChannelConfig config;

    // Move constructor (needed for std::vector with atomics)
    ChannelState(ChannelState&& other) noexcept
        : group_index(other.group_index)
        , gain_smoother(other.gain_smoother)
        , pan_left(other.pan_left)
        , pan_right(other.pan_right)
        , mute(other.mute.load())
        , solo(other.solo.load())
        , peak_level(other.peak_level.load())
        , rms_level(other.rms_level.load())
        , clip_count(other.clip_count.load())
        , config(std::move(other.config))
    {
        other.gain_smoother = nullptr;
        other.pan_left = nullptr;
        other.pan_right = nullptr;
    }

    // Default constructor
    ChannelState()
        : group_index(0)
        , gain_smoother(nullptr)
        , pan_left(nullptr)
        , pan_right(nullptr)
        , mute(false)
        , solo(false)
        , peak_level(0.0f)
        , rms_level(0.0f)
        , clip_count(0)
    {}

    // Deleted copy constructor (atomics are not copyable)
    ChannelState(const ChannelState&) = delete;
    ChannelState& operator=(const ChannelState&) = delete;
    ChannelState& operator=(ChannelState&&) = delete;
};

/// Internal group state (audio thread)
struct GroupState {
    GainSmoother* gain_smoother; ///< Gain smoothing
    std::atomic<bool> mute;
    std::atomic<bool> solo;

    // Metering
    std::atomic<float> peak_level;
    std::atomic<float> rms_level;
    std::atomic<uint32_t> clip_count;

    // Configuration
    GroupConfig config;

    // Move constructor (needed for std::vector with atomics)
    GroupState(GroupState&& other) noexcept
        : gain_smoother(other.gain_smoother)
        , mute(other.mute.load())
        , solo(other.solo.load())
        , peak_level(other.peak_level.load())
        , rms_level(other.rms_level.load())
        , clip_count(other.clip_count.load())
        , config(std::move(other.config))
    {
        other.gain_smoother = nullptr;
    }

    // Default constructor
    GroupState()
        : gain_smoother(nullptr)
        , mute(false)
        , solo(false)
        , peak_level(0.0f)
        , rms_level(0.0f)
        , clip_count(0)
    {}

    // Deleted copy constructor (atomics are not copyable)
    GroupState(const GroupState&) = delete;
    GroupState& operator=(const GroupState&) = delete;
    GroupState& operator=(GroupState&&) = delete;
};

/// Routing matrix implementation
class RoutingMatrix : public IRoutingMatrix {
public:
    RoutingMatrix();
    ~RoutingMatrix() override;

    // IRoutingMatrix interface
    SessionGraphError initialize(const RoutingConfig& config) override;
    RoutingConfig getConfig() const override;
    void setCallback(IRoutingCallback* callback) override;

    // Channel configuration
    SessionGraphError setChannelGroup(uint8_t channel_index, uint8_t group_index) override;
    SessionGraphError setChannelGain(uint8_t channel_index, float gain_db) override;
    SessionGraphError setChannelPan(uint8_t channel_index, float pan) override;
    SessionGraphError setChannelMute(uint8_t channel_index, bool mute) override;
    SessionGraphError setChannelSolo(uint8_t channel_index, bool solo) override;
    SessionGraphError configureChannel(uint8_t channel_index, const ChannelConfig& config) override;

    // Group configuration
    SessionGraphError setGroupGain(uint8_t group_index, float gain_db) override;
    SessionGraphError setGroupMute(uint8_t group_index, bool mute) override;
    SessionGraphError setGroupSolo(uint8_t group_index, bool solo) override;
    SessionGraphError configureGroup(uint8_t group_index, const GroupConfig& config) override;

    // Master configuration
    SessionGraphError setMasterGain(float gain_db) override;
    SessionGraphError setMasterMute(bool mute) override;

    // State queries
    bool isSoloActive() const override;
    bool isChannelMuted(uint8_t channel_index) const override;
    bool isGroupMuted(uint8_t group_index) const override;
    AudioMeter getChannelMeter(uint8_t channel_index) const override;
    AudioMeter getGroupMeter(uint8_t group_index) const override;
    AudioMeter getMasterMeter() const override;

    // Snapshots
    RoutingSnapshot saveSnapshot(const std::string& name) override;
    SessionGraphError loadSnapshot(const RoutingSnapshot& snapshot) override;
    SessionGraphError reset() override;

    // Audio processing
    SessionGraphError processRouting(
        const float* const* channel_inputs,
        float** master_output,
        uint32_t num_frames
    ) override;

private:
    // Internal helpers
    void initializeChannels();
    void initializeGroups();
    void cleanupChannels();
    void cleanupGroups();

    void updateSoloState();
    void updatePanLaw(uint8_t channel_index, float pan);

    float dbToLinear(float db) const;
    float linearToDb(float linear) const;

    void processMetering(float* buffer, size_t num_frames, std::atomic<float>& peak, std::atomic<float>& rms);
    bool detectClipping(float* buffer, size_t num_frames);

    // Configuration
    RoutingConfig m_config;
    std::atomic<bool> m_initialized{false};

    // Channel and group states
    std::vector<ChannelState> m_channels;
    std::vector<GroupState> m_groups;

    // Master output
    GainSmoother* m_master_gain_smoother;
    std::atomic<bool> m_master_mute;
    std::atomic<float> m_master_peak;
    std::atomic<float> m_master_rms;
    std::atomic<uint32_t> m_master_clip_count;

    // Solo state
    std::atomic<bool> m_solo_active;

    // Callback
    IRoutingCallback* m_callback;

    // Thread safety
    mutable std::mutex m_config_mutex;

    // Audio processing buffers (pre-allocated)
    std::vector<std::vector<float>> m_group_buffers;  // [num_groups][max_buffer_size]
    std::vector<float> m_temp_buffer;                  // Temp buffer for processing

    static constexpr size_t MAX_BUFFER_SIZE = 2048;
    static constexpr uint8_t UNASSIGNED_GROUP = 255;
};

} // namespace orpheus
