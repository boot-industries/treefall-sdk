// SPDX-License-Identifier: MIT

#include "HeartbeatIndicator.h"

//==============================================================================
HeartbeatIndicator::HeartbeatIndicator() {
  setOpaque(false);
}

void HeartbeatIndicator::paint(juce::Graphics& g) {
  if (m_isPulsing) {
    g.setColour(m_heartbeatColour.withAlpha(m_pulseAlpha));
    g.fillEllipse(getLocalBounds().toFloat());
  }
}

void HeartbeatIndicator::resized() {}

void HeartbeatIndicator::updateHeartbeat(bool shouldPulse) {
  if (m_isPulsing != shouldPulse) {
    m_isPulsing = shouldPulse;
    m_pulseAlpha = m_isPulsing ? 1.0f : 0.0f; // Instantly on or off
    repaint();
  }
}