// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <memory>
#include <orpheus/errors.h>
#include <orpheus/transport_controller.h> // For SessionGraphError
#include <string>
#include <vector>

namespace orpheus {

// Note: SessionGraphError is defined in transport_controller.h

// ============================================================================
// Forward Declarations
// ============================================================================

class IRoutingCallback;

// ============================================================================
// Constants
// ============================================================================

/// Special value indicating channel is not assigned to any group
constexpr uint8_t UNASSIGNED_GROUP = 255;

// ============================================================================
// Routing Configuration Types
// ============================================================================

/// Solo mode behavior (inspired by professional consoles)
enum class SoloMode : uint8_t {
  /// Solo-in-Place: Mutes all non-solo channels
  SIP = 0,

  /// After-Fader-Listen: Routes solo'd channels to dedicated AFL bus (preserves fader level)
  AFL = 1,

  /// Pre-Fader-Listen: Routes solo'd channels to dedicated PFL bus (ignores fader)
  PFL = 2,

  /// Destructive Solo: Stops all non-solo clips (like Ableton)
  Destructive = 3
};

/// Metering mode for audio level detection
enum class MeteringMode : uint8_t {
  Peak = 0,     ///< Peak hold (fastest, most responsive)
  RMS = 1,      ///< Root-mean-square (average energy)
  TruePeak = 2, ///< ITU-R BS.1770 true peak (oversampled)
  LUFS = 3      ///< Loudness Units Full Scale (broadcast standard)
};

/// Channel strip configuration (like a console channel)
struct ChannelConfig {
  std::string name;    ///< Channel name (e.g., "Kick", "Snare", "Music Bed 1")
  uint8_t group_index; ///< Assigned group (0-15, or 255 for unassigned)
  float gain_db;       ///< Channel gain in dB (-inf to +12 dB)
  float pan;           ///< Pan position (-1.0 = hard left, 0.0 = center, +1.0 = hard right)
  bool mute;           ///< Mute flag
  bool solo;           ///< Solo flag
  uint32_t color;      ///< UI color hint (RGBA)

  /// Default constructor
  ChannelConfig()
      : name(""), group_index(0), gain_db(0.0f), pan(0.0f), mute(false), solo(false),
        color(0xFFFFFFFF) {}
};

/// Group (bus) configuration (like a console subgroup)
struct GroupConfig {
  std::string name;   ///< Group name (e.g., "Drums", "Music", "SFX", "Dialogue")
  float gain_db;      ///< Group gain in dB (-inf to +12 dB)
  bool mute;          ///< Mute flag
  bool solo;          ///< Solo flag (groups can be solo'd too)
  uint8_t output_bus; ///< Output bus assignment (0 = master, 1-15 = aux/submix)
  uint32_t color;     ///< UI color hint (RGBA)

  /// Default constructor
  GroupConfig()
      : name(""), gain_db(0.0f), mute(false), solo(false), output_bus(0), color(0xFFFFFFFF) {}
};

/// Routing matrix configuration (complete topology)
struct RoutingConfig {
  uint8_t num_channels; ///< Number of input channels (clips) [1-64]
  uint8_t num_groups;   ///< Number of groups (buses) [1-16]
  uint8_t num_outputs;  ///< Number of output channels [2-32]

  SoloMode solo_mode;         ///< Solo behavior
  MeteringMode metering_mode; ///< Metering algorithm

  float gain_smoothing_ms; ///< Gain change smoothing time (1-100 ms, default 10ms)
  float dim_amount_db;     ///< Dim amount when solo active (-6 to -24 dB, default -12 dB)

  bool enable_metering;            ///< Enable real-time metering (small CPU cost)
  bool enable_clipping_protection; ///< Soft-clip at 0 dBFS to prevent hard clipping

  /// Default constructor (sensible defaults for OCC)
  RoutingConfig()
      : num_channels(16), num_groups(4), num_outputs(2), solo_mode(SoloMode::SIP),
        metering_mode(MeteringMode::Peak), gain_smoothing_ms(10.0f), dim_amount_db(-12.0f),
        enable_metering(true), enable_clipping_protection(true) {}
};

/// Audio level meters (per-channel or per-group)
struct AudioMeter {
  float peak_db;       ///< Peak level in dBFS (-inf to 0.0)
  float rms_db;        ///< RMS level in dBFS (-inf to 0.0)
  bool clipping;       ///< Clipping detected flag
  uint32_t clip_count; ///< Number of samples clipped since reset

  AudioMeter() : peak_db(-100.0f), rms_db(-100.0f), clipping(false), clip_count(0) {}
};

/// Routing snapshot (preset) - stores complete routing state
struct RoutingSnapshot {
  std::string name;      ///< Snapshot name
  uint32_t timestamp_ms; ///< Creation timestamp

  // Channel states
  std::vector<ChannelConfig> channels;

  // Group states
  std::vector<GroupConfig> groups;

  // Master output state
  float master_gain_db;
  bool master_mute;

