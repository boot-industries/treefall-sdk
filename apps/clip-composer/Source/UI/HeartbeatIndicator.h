// SPDX-License-Identifier: MIT

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
 * @brief A visual indicator that pulses in sync with a selected clock (system or timecode).
 *
 * This component visually confirms that the application is healthy and responding
 * to the chosen timing source.
 */
class HeartbeatIndicator : public juce::Component {
public:
  HeartbeatIndicator();
  ~HeartbeatIndicator() override = default;

  void paint(juce::Graphics& g) override;
  void resized() override;

  /**
   * @brief Updates the heartbeat state, typically called by a timer.
   * @param shouldPulse True if the indicator should be in its "on" state for this update.
   */
  void updateHeartbeat(bool shouldPulse);

private:
  bool m_isPulsing = false;  // Current state of the indicator (on/off)
  float m_pulseAlpha = 0.0f; // For smooth alpha animation
  juce::Colour m_heartbeatColour = juce::Colours::cyan;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeartbeatIndicator)
};
