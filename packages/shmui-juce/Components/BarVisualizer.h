/*
  ==============================================================================

    BarVisualizer.h
    Created: Shmui-to-JUCE Audio Visualization Port

    Multi-band frequency visualizer with state-based animations.
    Port of bar-visualizer.tsx from shmui.

  ==============================================================================
*/

#pragma once

#include "../Audio/AudioAnalyzer.h"
#include "../Utils/AgentState.h"
#include "../Utils/DesignTokens.h"
#include "../Utils/MessageThread.h"
#include "../Utils/ShmuiTheme.h"
#include <JuceHeader.h>
#include <memory>

namespace shmui {

/**
 * @brief Multi-band frequency visualizer with state animations.
 *
 * Displays audio as vertical bars representing frequency bands.
 * Supports state-based animations for AI/voice assistant interfaces.
 * Port of BarVisualizer component from bar-visualizer.tsx.
 */
class BarVisualizer : public juce::Component, public juce::Timer, public ThemeListener {
public:
  BarVisualizer();
  ~BarVisualizer() override;

  //==============================================================================
  // Audio

  /**
   * @brief Set the audio analyzer for real-time data.
   */
  void setAudioAnalyzer(std::shared_ptr<AudioAnalyzer> analyzer);

  /**
   * @brief Set volume bands directly (for external audio processing).
   *
   * @param bands Vector of band levels (0-1)
   */
  void setVolumeBands(const std::vector<float>& bands);

  //==============================================================================
  // State

  /**
   * @brief Set the agent state for animations.
   */
  void setAgentState(AgentState state);

  /**
   * @brief Get current agent state.
   */
  AgentState getAgentState() const {
    return agentState;
  }

  //==============================================================================
  // Configuration

  /**
   * @brief Set number of bars to display.
   */
  void setBarCount(int count);

  /**
   * @brief Get current bar count.
   */
  int getBarCount() const {
    return barCount;
  }

  /**
   * @brief Set min/max height as percentage (0-100).
   */
  void setHeightRange(float minPct, float maxPct);

  /**
   * @brief Enable demo mode with fake audio data.
   */
  void setDemoMode(bool demo);

  /**
   * @brief Align bars from center instead of bottom.
   */
  void setCenterAlign(bool center);

  //==============================================================================
  // Appearance

  /**
   * @brief Set bar color.
   */
  void setBarColour(const juce::Colour& colour);

  /**
   * @brief Set highlighted bar color.
   */
  void setHighlightColour(const juce::Colour& colour);

  /**
   * @brief Set background color.
   */
  void setBackgroundColour(const juce::Colour& colour);

  /**
   * @brief Enable VU-meter gradient mode (green-yellow-red).
   */
  void setGradientMode(bool gradient);

  /**
   * @brief Check if gradient mode is enabled.
   */
  bool isGradientMode() const {
    return gradientMode;
  }

  //==============================================================================
  // Component overrides

  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  void timerCallback() override;
  void visibilityChanged() override;
  void defaultThemeChanged(const ShmuiTheme& theme) override;
  void updateTimerState();
  bool hasActiveWork() const;
  int getAnimationInterval() const;
  std::vector<int> getHighlightedIndices() const;
  void updateFakeVolumeBands();
  void generateConnectingSequence();
  void generateListeningSequence();

  //==============================================================================

  std::shared_ptr<AudioAnalyzer> audioAnalyzer;
  AgentState agentState = AgentState::Idle;

  int barCount = 15;
  float minHeightPct = 20.0f;
  float maxHeightPct = 100.0f;
  bool demoMode = false;
  bool centerAlign = false;
  bool gradientMode = false;

  // Volume data
  std::vector<float> volumeBands;
  std::vector<float> fakeVolumeBands;

  // Animation
  int animationStep = 0;
  std::vector<std::vector<int>> animationSequence;
  int64_t lastAnimTime = 0;
  float demoTime = 0.0f;

  juce::Colour barColour = tokens::lab::muted();           // idle bars
  juce::Colour highlightColour = tokens::lab::tone();      // active — accent (--lab-tone)
  juce::Colour backgroundColour = tokens::lab::surface0(); // panel floor
  bool customBarColour = false;
  bool customHighlightColour = false;
  bool customBackgroundColour = false;
  // Frequency band configuration (matches AudioAnalyzer defaults and React)
  static constexpr int kLoPass = 100;
  static constexpr int kHiPass = 600;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BarVisualizer)
};

} // namespace shmui
