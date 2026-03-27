// SPDX-License-Identifier: MIT

#include "ClipComposerSessionController.h"
#include "AppCommandIds.h"
#include "../MainComponent.h"

namespace {

constexpr auto kRecentSessionsKey = "recentSessions";
constexpr auto kCurrentSessionPathKey = "currentSessionPath";
constexpr auto kRestoreLastSessionKey = "restoreLastSessionOnLaunch";

} // namespace

ClipComposerSessionController::ClipComposerSessionController(MainComponent& mainComponent)
    : m_mainComponent(mainComponent), m_properties(std::make_unique<juce::PropertiesFile>(
                                      createPropertiesOptions())) {
  m_recentSessions.restoreFromString(m_properties->getValue(kRecentSessionsKey));
  m_recentSessions.removeNonExistentFiles();
  m_recentSessions.setMaxNumberOfItems(occ::AppCommandIds::maxRecentSessionItems);
  persistRecentSessions();

  if (!m_properties->containsKey(kRestoreLastSessionKey)) {
    m_properties->setValue(kRestoreLastSessionKey, true);
    m_properties->saveIfNeeded();
  }
}

juce::PropertiesFile::Options ClipComposerSessionController::createPropertiesOptions() {
  juce::PropertiesFile::Options options;
  options.applicationName = "OrpheusClipComposer";
  options.filenameSuffix = ".settings";
  options.osxLibrarySubFolder = "Application Support";
  return options;
}

bool ClipComposerSessionController::requestQuit() {
  return confirmDiscardIfNeeded("quit");
}

void ClipComposerSessionController::restoreLastSessionIfAvailable() {
  if (!getRestoreLastSessionOnLaunch()) {
    return;
  }

  auto currentSessionPath = m_properties->getValue(kCurrentSessionPathKey);
  if (currentSessionPath.isEmpty()) {
    return;
  }

  auto file = juce::File(currentSessionPath);
  if (!file.existsAsFile()) {
    clearCurrentSessionReference();
    return;
  }

  loadSessionFile(file);
}

void ClipComposerSessionController::newSession() {
  if (!confirmDiscardIfNeeded("create a new session")) {
    return;
  }

  m_mainComponent.createNewSession();
  clearCurrentSessionReference();
}

void ClipComposerSessionController::openSession() {
  if (!confirmDiscardIfNeeded("open another session")) {
    return;
  }

  if (m_mainComponent.openSessionInteractive()) {
    auto currentFile = getCurrentSessionFile();
    if (currentFile != juce::File()) {
      rememberSessionFile(currentFile);
    }
  }
}

void ClipComposerSessionController::openRecentSession(int recentIndex) {
  syncRecentSessions();
  if (recentIndex < 0 || recentIndex >= m_recentSessions.getNumFiles()) {
    return;
  }

  auto file = m_recentSessions.getFile(recentIndex);
  if (!file.existsAsFile()) {
    syncRecentSessions();
    return;
  }

  if (!confirmDiscardIfNeeded("open another session")) {
    return;
  }

  if (!loadSessionFile(file)) {
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::WarningIcon, "Load Failed",
        "Could not load session file:\n" + file.getFullPathName(), "OK");
  }
}

void ClipComposerSessionController::clearRecentSessions() {
  m_recentSessions.clear();
  persistRecentSessions();
}

void ClipComposerSessionController::saveSession() {
  if (m_mainComponent.saveCurrentSession()) {
    auto currentFile = getCurrentSessionFile();
    if (currentFile != juce::File()) {
      rememberSessionFile(currentFile);
    }
  }
}

void ClipComposerSessionController::saveSessionAs() {
  if (m_mainComponent.saveCurrentSessionAs()) {
    auto currentFile = getCurrentSessionFile();
    if (currentFile != juce::File()) {
      rememberSessionFile(currentFile);
    }
  }
}

void ClipComposerSessionController::revertSession() {
  auto currentFile = getCurrentSessionFile();
  if (currentFile == juce::File()) {
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon, "Revert Session",
        "Save the session before using Revert.", "OK");
    return;
  }

  if (!confirmDiscardIfNeeded("revert the current session")) {
    return;
  }

  if (!loadSessionFile(currentFile)) {
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::WarningIcon, "Revert Failed",
        "Could not reload session file:\n" + currentFile.getFullPathName(), "OK");
  }
}

bool ClipComposerSessionController::getRestoreLastSessionOnLaunch() const {
  return m_properties->getBoolValue(kRestoreLastSessionKey, true);
}

void ClipComposerSessionController::setRestoreLastSessionOnLaunch(bool enabled) {
  m_properties->setValue(kRestoreLastSessionKey, enabled);
  m_properties->saveIfNeeded();
}

bool ClipComposerSessionController::isSessionDirty() const {
  return m_mainComponent.isSessionDirty();
}

juce::File ClipComposerSessionController::getCurrentSessionFile() const {
  return m_mainComponent.getCurrentSessionFile();
}

juce::String ClipComposerSessionController::getCurrentSessionLabel() const {
  return m_mainComponent.getCurrentSessionLabel();
}

juce::RecentlyOpenedFilesList ClipComposerSessionController::getRecentSessions() const {
  return m_recentSessions;
}

bool ClipComposerSessionController::confirmDiscardIfNeeded(const juce::String& actionLabel) {
  if (!isSessionDirty()) {
    return true;
  }

  int result = juce::AlertWindow::showYesNoCancelBox(
      juce::AlertWindow::QuestionIcon, "Unsaved Changes",
      "Save changes before you " + actionLabel + "?", "Save", "Don't Save", "Cancel");

  if (result == 0) {
    return false;
  }

  if (result == 1) {
    saveSession();
    return !isSessionDirty();
  }

  return true;
}

bool ClipComposerSessionController::loadSessionFile(const juce::File& file) {
  if (!file.existsAsFile()) {
    return false;
  }

  if (!m_mainComponent.loadSessionFromFile(file)) {
    return false;
  }

  rememberSessionFile(file);
  return true;
}

void ClipComposerSessionController::rememberSessionFile(const juce::File& file) {
  if (file == juce::File()) {
    return;
  }

  m_recentSessions.addFile(file);
  m_recentSessions.removeNonExistentFiles();
  persistRecentSessions();
  m_properties->setValue(kCurrentSessionPathKey, file.getFullPathName());
  m_properties->saveIfNeeded();
}

void ClipComposerSessionController::clearCurrentSessionReference() {
  m_properties->removeValue(kCurrentSessionPathKey);
  m_properties->saveIfNeeded();
}

void ClipComposerSessionController::persistRecentSessions() {
  m_properties->setValue(kRecentSessionsKey, m_recentSessions.toString());
  m_properties->saveIfNeeded();
}

void ClipComposerSessionController::syncRecentSessions() {
  m_recentSessions.removeNonExistentFiles();
  persistRecentSessions();
}
