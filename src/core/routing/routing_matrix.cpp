// SPDX-License-Identifier: MIT
#include "routing_matrix.h"
#include "gain_smoother.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace orpheus {

// ============================================================================
// RoutingMatrix Implementation
// ============================================================================

RoutingMatrix::RoutingMatrix()
    : m_initialized(false), m_master_gain_smoother(nullptr), m_master_mute(false),
      m_master_peak(0.0f), m_master_rms(0.0f), m_master_clip_count(0), m_solo_active(false),
      m_callback(nullptr) {}

RoutingMatrix::~RoutingMatrix() {
  // Vectors and unique_ptrs handle cleanup automatically
}

// ============================================================================
// Initialization
// ============================================================================

SessionGraphError RoutingMatrix::initialize(const RoutingConfig& config) {
  // Validate configuration
  if (config.num_channels == 0 || config.num_channels > 64) {
    return SessionGraphError::InvalidParameter;
  }
  if (config.num_groups == 0 || config.num_groups > 16) {
    return SessionGraphError::InvalidParameter;
  }
  if (config.num_outputs < 2 || config.num_outputs > 32) {
    return SessionGraphError::InvalidParameter;
  }

  // Clean up existing state if reinitializing
  if (m_initialized.load(std::memory_order_acquire)) {
    m_channels.clear();
    m_groups.clear();
  }

  // Store configuration (lock-free: write to inactive buffer, then atomic swap)
  int write_idx = 1 - m_active_config_idx.load(std::memory_order_acquire);
  m_config_buffers[write_idx] = config;
  m_active_config_idx.store(write_idx, std::memory_order_release);

  // Initialize channels
  initializeChannels();

  // Initialize groups
  initializeGroups();

  // Initialize master gain smoother
  // ORP121 Q-03: Use sample_rate from config (no hardcoded assumptions)
  m_master_gain_smoother =
      std::make_unique<GainSmoother>(config.sample_rate, config.gain_smoothing_ms);
  m_master_gain_smoother->reset(1.0f); // Unity gain

  // ORP121 A-02: Pre-allocate stereo audio processing buffers
  m_group_buffers.clear();
  m_group_buffers.resize(config.num_groups);
  for (auto& buffer : m_group_buffers) {
    buffer.resize(MAX_BUFFER_SIZE); // Allocates both L and R channels
  }

  m_temp_buffer.clear();
  m_temp_buffer.resize(MAX_BUFFER_SIZE, 0.0f);

  // Reset metering
  m_master_peak.store(0.0f, std::memory_order_release);
  m_master_rms.store(0.0f, std::memory_order_release);
  m_master_clip_count.store(0, std::memory_order_release);

  m_initialized.store(true, std::memory_order_release);

  return SessionGraphError::OK;
}

RoutingConfig RoutingMatrix::getConfig() const {
  // Lock-free read from active config buffer
  int read_idx = m_active_config_idx.load(std::memory_order_acquire);
  return m_config_buffers[read_idx];
}

void RoutingMatrix::setCallback(IRoutingCallback* callback) {
  m_callback = callback;
}

// ============================================================================
// Channel Configuration
// ============================================================================

SessionGraphError RoutingMatrix::setChannelGroup(uint8_t channel_index, uint8_t group_index) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }

  if (channel_index >= m_channels.size()) {
    return SessionGraphError::InvalidParameter;
  }

  if (group_index != UNASSIGNED_GROUP && group_index >= m_groups.size()) {
    return SessionGraphError::InvalidParameter;
  }

  // Lock-free update (atomic write)
  m_channels[channel_index].group_index = group_index;
  m_channels[channel_index].config.group_index = group_index;

  return SessionGraphError::OK;
}

SessionGraphError RoutingMatrix::setChannelGain(uint8_t channel_index, float gain_db) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }

  if (channel_index >= m_channels.size()) {
    return SessionGraphError::InvalidParameter;
  }

  // Clamp to valid range
  gain_db = std::clamp(gain_db, -100.0f, 12.0f);

  // Convert to linear and set target (lock-free)
  float gain_linear = dbToLinear(gain_db);
  m_channels[channel_index].gain_smoother->setTarget(gain_linear);
  m_channels[channel_index].config.gain_db = gain_db;

  // Notify callback
  if (m_callback) {
    m_callback->onChannelGainChanged(channel_index, gain_db);
  }

  return SessionGraphError::OK;
}

