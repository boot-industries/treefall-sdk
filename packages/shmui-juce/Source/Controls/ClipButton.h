/*
  ==============================================================================

    ClipButton.h
    Created: shmui Button System

    Clip trigger button with state machine (Empty → Loaded → Playing → Stopping).

    Based on OCC ClipButton implementation, generalized for reuse.

    Usage:
      shmui::ClipButton clipBtn(0);
      clipBtn.setClipName("Kick 01");
      clipBtn.setClipColor(juce::Colours::orange);
      clipBtn.setState(shmui::ClipButton::State::Loaded);
      clipBtn.onClick = [this](int idx) { handleClipTrigger(idx); };

  ==============================================================================
*/

#pragma once

#include "Button.h"
#include "../Icons/Icons.h"
#include "../Utils/DesignTokens.h"
#include "../Utils/Interpolation.h"
#include <vector>

namespace shmui
{

//==============================================================================
/**
 * @brief Corner / edge slot a clip indicator badge is drawn in.
 */
enum class BadgeSlot
{
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

//==============================================================================
/**
 * @brief A registered status indicator ("badge") on a ClipButton.
 *
 * Downstream apps register their own named indicators (icon + colour + slot)
 * without forking ClipButton — e.g. Clip Composer's choke, lock, FX, and trim
 * flags on top of the built-in loop / fade-in / fade-out.
 */
struct ClipIndicator
{
    juce::String name;                                 ///< Unique key (e.g. "choke")
    IconType icon = IconType::Info;                    ///< Glyph from the shmui icon set
    juce::Colour color = juce::Colours::white;         ///< Tint
    BadgeSlot slot = BadgeSlot::TopRight;              ///< Where it draws
    bool visible = false;                              ///< Current visibility
};

//==============================================================================
/**
 * @brief Clip trigger button with state machine.
 *
 * A stateful button for audio clip triggering with:
 * - State machine: Empty → Loaded → Playing → Stopping
 * - Waveform thumbnail preview (optional)
 * - Progress indicator during playback
 * - Keyboard shortcut display
 * - Color customization
 * - Right-click context menu support
 *
 * Commonly used in sample pads, clip launchers, and beat grids.
 */
class ClipButton : public Button
{
public:
    //==============================================================================
    /**
     * @brief Button states for visual feedback.
     */
    enum class State
    {
        Empty,   ///< No clip loaded (dark grey, no label)
        Loaded,  ///< Clip loaded, ready to play (colored, shows name)
        Playing, ///< Currently playing (bright border, progress animation)
        Stopping ///< Fade-out in progress (transitioning to Loaded)
    };

    //==============================================================================
    /** Create a clip button with the specified index. */
    explicit ClipButton(int buttonIndex);

    ~ClipButton() override = default;

    //==============================================================================
    /// @name Visual State
    /// @{

    /** Set the visual state of the button. */
    void setClipState(State newState);
    State getClipState() const { return m_clipState; }

    /// @}

    //==============================================================================
    /// @name Clip Data
    /// @{

    /** Set the display name for this clip. */
    void setClipName(const juce::String& name);
    juce::String getClipName() const { return m_clipName; }

    /** Set the visual color for this clip. */
    void setClipColor(juce::Colour color);
    juce::Colour getClipColor() const { return m_clipColor; }

    /** Set the clip duration for display. */
    void setClipDuration(double durationSeconds);
    double getClipDuration() const { return m_durationSeconds; }

    /** Set keyboard shortcut text to display. */
    void setKeyboardShortcut(const juce::String& shortcut);
    juce::String getKeyboardShortcut() const { return m_keyboardShortcut; }

    /** Clear all clip data and reset to Empty state. */
    void clearClip();

    /// @}

    //==============================================================================
    /// @name Playback
    /// @{

    /** Set playback progress for visual feedback (0.0 = start, 1.0 = end). */
    void setPlaybackProgress(float progress);
    float getPlaybackProgress() const { return m_playbackProgress; }

    /// @}

    //==============================================================================
    /// @name Status Flags (built-in convenience over the badge registry)
    /// @{

    /** Set loop indicator visibility. */
    void setLoopEnabled(bool enabled);
    bool isLoopEnabled() const { return isIndicatorVisible("loop"); }

    /** Set fade-in indicator visibility. */
    void setFadeInEnabled(bool enabled);
    bool isFadeInEnabled() const { return isIndicatorVisible("fadeIn"); }

    /** Set fade-out indicator visibility. */
    void setFadeOutEnabled(bool enabled);
    bool isFadeOutEnabled() const { return isIndicatorVisible("fadeOut"); }

    /// @}

    //==============================================================================
    /// @name Indicator / badge registry (extensible status flags)
    /// @{

    /**
     * @brief Register or replace a named indicator (icon + colour + slot).
     *
     * If an indicator with the same name exists it is updated in place
     * (preserving draw order). Downstream apps use this to add their own flags
     * (choke, lock, FX, trim, …) without subclassing.
     */
    void setIndicator(const ClipIndicator& indicator);

    /** Convenience: register/update an indicator from its parts. */
    void setIndicator(const juce::String& name, IconType icon, juce::Colour color,
                      BadgeSlot slot = BadgeSlot::TopRight, bool visible = true);

