# Session Schema - Explanatory Notes

## Overview

This diagram visualizes the complete JSON session file format for Orpheus Clip Composer v0.2.0. Sessions are the **primary data persistence mechanism** - they store all clip metadata (960 clips), routing configuration (4 groups), and user preferences. Understanding the schema is critical for implementing session save/load, migration, and debugging user-reported issues.

**File Format:** .occSession (UTF-8 JSON, human-readable, version control friendly)

## Key Architectural Decisions

### JSON vs Binary Format

**Why JSON?**
- **Human Readable** - Users can inspect/edit sessions in text editor (advanced use case)
- **Version Control Friendly** - Git diffs show exactly what changed between sessions
- **Cross-Platform** - No endianness issues, portable across macOS/Windows/Linux
- **Easy Debugging** - Can inspect session files without special tools
- **Extensible** - Can add new fields without breaking old parsers (forward compatibility)

**Trade-offs:**
- Larger file size than binary (~500KB for 960-clip session vs. ~200KB binary)
- Slower parsing than binary (~50ms vs. ~10ms)
- BUT: File size/parse time acceptable for current use case (960 clips, save/load infrequent)

### Absolute Paths vs Relative Paths

**Current Implementation (v0.2.0):**
- Absolute paths stored in JSON: `"/Users/username/Audio/clip.wav"`
- Works for single-user, single-machine workflows

**Future Enhancement (v0.3.0):**
- Support relative paths: `"./audio/clip.wav"` (relative to session file location)
- Enable portable sessions (share with team, move to different machine)
- Migration strategy: Detect absolute vs. relative, prefer relative

### Version Compatibility Strategy

**Schema Version Field:**
- Every session has `"version": "1.0.0"` (semantic versioning)
- Loader checks version, applies migration if needed
- Forward compatibility: Newer loaders can read older sessions
- Backward compatibility: Older loaders reject newer sessions (show "Update Clip Composer" message)

**Migration Path:**
```
v1.0.0 → v1.1.0: Add stopOthersOnPlay field (default: false)
v1.1.0 → v2.0.0: Add DSP effects metadata (default: empty)
```

## Session Schema Breakdown

### SessionRoot (Top-Level Object)

```json
{
  "sessionMetadata": { ... },
  "clips": [ ... ],
  "routing": { ... },
  "preferences": { ... }
}
```

**Design Rationale:**
- Four top-level sections (metadata, clips, routing, preferences)
- Metadata first (version check, session info)
- Clips array (bulk of data, 960 entries)
- Routing config (4 groups, master gain)
- Preferences (UI state, audio settings)

### SessionMetadata

**Purpose:** Session-level information (name, author, creation date, audio settings)

```json
{
  "name": "Evening Broadcast",
  "version": "1.0.0",
  "createdDate": "2025-10-12T12:00:00Z",
  "modifiedDate": "2025-10-12T15:30:00Z",
  "author": "John Doe",
  "description": "Evening broadcast session for October 12th",
  "sampleRate": 48000,
  "bufferSize": 512
}
```

**Field Details:**
- `name`: Human-readable session name (displayed in title bar)
- `version`: Schema version (semantic versioning, enables migration)
- `createdDate`: ISO 8601 timestamp (UTC timezone recommended)
- `modifiedDate`: Updated on every save (used for conflict resolution)
- `author`: Creator name (optional, used in team workflows)
- `description`: Session notes (optional, useful for archival)
- `sampleRate`: Target sample rate (clips auto-resampled if mismatch)
- `bufferSize`: Audio buffer size (512 default, configurable 128-1024)

**Validation Rules:**
- `version` must match "X.Y.Z" pattern
- `sampleRate` must be one of: 44100, 48000, 96000
- `bufferSize` must be power of 2 (128, 256, 512, 1024)

### ClipMetadata (960 Max)

**Purpose:** Complete metadata for a single clip (file path, trim, fade, gain, color, group)

