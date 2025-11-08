# Component Map Notes

## Overview

This document provides detailed information about each component in the Orpheus SDK, including their responsibilities, public APIs, implementation details, and where to find them in the codebase.

## Core SDK Components

### Session Management

#### SessionGraph
**Location:** `include/orpheus/session_graph.h`, `src/core/session/session_graph.cpp`

**Purpose:** In-memory representation of an audio project

The SessionGraph is the data model for an Orpheus session. It contains all tracks, clips, tempo information, and session metadata.

**Key responsibilities:**
- Maintain collection of tracks with unique IDs
- Store tempo and time signature information
- Validate session integrity (no duplicate IDs, valid references)
- Provide thread-safe read access for audio thread

**Public API:**
```cpp
class SessionGraph {
public:
    void addTrack(Track track);
    void removeTrack(const std::string& id);
    Track* getTrack(const std::string& id);

    TempoInfo getTempo() const;
    void setTempo(float bpm, TimeSignature timeSig);

    bool validate() const;
};
```

**Thread safety:**
- Reads from audio thread (immutable after initialization)
- Writes from UI thread only
- Use atomic updates when changing tempo during playback

**When to modify:**
- Adding new session-level metadata (markers, time signature changes)
- Extending track types (MIDI, automation)
- Adding validation rules

#### Track
**Location:** `include/orpheus/session_graph.h`

**Purpose:** Container for clips with track-level properties

Tracks organize clips into logical groups. Currently supports Audio tracks, with MIDI planned for future releases.

**Key responsibilities:**
- Maintain collection of clips
- Track naming and identification
- Track type specification (Audio, MIDI, Automation)

**Public API:**
```cpp
class Track {
public:
    void addClip(Clip clip);
    void removeClip(const std::string& id);
    Clip* getClip(const std::string& id);

    TrackType getType() const;
    std::string getName() const;
};
```

**When to modify:**
- Adding track-level effects/routing (future)
- Implementing MIDI tracks
- Adding track groups/folders

#### Clip
**Location:** `include/orpheus/session_graph.h`

**Purpose:** Audio clip metadata with trim, fade, and loop settings

A Clip represents a reference to an audio file with playback parameters. All clip settings are persisted to session JSON and survive reload.

**Key responsibilities:**
- Store audio file path and trim points (in samples)
- Define fade IN/OUT parameters (duration, curve type)
- Store gain adjustment (-96dB to +12dB)
- Enable/disable loop mode

**Public API:**
```cpp
class Clip {
public:
    std::string audioFilePath;
    uint64_t trimInSamples;
    uint64_t trimOutSamples;
    float fadeInSeconds;
    float fadeOutSeconds;
    FadeCurve fadeInCurve;
    FadeCurve fadeOutCurve;
    float gainDb;
    bool loopEnabled;
};
```

**Fade curves:**
- `Linear` - Constant slope
- `EqualPower` - Perceptually smooth (-3dB crossfade point)
- `Exponential` - Fast start, slow end

**When to modify:**
- Adding new fade curve types
- Extending clip metadata (color, notes, markers)
- Adding pitch/time-stretch parameters (future)

#### SessionJSON
**Location:** `include/orpheus/session_json.h`, `src/core/session/session_json.cpp`

**Purpose:** Serialize/deserialize SessionGraph to JSON

SessionJSON provides the persistence layer for sessions, using human-readable JSON format that's version-control friendly.

**Key responsibilities:**
- Load session from JSON file into SessionGraph
- Save SessionGraph to JSON file
- Validate JSON schema
- Handle version migration (future)

**Public API:**
```cpp
class SessionJSON {
public:
    static SessionGraph loadSession(const std::string& path);
    static ErrorCode saveSession(const std::string& path, const SessionGraph& session);
    static bool validateSchema(const json& sessionJson);
};
```

**JSON format example:**
```json
{
  "version": "1.0",
  "sessionName": "My Session",
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
          "name": "Bass Loop",
          "audioFilePath": "/path/to/bass.wav",
          "trimInSamples": 0,
          "trimOutSamples": 96000,
          "fadeInSeconds": 0.01,
          "fadeOutSeconds": 0.01,
          "fadeInCurve": "Linear",
          "fadeOutCurve": "EqualPower",
          "gainDb": 0.0,
          "loopEnabled": true
        }
      ]
    }
  ]
}
```

