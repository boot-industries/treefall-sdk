// SPDX-License-Identifier: MIT

#include "ClipButton.h"
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
  using namespace OCC::Design::ClipButton;
  auto bounds = getLocalBounds().toFloat();

  // Background color based on state
  juce::Colour bgColor;
  juce::Colour borderColor;

  switch (m_state) {
  case State::Empty:
    bgColor = juce::Colour(OCC::Design::kBgComponent);
    borderColor = juce::Colour(OCC::Design::kBorderDefault);
    break;

  case State::Loaded:
    bgColor = m_clipColor.withAlpha(kLoadedAlpha);
    borderColor = m_clipColor.darker(0.2f);
    break;

  case State::Playing:
    bgColor = m_clipColor.withAlpha(kLoadedAlpha);
    borderColor = juce::Colour(OCC::Design::kTextPrimary);
    break;

  case State::Stopping:
    bgColor = m_clipColor.withAlpha(kLoadedAlpha);
    borderColor = juce::Colour(OCC::Design::kMeterOrange);
    break;
  }

  // Apply hover brightness using shmui::Interpolation::lerp
  // Subtle lift effect: brighten background and border on hover
  if (m_hoverOpacity > 0.01f) {
    float hoverLift = m_hoverOpacity * kHoverBrightnessMax;
    bgColor = bgColor.brighter(hoverLift);
    borderColor = borderColor.brighter(hoverLift * 0.5f);
  }

  // Draw button background with rounded corners
  g.setColour(bgColor);
  g.fillRoundedRectangle(bounds.reduced(1.0f), CORNER_RADIUS);

  // Draw subtle hover glow (outer glow effect)
  if (m_hoverOpacity > 0.01f && m_state != State::Playing) {
    // Soft Neve blue glow around button on hover
    juce::Colour glowColor =
        juce::Colour(OCC::Design::kNeveBlue).withAlpha(m_hoverOpacity * kHoverGlowAlpha);
    g.setColour(glowColor);
    g.drawRoundedRectangle(bounds.reduced(0.5f), CORNER_RADIUS + 1, OCC::Design::kBorderMedium);
  }

  // Draw border (animated for Playing state)
  if (m_state == State::Playing) {
    // Glowing pulsing border for playing state
    // Use timestamp for pulsing animation
    auto now = juce::Time::getMillisecondCounterHiRes();
    float pulsePhase = std::fmod(now / static_cast<double>(kPulseCycleMs), 1.0);
    float pulseAlpha = 0.6f + 0.4f * std::sin(pulsePhase * juce::MathConstants<float>::twoPi);

    // Draw thick glowing border
    g.setColour(borderColor.withAlpha(pulseAlpha));
    g.drawRoundedRectangle(bounds.reduced(1.0f), CORNER_RADIUS, kPlayingBorderWidth);

    // NOTE: Animation driven by 75fps timer in ClipGrid, NOT by repaint() here
    // (calling repaint() from paint() would create infinite loop)
  } else {
    // Normal border for other states
    g.setColour(borderColor);
    g.drawRoundedRectangle(bounds.reduced(1.0f), CORNER_RADIUS, BORDER_THICKNESS);
  }

  // Item 60: Draw playbox outline (thin white border that follows arrow key navigation)
  if (m_isPlaybox) {
    g.setColour(juce::Colours::white);
    g.drawRoundedRectangle(bounds.reduced(0.5f), CORNER_RADIUS, kPlayboxBorderWidth);
  }

  if (m_state == State::Empty) {
    // Button index (larger, more prominent)
    // Feature 4: Use consecutive numbering across tabs
    g.setColour(juce::Colours::white.withAlpha(kTextShadowAlpha));
    g.setFont(juce::FontOptions("HK Grotesk", kFontButtonNumber, juce::Font::bold));
    g.drawText(juce::String(getDisplayNumber()), bounds, juce::Justification::centred, false);

    // No "Empty" text - just the button number on grey background is sufficient
  } else {
    // Modern HUD layout for loaded clips
    drawClipHUD(g, bounds);
  }
}

