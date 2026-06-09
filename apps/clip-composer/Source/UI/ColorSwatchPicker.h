// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <juce_gui_extra/juce_gui_extra.h>
#include <vector>

//==============================================================================
/**
 * ColorSwatchPicker - Inline color chip row (Ableton-style / design-kit aligned)
 *
 * Displays a horizontal row of color chips for quick color selection.
 * Replaces the old popup-grid dropdown with an always-visible, scrollable row.
 * Matches the Console design language: chips with inset style, amber-tinted
 * when selected, blue focus halo.
 *
 * Used in Clip Edit Dialog under COLOUR section (two-up with GROUP selector).
 *
 * Also provides a static method to create a compact popup for context menus.
 */
class ColorSwatchPicker : public juce::Component {
public:
  //==============================================================================
  ColorSwatchPicker();
  ~ColorSwatchPicker() override = default;

  //==============================================================================
  // Set currently selected color (finds closest chip and highlights it)
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

  //==============================================================================
  // Create a compact color picker popup (for context menus)
  // The popup uses CallOutBox and calls the callback on selection
  static void showPopupAt(const juce::Rectangle<int>& screenBounds,
                          const juce::Colour& currentColor,
                          std::function<void(const juce::Colour&)> onSelect);

private:
  //==============================================================================
  // Internal chip button for each color
  class ColorChip : public juce::Button {
  public:
    ColorChip(const juce::Colour& color, int index)
        : juce::Button("ColorChip" + juce::String(index)), m_color(color), m_chipIndex(index) {
      setClickingTogglesState(true);
      setWantsKeyboardFocus(true);
    }

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;
    void colourChanged() override;

    juce::Colour m_color;
    int m_chipIndex = -1;
    bool m_isSelected = false;
  };

  // Compact popup component (used by showPopupAt)
  class CompactPopup : public juce::Component {
  public:
    CompactPopup(const juce::Colour& currentColor,
                 std::function<void(const juce::Colour&)> onSelect);
    void paint(juce::Graphics& g) override;
    void resized() override;

  private:
    void initializeColorPalette();
    void createChips();
    void updateChipSelection();
    void selectColor(int index);

    std::vector<juce::Colour> m_colorPalette;
    juce::Colour m_selectedColor;
    int m_selectedIndex = -1;
    std::function<void(const juce::Colour&)> m_onSelect;
    std::vector<std::unique_ptr<ColorChip>> m_chips;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompactPopup)
  };

  void initializeColorPalette();
  void createChips();
  void updateChipSelection();

  // Color palette - 24 curated colors (2 rows of 12 for compact inline display)
  std::vector<juce::Colour> m_colorPalette;
  juce::Colour m_selectedColor = juce::Colours::red;
  int m_selectedIndex = -1;

  std::vector<std::unique_ptr<ColorChip>> m_chips;
  juce::Viewport m_viewport;
  std::unique_ptr<juce::Component> m_chipContainer;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ColorSwatchPicker)
};