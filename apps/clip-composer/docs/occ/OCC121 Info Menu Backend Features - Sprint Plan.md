# OCC121 Info Menu Backend Features - Sprint Plan

**Status:** Draft
**Created:** 2025-11-12
**Sprint:** Backend Architecture Analysis (Part of OCC114)
**Dependencies:** OCC114-OCC120 (All previous menus)

---

## Executive Summary

Analysis of SpotOn Manual Section 08 (Info Menu) identifying backend requirements for session notes, system status display, and comprehensive activity logging (playout and event logs) for royalty tracking and debugging.

**Complexity:** Medium (logging infrastructure)
**Backend Components:** Session notes, status aggregation, dual logging system
**Database Requirements:** Medium (logs with metadata)
**API Endpoints:** ~12-15 endpoints

---

## 1. Feature Overview

The Info Menu provides information display and activity logging:

- **Session Notes:** Free-text notepad saved with session file
- **Status:** System information display (version, session, audio config)
- **Playout Logs:** Comprehensive track playback logging for royalty reporting
- **Event Logs:** System event logging (GPI, MIDI, file operations, errors)
- **Local Files:** File management (disabled feature - noted for completeness)

**Key Insight:** This menu is primarily about observability, auditability, and regulatory compliance (royalty logging).

---

## 2. Backend Architecture Requirements

### 2.1 Session Notes System

**Requirements:**

- Free-text notepad associated with session
- Auto-show on session load (if notes exist and option enabled)
- Clear notes functionality
- Save with session file

**Implementation:**

```typescript
interface SessionNotes {
  sessionId: string;
  content: string; // Free text
  autoShow: boolean; // Show on session load
  lastModified: Date;
}

class SessionNotesService {
  async getNotes(sessionId: string): Promise<SessionNotes> {
    // Load notes for session
  }

  async updateNotes(sessionId: string, content: string): Promise<void> {
    // Update notes content
  }

  async clearNotes(sessionId: string): Promise<void> {
    // Clear notes content
  }

  async setAutoShow(sessionId: string, autoShow: boolean): Promise<void> {
    // Set auto-show preference
  }
}
```

**Database Schema:**

```sql
CREATE TABLE session_notes (
  session_id TEXT PRIMARY KEY,
  content TEXT,
  auto_show BOOLEAN DEFAULT TRUE,
  last_modified TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (session_id) REFERENCES sessions(id)
);
```

### 2.2 System Status Aggregation

**Requirements:**

- Collect and display system information
- Version information
- Session metadata (start time, duration)
- Audio configuration
- Display settings
- Copy to clipboard (full or selection)

**Implementation:**

```typescript
interface SystemStatus {
  application: ApplicationInfo;
  system: SystemInfo;
  session: SessionInfo;
  audio: AudioInfo;
  display: DisplayInfo;
  performance: PerformanceInfo;
}

interface ApplicationInfo {
  version: string;
  buildDate: Date;
  runningWithDongle: boolean;
}

interface SystemInfo {
  operatingSystem: string;
  cpuCount: number;
  computerName: string;
  userName: string;
  ipAddress: string;
}

interface SessionInfo {
  startTime: Date;
  duration: number; // Seconds
  currentSessionFile: string;
}

interface AudioInfo {
  defaultWavFolder: string;
  usingDefaultFolder: boolean;
  wavEditorPath: string;
  allowableChannels: number;
  multiChannelMode: boolean;
  channelCount: number;
  channelPatching: string;
  frequencyEffects: string[];
  fadeLimit: number; // dB
  voiceOverFadeDepth: number; // dB
}

interface DisplayInfo {
  keyboardRepeatDelay: number;
  keyboardRepeatSpeed: number;
  horizontalPixelsPerInch: number;
  verticalPixelsPerInch: number;
  screenResolution: string;
}

interface PerformanceInfo {
  minLoadTime: number; // ms
  maxLoadTime: number; // ms
  avgLoadTime: number; // ms
}

class SystemStatusService {
  async getStatus(): Promise<SystemStatus> {
    return {
      application: await this.getApplicationInfo(),
      system: await this.getSystemInfo(),
      session: await this.getSessionInfo(),
      audio: await this.getAudioInfo(),
      display: await this.getDisplayInfo(),
      performance: await this.getPerformanceInfo(),
    };
  }

  async exportToClipboard(status: SystemStatus, selection?: string): Promise<void> {
    const text = selection || this.formatStatus(status);
    await navigator.clipboard.writeText(text);
  }
}
```

