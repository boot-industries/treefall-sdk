# OCC119 Global Menu Backend Features - Sprint Plan

**Status:** Draft
**Created:** 2025-11-12
**Sprint:** Backend Architecture Analysis (Part of OCC114)
**Dependencies:** OCC114 (Backend Audit), OCC115 (File Menu), OCC116 (Setup Menu), OCC117 (Display/Edit Menu), OCC118 (Search Menu)

---

## Executive Summary

Analysis of SpotOn Manual Section 06 (Global Menu) identifying backend features required for utilities, master/slave button linking, play stack, preview output routing, and session management.

**Complexity:** High (most complex menu with 15+ distinct features)
**Backend Components:** 15+ core feature areas
**Database Requirements:** High (session metadata, links, play stacks, statistics)
**API Endpoints:** ~40-50 endpoints

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

### 1.1 Global Menu Purpose

The Global Menu provides system-wide utilities and advanced features:
- Session/package management tools
- Master/Slave button linking system
- Independent Play Stack for interval music
- Preview output routing
- Audio generation utilities (timecode, click track)
- Statistics and debugging tools
- CD burning integration
- Output assignment management

### 1.2 Core Features Identified

| Feature Area | Description | Backend Complexity |
|--------------|-------------|-------------------|
| Session/Package Viewer | View and export session/package contents | Low |
| Master/Slave Links | Complex button relationship system | Very High |
| Play Groups | Exclusive button groups (25 groups) | High |
| Play Stack | Independent track player | High |
| Display Names Editor | Bulk edit display names and reorder tracks | Medium |
| Refresh Tracks | Detect and reload modified files | Medium |
| Preview Output Assignment | Route preview to specific output | Medium |
| Change Output Assignment | Bulk change output assignments | Medium |
| Generate Timecode | Create SMPTE timecode WAV files | Medium |
| Click Track Generator | Create metronome/click tracks | Medium |
| Statistics | Session file size and folder analysis | Low |
| Clear Attributes | Bulk clear button attributes | Low |
| CD Burning | Integration with CD burner utilities | Medium |
| Debug Logging | Collect and package debug logs | Low |
| Page Image Export | Save page screenshots | Low |

---

## 2. Backend Architecture Requirements

### 2.1 Master/Slave Link System

**Requirements:**
- Support 100 independent links
- Each link has 1 master button
- Each link can have unlimited play slaves and stop slaves
- Support special modes: Voice Over, AutoPan, Pause/UnPause
- Enable/disable links globally
- Visual link representation

**Implementation Approach:**
```typescript
interface MasterSlaveLink {
  id: number;                    // Link ID (1-100)
  name: string;                  // User-defined link name
  masterButtonId: string;        // Master button ID
  playSlaves: string[];          // Button IDs that play when master plays
  stopSlaves: string[];          // Button IDs that stop when master plays
  mode: LinkMode;                // Normal, VoiceOver, AutoPan, UnPause/Pause
  enabled: boolean;              // Link enabled/disabled

  // Voice Over specific
  voiceOverGainReduction?: number; // dB reduction for slaves (0 to -60)

  // AutoPan specific
  autoPanStartPosition?: number;  // Starting pan (-1 to 1)
  autoPanEndPosition?: number;    // Ending pan (-1 to 1)
}

enum LinkMode {
  NORMAL = 'normal',             // Standard play/stop slaves
  VOICE_OVER = 'voice_over',     // Fade down slaves during master
  AUTO_PAN = 'auto_pan',         // Pan slaves during master
  UNPAUSE_PAUSE = 'unpause_pause' // Pause/unpause slaves
}

interface LinkValidation {
  isValid: boolean;
  errors: string[];
  warnings: string[];
}
```

**Database Schema:**
```sql
CREATE TABLE master_slave_links (
  id INTEGER PRIMARY KEY CHECK(id BETWEEN 1 AND 100),
  name TEXT NOT NULL DEFAULT 'Link ' || id,
  master_button_id TEXT,
  mode TEXT NOT NULL DEFAULT 'normal' CHECK(mode IN ('normal', 'voice_over', 'auto_pan', 'unpause_pause')),
  enabled BOOLEAN DEFAULT TRUE,
  voice_over_gain_reduction REAL,
  auto_pan_start REAL,
  auto_pan_end REAL,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE link_play_slaves (
  link_id INTEGER NOT NULL,
  button_id TEXT NOT NULL,
  FOREIGN KEY (link_id) REFERENCES master_slave_links(id) ON DELETE CASCADE,
  UNIQUE(link_id, button_id)
);

CREATE TABLE link_stop_slaves (
  link_id INTEGER NOT NULL,
  button_id TEXT NOT NULL,
  FOREIGN KEY (link_id) REFERENCES master_slave_links(id) ON DELETE CASCADE,
  UNIQUE(link_id, button_id)
);

-- Index for fast lookup
CREATE INDEX idx_links_master ON master_slave_links(master_button_id);
CREATE INDEX idx_play_slaves_button ON link_play_slaves(button_id);
CREATE INDEX idx_stop_slaves_button ON link_stop_slaves(button_id);
```

### 2.2 Play Groups System

**Requirements:**
- 25 regular play groups (exclusive playback)
- 4 buzzer groups (A-D) with timeout lockout
- Group 24: special function to fade out Play Stack
- Group 25: special function to reset buzzer timeouts
- Configurable buzzer timeout (0-99 minutes)

**Implementation:**
```typescript
interface PlayGroup {
  id: number;                    // 1-25
  type: GroupType;               // Regular or Buzzer
  members: string[];             // Button IDs in this group
  buzzerTimeout?: number;        // Seconds (for buzzer groups)
  buzzerLockedUntil?: Date;      // When buzzer lock expires
}

enum GroupType {
  REGULAR = 'regular',           // Groups 1-23, 25
  PLAY_STACK_FADEOUT = 'play_stack_fadeout', // Group 24
  BUZZER_TIMEOUT_RESET = 'buzzer_timeout_reset', // Group 25
  BUZZER = 'buzzer'              // Buzzer groups A-D
}

interface ButtonGroupAssignment {
  buttonId: string;
  groupId: number | null;        // null = not in any group
  groupType: GroupType | null;
}
```

**Database Schema:**
```sql
CREATE TABLE play_groups (
  id INTEGER PRIMARY KEY CHECK(id BETWEEN 1 AND 25),
  type TEXT NOT NULL DEFAULT 'regular',
  buzzer_timeout INTEGER,        -- Seconds
  buzzer_locked_until TIMESTAMP
);

CREATE TABLE buzzer_groups (
  id TEXT PRIMARY KEY CHECK(id IN ('A', 'B', 'C', 'D')),
  buzzer_timeout INTEGER,        -- Seconds
  buzzer_locked_until TIMESTAMP
);

CREATE TABLE button_group_assignments (
  button_id TEXT PRIMARY KEY,
  group_id INTEGER,              -- 1-25 or null
  buzzer_group_id TEXT,          -- A-D or null
  FOREIGN KEY (group_id) REFERENCES play_groups(id),
  FOREIGN KEY (buzzer_group_id) REFERENCES buzzer_groups(id),
  CHECK ((group_id IS NULL) OR (buzzer_group_id IS NULL)) -- Can't be in both
);
```

### 2.3 Play Stack System

