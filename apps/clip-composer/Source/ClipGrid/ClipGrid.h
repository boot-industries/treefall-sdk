// SPDX-License-Identifier: MIT

#pragma once

#include "ClipButton.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include <orpheus/transport_controller.h> // For PlaybackState enum
#include <vector>

//==============================================================================
/**
 * ClipGrid - Grid of clip trigger buttons
 *
 * MVP: 6×8 = 48 buttons (preview of full 960-button system)
 * Full: 10×12 × 8 tabs = 960 buttons
 *
 * Layout:
 * - 6 columns × 8 rows
 * - Responsive sizing
 * - 2px gaps between buttons
 * - Visual updates at 75fps (broadcast standard timing)
 */
class ClipGrid : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer {
public:
  //==============================================================================
  ClipGrid();
  ~ClipGrid() override = default;

  //==============================================================================
  // Grid configuration (Item 22: Resizable grid)
  void setGridSize(int columns, int rows); // Resize grid (5×4 to 12×8)
  int getColumns() const {
    return m_columns;
  }
  int getRows() const {
    return m_rows;
  }

  //==============================================================================
  // Button access
  ClipButton* getButton(int index);
  int getButtonCount() const {
    return static_cast<int>(m_buttons.size());
  }

  //==============================================================================
  // Callbacks for button events
  std::function<void(int buttonIndex)> onButtonClicked;             // Left-click (trigger)
  std::function<void(int buttonIndex)> onButtonRightClicked;        // Right-click (load)
  std::function<void(int buttonIndex)> onButtonDoubleClicked;       // Double-click (edit)
  std::function<void(int buttonIndex)> onButtonEditDialogRequested; // Ctrl+Opt+Cmd+Click (edit)
  std::function<void(const juce::Array<juce::File>& files, int buttonIndex)>
      onFilesDropped; // Drag & drop files
  std::function<void(int sourceButtonIndex, int targetButtonIndex)>
      onButtonDraggedToButton; // Drag clip to different button

  // Callback to get clip playback state (for 75fps visual sync)
  std::function<orpheus::PlaybackState(int)> getClipState;

  // Callback to check if clip exists (for 75fps state validation)
  std::function<bool(int)> hasClip;

  // Callback to get clip metadata (for 75fps state persistence)
  std::function<void(int, bool&, bool&, bool&, bool&)>
      getClipStates; // buttonIndex → (loop, fadeIn, fadeOut, stopOthers)

  // Callback to get clip playback position (for elapsed time display)
  std::function<float(int)> getClipPosition; // buttonIndex → progress (0.0-1.0)

  //==============================================================================
  // Timer management for performance optimization
  void setHasActiveClips(bool hasActive);

  //==============================================================================
  // Playbox navigation (Item 60: Arrow key navigation with thin outline)
  int getPlayboxIndex() const {
    return m_playboxIndex;
  }
  void setPlayboxIndex(int index);
  void movePlayboxUp();
  void movePlayboxDown();
  void movePlayboxLeft();
  void movePlayboxRight();
  void triggerPlayboxButton(); // Trigger button at playbox position (Enter key)

  //==============================================================================
  void paint(juce::Graphics& g) override;
  void resized() override;

  // FileDragAndDropTarget overrides
  bool isInterestedInFileDrag(const juce::StringArray& files) override;
  void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
  //==============================================================================
  void createButtons();
  void handleButtonLeftClick(int buttonIndex);
  void handleButtonRightClick(int buttonIndex);

  // Timer callback for 75fps visual updates (broadcast standard)
  void timerCallback() override;

  //==============================================================================
  // Grid dimensions (Item 22: now configurable, not constexpr)
  int m_columns = 6; // Default 6, configurable 5-12
  int m_rows = 8;    // Default 8, configurable 4-8
  static constexpr int GAP = 2;

  // Constraints for grid resizing
  static constexpr int MIN_COLUMNS = 5;
  static constexpr int MAX_COLUMNS = 12;
  static constexpr int MIN_ROWS = 4;
  static constexpr int MAX_ROWS = 8;

  std::vector<std::unique_ptr<ClipButton>> m_buttons;

  bool m_hasActiveClips = false; // Track if any clips are playing
  int m_playboxIndex = 0;        // Current playbox position (Item 60: arrow key navigation)

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipGrid)
};