### 2.3 Playout Logging System

**Requirements:**

- Log every track playback with comprehensive metadata
- Track trigger source (mouse, hotkey, GPI, MIDI, etc.)
- Record start time, duration, track metadata
- Support royalty reporting requirements
- Archive daily (28 days in default folder, unlimited in custom folder)
- Export with column filtering and output filtering
- Support extended metadata columns

**Trigger Source Codes:**

```typescript
enum TriggerSource {
  AUDIO_DIALOG = 'a',
  POSITION_BAR = 'b',
  MOUSE_CLICK = 'c',
  PLAY_DELAY = 'd',
  FINE_TRIM = 'e',
  DTMF_MIDI = 'f',
  GPI = 'g',
  HOTKEY = 'h',
  TCP_MESSAGE = 'i',
  TCP_MIDI = 'j',
  TIMECODE_LIST = 'k',
  PLAY_NEXT = 'n',
  MIDI_LOOPBACK = 'o',
  PBUS_MESSAGE = 'p',
  WIRED_MIDI = 'q',
  MIDI_TEST = 'r',
  SLAVE = 's',
  TRIM_WINDOW = 't',
  UDP_MESSAGE = 'u',
  MIDI_UDP = 'v',
  START_TIME_ARCHIVED = 'x',
  AUTO_PLAY = 'y',
  WAV_PREVIEW = '#',
  CLICK_TRACK = '@',
  TIMECODE_STACK = '[',
  UNPAUSE = ']',
  PREVIEW = '(',
  CENTRE_CLICK = ')',
  EFFECT_SETUP = '~',
  OUTPUT_ASSIGN = '^',
}
```

**Implementation:**

```typescript
interface PlayoutLogEntry {
  id: string;
  timestamp: Date;
  itemNumber: number; // Sequential item number
  duration: string; // HH:MM:SS or ??:??:?? if still playing
  midiTimeCode: string | null; // Optional MIDI timecode
  buttonNumber: number;
  outputId: string;
  triggerSource: TriggerSource;
  trackName: string;
  fileName: string;

  // Extended metadata (optional columns)
  title?: string;
  artist?: string;
  album?: string;
  genre?: string;
  project?: string;
  supplier?: string;
  source?: string;
  software?: string;
  engineer?: string;
  copyright?: string;
  comments?: string;
  keywords?: string;
}

interface PlayoutLogFilter {
  outputs?: string[]; // Filter by outputs
  columns?: PlayoutLogColumn[]; // Columns to include
  startDate?: Date;
  endDate?: Date;
}

enum PlayoutLogColumn {
  ITEM = 'item',
  TIME = 'time',
  DURATION = 'duration',
  MIDI_TC = 'midi_tc',
  BUTTON = 'button',
  OUTPUT = 'output',
  DISPLAY_NAME = 'display_name',
  FILE_NAME = 'file_name',
  TITLE = 'title',
  ARTIST = 'artist',
  ALBUM = 'album',
  GENRE = 'genre',
  PROJECT = 'project',
  SUPPLIER = 'supplier',
  SOURCE = 'source',
  SOFTWARE = 'software',
  ENGINEER = 'engineer',
  COPYRIGHT = 'copyright',
  COMMENTS = 'comments',
  KEYWORDS = 'keywords',
}

class PlayoutLogService {
  async logPlayback(entry: Omit<PlayoutLogEntry, 'id' | 'timestamp'>): Promise<void> {
    // Add entry to playout log
    // Duration is set to ??:??:?? initially
  }

  async updateDuration(entryId: string, duration: number): Promise<void> {
    // Update duration when track stops
  }

  async getLog(date: Date, filter?: PlayoutLogFilter): Promise<PlayoutLogEntry[]> {
    // Get log entries for date
    // Apply filters
  }

  async exportLog(
    entries: PlayoutLogEntry[],
    filter: PlayoutLogFilter,
    format: 'csv' | 'txt',
  ): Promise<string> {
    // Export filtered columns to file
  }

  async archiveLog(date: Date): Promise<void> {
    // Archive log to file
    // Maintain 28-day limit in default folder
  }

  async getArchivedLogs(): Promise<Date[]> {
    // Get list of archived log dates
  }

  async loadArchivedLog(date: Date): Promise<PlayoutLogEntry[]> {
    // Load archived log from file
  }
}
```

