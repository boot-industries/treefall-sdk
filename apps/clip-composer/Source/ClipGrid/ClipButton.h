// SPDX-License-Identifier: MIT

#pragma once

#include "../../../packages/shmui-juce/Utils/Interpolation.h"
#include "../Core/GridConstants.h"
#include <juce_gui_extra/juce_gui_extra.h>

// Forward declaration
class ClipGrid;

//==============================================================================
/**
 * @brief Individual clip trigger button in the ClipGrid.
 *
 * Represents a single clip in the grid (one of up to 960 for full app capacity).
 *
 * @section states Visual States
 * - Empty: Dark grey, no label
 * - Loaded: Colored based on clip type, shows clip name
 * - Playing: Bright border, animated progress
 * - Stopping: Fade-out animation in progress
 *
 * @section interaction Interaction
 * - Click: Trigger clip (start if stopped, stop if playing)
 * - Right-click: Show context menu (load clip, edit, remove)
 * - Ctrl+Opt+Cmd+Click: Open Edit Dialog
 * - Cmd+Drag: Rearrange clips between buttons
 *
 * @section numbering Button Numbering
 * Buttons are numbered consecutively across tabs (Feature 4):
 * Tab 1 = 1-100, Tab 2 = 101-200, etc.
 */
class ClipButton : public juce::Component, private juce::Timer {
public:
  //==============================================================================
  /**
   * @brief Button states for visual feedback.
   *
   * Determines the visual appearance and behavior of the button.
   */
  enum class State {
    Empty,   ///< No clip loaded (dark grey, no label)
    Loaded,  ///< Clip loaded, ready to play (colored, shows name)
    Playing, ///< Currently playing (bright border, progress animation)
    Stopping ///< Fade-out in progress (transitioning to Loaded)
  };

  //==============================================================================
  /**
   * @brief Construct a ClipButton.
   * @param buttonIndex Button index within the grid (0 to MAX-1)
   */
  ClipButton(int buttonIndex);
  ~ClipButton() override = default;

  //==============================================================================
  /// @name Visual State
  /// @{

  /**
   * @brief Set the visual state of the button.
   * @param newState New state (Empty, Loaded, Playing, Stopping)
   */
  void setState(State newState);

  /**
   * @brief Get current visual state.
   * @return Current state
   */
  State getState() const {
    return m_state;
  }

  /// @}

  //==============================================================================
  /// @name Clip Data
  /// @{

  /**
   * @brief Set the display name for this clip.
   * @param name Clip name to display on button
   */
  void setClipName(const juce::String& name);

  /**
   * @brief Set the visual color for this clip.
   * @param color JUCE color to use for button fill
   */
  void setClipColor(juce::Colour color);

  /**
   * @brief Set the clip duration for HUD display.
   * @param durationSeconds Duration in seconds
   */
  void setClipDuration(double durationSeconds);

  /**
   * @brief Set the clip group assignment.
   * @param group Clip group index (0-3) for routing
   */
  void setClipGroup(int group);

  /**
   * @brief Set keyboard shortcut text to display.
   * @param shortcut Shortcut label (e.g., "Q", "F1")
   */
  void setKeyboardShortcut(const juce::String& shortcut);

  /**
   * @brief Set beat offset text to display.
   * @param beatOffset Beat offset label (e.g., "1", "1+", "1++", "3+")
   */
  void setBeatOffset(const juce::String& beatOffset);

  /**
   * @brief Set bevel width as a percentage of button short dimension.
   * @param percent 0.0 = no bevel, 0.05 = 5%, up to 0.20 = 20%
   */
  void setBevelWidthPercent(float percent);

  /**
   * @brief Set button text display mode.
   * @param mode 0=None, 1=HotKey, 2=MidiNote
   */
  void setButtonTextMode(int mode);

  /**
   * @brief Clear all clip data and reset to Empty state.
   */
  void clearClip();

  /// @}

  //==============================================================================
  /// @name Playback
  /// @{

  /**
   * @brief Set playback progress for visual feedback.
   * @param progress Progress value (0.0 = start, 1.0 = end)
   */
  void setPlaybackProgress(float progress);

  /**
   * @brief Get current playback progress.
   * @return Progress value (0.0 to 1.0)
   */
  float getPlaybackProgress() const {
    return m_playbackProgress;
  }

  /// @}

  //==============================================================================
  /// @name Status Flags
  /// @{

  /** @brief Set loop indicator visibility. */
  void setLoopEnabled(bool enabled);

  /** @brief Set fade-in indicator visibility. */
  void setFadeInEnabled(bool enabled);

  /** @brief Set fade-out indicator visibility. */
  void setFadeOutEnabled(bool enabled);

  /** @brief Set effects (FX chain) indicator visibility. Drawn as the design-kit
   *         "g-fx" sine-wave squiggle in the bottom-row indicator strip. */
  void setEffectsEnabled(bool enabled);

  /** @brief Set stop-others indicator visibility. Drawn as the design-kit
   *         "g-solo" shield-with-dot glyph. */
  void setStopOthersEnabled(bool enabled);

  /** @brief Set trim indicator visibility — clip has non-default IN/OUT trim
   *         points set. Drawn as the design-kit "g-trim" range-bracket glyph. */
  void setTrimEnabled(bool enabled);

  /** @brief Set lock indicator visibility — clip is protected from accidental
   *         modification. Important for live operators preventing on-air
   *         changes. Drawn as the design-kit "g-lock" padlock glyph. */
  void setLockEnabled(bool enabled);

