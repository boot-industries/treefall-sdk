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
  uint8_t group_index;                         ///< Assigned group (255 = unassigned)
  std::unique_ptr<GainSmoother> gain_smoother; ///< Gain smoothing
  std::unique_ptr<GainSmoother> pan_left;      ///< Left pan gain
  std::unique_ptr<GainSmoother> pan_right;     ///< Right pan gain
  std::atomic<bool> mute;
  std::atomic<bool> solo;

  // Metering
  std::atomic<float> peak_level;
  std::atomic<float> rms_level;
  std::atomic<uint32_t> clip_count;
  TruePeakMeter true_peak_meter; ///< ORP121 Q-04: True-peak metering

  // Configuration (UI thread writes, audio thread reads)
  ChannelConfig config;

  // Move constructor (needed for std::vector with atomics)
  ChannelState(ChannelState&& other) noexcept
      : group_index(other.group_index), gain_smoother(std::move(other.gain_smoother)),
        pan_left(std::move(other.pan_left)), pan_right(std::move(other.pan_right)),
        mute(other.mute.load()), solo(other.solo.load()), peak_level(other.peak_level.load()),
        rms_level(other.rms_level.load()), clip_count(other.clip_count.load()),
        true_peak_meter(std::move(other.true_peak_meter)), config(std::move(other.config)) {}

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

  // Metering
  std::atomic<float> peak_level;
  std::atomic<float> rms_level;
  std::atomic<uint32_t> clip_count;
  TruePeakMeter true_peak_meter; ///< ORP121 Q-04: True-peak metering

  // Configuration
  GroupConfig config;

  // Move constructor (needed for std::vector with atomics)
  GroupState(GroupState&& other) noexcept
      : gain_smoother(std::move(other.gain_smoother)), mute(other.mute.load()),
        solo(other.solo.load()), peak_level(other.peak_level.load()),
        rms_level(other.rms_level.load()), clip_count(other.clip_count.load()),
        true_peak_meter(std::move(other.true_peak_meter)), config(std::move(other.config)) {}

  // Default constructor
  GroupState()
      : gain_smoother(nullptr), mute(false), solo(false), peak_level(0.0f), rms_level(0.0f),
        clip_count(0) {}

  // Deleted copy constructor (atomics are not copyable)
  GroupState(const GroupState&) = delete;
  GroupState& operator=(const GroupState&) = delete;
  GroupState& operator=(GroupState&&) = delete;
};

/// ORP121 A-02: Stereo group buffer for true stereo imaging
/// Each group has L/R channel pair instead of mono
struct StereoGroupBuffer {
  std::vector<float> left;  ///< Left channel samples
  std::vector<float> right; ///< Right channel samples

  void resize(size_t frames) {
    left.resize(frames, 0.0f);
    right.resize(frames, 0.0f);
  }

  void clear() {
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
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
  SessionGraphError processRouting(const float* const* channel_inputs, float** master_output,
                                   uint32_t num_frames) override;
  uint32_t maxBlockFrames() const override;

private:
  // FTR028: Process a single slice of at most MAX_BUFFER_SIZE frames. The
  // public processRouting() loops over this for arbitrarily large blocks;
  // this stays allocation-free and lock-free (operates on pre-allocated
  // scratch), so large offline blocks incur no audio-thread allocation.
  SessionGraphError processRoutingBlock(const float* const* channel_inputs, float** master_output,
                                        uint32_t num_frames);

  // Internal helpers
  void initializeChannels();
  void initializeGroups();
  void cleanupChannels();
  void cleanupGroups();

  void updateSoloState();
  void updatePanLaw(uint8_t channel_index, float pan);

  float dbToLinear(float db) const;
  float linearToDb(float linear) const;

  // ORP121 Q-05: Headroom compensation
  float getHeadroomCompensation(uint8_t group_index) const;
  uint8_t countActiveChannelsInGroup(uint8_t group_index) const;
  uint8_t countTotalActiveChannels() const;

  void processMetering(float* buffer, size_t num_frames, std::atomic<float>& peak,
                       std::atomic<float>& rms);
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
  TruePeakMeter m_master_true_peak_meter; ///< ORP121 Q-04: Master true-peak metering

  // Solo state
  std::atomic<bool> m_solo_active;

  // Callback
  IRoutingCallback* m_callback;

  // ORP121 A-02: Audio processing buffers (pre-allocated, stereo)
  std::vector<StereoGroupBuffer> m_group_buffers; // [num_groups] stereo L/R pairs
  std::vector<float> m_temp_buffer;               // Temp buffer for processing

  // FTR028: Internal slice size. Kept in lock-step with the public
  // orpheus::kRoutingSliceFrames contract so hosts and the implementation
  // agree on the chunking granularity.
  static constexpr size_t MAX_BUFFER_SIZE = kRoutingSliceFrames;
  static constexpr uint8_t UNASSIGNED_GROUP = 255;
};

} // namespace orpheus