```json
{
  "handle": 1,
  "name": "Intro Music",
  "filePath": "/Users/username/Audio/intro.wav",
  "buttonIndex": 0,
  "tabIndex": 0,
  "clipGroup": 0,
  "trimIn": 0,
  "trimOut": 240000,
  "gainDb": 0.0,
  "color": "#FF5733",
  "fadeInSamples": 0,
  "fadeOutSamples": 1000,
  "fadeInCurve": "Linear",
  "fadeOutCurve": "Linear",
  "loopEnabled": false,
  "playbackMode": "OneShot",
  "loopStart": 0,
  "loopEnd": 240000,
  "stopOthersOnPlay": false,
  "cuePoints": [
    { "name": "Verse", "position": 48000, "color": "#00FF00" },
    { "name": "Chorus", "position": 96000, "color": "#FF00FF" }
  ]
}
```

**Field Details:**

| Field              | Type    | Range/Values                       | Description                                 |
|--------------------|---------|------------------------------------|---------------------------------------------|
| `handle`           | int     | 1-960                              | Unique clip identifier (never reused)       |
| `name`             | string  | Any UTF-8                          | Display name (shown on button)              |
| `filePath`         | string  | Absolute or relative path          | Path to audio file (WAV/AIFF/FLAC)          |
| `buttonIndex`      | int     | 0-119                              | Button position (0=top-left, 119=bottom-right) |
| `tabIndex`         | int     | 0-7                                | Tab index (0-7, 8 tabs total)               |
| `clipGroup`        | int     | 0-3                                | Routing group (0-3, 4 groups total)         |
| `trimIn`           | int64   | 0 to file duration                 | Trim start (samples)                        |
| `trimOut`          | int64   | trimIn to file duration            | Trim end (samples)                          |
| `gainDb`           | float   | -48.0 to +12.0                     | Gain in decibels                            |
| `color`            | string  | Hex "#RRGGBB"                      | Button color (visual organization)          |
| `fadeInSamples`    | int     | 0 to trimOut-trimIn                | Fade-in duration (samples)                  |
| `fadeOutSamples`   | int     | 0 to trimOut-trimIn                | Fade-out duration (samples)                 |
| `fadeInCurve`      | string  | Linear, Exponential, Logarithmic   | Fade-in curve shape (v0.3.0+)               |
| `fadeOutCurve`     | string  | Linear, Exponential, Logarithmic   | Fade-out curve shape (v0.3.0+)              |
| `loopEnabled`      | bool    | true, false                        | Enable looping                              |
| `playbackMode`     | string  | OneShot, Loop                      | Playback mode                               |
| `loopStart`        | int64   | trimIn to trimOut                  | Loop start point (samples)                  |
| `loopEnd`          | int64   | loopStart to trimOut               | Loop end point (samples)                    |
| `stopOthersOnPlay` | bool    | true, false                        | Stop all other clips when this one starts   |
| `cuePoints`        | array   | 0-N CuePoint objects               | Named markers within clip                   |

**Validation Rules:**
- `trimIn` < `trimOut` (trim range must be valid)
- `fadeInSamples` + `fadeOutSamples` ≤ `trimOut - trimIn` (fades must fit within trim range)
- `loopStart` < `loopEnd` (loop range must be valid)
- `gainDb` clamped to -48.0 to +12.0 (prevent excessive gain)
- `color` must match `#[0-9A-Fa-f]{6}` pattern

**New in v0.2.0:**
- `stopOthersOnPlay`: Per-clip "solo" mode (OCC093 fix)
- `fadeInCurve`, `fadeOutCurve`: Curve shape selection (stored but not yet applied in playback)

### CuePoint

**Purpose:** Named marker within a clip (e.g., "Verse", "Chorus", "Outro")

```json
{
  "name": "Verse",
  "position": 48000,
  "color": "#00FF00"
}
```

**Use Cases:**
- Quick navigation (click cue point → jump to position)
- Visual reference (waveform display shows cue markers)
- Future: Auto-trigger actions (e.g., "Fade out at Outro cue")

### RoutingConfiguration

**Purpose:** Routing matrix configuration (4 Clip Groups → Master Output)

```json
{
  "clipGroups": [
    { "name": "Music", "gainDb": 0.0, "mute": false, "solo": false },
    { "name": "SFX", "gainDb": -3.0, "mute": false, "solo": false },
    { "name": "Voice", "gainDb": 0.0, "mute": false, "solo": false },
    { "name": "Backup", "gainDb": -6.0, "mute": false, "solo": false }
  ],
  "masterGain": 0.0,
  "masterMute": false
}
```

