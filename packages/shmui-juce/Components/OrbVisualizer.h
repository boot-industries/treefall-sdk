/*
  ==============================================================================

    OrbVisualizer.h
    Created: Shmui-to-JUCE Audio Visualization Port

    3D orb visualization using OpenGL shaders.
    Port of orb.tsx from shmui.

  ==============================================================================
*/

#pragma once

#include "../Utils/AgentState.h"
#include "../Utils/Interpolation.h"
#include <JuceHeader.h>

namespace shmui {

/**
 * @brief Volume control mode for orb.
 */
enum class OrbVolumeMode {
  Auto,  ///< Use internal oscillation based on state
  Manual ///< Use manual input/output values
};

/**
 * @brief 3D orb visualization with OpenGL shaders.
 *
 * Displays an animated orb that responds to agent state and volume levels.
 * Uses GLSL shaders for rendering with noise-based distortion effects.
 * Port of Orb component from orb.tsx.
 */
class OrbVisualizer : public juce::Component, public juce::OpenGLRenderer, public juce::Timer {
public:
  OrbVisualizer();
  ~OrbVisualizer() override;

  //==============================================================================
  // State

  /**
   * @brief Set the agent state for animation.
   */
  void setAgentState(AgentState state);

  /**
   * @brief Get current agent state.
   */
  AgentState getAgentState() const {
    return agentState;
  }

  //==============================================================================
  // Volume Control

  /**
   * @brief Set volume control mode.
   */
  void setVolumeMode(OrbVolumeMode mode);

  /**
   * @brief Set manual input volume (0-1).
   *
   * Only used when volumeMode is Manual.
   */
  void setInputVolume(float volume);

  /**
   * @brief Set manual output volume (0-1).
   *
   * Only used when volumeMode is Manual.
   */
  void setOutputVolume(float volume);

  /**
   * @brief Get current smoothed input volume.
   */
  float getInputVolume() const {
    return smoothedInput;
  }

  /**
   * @brief Get current smoothed output volume.
   */
  float getOutputVolume() const {
    return smoothedOutput;
  }

  //==============================================================================
  // Appearance

  /**
   * @brief Set the two gradient colors.
   *
   * @param color1 Primary gradient color
   * @param color2 Secondary gradient color
   */
  void setColors(const juce::Colour& color1, const juce::Colour& color2);

  /**
   * @brief Set random seed for animation offsets.
   */
  void setSeed(uint32_t seed);

  /**
   * @brief Set inverted mode (for dark theme).
   */
  void setInverted(bool inverted);

  //==============================================================================
  // OpenGLRenderer overrides

  void newOpenGLContextCreated() override;
  void renderOpenGL() override;
  void openGLContextClosing() override;

  //==============================================================================
  // Component overrides

  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  void timerCallback() override;
  void updateAnimationTargets();
  void createShaders();
  void createNoiseTexture();

  //==============================================================================

  juce::OpenGLContext openGLContext;
  std::unique_ptr<juce::OpenGLShaderProgram> shader;
  std::unique_ptr<juce::OpenGLShaderProgram::Attribute> position;
  std::unique_ptr<juce::OpenGLShaderProgram::Attribute> texCoord;
  GLuint noiseTexture = 0;
  GLuint vertexBuffer = 0;
  GLuint texCoordBuffer = 0;

  // State
  AgentState agentState = AgentState::Idle;
  OrbVolumeMode volumeMode = OrbVolumeMode::Auto;

  // Volume
  float manualInput = 0.0f;
  float manualOutput = 0.0f;
  float targetInput = 0.0f;
  float targetOutput = 0.3f;
  float smoothedInput = 0.0f;
  float smoothedOutput = 0.0f;

  // Animation
  float time = 0.0f;
  float animationTime = 0.0f;
  float animationSpeed = 0.1f;
  float opacity = 0.0f;

  // Colors — Orb shader gradient stops. Intentionally literal hex (a GPU
  // gradient ramp, not a themeable UI surface); see ColorUtils.h kOrbColor*
  // and orpheus-catalog.json. Do not migrate to tokens::*.
  juce::Colour color1{0xFFCADCFC};
  juce::Colour color2{0xFFA0B9D1};
  juce::Colour targetColor1{0xFFCADCFC};
  juce::Colour targetColor2{0xFFA0B9D1};
  juce::Colour currentColor1{0xFFCADCFC};
  juce::Colour currentColor2{0xFFA0B9D1};

  // Offsets for oval animation
  std::array<float, 7> offsets{};
  uint32_t seed = 0;
  bool inverted = false;

  // Constants
  static constexpr float kSmoothingFactor = 0.2f;
  static constexpr float kColorLerpFactor = 0.08f;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbVisualizer)
};

} // namespace shmui
