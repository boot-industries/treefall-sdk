// SPDX-License-Identifier: MIT

#include "ClipButton.h"
#include "ClipGrid.h"

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

  // Background color based on state
  juce::Colour bgColor;
  juce::Colour borderColor;

  switch (m_state) {
  case State::Empty:
    bgColor = juce::Colour(0xff2a2a2a);     // Dark grey
    borderColor = juce::Colour(0xff404040); // Slightly lighter border
    break;

  case State::Loaded:
    bgColor = m_clipColor.withAlpha(0.9f); // 90% opacity clip color
    borderColor = m_clipColor.darker(0.2f);
    break;

  case State::Playing:
    // Glowing pulsing border instead of bright fill (preserve clip color, only animate border)
    bgColor = m_clipColor.withAlpha(0.9f); // Keep 90% opacity clip color
    borderColor = juce::Colours::white;    // White glowing border
    break;

  case State::Stopping:
    bgColor = m_clipColor.withAlpha(0.9f); // 90% opacity
    borderColor = juce::Colours::orange;   // Orange border during fade-out
    break;
  }

  // Draw button background with rounded corners
  g.setColour(bgColor);
  g.fillRoundedRectangle(bounds.reduced(1.0f), CORNER_RADIUS);

  // Draw border (animated for Playing state)
  if (m_state == State::Playing) {
    // Glowing pulsing border for playing state
    // Use timestamp for pulsing animation
    auto now = juce::Time::getMillisecondCounterHiRes();
    float pulsePhase = std::fmod(now / 500.0, 1.0); // Fast pulse: 500ms cycle
    float pulseAlpha = 0.6f + 0.4f * std::sin(pulsePhase * juce::MathConstants<float>::twoPi);

    // Draw thick glowing border
    g.setColour(borderColor.withAlpha(pulseAlpha));
    g.drawRoundedRectangle(bounds.reduced(1.0f), CORNER_RADIUS,
                           5.0f); // Thick border for prominent glow

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
    g.drawRoundedRectangle(bounds.reduced(0.5f), CORNER_RADIUS, 1.5f); // Thin white outline
  }

  if (m_state == State::Empty) {
    // Button index (larger, more prominent) - 20% increase: 18 -> 21.6
    // Feature 4: Use consecutive numbering across tabs
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(juce::FontOptions("HK Grotesk", 21.6f, juce::Font::bold));
    g.drawText(juce::String(getDisplayNumber()), bounds, juce::Justification::centred, false);

    // No "Empty" text - just the button number on grey background is sufficient
  } else {
    // Modern HUD layout for loaded clips
    drawClipHUD(g, bounds);
  }
}