SessionGraphError RoutingMatrix::setChannelPan(uint8_t channel_index, float pan) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }

  if (channel_index >= m_channels.size()) {
    return SessionGraphError::InvalidParameter;
  }

  // Clamp to valid range
  pan = std::clamp(pan, -1.0f, 1.0f);

  // Update pan law (constant-power, -3 dB at center)
  updatePanLaw(channel_index, pan);

  m_channels[channel_index].config.pan = pan;

  return SessionGraphError::OK;
}

SessionGraphError RoutingMatrix::setChannelMute(uint8_t channel_index, bool mute) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }

  if (channel_index >= m_channels.size()) {
    return SessionGraphError::InvalidParameter;
  }

  // Atomic update (lock-free)
  m_channels[channel_index].mute.store(mute, std::memory_order_release);
  m_channels[channel_index].config.mute = mute;

  return SessionGraphError::OK;
}

SessionGraphError RoutingMatrix::setChannelSolo(uint8_t channel_index, bool solo) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }

  if (channel_index >= m_channels.size()) {
    return SessionGraphError::InvalidParameter;
  }

  // Atomic update (lock-free)
  m_channels[channel_index].solo.store(solo, std::memory_order_release);
  m_channels[channel_index].config.solo = solo;

  // Update global solo state
  updateSoloState();

  return SessionGraphError::OK;
}

SessionGraphError RoutingMatrix::configureChannel(uint8_t channel_index,
                                                  const ChannelConfig& config) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }

  if (channel_index >= m_channels.size()) {
    return SessionGraphError::InvalidParameter;
  }

  // Batch update all parameters
  setChannelGroup(channel_index, config.group_index);
  setChannelGain(channel_index, config.gain_db);
  setChannelPan(channel_index, config.pan);
  setChannelMute(channel_index, config.mute);
  setChannelSolo(channel_index, config.solo);

  m_channels[channel_index].config.name = config.name;
  m_channels[channel_index].config.color = config.color;

  return SessionGraphError::OK;
}

// ============================================================================
// Group Configuration
// ============================================================================

SessionGraphError RoutingMatrix::setGroupGain(uint8_t group_index, float gain_db) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }

  if (group_index >= m_groups.size()) {
    return SessionGraphError::InvalidParameter;
  }

  // Clamp to valid range
  gain_db = std::clamp(gain_db, -100.0f, 12.0f);

  // Convert to linear and set target (lock-free)
  float gain_linear = dbToLinear(gain_db);
  m_groups[group_index].gain_smoother->setTarget(gain_linear);
  m_groups[group_index].config.gain_db = gain_db;

  // Notify callback
  if (m_callback) {
    m_callback->onGroupGainChanged(group_index, gain_db);
  }

  return SessionGraphError::OK;
}

SessionGraphError RoutingMatrix::setGroupMute(uint8_t group_index, bool mute) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }

  if (group_index >= m_groups.size()) {
    return SessionGraphError::InvalidParameter;
  }

  // Atomic update (lock-free)
  m_groups[group_index].mute.store(mute, std::memory_order_release);
  m_groups[group_index].config.mute = mute;

  return SessionGraphError::OK;
}

SessionGraphError RoutingMatrix::setGroupSolo(uint8_t group_index, bool solo) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }

  if (group_index >= m_groups.size()) {
    return SessionGraphError::InvalidParameter;
  }

  // Atomic update (lock-free)
  m_groups[group_index].solo.store(solo, std::memory_order_release);
  m_groups[group_index].config.solo = solo;

  // Update global solo state
  updateSoloState();

  return SessionGraphError::OK;
}

SessionGraphError RoutingMatrix::configureGroup(uint8_t group_index, const GroupConfig& config) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }

  if (group_index >= m_groups.size()) {
    return SessionGraphError::InvalidParameter;
  }

  // Batch update all parameters
  setGroupGain(group_index, config.gain_db);
  setGroupMute(group_index, config.mute);
  setGroupSolo(group_index, config.solo);

  m_groups[group_index].config.name = config.name;
  m_groups[group_index].config.output_bus = config.output_bus;
  m_groups[group_index].config.color = config.color;

  return SessionGraphError::OK;
}

