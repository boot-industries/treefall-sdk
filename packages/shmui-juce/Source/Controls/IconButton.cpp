/*
  ==============================================================================

    IconButton.cpp
    Created: shmui Button System

    Icon-only button implementation.

  ==============================================================================
*/

#include "IconButton.h"

namespace shmui
{

//==============================================================================
IconButton::IconButton(IconType icon)
    : m_icon(icon)
{
}

IconButton::IconButton(IconType icon, ButtonStyle style)
    : m_icon(icon)
{
    setStyle(style);
}

//==============================================================================
void IconButton::setIcon(IconType icon)
{
    if (m_icon != icon)
    {
        m_icon = icon;
        repaint();
    }
}

void IconButton::setIconStrokeWidth(float strokeWidth)
{
    if (m_iconStrokeWidth != strokeWidth)
    {
        m_iconStrokeWidth = strokeWidth;
        repaint();
    }
}

int IconButton::getPreferredSize() const
{
    return static_cast<int>(getButtonHeight(getButtonSize()));
}

//==============================================================================
void IconButton::paintContent(juce::Graphics& g,
                              juce::Rectangle<float> bounds,
                              juce::Colour foregroundColor)
{
    const float iconSize = getIconSizeForButton(getButtonSize());

    // Center the icon in bounds
    auto iconBounds = bounds.withSizeKeepingCentre(iconSize, iconSize);

    Icons::drawIcon(g, m_icon, iconBounds, foregroundColor, m_iconStrokeWidth);
}

} // namespace shmui
