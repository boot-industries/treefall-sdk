/*
  ==============================================================================

    ClipButton.h
    Created: shmui Button System

    Clip trigger button with state machine (Empty → Loaded → Playing → Stopping).

    Based on OCC ClipButton implementation, generalized for reuse.

    Usage:
      shmui::ClipButton clipBtn(0);
      clipBtn.setClipName("Kick 01");
      clipBtn.setClipColor(juce::Colours::orange);
      clipBtn.setState(shmui::ClipButton::State::Loaded);
      clipBtn.onClick = [this](int idx) { handleClipTrigger(idx); };

  ==============================================================================
*/

#pragma once

#include "../Icons/Icons.h"
#include "../Utils/DesignTokens.h"
#include "../Utils/Interpolation.h"
#include "Button.h"

namespace shmui {

//==============================================================================
/**
 * @brief Clip trigger button with state machine.
 *
 * A stateful button for audio clip triggering with:
 * - State machine: Empty → Loaded → Playing → Stopping
 * - Waveform thumbnail preview (optional)
 * - Progress indicator during playback
 * - Keyboard shortcut display
 * - Color customization
 * - Right-click context menu support
 *
 * Commonly used in sample pads, clip launchers, and beat grids.
 */
class ClipButton : public Button {
public:
  //==============================================================================
  /**
   * @brief Button states for visual feedback.
   */
  enum class State {
    Empty,   ///< No clip loaded (dark grey, no label)
    Loaded,  ///< Clip loaded, ready to play (colored, shows name)
    Playing, ///< Currently playing (bright border, progress animation)
    Stopping ///< Fade-out in progress (transitioning to Loaded)
  };

  //==============================================================================
  /** Create a clip button with the specified index. */
  explicit ClipButton(int buttonIndex);

  ~ClipButton() override = default;

  //==============================================================================
  /// @name Visual State
  /// @{

  /** Set the visual state of the button. */
  void setClipState(State newState);
  State getClipState() const {
    return m_clipState;
  }

  /// @}

  //==============================================================================
  /// @name Clip Data
  /// @{

  /** Set the display name for this clip. */
  void setClipName(const juce::String& name);
  juce::String getClipName() const {
    return m_clipName;
  }

  /** Set the visual color for this clip. */
  void setClipColor(juce::Colour color);
  juce::Colour getClipColor() const {
    return m_clipColor;
  }

  /** Set the clip duration for display. */
  void setClipDuration(double durationSeconds);
  double getClipDuration() const {
    return m_durationSeconds;
  }

  /** Set keyboard shortcut text to display. */
  void setKeyboardShortcut(const juce::String& shortcut);
  juce::String getKeyboardShortcut() const {
    return m_keyboardShortcut;
  }

  /** Clear all clip data and reset to Empty state. */
  void clearClip();

  /// @}

  //==============================================================================
  /// @name Playback
  /// @{

  /** Set playback progress for visual feedback (0.0 = start, 1.0 = end). */
  void setPlaybackProgress(float progress);
  float getPlaybackProgress() const {
    return m_playbackProgress;
  }

  /// @}

  //==============================================================================
  /// @name Status Flags
  /// @{

  /** Set loop indicator visibility. */
  void setLoopEnabled(bool enabled);
  bool isLoopEnabled() const {
    return m_loopEnabled;
  }

  /** Set fade-in indicator visibility. */
  void setFadeInEnabled(bool enabled);
  bool isFadeInEnabled() const {
    return m_fadeInEnabled;
  }

  /** Set fade-out indicator visibility. */
  void setFadeOutEnabled(bool enabled);
  bool isFadeOutEnabled() const {
    return m_fadeOutEnabled;
  }

  /// @}

  //==============================================================================
  /// @name Identification
  /// @{

  /** Get the button index. */
  int getButtonIndex() const {
    return m_buttonIndex;
  }

  /** Set whether this is the currently selected/playbox button. */
  void setIsPlaybox(bool isPlaybox);
  bool getIsPlaybox() const {
    return m_isPlaybox;
  }

  /// @}

  //==============================================================================
  /// @name Callbacks
  /// @{

  /** Callback invoked on click with button index. */
  std::function<void(int buttonIndex)> onClipClick;

  /** Callback invoked on right-click with button index. */
  std::function<void(int buttonIndex)> onClipRightClick;

  /// @}

protected:
  //==============================================================================
  void paintContent(juce::Graphics& g, juce::Rectangle<float> bounds,
                    juce::Colour foregroundColor) override;

  void animationTick() override;

private:
  //==============================================================================
  void handleClipClick();
  void handleClipRightClick();
  void drawClipHUD(juce::Graphics& g, juce::Rectangle<float> bounds);
  void drawStatusIcons(juce::Graphics& g, juce::Rectangle<float> bounds);
  void drawProgressIndicator(juce::Graphics& g, juce::Rectangle<float> bounds);
  juce::String formatDuration(double seconds) const;

  //==============================================================================
  int m_buttonIndex;
  State m_clipState = State::Empty;
  juce::String m_clipName;
  juce::Colour m_clipColor = tokens::clip::empty(); // --clip-empty (unlit well)
  double m_durationSeconds = 0.0;
  juce::String m_keyboardShortcut;

  // Playback state
  float m_playbackProgress = 0.0f;

  // Status flags
  bool m_loopEnabled = false;
  bool m_fadeInEnabled = false;
  bool m_fadeOutEnabled = false;
  bool m_isPlaybox = false;

  // Animation
  float m_stateTransition = 0.0f;
  float m_playingPulse = 0.0f;

  // Visual constants
  static constexpr int BORDER_THICKNESS = 2;
  static constexpr int CORNER_RADIUS = 4;
  static constexpr int ICON_SIZE = 12;
  static constexpr int PADDING = 4;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipButton)
};

} // namespace shmui
