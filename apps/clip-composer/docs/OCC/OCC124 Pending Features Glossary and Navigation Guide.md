# OCC124 - Pending Features Glossary and Navigation Guide

**Version:** 1.0
**Date:** 2025-11-12
**Status:** Reference / Navigation Guide
**Purpose:** Fast navigation and targeted search for OCC115-OCC123 pending features

---

## Executive Summary

This document serves as a **comprehensive glossary and navigation guide** for all pending backend features documented in OCC115-OCC123. Use this guide to:

1. **Quickly locate** specific features across 9 menu documentation files
2. **Target precise sections** without reading entire documents
3. **Understand current state** via OCC114 baseline
4. **Execute efficient searches** using provided grep patterns
5. **Plan implementation** with sprint effort estimates

**Total Pending Features:** 100+ features across 9 menus
**Total Estimated Effort:** ~800-1000 hours
**Documentation Size:** ~38,000 lines across OCC115-OCC123

---

## Quick Reference: Document Map

| Doc | Menu | Size | Sprints | Key Features | Priority |
|-----|------|------|---------|--------------|----------|
| **OCC114** | Current State | 490 lines | N/A | Baseline audit | Reference |
| **OCC115** | File Menu | 1,304 lines | 8 sprints | Auto-backup, MRU, Missing files, Package system | CRITICAL |
| **OCC116** | Setup Menu | 1,295 lines | 5 sprints | External tools, HotKeys, MIDI devices, Monitoring | HIGH |
| **OCC117** | Display/Edit Menu | 1,541 lines | 6 sprints | Undo/Redo, Paste Special, Level meters, Page ops | HIGH |
| **OCC118** | Search Menu | 1,368 lines | 8 phases | Recent files (1000), Metadata, Preview, Drag-drop | MEDIUM |
| **OCC119** | Global Menu | 2,377 lines | 13 sprints | Master/Slave links, Play Stack, Preview output | VERY HIGH |
| **OCC120** | Options Menu | 477 lines | 4 phases | Settings system, Feature flags, Preferences | MEDIUM |
| **OCC121** | Info Menu | 999 lines | 6 phases | Playout logs, Event logs, Session notes, Status | HIGH |
| **OCC122** | Engineering Menu | 735 lines | 3 weeks | Timecode, DTMF, PBus, Network, Playout logs | MEDIUM |
| **OCC123** | Admin Menu | 900 lines | 3 weeks | Auth, Devices, Licensing, Diagnostics | MEDIUM |

---

## Part 1: Current State Baseline (OCC114)

### What Exists Today

**Search Strategy:** `grep "✅" OCC114` to find implemented features

#### Implemented ✅
- Basic session format (JSON, v1.0.0)
- Minimal audio settings persistence (sample rate, buffer size, device)
- AudioEngine with SDK integration
- ClipGrid UI (8 tabs × 48 buttons)
- Basic clip metadata (path, name, color, group, fade, gain, loop)
- Transport controls (play/stop/panic)
- Performance metrics (CPU usage atomics)

#### Missing ❌
**Search Strategy:** `grep "❌" OCC114` to find gaps

- No comprehensive preferences system
- No logging/diagnostics framework
- No file organization patterns
- No recent file history (MRU)
- No auto-save/backup system
- No search/filter infrastructure
- No engineering/admin tools
- No undo/redo
- No level meters
- No Master/Slave links
- No Play Stack

### Gap Priority Summary

**CRITICAL (Data Safety):**
- Auto-backup/restore (OCC115:184-357)
- Session validation (OCC115:84)
- Crash recovery (OCC115:296-335)

**HIGH (Production Readiness):**
- Logging framework (OCC115:812-1023, OCC121)
- Recent files MRU (OCC115:360-550)
- Missing file resolution (OCC115:553-808)

**MEDIUM (Workflow):**
- Undo/Redo system (OCC117:289-555)
- Paste Special (OCC117:557-826)
- Search/filter (OCC118)

---

## Part 2: Feature Location Matrix

### By Implementation Priority

#### P0: CRITICAL Features (Must implement first)

| Feature | Document | Line Range | Search Term | Effort | Dependencies |
|---------|----------|------------|-------------|--------|--------------|
| Auto-Backup System | OCC115 | 184-357 | `### Sprint 2:` | 2-3 weeks | Sprint 1 |
| Restore from Backup | OCC115 | 260-335 | `RestoreBackupDialog` | Part of Sprint 2 | Auto-Backup |
| Recent Files (MRU) | OCC115 | 360-550 | `### Sprint 3:` | 3-5 days | Sprint 1 |
| Missing File Resolution | OCC115 | 553-808 | `### Sprint 4:` | 3-4 weeks | Sprints 1-2 |
| Session Templates | OCC115 | 100-132 | `loadNewSession` | Part of Sprint 1 | None |
| Undo/Redo System | OCC117 | 289-555 | `class UndoManager` | 2-3 weeks | None |
| Master/Slave Links (Basic) | OCC119 | 71-148, 1593-1610 | `### 2.1 Master/Slave Link` | 32-40 hours | None |
| Play Groups | OCC119 | 150-206, 1656-1672 | `### 2.2 Play Groups` | 24-28 hours | None |

#### P1: HIGH Priority Features

