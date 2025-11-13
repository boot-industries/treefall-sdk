# OCC115 - File Menu Backend Features - Sprint Plan

**Version:** 1.0
**Date:** 2025-11-12
**Status:** Planning
**Sprint Context:** Backend Architecture Sprint (SpotOn Section 01 Analysis)
**Dependencies:** OCC114 (Codebase Audit)

---

## Executive Summary

This document presents a comprehensive sprint plan for implementing File Menu backend features in Clip Composer, based on analysis of the SpotOn Manual Section 01 (File Menu). SpotOn demonstrates mature, production-grade infrastructure for session management, auto-backup, missing file resolution, and package portability that OCC currently lacks.

**Key Finding:** SpotOn's File Menu reveals **10 critical backend systems** that are essential for broadcast/theater use cases but are absent or minimal in OCC.

---

## 1. SpotOn File Menu Analysis Summary

### 1.1 Core Features Identified

| Feature                       | SpotOn Has?                           | OCC Has?              | Priority |
| ----------------------------- | ------------------------------------- | --------------------- | -------- |
| **New Session Templates**     | ✅ Yes (~newsession.dta)              | ❌ No                 | HIGH     |
| **Recent Files List (MRU)**   | ✅ Yes (Sessions + Packages)          | ❌ No                 | CRITICAL |
| **Auto-Backup System**        | ✅ Yes (Timed, timestamped)           | ❌ No                 | CRITICAL |
| **Restore from Backup**       | ✅ Yes (UI with timestamp selector)   | ❌ No                 | CRITICAL |
| **Package System**            | ✅ Yes (.pkg format, session + audio) | ❌ No                 | HIGH     |
| **Missing File Resolution**   | ✅ Yes (Wizard dialog, auto-locate)   | ❌ No                 | CRITICAL |
| **File Timestamp Validation** | ✅ Yes (Detects modified files)       | ❌ No                 | MEDIUM   |
| **Track List Export**         | ✅ Yes (CSV/TXT export)               | ❌ No                 | LOW      |
| **Block Save/Load**           | ✅ Yes (Button range import/export)   | ❌ No                 | MEDIUM   |
| **Status Logs on Exit**       | ✅ Yes (~StatusLog.txt)               | ⚠️ Partial (DBG only) | HIGH     |

---

### 1.2 Key Insights

**SpotOn's Strengths:**

1. **Data Safety:** Auto-backup, restore from backup, missing file wizard
2. **Portability:** Package system bundles session + audio files
3. **Usability:** Recent files, session templates, block operations
4. **Diagnostics:** Status logs, event logs, load reports
5. **Robustness:** File timestamp validation, path resolution, version checking

**OCC's Gaps:**

