// SPDX-License-Identifier: MIT

#pragma once

#include "../UIState/ClipComposerUiSnapshot.h"
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
  std::function<void()> onCue;

  //==============================================================================
  // Update latency display (call periodically from MainComponent)
  void setLatencyInfo(double latencyMs, int bufferSize, int sampleRate);

  // Update CPU/Memory display (call periodically from MainComponent at 1Hz)
  // OCC109 v0.2.2: Real-time performance monitoring
  void setPerformanceInfo(float cpuPercent, int memoryMB);
  void setTransportSnapshot(const occ::ui::ClipComposerUiSnapshot& snapshot);

  //==============================================================================
  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  // Single source of truth for the transport strip geometry. Both paint() and
  // resized() call this so a) they can never disagree and b) we can collapse
  // surfaces gracefully under narrow widths without visual collisions.
  struct Layout {
    juce::Rectangle<int> stopAll;
    juce::Rectangle<int> panic;
    juce::Rectangle<int> latencyLabel;
    juce::Rectangle<int> cpuLabel;
    juce::Rectangle<int> memoryLabel;
    juce::Rectangle<int> statusZone;    // "PLAYING ·" + clip names. Empty if collapsed.
    juce::Rectangle<int> masterCluster; // MASTER label + meter + dB readout. Empty if collapsed.
    juce::Rectangle<int> cueButton;
    bool diagnosticsVisible = true;
    bool statusVisible = true;
    bool masterVisible = true;
  };
  Layout computeLayout() const;

  //==============================================================================
  std::unique_ptr<juce::TextButton> m_stopAllButton;
  std::unique_ptr<juce::TextButton> m_panicButton;
  std::unique_ptr<juce::TextButton> m_cueButton;
  std::unique_ptr<juce::Label> m_latencyLabel;
  std::unique_ptr<juce::Label> m_cpuLabel;    // OCC109 v0.2.2: CPU usage display
  std::unique_ptr<juce::Label> m_memoryLabel; // OCC109 v0.2.2: Memory usage display
  occ::ui::ClipComposerUiSnapshot m_snapshot;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportControls)
};
