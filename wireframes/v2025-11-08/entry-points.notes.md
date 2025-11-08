# Entry Points Notes

## Overview

The Orpheus SDK provides multiple entry points for different user personas and use cases. This document describes all the ways to interact with the SDK, from GUI applications to CLI tools to programmatic APIs.

## User Personas

### 1. End User (Non-Developer)

**Profile:** Broadcast engineer, theater technician, live sound operator

**Use cases:**
- Trigger audio clips during live performance
- Manage session files (save, load, organize)
- Configure audio hardware (device, sample rate, buffer size)
- Set up clip grid with fades, loops, gain

**Primary entry point:** Orpheus Clip Composer (GUI application)

**Requirements:**
- No programming knowledge required
- Graphical interface for all operations
- Visual feedback (waveforms, meters, transport position)

### 2. Application Developer (JavaScript/TypeScript)

**Profile:** Web developer, Electron app developer, Node.js engineer

**Use cases:**
- Build custom soundboard applications
- Integrate audio playback into web apps
- Create automation tools (batch processing, testing)
- Develop remote control interfaces (iOS companion app)

**Primary entry points:**
- `@orpheus/client` - Unified client API (recommended)
- `@orpheus/engine-native` - Direct access (Electron apps)
- `@orpheus/engine-service` - Remote access (web apps)
- `@orpheus/engine-wasm` - Browser-native (offline web apps)

**Requirements:**
- TypeScript/JavaScript knowledge
- npm/pnpm package management
- Basic understanding of async/await

### 3. Plugin Developer (C++)

**Profile:** Audio plugin developer, DAW integrator, embedded systems engineer

**Use cases:**
- Integrate SDK into existing C++ applications
- Build custom adapters for new host environments
- Create REAPER extensions
- Port SDK to embedded platforms (Raspberry Pi, etc.)

**Primary entry points:**
- Direct C++ API (`#include <orpheus/transport_controller.h>`)
- REAPER adapter (when stabilized)

**Requirements:**
- C++20 compiler
- CMake build system knowledge
- Understanding of audio thread safety

### 4. DevOps/QA Engineer

**Profile:** Test automation engineer, CI/CD maintainer, build engineer

**Use cases:**
- Automate session validation
- Batch render click tracks
- Verify cross-platform determinism
- Generate test fixtures

**Primary entry points:**
- `orpheus_minhost` - CLI for offline rendering
- `inspect_session` - Session validation tool
- `json_roundtrip` - Conformance testing

**Requirements:**
- Shell scripting knowledge
- Understanding of JSON format
- CI/CD pipeline experience

## GUI Applications

### Orpheus Clip Composer

**Location:** `apps/clip-composer/`

**Status:** v0.2.0-alpha (active development)

**Platform support:**
- macOS (primary development platform)
- Windows (planned v0.3.0)
- Linux (planned v0.3.0)

**Installation:**
```bash
# Development build
cd apps/clip-composer
./scripts/relaunch-occ.sh

# Release build (future)
# Download DMG/EXE/AppImage from releases page
```

**Usage:**

1. **Launch application**
   - Double-click app icon (macOS: Orpheus Clip Composer.app)
   - Or run from terminal: `./OrpheusClipComposer`

2. **Configure audio device**
   - Menu: Settings → Audio Settings
   - Select device, sample rate (48kHz), buffer size (256-512)
   - Click "Apply"

3. **Load session**
   - Menu: File → Open Session
   - Select `.json` file
   - Clips appear in grid

4. **Trigger clips**
   - Click button to start playback
   - Click again to stop
   - Use keyboard shortcuts (1-9, Q-Y, A-J, Z-,)

5. **Edit clip settings**
   - Right-click clip → Edit
   - Adjust trim points, fades, gain, loop
   - Preview changes before applying

**Features:**
- 48-button clip grid × 8 tabs = 384 total clips
- Loop playback with sample-accurate restart
- Fade IN/OUT with 3 curve types (Linear, EqualPower, Exponential)
- Waveform display with zoom/pan
- Audio device configuration dialog
- Session save/load with metadata persistence

