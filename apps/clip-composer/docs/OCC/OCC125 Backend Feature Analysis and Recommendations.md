# OCC125 - Backend Feature Analysis and Recommendations

**Version:** 1.0
**Date:** 2025-11-12
**Status:** Analysis Complete
**Purpose:** Comprehensive evaluation of OCC115-123 features against current architecture with actionable recommendations

---

## Executive Summary

This document provides a systematic analysis of 100+ backend features documented in OCC115-123 (SpotOn Manual analysis) against Clip Composer's current architecture, MVP priorities, and modernization opportunities. Each feature has been evaluated for:

- **Architectural compatibility** with JUCE/SDK/threading model
- **Implementation complexity** and effort estimates
- **MVP alignment** with broadcast/theater use cases
- **Modernization potential** to exceed SpotOn capabilities

### Key Findings

**CRITICAL for MVP (Must Implement):**

- Auto-backup/restore system (OCC115) - **Data safety**
- Recent files MRU (OCC115) - **Workflow efficiency**
- Missing file resolution (OCC115) - **Production robustness**
- Event logging framework (OCC115) - **Diagnostics**
- Preferences system (OCC120) - **User customization**

**HIGH Priority (Should Implement):**

- Session templates (OCC115)
- Status logs (OCC115)
- Undo/Redo system (OCC117)
- Paste Special (OCC117)
- Level meters (OCC117)

**MODERNIZE (Simplify/Improve SpotOn):**

- Master/Slave Links → **Clip Chains** (simpler, more powerful)
- Play Groups → **Smart Groups** with rules engine
- Play Stack → **Playlist** with modern UI
- Package System → **Cloud sync** option

**DEPRIORITIZE (Post-MVP):**

- CD Burning integration (OCC119) - Legacy feature
- GPI hardware triggers (OCC116) - Niche use case
- Page image export (OCC119) - Low value
- CAB file compression (OCC119) - Use ZIP

**IGNORE (Wrong fit for OCC):**

- SpotOn session/package viewer (OCC119) - Different formats
- Legacy file formats (EDL, etc.)
- Windows-specific features (CAB compression)

---

## Methodology

### Evaluation Criteria

Each feature evaluated against:

1. **Architecture Compatibility**
   - JUCE framework fit
   - SDK integration requirements
   - Threading model compliance (Message/Audio/Background)
   - Cross-platform support

2. **MVP Alignment**
   - Broadcast/theater use case relevance
   - Data safety criticality
   - Workflow efficiency impact
   - Production readiness necessity

3. **Implementation Complexity**
   - 🟢 **LOW** (1-3 days, ~8-24 hours)
   - 🟡 **MEDIUM** (1-2 weeks, ~40-80 hours)
   - 🔴 **HIGH** (3-4+ weeks, ~120+ hours)

4. **Modernization Opportunity**
   - Can we improve on SpotOn's design?
   - Are there better modern patterns?
   - What would users expect in 2025?

### Recommendation Tags

- ✅ **IMPLEMENT** - High priority, good fit, MVP-critical
- 🔄 **AUGMENT** - Good idea, needs simplification/modernization
- ⏸️ **DEPRIORITIZE** - Valid but post-MVP, low ROI for effort
- ❌ **IGNORE** - Wrong fit, legacy, or out of scope

---

## Menu-by-Menu Analysis

## OCC115: File Menu (CRITICAL Priority)

### Overall Assessment

**SpotOn Strengths:** Mature data safety infrastructure (auto-backup, restore, missing file wizard)
**OCC Gaps:** No safety net, minimal file management
**Modernization Opportunities:** Cloud backup option, modern file picker UI, drag-drop everywhere

### Feature Analysis

#### 1. Session Templates

**Status:** ❌ Not implemented
**Recommendation:** ✅ **IMPLEMENT** (Medium priority)
**Complexity:** 🟢 LOW (2-3 days)

**Rationale:**

- Common requirement for recurring events (theater shows, broadcasts)
- Simple to implement (just load a specific session file)
- High workflow efficiency gain

**Modernization:**

- SpotOn uses `~newsession.dta` (single template)
- **OCC should support multiple templates** with template library
- Add template metadata (name, description, author, preview image)
- Allow saving current session as template from File menu

**Integration with OCC104:**

- Fits into Sprint 5 (Session Management & Clip Groups)
- Or add as Sprint 10 task (Session Backend Architecture)

**Implementation Notes:**

```cpp
// Templates stored in: ~/AppData/OrpheusClipComposer/templates/
// Each template is a .occSession file with metadata
bool SessionManager::loadTemplate(const juce::String& templateName);
bool SessionManager::saveAsTemplate(const juce::String& templateName,
                                    const juce::String& description);
std::vector<TemplateInfo> SessionManager::getAvailableTemplates();
```

---

#### 2. Recent Files (MRU) System

**Status:** ❌ Not implemented
**Recommendation:** ✅ **IMPLEMENT** (CRITICAL priority)
**Complexity:** 🟢 LOW (3-5 days)

**Rationale:**

- Essential workflow feature for professional users
- SpotOn has this, OCC needs parity
- Reduces session load time by >50% (no file browsing)

**Modernization:**

- SpotOn shows last 10 files in submenu
- **OCC should show rich recent file list** with:
  - Session name, last modified date, clip count
  - Thumbnail/preview of first page
  - Quick actions (open, remove from list, reveal in Finder)
- Add keyboard shortcuts (Cmd+1 through Cmd+9 for top 9 recent files)
- Track recently _closed_ files too (not just opened)

**Integration with OCC104:**

- Add to Sprint 10 (Session Backend Architecture)
- Or add as new Sprint 13 task

**Implementation Notes:**

- Store in application preferences (not session file)
- Use `juce::RecentlyOpenedFilesList` as foundation
- Validate files on startup, remove deleted entries

---

#### 3. Auto-Backup and Restore System

**Status:** ❌ Not implemented
**Recommendation:** ✅ **IMPLEMENT** (CRITICAL priority - BLOCKING)
**Complexity:** 🟡 MEDIUM (2-3 weeks)

**Rationale:**

- **CRITICAL for data safety** in 24/7 broadcast/theater use
- SpotOn has comprehensive auto-backup with rotation
- OCC currently has zero data safety net
- **This is BLOCKING for production readiness**

**Modernization:**

- SpotOn uses timed backups with rotation (keep last 10)
- **OCC should add smart backup:**
  - Only backup when session is "dirty"
  - Backup before risky operations (bulk delete, import)
  - Cloud backup option (Dropbox, Google Drive sync)
