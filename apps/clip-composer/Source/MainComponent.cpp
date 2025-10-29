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

  // Double-click does nothing - right-click is the ONLY way to open edit dialog
  // m_clipGrid->onButtonDoubleClicked = [this](int buttonIndex) { onClipDoubleClicked(buttonIndex);
  // };

  // Wire up drag & drop handler
  m_clipGrid->onFilesDropped = [this](const juce::Array<juce::File>& files, int buttonIndex) {
    loadMultipleFiles(files, buttonIndex);
  };

  // Wire up clip drag-to-reorder handler
  m_clipGrid->onButtonDraggedToButton = [this](int sourceIndex, int targetIndex) {
    onClipDraggedToButton(sourceIndex, targetIndex);
  };

  // Wire up 75fps playback state sync
  m_clipGrid->isClipPlaying = [this](int buttonIndex) -> bool {
    if (m_audioEngine) {
      return m_audioEngine->isClipPlaying(buttonIndex);
    }
    return false;
  };

  // Wire up 75fps clip existence check (prevents orphaned states)
  m_clipGrid->hasClip = [this](int buttonIndex) -> bool {
    return m_sessionManager.hasClip(buttonIndex);
  };

  // Wire up 75fps clip state tracking (ensures fade, loop, stop-others persist)
  m_clipGrid->getClipStates = [this](int buttonIndex, bool& loopEnabled, bool& fadeInEnabled,
                                     bool& fadeOutEnabled, bool& stopOthersEnabled) {
    if (!m_sessionManager.hasClip(buttonIndex))
      return;

    auto clipData = m_sessionManager.getClip(buttonIndex);

    // Query clip metadata (these are CLIP properties, not button properties)
    loopEnabled = m_loopEnabled[buttonIndex];
    fadeInEnabled = (clipData.fadeInSeconds > 0.0);
    fadeOutEnabled = (clipData.fadeOutSeconds > 0.0);
    stopOthersEnabled = m_stopOthersOnPlay[buttonIndex];
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

  // Load saved audio settings (sample rate, buffer size, device)
  juce::PropertiesFile::Options options;
  options.applicationName = "OrpheusClipComposer";
  options.filenameSuffix = ".settings";
  options.osxLibrarySubFolder = "Application Support";
  juce::PropertiesFile settings(options);

  int savedSampleRate = settings.getIntValue("sampleRate", 48000);
  int savedBufferSize = settings.getIntValue("bufferSize", 512);
  juce::String savedDevice = settings.getValue("audioDevice", "Default Device");

  DBG("MainComponent: Restoring saved audio settings - Device: "
      << savedDevice << ", SR: " << savedSampleRate << " Hz, Buffer: " << savedBufferSize);

  // Initialize with saved sample rate
  if (!m_audioEngine->initialize(savedSampleRate)) {
    DBG("MainComponent: Failed to initialize audio engine!");
  } else {
    // Apply saved settings (device and buffer size)
    if (savedBufferSize != 512) {
      // Buffer size differs from default, apply saved settings
      m_audioEngine->setAudioDevice(savedDevice.toStdString(), savedSampleRate, savedBufferSize);
    }

    if (m_audioEngine->start()) {
      DBG("MainComponent: Audio engine started successfully");
    } else {
      DBG("MainComponent: Failed to start audio engine");
    }
  }

  // Start timer for latency display updates (once per second)
  startTimer(1000);

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
}