void ClipButton::drawClipHUD(juce::Graphics& g, juce::Rectangle<float> bounds) {
  auto contentArea = bounds.reduced(PADDING);
  float currentY = contentArea.getY();

  // Determine text color based on background brightness
  // Skew towards white/light text (dark mode app) - only use black on VERY light backgrounds
  juce::Colour bgColor;
  switch (m_state) {
  case State::Loaded:
    bgColor = m_clipColor.withAlpha(0.9f);
    break;
  case State::Playing:
    bgColor = m_clipColor.withAlpha(0.9f);
    break;
  case State::Stopping:
    bgColor = m_clipColor.withAlpha(0.9f);
    break;
  default:
    bgColor = juce::Colours::darkgrey;
    break;
  }

  // Use black text ONLY on extremely light backgrounds (>0.8), white otherwise
  // This ensures readability while strongly favoring white text for dark mode aesthetic
  float brightness = bgColor.getBrightness();
  juce::Colour textColor =
      brightness > 0.8f ? juce::Colours::black.withAlpha(0.95f) : juce::Colours::white;
  juce::Colour subtleTextColor = textColor.withAlpha(0.6f);
  juce::Colour prominentTextColor = textColor.withAlpha(0.95f);

  // === TOP ROW: Button Index (white rounded box) + Keyboard Shortcut ===
  {
    auto topRow = contentArea.removeFromTop(16.0f);

    // Button index in white rounded rectangle (Feature 3 - tighter margins)
    // Feature 4: Use consecutive numbering across tabs
    juce::String buttonNumber = juce::String(getDisplayNumber());
    auto numberFont = juce::FontOptions("HK Grotesk", 12.0f, juce::Font::bold);
    g.setFont(numberFont);

    // Reserve 3 characters of width (e.g., "960") - fixed width for consistent layout
    float boxWidth = 36.0f; // Fixed width for up to 3 digits
    float boxHeight = 16.0f;

    auto numberBox = topRow.removeFromLeft(boxWidth).withHeight(boxHeight);

    // Draw white rounded rectangle background (OCC130 Sprint A.4: 4px corner radius)
    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.fillRoundedRectangle(numberBox, 4.0f); // OCC130: Consistent 4px radius

    // Draw black text
    g.setColour(juce::Colours::black);
    g.drawText(buttonNumber, numberBox, juce::Justification::centred, false);

    // OCC130 Sprint A.3: Beat indicator next to clip number (top-left)
    // Format: " // {value}" (e.g., " // 3+")
    if (m_beatOffset.isNotEmpty()) {
      g.setColour(textColor); // Use adaptive text color (white/black based on bg)
      g.setFont(juce::FontOptions("HK Grotesk", 11.0f, juce::Font::plain)); // Thin/light weight
      juce::String beatDisplay = " // " + m_beatOffset;
      auto beatArea = topRow.removeFromLeft(40.0f); // Reserve space for beat display
      g.drawText(beatDisplay, beatArea, juce::Justification::centredLeft, false);
    }

    // OCC130 Sprint A.4: Keyboard shortcut indicator (top-right corner)
    // Thin outline box, transparent background, adaptive color
    if (m_keyboardShortcut.isNotEmpty()) {
      // Reserve 3 characters of width (e.g., "F12") - fixed width for consistent layout
      auto hotkeyFont = juce::FontOptions("HK Grotesk", 12.0f, juce::Font::bold);
      g.setFont(hotkeyFont);

      float hotkeyBoxWidth = 36.0f; // Fixed width for up to 3 characters
      float hotkeyBoxHeight = 16.0f;

      auto hotkeyBox = topRow.removeFromRight(hotkeyBoxWidth).withHeight(hotkeyBoxHeight);

      // Draw thin outline (1.5px, adaptive color)
      // Light outline on dark buttons, dark outline on light buttons
      juce::Colour outlineColor = brightness > 0.8f ? juce::Colours::black.withAlpha(0.6f)
                                                    : juce::Colours::white.withAlpha(0.8f);

      g.setColour(outlineColor);
      g.drawRoundedRectangle(hotkeyBox, 4.0f, 1.5f); // 4px radius, 1.5px stroke

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
    g.setColour(textColor);
    g.setFont(juce::FontOptions("HK Grotesk", 18.0f, juce::Font::bold));

    // Reserve minimal space for duration
    auto nameOnlyArea = nameArea.withTrimmedBottom(12.0f);
    g.drawFittedText(m_clipName, nameOnlyArea.toNearestInt(), juce::Justification::centred,
                     3, // Allow up to 3 lines for name
                     0.85f);

    // OCC130 Sprint A.2: Time display - show elapsed — remaining when playing, total when stopped
    // Font size: 15.0px (reduced ~15% from original 18px)
    auto durationArea = nameArea.removeFromBottom(22.0f); // Increased height for larger font
    if (m_durationSeconds > 0.0) {
      g.setFont(juce::FontOptions("HK Grotesk", 15.0f, juce::Font::plain));

      juce::String timeDisplay;

      // Show different formats based on playback state
      if (m_state == State::Playing || m_state == State::Stopping) {
        // Playing/Stopping: show elapsed — remaining
        // Elapsed = current playhead position within trimmed region (IN to OUT)
        double elapsed = m_durationSeconds * m_playbackProgress;
        double remaining = m_durationSeconds - elapsed;

        // Format: "MM:SS — MM:SS" (HH:MM:SS if >60 min)
        timeDisplay = formatDuration(elapsed) + " — " + formatDuration(remaining);

        // Color: green when playing, orange when stopping
        g.setColour(m_state == State::Playing ? juce::Colour(0xff00ff00).withAlpha(0.9f)
                                              : juce::Colour(0xffff8800).withAlpha(0.9f));
      } else {
        // Loaded/Empty: show total duration only
        timeDisplay = formatDuration(m_durationSeconds);

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

    // Item 29: Clip group indicator with 3-char abbreviations (right) - e.g., "MUS", "SFX", "VOC",
    // "G1"
    {
      juce::Colour groupColors[4] = {
          juce::Colour(0xff3498db), // Blue - Group 0
          juce::Colour(0xff2ecc71), // Green - Group 1
          juce::Colour(0xfff39c12), // Orange - Group 2
          juce::Colour(0xffe74c3c)  // Red - Group 3
      };

      // Reserve 3 characters of width (consistent with other indicators)
      auto groupBadge = bottomArea.removeFromRight(36.0f).withTrimmedTop(4.0f).withHeight(16.0f);

      // Draw group badge background (OCC130 Sprint A.4: 4px corner radius)
      g.setColour(groupColors[m_clipGroup].withAlpha(0.8f));
      g.fillRoundedRectangle(groupBadge, 4.0f); // OCC130: Consistent 4px radius

      // Draw group abbreviation (3 chars max)
      // TODO: Get abbreviation from SessionManager when available
      // For now, use default format
      juce::String groupText = "G" + juce::String(m_clipGroup + 1);

      g.setColour(juce::Colours::white);
      g.setFont(juce::FontOptions("HK Grotesk", 10.8f, juce::Font::bold));
      g.drawText(groupText, groupBadge, juce::Justification::centred, false);
    }
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

  constexpr float SMALL_ICON_SIZE = 14.0f;
  constexpr float ICON_GAP = 4.0f; // Increased spacing

  float xPos = bounds.getX();
  float yPos = bounds.getY();

  // Position 0: PLAY icon (green rounded rectangle, matches clip number box size)
  {
    if (m_state == State::Playing) {
      // Calculate box size (similar to clip number box in top-left)
      float boxWidth = 24.0f;  // Slightly wider for visibility
      float boxHeight = 16.0f; // Same height as clip number box
      auto playBox = juce::Rectangle<float>(xPos, yPos, boxWidth, boxHeight);

      // Draw green rounded rectangle background (bright green)
      g.setColour(juce::Colour(0xff00ff00)); // Bright green
      g.fillRoundedRectangle(playBox, 4.0f); // OCC130 Sprint A.4: Consistent 4px radius

      // Draw white play triangle inside
      juce::Path playTriangle;
      float cx = playBox.getCentreX();
      float cy = playBox.getCentreY();
      float size = 7.0f; // Triangle size

      playTriangle.addTriangle(cx - size * 0.3f, cy - size * 0.5f, // Top-left
                               cx - size * 0.3f, cy + size * 0.5f, // Bottom-left
                               cx + size * 0.6f, cy                // Right point
      );

      g.setColour(juce::Colours::white);
      g.fillPath(playTriangle);

      xPos += boxWidth + ICON_GAP; // Advance past PLAY box
    } else {
      // Reserve space even when not playing (fixed grid)
      xPos += 24.0f + ICON_GAP;
    }
  }

  // Position 1: STOP OTHERS icon (red hexagon, matches PLAY icon size)
  {
    if (m_stopOthersEnabled) {
      // Match PLAY icon dimensions
      float boxWidth = 24.0f;
      float boxHeight = 16.0f;
      auto stopBox = juce::Rectangle<float>(xPos, yPos, boxWidth, boxHeight);

      // Draw red hexagon (stop sign shape with flat bottom)
      juce::Path hexagon;
      float cx = stopBox.getCentreX();
      float cy = stopBox.getCentreY();
      float radius = 7.0f;

      // Create hexagon with 6 points, rotated so bottom is flat (like a stop sign)
      // Start at 0 degrees (right side) for proper stop sign orientation
      for (int i = 0; i < 6; ++i) {
        float angle = (i / 6.0f) * juce::MathConstants<float>::twoPi;
        float x = cx + radius * std::cos(angle);
        float y = cy + radius * std::sin(angle);

        if (i == 0)
          hexagon.startNewSubPath(x, y);
        else
          hexagon.lineTo(x, y);
      }
      hexagon.closeSubPath();

      // Fill with red
      g.setColour(juce::Colour(0xffff0000)); // Bright red
      g.fillPath(hexagon);

      // Thin white border
      g.setColour(juce::Colours::white);
      g.strokePath(hexagon, juce::PathStrokeType(1.0f));

      xPos += boxWidth + ICON_GAP;
    } else {
      // Reserve space even when not enabled (fixed grid)
      xPos += 24.0f + ICON_GAP;
    }
  }

  // Position 2: LOOP icon
  {
    auto iconBounds = juce::Rectangle<float>(xPos, yPos, SMALL_ICON_SIZE, SMALL_ICON_SIZE);

    if (m_loopEnabled) {
      // Draw circular arrow (loop symbol)
      juce::Path loopPath;
      float cx = iconBounds.getCentreX();
      float cy = iconBounds.getCentreY();
      float radius = 5.0f;

      // Draw circular arc (270 degrees)
      loopPath.addCentredArc(cx, cy, radius, radius, 0.0f, juce::MathConstants<float>::pi * 0.5f,
                             juce::MathConstants<float>::pi * 2.25f, true);

      // Add arrow head
      float arrowX = cx + radius * std::cos(juce::MathConstants<float>::pi * 2.25f);
      float arrowY = cy + radius * std::sin(juce::MathConstants<float>::pi * 2.25f);
      loopPath.lineTo(arrowX - 2.0f, arrowY - 2.0f);
      loopPath.startNewSubPath(arrowX, arrowY);
      loopPath.lineTo(arrowX + 2.0f, arrowY - 2.0f);

      // Draw with white/yellow color
      g.setColour(juce::Colour(0xffffff00).withAlpha(0.9f)); // Yellow
      g.strokePath(loopPath, juce::PathStrokeType(1.5f));
    }
    // Else: blank space

    xPos += SMALL_ICON_SIZE + ICON_GAP;
  }

  // Position 3: FADE IN icon
  {
    auto iconBounds = juce::Rectangle<float>(xPos, yPos, SMALL_ICON_SIZE, SMALL_ICON_SIZE);

    if (m_fadeInEnabled) {
      // Draw fade in ramp (ascending line)
      juce::Path fadePath;
      fadePath.startNewSubPath(iconBounds.getX() + 2.0f, iconBounds.getBottom() - 2.0f);
      fadePath.lineTo(iconBounds.getRight() - 2.0f, iconBounds.getY() + 2.0f);

      // Draw with cyan color
      g.setColour(juce::Colour(0xff00ffff).withAlpha(0.9f)); // Cyan
      g.strokePath(fadePath, juce::PathStrokeType(2.0f));
    }
    // Else: blank space

    xPos += SMALL_ICON_SIZE + ICON_GAP;
  }

  // Position 4: FADE OUT icon
  {
    auto iconBounds = juce::Rectangle<float>(xPos, yPos, SMALL_ICON_SIZE, SMALL_ICON_SIZE);

    if (m_fadeOutEnabled) {
      // Draw fade out ramp (descending line)
      juce::Path fadePath;
      fadePath.startNewSubPath(iconBounds.getX() + 2.0f, iconBounds.getY() + 2.0f);
      fadePath.lineTo(iconBounds.getRight() - 2.0f, iconBounds.getBottom() - 2.0f);

      // Draw with orange color
      g.setColour(juce::Colour(0xffff8800).withAlpha(0.9f)); // Orange
      g.strokePath(fadePath, juce::PathStrokeType(2.0f));
    }
    // Else: blank space

    xPos += SMALL_ICON_SIZE + ICON_GAP;
  }

  // Position 5: SPEED icon (placeholder - reserved for future)
  {
    auto iconBounds = juce::Rectangle<float>(xPos, yPos, SMALL_ICON_SIZE, SMALL_ICON_SIZE);

    // Future: if (speedModifier != 100) { draw speed icon }
    // For now: always blank

    // xPos += SMALL_ICON_SIZE + ICON_GAP; // Uncomment if adding more icons
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
