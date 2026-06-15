// SPDX-License-Identifier: MIT

#include "MainComponent.h"
#include "AppShell/AppCommandIds.h"
#include "BuildInfo.h"
#include "Core/ClipCommands.h"
#include "UI/ConsoleTheme.h"
#include "UI/DesignTokens.h"
#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <orpheus/app/ApplicationPaths.h>

#if JUCE_MAC
#include <mach/mach.h>
#endif

//==============================================================================
namespace {
constexpr int kButtonsPerTab = occ::ui::SessionUiSnapshot::kButtonsPerTab;

int getProcessMemoryMb() {
#if JUCE_MAC
  struct mach_task_basic_info info;
  mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &infoCount) ==
      KERN_SUCCESS) {
    return static_cast<int>(info.resident_size / (1024 * 1024));
  }
#endif
  return 0;
}

constexpr int kMenuCopyClip = 50;
constexpr int kMenuPasteClip = 51;
constexpr int kMenuSwapClip = 52;
constexpr int kMenuPasteSpecial = 53;
constexpr int kMenuPreviewClip = 54;
constexpr int kMenuRoutingInfo = 55;
constexpr int kMenuPlayNext = 56;

std::optional<ClipButtonAction> clipButtonActionForMenuItem(int result) {
  switch (result) {
  case kMenuCopyClip:
    return ClipButtonAction::Copy;
  case kMenuPasteClip:
    return ClipButtonAction::Paste;
  case kMenuSwapClip:
    return ClipButtonAction::Swap;
  case kMenuPasteSpecial:
    return ClipButtonAction::PasteSpecial;
  default:
    return std::nullopt;
  }
}

struct GridKeySpec {
  int keyCode = 0;
  juce::String label;
  bool shift = false;
  bool alt = false;
};

const std::array<std::array<GridKeySpec, 10>, 10>& gridKeyMap() {
  static const std::array<std::array<GridKeySpec, 10>, 10> map{{
      {{{'1', "1"},
        {'2', "2"},
        {'3', "3"},
        {'4', "4"},
        {'5', "5"},
        {'6', "6"},
        {'7', "7"},
        {'8', "8"},
        {'9', "9"},
        {'0', "0"}}},
      {{{'Q', "Q"},
        {'W', "W"},
        {'E', "E"},
        {'R', "R"},
        {'T', "T"},
        {'Y', "Y"},
        {'U', "U"},
        {'I', "I"},
        {'O', "O"},
        {'P', "P"}}},
      {{{'A', "A"},
        {'S', "S"},
        {'D', "D"},
        {'F', "F"},
        {'G', "G"},
        {'H', "H"},
        {'J', "J"},
        {'K', "K"},
        {'L', "L"},
        {';', ";"}}},
      {{{'Z', "Z"},
        {'X', "X"},
        {'C', "C"},
        {'V', "V"},
        {'B', "B"},
        {'N', "N"},
        {'M', "M"},
        {',', ","},
        {'.', "."},
        {'/', "/"}}},
      {{{juce::KeyPress::F1Key, "F1"},
        {juce::KeyPress::F2Key, "F2"},
        {juce::KeyPress::F3Key, "F3"},
        {juce::KeyPress::F4Key, "F4"},
        {juce::KeyPress::F5Key, "F5"},
        {juce::KeyPress::F6Key, "F6"},
        {juce::KeyPress::F7Key, "F7"},
        {juce::KeyPress::F8Key, "F8"},
        {juce::KeyPress::F9Key, "F9"},
        {juce::KeyPress::F10Key, "F10"}}},
      {{{'1', "S+1", true},
        {'2', "S+2", true},
        {'3', "S+3", true},
        {'4', "S+4", true},
        {'5', "S+5", true},
        {'6', "S+6", true},
        {'7', "S+7", true},
        {'8', "S+8", true},
        {'9', "S+9", true},
        {'0', "S+0", true}}},
      {{{'Q', "S+Q", true},
        {'W', "S+W", true},
        {'E', "S+E", true},
        {'R', "S+R", true},
        {'T', "S+T", true},
        {'Y', "S+Y", true},
        {'U', "S+U", true},
        {'I', "S+I", true},
        {'O', "S+O", true},
        {'P', "S+P", true}}},
      {{{'A', "S+A", true},
        {'S', "S+S", true},
        {'D', "S+D", true},
        {'F', "S+F", true},
        {'G', "S+G", true},
        {'H', "S+H", true},
        {'J', "S+J", true},
        {'K', "S+K", true},
        {'L', "S+L", true},
        {';', "S+;", true}}},
      {{{'Z', "S+Z", true},
        {'X', "S+X", true},
        {'C', "S+C", true},
        {'V', "S+V", true},
        {'B', "S+B", true},
        {'N', "S+N", true},
        {'M', "S+M", true},
        {',', "S+,", true},
        {'.', "S+.", true},
        {'/', "S+/", true}}},
      {{{'1', "O+1", false, true},
        {'2', "O+2", false, true},
        {'3', "O+3", false, true},
        {'4', "O+4", false, true},
        {'5', "O+5", false, true},
        {'6', "O+6", false, true},
        {'7', "O+7", false, true},
        {'8', "O+8", false, true},
        {'9', "O+9", false, true},
        {'0', "O+0", false, true}}},
  }};
  return map;
}
} // namespace

