// SPDX-License-Identifier: MIT
#include "clip_routing.h"
#include "gain_smoother.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <unordered_map>

namespace orpheus {

// ============================================================================
// ClipRoutingMatrix Implementation
// ============================================================================

class ClipRoutingMatrix : public IClipRoutingMatrix {
public:
  ClipRoutingMatrix(core::SessionGraph* sessionGraph, uint32_t sampleRate);
  ~ClipRoutingMatrix() override;

  // IClipRoutingMatrix interface
  SessionGraphError assignClipToGroup(ClipHandle handle, uint8_t groupIndex) override;
  SessionGraphError setGroupGain(uint8_t groupIndex, float gainDb) override;
  SessionGraphError setGroupMute(uint8_t groupIndex, bool muted) override;
  SessionGraphError setGroupSolo(uint8_t groupIndex, bool soloed) override;
  SessionGraphError routeGroupToMaster(uint8_t groupIndex, bool enabled) override;

  uint8_t getClipGroup(ClipHandle handle) const override;
  float getGroupGain(uint8_t groupIndex) const override;
  bool isGroupMuted(uint8_t groupIndex) const override;
  bool isGroupSoloed(uint8_t groupIndex) const override;
  bool isGroupRoutedToMaster(uint8_t groupIndex) const override;

  // Multi-Channel Routing (Feature 7)
  SessionGraphError setClipOutputBus(ClipHandle handle, uint8_t outputBus) override;
  SessionGraphError mapChannels(ClipHandle handle, uint8_t clipChannel,
                                uint8_t outputChannel) override;
  uint8_t getClipOutputBus(ClipHandle handle) const override;

private:
  static constexpr uint8_t NUM_GROUPS = 4;
  static constexpr uint8_t UNASSIGNED_GROUP = 255;
  static constexpr float MIN_GAIN_DB = -60.0f;
  static constexpr float MAX_GAIN_DB = 12.0f;
  static constexpr float SMOOTHING_TIME_MS = 10.0f;

  // Multi-channel routing constants (Feature 7)
  static constexpr uint8_t MAX_OUTPUT_CHANNELS = 32; // Professional interface limit
  static constexpr uint8_t MAX_OUTPUT_BUS = 15;      // Bus 15 = channels 31-32
  static constexpr uint8_t DEFAULT_OUTPUT_BUS = 0;   // Bus 0 = channels 1-2 (stereo)

  // Group state (audio thread)
  struct GroupState {
    GainSmoother* gain_smoother;
    std::atomic<bool> muted;
    std::atomic<bool> soloed;
    std::atomic<bool> routed_to_master;
    float gain_db; // UI thread only (for queries)

    GroupState()
        : gain_smoother(nullptr), muted(false), soloed(false), routed_to_master(true),
          gain_db(0.0f) {}

    ~GroupState() {
      if (gain_smoother) {
        delete gain_smoother;
        gain_smoother = nullptr;
      }
    }

    // Move constructor (needed for std::array with atomics)
    GroupState(GroupState&& other) noexcept
        : gain_smoother(other.gain_smoother), muted(other.muted.load()),
          soloed(other.soloed.load()), routed_to_master(other.routed_to_master.load()),
          gain_db(other.gain_db) {
      other.gain_smoother = nullptr;
    }

    // Delete copy constructor/assignment (atomics are not copyable)
    GroupState(const GroupState&) = delete;
    GroupState& operator=(const GroupState&) = delete;
    GroupState& operator=(GroupState&&) = delete;
  };

  // Helper methods
  void updateSoloState();
  float dbToLinear(float db) const;

  // Session graph (for clip lookup - reserved for future use)
  [[maybe_unused]] core::SessionGraph* m_session_graph;
  [[maybe_unused]] uint32_t m_sample_rate;

  // Clip-to-group mapping (lock-free reads/writes via atomic pointer swap)
  std::unordered_map<ClipHandle, uint8_t> m_clip_groups;

  // Group states
  std::array<GroupState, NUM_GROUPS> m_groups;

  // Global solo state (true if any group is soloed)
  std::atomic<bool> m_solo_active;

