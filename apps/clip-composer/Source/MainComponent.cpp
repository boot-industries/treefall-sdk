// SPDX-License-Identifier: MIT

#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent() {
  // Set Inter font as default for all components
  setLookAndFeel(&m_interLookAndFeel);

  // Create tab switcher (8 tabs for 384 total clips)
  m_tabSwitcher = std::make_unique<TabSwitcher>();
  addAndMakeVisible(m_tabSwitcher.get());

  // Wire up tab selection callback
  m_tabSwitcher->onTabSelected = [this](int tabIndex) { onTabSelected(tabIndex); };

  // Create clip grid (6×8 = 48 buttons per tab)
  m_clipGrid = std::make_unique<ClipGrid>();
  addAndMakeVisible(m_clipGrid.get());

  // Wire up right-click handler for loading clips
  m_clipGrid->onButtonRightClicked = [this](int buttonIndex) { onClipRightClicked(buttonIndex); };

  // Wire up left-click handler for triggering clips
  m_clipGrid->onButtonClicked = [this](int buttonIndex) { onClipTriggered(buttonIndex); };

  // Wire up drag & drop handler
  m_clipGrid->onFilesDropped = [this](const juce::Array<juce::File>& files, int buttonIndex) {
    loadMultipleFiles(files, buttonIndex);
  };

  // Wire up clip drag-to-reorder handler
  m_clipGrid->onButtonDraggedToButton = [this](int sourceIndex, int targetIndex) {
    onClipDraggedToButton(sourceIndex, targetIndex);
  };

  // Make this component capture keyboard focus
  setWantsKeyboardFocus(true);

  // Create transport controls
  m_transportControls = std::make_unique<TransportControls>();
  addAndMakeVisible(m_transportControls.get());

  // Wire up transport control callbacks
  m_transportControls->onStopAll = [this]() { onStopAll(); };
  m_transportControls->onPanic = [this]() { onPanic(); };

  // Set window size (1400×900 for better screen fit)
  setSize(1400, 900);

// Set up menu bar (macOS native)
#if JUCE_MAC
  juce::MenuBarModel::setMacMainMenu(this);
#endif

  // Initialize audio engine with real SDK components
  m_audioEngine = std::make_unique<AudioEngine>();
  if (!m_audioEngine->initialize(48000)) {
    DBG("MainComponent: Failed to initialize audio engine!");
  } else {
    if (m_audioEngine->start()) {
      DBG("MainComponent: Audio engine started successfully");
    } else {
      DBG("MainComponent: Failed to start audio engine");
    }
  }

  // TODO (Month 3-4): Create RoutingPanel component
  // TODO (Month 5-6): Create WaveformDisplay component
}

MainComponent::~MainComponent() {
// Clear menu bar
#if JUCE_MAC
  juce::MenuBarModel::setMacMainMenu(nullptr);
#endif

  // Clear LookAndFeel before destruction
  setLookAndFeel(nullptr);

  // Component cleanup handled by std::unique_ptr destructors
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g) {
  // Dark background (professional broadcast look)
  g.fillAll(juce::Colour(0xff151515));

  // Header bar area (top 60px)
  g.setColour(juce::Colour(0xff252525));
  g.fillRect(0, 0, getWidth(), 60);

  // Title
  g.setColour(juce::Colours::white);
  g.setFont(juce::FontOptions("Inter", 24.0f, juce::Font::bold));
  g.drawText("Clip Composer", 20, 0, 400, 60, juce::Justification::centredLeft, false);

  // Version badge
  g.setColour(juce::Colours::orange);
  g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
  g.drawText("MVP", 280, 0, 50, 60, juce::Justification::centredLeft, false);

  // Status text (right side)
  g.setColour(juce::Colours::lightgrey);
  g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::plain));
  g.drawText("48 Buttons | Week 3 | SDK Integration Pending", getWidth() - 350, 0, 330, 60,
             juce::Justification::centredRight, false);
}