void MainComponent::resized() {
  grabKeyboardFocus(); // Ensure we get keyboard events
  auto bounds = getLocalBounds();

  // Tab switcher at top (40px)
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

//==============================================================================
void MainComponent::timerCallback() {
  // Update latency display in transport controls
  if (m_transportControls && m_audioEngine) {
    uint32_t latencySamples = m_audioEngine->getLatencySamples();
    uint32_t sampleRate = m_audioEngine->getSampleRate();
    uint32_t bufferSize = m_audioEngine->getBufferSize();

    // Driver reports round-trip latency (input + output), but we want click-to-hear (output only)
    // So divide by 2 to get one-way latency
    double latencyMs = ((latencySamples / 2.0) / static_cast<double>(sampleRate)) * 1000.0;

    m_transportControls->setLatencyInfo(latencyMs, bufferSize, sampleRate);
  }
}

//==============================================================================
bool MainComponent::keyPressed(const juce::KeyPress& key) {
  // Issue #10: Tab switching: Cmd+Shift+1 through Cmd+Shift+8
  // (Edit Dialog overrides Cmd+Shift+[1-9,0] for fade times when it has focus)
  if ((key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()) &&
      key.getModifiers().isShiftDown()) {
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

    // Edit Clip at the top (most important action)
    menu.addItem(5, "Edit Clip...");
    menu.addSeparator();

    menu.addItem(1, "Load New Audio File...");
    menu.addItem(6, "Load Multiple Audio Files...");
    menu.addSeparator();

    // Set Color - will show ColorSwatchPicker popup
    menu.addItem(8, "Set Color...");

    menu.addSeparator();
    menu.addItem(4, "Stop Others On Play", true, m_stopOthersOnPlay[buttonIndex]);
    menu.addItem(7, "Loop", true, m_loopEnabled[buttonIndex]);
    menu.addSeparator();
    menu.addItem(2, "Remove Clip");
    menu.addSeparator();
    menu.addItem(3, "Clip Info: " + juce::String(clipData.displayName), false, false);
  } else {
    // Empty button - only show load option
    menu.addItem(1, "Load Audio File...");
    menu.addItem(6, "Load Multiple Audio Files...");
  }

  // Ensure menu uses Orpheus/Inter aesthetic (inherited from MainComponent's LookAndFeel)
  menu.setLookAndFeel(&m_interLookAndFeel);

  menu.showMenuAsync(juce::PopupMenu::Options(), [this, buttonIndex, hasClip](int result) {
    if (result == 5 && hasClip) {
      // Edit Clip - open edit dialog
      onClipDoubleClicked(buttonIndex);
    } else if (result == 1) {
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

      // CRITICAL: Persist to SessionManager
      if (m_sessionManager.hasClip(buttonIndex)) {
        auto clipData = m_sessionManager.getClip(buttonIndex);
        clipData.stopOthersEnabled = m_stopOthersOnPlay[buttonIndex];
        m_sessionManager.setClip(buttonIndex, clipData);
      }

      // Update button visual state
      auto button = m_clipGrid->getButton(buttonIndex);
      if (button) {
        button->setStopOthersEnabled(m_stopOthersOnPlay[buttonIndex]);
      }

      DBG("Button " << buttonIndex << ": Stop others on play = "
                    << (m_stopOthersOnPlay[buttonIndex] ? "ON" : "OFF"));
    } else if (result == 7 && hasClip) {
      // Toggle loop mode
      m_loopEnabled[buttonIndex] = !m_loopEnabled[buttonIndex];

      // CRITICAL: Persist to SessionManager
      auto clipData = m_sessionManager.getClip(buttonIndex);
      clipData.loopEnabled = m_loopEnabled[buttonIndex];
      m_sessionManager.setClip(buttonIndex, clipData);

      // Sync to AudioEngine (CRITICAL: Must update SDK loop state!)
      if (m_audioEngine) {
        m_audioEngine->setClipLoopMode(buttonIndex, m_loopEnabled[buttonIndex]);
      }

      // Update button visual state
      auto button = m_clipGrid->getButton(buttonIndex);
      if (button) {
        button->setLoopEnabled(m_loopEnabled[buttonIndex]);
      }
      DBG("Button " << buttonIndex << ": Loop = " << (m_loopEnabled[buttonIndex] ? "ON" : "OFF"));
    } else if (result == 6) {
      // Load multiple audio files
      juce::FileChooser chooser("Select Audio Files",
                                juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                                "*.wav;*.aiff;*.aif;*.flac");

      if (chooser.browseForMultipleFilesToOpen()) {
        auto files = chooser.getResults();
        loadMultipleFiles(files, buttonIndex);
      }
    } else if (result == 8 && hasClip) {
      // Set Color - show ColorSwatchPicker popup
      auto* colorGrid = new ColorSwatchGrid();

      // Set current color if clip has one
      if (m_sessionManager.hasClip(buttonIndex)) {
        auto clipData = m_sessionManager.getClip(buttonIndex);
        colorGrid->setSelectedColor(clipData.color);
      }

      // Color selection callback
      colorGrid->onColorSelected = [this, buttonIndex](const juce::Colour& newColor) {
        // Update button color
        auto button = m_clipGrid->getButton(buttonIndex);
        if (button) {
          button->setClipColor(newColor);
        }

        // CRITICAL: Persist color to SessionManager (otherwise Edit Dialog will overwrite it)
        if (m_sessionManager.hasClip(buttonIndex)) {
          auto clipData = m_sessionManager.getClip(buttonIndex);
          clipData.color = newColor;
          m_sessionManager.setClip(buttonIndex, clipData);
        }

        DBG("Button " << buttonIndex << ": Color changed to " << newColor.toString());
      };

      // Get button screen position to position popup hovering over it (centered)
      auto button = m_clipGrid->getButton(buttonIndex);
      juce::Rectangle<int> popupBounds;
      if (button) {
        auto buttonBounds = button->getScreenBounds();
        int popupWidth = 284; // Tight fit for 4×12 grid
        int popupHeight = 80; // 4 rows
        // Center popup OVER the button (not below it)
        int popupX = buttonBounds.getCentreX() - (popupWidth / 2);
        int popupY = buttonBounds.getCentreY() - (popupHeight / 2); // Hover over button
        popupBounds = juce::Rectangle<int>(popupX, popupY, popupWidth, popupHeight);
      } else {
        // Fallback: center on screen
        popupBounds = juce::Rectangle<int>(getScreenX() + getWidth() / 2 - 142,
                                           getScreenY() + getHeight() / 2 - 40, 284, 80);
      }

      // Show popup (CallOutBox takes ownership and deletes grid when closed)
      juce::CallOutBox::launchAsynchronously(std::unique_ptr<juce::Component>(colorGrid),
                                             popupBounds, nullptr);
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

void MainComponent::onClipDoubleClicked(int buttonIndex) {
  DBG("MainComponent: Button " + juce::String(buttonIndex) + " double-clicked (edit dialog)");

  // Check if clip is loaded
  if (!m_sessionManager.hasClip(buttonIndex)) {
    DBG("MainComponent: Button " + juce::String(buttonIndex) + " has no clip loaded");
    return;
  }

  // CRITICAL: Close any existing Edit Dialog before opening a new one
  // This prevents multiple dialogs from stacking and causing state corruption
  if (m_currentEditDialog != nullptr) {
    DBG("MainComponent: Closing existing Edit Dialog before opening new one");
    m_currentEditDialog->setVisible(false);
    delete m_currentEditDialog;
    m_currentEditDialog = nullptr;
  }

  // Get clip metadata from SessionManager
  auto clipData = m_sessionManager.getClip(buttonIndex);

  // Create edit dialog (pass AudioEngine and buttonIndex for main grid clip control)
  auto* dialog = new ClipEditDialog(m_audioEngine.get(), buttonIndex);
  m_currentEditDialog = dialog; // Track current dialog

  // Convert SessionManager::ClipData to ClipEditDialog::ClipMetadata
  ClipEditDialog::ClipMetadata metadata;
  metadata.displayName = juce::String(clipData.displayName);
  metadata.filePath = juce::String(clipData.filePath);
  metadata.color = clipData.color;
  metadata.clipGroup = clipData.clipGroup;
  metadata.sampleRate = clipData.sampleRate;
  metadata.numChannels = clipData.numChannels;
  metadata.durationSamples = clipData.durationSamples;

  // Phase 2: Trim points
  metadata.trimInSamples = clipData.trimInSamples;
  metadata.trimOutSamples = clipData.trimOutSamples;

  // Phase 3: Fade times
  metadata.fadeInSeconds = clipData.fadeInSeconds;
  metadata.fadeOutSeconds = clipData.fadeOutSeconds;
  metadata.fadeInCurve = juce::String(clipData.fadeInCurve);
  metadata.fadeOutCurve = juce::String(clipData.fadeOutCurve);

  // Sprint 2: Loop state (sync from MainComponent's internal state)
  metadata.loopEnabled = m_loopEnabled[buttonIndex];
  metadata.stopOthersEnabled = m_stopOthersOnPlay[buttonIndex];

  dialog->setClipMetadata(metadata);

  // Set up callbacks
  dialog->onOkClicked = [this, buttonIndex, dialog](const ClipEditDialog::ClipMetadata& edited) {
    // Update SessionManager with edited metadata
    auto clipData = m_sessionManager.getClip(buttonIndex);
    clipData.displayName = edited.displayName.toStdString();
    clipData.color = edited.color;
    clipData.clipGroup = edited.clipGroup;

    // Phase 2: Trim points
    clipData.trimInSamples = edited.trimInSamples;
    clipData.trimOutSamples = edited.trimOutSamples;

    // Phase 3: Fade times
    clipData.fadeInSeconds = edited.fadeInSeconds;
    clipData.fadeOutSeconds = edited.fadeOutSeconds;
    clipData.fadeInCurve = edited.fadeInCurve.toStdString();
    clipData.fadeOutCurve = edited.fadeOutCurve.toStdString();

    // CRITICAL: Persist loop and stopOthers state
    clipData.loopEnabled = edited.loopEnabled;
    clipData.stopOthersEnabled = edited.stopOthersEnabled;

    // Persist to SessionManager
    m_sessionManager.setClip(buttonIndex, clipData);

    // Apply trim/fade metadata to AudioEngine
    if (m_audioEngine) {
      bool updated = m_audioEngine->updateClipMetadata(
          buttonIndex, clipData.trimInSamples, clipData.trimOutSamples, clipData.fadeInSeconds,
          clipData.fadeOutSeconds, juce::String(clipData.fadeInCurve),
          juce::String(clipData.fadeOutCurve));

      if (updated) {
        DBG("MainComponent: Applied trim/fade metadata to AudioEngine for button " << buttonIndex);
      } else {
        DBG("MainComponent: Failed to apply trim/fade metadata to AudioEngine for button "
            << buttonIndex);
      }

      // Apply loop mode to AudioEngine (CRITICAL: Must apply loop state!)
      bool loopUpdated = m_audioEngine->setClipLoopMode(buttonIndex, edited.loopEnabled);
      if (loopUpdated) {
        // Sync MainComponent's internal loop state array
        m_loopEnabled[buttonIndex] = edited.loopEnabled;

        DBG("MainComponent: Applied loop mode to AudioEngine for button "
            << buttonIndex << " = " << (edited.loopEnabled ? "enabled" : "disabled"));
      } else {
        DBG("MainComponent: Failed to apply loop mode to AudioEngine for button " << buttonIndex);
      }
    }

    // Update button visual state
    auto button = m_clipGrid->getButton(buttonIndex);
    if (button) {
      button->setClipName(edited.displayName);
      button->setClipColor(edited.color);
      button->setClipGroup(edited.clipGroup);
      button->setLoopEnabled(edited.loopEnabled); // CRITICAL: Sync loop visual state

      // Update duration with trimmed values
      if (edited.sampleRate > 0) {
        int64_t trimmedSamples = edited.trimOutSamples - edited.trimInSamples;
        double durationSeconds = static_cast<double>(trimmedSamples) / edited.sampleRate;
        button->setClipDuration(durationSeconds);
      }
    }

    DBG("MainComponent: Updated clip metadata for button "
        << buttonIndex << " - Trim: [" << clipData.trimInSamples << ", " << clipData.trimOutSamples
        << "]"
        << " Fade: [" << clipData.fadeInSeconds << "s " << clipData.fadeInCurve << ", "
        << clipData.fadeOutSeconds << "s " << clipData.fadeOutCurve << "]");

    // Close dialog and clear reference
    dialog->setVisible(false);
    delete dialog;
    m_currentEditDialog = nullptr; // Clear reference to allow new dialog
  };

  dialog->onCancelClicked = [this, dialog]() {
    // Close dialog without saving
    dialog->setVisible(false);
    delete dialog;
    m_currentEditDialog = nullptr; // Clear reference to allow new dialog
  };

  // Show dialog as modal
  dialog->setSize(700, 800); // Expanded for Phases 2 & 3
  dialog->setCentrePosition(getWidth() / 2, getHeight() / 2);
  addAndMakeVisible(dialog);
  dialog->toFront(true);
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

  // CRITICAL: Check if either clip is currently playing BEFORE swapping
  // Playback state is tied to button index in AudioEngine, so we must stop playback first
  auto sourceButton = m_clipGrid->getButton(sourceButtonIndex);
  auto targetButton = m_clipGrid->getButton(targetButtonIndex);

  bool sourceWasPlaying = (sourceButton && sourceButton->getState() == ClipButton::State::Playing);
  bool targetWasPlaying = (targetButton && targetButton->getState() == ClipButton::State::Playing);

  // Stop both clips if playing (prevents orphaned playback state)
  if (sourceWasPlaying && m_audioEngine) {
    m_audioEngine->stopClip(sourceButtonIndex);
    DBG("MainComponent: Stopped source clip (button " << sourceButtonIndex << ") before swap");
  }
  if (targetWasPlaying && m_audioEngine) {
    m_audioEngine->stopClip(targetButtonIndex);
    DBG("MainComponent: Stopped target clip (button " << targetButtonIndex << ") before swap");
  }

  // Swap clips in SessionManager
  m_sessionManager.swapClips(sourceButtonIndex, targetButtonIndex);

  // Swap stop-others mode flags
  std::swap(m_stopOthersOnPlay[sourceButtonIndex], m_stopOthersOnPlay[targetButtonIndex]);

  // Swap loop mode flags
  std::swap(m_loopEnabled[sourceButtonIndex], m_loopEnabled[targetButtonIndex]);

  // Update both buttons visually (this reloads clips into AudioEngine at new positions)
  updateButtonFromClip(sourceButtonIndex);
  updateButtonFromClip(targetButtonIndex);

  // Restart clips at their NEW positions if they were playing
  if (sourceWasPlaying && m_audioEngine && m_sessionManager.hasClip(targetButtonIndex)) {
    m_audioEngine->startClip(targetButtonIndex);
    if (targetButton) {
      targetButton->setState(ClipButton::State::Playing);
    }
    DBG("MainComponent: Restarted source clip at new position (button " << targetButtonIndex
                                                                        << ")");
  }
  if (targetWasPlaying && m_audioEngine && m_sessionManager.hasClip(sourceButtonIndex)) {
    m_audioEngine->startClip(sourceButtonIndex);
    if (sourceButton) {
      sourceButton->setState(ClipButton::State::Playing);
    }
    DBG("MainComponent: Restarted target clip at new position (button " << sourceButtonIndex
                                                                        << ")");
  }
}

void MainComponent::updateButtonFromClip(int buttonIndex) {
  auto button = m_clipGrid->getButton(buttonIndex);
  if (!button)
    return;

  if (m_sessionManager.hasClip(buttonIndex)) {
    // Get real clip metadata from SessionManager
    auto clipData = m_sessionManager.getClip(buttonIndex);

    // Load clip into audio engine first
    if (m_audioEngine) {
      m_audioEngine->loadClip(buttonIndex, juce::String(clipData.filePath));

      // CRITICAL: Apply trim/fade metadata to AudioEngine
      bool metadataApplied = m_audioEngine->updateClipMetadata(
          buttonIndex, clipData.trimInSamples, clipData.trimOutSamples, clipData.fadeInSeconds,
          clipData.fadeOutSeconds, juce::String(clipData.fadeInCurve),
          juce::String(clipData.fadeOutCurve));

      if (metadataApplied) {
        DBG("MainComponent: Applied trim/fade metadata to AudioEngine for button " << buttonIndex);
      } else {
        DBG("MainComponent: Failed to apply trim/fade metadata to AudioEngine for button "
            << buttonIndex);
      }

      // CRITICAL: Apply loop mode to AudioEngine
      bool loopApplied = m_audioEngine->setClipLoopMode(buttonIndex, clipData.loopEnabled);
      if (!loopApplied) {
        DBG("MainComponent: Failed to apply loop mode to AudioEngine for button " << buttonIndex);
      }
    }

    // CRITICAL: Sync MainComponent's internal state arrays from SessionManager clipData
    m_loopEnabled[buttonIndex] = clipData.loopEnabled;
    m_stopOthersOnPlay[buttonIndex] = clipData.stopOthersEnabled;

    // Check if clip is currently playing and restore correct button state
    if (m_audioEngine && m_audioEngine->isClipPlaying(buttonIndex)) {
      button->setState(ClipButton::State::Playing);
    } else {
      button->setState(ClipButton::State::Loaded);
    }

    button->setClipName(juce::String(clipData.displayName));
    button->setClipColor(clipData.color);

    // Calculate TRIMMED duration in seconds (playable time)
    if (clipData.sampleRate > 0) {
      int64_t trimmedSamples = clipData.trimOutSamples - clipData.trimInSamples;
      double durationSeconds = static_cast<double>(trimmedSamples) / clipData.sampleRate;
      button->setClipDuration(durationSeconds);
    }

    // Set clip group (routing)
    button->setClipGroup(clipData.clipGroup);

    // Derive keyboard shortcut from button index
    juce::String shortcut = getKeyboardShortcutForButton(buttonIndex);
    button->setKeyboardShortcut(shortcut);

    // Restore loop state (from synced array)
    button->setLoopEnabled(m_loopEnabled[buttonIndex]);

    // Set stop others indicator (from synced array)
    button->setStopOthersEnabled(m_stopOthersOnPlay[buttonIndex]);

    // Set fade indicators (show if fade duration > 0)
    button->setFadeInEnabled(clipData.fadeInSeconds > 0.0);
    button->setFadeOutEnabled(clipData.fadeOutSeconds > 0.0);

    DBG("MainComponent: Updated button "
        << buttonIndex << " with clip: " << clipData.displayName << " (" << clipData.sampleRate
        << " Hz, " << clipData.numChannels
        << " ch) - Loop: " << (clipData.loopEnabled ? "ON" : "OFF")
        << ", StopOthers: " << (clipData.stopOthersEnabled ? "ON" : "OFF")
        << ", Fades: IN=" << clipData.fadeInSeconds << "s, OUT=" << clipData.fadeOutSeconds << "s");
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

  // Update SessionManager's active tab
  m_sessionManager.setActiveTab(tabIndex);

  // Refresh all buttons from SessionManager for the new tab
  // This will restore colors, names, and playing states from AudioEngine
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
    menu.addSeparator();
    menu.addItem(13, "Keyboard Shortcuts...");
  } else if (topLevelMenuIndex == 2) // Audio menu
  {
    menu.addItem(20, "Audio I/O Settings...");
    menu.addSeparator();
    menu.addItem(21, "Show Audio Engine Info");
    menu.addSeparator();
    menu.addItem(22, "Load Multiple Audio Files...");
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
  {
    // Warn user before clearing all clips
    bool confirmed =
        juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon, "Clear All Clips?",
                                           "This will remove all clips from all tabs.\n\n"
                                           "This action cannot be undone.\n\n"
                                           "Are you sure?",
                                           "Clear All", "Cancel");

    if (confirmed) {
      // Stop all playing audio first
      if (m_audioEngine) {
        m_audioEngine->stopAllClips();
      }

      m_sessionManager.clearSession();
      for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
        auto button = m_clipGrid->getButton(i);
        if (button)
          button->clearClip();
      }

      // Clear internal state arrays
      m_loopEnabled.fill(false);
      m_stopOthersOnPlay.fill(false);

      DBG("MainComponent: All clips cleared");
    }
    break;
  }

  case 11: // Stop All Clips
    onStopAll();
    break;

  case 12: // PANIC
    onPanic();
    break;

  case 13: // Keyboard Shortcuts
  {
    juce::String shortcuts = "=== ORPHEUS CLIP COMPOSER - KEYBOARD SHORTCUTS ===\n\n";
    shortcuts += "GLOBAL SHORTCUTS:\n";
    shortcuts += "  Space ........... Stop All Clips (with fade)\n";
    shortcuts += "  Esc ............. PANIC (immediate mute)\n";
    shortcuts += "  Cmd/Ctrl+Shift+[1-8] ... Switch to Tab 1-8\n";
    shortcuts += "  Q W E R T Y ..... Trigger clips (Row 0)\n";
    shortcuts += "  A S D F G H ..... Trigger clips (Row 1)\n";
    shortcuts += "  Z X C V B N ..... Trigger clips (Row 2)\n";
    shortcuts += "  1-6, 7-0, -=, [];\\',.  Trigger clips (Rows 3-5)\n";
    shortcuts += "  F1-F12 .......... Trigger clips (Rows 6-7)\n\n";
    shortcuts += "EDIT DIALOG SHORTCUTS:\n";
    shortcuts += "  Space ........... Toggle Play/Pause\n";
    shortcuts += "  Enter ........... Save & Close (OK)\n";
    shortcuts += "  Esc ............. Cancel & Close\n";
    shortcuts += "  ? ............... Toggle Loop\n\n";
    shortcuts += "TRIM POINTS:\n";
    shortcuts += "  I ............... Set IN point (at playhead)\n";
    shortcuts += "  O ............... Set OUT point (at playhead)\n";
    shortcuts += "  [ ............... Nudge IN point left (-1 tick)\n";
    shortcuts += "  ] ............... Nudge IN point right (+1 tick)\n";
    shortcuts += "  Shift+[ ......... Nudge IN point left (-15 ticks)\n";
    shortcuts += "  Shift+] ......... Nudge IN point right (+15 ticks)\n";
    shortcuts += "  ; ............... Nudge OUT point left (-1 tick)\n";
    shortcuts += "  ' ............... Nudge OUT point right (+1 tick)\n";
    shortcuts += "  Shift+; ......... Nudge OUT point left (-15 ticks)\n";
    shortcuts += "  Shift+' ......... Nudge OUT point right (+15 ticks)\n\n";
    shortcuts += "WAVEFORM ZOOM:\n";
    shortcuts += "  Cmd/Ctrl + Plus .. Zoom in (1x → 16x)\n";
    shortcuts += "  Cmd/Ctrl + Minus . Zoom out (16x → 1x)\n\n";
    shortcuts += "FADE TIMES (Edit Dialog only):\n";
    shortcuts += "  Cmd/Ctrl+Shift+[1-9] ... Set OUT fade (0.1s-0.9s)\n";
    shortcuts += "  Cmd/Ctrl+Shift+0 ....... Set OUT fade (1.0s)\n";
    shortcuts += "  Cmd/Ctrl+Opt+Shift+[1-9]  Set IN fade (0.1s-0.9s)\n";
    shortcuts += "  Cmd/Ctrl+Opt+Shift+0 .... Set IN fade (1.0s)\n\n";
    shortcuts += "NOTE: Hold < > buttons in Edit Dialog for auto-repeat";

    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Keyboard Shortcuts",
                                           shortcuts, "OK");
    break;
  }

  case 20: // Audio I/O Settings
  {
    // Create and show Audio I/O Settings Dialog
    auto* dialog = new AudioSettingsDialog(m_audioEngine.get());
    dialog->onCloseClicked = [this, dialog]() {
      dialog->setVisible(false);
      delete dialog;
    };
    dialog->setSize(500, 300); // Match AudioSettingsDialog's preferred size
    dialog->setCentrePosition(getWidth() / 2, getHeight() / 2);
    addAndMakeVisible(dialog);
    dialog->toFront(true);

    DBG("MainComponent: Audio I/O Settings dialog opened");
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

  case 22: // Load Multiple Audio Files
  {
    juce::FileChooser chooser("Select Audio Files",
                              juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                              "*.wav;*.aiff;*.aif;*.flac");

    if (chooser.browseForMultipleFilesToOpen()) {
      auto files = chooser.getResults();
      // Start loading from first button in current tab
      loadMultipleFiles(files, 0);
    }
    break;
  }

  default:
    break;
  }
}
