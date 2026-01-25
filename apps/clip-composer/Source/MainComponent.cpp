// SPDX-License-Identifier: MIT

#include "MainComponent.h"
#include "Core/ApplicationPaths.h"
#include "UI/DesignTokens.h"

#if JUCE_MAC
#include <mach/mach.h>
#endif

//==============================================================================
MainComponent::MainComponent() {
  // Sprint 0: Ensure application directories exist
  orpheus::ApplicationPaths::ensureDirectoriesExist();

  // Initialize Core Services
  m_audioEngine = std::make_unique<AudioEngine>();
  m_undoManager = std::make_unique<orpheus::UndoManager>();

  // Initialize Managers (OCC116/OCC117)
  m_displayPreferences = std::make_unique<orpheus::DisplayPreferences>();
  m_externalToolManager = std::make_unique<orpheus::ExternalToolManager>();
  m_hotKeyManager = std::make_unique<orpheus::HotKeyManager>();
  m_midiDeviceManager = std::make_unique<orpheus::MIDIDeviceManager>();

  // OCC144: Wire up DisplayPreferences callback to update UI when settings change
  m_displayPreferences->onPreferencesChanged = [this]() {
    applyDisplayPreferences();
    DBG("DisplayPreferences changed - UI updated");
  };

  // Initialize Database & Logging (Sprint 2)
  m_database = std::make_unique<orpheus::Database>();
  auto dbFile = orpheus::ApplicationPaths::getLogsDir().getChildFile("app.db");
  auto result = m_database->open(dbFile);

  if (result.failed()) {
    DBG("Failed to open database: " << result.getErrorMessage());
    // Fallback or fatal error handling? For now, we proceed without logging.
  } else {
    m_eventLogger = std::make_unique<orpheus::EventLogger>(*m_database);
    m_playoutLogger = std::make_unique<orpheus::PlayoutLogger>(*m_database);

    // Log startup
    m_eventLogger->log(orpheus::EventType::Startup, "MainComponent", "Application started");
  }

  // Note: Additional services (SettingsService, etc.) will be added in future sprints

  // Set HK Grotesk font as default for all components
  setLookAndFeel(&m_hkGroteskLookAndFeel);

  // Create tab switcher (8 tabs for MAX_CLIP_BUTTONS total clips)
  m_tabSwitcher = std::make_unique<TabSwitcher>();
  addAndMakeVisible(m_tabSwitcher.get());

  // Wire up tab selection callback
  m_tabSwitcher->onTabSelected = [this](int tabIndex) { onTabSelected(tabIndex); };

  // Create clip grid (6×8 = 48 buttons per tab)
  m_clipGrid = std::make_unique<ClipGrid>();
  addAndMakeVisible(m_clipGrid.get());

  // Create BarVisualizer (shmui VU meter - 4 bars for master level)
  m_barVisualizer = std::make_unique<shmui::BarVisualizer>();
  m_barVisualizer->setBarCount(4); // 4 bars showing master level (group routing TBD)
  m_barVisualizer->setBackgroundColour(juce::Colour(OCC::Design::kBgPrimary));
  m_barVisualizer->setHeightRange(5.0f, 100.0f); // Lower minimum for better dynamic range
  m_barVisualizer->setGradientMode(true);        // Enable VU meter gradient (green-yellow-red)
  // Note: Do NOT connect to AudioAnalyzer - we feed levels manually via setVolumeBands()
  addAndMakeVisible(m_barVisualizer.get());

  // Session History Window (initially hidden)
  m_sessionHistoryWindow = std::make_unique<SessionHistoryWindow>();
  m_sessionHistoryWindow->setVisible(false);

  // Wire up ClipGrid callbacks (moved to helper method for readability)
  wireUpClipGridCallbacks();

  // Make this component capture keyboard focus
  setWantsKeyboardFocus(true);

  // Wire up transport control callbacks (moved to helper method for readability)
  wireUpTransportCallbacks();

  // Set window size (1400×900 for better screen fit)
  setSize(1400, 900);

// Set up menu bar (macOS native)
#if JUCE_MAC
  juce::MenuBarModel::setMacMainMenu(this);
#endif

  // AudioEngine is already initialized at top of constructor
  // m_audioEngine = std::make_unique<AudioEngine>();

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

      // BarVisualizer will receive levels via timer callback, not AudioAnalyzer
      // This provides VU meter display of 4 group levels instead of FFT frequency bands
      DBG("MainComponent: BarVisualizer will receive group levels via timer");

      // OCC144: Wire up clip state callback for Session History logging
      // (implementation moved to handleClipStateChanged() for readability)
      m_audioEngine->onClipStateChanged = [this](int buttonIndex, orpheus::PlaybackState state) {
        handleClipStateChanged(buttonIndex, state);
      };
    } else {
      DBG("MainComponent: Failed to start audio engine");
    }
  }

  // Start timer for VU meter updates (30Hz) and performance display (1Hz subset)
  startTimerHz(30);

  // OCC144: Apply initial display preferences to UI components
  applyDisplayPreferences();

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
  g.fillAll(juce::Colour(OCC::Design::kBgSecondary));
}

void MainComponent::resized() {
  grabKeyboardFocus(); // Ensure we get keyboard events
  auto bounds = getLocalBounds();

  // OCC130 Sprint B: Merged tab switcher + transport controls at top (40px)
  auto tabArea = bounds.removeFromTop(40);
  if (m_tabSwitcher) {
    m_tabSwitcher->setBounds(tabArea); // Full width (no horizontal margin)
  }

  // Main content area
  auto contentArea = bounds.reduced(10); // 10px margin

  // BarVisualizer on the right (60px wide with 10px left margin)
  if (m_barVisualizer) {
    auto visualizerArea = contentArea.removeFromRight(60); // 60px wide for frequency bars
    contentArea.removeFromRight(10);                       // 10px left margin
    m_barVisualizer->setBounds(visualizerArea);
  }

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
  // OCC144: Update VU meter at 30Hz
  // Note: Until clip group routing is implemented, all bars show master RMS level
  if (m_barVisualizer && m_audioEngine) {
    float masterLevel = m_audioEngine->getMasterRmsLevel();
    // Show master level on all 4 bars (no group routing yet)
    std::vector<float> levels = {masterLevel, masterLevel, masterLevel, masterLevel};
    m_barVisualizer->setVolumeBands(levels);
  }

  // Static counter for 1Hz performance updates (every 30 timer ticks at 30Hz)
  static int performanceUpdateCounter = 0;
  performanceUpdateCounter++;

  // Only update performance display once per second (30 ticks at 30Hz)
  if (performanceUpdateCounter >= 30) {
    performanceUpdateCounter = 0;

    // OCC130 Sprint B: Update latency/performance info in merged TabSwitcher
    if (m_tabSwitcher && m_audioEngine) {
      uint32_t latencySamples = m_audioEngine->getLatencySamples();
      uint32_t sampleRate = m_audioEngine->getSampleRate();
      uint32_t bufferSize = m_audioEngine->getBufferSize();

      // Driver reports round-trip latency (input + output), but we want click-to-hear (output only)
      // So divide by 2 to get one-way latency
      double latencyMs = ((latencySamples / 2.0) / static_cast<double>(sampleRate)) * 1000.0;

      m_tabSwitcher->setLatencyInfo(latencyMs, bufferSize, sampleRate);

      // OCC109 v0.2.2: Update CPU and memory display (1Hz refresh rate)
      // CRITICAL: JUCE doesn't provide getCpuUsage() or getMemoryUsageInMegabytes()
      // Use platform-specific APIs instead
#if JUCE_MAC
      // Get process memory usage via macOS mach API
      struct mach_task_basic_info info;
      mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
      if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &infoCount) ==
          KERN_SUCCESS) {
        int memoryMB = static_cast<int>(info.resident_size / (1024 * 1024));

        // CPU: Placeholder until SDK IPerformanceMonitor integration in v0.3.0
        // SDK will provide per-thread CPU metrics for audio vs UI threads
        float cpuPercent = 0.0f; // TODO: Integrate SDK PerformanceMonitor (ORP110 Feature 3)

        m_tabSwitcher->setPerformanceInfo(cpuPercent, memoryMB);
      } else {
        // Fallback if mach API fails
        m_tabSwitcher->setPerformanceInfo(0.0f, 0);
      }
#else
      // Non-macOS platforms: Placeholder
      m_tabSwitcher->setPerformanceInfo(0.0f, 0);
#endif
    }
  }
}

//==============================================================================
// OCC144: Apply display preferences to UI components
void MainComponent::applyDisplayPreferences() {
  if (!m_displayPreferences)
    return;

  // Apply Page Tab Height to TabSwitcher
  if (m_tabSwitcher) {
    int tabHeight = orpheus::DisplayPreferences::getPageTabHeightPixels(
        m_displayPreferences->getPageTabHeight());
    m_tabSwitcher->setTabHeight(tabHeight);
  }

  // TODO: Apply additional preferences to ClipButton/ClipGrid:
  // - buttonTriggerSize
  // - showButtonTriggers
  // - buttonTextMode
  // - bevelWidth
  // - edgedText
  // - elapsedTimeMode

  // Force layout update
  resized();
  repaint();

  // Repaint child components
  if (m_clipGrid) {
    m_clipGrid->repaint();
  }
  if (m_tabSwitcher) {
    m_tabSwitcher->repaint();
  }
}

//==============================================================================
// Initialization helpers: Extract callback wiring from constructor for clarity

