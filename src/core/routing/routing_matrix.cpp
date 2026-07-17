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
  // Validate fixed-capacity realtime bounds.
  if (config.num_channels == 0 || config.num_channels > 256 || config.num_groups == 0 ||
      config.num_groups > 32 || config.num_outputs == 0 || config.num_outputs > 32) {
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

  // Preallocate every logical group at the configured output width.
  m_group_buffers.clear();
  m_group_buffers.resize(config.num_groups);
  for (auto& buffer : m_group_buffers) {
    buffer.resize(config.num_outputs, MAX_BUFFER_SIZE);
  }

  // One reusable stereo scratch buffer keeps channel meters isolated from the
  // shared group accumulators without allocating in processRouting().
  m_channel_meter_buffer.resize(2, MAX_BUFFER_SIZE);

  m_temp_buffer.clear();
  m_temp_buffer.resize(MAX_BUFFER_SIZE, 0.0f);

  // Reset metering
  m_master_peak.store(0.0f, std::memory_order_release);
  m_master_rms.store(0.0f, std::memory_order_release);
  m_master_clip_count.store(0, std::memory_order_release);
  for (auto& meter : m_master_true_peak_meters) {
    meter.reset();
  }

  m_group_control_sequence.store(0, std::memory_order_release);
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

SessionGraphError RoutingMatrix::setChannelGroup(RoutingChannelIndex channel_index,
                                                 RoutingGroupIndex group_index) {
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

SessionGraphError RoutingMatrix::setChannelRoute(RoutingChannelIndex channel_index,
                                                 RoutingGroupIndex group_index,
                                                 RoutingOutputIndex output_index) {
  if (!m_initialized.load(std::memory_order_acquire) || channel_index >= m_channels.size()) {
    return SessionGraphError::InvalidParameter;
  }
  if (group_index != UNASSIGNED_GROUP && group_index >= m_groups.size()) {
    return SessionGraphError::InvalidParameter;
  }
  const int configIndex = m_active_config_idx.load(std::memory_order_acquire);
  if (output_index >= m_config_buffers[configIndex].num_outputs) {
    return SessionGraphError::InvalidParameter;
  }

  auto& channel = m_channels[channel_index];
  channel.group_index = group_index;
  channel.config.group_index = group_index;
  channel.config.output_channel = output_index;
  return SessionGraphError::OK;
}

SessionGraphError RoutingMatrix::setChannelGain(RoutingChannelIndex channel_index, float gain_db) {
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

SessionGraphError RoutingMatrix::setChannelPan(RoutingChannelIndex channel_index, float pan) {
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

SessionGraphError RoutingMatrix::setChannelMute(RoutingChannelIndex channel_index, bool mute) {
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

SessionGraphError RoutingMatrix::setChannelSolo(RoutingChannelIndex channel_index, bool solo) {
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

SessionGraphError RoutingMatrix::configureChannel(RoutingChannelIndex channel_index,
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
  m_channels[channel_index].config.output_channel = config.output_channel;

  return SessionGraphError::OK;
}

// ============================================================================
// Group Configuration
// ============================================================================

SessionGraphError RoutingMatrix::setGroupGain(RoutingGroupIndex group_index, float gain_db) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }
  if (group_index >= m_groups.size()) {
    return SessionGraphError::InvalidParameter;
  }
  if (!std::isfinite(gain_db)) {
    return SessionGraphError::InvalidParameter;
  }

  gain_db = std::clamp(gain_db, -100.0f, 12.0f);
  beginGroupControlWrite();
  auto& group = m_groups[group_index];
  group.configured_gain_db.store(gain_db, std::memory_order_relaxed);
  group.config.gain_db = gain_db;
  endGroupControlWrite();

  if (m_callback) {
    m_callback->onGroupGainChanged(group_index, gain_db);
  }
  return SessionGraphError::OK;
}

SessionGraphError RoutingMatrix::setGroupMute(RoutingGroupIndex group_index, bool mute) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }
  if (group_index >= m_groups.size()) {
    return SessionGraphError::InvalidParameter;
  }

  beginGroupControlWrite();
  m_groups[group_index].mute.store(mute, std::memory_order_relaxed);
  m_groups[group_index].config.mute = mute;
  endGroupControlWrite();
  return SessionGraphError::OK;
}

SessionGraphError RoutingMatrix::setGroupSolo(RoutingGroupIndex group_index, bool solo) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }
  if (group_index >= m_groups.size()) {
    return SessionGraphError::InvalidParameter;
  }

  beginGroupControlWrite();
  m_groups[group_index].solo.store(solo, std::memory_order_relaxed);
  m_groups[group_index].config.solo = solo;
  updateSoloState(false);
  endGroupControlWrite();
  if (m_callback) {
    m_callback->onSoloStateChanged(m_solo_active.load(std::memory_order_acquire));
  }
  return SessionGraphError::OK;
}

