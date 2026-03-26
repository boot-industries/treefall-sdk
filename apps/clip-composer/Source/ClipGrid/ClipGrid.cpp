// SPDX-License-Identifier: MIT

#include "ClipGrid.h"

//==============================================================================
ClipGrid::ClipGrid() {
  createButtons();

  // Keep the 75fps polling model running continuously so the grid stays in sync
  // with shared playback state regardless of which controller initiated the change.
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
  m_hasActiveClips = hasActive;

  if (!isTimerRunning()) {
    startTimer(13);
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
// Display preferences pass-through
void ClipGrid::setBevelWidthPercent(float percent) {
  for (auto& button : m_buttons) {
    if (button)
      button->setBevelWidthPercent(percent);
  }
}

void ClipGrid::setButtonTextMode(int mode) {
  for (auto& button : m_buttons) {
    if (button)
      button->setButtonTextMode(mode);
  }
}

//==============================================================================
void ClipGrid::timerCallback() {
  // Poll the consolidated clip snapshot at 75fps so button visuals stay coherent
  // without the grid reaching into multiple services directly.
  // Repaint gating: only repaint buttons whose visual snapshot changed (P3-10).
  int buttonCount = static_cast<int>(m_buttons.size());
  for (int i = 0; i < buttonCount; ++i) {
    auto button = getButton(i);
    if (!button)
      continue;

    occ::ui::ClipUiSnapshot snapshot;
    if (!getClipSnapshot || !getClipSnapshot(i, snapshot)) {
      continue;
    }

    const bool snapshotChanged = !snapshot.visuallyEquals(m_prevSnapshots[i]);
    m_prevSnapshots[i] = snapshot;

    auto currentState = button->getState();

    if (!snapshot.hasClip) {
      if (currentState != ClipButton::State::Empty) {
        button->setState(ClipButton::State::Empty);
        button->clearClip();
      }
      continue;
    }

    bool stateChanged = false;
    if (snapshot.playbackState == orpheus::PlaybackState::Playing &&
        currentState != ClipButton::State::Playing) {
      button->setState(ClipButton::State::Playing);
      stateChanged = true;
    } else if (snapshot.playbackState == orpheus::PlaybackState::Stopped &&
               currentState == ClipButton::State::Playing) {
      button->setState(ClipButton::State::Loaded);
      stateChanged = true;
    }

    button->setLoopEnabled(snapshot.loopEnabled);
    button->setFadeInEnabled(snapshot.fadeInEnabled);
    button->setFadeOutEnabled(snapshot.fadeOutEnabled);
    button->setStopOthersEnabled(snapshot.stopOthersEnabled);

    if (button->getState() == ClipButton::State::Playing) {
      button->setPlaybackProgress(snapshot.playbackProgress);
    }

    // Only repaint if something visually changed — avoids up to 48 unnecessary
    // repaint regions per timer tick (freqfinder PartialButton pattern).
    if (snapshotChanged || stateChanged) {
      button->repaint();
    }
  }
}