  RoutingSnapshot() : name("Default"), timestamp_ms(0), master_gain_db(0.0f), master_mute(false) {}
};

// ============================================================================
// Routing Matrix Interface
// ============================================================================

/// Routing matrix interface - professional N×M audio routing
///
/// Architecture:
///   Channels (Clips) → Groups (Buses) → Outputs (Master, Aux, etc.)
///
/// Inspired by:
/// - Audinate Dante Controller: Flexible N×M routing, subscription model
/// - Calrec Argo: Summing buses, sophisticated solo/mute logic
/// - Yamaha CL/QL: Scene memory, smooth parameter changes
///
/// Key Features:
/// - Up to 64 channels → 16 groups → 32 outputs
/// - Multiple solo modes (SIP, AFL, PFL, Destructive)
/// - Per-channel and per-group gain with smoothing (click-free)
/// - Real-time metering (Peak/RMS/TruePeak/LUFS)
/// - Snapshot/preset system for instant recall
/// - Lock-free audio thread (UI updates never block audio)
/// - Clipping protection (soft-clip before 0 dBFS)
/// - Broadcast-safe (zero allocations in audio thread)
///
/// Thread Safety:
/// - initialize(), configure*(), load*(): UI thread only (mutex protected)
/// - process*(): Audio thread only (lock-free)
/// - get*(), isSolo*(), isMute*(): Any thread (atomic reads)
///
/// Typical Usage:
/// @code
///   auto routing = createRoutingMatrix();
///
///   RoutingConfig config;
///   config.num_channels = 16;
///   config.num_groups = 4;
///   config.solo_mode = SoloMode::SIP;
///   routing->initialize(config);
///
///   routing->setChannelGroup(0, 0);  // Assign channel 0 to group 0 (Drums)
///   routing->setChannelGain(0, -3.0f);  // -3 dB
///   routing->setGroupGain(0, 0.0f);  // 0 dB (unity)
///
///   // In audio thread:
///   routing->processRouting(clip_outputs, master_output, num_frames);
/// @endcode
class IRoutingMatrix {
public:
  virtual ~IRoutingMatrix() = default;

  // ========================================================================
  // Initialization & Configuration (UI Thread)
  // ========================================================================

  /// Initialize routing matrix with configuration
  /// @param config Routing configuration
  /// @return Error code
  /// @note Must be called before any other methods
  /// @note Can be called multiple times to reconfigure (stops audio processing)
  virtual SessionGraphError initialize(const RoutingConfig& config) = 0;

  /// Get current configuration
  /// @return Configuration snapshot
  virtual RoutingConfig getConfig() const = 0;

  /// Set routing callback for events (optional)
  /// @param callback Callback interface (nullptr to disable)
  virtual void setCallback(IRoutingCallback* callback) = 0;

  // ========================================================================
  // Channel Configuration (UI Thread, Lock-Free)
  // ========================================================================

  /// Assign channel to group (bus assignment)
  /// @param channel_index Channel index [0, num_channels)
  /// @param group_index Group index [0, num_groups) or 255 for unassigned
  /// @return Error code
  /// @note Lock-free update, takes effect on next audio callback
  virtual SessionGraphError setChannelGroup(uint8_t channel_index, uint8_t group_index) = 0;

  /// Set channel gain
  /// @param channel_index Channel index [0, num_channels)
  /// @param gain_db Gain in dB [-inf, +12.0]
  /// @return Error code
  /// @note Smoothed over gain_smoothing_ms to prevent clicks
  virtual SessionGraphError setChannelGain(uint8_t channel_index, float gain_db) = 0;

  /// Set channel pan (stereo positioning)
  /// @param channel_index Channel index [0, num_channels)
  /// @param pan Pan position [-1.0 = hard left, 0.0 = center, +1.0 = hard right]
  /// @return Error code
  /// @note Smoothed pan law: constant-power (-3 dB at center)
  virtual SessionGraphError setChannelPan(uint8_t channel_index, float pan) = 0;

  /// Set channel mute
  /// @param channel_index Channel index [0, num_channels)
  /// @param mute Mute flag
  /// @return Error code
  virtual SessionGraphError setChannelMute(uint8_t channel_index, bool mute) = 0;

  /// Set channel solo
  /// @param channel_index Channel index [0, num_channels)
  /// @param solo Solo flag
  /// @return Error code
  /// @note Behavior depends on solo_mode (SIP, AFL, PFL, Destructive)
  virtual SessionGraphError setChannelSolo(uint8_t channel_index, bool solo) = 0;

  /// Configure channel (batch update for efficiency)
  /// @param channel_index Channel index [0, num_channels)
  /// @param config Channel configuration
  /// @return Error code
  virtual SessionGraphError configureChannel(uint8_t channel_index,
                                             const ChannelConfig& config) = 0;

  // ========================================================================
  // Group Configuration (UI Thread, Lock-Free)
  // ========================================================================

  /// Set group gain
  /// @param group_index Group index [0, num_groups)
  /// @param gain_db Gain in dB [-inf, +12.0]
  /// @return Error code
  /// @note Smoothed over gain_smoothing_ms
  virtual SessionGraphError setGroupGain(uint8_t group_index, float gain_db) = 0;