SessionGraphError RoutingMatrix::configureGroup(RoutingGroupIndex group_index,
                                                const GroupConfig& config) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }
  if (group_index >= m_groups.size()) {
    return SessionGraphError::InvalidParameter;
  }

  auto snapshot = getRoutingControlSnapshot();
  auto& state = snapshot.groups[group_index];
  state.gain_db = config.gain_db;
  state.configured_mute = config.mute;
  state.configured_solo = config.solo;
  state.output_start = config.output_start;
  state.output_width = config.output_width;
  const auto result = applyGroupControlSnapshot(snapshot);
  if (result == SessionGraphError::OK) {
    m_groups[group_index].config.name = config.name;
    m_groups[group_index].config.color = config.color;
  }
  return result;
}

SessionGraphError RoutingMatrix::setGroupOutputRoute(RoutingGroupIndex group_index,
                                                     RoutingOutputIndex output_start,
                                                     uint16_t output_width) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }
  if (group_index >= m_groups.size()) {
    return SessionGraphError::InvalidParameter;
  }
  const auto& config = m_config_buffers[m_active_config_idx.load(std::memory_order_acquire)];
  if (output_width == 0 ||
      static_cast<uint32_t>(output_start) + output_width > config.num_outputs) {
    return SessionGraphError::InvalidParameter;
  }

  beginGroupControlWrite();
  auto& group = m_groups[group_index];
  group.output_start.store(output_start, std::memory_order_relaxed);
  group.output_width.store(output_width, std::memory_order_relaxed);
  group.config.output_start = output_start;
  group.config.output_width = output_width;
  endGroupControlWrite();
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

bool RoutingMatrix::isChannelMuted(RoutingChannelIndex channel_index) const {
  if (channel_index >= m_channels.size()) {
    return true;
  }

  bool is_muted = m_channels[channel_index].mute.load(std::memory_order_acquire);
  bool is_solo = m_channels[channel_index].solo.load(std::memory_order_acquire);
  bool solo_active = m_channel_solo_active.load(std::memory_order_acquire);

  // If solo is active and this channel is not solo'd, it's effectively muted
  if (solo_active && !is_solo) {
    return true;
  }

  return is_muted;
}

bool RoutingMatrix::isGroupMuted(RoutingGroupIndex group_index) const {
  const auto snapshot = getRoutingControlSnapshot();
  if (group_index >= snapshot.group_count) {
    return true;
  }
  return snapshot.groups[group_index].effective_mute;
}

