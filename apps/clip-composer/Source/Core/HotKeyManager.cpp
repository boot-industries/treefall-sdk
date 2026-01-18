/*
  ==============================================================================

    HotKeyManager.cpp
    Created: 12 Jan 2026
    Author:  Orpheus Clip Composer

    Sprint 10: HotKey Configuration System (OCC116)

  ==============================================================================
*/

#include "HotKeyManager.h"
#include "../Audio/AudioEngine.h"
#include "../Session/SessionManager.h"

namespace orpheus {

//==============================================================================
HotKeyManager::HotKeyManager() {
  load();
}

//==============================================================================
// Configuration

void HotKeyManager::setScope(Scope scope) {
  if (m_scope != scope) {
    m_scope = scope;
    save();
    notifyChanged();
  }
}

void HotKeyManager::setMultiButtonAction(MultiButtonAction action) {
  if (m_multiButtonAction != action) {
    m_multiButtonAction = action;
    // Reset sequence when switching to/from overlapped
    resetOverlappedSequence();
    save();
    notifyChanged();
  }
}

//==============================================================================
// Hotkey Triggering

std::vector<int> HotKeyManager::findClipsForHotKey(const juce::KeyPress& key, int currentTab,
                                                   SessionManager* sessionManager) const {
  std::vector<int> matchingClips;

  if (sessionManager == nullptr) {
    return matchingClips;
  }

  // Determine tab range based on scope
  int startTab = (m_scope == Scope::Paged) ? currentTab : 0;
  int endTab = (m_scope == Scope::Paged) ? currentTab : 7;

  for (int tabIndex = startTab; tabIndex <= endTab; ++tabIndex) {
    for (int buttonIndex = 0; buttonIndex < 48; ++buttonIndex) {
      // Calculate global clip index
      int globalIndex = tabIndex * 48 + buttonIndex;

      // Get clip data - need to temporarily switch tabs to query
      // Note: This is a simplified implementation. In practice,
      // SessionManager should provide a method to query by global index.
      // For now, we'll store the original tab and restore it.
      int originalTab = sessionManager->getActiveTab();
      const_cast<SessionManager*>(sessionManager)->setActiveTab(tabIndex);

      auto clipData = sessionManager->getClip(buttonIndex);

      const_cast<SessionManager*>(sessionManager)->setActiveTab(originalTab);

      if (!clipData.isValid()) {
        continue;
      }

      // Check if this clip's hotkey matches
      // Note: ClipData needs a hotKey field - for now we match by keyboard layout
      // The actual hotkey is determined by button position in the grid
      // This is a placeholder - real implementation would check clip-specific hotkey
    }
  }

  return matchingClips;
}

bool HotKeyManager::triggerHotKey(const juce::KeyPress& key, int currentTab,
                                  SessionManager* sessionManager, AudioEngine* audioEngine) {
  auto matchingClips = findClipsForHotKey(key, currentTab, sessionManager);

  if (matchingClips.empty()) {
    return false;
  }

  if (m_multiButtonAction == MultiButtonAction::Ganged) {
    // Play all matching clips simultaneously
    for (int clipIndex : matchingClips) {
      if (audioEngine) {
        audioEngine->startClip(clipIndex);
      }
    }
  } else {
    // Overlapped mode: Play next in sequence
    int keyCode = key.getKeyCode();
    int lastIndex = 0;

    auto it = m_lastTriggeredIndex.find(keyCode);
    if (it != m_lastTriggeredIndex.end()) {
      lastIndex = it->second;
    }

    // Find next button that's not already playing
    int nextIndex = -1;
    for (size_t i = 0; i < matchingClips.size(); ++i) {
      int candidateIndex =
          (lastIndex + 1 + static_cast<int>(i)) % static_cast<int>(matchingClips.size());
      int clipIndex = matchingClips[candidateIndex];

      if (audioEngine && !audioEngine->isClipPlaying(clipIndex)) {
        nextIndex = candidateIndex;
        break;
      }
    }

    // If all playing, choose the first one in rotation
    if (nextIndex == -1) {
      nextIndex = (lastIndex + 1) % static_cast<int>(matchingClips.size());
    }

    // Play selected clip
    if (nextIndex >= 0 && nextIndex < static_cast<int>(matchingClips.size())) {
      if (audioEngine) {
        audioEngine->startClip(matchingClips[nextIndex]);
      }
      m_lastTriggeredIndex[keyCode] = nextIndex;
    }
  }

  return true;
}

bool HotKeyManager::hasMultipleButtonsWithSameHotKey(const juce::KeyPress& key, int currentTab,
                                                     SessionManager* sessionManager) const {
  auto matchingClips = findClipsForHotKey(key, currentTab, sessionManager);
  return matchingClips.size() > 1;
}

void HotKeyManager::resetOverlappedSequence() {
  m_lastTriggeredIndex.clear();
}

//==============================================================================
// OCC144: Per-Button HotKey Assignment

void HotKeyManager::assignHotKey(int globalButtonIndex, const juce::KeyPress& key) {
  if (globalButtonIndex < 0 || globalButtonIndex >= MAX_BUTTONS) {
    return;
  }

  if (key.isValid()) {
    m_buttonHotKeys[globalButtonIndex] = key;
    DBG("HotKeyManager: Assigned hotkey to button " << globalButtonIndex << ": "
                                                    << key.getTextDescription());
  } else {
    clearHotKey(globalButtonIndex);
  }

  save();
  notifyChanged();
}

juce::KeyPress HotKeyManager::getHotKey(int globalButtonIndex) const {
  auto it = m_buttonHotKeys.find(globalButtonIndex);
  if (it != m_buttonHotKeys.end()) {
    return it->second;
  }
  return juce::KeyPress();
}

void HotKeyManager::clearHotKey(int globalButtonIndex) {
  auto it = m_buttonHotKeys.find(globalButtonIndex);
  if (it != m_buttonHotKeys.end()) {
    m_buttonHotKeys.erase(it);
    DBG("HotKeyManager: Cleared hotkey for button " << globalButtonIndex);
    save();
    notifyChanged();
  }
}

bool HotKeyManager::hasHotKey(int globalButtonIndex) const {
  return m_buttonHotKeys.find(globalButtonIndex) != m_buttonHotKeys.end();
}

juce::String HotKeyManager::getHotKeyDescription(int globalButtonIndex) const {
  auto it = m_buttonHotKeys.find(globalButtonIndex);
  if (it != m_buttonHotKeys.end()) {
    return it->second.getTextDescription();
  }
  return juce::String();
}

//==============================================================================
// Persistence

juce::PropertiesFile::Options HotKeyManager::getPropertiesFileOptions() const {
  juce::PropertiesFile::Options options;
  options.applicationName = "OrpheusClipComposer";
  options.filenameSuffix = ".hotkeys";
  options.osxLibrarySubFolder = "Application Support";
  options.folderName = "OrpheusClipComposer";
  options.storageFormat = juce::PropertiesFile::storeAsXML;
  return options;
}

void HotKeyManager::save() {
  juce::PropertiesFile prefs(getPropertiesFileOptions());

  prefs.setValue("scope", scopeToString(m_scope));
  prefs.setValue("multiButtonAction", multiButtonActionToString(m_multiButtonAction));

  // OCC144: Save per-button hotkey assignments
  juce::String hotkeysData;
  for (const auto& [buttonIndex, keyPress] : m_buttonHotKeys) {
    if (keyPress.isValid()) {
      // Format: buttonIndex:keyCode:modifiers;
      hotkeysData += juce::String(buttonIndex) + ":" + juce::String(keyPress.getKeyCode()) + ":" +
                     juce::String(keyPress.getModifiers().getRawFlags()) + ";";
    }
  }
  prefs.setValue("buttonHotKeys", hotkeysData);

  prefs.saveIfNeeded();
}

void HotKeyManager::load() {
  juce::PropertiesFile prefs(getPropertiesFileOptions());

  m_scope = stringToScope(prefs.getValue("scope", "Paged"));
  m_multiButtonAction = stringToMultiButtonAction(prefs.getValue("multiButtonAction", "Ganged"));

  // OCC144: Load per-button hotkey assignments
  m_buttonHotKeys.clear();
  juce::String hotkeysData = prefs.getValue("buttonHotKeys", "");
  if (hotkeysData.isNotEmpty()) {
    juce::StringArray entries;
    entries.addTokens(hotkeysData, ";", "");

    for (const auto& entry : entries) {
      if (entry.isEmpty())
        continue;

      juce::StringArray parts;
      parts.addTokens(entry, ":", "");
      if (parts.size() >= 3) {
        int buttonIndex = parts[0].getIntValue();
        int keyCode = parts[1].getIntValue();
        int modifiers = parts[2].getIntValue();

        if (buttonIndex >= 0 && buttonIndex < MAX_BUTTONS && keyCode != 0) {
          m_buttonHotKeys[buttonIndex] = juce::KeyPress(keyCode, juce::ModifierKeys(modifiers), 0);
        }
      }
    }
    DBG("HotKeyManager: Loaded " << m_buttonHotKeys.size() << " custom hotkey assignments");
  }
}

void HotKeyManager::notifyChanged() {
  if (onConfigChanged) {
    onConfigChanged();
  }
}

//==============================================================================
// Utility

juce::String HotKeyManager::scopeToString(Scope scope) {
  switch (scope) {
  case Scope::Global:
    return "Global";
  case Scope::Paged:
    return "Paged";
  }
  return "Paged";
}

HotKeyManager::Scope HotKeyManager::stringToScope(const juce::String& str) {
  if (str == "Global")
    return Scope::Global;
  return Scope::Paged;
}

juce::String HotKeyManager::multiButtonActionToString(MultiButtonAction action) {
  switch (action) {
  case MultiButtonAction::Ganged:
    return "Ganged";
  case MultiButtonAction::Overlapped:
    return "Overlapped";
  }
  return "Ganged";
}

HotKeyManager::MultiButtonAction HotKeyManager::stringToMultiButtonAction(const juce::String& str) {
  if (str == "Overlapped")
    return MultiButtonAction::Overlapped;
  return MultiButtonAction::Ganged;
}

} // namespace orpheus