// ============================================================================
// Master Configuration
// ============================================================================

SessionGraphError RoutingMatrix::setMasterGain(float gain_db) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }

  // Clamp to valid range
  gain_db = std::clamp(gain_db, -100.0f, 12.0f);

  // Convert to linear and set target (lock-free)
  float gain_linear = dbToLinear(gain_db);
  m_master_gain_smoother->setTarget(gain_linear);

  return SessionGraphError::OK;
}

SessionGraphError RoutingMatrix::setMasterMute(bool mute) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }

  // Atomic update (lock-free)
  m_master_mute.store(mute, std::memory_order_release);

  return SessionGraphError::OK;
}

// ============================================================================
// State Queries
// ============================================================================

bool RoutingMatrix::isSoloActive() const {
  return m_solo_active.load(std::memory_order_acquire);
}

bool RoutingMatrix::isChannelMuted(uint8_t channel_index) const {
  if (channel_index >= m_channels.size()) {
    return true;
  }

  bool is_muted = m_channels[channel_index].mute.load(std::memory_order_acquire);
  bool is_solo = m_channels[channel_index].solo.load(std::memory_order_acquire);
  bool solo_active = m_solo_active.load(std::memory_order_acquire);

  // If solo is active and this channel is not solo'd, it's effectively muted
  if (solo_active && !is_solo) {
    return true;
  }

  return is_muted;
}

bool RoutingMatrix::isGroupMuted(uint8_t group_index) const {
  if (group_index >= m_groups.size()) {
    return true;
  }

  bool is_muted = m_groups[group_index].mute.load(std::memory_order_acquire);
  bool is_solo = m_groups[group_index].solo.load(std::memory_order_acquire);
  bool solo_active = m_solo_active.load(std::memory_order_acquire);

  // If solo is active and this group is not solo'd, it's effectively muted
  if (solo_active && !is_solo) {
    return true;
  }

  return is_muted;
}

AudioMeter RoutingMatrix::getChannelMeter(uint8_t channel_index) const {
  AudioMeter meter;

  if (channel_index >= m_channels.size()) {
    return meter;
  }

  float peak = m_channels[channel_index].peak_level.load(std::memory_order_acquire);
  float rms = m_channels[channel_index].rms_level.load(std::memory_order_acquire);
  uint32_t clip_count = m_channels[channel_index].clip_count.load(std::memory_order_acquire);

  meter.peak_db = linearToDb(peak);
  meter.rms_db = linearToDb(rms);
  meter.clipping = (clip_count > 0);
  meter.clip_count = clip_count;

  return meter;
}

AudioMeter RoutingMatrix::getGroupMeter(uint8_t group_index) const {
  AudioMeter meter;

  if (group_index >= m_groups.size()) {
    return meter;
  }

  float peak = m_groups[group_index].peak_level.load(std::memory_order_acquire);
  float rms = m_groups[group_index].rms_level.load(std::memory_order_acquire);
  uint32_t clip_count = m_groups[group_index].clip_count.load(std::memory_order_acquire);

  meter.peak_db = linearToDb(peak);
  meter.rms_db = linearToDb(rms);
  meter.clipping = (clip_count > 0);
  meter.clip_count = clip_count;

  return meter;
}

AudioMeter RoutingMatrix::getMasterMeter() const {
  AudioMeter meter;

  float peak = m_master_peak.load(std::memory_order_acquire);
  float rms = m_master_rms.load(std::memory_order_acquire);
  uint32_t clip_count = m_master_clip_count.load(std::memory_order_acquire);

  meter.peak_db = linearToDb(peak);
  meter.rms_db = linearToDb(rms);
  meter.clipping = (clip_count > 0);
  meter.clip_count = clip_count;

  return meter;
}

// ============================================================================
// Snapshots
// ============================================================================

