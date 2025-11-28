/*
  ==============================================================================

    PlayoutLogger.cpp
    Created: 27 Nov 2025
    Author:  Orpheus Clip Composer

  ==============================================================================
*/

#include "PlayoutLogger.h"
#include "ApplicationPaths.h"

namespace orpheus {

PlayoutLogger::PlayoutLogger(Database& db) : m_db(db) {
  if (m_db.isOpen()) {
    // Schema optimized for reporting
    m_db.execute("CREATE TABLE IF NOT EXISTS playout_log ("
                 "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                 "start_time INTEGER NOT NULL,"
                 "duration REAL DEFAULT 0.0,"
                 "track_name TEXT,"
                 "file_name TEXT,"
                 "output_name TEXT,"
                 "trigger_source TEXT,"
                 "metadata TEXT"
                 ");");

    m_db.execute("CREATE INDEX IF NOT EXISTS idx_playout_time ON playout_log(start_time);");
  }

  startTimer(2000); // Flush every 2s
}

PlayoutLogger::~PlayoutLogger() {
  stopTimer();
  flushQueue();
}

int64_t PlayoutLogger::logPlaybackStart(const PlayoutEntry& entry) {
  // For immediate ID return, we must insert directly or use a reservation system.
  // To keep audio thread non-blocking, we can't do DB IO here.
  // Strategy: Generate a temporary ID or handle async.
  // BETTER: For MVP, we assume this is called from MessageThread (StartClip is UI triggered).
  // If called from AudioThread, we MUST queue.
  // Assuming MessageThread for StartClip logic:

  if (!m_db.isOpen())
    return -1;

  // Direct insert for ID retrieval (assuming low frequency of play starts vs audio blocks)
  juce::String sql =
      "INSERT INTO playout_log (start_time, track_name, file_name, output_name, trigger_source, "
      "metadata) VALUES (" +
      juce::String(entry.startTime.toMilliseconds()) + ", '" + entry.trackName.replace("'", "''") +
      "', '" + entry.fileName.replace("'", "''") + "', '" + entry.outputName.replace("'", "''") +
      "', '" + entry.triggerSource.replace("'", "''") + "', '" +
      entry.metadataJson.replace("'", "''") + "');";

  if (m_db.execute(sql).wasOk()) {
    Database::ResultSet rs;
    m_db.query("SELECT last_insert_rowid() as id;", rs);
    if (!rs.empty())
      return rs[0]["id"];
  }

  return -1;
}

void PlayoutLogger::logPlaybackStop(int64_t playbackId, double durationSeconds) {
  juce::ScopedLock lock(m_lock);
  m_stopQueue.push_back({playbackId, durationSeconds});
}

void PlayoutLogger::timerCallback() {
  flushQueue();
}

void PlayoutLogger::flushQueue() {
  std::vector<PendingUpdate> tempUpdates;

  {
    juce::ScopedLock lock(m_lock);
    if (m_stopQueue.empty())
      return;
    tempUpdates.swap(m_stopQueue);
  }

  if (!m_db.isOpen())
    return;

  m_db.beginTransaction();

  for (const auto& update : tempUpdates) {
    juce::String sql = "UPDATE playout_log SET duration = " + juce::String(update.duration) +
                       " WHERE id = " + juce::String(update.id) + ";";
    m_db.execute(sql);
  }

  m_db.commitTransaction();
}

juce::File PlayoutLogger::exportLogsToCsv(juce::Time startDate, juce::Time endDate) {
  if (!m_db.isOpen())
    return {};

  juce::String sql = "SELECT * FROM playout_log WHERE start_time BETWEEN " +
                     juce::String(startDate.toMilliseconds()) + " AND " +
                     juce::String(endDate.toMilliseconds()) + " ORDER BY start_time ASC;";

  Database::ResultSet rs;
  if (m_db.query(sql, rs).failed())
    return {};

  // Generate CSV
  juce::String csv = "StartTime,Duration,Track,File,Output,Trigger,Metadata\n";

  for (const auto& row : rs) {
    auto time = juce::Time(static_cast<int64_t>(row.at("start_time")));
    csv += time.toString(true, true) + ",";
    csv += row.at("duration").toString() + ",";
    csv += "\"" + row.at("track_name").toString().replace("\"", "\"\"") + "\",";
    csv += "\"" + row.at("file_name").toString().replace("\"", "\"\"") + "\",";
    csv += row.at("output_name").toString() + ",";
    csv += row.at("trigger_source").toString() + ",";
    csv += "\"" + row.at("metadata").toString().replace("\"", "\"\"") + "\"\n";
  }

  juce::File exportFile =
      ApplicationPaths::getLogsDir().getChildFile("PlayoutReport_" + startDate.formatted("%Y%m%d") +
                                                  "-" + endDate.formatted("%Y%m%d") + ".csv");

  exportFile.replaceWithText(csv);
  return exportFile;
}

} // namespace orpheus