**When to modify:**
- Adding new session metadata fields
- Implementing schema migration for version upgrades
- Adding export formats (XML, etc.)

### Real-Time Transport

#### ITransportController (Interface)
**Location:** `include/orpheus/transport_controller.h`

**Purpose:** Public interface for real-time clip playback

This is the primary interface for controlling audio playback. All transport operations are sample-accurate and thread-safe.

**Key responsibilities:**
- Start/stop clips with sample-accurate timing
- Update clip parameters (gain, trim, loop) in real-time
- Process audio buffers in audio thread callback
- Notify UI of clip events (finished, looped)

**Public API:**
```cpp
class ITransportController {
public:
    virtual ErrorCode startClip(const std::string& clipId) = 0;
    virtual ErrorCode stopClip(const std::string& clipId) = 0;
    virtual ErrorCode stopAllClips() = 0;

    virtual ErrorCode updateClipGain(const std::string& clipId, float gainDb) = 0;
    virtual ErrorCode updateClipTrim(const std::string& clipId,
                                    uint64_t trimIn, uint64_t trimOut) = 0;
    virtual ErrorCode setClipLoopMode(const std::string& clipId, bool enabled) = 0;

    virtual void processAudio(float** outputBuffers, uint32_t frameCount) = 0;

    virtual void setOnClipFinished(std::function<void(std::string)> callback) = 0;
    virtual void setOnClipLooped(std::function<void(std::string)> callback) = 0;
};
```

**Thread safety guarantees:**
- `startClip()`, `stopClip()`, `update*()` methods called from UI thread
- `processAudio()` called from audio thread
- Callbacks invoked on UI thread (never in audio callback)

**When to use:**
- Building applications that need clip playback
- Integrating with DAW hosts
- Creating custom transport controls

#### TransportController (Implementation)
**Location:** `src/core/transport/transport_controller.cpp`

**Purpose:** Concrete implementation of ITransportController

The TransportController manages active clips, applies audio processing (fade, gain), and handles loop restart logic.

**Key responsibilities:**
- Maintain map of active clips (playing clips only)
- Apply fade IN/OUT processing with curve types
- Apply per-clip gain adjustment
- Check loop points and restart playback
- Post events to UI thread via lock-free queue

**Implementation details:**

**Active clip management:**
```cpp
std::map<std::string, ActiveClip> activeClips_;  // Protected by mutex (UI thread only)
```

**Audio thread processing:**
1. Read atomic state from ActiveClip structures (no locks)
2. For each active clip:
   - Read samples from IAudioFileReader
   - Apply fade IN/OUT if within fade region
   - Apply gain adjustment
   - Check if reached trim OUT point
   - If loop enabled and reached end, restart at trim IN
3. Mix all clips into output buffer
4. Post events to queue if clips finished or looped

**Fade processing:**
Fades are computed in real-time using curve equations:
- Linear: `gain = t`
- EqualPower: `gain = sqrt(t)`
- Exponential: `gain = t * t`

Where `t` is normalized time through fade (0.0 to 1.0).

**When to modify:**
- Adding new fade curve types
- Implementing crossfades between clips
- Adding master transport (play/pause/seek)
- Implementing clip groups/buses

#### ActiveClip
**Location:** `src/core/transport/transport_controller.cpp` (internal)

**Purpose:** Lock-free audio thread state for playing clips

ActiveClip is an internal structure used by TransportController to manage real-time clip state. It uses atomic operations for thread-safe updates.

**Key data:**
```cpp
struct ActiveClip {
    std::atomic<uint64_t> currentFrame;      // Current playback position
    std::atomic<float> gainLinear;            // Linear gain (not dB)
    std::atomic<bool> loopEnabled;            // Loop mode flag
    std::shared_ptr<IAudioFileReader> reader; // Immutable after creation
    uint64_t trimInSamples;                   // Start point in source file
    uint64_t trimOutSamples;                  // End point in source file
    FadeState fadeState;                      // Fade IN/OUT parameters
};
```

**Thread safety:**
- `currentFrame` incremented by audio thread only
- `gainLinear`, `loopEnabled` written by UI thread, read by audio thread (atomic)
- `reader`, `trimInSamples`, `trimOutSamples` immutable after creation
- `fadeState` immutable (computed once during start)

