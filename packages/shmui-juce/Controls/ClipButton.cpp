/*
  ==============================================================================

    ClipButton.cpp
    Created: shmui Button System

    Clip button implementation with state machine.

  ==============================================================================
*/

#include "ClipButton.h"

namespace shmui {

//==============================================================================
ClipButton::ClipButton(int buttonIndex) : m_buttonIndex(buttonIndex) {
  setStyle(ButtonStyle::Ghost);

  // Override click handlers
  onClick = [this] { handleClipClick(); };
  onRightClick = [this] { handleClipRightClick(); };
}

//==============================================================================
void ClipButton::setClipState(State newState) {
  if (m_clipState != newState) {
    m_clipState = newState;
    m_stateTransition = 0.0f; // Reset transition animation

    // Start playing pulse animation
    if (newState == State::Playing) {
      m_playingPulse = 0.0f;
      startAnimation();
    }

    repaint();
  }
}

void ClipButton::setClipName(const juce::String& name) {
  if (m_clipName != name) {
    m_clipName = name;
    repaint();
  }
}

void ClipButton::setClipColor(juce::Colour color) {
  if (m_clipColor != color) {
    m_clipColor = color;
    repaint();
  }
}

void ClipButton::setClipDuration(double durationSeconds) {
  if (m_durationSeconds != durationSeconds) {
    m_durationSeconds = durationSeconds;
    repaint();
  }
}

void ClipButton::setKeyboardShortcut(const juce::String& shortcut) {
  if (m_keyboardShortcut != shortcut) {
    m_keyboardShortcut = shortcut;
    repaint();
  }
}

void ClipButton::clearClip() {
  m_clipName = "";
  m_clipColor = tokens::clip::empty(); // --clip-empty (unlit well)
  m_durationSeconds = 0.0;
  m_keyboardShortcut = "";
  m_playbackProgress = 0.0f;
  m_loopEnabled = false;
  m_fadeInEnabled = false;
  m_fadeOutEnabled = false;
  m_clipState = State::Empty;
  repaint();
}

void ClipButton::setPlaybackProgress(float progress) {
  const float clampedProgress = juce::jlimit(0.0f, 1.0f, progress);
  if (std::abs(m_playbackProgress - clampedProgress) > 0.001f) {
    m_playbackProgress = clampedProgress;
    if (m_clipState == State::Playing)
      repaint();
  }
}

void ClipButton::setLoopEnabled(bool enabled) {
  if (m_loopEnabled != enabled) {
    m_loopEnabled = enabled;
    repaint();
  }
}

void ClipButton::setFadeInEnabled(bool enabled) {
  if (m_fadeInEnabled != enabled) {
    m_fadeInEnabled = enabled;
    repaint();
  }
}

void ClipButton::setFadeOutEnabled(bool enabled) {
  if (m_fadeOutEnabled != enabled) {
    m_fadeOutEnabled = enabled;
    repaint();
  }
}

void ClipButton::setIsPlaybox(bool isPlaybox) {
  if (m_isPlaybox != isPlaybox) {
    m_isPlaybox = isPlaybox;
    repaint();
  }
}

//==============================================================================
void ClipButton::paintContent(juce::Graphics& g, juce::Rectangle<float> bounds,
                              juce::Colour foregroundColor) {
  juce::ignoreUnused(foregroundColor);

  // Background based on state
  juce::Colour bgColor;
  switch (m_clipState) {
  case State::Empty:
    bgColor = juce::Colour(0xFF2A2A2A);
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
  if (m_clipState == State::Playing) {
    g.setColour(juce::Colours::white.withAlpha(0.8f + m_playingPulse * 0.2f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), CORNER_RADIUS, BORDER_THICKNESS);
  }

  // Draw playbox indicator (selection outline)
  if (m_isPlaybox) {
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), CORNER_RADIUS + 1, 1.0f);
  }

  // Draw content based on state
  if (m_clipState != State::Empty) {
    // Draw clip name
    if (m_clipName.isNotEmpty()) {
      g.setColour(juce::Colours::white);
      g.setFont(juce::Font(11.0f));

      auto textBounds = bounds.reduced(PADDING);
      g.drawText(m_clipName, textBounds, juce::Justification::centred, true);
    }

    // Draw HUD (shortcut, duration)
    drawClipHUD(g, bounds);

    // Draw status icons (loop, fade in/out)
    drawStatusIcons(g, bounds);

    // Draw progress indicator when playing
    if (m_clipState == State::Playing) {
      drawProgressIndicator(g, bounds);
    }
  } else {
    // Draw button number for empty state
    g.setColour(juce::Colours::grey.withAlpha(0.3f));
    g.setFont(juce::Font(10.0f));
    g.drawText(juce::String(m_buttonIndex + 1), bounds, juce::Justification::centred, false);
  }
}

void ClipButton::animationTick() {
  Button::animationTick();

  // Pulse animation for playing state
  if (m_clipState == State::Playing) {
    m_playingPulse = std::sin(juce::Time::getMillisecondCounterHiRes() / 1000.0 *
                              juce::MathConstants<double>::twoPi * 0.5f) *
                         0.5f +
                     0.5f;
    repaint();
  }

  // State transition animation
  if (m_stateTransition < 1.0f) {
    m_stateTransition =
        Interpolation::smooth(m_stateTransition, 1.0f, Interpolation::kTransitionStep);
    repaint();
  }
}

//==============================================================================
void ClipButton::handleClipClick() {
  if (onClipClick)
    onClipClick(m_buttonIndex);
}

void ClipButton::handleClipRightClick() {
  if (onClipRightClick)
    onClipRightClick(m_buttonIndex);
}

void ClipButton::drawClipHUD(juce::Graphics& g, juce::Rectangle<float> bounds) {
  auto hudBounds = bounds.reduced(PADDING);

  // Keyboard shortcut (top-left)
  if (m_keyboardShortcut.isNotEmpty()) {
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(juce::Font(9.0f, juce::Font::bold));
    g.drawText(m_keyboardShortcut, hudBounds.removeFromTop(12.0f), juce::Justification::topLeft,
               false);
  }

  // Duration (bottom-right)
  if (m_durationSeconds > 0.0) {
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(juce::Font(8.0f));
    g.drawText(formatDuration(m_durationSeconds), bounds.reduced(PADDING),
               juce::Justification::bottomRight, false);
  }
}

void ClipButton::drawStatusIcons(juce::Graphics& g, juce::Rectangle<float> bounds) {
  float iconX = bounds.getRight() - PADDING - ICON_SIZE;
  float iconY = bounds.getY() + PADDING;

  g.setColour(juce::Colours::white.withAlpha(0.6f));

  if (m_loopEnabled) {
    auto iconBounds = juce::Rectangle<float>(iconX, iconY, ICON_SIZE, ICON_SIZE);
    Icons::drawIcon(g, IconType::Loop, iconBounds, juce::Colours::white.withAlpha(0.6f));
    iconX -= ICON_SIZE + 2;
  }

  // Future: Add fade in/out indicators
}

void ClipButton::drawProgressIndicator(juce::Graphics& g, juce::Rectangle<float> bounds) {
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

juce::String ClipButton::formatDuration(double seconds) const {
  if (seconds < 60.0) {
    return juce::String(seconds, 1) + "s";
  } else {
    int mins = static_cast<int>(seconds / 60.0);
    int secs = static_cast<int>(std::fmod(seconds, 60.0));
    return juce::String(mins) + ":" + juce::String(secs).paddedLeft('0', 2);
  }
}

} // namespace shmui
