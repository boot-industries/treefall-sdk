/*
  ==============================================================================

    ClipButton.cpp
    Created: shmui Button System

    Clip button implementation with state machine.

  ==============================================================================
*/

#include "ClipButton.h"
#include <algorithm>

namespace shmui
{

//==============================================================================
ClipButton::ClipButton(int buttonIndex)
    : m_buttonIndex(buttonIndex)
{
    setStyle(ButtonStyle::Ghost);

    // Pre-register the built-in indicators (hidden until toggled). Downstream
    // apps append their own (choke, lock, FX, trim, …) via setIndicator().
    m_indicators.push_back({ "loop",    IconType::Loop,       juce::Colours::white.withAlpha(0.6f), BadgeSlot::TopRight, false });
    m_indicators.push_back({ "fadeIn",  IconType::VolumeLow,  juce::Colours::white.withAlpha(0.6f), BadgeSlot::TopRight, false });
    m_indicators.push_back({ "fadeOut", IconType::VolumeMute, juce::Colours::white.withAlpha(0.6f), BadgeSlot::TopRight, false });

    // Override click handlers
    onClick = [this] { handleClipClick(); };
    onRightClick = [this] { handleClipRightClick(); };
}

//==============================================================================
void ClipButton::setClipState(State newState)
{
    if (m_clipState != newState)
    {
        m_clipState = newState;
        m_stateTransition = 0.0f; // Reset transition animation

        // Start playing pulse animation
        if (newState == State::Playing)
        {
            m_playingPulse = 0.0f;
            startAnimation();
        }

        repaint();
    }
}

void ClipButton::setClipName(const juce::String& name)
{
    if (m_clipName != name)
    {
        m_clipName = name;
        repaint();
    }
}

void ClipButton::setClipColor(juce::Colour color)
{
    if (m_clipColor != color)
    {
        m_clipColor = color;
        repaint();
    }
}

void ClipButton::setClipDuration(double durationSeconds)
{
    if (m_durationSeconds != durationSeconds)
    {
        m_durationSeconds = durationSeconds;
        repaint();
    }
}

void ClipButton::setKeyboardShortcut(const juce::String& shortcut)
{
    if (m_keyboardShortcut != shortcut)
    {
        m_keyboardShortcut = shortcut;
        repaint();
    }
}

void ClipButton::clearClip()
{
    m_clipName = "";
    m_clipColor = tokens::clip::empty();  // --clip-empty (unlit well)
    m_durationSeconds = 0.0;
    m_keyboardShortcut = "";
    m_playbackProgress = 0.0f;
    // Hide every indicator (built-in + app-registered) without unregistering.
    for (auto& ind : m_indicators)
        ind.visible = false;
    m_clipState = State::Empty;
    repaint();
}

void ClipButton::setPlaybackProgress(float progress)
{
    const float clampedProgress = juce::jlimit(0.0f, 1.0f, progress);
    if (std::abs(m_playbackProgress - clampedProgress) > 0.001f)
    {
        m_playbackProgress = clampedProgress;
        if (m_clipState == State::Playing)
            repaint();
    }
}

void ClipButton::setLoopEnabled(bool enabled)
{
    setIndicatorVisible("loop", enabled);
}

void ClipButton::setFadeInEnabled(bool enabled)
{
    setIndicatorVisible("fadeIn", enabled);
}

void ClipButton::setFadeOutEnabled(bool enabled)
{
    setIndicatorVisible("fadeOut", enabled);
}

//==============================================================================
ClipIndicator* ClipButton::findIndicator(const juce::String& name)
{
    for (auto& ind : m_indicators)
        if (ind.name == name)
            return &ind;
    return nullptr;
}

const ClipIndicator* ClipButton::findIndicator(const juce::String& name) const
{
    for (const auto& ind : m_indicators)
        if (ind.name == name)
            return &ind;
    return nullptr;
}

void ClipButton::setIndicator(const ClipIndicator& indicator)
{
    if (auto* existing = findIndicator(indicator.name))
        *existing = indicator;         // update in place, preserving draw order
    else
        m_indicators.push_back(indicator);
    repaint();
}

void ClipButton::setIndicator(const juce::String& name, IconType icon, juce::Colour color,
                              BadgeSlot slot, bool visible)
{
    setIndicator({ name, icon, color, slot, visible });
}

void ClipButton::setIndicatorVisible(const juce::String& name, bool visible)
{
    if (auto* ind = findIndicator(name))
    {
        if (ind->visible != visible)
        {
            ind->visible = visible;
            repaint();
        }
    }
}

bool ClipButton::isIndicatorVisible(const juce::String& name) const
{
    const auto* ind = findIndicator(name);
    return ind != nullptr && ind->visible;
}

bool ClipButton::hasIndicator(const juce::String& name) const
{
    return findIndicator(name) != nullptr;
}

void ClipButton::removeIndicator(const juce::String& name)
{
    const auto before = m_indicators.size();
    m_indicators.erase(
        std::remove_if(m_indicators.begin(), m_indicators.end(),
                       [&](const ClipIndicator& ind) { return ind.name == name; }),
        m_indicators.end());
    if (m_indicators.size() != before)
        repaint();
}

void ClipButton::clearIndicators()
{
    if (!m_indicators.empty())
    {
        m_indicators.clear();
        repaint();
    }
}

void ClipButton::setIsPlaybox(bool isPlaybox)
{
    if (m_isPlaybox != isPlaybox)
    {
        m_isPlaybox = isPlaybox;
        repaint();
    }
}

//==============================================================================
void ClipButton::setShowNumberWhenLoaded(bool shouldShow)
{
    if (m_showNumberWhenLoaded != shouldShow)
    {
        m_showNumberWhenLoaded = shouldShow;
        repaint();
    }
}

void ClipButton::setNumberSlot(BadgeSlot slot)
{
    if (m_numberSlot != slot)
    {
        m_numberSlot = slot;
        if (m_showNumberWhenLoaded)
            repaint();
    }
}

juce::String ClipButton::getDisplayNumber() const
{
    if (displayNumberProvider)
        return displayNumberProvider(m_buttonIndex);
    return juce::String(m_buttonIndex + 1);
}

void ClipButton::refreshDisplayNumber()
{
    repaint();
}

//==============================================================================
void ClipButton::paintContent(juce::Graphics& g,
                              juce::Rectangle<float> bounds,
                              juce::Colour foregroundColor)
{
    juce::ignoreUnused(foregroundColor);

    // Background based on state
    juce::Colour bgColor;
    switch (m_clipState)
    {
        case State::Empty:
            bgColor = tokens::clip::empty(); // --clip-empty
            break;
        case State::Loaded:
            bgColor = m_clipColor.withAlpha(0.8f);
            break;
        case State::Playing:
            bgColor = m_clipColor.brighter(0.2f + m_playingPulse * 0.1f);
            break;
        case State::Stopping:
            bgColor = m_clipColor.withAlpha(0.5f);
            break;
    }

    // Draw background
    g.setColour(bgColor);
    g.fillRoundedRectangle(bounds, CORNER_RADIUS);

    // Draw playing border
    if (m_clipState == State::Playing)
    {
        g.setColour(juce::Colours::white.withAlpha(0.8f + m_playingPulse * 0.2f));
        g.drawRoundedRectangle(bounds.reduced(1.0f), CORNER_RADIUS, BORDER_THICKNESS);
    }

    // Draw playbox indicator (selection outline)
    if (m_isPlaybox)
    {
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), CORNER_RADIUS + 1, 1.0f);
    }

    // Draw content based on state
    if (m_clipState != State::Empty)
    {
        // Draw clip name
        if (m_clipName.isNotEmpty())
        {
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(11.0f));

            auto textBounds = bounds.reduced(PADDING);
            g.drawText(m_clipName, textBounds, juce::Justification::centred, true);
        }

        // Draw HUD (shortcut, duration)
        drawClipHUD(g, bounds);

        // Draw registered status indicators (loop/fade + any app badges)
        drawStatusIcons(g, bounds);

        // Optional persistent clip number for loaded/playing/stopping pads
        if (m_showNumberWhenLoaded)
            drawNumberLabel(g, bounds);

        // Draw progress indicator when playing
        if (m_clipState == State::Playing)
        {
            drawProgressIndicator(g, bounds);
        }
    }
    else
    {
        // Draw display number for empty state (host provider, else index + 1)
        const auto number = getDisplayNumber();
        if (number.isNotEmpty())
        {
            g.setColour(juce::Colours::grey.withAlpha(0.3f));
            g.setFont(juce::Font(10.0f));
            g.drawText(number, bounds, juce::Justification::centred, false);
        }
    }
}