RoutingSnapshot RoutingMatrix::saveSnapshot(const std::string& name) {
  RoutingSnapshot snapshot;
  snapshot.name = name;
  snapshot.timestamp_ms = 0; // TODO: Get actual timestamp

  // Save channel states
  for (const auto& channel : m_channels) {
    snapshot.channels.push_back(channel.config);
  }

  // Save group states
  for (const auto& group : m_groups) {
    snapshot.groups.push_back(group.config);
  }

  // Save master state
  snapshot.master_gain_db = linearToDb(m_master_gain_smoother->getCurrent());
  snapshot.master_mute = m_master_mute.load(std::memory_order_acquire);

  return snapshot;
}

SessionGraphError RoutingMatrix::loadSnapshot(const RoutingSnapshot& snapshot) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }

  // Validate snapshot compatibility
  if (snapshot.channels.size() != m_channels.size()) {
    return SessionGraphError::InvalidParameter;
  }
  if (snapshot.groups.size() != m_groups.size()) {
    return SessionGraphError::InvalidParameter;
  }

  // Load channel states
  for (size_t i = 0; i < snapshot.channels.size(); ++i) {
    configureChannel(static_cast<uint8_t>(i), snapshot.channels[i]);
  }

  // Load group states
  for (size_t i = 0; i < snapshot.groups.size(); ++i) {
    configureGroup(static_cast<uint8_t>(i), snapshot.groups[i]);
  }

  // Load master state
  setMasterGain(snapshot.master_gain_db);
  setMasterMute(snapshot.master_mute);

  return SessionGraphError::OK;
}

SessionGraphError RoutingMatrix::reset() {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }

  // Reset all channels to default
  for (size_t i = 0; i < m_channels.size(); ++i) {
    ChannelConfig default_config;
    configureChannel(static_cast<uint8_t>(i), default_config);
  }

  // Reset all groups to default
  for (size_t i = 0; i < m_groups.size(); ++i) {
    GroupConfig default_config;
    configureGroup(static_cast<uint8_t>(i), default_config);
  }

  // Reset master
  setMasterGain(0.0f); // Unity gain
  setMasterMute(false);

  return SessionGraphError::OK;
}

// ============================================================================
// Audio Processing
// ============================================================================