**Why it's internal:**
Applications don't interact with ActiveClip directly - it's an implementation detail of TransportController.

### Audio I/O

#### IAudioFileReader (Interface)
**Location:** `include/orpheus/audio_file_reader.h`

**Purpose:** Platform-agnostic audio file reading interface

This interface abstracts file decoding, allowing different implementations (libsndfile, platform codecs, streaming, etc.).

**Public API:**
```cpp
class IAudioFileReader {
public:
    virtual uint64_t read(float** buffers, uint64_t frameCount) = 0;
    virtual ErrorCode seek(uint64_t frame) = 0;

    virtual uint32_t getSampleRate() const = 0;
    virtual uint32_t getChannelCount() const = 0;
    virtual uint64_t getTotalFrames() const = 0;
    virtual std::string getFilePath() const = 0;
};
```

**Key characteristics:**
- `read()` must be audio-thread safe (no allocations, no locks)
- Files pre-loaded into memory for real-time access
- Sample rate currently must be 48kHz (resampling planned v1.0)

**When to implement:**
- Adding platform-specific codecs (Core Audio on macOS, Media Foundation on Windows)
- Implementing streaming (for very large files)
- Adding format support (MP3, AAC, etc.)

#### LibsndfileReader (Implementation)
**Location:** `src/core/audio_io/audio_file_reader.cpp`

**Purpose:** File reader using libsndfile library

Current implementation uses libsndfile for WAV, AIFF, and FLAC decoding.

**Supported formats:**
- WAV (PCM, IEEE Float)
- AIFF (PCM)
- FLAC (lossless compression)

**Limitations:**
- Requires 48kHz sample rate (no resampling yet)
- Files loaded entirely into memory (streaming planned)
- Mono and stereo only (multi-channel planned)

**When to modify:**
- Adding resampling support
- Implementing streaming for large files
- Adding format-specific optimizations

#### IAudioDriver (Interface)
**Location:** `include/orpheus/audio_driver.h`

**Purpose:** Platform-agnostic audio I/O interface

This interface abstracts platform-specific audio APIs, allowing the same SDK code to work with CoreAudio, WASAPI, ASIO, ALSA, etc.

**Public API:**
```cpp
class IAudioDriver {
public:
    virtual ErrorCode initialize(const AudioConfig& config) = 0;
    virtual ErrorCode start() = 0;
    virtual ErrorCode stop() = 0;

    virtual std::vector<Device> getDeviceList() const = 0;
    virtual void setAudioCallback(AudioCallback callback) = 0;

    virtual uint32_t getSampleRate() const = 0;
    virtual uint32_t getBufferSize() const = 0;
};
```

**Callback signature:**
```cpp
using AudioCallback = std::function<void(float** outputBuffers, uint32_t frameCount)>;
```

**When to implement:**
- Adding new platform drivers (WASAPI, ALSA)
- Supporting network audio protocols (AES67)

#### CoreAudioDriver (macOS)
**Location:** `src/platform/coreaudio/coreaudio_driver.cpp`

**Purpose:** macOS native audio driver

Uses CoreAudio Audio Unit framework for low-latency I/O on macOS.

**Features:**
- Device enumeration (built-in, USB, aggregate devices)
- Configurable sample rate and buffer size
- Typical latency: 5-10ms (depending on buffer size)

**Status:** Production-ready, actively used by Clip Composer

#### DummyDriver (Testing)
**Location:** `src/core/audio_io/drivers/dummy_driver.cpp`

**Purpose:** Simulated driver for testing without hardware

The DummyDriver simulates an audio device by calling the callback on a timer thread. Useful for:
- Automated testing in CI (no audio hardware required)
- Offline rendering
- Development on headless servers

**Features:**
- Configurable sample rate and buffer size
- Accurate timing simulation
- No actual audio output

### Routing & DSP

#### IRoutingMatrix (Interface)
**Location:** `include/orpheus/routing_matrix.h`

**Purpose:** Professional N×M audio routing interface

The routing matrix enables flexible audio routing from source channels (clips, inputs) to destination channels (outputs, buses).

