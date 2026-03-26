// SPDX-License-Identifier: MIT

#include "ClipComposerMenuModel.h"

ClipComposerMenuModel::ClipComposerMenuModel(MainComponent& mainComponent,
                                             ClipComposerSessionController& sessionController,
                                             ClipComposerWindowController& windowController)
    : m_mainComponent(mainComponent), m_sessionController(sessionController),
      m_windowController(windowController) {
  m_commandManager.registerAllCommandsForTarget(this);
  m_commandManager.setFirstCommandTarget(this);
}

ClipComposerMenuModel::~ClipComposerMenuModel() {
  uninstall();
}

bool ClipComposerMenuModel::invokeCommand(int commandId) {
  return m_commandManager.invokeDirectly(commandId, true);
}

void ClipComposerMenuModel::install() const {
#if JUCE_MAC
  juce::MenuBarModel::setMacMainMenu(const_cast<ClipComposerMenuModel*>(this));
#endif
}

void ClipComposerMenuModel::uninstall() const {
#if JUCE_MAC
  if (juce::MenuBarModel::getMacMainMenu() == this) {
    juce::MenuBarModel::setMacMainMenu(nullptr);
  }
#endif
}

juce::StringArray ClipComposerMenuModel::getMenuBarNames() {
  return {"File", "Edit", "Session", "Setup", "Display", "Audio", "Window", "Help"};
}

juce::PopupMenu ClipComposerMenuModel::getMenuForIndex(int topLevelMenuIndex,
                                                       const juce::String& menuName) {
  juce::ignoreUnused(menuName);

  switch (topLevelMenuIndex) {
  case 0:
    return buildFileMenu();
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
    return m_mainComponent.getAppMenuForIndex(topLevelMenuIndex, menuName);
  case 6:
    return buildWindowMenu();
  case 7:
    return buildHelpMenu();
  default:
    return {};
  }
}

void ClipComposerMenuModel::menuItemSelected(int menuItemID, int topLevelMenuIndex) {
  if (menuItemID >= occ::AppCommandIds::openRecentSessionBase &&
      menuItemID < occ::AppCommandIds::openRecentSessionBase +
                       occ::AppCommandIds::maxRecentSessionItems) {
    m_sessionController.openRecentSession(menuItemID - occ::AppCommandIds::openRecentSessionBase);
    return;
  }

  if (invokeCommand(menuItemID)) {
    return;
  }

  if (topLevelMenuIndex >= 1 && topLevelMenuIndex <= 5) {
    m_mainComponent.handleMenuItemSelected(menuItemID, topLevelMenuIndex);
  }
}

juce::ApplicationCommandTarget* ClipComposerMenuModel::getNextCommandTarget() {
  return nullptr;
}

void ClipComposerMenuModel::getAllCommands(juce::Array<juce::CommandID>& commands) {
  commands.addArray({occ::AppCommandIds::newSession, occ::AppCommandIds::openSession,
                     occ::AppCommandIds::saveSession, occ::AppCommandIds::saveSessionAs,
                     occ::AppCommandIds::revertSession,
                     occ::AppCommandIds::clearRecentSessions,
                     occ::AppCommandIds::toggleRestoreLastSession,
                     occ::AppCommandIds::showAudioSettings,
                     occ::AppCommandIds::quitApplication,
                     occ::AppCommandIds::toggleFullscreen,
                     occ::AppCommandIds::minimiseWindow,
                     occ::AppCommandIds::zoomWindow,
                     occ::AppCommandIds::bringAllToFront,
                     occ::AppCommandIds::showKeyboardShortcuts,
                     occ::AppCommandIds::showAbout});
}

