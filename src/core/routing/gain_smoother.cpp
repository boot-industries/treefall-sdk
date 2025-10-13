// SPDX-License-Identifier: MIT
#include "gain_smoother.h"

#include <algorithm>
#include <cmath>

namespace orpheus {

GainSmoother::GainSmoother(uint32_t sample_rate, float smoothing_time_ms)
    : m_current(1.0f), m_target(1.0f), m_pending_target(1.0f), m_has_pending(false) {
  // Clamp smoothing time to reasonable range
  smoothing_time_ms = std::clamp(smoothing_time_ms, 1.0f, 100.0f);

  // Calculate increment per sample for full gain change (0.0 → 1.0)
  // Example: 48kHz, 10ms → 480 samples → increment = 1.0 / 480 ≈ 0.00208
  float smoothing_samples = (smoothing_time_ms / 1000.0f) * static_cast<float>(sample_rate);
  m_increment = 1.0f / smoothing_samples;
}

void GainSmoother::setTarget(float target) {
  // Clamp to valid range
  target = std::clamp(target, 0.0f, 1.0f);

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
  gain = std::clamp(gain, 0.0f, 1.0f);
  m_current = gain;
  m_target = gain;
  m_pending_target.store(gain, std::memory_order_release);
  m_has_pending.store(false, std::memory_order_release);
}

} // namespace orpheus