**Public API:**
```cpp
class IRoutingMatrix {
public:
    virtual ErrorCode setRouting(uint32_t srcChannel, uint32_t dstChannel, float gain) = 0;
    virtual ErrorCode clearRouting(uint32_t srcChannel, uint32_t dstChannel) = 0;
    virtual ErrorCode setChannelGain(uint32_t channel, float gainDb) = 0;

    virtual void process(float** inputs, float** outputs, uint32_t frameCount) = 0;
    virtual MeterData getChannelMeter(uint32_t channel) const = 0;
};
```

**Status:** Interface complete, implementation planned for OCC v0.3.0-alpha

**Use cases:**
- Multi-channel mixing (64 clips → 16 groups → 32 outputs)
- Bus architecture (Master, Cue, Aux sends/returns)
- Per-clip routing to different outputs
- Real-time metering (Peak, RMS, LUFS)

#### GainSmoother
**Location:** `src/dsp/gain_smoother.cpp`

**Purpose:** Click-free parameter automation

GainSmoother implements anti-click gain ramping for smooth parameter changes in real-time.

**Public API:**
```cpp
class GainSmoother {
public:
    void setTarget(float gainDb, float rampMs);
    void process(float* buffer, uint32_t frameCount);
    bool isRamping() const;
};
```

**Algorithm:**
Linear ramp in decibels (not linear gain) for perceptually smooth transitions.

**Typical ramp times:**
- Fades: 10-50ms
- Gain adjustments: 20-100ms
- Mutes: 5-10ms

**When to use:**
- Implementing routing matrix gain changes
- Automating plugin parameters (future)
- Smooth transport start/stop (future)

## Driver Layer Components

### Contract System

#### Contract
**Location:** `packages/contract/src/contract.ts`

**Purpose:** JSON schema validation for all SDK commands and events

The Contract system ensures type safety and version compatibility between drivers and the SDK.

**Key responsibilities:**
- Validate command JSON before SDK execution
- Validate event JSON before client delivery
- Enforce event frequency limits
- Provide schema documentation

**Available commands:**
- `LoadSession` - Load session from JSON file
- `SaveSession` - Save current session to file
- `RenderClick` - Render click track to WAV
- `SetTransport` - Start/stop/pause transport (planned)
- `TriggerClipGridScene` - Trigger clip grid scene (planned)

**Available events:**
- `SessionChanged` - Session was loaded/modified
- `TransportTick` - Real-time position updates (≤30 Hz)
- `RenderProgress` - Offline render progress (≤10 Hz)
- `Heartbeat` - Connection liveness check (1 Hz)

**Schema location:**
`packages/contract/schemas/v1.0.0-beta/`

**When to modify:**
- Adding new SDK commands
- Defining new event types
- Updating frequency limits
- Schema version bumps

#### AbiVersion
**Location:** `include/orpheus/abi_version.h`

**Purpose:** Semantic versioning for SDK compatibility

AbiVersion enables version negotiation between SDK and drivers, allowing graceful degradation when versions don't match.

**Public API:**
```cpp
class AbiVersion {
public:
    uint32_t major;
    uint32_t minor;
    uint32_t patch;

    bool isCompatible(const AbiVersion& other) const;
    std::string getVersionString() const;
    static AbiVersion fromString(const std::string& version);
};
```

**Compatibility rules:**
- Major version must match exactly (breaking changes)
- Minor version: newer SDK can handle older drivers
- Patch version: fully compatible (bug fixes only)

**When to modify:**
- Bumping SDK version (following semantic versioning)
- Adding compatibility checks for specific versions

### Driver Implementations

#### ClientBroker
**Location:** `packages/client/src/client.ts`

**Purpose:** Unified interface with automatic driver selection

The ClientBroker provides a single API that works with all three driver types, automatically selecting the best available option.

**Driver selection priority:**
1. Native Driver (fastest, in-process)
2. WASM Driver (browser-compatible, no server needed)
3. Service Driver (remote access, fallback)

**Public API:**
```typescript
class ClientBroker {
    async connect(config: ClientConfig): Promise<void>;
    async sendCommand(command: Command): Promise<Response>;
    subscribe(event: string, callback: EventCallback): void;
    async disconnect(): Promise<void>;
}
```

**Handshake protocol:**
1. Client requests capabilities from driver
2. Driver responds with ABI version and available commands
3. Client validates compatibility
4. Connection established, health monitoring begins

**When to use:**
- Building web applications
- Desktop apps (Electron)
- Any JavaScript/TypeScript application

