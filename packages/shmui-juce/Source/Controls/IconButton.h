/*
  ==============================================================================

    IconButton.h
    Created: shmui Button System

    Icon-only button variant with square aspect ratio.

    Usage:
      shmui::IconButton btn(shmui::IconType::Play);
      btn.setStyle(shmui::ButtonStyle::Ghost);
      btn.onClick = [this] { handlePlay(); };

  ==============================================================================
*/

#pragma once

#include "Button.h"
#include "../Icons/Icons.h"

namespace shmui
{

//==============================================================================
/**
 * @brief Icon-only button with square aspect ratio.
 *
 * Displays a single icon centered in a square button.
 * Ideal for toolbar buttons, action buttons, and compact UIs.
 */
class IconButton : public Button
{
public:
    //==============================================================================
    /** Create an icon button with the specified icon. */
    explicit IconButton(IconType icon = IconType::Play);

    /** Create an icon button with icon and style. */
    IconButton(IconType icon, ButtonStyle style);

    ~IconButton() override = default;

    //==============================================================================
    /// @name Icon
    /// @{

    /** Set the icon to display. */
    void setIcon(IconType icon);
    IconType getIcon() const { return m_icon; }

    /** Set stroke width for icon (0 = filled, >0 = stroked). */
    void setIconStrokeWidth(float strokeWidth);
    float getIconStrokeWidth() const { return m_iconStrokeWidth; }

    /// @}

    //==============================================================================
    /** Get preferred size for this button (square based on ButtonSize). */
    int getPreferredSize() const;

protected:
    //==============================================================================
    void paintContent(juce::Graphics& g,
                      juce::Rectangle<float> bounds,
                      juce::Colour foregroundColor) override;

private:
    IconType m_icon;
    float m_iconStrokeWidth = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IconButton)
};

} // namespace shmui