void ClipButton::drawClipHUD(juce::Graphics& g, juce::Rectangle<float> bounds) {
  using namespace OCC::Design::ClipButton;
  auto contentArea = bounds.reduced(PADDING);
  float currentY = contentArea.getY();

  // Determine text color based on background brightness
  // Skew towards white/light text (dark mode app) - only use black on VERY light backgrounds
  juce::Colour bgColor;
  switch (m_state) {
  case State::Loaded:
  case State::Playing:
  case State::Stopping:
    bgColor = m_clipColor.withAlpha(kLoadedAlpha);
    break;
  default:
    bgColor = juce::Colours::darkgrey;
    break;
  }

  // Use black text ONLY on extremely light backgrounds, white otherwise
  // This ensures readability while strongly favoring white text for dark mode aesthetic
  float brightness = bgColor.getBrightness();
  juce::Colour textColor = brightness > kBrightnessThreshold ? juce::Colours::black.withAlpha(0.95f)
                                                             : juce::Colours::white;
  juce::Colour subtleTextColor = textColor.withAlpha(0.6f);

  // === TOP ROW: Button Index (white rounded box) + Keyboard Shortcut ===
  {
    auto topRow = contentArea.removeFromTop(kIndicatorBoxHeight);

    // Button index in white rounded rectangle (Feature 3 - tighter margins)
    // Feature 4: Use consecutive numbering across tabs
    juce::String buttonNumber = juce::String(getDisplayNumber());
    g.setFont(juce::FontOptions("HK Grotesk", kFontHotkey, juce::Font::bold));

    auto numberBox = topRow.removeFromLeft(kIndicatorBoxWidth).withHeight(kIndicatorBoxHeight);

    // Draw white rounded rectangle background (OCC130 Sprint A.4: 4px corner radius)
    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.fillRoundedRectangle(numberBox, OCC::Design::kRadiusMD);

    // Draw black text
    g.setColour(juce::Colours::black);
    g.drawText(buttonNumber, numberBox, juce::Justification::centred, false);

    // OCC130 Sprint A.3: Beat indicator next to clip number (top-left)
    // Format: " // {value}" (e.g., " // 3+")
    if (m_beatOffset.isNotEmpty()) {
      g.setColour(textColor); // Use adaptive text color (white/black based on bg)
      g.setFont(juce::FontOptions("HK Grotesk", kFontBeatOffset, juce::Font::plain));
      juce::String beatDisplay = " // " + m_beatOffset;
      auto beatArea = topRow.removeFromLeft(kBeatAreaWidth);
      g.drawText(beatDisplay, beatArea, juce::Justification::centredLeft, false);
    }

    // OCC130 Sprint A.4: Keyboard shortcut indicator (top-right corner)
    // Thin outline box, transparent background, adaptive color
    if (m_keyboardShortcut.isNotEmpty()) {
      g.setFont(juce::FontOptions("HK Grotesk", kFontHotkey, juce::Font::bold));

      auto hotkeyBox = topRow.removeFromRight(kIndicatorBoxWidth).withHeight(kIndicatorBoxHeight);

      // Draw thin outline (adaptive color)
      // Light outline on dark buttons, dark outline on light buttons
      juce::Colour outlineColor = brightness > kBrightnessThreshold
                                      ? juce::Colours::black.withAlpha(0.6f)
                                      : juce::Colours::white.withAlpha(kGroupBadgeAlpha);

      g.setColour(outlineColor);
      g.drawRoundedRectangle(hotkeyBox, OCC::Design::kRadiusMD, kPlayboxBorderWidth);

      // Draw text (adaptive color)
      g.setColour(textColor);
      g.drawText(m_keyboardShortcut, hotkeyBox, juce::Justification::centred, false);
    }

    currentY = topRow.getBottom() + 2.0f;
  }

  // === MIDDLE: Clip Name (PRIMARY) + Duration (secondary) ===
  {
    // Give most space to clip name
    float nameHeight = contentArea.getHeight() * 0.65f;
    auto nameArea =
        juce::Rectangle<float>(contentArea.getX(), currentY, contentArea.getWidth(), nameHeight);

    // Clip Name (PRIMARY - MUCH larger, bold, 3 lines)
    g.setFont(juce::FontOptions("HK Grotesk", kFontClipName, juce::Font::bold));

    // Reserve minimal space for duration
    auto nameOnlyArea = nameArea.withTrimmedBottom(12.0f);

    // Draw 1px shadow first
    g.setColour(juce::Colours::black.withAlpha(kTextShadowAlpha));
    g.drawFittedText(m_clipName, nameOnlyArea.translated(0, 1).toNearestInt(),
                     juce::Justification::centred,
                     3, // Allow up to 3 lines for name
                     0.85f);

    // Draw main text on top
    g.setColour(textColor);
    g.drawFittedText(m_clipName, nameOnlyArea.toNearestInt(), juce::Justification::centred,
                     3, // Allow up to 3 lines for name
                     0.85f);

    // OCC130 Sprint A.2: Time display - show elapsed — remaining when playing, total when stopped
    auto durationArea = nameArea.removeFromBottom(22.0f);
    if (m_durationSeconds > 0.0) {
      g.setFont(juce::FontOptions("HK Grotesk", kFontTimeDisplay, juce::Font::plain));

      juce::String timeDisplay;

      // Show different formats based on playback state
      if (m_state == State::Playing || m_state == State::Stopping) {
        // Playing/Stopping: show elapsed — remaining
        // Elapsed = current playhead position within trimmed region (IN to OUT)
        double elapsed = m_durationSeconds * m_playbackProgress;
        double remaining = m_durationSeconds - elapsed;

        // Format: "MM:SS — MM:SS" (HH:MM:SS if >60 min)
        timeDisplay = formatDuration(elapsed) + " — " + formatDuration(remaining);

        // Draw dark grey rounded rectangle backdrop with padding
        auto backdropArea = durationArea.reduced(2.0f);
        g.setColour(juce::Colour(OCC::Design::kBgComponent).withAlpha(kBackdropAlpha));
        g.fillRoundedRectangle(backdropArea, OCC::Design::kRadiusMD);

        // Color: green when playing, orange when stopping
        juce::Colour timeColor =
            m_state == State::Playing
                ? juce::Colour(OCC::Design::kAccentGreen).withAlpha(kLoadedAlpha)
                : juce::Colour(OCC::Design::kMeterOrange).withAlpha(kLoadedAlpha);

        // Draw 2px shadow first
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawText(timeDisplay, durationArea.translated(0, 2), juce::Justification::centred, false);

        // Draw main text on top
        g.setColour(timeColor);
      } else {
        // Loaded/Empty: show total duration only
        timeDisplay = formatDuration(m_durationSeconds);

        // Draw 1px shadow for loaded state
        g.setColour(juce::Colours::black.withAlpha(kTextShadowAlpha));
        g.drawText(timeDisplay, durationArea.translated(0, 1), juce::Justification::centred, false);

        // Color: subtle text
        g.setColour(subtleTextColor);
      }

      g.drawText(timeDisplay, durationArea, juce::Justification::centred, false);
    }

    currentY = nameArea.getBottom();
  }

  // === BOTTOM ROW: Clip Group ===
  {
    auto bottomArea = juce::Rectangle<float>(contentArea.getX(), contentArea.getBottom() - 24.0f,
                                             contentArea.getWidth(), 24.0f);

    // Item 29: Clip group indicator with 3-char abbreviations (right)
    {
      juce::Colour groupColors[4] = {
          juce::Colour(OCC::Design::kGroupBlue), juce::Colour(OCC::Design::kGroupGreen),
          juce::Colour(OCC::Design::kGroupOrange), juce::Colour(OCC::Design::kGroupRed)};

      // Reserve 3 characters of width (consistent with other indicators)
      auto groupBadge = bottomArea.removeFromRight(kIndicatorBoxWidth)
                            .withTrimmedTop(4.0f)
                            .withHeight(kIndicatorBoxHeight);

      // Draw group badge background
      g.setColour(groupColors[m_clipGroup].withAlpha(kGroupBadgeAlpha));
      g.fillRoundedRectangle(groupBadge, OCC::Design::kRadiusMD);

      // Draw group abbreviation (3 chars max)
      // TODO: Get abbreviation from SessionManager when available
      juce::String groupText = "G" + juce::String(m_clipGroup + 1);

      g.setColour(juce::Colours::white);
      g.setFont(juce::FontOptions("HK Grotesk", kFontGroupLabel, juce::Font::bold));
      g.drawText(groupText, groupBadge, juce::Justification::centred, false);
    }
  }

  // === PROGRESS BAR ===
  if ((m_state == State::Playing || m_state == State::Stopping) && m_playbackProgress > 0.0f) {
    // Draw progress bar at the very bottom of the button
    auto progressArea = bounds.removeFromBottom(OCC::Design::kBorderThick).reduced(1.0f, 0.0f);
    float progressWidth = progressArea.getWidth() * m_playbackProgress;

    // Background (darker)
    g.setColour(juce::Colours::black.withAlpha(kProgressBgAlpha));
    g.fillRoundedRectangle(progressArea, kPlayboxBorderWidth);

    // Progress fill (bright accent color)
    if (progressWidth > 0.0f) {
      auto fillArea = progressArea.withWidth(progressWidth);
      g.setColour(m_state == State::Playing ? juce::Colours::cyan : juce::Colours::orange);
      g.fillRoundedRectangle(fillArea, kPlayboxBorderWidth);
    }
  }

  // === STATUS INDICATORS ===
  // Draw status icons in bottom-left corner (Feature 2 - fixed grid layout)
  // Order: [PLAY] [STOP OTHERS] [LOOP] [FADE IN] [FADE OUT] [SPEED]
  // Grid is ALWAYS drawn - blank spaces shown for inactive states
  auto indicatorArea = juce::Rectangle<float>(contentArea.getX(), contentArea.getBottom() - 16.0f,
                                              contentArea.getWidth(), 14.0f);
  drawStatusIcons(g, indicatorArea);
}

