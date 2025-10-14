// SPDX-License-Identifier: MIT

#pragma once

#include "ClipButton.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
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
 */
class ClipGrid : public juce::Component, public juce::FileDragAndDropTarget {
public:
  //==============================================================================
  ClipGrid();
  ~ClipGrid() override = default;

  //==============================================================================
  // Button access
  ClipButton* getButton(int index);
  int getButtonCount() const {
    return static_cast<int>(m_buttons.size());
  }

  //==============================================================================
  // Callbacks for button events
  std::function<void(int buttonIndex)> onButtonClicked;      // Left-click (trigger)
  std::function<void(int buttonIndex)> onButtonRightClicked; // Right-click (load)
  std::function<void(const juce::Array<juce::File>& files, int buttonIndex)>
      onFilesDropped; // Drag & drop

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

  //==============================================================================
  static constexpr int COLUMNS = 6;
  static constexpr int ROWS = 8;
  static constexpr int BUTTON_COUNT = COLUMNS * ROWS; // 48
  static constexpr int GAP = 2;

  std::vector<std::unique_ptr<ClipButton>> m_buttons;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipGrid)
};