**Database Schema:**

```sql
CREATE TABLE playout_log (
  id TEXT PRIMARY KEY,
  timestamp TIMESTAMP NOT NULL,
  item_number INTEGER,
  duration TEXT DEFAULT '??:??:??',    -- HH:MM:SS or ??:??:??
  midi_time_code TEXT,
  button_number INTEGER NOT NULL,
  output_id TEXT NOT NULL,
  trigger_source TEXT NOT NULL,
  track_name TEXT NOT NULL,
  file_name TEXT NOT NULL,

  -- Extended metadata
  title TEXT,
  artist TEXT,
  album TEXT,
  genre TEXT,
  project TEXT,
  supplier TEXT,
  source TEXT,
  software TEXT,
  engineer TEXT,
  copyright TEXT,
  comments TEXT,
  keywords TEXT,

  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Indices for performance
CREATE INDEX idx_playout_log_timestamp ON playout_log(timestamp);
CREATE INDEX idx_playout_log_date ON playout_log(DATE(timestamp));
CREATE INDEX idx_playout_log_button ON playout_log(button_number);
CREATE INDEX idx_playout_log_output ON playout_log(output_id);

-- Table for tracking currently playing (for duration updates)
CREATE TABLE playout_active (
  log_entry_id TEXT PRIMARY KEY,
  button_number INTEGER NOT NULL,
  start_time TIMESTAMP NOT NULL,
  FOREIGN KEY (log_entry_id) REFERENCES playout_log(id)
);
```

### 2.4 Event Logging System

**Requirements:**

- Log system events (GPI, MIDI, file operations, errors)
- Separate from playout logging
- Archive daily (28 days retention)
- Export capabilities

**Implementation:**

```typescript
interface EventLogEntry {
  id: string;
  timestamp: Date;
  eventType: EventType;
  action: string;
  details?: string;
}

enum EventType {
  STARTUP = 'startup',
  SHUTDOWN = 'shutdown',
  SESSION_LOADED = 'session_loaded',
  SESSION_SAVED = 'session_saved',
  PACKAGE_LOADED = 'package_loaded',
  FILE_LOADED = 'file_loaded',
  GPI_IN = 'gpi_in',
  GPI_OUT = 'gpi_out',
  MIDI_IN = 'midi_in',
  MIDI_OUT = 'midi_out',
  ERROR = 'error',
  WARNING = 'warning',
  SYSTEM = 'system',
}

class EventLogService {
  async logEvent(eventType: EventType, action: string, details?: string): Promise<void> {
    // Add entry to event log
  }

  async getLog(date: Date): Promise<EventLogEntry[]> {
    // Get event log for date
  }

  async exportLog(entries: EventLogEntry[], format: 'csv' | 'txt'): Promise<string> {
    // Export to file
  }

  async archiveLog(date: Date): Promise<void> {
    // Archive log to file
  }
}
```

**Database Schema:**

```sql
CREATE TABLE event_log (
  id TEXT PRIMARY KEY,
  timestamp TIMESTAMP NOT NULL,
  event_type TEXT NOT NULL,
  action TEXT NOT NULL,
  details TEXT,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_event_log_timestamp ON event_log(timestamp);
CREATE INDEX idx_event_log_date ON event_log(DATE(timestamp));
CREATE INDEX idx_event_log_type ON event_log(event_type);
```

### 2.5 Log Archival System

**Requirements:**

- Daily log files (one per day)
- Automatic archival on date rollover
- 28-day retention in default folder
- Unlimited retention in custom folder
- Background archival process

**Implementation:**