**Configuration files:**
- Session files: User-specified location (`.json` format)
- Preferences: Platform-specific (macOS: `~/Library/Application Support/Orpheus/`)

**Documentation:**
- User guide: `apps/clip-composer/docs/OCC/USER_GUIDE.md` (future)
- Release notes: `apps/clip-composer/CHANGELOG.md`

### JUCE Demo Host

**Location:** `apps/juce-demo-host/`

**Status:** Development demo

**Purpose:** Demonstrate JUCE integration patterns

**Features:**
- Basic clip playback
- Minimal UI (for testing only)
- Example code for JUCE developers

**Usage:**
```bash
# Build
cmake -S . -B build -DORPHEUS_ENABLE_APP_JUCE_DEMO_HOST=ON
cmake --build build

# Run
./build/apps/juce-demo-host/JuceDemoHost
```

**Not for end users** - This is a developer reference only.

### Custom Applications

**Integration options:**

**Option 1: Direct C++ linking** (lowest latency)
```cmake
# CMakeLists.txt
find_package(Orpheus REQUIRED)
target_link_libraries(your_app
    orpheus::core
    orpheus::transport
    orpheus::audio_io
)
```

**Option 2: JavaScript drivers** (cross-platform, web-compatible)
```typescript
// package.json
{
  "dependencies": {
    "@orpheus/client": "^1.0.0"
  }
}

// app.ts
import { OrpheusClient } from '@orpheus/client';
const client = new OrpheusClient();
await client.connect();
await client.loadSession('session.json');
```

**Option 3: REAPER adapter** (DAW integration)
```cpp
// reaper_plugin.cpp
#include <orpheus/reaper_adapter.h>
// Use REAPER extension API
```

**When to use each:**
- **Direct C++**: Desktop apps with C++ codebase (JUCE, Qt, wxWidgets)
- **JavaScript drivers**: Web apps, Electron, Node.js services
- **REAPER adapter**: REAPER plugins, custom actions

## Command-Line Tools

### Minhost

**Location:** `adapters/minhost/`

**Binary:** `build/adapters/minhost/orpheus_minhost`

**Purpose:** CLI interface for testing and automation

**Commands:**

**1. Load and inspect session:**
```bash
./orpheus_minhost --session tools/fixtures/test_session.json
# Output:
# Session loaded: test_session
# Tracks: 2
# Clips: 8
# Tempo: 120.0 BPM
```

**2. Render click track:**
```bash
./orpheus_minhost \
    --session tools/fixtures/solo_click.json \
    --render click.wav \
    --bars 4 \
    --bpm 120
# Output:
# Rendering 4 bars at 120 BPM...
# Output: click.wav (384000 samples, 8 seconds @ 48kHz)
```

**3. Simulate transport (for testing):**
```bash
./orpheus_minhost --session session.json --transport
# Output (to stdout, 1 line per beat):
# [TICK] Bar 1 Beat 1
# [TICK] Bar 1 Beat 2
# [TICK] Bar 1 Beat 3
# [TICK] Bar 1 Beat 4
# [TICK] Bar 2 Beat 1
# ...
```

**4. Offline render (future):**
```bash
./orpheus_minhost \
    --session session.json \
    --render-all output.wav \
    --start 0.0 \
    --end 60.0
# Render entire session to single WAV file
```

**Exit codes:**
- `0` - Success
- `1` - Invalid arguments
- `2` - Session load failed
- `3` - Render failed
- `4` - Audio driver error

**Use cases:**
- CI/CD testing (validate sessions, render click tracks)
- Batch processing (render multiple sessions)
- Headless servers (no GUI required)
- Automation scripts

### Session Inspector

**Location:** `tools/cli/inspect_session/`

**Binary:** `build/tools/cli/inspect_session`

**Purpose:** Validate and analyze session files

**Commands:**