| Feature | Document | Line Range | Search Term | Effort | Dependencies |
|---------|----------|------------|-------------|--------|--------------|
| Event Logging System | OCC115 | 886-1000 | `class EventLogger` | 1-2 weeks | Sprint 1 |
| HotKey Configuration | OCC116 | 248-570 | `### Sprint 10:` | 1-2 weeks | None |
| MIDI Device Management | OCC116 | 573-850 | `### Sprint 11:` | 2-3 weeks | Event Logging |
| Paste Special | OCC117 | 557-826 | `class PasteSpecialDialog` | 2-3 weeks | Undo/Redo |
| Level Meters | OCC117 | 1062-1304 | `### Sprint 19:` | 2-3 weeks | Event Logging |
| Play Stack | OCC119 | 208-285, 1675-1698 | `### 2.3 Play Stack` | 40-48 hours | Play Groups |
| Playout Logging | OCC121 | 179-372 | `### 2.3 Playout Logging` | 20-24 hours | None |
| Voice Over Mode | OCC119 | 1614-1632 | `### Phase 4:` | 24-32 hours | Links Basic |

#### P2: MEDIUM Priority Features

| Feature | Document | Line Range | Search Term | Effort | Dependencies |
|---------|----------|------------|-------------|--------|--------------|
| External Tool Registry | OCC116 | 55-244 | `### Sprint 9:` | 3-5 days | Sprint 1 (OCC115) |
| Display Preferences | OCC117 | 58-286 | `### Sprint 15:` | 3-5 days | Sprint 1 (OCC115) |
| Page Operations | OCC117 | 829-1059 | `### Sprint 18:` | 1-2 weeks | Undo/Redo, Paste Special |
| Recent File Search | OCC118 | 61-130 | `### 2.1 Recent Files` | 16-20 hours | None |
| WAV Metadata Parsing | OCC118 | 169-202 | `### 2.3 WAV Metadata` | 20-24 hours | Recent Files |
| Settings System | OCC120 | 34-265 | `class SettingsService` | 40-56 hours | None |
| Session Notes | OCC121 | 36-87 | `### 2.1 Session Notes` | 6-8 hours | None |

### By Menu Category

#### File Menu (OCC115) - Data Safety Focus

**Sprint Map:**
- Sprint 1 (2 weeks): Application data folder, templates, file validation (lines 63-181)
- Sprint 2 (3 weeks): Auto-backup, restore, crash recovery (lines 184-357)
- Sprint 3 (1 week): Recent files MRU (lines 360-550)
- Sprint 4 (4 weeks): Missing file wizard (lines 553-808)
- Sprint 5 (2 weeks): Status logs, event logging (lines 812-1023)
- Sprint 6-8: Track list export, block operations, package system (lines 1026-1072)

**Search Patterns:**
```bash
# Find auto-backup implementation
grep -n "performAutoBackup\|rotateBackups" OCC115

# Find missing file resolution
grep -n "LoadReport\|LocateFileWizard" OCC115

# Find recent files MRU
grep -n "RecentFilesManager\|addRecentFile" OCC115
```

#### Setup Menu (OCC116) - External Control

**Sprint Map:**
- Sprint 9 (1 week): External tools (lines 55-244)
- Sprint 10 (2 weeks): HotKey scope and modes (lines 248-570)
- Sprint 11 (3 weeks): MIDI devices, multiple inputs/outputs (lines 573-850)
- Sprint 12 (1 week): MIDI monitoring/logging (lines 853-1048)
- Sprint 13-14: Deferred (GPI, Timecode)

**Search Patterns:**
```bash
# Find HotKey system
grep -n "class HotKeyManager\|Scope::Global" OCC116

# Find MIDI management
grep -n "class MIDIDeviceManager\|handleIncomingMidiMessage" OCC116

# Find external tools
grep -n "class ExternalToolManager\|launchTool" OCC116
```

#### Display/Edit Menu (OCC117) - Editing Workflows

**Sprint Map:**
- Sprint 15 (1 week): Display preferences (lines 58-286)
- Sprint 16 (3 weeks): Undo/Redo with Command pattern (lines 289-555)
- Sprint 17 (3 weeks): Paste Special with AutoFill (lines 557-826)
- Sprint 18 (2 weeks): Page clipboard operations (lines 829-1059)
- Sprint 19 (3 weeks): Level meters and play history (lines 1062-1304)
- Sprint 20: Deferred (Grid layout)

**Search Patterns:**
```bash
# Find undo/redo system
grep -n "class UndoManager\|class.*Command" OCC117

# Find paste special
grep -n "class PasteSpecialDialog\|AutoFill" OCC117

# Find level meters
grep -n "class LevelMetersWindow\|PlayHistoryLogger" OCC117
```

#### Search Menu (OCC118) - File Discovery

**Phase Map:**
- Phase 1-2 (Sprints 1-3): Recent files circular buffer, search/sort (lines 61-227)
- Phase 3-4 (Sprints 3-4): WAV metadata parsing, highlighting (lines 169-202, 720-736)
- Phase 5 (Sprint 5): Remote file support (lines 244-273)
- Phase 6 (Sprints 5-6): Track preview player (lines 204-242, 763-779)
- Phase 7 (Sprint 6): Drag-drop with Alt+Top/Tail (lines 276-316, 781-801)