void MainComponent::wireUpClipGridCallbacks() {
  // Right-click handler for loading clips
  m_clipGrid->onButtonRightClicked = [this](int buttonIndex) { onClipRightClicked(buttonIndex); };

  // Left-click handler for triggering clips
  m_clipGrid->onButtonClicked = [this](int buttonIndex) { onClipTriggered(buttonIndex); };

  // Ctrl+Opt+Cmd+Click handler for edit dialog
  m_clipGrid->onButtonEditDialogRequested = [this](int buttonIndex) {
    onClipDoubleClicked(buttonIndex);
  };

  // Drag & drop handler
  m_clipGrid->onFilesDropped = [this](const juce::Array<juce::File>& files, int buttonIndex) {
    loadMultipleFiles(files, buttonIndex);
  };

  // Clip drag-to-reorder handler
  m_clipGrid->onButtonDraggedToButton = [this](int sourceIndex, int targetIndex) {
    onClipDraggedToButton(sourceIndex, targetIndex);
  };

  // 75fps playback state sync (uses global clip index for multi-tab isolation)
  m_clipGrid->getClipState = [this](int buttonIndex) -> orpheus::PlaybackState {
    if (m_audioEngine) {
      int globalClipIndex = getGlobalClipIndex(buttonIndex);
      return m_audioEngine->getClipState(globalClipIndex);
    }
    return orpheus::PlaybackState::Stopped;
  };

  // 75fps clip existence check (prevents orphaned states)
  m_clipGrid->hasClip = [this](int buttonIndex) -> bool {
    return m_sessionManager.hasClip(buttonIndex);
  };

  // 75fps clip state tracking (ensures fade, loop, stop-others persist)
  m_clipGrid->getClipStates = [this](int buttonIndex, bool& loopEnabled, bool& fadeInEnabled,
                                     bool& fadeOutEnabled, bool& stopOthersEnabled) {
    if (!m_sessionManager.hasClip(buttonIndex))
      return;

    auto clipData = m_sessionManager.getClip(buttonIndex);
    int globalClipIndex = getGlobalClipIndex(buttonIndex);

    // Query clip metadata (these are CLIP properties, not button properties)
    loopEnabled = m_loopEnabled[globalClipIndex];
    fadeInEnabled = (clipData.fadeInSeconds > 0.0);
    fadeOutEnabled = (clipData.fadeOutSeconds > 0.0);
    stopOthersEnabled = m_stopOthersOnPlay[globalClipIndex];
  };

  // 75fps playback position tracking (for elapsed time display)
  m_clipGrid->getClipPosition = [this](int buttonIndex) -> float {
    if (!m_audioEngine || !m_sessionManager.hasClip(buttonIndex))
      return 0.0f;

    int globalClipIndex = getGlobalClipIndex(buttonIndex);
    auto clipData = m_sessionManager.getClip(buttonIndex);

    // Get current sample position (absolute)
    int64_t currentSample = m_audioEngine->getClipPosition(globalClipIndex);

    // Calculate trimmed duration in samples
    int64_t trimmedSamples = clipData.trimOutSamples - clipData.trimInSamples;

    // Normalize to 0.0-1.0 progress within trimmed region
    if (trimmedSamples > 0) {
      float progress = static_cast<float>(currentSample - clipData.trimInSamples) /
                       static_cast<float>(trimmedSamples);
      return juce::jlimit(0.0f, 1.0f, progress);
    }

    return 0.0f;
  };
}

void MainComponent::wireUpTransportCallbacks() {
  m_tabSwitcher->onStopAll = [this]() { onStopAll(); };
  m_tabSwitcher->onPanic = [this]() { onPanic(); };
}

void MainComponent::handleClipStateChanged(int buttonIndex, orpheus::PlaybackState state) {
  // Get clip info for logging
  int tabIndex = buttonIndex / 48;
  int localIndex = buttonIndex % 48;

  // Build history entry with timestamp
  auto now = juce::Time::getCurrentTime();
  juce::String timestamp = now.formatted("%H:%M:%S");

  juce::String clipName;
  int groupIndex = 0;
  int sampleRate = 0;
  int numChannels = 0;
  int64_t trimInSamples = 0;
  int64_t trimOutSamples = 0;
  double fadeInSeconds = 0.0;
  double fadeOutSeconds = 0.0;

  // Get clip metadata from session manager (need to temporarily switch tabs)
  int currentTab = m_sessionManager.getActiveTab();
  m_sessionManager.setActiveTab(tabIndex);
  if (m_sessionManager.hasClip(localIndex)) {
    auto clipData = m_sessionManager.getClip(localIndex);
    if (clipData.displayName.empty()) {
      // Extract filename from filePath
      juce::File file(clipData.filePath);
      clipName = file.getFileNameWithoutExtension();
    } else {
      clipName = clipData.displayName;
    }
    groupIndex = clipData.clipGroup;
    sampleRate = clipData.sampleRate;
    numChannels = clipData.numChannels;
    trimInSamples = clipData.trimInSamples;
    trimOutSamples = clipData.trimOutSamples;
    fadeInSeconds = clipData.fadeInSeconds;
    fadeOutSeconds = clipData.fadeOutSeconds;
  }
  m_sessionManager.setActiveTab(currentTab);

  // Helper lambda: format samples as MM:SS.mmm
  auto formatTime = [](int64_t samples, int sr) -> juce::String {
    if (sr <= 0)
      return "--:--";
    double seconds = static_cast<double>(samples) / sr;
    int mins = static_cast<int>(seconds) / 60;
    double secs = std::fmod(seconds, 60.0);
    return juce::String::formatted("%02d:%05.2f", mins, secs);
  };

  // Calculate clip duration (trimmed region)
  int64_t trimmedDuration = trimOutSamples - trimInSamples;
  juce::String durationStr = formatTime(trimmedDuration, sampleRate);

  // Build state string and additional info
  juce::String stateStr;
  juce::String elapsedStr;
  juce::String metadataStr;

  switch (state) {
  case orpheus::PlaybackState::Playing:
    stateStr = "PLAY";
    // Record start time for elapsed calculation
    m_clipStartTimes[buttonIndex] = now;
    // Build metadata: duration, sample rate, channels, group, fades
    metadataStr = " | Dur: " + durationStr;
    if (sampleRate > 0) {
      metadataStr += " | " + juce::String(sampleRate / 1000) + "kHz";
    }
    if (numChannels > 0) {
      metadataStr += " " + juce::String(numChannels) + "ch";
    }
    metadataStr += " | G" + juce::String(groupIndex + 1);
    if (fadeInSeconds > 0.0) {
      metadataStr += " | FI:" + juce::String(fadeInSeconds, 1) + "s";
    }
    if (fadeOutSeconds > 0.0) {
      metadataStr += " FO:" + juce::String(fadeOutSeconds, 1) + "s";
    }
    break;

  case orpheus::PlaybackState::Stopped:
    stateStr = "STOP";
    // Calculate elapsed time since play started
    if (m_clipStartTimes.count(buttonIndex) > 0) {
      auto startTime = m_clipStartTimes[buttonIndex];
      auto elapsedMs = now.toMilliseconds() - startTime.toMilliseconds();
      double elapsedSecs = elapsedMs / 1000.0;
      int elapsedMins = static_cast<int>(elapsedSecs) / 60;
      double elapsedSecsPart = std::fmod(elapsedSecs, 60.0);
      elapsedStr =
          " | Played: " + juce::String::formatted("%02d:%05.2f", elapsedMins, elapsedSecsPart);
      m_clipStartTimes.erase(buttonIndex);
    }
    metadataStr = " | G" + juce::String(groupIndex + 1);
    break;

  case orpheus::PlaybackState::Stopping:
    stateStr = "FADE";
    metadataStr = " | G" + juce::String(groupIndex + 1);
    if (fadeOutSeconds > 0.0) {
      metadataStr += " | FO:" + juce::String(fadeOutSeconds, 1) + "s";
    }
    break;

  default:
    stateStr = "----";
    break;
  }

  // Log to SessionHistoryWindow with enhanced info
  if (m_sessionHistoryWindow && m_sessionHistoryWindow->isVisible()) {
    juce::String entry = timestamp + " | " + stateStr + " | Tab " + juce::String(tabIndex + 1) +
                         " | " + clipName + elapsedStr + metadataStr;
    m_sessionHistoryWindow->addHistoryEntry(entry);
  }

  // Log to LevelMetersWindow play history (only for PLAY events)
  if (state == orpheus::PlaybackState::Playing && m_levelMetersWindow) {
    m_levelMetersWindow->addPlayHistoryEntry(buttonIndex, clipName, groupIndex);
  }

  // Also log to database if available
  if (m_playoutLogger && state == orpheus::PlaybackState::Playing) {
    orpheus::PlayoutEntry entry;
    entry.startTime = now;
    entry.trackName = clipName;
    entry.outputName = "Group " + juce::String(groupIndex + 1);
    entry.triggerSource = "User";
    m_playoutLogger->logPlaybackStart(entry);
  }
}

