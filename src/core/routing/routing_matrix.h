// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/routing_matrix.h>

#include "true_peak_meter.h"

#include <array>
#include <atomic>
#include <mutex>
#include <vector>

namespace orpheus {

// Forward declaration for gain smoother
class GainSmoother;

/// Internal channel state (audio thread)
struct ChannelState {
  RoutingGroupIndex group_index;               ///< Assigned group or UNASSIGNED_GROUP
  std::unique_ptr<GainSmoother> gain_smoother; ///< Gain smoothing
  std::unique_ptr<GainSmoother> pan_left;      ///< Left pan gain
  std::unique_ptr<GainSmoother> pan_right;     ///< Right pan gain
  std::atomic<bool> mute;
  std::atomic<bool> solo;

  // Metering
  std::atomic<float> peak_level;
  std::atomic<float> rms_level;
  std::atomic<uint32_t> clip_count;
  std::array<TruePeakMeter, 2> true_peak_meters;

  // Configuration (UI thread writes, audio thread reads)
  ChannelConfig config;

  // Move constructor (needed for std::vector with atomics)
  ChannelState(ChannelState&& other) noexcept
      : group_index(other.group_index), gain_smoother(std::move(other.gain_smoother)),
        pan_left(std::move(other.pan_left)), pan_right(std::move(other.pan_right)),
        mute(other.mute.load()), solo(other.solo.load()), peak_level(other.peak_level.load()),
        rms_level(other.rms_level.load()), clip_count(other.clip_count.load()),
        true_peak_meters(std::move(other.true_peak_meters)), config(std::move(other.config)) {}

  // Default constructor
  ChannelState()
      : group_index(0), gain_smoother(nullptr), pan_left(nullptr), pan_right(nullptr), mute(false),
        solo(false), peak_level(0.0f), rms_level(0.0f), clip_count(0) {}

  // Deleted copy constructor (atomics are not copyable)
  ChannelState(const ChannelState&) = delete;
  ChannelState& operator=(const ChannelState&) = delete;
  ChannelState& operator=(ChannelState&&) = delete;
};

/// Internal group state (audio thread)
struct GroupState {
  std::unique_ptr<GainSmoother> gain_smoother; ///< Gain smoothing
  std::atomic<bool> mute;
  std::atomic<bool> solo;
  std::atomic<RoutingOutputIndex> output_start;
  std::atomic<uint16_t> output_width;

  // Metering
  std::atomic<float> peak_level;
  std::atomic<float> rms_level;
  std::atomic<uint32_t> clip_count;
  std::array<TruePeakMeter, 2> true_peak_meters;

  // Configuration
  GroupConfig config;

  // Move constructor (needed for std::vector with atomics)
  GroupState(GroupState&& other) noexcept
      : gain_smoother(std::move(other.gain_smoother)), mute(other.mute.load()),
        solo(other.solo.load()), output_start(other.output_start.load()),
        output_width(other.output_width.load()), peak_level(other.peak_level.load()),
        rms_level(other.rms_level.load()), clip_count(other.clip_count.load()),
        true_peak_meters(std::move(other.true_peak_meters)),
        config(std::move(other.config)) {}

  // Default constructor
  GroupState()
      : gain_smoother(nullptr), mute(false), solo(false), output_start(0),
        output_width(2), peak_level(0.0f), rms_level(0.0f), clip_count(0) {}

  // Deleted copy constructor (atomics are not copyable)
  GroupState(const GroupState&) = delete;
  GroupState& operator=(const GroupState&) = delete;
  GroupState& operator=(GroupState&&) = delete;
};

/// Preallocated planar buffer for one logical group.
struct MultichannelGroupBuffer {
  std::vector<std::vector<float>> channels;

  void resize(size_t channel_count, size_t frames) {
    channels.assign(channel_count, std::vector<float>(frames, 0.0f));
  }

  void clear() {
    for (auto& channel : channels) {
      std::fill(channel.begin(), channel.end(), 0.0f);
    }
  }
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
  SessionGraphError setChannelGroup(RoutingChannelIndex channel_index,
                                    RoutingGroupIndex group_index) override;
  SessionGraphError setChannelRoute(RoutingChannelIndex channel_index,
                                    RoutingGroupIndex group_index,
                                    RoutingOutputIndex output_index) override;
  SessionGraphError setChannelGain(RoutingChannelIndex channel_index, float gain_db) override;
  SessionGraphError setChannelPan(RoutingChannelIndex channel_index, float pan) override;
  SessionGraphError setChannelMute(RoutingChannelIndex channel_index, bool mute) override;
  SessionGraphError setChannelSolo(RoutingChannelIndex channel_index, bool solo) override;
  SessionGraphError configureChannel(RoutingChannelIndex channel_index,
                                     const ChannelConfig& config) override;

