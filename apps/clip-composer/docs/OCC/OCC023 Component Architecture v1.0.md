# OCC023 Component Architecture v1.0

**Document Version:** 1.0
**Date:** October 12, 2025
**Status:** Draft
**Supersedes:** None

---

## Executive Summary

This document defines the component architecture for Orpheus Clip Composer, establishing clear separation between:

1. **Orpheus SDK** - Host-neutral audio engine foundation
2. **OCC Application Layer** - Clip triggering, UI, recording, remote control
3. **OCC Adapters** - Platform-specific integrations (drivers, protocols)

**Core Principle:** OCC is a **full-featured application** that uses Orpheus SDK as its audio engine, not a thin wrapper around SDK functionality.

---

## 1. Architectural Layers

```
┌─────────────────────────────────────────────────────────────┐
│                   USER INTERFACE LAYER                       │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  UI Framework (JUCE or Electron - TBD)              │   │
│  │  • ClipGridView (10×12 grid, 8 tabs, dual view)    │   │
│  │  • WaveformEditorPanel (bottom panel: Editor)       │   │
│  │  • ClipLabellingPanel (bottom panel: Labelling)     │   │
│  │  • RoutingMatrixPanel (bottom panel: Routing)       │   │
│  │  • PreferencesPanel (bottom panel: Preferences)     │   │
│  │  • TransportControls (play, stop, record)           │   │
│  │  • PerformanceMonitor (CPU, latency meters)         │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                           ▼ (UI events)
┌─────────────────────────────────────────────────────────────┐
│               OCC APPLICATION LOGIC LAYER                    │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  ClipTriggerEngine                                   │   │
│  │  • MIDI/OSC input handling                           │   │
│  │  • Grid button trigger logic                        │   │
│  │  • Master/slave linking coordination                │   │
│  │  • AutoPlay/sequential playback                     │   │
│  └─────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  RecordingManager                                    │   │
│  │  • Direct-to-button recording                       │   │
│  │  • Input routing (Record A/B, LTC)                  │   │
│  │  • Recording metadata capture                       │   │
│  └─────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  SessionManager                                      │   │
│  │  • Session save/load (JSON format)                  │   │
│  │  • Intelligent file recovery                        │   │
│  │  • Project media folder management                  │   │
│  │  • Auto-save (incremental, non-blocking)            │   │
│  └─────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  RemoteControlServer                                 │   │
│  │  • WebSocket server (iOS companion app)             │   │
│  │  • OSC message routing                              │   │
│  │  • MIDI remote control mapping                      │   │
│  └─────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  LoggingEngine                                       │   │
│  │  • Real-time event logging (CSV/JSON/XML)           │   │
│  │  • Cloud sync integration (optional)                │   │
│  │  • Performance diagnostics                          │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                           ▼ (audio commands)
┌─────────────────────────────────────────────────────────────┐
│              ORPHEUS SDK INTEGRATION LAYER                   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  OCC-to-SDK Adapter                                  │   │
│  │  • Translates OCC clip metadata to SessionGraph     │   │
│  │  • Maps UI events to TransportController commands   │   │
│  │  • Bridges routing UI to RoutingMatrix              │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                           ▼ (SDK API calls)
┌─────────────────────────────────────────────────────────────┐
│                   ORPHEUS SDK CORE                           │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  SessionGraph                                        │   │
│  │  • Tracks, clips, tempo, transport state            │   │
│  │  • Deterministic rendering pipeline                 │   │
│  └─────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  TransportController                                 │   │
│  │  • Play, stop, record, loop control                 │   │
│  │  • Sample-accurate timing                           │   │
│  │  • LTC/MTC/MIDI Clock sync                          │   │
│  └─────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  RoutingMatrix                                       │   │
│  │  • Flexible input/output routing                    │   │
│  │  • Multi-channel bus management                     │   │
│  └─────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  AudioFileReader                                     │   │
│  │  • Multi-format decoding (WAV, AIFF, FLAC, MP3)     │   │
│  │  • Streaming playback (memory-efficient)            │   │
│  └─────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  PerformanceMonitor                                  │   │
│  │  • Real-time diagnostics (CPU, latency, dropouts)   │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                           ▼ (audio I/O)
┌─────────────────────────────────────────────────────────────┐
│                 PLATFORM AUDIO DRIVERS                       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │  macOS       │  │  Windows     │  │  iOS         │     │
│  │  CoreAudio   │  │  ASIO        │  │  CoreAudio   │     │
│  │              │  │  WASAPI      │  │  AUv3        │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Component Descriptions

### 2.1 UI Framework Layer

**Responsibility:** User interaction, visual feedback, input handling

**Components:**

**ClipGridView**
- Renders 10×12 button grid with 8 tabs and dual-tab view
- Handles button stretching (up to 4 cells)
- Displays clip colors, labels, waveforms
- Processes click/touch events for clip triggering

**WaveformEditorPanel**
- Bottom panel: Editor mode
- Waveform visualization with zoom/pan
- Mouse logic: Left=IN, Right=OUT, Middle=playhead jump
- Transport controls (play, pause, stop, loop toggle)
- DSP controls (time-stretch, pitch-shift)
- IN/OUT time entry fields

**ClipLabellingPanel**
- Bottom panel: Labelling mode
- Two tabs: "Label & Appearance" and "Metadata"
- Edit clip name, color, font size
- Schema-specific copy/paste for metadata

**RoutingMatrixPanel**
- Bottom panel: Routing mode
- Two tabs: "Outputs" and "Inputs"
- Visual matrix for routing configuration
- Shows: Clip Groups A/B/C/D → Output channels
- Shows: Record A/B, LTC → Input channels

**PreferencesPanel**
- Bottom panel: Preferences mode
- Application-wide settings (theme, layout, remote control)
- Persistent playback controls

**TransportControls**
- Global transport (currently not clip-specific)
- Future: Session-level playback control

**PerformanceMonitor**
- Real-time CPU/latency/dropout display
- Visual feedback for system health

---

### 2.2 OCC Application Logic Layer

**Responsibility:** Clip management, triggering logic, session state, remote control

**Components:**

**ClipTriggerEngine**
- Maps UI/MIDI/OSC events to clip playback commands
- Implements FIFO choke logic (per Clip Group)
- Handles master/slave linking (SpotOn-inspired)
- Executes AutoPlay/sequential playback
- Manages "stop others on trigger" behavior

**Key Methods:**
```cpp
void triggerClip(ClipUUID uuid);
void stopClip(ClipUUID uuid);
void stopAllInGroup(ClipGroup group);
void linkClipsAsMasterSlave(ClipUUID master, std::vector<ClipUUID> slaves);
void enableAutoPlay(ClipUUID current, ClipUUID next, int64_t delaySamples);
```

**RecordingManager**
- Captures audio from input channels (Record A/B)
- Records directly into designated clip button
- Embeds recording metadata (timestamp, LTC, operator)
- Supports simultaneous playback and recording

**Key Methods:**
```cpp
void startRecordingToButton(ClipUUID target, InputChannel channel);
void stopRecording();
bool isRecording();
RecordingStatus getStatus();
```

**SessionManager**
- Loads/saves session files (.occ format, JSON)
- Implements intelligent file recovery (hash-based search)
- Manages project media folder
- Auto-save with incremental, non-blocking writes

**Key Methods:**
```cpp
void saveSession(std::filesystem::path path);
Session loadSession(std::filesystem::path path);
void enableAutoSave(int intervalSeconds);
std::vector<std::filesystem::path> findMissingFiles(Session& session);
```

**RemoteControlServer**
- WebSocket server for iOS companion app
- OSC message routing (port 8000 default, configurable)
- MIDI remote control mapping

**Key Methods:**
```cpp
void startServer(int port);
void stopServer();
void broadcastClipStatus(ClipUUID uuid, PlaybackStatus status);
void handleRemoteTrigger(ClipUUID uuid);
```

**LoggingEngine**
- Real-time event logging (clip start/stop, duration, errors)
- Export formats: CSV, JSON, XML
- Optional cloud sync (Google Drive, Dropbox, AWS S3)
- Performance diagnostics logging

**Key Methods:**
```cpp
void logEvent(EventType type, ClipUUID uuid, std::optional<nlohmann::json> metadata);
void exportLog(std::filesystem::path path, LogFormat format);
void enableCloudSync(CloudProvider provider, std::string credentials);
```

---

### 2.3 Orpheus SDK Integration Layer

**Responsibility:** Bridge between OCC application logic and Orpheus SDK primitives

**OCC-to-SDK Adapter**

**Purpose:** Translate OCC-specific concepts (clip buttons, grid layout, master/slave linking) into SDK-compatible operations (SessionGraph updates, TransportController commands).

**Key Responsibilities:**
- Convert OCC clip metadata (JSON) to SDK SessionGraph clip entries
- Map OCC routing configuration to SDK RoutingMatrix
- Translate clip trigger events to SDK transport commands
- Handle sample-rate conversion when clip rate ≠ session rate

**Example Translation:**
```cpp
// OCC layer: User triggers clip button
ClipTriggerEngine::triggerClip(uuid);

