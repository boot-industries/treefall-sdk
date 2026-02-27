/*
  ==============================================================================

    EventLogger.cpp
    Created: 27 Nov 2025
    Author:  Orpheus Clip Composer

  ==============================================================================
*/

#include <orpheus/app/EventLogger.h>

namespace orpheus {

EventLogger::EventLogger(Database& db) : m_db(db) {
  // Ensure log table exists
  if (m_db.isOpen()) {
    m_db.execute("CREATE TABLE IF NOT EXISTS event_log ("
                 "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                 "timestamp INTEGER NOT NULL,"
                 "type TEXT NOT NULL,"
                 "component TEXT NOT NULL,"
                 "message TEXT"
                 ");");

    m_db.execute("CREATE INDEX IF NOT EXISTS idx_event_log_timestamp ON event_log(timestamp);");
  }

  // Start flush timer (1s interval)
  startTimer(1000);
}

EventLogger::~EventLogger() {
  stopTimer();
  flushQueue(); // Flush remaining logs on destruction
}

void EventLogger::log(EventType type, const juce::String& component, const juce::String& message) {
  juce::ScopedLock lock(m_lock);
  juce::Time now = juce::Time::getCurrentTime();
  m_queue.push_back({now, type, component, message});

  // Format the log entry for UI display and push to callback (if set)
  if (onNewLogEntry) {
    juce::String formattedEntry = now.formatted("%H:%M:%S") + " [" + eventTypeToString(type) +
                                  "] " + component + ": " + message;
    onNewLogEntry(formattedEntry);
  }
}

void EventLogger::timerCallback() {
  flushQueue();
}

void EventLogger::flushQueue() {
  std::vector<LogEntry> tempQueue;

  {
    juce::ScopedLock lock(m_lock);
    if (m_queue.empty())
      return;
    tempQueue.swap(m_queue);
  }

  if (!m_db.isOpen())
    return;

  m_db.beginTransaction();

  for (const auto& entry : tempQueue) {
    juce::String sql = "INSERT INTO event_log (timestamp, type, component, message) VALUES (" +
                       juce::String(entry.timestamp.toMilliseconds()) + ", '" +
                       eventTypeToString(entry.type) + "', '" + entry.component.replace("'", "''") +
                       "', '" + // Basic SQL escaping
                       entry.message.replace("'", "''") + "');";

    m_db.execute(sql);
  }

  m_db.commitTransaction();
}

void EventLogger::pruneOldLogs(int daysToKeep) {
  if (!m_db.isOpen())
    return;

  auto cutoffTime = juce::Time::getCurrentTime() - juce::RelativeTime::days(daysToKeep);
  juce::String sql =
      "DELETE FROM event_log WHERE timestamp < " + juce::String(cutoffTime.toMilliseconds()) + ";";
  m_db.execute(sql);
}

juce::String EventLogger::eventTypeToString(EventType type) {
  switch (type) {
  case EventType::Startup:
    return "STARTUP";
  case EventType::Shutdown:
    return "SHUTDOWN";
  case EventType::SessionLoad:
    return "SESSION_LOAD";
  case EventType::SessionSave:
    return "SESSION_SAVE";
  case EventType::AutoBackup:
    return "AUTO_BACKUP";
  case EventType::Error:
    return "ERROR";
  case EventType::Warning:
    return "WARNING";
  case EventType::Info:
    return "INFO";
  case EventType::UserAction:
    return "USER_ACTION";
  case EventType::System:
    return "SYSTEM";
  default:
    return "UNKNOWN";
  }
}

} // namespace orpheus