void ClipButton::animationTick()
{
    Button::animationTick();

    // Pulse animation for playing state
    if (m_clipState == State::Playing)
    {
        m_playingPulse = std::sin(juce::Time::getMillisecondCounterHiRes() / 1000.0 * juce::MathConstants<double>::twoPi * 0.5f) * 0.5f + 0.5f;
        repaint();
    }

    // State transition animation
    if (m_stateTransition < 1.0f)
    {
        m_stateTransition = Interpolation::smooth(m_stateTransition, 1.0f, Interpolation::kTransitionStep);
        repaint();
    }
}

//==============================================================================
void ClipButton::handleClipClick()
{
    if (onClipClick)
        onClipClick(m_buttonIndex);
}

void ClipButton::handleClipRightClick()
{
    if (onClipRightClick)
        onClipRightClick(m_buttonIndex);
}

void ClipButton::drawClipHUD(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    auto hudBounds = bounds.reduced(PADDING);

    // Keyboard shortcut (top-left)
    if (m_keyboardShortcut.isNotEmpty())
    {
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.drawText(m_keyboardShortcut,
                   hudBounds.removeFromTop(12.0f),
                   juce::Justification::topLeft, false);
    }

    // Duration (bottom-right)
    if (m_durationSeconds > 0.0)
    {
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.setFont(juce::Font(8.0f));
        g.drawText(formatDuration(m_durationSeconds),
                   bounds.reduced(PADDING),
                   juce::Justification::bottomRight, false);
    }
}