**1. Show session summary:**
```bash
./inspect_session --file session.json
# Output:
# Session: My Session
# Version: 1.0
# Tempo: 120.0 BPM (4/4)
# Tracks: 4
#   Track 1: "Audio Track 1" (audio) - 12 clips
#   Track 2: "Audio Track 2" (audio) - 8 clips
#   Track 3: "Audio Track 3" (audio) - 6 clips
#   Track 4: "Audio Track 4" (audio) - 10 clips
# Total clips: 36
```

**2. Validate schema:**
```bash
./inspect_session --file session.json --validate
# Output:
# Validating against schema v1.0.0-beta...
# ✓ Schema valid
# ✓ All audio files found
# ✓ No duplicate clip IDs
# ✓ All trim points within file bounds
# Validation: PASSED
```

**3. Show statistics:**
```bash
./inspect_session --file session.json --stats
# Output:
# Session statistics:
#   Total duration: 3600 seconds (1 hour)
#   Total audio data: 2.4 GB
#   Clips with loops: 12 (33%)
#   Clips with fades: 28 (78%)
#   Average clip gain: -3.2 dB
#   Most used fade curve: EqualPower (64%)
```

**Use cases:**
- Pre-flight checks (validate session before performance)
- Troubleshooting (find missing files, invalid IDs)
- Session analytics (understand usage patterns)

### JSON Round-Trip Tool

**Location:** `tools/conformance/json_roundtrip/`

**Binary:** `build/tools/conformance/json_roundtrip`

**Purpose:** Verify JSON serialization is lossless

**Commands:**

**1. Round-trip test:**
```bash
./json_roundtrip --input session.json
# Output:
# Loading session.json...
# Re-serializing to temp file...
# Comparing files byte-by-byte...
# ✓ Files identical
# Round-trip: PASSED
```

**2. Compare specific files:**
```bash
./json_roundtrip --input original.json --compare modified.json
# Output:
# Comparing original.json and modified.json...
# Differences found:
#   tracks[0].clips[2].gainDb: -6.0 → -3.0
#   tracks[1].clips[5].loopEnabled: true → false
# Files differ in 2 fields
```

