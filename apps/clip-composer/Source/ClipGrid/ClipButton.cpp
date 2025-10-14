// SPDX-License-Identifier: MIT

#include "ClipButton.h"

//==============================================================================
ClipButton::ClipButton(int buttonIndex) : m_buttonIndex(buttonIndex) {
  // Default empty state
  m_state = State::Empty;
  m_clipColor = juce::Colours::darkgrey;
  // m_clipName default-constructs to empty string, no need to assign
}

//==============================================================================
void ClipButton::setState(State newState) {
  if (m_state != newState) {
    m_state = newState;
    repaint();
  }
}

void ClipButton::setClipName(const juce::String& name) {
  m_clipName = name;
  repaint();
}

void ClipButton::setClipColor(juce::Colour color) {
  m_clipColor = color;
  repaint();
}

void ClipButton::setClipDuration(double durationSeconds) {
  m_durationSeconds = durationSeconds;
  repaint();
}

void ClipButton::setClipGroup(int group) {
  m_clipGroup = juce::jlimit(0, 3, group);
  repaint();
}

void ClipButton::setKeyboardShortcut(const juce::String& shortcut) {
  m_keyboardShortcut = shortcut;
  repaint();
}

void ClipButton::setBeatOffset(const juce::String& beatOffset) {
  m_beatOffset = beatOffset;
  repaint();
}

void ClipButton::clearClip() {
  m_state = State::Empty;
  m_clipName.clear();
  m_clipColor = juce::Colours::darkgrey;
  m_durationSeconds = 0.0;
  m_clipGroup = 0;
  m_keyboardShortcut.clear();
  m_beatOffset.clear();
  m_playbackProgress = 0.0f;
  m_loopEnabled = false;
  m_fadeInEnabled = false;
  m_fadeOutEnabled = false;
  m_effectsEnabled = false;
  repaint();
}

void ClipButton::setPlaybackProgress(float progress) {
  m_playbackProgress = juce::jlimit(0.0f, 1.0f, progress);

  // Only repaint if playing (avoid unnecessary repaints)
  if (m_state == State::Playing || m_state == State::Stopping)
    repaint();
}

void ClipButton::setLoopEnabled(bool enabled) {
  m_loopEnabled = enabled;
  repaint();
}

void ClipButton::setFadeInEnabled(bool enabled) {
  m_fadeInEnabled = enabled;
  repaint();
}

void ClipButton::setFadeOutEnabled(bool enabled) {
  m_fadeOutEnabled = enabled;
  repaint();
}

void ClipButton::setEffectsEnabled(bool enabled) {
  m_effectsEnabled = enabled;
  repaint();
}

//==============================================================================
juce::String ClipButton::formatDuration(double seconds) const {
  int totalSeconds = static_cast<int>(seconds);
  int minutes = totalSeconds / 60;
  int secs = totalSeconds % 60;
  return juce::String::formatted("%d:%02d", minutes, secs);
}

void ClipButton::paint(juce::Graphics& g) {
  auto bounds = getLocalBounds().toFloat();

  // Background color based on state
  juce::Colour bgColor;
  juce::Colour borderColor;

  switch (m_state) {
  case State::Empty:
    bgColor = juce::Colour(0xff2a2a2a);     // Dark grey
    borderColor = juce::Colour(0xff404040); // Slightly lighter border
    break;

  case State::Loaded:
    bgColor = m_clipColor.darker(0.6f); // Darkened clip color
    borderColor = m_clipColor.darker(0.2f);
    break;

  case State::Playing:
    bgColor = m_clipColor.brighter(0.8f); // Very bright when playing
    borderColor = juce::Colours::white;   // White border when playing
    break;

  case State::Stopping:
    bgColor = m_clipColor;
    borderColor = juce::Colours::orange; // Orange border during fade-out
    break;
  }

  // Draw button background with rounded corners
  g.setColour(bgColor);
  g.fillRoundedRectangle(bounds.reduced(1.0f), CORNER_RADIUS);

  // Draw border
  g.setColour(borderColor);
  g.drawRoundedRectangle(bounds.reduced(1.0f), CORNER_RADIUS, BORDER_THICKNESS);

  if (m_state == State::Empty) {
    // Button index (larger, more prominent)
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(juce::FontOptions("Inter", 18.0f, juce::Font::bold));
    g.drawText(juce::String(m_buttonIndex + 1), bounds, juce::Justification::centred, false);

    // "Empty" label (smaller, subtle)
    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.setFont(juce::FontOptions("Inter", 9.0f, juce::Font::plain));
    auto emptyLabel = bounds.reduced(PADDING).removeFromBottom(14);
    g.drawText("empty", emptyLabel, juce::Justification::centred, false);
  } else {
    // Modern HUD layout for loaded clips
    drawClipHUD(g, bounds);
  }
}

