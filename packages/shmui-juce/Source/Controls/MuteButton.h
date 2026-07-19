/*
  ==============================================================================

    MuteButton.h
    Created: shmui Button System

    Mute/Solo toggle button for mixer controls.

    Usage:
      shmui::MuteButton muteBtn(shmui::MuteButton::Type::Mute);
      muteBtn.setActive(true);  // Muted
      muteBtn.onToggle = [this](bool active) { handleMute(active); };

  ==============================================================================
*/

#pragma once

#include "Button.h"
#include "../Icons/Icons.h"

namespace shmui
{

//==============================================================================
/**
 * @brief Mute/Solo toggle button for mixer controls.
 *
 * Displays M or S icon with appropriate coloring when active.
 * Standard mixer channel strip button.
 */
class MuteButton : public Button
{
public:
    //==============================================================================
    /**
     * @brief Mute button types.
     */
    enum class Type
    {
        Mute,   ///< Mute button (M icon)
        Solo,   ///< Solo button (S icon)
        Bypass  ///< Bypass button (circle-slash icon)
    };

    //==============================================================================
    /** Create a mute/solo button of the specified type. */
    explicit MuteButton(Type type);

    ~MuteButton() override = default;

    //==============================================================================
    /// @name Type
    /// @{

    /** Set the button type. */
    void setType(Type type);
    Type getType() const { return m_type; }

    /// @}

    //==============================================================================
    /// @name State
    /// @{

    /** Set active state (muted, soloed, or bypassed). */
    void setActive(bool active);
    bool isActive() const { return m_isActive; }

    /** Toggle the active state. */
    void toggle() { setActive(!m_isActive); }

    /// @}

    //==============================================================================
    /// @name Callbacks
    /// @{

    /** Callback invoked when active state changes. */
    std::function<void(bool active)> onToggle;

    /// @}

    //==============================================================================
    /** Get preferred size (square based on ButtonSize). */
    int getPreferredSize() const;

protected:
    //==============================================================================
    void paintContent(juce::Graphics& g,
                      juce::Rectangle<float> bounds,
                      juce::Colour foregroundColor) override;

private:
    void handleClick();
    IconType getCurrentIcon() const;
    juce::Colour getActiveColor() const;

    Type m_type;
    bool m_isActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MuteButton)
};

} // namespace shmui