  // Group configuration
  SessionGraphError setGroupGain(RoutingGroupIndex group_index, float gain_db) override;
  SessionGraphError setGroupMute(RoutingGroupIndex group_index, bool mute) override;
  SessionGraphError setGroupSolo(RoutingGroupIndex group_index, bool solo) override;
  SessionGraphError configureGroup(RoutingGroupIndex group_index,
                                   const GroupConfig& config) override;

  SessionGraphError setGroupOutputRoute(
      RoutingGroupIndex group_index, RoutingOutputIndex output_start,
      uint16_t output_width) override;

  // Master configuration
  SessionGraphError setMasterGain(float gain_db) override;
  SessionGraphError setMasterMute(bool mute) override;

  // State queries
  bool isSoloActive() const override;
  bool isChannelMuted(RoutingChannelIndex channel_index) const override;
  bool isGroupMuted(RoutingGroupIndex group_index) const override;
  AudioMeter getChannelMeter(RoutingChannelIndex channel_index) const override;
  AudioMeter getGroupMeter(RoutingGroupIndex group_index) const override;
  AudioMeter getMasterMeter() const override;

  // Snapshots
  RoutingSnapshot saveSnapshot(const std::string& name,
                               RoutingSnapshotContext context = {}) override;
  SessionGraphError loadSnapshot(const RoutingSnapshot& snapshot) override;
  SessionGraphError reset() override;

  // Audio processing
  SessionGraphError processRouting(const float* const* channel_inputs,
                                   float* const* master_output, uint32_t num_frames) override;
  uint32_t maxBlockFrames() const override;

private:
  // FTR028: Process a single slice of at most MAX_BUFFER_SIZE frames. The
  // public processRouting() loops over this for arbitrarily large blocks;
  // this stays allocation-free and lock-free (operates on pre-allocated
  // scratch), so large offline blocks incur no audio-thread allocation.
  SessionGraphError processRoutingBlock(const float* const* channel_inputs,
                                        float* const* master_output, uint32_t num_frames);

  // Internal helpers
  void initializeChannels();
  void initializeGroups();
  void cleanupChannels();
  void cleanupGroups();

  void updateSoloState();
  void updatePanLaw(RoutingChannelIndex channel_index, float pan);

  float dbToLinear(float db) const;
  float linearToDb(float linear) const;

  // ORP121 Q-05: Headroom compensation
  float getHeadroomCompensation(RoutingGroupIndex group_index) const;
  uint32_t countActiveChannelsInGroup(RoutingGroupIndex group_index) const;
  uint32_t countTotalActiveChannels() const;

  void processStereoMetering(const float* left, const float* right, size_t num_frames,
                             std::array<TruePeakMeter, 2>& true_peak_meters,
                             std::atomic<float>& peak, std::atomic<float>& rms);
  void publishChannelMeterSilence(ChannelState& channel);
  bool detectClipping(float* buffer, size_t num_frames);

  // Configuration (lock-free double-buffer pattern)
  RoutingConfig m_config_buffers[2];
  std::atomic<int> m_active_config_idx{0}; // 0 or 1, for lock-free reads
  std::atomic<bool> m_initialized{false};

  // Channel and group states
  std::vector<ChannelState> m_channels;
  std::vector<GroupState> m_groups;

  // Master output
  std::unique_ptr<GainSmoother> m_master_gain_smoother;
  std::atomic<bool> m_master_mute;
  std::atomic<float> m_master_peak;
  std::atomic<float> m_master_rms;
  std::atomic<uint32_t> m_master_clip_count;
  std::array<TruePeakMeter, 2> m_master_true_peak_meters;

  // Solo domains are independent: a group solo must not mute every source
  // channel before group summing, and a channel solo must not mute every
  // logical group after summing. m_solo_active is their public union.
  std::atomic<bool> m_channel_solo_active{false};
  std::atomic<bool> m_group_solo_active{false};
  std::atomic<bool> m_solo_active{false};
  std::atomic<uint64_t> m_snapshot_revision{0};

  // Callback
  IRoutingCallback* m_callback;

  // Audio processing buffers, allocated once during initialize().
  std::vector<MultichannelGroupBuffer> m_group_buffers;
  MultichannelGroupBuffer m_channel_meter_buffer;
  std::vector<float> m_temp_buffer;

  // FTR028: Internal slice size. Kept in lock-step with the public
  // orpheus::kRoutingSliceFrames contract so hosts and the implementation
  // agree on the chunking granularity.
  static constexpr size_t MAX_BUFFER_SIZE = kRoutingSliceFrames;
  static constexpr RoutingGroupIndex UNASSIGNED_GROUP = orpheus::UNASSIGNED_GROUP;
};

} // namespace orpheus