void ClipButton::drawClipHUD(juce::Graphics& g, juce::Rectangle<float> bounds) {
  auto contentArea = bounds.reduced(PADDING);
  float currentY = contentArea.getY();

  // === TOP ROW: Button Index + Keyboard Shortcut ===
  {
    auto topRow = contentArea.removeFromTop(16.0f);

    // Button index (left, subtle)
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::plain));
    g.drawText(juce::String(m_buttonIndex + 1), topRow, juce::Justification::topLeft, false);

    // Keyboard shortcut (right, prominent)
    if (m_keyboardShortcut.isNotEmpty()) {
      g.setColour(juce::Colours::white.withAlpha(0.9f));
      g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
      g.drawText(m_keyboardShortcut, topRow, juce::Justification::topRight, false);
    }

    currentY = topRow.getBottom() + 2.0f;
  }

  // === MIDDLE: Clip Name (primary focus) ===
  {
    float nameHeight = contentArea.getHeight() * 0.5f;
    auto nameArea =
        juce::Rectangle<float>(contentArea.getX(), currentY, contentArea.getWidth(), nameHeight);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions("Inter", 14.0f, juce::Font::bold));

    // Truncate name if too long
    auto displayName = m_clipName;
    if (displayName.length() > 22)
      displayName = displayName.substring(0, 19) + "...";

    g.drawText(displayName, nameArea, juce::Justification::centred,
               true); // Allow word wrap

    currentY = nameArea.getBottom();
  }

  // === BOTTOM ROW: Duration + Beat Offset + Group ===
  {
    auto bottomArea = juce::Rectangle<float>(contentArea.getX(), contentArea.getBottom() - 28.0f,
                                             contentArea.getWidth(), 28.0f);

    // Duration (left, prominent)
    if (m_durationSeconds > 0.0) {
      g.setColour(juce::Colours::white.withAlpha(0.9f));
      g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::plain));
      g.drawText(formatDuration(m_durationSeconds),
                 bottomArea.withTrimmedRight(bottomArea.getWidth() * 0.5f),
                 juce::Justification::centredLeft, false);
    }

    // Beat offset (center, if present) - e.g., "//3+"
    if (m_beatOffset.isNotEmpty()) {
      g.setColour(juce::Colour(0xffffaa00)); // Orange for timing info
      g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::bold));
      g.drawText("//" + m_beatOffset, bottomArea.withSizeKeepingCentre(60.0f, 14.0f),
                 juce::Justification::centred, false);
    }

    // Clip group indicator (right) - e.g., "G1", "G2"
    {
      juce::Colour groupColors[4] = {
          juce::Colour(0xff3498db), // Blue - Group 0
          juce::Colour(0xff2ecc71), // Green - Group 1
          juce::Colour(0xfff39c12), // Orange - Group 2
          juce::Colour(0xffe74c3c)  // Red - Group 3
      };

      auto groupBadge = bottomArea.removeFromRight(24.0f).withTrimmedTop(8.0f).withHeight(16.0f);

      // Draw group badge background
      g.setColour(groupColors[m_clipGroup].withAlpha(0.8f));
      g.fillRoundedRectangle(groupBadge, 3.0f);

      // Draw group number
      g.setColour(juce::Colours::white);
      g.setFont(juce::FontOptions("Inter", 9.0f, juce::Font::bold));
      g.drawText("G" + juce::String(m_clipGroup + 1), groupBadge, juce::Justification::centred,
                 false);
    }
  }

  // === PLAYING STATE INDICATOR ===
  if (m_state == State::Playing) {
    // Play triangle icon (top-right corner, prominent)
    auto playIconBounds = bounds.removeFromTop(20.0f).removeFromRight(20.0f).reduced(4.0f);

    // Draw play triangle (pointing right)
    juce::Path playTriangle;
    float cx = playIconBounds.getCentreX();
    float cy = playIconBounds.getCentreY();
    float size = 10.0f;

    playTriangle.addTriangle(cx - size * 0.3f, cy - size * 0.5f, // Top-left
                             cx - size * 0.3f, cy + size * 0.5f, // Bottom-left
                             cx + size * 0.6f, cy                // Right point
    );

    // Bright green background circle
    g.setColour(juce::Colour(0xff00ff00)); // Bright green
    g.fillEllipse(playIconBounds);

    // White play triangle
    g.setColour(juce::Colours::white);
    g.fillPath(playTriangle);
  }

  // === PROGRESS BAR ===
  if ((m_state == State::Playing || m_state == State::Stopping) && m_playbackProgress > 0.0f) {
    // Draw progress bar at the very bottom of the button
    auto progressArea = bounds.removeFromBottom(3.0f).reduced(1.0f, 0.0f);
    float progressWidth = progressArea.getWidth() * m_playbackProgress;

    // Background (darker)
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.fillRoundedRectangle(progressArea, 1.5f);

    // Progress fill (bright accent color)
    if (progressWidth > 0.0f) {
      auto fillArea = progressArea.withWidth(progressWidth);
      g.setColour(m_state == State::Playing ? juce::Colours::cyan : juce::Colours::orange);
      g.fillRoundedRectangle(fillArea, 1.5f);
    }
  }

  // === STATUS ICONS ===
  // Draw status icons in the middle-left area if any are enabled
  if (m_loopEnabled || m_fadeInEnabled || m_fadeOutEnabled || m_effectsEnabled) {
    auto iconArea = juce::Rectangle<float>(contentArea.getX(), contentArea.getCentreY() - 8.0f,
                                           contentArea.getWidth() * 0.3f, 16.0f);
    drawStatusIcons(g, iconArea);
  }
}

