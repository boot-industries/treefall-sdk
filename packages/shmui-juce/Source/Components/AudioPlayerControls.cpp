/*
  ==============================================================================

    AudioPlayerControls.cpp
    Shmui - Audio/AI-focused UI component library

  ==============================================================================
*/

#include "AudioPlayerControls.h"

namespace shmui
{

//==============================================================================
AudioPlayerControls::AudioPlayerControls()
{
    setOpaque (false);
}

AudioPlayerControls::~AudioPlayerControls()
{
    stopTimer();
}

//==============================================================================
void AudioPlayerControls::setCurrentTime (double timeInSeconds)
{
    if (currentTime != timeInSeconds)
    {
        currentTime = timeInSeconds;
        repaint();
    }
}

void AudioPlayerControls::setDuration (double durationInSeconds)
{
    if (duration != durationInSeconds)
    {
        duration = durationInSeconds;
        repaint();
    }
}

void AudioPlayerControls::setPlaying (bool shouldBePlaying)
{
    if (playing != shouldBePlaying)
    {
        playing = shouldBePlaying;
        repaint();
        listeners.call ([shouldBePlaying] (Listener& l) { l.playStateChanged (shouldBePlaying); });
    }
}

void AudioPlayerControls::setPlaybackRate (double rate)
{
    rate = juce::jlimit (0.25, 2.0, rate);
    if (playbackRate != rate)
    {
        playbackRate = rate;
        repaint();
        listeners.call ([rate] (Listener& l) { l.playbackRateChanged (rate); });
    }
}

void AudioPlayerControls::setBuffering (bool isBuffering)
{
    if (buffering != isBuffering)
    {
        buffering = isBuffering;
        if (buffering)
            startTimerHz (60);
        else
            stopTimer();
        repaint();
    }
}

//==============================================================================
void AudioPlayerControls::addListener (Listener* listener)
{
    listeners.add (listener);
}

void AudioPlayerControls::removeListener (Listener* listener)
{
    listeners.remove (listener);
}

void AudioPlayerControls::setStyle (const Style& newStyle)
{
    style = newStyle;
    repaint();
}

//==============================================================================
void AudioPlayerControls::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (style.padding);

    // Background (optional)
    if (style.backgroundColor.getAlpha() > 0)
    {
        g.setColour (style.backgroundColor);
        g.fillRoundedRectangle (bounds, style.cornerRadius);
    }

    // Play/Pause button
    auto playBounds = getPlayButtonBounds();
    auto buttonColour = playButtonHovered ? style.buttonHoverColor : style.buttonColor;
    if (playButtonPressed)
        buttonColour = buttonColour.darker (0.2f);

    g.setColour (buttonColour);
    g.fillRoundedRectangle (playBounds, style.cornerRadius);

    // Icon or spinner
    auto iconBounds = playBounds.reduced (playBounds.getWidth() * 0.25f);
    g.setColour (style.iconColor);

    if (buffering && playing)
    {
        drawSpinner (g, iconBounds);
    }
    else if (playing)
    {
        drawPauseIcon (g, iconBounds);
    }
    else
    {
        drawPlayIcon (g, iconBounds);
    }

    // Time display
    auto timeBounds = getTimeDisplayBounds();
    g.setColour (style.textColor);
    g.setFont (style.fontSize);

    juce::String timeText = formatTime (currentTime) + " / " + formatTime (duration);
    g.drawText (timeText, timeBounds, juce::Justification::centred, true);

    // Speed button
    auto speedBounds = getSpeedButtonBounds();
    if (speedButtonHovered)
    {
        g.setColour (style.buttonColor.withAlpha (0.1f));
        g.fillRoundedRectangle (speedBounds, style.cornerRadius * 0.5f);
    }

    g.setColour (style.textColor);
    g.setFont (style.fontSize * 0.9f);

    juce::String speedText;
    if (playbackRate == 1.0)
        speedText = "1x";
    else
        speedText = juce::String (playbackRate, 2) + "x";

    g.drawText (speedText, speedBounds, juce::Justification::centred, true);
}

void AudioPlayerControls::resized()
{
    // Layout is computed dynamically in paint/getBounds methods
}

//==============================================================================
void AudioPlayerControls::mouseDown (const juce::MouseEvent& event)
{
    auto playBounds = getPlayButtonBounds();
    auto speedBounds = getSpeedButtonBounds();

    if (playBounds.contains (event.position))
    {
        playButtonPressed = true;
        repaint();
    }
    else if (speedBounds.contains (event.position))
    {
        showSpeedMenu();
    }
}

void AudioPlayerControls::mouseUp (const juce::MouseEvent& event)
{
    if (playButtonPressed)
    {
        playButtonPressed = false;
        auto playBounds = getPlayButtonBounds();

        if (playBounds.contains (event.position))
        {
            setPlaying (! playing);
        }

        repaint();
    }
}

void AudioPlayerControls::mouseMove (const juce::MouseEvent& event)
{
    auto playBounds = getPlayButtonBounds();
    auto speedBounds = getSpeedButtonBounds();

    bool newPlayHover = playBounds.contains (event.position);
    bool newSpeedHover = speedBounds.contains (event.position);

    if (newPlayHover != playButtonHovered || newSpeedHover != speedButtonHovered)
    {
        playButtonHovered = newPlayHover;
        speedButtonHovered = newSpeedHover;

        if (playButtonHovered || speedButtonHovered)
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        else
            setMouseCursor (juce::MouseCursor::NormalCursor);

        repaint();
    }
}