**Search Patterns:**
```bash
# Find recent files tracking
grep -n "interface RecentFileEntry\|circular buffer" OCC118

# Find WAV metadata
grep -n "class WavMetadataParser\|RIFF" OCC118

# Find preview system
grep -n "class TrackPreviewService\|startPreview" OCC118
```

#### Global Menu (OCC119) - Advanced Features (MOST COMPLEX)

**Phase Map:**
- Phase 1-2 (Sprints 1-3): Preview output, output assignment, display names, refresh tracks (lines 1550-1588)
- Phase 3-4 (Sprints 3-6): Master/Slave Links basic + advanced modes (lines 71-148, 1593-1632)
- Phase 5 (Sprints 6-7): Link visualization (lines 1635-1652)
- Phase 6 (Sprints 7-8): Play Groups and Buzzer Groups (lines 150-206, 1656-1672)
- Phase 7 (Sprints 8-10): Play Stack independent player (lines 208-285, 1675-1698)
- Phase 8-10 (Sprints 10-13): Utilities (timecode gen, click track, stats, etc.)

**Search Patterns:**
```bash
# Find Master/Slave links
grep -n "interface MasterSlaveLink\|LinkMode" OCC119

# Find Play Stack
grep -n "interface PlayStack\|PlayStackTrack" OCC119

# Find Play Groups
grep -n "interface PlayGroup\|buzzerTimeout" OCC119

# Find preview output
grep -n "PreviewOutputAssignment\|PreviewMode" OCC119
```

#### Options Menu (OCC120) - Settings & Preferences

**Phase Map:**
- Phase 1 (Sprint 1): Core settings infrastructure (lines 34-265, 363-377)
- Phase 2 (Sprint 2): Display & input settings (lines 56-82, 378-384)
- Phase 3 (Sprint 2): Audio & feature flags (lines 83-108, 385-395)
- Phase 4 (Sprint 3): MIDI & advanced settings (lines 100-118, 396-406)

**Search Patterns:**
```bash
# Find settings system
grep -n "class SettingsService\|interface SettingsCategory" OCC120

# Find feature flags
grep -n "interface FeatureFlags" OCC120

# Find display settings
grep -n "interface DisplaySettings" OCC120
```

#### Info Menu (OCC121) - Logging & Observability

**Phase Map:**
- Phase 1 (Sprint 1): Session notes (lines 36-87, 712-721)
- Phase 2 (Sprint 1): System status (lines 89-177, 722-732)
- Phase 3 (Sprint 2-3): Playout logging infrastructure (lines 179-372, 734-746)
- Phase 4 (Sprint 3-4): Extended metadata columns (lines 240-283, 747-758)
- Phase 5 (Sprint 4): Event logging (lines 374-447, 760-769)
- Phase 6 (Sprint 4-5): Log archival (lines 449-489, 771-782)

**Search Patterns:**
```bash
# Find playout logging
grep -n "interface PlayoutLogEntry\|TriggerSource" OCC121

# Find event logging
grep -n "class EventLogService\|enum EventType" OCC121

# Find log archival
grep -n "class LogArchivalService\|retentionDays" OCC121
```

#### Engineering Menu (OCC122) - Professional Features

**Phase Map:**
- Phase 1 (Week 1): Core infrastructure, property system (lines 22-53, 481-492)
- Phase 2 (Week 2): Audio processing (DTMF, LTC timecode) (lines 79-156, 493-506)
- Phase 3 (Week 3): Network, triggers, playout logs (lines 157-278, 507-520)

**Search Patterns:**
```bash
# Find DTMF decoder
grep -n "interface DTMFConfig\|Goertzel" OCC122

# Find timecode system
grep -n "interface TimecodeConfig\|LTC" OCC122

# Find timecode triggers
grep -n "interface TimecodeTrigger\|EDL" OCC122
```

#### Admin Menu (OCC123) - System Administration

**Phase Map:**
- Phase 1 (Week 1): Admin auth, folder management (lines 22-84, 551-564)
- Phase 2 (Week 2): Audio device management (lines 136-245, 565-578)
- Phase 3 (Week 3): Licensing, diagnostics (lines 197-228, 579-592)

**Search Patterns:**
```bash
# Find admin authentication
grep -n "interface AdminSession\|AdminAuditLog" OCC123

# Find output devices
grep -n "interface OutputDevice\|DevicePatch" OCC123

# Find licensing
grep -n "interface License\|UnlockCodeValidation" OCC123
```

---

## Part 3: Common Implementation Patterns

### Pattern 1: CRUD Operations

**Found in:** Most sprints
**Search:** `GET /api/.*POST /api/.*PUT /api/.*DELETE /api/`

**Typical Structure:**
```
Lines 1-50: Data models (TypeScript interfaces)
Lines 51-100: Service class implementation
Lines 101-150: API endpoints definition
Lines 151-200: Database schema
Lines 201-250: Integration points
```

### Pattern 2: UI Component Dialogs

**Found in:** OCC115, OCC116, OCC117, OCC119
**Search:** `class.*Dialog.*Component`

**Typical Structure:**
```
Lines 1-50: Dialog purpose and features
Lines 51-150: Implementation code with UI components
Lines 151-200: Event handlers
Lines 201-250: Acceptance criteria
```

### Pattern 3: Audio/Media Processing

