/*
  ==============================================================================

    ApplicationPaths.cpp
    Created: 27 Nov 2025
    Author:  Orpheus Clip Composer

  ==============================================================================
*/

#include <orpheus/app/ApplicationPaths.h>

namespace orpheus {

// Internal helper to get the root folder name
static juce::String getAppName() {
#if JUCE_LINUX
  return "orpheus-clip-composer";
#else
  return "OrpheusClipComposer";
#endif
}

juce::File ApplicationPaths::getAppDataDir() {
  auto options = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
  return options.getChildFile(getAppName());
}

juce::File ApplicationPaths::getSessionsDir() {
  // On macOS/Windows, Documents is often better for user-visible files than AppData
  // But for consistency with the plan, let's put "Sessions" in Documents
  // and system-managed stuff in AppData.

  // Actually, the plan said:
  // sessions/            # Default session storage
  // backups/             # Auto-backup files
  // ... inside the AppData structure.
  // However, users expect to save sessions in Documents.
  // Let's stick to the strict interpretation of the plan for now:
  // everything under the AppData root for self-contained simplicity,
  // EXCEPT maybe user sessions which might default to Documents.

  // Reviewing OCC115 Sprint 1:
  // "Establish standardized application data folder for persistent storage...
  // sessions/ # Default session storage"

  return getAppDataDir().getChildFile("sessions");
}

juce::File ApplicationPaths::getBackupsDir() {
  return getAppDataDir().getChildFile("backups");
}

juce::File ApplicationPaths::getLogsDir() {
  return getAppDataDir().getChildFile("logs");
}

juce::File ApplicationPaths::getTemplatesDir() {
  return getAppDataDir().getChildFile("templates");
}

juce::File ApplicationPaths::getTempDir() {
  return getAppDataDir().getChildFile("temp");
}

juce::File ApplicationPaths::getSettingsFile() {
  return getAppDataDir().getChildFile("settings.json");
}

juce::Result ApplicationPaths::ensureDirectoriesExist() {
  const juce::File dirs[] = {getAppDataDir(), getSessionsDir(),  getBackupsDir(),
                             getLogsDir(),    getTemplatesDir(), getTempDir()};

  for (const auto& dir : dirs) {
    if (!dir.exists()) {
      auto result = dir.createDirectory();
      if (result.failed())
        return result;
    }
  }

  return juce::Result::ok();
}

} // namespace orpheus