**Requirements:**
- Independent track player (separate from main buttons)
- Supports up to 20 tracks
- Loop mode or time-limited playback (1-90 minutes)
- Per-track play delay (0-9 seconds)
- Configurable fade out time (5-20 seconds)
- Delayed start via PC clock
- Independent output assignment
- Drag-and-drop reordering
- Track history (played/unplayed flags)

**Implementation:**
```typescript
interface PlayStack {
  id: string;
  name: string;
  tracks: PlayStackTrack[];
  currentTrackIndex: number;
  isPlaying: boolean;
  loopEnabled: boolean;
  playDuration?: number;         // Minutes (1-90)
  fadeOutDuration: number;       // Seconds (5-20)
  delayedStartTime?: Date;       // Optional scheduled start
  outputId: string;              // Output assignment
  masterGain: number;            // -20 to +10 dB
}

interface PlayStackTrack {
  id: string;
  filePath: string;
  fileName: string;              // Display name (truncated)
  duration: number;              // Samples
  playDelay: number;             // Seconds (0-9)
  hasPlayed: boolean;            // Track play history
  order: number;                 // Position in stack
}

interface PlayStackState {
  currentTrackId: string | null;
  position: number;              // Current position (samples)
  timeRemaining: number;         // Track time remaining (samples)
  stackTimeRemaining: number;    // Total stack time remaining (samples)
  isFading: boolean;
  fadeTimeRemaining: number;     // Seconds
}
```

**Database Schema:**
```sql
CREATE TABLE play_stacks (
  id TEXT PRIMARY KEY,
  name TEXT NOT NULL,
  loop_enabled BOOLEAN DEFAULT FALSE,
  play_duration INTEGER,         -- Minutes
  fade_out_duration INTEGER DEFAULT 9, -- Seconds
  delayed_start_time TIMESTAMP,
  output_id TEXT NOT NULL,
  master_gain REAL DEFAULT 0,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE play_stack_tracks (
  id TEXT PRIMARY KEY,
  stack_id TEXT NOT NULL,
  file_path TEXT NOT NULL,
  file_name TEXT NOT NULL,
  duration INTEGER NOT NULL,
  play_delay INTEGER DEFAULT 0,
  has_played BOOLEAN DEFAULT FALSE,
  track_order INTEGER NOT NULL,
  FOREIGN KEY (stack_id) REFERENCES play_stacks(id) ON DELETE CASCADE
);

CREATE INDEX idx_stack_tracks_order ON play_stack_tracks(stack_id, track_order);
```

### 2.4 Preview Output Assignment

**Requirements:**
- Assign a specific output for track preview
- Preview mode activation (Shift+Alt+Click)
- Only one track can preview at a time
- Visual indicator for preview mode
- Independent from normal playback

**Implementation:**
```typescript
interface PreviewOutputAssignment {
  outputId: string;              // Assigned preview output
  isActive: boolean;             // Preview mode currently active
  currentPreviewButtonId: string | null;
}

interface PreviewMode {
  isActive: boolean;
  buttonId: string;
  position: number;
  duration: number;
}

class PreviewService {
  private currentPreview: PreviewMode | null = null;

  async startPreview(
    buttonId: string,
    outputId: string
  ): Promise<void> {
    // Stop any existing preview
    if (this.currentPreview) {
      await this.stopPreview();
    }

    // Start new preview
    this.currentPreview = {
      isActive: true,
      buttonId,
      position: 0,
      duration: await this.getButtonDuration(buttonId)
    };

    // Route to preview output
    await this.audioEngine.routeToOutput(buttonId, outputId);
    await this.audioEngine.play(buttonId);
  }

  async stopPreview(): Promise<void> {
    if (this.currentPreview) {
      await this.audioEngine.stop(this.currentPreview.buttonId);
      this.currentPreview = null;
    }
  }
}
```

### 2.5 Output Assignment Management

**Requirements:**
- Bulk change output assignments globally
- Filter by current output device
- Scope: global, current page, button group, or button range
- Option to set as default output for new tracks

**Implementation:**
```typescript
interface OutputAssignmentChange {
  currentOutput: string | 'any';  // Filter: specific output or 'any'
  newOutput: string;              // Target output
  scope: AssignmentScope;
  setAsDefault: boolean;          // Make this the default for new tracks
}

interface AssignmentScope {
  type: 'global' | 'page' | 'group' | 'range';
  pageId?: string;
  groupId?: number;
  rangeStart?: number;
  rangeEnd?: number;
}

interface OutputAssignmentResult {
  buttonsAffected: number;
  buttonIds: string[];
  previousAssignments: Record<string, string>; // For undo
}
```

### 2.6 Display Names Editor

**Requirements:**
- Bulk edit display names for all buttons
- View button location (page, row, column)
- Drag-and-drop track reordering
- Swap button assignments (not just names)

**Implementation:**
```typescript
interface DisplayNameEntry {
  buttonId: string;
  buttonNumber: number;
  page: number;
  row: number;
  column: number;
  displayName: string;
  originalFileName: string;
}

interface TrackReorder {
  fromButtonId: string;
  toButtonId: string;
  insertBefore: boolean;         // Insert before or after target
}

class DisplayNamesService {
  async getAll Names(): Promise<DisplayNameEntry[]> {
    // Return all buttons with their display info
  }

  async updateDisplayName(
    buttonId: string,
    newName: string
  ): Promise<void> {
    // Update single display name
  }

  async reorderTracks(
    reorder: TrackReorder
  ): Promise<void> {
    // Swap button track assignments
    // This is NOT just a name change - actual track files are swapped
  }
}
```

### 2.7 Refresh Tracks System

**Requirements:**
- Scan all loaded tracks for file modifications
- Compare file timestamps against load timestamps
- List files with newer timestamps
- Reload modified files
- Handle playlist item selection reset
- Integration with external audio editors

**Implementation:**
```typescript
interface TrackModificationStatus {
  buttonId: string;
  filePath: string;
  loadedAt: Date;
  fileModifiedAt: Date;
  isModified: boolean;
}

interface RefreshResult {
  modifiedFiles: TrackModificationStatus[];
  refreshedButtons: string[];
  errors: RefreshError[];
}

interface RefreshError {
  buttonId: string;
  filePath: string;
  error: string;
}

class TrackRefreshService {
  async scanForModifications(): Promise<TrackModificationStatus[]> {
    // Compare file timestamps with loaded timestamps
    // Return list of modified files
  }

  async refreshTracks(
    buttonIds: string[]
  ): Promise<RefreshResult> {
    // Reload tracks that have been modified
    // Reset playlist selections and fade times
    // Return success/error results
  }
}
```

**Database Schema:**
```sql
-- Add file tracking to buttons table
ALTER TABLE buttons ADD COLUMN file_loaded_at TIMESTAMP;
ALTER TABLE buttons ADD COLUMN file_modified_at TIMESTAMP;

-- Track refresh history
CREATE TABLE track_refresh_history (
  id TEXT PRIMARY KEY,
  button_id TEXT NOT NULL,
  file_path TEXT NOT NULL,
  old_modified_at TIMESTAMP,
  new_modified_at TIMESTAMP,
  refreshed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (button_id) REFERENCES buttons(id)
);
```

### 2.8 Timecode Generator

**Requirements:**
- Generate SMPTE timecode as WAV file
- Support frame rates: 25Hz NDF, 30Hz NDF, 29.97Hz DF
- Configurable start timecode and duration
- Configurable signal level: -28dBFS, -18dBFS, -8dBFS
- Optional phase correction flag
- Save as 48kHz mono WAV file

