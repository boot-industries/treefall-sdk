/*
  ==============================================================================

    MainComponent.h
    Wave Finder — Minimal app exercising occ-app-platform services.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// occ-app-platform services
#include <orpheus/app/ApplicationPaths.h>
#include <orpheus/app/Database.h>
#include <orpheus/app/EventLogger.h>
#include <orpheus/app/ServiceContext.h>

class MainComponent : public juce::Component {
public:
  MainComponent();
  ~MainComponent() override;

  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  // Services from occ-app-platform
  std::unique_ptr<orpheus::Database> m_database;
  std::unique_ptr<orpheus::EventLogger> m_eventLogger;

  juce::TextEditor m_logDisplay;
  juce::TextButton m_logEventButton{"Log Event"};
  juce::TextButton m_clearButton{"Clear"};
  juce::Label m_statusLabel;

  void logTestEvent();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