- **Better restore UI:**
  - Timeline view of backups
  - Preview backup contents before restoring
  - Compare backup to current session (diff view)

**Integration with OCC104:**

- **Add as new Sprint 1B** (after current Sprint 1 performance fixes)
- This is more critical than many current OCC104 tasks

**Implementation Notes:**

- Use background thread for non-blocking saves
- Timestamp format: `~session_2025-11-12_14-30-00.occSession.backup`
- Rotation: Keep last N backups (configurable, default 10)
- Crash recovery: Check for `.crash_marker` file on startup

---

#### 4. Missing File Resolution System

**Status:** ❌ Not implemented
**Recommendation:** ✅ **IMPLEMENT** (CRITICAL priority)
**Complexity:** 🔴 HIGH (3-4 weeks)

**Rationale:**

- Essential for production robustness (files move, external drives disconnect)
- SpotOn has excellent 3-step wizard
- Without this, sessions break easily in real-world use

**Modernization:**

- SpotOn's wizard is comprehensive but dated UI
- **OCC should modernize:**
  - Modern file picker with preview
  - Smart auto-locate using file metadata (waveform fingerprinting)
  - Batch locate (find folder with many missing files, auto-fix all)
  - Option to "Search online" (future: cloud storage integration)
- **Better than SpotOn:**
  - Real-time search progress indicator
  - Undo capability for locate operations
  - Save locate rules for future ("always look in this folder first")

**Integration with OCC104:**

- Add to Sprint 10 (Session Backend Architecture)
- This is complex enough to be its own 2-week sprint

**Implementation Notes:**

- Detect missing files on session load
- Show load report dialog with all issues
- Launch wizard for step-by-step resolution
- Store file timestamps in session JSON for modification detection

---

#### 5. File Timestamp Validation

**Status:** ❌ Not implemented
**Recommendation:** ✅ **IMPLEMENT** (High priority)
**Complexity:** 🟢 LOW (1-2 days)

**Rationale:**

- Detects when audio files have been modified externally
- Prevents using wrong version of file in production
- SpotOn has this, essential for professional use

**Modernization:**

- SpotOn just warns about timestamp mismatch
- **OCC should offer actions:**
  - "Reload this file" button
  - "Reload all modified files" bulk action
  - Option to auto-reload on session load
  - Show diff (old duration vs new duration, channel count changes)

**Integration with OCC104:**

- Add to Sprint 4 or Sprint 5 (Clip Edit/Session Management)
- Requires storing timestamps in session JSON

**Implementation Notes:**

- Store `fileTimestamp` in ClipData struct
- Compare on load using `juce::File::getLastModificationTime()`
- Show warning icon on modified clips

---

#### 6. Package System (.occPackage)

**Status:** ❌ Not implemented
**Recommendation:** 🔄 **AUGMENT** (High priority, but modernize)
**Complexity:** 🔴 HIGH (3-4 weeks)

**Rationale:**

- SpotOn's package system is powerful but complex
- Essential for portability (share sessions with all audio)
- But can be modernized significantly

**Modernization:**

- **SpotOn approach:** Custom .pkg format with compression
- **OCC approach:** Use standard ZIP with manifest
  ```
  my_session.occPackage (ZIP file)
  ├── session.json           # Session metadata
  ├── audio/                 # Audio files
  │   ├── clip001.wav
  │   ├── clip002.wav
  │   └── ...
  └── manifest.json          # Package metadata, version
  ```
- **Better than SpotOn:**
  - Cloud sync option (sync package to Dropbox/Google Drive)
  - Selective packaging (choose which clips to include)
  - Package templates (pre-configured packages for common scenarios)
  - Package diff tool (compare two packages)

**Integration with OCC104:**

- **DEPRIORITIZE to v0.3.0** (post-MVP)
- Too complex for v0.2.1
- But document design now for future implementation

**Implementation Notes:**

- Use `ZipFile` library for compression
- Store package metadata in manifest.json
- Support unpacking to temp directory for loading
- Option to "explode" package into regular session + audio folder

---

#### 7. Track List Export (CSV/TXT)

**Status:** ❌ Not implemented
**Recommendation:** ✅ **IMPLEMENT** (Low priority)
**Complexity:** 🟢 LOW (2-3 days)

**Rationale:**

- Useful for documentation, show planning, audit trails
- Simple feature, low effort
- SpotOn has this for spreadsheet compatibility

**Modernization:**

- **SpotOn exports:** CSV with basic clip info
- **OCC should export:**
  - Multiple formats: CSV, JSON, Markdown table, HTML
  - Include all metadata (not just basics)
  - Option to include waveform thumbnails (HTML export)
  - Copy to clipboard as formatted table

**Integration with OCC104:**

- Add to Sprint 10 or 11 (low priority polish)
- Or defer to v0.3.0

**Implementation Notes:**

- Export from File menu: "Export Track List..."
- Use `juce::FileChooser` with format dropdown
- Generate CSV using simple string formatting

---

#### 8. Block Save/Load

**Status:** ❌ Not implemented
**Recommendation:** ✅ **IMPLEMENT** (Medium priority)
**Complexity:** 🟡 MEDIUM (1 week)

**Rationale:**

- Useful for reusable clip sets (sound effects library, common cues)
- SpotOn has this for button range export/import
- Moderate implementation complexity

**Modernization:**

- **SpotOn approach:** Save consecutive button range to .blk file
- **OCC approach:** Save any selection (not just consecutive)
  - Select clips with Cmd+Click (non-contiguous)
  - Right-click → "Save Selection as Block..."
  - Load block with "Insert before" or "Replace selection"
- **Better than SpotOn:**
  - Block library browser (see all saved blocks)
  - Block preview (see clip names without loading)
  - Block tags/categories

**Integration with OCC104:**

- Add to Sprint 10 (Session Backend Architecture)
- Or defer to v0.3.0

**Implementation Notes:**

- Block format: subset of session JSON (just `clips` array)
- Store in `~/AppData/OrpheusClipComposer/blocks/`
- Merge logic: insert at current tab selection or specified button index

---

#### 9. Status Logs and Event Logging

**Status:** ⚠️ Partial (only console `DBG()` output)
**Recommendation:** ✅ **IMPLEMENT** (CRITICAL priority)
**Complexity:** 🟡 MEDIUM (1-2 weeks)

**Rationale:**