//==============================================================================
MainComponent::MainComponent() {
  // paint() always fillAlls the bounds with kBgPrimary at line 397, so promise
  // JUCE the chassis is opaque. Children that mark themselves opaque (inspector,
  // transport, tab strip) then short-circuit cleanly without forcing the
  // chassis to re-paint behind them.
  setOpaque(true);

  // Pre-size the bar visualizer feed buffer so refreshUiSnapshot can copy
  // into it without realloc each frame.
  m_barVisualizerLevelsBuffer.assign(4, 0.0f);

  // PFL availability reason is a static literal until the SDK and Preferences
  // expose channel-count and cue-routing respectively. Set it once here so
  // refreshUiSnapshot() doesn't rebuild this ~120-char juce::String 30 times
  // per second.
  // TODO(occ149c-pfl-channels): AudioDeviceStatus needs an output-channel
  //   count so we can detect ">2ch device" honestly.
  // TODO(occ149c-pfl-routing): Preferences → Audio needs a cue-bus
  //   channel-selection field, persisted via DisplayPreferences.
  m_uiSnapshot.audio.pfl.available = false;
  m_uiSnapshot.audio.pfl.unavailableReason =
      "Pre-fader-listen requires a multichannel audio interface and cue "
      "routing configuration (Preferences → Audio).";

  // Sprint 0: Ensure application directories exist
  orpheus::ApplicationPaths::ensureDirectoriesExist();

  // Initialize ServiceContext and register all services
  auto& ctx = orpheus::ServiceContext::getInstance();

  // Core Services
  m_audioEngine = std::make_shared<AudioEngine>();
  m_sessionManager = std::make_shared<SessionManager>();
  m_undoManager = std::make_shared<orpheus::UndoManager>();

  // Managers (OCC116/OCC117)
  m_displayPreferences = std::make_shared<orpheus::DisplayPreferences>();
  m_externalToolManager = std::make_shared<orpheus::ExternalToolManager>();
  m_hotKeyManager = std::make_shared<orpheus::HotKeyManager>();
  m_midiDeviceManager = std::make_shared<orpheus::MIDIDeviceManager>();

  // OCC144: Wire up DisplayPreferences callback to update UI when settings change
  m_displayPreferences->onPreferencesChanged = [this]() {
    applyDisplayPreferences();
    DBG("DisplayPreferences changed - UI updated");
  };

  // Initialize Database & Logging (Sprint 2)
  m_database = std::make_shared<orpheus::Database>();
  auto dbFile = orpheus::ApplicationPaths::getLogsDir().getChildFile("app.db");
  auto result = m_database->open(dbFile);

  if (result.failed()) {
    DBG("Failed to open database: " << result.getErrorMessage());
    // Fallback or fatal error handling? For now, we proceed without logging.
  } else {
    m_eventLogger = std::make_shared<orpheus::EventLogger>(*m_database);
    m_playoutLogger = std::make_shared<orpheus::PlayoutLogger>(*m_database);

    // Log startup
    m_eventLogger->log(orpheus::EventType::Startup, "MainComponent", "Application started");
  }

  // Register all services in ServiceContext for cross-component access
  ctx.registerService<AudioEngine>(m_audioEngine);
  ctx.registerService<SessionManager>(m_sessionManager);
  ctx.registerService<orpheus::UndoManager>(m_undoManager);
  ctx.registerService<orpheus::DisplayPreferences>(m_displayPreferences);
  ctx.registerService<orpheus::ExternalToolManager>(m_externalToolManager);
  ctx.registerService<orpheus::HotKeyManager>(m_hotKeyManager);
  ctx.registerService<orpheus::MIDIDeviceManager>(m_midiDeviceManager);
  ctx.registerService<orpheus::Database>(m_database);
  if (m_eventLogger)
    ctx.registerService<orpheus::EventLogger>(m_eventLogger);
  if (m_playoutLogger)
    ctx.registerService<orpheus::PlayoutLogger>(m_playoutLogger);

  // Set HK Grotesk font as default for all components
  setLookAndFeel(&m_hkGroteskLookAndFeel);

  // Create tab switcher (8 tabs for logical clip pages)
  m_tabSwitcher = std::make_unique<TabSwitcher>();
  addAndMakeVisible(m_tabSwitcher.get());

  // Wire up tab selection callback
  m_tabSwitcher->onTabSelected = [this](int tabIndex) { onTabSelected(tabIndex); };
  m_tabSwitcher->onOperatorViewModeSelected = [this](occ::ui::OperatorViewMode mode) {
    setOperatorViewMode(mode);
  };
  m_tabSwitcher->setOperatorViewMode(m_operatorViewMode);

  // Create clip grid (default 8 x 6 visible buttons per tab)
  m_clipGrid = std::make_unique<ClipGrid>();
  addAndMakeVisible(m_clipGrid.get());

  m_transportControls = std::make_unique<TransportControls>();
  addAndMakeVisible(m_transportControls.get());

  m_inspectorPanel = std::make_unique<ConsoleInspectorPanel>();
  m_inspectorPanel->setOperatorViewMode(m_operatorViewMode);
  // Inspector Playout footer — Stop All + Cue Buss are real juce::Buttons that
  // dispatch into the shared transport handlers.
  m_inspectorPanel->onStopAll = [this]() { onStopAll(); };
  m_inspectorPanel->onCueBuss = [this]() {
    // Cue Buss / PFL — feature-gated. Until pfl.available flips true (needs
    // a multichannel device + configured cue routing) the click is a no-op;
    // the button reflects that visually via setEnabled(false) so the operator
    // sees why the affordance is dimmed before they try.
    // TODO(occ149c-pfl-dispatch): once enabled, dispatch the currently-armed
    // clip(s) onto the cue bus without affecting the main output.
    if (!m_uiSnapshot.audio.pfl.available)
      return;
  };
  // Routing matrix mute/solo — toggle through the SDK routing matrix. The
  // inspector reads the committed state back from the snapshot the next poll,
  // so a refresh is all we owe the UI here.
  m_inspectorPanel->onMutePressed = [this](int group) {
    if (!m_audioEngine || group < 0 || group >= 4)
      return;
    const auto g = static_cast<uint8_t>(group);
    m_audioEngine->setGroupMute(g, !m_audioEngine->isGroupMuted(g));
  };
  m_inspectorPanel->onSoloPressed = [this](int group) {
    if (!m_audioEngine || group < 0 || group >= 4)
      return;
    const auto g = static_cast<uint8_t>(group);
    m_audioEngine->setGroupSolo(g, !m_audioEngine->isGroupSoloed(g));
  };
  addAndMakeVisible(m_inspectorPanel.get());

  // Create BarVisualizer (shmui VU meter - 4 bars for master level)
  m_barVisualizer = std::make_unique<shmui::BarVisualizer>();
  m_barVisualizer->setBarCount(4); // 4 bars showing master level (group routing TBD)
  m_barVisualizer->setBackgroundColour(juce::Colour(OCC::Design::kBgPrimary));
  m_barVisualizer->setHeightRange(5.0f, 100.0f); // Lower minimum for better dynamic range
  m_barVisualizer->setGradientMode(true);        // Enable VU meter gradient (green-yellow-red)
  // Note: Do NOT connect to AudioAnalyzer - we feed levels manually via setVolumeBands()
  addAndMakeVisible(m_barVisualizer.get());

  // Wire up ClipGrid callbacks (moved to helper method for readability)
  wireUpClipGridCallbacks();

  // Make this component capture keyboard focus
  setWantsKeyboardFocus(true);

  // Wire up transport control callbacks (moved to helper method for readability)
  wireUpTransportCallbacks();

  // Set window size (1400×900 for better screen fit)
  setSize(1400, 900);

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
  auto audioInitStart = std::chrono::steady_clock::now();
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
  auto audioInitEnd = std::chrono::steady_clock::now();
  m_audioEngineInitializationMs =
      std::chrono::duration<double, std::milli>(audioInitEnd - audioInitStart).count();

  // Wire MIDI device manager to audio engine and session manager (Plan Task 7)
  m_midiDeviceManager->setAudioEngine(m_audioEngine.get());
  m_midiDeviceManager->setSessionManager(m_sessionManager.get());
  m_midiDeviceManager->setCurrentTab(m_sessionManager->getActiveTab());

  // Start timer for VU meter updates (30Hz) and performance display (1Hz subset)
  startTimerHz(30);

  // OCC144: Apply initial display preferences to UI components
  applyDisplayPreferences();

  // TODO (Month 3-4): Create RoutingPanel component
  // TODO (Month 5-6): Create WaveformDisplay component
}

MainComponent::~MainComponent() {
  // Clear LookAndFeel before destruction
  setLookAndFeel(nullptr);

  // Shutdown ServiceContext (reverse-order cleanup of all registered services)
  orpheus::ServiceContext::getInstance().shutdown();
}

SessionHistoryWindow* MainComponent::getOrCreateSessionHistoryWindow() {
  if (!m_sessionHistoryWindow) {
    m_sessionHistoryWindow = std::make_unique<SessionHistoryWindow>();
    m_sessionHistoryWindow->setVisible(false);
  }

  return m_sessionHistoryWindow.get();
}

MIDIMonitorWindow* MainComponent::getOrCreateMidiMonitorWindow() {
  if (!m_midiMonitorWindow) {
    m_midiMonitorWindow = std::make_unique<MIDIMonitorWindow>(m_midiDeviceManager.get());
  }

  return m_midiMonitorWindow.get();
}

LevelMetersWindow* MainComponent::getOrCreateLevelMetersWindow() {
  if (!m_levelMetersWindow) {
    m_levelMetersWindow = std::make_unique<LevelMetersWindow>(m_audioEngine.get());
  }

  return m_levelMetersWindow.get();
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g) {
  g.fillAll(juce::Colour(OCC::Design::kBgPrimary));
}

void MainComponent::resized() {
  // Only grab keyboard focus if we're visible (avoid assertion during construction)
  if (isShowing())
    grabKeyboardFocus();
  auto bounds = getLocalBounds();

  const bool livePlayout = m_operatorViewMode == occ::ui::OperatorViewMode::Playout;
  const int topStripHeight = livePlayout ? OCC::Console::Metrics::kLiveTopStripHeight
                                         : OCC::Console::Metrics::kFullChromeHeight;
  const int bottomStripHeight = livePlayout ? OCC::Console::Metrics::kLiveBottomStripHeight
                                            : OCC::Console::Metrics::kFullBottomStripHeight;

  auto tabArea = bounds.removeFromTop(topStripHeight);
  if (m_tabSwitcher) {
    m_tabSwitcher->setBounds(tabArea); // Full width (no horizontal margin)
  }

  if (m_transportControls) {
    m_transportControls->setVisible(true);
    m_transportControls->setBounds(bounds.removeFromBottom(bottomStripHeight));
  }

  // Main content area
  auto contentArea = bounds;

  if (m_inspectorPanel) {
    m_inspectorPanel->setVisible(!livePlayout);
    if (!livePlayout) {
      m_inspectorPanel->setBounds(
          contentArea.removeFromRight(OCC::Console::Metrics::kInspectorWidth));
    }
  }

  // BarVisualizer stays in the authoring shell; live playout gives the grid priority.
  if (m_barVisualizer) {
    m_barVisualizer->setVisible(false);
    m_barVisualizer->setBounds({});
  }

  // Clip grid takes most of the space
  if (m_clipGrid) {
    m_clipGrid->setBounds(contentArea);
  }
}

//==============================================================================
void MainComponent::updateWindowTitle() {
  if (auto* topLevel = getTopLevelComponent()) {
    juce::String title = "Clip Composer";
    auto sessionLabel = getCurrentSessionLabel();
    if (sessionLabel.isNotEmpty())
      title += " - " + sessionLabel;
    if (m_sessionManager->isDirty())
      title += " *";
#ifdef DEBUG
    title += " [DEBUG]";
#endif
    title += " - v";
    title += occ::BuildInfo::version;
    title += " [";
    title += occ::BuildInfo::gitHash;
    title += "]";
    topLevel->setName(title);
  }
}

//==============================================================================
juce::String MainComponent::getCurrentSessionLabel() const {
  auto currentFile = m_sessionManager->getCurrentFile();
  if (currentFile != juce::File()) {
    return currentFile.getFileNameWithoutExtension();
  }

  auto sessionName = juce::String(m_sessionManager->getSessionName());
  return sessionName.isNotEmpty() ? sessionName : "Untitled";
}