void ClipButton::drawStatusIcons(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    // Per-slot layout cursors. Icons stack horizontally, growing inward from the
    // slot corner: top slots grow downward-from-top, bottom slots upward-from-bottom.
    const float top = bounds.getY() + PADDING;
    const float bottom = bounds.getBottom() - PADDING - ICON_SIZE;
    float leftTop = bounds.getX() + PADDING;
    float rightTop = bounds.getRight() - PADDING - ICON_SIZE;
    float leftBottom = bounds.getX() + PADDING;
    float rightBottom = bounds.getRight() - PADDING - ICON_SIZE;
    const float step = ICON_SIZE + 2.0f;

    for (const auto& ind : m_indicators)
    {
        if (!ind.visible)
            continue;

        float x = 0.0f, y = 0.0f;
        switch (ind.slot)
        {
            case BadgeSlot::TopLeft:     x = leftTop;     y = top;    leftTop += step;     break;
            case BadgeSlot::TopRight:    x = rightTop;    y = top;    rightTop -= step;    break;
            case BadgeSlot::BottomLeft:  x = leftBottom;  y = bottom; leftBottom += step;  break;
            case BadgeSlot::BottomRight: x = rightBottom; y = bottom; rightBottom -= step; break;
        }

        auto iconBounds = juce::Rectangle<float>(x, y, ICON_SIZE, ICON_SIZE);
        Icons::drawIcon(g, ind.icon, iconBounds, ind.color);
    }
}

void ClipButton::drawNumberLabel(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto number = getDisplayNumber();
    if (number.isEmpty())
        return;

    // Small persistent number tucked into a corner, clear of the centred clip name.
    constexpr float kLabelW = 18.0f;
    constexpr float kLabelH = 12.0f;
    auto inner = bounds.reduced(PADDING);

    float x = inner.getX();
    float y = inner.getY();
    juce::Justification just = juce::Justification::topLeft;

    switch (m_numberSlot)
    {
        case BadgeSlot::TopLeft:
            x = inner.getX();               y = inner.getY();
            just = juce::Justification::topLeft;                 break;
        case BadgeSlot::TopRight:
            x = inner.getRight() - kLabelW; y = inner.getY();
            just = juce::Justification::topRight;                break;
        case BadgeSlot::BottomLeft:
            x = inner.getX();               y = inner.getBottom() - kLabelH;
            just = juce::Justification::bottomLeft;              break;
        case BadgeSlot::BottomRight:
            x = inner.getRight() - kLabelW; y = inner.getBottom() - kLabelH;
            just = juce::Justification::bottomRight;             break;
    }

    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.setFont(juce::Font(9.0f, juce::Font::bold));
    g.drawText(number, juce::Rectangle<float>(x, y, kLabelW, kLabelH), just, false);
}

void ClipButton::drawProgressIndicator(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    // Progress bar at bottom
    auto progressBounds = bounds;
    progressBounds = progressBounds.removeFromBottom(3.0f);

    // Background
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.fillRect(progressBounds);

    // Progress fill
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.fillRect(progressBounds.removeFromLeft(progressBounds.getWidth() * m_playbackProgress));
}

juce::String ClipButton::formatDuration(double seconds) const
{
    if (seconds < 60.0)
    {
        return juce::String(seconds, 1) + "s";
    }
    else
    {
        int mins = static_cast<int>(seconds / 60.0);
        int secs = static_cast<int>(std::fmod(seconds, 60.0));
        return juce::String(mins) + ":" + juce::String(secs).paddedLeft('0', 2);
    }
}

} // namespace shmui
