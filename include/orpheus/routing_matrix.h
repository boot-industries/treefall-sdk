// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/export.h>
#include <orpheus/time_domain.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <orpheus/errors.h>
#include <string>
#include <type_traits>
#include <vector>

namespace orpheus {

// ============================================================================
// Forward Declarations
// ============================================================================

class IRoutingCallback;

// ============================================================================
// Constants
// ============================================================================

using RoutingChannelIndex = uint16_t;
using RoutingGroupIndex = uint16_t;
using RoutingOutputIndex = uint16_t;

/// Special value indicating channel is not assigned to any group.
constexpr RoutingGroupIndex UNASSIGNED_GROUP = UINT16_MAX;

/// FTR028: Internal processing-slice size (frames) used by processRouting().
///
/// The routing matrix pre-allocates its scratch buffers to this size and
/// processes audio in slices no larger than this on the real-time path (no
/// audio-thread allocation, ever). processRouting() accepts an arbitrary
/// num_frames and chunks internally over slices of this size, so hosts do NOT
/// need to cap their block size against it.
///
/// It is exposed so real-time hosts that want each processRouting() call to
/// map to exactly one internal slice (e.g. to align with their audio device
/// buffer size) can size their render blocks against it. Offline hosts
/// (bounce/export) may pass blocks of any size and let chunking handle it.
constexpr uint32_t kRoutingSliceFrames = 2048;

/// Fixed public capacity for coherent group-control snapshots.
constexpr size_t kRoutingControlMaxGroups = 32;
/// Maximum physical output lanes exposed by routing and telemetry.
constexpr size_t kRoutingMaxOutputs = 32;

/// Schema version for RoutingControlSnapshot.
constexpr uint32_t kRoutingControlSnapshotSchemaVersion = 1;

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

/// ORP121 Q-05: Headroom management mode for automatic gain compensation
/// Prevents summing overflow when multiple channels are mixed to a group
enum class HeadroomMode : uint8_t {
  None = 0,       ///< No compensation (sum can exceed 0 dBFS)
  PerGroup = 1,   ///< Divide by number of active channels per group (1/n)
  Global = 2,     ///< Divide by total active channels across all groups
  Logarithmic = 3 ///< -3 dB per doubling of channels (1/sqrt(n))
};

/// How the routing graph treats source channels before they enter buses.
enum class SourceChannelPolicy : uint8_t {
  /// Each source channel is represented as an independent routing channel.
  /// This is the target policy for multichannel/broadcast workflows.
  Discrete = 0,

  /// Preserve stereo pairs, duplicating mono to L/R and downmixing wider
  /// sources according to downmix_policy. This matches the current transport
  /// controller behavior for clip playback.
  StereoPairs = 1,

  /// Collapse source channels to mono before routing.
  MonoFoldDown = 2
};

/// Downmix policy for sources wider than the configured output topology.
enum class DownmixPolicy : uint8_t {
  /// Do not downmix automatically. Missing routing must be configured by host.
  None = 0,

  /// ITU-R BS.775-style surround-to-stereo coefficients.
  ITU_BS775_3 = 1,

  /// Equal-power fold-down across source channels.
  EqualPower = 2
};

/// Channel strip configuration (like a console channel)
struct ChannelConfig {
  std::string name;                  ///< Human-readable channel name
  RoutingGroupIndex group_index;     ///< Assigned group, or UNASSIGNED_GROUP
  RoutingOutputIndex output_channel; ///< Discrete destination within the group bus
  float gain_db;                     ///< Channel gain in dB (-inf to +12 dB)
  float pan;      ///< Pan position (-1.0 = hard left, 0.0 = center, +1.0 = hard right)
  bool mute;      ///< Mute flag
  bool solo;      ///< Solo flag
  uint32_t color; ///< UI color hint (RGBA)

  /// Default constructor
  ChannelConfig()
      : name(""), group_index(0), output_channel(0), gain_db(0.0f), pan(0.0f), mute(false),
        solo(false), color(0xFFFFFFFF) {}
};

/// Group (bus) configuration (like a console subgroup)
struct GroupConfig {
  std::string name;                ///< Group name (e.g., "Drums", "Music", "SFX", "Dialogue")
  float gain_db;                   ///< Group gain in dB (-inf to +12 dB)
  bool mute;                       ///< Mute flag
  bool solo;                       ///< Solo flag (groups can be solo'd too)
  RoutingOutputIndex output_start; ///< First physical output for this logical bus
  uint16_t output_width;           ///< Number of routed channels in this bus
  uint32_t color;                  ///< UI color hint (RGBA)