void ClipComposerMenuModel::getCommandInfo(juce::CommandID commandID,
                                           juce::ApplicationCommandInfo& result) {
  switch (commandID) {
  case occ::AppCommandIds::newSession:
    result.setInfo("New Session", "Create a new session", "File", 0);
    result.defaultKeypresses.add({ 'n', juce::ModifierKeys::commandModifier, 0 });
    break;
  case occ::AppCommandIds::openSession:
    result.setInfo("Open Session...", "Open a saved session", "File", 0);
    result.defaultKeypresses.add({ 'o', juce::ModifierKeys::commandModifier, 0 });
    break;
  case occ::AppCommandIds::saveSession:
    result.setInfo("Save Session", "Save the current session", "File", 0);
    result.defaultKeypresses.add({ 's', juce::ModifierKeys::commandModifier, 0 });
    break;
  case occ::AppCommandIds::saveSessionAs:
    result.setInfo("Save Session As...", "Save the current session to a new file", "File", 0);
    result.defaultKeypresses.add(
        { 's', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0 });
    break;
  case occ::AppCommandIds::revertSession:
    result.setInfo("Revert to Saved", "Reload the current session from disk", "File", 0);
    result.defaultKeypresses.add({ 'r', juce::ModifierKeys::commandModifier, 0 });
    result.setActive(m_sessionController.getCurrentSessionFile() != juce::File());
    break;
  case occ::AppCommandIds::clearRecentSessions:
    result.setInfo("Clear Recent Sessions", "Clear the recent session list", "File", 0);
    result.setActive(m_sessionController.getRecentSessions().getNumFiles() > 0);
    break;
  case occ::AppCommandIds::toggleRestoreLastSession:
    result.setInfo("Restore Last Session On Launch",
                   "Reopen the most recent saved session on launch", "File", 0);
    result.setTicked(m_sessionController.getRestoreLastSessionOnLaunch());
    break;
  case occ::AppCommandIds::showAudioSettings:
    result.setInfo("Audio I/O Settings...", "Show audio device settings", "Audio", 0);
    result.defaultKeypresses.add({ ',', juce::ModifierKeys::commandModifier, 0 });
    break;
  case occ::AppCommandIds::quitApplication:
    result.setInfo("Quit", "Quit Clip Composer", "File", 0);
    result.defaultKeypresses.add({ 'q', juce::ModifierKeys::commandModifier, 0 });
    break;
  case occ::AppCommandIds::toggleFullscreen:
    result.setInfo("Toggle Full Screen", "Toggle the main window between fullscreen and windowed",
                   "Window", 0);
    result.defaultKeypresses.add(
        { 'f', juce::ModifierKeys::commandModifier | juce::ModifierKeys::ctrlModifier, 0 });
    break;
  case occ::AppCommandIds::minimiseWindow:
    result.setInfo("Minimise", "Minimise the main window", "Window", 0);
    result.defaultKeypresses.add({ 'm', juce::ModifierKeys::commandModifier, 0 });
    break;
  case occ::AppCommandIds::zoomWindow:
    result.setInfo("Zoom", "Zoom the main window", "Window", 0);
    break;
  case occ::AppCommandIds::bringAllToFront:
    result.setInfo("Bring All to Front", "Bring all Clip Composer windows to the front", "Window",
                   0);
    break;
  case occ::AppCommandIds::showKeyboardShortcuts:
    result.setInfo("Keyboard Shortcuts...", "Show keyboard shortcuts", "Help", 0);
    result.defaultKeypresses.add({ '/', juce::ModifierKeys::commandModifier, 0 });
    break;
  case occ::AppCommandIds::showAbout:
    result.setInfo("About Orpheus Clip Composer...", "Show application info", "Help", 0);
    break;
  default:
    break;
  }
}