```typescript
interface LogArchivalConfig {
  defaultFolder: string;
  customFolder: string | null;
  retentionDays: number; // 28 for default folder
}

class LogArchivalService {
  async archiveDailyLogs(): Promise<void> {
    // Run at midnight or on startup
    const yesterday = new Date();
    yesterday.setDate(yesterday.getDate() - 1);

    await this.playoutLogService.archiveLog(yesterday);
    await this.eventLogService.archiveLog(yesterday);

    await this.cleanupOldLogs();
  }

  async cleanupOldLogs(): Promise<void> {
    // Delete logs older than retention period
    // Only in default folder, not custom folder
    const cutoffDate = new Date();
    cutoffDate.setDate(cutoffDate.getDate() - 28);

    await this.deleteLogsOlderThan(cutoffDate, 'default');
  }

  async getArchivedLogDates(): Promise<Date[]> {
    // Get list of available archived logs
  }
}
```

---

## 3. API Contracts

### 3.1 Session Notes Endpoints

```typescript
// GET /api/info/session-notes/:sessionId
// Get session notes
interface GetSessionNotesResponse {
  notes: SessionNotes;
}

// PUT /api/info/session-notes/:sessionId
// Update session notes
interface UpdateSessionNotesRequest {
  content: string;
  autoShow?: boolean;
}

// DELETE /api/info/session-notes/:sessionId
// Clear session notes
interface ClearSessionNotesResponse {
  success: boolean;
}
```

### 3.2 System Status Endpoints

```typescript
// GET /api/info/status
// Get system status
interface GetSystemStatusResponse {
  status: SystemStatus;
}

// POST /api/info/status/export
// Export status to clipboard
interface ExportStatusRequest {
  selection?: string; // Optional selected text
}

interface ExportStatusResponse {
  success: boolean;
}
```

### 3.3 Playout Log Endpoints

```typescript
// POST /api/info/playout-log
// Add playout log entry
interface AddPlayoutLogRequest {
  buttonNumber: number;
  outputId: string;
  triggerSource: TriggerSource;
  trackName: string;
  fileName: string;
  metadata?: Partial<PlayoutLogEntry>;
}

interface AddPlayoutLogResponse {
  entryId: string;
}

// PUT /api/info/playout-log/:entryId/duration
// Update duration when track stops
interface UpdatePlayoutDurationRequest {
  duration: number; // Seconds
}

// GET /api/info/playout-log/current
// Get current playout log
interface GetCurrentPlayoutLogResponse {
  entries: PlayoutLogEntry[];
}

// GET /api/info/playout-log/archived
// Get list of archived logs
interface GetArchivedLogsResponse {
  dates: Date[];
}

// GET /api/info/playout-log/:date
// Get log for specific date
interface GetPlayoutLogRequest {
  date: string; // ISO date
  filter?: PlayoutLogFilter;
}

interface GetPlayoutLogResponse {
  entries: PlayoutLogEntry[];
}

// POST /api/info/playout-log/export
// Export log
interface ExportPlayoutLogRequest {
  entries: string[]; // Entry IDs
  filter: PlayoutLogFilter;
  format: 'csv' | 'txt';
}

interface ExportPlayoutLogResponse {
  filePath: string;
  fileSize: number;
}

// GET /api/info/playout-log/columns
// Get available extended columns
interface GetPlayoutColumnsResponse {
  availableColumns: PlayoutLogColumn[];
  visibleColumns: PlayoutLogColumn[];
}

// PUT /api/info/playout-log/columns
// Set visible columns
interface SetPlayoutColumnsRequest {
  columns: PlayoutLogColumn[];
}
```

### 3.4 Event Log Endpoints

```typescript
// POST /api/info/event-log
// Add event log entry
interface AddEventLogRequest {
  eventType: EventType;
  action: string;
  details?: string;
}

// GET /api/info/event-log/current
// Get current event log
interface GetCurrentEventLogResponse {
  entries: EventLogEntry[];
}

// GET /api/info/event-log/:date
// Get log for specific date
interface GetEventLogResponse {
  entries: EventLogEntry[];
}

// POST /api/info/event-log/export
// Export event log
interface ExportEventLogRequest {
  entries: string[];
  format: 'csv' | 'txt';
}

interface ExportEventLogResponse {
  filePath: string;
}
```

### 3.5 WebSocket Events

```typescript
// Real-time playout log updates
interface PlayoutLogUpdate {
  event: 'playout-log:entry';
  data: PlayoutLogEntry;
}

// Real-time event log updates
interface EventLogUpdate {
  event: 'event-log:entry';
  data: EventLogEntry;
}

// Duration updates for playing tracks
interface PlayoutDurationUpdate {
  event: 'playout-log:duration';
  data: {
    entryId: string;
    duration: string;
  };
}
```

