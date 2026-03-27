// SPDX-License-Identifier: MIT

#include "AppShell/ClipComposerMenuModel.h"
#include "AppShell/ClipComposerSessionController.h"
#include "AppShell/ClipComposerWindowController.h"
#include <BuildInfo.h>
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
  const juce::String getApplicationName() override {
    return "Clip Composer";
  }
  const juce::String getApplicationVersion() override {
    return occ::BuildInfo::version;
  }
  bool moreThanOneInstanceAllowed() override {
    return false;
  }

  //==============================================================================
  void initialise(const juce::String& commandLine) override {
    parseCommandLine(commandLine);

    auto windowStart = std::chrono::steady_clock::now();
    mainWindow = std::make_unique<MainWindow>(*this);
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
    mainWindow = nullptr;
  }

  void systemRequestedQuit() override {
    if (mainWindow == nullptr || mainWindow->requestQuit()) {
      quit();
    }
  }

  void anotherInstanceStarted(const juce::String& commandLine) override {
    juce::ignoreUnused(commandLine);
  }

  bool isSmokeTestMode() const {
    return smokeTestDurationSeconds.has_value();
  }

  //==============================================================================
  class MainWindow : public juce::DocumentWindow {
  public:
    explicit MainWindow(ClipComposerApplication& owner)
        : DocumentWindow(owner.getApplicationName(),
                         juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(
                             juce::ResizableWindow::backgroundColourId),
                         DocumentWindow::allButtons),
          m_owner(owner) {
      setUsingNativeTitleBar(true);

      auto* mainComponent = new MainComponent();
      m_mainComponent = mainComponent;
      setContentOwned(mainComponent, true);

#if JUCE_IOS || JUCE_ANDROID
      setFullScreen(true);
#else
      setResizable(true, true);
#endif

      m_sessionController = std::make_unique<ClipComposerSessionController>(*mainComponent);
      m_windowController =
          std::make_unique<ClipComposerWindowController>(*this, *m_sessionController);
      m_menuModel = std::make_unique<ClipComposerMenuModel>(*mainComponent, *m_sessionController,
                                                            *m_windowController);
      m_mainComponent->setAppCommandHandler(
          [this](int commandId) { m_menuModel->invokeCommand(commandId); });
      m_menuModel->install();

#if !JUCE_MAC
      setMenuBar(m_menuModel.get());
#endif

      if (auto* keyMappings = m_menuModel->getCommandManager().getKeyMappings()) {
        addKeyListener(keyMappings);
        mainComponent->addKeyListener(keyMappings);
      }

      m_windowController->restoreWindowState();
      setVisible(true);
      m_sessionController->restoreLastSessionIfAvailable();
      m_windowController->refreshTitleNow();
    }

    ~MainWindow() override {
      if (m_menuModel) {
#if !JUCE_MAC
        setMenuBar(nullptr);
#endif
        if (auto* keyMappings = m_menuModel->getCommandManager().getKeyMappings()) {
          if (m_mainComponent != nullptr) {
            m_mainComponent->removeKeyListener(keyMappings);
          }
          removeKeyListener(keyMappings);
        }
        if (m_mainComponent != nullptr) {
          m_mainComponent->setAppCommandHandler({});
        }
        m_menuModel->uninstall();
      }
    }

    bool requestQuit() {
      if (m_owner.isSmokeTestMode()) {
        return true;
      }
      return m_windowController != nullptr && m_windowController->requestQuit();
    }

    void closeButtonPressed() override {
      m_owner.systemRequestedQuit();
    }

    void moved() override {
      DocumentWindow::moved();
      if (m_windowController != nullptr) {
        m_windowController->persistWindowState();
      }
    }

    void resized() override {
      DocumentWindow::resized();
      if (m_windowController != nullptr) {
        m_windowController->persistWindowState();
      }
    }

    MainComponent* getMainComponent() const {
      return m_mainComponent;
    }

  private:
    ClipComposerApplication& m_owner;
    MainComponent* m_mainComponent = nullptr;
    std::unique_ptr<ClipComposerSessionController> m_sessionController;
    std::unique_ptr<ClipComposerWindowController> m_windowController;
    std::unique_ptr<ClipComposerMenuModel> m_menuModel;

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
           << " version=" << occ::BuildInfo::version
           << " git_hash=" << occ::BuildInfo::gitHash
           << " git_describe=" << occ::BuildInfo::gitDescribe
           << " build_date=" << occ::BuildInfo::buildDate
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