// Adapter translates to SDK:
SessionGraph& graph = sdk.getSessionGraph();
ClipHandle handle = graph.findClipByUUID(uuid);
TransportController& transport = sdk.getTransportController();
transport.startClipPlayback(handle, /* startSample */ 0);
```

**Design Principle:** Adapter is stateless translation layer, not business logic.

---

### 2.4 Orpheus SDK Core

**Responsibility:** Deterministic audio engine, sample-accurate transport, routing

**What OCC Uses from SDK:**

**SessionGraph**
- Holds clip references, tempo, time signature
- Provides deterministic render pipeline
- Supports offline and real-time rendering modes

**TransportController**
- Sample-accurate playback control
- LTC/MTC/MIDI Clock synchronization
- Loop and punch record modes

**RoutingMatrix**
- Flexible multi-channel routing
- Dynamic re-patching during playback
- Support for arbitrary channel counts

**AudioFileReader**
- Multi-format decoding (WAV, AIFF, FLAC, MP3, AAC, OGG)
- Streaming playback (no full-file loading)
- Memory-mapped I/O for efficiency

**PerformanceMonitor**
- Real-time CPU usage tracking
- Latency measurement
- Dropout detection

**What SDK Does NOT Provide (OCC implements):**
- Clip grid UI rendering
- Button triggering logic
- Session JSON serialization/deserialization
- Remote control protocols (WebSocket, OSC)
- Recording workflow
- File recovery heuristics

---

## 3. Data Flow Diagrams

### 3.1 Clip Trigger Flow

```
User clicks clip button
    ↓