  /** @brief Set the width (in digits) used when padding the ordinal number.
   *         Set by ClipGrid to the digit count of the largest visible ordinal,
   *         so the ordinal column reads uniformly across the grid:
   *         48-cell grid → 2 digits, 100-cell grid → 3 digits, etc.
   *         Default 2. */
  void setDisplayDigitWidth(int digits) {
    int clamped = juce::jlimit(1, 4, digits);
    if (m_displayDigitWidth != clamped) {
      m_displayDigitWidth = clamped;
      repaint();
    }
  }

  /// @}

  //==============================================================================
  /// @name Identification
  /// @{

  /**
   * @brief Get the button index within current tab.
   * @return Button index (0 to BUTTONS_PER_TAB-1)
   */
  int getButtonIndex() const {
    return m_buttonIndex;
  }

  /**
   * @brief Set the tab index for consecutive numbering.
   * @param tabIndex Tab index (0-7)
   */
  void setTabIndex(int tabIndex) {
    m_tabIndex = tabIndex;
    repaint();
  }

  /**
   * @brief Get the tab index.
   * @return Tab index (0-7)
   */
  int getTabIndex() const {
    return m_tabIndex;
  }

  /**
   * @brief Get display number (consecutive across all tabs).
   * @return Display number: Tab 1 = 1-100, Tab 2 = 101-200, etc.
   */
  int getDisplayNumber() const {
    return (m_tabIndex * occ::BUTTONS_PER_TAB) + m_buttonIndex + 1;
  }

  /**
   * @brief Set playbox indicator state (arrow key navigation).
   * @param isPlaybox true to show thin white outline
   */
  void setIsPlaybox(bool isPlaybox) {
    if (m_isPlaybox != isPlaybox) {
      m_isPlaybox = isPlaybox;
      repaint();
    }
  }

  /**
   * @brief Check if this button is the current playbox.
   * @return true if playbox indicator is active
   */
  bool getIsPlaybox() const {
    return m_isPlaybox;
  }

  /// @}

  //==============================================================================
  /// @name Callbacks
  /// Event callbacks for UI integration.
  /// @{

  /** @brief Callback invoked on button click (trigger clip). */
  std::function<void(int buttonIndex)> onClick;

  /** @brief Callback invoked on right-click (context menu). */
  std::function<void(int buttonIndex)> onRightClick;

  /** @brief Callback invoked on Ctrl+Opt+Cmd+Click (edit dialog). */
  std::function<void(int buttonIndex)> onEditDialogRequested;

  /** @brief Callback invoked when dragging clip to another button. */
  std::function<void(int sourceButtonIndex, int targetButtonIndex)> onDragToButton;

  /// @}

  //==============================================================================
  void paint(juce::Graphics& g) override;
  void resized() override;
  void mouseDown(const juce::MouseEvent& e) override;
  void mouseDrag(const juce::MouseEvent& e) override;
  void mouseUp(const juce::MouseEvent& e) override;
  void mouseEnter(const juce::MouseEvent& e) override;
  void mouseExit(const juce::MouseEvent& e) override;
  void timerCallback() override;

private:
  //==============================================================================
  // Helper methods for HUD rendering
  juce::String formatDuration(double seconds) const;
  void drawClipHUD(juce::Graphics& g, juce::Rectangle<float> bounds);
  void drawStatusIcons(juce::Graphics& g, juce::Rectangle<float> bounds);

  //==============================================================================
  int m_buttonIndex;
  int m_tabIndex = 0; // Current tab index (for consecutive numbering - Feature 4)
  State m_state = State::Empty;
  juce::String m_clipName;
  juce::Colour m_clipColor = juce::Colours::darkgrey;
  double m_durationSeconds = 0.0;
  int m_clipGroup = 0; // 0-3 for routing groups
  juce::String m_keyboardShortcut;
  juce::String m_beatOffset; // Optional: "3+", "2", "4-", etc.

  float m_bevelWidthPercent = 0.1f; // Default 10% bevel
  int m_buttonTextMode = 1;         // 0=None, 1=HotKey, 2=MidiNote

  // Playback state
  float m_playbackProgress = 0.0f; // 0.0 to 1.0

  // Status flags
  bool m_loopEnabled = false;
  bool m_fadeInEnabled = false;
  bool m_fadeOutEnabled = false;
  bool m_effectsEnabled = false; // a.k.a. FX chain
  bool m_stopOthersEnabled = false;
  bool m_trimEnabled = false;
  bool m_lockEnabled = false;

  // Digit width for the ordinal — driven by ClipGrid so all cells in the same
  // grid pad uniformly. Defaults to 2 (covers the smallest supported grid).
  int m_displayDigitWidth = 2;
  bool m_isPlaybox = false; // Item 60: Arrow key navigation outline

  // Drag state (Cmd+Drag to rearrange clips)
  juce::Point<int> m_mouseDownPosition;
  bool m_isDragging = false;

  // Animation state (shmui Interpolation-based)
  bool m_isHovered = false;
  float m_hoverOpacity = 0.0f;    // 0.0 = not hovered, 1.0 = fully hovered
  float m_pressOpacity = 0.0f;    // 0.0 = not pressed, 1.0 = fully pressed
  float m_stateTransition = 0.0f; // Smooth state change animation
  double m_lastAnimTime = 0.0;    // For frame-rate independent animation

  // Visual constants
  static constexpr int BORDER_THICKNESS = 2;
  static constexpr int CORNER_RADIUS = 4;
  static constexpr int ICON_SIZE = 16;
  static constexpr int PADDING = 4;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipButton)
};