**Found in:** OCC116, OCC118, OCC119, OCC122, OCC123
**Search:** `Audio.*\|MIDI.*\|Timecode.*`

**Typical Structure:**
```
Lines 1-100: Configuration interfaces
Lines 101-200: Processing service class
Lines 201-300: Real-time constraints
Lines 301-400: Integration with AudioEngine
```

### Pattern 4: Logging Systems

**Found in:** OCC115, OCC121, OCC122
**Search:** `Log.*Entry\|logger\|archival`

**Typical Structure:**
```
Lines 1-50: Log entry interface
Lines 51-150: Logger service class
Lines 151-250: Archival/rotation logic
Lines 251-350: Database schema
```

---

## Part 4: Cross-Cutting Concerns

### Threading Model (Applies to ALL features)

**Reference:** OCC096 SDK Integration Patterns (mentioned in OCC114:201-218, OCC115:1119-1136)

**Rules:**
- **Message Thread:** File I/O, UI updates, session save/load
- **Audio Thread:** NO allocations, NO locks, NO I/O
- **Background Threads:** Auto-backup, logging, waveform rendering

**Search across all docs:**
```bash
grep -n "threading\|Message Thread\|Audio Thread" OCC11*
```

### Data Models Location Pattern

**Every document follows:**
```
Section 3: Data Models
  - Core entities (interfaces)
  - Database schema (SQL)
  - Validation rules
```

**Fast navigation:**
```bash
# Find all data model sections
grep -n "^## 3\. Data Models" OCC11*

# Find specific interface
grep -n "^interface.*{$" OCC115
```

### API Contracts Location Pattern

**Every document follows:**
```
Section 4: API Contracts
  - REST endpoints
  - WebSocket events
  - Request/Response types
```

**Fast navigation:**
```bash
# Find all API sections
grep -n "^## 4\. API Contracts" OCC11*

# Find specific endpoint
grep -n "^// GET /api/\|^// POST /api/" OCC115
```

### Testing Strategy Location Pattern

**Every document follows:**
```
Section 9 or later: Testing Strategy
  - Unit tests
  - Integration tests
  - Performance tests (if applicable)
```

**Fast navigation:**
```bash
# Find testing sections
grep -n "^## .*Testing Strategy" OCC11*

# Find test examples
grep -n "describe\('.*'" OCC115
```

---

## Part 5: Search Strategies by Task Type

### Task: "I need to implement auto-backup"

**Step 1:** Check current state
```bash
grep -n "auto.*backup\|auto.*save" OCC114  # Expected: Line ~269, 494
```

**Step 2:** Find feature sprint
```bash
grep -n "Auto-Backup and Restore System" OCC115  # Expected: Line ~184
```

**Step 3:** Read targeted sections
- OCC115:184-357 (full sprint plan)
- OCC115:200-240 (implementation code)
- OCC115:338-357 (acceptance criteria)

**Step 4:** Find dependencies
```bash
grep -n "Dependencies:" OCC115 | head -n 5  # Check Sprint 2 dependencies
```

### Task: "I need to understand Master/Slave links"

**Step 1:** Check if it exists
```bash
grep -n "Master.*Slave\|master.*slave" OCC114  # Expected: Line ~434 (NOT implemented)
```

**Step 2:** Find in Global menu
```bash
grep -n "### 2.1 Master/Slave" OCC119  # Expected: Line ~71
```

**Step 3:** Understand complexity
```bash
grep -n "Complexity:\|Estimated Duration:" OCC119 | grep -A1 "Sprint 3:"  # 32-40 hours
```

**Step 4:** Read in stages
- OCC119:71-148 (data model and requirements)
- OCC119:1593-1610 (Phase 3: Basic implementation)
- OCC119:1614-1632 (Phase 4: Advanced modes - Voice Over, AutoPan)
- OCC119:1635-1652 (Phase 5: Visualization)

### Task: "I need to add undo/redo"

**Step 1:** Confirm not implemented
```bash
grep -n "undo\|redo" OCC114  # Expected: Line ~401 (Edit Menu, not in code)
```

**Step 2:** Find in Display/Edit menu
```bash
grep -n "### Sprint 16: Undo/Redo" OCC117  # Expected: Line ~289
```

**Step 3:** Understand Command pattern
```bash
grep -n "class Command\|class UndoManager" OCC117  # Lines 311-349
```

**Step 4:** Find concrete examples
```bash
grep -n "class.*Command.*:" OCC117  # Find EditClipCommand, ClearButtonsCommand, etc.
```

### Task: "I need to implement MIDI monitoring"

**Step 1:** Check current MIDI support
```bash
grep -n "MIDI" OCC114  # Expected: Lines about MIDI, basic support
```

**Step 2:** Find MIDI monitoring sprint
```bash
grep -n "### Sprint 12: MIDI Monitoring" OCC116  # Expected: Line ~853
```

**Step 3:** Check dependencies
```bash
grep -n "Dependencies: Sprint 11" OCC116  # Must do MIDI Device Management first
```

**Step 4:** Read related sections
- OCC116:853-1048 (MIDI Monitoring)
- OCC116:573-850 (MIDI Device Management - prerequisite)
- OCC120:100-118 (MIDI settings in Options menu)

### Task: "I need to add playout logs for royalties"