void MainComponent::resized() {
  grabKeyboardFocus(); // Ensure we get keyboard events
  auto bounds = getLocalBounds();

  // Header bar (60px)
  auto headerArea = bounds.removeFromTop(60);
  juce::ignoreUnused(headerArea);

  // Tab switcher below header (40px)
  auto tabArea = bounds.removeFromTop(40);
  if (m_tabSwitcher) {
    m_tabSwitcher->setBounds(tabArea.reduced(10, 0)); // 10px horizontal margin
  }

  // Transport controls at bottom (60px)
  auto transportArea = bounds.removeFromBottom(60);
  if (m_transportControls) {
    m_transportControls->setBounds(transportArea);
  }

  // Main content area
  auto contentArea = bounds.reduced(10); // 10px margin

  // Clip grid takes most of the space
  if (m_clipGrid) {
    m_clipGrid->setBounds(contentArea);
  }

  // Future layout:
  // - Bottom 150px: Routing panel
  // - Bottom 80px: Waveform display
  // - Remaining: Clip grid
}

bool MainComponent::keyPressed(const juce::KeyPress& key) {
  // Tab switching: Cmd+1 through Cmd+8 (Mac) or Ctrl+1 through Ctrl+8 (Windows/Linux)
  if (key.getModifiers().isCommandDown()) {
    int keyCode = key.getKeyCode();
    if (keyCode >= '1' && keyCode <= '8') {
      int tabIndex = keyCode - '1'; // Convert '1'-'8' to 0-7
      m_tabSwitcher->setActiveTab(tabIndex);
      return true;
    }
  }

  // Space bar = Stop All
  if (key == juce::KeyPress::spaceKey) {
    onStopAll();
    return true;
  }

  // Escape = PANIC
  if (key == juce::KeyPress::escapeKey) {
    onPanic();
    return true;
  }

  // Map key to button index
  int buttonIndex = getButtonIndexFromKey(key);
  if (buttonIndex >= 0 && buttonIndex < m_clipGrid->getButtonCount()) {
    onClipTriggered(buttonIndex);
    return true;
  }

  return false; // Key not handled
}

int MainComponent::getButtonIndexFromKey(const juce::KeyPress& key) const {
  // Current grid: 6 columns × 8 rows = 48 buttons
  // Keyboard layout (6 columns wide):
  //
  // Row 0: Q W E R T Y
  // Row 1: A S D F G H
  // Row 2: Z X C V B N
  // Row 3: 1 2 3 4 5 6
  // Row 4: 7 8 9 0 - =
  // Row 5: [ ] ; ' , .
  // Row 6: F1 F2 F3 F4 F5 F6
  // Row 7: F7 F8 F9 F10 F11 F12

  int keyCode = key.getKeyCode();

  // Row 0: Q W E R T Y
  if (keyCode == 'Q')
    return 0;
  if (keyCode == 'W')
    return 1;
  if (keyCode == 'E')
    return 2;
  if (keyCode == 'R')
    return 3;
  if (keyCode == 'T')
    return 4;
  if (keyCode == 'Y')
    return 5;

  // Row 1: A S D F G H
  if (keyCode == 'A')
    return 6;
  if (keyCode == 'S')
    return 7;
  if (keyCode == 'D')
    return 8;
  if (keyCode == 'F')
    return 9;
  if (keyCode == 'G')
    return 10;
  if (keyCode == 'H')
    return 11;

  // Row 2: Z X C V B N
  if (keyCode == 'Z')
    return 12;
  if (keyCode == 'X')
    return 13;
  if (keyCode == 'C')
    return 14;
  if (keyCode == 'V')
    return 15;
  if (keyCode == 'B')
    return 16;
  if (keyCode == 'N')
    return 17;

  // Row 3: 1 2 3 4 5 6
  if (keyCode == '1')
    return 18;
  if (keyCode == '2')
    return 19;
  if (keyCode == '3')
    return 20;
  if (keyCode == '4')
    return 21;
  if (keyCode == '5')
    return 22;
  if (keyCode == '6')
    return 23;

  // Row 4: 7 8 9 0 - =
  if (keyCode == '7')
    return 24;
  if (keyCode == '8')
    return 25;
  if (keyCode == '9')
    return 26;
  if (keyCode == '0')
    return 27;
  if (keyCode == '-')
    return 28;
  if (keyCode == '=')
    return 29;

  // Row 5: [ ] ; ' , .
  if (keyCode == '[')
    return 30;
  if (keyCode == ']')
    return 31;
  if (keyCode == ';')
    return 32;
  if (keyCode == '\'')
    return 33;
  if (keyCode == ',')
    return 34;
  if (keyCode == '.')
    return 35;

  // Row 6: F1 F2 F3 F4 F5 F6
  if (keyCode == juce::KeyPress::F1Key)
    return 36;
  if (keyCode == juce::KeyPress::F2Key)
    return 37;
  if (keyCode == juce::KeyPress::F3Key)
    return 38;
  if (keyCode == juce::KeyPress::F4Key)
    return 39;
  if (keyCode == juce::KeyPress::F5Key)
    return 40;
  if (keyCode == juce::KeyPress::F6Key)
    return 41;

  // Row 7: F7 F8 F9 F10 F11 F12
  if (keyCode == juce::KeyPress::F7Key)
    return 42;
  if (keyCode == juce::KeyPress::F8Key)
    return 43;
  if (keyCode == juce::KeyPress::F9Key)
    return 44;
  if (keyCode == juce::KeyPress::F10Key)
    return 45;
  if (keyCode == juce::KeyPress::F11Key)
    return 46;
  if (keyCode == juce::KeyPress::F12Key)
    return 47;

  return -1; // Key not mapped
}

