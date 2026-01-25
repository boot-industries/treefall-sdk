/*
  ==============================================================================

    MatrixDisplay.h
    Created: Shmui-to-JUCE Audio Visualization Port

    LED-style matrix display with frame-based animations.
    Port of matrix.tsx from shmui.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>

namespace shmui {

/**
 * @brief Frame data type for matrix display.
 *
 * 2D array of brightness values (0-1).
 */
using Frame = std::vector<std::vector<float>>;

/**
 * @brief Create an empty frame with all values set to 0.
 */
Frame createEmptyFrame(int rows, int cols);

/**
 * @brief Create a VU meter frame from level data.
 *
 * @param columns Number of columns
 * @param levels Vector of level values (0-1)
 * @return Frame with VU meter visualization
 */
Frame createVUMeterFrame(int columns, const std::vector<float>& levels);

//==============================================================================

/**
 * @brief LED-style matrix display component.
 *
 * Displays a grid of virtual LEDs with variable brightness.
 * Supports frame-based animations and real-time VU meter mode.
 * Port of Matrix component from matrix.tsx.
 */
class MatrixDisplay : public juce::Component, public juce::Timer {
public:
  MatrixDisplay();
  ~MatrixDisplay() override;

  //==============================================================================
  // Configuration

  /**
   * @brief Set matrix dimensions.
   *
   * @param rows Number of rows
   * @param cols Number of columns
   */
  void setSize(int rows, int cols);

  /**
   * @brief Get row count.
   */
  int getRowCount() const {
    return rows;
  }

  /**
   * @brief Get column count.
   */
  int getColumnCount() const {
    return cols;
  }

  //==============================================================================
  // Display Mode

  /**
   * @brief Set a static pattern to display.
   */
  void setPattern(const Frame& pattern);

  /**
   * @brief Set animation frames.
   *
   * @param frames Vector of frames for animation
   * @param fps Frames per second
   * @param loop Loop the animation
   */
  void setFrames(const std::vector<Frame>& frames, float fps = 12.0f, bool loop = true);

  /**
   * @brief Set VU meter levels for real-time display.
   *
   * @param levels Vector of level values (0-1)
   */
  void setLevels(const std::vector<float>& levels);

  /**
   * @brief Clear the display.
   */
  void clear();

  //==============================================================================
  // Animation Control

  /**
   * @brief Start animation playback.
   */
  void play();

  /**
   * @brief Stop animation playback.
   */
  void stop();

  /**
   * @brief Check if animation is playing.
   */
  bool isPlaying() const {
    return animationPlaying;
  }

  /**
   * @brief Set frame rate for animation.
   */
  void setFPS(float fps);

  /**
   * @brief Set loop mode.
   */
  void setLoop(bool shouldLoop);

  //==============================================================================
  // Appearance

  /**
   * @brief Set LED size in pixels.
   */
  void setLEDSize(float size);

  /**
   * @brief Set gap between LEDs in pixels.
   */
  void setLEDGap(float gap);

  /**
   * @brief Set active LED color.
   */
  void setOnColour(const juce::Colour& colour);

  /**
   * @brief Set inactive LED color.
   */
  void setOffColour(const juce::Colour& colour);

  /**
   * @brief Set global brightness multiplier.
   */
  void setBrightness(float brightness);

  //==============================================================================
  // Callbacks

  /**
   * @brief Called when a frame changes during animation.
   */
  std::function<void(int frameIndex)> onFrame;

  //==============================================================================
  // Component overrides

  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  void timerCallback() override;
  Frame ensureFrameSize(const Frame& frame) const;

  //==============================================================================

  int rows = 7;
  int cols = 7;

  // Display modes
  Frame currentFrame;
  std::vector<Frame> animationFrames;
  std::vector<float> vuLevels;

  // Animation state
  int frameIndex = 0;
  float accumulator = 0.0f;
  float fps = 12.0f;
  bool loop = true;
  bool animationPlaying = false;
  int64_t lastTime = 0;

  // Appearance
  float ledSize = 10.0f;
  float ledGap = 2.0f;
  juce::Colour onColour = juce::Colours::white;      // currentColor default
  juce::Colour offColour = juce::Colour(0x80808080); // muted-foreground
  float brightness = 1.0f;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MatrixDisplay)
};

//==============================================================================
// Pre-built animations from matrix.tsx

namespace MatrixAnimations {

/**
 * @brief Spinning loader animation.
 */
std::vector<Frame> createLoader();

/**
 * @brief Pulsing circle animation.
 */
std::vector<Frame> createPulse();

/**
 * @brief Wave animation.
 */
std::vector<Frame> createWave();

/**
 * @brief Snake animation.
 */
std::vector<Frame> createSnake();

/**
 * @brief Digit frames (0-9) for number display.
 */
const std::vector<Frame>& getDigits();

/**
 * @brief Chevron left symbol.
 */
const Frame& getChevronLeft();

/**
 * @brief Chevron right symbol.
 */
const Frame& getChevronRight();

} // namespace MatrixAnimations

} // namespace shmui
