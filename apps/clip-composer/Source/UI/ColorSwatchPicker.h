// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <juce_gui_extra/juce_gui_extra.h>
#include <vector>

//==============================================================================
/**
 * ColorSwatchGrid - The popup grid of color swatches (internal component)
 *
 * Displays a 5×14 grid of color swatches in a popup window.
 */
class ColorSwatchGrid : public juce::Component {
public:
  ColorSwatchGrid();
  ~ColorSwatchGrid() override = default;

  void setSelectedColor(const juce::Colour& color);
  juce::Colour getColorAtIndex(int index) const;

  std::function<void(const juce::Colour&)> onColorSelected;

  void paint(juce::Graphics& g) override;
  void mouseDown(const juce::MouseEvent& event) override;

private:
  static constexpr int ROWS = 4;  // 48 swatches in 4 rows
  static constexpr int COLS = 12; // 12 columns

  std::vector<juce::Colour> m_colorPalette;
  int m_selectedIndex = -1;

  void initializeColorPalette();
  int getSwatchIndexAt(int x, int y) const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ColorSwatchGrid)
};

//==============================================================================
/**
 * ColorSwatchPicker - Ableton-style expandable color selector
 *
 * Displays current color as a button. Clicking opens a popup with full swatch grid.
 * Similar to Ableton Live's color picker UI pattern.
 */
class ColorSwatchPicker : public juce::Component {
public:
  //==============================================================================
  ColorSwatchPicker();
  ~ColorSwatchPicker() override;

  //==============================================================================
  // Set currently selected color
  void setSelectedColor(const juce::Colour& color);
  juce::Colour getSelectedColor() const {
    return m_selectedColor;
  }

  //==============================================================================
  // Callback when color is selected
  std::function<void(const juce::Colour&)> onColorSelected;

  //==============================================================================
  void paint(juce::Graphics& g) override;
  void resized() override;
  void mouseDown(const juce::MouseEvent& event) override;

private:
  //==============================================================================
  void showColorPopup();
  void hideColorPopup();

  juce::Colour m_selectedColor;
  bool m_isPopupVisible = false;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ColorSwatchPicker)
};
