/*
  ==============================================================================

    MainComponent.cpp
    Wave Finder — Exercises occ-app-platform services.

  ==============================================================================
*/

#include "MainComponent.h"

MainComponent::MainComponent() {
  // Initialize occ-app-platform services
  orpheus::ApplicationPaths::ensureDirectoriesExist();
  auto dbFile = orpheus::ApplicationPaths::getAppDataDir().getChildFile("wave_finder.sqlite");
  m_database = std::make_unique<orpheus::Database>();
  m_database->open(dbFile);
  m_eventLogger = std::make_unique<orpheus::EventLogger>(*m_database);

  // Register in ServiceContext (proves DI container works for second app)
  auto& ctx = orpheus::ServiceContext::getInstance();
  ctx.registerService<orpheus::Database>(
      std::shared_ptr<orpheus::Database>(m_database.get(), [](auto*) {}));
  ctx.registerService<orpheus::EventLogger>(
      std::shared_ptr<orpheus::EventLogger>(m_eventLogger.get(), [](auto*) {}));

  m_eventLogger->log(orpheus::EventType::Startup, "WaveFinder", "Application started");

  // UI setup
  m_logDisplay.setMultiLine(true);
  m_logDisplay.setReadOnly(true);
  addAndMakeVisible(m_logDisplay);

  m_logEventButton.onClick = [this] { logTestEvent(); };
  addAndMakeVisible(m_logEventButton);

  m_clearButton.onClick = [this] { m_logDisplay.clear(); };
  addAndMakeVisible(m_clearButton);

  m_statusLabel.setText("occ-app-platform services active", juce::dontSendNotification);
  addAndMakeVisible(m_statusLabel);

  // Wire real-time log display
  m_eventLogger->onNewLogEntry = [this](const juce::String& entry) {
    juce::MessageManager::callAsync([this, entry] {
      m_logDisplay.moveCaretToEnd();
      m_logDisplay.insertTextAtCaret(entry + "\n");
    });
  };

  m_logDisplay.insertTextAtCaret("Wave Finder started.\n");
  m_logDisplay.insertTextAtCaret("Database: " + dbFile.getFullPathName() + "\n");
  m_logDisplay.insertTextAtCaret("ServiceContext services registered.\n\n");

  setSize(640, 400);
}

MainComponent::~MainComponent() {
  m_eventLogger->log(orpheus::EventType::Shutdown, "WaveFinder", "Application shutting down");
}

void MainComponent::paint(juce::Graphics& g) {
  g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized() {
  auto area = getLocalBounds().reduced(10);

  auto topBar = area.removeFromTop(30);
  m_logEventButton.setBounds(topBar.removeFromLeft(120));
  topBar.removeFromLeft(8);
  m_clearButton.setBounds(topBar.removeFromLeft(80));
  topBar.removeFromLeft(8);
  m_statusLabel.setBounds(topBar);

  area.removeFromTop(8);
  m_logDisplay.setBounds(area);
}

void MainComponent::logTestEvent() {
  static int eventCount = 0;
  ++eventCount;
  m_eventLogger->logInfo("WaveFinder", "Test event #" + juce::String(eventCount) + " at " +
                                           juce::Time::getCurrentTime().toString(true, true));
}