**ClipGroup Fields:**
- `name`: Human-readable group name (shown in UI)
- `gainDb`: Group gain in decibels (-48.0 to +12.0)
- `mute`: Mute group (no audio output)
- `solo`: Solo group (mute all other groups)

**Design Constraints:**
- Exactly 4 groups (hardcoded, not configurable in v0.2.0)
- Future: Flexible routing (v2.0 - aux sends, sub-groups)

### SessionPreferences

**Purpose:** User preferences (UI state, audio settings, auto-save config)

```json
{
  "autoSave": true,
  "autoSaveInterval": 300,
  "showWaveforms": true,
  "colorScheme": "Dark",
  "audioDriver": "CoreAudio",
  "audioDevice": "Built-in Output",
  "audioBufferSize": 512,
  "audioSampleRate": 48000,
  "showPerformanceMonitor": false,
  "uiRefreshRate": 75
}
```

**Field Details:**
- `autoSave`: Enable auto-save (every `autoSaveInterval` seconds)
- `autoSaveInterval`: Auto-save interval (seconds, default: 300 = 5 minutes)
- `showWaveforms`: Show waveform display in clip edit dialog
- `colorScheme`: UI theme (Dark, Light)
- `audioDriver`: Audio driver type (CoreAudio, WASAPI, ASIO)
- `audioDevice`: Audio device name (system-specific)
- `audioBufferSize`: Audio buffer size (128, 256, 512, 1024)
- `audioSampleRate`: Audio sample rate (44100, 48000, 96000)
- `showPerformanceMonitor`: Show CPU/latency monitor
- `uiRefreshRate`: UI timer frequency (30, 60, 75, 120 fps)

**Platform Differences:**
- macOS: `audioDriver` = "CoreAudio"
- Windows: `audioDriver` = "WASAPI" (v0.2.0) or "ASIO" (v1.0+)
- Linux: `audioDriver` = "ALSA" (planned for v1.0)

## File Path Resolution

### Absolute Paths (Current Implementation)

**Example:**
```json
"filePath": "/Users/username/Audio/intro.wav"
```

**Advantages:**
- Simple (no resolution logic needed)
- Unambiguous (always points to same file)

**Disadvantages:**
- Not portable (breaks when session moved to different machine/user)
- Verbose (long paths in JSON)

### Relative Paths (Future Enhancement)

**Example:**
```json
"filePath": "./audio/intro.wav"
```

**Advantages:**
- Portable (works when session + audio files moved together)
- Concise (shorter paths in JSON)

**Implementation Strategy:**
1. On save: Check if file is in subdirectory of session file → use relative path
2. On load: If path starts with "./", resolve relative to session file location
3. Fallback: If relative path fails, try absolute path, then prompt user to locate

### Missing File Handling

**Scenario:** User loads session, but `/Users/username/Audio/intro.wav` doesn't exist

**Current Behavior (v0.2.0):**
1. SessionManager logs warning: "File not found: intro.wav"
2. ClipButton displays "Missing File" state (red X icon)
3. User can right-click → "Locate File" to fix path
4. Session continues loading (graceful degradation)

**Future Enhancement (v0.3.0):**
- Auto-search common locations (~/Music, ~/Documents/Audio)
- Offer bulk relocation (update all clips with same base path)
- Track "last known location" history

## Version Migration

### Schema Version History

**v1.0.0 (October 2025):**
- Initial schema
- Supports: Clips, routing, preferences
- Fields: 20+ clip metadata fields

**v1.1.0 (Planned - v0.3.0):**
- Add: `fadeInCurve`, `fadeOutCurve` (curve shape selection)
- Add: `stopOthersOnPlay` (per-clip solo mode) - ALREADY IN v0.2.0
- Backward compatible: Old sessions default to Linear curves, stopOthersOnPlay=false

**v2.0.0 (Planned - v2.0):**
- Add: `effects` array (DSP effects metadata - EQ, compression, reverb)
- Add: `automation` array (parameter automation curves)
- Breaking change: Routing matrix expanded (4 groups → 8 groups + aux sends)
- Migration required: v1.x → v2.0 converter

### Migration Strategy