**Step 1:** Understand requirement
```bash
grep -n "royalty\|playout.*log" OCC121  # Expected: Line ~179
```

**Step 2:** Find trigger source codes
```bash
grep -n "enum TriggerSource" OCC121  # Expected: Line ~190 (26 different sources!)
```

**Step 3:** Read implementation
- OCC121:179-372 (full playout logging system)
- OCC121:224-283 (extended metadata columns)
- OCC121:449-489 (archival system)

**Step 4:** Check related in Engineering menu
```bash
grep -n "playout.*log" OCC122  # Additional engineering config
```

---

## Part 6: Effort Estimation Quick Reference

### By Sprint Size

**Extra Large (XL) - 3-4 weeks:**
- Missing File Resolution (OCC115:553-808)
- Package System (OCC115:1058-1072)
- Master/Slave Links Advanced (OCC119:1614-1632)
- Play Stack (OCC119:1675-1698)

**Large (L) - 2-3 weeks:**
- Auto-Backup & Restore (OCC115:184-357)
- MIDI Device Management (OCC116:573-850)
- Undo/Redo System (OCC117:289-555)
- Paste Special (OCC117:557-826)
- Level Meters & Play History (OCC117:1062-1304)
- Playout Logging Infrastructure (OCC121:734-746)

**Medium (M) - 1-2 weeks:**
- Session Management Foundation (OCC115:63-181)
- HotKey Configuration (OCC116:248-570)
- Display Preferences (OCC117:58-286)
- Page Operations (OCC117:829-1059)
- Master/Slave Links Basic (OCC119:1593-1610)
- Play Groups (OCC119:1656-1672)

**Small (S) - 3-5 days:**
- Recent Files MRU (OCC115:360-550)
- External Tool Registry (OCC116:55-244)
- MIDI Monitoring (OCC116:853-1048)
- Track List Export (OCC115:1026-1041)
- Session Notes (OCC121:712-721)

### Total Effort by Priority

**P0 (CRITICAL):** ~300-400 hours
- File Menu safety features (auto-backup, restore, MRU, missing files)
- Master/Slave Links basic
- Play Groups
- Undo/Redo

**P1 (HIGH):** ~350-450 hours
- Event logging
- HotKey/MIDI management
- Paste Special
- Level Meters
- Play Stack
- Playout Logging

**P2 (MEDIUM):** ~200-300 hours
- External tools
- Display preferences
- Search/filter
- Settings system
- Engineering features

**Total: ~850-1150 hours** (approximately 6-9 months for 1 developer, 3-5 months for 2 developers)

---

## Part 7: Integration Dependencies

### Critical Path Analysis

**Path 1: Data Safety (MUST DO FIRST)**
```
OCC115 Sprint 1 (Folders)
  ↓
OCC115 Sprint 2 (Auto-backup) ← CRITICAL
  ↓
OCC115 Sprint 3 (Recent Files)
  ↓
OCC115 Sprint 4 (Missing Files)
```

**Path 2: Editing Workflows**
```
OCC117 Sprint 16 (Undo/Redo)
  ↓
OCC117 Sprint 17 (Paste Special)
  ↓
OCC117 Sprint 18 (Page Operations)
```

**Path 3: External Control**
```
OCC116 Sprint 10 (HotKeys)
  ║
  ╠═══> OCC116 Sprint 11 (MIDI Devices)
  ║       ↓
  ║     OCC116 Sprint 12 (MIDI Monitoring)
  ║
  ╚═══> OCC120 (Settings for both)
```

**Path 4: Advanced Features (After Critical Path)**
```
OCC119 Phase 3 (Links Basic)
  ↓
OCC119 Phase 4 (Links Advanced) ← Voice Over, AutoPan
  ↓
OCC119 Phase 6 (Play Groups)
  ↓
OCC119 Phase 7 (Play Stack) ← Integrates with Group 24
```

**Path 5: Observability (Parallel)**
```
OCC115 Sprint 5 (Event Logging)
  ↓
OCC121 Phase 3 (Playout Logs) ← For royalty compliance
  ↓
OCC121 Phase 6 (Log Archival)
```

### Cross-Document Dependencies

**Display Settings affect:**
- OCC117:58-286 (Display preferences)
- OCC115:96-104 (Session templates)
- OCC119:348-422 (Display names editor)

**MIDI Settings affect:**
- OCC116:573-850 (MIDI Device Management)
- OCC116:853-1048 (MIDI Monitoring)
- OCC120:100-118 (MIDI settings)
- OCC119:587-656 (MIDI in Master/Slave links)

**Audio Engine integration required for:**
- OCC117:1062-1304 (Level Meters)
- OCC119:208-285 (Play Stack)
- OCC121:179-372 (Playout Logging)
- OCC122:79-156 (Timecode/DTMF)
- OCC123:136-245 (Output Devices)

---

## Part 8: Common Pitfalls & Solutions

### Pitfall 1: Reading entire documents

**Problem:** OCC119 is 2,377 lines - reading it all burns tokens
**Solution:** Use line ranges from this glossary

**Example:**
```
❌ BAD:  Read entire OCC119
✅ GOOD: Read OCC119:71-148 (Master/Slave data model only)
```

### Pitfall 2: Missing dependencies

**Problem:** Implementing Sprint 4 without Sprint 1 completed
**Solution:** Always check "Dependencies:" field in sprint header