- ❌ No safety net for data loss (no auto-backup, no restore)
- ❌ No portability (can't bundle session + audio)
- ❌ No workflow efficiency (no recent files, no templates)
- ❌ Minimal diagnostics (console-only DBG logs)

---

## 2. Sprint Breakdown

### Sprint 1: Session Management Foundation (CRITICAL)

**Complexity:** Medium (M)
**Estimated Duration:** 1-2 weeks
**Priority:** CRITICAL
**Dependencies:** None

#### 2.1 Scope

**A. Application Data Folder Structure**

Establish standardized application data folder for persistent storage:

**Directory Structure:**

```
~/.local/share/orpheus-clip-composer/  (Linux)
~/Library/Application Support/OrpheusClipComposer/  (macOS)
%APPDATA%\OrpheusClipComposer\  (Windows)
├── sessions/            # Default session storage
├── backups/             # Auto-backup files
├── templates/           # Session templates
├── logs/                # Application logs
│   ├── events/          # Event logs (session load/save)
│   └── status/          # Status logs (exit diagnostics)
└── preferences/         # Application-wide preferences
```

**Implementation:**

```cpp
// In SessionManager or new ApplicationPaths class
class ApplicationPaths {
public:
    static juce::File getAppDataFolder();
    static juce::File getSessionsFolder();
    static juce::File getBackupsFolder();
    static juce::File getTemplatesFolder();
    static juce::File getLogsFolder();
    static juce::File getPreferencesFolder();

    // Create folders if they don't exist
    static void ensureFolders();
};
```

**B. Session Templates**

Support for session templates to speed up new session creation:

**Features:**

- Default template: `~newsession.occSession` (loaded on "New Session")
- User-customizable (save current session as template)
- Template metadata (name, description, author)

**Implementation:**

```cpp
// In SessionManager
bool SessionManager::loadNewSession() {
    // Clear all clips first
    clearSession();

    // Check for template
    auto templateFile = ApplicationPaths::getTemplatesFolder()
        .getChildFile("newsession.occSession");

    if (templateFile.existsAsFile()) {
        return loadSession(templateFile);
    }

    // Otherwise, start with empty session
    return true;
}

bool SessionManager::saveAsTemplate(const juce::File& templateFile) {
    // Save current session as template
    return saveSession(templateFile);
}
```

**C. File Extension Validation**

Enforce `.occSession` extension and reject invalid extensions:

**Implementation:**

```cpp
// In SessionManager::saveSession()
bool SessionManager::validateSessionExtension(const juce::String& filename) {
    juce::String extension = juce::File(filename).getFileExtension();

    if (extension.isEmpty()) {
        // Auto-add extension
        return true;  // Will append .occSession
    }

    if (extension != ".occSession") {
        // Show error dialog
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Invalid File Extension",
            "Filename has an incorrect file extension \"" + extension +
            "\" instead of \".occSession\"\n\n"
            "The Session has not been saved...",
            "OK"
        );
        return false;
    }

    return true;
}
```

#### 2.2 Acceptance Criteria

- [ ] Application data folder structure created on first launch
- [ ] Default session template loads on "New Session" (if exists)
- [ ] User can save current session as template
- [ ] File extension validation works for save operations
- [ ] Invalid extensions show user-friendly error dialog
- [ ] Cross-platform paths work correctly (macOS, Windows, Linux)

#### 2.3 Technical Requirements

- Use JUCE `File::getSpecialLocation()` for cross-platform paths
- Create folders with appropriate permissions
- Handle folder creation failures gracefully
- Document folder structure in OCC docs

---

### Sprint 2: Auto-Backup and Restore System (CRITICAL)

**Complexity:** Large (L)
**Estimated Duration:** 2-3 weeks
**Priority:** CRITICAL
**Dependencies:** Sprint 1 (Application Data Folder)

#### 2.1 Scope

**A. Auto-Backup System**

Implement timed automatic backups to prevent data loss:

**Features:**

- Configurable backup interval (default: 5 minutes)
- Timestamped backup files (e.g., `~session_2025-11-12_14-30-00.occSession.backup`)
- Backup rotation (keep last N backups, default: 10)
- Background thread for non-blocking saves
- Only backup if session is "dirty" (modified since last save)

**Implementation:**

```cpp
// In SessionManager
class SessionManager : public juce::Timer {
public:
    void enableAutoBackup(bool enabled, int intervalSeconds = 300);
    void timerCallback() override;

private:
    void performAutoBackup();
    void rotateBackups(int maxBackups = 10);

    bool m_autoBackupEnabled = true;
    int m_autoBackupIntervalSeconds = 300;  // 5 minutes
    juce::File m_lastBackupFile;
};

void SessionManager::timerCallback() {
    if (!isDirty) return;  // No changes since last save

    performAutoBackup();
}

void SessionManager::performAutoBackup() {
    // Generate timestamped filename
    auto timestamp = juce::Time::getCurrentTime()
        .formatted("%Y-%m-%d_%H-%M-%S");
    auto backupFilename = "~session_" + timestamp + ".occSession.backup";
    auto backupFile = ApplicationPaths::getBackupsFolder()
        .getChildFile(backupFilename);

    // Save session to backup file (non-blocking)
    if (saveSession(backupFile)) {
        m_lastBackupFile = backupFile;
        rotateBackups();
    }
}

void SessionManager::rotateBackups(int maxBackups) {
    auto backupFolder = ApplicationPaths::getBackupsFolder();
    auto backupFiles = backupFolder.findChildFiles(
        juce::File::findFiles, false, "*.occSession.backup");

    // Sort by modification time (oldest first)
    std::sort(backupFiles.begin(), backupFiles.end(),
        [](const juce::File& a, const juce::File& b) {
            return a.getLastModificationTime() < b.getLastModificationTime();
        });

    // Delete oldest backups if over limit
    while (backupFiles.size() > maxBackups) {
        backupFiles[0].deleteFile();
        backupFiles.erase(backupFiles.begin());
    }
}
```

**B. Restore from Backup UI**

User-facing dialog to restore from automatic backups:

**Features:**

- List backups by timestamp (most recent first)
- Show relative timestamps ("11:27 today", "16:54 yesterday", "18:23 Sun 11 Apr")
- Preview backup metadata (session name, clip count, last modified)
- Confirm before restoring (non-destructive)

**Implementation:**

```cpp
// New UI component: RestoreBackupDialog
class RestoreBackupDialog : public juce::Component {
public:
    RestoreBackupDialog(SessionManager* sessionManager);

    void loadBackupList();
    void restoreSelectedBackup();

    std::function<void(juce::File)> onBackupSelected;

private:
    SessionManager* m_sessionManager;
    juce::ListBox m_backupList;
    juce::TextButton m_restoreButton;
    juce::TextButton m_cancelButton;

    std::vector<juce::File> m_backupFiles;
};
```

**C. Crash Recovery**

Auto-restore most recent backup on startup if previous session crashed:

**Implementation:**

```cpp
// In MainComponent::MainComponent() or Application startup
void Application::checkForCrashRecovery() {
    auto crashMarkerFile = ApplicationPaths::getAppDataFolder()
        .getChildFile(".crash_marker");

    if (crashMarkerFile.existsAsFile()) {
        // Previous session crashed
        auto mostRecentBackup = getMostRecentBackup();

        if (mostRecentBackup.existsAsFile()) {
            // Prompt user to restore
            auto result = juce::AlertWindow::show(
                juce::MessageBoxIconType::QuestionIcon,
                "Crash Recovery",
                "The application did not close properly last time.\n\n"
                "Would you like to restore the most recent backup?",
                "Restore", "Start Fresh", "Cancel"
            );

            if (result == 1) {  // Restore
                sessionManager->loadSession(mostRecentBackup);
            }
        }

        // Remove crash marker
        crashMarkerFile.deleteFile();
    }

    // Create new crash marker (removed on clean exit)
    crashMarkerFile.create();
}

void Application::shutdown() {
    // Remove crash marker on clean exit
    auto crashMarkerFile = ApplicationPaths::getAppDataFolder()
        .getChildFile(".crash_marker");
    crashMarkerFile.deleteFile();
}
```

#### 2.2 Acceptance Criteria

- [ ] Auto-backup saves session every N minutes (configurable)
- [ ] Backup files have timestamped filenames
- [ ] Backup rotation keeps last N backups, deletes older ones
- [ ] "Restore from Backup" UI shows list of backups with relative timestamps
- [ ] User can restore backup with confirmation dialog
- [ ] Crash recovery prompts user to restore most recent backup on startup
- [ ] Auto-backup respects session "dirty" state (only backups if modified)
- [ ] Backup operations don't block UI (background thread)

#### 2.3 Technical Requirements

- Use `juce::Timer` for periodic backups
- Use `juce::FileOutputStream` for non-blocking file writes
- Handle file system errors gracefully (disk full, permission denied)
- Log backup operations to event log
- Test with multiple rapid backups (ensure rotation works)
- Test crash recovery flow (simulate crash)

---

### Sprint 3: Recent Files (MRU) System (CRITICAL)

**Complexity:** Small (S)
**Estimated Duration:** 3-5 days
**Priority:** CRITICAL
**Dependencies:** Sprint 1 (Application Data Folder)

#### 3.1 Scope

**A. Recent Files Tracking**

Maintain Most Recently Used (MRU) list for sessions:

**Features:**

- Track last N sessions (configurable, default: 10)
- Store in application preferences
- Update on session save/load
- Remove invalid entries (deleted files) automatically
- Show in File menu as submenu

**Implementation:**

```cpp
// In SessionManager or new RecentFilesManager class
class RecentFilesManager {
public:
    RecentFilesManager(int maxFiles = 10);

    void addRecentFile(const juce::File& file);
    void removeRecentFile(const juce::File& file);
    void clearRecentFiles();

    std::vector<juce::File> getRecentFiles() const;

    // Save/load from preferences
    void saveToPreferences(juce::PropertiesFile& props);
    void loadFromPreferences(juce::PropertiesFile& props);

private:
    std::vector<juce::File> m_recentFiles;
    int m_maxFiles;

    void validateRecentFiles();  // Remove deleted files
};

void RecentFilesManager::addRecentFile(const juce::File& file) {
    // Remove if already in list (will re-add to front)
    m_recentFiles.erase(
        std::remove(m_recentFiles.begin(), m_recentFiles.end(), file),
        m_recentFiles.end()
    );

    // Add to front
    m_recentFiles.insert(m_recentFiles.begin(), file);

    // Trim to max size
    if (m_recentFiles.size() > m_maxFiles) {
        m_recentFiles.resize(m_maxFiles);
    }

    // Persist to preferences
    saveToPreferences(getApplicationProperties());
}

void RecentFilesManager::validateRecentFiles() {
    // Remove files that no longer exist
    m_recentFiles.erase(
        std::remove_if(m_recentFiles.begin(), m_recentFiles.end(),
            [](const juce::File& file) { return !file.existsAsFile(); }),
        m_recentFiles.end()
    );
}
```

**B. File Menu Integration**

Add "Load Recent Sessions" submenu to File menu:

**Implementation:**

```cpp
// In MainComponent or MenuBarModel
void MainComponent::getMenuBarNames(juce::StringArray& names) {
    names.add("File");
    // ...
}

juce::PopupMenu MainComponent::getMenuForIndex(int menuIndex,
                                                const juce::String& menuName) {
    juce::PopupMenu menu;

    if (menuName == "File") {
        menu.addItem(1, "New Session", true);
        menu.addItem(2, "Load Session...", true);

        // Recent Sessions submenu
        juce::PopupMenu recentMenu;
        auto recentFiles = recentFilesManager->getRecentFiles();

        if (recentFiles.empty()) {
            recentMenu.addItem(100, "(No recent files)", false);
        } else {
            int itemId = 100;
            for (const auto& file : recentFiles) {
                recentMenu.addItem(itemId++, file.getFileName(), true);
            }
        }

        menu.addSubMenu("Load Recent Sessions", recentMenu);

        menu.addItem(3, "Save Session", true);
        menu.addItem(4, "Save Session As...", true);
        // ...
    }

    return menu;
}

void MainComponent::menuItemSelected(int menuItemId, int topLevelMenuIndex) {
    if (menuItemId >= 100 && menuItemId < 200) {
        // Recent file selected
        int index = menuItemId - 100;
        auto recentFiles = recentFilesManager->getRecentFiles();

        if (index < recentFiles.size()) {
            sessionManager->loadSession(recentFiles[index]);
        }
    }
    // ... handle other menu items
}
```

**C. Preferences Persistence**

Store recent files list in application preferences:

**Format:**

```
recentFile1=/path/to/session1.occSession
recentFile2=/path/to/session2.occSession
recentFile3=/path/to/session3.occSession
...
```

**Implementation:**

```cpp
void RecentFilesManager::saveToPreferences(juce::PropertiesFile& props) {
    // Clear old entries
    for (int i = 0; i < 20; ++i) {  // Clear up to 20 old entries
        props.removeValue("recentFile" + juce::String(i + 1));
    }

    // Save current list
    for (int i = 0; i < m_recentFiles.size(); ++i) {
        props.setValue("recentFile" + juce::String(i + 1),
                       m_recentFiles[i].getFullPathName());
    }

    props.saveIfNeeded();
}

void RecentFilesManager::loadFromPreferences(juce::PropertiesFile& props) {
    m_recentFiles.clear();

    for (int i = 0; i < m_maxFiles; ++i) {
        auto path = props.getValue("recentFile" + juce::String(i + 1));
        if (path.isNotEmpty()) {
            juce::File file(path);
            if (file.existsAsFile()) {
                m_recentFiles.push_back(file);
            }
        }
    }
}
```

#### 3.2 Acceptance Criteria

- [ ] Recent files list tracks last N sessions (default: 10)
- [ ] Recent files list persists across app restarts
- [ ] "Load Recent Sessions" submenu shows recent files
- [ ] Clicking recent file loads session immediately
- [ ] Invalid entries (deleted files) are automatically removed
- [ ] Most recent file appears first in submenu
- [ ] Duplicate files don't appear in list (always moved to front)

#### 3.3 Technical Requirements

- Use `juce::PropertiesFile` for persistence
- Validate file existence on startup (remove stale entries)
- Update recent list on both save and load operations
- Handle edge cases (empty list, all files deleted)
- Cross-platform file paths (use absolute paths)

---

### Sprint 4: Missing File Resolution System (CRITICAL)

**Complexity:** Extra Large (XL)
**Estimated Duration:** 3-4 weeks
**Priority:** CRITICAL
**Dependencies:** Sprint 1 (Application Data Folder), Sprint 2 (Event Logs)

#### 4.1 Scope

**A. Missing File Detection on Load**

Detect missing audio files when loading session:

**Features:**

- Report all missing files with last known location
- Show "File not found" on affected clip buttons
- Log missing files to event log
- Option to "Try Remote Files" (search original locations)

**Implementation:**

```cpp
// In SessionManager::loadSession()
struct LoadReport {
    std::vector<ClipData> missingFiles;
    std::vector<ClipData> modifiedFiles;  // Timestamp mismatch
    bool success = true;
};

LoadReport SessionManager::loadSession(const juce::File& sessionFile) {
    LoadReport report;

    // ... parse JSON, load metadata ...

    // Validate audio files
    for (auto& clip : clips) {
        juce::File audioFile(clip.filePath);

        if (!audioFile.existsAsFile()) {
            // File not found
            clip.fileStatus = ClipData::FileStatus::Missing;
            report.missingFiles.push_back(clip);

            // Log to event log
            logEvent("File not found: Button " +
                     juce::String(clip.buttonIndex) + " - " +
                     clip.filePath);
        } else {
            // Check timestamp
            auto savedTimestamp = clip.fileTimestamp;  // Stored in session
            auto currentTimestamp = audioFile.getLastModificationTime();

            if (savedTimestamp != currentTimestamp.toMilliseconds()) {
                // File modified since session saved
                clip.fileStatus = ClipData::FileStatus::Modified;
                report.modifiedFiles.push_back(clip);

                logEvent("File timestamp mismatch: Button " +
                         juce::String(clip.buttonIndex) + " - " +
                         clip.filePath);
            }
        }
    }

    // Show load report dialog if issues found
    if (!report.missingFiles.empty() || !report.modifiedFiles.empty()) {
        showLoadReportDialog(report);
    }

    return report;
}
```

**B. Load Report Dialog**

Show detailed report of session loading issues:

**Features:**

- List all missing files with button numbers and paths
- List all modified files with timestamp differences
- Option to dismiss (continue with missing files)
- Option to launch "Locate File" wizard
- Copy report to event log for later review

**Implementation:**

```cpp
// New UI component: LoadReportDialog
class LoadReportDialog : public juce::Component {
public:
    LoadReportDialog(const LoadReport& report);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    LoadReport m_report;
    juce::TextEditor m_reportText;
    juce::TextButton m_okButton;
    juce::TextButton m_locateFilesButton;
};

void LoadReportDialog::LoadReportDialog(const LoadReport& report)
    : m_report(report) {

    // Build report text
    juce::String reportText;

    if (!report.missingFiles.empty()) {
        reportText << report.missingFiles.size() << " files not found\n\n";
        for (const auto& clip : report.missingFiles) {
            reportText << "Button " << clip.buttonIndex << " - "
                       << clip.filePath << "\n";
        }
    }

    if (!report.modifiedFiles.empty()) {
        reportText << "\n" << report.modifiedFiles.size()
                   << " files modified\n\n";
        for (const auto& clip : report.modifiedFiles) {
            reportText << "Button " << clip.buttonIndex << " - "
                       << clip.filePath << "\n";
            reportText << "  File is newer than that used previously\n";
        }
    }

    m_reportText.setText(reportText, false);
    m_reportText.setReadOnly(true);

    addAndMakeVisible(m_reportText);
    addAndMakeVisible(m_okButton);
    addAndMakeVisible(m_locateFilesButton);
}
```

**C. Locate File Wizard**

Step-by-step wizard for resolving missing files (modeled after SpotOn):

**Features:**

- **Step 1:** Browse to locate replacement file
- **Step 2:** Choose action:
  - 2a. Copy to original location (restore original structure)
  - 2b. Modify session to use replacement file in current location
  - 2c. Copy to new location and modify session
- **Step 3:** Reload session OR reload and select next missing file
- **Auto-locate:** If next missing file is in same folder, auto-fix it
- **Session preservation:** Option to save modified session to new file

**Implementation:** (See detailed SpotOn Locate File dialog in pages 13-19 of PDF)

```cpp
// New UI component: LocateFileWizard
class LocateFileWizard : public juce::Component {
public:
    LocateFileWizard(SessionManager* sessionManager, const ClipData& missingClip);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    enum Step { Browse, ChooseAction, ReloadSession };

    SessionManager* m_sessionManager;
    ClipData m_missingClip;
    juce::File m_replacementFile;
    juce::File m_newLocation;
    juce::File m_newSessionFile;

    Step m_currentStep = Step::Browse;

    // UI components
    juce::Label m_originalRemoteLocationLabel;
    juce::Label m_originalLocalLocationLabel;
    juce::Label m_replacementFileLabel;
    juce::Label m_newLocationLabel;
    juce::Label m_sessionFileLabel;

    juce::TextButton m_browseButton;
    juce::TextButton m_changeNewLocationButton;
    juce::TextButton m_changeSessionFileButton;

    juce::TextButton m_option2aButton;  // Copy to original location
    juce::TextButton m_option2bButton;  // Modify session (current location)
    juce::TextButton m_option2cButton;  // Copy to new location + modify

    juce::TextButton m_option3aButton;  // Reload session
    juce::TextButton m_option3bButton;  // Reload + select next button

    juce::Label m_hintLabel;  // Bottom hint panel

    void browseForReplacementFile();
    void chooseAction(int option);
    void reloadSession(bool selectNextMissing);

    void updateHint();
    void goToStep(Step step);
};
```

**D. File Timestamp Validation**

Store and validate file timestamps to detect modified files:

**Implementation:**

```cpp
// In ClipData struct (SessionManager.h)
struct ClipData {
    // ... existing fields ...

    int64_t fileTimestamp = 0;  // Milliseconds since epoch
    enum class FileStatus { Valid, Missing, Modified };
    FileStatus fileStatus = FileStatus::Valid;
};

// In SessionManager::loadClip()
bool SessionManager::loadClip(int buttonIndex, const juce::String& filePath) {
    ClipData clip;

    // ... extract metadata ...

    // Store file timestamp
    juce::File audioFile(filePath);
    clip.fileTimestamp = audioFile.getLastModificationTime().toMilliseconds();
    clip.fileStatus = ClipData::FileStatus::Valid;

    // ...
}

// In SessionManager::saveSession()
// Save fileTimestamp to JSON for each clip
clipObj->setProperty("fileTimestamp", clip.fileTimestamp);
```

#### 4.2 Acceptance Criteria

- [ ] Missing files detected on session load
- [ ] Load report dialog shows all missing files with button numbers
- [ ] "File not found" displayed on affected clip buttons
- [ ] Right-click menu on missing clip button shows "Locate File..." option
- [ ] Locate File wizard implements 3-step workflow (Browse, Choose Action, Reload)
- [ ] Option 2a: Copy replacement file to original location
- [ ] Option 2b: Modify session to use replacement file in current location
- [ ] Option 2c: Copy to new location and modify session
- [ ] Option 3b: Auto-locate remaining missing files in same folder
- [ ] File timestamp validation detects modified files
- [ ] Event log contains all file loading issues
- [ ] User can save modified session to alternate filename

#### 4.3 Technical Requirements

- Store file timestamps in session JSON (`fileTimestamp` field)
- Use `juce::File::getLastModificationTime()` for timestamp comparison
- Implement 3-step wizard UI with green progress bar (see SpotOn pages 14-17)
- Handle file copy operations with error handling
- Validate file paths before copying (check for duplicates)
- Test with moved files, deleted files, renamed files
- Test with network paths (remote locations)

---

### Sprint 5: Status Logs and Event Logging (HIGH)

**Complexity:** Medium (M)
**Estimated Duration:** 1-2 weeks
**Priority:** HIGH
**Dependencies:** Sprint 1 (Application Data Folder)

#### 5.1 Scope

**A. Status Log on Exit**

Save diagnostic status log when exiting via File > Exit:

**Features:**

- `~StatusLog.txt` saved to logs folder
- Contains: Session state, loaded clips, audio device info, errors
- Only saved on File > Exit (not window close button)
- Timestamped entries
- Use case: Support requests, debugging

**Implementation:**

```cpp
// In Application or MainComponent
void Application::exitViaFileMenu() {
    // Save status log
    saveStatusLog();

    // Normal shutdown
    quit();
}

void Application::saveStatusLog() {
    auto logFile = ApplicationPaths::getLogsFolder()
        .getChildFile("~StatusLog.txt");

    juce::FileOutputStream stream(logFile);

    if (stream.openedOk()) {
        stream.writeText("=== Orpheus Clip Composer Status Log ===\n",
                         false, false, nullptr);
        stream.writeText("Date: " + juce::Time::getCurrentTime()
                         .toString(true, true) + "\n\n", false, false, nullptr);

        // Session info
        stream.writeText("Session: " + sessionManager->getSessionName() + "\n",
                         false, false, nullptr);
        stream.writeText("Clip Count: " +
                         juce::String(sessionManager->getClipCount()) + "\n",
                         false, false, nullptr);

        // Audio device info
        stream.writeText("\nAudio Device: " +
                         audioEngine->getCurrentDeviceName() + "\n",
                         false, false, nullptr);
        stream.writeText("Sample Rate: " +
                         juce::String(audioEngine->getSampleRate()) + " Hz\n",
                         false, false, nullptr);
        stream.writeText("Buffer Size: " +
                         juce::String(audioEngine->getBufferSize()) + " samples\n",
                         false, false, nullptr);

        // Recent errors (if any)
        if (!recentErrors.empty()) {
            stream.writeText("\nRecent Errors:\n", false, false, nullptr);
            for (const auto& error : recentErrors) {
                stream.writeText("  " + error + "\n", false, false, nullptr);
            }
        }

        stream.writeText("\n=== End of Status Log ===\n",
                         false, false, nullptr);
    }
}
```

**B. Event Logging System**

Structured logging for session load/save events:

**Features:**

- Event log file: `events_YYYY-MM-DD.log` (daily rotation)
- Log entries: Timestamp, severity, component, message
- Severity levels: INFO, WARN, ERROR
- Components: Session, Audio, UI, Transport
- Log viewer UI (Info > Logs > Events)

**Implementation:**

```cpp
// New class: EventLogger
class EventLogger {
public:
    enum class Severity { Info, Warning, Error };
    enum class Component { Session, Audio, UI, Transport, System };

    static EventLogger& getInstance();

    void log(Severity severity, Component component, const juce::String& message);

    void logInfo(Component component, const juce::String& message) {
        log(Severity::Info, component, message);
    }

    void logWarning(Component component, const juce::String& message) {
        log(Severity::Warning, component, message);
    }

    void logError(Component component, const juce::String& message) {
        log(Severity::Error, component, message);
    }

    std::vector<LogEntry> getRecentLogs(int count = 100) const;

private:
    EventLogger();

    void rotateLogFile();
    juce::File getCurrentLogFile();

    juce::File m_currentLogFile;
    std::vector<LogEntry> m_recentLogs;  // In-memory cache
    juce::CriticalSection m_logLock;
};

void EventLogger::log(Severity severity, Component component,
                      const juce::String& message) {
    juce::ScopedLock lock(m_logLock);

    // Build log entry
    auto timestamp = juce::Time::getCurrentTime()
        .formatted("%Y-%m-%d %H:%M:%S");
    auto severityStr = (severity == Severity::Info) ? "INFO" :
                       (severity == Severity::Warning) ? "WARN" : "ERROR";
    auto componentStr = getComponentName(component);

    juce::String logEntry = "[" + timestamp + "] [" + severityStr + "] [" +
                            componentStr + "] " + message + "\n";

    // Write to file
    juce::FileOutputStream stream(getCurrentLogFile(), 256);
    if (stream.openedOk()) {
        stream.writeText(logEntry, false, false, nullptr);
    }

    // Add to in-memory cache
    m_recentLogs.push_back({timestamp, severity, component, message});
    if (m_recentLogs.size() > 1000) {
        m_recentLogs.erase(m_recentLogs.begin());
    }
}
```

**C. Log Viewer UI**

Display event logs in UI:

**Features:**

- Tabbed view: Events, Status
- Filter by severity (Info, Warning, Error)
- Filter by component (Session, Audio, UI, Transport)
- Search log entries
- Copy to clipboard
- Clear logs button

**Implementation:**

```cpp
// New UI component: LogViewerDialog
class LogViewerDialog : public juce::Component {
public:
    LogViewerDialog();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::TabbedComponent m_tabs;
    juce::TextEditor m_eventsLog;
    juce::TextEditor m_statusLog;

    juce::ComboBox m_severityFilter;
    juce::ComboBox m_componentFilter;
    juce::TextEditor m_searchBox;

    juce::TextButton m_copyButton;
    juce::TextButton m_clearButton;
    juce::TextButton m_closeButton;

    void loadEventLog();
    void loadStatusLog();
    void applyFilters();
};
```

#### 5.2 Acceptance Criteria

- [ ] Status log saved on File > Exit with session state, audio device info
- [ ] Event log records session load/save events with timestamps
- [ ] Event log uses severity levels (INFO, WARN, ERROR)
- [ ] Event log uses component categories (Session, Audio, UI, Transport)
- [ ] Log viewer UI shows events in tabbed view
- [ ] User can filter logs by severity and component
- [ ] User can search log entries
- [ ] User can copy logs to clipboard
- [ ] Daily log file rotation (events_YYYY-MM-DD.log)
- [ ] Status log only saved via File > Exit (not window close)

#### 5.3 Technical Requirements

- Use `juce::FileOutputStream` for log writes
- Thread-safe logging (use `juce::CriticalSection`)
- Daily log rotation (check date on each log write)
- Keep last N days of logs (default: 30 days)
- In-memory log cache for fast UI updates
- Handle log file write failures gracefully

---

## 3. Additional Backend Features (Medium Priority)

### Sprint 6: Track List Export (LOW)

**Complexity:** Small (S)
**Estimated Duration:** 2-3 days
**Priority:** LOW
**Dependencies:** None

**Features:**

- Export clip metadata to CSV/TXT files
- Spreadsheet-compatible format (MS Excel)
- Include: Button#, Track Name, Length, Trim In/Out, Gain, Clip Group, etc.

**Use Case:** Documentation, show planning, audit trails

---

### Sprint 7: Block Save/Load (MEDIUM)

**Complexity:** Medium (M)
**Estimated Duration:** 1 week
**Priority:** MEDIUM
**Dependencies:** Sprint 1

**Features:**

- Save consecutive button ranges to `.occBlock` file
- Load button blocks into existing session
- Merge dialog: Specify starting button index
- Use case: Reusable button sets (e.g., common sound effects)

---

### Sprint 8: Package System (HIGH - Future)

**Complexity:** Extra Large (XL)
**Estimated Duration:** 3-4 weeks
**Priority:** HIGH (Phase 2)
**Dependencies:** Sprint 1, Sprint 4

**Features:**

- Bundle session + audio files into `.occPackage` file
- Version checking (detect newer package versions)
- Mini packages (partial session import/export)
- UnPackage utility (offline audio extraction)
- CD/DVD burning integration (optional)

**Deferred Rationale:** Complex feature requiring compression, version migration, and extensive testing. Prioritize data safety (auto-backup, restore) first.

---

## 4. Implementation Roadmap

### Phase 1: Critical Safety Features (Weeks 1-6)

**Focus:** Data loss prevention, crash recovery, usability

1. **Sprint 1:** Session Management Foundation (2 weeks)
   - Application data folder structure
   - Session templates
   - File extension validation

2. **Sprint 2:** Auto-Backup and Restore System (3 weeks)
   - Auto-backup (timed, timestamped)
   - Restore from backup UI
   - Crash recovery

3. **Sprint 3:** Recent Files (MRU) System (1 week)
   - Recent files tracking
   - File menu integration
   - Preferences persistence

### Phase 2: Production Robustness (Weeks 7-12)

**Focus:** File management, diagnostics, logging

4. **Sprint 4:** Missing File Resolution System (4 weeks)
   - Missing file detection
   - Load report dialog
   - Locate File wizard
   - File timestamp validation

5. **Sprint 5:** Status Logs and Event Logging (2 weeks)
   - Status log on exit
   - Event logging system
   - Log viewer UI

### Phase 3: Workflow Enhancements (Weeks 13+)

**Focus:** Productivity, portability

6. **Sprint 7:** Block Save/Load (1 week)
7. **Sprint 6:** Track List Export (0.5 weeks)
8. **Sprint 8:** Package System (4 weeks) - Future

---

## 5. Cross-Cutting Concerns

### 5.1 Threading Model (from OCC096)

All backend operations must respect OCC's 3-thread model:

**Message Thread:**

- Session save/load (blocking I/O is OK)
- File dialogs (Browse, Save As)
- Auto-backup (background thread if needed)
- Preferences save/load

**Audio Thread:**

- **NO FILE I/O** (never read/write during audio callback)
- Audio engine queries are lock-free atomics (OK)

**Background Threads:**

- Auto-backup file writes (non-blocking)
- Log file writes (buffered)
- Missing file searches (optional)

### 5.2 Error Handling

All file operations must handle errors gracefully:

**Common Errors:**

- File not found (missing files)
- Permission denied (read-only folders)
- Disk full (auto-backup fails)
- Invalid JSON (corrupted session file)
- Network timeout (remote file paths)

**Error Handling Strategy:**

- Show user-friendly error dialogs
- Log errors to event log
- Provide recovery options (e.g., Locate File wizard)
- Never crash on file errors

### 5.3 Cross-Platform Compatibility

Test all features on:

- **macOS** (primary target for broadcast/theater)
- **Windows** (secondary target)
- **Linux** (optional, nice-to-have)

**Platform-Specific Concerns:**

- File paths (use JUCE cross-platform paths)
- Application data folder locations
- File permissions (handle read-only folders)

### 5.4 Performance

**Critical Metrics:**

- Auto-backup: <500ms for typical session (48 clips, 8 tabs)
- Session load: <2s for typical session
- Recent files list: <100ms to populate menu
- Log writes: <10ms per entry (buffered, non-blocking)

**Optimization:**

- Use background threads for auto-backup (don't block UI)
- Cache recent files list in memory (don't re-scan disk)
- Batch log writes (buffer in memory, flush periodically)

---

## 6. Testing Strategy

### 6.1 Unit Tests

**Test Coverage:**

- `ApplicationPaths` folder creation (all platforms)
- `SessionManager` load/save with missing files
- `RecentFilesManager` add/remove/validate
- `EventLogger` thread-safe logging
- File extension validation
- Timestamp comparison

### 6.2 Integration Tests

**Test Scenarios:**

- Auto-backup triggers after N minutes
- Crash recovery restores most recent backup
- Recent files list updates on save/load
- Missing file wizard relocates file and updates session
- Event log captures load/save errors
- Status log saved on File > Exit

### 6.3 Manual Testing

**Test Cases:**

- User loads session with missing files → Load report dialog appears
- User clicks "Locate File" → Wizard guides through 3 steps
- User exits via File > Exit → Status log saved
- User restarts after crash → Crash recovery dialog appears
- User saves session with invalid extension → Error dialog appears

---

## 7. Documentation Requirements

### 7.1 User Documentation

**Topics:**

- Session templates (how to create, use)
- Recent files list (how to clear, configure)
- Auto-backup (interval configuration, restore from backup)
- Missing file resolution (Locate File wizard walkthrough)
- Status logs (where to find, how to interpret)
- Event logs (how to view, filter, search)

### 7.2 Developer Documentation

**Topics:**

- Application data folder structure
- Session JSON schema (with `fileTimestamp` field)
- Event logging API (`EventLogger::log()`)
- Recent files manager API
- Threading model for file operations
- Error handling patterns

---

## 8. Dependencies on Other OCC Systems

### 8.1 OCC096 - SDK Integration Patterns

**Relevant Patterns:**

- Pattern 7: Session Save (Message Thread, I/O allowed)
- Threading model (Message/Audio/Background threads)

**Impact:** All file operations must follow threading rules (no audio thread I/O).

### 8.2 OCC097 - Session Format Specification

**Enhancements Required:**

- Add `fileTimestamp` field to clip metadata
- Add `templateMetadata` section for session templates
- Version field for migration (already specified, needs implementation)

**Impact:** Update session schema to v1.1.0 with new fields.

### 8.3 OCC111 - Gap Audit Report

**Alignment:**

- OCC111 identified: Auto-save, recent files, session integrity checks
- This sprint plan addresses all identified gaps

**Impact:** Closes critical gaps from OCC111.

---

## 9. Success Metrics

### 9.1 Data Safety

- **Zero data loss incidents** in production (24/7 operation)
- Auto-backup prevents loss from crashes (<5 minutes of work lost)
- Crash recovery rate: >95% (users successfully restore from backup)

### 9.2 Usability

- Recent files list reduces session load time by >50% (no file browser)
- Missing file wizard resolves >80% of missing files on first attempt
- Session templates reduce new session setup time by >60%

### 9.3 Diagnostics

- Status logs provide sufficient info for >90% of support requests
- Event logs capture 100% of file loading errors
- Log viewer UI used by >50% of users for troubleshooting

---

## 10. Related Documentation

- **OCC114** - Backend Infrastructure Audit (Current State)
- **OCC096** - SDK Integration Patterns (Threading Model)
- **OCC097** - Session Format Specification (JSON Schema)
- **OCC111** - Gap Audit Report (Missed Tasks)
- **SpotOn Manual Section 01** - File Menu (Peer Analysis)

---

**Last Updated:** 2025-11-12
**Maintainer:** OCC Development Team
**Status:** Planning
**Sprint Context:** Backend Architecture Sprint