juce::String MainComponent::getKeyboardShortcutForButton(int buttonIndex) const {
  // Return keyboard shortcut string for button index (inverse of getButtonIndexFromKey)
  // Current grid: 6 columns × 8 rows = 48 buttons
  //
  // Row 0 (0-5): Q W E R T Y
  // Row 1 (6-11): A S D F G H
  // Row 2 (12-17): Z X C V B N
  // Row 3 (18-23): 1 2 3 4 5 6
  // Row 4 (24-29): 7 8 9 0 - =
  // Row 5 (30-35): [ ] ; ' , .
  // Row 6 (36-41): F1 F2 F3 F4 F5 F6
  // Row 7 (42-47): F7 F8 F9 F10 F11 F12

  const char* shortcuts[] = {
      "Q",  "W",  "E",  "R",   "T",   "Y",  // Row 0 (0-5)
      "A",  "S",  "D",  "F",   "G",   "H",  // Row 1 (6-11)
      "Z",  "X",  "C",  "V",   "B",   "N",  // Row 2 (12-17)
      "1",  "2",  "3",  "4",   "5",   "6",  // Row 3 (18-23)
      "7",  "8",  "9",  "0",   "-",   "=",  // Row 4 (24-29)
      "[",  "]",  ";",  "'",   ",",   ".",  // Row 5 (30-35)
      "F1", "F2", "F3", "F4",  "F5",  "F6", // Row 6 (36-41)
      "F7", "F8", "F9", "F10", "F11", "F12" // Row 7 (42-47)
  };

  if (buttonIndex >= 0 && buttonIndex < 48)
    return juce::String(shortcuts[buttonIndex]);

  return ""; // Invalid button index
}