**Implementation:**
```typescript
interface TimecodeGeneratorParams {
  startTimecode: Timecode;
  duration: Duration;
  frameRate: FrameRate;
  signalLevel: SignalLevel;
  includePhaseCorrection: boolean;
  outputPath: string;
}

interface Timecode {
  hours: number;
  minutes: number;
  seconds: number;
  frames: number;
}

interface Duration {
  hours: number;
  minutes: number;
  seconds: number;
}

enum FrameRate {
  FPS_25_NDF = '25Hz NDF',
  FPS_30_NDF = '30Hz NDF',
  FPS_29_97_DF = '29.97Hz DF'
}

enum SignalLevel {
  MINUS_28_DBFS = -28,
  MINUS_18_DBFS = -18,
  MINUS_8_DBFS = -8
}

class TimecodeGenerator {
  async generateTimecode(
    params: TimecodeGeneratorParams,
    onProgress?: (progress: number) => void
  ): Promise<string> {
    // Generate SMPTE timecode waveform
    // Return path to generated WAV file
  }
}
```

### 2.9 Click Track Generator

**Requirements:**
- Generate metronome/click tracks
- Configurable beats per bar (1-16)
- Configurable number of bars (1-999)
- BPM calculation and tempo slider
- Preview beats and taps
- Save as 48kHz mono 16-bit WAV
- Option to load directly onto next free button

**Implementation:**
```typescript
interface ClickTrackParams {
  beatsPerBar: number;           // 1-16
  numberOfBars: number;          // 1-999
  bpm: number;                   // Calculated from slider
  outputPath: string;
  loadToButton: boolean;         // Auto-load to next free button
}

interface ClickTrackPreview {
  beatSound: AudioBuffer;        // Preview sound for beat
  tapSound: AudioBuffer;         // Preview sound for tap
}

class ClickTrackGenerator {
  async generateClickTrack(
    params: ClickTrackParams,
    onProgress?: (progress: number) => void
  ): Promise<string> {
    // Generate click track WAV file
    // Return path to generated file
  }

  async previewBeat(): Promise<void> {
    // Play beat sound via preview output
  }

  async previewTap(): Promise<void> {
    // Play tap sound via preview output
  }
}
```

### 2.10 Session/Package Viewer

**Requirements:**
- Display session/package contents
- Show button number, output device, track title
- Copy to clipboard for external use
- Parse session and package file formats

**Implementation:**
```typescript
interface SessionPackageEntry {
  buttonNumber: number;
  outputDevice: string;
  trackTitle: string;
}

interface SessionPackageContents {
  version: string;
  buttonCount: number;
  entries: SessionPackageEntry[];
}

class SessionPackageViewer {
  async loadSession(filePath: string): Promise<SessionPackageContents> {
    // Parse SpotOn session file
    // Return structured contents
  }

  async loadPackage(filePath: string): Promise<SessionPackageContents> {
    // Parse SpotOn package file
    // Return structured contents
  }

  async exportToClipboard(
    contents: SessionPackageContents
  ): Promise<void> {
    // Format as text and copy to clipboard
  }
}
```

### 2.11 Statistics System

**Requirements:**
- File size analysis (bar chart by button)
- Track folder fragmentation analysis
- Highlight "Local Files" folder (files copied from network/removable media)
- Copy files from fragmented folders to consolidated locations

**Implementation:**
```typescript
interface FileStatistics {
  fileSizes: FileSizeEntry[];
  folderFragmentation: FolderFragmentation[];
}

interface FileSizeEntry {
  buttonId: string;
  buttonNumber: number;
  fileName: string;
  fileSize: number;              // Bytes
}

interface FolderFragmentation {
  folderPath: string;
  fileCount: number;
  isLocalFilesFolder: boolean;   // Highlighted in red
  totalSize: number;
}

interface FileCopyOperation {
  sourceFolderPath: string;
  destinationFolderPath: string;
  fileCount: number;
  files: string[];
}

class StatisticsService {
  async analyzeSession(): Promise<FileStatistics> {
    // Analyze current session
    // Return file sizes and folder fragmentation
  }

  async copyFilesFromFolder(
    operation: FileCopyOperation,
    onProgress?: (progress: number) => void
  ): Promise<CopyResult> {
    // Copy files from source to destination
    // Skip files that already exist
    // Return results
  }
}
```

### 2.12 Debug Logging System

**Requirements:**
- Collect logs for specified date range (last week, month, 3 months)
- Include playout, event, and error logs
- Compress to CAB file
- Optional FTP upload to support server
- Browse debug folder after collection

**Implementation:**
```typescript
interface DebugLogCollection {
  dateRange: LogDateRange;
  includePlayoutLogs: boolean;
  includeEventLogs: boolean;
  includeErrorLogs: boolean;
  uploadToFTP: boolean;
}

enum LogDateRange {
  LAST_WEEK = 'last_week',
  LAST_MONTH = 'last_month',
  LAST_3_MONTHS = 'last_3_months'
}

interface DebugCollectionResult {
  cabFilePath: string;
  cabFileSize: number;
  logFileCount: number;
  uploadSuccess?: boolean;
  uploadError?: string;
}

class DebugLogService {
  async collectLogs(
    params: DebugLogCollection,
    onProgress?: (progress: number) => void
  ): Promise<DebugCollectionResult> {
    // Copy logs to debug folder
    // Compress to CAB file
    // Optionally upload to FTP
    // Return results
  }
}
```

### 2.13 Page Image Export

**Requirements:**
- Export page screenshots as BMP or JPEG
- Cycle through all pages automatically
- Save to specified directory
- Return to original page after export

**Implementation:**
```typescript
interface PageImageExport {
  format: ImageFormat;
  outputDirectory: string;
  fileNamePrefix: string;
}

enum ImageFormat {
  BMP = 'bmp',
  JPEG = 'jpg'
}

interface PageImageExportResult {
  pageCount: number;
  filesPaths: string[];
  totalSize: number;
}

class PageImageService {
  async exportAllPages(
    params: PageImageExport,
    onProgress?: (pageNumber: number, total: number) => void
  ): Promise<PageImageExportResult> {
    // Cycle through pages
    // Capture screenshots
    // Save to files
    // Return to original page
  }
}
```

### 2.14 CD Burning Integration

**Requirements:**
- Integration with CDBurnerXP or similar utility
- Support audio CD (CDA) or data CD (WAV)
- Burn from current page or entire session
- Support source regions for non-contiguous selections
- Files burned are originals (no SpotOn processing)
- Multi-channel files mixed to stereo for audio CDs

**Implementation:**
```typescript
interface CDBurnRequest {
  type: CDType;
  source: BurnSource;
  burnerPath: string;            // Path to CDBurnerXP executable
  buttons: string[];             // Buttons to burn
}

enum CDType {
  AUDIO_CD = 'audio_cd',         // CDA tracks
  DATA_CD = 'data_cd'            // WAV files
}

interface BurnSource {
  type: 'page' | 'session' | 'region';
  pageId?: string;
  regionId?: string;
}

class CDBurningService {
  async prepareBurnList(
    request: CDBurnRequest
  ): Promise<string[]> {
    // Get original file paths (not processed versions)
    // Mix multi-channel to stereo if audio CD
    // Return list of file paths
  }

  async launchBurnerUtility(
    request: CDBurnRequest,
    filePaths: string[]
  ): Promise<void> {
    // Launch CDBurnerXP with file list
    // Wait for user to complete burn process
  }
}
```