void ClipButton::drawStatusIcons(juce::Graphics& g, juce::Rectangle<float> bounds) {
  // Draw small status icons horizontally
  float iconSize = 12.0f;
  float iconSpacing = 2.0f;
  float currentX = bounds.getX();

  // Loop icon (circular arrow)
  if (m_loopEnabled) {
    auto iconBounds = juce::Rectangle<float>(currentX, bounds.getY(), iconSize, iconSize);

    // Draw circular arrow (simplified as a circle with arrow tip)
    g.setColour(juce::Colour(0xff00d4ff)); // Cyan
    g.drawEllipse(iconBounds.reduced(2.0f), 1.5f);

    // Arrow tip (small triangle)
    juce::Path arrowTip;
    arrowTip.addTriangle(iconBounds.getRight() - 3.0f, iconBounds.getY() + 2.0f,
                         iconBounds.getRight() - 1.0f, iconBounds.getY() + 4.0f,
                         iconBounds.getRight() - 3.0f, iconBounds.getY() + 6.0f);
    g.fillPath(arrowTip);

    currentX += iconSize + iconSpacing;
  }

  // Fade in icon (ramp up)
  if (m_fadeInEnabled) {
    auto iconBounds = juce::Rectangle<float>(currentX, bounds.getY(), iconSize, iconSize);

    g.setColour(juce::Colour(0xff00ff88)); // Green
    juce::Path fadeIn;
    fadeIn.startNewSubPath(iconBounds.getX() + 2.0f, iconBounds.getBottom() - 2.0f);
    fadeIn.lineTo(iconBounds.getRight() - 2.0f, iconBounds.getY() + 2.0f);
    g.strokePath(fadeIn, juce::PathStrokeType(1.5f));

    currentX += iconSize + iconSpacing;
  }

  // Fade out icon (ramp down)
  if (m_fadeOutEnabled) {
    auto iconBounds = juce::Rectangle<float>(currentX, bounds.getY(), iconSize, iconSize);

    g.setColour(juce::Colour(0xffff8800)); // Orange
    juce::Path fadeOut;
    fadeOut.startNewSubPath(iconBounds.getX() + 2.0f, iconBounds.getY() + 2.0f);
    fadeOut.lineTo(iconBounds.getRight() - 2.0f, iconBounds.getBottom() - 2.0f);
    g.strokePath(fadeOut, juce::PathStrokeType(1.5f));

    currentX += iconSize + iconSpacing;
  }

  // Effects icon (waveform with sparkle)
  if (m_effectsEnabled) {
    auto iconBounds = juce::Rectangle<float>(currentX, bounds.getY(), iconSize, iconSize);

    g.setColour(juce::Colour(0xffff00ff)); // Magenta

    // Draw wavy line
    juce::Path wave;
    float waveY = iconBounds.getCentreY();
    wave.startNewSubPath(iconBounds.getX() + 2.0f, waveY);

    for (float x = 2.0f; x < iconSize - 2.0f; x += 2.0f) {
      float offset = (static_cast<int>(x / 2.0f) % 2 == 0) ? 2.0f : -2.0f;
      wave.lineTo(iconBounds.getX() + x, waveY + offset);
    }

    g.strokePath(wave, juce::PathStrokeType(1.5f));

    // Add a small star/sparkle
    g.fillEllipse(iconBounds.getCentreX() - 1.0f, iconBounds.getY() + 2.0f, 2.0f, 2.0f);

    currentX += iconSize + iconSpacing;
  }
}

void ClipButton::resized() {
  // No child components yet, layout handled in paint()
}

void ClipButton::mouseDown(const juce::MouseEvent& e) {
  if (e.mods.isLeftButtonDown()) {
    // Left click - trigger clip
    if (onClick)
      onClick(m_buttonIndex);
  } else if (e.mods.isRightButtonDown()) {
    // Right click - context menu
    if (onRightClick)
      onRightClick(m_buttonIndex);
  }
}