SessionGraphError RoutingMatrix::processRouting(const float* const* channel_inputs,
                                                float** master_output, uint32_t num_frames) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }

  if (num_frames > MAX_BUFFER_SIZE) {
    return SessionGraphError::InvalidParameter;
  }

  // Get active config (lock-free read)
  int config_idx = m_active_config_idx.load(std::memory_order_acquire);
  const RoutingConfig& config = m_config_buffers[config_idx];

  // ========================================================================
  // Step 1: Clear stereo group buffers (ORP121 A-02)
  // ========================================================================
  for (uint8_t grp = 0; grp < config.num_groups; ++grp) {
    m_group_buffers[grp].clear();
  }

  // ========================================================================
  // Step 2: Process channels → groups (ORP121 A-02/C-04: Stereo with pan law)
  // ========================================================================
  // Note: Use Instruments/perf for audio thread profiling, not file I/O

  for (uint8_t ch = 0; ch < config.num_channels; ++ch) {
    auto& channel = m_channels[ch];
    uint8_t group_idx = channel.group_index;

    // Skip if unassigned or group index invalid
    if (group_idx == UNASSIGNED_GROUP || group_idx >= config.num_groups) {
      continue;
    }

    // Check if channel is effectively muted (explicit mute or solo)
    if (isChannelMuted(ch)) {
      // Still need to advance smoothers to stay in sync
      for (uint32_t frame = 0; frame < num_frames; ++frame) {
        channel.gain_smoother->process();
        channel.pan_left->process();
        channel.pan_right->process();
      }
      continue;
    }

    // Get input buffer for this channel
    const float* input = channel_inputs[ch];
    if (!input) {
      // No input for this channel - advance smoothers and skip
      for (uint32_t frame = 0; frame < num_frames; ++frame) {
        channel.gain_smoother->process();
        channel.pan_left->process();
        channel.pan_right->process();
      }
      continue;
    }

    // Get stereo group buffer (ORP121 A-02)
    auto& group_buffer = m_group_buffers[group_idx];

    // ORP121 A-02/C-04: Process channel gain + pan → stereo group buffer
    // Pan law: constant-power (-3 dB at center)
    // pan_left/pan_right are pre-computed in updatePanLaw()
    for (uint32_t frame = 0; frame < num_frames; ++frame) {
      // Get smoothed gain and pan values for this sample
      float channel_gain = channel.gain_smoother->process();
      float pan_left = channel.pan_left->process();
      float pan_right = channel.pan_right->process();

      // Read input sample and apply channel gain
      float sample = input[frame] * channel_gain;

      // ORP121 C-04: Apply pan law (constant-power stereo positioning)
      // pan_left/pan_right encode the constant-power coefficients:
      // - Center (pan=0): L=R=0.707 (-3 dB each, total energy preserved)
      // - Hard left (pan=-1): L=1.0, R=0.0
      // - Hard right (pan=+1): L=0.0, R=1.0
      float sample_L = sample * pan_left;
      float sample_R = sample * pan_right;

      // Sum into stereo group buffer
      group_buffer.left[frame] += sample_L;
      group_buffer.right[frame] += sample_R;
    }

    // Update channel meters using left channel (if enabled)
    // TODO: Proper stereo metering would meter both channels
    if (config.enable_metering) {
      processMetering(group_buffer.left.data(), num_frames, channel.peak_level, channel.rms_level);
      if (detectClipping(group_buffer.left.data(), num_frames) ||
          detectClipping(group_buffer.right.data(), num_frames)) {
        channel.clip_count.fetch_add(1, std::memory_order_relaxed);
      }
    }
  }

  // ========================================================================
  // Step 3: Process groups → master (ORP121 A-02: Stereo groups)
  // ========================================================================
  // Clear master output first
  for (uint8_t out = 0; out < config.num_outputs; ++out) {
    std::memset(master_output[out], 0, num_frames * sizeof(float));
  }

  for (uint8_t grp = 0; grp < config.num_groups; ++grp) {
    auto& group = m_groups[grp];

    // Check if group is effectively muted
    if (isGroupMuted(grp)) {
      // Still need to advance gain smoother to stay in sync
      for (uint32_t frame = 0; frame < num_frames; ++frame) {
        group.gain_smoother->process();
      }
      continue;
    }

    // Get stereo group buffer (ORP121 A-02)
    auto& group_buffer = m_group_buffers[grp];

    // ORP121 A-03: Get output bus assignment (stereo pair)
    // output_bus 0 = channels 0-1 (stereo pair 1)
    // output_bus 1 = channels 2-3 (stereo pair 2)
    // etc.
    uint8_t output_bus = group.config.output_bus;
    uint8_t out_L = output_bus * 2;
    uint8_t out_R = output_bus * 2 + 1;

    // Validate output channels exist
    if (out_R >= config.num_outputs) {
      // Output bus doesn't exist - route to master (0-1)
      out_L = 0;
      out_R = std::min((uint8_t)1, (uint8_t)(config.num_outputs - 1));
    }

    // ORP121 Q-05: Get headroom compensation for this group (computed once per buffer)
    float headroom = getHeadroomCompensation(grp);

    // Process group gain + sum into master (ORP121 A-02: stereo)
    for (uint32_t frame = 0; frame < num_frames; ++frame) {
      // Get smoothed group gain for this sample
      float group_gain = group.gain_smoother->process();

      // Read stereo group samples and apply gain + headroom compensation (Q-05)
      float sample_L = group_buffer.left[frame] * group_gain * headroom;
      float sample_R = group_buffer.right[frame] * group_gain * headroom;

      // Sum into master output at configured output bus
      master_output[out_L][frame] += sample_L;
      if (out_L != out_R) { // Only write R if stereo output
        master_output[out_R][frame] += sample_R;
      }
    }

    // Update group meters using left channel (if enabled)
    // TODO: Proper stereo metering would meter both channels
    if (config.enable_metering) {
      processMetering(group_buffer.left.data(), num_frames, group.peak_level, group.rms_level);
      if (detectClipping(group_buffer.left.data(), num_frames) ||
          detectClipping(group_buffer.right.data(), num_frames)) {
        group.clip_count.fetch_add(1, std::memory_order_relaxed);
      }
    }
  }

  // ========================================================================
  // Step 4: Apply master gain/mute
  // ========================================================================
  bool master_muted = m_master_mute.load(std::memory_order_acquire);

  for (uint32_t frame = 0; frame < num_frames; ++frame) {
    // Get smoothed master gain for this sample
    float master_gain = m_master_gain_smoother->process();

    // Apply mute
    if (master_muted) {
      master_gain = 0.0f;
    }

    // Apply master gain to all output channels
    for (uint8_t out = 0; out < config.num_outputs; ++out) {
      master_output[out][frame] *= master_gain;
    }
  }

  // ========================================================================
  // Step 5: Update master meters (if enabled)
  // ========================================================================
  if (config.enable_metering) {
    // Meter master left channel (or average of all channels)
    processMetering(master_output[0], num_frames, m_master_peak, m_master_rms);

    // Check for clipping on any master channel
    for (uint8_t out = 0; out < config.num_outputs; ++out) {
      if (detectClipping(master_output[out], num_frames)) {
        m_master_clip_count.fetch_add(1, std::memory_order_relaxed);
        break; // Only count once per buffer
      }
    }
  }

  // ========================================================================
  // Step 6: Apply clipping protection (soft limiter + hard clip)
  // ========================================================================
  // OCC109 v0.2.2: Fix "Stop All" distortion when 32 clips fade out simultaneously
  // Soft limiter prevents audible distortion when summed gains exceed 0dBFS
  //
  // ORP121 C-02: Fixed discontinuity in soft-knee limiter
  // Previous implementation had discontinuity at 0.9 threshold causing audible clicks
  // New implementation uses continuous soft-knee compression starting at -2 dBFS
  if (config.enable_clipping_protection) {
    // Soft-knee limiter constants (ORP121 C-02)
    static constexpr float THRESHOLD = 0.794f; // -2 dBFS (10^(-2/20))
    static constexpr float KNEE_WIDTH = 0.3f;  // Soft knee range for gradual compression
    static constexpr float CEILING = 0.9999f;  // Final hard limit (-0.001 dBFS)

    for (uint8_t out = 0; out < config.num_outputs; ++out) {
      for (uint32_t frame = 0; frame < num_frames; ++frame) {
        float sample = master_output[out][frame];
        float abs_sample = std::abs(sample);

        // ORP121 C-02: Continuous soft-knee limiter
        // Uses soft-knee compression starting at -2 dBFS with smooth transition
        // Curve is C1 continuous (no jumps, smooth derivative)
        //
        // Mathematical verification:
        // - At threshold (0.794): excess=0, tanh(0)=0, output=0.794 (continuous)
        // - At 1.0: excess=0.206, tanh(0.687)=0.596, output=0.794+0.179=0.973
        // - At 2.0: excess=1.206, tanh(4.02)=0.999, output clamped to 0.9999
        if (abs_sample > THRESHOLD) {
          float excess = abs_sample - THRESHOLD;
          float knee_ratio = excess / KNEE_WIDTH;
          float compressed = THRESHOLD + std::tanh(knee_ratio) * KNEE_WIDTH;
          sample = std::copysign(std::min(compressed, CEILING), sample);
        }

        master_output[out][frame] = sample;
      }
    }
  }

  return SessionGraphError::OK;
}

