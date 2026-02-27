/*
  ==============================================================================

    Database.cpp
    Created: 27 Nov 2025
    Author:  Orpheus Clip Composer

  ==============================================================================
*/

#include <orpheus/app/Database.h>

// If we are using system sqlite, this include should work.
// If we use the amalgamation, we'll need to ensure it's in the path.
#include <sqlite3.h>

namespace orpheus {

struct Database::Impl {
  sqlite3* db = nullptr;
  juce::String lastError;
};

Database::Database() : pImpl(std::make_unique<Impl>()) {}

Database::~Database() {
  close();
}

juce::Result Database::open(const juce::File& dbFile) {
  if (pImpl->db)
    close();

  // Ensure parent directory exists
  dbFile.getParentDirectory().createDirectory();

  int rc = sqlite3_open(dbFile.getFullPathName().toUTF8(), &pImpl->db);
  if (rc != SQLITE_OK) {
    pImpl->lastError = sqlite3_errmsg(pImpl->db);
    sqlite3_close(pImpl->db);
    pImpl->db = nullptr;
    return juce::Result::fail("Failed to open database: " + pImpl->lastError);
  }

  // Enable extended result codes
  sqlite3_extended_result_codes(pImpl->db, 1);

  // Set busy timeout (important for concurrency)
  sqlite3_busy_timeout(pImpl->db, 5000); // 5 seconds

  return juce::Result::ok();
}

void Database::close() {
  if (pImpl->db) {
    sqlite3_close(pImpl->db);
    pImpl->db = nullptr;
  }
}

bool Database::isOpen() const {
  return pImpl->db != nullptr;
}

juce::String Database::getLastError() const {
  return pImpl->lastError;
}

juce::Result Database::execute(const juce::String& sql) {
  if (!pImpl->db)
    return juce::Result::fail("Database not open");

  char* errMsg = nullptr;
  int rc = sqlite3_exec(pImpl->db, sql.toUTF8(), nullptr, nullptr, &errMsg);

  if (rc != SQLITE_OK) {
    pImpl->lastError = errMsg ? errMsg : "Unknown error";
    if (errMsg)
      sqlite3_free(errMsg);
    return juce::Result::fail(pImpl->lastError);
  }

  return juce::Result::ok();
}

juce::Result Database::query(const juce::String& sql, ResultSet& results) {
  if (!pImpl->db)
    return juce::Result::fail("Database not open");

  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(pImpl->db, sql.toUTF8(), -1, &stmt, nullptr);

  if (rc != SQLITE_OK) {
    pImpl->lastError = sqlite3_errmsg(pImpl->db);
    return juce::Result::fail(pImpl->lastError);
  }

  results.clear();

  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    Row row;
    int colCount = sqlite3_column_count(stmt);

    for (int i = 0; i < colCount; ++i) {
      juce::String colName = sqlite3_column_name(stmt, i);
      int colType = sqlite3_column_type(stmt, i);

      juce::var value;

      switch (colType) {
      case SQLITE_INTEGER:
        value = (int64_t)sqlite3_column_int64(stmt, i);
        break;
      case SQLITE_FLOAT:
        value = sqlite3_column_double(stmt, i);
        break;
      case SQLITE_TEXT:
        value = juce::String(reinterpret_cast<const char*>(sqlite3_column_text(stmt, i)));
        break;
      case SQLITE_BLOB:
        // Blobs are not fully supported in juce::var directly as we might expect
        // Storing as MemoryBlock in var? var doesn't support MemoryBlock.
        // For now, skip or treat as void.
        // Alternatively, encode as base64 string if needed.
        value = juce::var::undefined();
        break;
      case SQLITE_NULL:
      default:
        value = juce::var();
        break;
      }
      row[colName] = value;
    }
    results.push_back(row);
  }

  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    pImpl->lastError = sqlite3_errmsg(pImpl->db);
    return juce::Result::fail(pImpl->lastError);
  }

  return juce::Result::ok();
}

juce::Result Database::beginTransaction() {
  return execute("BEGIN TRANSACTION;");
}

juce::Result Database::commitTransaction() {
  return execute("COMMIT;");
}

juce::Result Database::rollbackTransaction() {
  return execute("ROLLBACK;");
}

} // namespace orpheus
