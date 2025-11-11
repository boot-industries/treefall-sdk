// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <juce_gui_extra/juce_gui_extra.h>

//==============================================================================
/**
 * TransportControls - Master transport controls
 *
 * Provides global playback controls:
 * - Stop All: Stop all playing clips (fade-out)
 * - Panic: Immediately mute all audio (emergency stop)
 *
 * Future features:
 * - Play All (selected group)
 * - Master volume
 * - Transport position display
 */
class TransportControls : public juce::Component {
public:
  //==============================================================================
  TransportControls();
  ~TransportControls() override = default;

  //==============================================================================
  // Callbacks for button events
  std::function<void()> onStopAll;
  std::function<void()> onPanic;

  //==============================================================================
  // Update latency display (call periodically from MainComponent)
  void setLatencyInfo(double latencyMs, int bufferSize, int sampleRate);

  // Update CPU/Memory display (call periodically from MainComponent at 1Hz)
  // OCC109 v0.2.2: Real-time performance monitoring
  void setPerformanceInfo(float cpuPercent, int memoryMB);

  //==============================================================================
  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  //==============================================================================
  std::unique_ptr<juce::TextButton> m_stopAllButton;
  std::unique_ptr<juce::TextButton> m_panicButton;
  std::unique_ptr<juce::Label> m_latencyLabel;
  std::unique_ptr<juce::Label> m_cpuLabel;    // OCC109 v0.2.2: CPU usage display
  std::unique_ptr<juce::Label> m_memoryLabel; // OCC109 v0.2.2: Memory usage display

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportControls)
};
