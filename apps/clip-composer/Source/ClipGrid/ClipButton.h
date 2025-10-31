// SPDX-License-Identifier: MIT

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

// Forward declaration
class ClipGrid;

//==============================================================================
/**
 * ClipButton - Individual clip trigger button
 *
 * Represents a single clip in the grid (one of 48 for MVP, 960 for full app).
 *
 * Visual States:
 * - Empty: Dark grey, no label
 * - Loaded: Colored based on clip type, shows clip name
 * - Playing: Bright border, animated
 * - Stopping: Fade-out animation
 *
 * Interaction:
 * - Click: Trigger clip (start if stopped, stop if playing)
 * - Right-click: Show context menu (load clip, edit, remove)
 * - Drag-drop: Load audio file onto button
 */
class ClipButton : public juce::Component {
public:
  //==============================================================================
  /**
   * Button states for visual feedback
   */
  enum class State {
    Empty,   // No clip loaded
    Loaded,  // Clip loaded, ready to play
    Playing, // Currently playing
    Stopping // Fade-out in progress
  };

  //==============================================================================
  ClipButton(int buttonIndex);
  ~ClipButton() override = default;

  //==============================================================================
  // Visual state
  void setState(State newState);
  State getState() const {
    return m_state;
  }

  // Clip data
  void setClipName(const juce::String& name);
  void setClipColor(juce::Colour color);
  void setClipDuration(double durationSeconds);
  void setClipGroup(int group); // 0-3 for routing
  void setKeyboardShortcut(const juce::String& shortcut);
  void setBeatOffset(const juce::String& beatOffset); // Optional: "1", "1+", "1++", "3+", etc.
  void clearClip();

  // Playback progress (0.0 = start, 1.0 = end)
  void setPlaybackProgress(float progress);
  float getPlaybackProgress() const {
    return m_playbackProgress;
  }

  // Status flags for icons
  void setLoopEnabled(bool enabled);
  void setFadeInEnabled(bool enabled);
  void setFadeOutEnabled(bool enabled);
  void setEffectsEnabled(bool enabled);
  void setStopOthersEnabled(bool enabled);

  int getButtonIndex() const {
    return m_buttonIndex;
  }

  // Tab management (for consecutive numbering across tabs - Feature 4)
  void setTabIndex(int tabIndex) {
    m_tabIndex = tabIndex;
    repaint();
  }
  int getTabIndex() const {
    return m_tabIndex;
  }

  // Get display number (consecutive across all tabs)
  // Tab 1 = 1-48, Tab 2 = 49-96, Tab 3 = 97-144, etc.
  int getDisplayNumber() const {
    return (m_tabIndex * 48) + m_buttonIndex + 1;
  }

  //==============================================================================
  // Callbacks
  std::function<void(int buttonIndex)> onClick;
  std::function<void(int buttonIndex)> onRightClick;
  std::function<void(int buttonIndex)> onEditDialogRequested; // Ctrl+Opt+Cmd+Click shortcut
  std::function<void(int sourceButtonIndex, int targetButtonIndex)> onDragToButton;

  //==============================================================================
  void paint(juce::Graphics& g) override;
  void resized() override;
  void mouseDown(const juce::MouseEvent& e) override;
  void mouseDrag(const juce::MouseEvent& e) override;
  void mouseUp(const juce::MouseEvent& e) override;

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

  // Playback state
  float m_playbackProgress = 0.0f; // 0.0 to 1.0

  // Status flags
  bool m_loopEnabled = false;
  bool m_fadeInEnabled = false;
  bool m_fadeOutEnabled = false;
  bool m_effectsEnabled = false;
  bool m_stopOthersEnabled = false;

  // Drag state (Cmd+Drag to rearrange clips)
  juce::Point<int> m_mouseDownPosition;
  bool m_isDragging = false;

  // Visual constants
  static constexpr int BORDER_THICKNESS = 2;
  static constexpr int CORNER_RADIUS = 4;
  static constexpr int ICON_SIZE = 16;
  static constexpr int PADDING = 4;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipButton)
};
