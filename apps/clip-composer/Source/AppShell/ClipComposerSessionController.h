// SPDX-License-Identifier: MIT

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

class MainComponent;

class ClipComposerSessionController {
public:
  explicit ClipComposerSessionController(MainComponent& mainComponent);

  bool requestQuit();
  void restoreLastSessionIfAvailable();

  void newSession();
  void openSession();
  void openRecentSession(int recentIndex);
  void clearRecentSessions();
  void saveSession();
  void saveSessionAs();
  void revertSession();
  bool getRestoreLastSessionOnLaunch() const;
  void setRestoreLastSessionOnLaunch(bool enabled);

  bool isSessionDirty() const;
  juce::File getCurrentSessionFile() const;
  juce::String getCurrentSessionLabel() const;
  juce::RecentlyOpenedFilesList getRecentSessions() const;

private:
  static juce::PropertiesFile::Options createPropertiesOptions();

  bool confirmDiscardIfNeeded(const juce::String& actionLabel);
  bool loadSessionFile(const juce::File& file);
  void rememberSessionFile(const juce::File& file);
  void clearCurrentSessionReference();
  void persistRecentSessions();
  void syncRecentSessions();

  MainComponent& m_mainComponent;
  juce::RecentlyOpenedFilesList m_recentSessions;
  std::unique_ptr<juce::PropertiesFile> m_properties;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipComposerSessionController)
};
