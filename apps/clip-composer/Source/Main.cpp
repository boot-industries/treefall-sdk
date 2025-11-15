// SPDX-License-Identifier: MIT

#include "MainComponent.h"
#include <juce_gui_extra/juce_gui_extra.h>

//==============================================================================
/**
 * Orpheus Clip Composer - Professional soundboard for broadcast, theater, and live performance
 *
 * Entry point for the JUCE application.
 */
class ClipComposerApplication : public juce::JUCEApplication {
public:
  //==============================================================================
  ClipComposerApplication() = default;

  const juce::String getApplicationName() override {
    return "Clip Composer"; // Item 51: Removed "Orpheus" from title
  }
  const juce::String getApplicationVersion() override {
    return "0.2.1"; // Item 51: Updated version for Sprint A/B fixes
  }
  bool moreThanOneInstanceAllowed() override {
    return false;
  }

  //==============================================================================
  void initialise(const juce::String& commandLine) override {
    // Ignore command line for now (MVP)
    juce::ignoreUnused(commandLine);

    // Create main window
    mainWindow.reset(new MainWindow(getApplicationName()));
  }

  void shutdown() override {
    // Clean up main window before application exits
    mainWindow = nullptr;
  }

  //==============================================================================
  void systemRequestedQuit() override {
    // User requested quit (Cmd+Q, close button, etc.)
    quit();
  }

  void anotherInstanceStarted(const juce::String& commandLine) override {
    // Another instance attempted to start (moreThanOneInstanceAllowed() = false)
    juce::ignoreUnused(commandLine);
  }

  //==============================================================================
  /**
   * Main application window that hosts the MainComponent
   */
  class MainWindow : public juce::DocumentWindow {
  public:
    explicit MainWindow(juce::String name)
        : DocumentWindow(name,
                         juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(
                             juce::ResizableWindow::backgroundColourId),
                         DocumentWindow::allButtons) {
      setUsingNativeTitleBar(true);
      auto* mainComponent = new MainComponent();
      setContentOwned(mainComponent, true);

      // Item 51: Dynamic application title
      updateTitle();

#if JUCE_IOS || JUCE_ANDROID
      setFullScreen(true);
#else
      setResizable(true, true);
      centreWithSize(getWidth(), getHeight());
#endif

      setVisible(true);
    }

    void closeButtonPressed() override {
      // This is called when user presses window close button
      JUCEApplication::getInstance()->systemRequestedQuit();
    }

    // Item 51: Update window title with session info
    void updateTitle() {
      juce::String title = "Clip Composer"; // Item 51: Removed "Orpheus"

// TODO: Add session name when SessionManager supports it
// For now, just show version and build info
#ifdef DEBUG
      title += " [DEBUG]";
#endif

      title += " - v0.2.1"; // Updated version for Sprint A/B fixes

      setName(title);
    }

  private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
  };

private:
  std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
// This macro generates the main() routine that launches the application
START_JUCE_APPLICATION(ClipComposerApplication)