RoutingControlSnapshot RoutingMatrix::getRoutingControlSnapshot() const noexcept {
  RoutingControlSnapshot snapshot;
  if (!m_initialized.load(std::memory_order_acquire)) {
    return snapshot;
  }

  for (;;) {
    const uint64_t before = m_group_control_sequence.load(std::memory_order_acquire);
    if ((before & 1u) != 0u) {
      continue;
    }

    snapshot.group_count =
        static_cast<RoutingGroupIndex>(std::min<size_t>(m_groups.size(), kRoutingControlMaxGroups));
    const bool groupSoloActive = m_group_solo_active.load(std::memory_order_relaxed);
    for (RoutingGroupIndex index = 0; index < snapshot.group_count; ++index) {
      const auto& group = m_groups[index];
      auto& state = snapshot.groups[index];
      state.gain_db = group.configured_gain_db.load(std::memory_order_relaxed);
      state.output_start = group.output_start.load(std::memory_order_relaxed);
      state.output_width = group.output_width.load(std::memory_order_relaxed);
      state.configured_mute = group.mute.load(std::memory_order_relaxed);
      state.configured_solo = group.solo.load(std::memory_order_relaxed);
      state.effective_mute = state.configured_mute || (groupSoloActive && !state.configured_solo);
    }

    const uint64_t after = m_group_control_sequence.load(std::memory_order_acquire);
    if (before == after) {
      snapshot.revision = after / 2u;
      return snapshot;
    }
  }
}

bool RoutingMatrix::validateGroupControlSnapshot(
    const RoutingControlSnapshot& snapshot) const noexcept {
  if (snapshot.schema_version != kRoutingControlSnapshotSchemaVersion ||
      snapshot.group_count != m_groups.size()) {
    return false;
  }
  const auto& config = m_config_buffers[m_active_config_idx.load(std::memory_order_acquire)];
  for (RoutingGroupIndex index = 0; index < snapshot.group_count; ++index) {
    const auto& state = snapshot.groups[index];
    if (!std::isfinite(state.gain_db) || state.gain_db < -100.0f || state.gain_db > 12.0f ||
        state.output_width == 0 ||
        static_cast<uint32_t>(state.output_start) + state.output_width > config.num_outputs) {
      return false;
    }
  }
  return true;
}

SessionGraphError RoutingMatrix::applyGroupControlSnapshot(const RoutingControlSnapshot& snapshot) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }
  if (!validateGroupControlSnapshot(snapshot)) {
    return SessionGraphError::InvalidParameter;
  }

  beginGroupControlWrite();
  for (RoutingGroupIndex index = 0; index < snapshot.group_count; ++index) {
    const auto& state = snapshot.groups[index];
    auto& group = m_groups[index];
    group.configured_gain_db.store(state.gain_db, std::memory_order_relaxed);
    group.mute.store(state.configured_mute, std::memory_order_relaxed);
    group.solo.store(state.configured_solo, std::memory_order_relaxed);
    group.output_start.store(state.output_start, std::memory_order_relaxed);
    group.output_width.store(state.output_width, std::memory_order_relaxed);
    group.config.gain_db = state.gain_db;
    group.config.mute = state.configured_mute;
    group.config.solo = state.configured_solo;
    group.config.output_start = state.output_start;
    group.config.output_width = state.output_width;
  }
  updateSoloState(false);
  endGroupControlWrite();

  if (m_callback) {
    for (RoutingGroupIndex index = 0; index < snapshot.group_count; ++index) {
      m_callback->onGroupGainChanged(index, snapshot.groups[index].gain_db);
    }
    m_callback->onSoloStateChanged(m_solo_active.load(std::memory_order_acquire));
  }
  return SessionGraphError::OK;
}