//==============================================================================
void MainComponent::onClipRightClicked(int buttonIndex) {
  // Show context menu (inherits Inter font from LookAndFeel)
  juce::PopupMenu menu;

  bool hasClip = m_sessionManager.hasClip(buttonIndex);

  if (hasClip) {
    // Clip is loaded - show options
    auto clipData = m_sessionManager.getClip(buttonIndex);
    menu.addItem(1, "Load New Audio File...");
    menu.addItem(6, "Load Multiple Audio Files...");
    menu.addSeparator();

    // Color submenu
    juce::PopupMenu colorMenu;
    colorMenu.addItem(100, "Red");
    colorMenu.addItem(101, "Orange");
    colorMenu.addItem(102, "Yellow");
    colorMenu.addItem(103, "Green");
    colorMenu.addItem(104, "Cyan");
    colorMenu.addItem(105, "Blue");
    colorMenu.addItem(106, "Purple");
    colorMenu.addItem(107, "Pink");
    menu.addSubMenu("Set Color", colorMenu);

    menu.addSeparator();
    menu.addItem(4, "Stop Others On Play", true, m_stopOthersOnPlay[buttonIndex]);
    menu.addSeparator();
    menu.addItem(2, "Remove Clip");
    menu.addSeparator();
    menu.addItem(3, "Clip Info: " + juce::String(clipData.displayName), false, false);
  } else {
    // Empty button - only show load option
    menu.addItem(1, "Load Audio File...");
    menu.addItem(6, "Load Multiple Audio Files...");
  }

  menu.showMenuAsync(juce::PopupMenu::Options(), [this, buttonIndex, hasClip](int result) {
    if (result == 1) {
      // Load audio file
      juce::FileChooser chooser("Select Audio File",
                                juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                                "*.wav;*.aiff;*.aif;*.flac");

      if (chooser.browseForFileToOpen()) {
        auto file = chooser.getResult();
        loadClipToButton(buttonIndex, file.getFullPathName());
      }
    } else if (result == 2 && hasClip) {
      // Remove clip
      m_sessionManager.removeClip(buttonIndex);
      updateButtonFromClip(buttonIndex);
    } else if (result == 4) {
      // Toggle "stop others on play" mode
      m_stopOthersOnPlay[buttonIndex] = !m_stopOthersOnPlay[buttonIndex];
      DBG("Button " << buttonIndex << ": Stop others on play = "
                    << (m_stopOthersOnPlay[buttonIndex] ? "ON" : "OFF"));
    } else if (result == 6) {
      // Load multiple audio files
      juce::FileChooser chooser("Select Audio Files",
                                juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                                "*.wav;*.aiff;*.aif;*.flac");

      if (chooser.browseForMultipleFilesToOpen()) {
        auto files = chooser.getResults();
        loadMultipleFiles(files, buttonIndex);
      }
    } else if (result >= 100 && result <= 107) {
      // Color selection
      juce::Colour newColor;
      switch (result) {
      case 100:
        newColor = juce::Colour(0xffe74c3c);
        break; // Red
      case 101:
        newColor = juce::Colour(0xfff39c12);
        break; // Orange
      case 102:
        newColor = juce::Colour(0xfff1c40f);
        break; // Yellow
      case 103:
        newColor = juce::Colour(0xff2ecc71);
        break; // Green
      case 104:
        newColor = juce::Colour(0xff1abc9c);
        break; // Cyan
      case 105:
        newColor = juce::Colour(0xff3498db);
        break; // Blue
      case 106:
        newColor = juce::Colour(0xff9b59b6);
        break; // Purple
      case 107:
        newColor = juce::Colour(0xffff69b4);
        break; // Pink (HotPink - lighter)
      }

      // Update button color
      auto button = m_clipGrid->getButton(buttonIndex);
      if (button) {
        button->setClipColor(newColor);
      }

      // Update session manager
      if (m_sessionManager.hasClip(buttonIndex)) {
        auto clipData = m_sessionManager.getClip(buttonIndex);
        clipData.color = newColor;
        // Note: SessionManager may need a setClipColor method, for now just update button
      }

      DBG("Button " << buttonIndex << ": Color changed to " << newColor.toString());
    }
  });
}