void ClipButton::drawStatusIcons(juce::Graphics& g, juce::Rectangle<float> bounds) {
  // Feature 2: Icon-based status indicators in bottom-left corner (fixed grid)
  // Order: [PLAY BOX] [STOP OTHERS] [LOOP] [FADE IN] [FADE OUT] [SPEED]
  // PLAY icon matches clip number box design (green rounded rectangle)
  using namespace OCC::Design::ClipButton;

  float xPos = bounds.getX();
  float yPos = bounds.getY();

  // Position 0: PLAY icon (green rounded rectangle, matches clip number box size)
  {
    if (m_state == State::Playing) {
      // Calculate box size (similar to clip number box in top-left)
      auto playBox = juce::Rectangle<float>(xPos, yPos, kPlayBoxWidth, kIndicatorBoxHeight);

      // Draw green rounded rectangle background (bright green)
      g.setColour(juce::Colour(OCC::Design::kAccentGreen));
      g.fillRoundedRectangle(playBox, OCC::Design::kRadiusMD);

      // Draw white play triangle inside
      juce::Path playTriangle;
      float cx = playBox.getCentreX();
      float cy = playBox.getCentreY();

      playTriangle.addTriangle(cx - kTriangleSize * 0.3f, cy - kTriangleSize * 0.5f, // Top-left
                               cx - kTriangleSize * 0.3f, cy + kTriangleSize * 0.5f, // Bottom-left
                               cx + kTriangleSize * 0.6f, cy                         // Right point
      );

      g.setColour(juce::Colours::white);
      g.fillPath(playTriangle);

      xPos += kPlayBoxWidth + kIconGap; // Advance past PLAY box
    } else {
      // Reserve space even when not playing (fixed grid)
      xPos += kPlayBoxWidth + kIconGap;
    }
  }

  // Position 1: STOP OTHERS icon (red hexagon, matches PLAY icon size)
  {
    if (m_stopOthersEnabled) {
      // Match PLAY icon dimensions
      auto stopBox = juce::Rectangle<float>(xPos, yPos, kPlayBoxWidth, kIndicatorBoxHeight);

      // Draw red hexagon (stop sign shape with flat bottom)
      juce::Path hexagon;
      float cx = stopBox.getCentreX();
      float cy = stopBox.getCentreY();

      // Create hexagon with 6 points, rotated so bottom is flat (like a stop sign)
      // Start at 0 degrees (right side) for proper stop sign orientation
      for (int i = 0; i < 6; ++i) {
        float angle = (i / 6.0f) * juce::MathConstants<float>::twoPi;
        float x = cx + kHexagonRadius * std::cos(angle);
        float y = cy + kHexagonRadius * std::sin(angle);

        if (i == 0)
          hexagon.startNewSubPath(x, y);
        else
          hexagon.lineTo(x, y);
      }
      hexagon.closeSubPath();

      // Fill with red
      g.setColour(juce::Colour(OCC::Design::kMeterRed));
      g.fillPath(hexagon);

      // Thin white border
      g.setColour(juce::Colours::white);
      g.strokePath(hexagon, juce::PathStrokeType(OCC::Design::kBorderThin));

      xPos += kPlayBoxWidth + kIconGap;
    } else {
      // Reserve space even when not enabled (fixed grid)
      xPos += kPlayBoxWidth + kIconGap;
    }
  }

  // Position 2: LOOP icon
  {
    auto iconBounds = juce::Rectangle<float>(xPos, yPos, kSmallIconSize, kSmallIconSize);

    if (m_loopEnabled) {
      // Draw circular arrow (loop symbol)
      juce::Path loopPath;
      float cx = iconBounds.getCentreX();
      float cy = iconBounds.getCentreY();

      // Draw circular arc (270 degrees)
      loopPath.addCentredArc(cx, cy, kIconRadius, kIconRadius, 0.0f,
                             juce::MathConstants<float>::pi * 0.5f,
                             juce::MathConstants<float>::pi * 2.25f, true);

      // Add arrow head
      float arrowX = cx + kIconRadius * std::cos(juce::MathConstants<float>::pi * 2.25f);
      float arrowY = cy + kIconRadius * std::sin(juce::MathConstants<float>::pi * 2.25f);
      loopPath.lineTo(arrowX - 2.0f, arrowY - 2.0f);
      loopPath.startNewSubPath(arrowX, arrowY);
      loopPath.lineTo(arrowX + 2.0f, arrowY - 2.0f);

      // Draw with white/yellow color
      g.setColour(juce::Colour(OCC::Design::kAccentYellow).withAlpha(kLoadedAlpha));
      g.strokePath(loopPath, juce::PathStrokeType(kPlayboxBorderWidth));
    }
    // Else: blank space

    xPos += kSmallIconSize + kIconGap;
  }

  // Position 3: FADE IN icon
  {
    auto iconBounds = juce::Rectangle<float>(xPos, yPos, kSmallIconSize, kSmallIconSize);

    if (m_fadeInEnabled) {
      // Draw fade in ramp (ascending line)
      juce::Path fadePath;
      fadePath.startNewSubPath(iconBounds.getX() + 2.0f, iconBounds.getBottom() - 2.0f);
      fadePath.lineTo(iconBounds.getRight() - 2.0f, iconBounds.getY() + 2.0f);

      // Draw with cyan color
      g.setColour(juce::Colour(OCC::Design::kAccentCyan).withAlpha(kLoadedAlpha));
      g.strokePath(fadePath, juce::PathStrokeType(OCC::Design::kBorderMedium));
    }
    // Else: blank space

    xPos += kSmallIconSize + kIconGap;
  }

  // Position 4: FADE OUT icon
  {
    auto iconBounds = juce::Rectangle<float>(xPos, yPos, kSmallIconSize, kSmallIconSize);

    if (m_fadeOutEnabled) {
      // Draw fade out ramp (descending line)
      juce::Path fadePath;
      fadePath.startNewSubPath(iconBounds.getX() + 2.0f, iconBounds.getY() + 2.0f);
      fadePath.lineTo(iconBounds.getRight() - 2.0f, iconBounds.getBottom() - 2.0f);

      // Draw with orange color
      g.setColour(juce::Colour(OCC::Design::kAccentOrange).withAlpha(kLoadedAlpha));
      g.strokePath(fadePath, juce::PathStrokeType(OCC::Design::kBorderMedium));
    }
    // Else: blank space

    xPos += kSmallIconSize + kIconGap;
  }

  // Position 5: SPEED icon (placeholder - reserved for future)
  {
    // Future: if (speedModifier != 100) { draw speed icon }
    // For now: always blank
    (void)xPos; // Suppress unused variable warning
  }
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
