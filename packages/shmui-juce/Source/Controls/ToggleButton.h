/*
  ==============================================================================

    ToggleButton.h
    Created: shmui Button System

    Stateful toggle button with on/off state.

    Usage:
      shmui::ToggleButton btn(shmui::IconType::Mute, shmui::IconType::VolumeMid);
      btn.setToggled(true);
      btn.onToggle = [this](bool on) { handleMute(on); };

  ==============================================================================
*/

#pragma once

#include "Button.h"
#include "../Icons/Icons.h"

namespace shmui
{

//==============================================================================
/**
 * @brief Stateful toggle button with on/off state.
 *
 * Displays different icons or colors for on/off states.
 * Ideal for mute/solo, loop, bypass, and other binary toggles.
 */
class ToggleButton : public Button
{
public:
    //==============================================================================
    /** Create a toggle button with on/off icons. */
    ToggleButton(IconType iconOff, IconType iconOn);

    /** Create a toggle with same icon for both states (color indicates state). */
    explicit ToggleButton(IconType icon);

    ~ToggleButton() override = default;

    //==============================================================================
    /// @name Toggle State
    /// @{

    /** Set the toggle state. */
    void setToggled(bool toggled);
    bool isToggled() const { return m_isToggled; }

    /** Toggle the state. */
    void toggle() { setToggled(!m_isToggled); }

    /// @}

    //==============================================================================
    /// @name Icons
    /// @{

    /** Set icons for off and on states. */
    void setIcons(IconType iconOff, IconType iconOn);

    /** Get the current icon (based on toggle state). */
    IconType getCurrentIcon() const { return m_isToggled ? m_iconOn : m_iconOff; }

    /// @}

    //==============================================================================
    /// @name Colors
    /// @{

    /** Set custom color for the "on" state. */
    void setOnColor(juce::Colour color);
    void clearOnColor();

    /// @}

    //==============================================================================
    /// @name Callbacks
    /// @{

    /** Callback invoked when toggle state changes. */
    std::function<void(bool isOn)> onToggle;

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

    IconType m_iconOff;
    IconType m_iconOn;
    bool m_isToggled = false;
    bool m_hasOnColor = false;
    juce::Colour m_onColor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToggleButton)
};

} // namespace shmui