void MainComponent::onClipTriggered(int buttonIndex) {
  auto button = m_clipGrid->getButton(buttonIndex);
  if (!button)
    return;

  // Check if clip is loaded
  if (!m_sessionManager.hasClip(buttonIndex)) {
    DBG("MainComponent: Button " + juce::String(buttonIndex) + " has no clip loaded");
    return;
  }

  // Toggle play/stop based on current state
  auto currentState = button->getState();

  if (currentState == ClipButton::State::Playing) {
    // Stop the clip
    if (m_audioEngine) {
      m_audioEngine->stopClip(buttonIndex);
    }
    button->setState(ClipButton::State::Loaded);
    DBG("Button " + juce::String(buttonIndex) + ": Stopped via keyboard/click");
  } else if (currentState == ClipButton::State::Loaded) {
    // Check if "stop others on play" mode is enabled for this button
    if (m_stopOthersOnPlay[buttonIndex]) {
      // Stop all other playing clips
      for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
        if (i != buttonIndex) {
          auto otherButton = m_clipGrid->getButton(i);
          if (otherButton && otherButton->getState() == ClipButton::State::Playing) {
            if (m_audioEngine) {
              m_audioEngine->stopClip(i);
            }
            otherButton->setState(ClipButton::State::Loaded);
            DBG("Button " + juce::String(i) + ": Stopped by 'stop others' from button " +
                juce::String(buttonIndex));
          }
        }
      }
    }

    // Start the clip
    if (m_audioEngine) {
      m_audioEngine->startClip(buttonIndex);
    }
    button->setState(ClipButton::State::Playing);
    DBG("Button " + juce::String(buttonIndex) + ": Started playing via keyboard/click");
  }
}

void MainComponent::loadMultipleFiles(const juce::Array<juce::File>& files, int startButtonIndex) {
  // Load multiple files sequentially starting from startButtonIndex
  // Files wrap by rows: if grid is 6 columns, files fill 0-5, 6-11, 12-17, etc.
  int buttonIndex = startButtonIndex;
  int totalButtons = m_clipGrid->getButtonCount();

  for (const auto& file : files) {
    if (buttonIndex >= totalButtons) {
      DBG("MainComponent: Ran out of buttons loading files (stopped at button " << buttonIndex
                                                                                << ")");
      break;
    }

    loadClipToButton(buttonIndex, file.getFullPathName());
    buttonIndex++;
  }

  DBG("MainComponent: Loaded " << files.size() << " files starting from button "
                               << startButtonIndex);
}

void MainComponent::loadClipToButton(int buttonIndex, const juce::String& filePath) {
  // Use SessionManager to load the clip (real functionality)
  bool success = m_sessionManager.loadClip(buttonIndex, filePath);

  if (success) {
    // Load audio file into AudioEngine for playback
    if (m_audioEngine) {
      bool audioLoaded = m_audioEngine->loadClip(buttonIndex, filePath);
      if (!audioLoaded) {
        DBG("MainComponent: Failed to load audio into engine for button " << buttonIndex);
      } else {
        // Check for sample rate mismatch and warn user
        auto metadata = m_audioEngine->getClipMetadata(buttonIndex);
        if (metadata.has_value() && metadata->sample_rate != 48000) {
          juce::AlertWindow::showMessageBoxAsync(
              juce::AlertWindow::WarningIcon, "Sample Rate Mismatch",
              "Warning: This audio file is " +
                  juce::String(static_cast<int>(metadata->sample_rate)) +
                  " Hz,\n"
                  "but the engine is running at 48000 Hz.\n\n"
                  "Audio will sound distorted or at the wrong speed.\n\n"
                  "Workaround: Convert your audio files to 48 kHz using:\n"
                  "• Audacity (File > Export > 48000 Hz)\n"
                  "• ffmpeg: ffmpeg -i input.wav -ar 48000 output.wav",
              "OK");
        }
      }
    }

    // Update button visual state with real metadata
    updateButtonFromClip(buttonIndex);

    DBG("MainComponent: Successfully loaded clip to button " << buttonIndex);
  } else {
    // Show error message
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Load Failed",
                                           "Could not load audio file:\n" + filePath, "OK");
  }
}

void MainComponent::onClipDraggedToButton(int sourceButtonIndex, int targetButtonIndex) {
  DBG("MainComponent: Dragging clip from button " << sourceButtonIndex << " to button "
                                                  << targetButtonIndex);

  // Swap clips in SessionManager
  m_sessionManager.swapClips(sourceButtonIndex, targetButtonIndex);

  // Swap stop-others mode flags
  std::swap(m_stopOthersOnPlay[sourceButtonIndex], m_stopOthersOnPlay[targetButtonIndex]);

  // Update both buttons visually
  updateButtonFromClip(sourceButtonIndex);
  updateButtonFromClip(targetButtonIndex);
}