//==============================================================================
bool MainComponent::saveSessionToFile(const juce::File& file) {
  auto targetFile = file;
  if (targetFile == juce::File()) {
    return false;
  }

  if (!targetFile.hasFileExtension(".json")) {
    targetFile = targetFile.withFileExtension(".json");
  }

  if (m_sessionManager->saveSession(targetFile)) {
    updateWindowTitle();
    return true;
  }

  juce::AlertWindow::showMessageBoxAsync(
      juce::AlertWindow::WarningIcon, "Save Failed",
      "Could not save session file:\n" + targetFile.getFullPathName(), "OK");
  return false;
}

//==============================================================================
bool MainComponent::saveCurrentSession() {
  auto currentFile = m_sessionManager->getCurrentFile();
  if (currentFile != juce::File()) {
    return saveSessionToFile(currentFile);
  }

  return saveCurrentSessionAs();
}

//==============================================================================
bool MainComponent::saveCurrentSessionAs() {
  juce::FileChooser chooser("Save Session As", orpheus::ApplicationPaths::getSessionsDir(),
                            "*.json");

  if (!chooser.browseForFileToSave(true)) {
    return false;
  }

  return saveSessionToFile(chooser.getResult());
}

//==============================================================================
bool MainComponent::loadSessionFromFile(const juce::File& file) {
  if (file == juce::File()) {
    return false;
  }

  if (m_audioEngine) {
    m_audioEngine->stopAllClips();
  }

  if (!m_sessionManager->loadSession(file)) {
    return false;
  }

  m_loopEnabled.fill(false);
  m_stopOthersOnPlay.fill(false);
  m_playNextEnabled.fill(false);
  m_pendingPlayNextGlobalIndex = -1;

  constexpr int numTabs = occ::NUM_TABS;
  auto activeTab = juce::jlimit(0, numTabs - 1, m_sessionManager->getActiveTab());
  for (int tabIndex = 0; tabIndex < numTabs; ++tabIndex) {
    m_sessionManager->setActiveTab(tabIndex);
    for (int buttonIndex = 0; buttonIndex < m_clipGrid->getButtonCount(); ++buttonIndex) {
      updateButtonFromClip(buttonIndex);
    }
  }

  onTabSelected(activeTab);
  updateWindowTitle();
  return true;
}

//==============================================================================
bool MainComponent::openSessionInteractive() {
  juce::FileChooser chooser("Open Session", orpheus::ApplicationPaths::getSessionsDir(), "*.json");

  if (!chooser.browseForFileToOpen()) {
    return false;
  }

  auto file = chooser.getResult();
  if (loadSessionFromFile(file)) {
    DBG("MainComponent: Successfully loaded session: " + file.getFileName());
    return true;
  }

  juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Load Failed",
                                         "Could not load session file:\n" + file.getFullPathName(),
                                         "OK");
  return false;
}

//==============================================================================
void MainComponent::createNewSession() {
  if (m_audioEngine) {
    m_audioEngine->stopAllClips();
  }

  m_sessionManager->clearSession();
  m_sessionManager->setActiveTab(0);
  m_loopEnabled.fill(false);
  m_stopOthersOnPlay.fill(false);
  m_playNextEnabled.fill(false);
  m_pendingPlayNextGlobalIndex = -1;

  if (m_tabSwitcher) {
    m_tabSwitcher->setActiveTab(0);
  }

  for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
    if (auto* button = m_clipGrid->getButton(i)) {
      button->clearClip();
    }
  }

  onTabSelected(0);
  updateWindowTitle();
}