  /// Set group mute
  /// @param group_index Group index [0, num_groups)
  /// @param mute Mute flag
  /// @return Error code
  virtual SessionGraphError setGroupMute(uint8_t group_index, bool mute) = 0;

  /// Set group solo
  /// @param group_index Group index [0, num_groups)
  /// @param solo Solo flag
  /// @return Error code
  virtual SessionGraphError setGroupSolo(uint8_t group_index, bool solo) = 0;

  /// Configure group (batch update)
  /// @param group_index Group index [0, num_groups)
  /// @param config Group configuration
  /// @return Error code
  virtual SessionGraphError configureGroup(uint8_t group_index, const GroupConfig& config) = 0;

  // ========================================================================
  // Master Output Configuration (UI Thread, Lock-Free)
  // ========================================================================

  /// Set master output gain
  /// @param gain_db Gain in dB [-inf, +12.0]
  /// @return Error code
  virtual SessionGraphError setMasterGain(float gain_db) = 0;

  /// Set master mute
  /// @param mute Mute flag
  /// @return Error code
  virtual SessionGraphError setMasterMute(bool mute) = 0;

  // ========================================================================
  // State Queries (Any Thread, Lock-Free Reads)
  // ========================================================================

  /// Check if any channel is solo'd
  /// @return True if solo active
  virtual bool isSoloActive() const = 0;

  /// Check if channel is muted (considering solo logic)
  /// @param channel_index Channel index [0, num_channels)
  /// @return True if effectively muted
  virtual bool isChannelMuted(uint8_t channel_index) const = 0;

  /// Check if group is muted (considering solo logic)
  /// @param group_index Group index [0, num_groups)
  /// @return True if effectively muted
  virtual bool isGroupMuted(uint8_t group_index) const = 0;

  /// Get channel meter
  /// @param channel_index Channel index [0, num_channels)
  /// @return Audio meter (peak, RMS, clipping)
  virtual AudioMeter getChannelMeter(uint8_t channel_index) const = 0;

  /// Get group meter
  /// @param group_index Group index [0, num_groups)
  /// @return Audio meter
  virtual AudioMeter getGroupMeter(uint8_t group_index) const = 0;

  /// Get master meter
  /// @return Audio meter
  virtual AudioMeter getMasterMeter() const = 0;

  // ========================================================================
  // Snapshot/Preset Management (UI Thread)
  // ========================================================================

  /// Save current routing state as snapshot
  /// @param name Snapshot name
  /// @return Snapshot object
  virtual RoutingSnapshot saveSnapshot(const std::string& name) = 0;

  /// Load routing state from snapshot
  /// @param snapshot Snapshot to load
  /// @return Error code
  /// @note All parameters smoothly transition to new values
  virtual SessionGraphError loadSnapshot(const RoutingSnapshot& snapshot) = 0;

  /// Reset all channels/groups to default state
  /// @return Error code
  virtual SessionGraphError reset() = 0;

  // ========================================================================
  // Audio Processing (Audio Thread, Lock-Free)
  // ========================================================================

  /// Process routing for one audio buffer
  ///
  /// Routing flow:
  ///   1. Read channel inputs (from clip outputs)
  ///   2. Apply channel gain/pan/mute/solo
  ///   3. Sum channels into groups
  ///   4. Apply group gain/mute/solo
  ///   5. Sum groups into master output
  ///   6. Apply master gain/mute
  ///   7. Update meters (if enabled)
  ///
  /// @param channel_inputs Input buffers [num_channels][num_frames] (planar float32)
  /// @param master_output Output buffer [num_outputs][num_frames] (planar float32)
  /// @param num_frames Number of frames to process
  /// @return Error code (unlikely to fail in audio thread)
  ///
  /// @note Zero allocations, lock-free, real-time safe
  /// @note Input buffers can be nullptr for channels with no audio
  virtual SessionGraphError processRouting(const float* const* channel_inputs,
                                           float** master_output, uint32_t num_frames) = 0;
};

// ============================================================================
// Routing Callback Interface
// ============================================================================

/// Callback interface for routing events (UI thread)
class IRoutingCallback {
public:
  virtual ~IRoutingCallback() = default;

  /// Called when channel gain changes
  /// @param channel_index Channel that changed
  /// @param gain_db New gain value
  virtual void onChannelGainChanged(uint8_t channel_index, float gain_db) = 0;

  /// Called when group gain changes
  /// @param group_index Group that changed
  /// @param gain_db New gain value
  virtual void onGroupGainChanged(uint8_t group_index, float gain_db) = 0;

  /// Called when solo state changes
  /// @param active True if any channel/group is solo'd
  virtual void onSoloStateChanged(bool active) = 0;

  /// Called when clipping detected
  /// @param channel_index Channel that clipped (255 for master)
  /// @param peak_db Peak level in dBFS
  virtual void onClippingDetected(uint8_t channel_index, float peak_db) = 0;
};

// ============================================================================
// Factory Function
// ============================================================================

/// Create routing matrix instance
/// @return Unique pointer to routing matrix
std::unique_ptr<IRoutingMatrix> createRoutingMatrix();

} // namespace orpheus
