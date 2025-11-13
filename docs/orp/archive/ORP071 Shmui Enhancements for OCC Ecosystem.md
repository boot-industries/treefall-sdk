# ORP071 - Shmui Enhancements for OCC Ecosystem

**Document ID:** ORP071
**Version:** 1.0
**Status:** Authoritative
**Date:** October 12, 2025
**Author:** OCC Development Team
**Related Plans:** ORP070 (OCC MVP Sprint)

---

## Executive Summary

This document outlines Shmui (Next.js/React UI prototypes) enhancements to support Orpheus Clip Composer development and the broader audio ecosystem. While OCC uses JUCE for production UI, Shmui provides critical value for rapid prototyping, session visualization, remote control interfaces, and documentation.

**Goal:** Leverage Shmui's fast iteration cycle to prototype OCC features, build the iOS companion app foundation, and create interactive documentation.

**Timeline:** Parallel with ORP070 (Months 1-6, 2025-2026)
**Effort:** ~1-2 weeks per module (lightweight, demo-focused)

---

## Table of Contents

1. [Rationale](#rationale)
2. [Architecture](#architecture)
3. [Module 1: Session File Viewer](#module-1-session-file-viewer)
4. [Module 2: Clip Grid Simulator](#module-2-clip-grid-simulator)
5. [Module 3: Remote Control Prototype](#module-3-remote-control-prototype)
6. [Module 4: Performance Visualizer](#module-4-performance-visualizer)
7. [Module 5: Waveform Renderer](#module-5-waveform-renderer)
8. [Module 6: Routing Matrix Simulator](#module-6-routing-matrix-simulator)
9. [Integration Strategy](#integration-strategy)
10. [Success Criteria](#success-criteria)

---

## Rationale

### Why Shmui for OCC Development?

**Problem:** JUCE development is slow for UI iteration

- C++ compile times (~90 seconds for full rebuild)
- No hot reload (must restart app to see changes)
- Difficult to prototype animations/interactions
- Hard to share UI concepts with stakeholders

**Solution:** Use Shmui for rapid prototyping

- Web development with hot reload (<1 second)
- Easy to experiment with layouts/interactions
- Shareable URLs for stakeholder review
- Foundation for iOS companion app
- Interactive documentation/demos

**Philosophy:** "Prototype in Shmui, implement in JUCE"

- Fast iteration → better UX decisions
- Test session formats before C++ implementation
- Build remote control before hardware integration
- Create marketing demos early

### Use Cases

1. **OCC Development Team:**
   - Prototype clip grid layouts (10×12? 8×6? Different tab strategies?)
   - Test keyboard mapping visualizations
   - Experiment with routing panel designs
   - Mock performance monitoring displays

2. **iOS Companion App:**
   - Remote control interface (trigger clips from iPad)
   - Session browser (view/edit on mobile)
   - Performance monitoring (CPU/latency in real-time)

3. **Documentation/Marketing:**
   - Interactive demos for website
   - Tutorial walkthroughs
   - Feature showcases for investors/users

4. **SDK Integration:**
   - Visualize SDK performance data
   - Debug transport callbacks
   - Display audio metrics
   - Test session formats

---

## Architecture

### Shmui Structure

```
packages/shmui/
├── apps/
│   └── www/                    # Main demo site (localhost:4000)
│       ├── app/
│       │   ├── occ/            # OCC-specific demos (NEW)
│       │   │   ├── session-viewer/
│       │   │   ├── clip-grid/
│       │   │   ├── remote-control/
│       │   │   ├── performance/
│       │   │   ├── waveform/
│       │   │   └── routing/
│       │   └── demos/          # Existing demos
│       └── components/
│           └── occ/            # Reusable OCC components (NEW)
│               ├── ClipButton.tsx
│               ├── ClipGrid.tsx
│               ├── SessionViewer.tsx
│               ├── PerformanceMonitor.tsx
│               └── RoutingMatrix.tsx
└── packages/
    └── occ-types/              # Shared TypeScript types (NEW)
        ├── session.ts          # Session JSON schema
        ├── clip.ts             # Clip metadata
        └── performance.ts      # Performance metrics
```

### Technology Stack

- **Framework:** Next.js 14 (React 18)
- **Styling:** Tailwind CSS
- **State:** Zustand (lightweight state management)
- **Audio:** Web Audio API (for waveform rendering)
- **Charts:** Recharts (performance graphs)
- **Communication:** WebSocket (for remote control)

### Design Principles

1. **Mock-first:** All demos work offline with mock data
2. **Real-time ready:** Structure allows real SDK integration later
3. **Responsive:** Works on desktop, tablet, mobile
4. **Professional:** Match OCC's dark theme, Inter font
5. **Fast:** <100ms interaction latency

---

## Module 1: Session File Viewer

**Priority:** HIGH
**Effort:** 3 days
**Dependencies:** None

### Objective

Create a web-based viewer/editor for OCC session files (.json) to:

- Validate session format before implementing in C++
- Visualize clip assignments
- Edit clip metadata (name, color, group)
- Export modified sessions

### Features

#### 1.1 File Upload

- Drag-and-drop .json files
- Parse session JSON
- Display validation errors

#### 1.2 Session Overview

- Session name and metadata
- Total clips loaded
- Button utilization (e.g., "12/48 buttons used")
- File path validation (highlight missing files)

#### 1.3 Clip List

- Table view: Button Index | Name | File Path | Sample Rate | Duration | Group
- Sortable columns
- Filter by group (0-3)
- Search by clip name

#### 1.4 Visual Grid Preview

- Show 6×8 grid (or configurable layout)
- Clip colors match assignments
- Click to edit clip metadata
- Empty slots shown as grey

#### 1.5 Editor

- Edit clip name
- Change clip color (color picker)
- Reassign group (dropdown)
- Move clip to different button (drag-and-drop)

#### 1.6 Export

- Download modified session as .json
- Validate before export
- Pretty-print JSON

### UI Mockup

```
┌─────────────────────────────────────────────────────────┐
│ Session Viewer                                  [Upload]│
├─────────────────────────────────────────────────────────┤
│ Session: "Evening Broadcast"                            │
│ Clips: 12/48    Sample Rate: 48kHz                      │
├─────────────────────────────────────────────────────────┤
│ ┌─────┬─────┬─────┬─────┬─────┬─────┐                  │
│ │ Q   │ W   │ E   │ R   │ T   │ Y   │  [Clip Grid]     │
│ │Intro│Music│ SFX │     │     │     │                  │
│ ├─────┼─────┼─────┼─────┼─────┼─────┤                  │
│ │ A   │ S   │ D   │ F   │ G   │ H   │                  │
│ │     │     │     │     │     │     │                  │
│ └─────┴─────┴─────┴─────┴─────┴─────┘                  │
├─────────────────────────────────────────────────────────┤
│ Clip List                           [Search...] [Filter]│
│ ┌───┬────────┬───────────────┬────────┬──────┬──────┐  │
│ │ 0 │ Intro  │/music/intro...│ 48kHz  │ 5.2s │ Grp0 │  │
│ │ 1 │ Music  │/music/song... │ 48kHz  │ 3:45 │ Grp0 │  │
│ │ 2 │ SFX    │/sfx/beep.wav  │ 48kHz  │ 0.5s │ Grp1 │  │
│ └───┴────────┴───────────────┴────────┴──────┴──────┘  │
└─────────────────────────────────────────────────────────┘
                            [Export JSON] [Download]
```

### Acceptance Criteria

- [ ] Upload .json session file
- [ ] Display clip grid with colors
- [ ] List all clips in table
- [ ] Edit clip metadata (name, color, group)
- [ ] Drag-and-drop to reassign buttons
- [ ] Export modified session
- [ ] Validate file paths (highlight missing files)

### Files to Create

- `apps/www/app/occ/session-viewer/page.tsx`
- `apps/www/components/occ/SessionViewer.tsx`
- `apps/www/components/occ/ClipGrid.tsx`
- `apps/www/components/occ/ClipButton.tsx`
- `packages/occ-types/session.ts`

---

## Module 2: Clip Grid Simulator

**Priority:** HIGH
**Effort:** 2 days
**Dependencies:** Module 1 (ClipButton component)

### Objective

Interactive clip grid simulator to:

- Test different grid layouts (6×8, 10×12, 8×6)
- Experiment with keyboard mappings
- Prototype visual feedback (playing state, colors, brightness)
- Test tab switching UI

### Features

#### 2.1 Grid Layout Tester

- Switch between layouts: 6×8 (MVP), 10×12 (Full), 8×6 (Compact)
- Show keyboard labels on buttons (Q, W, E, etc.)
- Highlight keys on keyboard press
- Display button count and utilization

#### 2.2 Interaction Simulator

- Click to toggle play/stop
- Keyboard shortcuts work (Q-Y, A-H, etc.)
- Visual feedback (brightness, border, animation)
- Show state: Empty | Loaded | Playing | Stopping

#### 2.3 Tab Switcher

- 8 tabs (Tab A-H)
- Switch via buttons or number keys (1-8)
- Show clips per tab
- Highlight active tab

#### 2.4 Theme Editor

- Adjust colors (empty, loaded, playing)
- Change button size/spacing
- Toggle labels (keyboard, button index)
- Export theme as CSS variables

### UI Mockup

```
┌─────────────────────────────────────────────────────────┐
│ Clip Grid Simulator                                     │
├─────────────────────────────────────────────────────────┤
│ Layout: [6×8] [10×12] [8×6]    Theme: [Dark] [Light]   │
│ Show: [✓] Keyboard  [✓] Index  [ ] Names               │
├─────────────────────────────────────────────────────────┤
│ Tabs: [A] [B] [C] [D] [E] [F] [G] [H]    Active: A     │
├─────────────────────────────────────────────────────────┤
│ ┌─────┬─────┬─────┬─────┬─────┬─────┐                  │
│ │ Q   │ W   │ E   │ R   │ T   │ Y   │                  │
│ │  0  │  1  │  2  │  3  │  4  │  5  │  [Playing]       │
│ ├─────┼─────┼─────┼─────┼─────┼─────┤                  │
│ │ A   │ S   │ D   │ F   │ G   │ H   │                  │
│ │  6  │  7  │  8  │  9  │ 10  │ 11  │                  │
│ └─────┴─────┴─────┴─────┴─────┴─────┘                  │
├─────────────────────────────────────────────────────────┤
│ Stats: 48 buttons | 12 loaded | 3 playing               │
└─────────────────────────────────────────────────────────┘
```

### Acceptance Criteria

- [ ] Switch between grid layouts (6×8, 10×12, 8×6)
- [ ] Keyboard shortcuts trigger buttons
- [ ] Visual feedback matches OCC design
- [ ] Tab switching works (8 tabs)
- [ ] Theme editor exports CSS
- [ ] Responsive (works on iPad)

### Files to Create

- `apps/www/app/occ/clip-grid/page.tsx`
- `apps/www/components/occ/TabSwitcher.tsx`
- `apps/www/components/occ/ThemeEditor.tsx`

---

## Module 3: Remote Control Prototype

**Priority:** MEDIUM
**Effort:** 4 days
**Dependencies:** Module 2 (ClipGrid), WebSocket server

### Objective

Prototype iOS companion app that controls OCC remotely:

- Trigger clips from iPad/iPhone
- View session state
- Adjust volume (future: routing matrix)
- Monitor performance

### Features

#### 3.1 Connection

- WebSocket connection to OCC (localhost:8080 for dev)
- Connection status indicator
- Auto-reconnect on disconnect
- Secure connection (WSS) for production

#### 3.2 Remote Clip Grid

- Display current session's clip grid
- Tap to trigger clips
- Visual feedback (playing state synced)
- Swipe to switch tabs

#### 3.3 Transport Controls

- Stop All button (prominent)
- PANIC button (red, emergency)
- Master volume slider (future)

#### 3.4 Session Browser

- List available sessions
- Load session remotely
- Show session metadata

#### 3.5 Performance Monitor

- CPU usage bar
- Latency display (ms)
- Active clips count
- Buffer health indicator

### Protocol (WebSocket Messages)

**Client → Server (OCC):**

```json
{
  "type": "trigger_clip",
  "buttonIndex": 0,
  "tabIndex": 0
}

{
  "type": "stop_all"
}

{
  "type": "load_session",
  "filePath": "/path/to/session.json"
}
```

**Server → Client:**

```json
{
  "type": "state_update",
  "clips": [
    {"buttonIndex": 0, "state": "playing", "name": "Intro"}
  ]
}

{
  "type": "performance",
  "cpu": 23.5,
  "latency": 8.2,
  "activeClips": 3
}
```

### UI Mockup (iPad)

```
┌─────────────────────────────────────────────────────────┐
│ ● Connected to "MacBook Pro"              [Settings]    │
├─────────────────────────────────────────────────────────┤
│ Session: "Evening Broadcast"              CPU: 23%      │
├─────────────────────────────────────────────────────────┤
│ Tabs: [A] [B] [C] [D] [E] [F] [G] [H]                  │
├─────────────────────────────────────────────────────────┤
│ ┌──────┬──────┬──────┬──────┬──────┬──────┐            │
│ │Intro │Music │ SFX  │      │      │      │            │
│ │ ●    │      │      │      │      │      │ [Playing]  │
│ ├──────┼──────┼──────┼──────┼──────┼──────┤            │
│ │      │      │      │      │      │      │            │
│ │      │      │      │      │      │      │            │
│ └──────┴──────┴──────┴──────┴──────┴──────┘            │
├─────────────────────────────────────────────────────────┤
│                 [STOP ALL]    [PANIC]                   │
└─────────────────────────────────────────────────────────┘
```

### Acceptance Criteria

- [ ] WebSocket connection to OCC
- [ ] Trigger clips remotely
- [ ] Stop All / PANIC work
- [ ] Session browser lists sessions
- [ ] Performance monitoring displays
- [ ] Works on iPad (responsive)
- [ ] Connection status indicator

### Files to Create

- `apps/www/app/occ/remote-control/page.tsx`
- `apps/www/components/occ/RemoteGrid.tsx`
- `apps/www/components/occ/TransportControls.tsx`
- `apps/www/lib/websocket-client.ts`
- `packages/occ-types/websocket-protocol.ts`

### OCC Integration Required

**C++ Side (Future):**

```cpp
// WebSocketServer.h
class WebSocketServer {
public:
    void start(uint16_t port = 8080);
    void broadcast(const juce::String& message);

    std::function<void(const juce::String& message)> onMessage;
};

// MainComponent.cpp
void MainComponent::setupWebSocket() {
    m_webSocketServer = std::make_unique<WebSocketServer>();

    m_webSocketServer->onMessage = [this](const juce::String& msg) {
        auto json = juce::JSON::parse(msg);
        auto type = json["type"].toString();

        if (type == "trigger_clip") {
            int buttonIndex = json["buttonIndex"];
            onClipTriggered(buttonIndex);
        }
    };

    m_webSocketServer->start(8080);
}
```

---

## Module 4: Performance Visualizer

**Priority:** LOW
**Effort:** 2 days
**Dependencies:** Module 3 (WebSocket for real-time data)

### Objective

Real-time performance monitoring dashboard to:

- Visualize CPU usage over time
- Display latency graphs
- Show active clips timeline
- Debug performance issues

### Features

#### 4.1 CPU Usage Graph

- Real-time line chart (last 60 seconds)
- Threshold indicators (30%, 50%, 80%)
- Peak markers
- Average calculation

#### 4.2 Latency Monitor

- Current latency (ms)
- Historical graph
- Target line (<10ms)
- Spike detection

#### 4.3 Active Clips Timeline

- Show which clips are playing over time
- Color-coded by group
- Duration bars
- Overlap visualization

#### 4.4 Memory Usage

- Current memory (MB)
- Trend line
- Leak detection (increasing baseline)

#### 4.5 Buffer Health

- Underrun count
- Underrun events timeline
- Severity indicator

### UI Mockup

```
┌─────────────────────────────────────────────────────────┐
│ Performance Monitor                 ● Live   [Pause]    │
├─────────────────────────────────────────────────────────┤
│ CPU Usage                                Current: 23.5% │
│ ┌───────────────────────────────────────────────────┐   │
│ │ 100% ┌─────────────────────────────────────────┐ │   │
│ │  80% │             ╱╲                           │ │   │
│ │  50% │            ╱  ╲                          │ │   │
│ │  30% │───────────╱────╲──────────────────────  │ │   │
│ │   0% └─────────────────────────────────────────┘ │   │
│ │       0s          30s           60s               │   │
│ └───────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────┤
│ Latency                                 Current: 8.2ms  │
│ ┌───────────────────────────────────────────────────┐   │
│ │  20ms ┌─────────────────────────────────────────┐│   │
│ │  10ms │───────────────────────────────────  ──  ││   │
│ │   5ms │                                         ││   │
│ │   0ms └─────────────────────────────────────────┘│   │
│ └───────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────┤
│ Active Clips                                            │
│ Clip 0: ████████                                        │
│ Clip 1:     ██████████                                  │
│ Clip 2:           ████                                  │
└─────────────────────────────────────────────────────────┘
```

### Acceptance Criteria

- [ ] Real-time CPU graph (60s history)
- [ ] Latency graph with target line
- [ ] Active clips timeline
- [ ] Memory usage tracking
- [ ] Buffer underrun counter
- [ ] Export data as CSV

### Files to Create

- `apps/www/app/occ/performance/page.tsx`
- `apps/www/components/occ/PerformanceMonitor.tsx`
- `apps/www/components/occ/CPUGraph.tsx`
- `apps/www/components/occ/LatencyGraph.tsx`

---

## Module 5: Waveform Renderer

**Priority:** LOW
**Effort:** 3 days
**Dependencies:** Web Audio API

### Objective

Waveform visualization tool to:

- Display audio file waveforms
- Mark trim points (IN/OUT)
- Show cue points
- Export waveform images

### Features

#### 5.1 Waveform Display

- Load audio file (drag-and-drop)
- Render waveform (Web Audio API)
- Zoom in/out (mousewheel)
- Pan (click-and-drag)

#### 5.2 Trim Points

- Set IN point (click + I)
- Set OUT point (click + O)
- Visual markers (green IN, red OUT)
- Playback trimmed region

#### 5.3 Cue Points

- Add cue point (click + C)
- Label cue points
- Color-coded markers
- Jump to cue (click)

#### 5.4 Playback

- Play full file
- Play trimmed region
- Loop trimmed region
- Scrub timeline

#### 5.5 Export

- Export waveform as PNG (for docs/marketing)
- Export trim metadata as JSON
- Copy to clipboard

### UI Mockup

```
┌─────────────────────────────────────────────────────────┐
│ Waveform Renderer                         [Load File]   │
├─────────────────────────────────────────────────────────┤
│ File: intro.wav  |  48kHz  |  Stereo  |  Duration: 5.2s │
├─────────────────────────────────────────────────────────┤
│ ┌───────────────────────────────────────────────────┐   │
│ │ L │█▌▌█▌▌▌█▌▌▌▌▌█▌██▌▌▌▌▌▌▌▌▌█▌▌█▌▌▌▌▌▌▌▌▌▌▌▌█│   │
│ │   │ IN          CUE1          OUT                 │   │
│ │ R │█▌▌█▌▌▌█▌▌▌▌▌█▌██▌▌▌▌▌▌▌▌▌█▌▌█▌▌▌▌▌▌▌▌▌▌▌▌█│   │
│ └───────────────────────────────────────────────────┘   │
│     0s         1s         2s         3s         4s      │
├─────────────────────────────────────────────────────────┤
│ Trim: IN: 0.5s  OUT: 4.2s  Duration: 3.7s              │
│ Cues: [CUE1: 2.1s "Drop"]                              │
├─────────────────────────────────────────────────────────┤
│ [▶ Play] [⏸ Pause] [⟲ Loop]  Zoom: [+] [-] [Fit]       │
└─────────────────────────────────────────────────────────┘
```

### Acceptance Criteria

- [ ] Load audio file (WAV, MP3, FLAC)
- [ ] Render stereo waveform
- [ ] Set IN/OUT trim points
- [ ] Add/edit cue points
- [ ] Playback with trim
- [ ] Export waveform as PNG
- [ ] Export trim data as JSON

### Files to Create

- `apps/www/app/occ/waveform/page.tsx`
- `apps/www/components/occ/WaveformDisplay.tsx`
- `apps/www/lib/audio-utils.ts`

---

## Module 6: Routing Matrix Simulator

**Priority:** LOW
**Effort:** 2 days
**Dependencies:** None

### Objective

Simulate OCC's routing matrix to:

- Test UI layouts before JUCE implementation
- Experiment with gain controls
- Visualize signal flow
- Prototype mute/solo behavior

### Features

#### 6.1 Routing Display

- 4 Clip Groups (Music, SFX, Voice, Backup)
- Master output
- Visual signal flow lines
- Clip assignments per group

#### 6.2 Gain Controls

- Faders for each group (-inf to +12 dB)
- Master fader
- VU meters (simulated)
- Numeric value display

#### 6.3 Mute/Solo

- Mute buttons per group
- Solo buttons per group
- Solo priority (mutes non-solo groups)
- Visual feedback (dimmed when muted)

#### 6.4 Clip Assignment

- Drag clips between groups
- Show clip count per group
- Color-coded assignments

### UI Mockup

```
┌─────────────────────────────────────────────────────────┐
│ Routing Matrix Simulator                                │
├─────────────────────────────────────────────────────────┤
│ ┌──────────┬──────────┬──────────┬──────────┬────────┐ │
│ │  Music   │   SFX    │  Voice   │  Backup  │ Master │ │
│ │  Grp 0   │  Grp 1   │  Grp 2   │  Grp 3   │        │ │
│ ├──────────┼──────────┼──────────┼──────────┼────────┤ │
│ │ [M] [S]  │ [M] [S]  │ [M] [S]  │ [M] [S]  │        │ │
│ │          │          │          │          │        │ │
│ │ ████     │ ██       │ ████     │ █        │ ███    │ │
│ │ ████     │ ██       │ ████     │ █        │ ███    │ │
│ │ ████     │ ██       │ ████     │ █        │ ███    │ │
│ │   │      │   │      │   │      │   │      │   │    │ │
│ │   │      │   │      │   │      │   │      │   │    │ │
│ │   ●      │   ●      │   ●      │   ●      │   ●    │ │
│ │          │          │          │          │        │ │
│ │  0.0 dB  │ -3.0 dB  │ +2.0 dB  │ -6.0 dB  │ 0.0 dB │ │
│ │          │          │          │          │        │ │
│ │ 3 clips  │ 2 clips  │ 1 clip   │ 0 clips  │        │ │
│ └──────────┴──────────┴──────────┴──────────┴────────┘ │
└─────────────────────────────────────────────────────────┘
```

### Acceptance Criteria

- [ ] 4 groups + master displayed
- [ ] Faders control gain (-inf to +12 dB)
- [ ] Mute/solo buttons functional
- [ ] VU meters show levels (simulated)
- [ ] Drag-and-drop clip assignment
- [ ] Visual signal flow

### Files to Create

- `apps/www/app/occ/routing/page.tsx`
- `apps/www/components/occ/RoutingMatrix.tsx`
- `apps/www/components/occ/Fader.tsx`
- `apps/www/components/occ/VUMeter.tsx`

---

## Integration Strategy

### Development Workflow

**Phase 1: Mock Data (Weeks 1-4)**

1. Build all modules with mock data
2. No real OCC connection required
3. Focus on UX and visual design
4. Iterate quickly based on feedback

**Phase 2: Local Integration (Weeks 5-8)**

1. Add WebSocket server to OCC (C++)
2. Connect Shmui to local OCC instance
3. Real-time state updates
4. Bi-directional control

**Phase 3: Production (Months 3-6)**

1. Secure WebSocket (WSS)
2. Authentication (token-based)
3. iOS app deployment (TestFlight)
4. Multi-device support

### OCC WebSocket Integration

**Required C++ Code (OCC Side):**

```cpp
// WebSocketServer.h
#pragma once
#include <juce_core/juce_core.h>
#include <memory>

class WebSocketServer {
public:
    WebSocketServer();
    ~WebSocketServer();

    void start(uint16_t port = 8080);
    void stop();

    void broadcast(const juce::String& message);

    std::function<void(const juce::String& message)> onMessage;
    std::function<void(const juce::String& clientId)> onConnect;
    std::function<void(const juce::String& clientId)> onDisconnect;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// MainComponent.h (additions)
class MainComponent {
private:
    std::unique_ptr<WebSocketServer> m_webSocketServer;

    void setupWebSocket();
    void handleWebSocketMessage(const juce::String& message);
    void broadcastState();
};

// MainComponent.cpp
void MainComponent::setupWebSocket() {
    m_webSocketServer = std::make_unique<WebSocketServer>();

    m_webSocketServer->onMessage = [this](const juce::String& msg) {
        handleWebSocketMessage(msg);
    };

    m_webSocketServer->start(8080);
    DBG("WebSocket server started on port 8080");
}

void MainComponent::handleWebSocketMessage(const juce::String& message) {
    auto json = juce::JSON::parse(message);
    if (!json.isObject()) return;

    auto type = json["type"].toString();

    if (type == "trigger_clip") {
        int buttonIndex = json["buttonIndex"];
        onClipTriggered(buttonIndex);
        broadcastState();  // Update all connected clients
    }
    else if (type == "stop_all") {
        onStopAll();
        broadcastState();
    }
    else if (type == "get_state") {
        broadcastState();
    }
}

void MainComponent::broadcastState() {
    // Create JSON state message
    juce::var state = juce::var(new juce::DynamicObject());
    auto* stateObj = state.getDynamicObject();

    juce::Array<juce::var> clipsArray;
    for (int i = 0; i < m_clipGrid->getButtonCount(); ++i) {
        if (m_sessionManager.hasClip(i)) {
            auto clipData = m_sessionManager.getClip(i);
            auto button = m_clipGrid->getButton(i);

            juce::var clipJson = juce::var(new juce::DynamicObject());
            auto* clipObj = clipJson.getDynamicObject();

            clipObj->setProperty("buttonIndex", i);
            clipObj->setProperty("name", clipData.displayName);
            clipObj->setProperty("state", button ? static_cast<int>(button->getState()) : 0);

            clipsArray.add(clipJson);
        }
    }

    stateObj->setProperty("type", "state_update");
    stateObj->setProperty("clips", clipsArray);
    stateObj->setProperty("sessionName", m_sessionManager.getSessionName());

    m_webSocketServer->broadcast(juce::JSON::toString(state));
}
```

### Shmui Development Setup

**1. Install dependencies:**

```bash
cd packages/shmui
pnpm install
```

**2. Start dev server:**

```bash
pnpm --filter www dev
# Runs on http://localhost:4000
```

**3. Navigate to OCC demos:**

```
http://localhost:4000/occ/session-viewer
http://localhost:4000/occ/clip-grid
http://localhost:4000/occ/remote-control
http://localhost:4000/occ/performance
http://localhost:4000/occ/waveform
http://localhost:4000/occ/routing
```

### Environment Variables

**Shmui (.env.local):**

```env
# OCC WebSocket connection
NEXT_PUBLIC_OCC_WS_URL=ws://localhost:8080

# Feature flags
NEXT_PUBLIC_ENABLE_REMOTE_CONTROL=true
NEXT_PUBLIC_ENABLE_PERFORMANCE_MONITOR=true

# Mock mode (no real OCC connection)
NEXT_PUBLIC_MOCK_MODE=true
```

---

## Success Criteria

### Module 1: Session Viewer

- [ ] Upload and parse .json sessions
- [ ] Display clip grid visually
- [ ] Edit clip metadata
- [ ] Export modified sessions
- [ ] Works offline with mock data

### Module 2: Clip Grid Simulator

- [ ] Multiple layout options (6×8, 10×12, 8×6)
- [ ] Keyboard shortcuts work
- [ ] Tab switching (8 tabs)
- [ ] Theme editor exports CSS
- [ ] Responsive design (iPad)

### Module 3: Remote Control

- [ ] WebSocket connection to OCC
- [ ] Trigger clips remotely
- [ ] Stop All / PANIC buttons
- [ ] Session browser
- [ ] Works on iPad (responsive)

### Module 4: Performance Visualizer

- [ ] Real-time CPU graph
- [ ] Latency graph
- [ ] Active clips timeline
- [ ] Export data as CSV

### Module 5: Waveform Renderer

- [ ] Load and display waveforms
- [ ] Set trim points (IN/OUT)
- [ ] Add cue points
- [ ] Export as PNG

### Module 6: Routing Matrix Simulator

- [ ] 4 groups + master display
- [ ] Gain faders functional
- [ ] Mute/solo behavior
- [ ] Visual signal flow

### Overall Success

- [ ] All 6 modules complete
- [ ] Consistent design language (dark theme, Inter font)
- [ ] Fast iteration (hot reload <1s)
- [ ] iOS companion app foundation
- [ ] Documentation demos ready

---

## Timeline and Effort

### Month 1-2 (Parallel with ORP070 Phase 1)

- **Week 1:** Module 1 (Session Viewer) - 3 days
- **Week 2:** Module 2 (Clip Grid Simulator) - 2 days
- **Week 3-4:** Module 3 (Remote Control) - 4 days

**Deliverable:** Session editing, grid prototyping, remote control foundation

### Month 3-4 (Parallel with ORP070 Phase 2)

- **Week 9-10:** Module 6 (Routing Matrix Simulator) - 2 days
- **Week 11-12:** OCC WebSocket integration (C++ side) - 3 days

**Deliverable:** Routing UI prototype, real-time OCC connection

### Month 5-6 (Parallel with ORP070 Phase 3)

- **Week 17-18:** Module 4 (Performance Visualizer) - 2 days
- **Week 19-20:** Module 5 (Waveform Renderer) - 3 days
- **Week 21:** Polish and documentation - 2 days

**Deliverable:** Performance monitoring, waveform tools, complete demo suite

**Total Effort:** ~3 weeks (spread over 6 months)
**Resources:** 1 frontend developer (part-time)

---

## Dependencies

### External Libraries

**Already in Shmui:**

- Next.js 14
- React 18
- Tailwind CSS
- TypeScript

**New Dependencies:**

```json
{
  "dependencies": {
    "zustand": "^4.4.0", // State management
    "recharts": "^2.9.0", // Charts/graphs
    "react-dropzone": "^14.2.0", // File uploads
    "ws": "^8.14.0" // WebSocket client
  }
}
```

### OCC Dependencies

**Required for Integration:**

- WebSocket server library (C++) - e.g., `websocketpp` or `boost::beast`
- JSON serialization (already have: juce::JSON)
- Threading (already have: JUCE message thread)

**Optional:**

- SSL/TLS for secure WebSocket (production only)
- Authentication tokens (production only)

---

## Risk Mitigation

### Risk 1: WebSocket Integration Complexity

**Probability:** Medium
**Impact:** High (blocks remote control)

**Mitigation:**

- Start with mock data (no integration required)
- Use simple WebSocket library (websocketpp)
- Defer authentication to Month 6
- Have fallback: HTTP polling if WebSocket fails

### Risk 2: Performance on Mobile

**Probability:** Low
**Impact:** Medium (remote control sluggish)

**Mitigation:**

- Target iPad (more powerful than iPhone)
- Optimize React rendering (React.memo, useMemo)
- Throttle WebSocket updates (max 30fps)
- Use Web Workers for heavy computation

### Risk 3: Shmui Development Resource

**Probability:** Medium
**Impact:** Low (nice-to-have, not critical)

**Mitigation:**

- All modules work offline with mock data
- OCC development continues without Shmui
- Can defer remote control to Month 6
- Frontend developer can be part-time

---

## Appendix: File Structure

### Complete File Tree

```
packages/shmui/
├── apps/www/
│   ├── app/
│   │   ├── occ/
│   │   │   ├── page.tsx                       # OCC demos index
│   │   │   ├── session-viewer/
│   │   │   │   └── page.tsx                   # Session viewer
│   │   │   ├── clip-grid/
│   │   │   │   └── page.tsx                   # Clip grid simulator
│   │   │   ├── remote-control/
│   │   │   │   └── page.tsx                   # Remote control
│   │   │   ├── performance/
│   │   │   │   └── page.tsx                   # Performance monitor
│   │   │   ├── waveform/
│   │   │   │   └── page.tsx                   # Waveform renderer
│   │   │   └── routing/
│   │   │       └── page.tsx                   # Routing matrix
│   │   └── layout.tsx
│   ├── components/
│   │   └── occ/
│   │       ├── ClipButton.tsx                 # Single clip button
│   │       ├── ClipGrid.tsx                   # Grid of buttons
│   │       ├── SessionViewer.tsx              # Session file viewer
│   │       ├── TabSwitcher.tsx                # 8-tab switcher
│   │       ├── ThemeEditor.tsx                # Visual theme editor
│   │       ├── RemoteGrid.tsx                 # Remote control grid
│   │       ├── TransportControls.tsx          # Stop All / PANIC
│   │       ├── PerformanceMonitor.tsx         # Performance dashboard
│   │       ├── CPUGraph.tsx                   # CPU graph component
│   │       ├── LatencyGraph.tsx               # Latency graph component
│   │       ├── WaveformDisplay.tsx            # Waveform viewer
│   │       ├── RoutingMatrix.tsx              # Routing UI
│   │       ├── Fader.tsx                      # Gain fader control
│   │       └── VUMeter.tsx                    # VU meter display
│   ├── lib/
│   │   ├── websocket-client.ts                # WebSocket connection
│   │   ├── audio-utils.ts                     # Web Audio API helpers
│   │   └── session-utils.ts                   # Session parsing
│   └── hooks/
│       ├── useWebSocket.ts                    # WebSocket hook
│       ├── usePerformance.ts                  # Performance data hook
│       └── useSession.ts                      # Session state hook
├── packages/
│   └── occ-types/
│       ├── package.json
│       ├── session.ts                         # Session JSON types
│       ├── clip.ts                            # Clip metadata types
│       ├── performance.ts                     # Performance types
│       └── websocket-protocol.ts              # WebSocket message types
└── package.json

Total: ~40 new files
```

---

## References

### OCC Design Documentation

- **OCC021** - Product Vision
- **OCC026** - MVP Definition
- **OCC027** - API Contracts

### Related Plans

- **ORP070** - OCC MVP Sprint (SDK modules)
- **ROADMAP.md** - Milestone M2

### Shmui Documentation

- `packages/shmui/README.md` - Shmui overview
- `/CLAUDE.md` - SDK development guide (mentions Shmui)

---

**Document Status:** Authoritative
**Next Review:** After Module 1 completion
**Maintained By:** OCC Development Team + Frontend Developer
**Last Updated:** October 12, 2025