### 2.15 Clear Attributes

**Requirements:**
- Clear GPIs (GPIO assignments)
- Clear Hotkeys
- Reset MIDI In triggers
- Reset MIDI Out triggers
- Clear all Master/Slave links
- Clear button images
- Sort Master/Slave links
- Clear external utility paths (CD burner, audio editor, search utility)

**Implementation:**
```typescript
interface ClearAttributesOperation {
  clearGPIs: boolean;
  clearHotkeys: boolean;
  clearMidiInTriggers: boolean;
  clearMidiOutTriggers: boolean;
  clearAllLinks: boolean;
  clearButtonImages: boolean;
  sortLinks: boolean;
  clearCDBurnerPath: boolean;
  clearAudioEditorPath: boolean;
  clearSearchUtilityPath: boolean;
}

interface ClearAttributesResult {
  gpisCleared: number;
  hotkeysCleared: number;
  midiInTriggersCleared: number;
  midiOutTriggersCleared: number;
  linksCleared: number;
  imagesCleared: number;
}

class ClearAttributesService {
  async clearAttributes(
    operation: ClearAttributesOperation
  ): Promise<ClearAttributesResult> {
    // Clear specified attributes
    // Return counts of cleared items
  }
}
```

---

## 3. Data Models

### 3.1 Master/Slave Link Models

```typescript
// Primary link entity
interface MasterSlaveLink {
  id: number;
  name: string;
  masterButtonId: string;
  playSlaves: string[];
  stopSlaves: string[];
  mode: LinkMode;
  enabled: boolean;
  voiceOverGainReduction?: number;
  autoPanStart?: number;
  autoPanEnd?: number;
}

// Link validation result
interface LinkValidation {
  linkId: number;
  isValid: boolean;
  isBroken: boolean;             // Master unused or all slaves unused
  errors: string[];
  warnings: string[];
}

// Link visualization (for Ctrl+Shift+Click display)
interface LinkVisualization {
  masterButton: string;
  playSlaves: LinkConnection[];
  stopSlaves: LinkConnection[];
  pauseSlaves: LinkConnection[];
  unpauseSlaves: LinkConnection[];
}

interface LinkConnection {
  fromButtonId: string;
  toButtonId: string;
  linkType: 'play' | 'stop' | 'pause' | 'unpause' | 'voice_over' | 'auto_pan';
  color: string;                 // Green, blue, magenta, white, etc.
}
```

### 3.2 Play Group Models

```typescript
interface PlayGroup {
  id: number;
  type: GroupType;
  members: string[];
  buzzerTimeout?: number;
  buzzerLockedUntil?: Date;
}

interface ButtonGroupAssignment {
  buttonId: string;
  groupId: number | null;
  buzzerGroupId: string | null;  // 'A', 'B', 'C', 'D'
}
```

### 3.3 Play Stack Models

```typescript
interface PlayStack {
  id: string;
  name: string;
  tracks: PlayStackTrack[];
  currentTrackIndex: number;
  isPlaying: boolean;
  loopEnabled: boolean;
  playDuration?: number;
  fadeOutDuration: number;
  delayedStartTime?: Date;
  outputId: string;
  masterGain: number;
}

interface PlayStackTrack {
  id: string;
  filePath: string;
  fileName: string;
  duration: number;
  playDelay: number;
  hasPlayed: boolean;
  order: number;
}

interface PlayStackState {
  currentTrackId: string | null;
  position: number;
  timeRemaining: number;
  stackTimeRemaining: number;
  isFading: boolean;
  fadeTimeRemaining: number;
}
```

### 3.4 Preview Output Models

```typescript
interface PreviewOutputAssignment {
  outputId: string;
  isActive: boolean;
  currentPreviewButtonId: string | null;
}

interface PreviewMode {
  isActive: boolean;
  buttonId: string;
  position: number;
  duration: number;
}
```

---

## 4. API Contracts

### 4.1 Master/Slave Links Endpoints

```typescript
// GET /api/global/links
// Get all Master/Slave links
interface GetLinksResponse {
  links: MasterSlaveLink[];
  enabled: boolean;              // Global enable/disable
}

// GET /api/global/links/:id
// Get a specific link
interface GetLinkResponse {
  link: MasterSlaveLink;
}

// POST /api/global/links
// Create a new link
interface CreateLinkRequest {
  name?: string;
  masterButtonId: string;
  playSlaves?: string[];
  stopSlaves?: string[];
  mode?: LinkMode;
}

interface CreateLinkResponse {
  link: MasterSlaveLink;
}

// PUT /api/global/links/:id
// Update an existing link
interface UpdateLinkRequest {
  name?: string;
  masterButtonId?: string;
  playSlaves?: string[];
  stopSlaves?: string[];
  mode?: LinkMode;
  voiceOverGainReduction?: number;
  autoPanStart?: number;
  autoPanEnd?: number;
}

// DELETE /api/global/links/:id
// Delete a link
interface DeleteLinkResponse {
  success: boolean;
}

// POST /api/global/links/validate
// Validate all links, find broken links
interface ValidateLinksResponse {
  validations: LinkValidation[];
  brokenLinks: number[];
}

// DELETE /api/global/links/broken
// Delete all broken links
interface DeleteBrokenLinksResponse {
  deletedCount: number;
  deletedLinkIds: number[];
}

// POST /api/global/links/sort
// Sort links by master button number
interface SortLinksResponse {
  success: boolean;
}

// GET /api/global/links/:id/visualization
// Get link visualization for graphical display
interface GetLinkVisualizationResponse {
  visualization: LinkVisualization;
}

// POST /api/global/links/enable
// Enable or disable links globally
interface EnableLinksRequest {
  enabled: boolean;
}
```

### 4.2 Play Groups Endpoints

```typescript
// GET /api/global/groups
// Get all play groups
interface GetGroupsResponse {
  regularGroups: PlayGroup[];
  buzzerGroups: PlayGroup[];
  enabled: boolean;
}

// GET /api/global/groups/:id
// Get a specific group
interface GetGroupResponse {
  group: PlayGroup;
}

// PUT /api/global/groups/:id
// Update group settings
interface UpdateGroupRequest {
  buzzerTimeout?: number;
}

// GET /api/global/groups/assignments
// Get button group assignments
interface GetGroupAssignmentsResponse {
  assignments: ButtonGroupAssignment[];
}

// PUT /api/global/groups/assignments/:buttonId
// Assign button to a group
interface AssignButtonToGroupRequest {
  groupId: number | null;
  buzzerGroupId: string | null;
}

// DELETE /api/global/groups/assignments
// Clear all group assignments
interface ClearGroupAssignmentsResponse {
  clearedCount: number;
}

// POST /api/global/groups/buzzer/reset
// Reset buzzer timeouts (triggered by Group 25)
interface ResetBuzzerTimeoutsResponse {
  success: boolean;
}
```

### 4.3 Play Stack Endpoints

