/*
  ==============================================================================

    Button.h
    Created: shmui Button System

    Base button class with shared state handling, animation, and accessibility.

    Usage:
      shmui::Button btn;
      btn.setStyle(shmui::ButtonStyle::Primary);
      btn.setSize(shmui::ButtonSize::Medium);
      btn.onClick = [this] { handleClick(); };

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "ButtonStyles.h"
#include "../Utils/Interpolation.h"

namespace shmui
{

//==============================================================================
/**
 * @brief Base button component with shared state handling.
 *
 * Provides:
 * - Style variants (Primary, Secondary, Ghost, Destructive, Success, Muted)
 * - Size variants (XSmall through XLarge)
 * - Smooth hover/press/focus animations
 * - Keyboard navigation (Tab, Enter, Space)
 * - Theme-aware colors (light/dark mode)
 * - Tooltip support
 * - Accessible semantics
 *
 * Subclasses: IconButton, TextButton, ToggleButton, ClipButton, etc.
 */
class Button : public juce::Component,
               public juce::SettableTooltipClient
{
public:
    //==============================================================================
    Button();
    ~Button() override;

    //==============================================================================
    /// @name Style & Appearance
    /// @{

    /** Set the visual style variant. */
    void setStyle(ButtonStyle style);
    ButtonStyle getStyle() const { return m_style; }

    /** Set the size variant. */
    void setSize(ButtonSize size);
    ButtonSize getButtonSize() const { return m_size; }

    /** Enable/disable dark theme. */
    void setDarkTheme(bool isDark);
    bool isDarkTheme() const { return m_isDarkTheme; }

    /** Set custom colors (overrides style). */
    void setCustomColors(const ButtonColors& colors);
    void clearCustomColors();

    /** Get the current effective colors. */
    ButtonColors getEffectiveColors() const;

    /// @}

    //==============================================================================
    /// @name State
    /// @{

    /** Enable/disable the button. */
    void setEnabled(bool enabled);
    bool isButtonEnabled() const { return m_isEnabled; }

    /** Set loading state (shows spinner, disables interaction). */
    void setLoading(bool loading);
    bool isLoading() const { return m_isLoading; }

    /** Check if button is currently pressed. */
    bool isPressed() const { return m_isPressed; }

    /** Check if button is currently hovered. */
    bool isHovered() const { return m_isHovered; }

    /** Check if button has keyboard focus. */
    bool hasFocusOutline() const { return m_hasFocus; }

    /// @}

    //==============================================================================
    /// @name Tooltip
    /// @{

    /** Set tooltip text. */
    void setTooltipText(const juce::String& tooltip);
    juce::String getTooltipText() const { return m_tooltipText; }

    /// @}

    //==============================================================================
    /// @name Callbacks
    /// @{

    /** Callback invoked on button click. */
    std::function<void()> onClick;

    /** Callback invoked on right-click (context menu). */
    std::function<void()> onRightClick;

    /// @}

    //==============================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void focusGained(FocusChangeType cause) override;
    void focusLost(FocusChangeType cause) override;
    bool keyPressed(const juce::KeyPress& key) override;

protected:
    //==============================================================================
    /**
     * @brief Paint the button content (override in subclasses).
     *
     * Called after background is drawn. Implement in subclasses to draw
     * icons, text, or other content.
     *
     * @param g Graphics context
     * @param bounds Content bounds (after padding)
     * @param foregroundColor Current foreground color (animated)
     */
    virtual void paintContent(juce::Graphics& g,
                              juce::Rectangle<float> bounds,
                              juce::Colour foregroundColor);

    /** Start the animation timer. */
    void startAnimation();

    /** Stop the animation timer. */
    void stopAnimation();

    /** Called every animation frame. */
    virtual void animationTick();

    //==============================================================================
    // Animation state
    float m_hoverOpacity = 0.0f;    // 0.0 = not hovered, 1.0 = fully hovered
    float m_pressOpacity = 0.0f;    // 0.0 = not pressed, 1.0 = fully pressed
    float m_focusOpacity = 0.0f;    // 0.0 = not focused, 1.0 = fully focused
    float m_loadingAngle = 0.0f;    // Loading spinner rotation

private:
    //==============================================================================
    class AnimationTimer;
    std::unique_ptr<AnimationTimer> m_animTimer;

    ButtonStyle m_style = ButtonStyle::Primary;
    ButtonSize m_size = ButtonSize::Medium;
    bool m_isDarkTheme = false;
    bool m_isEnabled = true;
    bool m_isLoading = false;
    bool m_isHovered = false;
    bool m_isPressed = false;
    bool m_hasFocus = false;
    bool m_hasCustomColors = false;
    ButtonColors m_customColors;
    juce::String m_tooltipText;

    double m_lastAnimTime = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Button)
};

} // namespace shmui
