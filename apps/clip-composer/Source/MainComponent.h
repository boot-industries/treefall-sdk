// SPDX-License-Identifier: MIT

#pragma once

#include "Audio/AudioEngine.h"
#include "ClipGrid/ClipGrid.h"
#include "Core/Database.h"
#include "Core/DisplayPreferences.h"
#include "Core/EventLogger.h"
#include "Core/ExternalToolManager.h"
#include "Core/HotKeyManager.h"
#include "Core/MIDIDeviceManager.h"
#include "Core/PlayoutLogger.h"
#include "Core/ServiceContext.h"
#include "Core/UndoManager.h"
#include "Session/SessionManager.h"
#include "UI/AudioSettingsDialog.h"
#include "UI/ClipEditDialog.h"
#include "UI/ColorSwatchPicker.h"
#include "UI/HKGroteskLookAndFeel.h"
#include "UI/HeartbeatIndicator.h"
#include "UI/HotKeySetupDialog.h"
#include "UI/LevelMetersWindow.h"
#include "UI/MIDIDevicesDialog.h"
#include "UI/MIDIMonitorWindow.h"
#include "UI/PasteSpecialDialog.h"
#include "UI/SessionHistoryWindow.h"
#include "UI/TabSwitcher.h"
#include <juce_gui_extra/juce_gui_extra.h>

// shmui visualizers (all components included via ShmUI.h)
#include <ShmUI.h>

//==============================================================================
/**
 * Main UI component for Orpheus Clip Composer
 *
 * This is the top-level component that hosts all UI elements:
 * - Clip Grid (48 buttons MVP, 960 buttons full version: 10×12 × 8 tabs)
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
class MainComponent : public juce::Component, public juce::MenuBarModel, private juce::Timer {
public:
  //==============================================================================
  MainComponent();
  ~MainComponent() override;

  //==============================================================================
  void paint(juce::Graphics&) override;
  void resized() override;
  bool keyPressed(const juce::KeyPress& key) override;

  //==============================================================================
  // MenuBarModel overrides
  juce::StringArray getMenuBarNames() override;
  juce::PopupMenu getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName) override;
  void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

private:
  //==============================================================================
  // Timer callback for latency updates
  void timerCallback() override;

  //==============================================================================
  // Core Functionality
  void onClipRightClicked(int buttonIndex);
  void onClipTriggered(int buttonIndex);     // Trigger clip (keyboard or mouse)
  void onClipDoubleClicked(int buttonIndex); // Edit clip metadata
  void loadClipToButton(int buttonIndex, const juce::String& filePath);
  void loadMultipleFiles(const juce::Array<juce::File>& files, int startButtonIndex);
  void updateButtonFromClip(int buttonIndex);
  void onClipDraggedToButton(int sourceButtonIndex, int targetButtonIndex);
  void onStopAll();
  void onPanic();

  // Tab management
  void onTabSelected(int tabIndex);

  // Keyboard mapping
  int getButtonIndexFromKey(const juce::KeyPress& key) const;
  juce::String getKeyboardShortcutForButton(int buttonIndex) const;

  // Global clip index calculation (buttonIndex 0-47 + current tab → 0-383)
  int getGlobalClipIndex(int buttonIndex) const {
    return m_sessionManager.getActiveTab() * 48 + buttonIndex;
  }

  //==============================================================================
  // Core Services (Ownership)
  std::unique_ptr<AudioEngine> m_audioEngine;
  SessionManager m_sessionManager; // Stack allocated in v0.1, maybe change to unique_ptr later

  // Infrastructure (Sprint 0-2)
  std::unique_ptr<orpheus::Database> m_database;
  std::unique_ptr<orpheus::EventLogger> m_eventLogger;
  std::unique_ptr<orpheus::PlayoutLogger> m_playoutLogger;
  std::unique_ptr<orpheus::UndoManager> m_undoManager;

  // Managers (OCC116/OCC117)
  std::unique_ptr<orpheus::DisplayPreferences> m_displayPreferences;
  std::unique_ptr<orpheus::ExternalToolManager> m_externalToolManager;
  std::unique_ptr<orpheus::HotKeyManager> m_hotKeyManager;
  std::unique_ptr<orpheus::MIDIDeviceManager> m_midiDeviceManager;

  //==============================================================================
  // UI Components
  std::unique_ptr<TabSwitcher> m_tabSwitcher;
  std::unique_ptr<ClipGrid> m_clipGrid;
  std::unique_ptr<shmui::BarVisualizer> m_barVisualizer; // shmui frequency visualizer

  std::unique_ptr<SessionHistoryWindow> m_sessionHistoryWindow;
  std::unique_ptr<HeartbeatIndicator> m_heartbeatIndicator;

  // Popup Windows (OCC116/OCC117)
  std::unique_ptr<MIDIMonitorWindow> m_midiMonitorWindow;
  std::unique_ptr<LevelMetersWindow> m_levelMetersWindow;

  // Custom Look and Feel (HK Grotesk font)
  HKGroteskLookAndFeel m_hkGroteskLookAndFeel;

  // Per-button "stop others on play" mode (bitset for MAX_CLIP_BUTTONS clips: 48 buttons × 8 tabs)
  std::array<bool, AudioEngine::MAX_CLIP_BUTTONS> m_stopOthersOnPlay = {};

  // Per-button loop mode (bitset for MAX_CLIP_BUTTONS clips: 48 buttons × 8 tabs)
  std::array<bool, AudioEngine::MAX_CLIP_BUTTONS> m_loopEnabled = {};

  // Single Edit Dialog tracking (ensures only one dialog open at a time)
  ClipEditDialog* m_currentEditDialog = nullptr;

  // Item 24: Clip Copy/Paste clipboard
  bool m_hasClipInClipboard = false;
  SessionManager::ClipData m_clipboardData;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};