```typescript
// GET /api/global/play-stack
// Get current play stack
interface GetPlayStackResponse {
  stack: PlayStack;
  state: PlayStackState;
}

// POST /api/global/play-stack/tracks
// Add track to play stack
interface AddStackTrackRequest {
  filePath: string;
  playDelay?: number;
  insertAt?: number;             // Optional position
}

// DELETE /api/global/play-stack/tracks/:id
// Remove track from stack
interface RemoveStackTrackResponse {
  success: boolean;
}

// PUT /api/global/play-stack/tracks/:id
// Update track settings
interface UpdateStackTrackRequest {
  playDelay?: number;
  order?: number;
}

// POST /api/global/play-stack/tracks/reorder
// Reorder tracks
interface ReorderStackTracksRequest {
  trackId: string;
  newPosition: number;
}

// POST /api/global/play-stack/play
// Start playing stack
interface PlayStackRequest {
  startAt?: number;              // Optional track index
}

// POST /api/global/play-stack/stop
// Stop playing stack
interface StopStackResponse {
  success: boolean;
}

// POST /api/global/play-stack/fade
// Fade out stack (triggered by Group 24)
interface FadeStackRequest {
  fadeDuration?: number;         // Override default fade duration
}

// PUT /api/global/play-stack/settings
// Update stack settings
interface UpdateStackSettingsRequest {
  loopEnabled?: boolean;
  playDuration?: number;
  fadeOutDuration?: number;
  delayedStartTime?: Date;
  outputId?: string;
  masterGain?: number;
}

// POST /api/global/play-stack/save
// Save stack to file
interface SaveStackRequest {
  filePath: string;
}

// POST /api/global/play-stack/load
// Load stack from file
interface LoadStackRequest {
  filePath: string;
}

// DELETE /api/global/play-stack/tracks
// Clear all tracks
interface ClearStackResponse {
  success: boolean;
}
```

### 4.4 Preview Output Endpoints

```typescript
// GET /api/global/preview-output
// Get current preview output assignment
interface GetPreviewOutputResponse {
  outputId: string;
  isActive: boolean;
  currentPreviewButtonId: string | null;
}

// PUT /api/global/preview-output
// Set preview output
interface SetPreviewOutputRequest {
  outputId: string;
}

// POST /api/global/preview/start
// Start preview (Shift+Alt+Click)
interface StartPreviewRequest {
  buttonId: string;
}

// POST /api/global/preview/stop
// Stop preview
interface StopPreviewResponse {
  success: boolean;
}

// GET /api/global/preview/state
// Get current preview state
interface GetPreviewStateResponse {
  mode: PreviewMode | null;
}
```

### 4.5 Utility Endpoints

```typescript
// POST /api/global/output-assignment
// Bulk change output assignments
interface BulkOutputAssignmentRequest {
  currentOutput: string | 'any';
  newOutput: string;
  scope: AssignmentScope;
  setAsDefault: boolean;
}

interface BulkOutputAssignmentResponse {
  result: OutputAssignmentResult;
}

// POST /api/global/refresh-tracks
// Refresh modified tracks
interface RefreshTracksRequest {
  buttonIds?: string[];          // Optional: specific buttons, else all
}

interface RefreshTracksResponse {
  result: RefreshResult;
}

// GET /api/global/refresh-tracks/scan
// Scan for modified tracks
interface ScanModifiedTracksResponse {
  modifiedFiles: TrackModificationStatus[];
}

// POST /api/global/generate-timecode
// Generate timecode file
interface GenerateTimecodeRequest {
  params: TimecodeGeneratorParams;
}

interface GenerateTimecodeResponse {
  filePath: string;
  fileSize: number;
  duration: number;
}

// POST /api/global/generate-click-track
// Generate click track
interface GenerateClickTrackRequest {
  params: ClickTrackParams;
}

interface GenerateClickTrackResponse {
  filePath: string;
  fileSize: number;
  loadedToButton?: string;       // If auto-loaded
}

// GET /api/global/statistics
// Get session statistics
interface GetStatisticsResponse {
  statistics: FileStatistics;
}

// POST /api/global/statistics/copy-files
// Copy files from fragmented folder
interface CopyFilesRequest {
  operation: FileCopyOperation;
}

interface CopyFilesResponse {
  copiedCount: number;
  skippedCount: number;
  errors: string[];
}

// GET /api/global/session-viewer
// View session/package contents
interface SessionViewerRequest {
  filePath: string;
  type: 'session' | 'package';
}

interface SessionViewerResponse {
  contents: SessionPackageContents;
}

// POST /api/global/debug-logs
// Collect debug logs
interface CollectDebugLogsRequest {
  params: DebugLogCollection;
}

interface CollectDebugLogsResponse {
  result: DebugCollectionResult;
}

// POST /api/global/export-page-images
// Export page screenshots
interface ExportPageImagesRequest {
  params: PageImageExport;
}

interface ExportPageImagesResponse {
  result: PageImageExportResult;
}

// POST /api/global/burn-cd
// Prepare CD burn
interface BurnCDRequest {
  request: CDBurnRequest;
}

interface BurnCDResponse {
  filePaths: string[];
  burnerLaunched: boolean;
}

// POST /api/global/clear-attributes
// Clear various attributes
interface ClearAttributesRequest {
  operation: ClearAttributesOperation;
}

interface ClearAttributesResponse {
  result: ClearAttributesResult;
}

// GET /api/global/display-names
// Get all display names
interface GetDisplayNamesResponse {
  entries: DisplayNameEntry[];
}

// PUT /api/global/display-names/:buttonId
// Update display name
interface UpdateDisplayNameRequest {
  displayName: string;
}

// POST /api/global/display-names/reorder
// Reorder tracks between buttons
interface ReorderTracksRequest {
  reorder: TrackReorder;
}
```

### 4.6 WebSocket Events

```typescript
// Link state changes
interface LinkStateChange {
  event: 'link:state';
  data: {
    linkId: number;
    masterButtonId: string;
    isPlaying: boolean;
  };
}

// Group playback (exclusive groups)
interface GroupPlaybackEvent {
  event: 'group:playback';
  data: {
    groupId: number;
    activeButtonId: string;
    stoppedButtons: string[];
  };
}

// Play stack state updates
interface PlayStackStateUpdate {
  event: 'play-stack:state';
  data: PlayStackState;
}

// Play stack track change
interface PlayStackTrackChange {
  event: 'play-stack:track';
  data: {
    trackId: string;
    trackIndex: number;
  };
}

// Preview mode state
interface PreviewModeChange {
  event: 'preview:state';
  data: PreviewMode | null;
}

// Refresh tracks completion
interface RefreshTracksComplete {
  event: 'refresh:complete';
  data: RefreshResult;
}
```

---

## 5. State Management

### 5.1 Master/Slave Links State

```typescript
interface LinksStore {
  // Current state
  links: MasterSlaveLink[];
  enabled: boolean;
  activeLinks: number[];         // Currently active link IDs
  brokenLinks: number[];

  // Actions
  loadLinks(): Promise<void>;
  createLink(request: CreateLinkRequest): Promise<MasterSlaveLink>;
  updateLink(id: number, request: UpdateLinkRequest): Promise<void>;
  deleteLink(id: number): Promise<void>;
  validateLinks(): Promise<LinkValidation[]>;
  deleteBrokenLinks(): Promise<void>;
  sortLinks(): Promise<void>;
  enableLinks(enabled: boolean): Promise<void>;

  // Derived state
  get linkCount(): number;
  get brokenLinkCount(): number;
  getLinksByMaster(buttonId: string): MasterSlaveLink[];
  getLinksBySlave(buttonId: string): MasterSlaveLink[];
}
```

### 5.2 Play Groups State