//==============================================================================
bool MainComponent::keyPressed(const juce::KeyPress& key) {
  // CRITICAL: Suspend main grid hotkeys when Clip Edit dialog is open
  // Edit Dialog key commands must take priority to prevent misfires during editing
  if (m_currentEditDialog != nullptr && m_currentEditDialog->isVisible()) {
    // Edit Dialog is open - do NOT process main grid hotkeys
    // Exception: Allow Cmd+Shift+Tab switching even when dialog is open (power user feature)
    if ((key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()) &&
        key.getModifiers().isShiftDown()) {
      int keyCode = key.getKeyCode();
      if (keyCode >= '1' && keyCode <= '8') {
        int tabIndex = keyCode - '1'; // Convert '1'-'8' to 0-7
        m_tabSwitcher->setActiveTab(tabIndex);
        return true;
      }
    }
    // All other keys are handled by Edit Dialog - do NOT process here
    return false;
  }

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

  // Item 54: Standard macOS key commands
  if (key.getModifiers().isCommandDown() && !key.getModifiers().isShiftDown()) {
    // Cmd+Z = Undo
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)) {
      menuItemSelected(100, 1); // Trigger Undo menu item
      return true;
    }

    // Cmd+S = Save Session
    if (key == juce::KeyPress('s', juce::ModifierKeys::commandModifier, 0)) {
      menuItemSelected(3, 0); // Trigger "Save Session" menu item
      return true;
    }
    // Cmd+, = Preferences (OCC144: Opens Audio I/O Settings as primary settings dialog)
    if (key == juce::KeyPress(',', juce::ModifierKeys::commandModifier, 0)) {
      DBG("MainComponent: Cmd+, pressed - Opening Audio I/O Settings");
      menuItemSelected(20, 5); // Trigger Audio I/O Settings dialog
      return true;
    }

    // Item 24: Clip Copy/Paste functionality
    // Cmd+C = Copy clip at playbox position
    if (key == juce::KeyPress('c', juce::ModifierKeys::commandModifier, 0)) {
      int playboxIndex = m_clipGrid->getPlayboxIndex();
      if (m_sessionManager.hasClip(playboxIndex)) {
        m_clipboardData = m_sessionManager.getClip(playboxIndex);
        m_hasClipInClipboard = true;
        DBG("MainComponent: Copied clip from button " << playboxIndex << " - "
                                                      << m_clipboardData.displayName);

        // Visual feedback - could add a status message later
      } else {
        DBG("MainComponent: No clip at playbox position " << playboxIndex << " to copy");
      }
      return true;
    }

    // Cmd+V = Paste clip at playbox position
    if (key == juce::KeyPress('v', juce::ModifierKeys::commandModifier, 0)) {
      if (m_hasClipInClipboard) {
        int playboxIndex = m_clipGrid->getPlayboxIndex();

        // Check if target has a clip and warn about overwrite
        if (m_sessionManager.hasClip(playboxIndex)) {
          bool confirmed = juce::AlertWindow::showOkCancelBox(
              juce::AlertWindow::WarningIcon, "Replace Clip?",
              "Button " + juce::String(playboxIndex + 1) + " already has a clip.\n\n" +
                  "Replace it with \"" + juce::String(m_clipboardData.displayName) + "\"?",
              "Replace", "Cancel");

          if (!confirmed) {
            return true;
          }
        }

        // Load the clip from clipboard
        loadClipToButton(playboxIndex, juce::String(m_clipboardData.filePath));

        // Only proceed if clip was successfully loaded (user didn't cancel copy/link dialog)
        if (m_sessionManager.hasClip(playboxIndex)) {
          // Copy all metadata
          auto clipData = m_sessionManager.getClip(playboxIndex);
          clipData.displayName = m_clipboardData.displayName;
          clipData.color = m_clipboardData.color;
          clipData.clipGroup = m_clipboardData.clipGroup;
          clipData.trimInSamples = m_clipboardData.trimInSamples;
          clipData.trimOutSamples = m_clipboardData.trimOutSamples;
          clipData.fadeInSeconds = m_clipboardData.fadeInSeconds;
          clipData.fadeOutSeconds = m_clipboardData.fadeOutSeconds;
          clipData.fadeInCurve = m_clipboardData.fadeInCurve;
          clipData.fadeOutCurve = m_clipboardData.fadeOutCurve;
          clipData.gainDb = m_clipboardData.gainDb;
          clipData.loopEnabled = m_clipboardData.loopEnabled;
          clipData.stopOthersEnabled = m_clipboardData.stopOthersEnabled;
          m_sessionManager.setClip(playboxIndex, clipData);

          // Update button visually
          updateButtonFromClip(playboxIndex);

          DBG("MainComponent: Pasted clip \"" << m_clipboardData.displayName << "\" to button "
                                              << playboxIndex);
        } else {
          DBG("MainComponent: Clip paste cancelled (user declined copy/link dialog)");
        }
      } else {
        DBG("MainComponent: No clip in clipboard to paste");
      }
      return true;
    }
  }

  // Cmd+Shift+S = Save Session As, Cmd+Shift+Z = Redo
  if (key.getModifiers().isCommandDown() && key.getModifiers().isShiftDown()) {
    if (key ==
        juce::KeyPress('s', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier,
                       0)) {
      menuItemSelected(4, 0); // Trigger "Save Session As" menu item
      return true;
    }
    // Cmd+Shift+Z = Redo
    if (key ==
        juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier,
                       0)) {
      menuItemSelected(101, 1); // Trigger Redo menu item
      return true;
    }
  }

  // Item 60: Arrow key navigation for playbox
  if (key == juce::KeyPress::upKey) {
    m_clipGrid->movePlayboxUp();
    return true;
  }
  if (key == juce::KeyPress::downKey) {
    m_clipGrid->movePlayboxDown();
    return true;
  }
  if (key == juce::KeyPress::leftKey) {
    m_clipGrid->movePlayboxLeft();
    return true;
  }
  if (key == juce::KeyPress::rightKey) {
    m_clipGrid->movePlayboxRight();
    return true;
  }

  // Space bar = Trigger playbox button
  // Enter = Also trigger playbox button (user's preference)
  if (key == juce::KeyPress::spaceKey || key == juce::KeyPress::returnKey) {
    m_clipGrid->triggerPlayboxButton();
    return true;
  }

  // Escape = Stop All (not PANIC)
  if (key == juce::KeyPress::escapeKey) {
    onStopAll();
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
  // Show context menu (inherits HK Grotesk font from LookAndFeel)
  juce::PopupMenu menu;

  bool hasClip = m_sessionManager.hasClip(buttonIndex);
  int globalClipIndex = getGlobalClipIndex(buttonIndex);

  if (hasClip) {
    // Clip is loaded - show options
    auto clipData = m_sessionManager.getClip(buttonIndex);

    // Edit Clip at the top (most important action)
    menu.addItem(5, "Edit Clip...");
    menu.addSeparator();

    menu.addItem(1, "Load New Audio File...");
    menu.addItem(6, "Load Multiple Audio Files...");
    menu.addSeparator();

    // OCC144: Show in Finder and Edit in External Editor
#if JUCE_MAC
    menu.addItem(9, "Show in Finder");
#else
    menu.addItem(9, "Show in Explorer");
#endif
    bool hasWavEditor =
        m_externalToolManager &&
        m_externalToolManager->isToolConfigured(orpheus::ExternalToolManager::ToolType::WAVEditor);
    menu.addItem(10, "Edit in External Editor...", hasWavEditor);
    menu.addSeparator();

    // Set Color - will show ColorSwatchPicker popup
    menu.addItem(8, "Set Color...");

    menu.addSeparator();
    menu.addItem(4, "Stop Others On Play", true, m_stopOthersOnPlay[globalClipIndex]);
    menu.addItem(7, "Loop", true, m_loopEnabled[globalClipIndex]);
    menu.addSeparator();

    // OCC144: HotKey assignment in context menu
    bool hasHotKey = m_hotKeyManager && m_hotKeyManager->hasHotKey(globalClipIndex);
    juce::String hotkeyText = "Assign HotKey...";
    if (hasHotKey) {
      hotkeyText = "HotKey: " + m_hotKeyManager->getHotKeyDescription(globalClipIndex);
    }
    menu.addItem(11, hotkeyText);
    if (hasHotKey) {
      menu.addItem(12, "Clear HotKey");
    }

    // OCC144: MIDI Learn in context menu
    bool hasMidiNote = m_midiDeviceManager && m_midiDeviceManager->hasMidiNote(globalClipIndex);
    juce::String midiText = "MIDI Learn...";
    if (hasMidiNote) {
      midiText = "MIDI: " + m_midiDeviceManager->getMidiNoteDescription(globalClipIndex);
    }
    menu.addItem(13, midiText);
    if (hasMidiNote) {
      menu.addItem(14, "Clear MIDI Note");
    }
    menu.addSeparator();

    menu.addItem(2, "Remove Clip");
    menu.addSeparator();
    menu.addItem(3, "Clip Info: " + juce::String(clipData.displayName), false, false);
  } else {
    // Empty button - only show load option
    menu.addItem(1, "Load Audio File...");
    menu.addItem(6, "Load Multiple Audio Files...");
  }

  // Ensure menu uses HK Grotesk aesthetic (inherited from MainComponent's LookAndFeel)
  menu.setLookAndFeel(&m_hkGroteskLookAndFeel);

  menu.showMenuAsync(juce::PopupMenu::Options(), [this, buttonIndex, hasClip,
                                                  globalClipIndex](int result) {
    if (result == 5 && hasClip) {
      // Edit Clip - open edit dialog
      onClipDoubleClicked(buttonIndex);
    } else if (result == 1) {
      // Load audio file
      // Item 9: Warning if replacing existing clip
      bool shouldLoad = true;
      if (hasClip) {
        auto clipData = m_sessionManager.getClip(buttonIndex);
        shouldLoad = juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::WarningIcon, "Replace Clip?",
            "Button " + juce::String(buttonIndex + 1) + " already has a clip:\n\"" +
                juce::String(clipData.displayName) + "\"\n\n" +
                "Do you want to replace it with a new clip?",
            "Replace", "Cancel");
      }

      if (shouldLoad) {
        juce::FileChooser chooser("Select Audio File",
                                  juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                                  "*.wav;*.aiff;*.aif;*.flac");

        if (chooser.browseForFileToOpen()) {
          auto file = chooser.getResult();
          loadClipToButton(buttonIndex, file.getFullPathName());
        }
      }
    } else if (result == 2 && hasClip) {
      // Remove clip
      // Item 9: Warning before removing clip
      auto clipData = m_sessionManager.getClip(buttonIndex);
      bool confirmed = juce::AlertWindow::showOkCancelBox(
          juce::AlertWindow::WarningIcon, "Remove Clip?",
          "Remove \"" + juce::String(clipData.displayName) + "\" from button " +
              juce::String(buttonIndex + 1) + "?\n\n" + "This action cannot be undone.",
          "Remove", "Cancel");

      if (confirmed) {
        m_sessionManager.removeClip(buttonIndex);
        updateButtonFromClip(buttonIndex);
        DBG("MainComponent: Removed clip from button " << buttonIndex);
      }
    } else if (result == 4) {
      // Toggle "stop others on play" mode
      m_stopOthersOnPlay[globalClipIndex] = !m_stopOthersOnPlay[globalClipIndex];

      // CRITICAL: Persist to SessionManager
      if (m_sessionManager.hasClip(buttonIndex)) {
        auto clipData = m_sessionManager.getClip(buttonIndex);
        clipData.stopOthersEnabled = m_stopOthersOnPlay[globalClipIndex];
        m_sessionManager.setClip(buttonIndex, clipData);
      }

      // Update button visual state
      auto button = m_clipGrid->getButton(buttonIndex);
      if (button) {
        button->setStopOthersEnabled(m_stopOthersOnPlay[globalClipIndex]);
      }

      DBG("Button " << buttonIndex << " (global: " << globalClipIndex << "): Stop others on play = "
                    << (m_stopOthersOnPlay[globalClipIndex] ? "ON" : "OFF"));
    } else if (result == 7 && hasClip) {
      // Toggle loop mode
      m_loopEnabled[globalClipIndex] = !m_loopEnabled[globalClipIndex];

      // CRITICAL: Persist to SessionManager
      auto clipData = m_sessionManager.getClip(buttonIndex);
      clipData.loopEnabled = m_loopEnabled[globalClipIndex];
      m_sessionManager.setClip(buttonIndex, clipData);

      // Sync to AudioEngine (CRITICAL: Must update SDK loop state!)
      if (m_audioEngine) {
        m_audioEngine->setClipLoopMode(globalClipIndex, m_loopEnabled[globalClipIndex]);
      }

      // Update button visual state
      auto button = m_clipGrid->getButton(buttonIndex);
      if (button) {
        button->setLoopEnabled(m_loopEnabled[globalClipIndex]);
      }
      DBG("Button " << buttonIndex << " (global: " << globalClipIndex
                    << "): Loop = " << (m_loopEnabled[globalClipIndex] ? "ON" : "OFF"));
    } else if (result == 6) {
      // Load multiple audio files
      // Item 9: Warning if clips will be overwritten
      bool shouldLoad = true;

      // Count how many clips would be overwritten
      int overwriteCount = 0;
      int totalButtons = 48; // Per tab
      int filesToLoad = 0;

      juce::FileChooser chooser("Select Audio Files",
                                juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                                "*.wav;*.aiff;*.aif;*.flac");

      if (chooser.browseForMultipleFilesToOpen()) {
        auto files = chooser.getResults();
        filesToLoad = files.size();

        // Check how many existing clips would be overwritten
        for (int i = buttonIndex; i < juce::jmin(buttonIndex + filesToLoad, totalButtons); ++i) {
          if (m_sessionManager.hasClip(i)) {
            overwriteCount++;
          }
        }

        if (overwriteCount > 0) {
          juce::String message = "Loading " + juce::String(filesToLoad) +
                                 " files starting at button " + juce::String(buttonIndex + 1) +
                                 " will overwrite " + juce::String(overwriteCount) +
                                 " existing clip" + (overwriteCount > 1 ? "s" : "") + ".\n\n" +
                                 "This action cannot be undone.\n\n" + "Do you want to continue?";

          shouldLoad = juce::AlertWindow::showOkCancelBox(
              juce::AlertWindow::WarningIcon,
              "Overwrite " + juce::String(overwriteCount) + " Clip" +
                  (overwriteCount > 1 ? "s" : "") + "?",
              message, "Overwrite", "Cancel");
        }

        if (shouldLoad) {
          loadMultipleFiles(files, buttonIndex);
        }
      }
    } else if (result == 11 && hasClip) {
      // OCC144: Assign HotKey - show keyboard capture dialog
      auto* alertWindow = new juce::AlertWindow(
          "Assign HotKey", "Press a key to assign it to this clip.\n\nPress Escape to cancel.",
          juce::AlertWindow::QuestionIcon);
      alertWindow->addButton("Cancel", 0);

      // Create a capture component
      class KeyCaptureComponent : public juce::Component, public juce::KeyListener {
      public:
        juce::KeyPress capturedKey;
        std::function<void(const juce::KeyPress&)> onKeyCaptured;

        KeyCaptureComponent() {
          setWantsKeyboardFocus(true);
          addKeyListener(this);
        }

        bool keyPressed(const juce::KeyPress& key, juce::Component*) override {
          if (key.getKeyCode() != juce::KeyPress::escapeKey) {
            capturedKey = key;
            if (onKeyCaptured)
              onKeyCaptured(capturedKey);
          }
          return true;
        }
      };

      auto* captureWidget = new KeyCaptureComponent();
      alertWindow->addCustomComponent(captureWidget);

      alertWindow->enterModalState(
          true,
          juce::ModalCallbackFunction::create(
              [this, alertWindow, captureWidget, globalClipIndex](int result) {
                if (captureWidget && captureWidget->capturedKey.isValid()) {
                  // Assign the captured key
                  if (m_hotKeyManager) {
                    m_hotKeyManager->assignHotKey(globalClipIndex, captureWidget->capturedKey);
                    DBG("MainComponent: Assigned hotkey to clip " << globalClipIndex);
                  }
                }
                delete alertWindow;
              }),
          true);

      // Give focus to capture component
      if (captureWidget) {
        captureWidget->grabKeyboardFocus();
      }
    } else if (result == 12 && hasClip) {
      // OCC144: Clear HotKey
      if (m_hotKeyManager && m_hotKeyManager->hasHotKey(globalClipIndex)) {
        m_hotKeyManager->clearHotKey(globalClipIndex);
        DBG("MainComponent: Cleared hotkey for clip " << globalClipIndex);
      }
    } else if (result == 13 && hasClip) {
      // OCC144: MIDI Learn - start MIDI learn mode
      if (m_midiDeviceManager) {
        // Show alert that we're waiting for MIDI input
        auto* alertWindow = new juce::AlertWindow(
            "MIDI Learn",
            "Press a MIDI note to assign it to this clip.\n\nWaiting for MIDI input...",
            juce::AlertWindow::InfoIcon);
        alertWindow->addButton("Cancel", 0);

        // Store globalClipIndex for the callback
        int targetClipIndex = globalClipIndex;

        m_midiDeviceManager->startMidiLearnMode(
            [this, targetClipIndex, alertWindow](int note, int channel) {
              // MIDI note received - assign it
              m_midiDeviceManager->assignMidiNote(targetClipIndex, note, channel);
              DBG("MainComponent: Assigned MIDI note " << note << " (Ch " << channel << ") to clip "
                                                       << targetClipIndex);

              // Close the alert window
              juce::MessageManager::callAsync([alertWindow]() {
                alertWindow->exitModalState(0);
                delete alertWindow;
              });

              // Show confirmation
              juce::MessageManager::callAsync([note, channel]() {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::InfoIcon, "MIDI Assigned",
                    "Assigned " + orpheus::MIDIDeviceManager::noteNumberToName(note) +
                        " (Channel " + juce::String(channel) + ") to this clip.",
                    "OK");
              });
            });

        // Run the modal dialog - if cancelled, cancel MIDI learn mode
        alertWindow->enterModalState(true, juce::ModalCallbackFunction::create([this](int result) {
                                       if (result == 0) {
                                         // User cancelled
                                         m_midiDeviceManager->cancelMidiLearnMode();
                                       }
                                     }),
                                     true);
      }
    } else if (result == 14 && hasClip) {
      // OCC144: Clear MIDI Note
      if (m_midiDeviceManager && m_midiDeviceManager->hasMidiNote(globalClipIndex)) {
        m_midiDeviceManager->clearMidiNote(globalClipIndex);
        DBG("MainComponent: Cleared MIDI note for clip " << globalClipIndex);
      }
    } else if (result == 9 && hasClip) {
      // OCC144: Show in Finder/Explorer - reveal audio file in native file manager
      auto clipData = m_sessionManager.getClip(buttonIndex);
      juce::File audioFile(clipData.filePath);
      if (audioFile.existsAsFile()) {
        audioFile.revealToUser();
        DBG("MainComponent: Revealed " << clipData.displayName << " in file manager");
      } else {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon, "File Not Found",
            "The audio file no longer exists:\n" + juce::String(clipData.filePath), "OK");
      }
    } else if (result == 10 && hasClip) {
      // OCC144: Edit in External Editor - launch configured WAV editor with file
      auto clipData = m_sessionManager.getClip(buttonIndex);
      juce::File audioFile(clipData.filePath);
      if (audioFile.existsAsFile()) {
        if (m_externalToolManager->launchTool(orpheus::ExternalToolManager::ToolType::WAVEditor,
                                              audioFile)) {
          DBG("MainComponent: Launched WAV editor for " << clipData.displayName);
        } else {
          juce::AlertWindow::showMessageBoxAsync(
              juce::AlertWindow::WarningIcon, "Launch Failed",
              "Could not launch the configured WAV editor.\n\n"
              "Check Setup → WAV Editor to configure an editor application.",
              "OK");
        }
      } else {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon, "File Not Found",
            "The audio file no longer exists:\n" + juce::String(clipData.filePath), "OK");
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

  // Item 60: Move playbox to the triggered button (follows last clip launched)
  m_clipGrid->setPlayboxIndex(buttonIndex);

  // Calculate global clip index (tab-aware: 0-383 for 8 tabs × 48 buttons)
  int globalClipIndex = getGlobalClipIndex(buttonIndex);

  // Toggle play/stop based on current visual state
  // Timer will sync visual state with SDK state at 75fps
  auto currentState = button->getState();

  if (currentState == ClipButton::State::Playing) {
    // Stop the clip
    if (m_audioEngine) {
      m_audioEngine->stopClip(globalClipIndex);
    }
    button->setState(ClipButton::State::Loaded);
    DBG("Button " + juce::String(buttonIndex) + " (global: " + juce::String(globalClipIndex) +
        "): Stopped via keyboard/click");
  } else if (currentState == ClipButton::State::Loaded) {
    // Check if "stop others on play" mode is enabled for this button (use global index)
    if (m_stopOthersOnPlay[globalClipIndex]) {
      // Stop all other playing clips ON THIS TAB
      for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
        if (i != buttonIndex) {
          auto otherButton = m_clipGrid->getButton(i);
          if (otherButton && otherButton->getState() == ClipButton::State::Playing) {
            int otherGlobalIndex = getGlobalClipIndex(i);
            if (m_audioEngine) {
              m_audioEngine->stopClip(otherGlobalIndex);
            }
            otherButton->setState(ClipButton::State::Loaded);
            DBG("Button " + juce::String(i) + " (global: " + juce::String(otherGlobalIndex) +
                "): Stopped by 'stop others' from button " + juce::String(buttonIndex) +
                " (global: " + juce::String(globalClipIndex) + ")");
          }
        }
      }
    }

    // Start the clip
    if (m_audioEngine) {
      m_audioEngine->startClip(globalClipIndex);
    }
    button->setState(ClipButton::State::Playing);
    DBG("Button " + juce::String(buttonIndex) + " (global: " + juce::String(globalClipIndex) +
        "): Started playing via keyboard/click");
  }

  // CRITICAL: Restore keyboard focus to Edit Dialog if it's open
  // Edit Dialog should always have keyboard priority when visible
  if (m_currentEditDialog != nullptr && m_currentEditDialog->isVisible()) {
    m_currentEditDialog->grabKeyboardFocus();
  }
}

