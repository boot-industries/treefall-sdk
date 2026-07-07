// SPDX-License-Identifier: MIT
#pragma once

#include <atomic>
#include <cstdint>

namespace orpheus {

/// Lock-free gain smoother for click-free parameter changes
///
/// Architecture:
/// - UI thread: Sets target gain via setTarget() (atomic write)
/// - Audio thread: Calls process() to get smoothed gain value
///
/// Design:
/// - Linear ramping (simple, predictable, low CPU)
/// - Configurable smoothing time (1-100ms typical)
/// - Lock-free (no mutex, no allocations)
/// - Zero overshoot (stops exactly at target)
///
/// Inspired by:
/// - Yamaha CL/QL: 10ms default fade time
/// - Calrec Argo: Smooth fader movements
/// - Pro Tools: Automation smoothing
///
/// Usage:
/// @code
///   GainSmoother smoother(sample_rate, 10.0f); // 10ms smoothing
///   smoother.setTarget(0.5f);  // UI thread
///
///   // Audio thread:
///   for (size_t i = 0; i < num_frames; ++i) {
///     float gain = smoother.process();
///     output[i] = input[i] * gain;
///   }
/// @endcode
class GainSmoother {
public:
  /// Construct gain smoother
  /// @param sample_rate Sample rate in Hz
  /// @param smoothing_time_ms Smoothing time in milliseconds. 0 disables
  ///        smoothing entirely (target changes apply immediately); positive
  ///        values are clamped to (0, 100.0]. (ORP127 G4: 0 is now honored
  ///        rather than silently clamped to a 1ms minimum.)
  GainSmoother(uint32_t sample_rate, float smoothing_time_ms);

  /// Set target gain (thread-safe, lock-free)
  /// @param target Target gain value (linear, 0.0 = silence, 1.0 = unity, max = +12 dB)
  /// @note Called from UI thread, takes effect on next audio callback
  void setTarget(float target);

  /// Maximum gain in dB (allows boost capability for professional mixing)
  static constexpr float MAX_GAIN_DB = 12.0f;

  /// Maximum linear gain value (10^(12/20) ≈ 3.981)
  /// 32-bit float provides ~1528 dB headroom; clipping at output stage is sufficient
  static constexpr float MAX_LINEAR_GAIN = 3.981071705534972f;

  /// Get current target gain (thread-safe read)
  /// @return Current target gain
  float getTarget() const;

  /// Process one sample (audio thread only)
  /// @return Current smoothed gain value
  /// @note Call once per sample, returns ramped value toward target
  float process();

  /// Get current gain without advancing (audio thread only)
  /// @return Current gain value
  float getCurrent() const {
    return m_current;
  }

  /// Reset to specific gain immediately (no smoothing)
  /// @param gain Gain value to reset to
  /// @note Use sparingly (causes discontinuity), mainly for initialization
  void reset(float gain);

  /// Check if currently ramping
  /// @return True if gain is changing toward target
  bool isRamping() const {
    if (m_smoothing_disabled) {
      return false; // snaps to target every process(); never "ramping"
    }
    return m_current != m_target || m_has_pending.load(std::memory_order_acquire);
  }

  /// @return True if smoothing is disabled (constructed with 0ms)
  bool isSmoothingDisabled() const {
    return m_smoothing_disabled;
  }

private:
  // Configuration (set once in constructor)
  float m_increment;                ///< Gain change per sample
  bool m_smoothing_disabled{false}; ///< ORP127 G4: true when constructed with 0ms

  // State (audio thread only)
  float m_current; ///< Current gain value
  float m_target;  ///< Target gain value (updated atomically)

  // Thread-safe target update (UI → audio thread)
  std::atomic<float> m_pending_target;
  std::atomic<bool> m_has_pending;
};

} // namespace orpheus
