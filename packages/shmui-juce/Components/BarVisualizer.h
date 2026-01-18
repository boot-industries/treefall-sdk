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
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace shmui {

/**
 * @brief Agent state for bar visualizer animations.
 */
enum class AgentState {
  Idle,         ///< Inactive state
  Connecting,   ///< Initial connection animation
  Initializing, ///< Setup animation
  Listening,    ///< Listening for input
  Thinking,     ///< Processing/thinking
  Speaking      ///< Output/speaking
};

/**
 * @brief Multi-band frequency visualizer with state animations.
 *
 * Displays audio as vertical bars representing frequency bands.
 * Supports state-based animations for AI/voice assistant interfaces.
 * Port of BarVisualizer component from bar-visualizer.tsx.
 */
class BarVisualizer : public juce::Component, public juce::Timer {
public:
  BarVisualizer();
  ~BarVisualizer() override;

  //==============================================================================
  // Audio

  /**
   * @brief Set the audio analyzer for real-time data.
   */
  void setAudioAnalyzer(AudioAnalyzer* analyzer);

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
   * @brief Enable VU meter gradient mode (green-yellow-red).
   * When enabled, bars are drawn with a vertical gradient based on level.
   */
  void setGradientMode(bool enabled);

  //==============================================================================
  // Component overrides

  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  void timerCallback() override;
  int getAnimationInterval() const;
  std::vector<int> getHighlightedIndices() const;
  void updateFakeVolumeBands();
  void generateConnectingSequence();
  void generateListeningSequence();

  //==============================================================================

  AudioAnalyzer* audioAnalyzer = nullptr;
  AgentState agentState = AgentState::Idle;

  int barCount = 15;
  float minHeightPct = 20.0f;
  float maxHeightPct = 100.0f;
  bool demoMode = false;
  bool centerAlign = false;
  bool gradientMode = false; // VU meter gradient mode (green-yellow-red)

  // Volume data
  std::vector<float> volumeBands;
  std::vector<float> fakeVolumeBands;

  // Animation
  int animationStep = 0;
  std::vector<std::vector<int>> animationSequence;
  int64_t lastAnimTime = 0;
  float demoTime = 0.0f;

  // Colours
  juce::Colour barColour = juce::Colour(0xFFE5E5E5);        // border color
  juce::Colour highlightColour = juce::Colour(0xFF3B82F6);  // primary
  juce::Colour backgroundColour = juce::Colour(0xFFF5F5F5); // muted

  // Frequency band configuration
  static constexpr int kLoPass = 100;
  static constexpr int kHiPass = 200;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BarVisualizer)
};

} // namespace shmui