// ============================================================================
// Internal Helpers
// ============================================================================

void RoutingMatrix::initializeChannels() {
  // ORP121 Q-03: Get sample rate from config (no hardcoded assumptions)
  // Get active config (lock-free read)
  int config_idx = m_active_config_idx.load(std::memory_order_acquire);
  const RoutingConfig& config = m_config_buffers[config_idx];
  uint32_t sample_rate = config.sample_rate;

  m_channels.clear();
  m_channels.reserve(config.num_channels);

  for (uint8_t i = 0; i < config.num_channels; ++i) {
    ChannelState channel;
    channel.group_index = 0; // Default to group 0
    channel.gain_smoother = std::make_unique<GainSmoother>(sample_rate, config.gain_smoothing_ms);
    channel.gain_smoother->reset(1.0f); // Unity gain

    channel.pan_left = std::make_unique<GainSmoother>(sample_rate, config.gain_smoothing_ms);
    channel.pan_left->reset(0.707f); // -3 dB (constant-power center)

    channel.pan_right = std::make_unique<GainSmoother>(sample_rate, config.gain_smoothing_ms);
    channel.pan_right->reset(0.707f); // -3 dB

    channel.mute.store(false, std::memory_order_release);
    channel.solo.store(false, std::memory_order_release);

    channel.peak_level.store(0.0f, std::memory_order_release);
    channel.rms_level.store(0.0f, std::memory_order_release);
    channel.clip_count.store(0, std::memory_order_release);

    // Default config
    channel.config.name = "Channel " + std::to_string(i + 1);
    channel.config.group_index = 0;
    channel.config.gain_db = 0.0f;
    channel.config.pan = 0.0f;
    channel.config.mute = false;
    channel.config.solo = false;
    channel.config.color = 0xFFFFFFFF;

    m_channels.push_back(std::move(channel));
  }
}