ClipGridView captures click event
    ↓
ClipTriggerEngine::triggerClip(uuid)
    ↓
Check FIFO choke: stop other clips in same group?
    ↓
Check master/slave linking: trigger linked clips?
    ↓
OCC-to-SDK Adapter translates to SDK command
    ↓
TransportController::startClipPlayback(handle, startSample)
    ↓
SessionGraph retrieves clip audio data
    ↓
AudioFileReader streams samples
    ↓
Apply DSP (time-stretch, pitch-shift, fades)
    ↓
RoutingMatrix routes to assigned outputs
    ↓
Platform driver (CoreAudio/ASIO) sends to hardware
    ↓
User hears audio
```

### 3.2 Recording Flow

```
User clicks "Record to Button" + selects target button
    ↓
RecordingManager::startRecordingToButton(uuid, Record_A)
    ↓
OCC-to-SDK Adapter enables input monitoring
    ↓
SessionGraph creates temporary recording clip
    ↓
Platform driver (CoreAudio/ASIO) captures input samples
    ↓
Samples written to temp file (WAV format)
    ↓
User clicks "Stop Recording"
    ↓
RecordingManager::stopRecording()
    ↓
Temp file moved to project media folder
    ↓
Clip metadata created (recording timestamp, LTC, etc.)
    ↓
SessionManager updates session state
    ↓
ClipGridView updates button to show new recorded clip
```

### 3.3 Session Load Flow

```
User selects "Open Session" → file picker
    ↓
SessionManager::loadSession(path)
    ↓
Parse JSON session file
    ↓
Validate schema version
    ↓
For each clip in session:
    ↓
    Check if audio file exists at relative path
    ↓
    If missing: use file_hash to search for relocated files
    ↓
    If found: prompt user for confirmation
    ↓
    If not found: mark clip as "missing media"
    ↓
OCC-to-SDK Adapter populates SessionGraph with valid clips
    ↓
RoutingMatrix configured per session routing settings
    ↓
ClipGridView renders grid with clip buttons
    ↓
