/*
  ==============================================================================

    LevelMetersWindow.h
    Created: 12 Jan 2026
    Author:  Orpheus Clip Composer

    Sprint 19: Level Meters Window with Play History (OCC117)

  ==============================================================================
*/

#pragma once

#include "../Audio/AudioEngine.h"
#include <array>
#include <deque>
#include <juce_gui_basics/juce_gui_basics.h>

/**
    Window displaying level meters and play history for all Clip Groups.

    Features:
    - Real-time VU meters for 4 Clip Groups + Master
    - Play history display showing recent clip activity
    - Peak hold indicators
    - Configurable orientation (horizontal/vertical)
*/
class LevelMetersWindow : public juce::DocumentWindow {
public:
  LevelMetersWindow(AudioEngine* audioEngine);
  ~LevelMetersWindow() override;

  void closeButtonPressed() override;

  /** Update meter levels (call from audio callback) */
  void updateLevels(const std::array<float, 4>& groupLevels, float masterLevel);

  /** Add a play history entry */
  void addPlayHistoryEntry(int clipIndex, const juce::String& clipName, int groupIndex);

private:
  class Content;
  std::unique_ptr<Content> m_content;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMetersWindow)
};

//==============================================================================
/**
    Content component for Level Meters window.
*/
class LevelMetersWindow::Content : public juce::Component, private juce::Timer {
public:
  Content(AudioEngine* audioEngine);
  ~Content() override = default;

  void paint(juce::Graphics& g) override;
  void resized() override;

  void updateLevels(const std::array<float, 4>& groupLevels, float masterLevel);
  void addPlayHistoryEntry(int clipIndex, const juce::String& clipName, int groupIndex);

private:
  void timerCallback() override;
  void paintMeter(juce::Graphics& g, juce::Rectangle<int> bounds, float level, float peakLevel,
                  const juce::String& label);

  struct PlayHistoryEntry {
    juce::Time timestamp;
    int clipIndex;
    juce::String clipName;
    int groupIndex;
  };

  AudioEngine* m_audioEngine;

  // Meter levels (thread-safe via atomics)
  std::atomic<float> m_groupLevels[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  std::atomic<float> m_masterLevel{0.0f};

  // Peak hold
  float m_groupPeaks[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float m_masterPeak = 0.0f;
  juce::Time m_peakHoldTimes[5]; // 4 groups + master

  // Play history
  std::deque<PlayHistoryEntry> m_playHistory;
  juce::CriticalSection m_historyLock;

  // UI components
  juce::Label m_titleLabel;
  juce::TextEditor m_historyText;

  static constexpr size_t MAX_HISTORY_ENTRIES = 50;
  static constexpr int PEAK_HOLD_MS = 2000;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Content)
};