**Example:**
```bash
# Before implementing Missing File Resolution
grep -n "Dependencies:.*Sprint" OCC115 | grep "Sprint 4"
# Output: Dependencies: Sprint 1 (Application Data Folder), Sprint 2 (Event Logs)
```

### Pitfall 3: Implementing in wrong order

**Problem:** Adding Play Stack before Play Groups
**Solution:** Follow phase numbers and check integration points

**Example:**
```
OCC119:1675 shows "Phase 7: Play Stack"
OCC119:1656 shows "Phase 6: Play Groups"
✅ Do Phase 6 first - Play Stack depends on Group 24 for fadeout
```

### Pitfall 4: Ignoring threading model

**Problem:** Adding file I/O to audio thread
**Solution:** Check OCC096 patterns and search for "Audio Thread" constraints

**Example:**
```bash
# Check threading constraints for any feature
grep -n "Audio Thread\|Message Thread" OCC115
```

### Pitfall 5: Missing cross-document impacts

**Problem:** Changing MIDI without checking Options menu
**Solution:** Search for feature name across all docs

**Example:**
```bash
# Check all references to MIDI
grep -n "MIDI" OCC11*.md | grep -i "menu\|setting\|config"
```

---

## Part 9: Quick Lookup Tables

### Table 1: Find Feature by Name

| Feature Name | Primary Doc | Line Range | Search Term |
|--------------|-------------|------------|-------------|
| Auto-Backup | OCC115 | 184-357 | `### Sprint 2:` |
| Auto-Fill MIDI | OCC117 | 699-743 | `AutoFill MIDI Notes` |
| Backup Rotation | OCC115 | 240-257 | `rotateBackups` |
| Buzzer Groups | OCC119 | 155-175 | `buzzerTimeout` |
| CD Burning | OCC119 | 772-819 | `CDBurningService` |
| Click Track Generator | OCC119 | 547-589 | `ClickTrackGenerator` |
| Clear Attributes | OCC119 | 821-865 | `ClearAttributesService` |
| Crash Recovery | OCC115 | 296-335 | `checkForCrashRecovery` |
| Display Names Editor | OCC119 | 376-422 | `DisplayNamesService` |
| DTMF Decoder | OCC122 | 79-110 | `interface DTMFConfig` |
| Event Logging | OCC115 | 886-1000 | `class EventLogger` |
| External Tools | OCC116 | 55-244 | `ExternalToolManager` |
| File Extension Validation | OCC115 | 134-164 | `validateSessionExtension` |
| Fill/Clear Buttons | OCC117 | 911-1030 | `FillButtonsDialog` |
| HotKey Scope | OCC116 | 254-383 | `enum Scope` |
| Level Meters | OCC117 | 1069-1174 | `LevelMetersWindow` |
| Link Visualization | OCC119 | 1635-1652 | `LinkVisualization` |
| Load Report Dialog | OCC115 | 625-682 | `LoadReportDialog` |
| Locate File Wizard | OCC115 | 686-747 | `LocateFileWizard` |
| Master/Slave Links | OCC119 | 71-148 | `interface MasterSlaveLink` |
| MIDI Monitoring | OCC116 | 859-988 | `MIDIMonitorDialog` |
| Missing File Detection | OCC115 | 574-622 | `LoadReport` |
| Overlapped Mode | OCC116 | 334-382 | `MultiButtonAction::Overlapped` |
| Package System | OCC115 | 1058-1072 | Deferred, complex |
| Page Operations | OCC117 | 848-909 | `copyPage\|pastePage` |
| Paste Special | OCC117 | 566-695 | `PasteSpecialDialog` |
| Play Groups | OCC119 | 150-206 | `interface PlayGroup` |
| Play History Logging | OCC117 | 1180-1226 | `PlayHistoryLogger` |
| Play Stack | OCC119 | 208-285 | `interface PlayStack` |
| Playout Logs | OCC121 | 179-372 | `PlayoutLogEntry` |
| Preview Output | OCC119 | 286-343 | `PreviewOutputAssignment` |
| Recent Files (MRU) | OCC115 | 369-430 | `RecentFilesManager` |
| Refresh Tracks | OCC119 | 435-489 | `TrackRefreshService` |
| Session Notes | OCC121 | 44-76 | `SessionNotes` |
| Session Templates | OCC115 | 100-132 | `loadNewSession` |
| Settings System | OCC120 | 41-265 | `SettingsService` |
| Statistics | OCC119 | 632-683 | `FileStatistics` |
| Status Log | OCC115 | 819-882 | `saveStatusLog` |
| Timecode Generator | OCC119 | 491-545 | `TimecodeGenerator` |
| Timecode System | OCC122 | 118-162 | `TimecodeConfig` |
| Timecode Triggers | OCC122 | 164-196 | `TimecodeTrigger` |
| Track Preview | OCC118 | 204-242 | `TrackPreviewService` |
| Undo/Redo | OCC117 | 298-349 | `UndoManager` |
| Voice Over Mode | OCC119 | 94-98 | `VOICE_OVER` |
| WAV Metadata Parser | OCC118 | 169-202 | `WavMetadataParser` |

### Table 2: Find by Complexity/Effort