void RoutingMatrix::initializeGroups() {
  // ORP121 Q-03: Get sample rate from config (no hardcoded assumptions)
  // Get active config (lock-free read)
  int config_idx = m_active_config_idx.load(std::memory_order_acquire);
  const RoutingConfig& config = m_config_buffers[config_idx];
  uint32_t sample_rate = config.sample_rate;

  m_groups.clear();
  m_groups.reserve(config.num_groups);

  for (uint8_t i = 0; i < config.num_groups; ++i) {
    GroupState group;
    group.gain_smoother = std::make_unique<GainSmoother>(sample_rate, config.gain_smoothing_ms);
    group.gain_smoother->reset(1.0f); // Unity gain

    group.mute.store(false, std::memory_order_release);
    group.solo.store(false, std::memory_order_release);

    group.peak_level.store(0.0f, std::memory_order_release);
    group.rms_level.store(0.0f, std::memory_order_release);
    group.clip_count.store(0, std::memory_order_release);

    // Default config
    group.config.name = "Group " + std::to_string(i + 1);
    group.config.gain_db = 0.0f;
    group.config.mute = false;
    group.config.solo = false;
    group.config.output_bus = 0; // Master
    group.config.color = 0xFFFFFFFF;

    m_groups.push_back(std::move(group));
  }
}

void RoutingMatrix::updateSoloState() {
  // Check if any channel or group is solo'd
  bool any_solo = false;

  for (const auto& channel : m_channels) {
    if (channel.solo.load(std::memory_order_acquire)) {
      any_solo = true;
      break;
    }
  }

  if (!any_solo) {
    for (const auto& group : m_groups) {
      if (group.solo.load(std::memory_order_acquire)) {
        any_solo = true;
        break;
      }
    }
  }

  m_solo_active.store(any_solo, std::memory_order_release);

  // Notify callback
  if (m_callback) {
    m_callback->onSoloStateChanged(any_solo);
  }
}

void RoutingMatrix::updatePanLaw(uint8_t channel_index, float pan) {
  // Constant-power pan law: L^2 + R^2 = 1
  // Center: -3 dB (0.707) on both channels
  // Hard left: 1.0 L, 0.0 R
  // Hard right: 0.0 L, 1.0 R

  float pan_radians = (pan + 1.0f) * 0.25f * 3.14159265359f; // Map [-1, 1] to [0, π/2]

  float gain_left = std::cos(pan_radians);
  float gain_right = std::sin(pan_radians);

  m_channels[channel_index].pan_left->setTarget(gain_left);
  m_channels[channel_index].pan_right->setTarget(gain_right);
}

