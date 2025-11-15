// SPDX-License-Identifier: MIT

#include "ClipGrid.h"

//==============================================================================
ClipGrid::ClipGrid() {
  createButtons();

  // CRITICAL: Start timer immediately for atomic state synchronization
  // Timer runs continuously at 75fps to sync button states with SDK
  // This ensures buttons reflect playback from ANY source:
  // - Main grid keyboard/click
  // - Edit Dialog play/stop
  // - SPACE bar (stop all)
  startTimer(13); // 75 FPS (13ms interval)

  // Item 60: Initialize playbox on first button
  setPlayboxIndex(0);
}

//==============================================================================
void ClipGrid::createButtons() {
  // Remove existing buttons from component
  for (auto& button : m_buttons) {
    if (button) {
      removeChildComponent(button.get());
    }
  }
  m_buttons.clear();

  // Create buttons based on current grid size
  int buttonCount = m_columns * m_rows;
  for (int i = 0; i < buttonCount; ++i) {
    auto button = std::make_unique<ClipButton>(i);

    // Wire up callbacks
    button->onClick = [this](int index) { handleButtonLeftClick(index); };
    button->onRightClick = [this](int index) { handleButtonRightClick(index); };
    button->onEditDialogRequested = [this](int index) {
      if (onButtonEditDialogRequested) {
        onButtonEditDialogRequested(index);
      }
    };
    // Note: onDoubleClick removed - clip buttons prioritize single-click for PLAY/STOP
    // Use right-click menu or Ctrl+Opt+Cmd+Click to access Edit Dialog
    button->onDragToButton = [this](int sourceIndex, int targetIndex) {
      if (onButtonDraggedToButton) {
        onButtonDraggedToButton(sourceIndex, targetIndex);
      }
    };

    // All buttons start empty - clips will be loaded by SessionManager
    addAndMakeVisible(button.get());
    m_buttons.push_back(std::move(button));
  }
}

ClipButton* ClipGrid::getButton(int index) {
  if (index >= 0 && index < static_cast<int>(m_buttons.size()))
    return m_buttons[static_cast<size_t>(index)].get();
  return nullptr;
}

//==============================================================================
void ClipGrid::setGridSize(int columns, int rows) {
  // Validate grid size constraints (Item 22: 5×4 to 12×8)
  columns = juce::jlimit(MIN_COLUMNS, MAX_COLUMNS, columns);
  rows = juce::jlimit(MIN_ROWS, MAX_ROWS, rows);

  // Only recreate if size actually changed
  if (columns == m_columns && rows == m_rows) {
    return;
  }

  DBG("ClipGrid: Resizing from " << m_columns << "×" << m_rows << " to " << columns << "×" << rows);

  // Store old playbox position (button index)
  int oldPlayboxIndex = m_playboxIndex;

  // Update dimensions
  m_columns = columns;
  m_rows = rows;

  // Recreate buttons for new grid size
  createButtons();

  // Restore playbox to same index if valid, otherwise reset to 0
  if (oldPlayboxIndex < m_columns * m_rows) {
    setPlayboxIndex(oldPlayboxIndex);
  } else {
    setPlayboxIndex(0);
  }

  // Re-layout buttons
  resized();
}

//==============================================================================
void ClipGrid::setHasActiveClips(bool hasActive) {
  if (hasActive != m_hasActiveClips) {
    m_hasActiveClips = hasActive;

    if (m_hasActiveClips) {
      startTimer(13); // Start 75fps updates when clips are active
      DBG("ClipGrid: Started 75fps timer (clips active)");
    } else {
      stopTimer(); // Stop 75fps updates when no clips active
      DBG("ClipGrid: Stopped 75fps timer (no active clips)");
    }
  }
}

//==============================================================================
void ClipGrid::handleButtonLeftClick(int buttonIndex) {
  DBG("ClipGrid: Button " + juce::String(buttonIndex) + " left-clicked");

  // Forward to MainComponent via callback
  if (onButtonClicked)
    onButtonClicked(buttonIndex);
}

void ClipGrid::handleButtonRightClick(int buttonIndex) {
  DBG("ClipGrid: Button " + juce::String(buttonIndex) + " right-clicked");

  // Forward to MainComponent via callback
  if (onButtonRightClicked)
    onButtonRightClicked(buttonIndex);
}

