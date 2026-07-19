/*
  ==============================================================================

    TransportButton.h
    Created: shmui Button System

    Transport control button (Play/Pause/Stop/Record) with standard icons.

    Usage:
      shmui::TransportButton playBtn(shmui::TransportButton::Type::PlayPause);
      playBtn.setPlaying(true);  // Changes icon from Play to Pause
      playBtn.onClick = [this] { handlePlayPause(); };

  ==============================================================================
*/

#pragma once

#include "Button.h"
#include "../Icons/Icons.h"

namespace shmui
{

//==============================================================================
/**
 * @brief Transport control button with standard audio transport icons.
 *
 * Provides Play, Pause, Stop, Record buttons with proper icons and
 * state-based icon switching (e.g., Play <-> Pause).
 */
class TransportButton : public Button
{
public:
    //==============================================================================
    /**
     * @brief Transport button types.
     */
    enum class Type
    {
        Play,       ///< Play icon only
        Pause,      ///< Pause icon only
        PlayPause,  ///< Toggles between Play/Pause based on isPlaying
        Stop,       ///< Stop icon
        Record,     ///< Record (circle) icon
        FastForward,///< Fast forward icon
        Rewind,     ///< Rewind icon
        SkipNext,   ///< Skip to next icon
        SkipPrev,   ///< Skip to previous icon
        Loop        ///< Loop icon (toggle)
    };

    //==============================================================================
    /** Create a transport button of the specified type. */
    explicit TransportButton(Type type);

    /** Create a transport button with type and style. */
    TransportButton(Type type, ButtonStyle style);

    ~TransportButton() override = default;

    //==============================================================================
    /// @name Type
    /// @{

    /** Set the button type. */
    void setType(Type type);
    Type getType() const { return m_type; }

    /// @}

    //==============================================================================
    /// @name State (for PlayPause type)
    /// @{

    /** Set playing state (for PlayPause type - determines icon). */
    void setPlaying(bool playing);
    bool isPlaying() const { return m_isPlaying; }

    /** Set recording state (for Record type). */
    void setRecording(bool recording);
    bool isRecording() const { return m_isRecording; }

    /** Set looping state (for Loop type). */
    void setLooping(bool looping);
    bool isLooping() const { return m_isLooping; }

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
    IconType getCurrentIcon() const;

    Type m_type;
    bool m_isPlaying = false;
    bool m_isRecording = false;
    bool m_isLooping = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportButton)
};

} // namespace shmui
