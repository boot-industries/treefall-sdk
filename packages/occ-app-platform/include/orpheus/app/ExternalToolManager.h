/*
  ==============================================================================

    ExternalToolManager.h
    Created: 12 Jan 2026
    Author:  Orpheus Clip Composer

    Sprint 9: External Tool Registry (OCC116)
    Manages paths to external applications (WAV editor, search utility, etc.)

  ==============================================================================
*/

#pragma once

#include <functional>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <map>

namespace orpheus {

/**
    Manages external tool paths for integration with external applications.
    Allows users to configure WAV editor, search utility, etc.
    Paths are persisted via JUCE PropertiesFile.
*/
class ExternalToolManager {
public:
  //==============================================================================
  enum class ToolType { WAVEditor, SearchUtility, FileBrowser };

  //==============================================================================
  ExternalToolManager();
  ~ExternalToolManager() = default;

  //==============================================================================
  // Tool Path Management

  /**
   * Set the executable path for a tool type.
   * @param type The tool type to configure
   * @param executablePath Path to the executable (.exe, .app, or no extension)
   * @return true if path is valid and was set
   */
  bool setToolPath(ToolType type, const juce::File& executablePath);

  /**
   * Get the configured path for a tool type.
   */
  juce::File getToolPath(ToolType type) const;

  /**
   * Clear the configured path for a tool type.
   */
  void clearToolPath(ToolType type);

  /**
   * Check if a tool type has a configured path.
   */
  bool isToolConfigured(ToolType type) const;

  //==============================================================================
  // Tool Launching

  /**
   * Launch an external tool with a file argument.
   * @param type The tool to launch
   * @param file The file to open in the tool
   * @return true if launch was successful
   */
  bool launchTool(ToolType type, const juce::File& file);

  /**
   * Launch an external tool without arguments.
   */
  bool launchTool(ToolType type);

  //==============================================================================
  // Tool Information

  /**
   * Get a human-readable name for a tool type.
   */
  static juce::String getToolTypeName(ToolType type);

  /**
   * Get the file name (without path) of a configured tool.
   * Returns empty string if not configured.
   */
  juce::String getToolFileName(ToolType type) const;

  //==============================================================================
  // Persistence

  /** Save all tool paths to disk */
  void save();

  /** Load tool paths from disk */
  void load();

  //==============================================================================
  // Callbacks

  /** Called when a tool is successfully launched */
  std::function<void(ToolType, const juce::File&)> onToolLaunched;

  /** Called when a tool launch fails */
  std::function<void(ToolType, const juce::String& errorMessage)> onToolLaunchFailed;

private:
  //==============================================================================
  juce::PropertiesFile::Options getPropertiesFileOptions() const;
  static juce::String toolTypeToKey(ToolType type);

  //==============================================================================
  std::map<ToolType, juce::File> m_toolPaths;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExternalToolManager)
};

} // namespace orpheus