    /** Toggle an existing indicator's visibility (no-op if not registered). */
    void setIndicatorVisible(const juce::String& name, bool visible);

    /** @return true if the named indicator exists and is visible. */
    bool isIndicatorVisible(const juce::String& name) const;

    /** @return true if the named indicator is registered (visible or not). */
    bool hasIndicator(const juce::String& name) const;

    /** Remove a named indicator entirely. */
    void removeIndicator(const juce::String& name);

    /** Remove all indicators (including the built-in loop/fade badges). */
    void clearIndicators();

    /// @}

    //==============================================================================
    /// @name Identification
    /// @{

    /** Get the button index. */
    int getButtonIndex() const { return m_buttonIndex; }

    /** Set whether this is the currently selected/playbox button. */
    void setIsPlaybox(bool isPlaybox);
    bool getIsPlaybox() const { return m_isPlaybox; }

    /// @}

    //==============================================================================
    /// @name Display numbering / labeling
    /// @{

    /**
     * @brief App-supplied provider for the button's display number / label.
     *
     * By default a ClipButton labels itself `buttonIndex + 1` (drawn in the empty
     * state, hidden once a clip is loaded). Hosts that number clips *consecutively
     * across tabs* — e.g. Clip Composer, where the visible-grid density (and a
     * per-session stride) drive the number, not the raw button index — set this to
     * return their own label. Returning an empty string suppresses the number.
     *
     * This lets those hosts express cross-tab numbering without subclassing
     * ClipButton. Called from paint on the message thread; keep it cheap and
     * side-effect-free. Assign `nullptr` to restore the default `index + 1`.
     */
    std::function<juce::String(int buttonIndex)> displayNumberProvider;

    /**
     * @brief Show the display number even when a clip is loaded.
     *
     * Default `false` preserves the original look (number only in the empty state).
     * When `true`, the resolved display number is drawn as a small corner label in
     * the Loaded / Playing / Stopping states too — matching hosts that keep a
     * persistent clip number visible on populated pads.
     */
    void setShowNumberWhenLoaded(bool shouldShow);
    bool getShowNumberWhenLoaded() const { return m_showNumberWhenLoaded; }

    /**
     * @brief Corner slot for the loaded-state number label.
     *
     * Only used when @ref setShowNumberWhenLoaded is enabled. Defaults to
     * `BottomLeft` so it clears the top-corner status badges and the top-left
     * keyboard-shortcut HUD.
     */
    void setNumberSlot(BadgeSlot slot);
    BadgeSlot getNumberSlot() const { return m_numberSlot; }

    /** @return the resolved display number/label (provider result or `index + 1`). */
    juce::String getDisplayNumber() const;

    /**
     * @brief Force a repaint after the numbering context changes.
     *
     * The provider closure is host-owned, so ClipButton cannot observe when its
     * result changes (e.g. a tab switch alters the cross-tab stride). Hosts call
     * this to re-pull the label. No-op safe to call from the message thread.
     */
    void refreshDisplayNumber();

    /// @}

    //==============================================================================
    /// @name Callbacks
    /// @{

    /** Callback invoked on click with button index. */
    std::function<void(int buttonIndex)> onClipClick;

    /** Callback invoked on right-click with button index. */
    std::function<void(int buttonIndex)> onClipRightClick;

    /// @}

protected:
    //==============================================================================
    void paintContent(juce::Graphics& g,
                      juce::Rectangle<float> bounds,
                      juce::Colour foregroundColor) override;

    void animationTick() override;

private:
    //==============================================================================
    void handleClipClick();
    void handleClipRightClick();
    void drawClipHUD(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawStatusIcons(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawNumberLabel(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawProgressIndicator(juce::Graphics& g, juce::Rectangle<float> bounds);
    juce::String formatDuration(double seconds) const;

    //==============================================================================
    int m_buttonIndex;
    State m_clipState = State::Empty;
    juce::String m_clipName;
    juce::Colour m_clipColor = tokens::clip::empty();  // --clip-empty (unlit well)
    double m_durationSeconds = 0.0;
    juce::String m_keyboardShortcut;

    // Playback state
    float m_playbackProgress = 0.0f;

    // Status indicators (built-in loop/fadeIn/fadeOut are pre-registered in the
    // ctor; downstream apps append their own via setIndicator()).
    std::vector<ClipIndicator> m_indicators;
    ClipIndicator* findIndicator(const juce::String& name);
    const ClipIndicator* findIndicator(const juce::String& name) const;

    bool m_isPlaybox = false;

    // Display numbering (G12 — cross-tab numbering hook)
    bool m_showNumberWhenLoaded = false;
    BadgeSlot m_numberSlot = BadgeSlot::BottomLeft;

    // Animation
    float m_stateTransition = 0.0f;
    float m_playingPulse = 0.0f;

    // Visual constants
    static constexpr int BORDER_THICKNESS = 2;
    static constexpr int CORNER_RADIUS = 4;
    static constexpr int ICON_SIZE = 12;
    static constexpr int PADDING = 4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipButton)
};

} // namespace shmui