**Use cases:**
- Conformance testing (ensure SDK doesn't corrupt sessions)
- Version migration validation (old format → new format → old format)
- Debugging serialization issues

## JavaScript/TypeScript Drivers

### Client Broker (`@orpheus/client`)

**Purpose:** Unified API with automatic driver selection

**Installation:**
```bash
npm install @orpheus/client
# or
pnpm add @orpheus/client
```

**Usage:**
```typescript
import { OrpheusClient } from '@orpheus/client';

// Create client (auto-selects best driver)
const client = new OrpheusClient();

// Connect with config
await client.connect({
    preferredDriver: 'native',  // Optional: native|service|wasm
    servicePort: 8080,           // For service driver
    logLevel: 'info'             // error|warn|info|debug
});

// Check capabilities
const capabilities = await client.getCapabilities();
console.log('Available commands:', capabilities.commands);

// Execute commands
const result = await client.sendCommand({
    command: 'LoadSession',
    params: { sessionPath: '/path/to/session.json' }
});

// Subscribe to events
client.subscribe('SessionChanged', (event) => {
    console.log('Session changed:', event.data);
});

// Disconnect when done
await client.disconnect();
```

**Driver selection logic:**
1. Check if native driver available (Node.js with native addon)
2. Check if WASM driver available (browser with WebAssembly support)
3. Fall back to service driver (HTTP + WebSocket)

**When to use:**
- **Always use this** unless you have specific driver requirements
- Provides consistent API across all drivers
- Handles version negotiation and health monitoring

### Native Driver (`@orpheus/engine-native`)

**Purpose:** Direct C++ access via N-API bindings

**Installation:**
```bash
npm install @orpheus/engine-native
```

**Usage:**
```typescript
import { NativeEngine } from '@orpheus/engine-native';

const engine = new NativeEngine();

// Direct function calls (no IPC)
await engine.loadSession('/path/to/session.json');

const info = engine.getSessionInfo();
console.log('Tracks:', info.trackCount);
console.log('Clips:', info.clipCount);

// Render click track
await engine.renderClick({
    outputPath: 'click.wav',
    bars: 4,
    bpm: 120,
    timeSignature: '4/4'
});

// Subscribe to events (callback invoked directly)
engine.subscribe('ClipFinished', (clipId) => {
    console.log('Clip finished:', clipId);
});
```

**Characteristics:**
- **Fastest** - Direct C++ function calls, no serialization
- **In-process** - Runs in same process as Node.js
- **Platform-specific** - Requires native build for each platform
- **Node.js only** - Cannot run in browser

**When to use:**
- Desktop applications (Electron)
- Server-side rendering (maximum performance)
- Low-latency requirements (<5ms command latency)

### Service Driver (`@orpheus/engine-service`)

**Purpose:** HTTP server with WebSocket events

**Installation:**
```bash
npm install @orpheus/engine-service
```

**Usage:**

**Server side:**
```typescript
import { ServiceEngine } from '@orpheus/engine-service';

const engine = new ServiceEngine({
    port: 8080,
    host: '127.0.0.1',  // Localhost only (security)
    authToken: 'your-secret-token'
});

await engine.start();
console.log('Service running on http://127.0.0.1:8080');
```

**Client side:**
```typescript
import { ServiceClient } from '@orpheus/engine-service/client';

const client = new ServiceClient({
    baseURL: 'http://127.0.0.1:8080',
    authToken: 'your-secret-token'
});

await client.connect();

// Commands via HTTP POST
const result = await client.post('/command', {
    command: 'LoadSession',
    params: { sessionPath: '/path/to/session.json' }
});

// Events via WebSocket
const ws = await client.connectWebSocket();
ws.on('message', (event) => {
    console.log('Event:', event);
});
```

**Endpoints:**
- `GET /health` - Health check (public)
- `GET /version` - SDK version (public)
- `GET /contract` - Available commands/events (authenticated)
- `POST /command` - Execute command (authenticated)
- `WS /ws` - WebSocket event stream (authenticated)

**Characteristics:**
- **Remote access** - Can run on different machine
- **Web-compatible** - Works with any HTTP client
- **Multi-process** - Spawns `orpheus_minhost` as child process
- **Language-agnostic** - Any language can call HTTP endpoints

**When to use:**
- Web applications (browser-based UI)
- Microservices architecture (audio service)
- Remote control scenarios (iOS companion app)
- Cross-language integration (Python, Ruby, etc.)

### WASM Driver (`@orpheus/engine-wasm`)

**Purpose:** Browser-native audio engine

**Installation:**
```bash
npm install @orpheus/engine-wasm
```

**Usage:**
```typescript
import { WASMEngine } from '@orpheus/engine-wasm';

const engine = new WASMEngine();

// Initialize WASM module (loads .wasm file)
await engine.initialize({
    wasmURL: '/orpheus.wasm',  // Emscripten-compiled SDK
    wasmSRI: 'sha384-...'       // Subresource Integrity hash
});

// Commands executed in Web Worker
await engine.loadSession('/path/to/session.json');

// Events posted from worker
engine.on('SessionChanged', (event) => {
    console.log('Session changed:', event);
});
```

**Characteristics:**
- **Browser-compatible** - No native addon required
- **Offline-capable** - No server needed (all runs in browser)
- **Isolated** - Runs in Web Worker (doesn't block main thread)
- **Secure** - Subresource Integrity verification

**Status:** Infrastructure complete, awaiting Emscripten SDK compilation

**When to use:**
- Browser-based DAWs (offline editing)
- Web apps without server backend
- Progressive Web Apps (PWA)
- Offline audio processing

## Native C++ API

### Direct C++ Linking

**Purpose:** Lowest-level SDK access

**Usage:**

**1. Add to CMakeLists.txt:**
```cmake
find_package(Orpheus REQUIRED)

add_executable(your_app main.cpp)

target_link_libraries(your_app
    PRIVATE
        orpheus::core
        orpheus::transport
        orpheus::audio_io
)
```

**2. Include headers:**
```cpp
#include <orpheus/session_graph.h>
#include <orpheus/transport_controller.h>
#include <orpheus/audio_driver.h>
#include <orpheus/audio_file_reader.h>
```

**3. Use SDK:**
```cpp
// Load session
auto session = orpheus::core::SessionJSON::loadSession("session.json");

// Create audio driver
auto driver = orpheus::platform::createCoreAudioDriver();
driver->initialize({ .sampleRate = 48000, .bufferSize = 256 });

// Create transport controller
orpheus::core::TransportController transport(&session, driver.get());

// Set up callbacks
transport.setOnClipFinished([](const std::string& clipId) {
    std::cout << "Clip finished: " << clipId << "\n";
});

// Start playback
auto result = transport.startClip("clip-1");
if (result != orpheus::ErrorCode::OK) {
    std::cerr << "Failed to start clip\n";
}

// Start audio driver
driver->start();

// ... app runs, audio plays ...

// Clean shutdown
driver->stop();
```

**When to use:**
- C++ applications with existing codebase
- Maximum performance requirements
- Custom audio processing pipelines
- Embedded systems (Raspberry Pi, etc.)

### REAPER Adapter

**Location:** `adapters/reaper/`

**Status:** Quarantined (pending SDK stabilization)

**Purpose:** Integrate SDK into REAPER DAW

**Installation (when available):**
```bash
# Build extension
cmake -S . -B build -DORPHEUS_ENABLE_ADAPTER_REAPER=ON
cmake --build build

# Install
cp build/adapters/reaper/reaper_orpheus.dylib \
   ~/Library/Application\ Support/REAPER/UserPlugins/
```

**Usage:**
1. Launch REAPER
2. Extensions menu → Orpheus → Load Session
3. Select session JSON file
4. Clips appear as REAPER items on tracks

**Features (planned):**
- Bidirectional sync (REAPER ↔ Orpheus session)
- Clip triggering from REAPER transport
- Export REAPER project to Orpheus session

**Timeline:**
- Reactivation planned for v0.4.0-alpha (after SDK stabilization)

## Configuration & Environment

### Session JSON Files

**Format:** Human-readable JSON

**Location:** User-specified (via File → Open dialog or CLI argument)

**Schema:** `packages/contract/schemas/v1.0.0-beta/session.schema.json`

**Example:**
```json
{
  "version": "1.0",
  "sessionName": "My Performance",
  "tempo": {
    "bpm": 120.0,
    "timeSignature": "4/4"
  },
  "tracks": [
    {
      "id": "track-1",
      "name": "Audio Track 1",
      "type": "audio",
      "clips": [
        {
          "id": "clip-1",
          "name": "Intro Music",
          "audioFilePath": "/path/to/intro.wav",
          "trimInSamples": 0,
          "trimOutSamples": 96000,
          "fadeInSeconds": 0.01,
          "fadeOutSeconds": 0.01,
          "fadeInCurve": "EqualPower",
          "fadeOutCurve": "EqualPower",
          "gainDb": 0.0,
          "loopEnabled": false
        }
      ]
    }
  ]
}
```

**Best practices:**
- Use absolute paths for audio files (or relative to session file)
- Keep session files in version control (Git-friendly format)
- Validate with `inspect_session --validate` before deployment

### Environment Variables

**Available variables:**

**`ORPHEUS_LOG_LEVEL`**
- Values: `ERROR`, `WARN`, `INFO`, `DEBUG`
- Default: `INFO`
- Usage: Control logging verbosity

```bash
export ORPHEUS_LOG_LEVEL=DEBUG
./orpheus_minhost --session session.json
```

**`ORPHEUS_DRIVER_TYPE`**
- Values: `native`, `service`, `wasm`
- Default: Auto-select best available
- Usage: Force specific driver type

```bash
export ORPHEUS_DRIVER_TYPE=native
node your_app.js
```

**`ORPHEUS_SERVICE_PORT`**
- Values: Port number (1-65535)
- Default: `8080`
- Usage: Service driver HTTP port

```bash
export ORPHEUS_SERVICE_PORT=9000
./orpheus_minhost --service
```

**`ORPHEUS_AUDIO_DRIVER`**
- Values: `coreaudio`, `wasapi`, `asio`, `alsa`, `dummy`
- Default: Platform default (CoreAudio on macOS)
- Usage: Override audio driver selection

```bash
export ORPHEUS_AUDIO_DRIVER=dummy
./orpheus_minhost --session session.json
```

### Build Configuration (CMake)

**Available options:**

**`CMAKE_BUILD_TYPE`**
- Values: `Debug`, `Release`, `RelWithDebInfo`
- Default: `Debug`
- Usage: Build type (affects optimizations, sanitizers)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

**`ORPHEUS_ENABLE_APP_CLIP_COMPOSER`**
- Values: `ON`, `OFF`
- Default: `OFF`
- Usage: Build Clip Composer application

```bash
cmake -S . -B build -DORPHEUS_ENABLE_APP_CLIP_COMPOSER=ON
```

**`ORPHEUS_ENABLE_REALTIME`**
- Values: `ON`, `OFF`
- Default: `ON`
- Usage: Enable real-time audio drivers (disable for offline-only)

```bash
cmake -S . -B build -DORPHEUS_ENABLE_REALTIME=OFF
```

**`ORPHEUS_ENABLE_ADAPTER_REAPER`**
- Values: `ON`, `OFF`
- Default: `OFF`
- Usage: Build REAPER adapter (quarantined, not recommended)

```bash
cmake -S . -B build -DORPHEUS_ENABLE_ADAPTER_REAPER=ON
```

**`ORPHEUS_ENABLE_TESTS`**
- Values: `ON`, `OFF`
- Default: `ON`
- Usage: Build test suite

```bash
cmake -S . -B build -DORPHEUS_ENABLE_TESTS=OFF
```

## Quick Start Guides

### For End Users

1. Download Orpheus Clip Composer from releases page
2. Install application (DMG on macOS, EXE on Windows)
3. Launch application
4. Settings → Audio Settings → Select device
5. File → Open Session → Select `.json` file
6. Click buttons to trigger clips

### For Application Developers (JavaScript)

1. Install client package:
   ```bash
   npm install @orpheus/client
   ```

2. Use in your app:
   ```typescript
   import { OrpheusClient } from '@orpheus/client';
   const client = new OrpheusClient();
   await client.connect();
   await client.loadSession('session.json');
   ```

3. See `packages/client/examples/` for full examples

### For Plugin Developers (C++)

1. Build SDK:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```

2. Link to your app:
   ```cmake
   target_link_libraries(your_app orpheus::core orpheus::transport)
   ```

3. Use API:
   ```cpp
   #include <orpheus/transport_controller.h>
   ```

4. See `examples/` for full examples

### For DevOps Engineers

1. Build CLI tools:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```

2. Use in CI:
   ```bash
   ./build/adapters/minhost/orpheus_minhost \
       --session session.json \
       --render output.wav
   ```

3. Validate sessions:
   ```bash
   ./build/tools/cli/inspect_session \
       --file session.json \
       --validate
   ```

## Related Documentation

**For end users:**
- Clip Composer user guide (future): `apps/clip-composer/docs/USER_GUIDE.md`
- Tutorial videos (future): TBD

**For developers:**
- Integration guide: `docs/integration/TRANSPORT_INTEGRATION.md`
- API reference: `docs/api/` (Doxygen-generated)
- Contract specification: `packages/contract/README.md`

**For contributors:**
- Development guide: `CLAUDE.md`
- Contribution guidelines: `docs/CONTRIBUTING.md`
- Architecture overview: `ARCHITECTURE.md`

## Related Diagrams

- See `architecture-overview.mermaid.md` for system layers
- See `component-map.mermaid.md` for component relationships
- See `data-flow.mermaid.md` for thread communication patterns
