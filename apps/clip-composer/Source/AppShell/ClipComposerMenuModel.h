// SPDX-License-Identifier: MIT

#pragma once

#include "../MainComponent.h"
#include "AppCommandIds.h"
#include "ClipComposerSessionController.h"
#include "ClipComposerWindowController.h"
#include <juce_gui_extra/juce_gui_extra.h>

class ClipComposerMenuModel : public juce::MenuBarModel,
                              public juce::ApplicationCommandTarget {
public:
  ClipComposerMenuModel(MainComponent& mainComponent,
                        ClipComposerSessionController& sessionController,
                        ClipComposerWindowController& windowController);
  ~ClipComposerMenuModel() override;

  juce::ApplicationCommandManager& getCommandManager() {
    return m_commandManager;
  }

  bool invokeCommand(int commandId);
  void install() const;
  void uninstall() const;

  juce::StringArray getMenuBarNames() override;
  juce::PopupMenu getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName) override;
  void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

  juce::ApplicationCommandTarget* getNextCommandTarget() override;
  void getAllCommands(juce::Array<juce::CommandID>& commands) override;
  void getCommandInfo(juce::CommandID commandID, juce::ApplicationCommandInfo& result) override;
  bool perform(const InvocationInfo& info) override;

private:
  juce::PopupMenu buildFileMenu();
  juce::PopupMenu buildWindowMenu();
  juce::PopupMenu buildHelpMenu();

  MainComponent& m_mainComponent;
  ClipComposerSessionController& m_sessionController;
  ClipComposerWindowController& m_windowController;
  juce::ApplicationCommandManager m_commandManager;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipComposerMenuModel)
};
