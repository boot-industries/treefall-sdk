// SPDX-License-Identifier: MIT

#include "BuildInfo.h" // Include the generated build info
#include "MainComponent.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <juce_gui_extra/juce_gui_extra.h>
#include <optional>
#include <sstream>

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
    parseCommandLine(commandLine);

    // Create main window
    auto windowStart = std::chrono::steady_clock::now();
    mainWindow.reset(new MainWindow(getApplicationName()));
    auto windowReady = std::chrono::steady_clock::now();

    if (auto* mainComponent = mainWindow->getMainComponent()) {
      auto windowReadyMs =
          std::chrono::duration<double, std::milli>(windowReady - windowStart).count();
      logStartupMetrics(*mainComponent, windowReadyMs);
    }

    if (smokeTestDurationSeconds.has_value()) {
      juce::Timer::callAfterDelay(*smokeTestDurationSeconds * 1000, [this]() { quit(); });
    }
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

  bool isSmokeTestMode() const {
    return smokeTestDurationSeconds.has_value();
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
      auto* app = dynamic_cast<ClipComposerApplication*>(juce::JUCEApplication::getInstance());
      if (app && app->isSmokeTestMode()) {
        JUCEApplication::getInstance()->systemRequestedQuit();
        return;
      }

      auto* mainComp = dynamic_cast<MainComponent*>(getContentComponent());
      if (mainComp && mainComp->isSessionDirty()) {
        int result = juce::AlertWindow::showYesNoCancelBox(
            juce::AlertWindow::QuestionIcon, "Unsaved Changes",
            "Do you want to save changes before closing?", "Save", "Don't Save", "Cancel");

        if (result == 0) // Cancel
          return;
        if (result == 1) { // Save
          mainComp->saveCurrentSession();
        }
        // result == 2 means Don't Save - fall through to quit
      }
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
      title += " [" + juce::String(APP_BUILD_DATE) + "-" + juce::String(APP_COMMIT_HASH) +
               "]"; // Add build info

      setName(title);
    }

    MainComponent* getMainComponent() const {
      return dynamic_cast<MainComponent*>(getContentComponent());
    }

  private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
  };

private:
  void parseCommandLine(const juce::String& commandLine) {
    juce::StringArray args;
    args.addTokens(commandLine, true);
    args.trim();
    args.removeEmptyStrings();

    for (const auto& arg : args) {
      if (arg == "--smoke-test") {
        smokeTestDurationSeconds = 3;
      } else if (arg.startsWith("--smoke-test=")) {
        auto value = arg.fromFirstOccurrenceOf("=", false, false).getIntValue();
        smokeTestDurationSeconds = std::max(1, value);
      }
    }
  }

  void logStartupMetrics(const MainComponent& mainComponent, double windowReadyMs) const {
    std::ostringstream stream;
    stream << "STARTUP_METRICS"
           << " window_ready_ms=" << windowReadyMs
           << " audio_engine_init_ms=" << mainComponent.getAudioEngineInitializationMs()
           << " session_history_window_created="
           << (mainComponent.hasSessionHistoryWindow() ? 1 : 0)
           << " midi_monitor_window_created=" << (mainComponent.hasMidiMonitorWindow() ? 1 : 0)
           << " level_meters_window_created=" << (mainComponent.hasLevelMetersWindow() ? 1 : 0)
           << " smoke_test_mode=" << (isSmokeTestMode() ? 1 : 0);
    std::cout << stream.str() << std::endl;
  }

  std::unique_ptr<MainWindow> mainWindow;
  std::optional<int> smokeTestDurationSeconds;
};

//==============================================================================
// This macro generates the main() routine that launches the application
START_JUCE_APPLICATION(ClipComposerApplication)
