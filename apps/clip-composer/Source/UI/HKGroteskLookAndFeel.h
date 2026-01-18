// SPDX-License-Identifier: MIT

#pragma once

#include "DesignTokens.h"
#include <juce_gui_extra/juce_gui_extra.h>

//==============================================================================
/**
 * HKGroteskLookAndFeel - Neo-vintage console aesthetic
 *
 * Inspired by Neve hardware: brushed aluminum, warm amber indicators,
 * tactile button feedback with depth and bevels. Premium broadcast feel.
 *
 * Visual Effects:
 * - Top-lit gradient buttons (simulating studio overhead lighting)
 * - Inner shadow for inset controls
 * - Warm color palette with cream text
 * - Metallic border highlights
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
  // BUTTON DRAWING (Neo-vintage console aesthetic)
  //============================================================================

  void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                            const juce::Colour& backgroundColour,
                            bool shouldDrawButtonAsHighlighted,
                            bool shouldDrawButtonAsDown) override {
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto cornerSize = OCC::Design::kRadiusMD;

    juce::Colour baseColour = backgroundColour;

    // Disabled state
    if (!button.isEnabled()) {
      baseColour = baseColour.withAlpha(0.4f);
    }

    // === PRESSED STATE: Inset effect ===
    if (shouldDrawButtonAsDown) {
      // Darker, pressed into the console surface
      juce::Colour pressedTop = baseColour.darker(0.15f);
      juce::Colour pressedBottom = baseColour.darker(0.05f);

      juce::ColourGradient pressedGradient(pressedTop, bounds.getX(), bounds.getY(), pressedBottom,
                                           bounds.getX(), bounds.getBottom(), false);

      g.setGradientFill(pressedGradient);
      g.fillRoundedRectangle(bounds, cornerSize);

      // Inner shadow (top edge darker = pressed in)
      g.setColour(juce::Colours::black.withAlpha(0.25f));
      g.drawRoundedRectangle(bounds.reduced(1.0f), cornerSize - 1, 1.0f);

      // Subtle border
      g.setColour(juce::Colour(OCC::Design::kBorderDefault).darker(0.2f));
      g.drawRoundedRectangle(bounds, cornerSize, OCC::Design::kBorderThin);

      // === HOVER STATE: Lifted, lit from above ===
    } else if (shouldDrawButtonAsHighlighted) {
      // Brighter top = lit from above
      juce::Colour hoverTop = baseColour.brighter(0.12f);
      juce::Colour hoverBottom = baseColour.darker(0.05f);

      juce::ColourGradient hoverGradient(hoverTop, bounds.getX(), bounds.getY(), hoverBottom,
                                         bounds.getX(), bounds.getBottom(), false);

      g.setGradientFill(hoverGradient);
      g.fillRoundedRectangle(bounds, cornerSize);

      // Highlight edge (top metallic sheen)
      g.setColour(juce::Colour(OCC::Design::kMetalLight).withAlpha(0.15f));
      auto highlightBounds = bounds.withHeight(bounds.getHeight() * 0.4f);
      g.fillRoundedRectangle(highlightBounds, cornerSize);

      // Brighter border on hover
      g.setColour(juce::Colour(OCC::Design::kBorderHighlight));
      g.drawRoundedRectangle(bounds, cornerSize, OCC::Design::kBorderThin);

      // === NORMAL STATE: Subtle top-lit gradient ===
    } else {
      // Gentle gradient: slightly lighter at top (studio lighting simulation)
      juce::Colour normalTop = baseColour.brighter(0.04f);
      juce::Colour normalBottom = baseColour.darker(0.04f);

      juce::ColourGradient normalGradient(normalTop, bounds.getX(), bounds.getY(), normalBottom,
                                          bounds.getX(), bounds.getBottom(), false);

      g.setGradientFill(normalGradient);
      g.fillRoundedRectangle(bounds, cornerSize);

      // Standard border
      g.setColour(juce::Colour(OCC::Design::kBorderDefault));
      g.drawRoundedRectangle(bounds, cornerSize, OCC::Design::kBorderThin);
    }
  }

  //============================================================================
  // SLIDER DRAWING (Console fader aesthetic)
  //============================================================================

  void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                        float minSliderPos, float maxSliderPos,
                        const juce::Slider::SliderStyle style, juce::Slider& slider) override {
    const bool isVertical =
        style == juce::Slider::LinearVertical || style == juce::Slider::LinearBarVertical;

    auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                         static_cast<float>(width), static_cast<float>(height));

    // === TRACK (Recessed channel) ===
    const float trackWidth = isVertical ? 6.0f : bounds.getHeight() * 0.4f;
    juce::Rectangle<float> track;

    if (isVertical) {
      track = bounds.withSizeKeepingCentre(trackWidth, bounds.getHeight());
    } else {
      track = bounds.withSizeKeepingCentre(bounds.getWidth(), trackWidth);
    }

    // Track background (inset)
    g.setColour(juce::Colour(OCC::Design::kBgInset));
    g.fillRoundedRectangle(track, 2.0f);

    // Track inner shadow
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.drawRoundedRectangle(track, 2.0f, 1.0f);

    // === FILL (Active portion) ===
    juce::Rectangle<float> fill;
    if (isVertical) {
      float fillHeight = sliderPos - static_cast<float>(y);
      fill = track.withTop(sliderPos).withBottom(track.getBottom());
    } else {
      fill = track.withRight(sliderPos);
    }

    // Gradient fill (Neve blue)
    juce::Colour fillTop = juce::Colour(OCC::Design::kNeveBlue);
    juce::Colour fillBottom = juce::Colour(OCC::Design::kNeveBlueDark);

    juce::ColourGradient fillGradient(fillTop, fill.getX(), fill.getY(), fillBottom, fill.getX(),
                                      fill.getBottom(), false);
    g.setGradientFill(fillGradient);
    g.fillRoundedRectangle(fill, 2.0f);

    // === THUMB (Console knob) ===
    const float thumbSize = isVertical ? 20.0f : 16.0f;
    juce::Rectangle<float> thumbBounds;

    if (isVertical) {
      thumbBounds =
          juce::Rectangle<float>(thumbSize, thumbSize).withCentre({bounds.getCentreX(), sliderPos});
    } else {
      thumbBounds =
          juce::Rectangle<float>(thumbSize, thumbSize).withCentre({sliderPos, bounds.getCentreY()});
    }

    // Thumb gradient (brushed aluminum effect)
    juce::Colour thumbTop = juce::Colour(OCC::Design::kMetalLight);
    juce::Colour thumbBottom = juce::Colour(OCC::Design::kMetalDark);

    juce::ColourGradient thumbGradient(thumbTop, thumbBounds.getX(), thumbBounds.getY(),
                                       thumbBottom, thumbBounds.getX(), thumbBounds.getBottom(),
                                       false);
    g.setGradientFill(thumbGradient);
    g.fillEllipse(thumbBounds);

    // Thumb border
    g.setColour(juce::Colour(OCC::Design::kBorderDefault));
    g.drawEllipse(thumbBounds, 1.0f);

    // Center indicator line
    g.setColour(juce::Colour(OCC::Design::kNeveBlue));
    if (isVertical) {
      g.drawHorizontalLine(static_cast<int>(thumbBounds.getCentreY()), thumbBounds.getX() + 4.0f,
                           thumbBounds.getRight() - 4.0f);
    } else {
      g.drawVerticalLine(static_cast<int>(thumbBounds.getCentreX()), thumbBounds.getY() + 4.0f,
                         thumbBounds.getBottom() - 4.0f);
    }
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

  //============================================================================
  // TEXT EDITOR DRAWING (Inset field)
  //============================================================================

  void fillTextEditorBackground(juce::Graphics& g, int width, int height,
                                juce::TextEditor& editor) override {
    auto bounds =
        juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));

    // Inset background
    g.setColour(juce::Colour(OCC::Design::kBgInset));
    g.fillRoundedRectangle(bounds, OCC::Design::kRadiusSM);

    // Inner shadow (top edge)
    g.setColour(juce::Colours::black.withAlpha(0.15f));
    g.drawHorizontalLine(1, 1.0f, static_cast<float>(width) - 1.0f);
  }

  void drawTextEditorOutline(juce::Graphics& g, int width, int height,
                             juce::TextEditor& editor) override {
    auto bounds =
        juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));

    if (editor.hasKeyboardFocus(true)) {
      g.setColour(juce::Colour(OCC::Design::kNeveBlue));
      g.drawRoundedRectangle(bounds.reduced(0.5f), OCC::Design::kRadiusSM, 1.5f);
    } else {
      g.setColour(juce::Colour(OCC::Design::kBorderDefault));
      g.drawRoundedRectangle(bounds.reduced(0.5f), OCC::Design::kRadiusSM, 1.0f);
    }
  }
};
