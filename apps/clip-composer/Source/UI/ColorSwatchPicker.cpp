// SPDX-License-Identifier: MIT

#include "ColorSwatchPicker.h"
#include "ConsoleTheme.h"
#include "DesignTokens.h"

using namespace OCC::Design;

//==============================================================================
// ColorChip implementation (shared by inline picker and compact popup)
//==============================================================================
void ColorSwatchPicker::ColorChip::paintButton(juce::Graphics& g,
                                               bool shouldDrawButtonAsHighlighted,
                                               bool shouldDrawButtonAsDown) {
  auto bounds = getLocalBounds().toFloat();

  // Base inset field style (Console matte well)
  OCC::Console::drawInsetField(g, bounds);

  // Fill with the chip's color
  g.setColour(m_color);
  g.fillRoundedRectangle(bounds.reduced(4.0f), 4.0f);

  // Selection ring: amber-tinted when selected, subtle when not
  if (m_isSelected) {
    g.setColour(juce::Colour(OCC::Design::kAmber));
    g.drawRoundedRectangle(bounds.reduced(4.0f), 4.0f, 2.5f);

    // Inner highlight
    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.drawRoundedRectangle(bounds.reduced(6.0f), 4.0f, 1.0f);
  } else {
    // Subtle border for unselected chips
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawRoundedRectangle(bounds.reduced(4.0f), 4.0f, 1.0f);
  }

  // Focus halo (blue) when keyboard-focused
  if (hasKeyboardFocus(false)) {
    g.setColour(juce::Colour(OCC::Design::kNeveBlue).withAlpha(0.6f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 5.0f, 2.0f);
  }

  // Down/pressed state
  if (shouldDrawButtonAsDown) {
    g.setColour(juce::Colours::black.withAlpha(0.2f));
    g.fillRoundedRectangle(bounds.reduced(4.0f), 4.0f);
  }
}

void ColorSwatchPicker::ColorChip::colourChanged() {
  repaint();
}

//==============================================================================
// ColorSwatchPicker (inline horizontal row) implementation
//==============================================================================
ColorSwatchPicker::ColorSwatchPicker() {
  initializeColorPalette();
  createChips();

  // Viewport setup for horizontal scrolling
  addAndMakeVisible(m_viewport);
  m_viewport.setViewedComponent(m_chipContainer.get(), false);
  m_viewport.setScrollBarsShown(false, false); // No visible scrollbars
}

void ColorSwatchPicker::initializeColorPalette() {
  // 24 curated colors (2 rows of 12) for inline display
  // Designed for dark mode UI with excellent text contrast on clip buttons
  m_colorPalette = {
      // Row 1: Warm spectrum (reds -> oranges -> yellows)
      juce::Colour(0xffCC2936), // Deep red
      juce::Colour(0xffE63946), // Bright red
      juce::Colour(0xffF4442E), // Red-orange
      juce::Colour(0xffFF6B35), // Vivid orange
      juce::Colour(0xffFF8C42), // Light orange
      juce::Colour(0xffFFA500), // Pure orange
      juce::Colour(0xffFFB627), // Golden orange
      juce::Colour(0xffFFC857), // Amber
      juce::Colour(0xffFFD93D), // Golden yellow
      juce::Colour(0xffFFE66D), // Bright yellow
      juce::Colour(0xffFFF176), // Light yellow
      juce::Colour(0xffFFF9C4), // Pale yellow

      // Row 2: Cool spectrum (greens -> cyans -> blues -> purples -> magentas)
      juce::Colour(0xff4CAF50), // Green
      juce::Colour(0xff26C6DA), // Bright cyan
      juce::Colour(0xff00BCD4), // Cyan
      juce::Colour(0xff039BE5), // Blue
      juce::Colour(0xff1976D2), // Strong blue
      juce::Colour(0xff7E57C2), // Medium purple
      juce::Colour(0xff9C27B0), // Purple
      juce::Colour(0xffE91E63), // Pink
      juce::Colour(0xffF06292), // Light pink
      juce::Colour(0xff8D6E63), // Brown
      juce::Colour(0xffFFFFFF), // White
      juce::Colour(0xff212121), // Black (dark neutral)
  };
}

void ColorSwatchPicker::createChips() {
  m_chipContainer = std::make_unique<juce::Component>();
  m_chipContainer->setName("ColorChipContainer");

  const int numColors = static_cast<int>(m_colorPalette.size());
  const int chipWidth = 28;
  const int chipHeight = 28;
  const int spacing = 6;

  for (int i = 0; i < numColors; ++i) {
    auto chip = std::make_unique<ColorChip>(m_colorPalette[i], i);

    chip->onClick = [this, i]() {
      m_selectedIndex = i;
      m_selectedColor = m_colorPalette[i];
      updateChipSelection();

      if (onColorSelected) {
        onColorSelected(m_selectedColor);
      }
    };

    m_chips.push_back(std::move(chip));
    m_chipContainer->addAndMakeVisible(m_chips.back().get());
  }

  // Set container size to fit all chips horizontally
  int totalWidth = numColors * chipWidth + (numColors - 1) * spacing;
  m_chipContainer->setSize(totalWidth, chipHeight);
}

void ColorSwatchPicker::resized() {
  // Viewport fills the entire component
  m_viewport.setBounds(getLocalBounds());

  // Position chip container at top-left of viewport
  if (m_chipContainer) {
    m_chipContainer->setTopLeftPosition(0, 0);

    // Layout chips in a horizontal row
    const int chipWidth = 28;
    const int chipHeight = 28;
    const int spacing = 6;

    int x = 0;
    for (auto& chip : m_chips) {
      chip->setBounds(x, 0, chipWidth, chipHeight);
      x += chipWidth + spacing;
    }
  }
}

void ColorSwatchPicker::paint(juce::Graphics& g) {
  // Background matches Console inset field
  auto bounds = getLocalBounds().toFloat();
  OCC::Console::drawInsetField(g, bounds);
}

void ColorSwatchPicker::setSelectedColor(const juce::Colour& color) {
  m_selectedColor = color;

  // Find closest matching color in palette
  m_selectedIndex = -1;
  float minDistance = std::numeric_limits<float>::max();

  for (int i = 0; i < static_cast<int>(m_colorPalette.size()); ++i) {
    auto paletteColor = m_colorPalette[i];
    float distance = std::abs(paletteColor.getRed() - color.getRed()) +
                     std::abs(paletteColor.getGreen() - color.getGreen()) +
                     std::abs(paletteColor.getBlue() - color.getBlue());

    if (distance < minDistance) {
      minDistance = distance;
      m_selectedIndex = i;
    }
  }

  updateChipSelection();
  repaint();
}

void ColorSwatchPicker::updateChipSelection() {
  for (auto& chip : m_chips) {
    chip->m_isSelected = (chip->m_chipIndex == m_selectedIndex);
    chip->setToggleState(chip->m_isSelected, juce::dontSendNotification);
    chip->repaint();
  }

  // Scroll viewport to show selected chip
  if (m_selectedIndex >= 0 && m_chipContainer &&
      m_selectedIndex < static_cast<int>(m_chips.size())) {
    const int chipWidth = 28;
    const int spacing = 6;
    int targetX = m_selectedIndex * (chipWidth + spacing);

    // Center the selected chip in the viewport
    int viewportWidth = m_viewport.getWidth();
    int centerX = targetX - (viewportWidth / 2) + (chipWidth / 2);

    m_viewport.setViewPosition(juce::jmax(0, centerX), 0);
  }
}

//==============================================================================
// CompactPopup implementation (for context menu popup)
//==============================================================================
ColorSwatchPicker::CompactPopup::CompactPopup(const juce::Colour& currentColor,
                                              std::function<void(const juce::Colour&)> onSelect)
    : m_selectedColor(currentColor), m_onSelect(std::move(onSelect)) {
  initializeColorPalette();
  createChips();
  updateChipSelection();
}

void ColorSwatchPicker::CompactPopup::initializeColorPalette() {
  // Same 24 curated colors as inline picker
  m_colorPalette = {
      juce::Colour(0xffCC2936), juce::Colour(0xffE63946), juce::Colour(0xffF4442E),
      juce::Colour(0xffFF6B35), juce::Colour(0xffFF8C42), juce::Colour(0xffFFA500),
      juce::Colour(0xffFFB627), juce::Colour(0xffFFC857), juce::Colour(0xffFFD93D),
      juce::Colour(0xffFFE66D), juce::Colour(0xffFFF176), juce::Colour(0xffFFF9C4),
      juce::Colour(0xff4CAF50), juce::Colour(0xff26C6DA), juce::Colour(0xff00BCD4),
      juce::Colour(0xff039BE5), juce::Colour(0xff1976D2), juce::Colour(0xff7E57C2),
      juce::Colour(0xff9C27B0), juce::Colour(0xffE91E63), juce::Colour(0xffF06292),
      juce::Colour(0xff8D6E63), juce::Colour(0xffFFFFFF), juce::Colour(0xff212121),
  };
}

void ColorSwatchPicker::CompactPopup::createChips() {
  const int numColors = static_cast<int>(m_colorPalette.size());
  const int chipWidth = 28;
  const int chipHeight = 28;
  const int spacing = 6;
  const int cols = 12;
  const int rows = 2;

  for (int i = 0; i < numColors; ++i) {
    auto chip = std::make_unique<ColorChip>(m_colorPalette[i], i);

    chip->onClick = [this, i]() { selectColor(i); };

    m_chips.push_back(std::move(chip));
    addAndMakeVisible(m_chips.back().get());
  }

  // Set component size for 2-row grid
  int totalWidth = cols * chipWidth + (cols - 1) * spacing;
  int totalHeight = rows * chipHeight + (rows - 1) * spacing;
  setSize(totalWidth + 16, totalHeight + 16); // Add padding
}

void ColorSwatchPicker::CompactPopup::resized() {
  const int chipWidth = 28;
  const int chipHeight = 28;
  const int spacing = 6;
  const int cols = 12;
  const int padding = 8;

  int x = padding;
  int y = padding;

  for (int i = 0; i < static_cast<int>(m_chips.size()); ++i) {
    int col = i % cols;
    int row = i / cols;

    x = padding + col * (chipWidth + spacing);
    y = padding + row * (chipHeight + spacing);

    m_chips[i]->setBounds(x, y, chipWidth, chipHeight);
  }
}

void ColorSwatchPicker::CompactPopup::paint(juce::Graphics& g) {
  auto bounds = getLocalBounds().toFloat();
  // Console chassis background with rounded corners
  g.setColour(juce::Colour(OCC::Design::kBgComponent));
  g.fillRoundedRectangle(bounds.reduced(2.0f), 6.0f);
  g.setColour(juce::Colour(OCC::Design::kBorderDefault));
  g.drawRoundedRectangle(bounds.reduced(2.0f), 6.0f, 1.5f);
}

void ColorSwatchPicker::CompactPopup::updateChipSelection() {
  for (auto& chip : m_chips) {
    chip->m_isSelected = (chip->m_chipIndex == m_selectedIndex);
    chip->setToggleState(chip->m_isSelected, juce::dontSendNotification);
    chip->repaint();
  }
}

void ColorSwatchPicker::CompactPopup::selectColor(int index) {
  if (index < 0 || index >= static_cast<int>(m_colorPalette.size()))
    return;

  m_selectedIndex = index;
  m_selectedColor = m_colorPalette[index];
  updateChipSelection();

  if (m_onSelect) {
    m_onSelect(m_selectedColor);
  }

  // Close the popup (CallOutBox will handle cleanup)
  if (auto* parent = getParentComponent()) {
    if (auto* callOutBox = dynamic_cast<juce::CallOutBox*>(parent)) {
      callOutBox->dismiss();
    }
  }
}

//==============================================================================
// Static method to show compact popup
//==============================================================================
void ColorSwatchPicker::showPopupAt(const juce::Rectangle<int>& screenBounds,
                                    const juce::Colour& currentColor,
                                    std::function<void(const juce::Colour&)> onSelect) {
  auto* popup = new CompactPopup(currentColor, std::move(onSelect));

  juce::CallOutBox::launchAsynchronously(std::unique_ptr<juce::Component>(popup), screenBounds,
                                         nullptr);
}