void AudioPlayerControls::mouseExit (const juce::MouseEvent& event)
{
    playButtonHovered = false;
    speedButtonHovered = false;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

void AudioPlayerControls::timerCallback()
{
    spinnerAngle += 0.15f;
    if (spinnerAngle > juce::MathConstants<float>::twoPi)
        spinnerAngle -= juce::MathConstants<float>::twoPi;
    repaint();
}

//==============================================================================
juce::String AudioPlayerControls::formatTime (double seconds)
{
    if (std::isnan (seconds) || std::isinf (seconds) || seconds < 0)
        return "--:--";

    int totalSeconds = static_cast<int> (seconds);
    int hrs = totalSeconds / 3600;
    int mins = (totalSeconds % 3600) / 60;
    int secs = totalSeconds % 60;

    if (hrs > 0)
    {
        return juce::String::formatted ("%d:%02d:%02d", hrs, mins, secs);
    }
    else
    {
        return juce::String::formatted ("%d:%02d", mins, secs);
    }
}

juce::Rectangle<float> AudioPlayerControls::getPlayButtonBounds() const
{
    auto bounds = getLocalBounds().toFloat().reduced (style.padding);
    return juce::Rectangle<float> (bounds.getX(),
                                   bounds.getCentreY() - style.buttonSize * 0.5f,
                                   style.buttonSize,
                                   style.buttonSize);
}

juce::Rectangle<float> AudioPlayerControls::getTimeDisplayBounds() const
{
    auto bounds = getLocalBounds().toFloat().reduced (style.padding);
    auto playBounds = getPlayButtonBounds();
    auto speedBounds = getSpeedButtonBounds();

    float left = playBounds.getRight() + style.padding;
    float right = speedBounds.getX() - style.padding;

    return juce::Rectangle<float> (left,
                                   bounds.getCentreY() - style.fontSize * 0.75f,
                                   right - left,
                                   style.fontSize * 1.5f);
}

juce::Rectangle<float> AudioPlayerControls::getSpeedButtonBounds() const
{
    auto bounds = getLocalBounds().toFloat().reduced (style.padding);
    float speedWidth = style.fontSize * 3.0f;

    return juce::Rectangle<float> (bounds.getRight() - speedWidth,
                                   bounds.getCentreY() - style.buttonSize * 0.4f,
                                   speedWidth,
                                   style.buttonSize * 0.8f);
}

void AudioPlayerControls::drawPlayIcon (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    juce::Path triangle;
    float x = bounds.getX();
    float y = bounds.getY();
    float w = bounds.getWidth();
    float h = bounds.getHeight();

    // Offset slightly to the right for visual balance
    triangle.addTriangle (x + w * 0.15f, y,
                          x + w * 0.15f, y + h,
                          x + w, y + h * 0.5f);
    g.fillPath (triangle);
}

void AudioPlayerControls::drawPauseIcon (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    float barWidth = bounds.getWidth() * 0.3f;
    float gap = bounds.getWidth() * 0.2f;

    auto leftBar = bounds.withWidth (barWidth);
    auto rightBar = bounds.withWidth (barWidth).withX (bounds.getX() + barWidth + gap);

    g.fillRoundedRectangle (leftBar, 2.0f);
    g.fillRoundedRectangle (rightBar, 2.0f);
}

void AudioPlayerControls::drawSpinner (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    float strokeWidth = bounds.getWidth() * 0.15f;
    auto centre = bounds.getCentre();
    float radius = (bounds.getWidth() - strokeWidth) * 0.5f;

    // Background arc
    g.setColour (style.iconColor.withAlpha (0.3f));
    juce::Path bgArc;
    bgArc.addCentredArc (centre.x, centre.y, radius, radius,
                         0.0f, 0.0f, juce::MathConstants<float>::twoPi, true);
    g.strokePath (bgArc, juce::PathStrokeType (strokeWidth));

    // Animated arc
    g.setColour (style.iconColor);
    juce::Path fgArc;
    float arcLength = juce::MathConstants<float>::pi * 0.75f;
    fgArc.addCentredArc (centre.x, centre.y, radius, radius,
                         0.0f, spinnerAngle, spinnerAngle + arcLength, true);
    g.strokePath (fgArc, juce::PathStrokeType (strokeWidth, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
}

void AudioPlayerControls::showSpeedMenu()
{
    juce::PopupMenu menu;

    for (int i = 0; i < numPlaybackSpeeds; ++i)
    {
        double speed = playbackSpeeds[i];
        juce::String text = (speed == 1.0) ? "Normal" : juce::String (speed, 2) + "x";

        menu.addItem (i + 1, text, true, juce::approximatelyEqual (playbackRate, speed));
    }

    menu.showMenuAsync (juce::PopupMenu::Options()
                           .withTargetComponent (this)
                           .withTargetScreenArea (getSpeedButtonBounds().toNearestInt().translated (getScreenX(), getScreenY())),
                        [this] (int result)
                        {
                            if (result > 0 && result <= numPlaybackSpeeds)
                            {
                                setPlaybackRate (playbackSpeeds[result - 1]);
                            }
                        });
}

} // namespace shmui
