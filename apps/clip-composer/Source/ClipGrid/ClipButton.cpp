// SPDX-License-Identifier: MIT

#include "ClipButton.h"
#include "../UI/ConsoleTheme.h"
#include "../UI/DesignTokens.h"
#include "ClipGrid.h"

//==============================================================================
ClipButton::ClipButton(int buttonIndex) : m_buttonIndex(buttonIndex) {
  // Default empty state
  m_state = State::Empty;
  m_clipColor = juce::Colours::darkgrey;
  // m_clipName default-constructs to empty string, no need to assign

  // Initialize animation timer (60fps for smooth hover effects)
  m_lastAnimTime = juce::Time::getMillisecondCounterHiRes();
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

void ClipButton::setBevelWidthPercent(float percent) {
  float clamped = juce::jlimit(0.0f, 0.2f, percent);
  if (std::abs(m_bevelWidthPercent - clamped) > 0.001f) {
    m_bevelWidthPercent = clamped;
    repaint();
  }
}

void ClipButton::setButtonTextMode(int mode) {
  int clamped = juce::jlimit(0, 2, mode);
  if (m_buttonTextMode != clamped) {
    m_buttonTextMode = clamped;
    repaint();
  }
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
  m_stopOthersEnabled = false;
  repaint();
}

void ClipButton::setPlaybackProgress(float progress) {
  float newProgress = juce::jlimit(0.0f, 1.0f, progress);

  // Only update and repaint if progress changed meaningfully (>0.1% difference)
  // This avoids repainting for tiny floating-point variations
  if (std::abs(newProgress - m_playbackProgress) > 0.001f) {
    m_playbackProgress = newProgress;

    // Only repaint if playing (avoid unnecessary repaints)
    if (m_state == State::Playing || m_state == State::Stopping)
      repaint();
  }
}

void ClipButton::setLoopEnabled(bool enabled) {
  if (m_loopEnabled != enabled) {
    m_loopEnabled = enabled;
    repaint(); // CRITICAL: Trigger repaint immediately
    DBG("ClipButton " << m_buttonIndex << ": Loop icon = " << (enabled ? "ON" : "OFF"));
  }
}

void ClipButton::setFadeInEnabled(bool enabled) {
  if (m_fadeInEnabled != enabled) {
    m_fadeInEnabled = enabled;
    repaint(); // CRITICAL: Trigger repaint immediately
    DBG("ClipButton " << m_buttonIndex << ": Fade-in icon = " << (enabled ? "ON" : "OFF"));
  }
}

void ClipButton::setFadeOutEnabled(bool enabled) {
  if (m_fadeOutEnabled != enabled) {
    m_fadeOutEnabled = enabled;
    repaint(); // CRITICAL: Trigger repaint immediately
    DBG("ClipButton " << m_buttonIndex << ": Fade-out icon = " << (enabled ? "ON" : "OFF"));
  }
}

void ClipButton::setEffectsEnabled(bool enabled) {
  if (m_effectsEnabled != enabled) {
    m_effectsEnabled = enabled;
    repaint(); // CRITICAL: Trigger repaint immediately
    DBG("ClipButton " << m_buttonIndex << ": Effects icon = " << (enabled ? "ON" : "OFF"));
  }
}

void ClipButton::setStopOthersEnabled(bool enabled) {
  if (m_stopOthersEnabled != enabled) {
    m_stopOthersEnabled = enabled;
    repaint(); // CRITICAL: Trigger repaint immediately
    DBG("ClipButton " << m_buttonIndex << ": Stop-others icon = " << (enabled ? "ON" : "OFF"));
  }
}

//==============================================================================
juce::String ClipButton::formatDuration(double seconds) const {
  int totalSeconds = static_cast<int>(seconds);
  int hours = totalSeconds / 3600;
  int minutes = (totalSeconds % 3600) / 60;
  int secs = totalSeconds % 60;

  // Show MM:SS by default, HH:MM:SS only if >60 minutes
  // No frames/hundredths shown on clip button faces
  if (hours > 0) {
    return juce::String::formatted("%02d:%02d:%02d", hours, minutes, secs);
  } else {
    return juce::String::formatted("%02d:%02d", minutes, secs);
  }
}

void ClipButton::paint(juce::Graphics& g) {
  auto bounds = getLocalBounds().toFloat();
  if (bounds.isEmpty())
    return;

  juce::Colour groupColors[4] = {
      juce::Colour(OCC::Design::kGroupBlue), juce::Colour(OCC::Design::kGroupGreen),
      juce::Colour(OCC::Design::kGroupOrange), juce::Colour(OCC::Design::kGroupRed)};
  const auto groupColor = groupColors[juce::jlimit(0, 3, m_clipGroup)];

  // Face tint comes from the per-clip swatch colour (visual identifier).
  // Stripe comes from group colour (routing channel). These are independent dimensions.
  // The constructor seeds m_clipColor to juce::Colours::darkgrey, which is opaque but
  // visually neutral, so we treat both transparent and pure dark-grey as "no swatch set"
  // and fall back to a neutral chassis tint (not the group colour — that lives on the stripe).
  const bool hasSwatch = !m_clipColor.isTransparent() && m_clipColor != juce::Colours::darkgrey &&
                         m_clipColor != juce::Colour(OCC::Design::kBgComponent);
  const auto neutralChassisTint = juce::Colour(OCC::Design::kBgComponent);
  const auto swatchTint = hasSwatch ? m_clipColor : neutralChassisTint;

  auto face = bounds.reduced(1.0f);
  juce::Colour top;
  juce::Colour bottom;
  juce::Colour border = juce::Colour(OCC::Design::kBorderDefault).withAlpha(0.72f);

  switch (m_state) {
  case State::Empty:
    top = juce::Colour(OCC::Design::kClipEmpty).brighter(0.05f);
    bottom = juce::Colour(OCC::Design::kClipEmpty).darker(0.08f);
    break;
  case State::Loaded:
    if (hasSwatch) {
      // Saturated tint so the swatch reads at a glance across the grid. The stripe
      // and inset chassis-to-swatch transition keep the chip from looking flat.
      top = juce::Colour(OCC::Design::kBgComponent).interpolatedWith(swatchTint, 0.55f);
      bottom = juce::Colour(OCC::Design::kBgSecondary).interpolatedWith(swatchTint, 0.40f);
    } else {
      // No swatch — show the neutral loaded chassis (cell wells stay readable; the
      // stripe alone communicates group routing).
      top = juce::Colour(OCC::Design::kClipLoaded).brighter(0.04f);
      bottom = juce::Colour(OCC::Design::kClipLoaded).darker(0.05f);
    }
    break;
  case State::Playing:
    top = juce::Colour(OCC::Design::kClipPlaying).brighter(0.20f);
    bottom = juce::Colour(OCC::Design::kClipPlaying).darker(0.06f);
    border = juce::Colour(OCC::Design::kAccentGreen).brighter(0.26f);
    break;
  case State::Stopping:
    top = juce::Colour(OCC::Design::kClipStopping).brighter(0.15f);
    bottom = juce::Colour(OCC::Design::kClipStopping).darker(0.08f);
    border = juce::Colour(OCC::Design::kMeterOrange);
    break;
  }

  if (m_hoverOpacity > 0.01f) {
    top = top.brighter(m_hoverOpacity * 0.10f);
    bottom = bottom.brighter(m_hoverOpacity * 0.07f);
    border = border.brighter(m_hoverOpacity * 0.18f);
  }

  g.setGradientFill(OCC::Console::verticalGradient(top, bottom, face));
  g.fillRoundedRectangle(face, CORNER_RADIUS);

  g.setColour(juce::Colours::white.withAlpha(m_state == State::Empty ? 0.025f : 0.065f));
  g.drawLine(face.getX() + 2.0f, face.getY() + 1.0f, face.getRight() - 2.0f, face.getY() + 1.0f,
             1.0f);
  g.setColour(juce::Colours::black.withAlpha(0.38f));
  g.drawLine(face.getX() + 2.0f, face.getBottom() - 1.0f, face.getRight() - 2.0f,
             face.getBottom() - 1.0f, 1.0f);

  if (m_state != State::Empty) {
    auto stripe = face.withWidth(4.0f);
    g.setColour(groupColor);
    g.fillRoundedRectangle(stripe, CORNER_RADIUS);
    g.setColour(juce::Colours::black.withAlpha(0.28f));
    g.drawVerticalLine(static_cast<int>(stripe.getRight()), stripe.getY(), stripe.getBottom());
  }

  if (m_state == State::Playing) {
    auto now = juce::Time::getMillisecondCounterHiRes();
    float pulsePhase =
        std::fmod(now / static_cast<double>(OCC::Design::ClipButton::kPulseCycleMs), 1.0);
    float pulseAlpha = 0.6f + 0.4f * std::sin(pulsePhase * juce::MathConstants<float>::twoPi);
    g.setColour(border.withAlpha(pulseAlpha));
    g.drawRoundedRectangle(face, CORNER_RADIUS, 3.0f);
  } else {
    g.setColour(border);
    g.drawRoundedRectangle(face, CORNER_RADIUS, 1.0f);
  }

  if (m_hoverOpacity > 0.01f && m_state != State::Playing) {
    g.setColour(juce::Colour(OCC::Design::kNeveBlue).withAlpha(m_hoverOpacity * 0.22f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), CORNER_RADIUS + 1.0f, 1.5f);
  }

  if (m_isPlaybox) {
    g.setColour(juce::Colour(OCC::Design::kTextPrimary).withAlpha(0.82f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), CORNER_RADIUS, 1.5f);
  }

  if (m_bevelWidthPercent > 0.001f && m_state != State::Empty && m_bevelWidthPercent <= 0.12f) {
    float bevelWidth = std::min(bounds.getWidth(), bounds.getHeight()) * m_bevelWidthPercent;
    auto innerBounds = bounds.reduced(BORDER_THICKNESS);
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.fillRect(innerBounds.getX() + CORNER_RADIUS, innerBounds.getY(),
               innerBounds.getWidth() - CORNER_RADIUS * 2.0f, bevelWidth);
    g.setColour(juce::Colours::black.withAlpha(0.12f));
    g.fillRect(innerBounds.getX() + CORNER_RADIUS, innerBounds.getBottom() - bevelWidth,
               innerBounds.getWidth() - CORNER_RADIUS * 2.0f, bevelWidth);
  }

  if (m_state == State::Empty) {
    const bool tightWidth = bounds.getWidth() < 90.0f;
    const bool tightHeight = bounds.getHeight() < 64.0f;
    auto meta = face.reduced(tightWidth ? 6.0f : 10.0f, tightHeight ? 5.0f : 8.0f);
    g.setColour(juce::Colour(OCC::Design::kTextPrimary).withAlpha(0.34f));
    g.setFont(OCC::Console::monoFont(tightHeight ? 12.0f : 18.0f, juce::Font::plain));
    g.drawText(juce::String(getDisplayNumber()).paddedLeft('0', getDisplayNumber() < 100 ? 2 : 3),
               meta.removeFromTop(tightHeight ? 16.0f : 22.0f).toNearestInt(),
               juce::Justification::topLeft, false);

    if (!tightWidth && !tightHeight && m_keyboardShortcut.isNotEmpty()) {
      auto hotkey = face.reduced(10.0f, 8.0f).removeFromTop(20.0f).removeFromRight(42.0f);
      g.setFont(OCC::Console::consoleFont(14.0f, juce::Font::bold));
      g.setColour(juce::Colour(OCC::Design::kTextPrimary).withAlpha(0.42f));
      g.drawText(m_keyboardShortcut, hotkey.toNearestInt(), juce::Justification::topRight, false);
    }
  } else {
    drawClipHUD(g, face);
  }
}

void ClipButton::drawClipHUD(juce::Graphics& g, juce::Rectangle<float> bounds) {
  using namespace OCC::Design::ClipButton;
  auto contentArea = bounds.reduced(10.0f, 8.0f);
  contentArea.removeFromLeft(5.0f);
  const bool tightWidth = bounds.getWidth() < 92.0f;
  const bool compactWidth = bounds.getWidth() < 116.0f;
  const bool tightHeight = bounds.getHeight() < 62.0f;
  const bool compactHeight = bounds.getHeight() < 78.0f;
  const bool showMeta = !tightHeight;
  const bool showHotkey = !tightWidth && !tightHeight && m_buttonTextMode > 0;
  const bool showIndicators = bounds.getWidth() >= 82.0f && bounds.getHeight() >= 54.0f;
  const bool useDarkText = m_state == State::Playing || m_state == State::Stopping;
  const auto textColor =
      useDarkText ? juce::Colour(0xfff8eed7) : juce::Colour(OCC::Design::kTextPrimary);
  const auto mutedText = textColor.withAlpha(useDarkText ? 0.78f : 0.62f);
  const float nameFont = tightHeight ? 11.0f : (compactWidth || compactHeight ? 13.0f : 16.0f);
  const int nameLines = compactHeight ? 1 : 2;

  if (showMeta) {
    auto topRow = contentArea.removeFromTop(16.0f);
    juce::String buttonNumber = juce::String(getDisplayNumber());
    g.setFont(OCC::Console::monoFont(compactWidth ? 9.0f : 10.0f, juce::Font::plain));
    g.setColour(mutedText.withAlpha(0.58f));
    g.drawText(buttonNumber.paddedLeft('0', buttonNumber.getIntValue() < 100 ? 2 : 3),
               topRow.removeFromLeft(42.0f).toNearestInt(), juce::Justification::topLeft, false);

    if (m_beatOffset.isNotEmpty()) {
      g.setFont(OCC::Console::consoleFont(10.0f, juce::Font::plain));
      juce::String beatDisplay = " // " + m_beatOffset;
      g.drawText(beatDisplay, topRow.removeFromLeft(42.0f).toNearestInt(),
                 juce::Justification::topLeft, false);
    }

    // Design kit's playing-state treatment is: green-fill cell + bottom progress
    // bar (drawn below). No chip or badge in the meta row. The hotkey remains
    // visible throughout so the operator always knows the trigger binding.
    if (showHotkey) {
      juce::String displayText;
      if (m_buttonTextMode == 1 && m_keyboardShortcut.isNotEmpty())
        displayText = m_keyboardShortcut;
      else if (m_buttonTextMode == 2)
        displayText = "MIDI";
      if (displayText.isNotEmpty()) {
        g.setFont(OCC::Console::consoleFont(compactWidth ? 12.0f : 15.0f, juce::Font::bold));
        g.setColour(textColor);
        g.drawText(displayText, topRow.toNearestInt(), juce::Justification::topRight, false);
      }
    }
  }

  auto nameArea = contentArea;
  if (!tightHeight && m_durationSeconds > 0.0)
    nameArea.removeFromBottom(18.0f);
  if (showIndicators)
    nameArea.removeFromBottom(14.0f);

  g.setFont(OCC::Console::consoleFont(nameFont, juce::Font::bold));
  g.setColour(juce::Colours::black.withAlpha(0.38f));
  g.drawFittedText(m_clipName, nameArea.translated(0, 1).toNearestInt(),
                   juce::Justification::centredLeft, nameLines, 0.86f);
  g.setColour(textColor);
  g.drawFittedText(m_clipName, nameArea.toNearestInt(), juce::Justification::centredLeft, nameLines,
                   0.86f);

  // Bottom row: time on the left, flag indicators on the right.
  if (!tightHeight && m_durationSeconds > 0.0) {
    auto bottomRow = contentArea.removeFromBottom(18.0f);

    if (showIndicators) {
      // Reserve right-side space for the active flag glyphs (10x10 each, 5 px gap).
      constexpr float iconSize = 10.0f;
      constexpr float iconGap = 5.0f;
      const int activeCount = (m_loopEnabled ? 1 : 0) + (m_fadeInEnabled ? 1 : 0) +
                              (m_fadeOutEnabled ? 1 : 0) + (m_stopOthersEnabled ? 1 : 0);
      if (activeCount > 0) {
        const float reserved = activeCount * iconSize + (activeCount - 1) * iconGap;
        auto iconRow = bottomRow.removeFromRight(reserved);
        // Vertically center within the 18 px bottom row.
        iconRow = iconRow.withSizeKeepingCentre(reserved, iconSize);
        drawStatusIcons(g, iconRow);
      }
    }

    juce::String timeDisplay;
    if (m_state == State::Playing || m_state == State::Stopping) {
      const double elapsed = m_durationSeconds * m_playbackProgress;
      const double remaining = m_durationSeconds - elapsed;
      timeDisplay = formatDuration(elapsed) + " / " + formatDuration(remaining);
    } else {
      timeDisplay = formatDuration(m_durationSeconds);
    }
    g.setFont(OCC::Console::monoFont(compactHeight ? 9.0f : 10.5f, juce::Font::plain));
    g.setColour(mutedText.withAlpha(0.92f));
    g.drawText(timeDisplay, bottomRow.toNearestInt(), juce::Justification::centredLeft, false);
  } else if (showIndicators) {
    // No duration row to share — render icons at the bottom-right anyway.
    constexpr float iconSize = 10.0f;
    constexpr float iconGap = 5.0f;
    const int activeCount = (m_loopEnabled ? 1 : 0) + (m_fadeInEnabled ? 1 : 0) +
                            (m_fadeOutEnabled ? 1 : 0) + (m_stopOthersEnabled ? 1 : 0);
    if (activeCount > 0) {
      const float reserved = activeCount * iconSize + (activeCount - 1) * iconGap;
      auto iconRow =
          juce::Rectangle<float>(bounds.getRight() - 10.0f - reserved,
                                 bounds.getBottom() - 10.0f - iconSize, reserved, iconSize);
      drawStatusIcons(g, iconRow);
    }
  }

  // Progress bar — design kit spec: 3 px high, anchored to bottom-left, white
  // rgba(255,255,255,0.9) with a soft glow (boxShadow 0 0 6px rgba(255,255,255,0.5)).
  // No track / no inset / no rounding.
  if ((m_state == State::Playing || m_state == State::Stopping) && m_playbackProgress > 0.0f) {
    const float barHeight = 3.0f;
    const float fillWidth = bounds.getWidth() * m_playbackProgress;
    auto fill =
        juce::Rectangle<float>(bounds.getX(), bounds.getBottom() - barHeight, fillWidth, barHeight);
    // Glow underlay: paint a slightly larger soft-edge rectangle behind the bar to
    // approximate the 6 px CSS shadow.
    g.setColour(juce::Colours::white.withAlpha(0.18f));
    g.fillRect(fill.expanded(0.0f, 2.0f));
    g.setColour(juce::Colours::white.withAlpha(0.90f));
    g.fillRect(fill);
  }
}

void ClipButton::drawStatusIcons(juce::Graphics& g, juce::Rectangle<float> bounds) {
  // Glyphs ported verbatim from the design kit (orpheus_design-system_2605 /
  // ui_kits/clip-composer/components.jsx, latest indicator block).
  //   * 10x10 px target on screen, drawn from a 16-unit viewBox via scale factor.
  //   * Single cream stroke / fill — rgba(237, 226, 204, 0.95).
  //   * Loop       → two-arc "refresh cycle" symbol with two arrowheads.
  //   * Fade In    → filled triangle, hypotenuse rising left→right.
  //   * Fade Out   → filled triangle, hypotenuse falling left→right.
  //   * Stop Others→ 5-point shield outline with a filled centre dot.
  constexpr float iconSize = 10.0f;
  constexpr float iconGap = 5.0f;
  // Each glyph is authored in a 16x16 viewBox; we render at iconSize so the scale
  // factor is iconSize / 16.
  constexpr float kViewBox = 16.0f;
  const float scale = iconSize / kViewBox;
  const auto cream = juce::Colour(OCC::Design::kTextPrimary).withAlpha(0.95f);

  float xPos = bounds.getX();
  const float yPos = bounds.getY();

  auto pt = [&](float x, float y) {
    return juce::Point<float>(xPos + x * scale, yPos + y * scale);
  };

  if (m_loopEnabled) {
    // SVG: M3 8a5 5 0 0 1 9-3   M13 8a5 5 0 0 1-9 3
    //      M12 3v3h-3            M4 13v-3h3
    juce::Path loop;
    // Top arc M3 8 a5 5 0 0 1 9 -3  (sweep flag 1 = clockwise from start)
    const auto arc = [&](float cx, float cy, float rx, float ry, float startDeg, float endDeg) {
      const float startRad = juce::degreesToRadians(startDeg);
      const float endRad = juce::degreesToRadians(endDeg);
      const auto p0 = pt(cx + rx * std::cos(startRad), cy + ry * std::sin(startRad));
      loop.startNewSubPath(p0);
      const int steps = 32;
      for (int i = 1; i <= steps; ++i) {
        const float t = startRad + (endRad - startRad) * (static_cast<float>(i) / steps);
        loop.lineTo(pt(cx + rx * std::cos(t), cy + ry * std::sin(t)));
      }
    };
    // Top half arc from (3,8) to (12,5): centre near (7.5, 8), radius ~5, sweep CW above.
    arc(7.5f, 8.0f, 5.0f, 5.0f, 180.0f, 300.0f);
    // Bottom half arc from (13,8) to (4,11).
    arc(8.5f, 8.0f, 5.0f, 5.0f, 0.0f, 120.0f);
    // Arrowhead 1 — at top-right: M12 3v3h-3
    loop.startNewSubPath(pt(12.0f, 3.0f));
    loop.lineTo(pt(12.0f, 6.0f));
    loop.lineTo(pt(9.0f, 6.0f));
    // Arrowhead 2 — at bottom-left: M4 13v-3h3
    loop.startNewSubPath(pt(4.0f, 13.0f));
    loop.lineTo(pt(4.0f, 10.0f));
    loop.lineTo(pt(7.0f, 10.0f));
    g.setColour(cream);
    g.strokePath(loop, juce::PathStrokeType(1.6f * scale, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::butt));
    xPos += iconSize + iconGap;
  }

  if (m_fadeInEnabled) {
    // SVG fill path: M2 13 14 3 v10 z  (triangle: (2,13)→(14,3)→(14,13)→close).
    juce::Path tri;
    tri.startNewSubPath(pt(2.0f, 13.0f));
    tri.lineTo(pt(14.0f, 3.0f));
    tri.lineTo(pt(14.0f, 13.0f));
    tri.closeSubPath();
    g.setColour(cream);
    g.fillPath(tri);
    xPos += iconSize + iconGap;
  }

  if (m_fadeOutEnabled) {
    // SVG fill path: M2 3 v10 l12 -3 z  (triangle: (2,3)→(2,13)→(14,10)→close).
    juce::Path tri;
    tri.startNewSubPath(pt(2.0f, 3.0f));
    tri.lineTo(pt(2.0f, 13.0f));
    tri.lineTo(pt(14.0f, 10.0f));
    tri.closeSubPath();
    g.setColour(cream);
    g.fillPath(tri);
    xPos += iconSize + iconGap;
  }

  if (m_stopOthersEnabled) {
    // SVG: shield M8 1.5 14 5 v6 L8 14.5 2 11 V5 z  plus filled circle r=2 at (8,8).
    juce::Path shield;
    shield.startNewSubPath(pt(8.0f, 1.5f));
    shield.lineTo(pt(14.0f, 5.0f));
    shield.lineTo(pt(14.0f, 11.0f));
    shield.lineTo(pt(8.0f, 14.5f));
    shield.lineTo(pt(2.0f, 11.0f));
    shield.lineTo(pt(2.0f, 5.0f));
    shield.closeSubPath();
    g.setColour(cream);
    g.strokePath(shield, juce::PathStrokeType(1.6f * scale, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::butt));
    juce::Path dot;
    const auto centre = pt(8.0f, 8.0f);
    const float dotR = 2.0f * scale;
    dot.addEllipse(centre.x - dotR, centre.y - dotR, dotR * 2.0f, dotR * 2.0f);
    g.fillPath(dot);
    xPos += iconSize + iconGap;
  }

  (void)xPos;
}

void ClipButton::resized() {
  // No child components yet, layout handled in paint()
}

void ClipButton::mouseDown(const juce::MouseEvent& e) {
  if (e.mods.isLeftButtonDown()) {
    // Check for Ctrl+Opt+Cmd+LeftClick to open Edit Dialog directly (Feature 1)
    // This bypasses the right-click menu and goes straight to the edit dialog
    if (e.mods.isCommandDown() && e.mods.isCtrlDown() && e.mods.isAltDown()) {
      // Only trigger if clip is loaded (Edit Dialog requires a clip)
      if (m_state != State::Empty && onEditDialogRequested) {
        onEditDialogRequested(m_buttonIndex);
      }
      return; // Don't process as drag or regular click
    }

    // Item 53: Ctrl+Click (or Cmd+Click on Mac) as alternative to right-click for context menu
    // Safer than right-click in live scenarios
    if (e.mods.isCtrlDown() || (e.mods.isCommandDown() && !e.mods.isAltDown())) {
      // Show context menu (same as right-click)
      if (onRightClick)
        onRightClick(m_buttonIndex);
      return; // Don't process as regular click
    }

    // Record mouse down position for potential Cmd+Drag rearrangement
    m_mouseDownPosition = e.getPosition();
    m_isDragging = false;

    // Fire click immediately (don't wait for mouseUp to avoid double-click delay)
    // This makes rapid clicking feel responsive
    if (!e.mods.isCommandDown() && m_state != State::Empty) {
      // Only fire if not holding Cmd (which would be drag-to-rearrange)
      if (onClick)
        onClick(m_buttonIndex);
    }
  } else if (e.mods.isRightButtonDown()) {
    // Right click - context menu (works on both empty and loaded buttons)
    if (onRightClick)
      onRightClick(m_buttonIndex);
  }
}

void ClipButton::mouseDrag(const juce::MouseEvent& e) {
  // Only allow drag if Cmd/Ctrl key is held and clip is loaded
  if (!e.mods.isLeftButtonDown() || !e.mods.isCommandDown() || m_state == State::Empty)
    return;

  // Check if we've moved enough to consider it a drag
  auto dragDistance = e.getPosition().getDistanceFrom(m_mouseDownPosition);
  if (dragDistance < 10.0f && !m_isDragging)
    return;

  m_isDragging = true;

  // Visual feedback: make button slightly transparent while dragging
  setAlpha(0.6f);
}

void ClipButton::mouseUp(const juce::MouseEvent& e) {
  // Restore full opacity
  setAlpha(1.0f);

  if (!e.mods.isLeftButtonDown())
    return;

  if (m_isDragging) {
    // We were dragging - find target button under mouse
    auto* grid = findParentComponentOfClass<ClipGrid>();
    if (grid) {
      // Convert to grid coordinates
      auto posInGrid = grid->getLocalPoint(this, e.getPosition());

      // Find which button we're over
      for (int i = 0; i < grid->getButtonCount(); ++i) {
        auto* targetButton = grid->getButton(i);
        if (targetButton && targetButton != this && targetButton->getBounds().contains(posInGrid)) {
          // Trigger drag callback
          if (onDragToButton) {
            onDragToButton(m_buttonIndex, i);
          }
          break;
        }
      }
    }
    m_isDragging = false;
  }
  // Note: Double-click behavior intentionally removed
  // Clip buttons prioritize single-click for PLAY/STOP at all times
  // Use right-click menu or Ctrl+Opt+Cmd+Click to access Edit Dialog
}

//==============================================================================
// Animation Handlers (shmui Interpolation-based)

void ClipButton::mouseEnter(const juce::MouseEvent& /*e*/) {
  m_isHovered = true;
  // Start animation timer if not already running
  if (!isTimerRunning())
    startTimerHz(60); // 60fps for smooth hover animation
}

void ClipButton::mouseExit(const juce::MouseEvent& /*e*/) {
  m_isHovered = false;
  // Timer will stop when animation completes
}

void ClipButton::timerCallback() {
  // Calculate delta time for frame-rate independent animation
  double now = juce::Time::getMillisecondCounterHiRes();
  float deltaTime = static_cast<float>((now - m_lastAnimTime) / 1000.0);
  m_lastAnimTime = now;

  // Clamp delta time to prevent huge jumps after focus loss
  deltaTime = juce::jlimit(0.0f, 0.1f, deltaTime);

  // Animate hover opacity using shmui::Interpolation::smoothDelta
  float targetHover = m_isHovered ? 1.0f : 0.0f;
  m_hoverOpacity = shmui::Interpolation::smoothDelta(m_hoverOpacity, targetHover, 0.25f, deltaTime);

  // Animate press opacity (for click feedback)
  float targetPress = 0.0f; // Will be set to 1.0 during mouseDown
  m_pressOpacity = shmui::Interpolation::smoothDelta(m_pressOpacity, targetPress, 0.35f, deltaTime);

  // Check if animation is complete
  bool hoverComplete = std::abs(m_hoverOpacity - targetHover) < 0.01f;
  bool pressComplete = std::abs(m_pressOpacity - targetPress) < 0.01f;

  if (hoverComplete && pressComplete && !m_isHovered) {
    // Animation complete, stop timer
    stopTimer();
    m_hoverOpacity = 0.0f;
    m_pressOpacity = 0.0f;
  }

  // Trigger repaint for visual update
  repaint();
}