**Forward Compatibility (New Loader, Old Session):**
```cpp
if (sessionVersion == "1.0.0") {
    // Add missing fields with defaults
    for (auto& clip : clips) {
        if (!clip.has("fadeInCurve")) {
            clip["fadeInCurve"] = "Linear";  // Default
        }
        if (!clip.has("stopOthersOnPlay")) {
            clip["stopOthersOnPlay"] = false;  // Default
        }
    }
}
```

**Backward Compatibility (Old Loader, New Session):**
```cpp
if (sessionVersion > "1.0.0") {
    showError("This session was created with a newer version of Clip Composer. Please update to the latest version.");
    return;
}
```

**Migration Logic Location:**
- `Source/Session/SessionManager.cpp` - `loadSession()` method
- Check version, apply migration transformations
- Log migration actions (useful for debugging)

## Example Session File (Complete)

```json
{
  "sessionMetadata": {
    "name": "Evening Broadcast - October 12",
    "version": "1.0.0",
    "createdDate": "2025-10-12T12:00:00Z",
    "modifiedDate": "2025-10-12T15:30:00Z",
    "author": "John Doe",
    "description": "Evening show with music, SFX, and voice clips",
    "sampleRate": 48000,
    "bufferSize": 512
  },
  "clips": [
    {
      "handle": 1,
      "name": "Intro Music",
      "filePath": "/Users/johndoe/Audio/intro.wav",
      "buttonIndex": 0,
      "tabIndex": 0,
      "clipGroup": 0,
      "trimIn": 0,
      "trimOut": 240000,
      "gainDb": 0.0,
      "color": "#FF5733",
      "fadeInSamples": 2400,
      "fadeOutSamples": 4800,
      "fadeInCurve": "Exponential",
      "fadeOutCurve": "Logarithmic",
      "loopEnabled": false,
      "playbackMode": "OneShot",
      "loopStart": 0,
      "loopEnd": 240000,
      "stopOthersOnPlay": true,
      "cuePoints": [
        { "name": "Verse 1", "position": 48000, "color": "#00FF00" },
        { "name": "Chorus", "position": 96000, "color": "#FF00FF" },
        { "name": "Outro", "position": 192000, "color": "#0000FF" }
      ]
    }
  ],
  "routing": {
    "clipGroups": [
      { "name": "Music", "gainDb": 0.0, "mute": false, "solo": false },
      { "name": "SFX", "gainDb": -3.0, "mute": false, "solo": false },
      { "name": "Voice", "gainDb": 0.0, "mute": false, "solo": false },
      { "name": "Backup", "gainDb": -6.0, "mute": true, "solo": false }
    ],
    "masterGain": 0.0,
    "masterMute": false
  },
  "preferences": {
    "autoSave": true,
    "autoSaveInterval": 300,
    "showWaveforms": true,
    "colorScheme": "Dark",
    "audioDriver": "CoreAudio",
    "audioDevice": "Built-in Output",
    "audioBufferSize": 512,
    "audioSampleRate": 48000,
    "showPerformanceMonitor": true,
    "uiRefreshRate": 75
  }
}
```

**File Size:** ~800 bytes for 1 clip, ~500KB for 960 clips (acceptable)

## Common Workflows

### Workflow: Create New Session

1. User clicks "New Session"
2. SessionManager creates default SessionRoot:
   - sessionMetadata: Name="Untitled", version="1.0.0", sampleRate=48000
   - clips: Empty array []
   - routing: 4 default groups (Music, SFX, Voice, Backup)
   - preferences: User defaults (Dark theme, CoreAudio, 512 buffer, 75fps)
3. ClipGrid refreshes, shows empty 120 buttons
4. User drags audio files onto buttons
5. User clicks "Save" → Show file picker
6. SessionManager serializes to JSON, writes to disk

### Workflow: Load Existing Session

1. User clicks "Open Session"
2. File picker shows `~/Documents/Orpheus Clip Composer/Sessions/`
3. User selects `evening_broadcast.occSession`
4. SessionManager reads file, parses JSON
5. Validate schema version (check "version": "1.0.0")
6. Parse clips array (960 entries)
7. For each clip:
   - Open audio file (background thread)
   - Render waveform (background thread)
8. Parse routing config (4 groups)
9. Parse preferences (apply audio settings, UI theme)
10. ClipGrid refreshes, shows 120 buttons (tab 0)
11. Show notification: "Session loaded: 960 clips"