//==============================================================================
void ClipGrid::paint(juce::Graphics& g) {
  // Grid background
  g.fillAll(juce::Colour(0xff1a1a1a)); // Very dark grey
}

void ClipGrid::resized() {
  auto bounds = getLocalBounds();

  // Calculate button size based on grid dimensions
  // Item 22: Buttons stretch to fill available space
  // Constraint (width > height) is enforced by only offering valid grid dimension combinations
  int availableWidth = bounds.getWidth() - (GAP * (m_columns + 1));
  int availableHeight = bounds.getHeight() - (GAP * (m_rows + 1));

  int buttonWidth = availableWidth / m_columns;
  int buttonHeight = availableHeight / m_rows;

  // Layout buttons in grid
  for (int row = 0; row < m_rows; ++row) {
    for (int col = 0; col < m_columns; ++col) {
      int index = row * m_columns + col;
      auto button = getButton(index);

      if (button) {
        int x = GAP + col * (buttonWidth + GAP);
        int y = GAP + row * (buttonHeight + GAP);

        button->setBounds(x, y, buttonWidth, buttonHeight);
      }
    }
  }
}

//==============================================================================
bool ClipGrid::isInterestedInFileDrag(const juce::StringArray& files) {
  // Accept any audio files
  for (const auto& file : files) {
    if (file.endsWithIgnoreCase(".wav") || file.endsWithIgnoreCase(".aiff") ||
        file.endsWithIgnoreCase(".aif") || file.endsWithIgnoreCase(".flac")) {
      return true;
    }
  }
  return false;
}

void ClipGrid::filesDropped(const juce::StringArray& files, int x, int y) {
  // Find which button was dropped on
  int targetButtonIndex = -1;
  int buttonCount = m_columns * m_rows;

  for (int i = 0; i < buttonCount; ++i) {
    auto button = getButton(i);
    if (button && button->getBounds().contains(x, y)) {
      targetButtonIndex = i;
      break;
    }
  }

  // If dropped on a button, load files starting from that button
  // Otherwise, load starting from first empty button
  if (targetButtonIndex < 0) {
    targetButtonIndex = 0; // Default to first button
  }

  // Convert StringArray to Array<File>
  juce::Array<juce::File> audioFiles;
  for (const auto& filePath : files) {
    juce::File file(filePath);
    if (file.existsAsFile()) {
      audioFiles.add(file);
    }
  }

  if (audioFiles.isEmpty()) {
    DBG("ClipGrid: No valid audio files dropped");
    return;
  }

  DBG("ClipGrid: " << audioFiles.size() << " file(s) dropped on button " << targetButtonIndex);

  // Forward to MainComponent via callback
  if (onFilesDropped) {
    onFilesDropped(audioFiles, targetButtonIndex);
  }
}

//==============================================================================
// Playbox navigation (Item 60: Arrow key navigation)
void ClipGrid::setPlayboxIndex(int index) {
  int buttonCount = m_columns * m_rows;
  if (index < 0 || index >= buttonCount)
    return;

  // Clear old playbox
  if (m_playboxIndex >= 0 && m_playboxIndex < buttonCount) {
    if (auto* oldButton = getButton(m_playboxIndex)) {
      oldButton->setIsPlaybox(false);
    }
  }

  // Set new playbox
  m_playboxIndex = index;
  if (auto* newButton = getButton(m_playboxIndex)) {
    newButton->setIsPlaybox(true);
  }
}

void ClipGrid::movePlayboxUp() {
  int newIndex = m_playboxIndex - m_columns;
  if (newIndex >= 0) {
    setPlayboxIndex(newIndex);
  } else {
    // Wrap to same column in last row
    int column = m_playboxIndex % m_columns;
    int lastRowStart = (m_rows - 1) * m_columns;
    setPlayboxIndex(lastRowStart + column);
  }
}

void ClipGrid::movePlayboxDown() {
  int buttonCount = m_columns * m_rows;
  int newIndex = m_playboxIndex + m_columns;
  if (newIndex < buttonCount) {
    setPlayboxIndex(newIndex);
  } else {
    // Wrap to same column in first row
    int column = m_playboxIndex % m_columns;
    setPlayboxIndex(column);
  }
}