```typescript
interface GroupsStore {
  // Current state
  regularGroups: PlayGroup[];
  buzzerGroups: PlayGroup[];
  assignments: ButtonGroupAssignment[];
  enabled: boolean;

  // Actions
  loadGroups(): Promise<void>;
  updateGroup(id: number, settings: UpdateGroupRequest): Promise<void>;
  assignButtonToGroup(buttonId: string, request: AssignButtonToGroupRequest): Promise<void>;
  clearAllAssignments(): Promise<void>;
  resetBuzzerTimeouts(): Promise<void>;

  // Derived state
  getGroupMembers(groupId: number): string[];
  getButtonGroup(buttonId: string): number | null;
  isButtonInGroup(buttonId: string, groupId: number): boolean;
}
```

### 5.3 Play Stack State

```typescript
interface PlayStackStore {
  // Current state
  stack: PlayStack;
  state: PlayStackState;
  isVisible: boolean;            // Window shown/hidden

  // Actions
  loadStack(): Promise<void>;
  addTrack(request: AddStackTrackRequest): Promise<void>;
  removeTrack(trackId: string): Promise<void>;
  updateTrack(trackId: string, request: UpdateStackTrackRequest): Promise<void>;
  reorderTracks(request: ReorderStackTracksRequest): Promise<void>;
  play(startAt?: number): Promise<void>;
  stop(): Promise<void>;
  fade(duration?: number): Promise<void>;
  updateSettings(request: UpdateStackSettingsRequest): Promise<void>;
  saveStack(filePath: string): Promise<void>;
  loadStack(filePath: string): Promise<void>;
  clearAll(): Promise<void>;

  // Derived state
  get trackCount(): number;
  get totalDuration(): number;
  get isPlaying(): boolean;
  get currentTrack(): PlayStackTrack | null;
}
```

### 5.4 Preview State

```typescript
interface PreviewStore {
  // Current state
  outputId: string;
  isActive: boolean;
  mode: PreviewMode | null;

  // Actions
  setOutput(outputId: string): Promise<void>;
  startPreview(buttonId: string): Promise<void>;
  stopPreview(): Promise<void>;

  // Derived state
  get isPreviewActive(): boolean;
  get currentButtonId(): string | null;
}
```

---

## 6. Implementation Priorities

### 6.1 Phase 1: Preview Output & Output Assignment (Sprint 1-2)

**Priority:** P0 (Critical)

**Tasks:**
1. Implement preview output assignment
2. Create preview mode service
3. Add Shift+Alt keyboard modifier detection
4. Implement visual preview indicator
5. Create bulk output assignment service
6. Add assignment scope filtering

**Deliverables:**
- Preview output API endpoints
- Preview mode UI controls
- Output assignment API endpoints

**Estimated Effort:** 20-24 hours

---

### 6.2 Phase 2: Display Names & Refresh Tracks (Sprint 2-3)

**Priority:** P0 (Critical)

**Tasks:**
1. Implement display names editor service
2. Add drag-and-drop track reordering
3. Create track modification scanner
4. Implement file timestamp comparison
5. Add track refresh service
6. Handle playlist reset on refresh

**Deliverables:**
- Display names API endpoints
- Track refresh API endpoints
- File monitoring service

**Estimated Effort:** 16-20 hours

---

### 6.3 Phase 3: Master/Slave Links - Basic (Sprint 3-5)

**Priority:** P0 (Critical)

**Tasks:**
1. Implement link data model
2. Create link CRUD operations
3. Add play slave execution
4. Add stop slave execution
5. Implement link validation
6. Add broken link detection
7. Create link enable/disable

**Deliverables:**
- Links database schema
- Links API endpoints
- Link execution engine

**Estimated Effort:** 32-40 hours

---

### 6.4 Phase 4: Master/Slave Links - Advanced Modes (Sprint 5-6)

**Priority:** P1 (High)

**Tasks:**
1. Implement Voice Over mode
2. Add gain reduction for slaves
3. Implement AutoPan mode
4. Add pan transition logic
5. Implement Pause/Unpause mode
6. Add fade time validation for Voice Over

**Deliverables:**
- Voice Over implementation
- AutoPan implementation
- Pause/Unpause implementation

**Estimated Effort:** 24-32 hours

---

### 6.5 Phase 5: Link Visualization (Sprint 6-7)

**Priority:** P2 (Medium)

**Tasks:**
1. Implement link visualization algorithm
2. Add Ctrl+Shift+Click handler
3. Create visual link rendering
4. Add color coding for link types
5. Implement drag-to-create links
6. Add drag-to-delete links

**Deliverables:**
- Link visualization API
- Visual link creation/deletion

**Estimated Effort:** 16-20 hours

---

### 6.6 Phase 6: Play Groups (Sprint 7-8)

**Priority:** P1 (High)

**Tasks:**
1. Implement play groups data model
2. Create exclusive playback logic
3. Add buzzer groups with timeout
4. Implement Group 24 (Play Stack fadeout)
5. Implement Group 25 (Buzzer reset)
6. Add group assignment UI

**Deliverables:**
- Play groups database schema
- Exclusive playback engine
- Buzzer timeout management

**Estimated Effort:** 24-28 hours

---

### 6.7 Phase 7: Play Stack (Sprint 8-10)

**Priority:** P1 (High)

**Tasks:**
1. Implement play stack data model
2. Create independent audio player
3. Add loop mode
4. Add time-limited playback
5. Implement per-track play delay
6. Add delayed start scheduling
7. Implement fade out
8. Add track reordering
9. Create stack save/load
10. Integrate with Group 24 fadeout

**Deliverables:**
- Play stack database schema
- Independent audio player
- Stack management API endpoints

**Estimated Effort:** 40-48 hours

---

### 6.8 Phase 8: Audio Generation Utilities (Sprint 10-11)

**Priority:** P2 (Medium)

**Tasks:**
1. Implement SMPTE timecode generator
2. Add frame rate support
3. Implement click track generator
4. Add BPM calculation
5. Create preview functionality
6. Add progress reporting

**Deliverables:**
- Timecode generator
- Click track generator
- Generation API endpoints

**Estimated Effort:** 24-32 hours

---

### 6.9 Phase 9: Statistics & Session Viewer (Sprint 11-12)

**Priority:** P2 (Medium)

**Tasks:**
1. Implement file size analysis
2. Create folder fragmentation analysis
3. Add file copy service
4. Implement session/package parser
5. Add clipboard export

**Deliverables:**
- Statistics API endpoints
- Session viewer API endpoints
- File copy service

**Estimated Effort:** 16-20 hours

---

### 6.10 Phase 10: Utility Features (Sprint 12-13)

**Priority:** P3 (Low)

**Tasks:**
1. Implement debug log collection
2. Add CAB compression
3. Implement FTP upload
4. Create page image export
5. Add CD burn integration
6. Implement clear attributes

**Deliverables:**
- Debug logging service
- Page export service
- CD burn service
- Clear attributes service

**Estimated Effort:** 20-24 hours

---

## 7. Technical Challenges

### 7.1 Master/Slave Link Conflicts

**Challenge:**
Preventing conflicts between Master/Slave links and Play Groups.

**Solution:**
- Enforce mutual exclusion at database level
- Validate before saving
- Auto-delete conflicting assignments
- Clear visual indicators of conflicts

```typescript
interface LinkGroupConflict {
  buttonId: string;
  linkId: number;
  groupId: number;
  resolution: 'delete_link' | 'delete_group';
}

class ConflictResolver {
  async detectConflicts(): Promise<LinkGroupConflict[]> {
    // Find buttons in both links and groups
  }

  async resolveConflicts(
    conflicts: LinkGroupConflict[]
  ): Promise<void> {
    // Auto-resolve based on priority
    // Links take precedence over groups
  }
}
```

