# OCC118 Search Menu Backend Features - Sprint Plan

**Status:** Draft
**Created:** 2025-11-12
**Sprint:** Backend Architecture Analysis (Part of OCC114)
**Dependencies:** OCC114 (Backend Audit), OCC115 (File Menu), OCC116 (Setup Menu), OCC117 (Display/Edit Menu)

---

## Executive Summary

Analysis of SpotOn Manual Section 05 (Search Menu) identifying backend features required for recent file search, metadata-based filtering, sorting, and track preview functionality.

**Complexity:** Medium (search/filter logic, metadata parsing, preview routing)
**Backend Components:** 6 core features
**Database Requirements:** Medium (recent files history, metadata cache)
**API Endpoints:** ~8-10 endpoints

---

## Table of Contents

1. [Feature Overview](#1-feature-overview)
2. [Backend Architecture Requirements](#2-backend-architecture-requirements)
3. [Data Models](#3-data-models)
4. [API Contracts](#4-api-contracts)
5. [State Management](#5-state-management)
6. [Implementation Priorities](#6-implementation-priorities)
7. [Technical Challenges](#7-technical-challenges)
8. [Integration Points](#8-integration-points)

---

## 1. Feature Overview

### 1.1 Search Menu Purpose

The Search Menu provides:
- Quick access to 1000 most recent files loaded into SpotOn
- Real-time text-based search/filtering
- Multiple sorting options (name, date)
- Metadata-based highlighting (copyright, comments)
- Track preview functionality
- Remote file location support
- Drag-and-drop to main window buttons

### 1.2 Core Features Identified

| Feature | Description | Backend Complexity |
|---------|-------------|-------------------|
| Recent Files List | Track 1000 most recent loaded files | Medium |
| Real-time Search | Filter files by text search | Low |
| Multi-criteria Sorting | Sort by name or date | Low |
| Remote Files Support | Include/exclude remote locations | Medium |
| WAV Metadata Parsing | Extract copyright/comment fields | Medium |
| Track Preview | Route preview to assigned output | Medium |

---

## 2. Backend Architecture Requirements

### 2.1 Recent Files Tracking System

**Requirements:**
- Maintain circular buffer of 1000 most recent files
- Track load timestamp for recency sorting
- Store both disk filename and display name
- Persist across application sessions
- Fast insertion (O(1) for new entries)
- Efficient querying (O(n) for search, O(n log n) for sort)

**Implementation Approach:**
```typescript
interface RecentFileEntry {
  id: string;                    // UUID
  diskFilename: string;          // Original filename on disk
  displayName: string;           // User-defined display name (if altered)
  filePath: string;              // Full absolute path
  loadedAt: Date;                // Timestamp when loaded
  isRemote: boolean;             // Remote vs local location
  fileSize: number;              // Bytes
  duration: number;              // Samples
  sampleRate: number;            // Hz
  channels: number;              // Channel count
  format: string;                // WAV, MP3, etc.
  metadata?: WavMetadata;        // Optional WAV metadata
}

interface WavMetadata {
  copyright?: string;            // Copyright field
  comment?: string;              // Comment field
  artist?: string;               // Artist field
  creationDate?: Date;           // Creation date
  [key: string]: any;            // Other RIFF/BWF fields
}
```

**Database Schema:**
```sql
CREATE TABLE recent_files (
  id TEXT PRIMARY KEY,
  disk_filename TEXT NOT NULL,
  display_name TEXT NOT NULL,
  file_path TEXT NOT NULL,
  loaded_at TIMESTAMP NOT NULL,
  is_remote BOOLEAN DEFAULT FALSE,
  file_size INTEGER,
  duration INTEGER,
  sample_rate INTEGER,
  channels INTEGER,
  format TEXT,
  metadata_json TEXT,           -- JSON serialized metadata
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_loaded_at (loaded_at DESC),
  INDEX idx_display_name (display_name COLLATE NOCASE)
);

-- Maintain 1000 most recent entries
CREATE TRIGGER maintain_recent_files_limit
AFTER INSERT ON recent_files
BEGIN
  DELETE FROM recent_files
  WHERE id NOT IN (
    SELECT id FROM recent_files
    ORDER BY loaded_at DESC
    LIMIT 1000
  );
END;
```

### 2.2 Search and Filter Engine

**Requirements:**
- Case-insensitive substring search
- Real-time filtering (< 50ms response)
- Search across display names only (not full path)
- Partial match highlighting
- Result ranking by relevance

**Implementation:**
```typescript
interface SearchCriteria {
  query: string;                 // Search text
  includeRemote: boolean;        // Include remote files
  highlightWithComments: boolean; // Highlight files with comments
  highlightWithCopyright: boolean; // Highlight files with copyright
  sortBy: 'name' | 'date';       // Sort order
}

interface SearchResult {
  file: RecentFileEntry;
  matchScore: number;            // Relevance score (0-1)
  matchIndices: [number, number][]; // Match positions for highlighting
  displayColor: 'default' | 'red' | 'blue'; // Text color in UI
}

class RecentFileSearchEngine {
  search(criteria: SearchCriteria): SearchResult[] {
    // 1. Filter by query (case-insensitive substring)
    // 2. Filter by remote inclusion
    // 3. Apply sorting
    // 4. Determine display colors
    // 5. Calculate match scores
    // 6. Return sorted results
  }
}
```

### 2.3 WAV Metadata Parser

**Requirements:**
- Parse RIFF/BWF metadata chunks
- Extract standard fields (copyright, comment, artist, date)
- Cache metadata to avoid re-parsing
- Handle corrupted/incomplete metadata gracefully
- Support WAV files only (per manual)

**Implementation:**
```typescript
interface RiffChunk {
  id: string;                    // FourCC chunk ID
  size: number;                  // Chunk size
  data: Buffer;                  // Chunk data
}

class WavMetadataParser {
  parseFile(filePath: string): WavMetadata;
  extractChunk(buffer: Buffer, chunkId: string): RiffChunk | null;
  parseListInfo(data: Buffer): Record<string, string>;
  parseBext(data: Buffer): BextMetadata; // Broadcast Wave metadata
}

// Standard RIFF INFO chunk fields
const INFO_CHUNK_FIELDS = {
  'ICOP': 'copyright',           // Copyright
  'ICMT': 'comment',             // Comment
  'IART': 'artist',              // Artist
  'ICRD': 'creationDate',        // Creation date
  'IGNR': 'genre',               // Genre
  'INAM': 'title',               // Title
  // ... other standard fields
};
```

### 2.4 Track Preview System

**Requirements:**
- Route preview to designated preview output
- Support double-click to play, right-click to stop
- Stop preview when search window closes
- Use preview output assignment from Global menu
- Handle preview conflicts (only one preview at a time)

**Implementation:**
```typescript
interface PreviewRequest {
  fileId: string;                // Recent file ID
  outputId: string;              // Preview output assignment
  startPosition?: number;        // Optional start position (samples)
}

class TrackPreviewService {
  private currentPreview: string | null = null;

  async startPreview(request: PreviewRequest): Promise<void> {
    // 1. Stop current preview if active
    // 2. Load file into preview player
    // 3. Route to preview output
    // 4. Start playback
    // 5. Track current preview
  }

  async stopPreview(): Promise<void> {
    // 1. Stop playback
    // 2. Clear current preview
    // 3. Release preview player resources
  }

  isPreviewActive(fileId: string): boolean {
    return this.currentPreview === fileId;
  }
}
```

### 2.5 Remote File Handling

**Requirements:**
- Detect remote vs local file paths
- Optional inclusion via "Try Remote Files" toggle
- Display remote files in distinct color (red)
- Validate remote file accessibility
- Handle network timeouts gracefully

**Implementation:**
```typescript
class RemoteFileDetector {
  isRemotePath(path: string): boolean {
    // Check for UNC paths (\\server\share)
    // Check for network drives
    // Check for URL schemes (ftp://, http://, etc.)
  }

  async validateRemoteAccess(path: string): Promise<boolean> {
    // Attempt to stat file with timeout
    // Return accessibility status
  }
}

interface RemoteFileOptions {
  includeRemote: boolean;        // Include remote files in search
  timeout: number;               // Network timeout (ms)
  retryAttempts: number;         // Retry count for failed access
}
```

### 2.6 Drag-and-Drop Integration

**Requirements:**
- Support drag from search list to main window buttons
- Support Alt+Drop for automatic Top/Tail
- Provide file metadata for drop operation
- Coordinate with Edit Menu Top/Tail feature (OCC117)

**Implementation:**
```typescript
interface DragPayload {
  fileId: string;                // Recent file ID
  filePath: string;              // Full path for loading
  displayName: string;           // Display name
  autoTopTail: boolean;          // Alt key pressed during drop
}

class SearchDragDropService {
  createDragPayload(
    file: RecentFileEntry,
    modifiers: { alt: boolean }
  ): DragPayload {
    return {
      fileId: file.id,
      filePath: file.filePath,
      displayName: file.displayName,
      autoTopTail: modifiers.alt
    };
  }

  async handleDrop(
    payload: DragPayload,
    targetButtonId: string
  ): Promise<void> {
    // 1. Load file from path
    // 2. Apply Top/Tail if requested
    // 3. Assign to target button
    // 4. Update recent files timestamp
  }
}
```

---

## 3. Data Models

### 3.1 Core Entities

```typescript
// Recent File Entry (primary model)
interface RecentFileEntry {
  id: string;
  diskFilename: string;
  displayName: string;
  filePath: string;
  loadedAt: Date;
  isRemote: boolean;
  fileSize: number;
  duration: number;
  sampleRate: number;
  channels: number;
  format: string;
  metadata?: WavMetadata;
}

// WAV Metadata
interface WavMetadata {
  copyright?: string;
  comment?: string;
  artist?: string;
  creationDate?: Date;
  title?: string;
  genre?: string;
  // BWF-specific fields
  description?: string;
  originator?: string;
  originatorReference?: string;
  timeReference?: number;        // Samples since midnight
  codingHistory?: string;
}

// Search State
interface SearchState {
  query: string;
  results: SearchResult[];
  sortBy: 'name' | 'date';
  includeRemote: boolean;
  highlightComments: boolean;
  highlightCopyright: boolean;
  totalFiles: number;
  filteredCount: number;
}

// Preview State
interface PreviewState {
  activeFileId: string | null;
  outputId: string | null;
  isPlaying: boolean;
  position: number;              // Current position (samples)
  duration: number;              // Total duration (samples)
}
```

### 3.2 Database Schema

```sql
-- Recent files table (main storage)
CREATE TABLE recent_files (
  id TEXT PRIMARY KEY,
  disk_filename TEXT NOT NULL,
  display_name TEXT NOT NULL,
  file_path TEXT NOT NULL UNIQUE,
  loaded_at TIMESTAMP NOT NULL,
  is_remote BOOLEAN DEFAULT FALSE,
  file_size INTEGER,
  duration INTEGER,
  sample_rate INTEGER,
  channels INTEGER,
  format TEXT,
  metadata_json TEXT,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Indices for performance
CREATE INDEX idx_recent_files_loaded_at ON recent_files(loaded_at DESC);
CREATE INDEX idx_recent_files_display_name ON recent_files(display_name COLLATE NOCASE);
CREATE INDEX idx_recent_files_is_remote ON recent_files(is_remote);

-- Full-text search index (optional, for advanced search)
CREATE VIRTUAL TABLE recent_files_fts USING fts5(
  display_name,
  disk_filename,
  content='recent_files',
  content_rowid='rowid'
);

-- Triggers to maintain FTS index
CREATE TRIGGER recent_files_fts_insert AFTER INSERT ON recent_files BEGIN
  INSERT INTO recent_files_fts(rowid, display_name, disk_filename)
  VALUES (new.rowid, new.display_name, new.disk_filename);
END;

CREATE TRIGGER recent_files_fts_delete AFTER DELETE ON recent_files BEGIN
  DELETE FROM recent_files_fts WHERE rowid = old.rowid;
END;

CREATE TRIGGER recent_files_fts_update AFTER UPDATE ON recent_files BEGIN
  UPDATE recent_files_fts
  SET display_name = new.display_name, disk_filename = new.disk_filename
  WHERE rowid = new.rowid;
END;

-- Metadata cache table (optional, for pre-parsed metadata)
CREATE TABLE wav_metadata_cache (
  file_path TEXT PRIMARY KEY,
  metadata_json TEXT NOT NULL,
  file_modified_at TIMESTAMP,
  cached_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_metadata_cache_modified ON wav_metadata_cache(file_modified_at);
```

---

## 4. API Contracts

### 4.1 REST Endpoints

```typescript
// GET /api/search/recent-files
// Get recent files with optional search/filter
interface GetRecentFilesRequest {
  query?: string;                // Optional search query
  sortBy?: 'name' | 'date';      // Sort order (default: 'name')
  includeRemote?: boolean;       // Include remote files (default: false)
  highlightComments?: boolean;   // Highlight files with comments (default: false)
  highlightCopyright?: boolean;  // Highlight files with copyright (default: false)
  limit?: number;                // Max results (default: 1000)
  offset?: number;               // Pagination offset (default: 0)
}

interface GetRecentFilesResponse {
  files: SearchResult[];
  total: number;
  filtered: number;
  query: string;
}

// POST /api/search/recent-files
// Add a file to recent files history
interface AddRecentFileRequest {
  filePath: string;
  displayName?: string;          // Optional display name
}

interface AddRecentFileResponse {
  file: RecentFileEntry;
  isNew: boolean;                // Whether this is a new entry
}

// GET /api/search/recent-files/:id
// Get a specific recent file by ID
interface GetRecentFileResponse {
  file: RecentFileEntry;
}

// DELETE /api/search/recent-files/:id
// Remove a file from recent files
interface DeleteRecentFileResponse {
  success: boolean;
}

// GET /api/search/metadata/:fileId
// Get metadata for a specific file
interface GetMetadataResponse {
  metadata: WavMetadata;
  cached: boolean;               // Whether from cache
  parsedAt: Date;
}

// POST /api/search/preview/start
// Start track preview
interface StartPreviewRequest {
  fileId: string;
  outputId: string;              // Preview output assignment
  startPosition?: number;        // Optional start position
}

interface StartPreviewResponse {
  success: boolean;
  previewState: PreviewState;
}

// POST /api/search/preview/stop
// Stop track preview
interface StopPreviewResponse {
  success: boolean;
}

// GET /api/search/preview/state
// Get current preview state
interface GetPreviewStateResponse {
  state: PreviewState;
}

// POST /api/search/validate-remote
// Validate remote file accessibility
interface ValidateRemoteRequest {
  filePath: string;
  timeout?: number;
}

interface ValidateRemoteResponse {
  isAccessible: boolean;
  error?: string;
  latency?: number;              // Network latency (ms)
}
```

### 4.2 WebSocket Events

```typescript
// Real-time search results (for responsive UI)
interface SearchResultsUpdate {
  event: 'search:results';
  data: {
    query: string;
    results: SearchResult[];
    timestamp: Date;
  };
}

// Preview position updates
interface PreviewPositionUpdate {
  event: 'preview:position';
  data: {
    fileId: string;
    position: number;
    duration: number;
  };
}

// Preview state changes
interface PreviewStateChange {
  event: 'preview:state';
  data: PreviewState;
}

// Recent files list update
interface RecentFilesUpdate {
  event: 'recent-files:update';
  data: {
    added?: RecentFileEntry[];
    removed?: string[];          // File IDs
    total: number;
  };
}
```

---

## 5. State Management

### 5.1 Search State

```typescript
interface SearchStore {
  // Current state
  query: string;
  results: SearchResult[];
  sortBy: 'name' | 'date';
  includeRemote: boolean;
  highlightComments: boolean;
  highlightCopyright: boolean;
  isSearching: boolean;

  // Actions
  setQuery(query: string): void;
  setSortBy(sortBy: 'name' | 'date'): void;
  setIncludeRemote(include: boolean): void;
  setHighlightComments(highlight: boolean): void;
  setHighlightCopyright(highlight: boolean): void;
  search(): Promise<void>;
  clear(): void;

  // Derived state
  get filteredCount(): number;
  get totalCount(): number;
  get hasResults(): boolean;
}
```

### 5.2 Preview State

```typescript
interface PreviewStore {
  // Current state
  activeFileId: string | null;
  outputId: string | null;
  isPlaying: boolean;
  position: number;
  duration: number;

  // Actions
  startPreview(fileId: string, outputId: string): Promise<void>;
  stopPreview(): Promise<void>;
  seek(position: number): Promise<void>;

  // Derived state
  get isActive(): boolean;
  get progress(): number;        // 0-1
}
```

### 5.3 Recent Files Store

```typescript
interface RecentFilesStore {
  // Current state
  files: RecentFileEntry[];
  isLoading: boolean;
  lastUpdated: Date | null;

  // Actions
  loadRecentFiles(): Promise<void>;
  addFile(filePath: string, displayName?: string): Promise<void>;
  removeFile(fileId: string): Promise<void>;
  getFile(fileId: string): RecentFileEntry | null;

  // Derived state
  get count(): number;
  get hasFiles(): boolean;
}
```

---

## 6. Implementation Priorities

### 6.1 Phase 1: Core Search Infrastructure (Sprint 1-2)

**Priority:** P0 (Critical)

**Tasks:**
1. Implement `RecentFileEntry` data model
2. Create `recent_files` database schema
3. Implement recent files circular buffer (1000 limit)
4. Create basic CRUD operations for recent files
5. Implement file path tracking on load
6. Add database indices for performance

**Deliverables:**
- Recent files database table
- Basic API endpoints for CRUD
- Automatic tracking when files loaded

**Estimated Effort:** 16-20 hours

---

### 6.2 Phase 2: Search and Sorting (Sprint 2-3)

**Priority:** P0 (Critical)

**Tasks:**
1. Implement case-insensitive search engine
2. Add sorting by name (alphanumeric)
3. Add sorting by date (most recent first)
4. Implement search result ranking
5. Add match highlighting positions
6. Optimize search performance (< 50ms)

**Deliverables:**
- Search API endpoint with filtering
- Sorting options
- Result ranking and highlighting

**Estimated Effort:** 12-16 hours

---

### 6.3 Phase 3: WAV Metadata Parsing (Sprint 3-4)

**Priority:** P1 (High)

**Tasks:**
1. Implement RIFF chunk parser
2. Extract copyright field from INFO chunk
3. Extract comment field from INFO chunk
4. Parse BWF metadata (broadcast wave)
5. Create metadata cache system
6. Add metadata to recent files entries

**Deliverables:**
- WAV metadata parser service
- Metadata cache database
- Metadata API endpoint

**Estimated Effort:** 20-24 hours

---

### 6.4 Phase 4: Metadata Highlighting (Sprint 4)

**Priority:** P1 (High)

**Tasks:**
1. Implement comment field detection
2. Implement copyright field detection
3. Add display color logic (blue for comments)
4. Add "Highlight Comments" option
5. Add "Highlight Copyright" option
6. Display metadata in status bar

**Deliverables:**
- Highlighting options in search
- Color-coded results
- Status bar metadata display

**Estimated Effort:** 8-12 hours

---

### 6.5 Phase 5: Remote File Support (Sprint 5)

**Priority:** P2 (Medium)

**Tasks:**
1. Implement remote path detection (UNC, network drives)
2. Add "Try Remote Files" option
3. Implement remote file validation
4. Add network timeout handling
5. Display remote files in red text
6. Handle remote access errors gracefully

**Deliverables:**
- Remote file detection
- Remote validation API
- Red text display for remote files

**Estimated Effort:** 12-16 hours

---

### 6.6 Phase 6: Track Preview (Sprint 5-6)

**Priority:** P1 (High)

**Tasks:**
1. Implement preview audio player service
2. Add preview output routing (from Global menu)
3. Implement double-click to play
4. Implement right-click to stop
5. Stop preview on window close
6. Add preview state management
7. Handle preview conflicts (one at a time)

**Deliverables:**
- Preview player service
- Preview API endpoints
- Preview state management

**Estimated Effort:** 16-20 hours

---

### 6.7 Phase 7: Drag-and-Drop Integration (Sprint 6)

**Priority:** P1 (High)

**Tasks:**
1. Implement drag payload creation
2. Add Alt+Drop detection for Top/Tail
3. Integrate with Edit Menu Top/Tail (OCC117)
4. Handle drop on main window buttons
5. Update recent files timestamp on drop
6. Add drag-and-drop error handling

**Deliverables:**
- Drag-and-drop service
- Alt+Drop Top/Tail integration
- Drop handling on main window

**Estimated Effort:** 12-16 hours

---

### 6.8 Phase 8: Performance Optimization (Sprint 7)

**Priority:** P2 (Medium)

**Tasks:**
1. Optimize search query performance
2. Add database query caching
3. Implement metadata cache cleanup
4. Add lazy loading for large result sets
5. Optimize sorting algorithms
6. Profile and fix performance bottlenecks

**Deliverables:**
- < 50ms search response time
- Efficient database queries
- Optimized sorting

**Estimated Effort:** 8-12 hours

---

## 7. Technical Challenges

### 7.1 1000-File Circular Buffer Management

**Challenge:**
Efficiently maintain a 1000-file circular buffer with fast insertion and querying.

**Solution:**
- Use database trigger to automatically prune oldest entries
- Maintain indices on `loaded_at` for efficient sorting
- Use batch operations for insertions

```sql
-- Automatic pruning trigger
CREATE TRIGGER maintain_recent_files_limit
AFTER INSERT ON recent_files
BEGIN
  DELETE FROM recent_files
  WHERE id NOT IN (
    SELECT id FROM recent_files
    ORDER BY loaded_at DESC
    LIMIT 1000
  );
END;
```

**Risk:** Medium
**Mitigation:** Benchmark trigger performance, consider periodic cleanup vs per-insert cleanup

---

### 7.2 Real-time Search Performance

**Challenge:**
Provide < 50ms search response for 1000 files with real-time filtering.

**Solution:**
- Use SQLite FTS5 (full-text search) for efficient text queries
- Implement client-side filtering for small result sets
- Add debouncing to search input (200-300ms)
- Cache search results

```typescript
// Debounced search
const debouncedSearch = debounce(async (query: string) => {
  const results = await searchRecentFiles(query);
  updateResults(results);
}, 250);
```

**Risk:** Low
**Mitigation:** Profile query performance, optimize indices, use FTS5 for substring search

---

### 7.3 WAV Metadata Parsing Performance

**Challenge:**
Parsing WAV metadata on-demand can be slow for large files.

**Solution:**
- Parse metadata once when file added to recent list
- Cache metadata in database as JSON
- Invalidate cache if file modified timestamp changes
- Parse asynchronously in background

```typescript
class MetadataCache {
  async getMetadata(filePath: string): Promise<WavMetadata> {
    // Check cache first
    const cached = await this.getCached(filePath);
    if (cached && !this.isStale(cached, filePath)) {
      return cached.metadata;
    }

    // Parse and cache
    const metadata = await this.parser.parseFile(filePath);
    await this.setCached(filePath, metadata);
    return metadata;
  }
}
```

**Risk:** Medium
**Mitigation:** Implement robust caching, handle corrupt files gracefully, async parsing

---

### 7.4 Remote File Accessibility

**Challenge:**
Validating remote file access can be slow and unreliable.

**Solution:**
- Implement aggressive timeouts (2-5 seconds)
- Cache accessibility status with TTL
- Show stale results while validating in background
- Provide visual indicator for validation state

```typescript
interface AccessibilityCache {
  [filePath: string]: {
    isAccessible: boolean;
    lastChecked: Date;
    ttl: number;                 // Time-to-live (ms)
  };
}

class RemoteFileValidator {
  async validate(
    filePath: string,
    useCache: boolean = true
  ): Promise<boolean> {
    if (useCache) {
      const cached = this.cache.get(filePath);
      if (cached && !this.isExpired(cached)) {
        return cached.isAccessible;
      }
    }

    // Validate with timeout
    const result = await this.validateWithTimeout(filePath, 3000);
    this.cache.set(filePath, result);
    return result;
  }
}
```

**Risk:** High
**Mitigation:** Aggressive timeouts, caching, background validation, graceful degradation

---

### 7.5 Preview Output Routing

**Challenge:**
Coordinating preview output assignment from Global menu (OCC119+) with preview playback.

**Solution:**
- Create preview service abstraction
- Query Global menu for current preview output
- Handle output assignment changes during preview
- Stop preview if output becomes unavailable

```typescript
class PreviewService {
  async startPreview(fileId: string): Promise<void> {
    // Get current preview output assignment
    const outputId = await this.globalMenu.getPreviewOutput();

    // Validate output availability
    if (!await this.audioEngine.isOutputAvailable(outputId)) {
      throw new Error('Preview output not available');
    }

    // Route audio to preview output
    await this.player.setOutput(outputId);
    await this.player.load(fileId);
    await this.player.play();
  }
}
```

**Risk:** Medium
**Mitigation:** Global menu integration, output validation, error handling

---

### 7.6 Drag-and-Drop with Auto Top/Tail

**Challenge:**
Coordinating drag-and-drop with Alt key modifier for automatic Top/Tail processing.

**Solution:**
- Capture keyboard modifiers during drag operation
- Include modifiers in drag payload
- Call Top/Tail service before loading (from OCC117)
- Show visual feedback during processing

```typescript
class SearchDragDropService {
  handleDragStart(
    file: RecentFileEntry,
    event: DragEvent
  ): DragPayload {
    const payload: DragPayload = {
      fileId: file.id,
      filePath: file.filePath,
      displayName: file.displayName,
      autoTopTail: event.altKey  // Capture Alt key state
    };

    event.dataTransfer.setData('application/occ-file', JSON.stringify(payload));
    return payload;
  }

  async handleDrop(
    payload: DragPayload,
    targetButtonId: string
  ): Promise<void> {
    let filePath = payload.filePath;

    // Apply Top/Tail if Alt key was pressed
    if (payload.autoTopTail) {
      filePath = await this.topTailService.process(filePath);
    }

    // Load onto button
    await this.buttonService.loadFile(targetButtonId, filePath);
  }
}
```

**Risk:** Low
**Mitigation:** Robust keyboard event handling, Top/Tail integration, user feedback

---

## 8. Integration Points

### 8.1 File Menu Integration (OCC115)

**Dependencies:**
- File loading events trigger recent files updates
- Display name from File|Load File dialog
- File metadata extraction on load

**Integration Tasks:**
- Hook into file load events
- Extract display name from load dialog
- Add file to recent list automatically

---

### 8.2 Edit Menu Integration (OCC117)

**Dependencies:**
- Top/Tail processing for Alt+Drop
- Edit metadata affecting WAV metadata display

**Integration Tasks:**
- Call Top/Tail service on Alt+Drop
- Update metadata cache after edits
- Coordinate Top/Tail settings

---

### 8.3 Global Menu Integration (Future - OCC119+)

**Dependencies:**
- Preview output assignment
- Global preferences for search behavior

**Integration Tasks:**
- Query preview output from Global menu
- Respect global search preferences
- Handle output assignment changes

---

### 8.4 Main Window Button Integration

**Dependencies:**
- Drag-and-drop target handling
- Button assignment after drop
- Button state updates

**Integration Tasks:**
- Implement drop zones on buttons
- Handle file assignment
- Update button state after drop

---

### 8.5 Display Menu Integration (OCC117)

**Dependencies:**
- File location display in status bar
- Metadata display in status bar

**Integration Tasks:**
- Show full file path in status bar
- Display metadata (comments) in status bar
- Coordinate status bar updates

---

## 9. Testing Strategy

### 9.1 Unit Tests

```typescript
// Recent files service tests
describe('RecentFilesService', () => {
  test('maintains 1000 file limit', async () => {
    // Add 1100 files
    // Verify only 1000 most recent retained
  });

  test('sorts by name correctly', async () => {
    // Add files with various names
    // Verify alphanumeric sorting
  });

  test('sorts by date correctly', async () => {
    // Add files at different times
    // Verify recency sorting
  });
});

// Search engine tests
describe('SearchEngine', () => {
  test('case-insensitive search', async () => {
    // Search for 'SUN' should match 'sunshine'
  });

  test('partial match highlighting', async () => {
    // Verify match indices returned correctly
  });

  test('filters remote files correctly', async () => {
    // Add mix of local and remote
    // Verify filtering by includeRemote option
  });
});

// Metadata parser tests
describe('WavMetadataParser', () => {
  test('extracts copyright field', async () => {
    // Parse WAV with copyright
    // Verify extracted correctly
  });

  test('extracts comment field', async () => {
    // Parse WAV with comment
    // Verify extracted correctly
  });

  test('handles corrupt metadata gracefully', async () => {
    // Parse WAV with corrupt chunks
    // Verify no crash, partial extraction
  });
});
```

### 9.2 Integration Tests

```typescript
// End-to-end search flow
describe('Search Integration', () => {
  test('file loaded → appears in search', async () => {
    // Load a file
    // Open search menu
    // Verify file appears in list
  });

  test('search → filter → sort', async () => {
    // Load multiple files
    // Search for query
    // Change sort order
    // Verify results correct
  });

  test('preview → drag → drop with Alt', async () => {
    // Preview a file
    // Drag with Alt key
    // Drop on button
    // Verify Top/Tail applied
  });
});
```

### 9.3 Performance Tests

```typescript
// Search performance benchmarks
describe('Search Performance', () => {
  test('search 1000 files in < 50ms', async () => {
    // Add 1000 files
    // Time search operation
    // Assert < 50ms
  });

  test('metadata parsing < 100ms per file', async () => {
    // Parse 100 WAV files
    // Verify average < 100ms
  });
});
```

---

## 10. Documentation Requirements

### 10.1 API Documentation

- OpenAPI/Swagger spec for all REST endpoints
- WebSocket event documentation
- Code examples for common operations

### 10.2 Architecture Documentation

- Recent files circular buffer design
- Search engine architecture
- Metadata caching strategy
- Preview routing architecture

### 10.3 User Documentation

- Search menu user guide
- Metadata highlighting explanation
- Drag-and-drop usage
- Remote files configuration

---

## 11. Success Metrics

### 11.1 Performance Metrics

| Metric | Target | Measurement |
|--------|--------|-------------|
| Search response time | < 50ms | P95 latency |
| Metadata parsing | < 100ms/file | Average time |
| Recent files load | < 200ms | Initial load |
| Preview start latency | < 500ms | Time to first audio |

### 11.2 Functional Metrics

| Metric | Target | Measurement |
|--------|--------|-------------|
| Recent files capacity | 1000 files | Max entries |
| Search accuracy | 100% | Substring matches |
| Metadata extraction | > 95% | Success rate |
| Remote file validation | > 90% | Success rate |

---

## 12. Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Remote file timeouts | High | Medium | Aggressive timeouts, caching |
| Metadata parsing errors | Medium | Low | Graceful degradation, error handling |
| Search performance degradation | Low | High | Indexing, FTS5, profiling |
| Preview output unavailable | Medium | Medium | Validation, fallback output |
| Circular buffer corruption | Low | High | Database constraints, testing |

---

## 13. Future Enhancements

### 13.1 Advanced Search

- Regular expression search
- Multi-field search (filename, metadata, etc.)
- Saved search queries
- Search history

### 13.2 Smart Sorting

- Sort by file size, duration, sample rate
- Multi-criteria sorting
- Custom sort orders

### 13.3 Metadata Editing

- Edit metadata inline from search
- Batch metadata updates
- Metadata templates

### 13.4 Search Filters

- Filter by format (WAV, MP3, etc.)
- Filter by sample rate
- Filter by channel count
- Date range filtering

### 13.5 Preview Enhancements

- Waveform display in search results
- Preview volume control
- Preview loop mode
- Preview keyboard shortcuts

---

## 14. Sprint Summary

### Estimated Total Effort

| Phase | Effort (hours) | Priority |
|-------|----------------|----------|
| Phase 1: Core Search Infrastructure | 16-20 | P0 |
| Phase 2: Search and Sorting | 12-16 | P0 |
| Phase 3: WAV Metadata Parsing | 20-24 | P1 |
| Phase 4: Metadata Highlighting | 8-12 | P1 |
| Phase 5: Remote File Support | 12-16 | P2 |
| Phase 6: Track Preview | 16-20 | P1 |
| Phase 7: Drag-and-Drop Integration | 12-16 | P1 |
| Phase 8: Performance Optimization | 8-12 | P2 |
| **Total** | **104-136 hours** | - |

### Critical Path

1. Core Search Infrastructure → Search and Sorting (Dependencies: None)
2. WAV Metadata Parsing → Metadata Highlighting (Dependencies: Phase 1-2)
3. Track Preview (Dependencies: Global Menu for output assignment)
4. Drag-and-Drop (Dependencies: Edit Menu Top/Tail)

### Recommended Sprint Allocation

- **Sprint 1-2:** Phase 1-2 (Core search and sorting)
- **Sprint 3-4:** Phase 3-4 (Metadata parsing and highlighting)
- **Sprint 5:** Phase 5 (Remote files)
- **Sprint 6:** Phase 6-7 (Preview and drag-and-drop)
- **Sprint 7:** Phase 8 (Performance optimization)

---

## Appendix A: SpotOn Manual References

**Source:** SpotOn Manual - Section 05 - Search Menu

**Key Features:**
- Page 1: Recent file search window, 1000 file limit, real-time search
- Page 2: Sorting options, remote files (red text), metadata highlighting (blue text)
- Page 3: Track preview (double-click play, right-click stop), preview output routing

---

## Appendix B: Related Documents

- **OCC114:** Backend Architecture Audit (foundation)
- **OCC115:** File Menu Backend Features (file loading integration)
- **OCC116:** Setup Menu Backend Features (configuration)
- **OCC117:** Display/Edit Menu Backend Features (Top/Tail integration)
- **OCC119+:** Global Menu Backend Features (preview output assignment - TBD)

---

**Document Prepared By:** Backend Architecture Analysis Sprint
**Next Steps:** Analyze Section 06 (Global Menu) - OCC119