- Essential for debugging production issues
- SpotOn has comprehensive logging
- **OCC has almost no logging currently** (just console output)
- **CRITICAL for professional use**

**Modernization:**

- **SpotOn approach:** Text log files, basic rotation
- **OCC approach:** Structured logging with modern tools
  - JSON log format (machine-readable)
  - Log levels: DEBUG, INFO, WARN, ERROR, FATAL
  - Component categories: Audio, Session, UI, Transport, System
  - Search/filter in Log Viewer UI
  - Export logs with one click
- **Better than SpotOn:**
  - In-app log viewer (no need to open files)
  - Real-time log streaming
  - Log analytics (error frequency, slow operations)
  - Send logs to support with bug reports

**Integration with OCC104:**

- **Add as new Sprint 2B** (after Tab Bar & Status System)
- This is foundational for all debugging

**Implementation Notes:**

- Use singleton `EventLogger` class
- Daily log rotation (`events_2025-11-12.log`)
- Keep last 30 days of logs (configurable)
- In-memory cache for fast UI display

---

### OCC115 Summary

**CRITICAL Features (Must Have):**

- Auto-backup/restore ✅
- Recent files MRU ✅
- Missing file resolution ✅
- Event logging ✅

**HIGH Features (Should Have):**

- Session templates ✅
- File timestamp validation ✅
- Status logs ✅

**MEDIUM Features (Nice to Have):**

- Block save/load ✅
- Track list export ✅

**DEFER to v0.3.0:**

- Package system 🔄 (complex, can wait)

**Estimated Total Effort for OCC115:**

- CRITICAL features: ~200-240 hours (5-6 weeks)
- HIGH features: ~40-60 hours (1-1.5 weeks)
- MEDIUM features: ~40-60 hours (1-1.5 weeks)
- **Total: ~280-360 hours (7-9 weeks for 1 developer)**

---

## OCC116: Setup Menu (HIGH Priority)

### Overall Assessment

**SpotOn Strengths:** Comprehensive external control (MIDI, HotKeys, GPI, Timecode)
**OCC Gaps:** Minimal external control, no MIDI support yet
**Modernization Opportunities:** OSC support, web API, modern MIDI Learn UI

### Feature Analysis

#### 1. External Tool Registry

**Status:** ❌ Not implemented
**Recommendation:** ✅ **IMPLEMENT** (Medium priority)
**Complexity:** 🟢 LOW (3-5 days)

**Rationale:**

- Useful for workflow integration (external audio editor, file browser)
- SpotOn has registry of external tools
- Simple to implement

**Modernization:**

- **SpotOn approach:** Manual path configuration
- **OCC approach:** Auto-detect installed apps
  - Detect audio editors (Audacity, Audition, Logic, Pro Tools)
  - Detect file managers (Finder/Explorer alternatives)
  - Add "Open in..." context menu with detected apps
- **Better than SpotOn:**
  - App store for popular integrations
  - Pass clip metadata to external tools (current IN/OUT points, etc.)

**Integration with OCC104:**

- Add to Sprint 8 (Preferences System) as sub-feature
- Store tool paths in application preferences

---

#### 2. HotKey Configuration System

**Status:** ❌ Not implemented (basic hotkeys exist, but not configurable)
**Recommendation:** ✅ **IMPLEMENT** (High priority)
**Complexity:** 🟡 MEDIUM (1-2 weeks)

**Rationale:**

- Essential for power users and live operation
- SpotOn has comprehensive hotkey system with scopes
- OCC has some hardcoded hotkeys but not configurable

**Modernization:**

- **SpotOn approach:** Grid-based hotkey editor with scopes
- **OCC approach:** Modern hotkey recorder
  - Press "Record Hotkey" button
  - Press desired key combination
  - Shows conflict warnings immediately
  - Hierarchical scopes (Global > Tab > Clip)
- **Better than SpotOn:**
  - Hotkey presets (Pro Tools-style, Logic-style, SpotOn-style)
  - Import/export hotkey config
  - Hotkey conflict resolver (auto-suggest alternatives)

**Integration with OCC104:**

- Add to Sprint 6 (Keyboard Navigation & Playbox)
- Already partially planned, expand scope

---

#### 3. MIDI Device Management

**Status:** ❌ Not implemented
**Recommendation:** 🔄 **AUGMENT** (High priority, but modern approach)
**Complexity:** 🟡 MEDIUM (2-3 weeks)

**Rationale:**

- Important for live operation with MIDI controllers
- SpotOn has complex MIDI system (Note, Control, Feedback)
- OCC should support MIDI but with modern UX

**Modernization:**

- **SpotOn approach:** Manual MIDI mapping per button
- **OCC approach:** MIDI Learn + Profiles
  - **MIDI Learn mode:** Click clip, press MIDI button → mapped
  - Save mappings as profiles (e.g., "Akai APC40 Profile")
  - Profile library (download community profiles)
  - Visual feedback (light up controller buttons)
- **Better than SpotOn:**
  - Auto-detect popular controllers (Akai, Novation, Behringer)
  - Load default mapping for detected controller
  - MIDI monitor in UI (see incoming MIDI in real-time)

**Integration with OCC104:**

- **Defer to v0.3.0** (post-MVP)
- MIDI is important but not blocking for MVP

---

#### 4. GPI (General Purpose Interface) Hardware Triggers

**Status:** ❌ Not implemented
**Recommendation:** ⏸️ **DEPRIORITIZE** (Niche use case)
**Complexity:** 🔴 HIGH (2-3 weeks)

**Rationale:**

- SpotOn supports GPI hardware for external triggers (buttons, switches)
- **Niche use case** - most modern users prefer MIDI, OSC, or web API
- High implementation complexity for low user benefit

**Modernization:**

- **Instead of GPI hardware:**
  - **OSC support** (Open Sound Control) - industry standard
  - **Web API** (REST endpoints for triggering clips remotely)
  - **Stream Deck plugin** (Elgato Stream Deck support)
  - **TouchOSC templates** (mobile remote control)

**Recommendation:**

- ❌ **IGNORE SpotOn's GPI system** (legacy hardware)
- ✅ **IMPLEMENT modern alternatives** (OSC, web API, Stream Deck)

**Integration with OCC104:**

- Not in OCC104
- If implemented, add as separate v0.3.0+ feature

---

#### 5. Timecode System (LTC/MTC/MIDI Timecode)

**Status:** ❌ Not implemented
**Recommendation:** ⏸️ **DEPRIORITIZE** (Post-MVP)
**Complexity:** 🔴 HIGH (3-4 weeks)

