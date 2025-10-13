// SPDX-License-Identifier: MIT

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

//==============================================================================
/**
 * InterLookAndFeel - Custom look and feel using Inter font
 *
 * Applies Inter font to all UI components including menus and dialogs
 */
class InterLookAndFeel : public juce::LookAndFeel_V4
{
public:
    InterLookAndFeel()
    {
        // Set Inter as default font for all components
        setDefaultSansSerifTypefaceName("Inter");

        // Force popup menus to use dark theme
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff2a2a2a));
        setColour(juce::PopupMenu::textColourId, juce::Colours::white);
        setColour(juce::PopupMenu::headerTextColourId, juce::Colours::lightgrey);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff404040));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    }

    juce::Font getPopupMenuFont() override
    {
        return juce::FontOptions("Inter", 14.0f, juce::Font::plain);
    }

    juce::Font getMenuBarFont(juce::MenuBarComponent&, int, const juce::String&) override
    {
        return juce::FontOptions("Inter", 14.0f, juce::Font::plain);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int) override
    {
        return juce::FontOptions("Inter", 14.0f, juce::Font::plain);
    }

    juce::Font getAlertWindowFont() override
    {
        return juce::FontOptions("Inter", 14.0f, juce::Font::plain);
    }

    juce::Font getAlertWindowTitleFont() override
    {
        return juce::FontOptions("Inter", 16.0f, juce::Font::bold);
    }

    juce::Font getAlertWindowMessageFont() override
    {
        return juce::FontOptions("Inter", 14.0f, juce::Font::plain);
    }
};
