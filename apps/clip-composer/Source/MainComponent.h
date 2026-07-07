// SPDX-License-Identifier: MIT

#pragma once

#include "Audio/AudioEngine.h"
#include "ClipGrid/ClipGrid.h"
#include "Core/GridConstants.h"
#include "Core/HotKeyManager.h"
#include "Core/MIDIDeviceManager.h"
#include "Session/SessionManager.h"
#include "Transport/TransportControls.h"
#include "UI/AboutDialog.h"
#include "UI/AudioSettingsDialog.h"
#include "UI/ClipEditDialog.h"
#include "UI/ColorSwatchPicker.h"
#include "UI/ConsoleInspectorPanel.h"
#include "UI/HKGroteskLookAndFeel.h"
#include "UI/HeartbeatIndicator.h"
#include "UI/HotKeySetupDialog.h"
#include "UI/LevelMetersWindow.h"
#include "UI/MIDIDevicesDialog.h"
#include "UI/MIDIMonitorWindow.h"
#include "UI/PasteSpecialDialog.h"
#include "UI/SessionHistoryWindow.h"
#include "UI/TabSwitcher.h"
#include "UIState/ClipComposerUiSnapshot.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <orpheus/app/Database.h>
#include <orpheus/app/DisplayPreferences.h>
#include <orpheus/app/EventLogger.h>
#include <orpheus/app/ExternalToolManager.h>
#include <orpheus/app/PlayoutLogger.h>
#include <orpheus/app/ServiceContext.h>
#include <orpheus/app/UndoManager.h>

// shmui visualizers (all components included via ShmUI.h)
#include <ShmUI.h>

//==============================================================================
/**
 * Main UI component for Orpheus Clip Composer
 *
 * This is the top-level component that hosts all UI elements:
 * - Clip Grid (configurable visible density, 100 logical slots x 8 tabs)
 * - Transport Controls
 * - Routing Panel
 * - Waveform Display
 * - Performance Monitor
 *
 * Threading Model:
 * - Runs on JUCE Message Thread (UI thread)
 * - Communicates with audio thread via lock-free commands
 * - Never blocks the audio thread
 */
class MainComponent : public juce::Component, private juce::Timer {
public:
  //==============================================================================
  MainComponent();
  ~MainComponent() override;

  bool isSessionDirty() const {
    return m_sessionManager->isDirty();
  }
  bool saveCurrentSession();
  bool saveCurrentSessionAs();
  bool saveSessionToFile(const juce::File& file);
  bool loadSessionFromFile(const juce::File& file);
  bool openSessionInteractive();
  void createNewSession();
  juce::File getCurrentSessionFile() const {
    return m_sessionManager->getCurrentFile();
  }
  juce::String getCurrentSessionLabel() const;
  void setAppCommandHandler(std::function<void(int)> handler) {
    m_appCommandHandler = std::move(handler);
  }
  void setOperatorViewMode(occ::ui::OperatorViewMode mode);
  occ::ui::OperatorViewMode getOperatorViewMode() const {
    return m_operatorViewMode;
  }

  //==============================================================================
  void paint(juce::Graphics&) override;
  void resized() override;
  bool keyPressed(const juce::KeyPress& key) override;
  double getAudioEngineInitializationMs() const {
    return m_audioEngineInitializationMs;
  }
  bool hasSessionHistoryWindow() const {
    return m_sessionHistoryWindow != nullptr;
  }
  bool hasMidiMonitorWindow() const {
    return m_midiMonitorWindow != nullptr;
  }
  bool hasLevelMetersWindow() const {
    return m_levelMetersWindow != nullptr;
  }

  //==============================================================================
  // Menu helpers (delegated from ClipComposerMenuModel)
  juce::StringArray getAppMenuBarNames();
  juce::PopupMenu getAppMenuForIndex(int topLevelMenuIndex, const juce::String& menuName);
  void handleMenuItemSelected(int menuItemID, int topLevelMenuIndex);
  void showAudioSettings();
  void showKeyboardShortcutsDialog();

private:
  //==============================================================================
  // Timer callback for latency updates
  void timerCallback() override;

  // OCC144: Apply display preferences to UI components
  void applyDisplayPreferences();

  //==============================================================================
  // Initialization helpers (extract callback wiring from constructor)
  void wireUpClipGridCallbacks();
  void wireUpTransportCallbacks();
  void handleClipStateChanged(int buttonIndex, orpheus::PlaybackState state);
  void refreshUiSnapshot();
  occ::ui::ClipUiSnapshot buildClipUiSnapshot(int buttonIndex) const;
  float calculateClipProgress(const SessionManager::ClipData& clipData, int globalClipIndex) const;
  void handleClipButtonAction(int buttonIndex, ClipButtonAction action);
  void showRoutingInfoForClip(int globalClipIndex) const;

  //==============================================================================
  // Core Functionality
  void onClipRightClicked(int buttonIndex);
  void onClipTriggered(int buttonIndex);     // Trigger clip (keyboard or mouse)
  void onClipDoubleClicked(int buttonIndex); // Edit clip metadata