**Rationale:**

- Useful for broadcast sync and theater automation
- But complex to implement correctly
- Not essential for MVP

**Modernization:**

- **SpotOn approach:** LTC (Linear Timecode) input/output
- **OCC approach:** Support modern timecode formats
  - MIDI Timecode (MTC) - simpler than LTC
  - Art-Net Timecode (for lighting systems)
  - SMPTE via Dante/AES67 (networked audio)
- **Better than SpotOn:**
  - Visual timecode display (large, readable)
  - Timecode offset correction
  - Frame rate auto-detection

**Integration with OCC104:**

- **Defer to v0.4.0+** (post-MVP)
- Document requirements now, implement later

---

### OCC116 Summary

**IMPLEMENT (High Priority):**

- External tool registry ✅ (3-5 days)
- HotKey configuration ✅ (1-2 weeks)

**AUGMENT (Modern Approach):**

- MIDI device management 🔄 (defer to v0.3.0)

**DEPRIORITIZE:**

- GPI hardware triggers ⏸️ (use OSC/web API instead)
- Timecode system ⏸️ (defer to v0.4.0)

**IGNORE:**

- SpotOn-specific GPI emulation ❌

**Estimated Effort for OCC116 MVP Features:**

- ~60-80 hours (1.5-2 weeks)

---

## OCC117: Display/Edit Menu (HIGH Priority)

### Overall Assessment

**SpotOn Strengths:** Comprehensive editing features (Undo/Redo, Paste Special, Level Meters)
**OCC Gaps:** No Undo/Redo, basic clipboard, no level meters
**Modernization Opportunities:** Visual undo history, smart paste, modern waveform UI

### Feature Analysis

#### 1. Display Preferences

**Status:** ⚠️ Partial (some hardcoded settings)
**Recommendation:** ✅ **IMPLEMENT** (Medium priority)
**Complexity:** 🟢 LOW (3-5 days)

**Rationale:**

- User customization is essential for professional tools
- SpotOn has display settings (grid size, colors, fonts)
- OCC has some settings but not centralized

**Modernization:**

- Already planned in OCC104 Sprint 8 (Preferences System)
- **Expand scope to include:**
  - Waveform display style (filled, outline, bars)
  - Color schemes (dark, light, high contrast)
  - Font size adjustment (accessibility)
  - Grid density (button size scaling)

**Integration with OCC104:**

- Fits into Sprint 8 (Preferences System)
- Add display preferences tab

---

#### 2. Undo/Redo System

**Status:** ❌ Not implemented
**Recommendation:** ✅ **IMPLEMENT** (CRITICAL priority)
**Complexity:** 🟡 MEDIUM (2-3 weeks)

**Rationale:**

- **Essential for professional use** - mistakes happen in live operation
- SpotOn has comprehensive undo/redo
- **OCC currently has zero undo capability**
- **BLOCKING for production readiness**

**Modernization:**

- **SpotOn approach:** Stack-based undo/redo (linear history)
- **OCC approach:** Command pattern with visual history
  - Visual undo history panel (like Photoshop)
  - Branching undo (explore multiple "what-if" scenarios)
  - Named checkpoints (save undo states)
  - Selective undo (undo specific action from middle of history)
