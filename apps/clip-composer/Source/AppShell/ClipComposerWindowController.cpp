// SPDX-License-Identifier: MIT

#include "ClipComposerWindowController.h"
#include "BuildInfo.h"

namespace {

constexpr auto kWindowStateKey = "mainWindowState";
constexpr auto kWindowFullscreenKey = "mainWindowFullscreen";

} // namespace

ClipComposerWindowController::ClipComposerWindowController(
    juce::DocumentWindow& window, ClipComposerSessionController& sessionController)
    : m_window(window), m_sessionController(sessionController),
      m_properties(std::make_unique<juce::PropertiesFile>(createPropertiesOptions())) {
  startTimerHz(4);
}

juce::PropertiesFile::Options ClipComposerWindowController::createPropertiesOptions() {
  juce::PropertiesFile::Options options;
  options.applicationName = "OrpheusClipComposer";
  options.filenameSuffix = ".settings";
  options.osxLibrarySubFolder = "Application Support";
  return options;
}

void ClipComposerWindowController::restoreWindowState() {
  auto savedState = m_properties->getValue(kWindowStateKey);
  if (savedState.isNotEmpty()) {
    m_window.restoreWindowStateFromString(savedState);
  } else {
    m_window.centreWithSize(m_window.getWidth(), m_window.getHeight());
  }

  if (m_properties->getBoolValue(kWindowFullscreenKey, false)) {
    m_window.setFullScreen(true);
  }

  updateWindowTitle();
}

void ClipComposerWindowController::persistWindowState() {
  m_properties->setValue(kWindowStateKey, m_window.getWindowStateAsString());
  m_properties->setValue(kWindowFullscreenKey, m_window.isFullScreen());
  m_properties->saveIfNeeded();
}

bool ClipComposerWindowController::requestQuit() {
  if (!m_sessionController.requestQuit()) {
    return false;
  }

  persistWindowState();
  return true;
}

void ClipComposerWindowController::toggleFullscreen() {
  m_window.setFullScreen(!m_window.isFullScreen());
  persistWindowState();
}

void ClipComposerWindowController::minimiseWindow() {
  m_window.setMinimised(true);
}

void ClipComposerWindowController::zoomWindow() {
  auto area = m_window.getParentMonitorArea().reduced(24);
  m_window.setBoundsConstrained(area);
  persistWindowState();
}

void ClipComposerWindowController::bringAllToFront() {
  m_window.toFront(true);
}

void ClipComposerWindowController::showAboutDialog() {
  juce::DialogWindow::LaunchOptions options;
  options.content.setOwned(new AboutDialog());
  options.dialogTitle = "About Orpheus Clip Composer";
  options.dialogBackgroundColour = juce::Colours::transparentBlack;
  options.escapeKeyTriggersCloseButton = true;
  options.useNativeTitleBar = true;
  options.resizable = false;
  options.componentToCentreAround = &m_window;

  if (auto* aboutDialog = dynamic_cast<AboutDialog*>(options.content.get())) {
    aboutDialog->onOkClicked = [dialog = options.content.get()]() {
      if (auto* topLevel = dialog->getTopLevelComponent()) {
        topLevel->exitModalState(0);
        topLevel->setVisible(false);
      }
    };
  }

  options.runModal();
}

void ClipComposerWindowController::refreshTitleNow() {
  updateWindowTitle();
}

void ClipComposerWindowController::timerCallback() {
  updateWindowTitle();
}

void ClipComposerWindowController::updateWindowTitle() const {
  juce::String title = "Clip Composer";
  auto sessionLabel = m_sessionController.getCurrentSessionLabel();
  if (sessionLabel.isNotEmpty()) {
    title += " - " + sessionLabel;
  }
  if (m_sessionController.isSessionDirty()) {
    title += " *";
  }
#ifdef DEBUG
  title += " [DEBUG]";
#endif
  title += " - v";
  title += occ::BuildInfo::version;
  title += " [";
  title += occ::BuildInfo::gitHash;
  title += "]";
  m_window.setName(title);
}