#### NativeDriver
**Location:** `packages/engine-native/src/bindings/session_wrapper.cpp`

**Purpose:** N-API bindings for direct C++ access

The Native Driver provides the lowest-latency integration by calling C++ SDK functions directly from Node.js.

**Architecture:**
- In-process (no IPC overhead)
- Direct memory access to SDK structures
- Synchronous command execution

**Status:** Production-ready

**Use cases:**
- Desktop applications (Electron)
- Server-side rendering
- Maximum performance requirements

#### ServiceDriver
**Location:** `packages/engine-service/src/server.ts`

**Purpose:** HTTP + WebSocket server for remote access

The Service Driver runs the SDK in a separate Node.js process, exposing HTTP endpoints for commands and WebSocket for events.

**Architecture:**
- Spawns `orpheus_minhost` as child process
- HTTP POST for command execution
- WebSocket for event streaming
- Bearer token authentication

**Endpoints:**
- `GET /health` - Health check (public)
- `GET /version` - SDK version (public)
- `GET /contract` - Available commands/events (authenticated)
- `POST /command` - Execute command (authenticated)
- `WS /ws` - Event stream (authenticated)

**Status:** Production-ready

**Use cases:**
- Web applications (no native addon required)
- Microservices architecture
- Remote control scenarios

#### WASMDriver
**Location:** `packages/engine-wasm/src/worker.ts`, `packages/engine-wasm/src/wasm_bindings.cpp`

**Purpose:** Browser-compatible WASM driver

The WASM Driver compiles the SDK to WebAssembly using Emscripten, enabling browser-based DAWs and offline-capable web apps.

**Architecture:**
- Main thread client handles messaging
- Web Worker runs WASM module
- Structured cloning for data transfer
- Subresource Integrity (SRI) verification

**Status:** Infrastructure complete, awaiting Emscripten SDK integration

**Use cases:**
- Browser-based DAWs
- Offline-capable web apps
- No server backend required

## Testing Components

### Test Infrastructure
**Location:** `tests/`

**Unit tests:**
- `abi_smoke.cpp` - ABI version negotiation
- `session_roundtrip.cpp` - JSON serialization
- `transport/` - Transport controller tests (51+ tests)
- `routing/` - Routing matrix tests
- `audio_io/` - Driver and file reader tests

**Integration tests:**
- `multi_clip_stress_test.cpp` - 16 simultaneous clips
- `offline_render_test.cpp` - Determinism validation

**Conformance tools:**
- `tools/conformance/json_roundtrip.cpp` - Full-file comparison
- `tools/cli/inspect_session` - Session inspector

## Component Relationships

### Dependency Flow

```
Applications
    ↓
Adapters / Drivers
    ↓
Core SDK (Transport, Session, Audio I/O)
    ↓
Platform (CoreAudio, WASAPI, libsndfile)
```

### Key Interfaces

**For applications:**
- `ITransportController` - Clip playback control
- `SessionGraph` / `SessionJSON` - Session management
- `IAudioDriver` - Audio device access

**For drivers:**
- `Contract` - Command/event validation
- `AbiVersion` - Version negotiation
- `ClientBroker` - Unified access

**For platform integration:**
- `IAudioDriver` - Platform audio APIs
- `IAudioFileReader` - File decoding

## Where to Look When...

**Adding a new clip feature:**
1. Update `Clip` structure in `session_graph.h`
2. Update `SessionJSON` serialization
3. Update `TransportController` to use new feature
4. Add tests in `tests/transport/`

**Adding a new audio driver:**
1. Implement `IAudioDriver` interface
2. Add to driver factory in `audio_io/drivers/`
3. Add platform-specific CMake target
4. Test with `DummyDriver` first

**Adding a new SDK command:**
1. Define schema in `packages/contract/schemas/`
2. Implement in all three drivers (native, service, WASM)
3. Update `ClientBroker` interface
4. Add integration test

**Debugging audio thread issues:**
1. Check `TransportController::processAudio()` implementation
2. Verify no allocations with AddressSanitizer
3. Use `rt.safety.auditor` skill for static analysis
4. Test with multiple buffer sizes (128, 256, 512, 1024)

## Related Diagrams

- See `data-flow.mermaid.md` for thread communication patterns
- See `architecture-overview.mermaid.md` for layer relationships
- See `entry-points.mermaid.md` for SDK access methods