---

## 4. Integration Points

### 4.1 Session Management Integration

- Session notes loaded with session
- Session notes saved with session
- Auto-show on session load (if enabled and non-empty)

### 4.2 Audio Engine Integration

- Playout log entry created on button play
- Trigger source captured from play command
- Duration updated on button stop
- Handle long-running/looping tracks (delayed duration update)

### 4.3 External Control Integration

- Capture trigger source for every playback:
  - Mouse clicks (OCC120 mouse function settings)
  - Hotkeys (OCC120)
  - GPI (OCC116)
  - MIDI (OCC116)
  - SMPTE timecode (OCC120)
  - Master/Slave links (OCC119)
  - PlayNext (OCC117)
  - And 20+ other sources

### 4.4 Metadata Integration

- Extract track metadata for playout log
- Support extended metadata columns (title, artist, album, etc.)
- WAV file metadata (from OCC118)

### 4.5 File System Integration

- Daily log archival
- 28-day retention management
- Custom folder support (unlimited retention)

---

## 5. Implementation Priorities

### Phase 1: Session Notes (Sprint 1)

**Priority:** P1

**Tasks:**

1. Implement session notes data model
2. Create session notes CRUD API
3. Add auto-show logic
4. Integrate with session save/load

**Estimated Effort:** 6-8 hours

### Phase 2: System Status (Sprint 1)

**Priority:** P2

**Tasks:**

1. Implement status aggregation service
2. Collect system information
3. Add clipboard export
4. Create status API endpoint

**Estimated Effort:** 8-12 hours

### Phase 3: Playout Logging Infrastructure (Sprint 2-3)

**Priority:** P0 (Critical for royalty compliance)

**Tasks:**

1. Implement playout log data model
2. Create playout log service
3. Add trigger source tracking
4. Implement duration tracking
5. Add database schema
6. Create log entry API

**Estimated Effort:** 20-24 hours

### Phase 4: Playout Log Extended Features (Sprint 3-4)

**Priority:** P1

**Tasks:**

1. Add extended metadata columns
2. Implement column selection
3. Add output filtering
4. Create export functionality
5. Add archived log viewing

**Estimated Effort:** 16-20 hours

### Phase 5: Event Logging (Sprint 4)

**Priority:** P1

**Tasks:**

1. Implement event log data model
2. Create event log service
3. Add event logging hooks
4. Create export functionality

**Estimated Effort:** 12-16 hours

### Phase 6: Log Archival (Sprint 4-5)

**Priority:** P1

**Tasks:**

1. Implement daily archival process
2. Add retention management
3. Create archived log loading
4. Add custom folder support
5. Implement cleanup process

**Estimated Effort:** 12-16 hours

---

## 6. Technical Challenges

### 6.1 Duration Tracking for Long/Looping Tracks

**Challenge:**
Tracks can play for hours or loop indefinitely, making duration calculation complex.

**Solution:**

- Log entry created with duration = "??:??:??"
- Track active playback in separate table
- Update duration on stop or periodically
- Engineering menu option to delay archival until all tracks stopped

```typescript
class PlayoutDurationTracker {
  private activePlaybacks: Map<string, Date> = new Map();

  async startTracking(entryId: string, buttonNumber: number): Promise<void> {
    this.activePlaybacks.set(entryId, new Date());
  }

  async stopTracking(entryId: string): Promise<void> {
    const startTime = this.activePlaybacks.get(entryId);
    if (startTime) {
      const duration = (new Date().getTime() - startTime.getTime()) / 1000;
      await this.playoutLogService.updateDuration(entryId, duration);
      this.activePlaybacks.delete(entryId);
    }
  }

  async getActiveDurations(): Promise<Record<string, number>> {
    const now = new Date();
    const durations: Record<string, number> = {};

    for (const [entryId, startTime] of this.activePlaybacks) {
      durations[entryId] = (now.getTime() - startTime.getTime()) / 1000;
    }

    return durations;
  }
}
```

**Risk:** Medium
**Mitigation:** Active tracking table, periodic duration updates, delayed archival option

### 6.2 Log Archival Performance

**Challenge:**
Daily archival of potentially thousands of log entries.

**Solution:**

