/*
  ==============================================================================

    PlayoutLogger.h
    Created: 27 Nov 2025
    Author:  Orpheus Clip Composer

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <orpheus/app/Database.h>

namespace orpheus {

/**
    Specialized logger for A/V Sync Licensing and Playout Reporting.
    Tracks strict, high-fidelity playback events including:
    - Exact playback duration
    - Trigger source (User, MIDI, Timecode, Automation)
    - Output assignment
    - Track metadata (ISRC, Artist, Title)

    Designed for export to CSV/XML for PRO (Performance Rights Org) reporting.
*/
struct PlayoutEntry {
  juce::Time startTime;
  double durationSeconds = 0.0;
  juce::String trackName;
  juce::String fileName;
  juce::String outputName;
  juce::String triggerSource; // "User", "Timecode", "MIDI", etc.
  juce::String metadataJson;  // Extensible metadata bag
};

class PlayoutLogger : private juce::Timer {
public:
  explicit PlayoutLogger(Database& db);
  ~PlayoutLogger() override;

  /** Logs the start of a track playback. Returns a unique playback ID. */
  int64_t logPlaybackStart(const PlayoutEntry& entry);

  /** Updates the duration of a playback event (when stopped/paused). */
  void logPlaybackStop(int64_t playbackId, double durationSeconds);

  /** Exports logs for a date range to CSV format. */
  juce::File exportLogsToCsv(juce::Time startDate, juce::Time endDate);

  // TODO: Scheduled email export (Sprint 2 extension)

private:
  void timerCallback() override;
  void flushQueue();

  Database& m_db;

  struct PendingUpdate {
    int64_t id;
    double duration;
  };

  juce::CriticalSection m_lock;
  std::vector<PlayoutEntry> m_startQueue;
  std::vector<PendingUpdate> m_stopQueue;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlayoutLogger)
};

} // namespace orpheus
