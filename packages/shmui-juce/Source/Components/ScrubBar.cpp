/*
  ==============================================================================

    ScrubBar.cpp
    Shmui - Audio/AI-focused UI component library

  ==============================================================================
*/

#include "ScrubBar.h"

namespace shmui
{

//==============================================================================
ScrubBar::ScrubBar()
{
    setOpaque (false);
}

ScrubBar::~ScrubBar() = default;

//==============================================================================
void ScrubBar::setPosition (double newPosition)
{
    newPosition = juce::jlimit (0.0, 1.0, newPosition);

    if (position != newPosition)
    {
        position = newPosition;
        repaint();
    }
}

void ScrubBar::setCurrentTime (double timeInSeconds)
{
    if (currentTime != timeInSeconds)
    {
        currentTime = timeInSeconds;

        // Update position based on duration
        if (duration > 0.0)
            setPosition (currentTime / duration);
        else
            setPosition (0.0);
    }
}

void ScrubBar::setDuration (double durationInSeconds)
{
    if (duration != durationInSeconds)
    {
        duration = durationInSeconds;

        // Recalculate position
        if (duration > 0.0)
            setPosition (currentTime / duration);
        else
            setPosition (0.0);
    }
}

void ScrubBar::setThumbVisible (bool shouldBeVisible)
{
    if (showThumb != shouldBeVisible)
    {
        showThumb = shouldBeVisible;
        repaint();
    }
}

//==============================================================================
void ScrubBar::addListener (Listener* listener)
{
    listeners.add (listener);
}

void ScrubBar::removeListener (Listener* listener)
{
    listeners.remove (listener);
}

void ScrubBar::setStyle (const Style& newStyle)
{
    style = newStyle;
    repaint();
}

//==============================================================================
void ScrubBar::paint (juce::Graphics& g)
{
    auto trackBounds = getTrackBounds();

    // Draw track background
    g.setColour (style.trackColor);
    g.fillRoundedRectangle (trackBounds, style.cornerRadius);

    // Draw progress
    if (position > 0.0)
    {
        float progressWidth = static_cast<float> (trackBounds.getWidth() * position);
        auto progressBounds = trackBounds.withWidth (progressWidth);

        g.setColour (style.progressColor);
        g.fillRoundedRectangle (progressBounds, style.cornerRadius);
    }

    // Draw thumb
    if (showThumb && (isHovering || isDragging))
    {
        auto thumbBounds = getThumbBounds();

        // Border
        g.setColour (style.thumbBorderColor);
        g.fillEllipse (thumbBounds);

        // Inner fill
        auto innerBounds = thumbBounds.reduced (style.thumbBorderWidth);
        g.setColour (style.thumbColor);
        g.fillEllipse (innerBounds);
    }
}

void ScrubBar::resized()
{
    // Layout is computed dynamically
}

//==============================================================================
void ScrubBar::mouseDown (const juce::MouseEvent& event)
{
    isDragging = true;
    listeners.call ([] (Listener& l) { l.scrubStarted(); });
    updatePositionFromMouse (event);
}

void ScrubBar::mouseDrag (const juce::MouseEvent& event)
{
    if (isDragging)
    {
        updatePositionFromMouse (event);
    }
}

void ScrubBar::mouseUp (const juce::MouseEvent& event)
{
    if (isDragging)
    {
        isDragging = false;
        listeners.call ([] (Listener& l) { l.scrubEnded(); });

        // Final seek
        if (duration > 0.0)
        {
            double seekTime = position * duration;
            listeners.call ([seekTime] (Listener& l) { l.seekRequested (seekTime); });
        }
    }
}

void ScrubBar::mouseMove (const juce::MouseEvent& event)
{
    bool wasHovering = isHovering;
    isHovering = true;
    setMouseCursor (juce::MouseCursor::PointingHandCursor);

    if (! wasHovering)
        repaint();
}

void ScrubBar::mouseExit (const juce::MouseEvent& event)
{
    isHovering = false;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

//==============================================================================
double ScrubBar::xToPosition (float x) const
{
    auto trackBounds = getTrackBounds();
    float relativeX = x - trackBounds.getX();
    double pos = relativeX / trackBounds.getWidth();
    return juce::jlimit (0.0, 1.0, pos);
}

float ScrubBar::positionToX (double pos) const
{
    auto trackBounds = getTrackBounds();
    return static_cast<float> (trackBounds.getX() + trackBounds.getWidth() * pos);
}

juce::Rectangle<float> ScrubBar::getTrackBounds() const
{
    auto bounds = getLocalBounds().toFloat();
    float y = (bounds.getHeight() - style.trackHeight) * 0.5f;

    return juce::Rectangle<float> (bounds.getX(),
                                   y,
                                   bounds.getWidth(),
                                   style.trackHeight);
}

juce::Rectangle<float> ScrubBar::getThumbBounds() const
{
    auto trackBounds = getTrackBounds();
    float x = positionToX (position);
    float y = trackBounds.getCentreY();

    return juce::Rectangle<float> (x - style.thumbSize * 0.5f,
                                   y - style.thumbSize * 0.5f,
                                   style.thumbSize,
                                   style.thumbSize);
}

void ScrubBar::updatePositionFromMouse (const juce::MouseEvent& event)
{
    double newPosition = xToPosition (event.position.x);

    if (position != newPosition)
    {
        position = newPosition;
        currentTime = position * duration;
        repaint();

        listeners.call ([newPosition] (Listener& l) { l.scrubPositionChanged (newPosition); });

        // Also notify seek during drag for real-time feedback
        if (duration > 0.0)
        {
            double seekTime = position * duration;
            listeners.call ([seekTime] (Listener& l) { l.seekRequested (seekTime); });
        }
    }
}

} // namespace shmui
