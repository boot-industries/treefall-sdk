// SPDX-License-Identifier: MIT
#include "gain_smoother.h"

#include <algorithm>
#include <cmath>

namespace orpheus {

GainSmoother::GainSmoother(uint32_t sample_rate, float smoothing_time_ms)
    : m_current(1.0f), m_target(1.0f), m_pending_target(1.0f), m_has_pending(false) {
  // ORP127 G4 (F-SDK-5): smoothing_time_ms == 0 means NO smoothing — apply
  // target changes immediately. Previously this was silently clamped to a 1ms
  // minimum, so callers requesting 0 got ~1ms of unexpected ramp (e.g. pan
  // smoothers audibly ramping from their 0.707 init on the first sample). We
  // now honor 0 explicitly; any positive value is still clamped to <=100ms.
  if (smoothing_time_ms <= 0.0f) {
    m_smoothing_disabled = true;
    m_increment = 1.0f; // unused when disabled, but keep it well-defined
    return;
  }

  smoothing_time_ms = std::clamp(smoothing_time_ms, 0.0f, 100.0f);

  // Calculate increment per sample for full gain change (0.0 → 1.0)
  // Example: 48kHz, 10ms → 480 samples → increment = 1.0 / 480 ≈ 0.00208
  float smoothing_samples = (smoothing_time_ms / 1000.0f) * static_cast<float>(sample_rate);
  m_increment = 1.0f / smoothing_samples;
}

void GainSmoother::setTarget(float target) {
  // ORP121 C-01: Allow gains up to +12 dB (3.981 linear)
  // Rationale: Professional mixing requires boost capability
  // 32-bit float provides ~1528 dB headroom, clipping at output stage is sufficient
  target = std::clamp(target, 0.0f, MAX_LINEAR_GAIN);

  // Atomic write (lock-free)
  m_pending_target.store(target, std::memory_order_release);
  m_has_pending.store(true, std::memory_order_release);
}

float GainSmoother::getTarget() const {
  // Check if pending update exists
  if (m_has_pending.load(std::memory_order_acquire)) {
    return m_pending_target.load(std::memory_order_acquire);
  }
  return m_target;
}

float GainSmoother::process() {
  // Check for pending target update (lock-free)
  if (m_has_pending.load(std::memory_order_acquire)) {
    m_target = m_pending_target.load(std::memory_order_acquire);
    m_has_pending.store(false, std::memory_order_release);
  }

  // ORP127 G4 (F-SDK-5): with smoothing disabled, snap to target immediately.
  if (m_smoothing_disabled) {
    m_current = m_target;
    return m_current;
  }

  // Save current value to return (before ramping)
  float output = m_current;

  // Ramp toward target for next sample
  if (m_current < m_target) {
    // Ramping up
    m_current += m_increment;
    if (m_current >= m_target) {
      m_current = m_target; // Clamp to target (no overshoot)
    }
  } else if (m_current > m_target) {
    // Ramping down
    m_current -= m_increment;
    if (m_current <= m_target) {
      m_current = m_target; // Clamp to target (no overshoot)
    }
  }

  return output;
}

void GainSmoother::reset(float gain) {
  // ORP121 C-01: Allow gains up to +12 dB (3.981 linear)
  gain = std::clamp(gain, 0.0f, MAX_LINEAR_GAIN);
  m_current = gain;
  m_target = gain;
  m_pending_target.store(gain, std::memory_order_release);
  m_has_pending.store(false, std::memory_order_release);
}

} // namespace orpheus