bool ClipComposerMenuModel::perform(const InvocationInfo& info) {
  switch (info.commandID) {
  case occ::AppCommandIds::newSession:
    m_sessionController.newSession();
    return true;
  case occ::AppCommandIds::openSession:
    m_sessionController.openSession();
    return true;
  case occ::AppCommandIds::saveSession:
    m_sessionController.saveSession();
    return true;
  case occ::AppCommandIds::saveSessionAs:
    m_sessionController.saveSessionAs();
    return true;
  case occ::AppCommandIds::revertSession:
    m_sessionController.revertSession();
    return true;
  case occ::AppCommandIds::clearRecentSessions:
    m_sessionController.clearRecentSessions();
    return true;
  case occ::AppCommandIds::toggleRestoreLastSession:
    m_sessionController.setRestoreLastSessionOnLaunch(
        !m_sessionController.getRestoreLastSessionOnLaunch());
    return true;
  case occ::AppCommandIds::showAudioSettings:
    m_mainComponent.showAudioSettings();
    return true;
  case occ::AppCommandIds::quitApplication:
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
    return true;
  case occ::AppCommandIds::toggleFullscreen:
    m_windowController.toggleFullscreen();
    return true;
  case occ::AppCommandIds::minimiseWindow:
    m_windowController.minimiseWindow();
    return true;
  case occ::AppCommandIds::zoomWindow:
    m_windowController.zoomWindow();
    return true;
  case occ::AppCommandIds::bringAllToFront:
    m_windowController.bringAllToFront();
    return true;
  case occ::AppCommandIds::showKeyboardShortcuts:
    m_mainComponent.showKeyboardShortcutsDialog();
    return true;
  case occ::AppCommandIds::showAbout:
    m_windowController.showAboutDialog();
    return true;
  default:
    return false;
  }
}

juce::PopupMenu ClipComposerMenuModel::buildFileMenu() {
  juce::PopupMenu menu;
  menu.addCommandItem(&m_commandManager, occ::AppCommandIds::newSession);
  menu.addCommandItem(&m_commandManager, occ::AppCommandIds::openSession);

  juce::PopupMenu recentMenu;
  auto recentSessions = m_sessionController.getRecentSessions();
  for (int index = 0; index < recentSessions.getNumFiles() &&
                           index < occ::AppCommandIds::maxRecentSessionItems;
       ++index) {
    auto file = recentSessions.getFile(index);
    auto label = file.getFileNameWithoutExtension();
    auto parent = file.getParentDirectory().getFileName();
    if (parent.isNotEmpty()) {
      label << "  (" << parent << ")";
    }
    recentMenu.addItem(occ::AppCommandIds::openRecentSessionBase + index, label);
  }
  if (recentSessions.getNumFiles() == 0) {
    recentMenu.addItem(0, "No Recent Sessions", false);
  }
  recentMenu.addSeparator();
  recentMenu.addCommandItem(&m_commandManager, occ::AppCommandIds::clearRecentSessions);
  menu.addSubMenu("Open Recent", recentMenu, true);

  menu.addSeparator();
  menu.addCommandItem(&m_commandManager, occ::AppCommandIds::toggleRestoreLastSession);
  menu.addSeparator();
  menu.addCommandItem(&m_commandManager, occ::AppCommandIds::saveSession);
  menu.addCommandItem(&m_commandManager, occ::AppCommandIds::saveSessionAs);
  menu.addCommandItem(&m_commandManager, occ::AppCommandIds::revertSession);
  menu.addSeparator();
  menu.addCommandItem(&m_commandManager, occ::AppCommandIds::quitApplication);
  return menu;
}

juce::PopupMenu ClipComposerMenuModel::buildWindowMenu() {
  juce::PopupMenu menu;
  menu.addCommandItem(&m_commandManager, occ::AppCommandIds::minimiseWindow);
  menu.addCommandItem(&m_commandManager, occ::AppCommandIds::zoomWindow);
  menu.addSeparator();
  menu.addCommandItem(&m_commandManager, occ::AppCommandIds::toggleFullscreen);
  menu.addSeparator();
  menu.addCommandItem(&m_commandManager, occ::AppCommandIds::bringAllToFront);
  return menu;
}

juce::PopupMenu ClipComposerMenuModel::buildHelpMenu() {
  juce::PopupMenu menu;
  menu.addCommandItem(&m_commandManager, occ::AppCommandIds::showKeyboardShortcuts);
  menu.addSeparator();
  menu.addCommandItem(&m_commandManager, occ::AppCommandIds::showAbout);
  return menu;
}