void RoutingMatrix::beginGroupControlWrite() noexcept {
  bool expected = false;
  while (!m_group_control_publication_in_progress.compare_exchange_weak(
      expected, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
    expected = false;
  }
  while (m_group_control_render_reading.load(std::memory_order_acquire)) {
  }
  m_group_control_sequence.fetch_add(1, std::memory_order_acq_rel);
}

void RoutingMatrix::endGroupControlWrite() noexcept {
  m_group_control_sequence.fetch_add(1, std::memory_order_release);
  m_group_control_publication_in_progress.store(false, std::memory_order_release);
}

void RoutingMatrix::refreshRenderGroupControls() noexcept {
  if (m_group_control_publication_in_progress.load(std::memory_order_acquire)) {
    return;
  }

  m_group_control_render_reading.store(true, std::memory_order_release);
  if (m_group_control_publication_in_progress.load(std::memory_order_acquire)) {
    m_group_control_render_reading.store(false, std::memory_order_release);
    return;
  }

  bool groupSoloActive = false;
  for (RoutingGroupIndex index = 0; index < m_groups.size(); ++index) {
    const auto& group = m_groups[index];
    auto& state = m_render_group_controls[index];
    const float gainDb = group.configured_gain_db.load(std::memory_order_relaxed);
    if (gainDb != state.gain_db) {
      state.gain_db = gainDb;
      group.gain_smoother->setTarget(dbToLinear(gainDb));
    }
    state.output_start = group.output_start.load(std::memory_order_relaxed);
    state.output_width = group.output_width.load(std::memory_order_relaxed);
    state.configured_mute = group.mute.load(std::memory_order_relaxed);
    state.configured_solo = group.solo.load(std::memory_order_relaxed);
    groupSoloActive = groupSoloActive || state.configured_solo;
  }
  for (RoutingGroupIndex index = 0; index < m_groups.size(); ++index) {
    auto& state = m_render_group_controls[index];
    state.effective_mute = state.configured_mute || (groupSoloActive && !state.configured_solo);
  }
  m_group_control_render_reading.store(false, std::memory_order_release);
}

AudioMeter RoutingMatrix::getChannelMeter(RoutingChannelIndex channel_index) const {
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

AudioMeter RoutingMatrix::getGroupMeter(RoutingGroupIndex group_index) const {
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

RoutingSnapshot RoutingMatrix::saveSnapshot(const std::string& name,
                                            RoutingSnapshotContext context) {
  RoutingSnapshot snapshot;
  snapshot.name = name;
  snapshot.captureRevision = m_snapshot_revision.fetch_add(1, std::memory_order_relaxed) + 1;
  snapshot.controlTimeMs = context.controlTimeMs;
  snapshot.audioPosition = context.audioPosition;

  // Save channel states
  for (const auto& channel : m_channels) {
    snapshot.channels.push_back(channel.config);
  }

  // Save group controls from one coherent transaction boundary.
  const auto controls = getRoutingControlSnapshot();
  for (RoutingGroupIndex index = 0; index < controls.group_count; ++index) {
    auto config = m_groups[index].config;
    const auto& state = controls.groups[index];
    config.gain_db = state.gain_db;
    config.mute = state.configured_mute;
    config.solo = state.configured_solo;
    config.output_start = state.output_start;
    config.output_width = state.output_width;
    snapshot.groups.push_back(std::move(config));
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

  auto controls = getRoutingControlSnapshot();
  for (RoutingGroupIndex index = 0; index < controls.group_count; ++index) {
    const auto& group = snapshot.groups[index];
    auto& state = controls.groups[index];
    state.gain_db = group.gain_db;
    state.configured_mute = group.mute;
    state.configured_solo = group.solo;
    state.output_start = group.output_start;
    state.output_width = group.output_width;
  }
  const auto groupResult = applyGroupControlSnapshot(controls);
  if (groupResult != SessionGraphError::OK) {
    return groupResult;
  }
  for (RoutingGroupIndex index = 0; index < controls.group_count; ++index) {
    m_groups[index].config.name = snapshot.groups[index].name;
    m_groups[index].config.color = snapshot.groups[index].color;
  }

  // Load channel states after the group transaction has validated.
  for (size_t i = 0; i < snapshot.channels.size(); ++i) {
    configureChannel(static_cast<RoutingChannelIndex>(i), snapshot.channels[i]);
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
    configureChannel(static_cast<RoutingChannelIndex>(i), default_config);
  }

  // Reset all groups in one validated transaction.
  auto controls = getRoutingControlSnapshot();
  const auto& routingConfig = m_config_buffers[m_active_config_idx.load(std::memory_order_acquire)];
  for (RoutingGroupIndex index = 0; index < controls.group_count; ++index) {
    auto& state = controls.groups[index];
    state.gain_db = 0.0f;
    state.configured_mute = false;
    state.configured_solo = false;
    state.output_start = 0;
    state.output_width = routingConfig.num_outputs;
  }
  const auto groupResult = applyGroupControlSnapshot(controls);
  if (groupResult != SessionGraphError::OK) {
    return groupResult;
  }

  // Reset master
  setMasterGain(0.0f); // Unity gain
  setMasterMute(false);

  return SessionGraphError::OK;
}

// ============================================================================
// Audio Processing
// ============================================================================

uint32_t RoutingMatrix::maxBlockFrames() const {
  return static_cast<uint32_t>(MAX_BUFFER_SIZE);
}

SessionGraphError RoutingMatrix::processRouting(const float* const* channel_inputs,
                                                float* const* master_output, uint32_t num_frames) {
  if (!m_initialized.load(std::memory_order_acquire)) {
    return SessionGraphError::NotInitialized;
  }

  // FTR028: Accept arbitrarily large blocks by chunking over slices of at most
  // MAX_BUFFER_SIZE frames. This is allocation-free and lock-free — we only
  // offset into the caller-supplied planar buffers and reuse the pre-allocated
  // scratch inside processRoutingBlock. Offline hosts (bounce/export) can pass
  // any block size and it "just works"; the previous private ceiling no longer
  // silently rejects large blocks.
  //
  // Get active config (lock-free read) to know the channel/output counts for
  // pointer offsetting.
  refreshRenderGroupControls();
  {
    int cfg_idx = m_active_config_idx.load(std::memory_order_acquire);
    const RoutingConfig& cfg = m_config_buffers[cfg_idx];

    // Fixed stack scratch at the validated realtime maxima.
    const float* input_slice_ptrs[256];
    float* output_slice_ptrs[32];

    uint32_t offset = 0;
    while (offset < num_frames) {
      const uint32_t slice =
          std::min<uint32_t>(num_frames - offset, static_cast<uint32_t>(MAX_BUFFER_SIZE));

      for (RoutingChannelIndex ch = 0; ch < cfg.num_channels; ++ch) {
        const float* in = channel_inputs ? channel_inputs[ch] : nullptr;
        input_slice_ptrs[ch] = in ? (in + offset) : nullptr;
      }
      for (RoutingOutputIndex out = 0; out < cfg.num_outputs; ++out) {
        output_slice_ptrs[out] = master_output[out] + offset;
      }

      SessionGraphError err = processRoutingBlock(input_slice_ptrs, output_slice_ptrs, slice);
      if (err != SessionGraphError::OK) {
        return err;
      }

      offset += slice;
    }
  }

  return SessionGraphError::OK;
}

SessionGraphError RoutingMatrix::processRoutingBlock(const float* const* channel_inputs,
                                                     float* const* master_output,
                                                     uint32_t num_frames) {
  if (num_frames > MAX_BUFFER_SIZE) {
    return SessionGraphError::InvalidParameter;
  }

  const int config_idx = m_active_config_idx.load(std::memory_order_acquire);
  const RoutingConfig& config = m_config_buffers[config_idx];

  for (RoutingGroupIndex group = 0; group < config.num_groups; ++group) {
    m_group_buffers[group].clear();
  }

  for (RoutingChannelIndex channel_index = 0; channel_index < config.num_channels;
       ++channel_index) {
    auto& channel = m_channels[channel_index];
    const RoutingGroupIndex group_index = channel.group_index;

    if (group_index == UNASSIGNED_GROUP || group_index >= config.num_groups) {
      if (config.enable_metering) {
        publishChannelMeterSilence(channel);
      }
      continue;
    }

    const float* input = channel_inputs ? channel_inputs[channel_index] : nullptr;
    const bool muted = isChannelMuted(channel_index) || input == nullptr;
    auto& group_buffer = m_group_buffers[group_index];
    auto& meter_left = m_channel_meter_buffer.channels[0];
    auto& meter_right = m_channel_meter_buffer.channels[1];
    if (config.enable_metering) {
      std::fill_n(meter_left.begin(), num_frames, 0.0f);
      std::fill_n(meter_right.begin(), num_frames, 0.0f);
    }

    RoutingOutputIndex discrete_output = channel.config.output_channel;
    if (discrete_output >= config.num_outputs) {
      discrete_output = 0;
    }

    for (uint32_t frame = 0; frame < num_frames; ++frame) {
      const float channel_gain = channel.gain_smoother->process();
      const float pan_left = channel.pan_left->process();
      const float pan_right = channel.pan_right->process();
      if (muted) {
        continue;
      }

      const float sample = input[frame] * channel_gain;
      switch (config.source_channel_policy) {
      case SourceChannelPolicy::Discrete:
        group_buffer.channels[discrete_output][frame] += sample;
        if (config.enable_metering) {
          meter_left[frame] = sample;
        }
        break;
      case SourceChannelPolicy::StereoPairs: {
        const float left = sample * pan_left;
        group_buffer.channels[0][frame] += left;
        if (config.enable_metering) {
          meter_left[frame] = left;
        }
        if (config.num_outputs > 1) {
          const float right = sample * pan_right;
          group_buffer.channels[1][frame] += right;
          if (config.enable_metering) {
            meter_right[frame] = right;
          }
        }
        break;
      }
      case SourceChannelPolicy::MonoFoldDown:
        group_buffer.channels[0][frame] += sample;
        if (config.enable_metering) {
          meter_left[frame] = sample;
        }
        break;
      }
    }

    if (config.enable_metering) {
      if (muted) {
        publishChannelMeterSilence(channel);
        continue;
      }

      const bool stereo = config.source_channel_policy == SourceChannelPolicy::StereoPairs &&
                          config.num_outputs > 1;
      const float* right = stereo ? meter_right.data() : nullptr;
      processStereoMetering(meter_left.data(), right, num_frames, channel.true_peak_meters,
                            channel.peak_level, channel.rms_level);
      if (detectClipping(meter_left.data(), num_frames) ||
          (right != nullptr && detectClipping(meter_right.data(), num_frames))) {
        channel.clip_count.fetch_add(1, std::memory_order_relaxed);
      }
    }
  }

  for (RoutingOutputIndex output = 0; output < config.num_outputs; ++output) {
    std::memset(master_output[output], 0, num_frames * sizeof(float));
  }

  for (RoutingGroupIndex group_index = 0; group_index < config.num_groups; ++group_index) {
    auto& group = m_groups[group_index];
    auto& group_buffer = m_group_buffers[group_index];
    const auto& controls = m_render_group_controls[group_index];
    const bool muted = controls.effective_mute;
    const float headroom = getHeadroomCompensation(group_index);

    const RoutingOutputIndex outputStart = controls.output_start;
    const uint16_t outputWidth = controls.output_width;

    for (uint32_t frame = 0; frame < num_frames; ++frame) {
      const float group_gain = group.gain_smoother->process();
      if (muted) {
        continue;
      }

      if (config.source_channel_policy == SourceChannelPolicy::StereoPairs) {
        if (outputWidth > 0) {
          master_output[outputStart][frame] +=
              group_buffer.channels[0][frame] * group_gain * headroom;
        }
        if (outputWidth > 1) {
          master_output[outputStart + 1][frame] +=
              group_buffer.channels[1][frame] * group_gain * headroom;
        }
      } else {
        for (uint16_t lane = 0; lane < outputWidth; ++lane) {
          master_output[outputStart + lane][frame] +=
              group_buffer.channels[lane][frame] * group_gain * headroom;
        }
      }
    }

    if (config.enable_metering) {
      const float* right = config.num_outputs > 1 ? group_buffer.channels[1].data() : nullptr;
      processStereoMetering(group_buffer.channels[0].data(), right, num_frames,
                            group.true_peak_meters, group.peak_level, group.rms_level);
      for (RoutingOutputIndex output = 0; output < config.num_outputs; ++output) {
        if (detectClipping(group_buffer.channels[output].data(), num_frames)) {
          group.clip_count.fetch_add(1, std::memory_order_relaxed);
          break;
        }
      }
    }
  }

  const bool master_muted = m_master_mute.load(std::memory_order_acquire);
  for (uint32_t frame = 0; frame < num_frames; ++frame) {
    const float master_gain = master_muted ? 0.0f : m_master_gain_smoother->process();
    if (master_muted) {
      m_master_gain_smoother->process();
    }
    for (RoutingOutputIndex output = 0; output < config.num_outputs; ++output) {
      master_output[output][frame] *= master_gain;
    }
  }

  if (config.enable_metering) {
    processStereoMetering(master_output[0], config.num_outputs > 1 ? master_output[1] : nullptr,
                          num_frames, m_master_true_peak_meters, m_master_peak, m_master_rms);
    for (RoutingOutputIndex output = 0; output < config.num_outputs; ++output) {
      if (detectClipping(master_output[output], num_frames)) {
        m_master_clip_count.fetch_add(1, std::memory_order_relaxed);
        break;
      }
    }
  }

  if (config.enable_clipping_protection) {
    static constexpr float threshold = 0.794f;
    static constexpr float knee_width = 0.3f;
    static constexpr float ceiling = 0.9999f;

    for (RoutingOutputIndex output = 0; output < config.num_outputs; ++output) {
      for (uint32_t frame = 0; frame < num_frames; ++frame) {
        float sample = master_output[output][frame];
        const float magnitude = std::abs(sample);
        if (magnitude > threshold) {
          const float compressed =
              threshold + std::tanh((magnitude - threshold) / knee_width) * knee_width;
          sample = std::copysign(std::min(compressed, ceiling), sample);
        }
        master_output[output][frame] = sample;
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

  for (RoutingChannelIndex i = 0; i < config.num_channels; ++i) {
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
    channel.config.output_channel = static_cast<RoutingOutputIndex>(i % config.num_outputs);
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

  for (RoutingGroupIndex i = 0; i < config.num_groups; ++i) {
    GroupState group;
    group.gain_smoother = std::make_unique<GainSmoother>(sample_rate, config.gain_smoothing_ms);
    group.gain_smoother->reset(1.0f); // Unity gain

    group.mute.store(false, std::memory_order_release);
    group.solo.store(false, std::memory_order_release);
    group.configured_gain_db.store(0.0f, std::memory_order_release);

    group.peak_level.store(0.0f, std::memory_order_release);
    group.rms_level.store(0.0f, std::memory_order_release);
    group.clip_count.store(0, std::memory_order_release);

    // Default config
    group.config.name = "Group " + std::to_string(i + 1);
    group.config.gain_db = 0.0f;
    group.config.mute = false;
    group.config.solo = false;
    group.output_start.store(0, std::memory_order_release);
    group.output_width.store(config.num_outputs, std::memory_order_release);
    group.config.output_start = 0;
    group.config.output_width = config.num_outputs;
    group.config.color = 0xFFFFFFFF;

    auto& renderState = m_render_group_controls[i];
    renderState.gain_db = 0.0f;
    renderState.output_start = 0;
    renderState.output_width = config.num_outputs;
    renderState.configured_mute = false;
    renderState.configured_solo = false;
    renderState.effective_mute = false;

    m_groups.push_back(std::move(group));
  }
}

void RoutingMatrix::updateSoloState(bool notifyCallback) {
  bool any_channel_solo = false;
  for (const auto& channel : m_channels) {
    if (channel.solo.load(std::memory_order_acquire)) {
      any_channel_solo = true;
      break;
    }
  }

  bool any_group_solo = false;
  for (const auto& group : m_groups) {
    if (group.solo.load(std::memory_order_acquire)) {
      any_group_solo = true;
      break;
    }
  }

  m_channel_solo_active.store(any_channel_solo, std::memory_order_release);
  m_group_solo_active.store(any_group_solo, std::memory_order_release);
  const bool any_solo = any_channel_solo || any_group_solo;
  m_solo_active.store(any_solo, std::memory_order_release);

  // Notify callback after state publication unless a surrounding transaction
  // owns the notification boundary.
  if (notifyCallback && m_callback) {
    m_callback->onSoloStateChanged(any_solo);
  }
}

void RoutingMatrix::updatePanLaw(RoutingChannelIndex channel_index, float pan) {
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

void RoutingMatrix::processStereoMetering(const float* left, const float* right, size_t num_frames,
                                          std::array<TruePeakMeter, 2>& true_peak_meters,
                                          std::atomic<float>& peak, std::atomic<float>& rms) {
  if (num_frames == 0 || (left == nullptr && right == nullptr)) {
    peak.store(0.0f, std::memory_order_release);
    rms.store(0.0f, std::memory_order_release);
    return;
  }

  const int config_idx = m_active_config_idx.load(std::memory_order_acquire);
  const MeteringMode mode = m_config_buffers[config_idx].metering_mode;
  float peak_value = 0.0f;
  double sum_squares = 0.0;

  for (size_t i = 0; i < num_frames; ++i) {
    const float left_sample = left != nullptr ? left[i] : 0.0f;
    const float right_sample = right != nullptr ? right[i] : 0.0f;
    peak_value = std::max(peak_value, std::max(std::abs(left_sample), std::abs(right_sample)));
    sum_squares += static_cast<double>(left_sample) * left_sample +
                   static_cast<double>(right_sample) * right_sample;
  }

  if (mode == MeteringMode::TruePeak) {
    const float left_peak =
        left != nullptr ? true_peak_meters[0].processBuffer(left, num_frames) : 0.0f;
    const float right_peak =
        right != nullptr ? true_peak_meters[1].processBuffer(right, num_frames) : 0.0f;
    peak_value = std::max(left_peak, right_peak);
  } else if (mode == MeteringMode::LUFS) {
    const double mean_square = sum_squares / static_cast<double>(num_frames * 2);
    const double lufs = mean_square > 0.0 ? -0.691 + 10.0 * std::log10(mean_square) : -100.0;
    peak_value = static_cast<float>(std::pow(10.0, lufs / 20.0));
  }

  const float rms_value =
      static_cast<float>(std::sqrt(sum_squares / static_cast<double>(num_frames * 2)));
  peak.store(peak_value, std::memory_order_release);
  rms.store(rms_value, std::memory_order_release);
}

void RoutingMatrix::publishChannelMeterSilence(ChannelState& channel) {
  channel.peak_level.store(0.0f, std::memory_order_release);
  channel.rms_level.store(0.0f, std::memory_order_release);
  for (auto& meter : channel.true_peak_meters) {
    meter.reset();
  }
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

uint32_t RoutingMatrix::countActiveChannelsInGroup(RoutingGroupIndex group_index) const {
  uint32_t count = 0;
  for (const auto& channel : m_channels) {
    if (channel.group_index == group_index && !channel.mute.load(std::memory_order_acquire)) {
      ++count;
    }
  }
  return count;
}

uint32_t RoutingMatrix::countTotalActiveChannels() const {
  uint32_t count = 0;
  for (const auto& channel : m_channels) {
    if (channel.group_index != UNASSIGNED_GROUP && !channel.mute.load(std::memory_order_acquire)) {
      ++count;
    }
  }
  return count;
}

float RoutingMatrix::getHeadroomCompensation(RoutingGroupIndex group_index) const {
  // Get active config (lock-free read)
  int config_idx = m_active_config_idx.load(std::memory_order_acquire);
  const RoutingConfig& config = m_config_buffers[config_idx];

  switch (config.headroom_mode) {
  case HeadroomMode::None:
    return 1.0f;

  case HeadroomMode::PerGroup: {
    uint32_t active = countActiveChannelsInGroup(group_index);
    return active > 0 ? 1.0f / static_cast<float>(active) : 1.0f;
  }

  case HeadroomMode::Global: {
    uint32_t active = countTotalActiveChannels();
    return active > 0 ? 1.0f / static_cast<float>(active) : 1.0f;
  }

  case HeadroomMode::Logarithmic: {
    // -3 dB per doubling of channels: 1/sqrt(n)
    uint32_t active = countActiveChannelsInGroup(group_index);
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
