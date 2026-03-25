// SPDX-License-Identifier: MIT

#pragma once

#include "../UI/AboutDialog.h"
#include "ClipComposerSessionController.h"
#include <juce_gui_extra/juce_gui_extra.h>

class ClipComposerWindowController : private juce::Timer {
public:
  ClipComposerWindowController(juce::DocumentWindow& window,
                               ClipComposerSessionController& sessionController);

  void restoreWindowState();
  void persistWindowState();
  bool requestQuit();
  void toggleFullscreen();
  void minimiseWindow();
  void zoomWindow();
  void bringAllToFront();
  void showAboutDialog();
  void refreshTitleNow();

private:
  static juce::PropertiesFile::Options createPropertiesOptions();
  void timerCallback() override;
  void updateWindowTitle() const;

  juce::DocumentWindow& m_window;
  ClipComposerSessionController& m_sessionController;
  std::unique_ptr<juce::PropertiesFile> m_properties;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipComposerWindowController)
};