Session ready for use
```

---

## 4. Threading Model

### 4.1 Thread Responsibilities

**Main UI Thread**
- Render UI (60fps target)
- Handle user input (mouse, keyboard, touch)
- Update visual feedback (button states, waveforms, meters)

**Audio Thread (Real-Time Priority)**
- Sample generation and mixing
- Driver I/O (CoreAudio/ASIO callbacks)
- **NO allocations, NO mutexes, NO disk I/O**

**Background I/O Thread**
- Load audio files asynchronously
- Waveform rendering
- Session save/load (non-blocking)
- File search (intelligent recovery)

**Network Thread**
- WebSocket server (iOS companion app)
- OSC message handling
- Cloud sync (optional logging)

**Recording Thread**
- Capture input samples
- Write to disk (ring buffer → file)
- Metadata embedding

### 4.2 Thread Communication

**Audio Thread → UI Thread:**
- Lock-free ring buffer for playback state updates
- Atomic flags for clip status (playing, stopped, error)

**UI Thread → Audio Thread:**
- Lock-free command queue for clip triggers
- Atomic parameters for volume, pan, routing changes

**Background I/O Thread → Main Thread:**
- Message queue for "file loaded" notifications
- Callback system for async operations

---

## 5. Dependency Graph

```
┌─────────────────────────────────────────────────────────────┐
│ OCC Application                                              │
│   Depends on:                                                │
│   • Orpheus SDK (audio engine)                               │
│   • UI Framework (JUCE or Electron - TBD)                   │
│   • JSON library (nlohmann/json)                            │
│   • WebSocket library (libwebsockets or Boost.Beast)        │
│   • OSC library (oscpack or liblo)                          │
│   • Optional: Cloud sync SDKs (Google Drive, AWS S3)        │
└─────────────────────────────────────────────────────────────┘
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ Orpheus SDK                                                  │
│   Depends on:                                                │
│   • Platform audio drivers (CoreAudio, ASIO, WASAPI)        │
│   • Audio codec libraries (FLAC, libsndfile, etc.)          │
│   • DSP libraries (Rubber Band, SoundTouch, Sonic)          │
│   • C++20 standard library                                  │
└─────────────────────────────────────────────────────────────┘
```

**Build Flags for Modularity:**
```cmake
# OCC-specific build
ORPHEUS_BUILD_REALTIME_ENGINE=ON
ORPHEUS_BUILD_CLIP_GRID=ON
OCC_ENABLE_REMOTE_CONTROL=ON
OCC_ENABLE_RECORDING=ON
OCC_ENABLE_CLOUD_SYNC=OFF  # Optional

