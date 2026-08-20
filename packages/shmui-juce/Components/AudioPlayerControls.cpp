/*
  ==============================================================================

    AudioPlayerControls.cpp
    Shmui - Audio/AI-focused UI component library

  ==============================================================================
*/

#include "AudioPlayerControls.h"
#include <limits>

namespace shmui {

//==============================================================================
AudioPlayerControls::AudioPlayerControls() {
  setOpaque(false);
  const auto& theme = defaultTheme();
  style.buttonColor = theme.accent;
  style.buttonHoverColor = theme.accent.brighter(0.15f);
  style.textColor = theme.fgMuted;
  style.iconColor = theme.fg;
  sanitizeStyle();
  addDefaultThemeListener(this);
}

AudioPlayerControls::~AudioPlayerControls() {
  stopTimer();
  removeDefaultThemeListener(this);
}

//==============================================================================
void AudioPlayerControls::setCurrentTime(double timeInSeconds) {
  if (!requireMessageThread())
    return;

  const double safeTime = std::isfinite(timeInSeconds) ? juce::jmax(0.0, timeInSeconds) : 0.0;
  if (currentTime != safeTime) {
    currentTime = safeTime;
    repaint();
  }
}

void AudioPlayerControls::setDuration(double durationInSeconds) {
  if (!requireMessageThread())
    return;

  const double safeDuration =
      std::isfinite(durationInSeconds) ? juce::jmax(0.0, durationInSeconds) : 0.0;
  duration = safeDuration;
  if (currentTime > duration)
    currentTime = duration;
  repaint();
}

void AudioPlayerControls::setPlaying(bool shouldBePlaying) {
  if (!requireMessageThread())
    return;

  if (playing != shouldBePlaying) {
    playing = shouldBePlaying;
    repaint();
    auto listenerList = listeners;
    juce::Component::SafePointer<AudioPlayerControls> safeThis(this);
    listenerList->call([shouldBePlaying](Listener& l) { l.playStateChanged(shouldBePlaying); });
    if (safeThis == nullptr)
      return;
  }
}

void AudioPlayerControls::setPlaybackRate(double rate) {
  if (!requireMessageThread())
    return;

  rate = std::isfinite(rate) ? juce::jlimit(0.25, 2.0, rate) : 1.0;
  if (playbackRate != rate) {
    playbackRate = rate;
    repaint();
    auto listenerList = listeners;
    juce::Component::SafePointer<AudioPlayerControls> safeThis(this);
    listenerList->call([rate](Listener& l) { l.playbackRateChanged(rate); });
    if (safeThis == nullptr)
      return;
  }
}

void AudioPlayerControls::setBuffering(bool isBuffering) {
  if (!requireMessageThread())
    return;

  if (buffering != isBuffering) {
    buffering = isBuffering;
    if (buffering)
      startTimerHz(60);
    else
      stopTimer();
    repaint();
  }
}

//==============================================================================
void AudioPlayerControls::addListener(Listener* listener) {
  if (!requireMessageThread())
    return;
  listeners->add(listener);
}

void AudioPlayerControls::removeListener(Listener* listener) {
  if (!requireMessageThread())
    return;
  listeners->remove(listener);
}

void AudioPlayerControls::setStyle(const Style& newStyle) {
  if (!requireMessageThread())
    return;
  style = newStyle;
  sanitizeStyle();
  customStyle = true;
  repaint();
}

void AudioPlayerControls::defaultThemeChanged(const ShmuiTheme& theme) {
  if (!requireMessageThread() || customStyle)
    return;

  style.buttonColor = theme.accent;
  style.buttonHoverColor = theme.accent.brighter(0.15f);
  style.textColor = theme.fgMuted;
  style.iconColor = theme.fg;
  sanitizeStyle();
  repaint();
}
void AudioPlayerControls::sanitizeStyle() {
  style.buttonSize =
      std::isfinite(style.buttonSize) ? juce::jlimit(1.0f, 1000.0f, style.buttonSize) : 40.0f;
  style.fontSize =
      std::isfinite(style.fontSize) ? juce::jlimit(1.0f, 256.0f, style.fontSize) : 14.0f;
  style.cornerRadius =
      std::isfinite(style.cornerRadius) ? juce::jlimit(0.0f, 1000.0f, style.cornerRadius) : 8.0f;
  style.padding = std::isfinite(style.padding) ? juce::jmax(0.0f, style.padding) : 8.0f;
}

//==============================================================================
void AudioPlayerControls::paint(juce::Graphics& g) {
  auto bounds = getLocalBounds().toFloat().reduced(style.padding);

  // Background (optional)
  if (style.backgroundColor.getAlpha() > 0) {
    g.setColour(style.backgroundColor);
    g.fillRoundedRectangle(bounds, style.cornerRadius);
  }

  // Play/Pause button
  auto playBounds = getPlayButtonBounds();
  auto buttonColour = playButtonHovered ? style.buttonHoverColor : style.buttonColor;
  if (playButtonPressed)
    buttonColour = buttonColour.darker(0.2f);

  g.setColour(buttonColour);
  g.fillRoundedRectangle(playBounds, style.cornerRadius);

  // Icon or spinner
  auto iconBounds = playBounds.reduced(playBounds.getWidth() * 0.25f);
  g.setColour(style.iconColor);

  if (buffering && playing) {
    drawSpinner(g, iconBounds);
  } else if (playing) {
    drawPauseIcon(g, iconBounds);
  } else {
    drawPlayIcon(g, iconBounds);
  }

  // Time display
  auto timeBounds = getTimeDisplayBounds();
  g.setColour(style.textColor);
  g.setFont(style.fontSize);

  juce::String timeText = formatTime(currentTime) + " / " + formatTime(duration);
  g.drawText(timeText, timeBounds, juce::Justification::centred, true);

  // Speed button
  auto speedBounds = getSpeedButtonBounds();
  if (speedButtonHovered) {
    g.setColour(style.buttonColor.withAlpha(0.1f));
    g.fillRoundedRectangle(speedBounds, style.cornerRadius * 0.5f);
  }

  g.setColour(style.textColor);
  g.setFont(style.fontSize * 0.9f);

  juce::String speedText;
  if (playbackRate == 1.0)
    speedText = "1x";
  else
    speedText = juce::String(playbackRate, 2) + "x";

  g.drawText(speedText, speedBounds, juce::Justification::centred, true);
}
void AudioPlayerControls::mouseDown(const juce::MouseEvent& event) {
  if (!requireMessageThread())
    return;

  auto playBounds = getPlayButtonBounds();
  auto speedBounds = getSpeedButtonBounds();
  auto timeBounds = getTimeDisplayBounds();

  if (playBounds.contains(event.position)) {
    playButtonPressed = true;
    repaint();
  } else if (speedBounds.contains(event.position)) {
    showSpeedMenu();
  } else if (timeBounds.contains(event.position) && duration > 0.0) {
    timeDisplayPressed = true;
    repaint();
  }
}

void AudioPlayerControls::mouseUp(const juce::MouseEvent& event) {
  if (!requireMessageThread())
    return;

  if (timeDisplayPressed) {
    timeDisplayPressed = false;
    const auto timeBounds = getTimeDisplayBounds();
    if (timeBounds.contains(event.position) && duration > 0.0) {
      const double position =
          juce::jlimit(0.0, 1.0,
                       static_cast<double>(event.position.x - timeBounds.getX()) /
                           static_cast<double>(timeBounds.getWidth()));
      const double seekTime = position * duration;
      auto listenerList = listeners;
      juce::Component::SafePointer<AudioPlayerControls> safeThis(this);
      listenerList->call([seekTime](Listener& listener) { listener.seekRequested(seekTime); });
      if (safeThis == nullptr)
        return;
    }
    repaint();
  }

  if (playButtonPressed) {
    playButtonPressed = false;
    auto playBounds = getPlayButtonBounds();
    if (playBounds.contains(event.position))
      setPlaying(!playing);
    repaint();
  }
}

void AudioPlayerControls::mouseMove(const juce::MouseEvent& event) {
  if (!requireMessageThread())
    return;

  auto playBounds = getPlayButtonBounds();
  auto speedBounds = getSpeedButtonBounds();
  const bool newPlayHover = playBounds.contains(event.position);
  const bool newSpeedHover = speedBounds.contains(event.position);

  if (newPlayHover != playButtonHovered || newSpeedHover != speedButtonHovered) {
    playButtonHovered = newPlayHover;
    speedButtonHovered = newSpeedHover;
    setMouseCursor((playButtonHovered || speedButtonHovered) ? juce::MouseCursor::PointingHandCursor
                                                             : juce::MouseCursor::NormalCursor);
    repaint();
  }
}

void AudioPlayerControls::mouseExit(const juce::MouseEvent& event) {
  juce::ignoreUnused(event);
  if (!requireMessageThread())
    return;

  playButtonHovered = false;
  speedButtonHovered = false;
  setMouseCursor(juce::MouseCursor::NormalCursor);
  repaint();
}

void AudioPlayerControls::resized() {
  if (!requireMessageThread())
    return;
  repaint();
}

void AudioPlayerControls::timerCallback() {
  if (!requireMessageThread())
    return;

  spinnerAngle += 0.15f;
  if (spinnerAngle > juce::MathConstants<float>::twoPi)
    spinnerAngle -= juce::MathConstants<float>::twoPi;
  repaint();
}

//==============================================================================
juce::String AudioPlayerControls::formatTime(double seconds) {
  if (!std::isfinite(seconds) || seconds < 0.0)
    return "--:--";

  const double boundedSeconds =
      juce::jmin(seconds, static_cast<double>(std::numeric_limits<int>::max()));
  const int totalSeconds = static_cast<int>(boundedSeconds);
  const int hrs = totalSeconds / 3600;
  const int mins = (totalSeconds % 3600) / 60;
  const int secs = totalSeconds % 60;

  if (hrs > 0)
    return juce::String::formatted("%d:%02d:%02d", hrs, mins, secs);

  return juce::String::formatted("%d:%02d", mins, secs);
}

juce::Rectangle<float> AudioPlayerControls::getPlayButtonBounds() const {
  auto bounds = getLocalBounds().toFloat().reduced(style.padding);
  return juce::Rectangle<float>(bounds.getX(), bounds.getCentreY() - style.buttonSize * 0.5f,
                                style.buttonSize, style.buttonSize);
}

juce::Rectangle<float> AudioPlayerControls::getTimeDisplayBounds() const {
  auto bounds = getLocalBounds().toFloat().reduced(style.padding);
  auto playBounds = getPlayButtonBounds();
  auto speedBounds = getSpeedButtonBounds();

  float left = playBounds.getRight() + style.padding;
  float right = speedBounds.getX() - style.padding;

  return juce::Rectangle<float>(left, bounds.getCentreY() - style.fontSize * 0.75f, right - left,
                                style.fontSize * 1.5f);
}

juce::Rectangle<float> AudioPlayerControls::getSpeedButtonBounds() const {
  auto bounds = getLocalBounds().toFloat().reduced(style.padding);
  const float speedWidth = style.fontSize * 3.0f;

  return juce::Rectangle<float>(bounds.getRight() - speedWidth,
                                bounds.getCentreY() - style.buttonSize * 0.4f, speedWidth,
                                style.buttonSize * 0.8f);
}

void AudioPlayerControls::drawPlayIcon(juce::Graphics& g, juce::Rectangle<float> bounds) {
  juce::Path triangle;
  const float x = bounds.getX();
  const float y = bounds.getY();
  const float w = bounds.getWidth();
  const float h = bounds.getHeight();
  triangle.addTriangle(x + w * 0.15f, y, x + w * 0.15f, y + h, x + w, y + h * 0.5f);
  g.fillPath(triangle);
}

void AudioPlayerControls::drawPauseIcon(juce::Graphics& g, juce::Rectangle<float> bounds) {
  const float barWidth = bounds.getWidth() * 0.3f;
  const float gap = bounds.getWidth() * 0.2f;
  auto leftBar = bounds.withWidth(barWidth);
  auto rightBar = bounds.withWidth(barWidth).withX(bounds.getX() + barWidth + gap);
  g.fillRoundedRectangle(leftBar, 2.0f);
  g.fillRoundedRectangle(rightBar, 2.0f);
}

void AudioPlayerControls::drawSpinner(juce::Graphics& g, juce::Rectangle<float> bounds) {
  const float strokeWidth = bounds.getWidth() * 0.15f;
  const auto centre = bounds.getCentre();
  const float radius = (bounds.getWidth() - strokeWidth) * 0.5f;

  g.setColour(style.iconColor.withAlpha(0.3f));
  juce::Path bgArc;
  bgArc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, 0.0f,
                      juce::MathConstants<float>::twoPi, true);
  g.strokePath(bgArc, juce::PathStrokeType(strokeWidth));

  g.setColour(style.iconColor);
  juce::Path fgArc;
  const float arcLength = juce::MathConstants<float>::pi * 0.75f;
  fgArc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, spinnerAngle,
                      spinnerAngle + arcLength, true);
  g.strokePath(fgArc, juce::PathStrokeType(strokeWidth, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
}

void AudioPlayerControls::showSpeedMenu() {
  if (!requireMessageThread())
    return;

  juce::PopupMenu menu;
  for (int i = 0; i < numPlaybackSpeeds; ++i) {
    const double speed = playbackSpeeds[i];
    const juce::String text = (speed == 1.0) ? "Normal" : juce::String(speed, 2) + "x";
    menu.addItem(i + 1, text, true, juce::approximatelyEqual(playbackRate, speed));
  }

  juce::Component::SafePointer<AudioPlayerControls> safeThis(this);
  menu.showMenuAsync(
      juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(
          getSpeedButtonBounds().toNearestInt().translated(getScreenX(), getScreenY())),
      [safeThis](int result) mutable {
        if (safeThis != nullptr && result > 0 && result <= numPlaybackSpeeds)
          safeThis->setPlaybackRate(playbackSpeeds[result - 1]);
      });
}

} // namespace shmui