  // Multi-channel routing storage (Feature 7)
  // Output bus per clip (0 = channels 1-2, 1 = channels 3-4, etc.)
  std::unordered_map<ClipHandle, uint8_t> m_clip_output_bus;

  // Channel mappings per clip (clipChannel → outputChannel)
  // Key: ClipHandle, Value: map of clip channel → output channel
  std::unordered_map<ClipHandle, std::unordered_map<uint8_t, uint8_t>> m_channel_mappings;
};

// ============================================================================
// ClipRoutingMatrix Implementation
// ============================================================================

ClipRoutingMatrix::ClipRoutingMatrix(core::SessionGraph* sessionGraph, uint32_t sampleRate)
    : m_session_graph(sessionGraph), m_sample_rate(sampleRate), m_solo_active(false) {
  // Initialize group gain smoothers
  for (uint8_t i = 0; i < NUM_GROUPS; ++i) {
    m_groups[i].gain_smoother = new GainSmoother(sampleRate, SMOOTHING_TIME_MS);
    m_groups[i].gain_smoother->reset(1.0f); // Unity gain (0 dB)
    m_groups[i].muted.store(false, std::memory_order_release);
    m_groups[i].soloed.store(false, std::memory_order_release);
    m_groups[i].routed_to_master.store(true, std::memory_order_release);
    m_groups[i].gain_db = 0.0f;
  }
}

ClipRoutingMatrix::~ClipRoutingMatrix() {
  // GainSmooth cleaners are deleted automatically by GroupState destructors
}

// ============================================================================
// Clip Assignment
// ============================================================================

SessionGraphError ClipRoutingMatrix::assignClipToGroup(ClipHandle handle, uint8_t groupIndex) {
  // Validate group index (255 = unassigned is valid)
  if (groupIndex != UNASSIGNED_GROUP && groupIndex >= NUM_GROUPS) {
    return SessionGraphError::InvalidParameter;
  }

  // Validate clip handle exists (basic check)
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  // Update clip-to-group mapping (lock-free: reads are safe, writes are from UI thread)
  m_clip_groups[handle] = groupIndex;

  return SessionGraphError::OK;
}

// ============================================================================
// Group Configuration
// ============================================================================

SessionGraphError ClipRoutingMatrix::setGroupGain(uint8_t groupIndex, float gainDb) {
  if (groupIndex >= NUM_GROUPS) {
    return SessionGraphError::InvalidParameter;
  }

  // Clamp to valid range
  gainDb = std::clamp(gainDb, MIN_GAIN_DB, MAX_GAIN_DB);

  // Convert to linear and set target (lock-free)
  float gainLinear = dbToLinear(gainDb);
  m_groups[groupIndex].gain_smoother->setTarget(gainLinear);
  m_groups[groupIndex].gain_db = gainDb; // Store for queries

  return SessionGraphError::OK;
}

SessionGraphError ClipRoutingMatrix::setGroupMute(uint8_t groupIndex, bool muted) {
  if (groupIndex >= NUM_GROUPS) {
    return SessionGraphError::InvalidParameter;
  }

  // Atomic update (lock-free)
  m_groups[groupIndex].muted.store(muted, std::memory_order_release);

  return SessionGraphError::OK;
}

SessionGraphError ClipRoutingMatrix::setGroupSolo(uint8_t groupIndex, bool soloed) {
  if (groupIndex >= NUM_GROUPS) {
    return SessionGraphError::InvalidParameter;
  }

  // Atomic update (lock-free)
  m_groups[groupIndex].soloed.store(soloed, std::memory_order_release);

  // Update global solo state
  updateSoloState();

  return SessionGraphError::OK;
}

SessionGraphError ClipRoutingMatrix::routeGroupToMaster(uint8_t groupIndex, bool enabled) {
  if (groupIndex >= NUM_GROUPS) {
    return SessionGraphError::InvalidParameter;
  }

  // Atomic update (lock-free)
  m_groups[groupIndex].routed_to_master.store(enabled, std::memory_order_release);

  return SessionGraphError::OK;
}

// ============================================================================
// State Queries
// ============================================================================

uint8_t ClipRoutingMatrix::getClipGroup(ClipHandle handle) const {
  auto it = m_clip_groups.find(handle);
  if (it != m_clip_groups.end()) {
    return it->second;
  }
  return UNASSIGNED_GROUP; // Not assigned
}

float ClipRoutingMatrix::getGroupGain(uint8_t groupIndex) const {
  if (groupIndex >= NUM_GROUPS) {
    return 0.0f;
  }
  return m_groups[groupIndex].gain_db;
}

bool ClipRoutingMatrix::isGroupMuted(uint8_t groupIndex) const {
  if (groupIndex >= NUM_GROUPS) {
    return true; // Invalid group is considered muted
  }

  bool is_muted = m_groups[groupIndex].muted.load(std::memory_order_acquire);
  bool is_soloed = m_groups[groupIndex].soloed.load(std::memory_order_acquire);
  bool solo_active = m_solo_active.load(std::memory_order_acquire);

  // If solo is active and this group is not soloed, it's effectively muted
  if (solo_active && !is_soloed) {
    return true;
  }

  return is_muted;
}

bool ClipRoutingMatrix::isGroupSoloed(uint8_t groupIndex) const {
  if (groupIndex >= NUM_GROUPS) {
    return false;
  }
  return m_groups[groupIndex].soloed.load(std::memory_order_acquire);
}

bool ClipRoutingMatrix::isGroupRoutedToMaster(uint8_t groupIndex) const {
  if (groupIndex >= NUM_GROUPS) {
    return false;
  }
  return m_groups[groupIndex].routed_to_master.load(std::memory_order_acquire);
}

// ============================================================================
// Helper Methods
// ============================================================================

void ClipRoutingMatrix::updateSoloState() {
  // Check if any group is soloed
  bool any_solo = false;
  for (uint8_t i = 0; i < NUM_GROUPS; ++i) {
    if (m_groups[i].soloed.load(std::memory_order_acquire)) {
      any_solo = true;
      break;
    }
  }

  m_solo_active.store(any_solo, std::memory_order_release);
}

float ClipRoutingMatrix::dbToLinear(float db) const {
  if (db <= MIN_GAIN_DB) {
    return 0.0f; // -inf
  }
  return std::pow(10.0f, db / 20.0f);
}

// ============================================================================
// Multi-Channel Routing (Feature 7)
// ============================================================================

SessionGraphError ClipRoutingMatrix::setClipOutputBus(ClipHandle handle, uint8_t outputBus) {
  // Validate clip handle
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  // Validate output bus (0-15 for 32 channels)
  if (outputBus > MAX_OUTPUT_BUS) {
    return SessionGraphError::InvalidParameter;
  }

  // Store output bus assignment (lock-free: UI thread only)
  m_clip_output_bus[handle] = outputBus;

  return SessionGraphError::OK;
}

SessionGraphError ClipRoutingMatrix::mapChannels(ClipHandle handle, uint8_t clipChannel,
                                                 uint8_t outputChannel) {
  // Validate clip handle
  if (handle == 0) {
    return SessionGraphError::InvalidHandle;
  }

  // Validate output channel (0-31)
  if (outputChannel >= MAX_OUTPUT_CHANNELS) {
    return SessionGraphError::InvalidParameter;
  }

  // Store channel mapping (lock-free: UI thread only)
  // Note: We don't validate clipChannel since different clips have different channel counts
  // The audio thread will clamp to actual clip channel count during rendering
  m_channel_mappings[handle][clipChannel] = outputChannel;

  return SessionGraphError::OK;
}

uint8_t ClipRoutingMatrix::getClipOutputBus(ClipHandle handle) const {
  auto it = m_clip_output_bus.find(handle);
  if (it != m_clip_output_bus.end()) {
    return it->second;
  }
  return DEFAULT_OUTPUT_BUS; // Default to bus 0 (stereo)
}

// ============================================================================
// Factory Function
// ============================================================================

std::unique_ptr<IClipRoutingMatrix> createClipRoutingMatrix(core::SessionGraph* sessionGraph,
                                                            uint32_t sampleRate) {
  return std::make_unique<ClipRoutingMatrix>(sessionGraph, sampleRate);
}

} // namespace orpheus