void MainComponent::updateButtonFromClip(int buttonIndex) {
  auto button = m_clipGrid->getButton(buttonIndex);
  if (!button)
    return;

  if (m_sessionManager.hasClip(buttonIndex)) {
    // Get real clip metadata from SessionManager
    auto clipData = m_sessionManager.getClip(buttonIndex);

    // Update button with real data
    button->setState(ClipButton::State::Loaded);
    button->setClipName(juce::String(clipData.displayName));
    button->setClipColor(clipData.color);

    // Calculate duration in seconds
    if (clipData.sampleRate > 0) {
      double durationSeconds = static_cast<double>(clipData.durationSamples) / clipData.sampleRate;
      button->setClipDuration(durationSeconds);
    }

    // Set clip group (routing)
    button->setClipGroup(clipData.clipGroup);

    // Derive keyboard shortcut from button index
    juce::String shortcut = getKeyboardShortcutForButton(buttonIndex);
    button->setKeyboardShortcut(shortcut);

    // TODO: Parse beat offset from clip name if it contains "//" notation
    // For now, leave beat offset empty (will be set via edit dialogue later)

    DBG("MainComponent: Updated button " << buttonIndex << " with clip: " << clipData.displayName
                                         << " (" << clipData.sampleRate << " Hz, "
                                         << clipData.numChannels << " ch)");
  } else {
    // Clear button
    button->clearClip();
  }
}

void MainComponent::onStopAll() {
  DBG("MainComponent: Stop All pressed");

  // Call AudioEngine to stop all clips (with fade-out)
  if (m_audioEngine) {
    m_audioEngine->stopAllClips();
  }

  // Update UI state for all playing clips
  for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
    auto button = m_clipGrid->getButton(i);
    if (button && button->getState() == ClipButton::State::Playing) {
      button->setState(ClipButton::State::Loaded);
      DBG("MainComponent: Stopped clip on button " << i);
    }
  }
}

void MainComponent::onPanic() {
  DBG("MainComponent: PANIC pressed - immediate mute!");

  // Call AudioEngine for immediate mute (no fade-out)
  if (m_audioEngine) {
    m_audioEngine->panicStop();
  }

  // Update UI state for all clips
  for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
    auto button = m_clipGrid->getButton(i);
    if (button && (button->getState() == ClipButton::State::Playing ||
                   button->getState() == ClipButton::State::Stopping)) {
      button->setState(ClipButton::State::Loaded);
      DBG("MainComponent: PANIC stopped clip on button " << i);
    }
  }
}

void MainComponent::onTabSelected(int tabIndex) {
  DBG("MainComponent: Tab " << tabIndex << " selected");

  // Stop all playing clips when switching tabs (safety measure)
  for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
    auto button = m_clipGrid->getButton(i);
    if (button && button->getState() == ClipButton::State::Playing) {
      button->setState(ClipButton::State::Loaded);
    }
  }

  // Update SessionManager's active tab
  m_sessionManager.setActiveTab(tabIndex);

  // Refresh all buttons from SessionManager for the new tab
  for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
    updateButtonFromClip(i);
  }

  repaint();
}

//==============================================================================
// Menu Bar Implementation
juce::StringArray MainComponent::getMenuBarNames() {
  return {"File", "Session", "Audio"};
}

juce::PopupMenu MainComponent::getMenuForIndex(int topLevelMenuIndex,
                                               const juce::String& /*menuName*/) {
  juce::PopupMenu menu;

  if (topLevelMenuIndex == 0) // File menu
  {
    menu.addItem(1, "New Session");
    menu.addItem(2, "Open Session...");
    menu.addSeparator();
    menu.addItem(3, "Save Session");
    menu.addItem(4, "Save Session As...");
    menu.addSeparator();
    menu.addItem(5, "Quit");
  } else if (topLevelMenuIndex == 1) // Session menu
  {
    menu.addItem(10, "Clear All Clips");
    menu.addSeparator();
    menu.addItem(11, "Stop All Clips");
    menu.addItem(12, "PANIC");
  } else if (topLevelMenuIndex == 2) // Audio menu
  {
    menu.addItem(20, "Audio I/O Settings...");
    menu.addSeparator();
    menu.addItem(21, "Show Audio Engine Info");
  }

  return menu;
}

