// SPDX-License-Identifier: MIT

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

//==============================================================================
/**
 * InterLookAndFeel - Custom look and feel using Inter font
 *
 * Applies Inter font to all UI components including menus and dialogs
 */
class InterLookAndFeel : public juce::LookAndFeel_V4 {
public:
  InterLookAndFeel() {
    // Set Inter as default font for all components
    setDefaultSansSerifTypefaceName("Inter");

    // Force popup menus to use dark theme
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff2a2a2a));
    setColour(juce::PopupMenu::textColourId, juce::Colours::white);
    setColour(juce::PopupMenu::headerTextColourId, juce::Colours::lightgrey);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff404040));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
  }

  juce::Font getPopupMenuFont() override {
    return juce::FontOptions("Inter", 14.0f, juce::Font::plain);
  }

  juce::Font getMenuBarFont(juce::MenuBarComponent&, int, const juce::String&) override {
    return juce::FontOptions("Inter", 14.0f, juce::Font::plain);
  }

  juce::Font getTextButtonFont(juce::TextButton&, int) override {
    return juce::FontOptions("Inter", 14.0f, juce::Font::plain);
  }

  juce::Font getAlertWindowFont() override {
    return juce::FontOptions("Inter", 14.0f, juce::Font::plain);
  }

  juce::Font getAlertWindowTitleFont() override {
    return juce::FontOptions("Inter", 16.0f, juce::Font::bold);
  }

  juce::Font getAlertWindowMessageFont() override {
    return juce::FontOptions("Inter", 14.0f, juce::Font::plain);
  }

  // Issue #12: Add vertical padding to popup menu items (8px total = 4px top + 4px bottom)
  void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
                                 int standardMenuItemHeight, int& idealWidth,
                                 int& idealHeight) override {
    // Call parent to get standard sizing
    juce::LookAndFeel_V4::getIdealPopupMenuItemSize(text, isSeparator, standardMenuItemHeight,
                                                    idealWidth, idealHeight);

    // Add 8px vertical padding (4px top + 4px bottom) for better spacing
    if (!isSeparator) {
      idealHeight += 8;
    }
  }
};
