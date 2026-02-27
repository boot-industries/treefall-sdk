/*
  ==============================================================================

    Database.h
    Created: 27 Nov 2025
    Author:  Orpheus Clip Composer

  ==============================================================================
*/

#pragma once

#include <functional>
#include <juce_core/juce_core.h>
#include <map>
#include <memory>
#include <vector>

namespace orpheus {

/**
    Abstraction layer for SQLite database interactions.
    Handles database connection, statement execution, and result retrieval.
    Designed to be used from background threads (thread-safe connection, but
    prepared statements should be used on the thread that created them).
*/
class Database {
public:
  using Row = std::map<juce::String, juce::var>;
  using ResultSet = std::vector<Row>;

  Database();
  ~Database();

  /** Opens a connection to the specified database file.
      If the file does not exist, it will be created.
  */
  juce::Result open(const juce::File& dbFile);

  /** Closes the database connection. */
  void close();

  /** Executes a SQL statement that doesn't return rows (INSERT, UPDATE, DELETE, CREATE). */
  juce::Result execute(const juce::String& sql);

  /** Executes a SQL query and returns the results. */
  juce::Result query(const juce::String& sql, ResultSet& results);

  /** Begins a transaction. */
  juce::Result beginTransaction();

  /** Commits a transaction. */
  juce::Result commitTransaction();

  /** Rolls back a transaction. */
  juce::Result rollbackTransaction();

  /** Returns the last error message. */
  juce::String getLastError() const;

  /** Checks if the database is open. */
  bool isOpen() const;

private:
  struct Impl;
  std::unique_ptr<Impl> pImpl;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Database)
};

} // namespace orpheus
