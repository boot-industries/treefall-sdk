/*
  ==============================================================================

    EventLogger.h
    Created: 27 Nov 2025
    Author:  Orpheus Clip Composer

  ==============================================================================
*/

#pragma once

#include "Database.h"
#include <juce_core/juce_core.h>

namespace orpheus {

enum class EventType {
  Startup,
  Shutdown,
  SessionLoad,
  SessionSave,
  AutoBackup,
  Error,
  Warning,
  Info,
  UserAction,
  System
};

/**
    Structured application event logger backed by SQLite.
    Thread-safe and non-blocking (writes are queued/batched).
*/
class EventLogger : private juce::Timer {
public:
  explicit EventLogger(Database& db);
  ~EventLogger() override;

  /** Logs an event with the specified type and message. */
  void log(EventType type, const juce::String& component, const juce::String& message);

  /** Convenience helpers. */
  void logInfo(const juce::String& component, const juce::String& message) {
    log(EventType::Info, component, message);
  }
  void logWarning(const juce::String& component, const juce::String& message) {
    log(EventType::Warning, component, message);
  }
  void logError(const juce::String& component, const juce::String& message) {
    log(EventType::Error, component, message);
  }

  /** Prunes old logs based on retention policy (default 30 days). */
  void pruneOldLogs(int daysToKeep = 30);

private:
  void timerCallback() override;
  void flushQueue();

  juce::String eventTypeToString(EventType type);

  Database& m_db;

  struct LogEntry {
    juce::Time timestamp;
    EventType type;
    juce::String component;
    juce::String message;
  };

  juce::CriticalSection m_lock;
  std::vector<LogEntry> m_queue;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EventLogger)
};

} // namespace orpheus