//==============================================================================
void MainComponent::timerCallback() {
  refreshUiSnapshot();

  if (m_barVisualizer) {
    // OCC149c perf: reuse a member buffer instead of heap-allocating a
    // 4-float vector every 33ms. setVolumeBands still takes const std::vector&
    // (shmui surface), so we copy into the prepared buffer in place.
    std::copy(m_uiSnapshot.audio.groupLevels.begin(), m_uiSnapshot.audio.groupLevels.end(),
              m_barVisualizerLevelsBuffer.begin());
    m_barVisualizer->setVolumeBands(m_barVisualizerLevelsBuffer);
  }

  if (m_levelMetersWindow && m_levelMetersWindow->isVisible()) {
    m_levelMetersWindow->updateLevels(m_uiSnapshot.audio.groupLevels,
                                      m_uiSnapshot.audio.masterRmsLevel);
  }

  if (m_inspectorPanel) {
    m_inspectorPanel->setSnapshot(m_uiSnapshot);
  }
  if (m_transportControls) {
    m_transportControls->setTransportSnapshot(m_uiSnapshot);
  }

  // Static counter for 1Hz performance updates (every 30 timer ticks at 30Hz)
  static int performanceUpdateCounter = 0;
  performanceUpdateCounter++;

  // Only update performance display once per second (30 ticks at 30Hz)
  if (performanceUpdateCounter >= 30) {
    performanceUpdateCounter = 0;

    // OCC149c perf: sample process memory at 1Hz, not 30Hz. The syscall
    // (task_info on macOS) is not catastrophic but it's pointless overhead
    // when the displayed number rounds to MB and the inspector only renders
    // it at this same 1Hz cadence anyway. refreshUiSnapshot() reads this
    // cached value into m_uiSnapshot.audio.health.memoryMB.
    m_cachedProcessMemoryMB = getProcessMemoryMb();

    // Update window title (dirty flag, session name) at 1Hz
    updateWindowTitle();

    // OCC130 Sprint B: Update latency/performance info in merged TabSwitcher
    if (m_tabSwitcher && m_audioEngine) {
      uint32_t latencySamples = m_audioEngine->getLatencySamples();
      uint32_t sampleRate = m_audioEngine->getSampleRate();
      uint32_t bufferSize = m_audioEngine->getBufferSize();

      // Driver reports round-trip latency (input + output), but we want click-to-hear (output only)
      // So divide by 2 to get one-way latency
      double latencyMs = ((latencySamples / 2.0) / static_cast<double>(sampleRate)) * 1000.0;

      m_tabSwitcher->setLatencyInfo(latencyMs, bufferSize, sampleRate);
      m_tabSwitcher->setPerformanceInfo(m_uiSnapshot.audio.health.cpuPercent,
                                        m_uiSnapshot.audio.health.memoryMB);
      m_tabSwitcher->setHealthSnapshot(m_uiSnapshot.audio.health);
      m_tabSwitcher->setDeviceRouteStatus(m_uiSnapshot.audio.device);
      if (m_transportControls) {
        m_transportControls->setLatencyInfo(latencyMs, bufferSize, sampleRate);
        m_transportControls->setPerformanceInfo(m_uiSnapshot.audio.health.cpuPercent,
                                                m_uiSnapshot.audio.health.memoryMB);
        m_transportControls->setTransportSnapshot(m_uiSnapshot);
      }
    }

    // Auto-backup: save dirty sessions every 60 seconds (60 × 1Hz ticks)
    m_autoBackupCounter++;
    if (m_autoBackupCounter >= 60 && m_sessionManager->isDirty()) {
      m_autoBackupCounter = 0;

      // Generate timestamped backup filename
      auto now = juce::Time::getCurrentTime();
      auto backupDir = orpheus::ApplicationPaths::getBackupsDir();
      auto timestamp = now.formatted("%Y%m%d_%H%M%S");
      auto backupFile = backupDir.getChildFile("session_backup_" + timestamp + ".json");

      if (m_sessionManager->saveSession(backupFile)) {
        // Re-mark dirty since backup shouldn't clear dirty state
        m_sessionManager->markDirty();
        DBG("Auto-backup saved: " << backupFile.getFullPathName());

        // Prune old backups: keep only 5 most recent
        auto backupFiles =
            backupDir.findChildFiles(juce::File::findFiles, false, "session_backup_*.json");
        if (backupFiles.size() > 5) {
          // Sort by creation time (oldest first)
          backupFiles.sort();
          int toRemove = backupFiles.size() - 5;
          for (int i = 0; i < toRemove; ++i) {
            backupFiles[i].deleteFile();
            DBG("Auto-backup pruned: " << backupFiles[i].getFileName());
          }
        }
      }
    } else if (!m_sessionManager->isDirty()) {
      m_autoBackupCounter = 0; // Reset counter when session is clean
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

  // Apply button appearance preferences to ClipGrid
  if (m_clipGrid) {
    auto gridLayout = m_displayPreferences->getGridLayout();
    m_clipGrid->setGridSize(orpheus::DisplayPreferences::getGridLayoutColumns(gridLayout),
                            orpheus::DisplayPreferences::getGridLayoutRows(gridLayout));

    // Bevel width (None=0.0, 5%=0.05, 10%=0.10, 15%=0.15, 20%=0.20)
    float bevelPercent =
        orpheus::DisplayPreferences::getBevelWidthPercent(m_displayPreferences->getBevelWidth());
    m_clipGrid->setBevelWidthPercent(bevelPercent);

    // Button text mode (0=None, 1=HotKey, 2=MidiNote)
    int textMode = static_cast<int>(m_displayPreferences->getButtonTextMode());
    m_clipGrid->setButtonTextMode(textMode);

    const int activeTab = m_sessionManager ? m_sessionManager->getActiveTab() : 0;
    for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
      if (auto* button = m_clipGrid->getButton(i))
        button->setTabIndex(activeTab);
      updateButtonFromClip(i);
    }
  }

  // TODO: Remaining preferences to wire:
  // - buttonTriggerSize
  // - showButtonTriggers
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

  m_clipGrid->getClipSnapshot = [this](int buttonIndex, occ::ui::ClipUiSnapshot& snapshot) {
    if (buttonIndex >= 0 && buttonIndex < kButtonsPerTab) {
      snapshot = m_uiSnapshot.session.clips[buttonIndex];
      return true;
    }
    return false;
  };
}

void MainComponent::wireUpTransportCallbacks() {
  m_tabSwitcher->onStopAll = [this]() { onStopAll(); };
  m_tabSwitcher->onPanic = [this]() { onPanic(); };
  if (m_transportControls) {
    m_transportControls->onStopAll = [this]() { onStopAll(); };
    m_transportControls->onPanic = [this]() { onPanic(); };
    m_transportControls->onCue = [this]() {
      // Cue / PFL — feature-gated, mirrors the inspector Cue Buss handler.
      // TODO(occ149c-pfl-dispatch): once enabled, dispatch the armed clip(s)
      // to the cue bus without affecting the main output.
      if (!m_uiSnapshot.audio.pfl.available)
        return;
    };
  }
}

void MainComponent::handleClipStateChanged(int buttonIndex, orpheus::PlaybackState state) {
  // Get clip info for logging
  int tabIndex = buttonIndex / kButtonsPerTab;

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

  auto clipData = m_sessionManager->getClipByGlobalIndex(buttonIndex);
  if (clipData.isValid()) {
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

  if (state == orpheus::PlaybackState::Stopped && m_pendingPlayNextGlobalIndex >= 0 &&
      m_audioEngine) {
    bool anyPlaybackActive = false;
    for (int clipIndex = 0; clipIndex < occ::TOTAL_BUTTONS; ++clipIndex) {
      if (m_audioEngine->getClipState(clipIndex) == orpheus::PlaybackState::Playing ||
          m_audioEngine->getClipState(clipIndex) == orpheus::PlaybackState::Stopping) {
        anyPlaybackActive = true;
        break;
      }
    }

    if (!anyPlaybackActive) {
      const int nextGlobalIndex = m_pendingPlayNextGlobalIndex;
      m_pendingPlayNextGlobalIndex = -1;
      m_audioEngine->startClip(nextGlobalIndex);
      if ((nextGlobalIndex / kButtonsPerTab) == m_sessionManager->getActiveTab()) {
        if (auto* nextButton = m_clipGrid->getButton(nextGlobalIndex % kButtonsPerTab)) {
          nextButton->setState(ClipButton::State::Playing);
        }
      }
      DBG("MainComponent: Play Next fired for clip " << nextGlobalIndex);
    }
  }
}

void MainComponent::refreshUiSnapshot() {
  // OCC149c perf: do NOT reset the whole snapshot to `{}` — that destroys and
  // reallocates a fistful of juce::Strings (routing labels, device labels,
  // PFL reason text) every 33 ms. Instead, explicitly clear only the OR-true
  // accumulator field and let the per-frame writes below overwrite scalars.
  // Strings that change at < 1 Hz are gated on their inputs.
  m_uiSnapshot.activeViewMode = m_operatorViewMode;
  m_uiSnapshot.session.hasActivePlayback = false;

  if (!m_sessionManager)
    return;

  const int activeTab = m_sessionManager->getActiveTab();
  m_uiSnapshot.session.activeTab = activeTab;

  for (int buttonIndex = 0; buttonIndex < kButtonsPerTab; ++buttonIndex) {
    auto clipSnapshot = buildClipUiSnapshot(buttonIndex);
    m_uiSnapshot.session.clips[buttonIndex] = clipSnapshot;

    if (clipSnapshot.playbackState == orpheus::PlaybackState::Playing ||
        clipSnapshot.playbackState == orpheus::PlaybackState::Stopping) {
      m_uiSnapshot.session.hasActivePlayback = true;
    }
  }

  if (m_audioEngine) {
    const auto deviceStatus = m_audioEngine->getAudioDeviceStatus();
    const auto perfMetrics = m_audioEngine->getPerformanceMetrics();
    m_uiSnapshot.audio.masterRmsLevel = m_audioEngine->getMasterRmsLevel();
    m_audioEngine->getGroupLevels(m_uiSnapshot.audio.groupLevels);
    // OCC149c: per-group routing row data drives the Inspector's Routing view.
    for (uint8_t g = 0; g < 4; ++g) {
      auto& row = m_uiSnapshot.audio.groupRouting[g];
      row.outputLabel = m_audioEngine->getGroupOutputLabel(g);
      row.gainDb = m_audioEngine->getGroupGainDb(g);
      row.muted = m_audioEngine->isGroupMuted(g);
      row.soloed = m_audioEngine->isGroupSoloed(g);
    }
    // Device identity + route labels rebuild only when the device name actually
    // changes. Re-concatenating "Playout: Groups 1-4 via …" every 33 ms
    // accomplishes nothing but heap churn when nothing has changed.
    const juce::String deviceName = juce::String(deviceStatus.activeDeviceName);
    if (deviceName != m_cachedDeviceNameForLabels) {
      m_cachedDeviceNameForLabels = deviceName;
      m_uiSnapshot.audio.activeDeviceIdentifier = deviceName;
      m_uiSnapshot.audio.device.activeDeviceIdentifier = deviceName;
      m_uiSnapshot.audio.device.playoutRouteLabel = "Playout: Groups 1-4 via " + deviceName;
      m_uiSnapshot.audio.audition.routeLabel = "Audition: cue buss via " + deviceName;
    }
    m_uiSnapshot.audio.device.initialized = deviceStatus.initialized;
    m_uiSnapshot.audio.device.running = deviceStatus.running;
    m_uiSnapshot.audio.device.usingFallbackDriver = deviceStatus.usingFallbackDriver;
    m_uiSnapshot.audio.device.routesAvailable = deviceStatus.initialized;
    m_uiSnapshot.audio.device.deviceSummary = juce::String(deviceStatus.summary);
    m_uiSnapshot.audio.audition.usesDedicatedBus = true;
    m_uiSnapshot.audio.audition.validationMessage =
        deviceStatus.lastError.empty() ? juce::String() : juce::String(deviceStatus.lastError);

    // PFL availability + reason are set once in the constructor — the feature
    // is gated off until SDK + Preferences surface exists, so there's nothing
    // to update here. See ctor for the TODOs.

    m_uiSnapshot.audio.health.cpuPercent = perfMetrics.cpuUsagePercent;
    // getProcessMemoryMb() syscall is gated to 1Hz inside timerCallback();
    // reuse the last sampled value here so the inspector keeps reading a
    // stable MB number across the intervening frames.
    m_uiSnapshot.audio.health.memoryMB = m_cachedProcessMemoryMB;
    m_uiSnapshot.audio.health.bufferSize = static_cast<int>(deviceStatus.bufferSize);
    m_uiSnapshot.audio.health.sampleRate = static_cast<int>(deviceStatus.sampleRate);
    m_uiSnapshot.audio.health.dropoutCount = static_cast<int>(perfMetrics.bufferUnderrunCount);
    m_uiSnapshot.audio.health.statusText = juce::String(deviceStatus.summary);
  }
}

occ::ui::ClipUiSnapshot MainComponent::buildClipUiSnapshot(int buttonIndex) const {
  occ::ui::ClipUiSnapshot snapshot;
  snapshot.tabIndex = m_sessionManager ? m_sessionManager->getActiveTab() : 0;
  snapshot.buttonIndex = buttonIndex;
  snapshot.globalClipIndex = (snapshot.tabIndex * kButtonsPerTab) + buttonIndex;

  if (!m_sessionManager || buttonIndex < 0 || buttonIndex >= kButtonsPerTab)
    return snapshot;

  const auto clipData = m_sessionManager->getClipByGlobalIndex(snapshot.globalClipIndex);
  snapshot.hasClip = clipData.isValid();
  if (!snapshot.hasClip)
    return snapshot;

  snapshot.loopEnabled = m_loopEnabled[snapshot.globalClipIndex];
  snapshot.fadeInEnabled = clipData.fadeInSeconds > 0.0;
  snapshot.fadeOutEnabled = clipData.fadeOutSeconds > 0.0;
  snapshot.stopOthersEnabled = m_stopOthersOnPlay[snapshot.globalClipIndex];
  snapshot.displayName = juce::String(clipData.displayName);
  snapshot.color = clipData.color;
  snapshot.clipGroup = clipData.clipGroup;
  snapshot.playbackProgress = calculateClipProgress(clipData, snapshot.globalClipIndex);

  if (m_audioEngine) {
    snapshot.playbackState = m_audioEngine->getClipState(snapshot.globalClipIndex);
  }

  return snapshot;
}

float MainComponent::calculateClipProgress(const SessionManager::ClipData& clipData,
                                           int globalClipIndex) const {
  if (!m_audioEngine || !clipData.isValid())
    return 0.0f;

  const int64_t currentSample = m_audioEngine->getClipPosition(globalClipIndex);
  const int64_t effectiveTrimOut =
      (clipData.trimOutSamples > 0) ? clipData.trimOutSamples : clipData.durationSamples;
  const int64_t trimmedSamples = effectiveTrimOut - clipData.trimInSamples;
  if (trimmedSamples <= 0)
    return 0.0f;

  const float progress = static_cast<float>(currentSample - clipData.trimInSamples) /
                         static_cast<float>(trimmedSamples);
  return juce::jlimit(0.0f, 1.0f, progress);
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
      handleMenuItemSelected(100, 1); // Trigger Undo menu item
      return true;
    }

    if (key == juce::KeyPress('n', juce::ModifierKeys::commandModifier, 0)) {
      if (m_appCommandHandler) {
        m_appCommandHandler(occ::AppCommandIds::newSession);
      } else {
        handleMenuItemSelected(1, 0);
      }
      return true;
    }

    if (key == juce::KeyPress('o', juce::ModifierKeys::commandModifier, 0)) {
      if (m_appCommandHandler) {
        m_appCommandHandler(occ::AppCommandIds::openSession);
      } else {
        handleMenuItemSelected(2, 0);
      }
      return true;
    }

    if (key == juce::KeyPress('s', juce::ModifierKeys::commandModifier, 0)) {
      if (m_appCommandHandler) {
        m_appCommandHandler(occ::AppCommandIds::saveSession);
      } else {
        handleMenuItemSelected(3, 0);
      }
      return true;
    }

    if (key == juce::KeyPress(',', juce::ModifierKeys::commandModifier, 0)) {
      if (m_appCommandHandler) {
        m_appCommandHandler(occ::AppCommandIds::showAudioSettings);
      } else {
        showAudioSettings();
      }
      return true;
    }

    if (key == juce::KeyPress('1', juce::ModifierKeys::commandModifier, 0)) {
      setOperatorViewMode(occ::ui::OperatorViewMode::Playout);
      return true;
    }
    if (key == juce::KeyPress('2', juce::ModifierKeys::commandModifier, 0)) {
      setOperatorViewMode(occ::ui::OperatorViewMode::Edit);
      return true;
    }
    if (key == juce::KeyPress('3', juce::ModifierKeys::commandModifier, 0)) {
      setOperatorViewMode(occ::ui::OperatorViewMode::Routing);
      return true;
    }
    if (key == juce::KeyPress('4', juce::ModifierKeys::commandModifier, 0)) {
      setOperatorViewMode(occ::ui::OperatorViewMode::Preferences);
      return true;
    }

    // Item 24: Clip Copy/Paste functionality
    // Cmd+C = Copy clip at playbox position
    if (key == juce::KeyPress('c', juce::ModifierKeys::commandModifier, 0)) {
      int playboxIndex = m_clipGrid->getPlayboxIndex();
      if (m_sessionManager->hasClip(playboxIndex)) {
        m_clipboardData = m_sessionManager->getClip(playboxIndex);
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
        if (m_sessionManager->hasClip(playboxIndex)) {
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
        if (m_sessionManager->hasClip(playboxIndex)) {
          // Copy all metadata
          auto clipData = m_sessionManager->getClip(playboxIndex);
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
          m_sessionManager->setClip(playboxIndex, clipData);

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
      if (m_appCommandHandler) {
        m_appCommandHandler(occ::AppCommandIds::saveSessionAs);
      } else {
        handleMenuItemSelected(4, 0);
      }
      return true;
    }
    // Cmd+Shift+Z = Redo
    if (key ==
        juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier,
                       0)) {
      handleMenuItemSelected(101, 1); // Trigger Redo menu item
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

  // OCC144: Check custom hotkey assignments before keyboard grid
  if (m_hotKeyManager) {
    bool triggered = m_hotKeyManager->triggerHotKey(key, m_sessionManager->getActiveTab(),
                                                    m_sessionManager.get(), m_audioEngine.get());
    if (triggered)
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
  if (!m_clipGrid)
    return -1;

  const int columns = m_clipGrid->getColumns();
  const int rows = m_clipGrid->getRows();
  const int keyCode = key.getKeyCode();
  const bool shiftDown = key.getModifiers().isShiftDown();
  const bool altDown = key.getModifiers().isAltDown();
  const auto& map = gridKeyMap();

  for (int row = 0; row < rows && row < static_cast<int>(map.size()); ++row) {
    for (int col = 0; col < columns && col < static_cast<int>(map[row].size()); ++col) {
      const auto& spec = map[row][col];
      if (spec.keyCode == keyCode && spec.shift == shiftDown && spec.alt == altDown) {
        return row * columns + col;
      }
    }
  }

  return -1;
}

juce::String MainComponent::getKeyboardShortcutForButton(int buttonIndex) const {
  if (!m_clipGrid || buttonIndex < 0 || buttonIndex >= m_clipGrid->getButtonCount())
    return "";

  const int columns = m_clipGrid->getColumns();
  const int row = buttonIndex / columns;
  const int col = buttonIndex % columns;
  const auto& map = gridKeyMap();

  if (row >= 0 && row < static_cast<int>(map.size()) && col >= 0 &&
      col < static_cast<int>(map[row].size())) {
    return map[row][col].label;
  }

  return "";
}

//==============================================================================
void MainComponent::onClipRightClicked(int buttonIndex) {
  // Show context menu (inherits HK Grotesk font from LookAndFeel)
  juce::PopupMenu menu;

  bool hasClip = m_sessionManager->hasClip(buttonIndex);
  int globalClipIndex = getGlobalClipIndex(buttonIndex);

  if (hasClip) {
    // Clip is loaded - show options
    auto clipData = m_sessionManager->getClip(buttonIndex);

    // Edit Clip at the top (most important action)
    menu.addItem(5, "Edit Clip...");
    menu.addSeparator();

    menu.addItem(kMenuCopyClip, "Copy Clip");
    menu.addItem(kMenuPasteClip, "Paste Clipboard", m_hasClipInClipboard);
    menu.addItem(kMenuSwapClip, "Swap With Clipboard", m_hasClipInClipboard);
    menu.addItem(kMenuPasteSpecial, "Paste Special...", m_hasClipInClipboard);
    menu.addSeparator();
    menu.addItem(kMenuPreviewClip, "Preview / Audition");
    menu.addItem(kMenuRoutingInfo, "Routing / Output Info");
    menu.addItem(kMenuPlayNext, "Play Next", true, m_playNextEnabled[globalClipIndex]);
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
    menu.addSeparator();
    menu.addItem(kMenuPasteClip, "Paste Clipboard", m_hasClipInClipboard);
    menu.addItem(kMenuPasteSpecial, "Paste Special...", m_hasClipInClipboard);
  }

  // Ensure menu uses HK Grotesk aesthetic (inherited from MainComponent's LookAndFeel)
  menu.setLookAndFeel(&m_hkGroteskLookAndFeel);

  menu.showMenuAsync(juce::PopupMenu::Options(), [this, buttonIndex, hasClip,
                                                  globalClipIndex](int result) {
    if (auto action = clipButtonActionForMenuItem(result)) {
      handleClipButtonAction(buttonIndex, *action);
      return;
    }

    if (result == kMenuPreviewClip && hasClip) {
      setOperatorViewMode(occ::ui::OperatorViewMode::Edit);
      onClipDoubleClicked(buttonIndex);
      return;
    }

    if (result == kMenuRoutingInfo && hasClip) {
      showRoutingInfoForClip(globalClipIndex);
      setOperatorViewMode(occ::ui::OperatorViewMode::Routing);
      return;
    }

    if (result == kMenuPlayNext && hasClip) {
      m_playNextEnabled[globalClipIndex] = !m_playNextEnabled[globalClipIndex];
      return;
    }

    if (result == 5 && hasClip) {
      // Edit Clip - open edit dialog
      onClipDoubleClicked(buttonIndex);
    } else if (result == 1) {
      // Load audio file
      // Item 9: Warning if replacing existing clip
      bool shouldLoad = true;
      if (hasClip) {
        auto clipData = m_sessionManager->getClip(buttonIndex);
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
      auto clipData = m_sessionManager->getClip(buttonIndex);
      bool confirmed = juce::AlertWindow::showOkCancelBox(
          juce::AlertWindow::WarningIcon, "Remove Clip?",
          "Remove \"" + juce::String(clipData.displayName) + "\" from button " +
              juce::String(buttonIndex + 1) + "?",
          "Remove", "Cancel");

      if (confirmed) {
        auto cmd = std::make_unique<orpheus::ClearClipCommand>(
            m_sessionManager.get(), m_sessionManager->getActiveTab(), buttonIndex);
        m_undoManager->executeCommand(std::move(cmd));
        updateButtonFromClip(buttonIndex);
        DBG("MainComponent: Removed clip from button " << buttonIndex);
      }
    } else if (result == 4) {
      // Toggle "stop others on play" mode
      m_stopOthersOnPlay[globalClipIndex] = !m_stopOthersOnPlay[globalClipIndex];

      // CRITICAL: Persist to SessionManager (via UndoManager)
      if (m_sessionManager->hasClip(buttonIndex)) {
        auto oldData = m_sessionManager->getClip(buttonIndex);
        auto newData = oldData;
        newData.stopOthersEnabled = m_stopOthersOnPlay[globalClipIndex];
        auto cmd = std::make_unique<orpheus::EditClipCommand>(m_sessionManager.get(),
                                                              m_sessionManager->getActiveTab(),
                                                              buttonIndex, oldData, newData);
        m_undoManager->executeCommand(std::move(cmd));
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

      // CRITICAL: Persist to SessionManager (via UndoManager)
      auto oldData = m_sessionManager->getClip(buttonIndex);
      auto newData = oldData;
      newData.loopEnabled = m_loopEnabled[globalClipIndex];
      auto cmd = std::make_unique<orpheus::EditClipCommand>(
          m_sessionManager.get(), m_sessionManager->getActiveTab(), buttonIndex, oldData, newData);
      m_undoManager->executeCommand(std::move(cmd));

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
      int totalButtons = kButtonsPerTab;
      int filesToLoad = 0;

      juce::FileChooser chooser("Select Audio Files",
                                juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                                "*.wav;*.aiff;*.aif;*.flac");

      if (chooser.browseForMultipleFilesToOpen()) {
        auto files = chooser.getResults();
        filesToLoad = files.size();

        // Check how many existing clips would be overwritten
        for (int i = buttonIndex; i < juce::jmin(buttonIndex + filesToLoad, totalButtons); ++i) {
          if (m_sessionManager->hasClip(i)) {
            overwriteCount++;
          }
        }

        if (overwriteCount > 0) {
          juce::String message = "Loading " + juce::String(filesToLoad) +
                                 " files starting at button " + juce::String(buttonIndex + 1) +
                                 " will overwrite " + juce::String(overwriteCount) +
                                 " existing clip" + (overwriteCount > 1 ? "s" : "") + ".\n\n" +
                                 "Do you want to continue?";

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
      auto clipData = m_sessionManager->getClip(buttonIndex);
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
      auto clipData = m_sessionManager->getClip(buttonIndex);
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
      if (m_sessionManager->hasClip(buttonIndex)) {
        auto clipData = m_sessionManager->getClip(buttonIndex);

        auto button = m_clipGrid->getButton(buttonIndex);
        if (button) {
          // Center popup OVER the button (not below it)
          auto buttonBounds = button->getScreenBounds();
          int popupWidth = 12 * 28 + 11 * 6 + 16; // 12 cols, 2 rows, padding
          int popupHeight = 2 * 28 + 6 + 16;
          int popupX = buttonBounds.getCentreX() - (popupWidth / 2);
          int popupY = buttonBounds.getCentreY() - (popupHeight / 2); // Hover over button
          juce::Rectangle<int> popupBounds(popupX, popupY, popupWidth, popupHeight);

          ColorSwatchPicker::showPopupAt(
              popupBounds, clipData.color, [this, buttonIndex](const juce::Colour& newColor) {
                // Update button color
                auto button = m_clipGrid->getButton(buttonIndex);
                if (button) {
                  button->setClipColor(newColor);
                }

                // CRITICAL: Persist color to SessionManager (via UndoManager)
                if (m_sessionManager->hasClip(buttonIndex)) {
                  auto oldData = m_sessionManager->getClip(buttonIndex);
                  auto newData = oldData;
                  newData.color = newColor;
                  auto cmd = std::make_unique<orpheus::EditClipCommand>(
                      m_sessionManager.get(), m_sessionManager->getActiveTab(), buttonIndex,
                      oldData, newData);
                  m_undoManager->executeCommand(std::move(cmd));
                }

                DBG("Button " << buttonIndex << ": Color changed to " << newColor.toString());
              });
        }
      }
    }
  });
}

void MainComponent::onClipTriggered(int buttonIndex) {
  auto button = m_clipGrid->getButton(buttonIndex);
  if (!button)
    return;

  // Check if clip is loaded
  if (!m_sessionManager->hasClip(buttonIndex)) {
    DBG("MainComponent: Button " + juce::String(buttonIndex) + " has no clip loaded");
    return;
  }

  // Item 60: Move playbox to the triggered button (follows last clip launched)
  m_clipGrid->setPlayboxIndex(buttonIndex);

  // Calculate global clip index (tab-aware logical grid)
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
    if (m_playNextEnabled[globalClipIndex] && m_audioEngine) {
      bool anotherClipActive = false;
      for (int clipIndex = 0; clipIndex < occ::TOTAL_BUTTONS; ++clipIndex) {
        if (clipIndex == globalClipIndex)
          continue;
        if (m_audioEngine->getClipState(clipIndex) == orpheus::PlaybackState::Playing ||
            m_audioEngine->getClipState(clipIndex) == orpheus::PlaybackState::Stopping) {
          anotherClipActive = true;
          break;
        }
      }

      if (anotherClipActive) {
        m_pendingPlayNextGlobalIndex = globalClipIndex;
        DBG("MainComponent: Queued Play Next for clip " << globalClipIndex);
        return;
      }
    }

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
  if (!m_sessionManager->hasClip(buttonIndex)) {
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
  auto clipData = m_sessionManager->getClip(buttonIndex);

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
    auto clipData = m_sessionManager->getClip(buttonIndex);
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

    // Persist to SessionManager (via UndoManager)
    auto oldClipData = m_sessionManager->getClip(buttonIndex);
    auto editCmd = std::make_unique<orpheus::EditClipCommand>(m_sessionManager.get(),
                                                              m_sessionManager->getActiveTab(),
                                                              buttonIndex, oldClipData, clipData);
    m_undoManager->executeCommand(std::move(editCmd));

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

    refreshUiSnapshot();

    // Close dialog and clear reference
    dialog->setVisible(false);
    delete dialog;
    m_currentEditDialog = nullptr; // Clear reference to allow new dialog
  };

  dialog->onCancelClicked = [this, buttonIndex, globalClipIndex, dialog, metadata]() {
    // CRITICAL: Restore original metadata on CANCEL (discard temporary edits)
    // Edits are live during preview, but must be reverted if user cancels

    // Restore SessionManager clip data
    auto clipData = m_sessionManager->getClip(buttonIndex);
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
    m_sessionManager->setClip(buttonIndex, clipData);

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

    refreshUiSnapshot();

    // Close dialog
    dialog->setVisible(false);
    delete dialog;
    m_currentEditDialog = nullptr; // Clear reference to allow new dialog
  };

  // OCC149c: Action Triad REPLACE FILE — keep replacement on the message thread
  // and close the dialog after a successful swap so the operator never edits a
  // stale waveform/metadata view. AudioEngine loading still uses the same
  // pre-buffered clip slot path as drag/drop, so the audio callback remains
  // allocation-free.
  dialog->onReplaceFileClicked = [this, buttonIndex, dialog]() {
    auto safeDialog = juce::Component::SafePointer<ClipEditDialog>(dialog);
    auto chooser = std::make_shared<juce::FileChooser>("Replace clip audio file", juce::File{},
                                                       "*.wav;*.aiff;*.aif;*.flac;*.mp3");

    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectFiles,
                         [this, buttonIndex, safeDialog, chooser](const juce::FileChooser& fc) {
                           (void)chooser; // captured to keep the async chooser alive
                           const auto file = fc.getResult();
                           if (!file.existsAsFile())
                             return;

                           loadClipToButton(buttonIndex, file.getFullPathName());
                           refreshUiSnapshot();

                           if (auto* activeDialog = safeDialog.getComponent()) {
                             activeDialog->setVisible(false);
                             delete activeDialog;
                             if (m_currentEditDialog == activeDialog)
                               m_currentEditDialog = nullptr;
                           }
                         });
  };

  // OCC149c: Action Triad CLEAR — the Edit Dialog's destructive escape hatch.
  // Mirrors the right-click "Remove Clip?" flow so undo history stays
  // consistent regardless of how the operator initiated the clear. Confirmation
  // is mandatory: this is the only Danger-variant action in the dialog and the
  // operator may have just opened the dialog to make changes, not to nuke it.
  dialog->onClearClicked = [this, buttonIndex, globalClipIndex, dialog]() {
    if (!m_sessionManager->hasClip(buttonIndex))
      return;
    const auto clipData = m_sessionManager->getClip(buttonIndex);
    const bool confirmed = juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::WarningIcon, "Clear Clip?",
        "Clear \"" + juce::String(clipData.displayName) + "\" from button " +
            juce::String(buttonIndex + 1) + "? This can be undone.",
        "Clear", "Cancel");
    if (!confirmed)
      return;

    auto cmd = std::make_unique<orpheus::ClearClipCommand>(
        m_sessionManager.get(), m_sessionManager->getActiveTab(), buttonIndex);
    m_undoManager->executeCommand(std::move(cmd));
    updateButtonFromClip(buttonIndex);

    // Reset per-clip UI flags so the freshly empty slot doesn't carry the old
    // clip's loop/stop-others state into the next file assignment.
    m_loopEnabled[globalClipIndex] = false;
    m_stopOthersOnPlay[globalClipIndex] = false;

    refreshUiSnapshot();

    dialog->setVisible(false);
    delete dialog;
    m_currentEditDialog = nullptr;
  };

  // Real-time color update: Repaint button immediately when color changes (75fps)
  dialog->onColorChanged = [this, buttonIndex](const juce::Colour& newColor) {
    auto button = m_clipGrid->getButton(buttonIndex);
    if (button) {
      button->setClipColor(newColor); // Triggers immediate repaint (75fps grid refresh)
    }

    // CRITICAL: Persist color to SessionManager (prevents Edit Dialog from overwriting it)
    if (m_sessionManager->hasClip(buttonIndex)) {
      auto clipData = m_sessionManager->getClip(buttonIndex);
      clipData.color = newColor;
      m_sessionManager->setClip(buttonIndex, clipData);
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
  bool success = m_sessionManager->loadClip(buttonIndex, finalPath);

  if (success) {
    // Calculate global clip index for multi-tab isolation
    int globalClipIndex = getGlobalClipIndex(buttonIndex);

    // Load audio file into AudioEngine for playback (use global index)
    if (m_audioEngine) {
      bool audioLoaded = m_audioEngine->loadClip(globalClipIndex, finalPath);
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

  // Swap clips in SessionManager (via UndoManager)
  auto swapCmd = std::make_unique<orpheus::SwapClipsCommand>(m_sessionManager.get(),
                                                             getGlobalClipIndex(sourceButtonIndex),
                                                             getGlobalClipIndex(targetButtonIndex));
  m_undoManager->executeCommand(std::move(swapCmd));

  // Swap stop-others mode flags (use global indices)
  std::swap(m_stopOthersOnPlay[sourceGlobalIndex], m_stopOthersOnPlay[targetGlobalIndex]);

  // Swap loop mode flags (use global indices)
  std::swap(m_loopEnabled[sourceGlobalIndex], m_loopEnabled[targetGlobalIndex]);

  // Update both buttons visually (this reloads clips into AudioEngine at new positions)
  updateButtonFromClip(sourceButtonIndex);
  updateButtonFromClip(targetButtonIndex);

  // Restart clips at their NEW positions if they were playing
  if (sourceWasPlaying && m_audioEngine && m_sessionManager->hasClip(targetButtonIndex)) {
    m_audioEngine->startClip(targetGlobalIndex);
    if (targetButton) {
      targetButton->setState(ClipButton::State::Playing);
    }
    DBG("MainComponent: Restarted source clip at new position (button "
        << targetButtonIndex << ", global: " << targetGlobalIndex << ")");
  }
  if (targetWasPlaying && m_audioEngine && m_sessionManager->hasClip(sourceButtonIndex)) {
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

  if (m_sessionManager->hasClip(buttonIndex)) {
    // Calculate global clip index for multi-tab isolation
    int globalClipIndex = getGlobalClipIndex(buttonIndex);

    // Get real clip metadata from SessionManager
    auto clipData = m_sessionManager->getClip(buttonIndex);

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
    m_playNextEnabled[getGlobalClipIndex(buttonIndex)] = false;
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
  m_sessionManager->setActiveTab(tabIndex);

  // Update MIDI device manager's current tab for Paged scope (Plan Task 7)
  if (m_midiDeviceManager)
    m_midiDeviceManager->setCurrentTab(tabIndex);

  // Feature 4: Update tab index on all buttons for consecutive numbering
  // Tab 1 = clips 1-100, Tab 2 = clips 101-200, etc.
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

  refreshUiSnapshot();
  repaint();
}

void MainComponent::setOperatorViewMode(occ::ui::OperatorViewMode mode) {
  if (m_operatorViewMode == mode)
    return;

  m_operatorViewMode = mode;

  if (m_tabSwitcher)
    m_tabSwitcher->setOperatorViewMode(mode);
  if (m_inspectorPanel)
    m_inspectorPanel->setOperatorViewMode(mode);

  DBG("MainComponent: Operator view mode set to " << static_cast<int>(mode));
  resized();
  repaint();
}

void MainComponent::showRoutingInfoForClip(int globalClipIndex) const {
  if (!m_sessionManager || !m_audioEngine)
    return;

  const auto clipData = m_sessionManager->getClipByGlobalIndex(globalClipIndex);
  const auto deviceStatus = m_audioEngine->getAudioDeviceStatus();
  juce::String message;
  message << "Clip: " << juce::String(clipData.displayName) << "\n";
  message << "Group Output: G" << juce::String(clipData.clipGroup + 1) << "\n";
  message << "Playout Path: Groups 1-4 via " << juce::String(deviceStatus.activeDeviceName) << "\n";
  message << "Audition Path: Dedicated cue buss via " << juce::String(deviceStatus.activeDeviceName)
          << "\n";
  if (!deviceStatus.lastError.empty()) {
    message << "Validation: " << juce::String(deviceStatus.lastError) << "\n";
  } else {
    message << "Validation: Routing available\n";
  }

  juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Routing / Output Info",
                                         message, "OK");
}

void MainComponent::handleClipButtonAction(int buttonIndex, ClipButtonAction action) {
  if (buttonIndex < 0 || buttonIndex >= m_clipGrid->getButtonCount())
    return;

  auto clipExists = m_sessionManager->hasClip(buttonIndex);

  switch (action) {
  case ClipButtonAction::Copy:
    if (clipExists) {
      m_clipboardData = m_sessionManager->getClip(buttonIndex);
      m_hasClipInClipboard = true;
      for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
        updateButtonFromClip(i);
      }
      DBG("MainComponent: Copied clip from button " << buttonIndex << " - "
                                                    << m_clipboardData.displayName);
    }
    break;

  case ClipButtonAction::Paste:
    if (m_hasClipInClipboard) {
      auto oldData = m_sessionManager->hasClip(buttonIndex) ? m_sessionManager->getClip(buttonIndex)
                                                            : SessionManager::ClipData{};
      auto newData = m_clipboardData;
      auto cmd = std::make_unique<orpheus::EditClipCommand>(
          m_sessionManager.get(), m_sessionManager->getActiveTab(), buttonIndex, oldData, newData);
      m_undoManager->executeCommand(std::move(cmd));
      updateButtonFromClip(buttonIndex);
      DBG("MainComponent: Pasted clipboard clip to button " << buttonIndex);
    }
    break;

  case ClipButtonAction::Swap:
    if (m_hasClipInClipboard && clipExists) {
      auto oldData = m_sessionManager->getClip(buttonIndex);
      auto newData = m_clipboardData;
      auto cmd = std::make_unique<orpheus::EditClipCommand>(
          m_sessionManager.get(), m_sessionManager->getActiveTab(), buttonIndex, oldData, newData);
      m_undoManager->executeCommand(std::move(cmd));
      m_clipboardData = oldData;
      updateButtonFromClip(buttonIndex);
      DBG("MainComponent: Swapped clipboard clip with button " << buttonIndex);
    }
    break;

  case ClipButtonAction::PasteSpecial:
    handleMenuItemSelected(102, 1);
    break;

  default:
    break;
  }
}

//==============================================================================
// Menu Bar Implementation
juce::StringArray MainComponent::getAppMenuBarNames() {
  return {"File", "Edit", "Session", "Setup", "Display", "Audio", "Help"};
}

juce::PopupMenu MainComponent::getAppMenuForIndex(int topLevelMenuIndex,
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
    menu.addItem(15, "Toggle Session History Window", true,
                 m_sessionHistoryWindow != nullptr && m_sessionHistoryWindow->isVisible());
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
    auto gridLayout = m_displayPreferences->getGridLayout();
    auto addGridItem = [&gridLayout](juce::PopupMenu& gridMenu, int itemId,
                                     const juce::String& label,
                                     orpheus::DisplayPreferences::GridLayout layout) {
      gridMenu.addItem(itemId, label, true, gridLayout == layout);
    };

    juce::PopupMenu gridMenu;
    addGridItem(gridMenu, 350, "6 x 6", orpheus::DisplayPreferences::GridLayout::Columns6Rows6);
    addGridItem(gridMenu, 351, "8 x 6", orpheus::DisplayPreferences::GridLayout::Columns8Rows6);
    addGridItem(gridMenu, 352, "10 x 6", orpheus::DisplayPreferences::GridLayout::Columns10Rows6);
    addGridItem(gridMenu, 353, "6 x 8", orpheus::DisplayPreferences::GridLayout::Columns6Rows8);
    addGridItem(gridMenu, 354, "8 x 8", orpheus::DisplayPreferences::GridLayout::Columns8Rows8);
    addGridItem(gridMenu, 355, "10 x 8", orpheus::DisplayPreferences::GridLayout::Columns10Rows8);
    addGridItem(gridMenu, 356, "12 x 8 (Live Dense)",
                orpheus::DisplayPreferences::GridLayout::Columns12Rows8);
    addGridItem(gridMenu, 357, "6 x 10", orpheus::DisplayPreferences::GridLayout::Columns6Rows10);
    addGridItem(gridMenu, 358, "8 x 10", orpheus::DisplayPreferences::GridLayout::Columns8Rows10);
    addGridItem(gridMenu, 359, "10 x 10", orpheus::DisplayPreferences::GridLayout::Columns10Rows10);
    menu.addSubMenu("Clip Grid Layout", gridMenu);
    menu.addSeparator();

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

void MainComponent::showAudioSettings() {
  auto* dialog = new AudioSettingsDialog(m_audioEngine.get());
  dialog->onCloseClicked = [this, dialog]() {
    dialog->setVisible(false);
    delete dialog;
  };
  dialog->setSize(500, 300);
  dialog->setCentrePosition(getWidth() / 2, getHeight() / 2);
  addAndMakeVisible(dialog);
  dialog->toFront(true);
  DBG("MainComponent: Audio I/O Settings dialog opened");
}

void MainComponent::showKeyboardShortcutsDialog() {
  juce::String shortcuts = "=== ORPHEUS CLIP COMPOSER - KEYBOARD SHORTCUTS ===\n\n";
  shortcuts += "GLOBAL SHORTCUTS:\n";
  shortcuts +=
      "  \xe2\x86\x91 \xe2\x86\x93 \xe2\x86\x90 \xe2\x86\x92 ......... Move playbox around grid\n";
  shortcuts += "  Space/Enter ..... Trigger playbox button\n";
  shortcuts += "  Esc ............. Stop All Clips (with fade)\n";
  shortcuts += "  Cmd/Ctrl+S ...... Save Session\n";
  shortcuts += "  Cmd/Ctrl+Shift+S  Save Session As\n";
  shortcuts += "  Cmd/Ctrl+, ...... Audio I/O Settings\n";
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
  shortcuts += "  Cmd/Ctrl + Plus .. Zoom in (1x \xe2\x86\x92 16x)\n";
  shortcuts += "  Cmd/Ctrl + Minus . Zoom out (16x \xe2\x86\x92 1x)\n\n";
  shortcuts += "FADE TIMES (Edit Dialog only):\n";
  shortcuts += "  Cmd/Ctrl+Shift+[1-9] ... Set OUT fade (0.1s-0.9s)\n";
  shortcuts += "  Cmd/Ctrl+Shift+0 ....... Set OUT fade (1.0s)\n";
  shortcuts += "  Cmd/Ctrl+Opt+Shift+[1-9] Set IN fade (0.1s-0.9s)\n";
  shortcuts += "  Cmd/Ctrl+Opt+Shift+0 ... Set IN fade (1.0s)\n\n";
  shortcuts += "NOTE: Hold < > buttons in Edit Dialog for auto-repeat";

  juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Keyboard Shortcuts",
                                         shortcuts, "OK");
}

void MainComponent::handleMenuItemSelected(int menuItemID, int /*topLevelMenuIndex*/) {
  switch (menuItemID) {
  case 1: // New Session
    createNewSession();
    DBG("MainComponent: New session created");
    break;

  case 2: // Open Session
    openSessionInteractive();
    break;

  case 3: // Save Session (OCC144: Now tracks current file)
    saveCurrentSession();
    break;

  case 4: // Save Session As
    saveCurrentSessionAs();
    break;

  case 5: // Quit
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
    break;

  case 10: // Clear All Clips
  {
    // Warn user before clearing all clips
    bool confirmed =
        juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon, "Clear All Clips?",
                                           "This will remove all clips from all tabs.\n\n"
                                           "Are you sure?",
                                           "Clear All", "Cancel");

    if (confirmed) {
      // Stop all playing audio first
      if (m_audioEngine) {
        m_audioEngine->stopAllClips();
      }

      auto clearAllCmd =
          std::make_unique<orpheus::ClearButtonsCommand>(m_sessionManager.get(), 0, 383);
      m_undoManager->executeCommand(std::move(clearAllCmd));

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
    showKeyboardShortcutsDialog();
    break;

  case 14: // Clear Current Tab (Item 8)
  {
    // Get current tab index
    int currentTab = m_sessionManager->getActiveTab();
    int clipCount = 0;

    // Count how many clips are on current tab
    for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
      if (m_sessionManager->hasClip(i)) {
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
                           juce::String(currentTab + 1) + ".\n\n" + "Are you sure?";

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

      // Clear all clips on current tab (via UndoManager)
      auto clearTabCmd = std::make_unique<orpheus::ClearPageCommand>(
          m_sessionManager.get(), m_sessionManager->getActiveTab());
      m_undoManager->executeCommand(std::move(clearTabCmd));

      // Update UI
      for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
        auto button = m_clipGrid->getButton(i);
        if (button)
          button->clearClip();
        int globalIndex = getGlobalClipIndex(i);
        m_loopEnabled[globalIndex] = false;
        m_stopOthersOnPlay[globalIndex] = false;
      }

      refreshUiSnapshot();
      DBG("MainComponent: Cleared " << clipCount << " clips from Tab " << (currentTab + 1));
    }
    break;
  }

  case 15: // Toggle Session History Window
  {
    auto* sessionHistoryWindow = getOrCreateSessionHistoryWindow();
    bool isVisible = sessionHistoryWindow->isVisible();
    sessionHistoryWindow->setVisible(!isVisible);
    DBG("MainComponent: Session History Window visibility toggled to "
        << (!isVisible ? "visible" : "hidden"));
    break;
  }

  case 20: // Audio I/O Settings
    showAudioSettings();
    break;

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

    auto* dialog = new PasteSpecialDialog(m_sessionManager.get(), m_clipboardData,
                                          m_sessionManager->getActiveTab());
    dialog->onOkClicked = [this, dialog]() {
      auto options = dialog->getOptions();
      auto targetIndices = dialog->getTargetIndices();

      if (!targetIndices.empty()) {
        // Apply paste special to all target indices
        for (int targetIndex : targetIndices) {
          int buttonIndex = targetIndex % kButtonsPerTab;
          int tabIndex = targetIndex / kButtonsPerTab;

          if (m_sessionManager->hasClip(buttonIndex, tabIndex)) {
            auto targetClip = m_sessionManager->getClip(buttonIndex, tabIndex);

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

            m_sessionManager->setClip(buttonIndex, targetClip, tabIndex);
          }
        }

        for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
          updateButtonFromClip(i);
        }
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
      auto* midiMonitorWindow = getOrCreateMidiMonitorWindow();
      midiMonitorWindow->setVisible(true);
      midiMonitorWindow->toFront(true);
    };

    dialog->setCentrePosition(getWidth() / 2, getHeight() / 2);
    addAndMakeVisible(dialog);
    dialog->toFront(true);
    break;
  }

  case 205: // MIDI Monitor
  {
    auto* midiMonitorWindow = getOrCreateMidiMonitorWindow();
    midiMonitorWindow->setVisible(true);
    midiMonitorWindow->toFront(true);
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
    auto* levelMetersWindow = getOrCreateLevelMetersWindow();
    levelMetersWindow->setVisible(true);
    levelMetersWindow->toFront(true);
    break;
  }

  case 350:
    m_displayPreferences->setGridLayout(orpheus::DisplayPreferences::GridLayout::Columns6Rows6);
    break;
  case 351:
    m_displayPreferences->setGridLayout(orpheus::DisplayPreferences::GridLayout::Columns8Rows6);
    break;
  case 352:
    m_displayPreferences->setGridLayout(orpheus::DisplayPreferences::GridLayout::Columns10Rows6);
    break;
  case 353:
    m_displayPreferences->setGridLayout(orpheus::DisplayPreferences::GridLayout::Columns6Rows8);
    break;
  case 354:
    m_displayPreferences->setGridLayout(orpheus::DisplayPreferences::GridLayout::Columns8Rows8);
    break;
  case 355:
    m_displayPreferences->setGridLayout(orpheus::DisplayPreferences::GridLayout::Columns10Rows8);
    break;
  case 356:
    m_displayPreferences->setGridLayout(orpheus::DisplayPreferences::GridLayout::Columns12Rows8);
    break;
  case 357:
    m_displayPreferences->setGridLayout(orpheus::DisplayPreferences::GridLayout::Columns6Rows10);
    break;
  case 358:
    m_displayPreferences->setGridLayout(orpheus::DisplayPreferences::GridLayout::Columns8Rows10);
    break;
  case 359:
    m_displayPreferences->setGridLayout(orpheus::DisplayPreferences::GridLayout::Columns10Rows10);
    break;

  //==============================================================================
  // Help Menu (OCC144)
  case 400: // Keyboard Shortcuts (duplicated from Session menu for discoverability)
    showKeyboardShortcutsDialog();
    break;

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