**Risk:** Medium
**Mitigation:** Clear user warnings, automatic conflict resolution, undo capability

---

### 7.2 Voice Over Fade Timing

**Challenge:**
Synchronizing Voice Over gain reduction with master button fade in/out times.

**Solution:**
- Validate master button has non-zero fade times
- Use master fade times for slave gain transitions
- Implement smooth gain curves
- Handle master button stop during fade

```typescript
class VoiceOverProcessor {
  async applyVoiceOver(
    masterButtonId: string,
    slaveButtonIds: string[],
    gainReduction: number
  ): Promise<void> {
    const masterFadeIn = await this.getFadeInTime(masterButtonId);
    const masterFadeOut = await this.getFadeOutTime(masterButtonId);

    if (masterFadeIn === 0 || masterFadeOut === 0) {
      throw new Error('Voice Over requires non-zero fade times');
    }

    // Apply gain reduction during master playback
    await this.scheduleGainTransitions(
      slaveButtonIds,
      gainReduction,
      masterFadeIn,
      masterFadeOut
    );
  }
}
```

**Risk:** Medium
**Mitigation:** Validation before link creation, clear error messages, fade time warnings

---

### 7.3 Play Stack Independent Playback

**Challenge:**
Running Play Stack completely independently from main SpotOn playback.

**Solution:**
- Separate audio player instance
- Dedicated output routing
- Independent state management
- No interference with main buttons

```typescript
class PlayStackAudioEngine {
  private player: AudioPlayer;
  private outputId: string;
  private isIndependent: boolean = true;

  async play(track: PlayStackTrack): Promise<void> {
    // Use dedicated audio player
    // Route to assigned output only
    // No interaction with main button system
  }
}
```

**Risk:** Low
**Mitigation:** Clear separation of concerns, independent audio thread, isolated state

---

### 7.4 Buzzer Group Timeout Management

**Challenge:**
Implementing buzzer group lockout after first trigger, with timeout reset.

**Solution:**
- Track timeout expiration per buzzer group
- Block play commands during lockout
- Reset via Group 25 or GPI toggle
- Handle clock sync for accurate timeouts

```typescript
class BuzzerGroupManager {
  private timeouts: Map<string, Date> = new Map();

  async canPlay(
    buttonId: string,
    groupId: string
  ): Promise<boolean> {
    const lockoutExpires = this.timeouts.get(groupId);
    if (!lockoutExpires) return true;

    return new Date() > lockoutExpires;
  }

  async lockGroup(
    groupId: string,
    timeoutSeconds: number
  ): Promise<void> {
    const expiration = new Date();
    expiration.setSeconds(expiration.getSeconds() + timeoutSeconds);
    this.timeouts.set(groupId, expiration);
  }

  async resetAllTimeouts(): Promise<void> {
    this.timeouts.clear();
  }
}
```

**Risk:** Medium
**Mitigation:** Accurate timing, clock synchronization, clear timeout indicators

---

### 7.5 Link Visualization Performance

**Challenge:**
Rendering complex link diagrams with many connections in real-time.

**Solution:**
- Use canvas-based rendering
- Implement spatial indexing for collision detection
- Limit visible links to current view
- Cache rendered paths

```typescript
class LinkVisualizationRenderer {
  private canvas: OffscreenCanvas;
  private linkCache: Map<number, Path2D> = new Map();

  async renderLinks(
    links: LinkVisualization[],
    viewport: Rect
  ): Promise<ImageBitmap> {
    // Only render links in viewport
    const visibleLinks = this.filterVisible(links, viewport);

    // Use cached paths when possible
    for (const link of visibleLinks) {
      const path = this.linkCache.get(link.linkId) ||
                    this.createPath(link);
      this.drawPath(path, link.color);
    }

    return this.canvas.transferToImageBitmap();
  }
}
```

**Risk:** Low
**Mitigation:** Canvas rendering, caching, viewport culling, progressive loading

---

### 7.6 Track Refresh File Locking

**Challenge:**
Refreshing tracks that may be currently playing or locked by external editors.

**Solution:**
- Check playback state before refresh
- Attempt file access with timeout
- Skip locked files with warning
- Queue for retry

```typescript
class TrackRefreshService {
  async refreshTrack(buttonId: string): Promise<RefreshResult> {
    // Check if button is playing
    if (await this.isPlaying(buttonId)) {
      return {
        success: false,
        error: 'Cannot refresh playing track'
      };
    }

    // Try to access file with timeout
    try {
      const file = await this.accessFileWithTimeout(
        buttonId,
        5000  // 5 second timeout
      );
      await this.reloadTrack(buttonId, file);
      return { success: true };
    } catch (error) {
      return {
        success: false,
        error: 'File locked or inaccessible'
      };
    }
  }
}
```

**Risk:** Medium
**Mitigation:** Playback state checking, file locking detection, retry mechanism, user warnings

---

## 8. Integration Points

### 8.1 File Menu Integration (OCC115)

**Dependencies:**
- Track loading triggers Master/Slave link assignments
- Display name setting during file load
- Output assignment from default setting

**Integration Tasks:**
- Hook into file load events for link setup
- Coordinate display name assignment
- Apply default output if set

---

### 8.2 Setup Menu Integration (OCC116)

**Dependencies:**
- MIDI trigger assignments for Master/Slave links
- GPI assignments for Master/Slave triggers
- Buzzer group timeout configuration
- External utility paths (CD burner, audio editor)

**Integration Tasks:**
- MIDI trigger link execution
- GPI trigger link execution
- Buzzer timeout settings persistence

---

### 8.3 Display/Edit Menu Integration (OCC117)

**Dependencies:**
- Audio setup for Voice Over gain reduction
- Fade times for Voice Over mode
- Pan settings for AutoPan mode

**Integration Tasks:**
- Voice Over gain reduction UI
- Fade time validation for Voice Over
- AutoPan endpoint values

---

### 8.4 Search Menu Integration (OCC118)

**Dependencies:**
- Preview output used for search track preview
- Display names shown in recent files

**Integration Tasks:**
- Use preview output for double-click preview
- Coordinate preview state

---

### 8.5 Audio Engine Integration

**Dependencies:**
- Master/Slave link execution during playback
- Voice Over gain automation
- AutoPan automation
- Preview mode routing
- Play Stack independent playback

**Integration Tasks:**
- Link trigger on play/stop
- Real-time gain automation for Voice Over
- Real-time pan automation for AutoPan
- Preview output routing
- Play Stack audio routing

---

## 9. Testing Strategy

### 9.1 Unit Tests