void ClipGrid::movePlayboxLeft() {
  // Wrap to previous row when at leftmost column
  if (m_playboxIndex > 0) {
    setPlayboxIndex(m_playboxIndex - 1);
  } else {
    // Wrap to last button
    int buttonCount = m_columns * m_rows;
    setPlayboxIndex(buttonCount - 1);
  }
}

void ClipGrid::movePlayboxRight() {
  // Wrap to next row when at rightmost column
  int buttonCount = m_columns * m_rows;
  if (m_playboxIndex < buttonCount - 1) {
    setPlayboxIndex(m_playboxIndex + 1);
  } else {
    // Wrap to first button
    setPlayboxIndex(0);
  }
}

void ClipGrid::triggerPlayboxButton() {
  int buttonCount = m_columns * m_rows;
  if (m_playboxIndex >= 0 && m_playboxIndex < buttonCount) {
    // Trigger the button at playbox position
    handleButtonLeftClick(m_playboxIndex);
  }
}

//==============================================================================
void ClipGrid::timerCallback() {
  // Sync button states from AudioEngine at 75fps (broadcast standard timing)
  // Timer runs continuously to ensure atomic state synchronization from ANY source:
  // - Main grid keyboard/click
  // - Edit Dialog play/stop
  // - SPACE bar (stop all)
  int buttonCount = static_cast<int>(m_buttons.size());
  for (int i = 0; i < buttonCount; ++i) {
    auto button = getButton(i);
    if (!button)
      continue;

    // CRITICAL: Check if clip still exists (prevents orphaned play states)
    if (hasClip) {
      bool clipExists = hasClip(i);
      auto currentState = button->getState();

      // If clip was removed, ensure button goes to Empty state
      if (!clipExists && currentState != ClipButton::State::Empty) {
        button->setState(ClipButton::State::Empty);
        button->clearClip(); // Clear visual indicators
        // setState and clearClip already call repaint internally
        continue; // Skip to next button
      }

      // If no clip, skip playback state check
      if (!clipExists)
        continue;
    }

    // Query AudioEngine for atomic playback state
    if (getClipState) {
      auto sdkState = getClipState(i);
      auto currentState = button->getState();

      // CRITICAL: Only update button state if it doesn't conflict with user actions
      // - Playing → set button to Playing (clip started elsewhere, e.g., Edit Dialog)
      // - Stopped → set button to Loaded (natural clip end)
      // - Stopping → DO NOTHING (user already stopped, let fade finish without overriding)
      if (sdkState == orpheus::PlaybackState::Playing &&
          currentState != ClipButton::State::Playing) {
        button->setState(ClipButton::State::Playing);
        // setState already calls repaint internally
      } else if (sdkState == orpheus::PlaybackState::Stopped &&
                 currentState == ClipButton::State::Playing) {
        // Clip stopped (fade complete) - reset to Loaded (NOT Empty)
        button->setState(ClipButton::State::Loaded);
        // setState already calls repaint internally
      }
      // If sdkState == Stopping, do NOT override button state (user already set it to Loaded)
    }

    // CRITICAL: Sync clip metadata indicators at 75fps (loop, fade, stop-others)
    // This ensures clip states persist and follow clips during drag-to-reorder
    if (getClipStates) {
      bool loopEnabled = false;
      bool fadeInEnabled = false;
      bool fadeOutEnabled = false;
      bool stopOthersEnabled = false;

      getClipStates(i, loopEnabled, fadeInEnabled, fadeOutEnabled, stopOthersEnabled);

      // Update button indicators (these are CLIP properties, not button properties)
      // These setters already check for changes and only repaint if needed
      button->setLoopEnabled(loopEnabled);
      button->setFadeInEnabled(fadeInEnabled);
      button->setFadeOutEnabled(fadeOutEnabled);
      button->setStopOthersEnabled(stopOthersEnabled);
    }

    // Update playback progress for playing clips (75fps smooth animation)
    if (getClipPosition && button->getState() == ClipButton::State::Playing) {
      float progress = getClipPosition(i);
      button->setPlaybackProgress(progress);
      // setPlaybackProgress already calls repaint() internally for playing clips
      // No additional repaint needed here
    }

    // NOTE: All state setters (setState, setLoopEnabled, etc.) already handle repaint internally
    // No manual repaint() calls needed - everything goes through setters that manage their own
    // repainting
  }
}
