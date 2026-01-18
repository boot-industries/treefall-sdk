// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <map>
#include <string>

//==============================================================================
/**
 * @brief Manages clip metadata and session state for Clip Composer.
 *
 * SessionManager handles persistent storage of clip assignments and session data.
 *
 * @section resp Responsibilities
 * - Store clip assignments (buttonIndex → ClipData)
 * - Load/save session files (JSON format)
 * - Validate audio file paths
 * - Provide clip metadata queries
 * - Manage tab labels and clip group names
 *
 * @section not NOT responsible for
 * - Audio playback (that's AudioEngine)
 * - UI rendering (that's ClipGrid)
 *
 * @section json Session File Format
 * Sessions are stored as JSON files with clip metadata, tab labels,
 * and group names. See saveSession()/loadSession() for format details.
 */
class SessionManager {
public:
  //==============================================================================
  /**
   * @brief Clip metadata stored per button in the session.
   *
   * Contains all persistent information about a clip assignment,
   * including file path, display name, audio properties, trim/fade settings,
   * and playback options.
   */
  struct ClipData {
    std::string filePath;    ///< Absolute path to audio file
    std::string displayName; ///< User-visible name (default: filename without extension)
    juce::Colour color;      ///< Visual color in grid
    int clipGroup = 0;       ///< Clip group assignment (0-3 for routing)
    int tabIndex = 0;        ///< Tab index (0-7) this clip belongs to

    /// @name Audio Metadata
    /// Populated when audio file is loaded.
    /// @{
    int sampleRate = 0;          ///< Sample rate in Hz
    int numChannels = 0;         ///< Number of audio channels
    int64_t durationSamples = 0; ///< Total duration in samples
    /// @}

    /// @name Trim Points
    /// In/out points in samples (relative to file start).
    /// @{
    int64_t trimInSamples = 0;  ///< Playback start point (samples)
    int64_t trimOutSamples = 0; ///< Playback end point (samples)
    /// @}

    /// @name Fade Settings
    /// Fade-in and fade-out configuration.
    /// @{
    double fadeInSeconds = 0.0;          ///< Fade-in duration (seconds)
    double fadeOutSeconds = 0.0;         ///< Fade-out duration (seconds)
    std::string fadeInCurve = "Linear";  ///< Fade-in curve: "Linear", "EqualPower", "Exponential"
    std::string fadeOutCurve = "Linear"; ///< Fade-out curve: "Linear", "EqualPower", "Exponential"
    /// @}

    /// @name Gain and Playback Options
    /// @{
    double gainDb = 0.0;            ///< Output gain in dB (-30 to +10, default 0)
    bool loopEnabled = false;       ///< Whether clip loops indefinitely
    bool stopOthersEnabled = false; ///< Stop all other clips when this one plays
    /// @}

    /**
     * @brief Check if clip data is valid (has a file path).
     * @return true if filePath is not empty
     */
    bool isValid() const {
      return !filePath.empty();
    }
  };

  //==============================================================================
  SessionManager();
  ~SessionManager() = default;

  //==============================================================================
  /// @name Tab Management
  /// @{

  /**
   * @brief Set the currently active tab.
   * @param tabIndex Tab index (0-7)
   */
  void setActiveTab(int tabIndex);

  /**
   * @brief Get the currently active tab.
   * @return Tab index (0-7)
   */
  int getActiveTab() const {
    return m_currentTab;
  }

  /**
   * @brief Get label for a tab.
   * @param tabIndex Tab index (0-7)
   * @return Tab label (default: "Tab 1", "Tab 2", etc.)
   */
  std::string getTabLabel(int tabIndex) const;

  /**
   * @brief Set custom label for a tab.
   * @param tabIndex Tab index (0-7)
   * @param label New tab label
   */
  void setTabLabel(int tabIndex, const std::string& label);

  /// @}

  //==============================================================================
  /// @name Clip Management
  /// @{

