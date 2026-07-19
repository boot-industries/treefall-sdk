/*
  ==============================================================================

    TextButton.cpp
    Created: shmui Button System

    Text button implementation.

  ==============================================================================
*/

#include "TextButton.h"

namespace shmui
{

//==============================================================================
TextButton::TextButton(const juce::String& text)
    : m_text(text)
{
}

TextButton::TextButton(const juce::String& text, ButtonStyle style)
    : m_text(text)
{
    setStyle(style);
}

//==============================================================================
void TextButton::setText(const juce::String& text)
{
    if (m_text != text)
    {
        m_text = text;
        repaint();
    }
}

void TextButton::setLeadingIcon(IconType icon)
{
    m_leadingIcon = icon;
    m_hasLeadingIcon = true;
    repaint();
}

void TextButton::clearLeadingIcon()
{
    m_hasLeadingIcon = false;
    repaint();
}

void TextButton::setTrailingIcon(IconType icon)
{
    m_trailingIcon = icon;
    m_hasTrailingIcon = true;
    repaint();
}

void TextButton::clearTrailingIcon()
{
    m_hasTrailingIcon = false;
    repaint();
}

int TextButton::getPreferredWidth() const
{
    const float height = getButtonHeight(getButtonSize());
    const float fontSize = getFontHeightForButton(getButtonSize());
    const float padding = getPaddingForButton(getButtonSize());
    const float iconSize = getIconSizeForButton(getButtonSize());
    const float iconGap = 6.0f;

    juce::Font font(fontSize);
    float textWidth = font.getStringWidthFloat(m_text);

    float totalWidth = padding * 2.0f + textWidth;

    if (m_hasLeadingIcon)
        totalWidth += iconSize + iconGap;

    if (m_hasTrailingIcon)
        totalWidth += iconSize + iconGap;

    // Minimum width is height (square)
    return static_cast<int>(std::max(totalWidth, height));
}

//==============================================================================
void TextButton::paintContent(juce::Graphics& g,
                              juce::Rectangle<float> bounds,
                              juce::Colour foregroundColor)
{
    const float fontSize = getFontHeightForButton(getButtonSize());
    const float iconSize = getIconSizeForButton(getButtonSize()) * 0.75f; // Slightly smaller icons for text buttons
    const float iconGap = 6.0f;

    g.setColour(foregroundColor);
    juce::Font font(fontSize);
    g.setFont(font);

    float contentWidth = font.getStringWidthFloat(m_text);
    if (m_hasLeadingIcon)
        contentWidth += iconSize + iconGap;
    if (m_hasTrailingIcon)
        contentWidth += iconSize + iconGap;

    // Center content horizontally
    float x = bounds.getX() + (bounds.getWidth() - contentWidth) * 0.5f;
    float centerY = bounds.getCentreY();

    // Draw leading icon
    if (m_hasLeadingIcon)
    {
        auto iconBounds = juce::Rectangle<float>(x, centerY - iconSize * 0.5f, iconSize, iconSize);
        Icons::drawIcon(g, m_leadingIcon, iconBounds, foregroundColor);
        x += iconSize + iconGap;
    }

    // Draw text
    float textWidth = font.getStringWidthFloat(m_text);
    g.drawText(m_text,
               juce::Rectangle<float>(x, bounds.getY(), textWidth, bounds.getHeight()),
               juce::Justification::centredLeft, false);
    x += textWidth;

    // Draw trailing icon
    if (m_hasTrailingIcon)
    {
        x += iconGap;
        auto iconBounds = juce::Rectangle<float>(x, centerY - iconSize * 0.5f, iconSize, iconSize);
        Icons::drawIcon(g, m_trailingIcon, iconBounds, foregroundColor);
    }
}

//==============================================================================
IconTextButton::IconTextButton(IconType icon, const juce::String& text)
    : TextButton(text)
{
    setLeadingIcon(icon);
}

IconTextButton::IconTextButton(IconType icon, const juce::String& text, ButtonStyle style)
    : TextButton(text, style)
{
    setLeadingIcon(icon);
}

} // namespace shmui
