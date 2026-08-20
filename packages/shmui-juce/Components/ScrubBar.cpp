/*
  ==============================================================================

    ScrubBar.cpp
    Shmui - Audio/AI-focused UI component library

  ==============================================================================
*/

#include "ScrubBar.h"
#include <cmath>

namespace shmui {

//==============================================================================
ScrubBar::ScrubBar() {
  setOpaque(false);
  const auto& theme = defaultTheme();
  style.trackColor = theme.bgRaise;
  style.progressColor = theme.accent;
  style.thumbColor = theme.accent;
  style.thumbBorderColor = theme.fg;
  sanitizeStyle();
  addDefaultThemeListener(this);
}

ScrubBar::~ScrubBar() {
  removeDefaultThemeListener(this);
}

//==============================================================================
void ScrubBar::setPosition(double newPosition) {
  if (!requireMessageThread())
    return;

  newPosition = std::isfinite(newPosition) ? juce::jlimit(0.0, 1.0, newPosition) : 0.0;

  if (position != newPosition) {
    position = newPosition;
    currentTime = duration > 0.0 ? position * duration : 0.0;
    repaint();
  }
}

void ScrubBar::setCurrentTime(double timeInSeconds) {
  if (!requireMessageThread())
    return;

  const double safeTime = std::isfinite(timeInSeconds) ? juce::jmax(0.0, timeInSeconds) : 0.0;
  if (currentTime != safeTime) {
    currentTime = safeTime;
    position = duration > 0.0 ? juce::jlimit(0.0, 1.0, currentTime / duration) : 0.0;
    repaint();
  }
}

void ScrubBar::setDuration(double durationInSeconds) {
  if (!requireMessageThread())
    return;

  duration = std::isfinite(durationInSeconds) ? juce::jmax(0.0, durationInSeconds) : 0.0;
  if (duration > 0.0 && currentTime > duration)
    currentTime = duration;
  position = duration > 0.0 ? juce::jlimit(0.0, 1.0, currentTime / duration) : 0.0;
  repaint();
}

void ScrubBar::setThumbVisible(bool shouldBeVisible) {
  if (!requireMessageThread())
    return;

  if (showThumb != shouldBeVisible) {
    showThumb = shouldBeVisible;
    repaint();
  }
}

//==============================================================================
void ScrubBar::addListener(Listener* listener) {
  if (!requireMessageThread() || listener == nullptr)
    return;
  listeners->add(listener);
}

void ScrubBar::removeListener(Listener* listener) {
  if (!requireMessageThread())
    return;
  listeners->remove(listener);
}

void ScrubBar::setStyle(const Style& newStyle) {
  if (!requireMessageThread())
    return;
  style = newStyle;
  sanitizeStyle();
  customStyle = true;
  repaint();
}

void ScrubBar::defaultThemeChanged(const ShmuiTheme& theme) {
  if (!requireMessageThread() || customStyle)
    return;

  style.trackColor = theme.bgRaise;
  style.progressColor = theme.accent;
  style.thumbColor = theme.accent;
  style.thumbBorderColor = theme.fg;
  sanitizeStyle();
  repaint();
}

void ScrubBar::sanitizeStyle() {
  style.trackHeight =
      std::isfinite(style.trackHeight) ? juce::jlimit(1.0f, 1000.0f, style.trackHeight) : 8.0f;
  style.thumbSize =
      std::isfinite(style.thumbSize) ? juce::jlimit(1.0f, 1000.0f, style.thumbSize) : 16.0f;
  style.thumbBorderWidth = std::isfinite(style.thumbBorderWidth)
                               ? juce::jlimit(0.0f, style.thumbSize * 0.5f, style.thumbBorderWidth)
                               : 0.0f;
  style.cornerRadius =
      std::isfinite(style.cornerRadius) ? juce::jmax(0.0f, style.cornerRadius) : 0.0f;
}

//==============================================================================
void ScrubBar::paint(juce::Graphics& g) {
  auto trackBounds = getTrackBounds();

  // Draw track background
  g.setColour(style.trackColor);
  g.fillRoundedRectangle(trackBounds, style.cornerRadius);

  // Draw progress
  if (position > 0.0) {
    float progressWidth = static_cast<float>(trackBounds.getWidth() * position);
    auto progressBounds = trackBounds.withWidth(progressWidth);

    g.setColour(style.progressColor);
    g.fillRoundedRectangle(progressBounds, style.cornerRadius);
  }

  // Draw thumb
  if (showThumb && (isHovering || isDragging)) {
    auto thumbBounds = getThumbBounds();

    // Border
    g.setColour(style.thumbBorderColor);
    g.fillEllipse(thumbBounds);

    // Inner fill
    auto innerBounds = thumbBounds.reduced(style.thumbBorderWidth);
    g.setColour(style.thumbColor);
    g.fillEllipse(innerBounds);
  }
}

void ScrubBar::resized() {
  // Layout is computed dynamically
}

//==============================================================================
void ScrubBar::mouseDown(const juce::MouseEvent& event) {
  if (!requireMessageThread())
    return;

  isDragging = true;
  auto listenerList = listeners;
  juce::Component::SafePointer<ScrubBar> safeThis(this);
  listenerList->call([](Listener& l) { l.scrubStarted(); });
  if (safeThis == nullptr)
    return;
  updatePositionFromMouse(event);
}

void ScrubBar::mouseDrag(const juce::MouseEvent& event) {
  if (!requireMessageThread())
    return;

  if (isDragging)
    updatePositionFromMouse(event);
}

void ScrubBar::mouseUp(const juce::MouseEvent& event) {
  juce::ignoreUnused(event);
  if (!requireMessageThread() || !isDragging)
    return;

  isDragging = false;
  const double seekTime = duration > 0.0 ? position * duration : -1.0;
  auto listenerList = listeners;
  juce::Component::SafePointer<ScrubBar> safeThis(this);
  listenerList->call([](Listener& l) { l.scrubEnded(); });
  if (safeThis == nullptr)
    return;

  if (seekTime >= 0.0) {
    auto listenerList = listeners;
    juce::Component::SafePointer<ScrubBar> safeSeekThis(this);
    listenerList->call([seekTime](Listener& l) { l.seekRequested(seekTime); });
    if (safeSeekThis == nullptr)
      return;
  }
}

void ScrubBar::mouseMove(const juce::MouseEvent& event) {
  juce::ignoreUnused(event);
  if (!requireMessageThread())
    return;

  const bool wasHovering = isHovering;
  isHovering = true;
  setMouseCursor(juce::MouseCursor::PointingHandCursor);

  if (!wasHovering)
    repaint();
}

void ScrubBar::mouseExit(const juce::MouseEvent& event) {
  juce::ignoreUnused(event);
  if (!requireMessageThread())
    return;

  isHovering = false;
  setMouseCursor(juce::MouseCursor::NormalCursor);
  repaint();
}

//==============================================================================
double ScrubBar::xToPosition(float x) const {
  auto trackBounds = getTrackBounds();
  if (trackBounds.getWidth() <= 0.0f)
    return 0.0;

  const float relativeX = x - trackBounds.getX();
  const double pos = relativeX / trackBounds.getWidth();
  return juce::jlimit(0.0, 1.0, pos);
}

float ScrubBar::positionToX(double pos) const {
  auto trackBounds = getTrackBounds();
  return static_cast<float>(trackBounds.getX() +
                            trackBounds.getWidth() * juce::jlimit(0.0, 1.0, pos));
}

juce::Rectangle<float> ScrubBar::getTrackBounds() const {
  auto bounds = getLocalBounds().toFloat();
  float y = (bounds.getHeight() - style.trackHeight) * 0.5f;

  return juce::Rectangle<float>(bounds.getX(), y, bounds.getWidth(), style.trackHeight);
}

juce::Rectangle<float> ScrubBar::getThumbBounds() const {
  auto trackBounds = getTrackBounds();
  float x = positionToX(position);
  float y = trackBounds.getCentreY();

  return juce::Rectangle<float>(x - style.thumbSize * 0.5f, y - style.thumbSize * 0.5f,
                                style.thumbSize, style.thumbSize);
}

void ScrubBar::updatePositionFromMouse(const juce::MouseEvent& event) {
  const double newPosition = xToPosition(event.position.x);

  if (position != newPosition) {
    position = newPosition;
    currentTime = duration > 0.0 ? position * duration : 0.0;
    const double seekTime = duration > 0.0 ? currentTime : -1.0;
    repaint();

    auto listenerList = listeners;
    juce::Component::SafePointer<ScrubBar> safeThis(this);
    listenerList->call([newPosition](Listener& l) { l.scrubPositionChanged(newPosition); });
    if (safeThis == nullptr)
      return;

    if (seekTime >= 0.0) {
      auto listenerList = listeners;
      juce::Component::SafePointer<ScrubBar> safeSeekThis(this);
      listenerList->call([seekTime](Listener& l) { l.seekRequested(seekTime); });
      if (safeSeekThis == nullptr)
        return;
    }
  }
}

} // namespace shmui