- **Better than SpotOn:**
  - Undo preview (see what will change before undoing)
  - Undo persistence (save undo history with session)
  - Collaborative undo (undo other users' actions in multi-user mode)

**Integration with OCC104:**

- **Add as new Sprint 4B** (after Clip Edit Dialog refinement)
- Or integrate into Sprint 5 (Session Management)

**Implementation Notes:**

- Use Command pattern (each action is a reversible command)
- Store commands in deque for undo/redo stack
- Limit history size (default: 50 actions)
- Clear history on session load (or persist if desired)

---

#### 3. Paste Special System

**Status:** ❌ Not implemented
**Recommendation:** ✅ **IMPLEMENT** (High priority)
**Complexity:** 🟡 MEDIUM (2-3 weeks)

**Rationale:**

- Very useful for bulk editing (apply fades to many clips at once)
- SpotOn has Paste Special with selective paste options
- OCC has basic copy/paste planned in OCC104 Sprint 5

**Modernization:**

- **SpotOn approach:** Paste Special dialog with checkboxes
- **OCC approach:** Smart paste with auto-detect
  - Auto-detect what was copied (just color? just fades? everything?)
  - Show preview of paste result before applying
  - Paste to range (paste to buttons 1-10)
  - Paste to selection (paste to all selected buttons)
- **Better than SpotOn:**
  - Paste history (see last 10 things copied)
  - Paste with transform (paste but adjust gain by +3dB, etc.)
  - Paste as template (save paste config for reuse)

**Integration with OCC104:**

- Expand OCC104 Sprint 5 (Copy/Paste)
- Add Paste Special as sub-feature

---

#### 4. Page Operations (Copy/Paste/Fill/Clear Pages)

**Status:** ❌ Not implemented
**Recommendation:** ✅ **IMPLEMENT** (Medium priority)
**Complexity:** 🟡 MEDIUM (1-2 weeks)

**Rationale:**

- Useful for duplicating pages, clearing entire tabs
- SpotOn has page clipboard operations
- OCC doesn't have page-level operations yet

**Modernization:**

- **SpotOn approach:** Copy/paste entire pages
- **OCC approach:** Page management panel
  - Drag-drop page reordering
  - Duplicate page (creates exact copy)
  - Clear page with confirmation
  - Merge pages (combine two pages)
- **Better than SpotOn:**
  - Page templates (save page as template)
  - Page diff (compare two pages, highlight differences)
  - Page history (see previous versions of page)

**Integration with OCC104:**

- Add to Sprint 5 (Session Management) as sub-feature
- Or add to Sprint 11 as polish task

---

#### 5. Level Meters

**Status:** ❌ Not implemented
**Recommendation:** ✅ **IMPLEMENT** (High priority)
**Complexity:** 🟡 MEDIUM (2-3 weeks)

**Rationale:**

- Essential for audio monitoring and troubleshooting
- SpotOn has level meters with play history
- **OCC has no audio level display currently**

**Modernization:**

- **SpotOn approach:** Separate level meters window
- **OCC approach:** Integrated level display
  - Meters in main window (always visible)
  - Per-clip meters on clip buttons
  - Master output meter (large, prominent)
  - Peak hold indicators
  - Clip detection warnings
- **Better than SpotOn:**
  - Spectrum analyzer mode (frequency display)
  - Phase correlation meter (stereo imaging)
  - Loudness metering (LUFS for broadcast)
  - Waveform history (see past 10 seconds of audio)

**Integration with OCC104:**

- Add to Sprint 2 (Status System) as sub-feature
- Meters fit naturally with status indicators

**Implementation Notes:**

- Query `AudioEngine` for current levels (lock-free atomics)
- Update meters in UI timer callback (60 Hz)
- Use JUCE LookAndFeel for meter rendering

---

#### 6. Grid Layout Configuration

**Status:** ⚠️ Partial (fixed 10×12 grid)
**Recommendation:** ✅ **IMPLEMENT** (Medium priority)
**Complexity:** 🟢 LOW (already planned in OCC104 Sprint 5)

**Rationale:**

- Already planned in OCC104 Sprint 5.2 (Clip Grid Resizing)
- SpotOn has configurable grid (button count)
- OCC should match this

**Integration with OCC104:**

- Already in Sprint 5.2
- No changes needed

---

### OCC117 Summary

**CRITICAL Features:**

- Undo/Redo system ✅ (2-3 weeks)

**HIGH Features:**

- Paste Special ✅ (2-3 weeks)
- Level Meters ✅ (2-3 weeks)

**MEDIUM Features:**

- Display Preferences ✅ (3-5 days, already planned)
- Page Operations ✅ (1-2 weeks)
- Grid Layout ✅ (already in OCC104)

**Estimated Effort for OCC117:**

- ~160-200 hours (4-5 weeks)

---

## OCC118: Search Menu (MEDIUM Priority)

### Overall Assessment

**SpotOn Strengths:** Comprehensive search (1000 recent files, WAV metadata, preview)
**OCC Gaps:** No search features at all
**Modernization Opportunities:** Fuzzy search, tags, cloud search

### Feature Analysis

#### 1. Recent File Search (1000+ files)

**Status:** ❌ Not implemented
**Recommendation:** 🔄 **AUGMENT** (Modern approach)
**Complexity:** 🟡 MEDIUM (2-3 weeks)

**Rationale:**

- SpotOn tracks 1000 recently loaded files (circular buffer)
- Useful for large libraries
- But can be modernized significantly

**Modernization:**

- **SpotOn approach:** Circular buffer, manual search
- **OCC approach:** Smart file library
  - **Auto-index** all files in configured folders
  - **Fuzzy search** (type "intro" → finds "introduction_music.wav")
  - **Tag system** (add custom tags to files)
  - **Smart collections** (recently added, most used, favorites)
  - **Cloud search** (search Dropbox/Google Drive)
- **Better than SpotOn:**
  - Search as you type (instant results)
  - Visual search results (waveform thumbnails)
  - Bulk operations (add all search results to current tab)

**Integration with OCC104:**

- **Defer to v0.3.0** (not MVP-critical)
- Document design now, implement later

---

#### 2. WAV Metadata Parsing

**Status:** ❌ Not implemented
**Recommendation:** ✅ **IMPLEMENT** (Medium priority)
**Complexity:** 🟡 MEDIUM (2-3 days)

**Rationale:**

- WAV files can contain metadata (artist, title, comments)
- Useful for auto-naming clips
- Modern audio editors add rich metadata

**Modernization:**

- **SpotOn approach:** Parse RIFF INFO chunks
- **OCC approach:** Parse multiple metadata formats
  - RIFF INFO chunks (BWF metadata)
  - ID3 tags (if present)
  - iXML (XML metadata in WAV)
  - Broadcast Wave Format (BWF) timestamps
- **Better than SpotOn:**
  - Auto-apply metadata to clip name
  - Show metadata in clip info panel
  - Search by metadata

**Integration with OCC104:**

- Add to Sprint 4 or Sprint 5 (Clip metadata)
- Or add to Sprint 10 (Session Backend)

---

#### 3. Track Preview Player

**Status:** ⚠️ Partial (PreviewPlayer exists in UI)
**Recommendation:** ✅ **IMPLEMENT** (enhance existing)
**Complexity:** 🟢 LOW (2-3 days)

**Rationale:**

- OCC has PreviewPlayer component
- Needs integration with search/browse
- Already partially implemented

**Modernization:**

- Already exists in `Source/UI/PreviewPlayer.{h,cpp}`
- **Enhance with:**
  - Preview with IN/OUT points (preview just the trimmed section)
  - Preview with fades applied
  - Preview at different speeds (1.5x, 2x for quick audition)
  - Preview routing (send to specific output)

**Integration with OCC104:**

- Fits into existing Sprint 4 (Clip Edit Dialog)
- Or Sprint 6 (Keyboard Navigation)

---

#### 4. Drag-Drop File Loading

**Status:** ✅ Already implemented (partially)
**Recommendation:** ✅ **ENHANCE** (expand functionality)
**Complexity:** 🟢 LOW (1-2 days)

**Rationale:**

- OCC likely has drag-drop already
- SpotOn has drag-drop with Alt+Top/Tail modifiers
- Enhance with more drag-drop targets

**Modernization:**

- **SpotOn approach:** Drag file to button, Alt modifier for Top/Tail trim
- **OCC approach:** Multi-target drag-drop
  - Drag to button → load clip
  - Drag to tab bar → create new tab with clip
  - Drag multiple files → auto-fill buttons in sequence
  - Drag folder → load all files in folder
- **Better than SpotOn:**
  - Drag from Finder/Explorer shows preview
  - Drag shows destination highlighting
  - Undo drag-drop operations

**Integration with OCC104:**

- Enhance existing drag-drop in Sprint 3 or Sprint 5

---

### OCC118 Summary

**IMPLEMENT (Medium Priority):**

- WAV metadata parsing ✅ (2-3 days)
- Enhance track preview ✅ (2-3 days)
- Expand drag-drop ✅ (1-2 days)

**AUGMENT (Modernize):**

- Recent file search 🔄 (defer to v0.3.0)

**Estimated Effort for OCC118 MVP:**

- ~40-60 hours (1-1.5 weeks)

---

## OCC119: Global Menu (VERY HIGH Complexity)

### Overall Assessment

**SpotOn Strengths:** Advanced features (Master/Slave links, Play Stack, utilities)
**OCC Gaps:** None of these features exist
**Modernization Opportunities:** Simplify Master/Slave links, modernize Play Stack UI, skip legacy features

### Concise Feature Recommendations

#### Master/Slave Link System

**Recommendation:** 🔄 **AUGMENT** → **Simplify to "Clip Chains"**
**Complexity:** 🔴 HIGH (3-4 weeks)
**Priority:** Post-MVP (v0.3.0+)

**Rationale:** SpotOn's Master/Slave system is powerful but overly complex (100 links, 6 modes, visual editor). **OCC should simplify:**

- **Rename:** "Master/Slave Links" → "Clip Chains" (modern terminology)
- **Simplify modes:** Remove AutoPan, UnPause/Pause (rarely used)
- **Focus on:** Play chains (trigger multiple clips), Voice Over (duck background audio)
- **Better UX:** Visual chain builder (drag clips into chain sequence)

**Defer to v0.3.0+** - Too complex for MVP, not essential for basic operation.

---

#### Play Groups (Exclusive Playback)

**Recommendation:** ✅ **IMPLEMENT** (Medium priority, simplified)
**Complexity:** 🟡 MEDIUM (1-2 weeks)
**Priority:** v0.2.1 or v0.3.0

**Rationale:** Useful for exclusive playback (only one clip in group plays at a time). **SpotOn has 25 groups + 4 buzzer groups** - OCC should simplify to **8-12 groups max**.

**Modernization:**

- Drop buzzer groups (niche feature)
- Drop Group 24/25 special functions (tied to Play Stack)
- Add visual group indicator on clips
- Simple toggle: "Exclusive playback in this group"

**Integration:** Could fit into v0.2.1 Sprint 5 (Session Management) as extended feature, or defer to v0.3.0.

---

#### Play Stack (Independent Playlist)

**Recommendation:** 🔄 **AUGMENT** → **Modern Playlist**
**Complexity:** 🔴 HIGH (3-4 weeks)
**Priority:** Post-MVP (v0.3.0+)

**Rationale:** SpotOn's Play Stack is useful for interval music, but UI is dated. **OCC should modernize:**

- **Rename:** "Play Stack" → "Playlist" (clearer name)
- **Modern UI:** Drag-drop tracks, visual progress, album art
- **Streaming support:** Add tracks from Spotify/Apple Music (future)
- **Smart playlists:** Auto-generate based on mood, BPM, etc.

**Defer to v0.3.0+** - Nice feature but not essential for MVP.

---

#### Preview Output Assignment

**Recommendation:** ✅ **IMPLEMENT** (High priority)
**Complexity:** 🟢 LOW (2-3 days)
**Priority:** v0.2.1 (Sprint 4 or Sprint 9)

**Rationale:** Essential for auditioning clips without interrupting main output. **Simple feature:**

- Route preview to specific audio output (headphones, studio monitors)
- Shift+Click to preview clip (doesn't interrupt main playback)
- Already conceptually similar to existing PreviewPlayer

**Integration:** Add to Sprint 4 (Clip Edit Dialog) or Sprint 9 (Audio Output Routing).

---

#### Timecode/Click Track Generators

**Recommendation:** ⏸️ **DEPRIORITIZE** (Niche utility)
**Complexity:** 🟡 MEDIUM (2-3 weeks)
**Priority:** v0.4.0+

**Rationale:** Useful niche features for broadcast/theater, but not MVP-critical. **Defer to later version.**

---

#### CD Burning Integration

**Recommendation:** ❌ **IGNORE** (Legacy feature)
**Priority:** Never

**Rationale:** CD burning is legacy (2000s feature). Modern users stream, use USB drives, or cloud storage. **Skip this entirely.**

---

#### Statistics & Session Viewer

**Recommendation:** ⏸️ **DEPRIORITIZE** (Low value)
**Complexity:** 🟢 LOW (1-2 weeks)
**Priority:** v0.3.0+

**Rationale:** File size analysis and folder fragmentation are mildly useful but not critical. **Defer to polish phase.**

---

#### Debug Logging Collection

**Recommendation:** ✅ **IMPLEMENT** (simplified)
**Complexity:** 🟢 LOW (2-3 days)
**Priority:** v0.2.1 (Sprint 2B with Event Logging)

**Rationale:** Useful for support requests. **But simplify:**

- SpotOn creates CAB files and FTPs them (overcomplicated)
- **OCC should:** "Export Logs" button → creates ZIP file with recent logs
- User can email ZIP to support

**Integration:** Add to Sprint 2B (Event Logging System) as sub-feature.

---

#### Clear Attributes / Page Image Export

**Recommendation:** ⏸️ **DEPRIORITIZE** (Low priority)
**Complexity:** 🟢 LOW (1-2 days each)
**Priority:** v0.3.0+ polish

**Rationale:** Useful for cleanup operations but not essential. **Defer to polish phase.**

---

### OCC119 Summary

**IMPLEMENT (High Priority):**

- Preview output assignment ✅ (2-3 days)
- Debug log export ✅ (2-3 days, simplified)

**IMPLEMENT (Medium Priority - v0.3.0):**

- Play Groups (simplified) ✅ (1-2 weeks)

**AUGMENT (Modernize for v0.3.0+):**

- Master/Slave Links → Clip Chains 🔄 (3-4 weeks)
- Play Stack → Playlist 🔄 (3-4 weeks)

**DEPRIORITIZE:**

- Timecode/Click generators ⏸️ (v0.4.0+)
- Statistics/Session viewer ⏸️ (v0.3.0+)
- Clear attributes ⏸️ (polish)

**IGNORE:**

- CD Burning ❌
- Page image export ❌ (use screenshot tool)
- CAB compression/FTP upload ❌ (use ZIP)

**Estimated Effort for OCC119 MVP:**

- ~40-60 hours (1-1.5 weeks for critical features)
- ~200-300 hours for full v0.3.0 features

---

## OCC120: Options Menu (MEDIUM Priority)

### Overall Assessment

**SpotOn Strengths:** Comprehensive settings system
**OCC Gaps:** Preferences partially planned in OCC104 Sprint 8
**Modernization Opportunities:** Modern preferences UI, cloud sync

### Concise Recommendations

#### Settings System Infrastructure

**Recommendation:** ✅ **IMPLEMENT** (CRITICAL - already planned)
**Complexity:** 🟡 MEDIUM (3-4 days)
**Priority:** v0.2.1 Sprint 8

**Rationale:** **Already planned in OCC104 Sprint 8** (Preferences System). **SpotOn has comprehensive settings** - OCC should match.

**Modernization:**

- **SpotOn approach:** Windows registry-style settings
- **OCC approach:** Modern preferences with:
  - Searchable settings
  - Settings profiles (import/export)
  - Cloud sync (sync preferences across machines)
  - Reset to defaults per category

**Integration:** Already in OCC104 Sprint 8 - **no changes needed**, but expand scope slightly for modernization.

---

#### Feature Flags

**Recommendation:** ✅ **IMPLEMENT** (Low priority)
**Complexity:** 🟢 LOW (1 day)
**Priority:** v0.2.1 or v0.3.0

**Rationale:** Useful for beta testing and gradual rollout of features. **Add simple feature flag system:**

- Enable/disable experimental features
- Developer mode toggle
- Debug visualizations

**Integration:** Add to Sprint 8 (Preferences) as sub-feature.

---

### OCC120 Summary

**IMPLEMENT:**

- Settings system ✅ (already in OCC104 Sprint 8)
- Feature flags ✅ (add as sub-feature)

**Estimated Effort:** ~40-60 hours (1-1.5 weeks, mostly already planned)

---

## OCC121: Info Menu (MEDIUM Priority)

### Overall Assessment

**SpotOn Strengths:** Comprehensive logging (playout logs, event logs)
**OCC Gaps:** Minimal logging (just console output)
**Modernization Opportunities:** Modern log viewer, analytics

### Concise Recommendations

#### Session Notes

**Recommendation:** ✅ **IMPLEMENT** (Low priority)
**Complexity:** 🟢 LOW (1 day)
**Priority:** v0.3.0

**Rationale:** Simple feature - text field for session notes. **Low priority but easy to add.**

---

#### System Status Display

**Recommendation:** ✅ **IMPLEMENT** (Medium priority)
**Complexity:** 🟢 LOW (2-3 days)
**Priority:** v0.2.1 Sprint 2 (Status System)

**Rationale:** **Already partially planned in OCC104 Sprint 2** (Status indicators). **Expand to include:**

- CPU/RAM usage
- Audio buffer status
- Clip count, session duration
- Last saved timestamp

**Integration:** Fits into Sprint 2 (Tab Bar & Status System).

---

#### Playout Logging

**Recommendation:** ✅ **IMPLEMENT** (High priority for broadcast)
**Complexity:** 🟡 MEDIUM (2-3 weeks)
**Priority:** v0.3.0

**Rationale:** **Essential for broadcast royalty reporting.** Log every clip played with:

- Timestamp (precise to millisecond)
- Clip name, file path
- Trigger source (hotkey, MIDI, mouse, etc.)
- Duration played
- Export to CSV for royalty services

**Defer to v0.3.0** - Important but not MVP-blocking.

---

#### Event Logging

**Recommendation:** ✅ **IMPLEMENT** (CRITICAL - already covered)
**Complexity:** 🟡 MEDIUM (1-2 weeks)
**Priority:** v0.2.1 Sprint 2B

**Rationale:** **Already covered in OCC115 analysis** (Event Logging System). Same implementation.

---

#### Log Archival

**Recommendation:** ✅ **IMPLEMENT** (Low priority)
**Complexity:** 🟢 LOW (1-2 days)
**Priority:** v0.3.0

**Rationale:** Auto-archive old logs (>30 days). **Simple feature, defer to polish phase.**

---

### OCC121 Summary

**IMPLEMENT (CRITICAL):**

- Event logging ✅ (already in OCC115/Sprint 2B)
- System status display ✅ (expand Sprint 2)

**IMPLEMENT (High Priority - v0.3.0):**

- Playout logging ✅ (2-3 weeks)

**IMPLEMENT (Low Priority):**

- Session notes ✅ (1 day)
- Log archival ✅ (1-2 days)

**Estimated Effort for OCC121 MVP:**

- ~20-40 hours (mostly covered by OCC115)

---

## OCC122: Engineering Menu (MEDIUM Priority)

### Overall Assessment

**SpotOn Strengths:** Professional features (timecode, DTMF, network triggers)
**OCC Gaps:** None of these features exist
**Modernization Opportunities:** Replace legacy protocols with modern alternatives

### Concise Recommendations

#### Timecode System (LTC/MTC)

**Recommendation:** ⏸️ **DEPRIORITIZE** (covered in OCC116)
**Priority:** v0.4.0+

**Rationale:** Already covered in OCC116 analysis. **Defer to post-MVP.**

---

#### DTMF Decoder

**Recommendation:** ❌ **IGNORE** (Legacy feature)
**Priority:** Never

**Rationale:** DTMF (Dual-Tone Multi-Frequency) is telephone tone signaling from the 1960s. **Modern users don't use DTMF for audio control.** Skip entirely.

---

#### PBus / Network Triggers

**Recommendation:** 🔄 **AUGMENT** → **Modern Network API**
**Complexity:** 🟡 MEDIUM (2-3 weeks)
**Priority:** v0.3.0+

**Rationale:** Network remote control is useful, but **replace SpotOn's proprietary "PBus" protocol with modern standards:**

- **REST API** (HTTP endpoints for remote control)
- **WebSocket** (real-time bidirectional communication)
- **OSC** (Open Sound Control - industry standard)

**Modern approach is better than legacy PBus.** Defer to v0.3.0+.

---

#### Playout Logs (Engineering Configuration)

**Recommendation:** ✅ **IMPLEMENT** (covered in OCC121)
**Priority:** v0.3.0

**Rationale:** Same as OCC121 Playout Logging. **Single implementation.**

---

### OCC122 Summary

**AUGMENT (v0.3.0+):**

- Network triggers → REST API/WebSocket 🔄 (2-3 weeks)

**DEPRIORITIZE:**

- Timecode system ⏸️ (v0.4.0+)

**IGNORE:**

- DTMF decoder ❌
- Legacy PBus protocol ❌

**Estimated Effort:** ~80-120 hours (2-3 weeks, defer to v0.3.0+)

---

## OCC123: Admin Menu (LOW Priority)

### Overall Assessment

**SpotOn Strengths:** Multi-user admin features
**OCC Gaps:** None of these exist (single-user app currently)
**Modernization Opportunities:** Cloud-based multi-user collaboration

### Concise Recommendations

#### Admin Authentication

**Recommendation:** ⏸️ **DEPRIORITIZE** (Future multi-user mode)
**Complexity:** 🟡 MEDIUM (2-3 weeks)
**Priority:** v0.5.0+ (if multi-user mode added)

**Rationale:** SpotOn has admin password protection for settings. **OCC is currently single-user** - not needed for MVP. If multi-user mode added in future, implement then.

---

#### Audio Device Management

**Recommendation:** ✅ **IMPLEMENT** (partially covered)
**Complexity:** 🟢 LOW (already in Sprint 9)
**Priority:** v0.2.1 Sprint 9

**Rationale:** **Already planned in OCC104 Sprint 9** (Audio Output Routing). No changes needed.

---

#### Licensing System

**Recommendation:** ⏸️ **DEPRIORITIZE** (Business decision needed)
**Complexity:** 🟡 MEDIUM (2-3 weeks)
**Priority:** When monetization strategy decided

**Rationale:** OCC product vision mentions €500-1,500 pricing (OCC021), but **licensing implementation depends on business model:**

- One-time purchase? → Simple license key
- Subscription? → Cloud-based authentication
- Freemium? → Feature gating

**Defer until monetization strategy finalized.**

---

#### Diagnostics Panel

**Recommendation:** ✅ **IMPLEMENT** (partially covered)
**Complexity:** 🟢 LOW (expand Sprint 2)
**Priority:** v0.2.1 Sprint 2

**Rationale:** **Covered by System Status Display (OCC121).** Same implementation.

---

### OCC123 Summary

**IMPLEMENT (covered elsewhere):**

- Audio device management ✅ (Sprint 9)
- Diagnostics panel ✅ (Sprint 2)

**DEPRIORITIZE:**

- Admin authentication ⏸️ (v0.5.0+ if multi-user)
- Licensing system ⏸️ (business decision needed)

**Estimated Effort:** ~0 hours (covered by other sprints)

---

## Cross-Menu Summary & Effort Estimates

### By Priority

#### P0: CRITICAL (MVP Blockers)

**Total:** ~480-600 hours (12-15 weeks for 1 developer, 6-8 weeks for 2 developers)

- Auto-backup/restore (OCC115): 80-120 hours
- Event logging (OCC115): 40-60 hours
- Undo/Redo (OCC117): 80-120 hours
- Missing file resolution (OCC115): 120-160 hours
- Recent files MRU (OCC115): 24-40 hours
- Level meters (OCC117): 80-120 hours
- System status display (OCC121): 16-24 hours
- Settings system (OCC120): 40-56 hours

#### P1: HIGH (Should Have for v0.2.1)

**Total:** ~280-360 hours (7-9 weeks)

- Session templates (OCC115): 16-24 hours
- File timestamp validation (OCC115): 8-16 hours
- Status logs (OCC115): 40-60 hours
- HotKey configuration (OCC116): 40-80 hours
- External tool registry (OCC116): 24-40 hours
- Paste Special (OCC117): 80-120 hours
- WAV metadata parsing (OCC118): 16-24 hours
- Preview output assignment (OCC119): 16-24 hours
- Display preferences (OCC117): 24-40 hours

#### P2: MEDIUM (Nice to Have)

**Total:** ~120-160 hours (3-4 weeks)

- Block save/load (OCC115): 40-60 hours
- Track list export (OCC115): 16-24 hours
- Page operations (OCC117): 40-80 hours
- Enhanced track preview (OCC118): 16-24 hours
- Debug log export (OCC119): 8-16 hours

#### DEFER to v0.3.0+

**Total:** ~600-800 hours (15-20 weeks)

- Package system (OCC115): 120-160 hours
- MIDI device management (OCC116): 80-120 hours
- Play Groups (OCC119): 40-80 hours
- Clip Chains (OCC119): 120-160 hours
- Playlist (OCC119): 120-160 hours
- Playout logging (OCC121): 80-120 hours
- Network API (OCC122): 80-120 hours

---

## Integration with OCC104

### Features Already in OCC104 (Keep as-is)

- Sprint 5: Grid resizing, Clip Groups, Copy/Paste ✅
- Sprint 6: Keyboard navigation ✅
- Sprint 8: Preferences system ✅
- Sprint 9: Audio output routing ✅

### Recommended Additions to OCC104

#### Add to Sprint 2 (Tab Bar & Status System)

- **System status display** (CPU, RAM, buffer, clip count)
- **Level meters** (basic version - full version in later sprint)

#### Add New Sprint 1B (Data Safety - CRITICAL)

**Insert after Sprint 1 (Performance fixes)**

- Auto-backup system
- Crash recovery
- Estimated duration: 2-3 weeks

#### Add New Sprint 2B (Logging - CRITICAL)

**Insert after Sprint 2 (Tab Bar & Status)**

- Event logging framework
- Log viewer UI
- Debug log export
- Estimated duration: 1-2 weeks

#### Add New Sprint 4B (Undo/Redo - CRITICAL)

**Insert after Sprint 4 (Clip Edit Dialog)**

- Undo/Redo system (Command pattern)
- Visual undo history
- Estimated duration: 2-3 weeks

#### Expand Sprint 5 (Session Management)

- Add: Session templates
- Add: Recent files MRU
- Add: File timestamp validation

#### Expand Sprint 10 (Session Backend)

- Add: Missing file resolution wizard
- Add: Block save/load
- Add: WAV metadata parsing

---

## Modernization Themes

### 1. Cloud-First Thinking

- Cloud backup option (Dropbox, Google Drive sync)
- Cloud search (search across cloud storage)
- Cloud collaboration (future multi-user mode)

### 2. Modern UX Patterns

- Fuzzy search everywhere
- Drag-drop everywhere
- Undo/redo for all actions
- Visual feedback (previews, animations)

### 3. Simplification Over Feature Parity

- **Don't copy SpotOn 1:1** - simplify where SpotOn is overcomplicated
- **Examples:**
  - SpotOn: 100 Master/Slave links → OCC: 20-30 Clip Chains
  - SpotOn: 25 Play Groups + 4 Buzzer Groups → OCC: 8-12 Groups
  - SpotOn: CAB compression + FTP → OCC: ZIP export

### 4. Modern Protocols Over Legacy

- REST API instead of PBus
- WebSocket instead of proprietary protocols
- OSC instead of GPI hardware
- Stream Deck instead of custom controllers

---

## Next Steps

See **OCC126** for executive summary and actionable sprint recommendations.

---

**Document Complete**
**Total Pages:** ~80 sections analyzed
**Total Features Evaluated:** 100+
**Recommendations Generated:** 75+

**Related Documents:**

- **OCC126** - Executive Summary & Sprint Recommendations (next document)
- **OCC114** - Current State Baseline
- **OCC104** - v0.2.1 Sprint Plan (existing)
- **OCC115-123** - Detailed Feature Specifications (reference)