# SDK-only build (for other applications)
ORPHEUS_BUILD_REALTIME_ENGINE=OFF
ORPHEUS_BUILD_CLIP_GRID=OFF
```

---

## 6. Extensibility Points

### 6.1 Plugin Architecture (Future: v2.0)

**VST3 Hosting:**
- Per-clip DSP effects
- Master bus effects
- AU support on macOS

**Custom Protocols:**
- Plugin API for external control surfaces
- Third-party remote control integrations

### 6.2 Scripting (Future)

**Lua or JavaScript scripting:**
- Custom automation rules (Ovation-inspired interaction rules)
- Conditional triggering
- Dynamic clip parameter adjustments

---

## 7. Platform-Specific Components

### 7.1 macOS-Specific

**CoreAudio Integration:**
- Aggregated device support
- Sample rate negotiation
- Buffer size management

**App Sandbox:**
- Security-scoped bookmarks for file access
- Entitlements for audio recording

### 7.2 Windows-Specific

**ASIO Integration:**
- ASIO SDK integration
- Dynamic ASIO driver detection
- Fallback to WASAPI exclusive mode

**Windows Audio Session API:**
- Device enumeration
- Notification handling for device changes

### 7.3 iOS-Specific

**CoreAudio + AUv3:**
- Inter-app audio (AUv3 hosting)
- Background audio modes
- Interruption handling (phone calls)

**Network Discovery:**
- Bonjour/mDNS for discovering desktop OCC instances

---

## 8. Configuration Management

### 8.1 Application Preferences

**Stored in:** Platform-specific locations
- macOS: `~/Library/Application Support/OrpheusClipComposer/preferences.json`
- Windows: `%APPDATA%\OrpheusClipComposer\preferences.json`
- iOS: UserDefaults (plist)

**Preference Categories:**
- UI theme and layout
- Audio driver selection and buffer size
- Remote control port settings
- Cloud sync credentials (encrypted)
- Keyboard shortcuts
- Default project media folder

### 8.2 Session Files

**Format:** JSON (.occ extension)
**Structure:**
```json
{
  "session_metadata": { /* per OCC009 */ },
  "clips": [ /* array of OCC022 clip metadata */ ],
  "routing": { /* RoutingMatrix configuration */ },
  "transport": { /* tempo, time signature, sync source */ }
}
```

**Portability:**
- Relative file paths (session-relative)
- Optional media consolidation (copy files to project folder)

---

## 9. Error Handling Strategy

### 9.1 Crash-Proof Architecture

**Audio Thread:**
- **Never crash** - catch and log all exceptions
- Gracefully degrade (silence output if error)
- Set error flag for UI notification

**UI Thread:**
- Display user-friendly error messages
- Offer recovery options (reload session, locate missing files)
- Log detailed diagnostics for support

**File I/O:**
- Atomic writes for session files (write to .tmp, then rename)
- Verify file integrity before overwriting

### 9.2 Diagnostics

**Performance Monitoring:**
- Real-time CPU usage per thread
- Latency spikes detection
- Buffer underrun/overrun tracking

**Event Logging:**
- All clip triggers logged with timestamp
- Errors logged with stack trace
- Session state changes logged

---

## 10. Relationship to Orpheus SDK Roadmap

**OCC Drives SDK Evolution:**

**Phase 1 (MVP):**
- SDK provides: SessionGraph, TransportController, AudioFileReader
- OCC implements: Clip triggering UI, basic recording

**Phase 2 (v1.0):**
- SDK adds: Real-time playback mode, input audio graphs
- OCC implements: Advanced editor, iOS app, remote control

**Phase 3 (v2.0):**
- SDK adds: Plugin architecture (VST3 hosting), advanced routing
- OCC implements: Per-clip effects, interaction rules, GPI triggering

**Feedback Loop:**
- OCC identifies SDK limitations during implementation
- SDK team prioritizes features based on OCC requirements
- Other applications (Wave Finder, FX Engine) benefit from SDK improvements

---

## 11. Security & Data Integrity

**Audio Thread Safety:**
- No allocations in real-time code paths
- Lock-free communication with UI thread

**File Access:**
- Validate all file paths before access
- Sandboxing on macOS/iOS

**Network Security:**
- WebSocket connections: optional TLS/SSL
- OSC: localhost-only by default (opt-in for network access)
- No telemetry without explicit user consent

**Session Integrity:**
- JSON schema validation on load
- File hash verification for audio files
- Auto-save prevents data loss

---

## 12. Testing Strategy

### 12.1 Unit Tests
- Individual component testing (ClipTriggerEngine, SessionManager, etc.)
- Mock SDK dependencies for isolated testing

### 12.2 Integration Tests
- Full UI → SDK → Audio Driver pipeline
- Cross-platform compatibility (macOS, Windows, iOS)

### 12.3 Performance Tests
- Latency benchmarks (target: <5ms ASIO, <10ms WASAPI)
- CPU usage under load (16 simultaneous clips)
- Memory leak detection (long-duration stress tests)

### 12.4 Reliability Tests
- 24/7 continuous operation (broadcast use case)
- Device hotplug/unplug handling
- Session corruption recovery

---

## 13. Related Documents

- **OCC021** - Product Vision (authoritative direction)
- **OCC022** - Clip Metadata Schema (data model)
- **OCC009** - Session Metadata Manifest (session-level data)
- **OCC011** - Wireframes v2 (UI design)
- **OCC013** - Audio Driver Integration (platform-specific)
- **Orpheus SDK AGENTS.md** - SDK coding guidelines

---

## 14. Open Questions for Resolution

1. **UI Framework Decision** - JUCE vs Electron? (Impacts entire UI layer architecture)
2. **Plugin API** - VST3 only or also AU/LV2? (Impacts v2.0 design)
3. **Scripting Engine** - Lua, JavaScript, or built-in rules engine? (Future extensibility)
4. **Cloud Sync** - Direct API integration or file-based sync? (Network layer design)

---

**Document Status:** Ready for technical review and implementation planning.