  /// Default constructor
  GroupConfig()
      : name(""), gain_db(0.0f), mute(false), solo(false), output_start(0), output_width(2),
        color(0xFFFFFFFF) {}
};

/// One logical group's configured and effective control state.
struct RoutingGroupControlState {
  float gain_db{0.0f};
  RoutingOutputIndex output_start{0};
  uint16_t output_width{0};
  bool configured_mute{false};
  bool configured_solo{false};
  bool effective_mute{false};
  uint8_t reserved{0};
};

/// Fixed-capacity coherent view of every configured logical group.
///
/// revision changes after every accepted group-control mutation. configured_mute
/// remains the operator-authored flag; effective_mute additionally reflects
/// group-solo logic.
struct RoutingControlSnapshot {
  uint32_t schema_version{kRoutingControlSnapshotSchemaVersion};
  RoutingGroupIndex group_count{0};
  uint16_t reserved{0};
  uint64_t revision{0};
  std::array<RoutingGroupControlState, kRoutingControlMaxGroups> groups{};
};

static_assert(std::is_trivially_copyable_v<RoutingGroupControlState>);
static_assert(std::is_standard_layout_v<RoutingGroupControlState>);
static_assert(std::is_trivially_copyable_v<RoutingControlSnapshot>);
static_assert(std::is_standard_layout_v<RoutingControlSnapshot>);

/// Routing matrix configuration (complete topology)
struct RoutingConfig {
  RoutingChannelIndex num_channels; ///< Number of input routing lanes [1-256]
  RoutingGroupIndex num_groups;     ///< Number of logical groups [1-32]
  RoutingOutputIndex num_outputs;   ///< Number of output channels [1-32]

  SoloMode solo_mode;         ///< Solo behavior
  MeteringMode metering_mode; ///< Metering algorithm

  float gain_smoothing_ms; ///< Gain change smoothing time (1-100 ms, default 10ms)
  float dim_amount_db;     ///< Dim amount when solo active (-6 to -24 dB, default -12 dB)

  bool enable_metering;            ///< Enable real-time metering (small CPU cost)
  bool enable_clipping_protection; ///< Soft-clip at 0 dBFS to prevent hard clipping

  /// ORP121 Q-03: Audio sample rate for smoother calculations
  uint32_t sample_rate; ///< Audio sample rate in Hz (default 48000)

  /// ORP121 Q-05: Automatic headroom management
  HeadroomMode headroom_mode; ///< Gain compensation mode (default: None)

  /// Multichannel source/channel contract.
  SourceChannelPolicy source_channel_policy; ///< How source channels enter routing
  DownmixPolicy downmix_policy;              ///< Wider-source downmix strategy

  /// Default constructor (typical broadcast/soundboard defaults; hosts should
  /// configure num_channels/num_groups/etc. as needed on init)
  RoutingConfig()
      : num_channels(16), num_groups(4), num_outputs(2), solo_mode(SoloMode::SIP),
        metering_mode(MeteringMode::Peak), gain_smoothing_ms(10.0f), dim_amount_db(-12.0f),
        enable_metering(true), enable_clipping_protection(true), sample_rate(48000),
        headroom_mode(HeadroomMode::None), source_channel_policy(SourceChannelPolicy::StereoPairs),
        downmix_policy(DownmixPolicy::ITU_BS775_3) {}
};

/// Audio level meters (per-channel or per-group)
struct AudioMeter {
  float peak_db;       ///< Peak level in dBFS (-inf to 0.0)
  float rms_db;        ///< RMS level in dBFS (-inf to 0.0)
  bool clipping;       ///< Clipping detected flag
  uint32_t clip_count; ///< Number of samples clipped since reset

  AudioMeter() : peak_db(-100.0f), rms_db(-100.0f), clipping(false), clip_count(0) {}
};

/// Optional caller-supplied provenance for a routing snapshot. Neither field
/// participates in deterministic routing identity or render hashes.
struct RoutingSnapshotContext {
  std::optional<uint64_t> controlTimeMs;
  std::optional<TimePoint> audioPosition;
};

/// Routing snapshot (preset) - stores complete routing state
struct RoutingSnapshot {
  std::string name;
  uint64_t captureRevision{0};            ///< Monotonic per-matrix capture order
  std::optional<uint64_t> controlTimeMs;  ///< Caller-provided display provenance
  std::optional<TimePoint> audioPosition; ///< Caller-provided audio coordinate