```typescript
// Master/Slave Links
describe('MasterSlaveLinksService', () => {
  test('creates link with play slaves', async () => {
    // Create link with play slaves
    // Verify slaves play when master plays
  });

  test('creates link with stop slaves', async () => {
    // Create link with stop slaves
    // Verify slaves stop when master plays
  });

  test('validates broken links', async () => {
    // Create link with unused master
    // Verify marked as broken
  });

  test('deletes broken links', async () => {
    // Create broken links
    // Delete broken links
    // Verify only broken links deleted
  });
});

// Voice Over mode
describe('VoiceOverMode', () => {
  test('fades down slaves during master play', async () => {
    // Create Voice Over link
    // Play master
    // Verify slave gain reduced
  });

  test('requires non-zero fade times', async () => {
    // Create Voice Over link with zero fade times
    // Verify error thrown
  });
});

// Play Groups
describe('PlayGroups', () => {
  test('stops other group members when one plays', async () => {
    // Assign buttons to group
    // Play one button
    // Verify others stopped
  });

  test('buzzer group locks out after first trigger', async () => {
    // Assign buttons to buzzer group
    // Trigger first button via GPI
    // Attempt to trigger second button
    // Verify blocked
  });
});

// Play Stack
describe('PlayStack', () => {
  test('plays tracks in sequence', async () => {
    // Add tracks to stack
    // Play stack
    // Verify tracks play in order
  });

  test('loops when loop enabled', async () => {
    // Add tracks, enable loop
    // Play to end
    // Verify returns to start
  });

  test('fades out after duration', async () => {
    // Set play duration
    // Play stack
    // Verify fades out at duration
  });
});
```

### 9.2 Integration Tests

```typescript
// End-to-end link execution
describe('MasterSlaveExecution', () => {
  test('master plays slaves', async () => {
    // Create link
    // Play master button
    // Verify slaves play
  });

  test('Voice Over reduces slave gain', async () => {
    // Create Voice Over link
    // Play master
    // Verify slave gain reduced
    // Verify slave restored after master stops
  });
});

// Play Stack integration
describe('PlayStackIntegration', () => {
  test('Group 24 fades out stack', async () => {
    // Play stack
    // Play button in Group 24
    // Verify stack fades out
  });

  test('stack plays independently from main buttons', async () => {
    // Play stack
    // Play main button
    // Verify both play simultaneously
  });
});
```

### 9.3 Performance Tests

```typescript
// Link execution performance
describe('LinkPerformance', () => {
  test('executes 100 links in < 100ms', async () => {
    // Create 100 links
    // Trigger all masters
    // Verify execution < 100ms
  });
});

// Link visualization performance
describe('VisualizationPerformance', () => {
  test('renders 50 links in < 16ms', async () => {
    // Create 50 links
    // Render visualization
    // Verify < 16ms (60fps)
  });
});
```

---

## 10. Documentation Requirements

### 10.1 API Documentation

- OpenAPI/Swagger spec for all endpoints
- WebSocket event documentation
- Code examples for complex operations (Voice Over, AutoPan)

### 10.2 Architecture Documentation

- Master/Slave link execution flow
- Play groups exclusive playback logic
- Play Stack independent architecture
- Buzzer group timeout mechanism

### 10.3 User Documentation

- Master/Slave link creation guide
- Voice Over setup tutorial
- Play Stack usage guide
- Buzzer group configuration

---

## 11. Success Metrics

### 11.1 Performance Metrics

| Metric | Target | Measurement |
|--------|--------|-------------|
| Link execution latency | < 10ms | Time from master trigger to slave action |
| Voice Over fade smoothness | No audible steps | Audio analysis |
| Play Stack playback | Sample-accurate | Gap/overlap detection |
| Link visualization render | < 16ms | 60fps rendering |

### 11.2 Functional Metrics

| Metric | Target | Measurement |
|--------|--------|-------------|
| Maximum links | 100 | System limit |
| Maximum play groups | 25 | System limit |
| Play Stack capacity | 20 tracks | System limit |
| Buzzer timeout accuracy | ±100ms | Clock accuracy |

---

## 12. Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Link/Group conflicts | High | Medium | Automatic conflict resolution |
| Voice Over timing errors | Medium | High | Fade time validation, testing |
| Play Stack audio glitches | Low | High | Independent audio thread, testing |
| Buzzer timeout drift | Medium | Medium | Accurate clock sync, testing |
| Link visualization lag | Low | Low | Canvas rendering, caching |

---

## 13. Future Enhancements

### 13.1 Advanced Link Features

- Conditional links (only trigger if condition met)
- Delayed link execution (trigger after X seconds)
- Link chains (master→slave→slave...)
- Link templates for common patterns

### 13.2 Enhanced Play Stack

- Multiple play stacks
- Crossfade between stack tracks
- Smart playlist generation
- BPM matching for seamless transitions

### 13.3 Link Visualization Improvements

- 3D link visualization
- Animated link flow
- Link strength indicators (how often triggered)
- Link conflict warnings

### 13.4 Advanced Grouping

- Hierarchical groups (groups within groups)
- Dynamic group membership
- Group templates

---

## 14. Sprint Summary

### Estimated Total Effort

| Phase | Effort (hours) | Priority |
|-------|----------------|----------|
| Phase 1: Preview & Output Assignment | 20-24 | P0 |
| Phase 2: Display Names & Refresh | 16-20 | P0 |
| Phase 3: Links - Basic | 32-40 | P0 |
| Phase 4: Links - Advanced Modes | 24-32 | P1 |
| Phase 5: Link Visualization | 16-20 | P2 |
| Phase 6: Play Groups | 24-28 | P1 |
| Phase 7: Play Stack | 40-48 | P1 |
| Phase 8: Audio Generation | 24-32 | P2 |
| Phase 9: Statistics & Viewer | 16-20 | P2 |
| Phase 10: Utility Features | 20-24 | P3 |
| **Total** | **232-288 hours** | - |

### Critical Path

1. Preview & Output Assignment → Display Names (P0 dependencies)
2. Links Basic → Links Advanced → Link Visualization (Sequential)
3. Play Groups (Parallel with Links Advanced)
4. Play Stack (Integration with Groups)
5. Utilities (Parallel, lower priority)

### Recommended Sprint Allocation

- **Sprint 1-2:** Phase 1-2 (Preview, Output, Display Names, Refresh)
- **Sprint 3-5:** Phase 3 (Master/Slave Links - Basic)
- **Sprint 5-6:** Phase 4 (Master/Slave Links - Advanced Modes)
- **Sprint 6-7:** Phase 5 (Link Visualization) + Phase 6 (Play Groups - parallel)
- **Sprint 8-10:** Phase 7 (Play Stack)
- **Sprint 10-11:** Phase 8 (Audio Generation)
- **Sprint 11-12:** Phase 9 (Statistics & Viewer)
- **Sprint 12-13:** Phase 10 (Utility Features)

---

## Appendix A: SpotOn Manual References

**Source:** SpotOn Manual - Section 06 - Global Menu

**Key Features:**
- Pages 1-2: Global Menu overview, utilities list
- Pages 12-17: Display Names editor, track reordering
- Pages 13-14: Preview output assignment
- Pages 14-15: Change output assignment
- Pages 18-31: Master/Slave Links (comprehensive coverage)
- Pages 22-24: Play Groups and Buzzer Groups
- Pages 25-31: Voice Over, AutoPan, Pause/Unpause modes, link visualization
- Pages 32-41: Play Stack (detailed coverage)
- Pages 2-3: Generate Timecode utility
- Pages 4-5: Click Track generator
- Pages 5-7: Statistics and file management

---

## Appendix B: Related Documents

- **OCC114:** Backend Architecture Audit (foundation)
- **OCC115:** File Menu Backend Features (file loading integration)
- **OCC116:** Setup Menu Backend Features (MIDI, GPI integration)
- **OCC117:** Display/Edit Menu Backend Features (audio setup, fade times)
- **OCC118:** Search Menu Backend Features (preview output usage)

---

**Document Prepared By:** Backend Architecture Analysis Sprint
**Next Steps:** Analyze Section 07 (Options Menu) - OCC120
