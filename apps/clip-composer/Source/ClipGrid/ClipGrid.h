// SPDX-License-Identifier: MIT

#pragma once

#include "../Core/GridConstants.h"
#include "../UIState/ClipComposerUiSnapshot.h"
#include "ClipButton.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include <vector>

//==============================================================================
/**
 * ClipGrid - Grid of clip trigger buttons
 *
 * Visible grid presets span 6 x 6 through 10 x 10 while each tab keeps
 * 100 logical slots for stable clip mapping.
 *
 * Layout:
 * - User-selected columns x rows
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
  void setGridSize(int columns, int rows); // Resize visible grid (6 x 6 to 10 x 10)
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

  // Callback to poll the current clip snapshot for a button.
  std::function<bool(int, occ::ui::ClipUiSnapshot&)> getClipSnapshot;

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

  // Display preferences pass-through to all buttons
  void setBevelWidthPercent(float percent);
  void setButtonTextMode(int mode);

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

  // Timer callback for 75fps visual updates. The timing stays continuous; only the
  // source of truth is consolidated into a snapshot per button.
  void timerCallback() override;

  //==============================================================================
  // Grid dimensions (Item 22: now configurable, not constexpr)
  int m_columns = occ::DEFAULT_GRID_COLUMNS;
  int m_rows = occ::DEFAULT_GRID_ROWS;
  static constexpr int GAP = 2;

  // Constraints for grid resizing
  static constexpr int MIN_COLUMNS = occ::MIN_GRID_COLUMNS;
  static constexpr int MAX_COLUMNS = occ::MAX_GRID_COLUMNS;
  static constexpr int MIN_ROWS = occ::MIN_GRID_ROWS;
  static constexpr int MAX_ROWS = occ::MAX_GRID_ROWS;

  std::vector<std::unique_ptr<ClipButton>> m_buttons;

  bool m_hasActiveClips = false; // Track if any clips are playing
  int m_playboxIndex = 0;        // Current playbox position (Item 60: arrow key navigation)

  // Repaint gating: only repaint buttons whose snapshot changed since last frame
  std::array<occ::ui::ClipUiSnapshot, occ::BUTTONS_PER_TAB> m_prevSnapshots{};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipGrid)
};