- Background archival process
- Incremental archival (archive throughout day)
- Batch operations
- Index optimization

```typescript
class IncrementalArchivalService {
  private archivalQueue: PlayoutLogEntry[] = [];
  private maxQueueSize = 100;

  async queueForArchival(entry: PlayoutLogEntry): Promise<void> {
    this.archivalQueue.push(entry);

    if (this.archivalQueue.length >= this.maxQueueSize) {
      await this.flushQueue();
    }
  }

  async flushQueue(): Promise<void> {
    if (this.archivalQueue.length === 0) return;

    // Write batch to archive file
    await this.writeToArchive(this.archivalQueue);
    this.archivalQueue = [];
  }
}
```

**Risk:** Low
**Mitigation:** Background processing, batching, index optimization

### 6.3 Extended Metadata Performance

**Challenge:**
Loading extended metadata for thousands of log entries.

**Solution:**

- Lazy loading of extended columns
- Only load when columns are visible
- Cache metadata in log entry
- Index on metadata fields

**Risk:** Low
**Mitigation:** Lazy loading, caching, indexing

---

## 7. Testing Strategy

```typescript
describe('PlayoutLogService', () => {
  test('logs playback with trigger source', async () => {
    const entryId = await playoutLog.logPlayback({
      buttonNumber: 1,
      outputId: 'A',
      triggerSource: TriggerSource.HOTKEY,
      trackName: 'Test Track',
      fileName: 'test.wav'
    });

    const entry = await playoutLog.getEntry(entryId);
    expect(entry.triggerSource).toBe('h');
    expect(entry.duration).toBe('??:??:??');
  });

  test('updates duration on stop', async () => {
    const entryId = await playoutLog.logPlayback({...});
    await new Promise(resolve => setTimeout(resolve, 2000));
    await playoutLog.updateDuration(entryId, 2);

    const entry = await playoutLog.getEntry(entryId);
    expect(entry.duration).toBe('00:00:02');
  });

  test('filters by output', async () => {
    // Log playbacks to different outputs
    // Filter by output A
    const entries = await playoutLog.getLog(new Date(), {
      outputs: ['A']
    });
    expect(entries.every(e => e.outputId === 'A')).toBe(true);
  });

  test('exports with column selection', async () => {
    const filePath = await playoutLog.exportLog(entries, {
      columns: [
        PlayoutLogColumn.TIME,
        PlayoutLogColumn.TRACK_NAME,
        PlayoutLogColumn.ARTIST
      ]
    }, 'csv');

    const content = await readFile(filePath);
    expect(content).toContain('Time,Track Name,Artist');
  });
});

describe('LogArchivalService', () => {
  test('archives daily logs', async () => {
    // Add entries
    await archival.archiveDailyLogs();

    // Verify archived
    const dates = await archival.getArchivedLogDates();
    expect(dates.length).toBeGreaterThan(0);
  });

  test('maintains 28-day retention', async () => {
    // Create logs older than 28 days
    await archival.cleanupOldLogs();

    // Verify old logs deleted
    const dates = await archival.getArchivedLogDates();
    expect(dates.every(d =>
      d > new Date(Date.now() - 28 * 24 * 60 * 60 * 1000)
    )).toBe(true);
  });
});
```

---

## 8. Sprint Summary

**Total Estimated Effort:** 74-96 hours

**Critical Path:**

1. Playout Logging Infrastructure → Extended Features → Archival
2. Event Logging → Archival
3. Session Notes (parallel)
4. System Status (parallel)

**Key Deliverables:**

- Session notes system
- System status aggregation
- Comprehensive playout logging (royalty compliance)
- Event logging
- Log archival system

**Regulatory Compliance:**

- Playout logs meet royalty reporting requirements
- 28-day minimum retention
- Comprehensive metadata tracking
- Trigger source tracking

---

## Appendix: SpotOn Manual References

**Source:** SpotOn Manual - Section 08 - Info Menu

**Key Pages:**

- Page 1: Session Notes
- Page 2: System Status display
- Pages 3-7: Playout and Event Logs
- Page 4: Trigger source codes (26 different sources)
- Pages 6-7: Extended metadata columns, archival, export

---

**Document Prepared By:** Backend Architecture Analysis Sprint
**Next Steps:** Analyze Section 09 (Engineering Menu) - OCC122