  /**
   * @brief Load an audio file onto a button in the current tab.
   * @param buttonIndex Button index (0-47 per tab for MVP, up to 119 per tab for full 960)
   * @param filePath Absolute path to WAV/AIFF/FLAC file
   * @return true if file exists and metadata extracted successfully
   */
  bool loadClip(int buttonIndex, const juce::String& filePath);

  /**
   * @brief Update clip metadata for a button in current tab.
   * @param buttonIndex Button index within current tab
   * @param clipData Updated clip metadata
   */
  void setClip(int buttonIndex, const ClipData& clipData);

  /**
   * @brief Remove clip from button in current tab.
   * @param buttonIndex Button index to clear
   */
  void removeClip(int buttonIndex);

  /**
   * @brief Swap clips between two buttons in current tab.
   * @param buttonIndex1 First button index
   * @param buttonIndex2 Second button index
   */
  void swapClips(int buttonIndex1, int buttonIndex2);

  /**
   * @brief Get clip data for a button in current tab.
   * @param buttonIndex Button index within current tab
   * @return ClipData if assigned, or ClipData with empty filePath if not
   */
  ClipData getClip(int buttonIndex) const;

  /**
   * @brief Get clip data for a button in a specific tab.
   * @param buttonIndex Button index within the tab
   * @param tabIndex Tab index (0-7)
   * @return ClipData if assigned, or ClipData with empty filePath if not
   */
  ClipData getClip(int buttonIndex, int tabIndex) const;

  /**
   * @brief Check if button has a clip assigned in current tab.
   * @param buttonIndex Button index within current tab
   * @return true if button has a valid clip
   */
  bool hasClip(int buttonIndex) const;

  /**
   * @brief Check if button has a clip assigned in a specific tab.
   * @param buttonIndex Button index within the tab
   * @param tabIndex Tab index (0-7)
   * @return true if button has a valid clip
   */
  bool hasClip(int buttonIndex, int tabIndex) const;

  /**
   * @brief Update clip metadata for a button in a specific tab.
   * @param buttonIndex Button index within the tab
   * @param clipData Updated clip metadata
   * @param tabIndex Tab index (0-7)
   */
  void setClip(int buttonIndex, const ClipData& clipData, int tabIndex);

  /**
   * @brief Get all assigned clips (for session save).
   * @return Map of composite keys (tab*100 + button) to ClipData
   */
  std::map<int, ClipData> getAllClips() const {
    return m_clips;
  }

  /// @}

  //==============================================================================
  /// @name Session Persistence
  /// @{

  /**
   * @brief Save current session to JSON file.
   * @param file Destination file path
   * @return true on successful write
   *
   * @par JSON Format
   * @code{.json}
   * {
   *   "name": "My Session",
   *   "clips": [
   *     {"buttonIndex": 0, "filePath": "/path/to/audio.wav", "name": "Intro", ...}
   *   ]
   * }
   * @endcode
   */
  bool saveSession(const juce::File& file);

  /**
   * @brief Load session from JSON file.
   * @param file Source file path
   * @return true on successful load, false if file doesn't exist or is invalid
   */
  bool loadSession(const juce::File& file);

  /**
   * @brief Clear all clips and reset to new session state.
   */
  void clearSession();

  /// @}

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

  //==============================================================================
  // Clip Group management (Item 29)

  /**
   * Get the name of a clip group (0-3)
   * Default names: "Group 1", "Group 2", etc.
   */
  std::string getClipGroupName(int groupIndex) const;

  /**
   * Set custom name for a clip group
   */
  void setClipGroupName(int groupIndex, const std::string& name);

  /**
   * Get abbreviation for a clip group (<3 chars)
   * Examples: "MUS", "SFX", "VOC", "G1"
   */
  std::string getClipGroupAbbreviation(int groupIndex) const;

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

  // Item 29: Clip Group names (user-editable, default to "Group 1", etc.)
  std::array<std::string, 4> m_clipGroupNames = {"Group 1", "Group 2", "Group 3", "Group 4"};

  static constexpr int NUM_TABS = 8;
  static constexpr int BUTTONS_PER_TAB = 48;
  static constexpr int NUM_CLIP_GROUPS = 4;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SessionManager)
};
