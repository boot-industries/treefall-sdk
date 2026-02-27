/*
  ==============================================================================

    Main.cpp
    Wave Finder — Orpheus SDK proof-of-concept application.
    Validates that occ-app-platform package works for a second consumer.

  ==============================================================================
*/

#include "MainComponent.h"
#include <juce_gui_basics/juce_gui_basics.h>

class WaveFinderApplication : public juce::JUCEApplication {
public:
  WaveFinderApplication() = default;

  const juce::String getApplicationName() override {
    return "Orpheus Wave Finder";
  }
  const juce::String getApplicationVersion() override {
    return "0.1.0";
  }
  bool moreThanOneInstanceAllowed() override {
    return false;
  }

  void initialise(const juce::String& /*commandLine*/) override {
    m_mainWindow = std::make_unique<MainWindow>(getApplicationName());
  }

  void shutdown() override {
    m_mainWindow.reset();
  }

  void systemRequestedQuit() override {
    quit();
  }

private:
  class MainWindow : public juce::DocumentWindow {
  public:
    explicit MainWindow(const juce::String& name)
        : DocumentWindow(name,
                         juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(
                             ResizableWindow::backgroundColourId),
                         DocumentWindow::allButtons) {
      setUsingNativeTitleBar(true);
      setContentOwned(new MainComponent(), true);
      setResizable(true, true);
      centreWithSize(getWidth(), getHeight());
      setVisible(true);
    }

    void closeButtonPressed() override {
      JUCEApplication::getInstance()->systemRequestedQuit();
    }

  private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
  };

  std::unique_ptr<MainWindow> m_mainWindow;
};

START_JUCE_APPLICATION(WaveFinderApplication)
