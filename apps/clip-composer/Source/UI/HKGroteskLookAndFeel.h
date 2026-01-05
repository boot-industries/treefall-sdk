// SPDX-License-Identifier: MIT

#pragma once

#include "DesignTokens.h"
#include <juce_gui_extra/juce_gui_extra.h>

//==============================================================================
/**
 * HKGroteskLookAndFeel - Custom look and feel using HK Grotesk font
 *
 * Applies HK Grotesk font to all UI components including menus and dialogs
 * Uses design tokens from DesignTokens.h for consistent styling
 */
class HKGroteskLookAndFeel : public juce::LookAndFeel_V4 {
public:
  HKGroteskLookAndFeel() {
    // Set HK Grotesk as default font for all components
    setDefaultSansSerifTypefaceName("HK Grotesk");

    //==========================================================================
    // POPUP MENU COLORS
    //==========================================================================
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(OCC::Design::kBgComponent));
    setColour(juce::PopupMenu::textColourId, juce::Colour(OCC::Design::kTextPrimary));
    setColour(juce::PopupMenu::headerTextColourId, juce::Colour(OCC::Design::kTextSecondary));
    setColour(juce::PopupMenu::highlightedBackgroundColourId,
              juce::Colour(OCC::Design::kBorderDefault));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(OCC::Design::kTextPrimary));

    //==========================================================================
    // TEXT BUTTON COLORS
    //==========================================================================
    setColour(juce::TextButton::buttonColourId, juce::Colour(OCC::Design::kBgComponent));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(OCC::Design::kAccentTeal));
    setColour(juce::TextButton::textColourOffId, juce::Colour(OCC::Design::kTextPrimary));
    setColour(juce::TextButton::textColourOnId, juce::Colour(OCC::Design::kTextPrimary));

    //==========================================================================
    // TOGGLE BUTTON COLORS
    //==========================================================================
    setColour(juce::ToggleButton::textColourId, juce::Colour(OCC::Design::kTextPrimary));
    setColour(juce::ToggleButton::tickColourId, juce::Colour(OCC::Design::kAccentCyan));
    setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(OCC::Design::kTextSecondary));

    //==========================================================================
    // SLIDER COLORS
    //==========================================================================
    setColour(juce::Slider::backgroundColourId, juce::Colour(OCC::Design::kBgSurface));
    setColour(juce::Slider::trackColourId, juce::Colour(OCC::Design::kAccentTeal));
    setColour(juce::Slider::thumbColourId, juce::Colour(OCC::Design::kAccentCyan));

    //==========================================================================
    // LABEL COLORS
    //==========================================================================
    setColour(juce::Label::textColourId, juce::Colour(OCC::Design::kTextPrimary));
    setColour(juce::Label::outlineColourId, juce::Colour(OCC::Design::kBorderDefault));

    //==========================================================================
    // COMBO BOX COLORS
    //==========================================================================
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(OCC::Design::kBgComponent));
    setColour(juce::ComboBox::textColourId, juce::Colour(OCC::Design::kTextPrimary));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(OCC::Design::kBorderDefault));
    setColour(juce::ComboBox::arrowColourId, juce::Colour(OCC::Design::kTextSecondary));

    //==========================================================================
    // TEXT EDITOR COLORS
    //==========================================================================
    setColour(juce::TextEditor::backgroundColourId, juce::Colour(OCC::Design::kBgSurface));
    setColour(juce::TextEditor::textColourId, juce::Colour(OCC::Design::kTextPrimary));
    setColour(juce::TextEditor::outlineColourId, juce::Colour(OCC::Design::kBorderDefault));
    setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(OCC::Design::kBorderActive));
    setColour(juce::TextEditor::highlightColourId,
              juce::Colour(OCC::Design::kAccentTeal).withAlpha(0.4f));
    setColour(juce::TextEditor::highlightedTextColourId, juce::Colour(OCC::Design::kTextPrimary));

    //==========================================================================
    // SCROLL BAR COLORS
    //==========================================================================
    setColour(juce::ScrollBar::backgroundColourId, juce::Colour(OCC::Design::kBgSecondary));
    setColour(juce::ScrollBar::thumbColourId, juce::Colour(OCC::Design::kBorderDefault));

    //==========================================================================
    // LIST BOX COLORS
    //==========================================================================
    setColour(juce::ListBox::backgroundColourId, juce::Colour(OCC::Design::kBgComponent));
    setColour(juce::ListBox::textColourId, juce::Colour(OCC::Design::kTextPrimary));
    setColour(juce::ListBox::outlineColourId, juce::Colour(OCC::Design::kBorderDefault));

    //==========================================================================
    // ALERT WINDOW COLORS
    //==========================================================================
    setColour(juce::AlertWindow::backgroundColourId, juce::Colour(OCC::Design::kBgSurface));
    setColour(juce::AlertWindow::textColourId, juce::Colour(OCC::Design::kTextPrimary));
    setColour(juce::AlertWindow::outlineColourId, juce::Colour(OCC::Design::kBorderDefault));

    //==========================================================================
    // TOOLTIP COLORS
    //==========================================================================
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(OCC::Design::kBgSurface));
    setColour(juce::TooltipWindow::textColourId, juce::Colour(OCC::Design::kTextPrimary));
    setColour(juce::TooltipWindow::outlineColourId, juce::Colour(OCC::Design::kBorderDefault));
  }

  //============================================================================
  // FONT OVERRIDES
  //============================================================================

  juce::Font getPopupMenuFont() override {
    return juce::FontOptions("HK Grotesk", OCC::Design::kFontMD, juce::Font::plain);
  }

  juce::Font getMenuBarFont(juce::MenuBarComponent&, int, const juce::String&) override {
    return juce::FontOptions("HK Grotesk", OCC::Design::kFontMD, juce::Font::plain);
  }

  juce::Font getTextButtonFont(juce::TextButton&, int) override {
    return juce::FontOptions("HK Grotesk", OCC::Design::kFontMD, juce::Font::plain);
  }

  juce::Font getAlertWindowFont() override {
    return juce::FontOptions("HK Grotesk", OCC::Design::kFontMD, juce::Font::plain);
  }

  juce::Font getAlertWindowTitleFont() override {
    return juce::FontOptions("HK Grotesk", OCC::Design::kFontLG, juce::Font::bold);
  }

  juce::Font getAlertWindowMessageFont() override {
    return juce::FontOptions("HK Grotesk", OCC::Design::kFontMD, juce::Font::plain);
  }

  juce::Font getLabelFont(juce::Label&) override {
    return juce::FontOptions("HK Grotesk", OCC::Design::kFontMD, juce::Font::plain);
  }

  juce::Font getComboBoxFont(juce::ComboBox&) override {
    return juce::FontOptions("HK Grotesk", OCC::Design::kFontMD, juce::Font::plain);
  }

  //============================================================================
  // BUTTON DRAWING
  //============================================================================

  void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                            const juce::Colour& backgroundColour,
                            bool shouldDrawButtonAsHighlighted,
                            bool shouldDrawButtonAsDown) override {
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto cornerSize = OCC::Design::kRadiusMD;

    juce::Colour baseColour = backgroundColour;

    if (shouldDrawButtonAsDown) {
      baseColour = baseColour.brighter(0.2f);
    } else if (shouldDrawButtonAsHighlighted) {
      baseColour = baseColour.brighter(0.1f);
    }

    if (!button.isEnabled()) {
      baseColour = baseColour.withAlpha(0.5f);
    }

    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, cornerSize);

    // Border
    g.setColour(juce::Colour(OCC::Design::kBorderDefault));
    g.drawRoundedRectangle(bounds, cornerSize, OCC::Design::kBorderThin);
  }

  //============================================================================
  // POPUP MENU SIZING
  //============================================================================

  // Issue #12: Add vertical padding to popup menu items (8px total = 4px top +
  // 4px bottom)
  void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
                                 int standardMenuItemHeight, int& idealWidth,
                                 int& idealHeight) override {
    // Call parent to get standard sizing
    juce::LookAndFeel_V4::getIdealPopupMenuItemSize(text, isSeparator, standardMenuItemHeight,
                                                    idealWidth, idealHeight);

    // Add 8px vertical padding (4px top + 4px bottom) for better spacing
    if (!isSeparator) {
      idealHeight += static_cast<int>(OCC::Design::kSpace2);
    }
  }
};
