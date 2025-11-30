/*
  ==============================================================================

    ApplicationPaths.h
    Created: 27 Nov 2025
    Author:  Orpheus Clip Composer

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>

namespace orpheus {

/**
    Manages standard application paths and ensures directory structure existence.
    Follows the XDG Base Directory specification on Linux where possible,
    and platform standards on macOS/Windows.
*/
class ApplicationPaths {
public:
  //==============================================================================
  /** Returns the root application data directory.
      macOS: ~/Library/Application Support/Orpheus Clip Composer/
      Windows: %APPDATA%\Orpheus Clip Composer\
      Linux: ~/.local/share/orpheus-clip-composer/
  */
  static juce::File getAppDataDir();

  /** Returns the directory for user sessions. */
  static juce::File getSessionsDir();

  /** Returns the directory for automatic backups. */
  static juce::File getBackupsDir();

  /** Returns the directory for application logs. */
  static juce::File getLogsDir();

  /** Returns the directory for session templates. */
  static juce::File getTemplatesDir();

  /** Returns the directory for temporary files. */
  static juce::File getTempDir();

  /** Returns the global settings file. */
  static juce::File getSettingsFile();

  //==============================================================================
  /** Ensures all standard directories exist.
      Call this during application startup.
  */
  static juce::Result ensureDirectoriesExist();

private:
  ApplicationPaths() = delete; // Static only
};

} // namespace orpheus