void MainComponent::menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/) {
  switch (menuItemID) {
  case 1: // New Session
    // Stop all playing audio first
    if (m_audioEngine) {
      m_audioEngine->stopAllClips();
    }

    m_sessionManager.clearSession();
    // Clear all buttons
    for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
      auto button = m_clipGrid->getButton(i);
      if (button)
        button->clearClip();
    }
    DBG("MainComponent: New session created");
    break;

  case 2: // Open Session
  {
    juce::FileChooser chooser("Open Session",
                              juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                  .getChildFile("Orpheus Clip Composer/Sessions"),
                              "*.json");

    if (chooser.browseForFileToOpen()) {
      auto file = chooser.getResult();
      if (m_sessionManager.loadSession(file)) {
        // Update all buttons from loaded session
        for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
          updateButtonFromClip(i);
        }
        DBG("MainComponent: Successfully loaded session: " + file.getFileName());
      } else {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon, "Load Failed",
            "Could not load session file:\\n" + file.getFullPathName(), "OK");
      }
    }
    break;
  }

  case 3: // Save Session
  {
    // TODO: Save to current file if exists, otherwise show save dialog
    juce::FileChooser chooser("Save Session",
                              juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                  .getChildFile("Orpheus Clip Composer/Sessions"),
                              "*.json");

    if (chooser.browseForFileToSave(true)) {
      auto file = chooser.getResult();
      // Ensure .json extension
      if (!file.hasFileExtension(".json"))
        file = file.withFileExtension(".json");

      if (m_sessionManager.saveSession(file)) {
        DBG("MainComponent: Successfully saved session: " + file.getFileName());
      } else {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon, "Save Failed",
            "Could not save session file:\\n" + file.getFullPathName(), "OK");
      }
    }
    break;
  }

  case 4: // Save Session As
  {
    juce::FileChooser chooser("Save Session As",
                              juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                  .getChildFile("Orpheus Clip Composer/Sessions"),
                              "*.json");

    if (chooser.browseForFileToSave(true)) {
      auto file = chooser.getResult();
      // Ensure .json extension
      if (!file.hasFileExtension(".json"))
        file = file.withFileExtension(".json");

      if (m_sessionManager.saveSession(file)) {
        DBG("MainComponent: Successfully saved session as: " + file.getFileName());
      } else {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon, "Save Failed",
            "Could not save session file:\\n" + file.getFullPathName(), "OK");
      }
    }
    break;
  }

  case 5: // Quit
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
    break;

  case 10: // Clear All Clips
    m_sessionManager.clearSession();
    for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
      auto button = m_clipGrid->getButton(i);
      if (button)
        button->clearClip();
    }
    DBG("MainComponent: All clips cleared");
    break;

  case 11: // Stop All Clips
    onStopAll();
    break;

  case 12: // PANIC
    onPanic();
    break;

  case 20: // Audio I/O Settings
  {
    juce::String info = "Audio Engine Status:\n\n";
    if (m_audioEngine && m_audioEngine->isRunning()) {
      info += "Status: Running\n";
      info += "Sample Rate: 48000 Hz\n";
      info += "Buffer Size: 1024 samples\n";
      info += "Channels: 2 (Stereo)\n";
      info += "Latency: ~21 ms\n\n";
      info += "To change settings, restart the application.";
    } else {
      info += "Status: Not running\n";
    }

    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Audio I/O Settings", info,
                                           "OK");
    break;
  }

  case 21: // Show Audio Engine Info
  {
    juce::String info = "Orpheus Audio Engine\n\n";
    info += "Driver: CoreAudio (macOS)\n";
    info += "Real-time Processing: Active\n";
    info += "SDK Version: M2 Infrastructure\n";
    info += "Transport: Lock-Free\n";
    info += "File Formats: WAV, AIFF, FLAC\n";

    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Audio Engine Info", info,
                                           "OK");
    break;
  }

  default:
    break;
  }
}
