// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <memory>
#include <orpheus/transport_controller.h> // For ClipHandle, SessionGraphError

namespace orpheus {

// Forward declaration
namespace core {
class SessionGraph;
} // namespace core

/// Clip-to-group routing matrix for multi-group mixing
///
/// This is a simplified routing API designed for clip-based workflows
/// (e.g., Orpheus Clip Composer). Unlike the full RoutingMatrix API
/// which uses generic channel indices, this API works with ClipHandles
/// from the transport system.
///
/// Architecture:
///   Clips (via ClipHandle) → 4 Clip Groups → Master Bus
///
/// Key Features:
/// - 4 Clip Groups (0-3) with independent gain/mute/solo controls
/// - Per-group gain (-60 to +12 dB) with 10ms smoothing
/// - Solo logic: When any group is soloed, all non-soloed groups are muted
/// - Thread-safe: UI thread updates, audio thread processes
/// - Sample-accurate mute/solo (no mid-buffer discontinuities)
///
/// Thread Safety:
/// - assignClipToGroup(), setGroup*(): UI thread only
/// - getClipGroup(), getGroupGain(), isGroup*(): Any thread (atomic reads)
///
/// Typical Usage:
/// @code
///   auto routing = createClipRoutingMatrix(sessionGraph, 48000);
///
///   // Assign clips to groups
///   routing->assignClipToGroup(clipHandle1, 0);  // Drums
///   routing->assignClipToGroup(clipHandle2, 1);  // Music
///
///   // Configure group mixing
///   routing->setGroupGain(0, -3.0f);  // Drums at -3 dB
///   routing->setGroupSolo(1, true);   // Solo music group
/// @endcode
class IClipRoutingMatrix {
public:
  virtual ~IClipRoutingMatrix() = default;

  // ========================================================================
  // Clip Assignment (UI Thread)
  // ========================================================================

  /// Assign clip to one of 4 Clip Groups (0-3)
  /// @param handle Clip handle from transport controller
  /// @param groupIndex Clip Group index (0-3, or 255 for "no group")
  /// @return SessionGraphError::OK on success
  /// @note Lock-free update, takes effect immediately in audio thread
  virtual SessionGraphError assignClipToGroup(ClipHandle handle, uint8_t groupIndex) = 0;

  // ========================================================================
  // Group Configuration (UI Thread, Lock-Free)
  // ========================================================================

  /// Set gain for entire Clip Group
  /// @param groupIndex Group index (0-3)
  /// @param gainDb Gain in decibels (-60 to +12 dB)
  /// @return SessionGraphError::OK on success
  /// @note Gain changes are smoothed over 10ms to prevent clicks
  virtual SessionGraphError setGroupGain(uint8_t groupIndex, float gainDb) = 0;

  /// Mute/unmute Clip Group
  /// @param groupIndex Group index (0-3)
  /// @param muted true = mute, false = unmute
  /// @return SessionGraphError::OK on success
  /// @note Sample-accurate mute (no mid-buffer discontinuities)
  virtual SessionGraphError setGroupMute(uint8_t groupIndex, bool muted) = 0;

  /// Solo Clip Group (mutes all other groups)
  /// @param groupIndex Group index (0-3)
  /// @param soloed true = solo this group, false = unsolo
  /// @return SessionGraphError::OK on success
  /// @note Solo logic: When any group is soloed, all non-soloed groups are muted
  virtual SessionGraphError setGroupSolo(uint8_t groupIndex, bool soloed) = 0;

  /// Enable/disable routing of group to master bus
  /// @param groupIndex Group index (0-3)
  /// @param enabled true = route to master, false = disable
  /// @return SessionGraphError::OK on success
  /// @note Disabled groups do not contribute to master output
  virtual SessionGraphError routeGroupToMaster(uint8_t groupIndex, bool enabled) = 0;

  // ========================================================================
  // State Queries (Any Thread, Lock-Free Reads)
  // ========================================================================

  /// Get current group assignment for clip
  /// @param handle Clip handle
  /// @return Group index (0-3), or 255 if not assigned
  virtual uint8_t getClipGroup(ClipHandle handle) const = 0;

  /// Get current group gain
  /// @param groupIndex Group index (0-3)
  /// @return Gain in dB, or 0.0 if group not initialized
  virtual float getGroupGain(uint8_t groupIndex) const = 0;

  /// Query if group is muted
  /// @param groupIndex Group index (0-3)
  /// @return true if muted (or effectively muted by solo logic)
  virtual bool isGroupMuted(uint8_t groupIndex) const = 0;

  /// Query if group is soloed
  /// @param groupIndex Group index (0-3)
  /// @return true if soloed
  virtual bool isGroupSoloed(uint8_t groupIndex) const = 0;

  /// Query if group is routed to master
  /// @param groupIndex Group index (0-3)
  /// @return true if routing enabled
  virtual bool isGroupRoutedToMaster(uint8_t groupIndex) const = 0;

  // ========================================================================
  // Multi-Channel Routing (Beyond Stereo - Feature 7)
  // ========================================================================

  /// Set output bus for clip (beyond stereo)
  ///
  /// Professional audio interfaces have 8-32 channels. This allows routing clips
  /// to specific output buses (e.g., channels 5-6, 7-8, etc.).
  ///
  /// @param handle Clip handle from transport controller
  /// @param outputBus Bus index (0 = channels 1-2, 1 = channels 3-4, etc.)
  /// @return SessionGraphError::OK on success
  /// @note Bus 0 is default (stereo output, channels 1-2)
  /// @note Maximum bus index is 15 (channels 31-32)
  virtual SessionGraphError setClipOutputBus(ClipHandle handle, uint8_t outputBus) = 0;

  /// Map clip channel to output channel
  ///
  /// Allows fine-grained channel routing (e.g., clip channel 0 → output channel 5).
  /// This overrides bus-based routing for individual channels.
  ///
  /// @param handle Clip handle
  /// @param clipChannel Clip channel (0 = L, 1 = R for stereo clip)
  /// @param outputChannel Output channel (0-31, 0 = left channel 1, 1 = right channel 2)
  /// @return SessionGraphError::OK on success
  /// @note This is advanced routing - most users will use setClipOutputBus()
  /// @note Output channels are 0-indexed (0 = physical channel 1)
  virtual SessionGraphError mapChannels(ClipHandle handle, uint8_t clipChannel,
                                        uint8_t outputChannel) = 0;

  /// Get clip output bus
  /// @param handle Clip handle
  /// @return Output bus index (0-15), or 0 if not assigned
  virtual uint8_t getClipOutputBus(ClipHandle handle) const = 0;
};

/// Create clip routing matrix instance
/// @param sessionGraph Session graph containing clip metadata
/// @param sampleRate Audio sample rate (e.g., 48000)
/// @return Unique pointer to clip routing matrix
std::unique_ptr<IClipRoutingMatrix> createClipRoutingMatrix(core::SessionGraph* sessionGraph,
                                                            uint32_t sampleRate);

} // namespace orpheus