void RoutingMatrix::processMetering(float* buffer, size_t num_frames, std::atomic<float>& peak,
                                    std::atomic<float>& rms) {
  // Get current config for metering mode
  int config_idx = m_active_config_idx.load(std::memory_order_acquire);
  const RoutingConfig& config = m_config_buffers[config_idx];

  // Calculate peak based on metering mode
  float peak_value = 0.0f;

  if (config.metering_mode == MeteringMode::TruePeak) {
    // ORP121 Q-04: True-peak metering with 4x oversampling (ITU-R BS.1770-4)
    // Note: We use the master true-peak meter here since processMetering is
    // called for various metering points. For per-channel/group true-peak,
    // the TruePeakMeter instances in ChannelState/GroupState should be used.
    for (size_t i = 0; i < num_frames; ++i) {
      float true_peak = m_master_true_peak_meter.process(buffer[i]);
      if (true_peak > peak_value) {
        peak_value = true_peak;
      }
    }
  } else {
    // Standard peak metering (maximum absolute value)
    for (size_t i = 0; i < num_frames; ++i) {
      float abs_sample = std::abs(buffer[i]);
      if (abs_sample > peak_value) {
        peak_value = abs_sample;
      }
    }
  }

  // Calculate RMS (root mean square) - same for all metering modes
  float sum_squares = 0.0f;
  for (size_t i = 0; i < num_frames; ++i) {
    sum_squares += buffer[i] * buffer[i];
  }
  float rms_value = std::sqrt(sum_squares / static_cast<float>(num_frames));

  // Update atomics (simple replace for now, could use decay for smoother meters)
  peak.store(peak_value, std::memory_order_release);
  rms.store(rms_value, std::memory_order_release);
}

bool RoutingMatrix::detectClipping(float* buffer, size_t num_frames) {
  // Clipping threshold: ≥ 1.0 or ≤ -1.0 (0 dBFS)
  constexpr float CLIPPING_THRESHOLD = 1.0f;

  for (size_t i = 0; i < num_frames; ++i) {
    float abs_sample = std::abs(buffer[i]);
    if (abs_sample >= CLIPPING_THRESHOLD) {
      return true; // Clipping detected
    }
  }

  return false; // No clipping
}

float RoutingMatrix::dbToLinear(float db) const {
  if (db <= -100.0f)
    return 0.0f; // -inf
  return std::pow(10.0f, db / 20.0f);
}

float RoutingMatrix::linearToDb(float linear) const {
  if (linear <= 0.0f)
    return -100.0f; // -inf
  return 20.0f * std::log10(linear);
}

// ============================================================================
// ORP121 Q-05: Headroom Management
// ============================================================================

uint8_t RoutingMatrix::countActiveChannelsInGroup(uint8_t group_index) const {
  uint8_t count = 0;
  for (const auto& channel : m_channels) {
    if (channel.group_index == group_index && !channel.mute.load(std::memory_order_acquire)) {
      ++count;
    }
  }
  return count;
}

uint8_t RoutingMatrix::countTotalActiveChannels() const {
  uint8_t count = 0;
  for (const auto& channel : m_channels) {
    if (channel.group_index != UNASSIGNED_GROUP && !channel.mute.load(std::memory_order_acquire)) {
      ++count;
    }
  }
  return count;
}

float RoutingMatrix::getHeadroomCompensation(uint8_t group_index) const {
  // Get active config (lock-free read)
  int config_idx = m_active_config_idx.load(std::memory_order_acquire);
  const RoutingConfig& config = m_config_buffers[config_idx];

  switch (config.headroom_mode) {
  case HeadroomMode::None:
    return 1.0f;

  case HeadroomMode::PerGroup: {
    uint8_t active = countActiveChannelsInGroup(group_index);
    return active > 0 ? 1.0f / static_cast<float>(active) : 1.0f;
  }

  case HeadroomMode::Global: {
    uint8_t active = countTotalActiveChannels();
    return active > 0 ? 1.0f / static_cast<float>(active) : 1.0f;
  }

  case HeadroomMode::Logarithmic: {
    // -3 dB per doubling of channels: 1/sqrt(n)
    uint8_t active = countActiveChannelsInGroup(group_index);
    return active > 0 ? 1.0f / std::sqrt(static_cast<float>(active)) : 1.0f;
  }

  default:
    return 1.0f;
  }
}

// ============================================================================
// Factory Function
// ============================================================================

std::unique_ptr<IRoutingMatrix> createRoutingMatrix() {
  return std::make_unique<RoutingMatrix>();
}

} // namespace orpheus
