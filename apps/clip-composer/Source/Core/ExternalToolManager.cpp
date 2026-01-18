/*
  ==============================================================================

    ExternalToolManager.cpp
    Created: 12 Jan 2026
    Author:  Orpheus Clip Composer

    Sprint 9: External Tool Registry (OCC116)

  ==============================================================================
*/

#include "ExternalToolManager.h"

namespace orpheus {

//==============================================================================
ExternalToolManager::ExternalToolManager() {
  load();
}

//==============================================================================
// Tool Path Management

bool ExternalToolManager::setToolPath(ToolType type, const juce::File& executablePath) {
  // Validate the path exists
  if (!executablePath.exists()) {
    if (onToolLaunchFailed) {
      onToolLaunchFailed(type, "File not found: " + executablePath.getFullPathName());
    }
    return false;
  }

// On macOS, .app bundles are directories
#if JUCE_MAC
  if (!executablePath.isDirectory() && !executablePath.existsAsFile()) {
    if (onToolLaunchFailed) {
      onToolLaunchFailed(type, "Invalid executable: " + executablePath.getFullPathName());
    }
    return false;
  }
#else
  if (!executablePath.existsAsFile()) {
    if (onToolLaunchFailed) {
      onToolLaunchFailed(type, "Not a file: " + executablePath.getFullPathName());
    }
    return false;
  }
#endif

  m_toolPaths[type] = executablePath;
  save();
  return true;
}

juce::File ExternalToolManager::getToolPath(ToolType type) const {
  auto it = m_toolPaths.find(type);
  if (it != m_toolPaths.end()) {
    return it->second;
  }
  return juce::File();
}

void ExternalToolManager::clearToolPath(ToolType type) {
  m_toolPaths.erase(type);
  save();
}

bool ExternalToolManager::isToolConfigured(ToolType type) const {
  auto it = m_toolPaths.find(type);
  return it != m_toolPaths.end() && it->second.exists();
}

//==============================================================================
// Tool Launching

bool ExternalToolManager::launchTool(ToolType type, const juce::File& file) {
  if (!isToolConfigured(type)) {
    if (onToolLaunchFailed) {
      onToolLaunchFailed(type, "Tool not configured. Please set path in Setup menu.");
    }
    return false;
  }

  auto toolPath = getToolPath(type);

#if JUCE_MAC
  // On macOS, use 'open -a' for .app bundles
  if (toolPath.getFileExtension() == ".app") {
    juce::String command =
        "open -a \"" + toolPath.getFullPathName() + "\" \"" + file.getFullPathName() + "\"";
    if (std::system(command.toRawUTF8()) == 0) {
      if (onToolLaunched) {
        onToolLaunched(type, file);
      }
      return true;
    } else {
      if (onToolLaunchFailed) {
        onToolLaunchFailed(type, "Failed to launch: " + toolPath.getFileName());
      }
      return false;
    }
  }
#endif

  // Generic launch using ChildProcess
  juce::ChildProcess process;
  juce::StringArray args;
  args.add(toolPath.getFullPathName());
  args.add(file.getFullPathName());

  if (process.start(args)) {
    if (onToolLaunched) {
      onToolLaunched(type, file);
    }
    return true;
  } else {
    if (onToolLaunchFailed) {
      onToolLaunchFailed(type, "Failed to launch: " + toolPath.getFileName());
    }
    return false;
  }
}

bool ExternalToolManager::launchTool(ToolType type) {
  if (!isToolConfigured(type)) {
    if (onToolLaunchFailed) {
      onToolLaunchFailed(type, "Tool not configured.");
    }
    return false;
  }

  auto toolPath = getToolPath(type);

#if JUCE_MAC
  if (toolPath.getFileExtension() == ".app") {
    juce::String command = "open -a \"" + toolPath.getFullPathName() + "\"";
    if (std::system(command.toRawUTF8()) == 0) {
      if (onToolLaunched) {
        onToolLaunched(type, juce::File());
      }
      return true;
    }
    return false;
  }
#endif

  juce::ChildProcess process;
  juce::StringArray args;
  args.add(toolPath.getFullPathName());

  if (process.start(args)) {
    if (onToolLaunched) {
      onToolLaunched(type, juce::File());
    }
    return true;
  }
  return false;
}

//==============================================================================
// Tool Information

juce::String ExternalToolManager::getToolTypeName(ToolType type) {
  switch (type) {
  case ToolType::WAVEditor:
    return "WAV Editor";
  case ToolType::SearchUtility:
    return "Search Utility";
  case ToolType::FileBrowser:
    return "File Browser";
  }
  return "Unknown Tool";
}

juce::String ExternalToolManager::getToolFileName(ToolType type) const {
  if (!isToolConfigured(type)) {
    return juce::String();
  }
  return getToolPath(type).getFileNameWithoutExtension();
}

//==============================================================================
// Persistence

juce::PropertiesFile::Options ExternalToolManager::getPropertiesFileOptions() const {
  juce::PropertiesFile::Options options;
  options.applicationName = "OrpheusClipComposer";
  options.filenameSuffix = ".externaltools";
  options.osxLibrarySubFolder = "Application Support";
  options.folderName = "OrpheusClipComposer";
  options.storageFormat = juce::PropertiesFile::storeAsXML;
  return options;
}

juce::String ExternalToolManager::toolTypeToKey(ToolType type) {
  switch (type) {
  case ToolType::WAVEditor:
    return "wavEditorPath";
  case ToolType::SearchUtility:
    return "searchUtilityPath";
  case ToolType::FileBrowser:
    return "fileBrowserPath";
  }
  return "unknownTool";
}

void ExternalToolManager::save() {
  juce::PropertiesFile prefs(getPropertiesFileOptions());

  for (const auto& [type, path] : m_toolPaths) {
    prefs.setValue(toolTypeToKey(type), path.getFullPathName());
  }

  prefs.saveIfNeeded();
}

void ExternalToolManager::load() {
  juce::PropertiesFile prefs(getPropertiesFileOptions());

  // Load all tool types
  auto loadTool = [&](ToolType type) {
    juce::String pathStr = prefs.getValue(toolTypeToKey(type), "");
    if (pathStr.isNotEmpty()) {
      juce::File file(pathStr);
      if (file.exists()) {
        m_toolPaths[type] = file;
      }
    }
  };

  loadTool(ToolType::WAVEditor);
  loadTool(ToolType::SearchUtility);
  loadTool(ToolType::FileBrowser);
}

} // namespace orpheus
