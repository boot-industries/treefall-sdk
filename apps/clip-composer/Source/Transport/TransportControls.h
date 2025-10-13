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
  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  //==============================================================================
  std::unique_ptr<juce::TextButton> m_stopAllButton;
  std::unique_ptr<juce::TextButton> m_panicButton;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportControls)
};
