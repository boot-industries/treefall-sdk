/*
  ==============================================================================

    TextButton.h
    Created: shmui Button System

    Text label button variant.

    Usage:
      shmui::TextButton btn("Submit");
      btn.setStyle(shmui::ButtonStyle::Primary);
      btn.onClick = [this] { handleSubmit(); };

  ==============================================================================
*/

#pragma once

#include "Button.h"
#include "../Icons/Icons.h"

namespace shmui
{

//==============================================================================
/**
 * @brief Text label button with optional leading/trailing icons.
 *
 * Displays text with optional icons before or after the label.
 * Standard button for most UI interactions.
 */
class TextButton : public Button
{
public:
    //==============================================================================
    /** Create a text button with the specified label. */
    explicit TextButton(const juce::String& text = "Button");

    /** Create a text button with text and style. */
    TextButton(const juce::String& text, ButtonStyle style);

    ~TextButton() override = default;

    //==============================================================================
    /// @name Text
    /// @{

    /** Set the button text. */
    void setText(const juce::String& text);
    juce::String getText() const { return m_text; }

    /// @}

    //==============================================================================
    /// @name Icons
    /// @{

    /** Set a leading icon (before text). */
    void setLeadingIcon(IconType icon);
    void clearLeadingIcon();
    bool hasLeadingIcon() const { return m_hasLeadingIcon; }

    /** Set a trailing icon (after text). */
    void setTrailingIcon(IconType icon);
    void clearTrailingIcon();
    bool hasTrailingIcon() const { return m_hasTrailingIcon; }

    /// @}

    //==============================================================================
    /** Get preferred width for this button. */
    int getPreferredWidth() const;

protected:
    //==============================================================================
    void paintContent(juce::Graphics& g,
                      juce::Rectangle<float> bounds,
                      juce::Colour foregroundColor) override;

private:
    juce::String m_text;
    IconType m_leadingIcon = IconType::Play;
    IconType m_trailingIcon = IconType::ChevronRight;
    bool m_hasLeadingIcon = false;
    bool m_hasTrailingIcon = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TextButton)
};

//==============================================================================
/**
 * @brief Button with icon and text together.
 *
 * Convenience class that pre-configures TextButton with a leading icon.
 */
class IconTextButton : public TextButton
{
public:
    /** Create an icon+text button. */
    IconTextButton(IconType icon, const juce::String& text);

    /** Create an icon+text button with style. */
    IconTextButton(IconType icon, const juce::String& text, ButtonStyle style);

    ~IconTextButton() override = default;

    /** Change the icon. */
    void setIcon(IconType icon) { setLeadingIcon(icon); }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IconTextButton)
};

} // namespace shmui