**3-5 days (Small):**
- External Tool Registry (OCC116:55-244)
- Recent Files MRU (OCC115:360-550)
- MIDI Monitoring (OCC116:853-1048)
- Display Preferences (OCC117:58-286)
- Session Notes (OCC121:712-721)
- Track List Export (OCC115:1026-1041)

**1-2 weeks (Medium):**
- Session Management Foundation (OCC115:63-181)
- Status Logs & Event Logging (OCC115:812-1023)
- HotKey Configuration (OCC116:248-570)
- Page Operations & Bulk Edits (OCC117:829-1059)
- Master/Slave Links Basic (OCC119:1593-1610)
- Play Groups (OCC119:1656-1672)
- Display Names & Refresh (OCC119:1570-1588)

**2-3 weeks (Large):**
- Auto-Backup & Restore (OCC115:184-357)
- MIDI Device Management (OCC116:573-850)
- Undo/Redo System (OCC117:289-555)
- Paste Special System (OCC117:557-826)
- Level Meters & Play History (OCC117:1062-1304)
- Voice Over/AutoPan Modes (OCC119:1614-1632)
- Playout Logging Infrastructure (OCC121:734-746)
- Recent File Search (OCC118:656-695)

**3-4 weeks (Extra Large):**
- Missing File Resolution (OCC115:553-808)
- Master/Slave Links (All) (OCC119:1593-1652)
- Play Stack (OCC119:1675-1698)
- Search Menu (Full) (OCC118:All)
- Package System (OCC115:1058-1072 - DEFERRED)

### Table 3: Find Database Schema

| Schema | Document | Line Range | Search Term |
|--------|----------|------------|-------------|
| Recent Files | OCC115 | 98-117 | `CREATE TABLE recent_files` |
| Master/Slave Links | OCC119 | 116-148 | `CREATE TABLE master_slave_links` |
| Play Groups | OCC119 | 183-205 | `CREATE TABLE play_groups` |
| Play Stack | OCC119 | 257-285 | `CREATE TABLE play_stacks` |
| Settings | OCC120 | 122-190 | `CREATE TABLE user_settings` |
| Playout Log | OCC121 | 327-370 | `CREATE TABLE playout_log` |
| Event Log | OCC121 | 433-447 | `CREATE TABLE event_log` |
| Timecode Triggers | OCC122 | 386-396 | `CREATE TABLE timecode_triggers` |
| Admin Sessions | OCC123 | 387-395 | `CREATE TABLE admin_sessions` |
| Output Devices | OCC123 | 418-432 | `CREATE TABLE output_devices` |

---

## Part 10: Usage Examples for Claude Agents

### Example 1: Implementing a Specific Feature

**User Request:** "Implement auto-backup for sessions"

**Agent Strategy:**
```
1. Check current state:
   - Read OCC114:269-279 (find ❌ No auto-save/backup)

2. Locate feature:
   - Use Table 1: Auto-Backup → OCC115:184-357

3. Read targeted sections:
   - OCC115:190-240 (implementation code)
   - OCC115:338-357 (acceptance criteria)
   - OCC115:349-357 (technical requirements)

4. Check dependencies:
   - OCC115:188 shows "Dependencies: Sprint 1 (Application Data Folder)"
   - Read OCC115:63-181 (Sprint 1) first if not done

5. Understand threading:
   - Search "timerCallback\|background" in OCC115:200-240
   - Confirm: Uses juce::Timer (message thread), non-blocking

6. Implement:
   - Follow code patterns in OCC115:204-258
   - Create tests from OCC115:1185-1203
```

**Token Efficiency:** ~300 lines read vs 1,304 lines (77% savings)

### Example 2: Understanding System Architecture

**User Request:** "How does the Master/Slave link system work?"

**Agent Strategy:**
```
1. Check if exists:
   - Grep OCC114 for "Master/Slave" → Line 434 shows NOT implemented

2. Find documentation:
   - Table 1: Master/Slave Links → OCC119:71-148

3. Read architecture in stages:
   - OCC119:71-113 (Data model - MasterSlaveLink interface)
   - OCC119:116-148 (Database schema)
   - OCC119:1593-1610 (Phase 3: Basic implementation)
   - OCC119:1766-1836 (Challenge 7.1: Conflict resolution)

4. Understand modes:
   - OCC119:94-105 (LinkMode enum: VOICE_OVER, AUTO_PAN, etc.)
   - OCC119:1614-1632 (Phase 4: Advanced modes implementation)

5. Check integration:
   - OCC119:2009-2046 (Integration points with other menus)
   - OCC119:1810-1843 (Voice Over fade timing challenge)
```

**Token Efficiency:** ~400 lines read vs 2,377 lines (83% savings)

### Example 3: Planning Implementation Order

**User Request:** "What should I implement first for production readiness?"