  /// OCC151 T8 / G6: "Stop Others On Play" choke, scoped to the firing clip's
  /// playgroup (its clipGroup 0-3), NOT global and NOT the visible tab. Walks all
  /// clips and stops those in the same group that are currently playing. The SDK
  /// owns no playgroup concept, so scoping lives here.
  void stopOthersInPlaygroup(int firingGlobalIndex);
  void loadClipToButton(int buttonIndex, const juce::String& filePath);
  void loadMultipleFiles(const juce::Array<juce::File>& files, int startButtonIndex);
  void updateButtonFromClip(int buttonIndex);
  void onClipDraggedToButton(int sourceButtonIndex, int targetButtonIndex);
  void onStopAll();
  void onPanic();
  void updateWindowTitle();
  SessionHistoryWindow* getOrCreateSessionHistoryWindow();
  MIDIMonitorWindow* getOrCreateMidiMonitorWindow();
  LevelMetersWindow* getOrCreateLevelMetersWindow();

  // Tab management
  void onTabSelected(int tabIndex);

  // Keyboard mapping
  int getButtonIndexFromKey(const juce::KeyPress& key) const;
  juce::String getKeyboardShortcutForButton(int buttonIndex) const;

  // Global clip index calculation with 100 logical slots per tab.
  int getGlobalClipIndex(int buttonIndex) const {
    return m_sessionManager->getActiveTab() * occ::BUTTONS_PER_TAB + buttonIndex;
  }

  //==============================================================================
  // Core Services (registered in ServiceContext for cross-component access)
  std::shared_ptr<AudioEngine> m_audioEngine;
  std::shared_ptr<SessionManager> m_sessionManager;

  // Infrastructure (Sprint 0-2)
  std::shared_ptr<orpheus::Database> m_database;
  std::shared_ptr<orpheus::EventLogger> m_eventLogger;
  std::shared_ptr<orpheus::PlayoutLogger> m_playoutLogger;
  std::shared_ptr<orpheus::UndoManager> m_undoManager;

  // Managers (OCC116/OCC117)
  std::shared_ptr<orpheus::DisplayPreferences> m_displayPreferences;
  std::shared_ptr<orpheus::ExternalToolManager> m_externalToolManager;
  std::shared_ptr<orpheus::HotKeyManager> m_hotKeyManager;
  std::shared_ptr<orpheus::MIDIDeviceManager> m_midiDeviceManager;

  //==============================================================================
  // UI Components
  std::unique_ptr<TabSwitcher> m_tabSwitcher;
  std::unique_ptr<ClipGrid> m_clipGrid;
  std::unique_ptr<TransportControls> m_transportControls;
  std::unique_ptr<ConsoleInspectorPanel> m_inspectorPanel;
  std::unique_ptr<shmui::BarVisualizer> m_barVisualizer; // shmui frequency visualizer

  std::unique_ptr<SessionHistoryWindow> m_sessionHistoryWindow;
  std::unique_ptr<HeartbeatIndicator> m_heartbeatIndicator;

  // Popup Windows (OCC116/OCC117)
  std::unique_ptr<MIDIMonitorWindow> m_midiMonitorWindow;
  std::unique_ptr<LevelMetersWindow> m_levelMetersWindow;

  // Custom Look and Feel (HK Grotesk font)
  HKGroteskLookAndFeel m_hkGroteskLookAndFeel;

  // Per-button "stop others on play" mode (audio slot indexed)
  std::array<bool, AudioEngine::MAX_CLIP_BUTTONS> m_stopOthersOnPlay = {};

  // Per-button loop mode (audio slot indexed)
  std::array<bool, AudioEngine::MAX_CLIP_BUTTONS> m_loopEnabled = {};

  // Explicit operator view mode
  occ::ui::OperatorViewMode m_operatorViewMode = occ::ui::OperatorViewMode::Playout;
  std::array<bool, AudioEngine::MAX_CLIP_BUTTONS> m_playNextEnabled = {};
  int m_pendingPlayNextGlobalIndex = -1;

  // Single Edit Dialog tracking (ensures only one dialog open at a time)
  ClipEditDialog* m_currentEditDialog = nullptr;

  // Item 24: Clip Copy/Paste clipboard
  bool m_hasClipInClipboard = false;
  SessionManager::ClipData m_clipboardData;

  // OCC144: Track clip start times for elapsed time logging
  std::unordered_map<int, juce::Time> m_clipStartTimes;

  // Auto-backup: save dirty sessions every 60 seconds
  int m_autoBackupCounter = 0;
  juce::Time m_lastAutoBackupTime;
  double m_audioEngineInitializationMs = 0.0;
  std::function<void(int)> m_appCommandHandler;
  occ::ui::ClipComposerUiSnapshot m_uiSnapshot;

  // OCC149c perf: refreshUiSnapshot() inputs that change at < 1Hz are cached
  // so the 30Hz refresh path doesn't allocate juce::Strings or hit syscalls
  // every frame.
  juce::String m_cachedDeviceNameForLabels; // re-derive playout/audition labels only on change
  int m_cachedProcessMemoryMB = 0;          // populated at 1Hz inside the timer gate
  // Pre-allocated buffer for BarVisualizer::setVolumeBands(const std::vector&)
  // so the 30Hz feed doesn't heap-alloc a 4-float vector every frame.
  std::vector<float> m_barVisualizerLevelsBuffer;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