void MainComponent::onClipDoubleClicked(int buttonIndex) {
  DBG("MainComponent: Button " + juce::String(buttonIndex) + " double-clicked (edit dialog)");

  // Check if clip is loaded
  if (!m_sessionManager.hasClip(buttonIndex)) {
    DBG("MainComponent: Button " + juce::String(buttonIndex) + " has no clip loaded");
    return;
  }

  // Calculate global clip index for multi-tab isolation
  int globalClipIndex = getGlobalClipIndex(buttonIndex);

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

  // Create edit dialog (pass AudioEngine and GLOBAL clip index for main grid clip control)
  auto* dialog = new ClipEditDialog(m_audioEngine.get(), globalClipIndex);
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

  // Feature 5: Gain
  metadata.gainDb = clipData.gainDb;

  // Sprint 2: Loop state (sync from MainComponent's internal state using global index)
  metadata.loopEnabled = m_loopEnabled[globalClipIndex];
  metadata.stopOthersEnabled = m_stopOthersOnPlay[globalClipIndex];

  dialog->setClipMetadata(metadata);

  // Set up callbacks
  dialog->onOkClicked = [this, buttonIndex, globalClipIndex,
                         dialog](const ClipEditDialog::ClipMetadata& edited) {
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

    // Feature 5: Gain
    clipData.gainDb = edited.gainDb;

    // CRITICAL: Persist loop and stopOthers state
    clipData.loopEnabled = edited.loopEnabled;
    clipData.stopOthersEnabled = edited.stopOthersEnabled;

    // Persist to SessionManager
    m_sessionManager.setClip(buttonIndex, clipData);

    // Apply trim/fade metadata to AudioEngine (use global index for multi-tab isolation)
    if (m_audioEngine) {
      bool updated = m_audioEngine->updateClipMetadata(
          globalClipIndex, clipData.trimInSamples, clipData.trimOutSamples, clipData.fadeInSeconds,
          clipData.fadeOutSeconds, juce::String(clipData.fadeInCurve),
          juce::String(clipData.fadeOutCurve));

      if (updated) {
        DBG("MainComponent: Applied trim/fade metadata to AudioEngine for button "
            << buttonIndex << " (global: " << globalClipIndex << ")");
      } else {
        DBG("MainComponent: Failed to apply trim/fade metadata to AudioEngine for button "
            << buttonIndex << " (global: " << globalClipIndex << ")");
      }

      // Apply loop mode to AudioEngine (CRITICAL: Must apply loop state using global index!)
      bool loopUpdated = m_audioEngine->setClipLoopMode(globalClipIndex, edited.loopEnabled);
      if (loopUpdated) {
        // Sync MainComponent's internal loop state array
        m_loopEnabled[globalClipIndex] = edited.loopEnabled;

        DBG("MainComponent: Applied loop mode to AudioEngine for button "
            << buttonIndex << " (global: " << globalClipIndex
            << ") = " << (edited.loopEnabled ? "enabled" : "disabled"));
      } else {
        DBG("MainComponent: Failed to apply loop mode to AudioEngine for button "
            << buttonIndex << " (global: " << globalClipIndex << ")");
      }
    }

    // Update button visual state
    auto button = m_clipGrid->getButton(buttonIndex);
    if (button) {
      button->setClipName(edited.displayName);
      button->setClipColor(edited.color);
      button->setClipGroup(edited.clipGroup);
      button->setLoopEnabled(edited.loopEnabled); // CRITICAL: Sync loop visual state

      // CRITICAL: Sync fade indicator visual state (not handled by 75fps polling)
      button->setFadeInEnabled(edited.fadeInSeconds > 0.0);
      button->setFadeOutEnabled(edited.fadeOutSeconds > 0.0);

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

  dialog->onCancelClicked = [this, buttonIndex, globalClipIndex, dialog, metadata]() {
    // CRITICAL: Restore original metadata on CANCEL (discard temporary edits)
    // Edits are live during preview, but must be reverted if user cancels

    // Restore SessionManager clip data
    auto clipData = m_sessionManager.getClip(buttonIndex);
    clipData.displayName = metadata.displayName.toStdString();
    clipData.color = metadata.color;
    clipData.clipGroup = metadata.clipGroup;
    clipData.trimInSamples = metadata.trimInSamples;
    clipData.trimOutSamples = metadata.trimOutSamples;
    clipData.fadeInSeconds = metadata.fadeInSeconds;
    clipData.fadeOutSeconds = metadata.fadeOutSeconds;
    clipData.fadeInCurve = metadata.fadeInCurve.toStdString();
    clipData.fadeOutCurve = metadata.fadeOutCurve.toStdString();
    clipData.gainDb = metadata.gainDb;
    clipData.loopEnabled = metadata.loopEnabled;
    clipData.stopOthersEnabled = metadata.stopOthersEnabled;
    m_sessionManager.setClip(buttonIndex, clipData);

    // Restore SDK state (trim points, fades, loop mode)
    if (m_audioEngine) {
      m_audioEngine->updateClipMetadata(
          globalClipIndex, metadata.trimInSamples, metadata.trimOutSamples, metadata.fadeInSeconds,
          metadata.fadeOutSeconds, metadata.fadeInCurve, metadata.fadeOutCurve);
      m_audioEngine->setClipLoopMode(globalClipIndex, metadata.loopEnabled);
      m_loopEnabled[globalClipIndex] = metadata.loopEnabled;
      m_stopOthersOnPlay[globalClipIndex] = metadata.stopOthersEnabled;
    }

    // Restore button visual state
    auto button = m_clipGrid->getButton(buttonIndex);
    if (button) {
      button->setClipName(metadata.displayName);
      button->setClipColor(metadata.color);
      button->setClipGroup(metadata.clipGroup);
      button->setLoopEnabled(metadata.loopEnabled);
      button->setFadeInEnabled(metadata.fadeInSeconds > 0.0);
      button->setFadeOutEnabled(metadata.fadeOutSeconds > 0.0);
      button->setStopOthersEnabled(metadata.stopOthersEnabled);

      // Restore trimmed duration
      if (metadata.sampleRate > 0) {
        int64_t trimmedSamples = metadata.trimOutSamples - metadata.trimInSamples;
        double durationSeconds = static_cast<double>(trimmedSamples) / metadata.sampleRate;
        button->setClipDuration(durationSeconds);
      }
    }

    DBG("MainComponent: CANCEL - Restored original metadata for button " << buttonIndex);

    // Close dialog
    dialog->setVisible(false);
    delete dialog;
    m_currentEditDialog = nullptr; // Clear reference to allow new dialog
  };

  // Real-time color update: Repaint button immediately when color changes (75fps)
  dialog->onColorChanged = [this, buttonIndex](const juce::Colour& newColor) {
    auto button = m_clipGrid->getButton(buttonIndex);
    if (button) {
      button->setClipColor(newColor); // Triggers immediate repaint (75fps grid refresh)
    }

    // CRITICAL: Persist color to SessionManager (prevents Edit Dialog from overwriting it)
    if (m_sessionManager.hasClip(buttonIndex)) {
      auto clipData = m_sessionManager.getClip(buttonIndex);
      clipData.color = newColor;
      m_sessionManager.setClip(buttonIndex, clipData);
    }

    DBG("Button " << buttonIndex << ": Color changed in real-time to " << newColor.toString());
  };

  // Show dialog as modal
  dialog->setSize(700, 850); // Expanded for all phases, increased height to prevent button crushing
  dialog->setCentrePosition(getWidth() / 2, getHeight() / 2);
  addAndMakeVisible(dialog);
  dialog->toFront(true);
  dialog->grabKeyboardFocus(); // CRITICAL: Grab focus to ensure keyboard commands work immediately
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
  // Item 32: Audio asset copying - copy file to project folder
  juce::File sourceFile(filePath);
  juce::String finalPath = filePath;

  // Check if we should copy the audio file to project folder
  // Only copy if source is outside our project audio folder
  juce::File projectAudioDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                   .getChildFile("Orpheus Clip Composer")
                                   .getChildFile("Audio");

  if (!sourceFile.isAChildOf(projectAudioDir)) {
    // Ask user if they want to copy the file
    int result = juce::AlertWindow::showYesNoCancelBox(
        juce::AlertWindow::QuestionIcon, "Copy Audio File?",
        juce::String("Would you like to copy this audio file to your project folder?\n\n") +
            "This ensures your session remains portable even if the original file is moved.\n\n" +
            "File: " + sourceFile.getFileName(),
        "Copy to Project", "Link to Original", "Cancel");

    if (result == 0) {
      // Cancel - don't load
      return;
    } else if (result == 1) {
      // Copy to project folder
      if (!projectAudioDir.exists()) {
        projectAudioDir.createDirectory();
      }

      // Create unique filename if file already exists
      juce::File destFile = projectAudioDir.getChildFile(sourceFile.getFileName());
      int counter = 1;
      while (destFile.exists()) {
        juce::String nameWithoutExt = sourceFile.getFileNameWithoutExtension();
        juce::String ext = sourceFile.getFileExtension();
        destFile = projectAudioDir.getChildFile(nameWithoutExt + "_" + juce::String(counter) + ext);
        counter++;
      }

      if (sourceFile.copyFileTo(destFile)) {
        finalPath = destFile.getFullPathName();
        DBG("MainComponent: Copied audio file to project folder: " << finalPath);
      } else {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Copy Failed",
                                               "Failed to copy audio file to project folder.\n"
                                               "Using original file location instead.",
                                               "OK");
      }
    }
    // else result == 2: Link to original (use original path)
  }

  // Use SessionManager to load the clip (with final path)
  bool success = m_sessionManager.loadClip(buttonIndex, finalPath);

  if (success) {
    // Calculate global clip index for multi-tab isolation
    int globalClipIndex = getGlobalClipIndex(buttonIndex);

    // Load audio file into AudioEngine for playback (use global index)
    if (m_audioEngine) {
      bool audioLoaded = m_audioEngine->loadClip(globalClipIndex, filePath);
      if (!audioLoaded) {
        DBG("MainComponent: Failed to load audio into engine for button "
            << buttonIndex << " (global: " << globalClipIndex << ")");
      } else {
        // Check for sample rate mismatch and warn user
        auto metadata = m_audioEngine->getClipMetadata(globalClipIndex);
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

  // Calculate global clip indices for multi-tab isolation
  int sourceGlobalIndex = getGlobalClipIndex(sourceButtonIndex);
  int targetGlobalIndex = getGlobalClipIndex(targetButtonIndex);

  // CRITICAL: Check if either clip is currently playing BEFORE swapping
  // Playback state is tied to GLOBAL clip index in AudioEngine, so we must stop playback first
  auto sourceButton = m_clipGrid->getButton(sourceButtonIndex);
  auto targetButton = m_clipGrid->getButton(targetButtonIndex);

  bool sourceWasPlaying = (sourceButton && sourceButton->getState() == ClipButton::State::Playing);
  bool targetWasPlaying = (targetButton && targetButton->getState() == ClipButton::State::Playing);

  // Stop both clips if playing (prevents orphaned playback state)
  if (sourceWasPlaying && m_audioEngine) {
    m_audioEngine->stopClip(sourceGlobalIndex);
    DBG("MainComponent: Stopped source clip (button "
        << sourceButtonIndex << ", global: " << sourceGlobalIndex << ") before swap");
  }
  if (targetWasPlaying && m_audioEngine) {
    m_audioEngine->stopClip(targetGlobalIndex);
    DBG("MainComponent: Stopped target clip (button "
        << targetButtonIndex << ", global: " << targetGlobalIndex << ") before swap");
  }

  // Swap clips in SessionManager
  m_sessionManager.swapClips(sourceButtonIndex, targetButtonIndex);

  // Swap stop-others mode flags (use global indices)
  std::swap(m_stopOthersOnPlay[sourceGlobalIndex], m_stopOthersOnPlay[targetGlobalIndex]);

  // Swap loop mode flags (use global indices)
  std::swap(m_loopEnabled[sourceGlobalIndex], m_loopEnabled[targetGlobalIndex]);

  // Update both buttons visually (this reloads clips into AudioEngine at new positions)
  updateButtonFromClip(sourceButtonIndex);
  updateButtonFromClip(targetButtonIndex);

  // Restart clips at their NEW positions if they were playing
  if (sourceWasPlaying && m_audioEngine && m_sessionManager.hasClip(targetButtonIndex)) {
    m_audioEngine->startClip(targetGlobalIndex);
    if (targetButton) {
      targetButton->setState(ClipButton::State::Playing);
    }
    DBG("MainComponent: Restarted source clip at new position (button "
        << targetButtonIndex << ", global: " << targetGlobalIndex << ")");
  }
  if (targetWasPlaying && m_audioEngine && m_sessionManager.hasClip(sourceButtonIndex)) {
    m_audioEngine->startClip(sourceGlobalIndex);
    if (sourceButton) {
      sourceButton->setState(ClipButton::State::Playing);
    }
    DBG("MainComponent: Restarted target clip at new position (button "
        << sourceButtonIndex << ", global: " << sourceGlobalIndex << ")");
  }
}

void MainComponent::updateButtonFromClip(int buttonIndex) {
  auto button = m_clipGrid->getButton(buttonIndex);
  if (!button)
    return;

  if (m_sessionManager.hasClip(buttonIndex)) {
    // Calculate global clip index for multi-tab isolation
    int globalClipIndex = getGlobalClipIndex(buttonIndex);

    // Get real clip metadata from SessionManager
    auto clipData = m_sessionManager.getClip(buttonIndex);

    // Load clip into audio engine first (use global index)
    if (m_audioEngine) {
      m_audioEngine->loadClip(globalClipIndex, juce::String(clipData.filePath));

      // CRITICAL: Apply trim/fade metadata to AudioEngine (use global index)
      bool metadataApplied = m_audioEngine->updateClipMetadata(
          globalClipIndex, clipData.trimInSamples, clipData.trimOutSamples, clipData.fadeInSeconds,
          clipData.fadeOutSeconds, juce::String(clipData.fadeInCurve),
          juce::String(clipData.fadeOutCurve));

      if (metadataApplied) {
        DBG("MainComponent: Applied trim/fade metadata to AudioEngine for button "
            << buttonIndex << " (global: " << globalClipIndex << ")");
      } else {
        DBG("MainComponent: Failed to apply trim/fade metadata to AudioEngine for button "
            << buttonIndex << " (global: " << globalClipIndex << ")");
      }

      // CRITICAL: Apply loop mode to AudioEngine (use global index)
      bool loopApplied = m_audioEngine->setClipLoopMode(globalClipIndex, clipData.loopEnabled);
      if (!loopApplied) {
        DBG("MainComponent: Failed to apply loop mode to AudioEngine for button "
            << buttonIndex << " (global: " << globalClipIndex << ")");
      }
    }

    // CRITICAL: Sync MainComponent's internal state arrays from SessionManager clipData (use global
    // index)
    m_loopEnabled[globalClipIndex] = clipData.loopEnabled;
    m_stopOthersOnPlay[globalClipIndex] = clipData.stopOthersEnabled;

    // Check if clip is currently playing and restore correct button state (use global index)
    if (m_audioEngine && m_audioEngine->isClipPlaying(globalClipIndex)) {
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

    // Restore loop state (from synced array using global index)
    button->setLoopEnabled(m_loopEnabled[globalClipIndex]);

    // Set stop others indicator (from synced array using global index)
    button->setStopOthersEnabled(m_stopOthersOnPlay[globalClipIndex]);

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

  // Feature 4: Update tab index on all buttons for consecutive numbering
  // Tab 1 = clips 1-48, Tab 2 = clips 49-96, Tab 3 = clips 97-144, etc.
  for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
    auto button = m_clipGrid->getButton(i);
    if (button) {
      button->setTabIndex(tabIndex);
    }
  }

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
  return {"File", "Edit", "Session", "Setup", "Display", "Audio", "Help"};
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
  } else if (topLevelMenuIndex == 1) // Edit menu (OCC117)
  {
    bool canUndo = m_undoManager && m_undoManager->canUndo();
    bool canRedo = m_undoManager && m_undoManager->canRedo();
    juce::String undoText = canUndo ? ("Undo " + m_undoManager->getUndoDescription()) : "Undo";
    juce::String redoText = canRedo ? ("Redo " + m_undoManager->getRedoDescription()) : "Redo";

    menu.addItem(100, undoText, canUndo);
    menu.addItem(101, redoText, canRedo);
    menu.addSeparator();
    menu.addItem(102, "Paste Special...", m_hasClipInClipboard);
  } else if (topLevelMenuIndex == 2) // Session menu
  {
    menu.addItem(10, "Clear All Clips");
    menu.addItem(14, "Clear Current Tab");
    menu.addSeparator();
    menu.addItem(11, "Stop All Clips");
    menu.addItem(12, "PANIC");
    menu.addSeparator();
    menu.addItem(13, "Keyboard Shortcuts...");
    menu.addSeparator();
    menu.addItem(15, "Toggle Session History Window", true, m_sessionHistoryWindow->isVisible());
  } else if (topLevelMenuIndex == 3) // Setup menu (OCC116, updated OCC144)
  {
    // OCC144: Removed Search Utility and File Browser (use native Finder via context menu)
    menu.addItem(200, "WAV Editor...");
    menu.addSeparator();
    menu.addItem(203, "HotKey Setup...");
    menu.addItem(204, "MIDI Devices...");
    menu.addSeparator();
    menu.addItem(205, "MIDI Monitor...");
  } else if (topLevelMenuIndex == 4) // Display menu (OCC117)
  {
    // Page Tabs submenu
    juce::PopupMenu pageTabsMenu;
    pageTabsMenu.addItem(300, "Small", true,
                         m_displayPreferences->getPageTabHeight() ==
                             orpheus::DisplayPreferences::Size::Small);
    pageTabsMenu.addItem(301, "Medium", true,
                         m_displayPreferences->getPageTabHeight() ==
                             orpheus::DisplayPreferences::Size::Medium);
    pageTabsMenu.addItem(302, "Large", true,
                         m_displayPreferences->getPageTabHeight() ==
                             orpheus::DisplayPreferences::Size::Large);
    menu.addSubMenu("Page Tabs", pageTabsMenu);

    // Status Bar submenu
    juce::PopupMenu statusBarMenu;
    statusBarMenu.addItem(310, "Small", true,
                          m_displayPreferences->getStatusBarHeight() ==
                              orpheus::DisplayPreferences::Size::Small);
    statusBarMenu.addItem(311, "Medium", true,
                          m_displayPreferences->getStatusBarHeight() ==
                              orpheus::DisplayPreferences::Size::Medium);
    statusBarMenu.addItem(312, "Large", true,
                          m_displayPreferences->getStatusBarHeight() ==
                              orpheus::DisplayPreferences::Size::Large);
    menu.addSubMenu("Status Bar", statusBarMenu);

    // Bevel Width submenu
    juce::PopupMenu bevelMenu;
    bevelMenu.addItem(320, "None", true,
                      m_displayPreferences->getBevelWidth() ==
                          orpheus::DisplayPreferences::BevelWidth::None);
    bevelMenu.addItem(321, "5%", true,
                      m_displayPreferences->getBevelWidth() ==
                          orpheus::DisplayPreferences::BevelWidth::Percent5);
    bevelMenu.addItem(322, "10%", true,
                      m_displayPreferences->getBevelWidth() ==
                          orpheus::DisplayPreferences::BevelWidth::Percent10);
    bevelMenu.addItem(323, "15%", true,
                      m_displayPreferences->getBevelWidth() ==
                          orpheus::DisplayPreferences::BevelWidth::Percent15);
    bevelMenu.addItem(324, "20%", true,
                      m_displayPreferences->getBevelWidth() ==
                          orpheus::DisplayPreferences::BevelWidth::Percent20);
    menu.addSubMenu("Bevel Width", bevelMenu);

    // Button Text Mode submenu
    juce::PopupMenu textModeMenu;
    textModeMenu.addItem(330, "None", true,
                         m_displayPreferences->getButtonTextMode() ==
                             orpheus::DisplayPreferences::ButtonTextMode::None);
    textModeMenu.addItem(331, "Hot Key", true,
                         m_displayPreferences->getButtonTextMode() ==
                             orpheus::DisplayPreferences::ButtonTextMode::HotKey);
    textModeMenu.addItem(332, "MIDI Note", true,
                         m_displayPreferences->getButtonTextMode() ==
                             orpheus::DisplayPreferences::ButtonTextMode::MidiNote);
    menu.addSubMenu("Button Text", textModeMenu);

    menu.addSeparator();
    menu.addItem(340, "Level Meters...");
  } else if (topLevelMenuIndex == 5) // Audio menu
  {
    menu.addItem(20, "Audio I/O Settings...");
    menu.addSeparator();
    menu.addItem(21, "Show Audio Engine Info");
    menu.addSeparator();
    menu.addItem(22, "Load Multiple Audio Files...");
  } else if (topLevelMenuIndex == 6) // Help menu (OCC144)
  {
    menu.addItem(400, "Keyboard Shortcuts...");
    menu.addSeparator();
    menu.addItem(401, "About Orpheus Clip Composer...");
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

  case 3: // Save Session (OCC144: Now tracks current file)
  {
    // Check if session has been saved before
    auto currentFile = m_sessionManager.getCurrentFile();
    if (currentFile.existsAsFile()) {
      // Save to existing file without prompting
      if (m_sessionManager.saveSession(currentFile)) {
        DBG("MainComponent: Successfully saved session: " + currentFile.getFileName());
      } else {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon, "Save Failed",
            "Could not save session file:\n" + currentFile.getFullPathName(), "OK");
      }
    } else {
      // No previous file - show save dialog (same as Save As)
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
              "Could not save session file:\n" + file.getFullPathName(), "OK");
        }
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
    shortcuts += "  ↑ ↓ ← → ......... Move playbox around grid\n";
    shortcuts += "  Space/Enter ..... Trigger playbox button\n";
    shortcuts += "  Esc ............. Stop All Clips (with fade)\n";
    shortcuts += "  Cmd/Ctrl+S ...... Save Session\n";
    shortcuts += "  Cmd/Ctrl+Shift+S  Save Session As\n";
    shortcuts += "  Cmd/Ctrl+, ...... Preferences (coming soon)\n";
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

  case 14: // Clear Current Tab (Item 8)
  {
    // Get current tab index
    int currentTab = m_sessionManager.getActiveTab();
    int clipCount = 0;

    // Count how many clips are on current tab
    for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
      if (m_sessionManager.hasClip(i)) {
        clipCount++;
      }
    }

    if (clipCount == 0) {
      juce::AlertWindow::showMessageBoxAsync(
          juce::AlertWindow::InfoIcon, "Clear Tab",
          "Tab " + juce::String(currentTab + 1) + " has no clips to clear.", "OK");
      break;
    }

    // Warn user before clearing tab
    juce::String message = "This will remove all " + juce::String(clipCount) + " clips from Tab " +
                           juce::String(currentTab + 1) + ".\n\n" +
                           "This action cannot be undone.\n\n" + "Are you sure?";

    bool confirmed = juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::WarningIcon, "Clear Tab " + juce::String(currentTab + 1) + "?", message,
        "Clear Tab", "Cancel");

    if (confirmed) {
      // Stop playing clips on current tab first
      for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
        auto button = m_clipGrid->getButton(i);
        if (button && button->getState() == ClipButton::State::Playing) {
          int globalIndex = getGlobalClipIndex(i);
          if (m_audioEngine) {
            m_audioEngine->stopClip(globalIndex);
          }
        }
      }

      // Clear all clips on current tab
      for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
        if (m_sessionManager.hasClip(i)) {
          m_sessionManager.removeClip(i);
          auto button = m_clipGrid->getButton(i);
          if (button) {
            button->clearClip();
          }

          // Clear internal state for this button's global index
          int globalIndex = getGlobalClipIndex(i);
          m_loopEnabled[globalIndex] = false;
          m_stopOthersOnPlay[globalIndex] = false;
        }
      }

      DBG("MainComponent: Cleared " << clipCount << " clips from Tab " << (currentTab + 1));
    }
    break;
  }

  case 15: // Toggle Session History Window
  {
    bool isVisible = m_sessionHistoryWindow->isVisible();
    m_sessionHistoryWindow->setVisible(!isVisible);
    DBG("MainComponent: Session History Window visibility toggled to "
        << (!isVisible ? "visible" : "hidden"));
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

  //==============================================================================
  // Edit Menu (OCC117)
  case 100: // Undo
    if (m_undoManager && m_undoManager->canUndo()) {
      m_undoManager->undo();
      // Refresh UI to reflect undone state
      for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
        updateButtonFromClip(i);
      }
      DBG("MainComponent: Undo executed");
    }
    break;

  case 101: // Redo
    if (m_undoManager && m_undoManager->canRedo()) {
      m_undoManager->redo();
      // Refresh UI to reflect redone state
      for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
        updateButtonFromClip(i);
      }
      DBG("MainComponent: Redo executed");
    }
    break;

  case 102: // Paste Special
  {
    if (!m_hasClipInClipboard) {
      juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Paste Special",
                                             "No clip in clipboard. Copy a clip first with Cmd+C.",
                                             "OK");
      break;
    }

    auto* dialog =
        new PasteSpecialDialog(&m_sessionManager, m_clipboardData, m_sessionManager.getActiveTab());
    dialog->onOkClicked = [this, dialog]() {
      auto options = dialog->getOptions();
      auto targetIndices = dialog->getTargetIndices();

      // Apply paste special to all target indices
      for (int targetIndex : targetIndices) {
        int buttonIndex = targetIndex % 48;
        int tabIndex = targetIndex / 48;

        // Only paste if target has a clip
        if (m_sessionManager.hasClip(buttonIndex, tabIndex)) {
          auto targetClip = m_sessionManager.getClip(buttonIndex, tabIndex);

          // Apply selected options
          if (options.gainAbsolute) {
            targetClip.gainDb = m_clipboardData.gainDb;
          } else if (options.gainRelative) {
            targetClip.gainDb += options.gainRelativeDb;
          }

          if (options.fadeIn) {
            targetClip.fadeInSeconds = m_clipboardData.fadeInSeconds;
          }
          if (options.fadeInCurve) {
            targetClip.fadeInCurve = m_clipboardData.fadeInCurve;
          }
          if (options.fadeOut) {
            targetClip.fadeOutSeconds = m_clipboardData.fadeOutSeconds;
          }
          if (options.fadeOutCurve) {
            targetClip.fadeOutCurve = m_clipboardData.fadeOutCurve;
          }

          if (options.color) {
            targetClip.color = m_clipboardData.color;
          }
          if (options.clipGroup) {
            targetClip.clipGroup = m_clipboardData.clipGroup;
          }
          if (options.loop) {
            targetClip.loopEnabled = m_clipboardData.loopEnabled;
          }
          if (options.stopOthers) {
            targetClip.stopOthersEnabled = m_clipboardData.stopOthersEnabled;
          }

          m_sessionManager.setClip(buttonIndex, targetClip, tabIndex);
        }
      }

      // Refresh current tab display
      for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
        updateButtonFromClip(i);
      }

      dialog->setVisible(false);
      delete dialog;
      DBG("MainComponent: Paste Special applied to " << targetIndices.size() << " clips");
    };

    dialog->onCancelClicked = [dialog]() {
      dialog->setVisible(false);
      delete dialog;
    };

    dialog->setCentrePosition(getWidth() / 2, getHeight() / 2);
    addAndMakeVisible(dialog);
    dialog->toFront(true);
    break;
  }

  //==============================================================================
  // Setup Menu (OCC116, updated OCC144)
  case 200: // WAV Editor
  {
    juce::FileChooser chooser("Select WAV Editor Application", juce::File("/Applications"),
                              "*.app");
    if (chooser.browseForFileToOpen()) {
      m_externalToolManager->setToolPath(orpheus::ExternalToolManager::ToolType::WAVEditor,
                                         chooser.getResult());
      DBG("MainComponent: WAV Editor set to " << chooser.getResult().getFullPathName());
    }
    break;
  }

    // OCC144: Removed case 201 (Search Utility) and case 202 (File Browser)
    // Use native Finder integration via clip right-click menu instead

  case 203: // HotKey Setup
  {
    auto* dialog = new HotKeySetupDialog(m_hotKeyManager.get());
    dialog->onOkClicked = [dialog]() {
      dialog->setVisible(false);
      delete dialog;
    };
    dialog->onCancelClicked = [dialog]() {
      dialog->setVisible(false);
      delete dialog;
    };

    dialog->setCentrePosition(getWidth() / 2, getHeight() / 2);
    addAndMakeVisible(dialog);
    dialog->toFront(true);
    break;
  }

  case 204: // MIDI Devices
  {
    auto* dialog = new MIDIDevicesDialog(m_midiDeviceManager.get());
    dialog->onOkClicked = [dialog]() {
      dialog->setVisible(false);
      delete dialog;
    };
    dialog->onCancelClicked = [dialog]() {
      dialog->setVisible(false);
      delete dialog;
    };
    dialog->onMonitorClicked = [this, dialog]() {
      // Open MIDI Monitor window
      if (!m_midiMonitorWindow) {
        m_midiMonitorWindow = std::make_unique<MIDIMonitorWindow>(m_midiDeviceManager.get());
      }
      m_midiMonitorWindow->setVisible(true);
      m_midiMonitorWindow->toFront(true);
    };

    dialog->setCentrePosition(getWidth() / 2, getHeight() / 2);
    addAndMakeVisible(dialog);
    dialog->toFront(true);
    break;
  }

  case 205: // MIDI Monitor
  {
    if (!m_midiMonitorWindow) {
      m_midiMonitorWindow = std::make_unique<MIDIMonitorWindow>(m_midiDeviceManager.get());
    }
    m_midiMonitorWindow->setVisible(true);
    m_midiMonitorWindow->toFront(true);
    break;
  }

  //==============================================================================
  // Display Menu (OCC117)
  case 300: // Page Tabs Small
    m_displayPreferences->setPageTabHeight(orpheus::DisplayPreferences::Size::Small);
    break;
  case 301: // Page Tabs Medium
    m_displayPreferences->setPageTabHeight(orpheus::DisplayPreferences::Size::Medium);
    break;
  case 302: // Page Tabs Large
    m_displayPreferences->setPageTabHeight(orpheus::DisplayPreferences::Size::Large);
    break;

  case 310: // Status Bar Small
    m_displayPreferences->setStatusBarHeight(orpheus::DisplayPreferences::Size::Small);
    break;
  case 311: // Status Bar Medium
    m_displayPreferences->setStatusBarHeight(orpheus::DisplayPreferences::Size::Medium);
    break;
  case 312: // Status Bar Large
    m_displayPreferences->setStatusBarHeight(orpheus::DisplayPreferences::Size::Large);
    break;

  case 320: // Bevel None
    m_displayPreferences->setBevelWidth(orpheus::DisplayPreferences::BevelWidth::None);
    break;
  case 321: // Bevel 5%
    m_displayPreferences->setBevelWidth(orpheus::DisplayPreferences::BevelWidth::Percent5);
    break;
  case 322: // Bevel 10%
    m_displayPreferences->setBevelWidth(orpheus::DisplayPreferences::BevelWidth::Percent10);
    break;
  case 323: // Bevel 15%
    m_displayPreferences->setBevelWidth(orpheus::DisplayPreferences::BevelWidth::Percent15);
    break;
  case 324: // Bevel 20%
    m_displayPreferences->setBevelWidth(orpheus::DisplayPreferences::BevelWidth::Percent20);
    break;

  case 330: // Button Text None
    m_displayPreferences->setButtonTextMode(orpheus::DisplayPreferences::ButtonTextMode::None);
    break;
  case 331: // Button Text Hot Key
    m_displayPreferences->setButtonTextMode(orpheus::DisplayPreferences::ButtonTextMode::HotKey);
    break;
  case 332: // Button Text MIDI Note
    m_displayPreferences->setButtonTextMode(orpheus::DisplayPreferences::ButtonTextMode::MidiNote);
    break;

  case 340: // Level Meters
  {
    if (!m_levelMetersWindow) {
      m_levelMetersWindow = std::make_unique<LevelMetersWindow>(m_audioEngine.get());
    }
    m_levelMetersWindow->setVisible(true);
    m_levelMetersWindow->toFront(true);
    break;
  }

  //==============================================================================
  // Help Menu (OCC144)
  case 400: // Keyboard Shortcuts (duplicated from Session menu for discoverability)
  {
    juce::String shortcuts = "=== ORPHEUS CLIP COMPOSER - KEYBOARD SHORTCUTS ===\n\n";
    shortcuts += "GLOBAL SHORTCUTS:\n";
    shortcuts += "  ↑ ↓ ← → ......... Move playbox around grid\n";
    shortcuts += "  Space/Enter ..... Trigger playbox button\n";
    shortcuts += "  Esc ............. Stop All Clips (with fade)\n";
    shortcuts += "  Cmd/Ctrl+S ...... Save Session\n";
    shortcuts += "  Cmd/Ctrl+Shift+S  Save Session As\n";
    shortcuts += "  Cmd/Ctrl+, ...... Preferences (coming soon)\n";
    shortcuts += "  Cmd/Ctrl+Shift+[1-8] ... Switch to Tab 1-8\n";
    shortcuts += "  Q W E R T Y ..... Trigger clips (Row 0)\n";
    shortcuts += "  A S D F G H ..... Trigger clips (Row 1)\n";
    shortcuts += "  Z X C V B N ..... Trigger clips (Row 2)\n";
    shortcuts += "  1-6, 7-0, -=, [];',. Trigger clips (Rows 3-5)\n";
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
    shortcuts += "  Cmd/Ctrl+Opt+Shift+[1-9] Set IN fade (0.1s-0.9s)\n";
    shortcuts += "  Cmd/Ctrl+Opt+Shift+0 ... Set IN fade (1.0s)\n\n";
    shortcuts += "NOTE: Hold < > buttons in Edit Dialog for auto-repeat";

    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Keyboard Shortcuts",
                                           shortcuts, "OK");
    break;
  }

  case 401: // About Orpheus Clip Composer
  {
    auto* dialog = new AboutDialog();
    dialog->onOkClicked = [dialog]() {
      dialog->setVisible(false);
      delete dialog;
    };
    dialog->setCentrePosition(getWidth() / 2, getHeight() / 2);
    addAndMakeVisible(dialog);
    dialog->toFront(true);
    break;
  }

  default:
    break;
  }
}