### Workflow: Edit Clip Metadata, Save Session

1. User right-clicks ClipButton #5 → "Edit Clip"
2. ClipEditDialog loads clip metadata from SessionManager
3. User changes trim IN (0 → 48,000 samples)
4. User changes fade OUT (0 → 2,400 samples)
5. User changes color ("#FF5733" → "#00FF00")
6. User clicks "Save" in dialog
7. ClipEditDialog returns updated metadata
8. SessionManager updates clip in internal state
9. User presses Cmd+S (save session)
10. SessionManager serializes updated clip to JSON
11. Write JSON to disk (overwrite existing file)
12. Show confirmation: "Session saved"

## Where to Make Changes

### Feature: Add DSP Effects Metadata

**Impact on Schema:**
- Add `effects` array to ClipMetadata
- Each effect: `{ "type": "EQ", "params": { "freq": 1000, "gain": 3.0 } }`

**Files to Modify:**
- `Source/Session/SessionManager.cpp` - Serialize/deserialize effects array
- `docs/occ/OCC097.md` - Update session schema documentation
- Increment schema version to v2.0.0

**Example JSON:**
```json
{
  "handle": 1,
  "effects": [
    { "type": "EQ", "params": { "freq": 1000, "gain": 3.0, "Q": 1.0 } },
    { "type": "Compressor", "params": { "threshold": -12.0, "ratio": 4.0 } }
  ]
}
```

### Feature: Support Relative Paths

**Impact on Schema:**
- No schema change (just interpretation of `filePath` field)
- If path starts with "./", resolve relative to session file

**Files to Modify:**
- `Source/Session/SessionManager.cpp` - Add path resolution logic

**Implementation:**
```cpp
juce::File resolveFilePath(const std::string& filePath, const juce::File& sessionFile) {
    if (filePath.startsWith("./")) {
        // Relative to session file
        return sessionFile.getParentDirectory().getChildFile(filePath.substring(2));
    } else {
        // Absolute path
        return juce::File(filePath);
    }
}
```

### Bug Fix: Session Save Fails with Unicode Filenames

**Investigation Path:**
1. Check JSON encoding (should be UTF-8)
2. Verify file write uses UTF-8 encoding
3. Test with non-ASCII characters (e.g., "音楽.wav")
4. Check JUCE File API documentation

**Common Causes:**
- JSON library not configured for UTF-8 (set encoding explicitly)
- File path encoding mismatch (Windows UTF-16 vs. UTF-8)

**Files to Check:**
- `Source/Session/SessionManager.cpp` - `saveSession()` method

## Performance Considerations

### Session Size

**Typical Session:**
- Session metadata: ~200 bytes
- 960 clips × 500 bytes/clip = 480KB
- Routing config: ~500 bytes
- Preferences: ~300 bytes
- **Total: ~500KB** (acceptable for JSON)

**Large Session:**
- 960 clips with many cue points (10+ per clip) = ~1MB
- Still acceptable (load time <200ms on modern hardware)

### Load Time

**Profiling Results (960-clip session, macOS M1, Release Build):**
- Read file from disk: ~10ms
- Parse JSON: ~50ms
- Validate schema: ~5ms
- Open audio files (background thread): ~500ms (parallel)
- Render waveforms (background thread): ~300ms (parallel)
- **Total perceived load time:** ~1 second

**Optimization Opportunities:**
- Cache parsed sessions (avoid re-parsing on reload)
- Lazy-load audio files (only when button visible or played)
- Pre-render waveforms (cache to disk, load on demand)

## Related Diagrams

- **3-component-map.mermaid.md** - Shows SessionManager component (handles JSON parsing)
- **4-data-flow.mermaid.md** - Shows session save/load data flow
- **5-entry-points.mermaid.md** - Shows session loading as entry point

## Cross-References to OCC Docs

- **OCC022** - Clip Metadata Schema v1.0 (authoritative spec for ClipMetadata fields)
- **OCC097** - Session Format (complete JSON schema reference, code examples)
- **OCC096** - SDK Integration Patterns (how SessionManager interacts with AudioEngine)

---

**Last Updated:** October 31, 2025
**Version:** v0.2.0
**Schema Version:** 1.0.0
**Maintained By:** Claude Code + Human Developers
