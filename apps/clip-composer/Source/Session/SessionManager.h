// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <map>
#include <string>

//==============================================================================
/**
 * SessionManager - Manages clip metadata and session state
 *
 * Responsibilities:
 * - Store clip assignments (buttonIndex → ClipData)
 * - Load/save session files (JSON)
 * - Validate audio file paths
 * - Provide clip metadata queries
 *
 * NOT responsible for:
 * - Audio playback (that's AudioEngine)
 * - UI rendering (that's ClipGrid)
 */
class SessionManager {
public:
  //==============================================================================
  /**
   * Clip metadata stored per button
   */
  struct ClipData {
    std::string filePath;    // Absolute path to audio file
    std::string displayName; // User-visible name (default: filename without extension)
    juce::Colour color;      // Visual color in grid
    int clipGroup = 0;       // 0-3 (for routing, future)
    int tabIndex = 0;        // 0-7 (which tab this clip belongs to)

    // Audio metadata (populated when file loads)
    int sampleRate = 0;
    int numChannels = 0;
    int64_t durationSamples = 0;

    // Phase 2: Trim points (samples)
    int64_t trimInSamples = 0;
    int64_t trimOutSamples = 0;

    // Phase 3: Fade times (seconds)
    double fadeInSeconds = 0.0;
    double fadeOutSeconds = 0.0;
    std::string fadeInCurve = "Linear"; // Linear, EqualPower, Exponential
    std::string fadeOutCurve = "Linear";

    // Gain (Feature 5: -30dB to +10dB, default 0dB)
    double gainDb = 0.0;

    // Playback modes
    bool loopEnabled = false;
    bool stopOthersEnabled = false; // Phase 2.1: Stop all other clips when this one plays

    bool isValid() const {
      return !filePath.empty();
    }
  };

  //==============================================================================
  SessionManager();
  ~SessionManager() = default;

  //==============================================================================
  // Tab management

  /**
   * Set the currently active tab (0-7)
   */
  void setActiveTab(int tabIndex);

  /**
   * Get the currently active tab
   */
  int getActiveTab() const {
    return m_currentTab;
  }

  /**
   * Get tab label
   */
  std::string getTabLabel(int tabIndex) const;

  /**
   * Set tab label
   */
  void setTabLabel(int tabIndex, const std::string& label);

  //==============================================================================
  // Clip management

  /**
   * Load an audio file onto a button in the current tab
   *
   * @param buttonIndex 0-47 for MVP (per tab)
   * @param filePath Absolute path to WAV/AIFF/FLAC file
   * @return true if file exists and metadata extracted
   */
  bool loadClip(int buttonIndex, const juce::String& filePath);

  /**
   * Update clip metadata for a button in current tab
   *
   * @param buttonIndex 0-47 for MVP (per tab)
   * @param clipData Updated clip metadata
   */
  void setClip(int buttonIndex, const ClipData& clipData);

  /**
   * Remove clip from button in current tab
   */
  void removeClip(int buttonIndex);

  /**
   * Swap clips between two buttons in current tab
   */
  void swapClips(int buttonIndex1, int buttonIndex2);

  /**
   * Get clip data for a button in current tab
   *
   * @return ClipData if assigned, null ClipData if empty
   */
  ClipData getClip(int buttonIndex) const;

  /**
   * Check if button has a clip assigned in current tab
   */
  bool hasClip(int buttonIndex) const;

  /**
   * Get all assigned clips (for session save)
   */
  std::map<int, ClipData> getAllClips() const {
    return m_clips;
  }

  //==============================================================================
  // Session persistence (JSON)

  /**
   * Save current session to JSON file
   *
   * Format:
   * {
   *   "name": "My Session",
   *   "clips": [
   *     {"buttonIndex": 0, "filePath": "/path/to/audio.wav", "name": "Intro", ...}
   *   ]
   * }
   */
  bool saveSession(const juce::File& file);

  /**
   * Load session from JSON file
   */
  bool loadSession(const juce::File& file);

  /**
   * Clear all clips (new session)
   */
  void clearSession();

  //==============================================================================
  // Session info

  std::string getSessionName() const {
    return m_sessionName;
  }
  void setSessionName(const std::string& name) {
    m_sessionName = name;
  }

  int getClipCount() const {
    return static_cast<int>(m_clips.size());
  }

  juce::File getCurrentFile() const {
    return m_currentFile;
  }

private:
  //==============================================================================
  // Helper: Create composite key from tab and button indices
  int makeKey(int tabIndex, int buttonIndex) const {
    return (tabIndex * 100) + buttonIndex;
  }

  // Helper: Extract metadata from audio file
  ClipData extractMetadata(const juce::String& filePath);

  //==============================================================================
  std::map<int, ClipData> m_clips; // composite key (tab*100 + button) → ClipData
  std::string m_sessionName = "Untitled";
  juce::File m_currentFile; // Last saved/loaded file

  int m_currentTab = 0;                   // Currently active tab (0-7)
  std::array<std::string, 8> m_tabLabels; // Tab labels (default: "Tab 1", "Tab 2", etc.)

  static constexpr int NUM_TABS = 8;
  static constexpr int BUTTONS_PER_TAB = 48;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SessionManager)
};