  std::vector<ChannelConfig> channels;
  std::vector<GroupConfig> groups;

  float master_gain_db{0.0f};
  bool master_mute{false};
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
  /// @param group_index Group index [0, num_groups) or UNASSIGNED_GROUP
  /// @return Error code
  /// @note Lock-free update, takes effect on next audio callback
  virtual SessionGraphError setChannelGroup(RoutingChannelIndex channel_index,
                                            RoutingGroupIndex group_index) = 0;

  /// Atomically assign one source lane to a logical bus and hardware output.
  /// Audio-thread-safe: updates fixed-capacity POD routing state only.
  virtual SessionGraphError setChannelRoute(RoutingChannelIndex channel_index,
                                            RoutingGroupIndex group_index,
                                            RoutingOutputIndex output_index) = 0;

  /// Set channel gain
  /// @param channel_index Channel index [0, num_channels)
  /// @param gain_db Gain in dB [-inf, +12.0]
  /// @return Error code
  /// @note Smoothed over gain_smoothing_ms to prevent clicks
  virtual SessionGraphError setChannelGain(RoutingChannelIndex channel_index, float gain_db) = 0;

  /// Set channel pan (stereo positioning)
  /// @param channel_index Channel index [0, num_channels)
  /// @param pan Pan position [-1.0 = hard left, 0.0 = center, +1.0 = hard right]
  /// @return Error code
  /// @note Smoothed pan law: constant-power (-3 dB at center)
  virtual SessionGraphError setChannelPan(RoutingChannelIndex channel_index, float pan) = 0;

  /// Set channel mute
  /// @param channel_index Channel index [0, num_channels)
  /// @param mute Mute flag
  /// @return Error code
  virtual SessionGraphError setChannelMute(RoutingChannelIndex channel_index, bool mute) = 0;

  /// Set channel solo
  /// @param channel_index Channel index [0, num_channels)
  /// @param solo Solo flag
  /// @return Error code
  /// @note Behavior depends on solo_mode (SIP, AFL, PFL, Destructive)
  virtual SessionGraphError setChannelSolo(RoutingChannelIndex channel_index, bool solo) = 0;

  /// Configure channel (batch update for efficiency)
  /// @param channel_index Channel index [0, num_channels)
  /// @param config Channel configuration
  /// @return Error code
  virtual SessionGraphError configureChannel(RoutingChannelIndex channel_index,
                                             const ChannelConfig& config) = 0;

  // ========================================================================
  // Group Configuration (UI Thread, Lock-Free)
  // ========================================================================

  /// Set group gain
  /// @param group_index Group index [0, num_groups)
  /// @param gain_db Gain in dB [-inf, +12.0]
  /// @return Error code
  /// @note Smoothed over gain_smoothing_ms
  virtual SessionGraphError setGroupGain(RoutingGroupIndex group_index, float gain_db) = 0;

  /// Set group mute
  /// @param group_index Group index [0, num_groups)
  /// @param mute Mute flag
  /// @return Error code
  virtual SessionGraphError setGroupMute(RoutingGroupIndex group_index, bool mute) = 0;

  /// Set group solo
  /// @param group_index Group index [0, num_groups)
  /// @param solo Solo flag
  /// @return Error code
  virtual SessionGraphError setGroupSolo(RoutingGroupIndex group_index, bool solo) = 0;

  /// Configure group (batch update)
  /// @param group_index Group index [0, num_groups)
  /// @param config Group configuration
  /// @return Error code
  virtual SessionGraphError configureGroup(RoutingGroupIndex group_index,
                                           const GroupConfig& config) = 0;

  /// Atomically route a logical group bus to a contiguous physical output range.
  virtual SessionGraphError setGroupOutputRoute(RoutingGroupIndex group_index,
                                                RoutingOutputIndex output_start,
                                                uint16_t output_width) = 0;

  /// Validate and apply all configured group controls as one transaction.
  ///
  /// group_count must exactly match the matrix configuration. Validation
  /// completes before live state changes; any rejection preserves the prior
  /// routing state and revision. Accepted controls become visible together at
  /// one render boundary. Call from the host's single control thread.
  virtual SessionGraphError applyGroupControlSnapshot(const RoutingControlSnapshot& snapshot) = 0;

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
  virtual bool isChannelMuted(RoutingChannelIndex channel_index) const = 0;

