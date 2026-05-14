/*
  ==============================================================================

    HotKeyManager.h
    Created: 12 Jan 2026
    Author:  Orpheus Clip Composer

    Sprint 10: HotKey Configuration System (OCC116)
    Manages hotkey scope (Global/Paged) and multi-button action (Ganged/Overlapped).

  ==============================================================================
*/

#pragma once

#include "GridConstants.h"
#include <functional>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <vector>

// Forward declarations
class SessionManager;
class AudioEngine;

namespace orpheus {

/**
    Manages hotkey configuration for clip triggering.

    Scope modes:
    - Global: Hotkeys trigger clips across all tabs
    - Paged: Hotkeys only trigger clips on the current tab

    Multi-button action modes:
    - Ganged: All buttons with same hotkey play simultaneously
    - Overlapped: Buttons play in sequence (round-robin, next not playing)
*/
class HotKeyManager {
public:
  //==============================================================================
  enum class Scope {
    Global, // Hotkey searches all tabs
    Paged   // Hotkey searches current tab only
  };

  enum class MultiButtonAction {
    Ganged,    // All buttons with same hotkey play together
    Overlapped // Buttons play in sequence (round-robin)
  };

  //==============================================================================
  HotKeyManager();
  ~HotKeyManager() = default;

  //==============================================================================
  // Configuration

  void setScope(Scope scope);
  Scope getScope() const {
    return m_scope;
  }

  void setMultiButtonAction(MultiButtonAction action);
  MultiButtonAction getMultiButtonAction() const {
    return m_multiButtonAction;
  }

  //==============================================================================
  // Hotkey Triggering

  /**
   * Find all clip indices matching a hotkey.
   * Respects current scope setting.
   *
   * @param key The key press to match
   * @param currentTab The currently active tab (0-7)
   * @param sessionManager Session manager to query clip hotkeys
   * @return Vector of global clip indices (0-383) that match
   */
  std::vector<int> findClipsForHotKey(const juce::KeyPress& key, int currentTab,
                                      SessionManager* sessionManager) const;

  /**
   * Trigger a hotkey.
   * Finds matching clips and triggers them according to scope and action settings.
   *
   * @param key The key press
   * @param currentTab The currently active tab
   * @param sessionManager Session manager to query clips
   * @param audioEngine Audio engine to trigger playback
   * @return true if any clip was triggered
   */
  bool triggerHotKey(const juce::KeyPress& key, int currentTab, SessionManager* sessionManager,
                     AudioEngine* audioEngine);

  //==============================================================================
  // Overlapped Mode State

  /**
   * Check if a hotkey has multiple buttons assigned.
   * Used for visual indicator (underline hotkey text).
   */
  bool hasMultipleButtonsWithSameHotKey(const juce::KeyPress& key, int currentTab,
                                        SessionManager* sessionManager) const;

  /**
   * Reset overlapped mode sequence tracking.
   * Call this on session load/new.
   */
  void resetOverlappedSequence();

  //==============================================================================
  // OCC144: Per-Button HotKey Assignment

  /**
   * Assign a custom hotkey to a specific button position.
   * @param globalButtonIndex Global button index across the logical tab capacity
   * @param key The key to assign (empty KeyPress to clear)
   */
  void assignHotKey(int globalButtonIndex, const juce::KeyPress& key);

  /**
   * Get the assigned hotkey for a button position.
   * @param globalButtonIndex Global button index
   * @return The assigned KeyPress, or invalid if none assigned
   */
  juce::KeyPress getHotKey(int globalButtonIndex) const;

  /**
   * Clear the assigned hotkey for a button position.
   */
  void clearHotKey(int globalButtonIndex);

  /**
   * Check if a button has a custom hotkey assigned.
   */
  bool hasHotKey(int globalButtonIndex) const;

  /**
   * Get human-readable string for a button's assigned hotkey.
   * Returns empty string if no hotkey assigned.
   */
  juce::String getHotKeyDescription(int globalButtonIndex) const;

  //==============================================================================
  // Persistence

  void save();
  void load();

  //==============================================================================
  // Callbacks

  /** Called when configuration changes */
  std::function<void()> onConfigChanged;

  //==============================================================================
  // Utility

  static juce::String scopeToString(Scope scope);
  static Scope stringToScope(const juce::String& str);

  static juce::String multiButtonActionToString(MultiButtonAction action);
  static MultiButtonAction stringToMultiButtonAction(const juce::String& str);

private:
  //==============================================================================
  juce::PropertiesFile::Options getPropertiesFileOptions() const;
  void notifyChanged();

  //==============================================================================
  Scope m_scope = Scope::Paged;
  MultiButtonAction m_multiButtonAction = MultiButtonAction::Ganged;

  // Track last triggered button per hotkey for overlapped mode
  // Key: keyCode, Value: last triggered clip index in the matching set
  std::map<int, int> m_lastTriggeredIndex;

  // OCC144: Per-button custom hotkey assignments
  // Key: global button index (0-383), Value: assigned KeyPress
  std::map<int, juce::KeyPress> m_buttonHotKeys;

  static constexpr int MAX_BUTTONS = occ::TOTAL_BUTTONS;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HotKeyManager)
};

} // namespace orpheus