**Agent Strategy:**
```
1. Check gap priorities:
   - Read OCC114:343-356 (Critical gaps table)
   - CRITICAL: Auto-backup, Logging, Recent files, Session validation

2. Build dependency tree:
   - Sprint 1: OCC115:63-181 (Folder structure - NO dependencies)
   - Sprint 2: OCC115:184-357 (Auto-backup - depends on Sprint 1)
   - Sprint 5: OCC115:812-1023 (Logging - depends on Sprint 1)
   - Sprint 3: OCC115:360-550 (Recent files - depends on Sprint 1)

3. Check critical path:
   - Part 7: Integration Dependencies (this document)
   - Path 1 (Data Safety) is CRITICAL - do first

4. Estimate effort:
   - Part 6: Effort Estimation Quick Reference
   - P0 features: ~300-400 hours

5. Create plan:
   Week 1-2:  OCC115 Sprint 1 (Foundation)
   Week 3-5:  OCC115 Sprint 2 (Auto-backup)
   Week 6:    OCC115 Sprint 3 (Recent files)
   Week 7-8:  OCC115 Sprint 5 (Logging)
   Week 9-12: OCC115 Sprint 4 (Missing files)
```

**Result:** Clear 12-week critical path for production readiness

### Example 4: Debugging Integration Issues

**User Request:** "My MIDI settings aren't persisting across restarts"

**Agent Strategy:**
```
1. Find MIDI settings:
   - Table 1: MIDI settings → Multiple locations
   - OCC116:573-850 (MIDI Device Management)
   - OCC120:100-118 (MIDI settings in Options)

2. Check persistence pattern:
   - OCC116:637-657 (saveToPreferences/loadFromPreferences)
   - OCC120:195-265 (SettingsService persistence)

3. Identify scope confusion:
   - OCC116:644 shows: Global preferences (application-wide)
   - OCC116:796 shows: Per-clip MIDI note (session-specific)
   - Problem: Mixing session vs application settings

4. Find cross-document dependencies:
   - Grep "MIDI" across OCC116, OCC120
   - OCC116:1143-1156 shows: Must store in PropertiesFile (global)
   - OCC120:168-173 shows: MIDI settings in user_settings table

5. Solution:
   - Application MIDI settings → OCC120 settings system
   - Per-button MIDI notes → OCC116 session JSON
```

**Result:** Clear separation of concerns identified

---

## Appendix A: Document Statistics

### Line Counts by Section

| Document | Data Models | API Contracts | Implementation | Testing | Total |
|----------|-------------|---------------|----------------|---------|-------|
| OCC115 | ~200 | ~150 | ~650 | ~100 | 1,304 |
| OCC116 | ~180 | ~140 | ~700 | ~80 | 1,295 |
| OCC117 | ~220 | ~160 | ~850 | ~110 | 1,541 |
| OCC118 | ~250 | ~180 | ~650 | ~90 | 1,368 |
| OCC119 | ~350 | ~250 | ~1,400 | ~150 | 2,377 |
| OCC120 | ~120 | ~80 | ~180 | ~50 | 477 |
| OCC121 | ~280 | ~170 | ~350 | ~80 | 999 |
| OCC122 | ~200 | ~120 | ~280 | ~100 | 735 |
| OCC123 | ~220 | ~140 | ~380 | ~120 | 900 |
| **Total** | **2,020** | **1,390** | **5,440** | **880** | **10,996** |

### Grep Pattern Library

**Find all sprint headers:**
```bash
grep -n "^### Sprint [0-9]*:" OCC11[5-9].md
```

**Find all data models:**
```bash
grep -n "^interface\|^enum\|^type " OCC11*.md
```

**Find all API endpoints:**
```bash
grep -n "^// GET\|^// POST\|^// PUT\|^// DELETE" OCC11*.md
```

**Find all database schemas:**
```bash
grep -n "^CREATE TABLE" OCC11*.md
```

**Find all effort estimates:**
```bash
grep -n "Estimated Effort:\|Estimated Duration:" OCC11*.md
```

**Find all dependencies:**
```bash
grep -n "^Dependencies:" OCC11*.md
```

**Find all priorities:**
```bash
grep -n "Priority: P[0-2]\|Priority: CRITICAL\|Priority: HIGH" OCC11*.md
```

---

## Appendix B: SpotOn Manual Section Map

| OCC Doc | SpotOn Section | Pages | Key Topics |
|---------|----------------|-------|------------|
| OCC115 | Section 01: File Menu | 1-20 | Auto-backup, packages, EDL, missing files |
| OCC116 | Section 02: Setup Menu | 1-15 | External tools, HotKeys, MIDI, GPI |
| OCC117 | Section 03: Display Menu | 1-12 | Layout, names, sizes, meters |
| OCC117 | Section 04: Edit Menu | 1-8 | Undo, paste special, page ops |
| OCC118 | Section 05: Search Menu | 1-3 | Recent 1000 files, search, preview |
| OCC119 | Section 06: Global Menu | 1-41 | Links, groups, stack, utilities |
| OCC120 | Section 07: Options Menu | 1-9 | Settings, flags, preferences |
| OCC121 | Section 08: Info Menu | 1-7 | Notes, status, playout/event logs |
| OCC122 | Section 09: Engineering Menu | 1-12 | Timecode, DTMF, PBus, network |
| OCC123 | Section 10: Admin Menu | 1-15 | Auth, devices, licensing, diagnostics |

---

## Document Version History

**v1.0 (2025-11-12):**
- Initial release
- Covers OCC114-OCC123 (10 documents)
- 100+ features cataloged
- Navigation strategies defined
- Search patterns established

---

**Last Updated:** 2025-11-12
**Maintainer:** OCC Development Team
**Related Documents:** OCC114-OCC123
**Usage:** Navigation and quick reference for Claude agents implementing backend features