  /// Check if group is muted (considering solo logic)
  /// @param group_index Group index [0, num_groups)
  /// @return True if effectively muted
  virtual bool isGroupMuted(RoutingGroupIndex group_index) const = 0;

  /// Read one coherent fixed-capacity group-control state value.
  ///
  /// Call from a host control thread; it remains safe while rendering runs
  /// concurrently. This query does not allocate and never exposes a partially
  /// applied applyGroupControlSnapshot() transaction.
  virtual RoutingControlSnapshot getRoutingControlSnapshot() const noexcept = 0;

  /// Get one channel's isolated effective contribution meter.
  ///
  /// The reading is taken after channel gain smoothing, pan, mute, and
  /// effective channel-solo logic, but before unrelated channels are summed
  /// and before group or master processing. Stereo peak is the maximum lane
  /// peak; RMS is the square root of the mean power across both lanes. A null,
  /// unrouted, or effectively muted channel publishes silence in the current
  /// processRouting() call. clip_count remains cumulative until reset.
  /// @param channel_index Channel index [0, num_channels)
  /// @return Isolated channel meter (peak, RMS, clipping)
  virtual AudioMeter getChannelMeter(RoutingChannelIndex channel_index) const = 0;

  /// Get group meter
  /// @param group_index Group index [0, num_groups)
  /// @return Audio meter
  virtual AudioMeter getGroupMeter(RoutingGroupIndex group_index) const = 0;
  /// Get one physical output lane's post-master, post-protection meter.
  /// Invalid or unconfigured output indices return silence.
  virtual AudioMeter getOutputMeter(RoutingOutputIndex output_index) const = 0;

  /// Get master meter
  /// @return Audio meter
  virtual AudioMeter getMasterMeter() const = 0;

  // ========================================================================
  // Snapshot/Preset Management (UI Thread)
  // ========================================================================

  /// Save current routing state as snapshot
  /// @param name Snapshot name
  /// @return Snapshot object
  virtual RoutingSnapshot saveSnapshot(const std::string& name,
                                       RoutingSnapshotContext context = {}) = 0;

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
  /// @param num_frames Number of frames to process (any size; see @note below)
  /// @return Error code (unlikely to fail in audio thread)
  ///
  /// @note Zero allocations, lock-free, real-time safe
  /// @note Input buffers can be nullptr for channels with no audio
  /// @note FTR028: num_frames may be arbitrarily large. Internally the matrix
  ///       processes the buffer in slices of at most kRoutingSliceFrames
  ///       (== maxBlockFrames()); this chunking is allocation-free and
  ///       lock-free, so large offline blocks (bounce/export) "just work"
  ///       without the host needing to cap its render block size. Metering
  ///       reflects the final slice of the call.
  virtual SessionGraphError processRouting(const float* const* channel_inputs,
                                           float* const* master_output, uint32_t num_frames) = 0;

  /// FTR028: Largest number of frames processed in a single internal slice.
  ///
  /// processRouting() accepts any num_frames and chunks internally, so hosts
  /// are NOT required to respect this limit. It is exposed so real-time hosts
  /// that want a 1:1 mapping between a processRouting() call and one internal
  /// slice can size their render loops against it.
  ///
  /// @return Slice size in frames (kRoutingSliceFrames)
  virtual uint32_t maxBlockFrames() const = 0;
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
  virtual void onChannelGainChanged(RoutingChannelIndex channel_index, float gain_db) = 0;

  /// Called when group gain changes
  /// @param group_index Group that changed
  /// @param gain_db New gain value
  virtual void onGroupGainChanged(RoutingGroupIndex group_index, float gain_db) = 0;

  /// Called when solo state changes
  /// @param active True if any channel/group is solo'd
  virtual void onSoloStateChanged(bool active) = 0;

  /// Called when clipping detected
  /// @param channel_index Channel that clipped (UNASSIGNED_GROUP for master)
  /// @param peak_db Peak level in dBFS
  virtual void onClippingDetected(RoutingChannelIndex channel_index, float peak_db) = 0;
};

// ============================================================================
// Factory Function
// ============================================================================

/// Create routing matrix instance
/// @return Unique pointer to routing matrix
ORPHEUS_API std::unique_ptr<IRoutingMatrix> createRoutingMatrix();

} // namespace orpheus
