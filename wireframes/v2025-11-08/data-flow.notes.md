# Data Flow Notes

## Overview

The Orpheus SDK uses a strict **two-thread model** with lock-free communication patterns to ensure real-time safety and broadcast reliability. This document explains how data flows between threads, what operations are allowed on each thread, and how the system maintains sample-accurate timing without glitches.

## Threading Model

### UI Thread (Non-Real-Time)

**Purpose:** Handle user interaction and file I/O

The UI thread is where all "slow" operations happen. It's responsible for:
- Processing user input (button clicks, keyboard shortcuts)
- Loading audio files from disk
- Parsing and saving session JSON
- Allocating memory for new clips
- Sending commands to the transport controller

**Allowed operations:**
- ✅ Memory allocation (`new`, `malloc`, `std::vector::push_back`)
- ✅ File I/O (`fread()`, `fwrite()`, `fopen()`)
- ✅ Network calls (HTTP, WebSocket - for optional features only)
- ✅ Lock acquisition (`std::mutex::lock()`)
- ✅ Blocking operations (waiting for I/O)

**Forbidden operations:**
- ❌ Direct audio buffer manipulation (use TransportController instead)

**Thread safety:**
The UI thread must use the TransportController's public API (which is thread-safe) to communicate with the audio thread. Never access audio thread state directly.

### Audio Thread (Real-Time)

**Purpose:** Process audio buffers with strict timing requirements

The audio thread is called by the audio driver at regular intervals (e.g., every 256 frames at 48kHz = ~5.3ms). This thread must:
- Fill the output buffer with audio samples
- Never miss a deadline (buffer underrun = audio glitch)
- Maintain sample-accurate timing
- Run deterministically (same input → same output)

**Allowed operations:**
- ✅ Read atomic values (clip state)
- ✅ Lock-free queue operations
- ✅ CPU-bound calculations (fade curves, gain conversion)
- ✅ Pre-allocated buffer access

**Forbidden operations (Broadcast-Safe Contract):**
- ❌ Memory allocation (`new`, `malloc`, `std::vector::push_back`)
- ❌ Locks (`std::mutex::lock()`, `std::condition_variable::wait()`)
- ❌ File I/O (`fread()`, `fwrite()`, `fopen()`)
- ❌ Network I/O (HTTP, WebSocket, TCP/UDP)
- ❌ System calls (blocking operations)

**Why these restrictions?**
Any of these operations could take unpredictable time, causing the audio thread to miss its deadline and produce a glitch (click, pop, or silence).

## Data Flow Patterns

### 1. Initialization Phase

**Sequence:**
1. User loads a session file
2. UI thread parses JSON into `SessionGraph`
3. UI thread creates `TransportController`
4. Transport controller initializes audio driver
5. Driver starts calling audio thread callback

**Key points:**
- Session parsing happens on UI thread (blocking OK)
- Audio files are NOT loaded during initialization
- Files loaded on-demand when clips are first triggered
- Driver initialization may enumerate devices (blocking OK)

### 2. Start Clip Flow (UI → Audio Thread)

**Sequence:**
1. User clicks play button
2. UI thread calls `TransportController::startClip(clipId)`
3. Transport controller:
   - Looks up clip in session graph
   - Loads audio file if not already loaded (UI thread, blocking OK)
   - Creates `ActiveClip` structure with atomic state
   - Adds to `activeClips` map (protected by mutex, UI thread only)
4. Next audio callback:
   - Audio thread reads atomic state from `ActiveClip`
   - Starts rendering audio into output buffer

**Thread handoff:**
```
UI Thread                          Audio Thread
┌─────────────┐                   ┌─────────────┐
│startClip()  │                   │             │
│             │                   │             │
│Create       │                   │             │
│ActiveClip   │                   │             │
│             │                   │             │
│Set atomic   │──────────────────>│Read atomic  │
│state        │    (no lock)      │state        │
│             │                   │             │
│Add to map   │                   │processAudio()│
│(mutex)      │                   │             │
│             │                   │             │
└─────────────┘                   └─────────────┘
```

**Key points:**
- File loading happens on UI thread (before audio thread involvement)
- `activeClips` map modified only on UI thread (mutex protection OK)
- Audio thread reads state via atomics (no mutex needed)
- Audio thread never allocates or deallocates clips

### 3. Real-Time Audio Processing (Audio Thread Loop)

**Sequence:**
1. Audio driver calls `processAudio(buffer, frameCount)`
2. For each active clip:
   a. Read atomic state (`currentFrame`, `gainLinear`, `loopEnabled`)
   b. Read audio samples from pre-loaded file
   c. Apply fade IN if within fade region
   d. Apply clip gain
   e. Check if reached trim OUT point
   f. If loop enabled and at end, restart at trim IN
   g. If reached end without loop, queue "ClipFinished" event
   h. Increment `currentFrame` (atomic)
3. Mix all clips into output buffer
4. Return from callback (must complete within deadline)

**Typical timing:**
- Buffer size: 256 frames at 48kHz = 5.33ms deadline
- Processing time: <1ms for 8 clips (target <20% CPU)
- Remaining time: ~4ms safety margin

**Fade processing:**
Fades are computed in real-time using curve equations:

```cpp
// Linear fade
float t = (currentFrame - fadeStartFrame) / fadeLengthFrames;
float fadeGain = t;

// Equal power fade (perceptually smooth)
float fadeGain = std::sqrt(t);

// Exponential fade
float fadeGain = t * t;
```

**Loop restart:**
```cpp
if (currentFrame >= trimOutSamples) {
    if (loopEnabled) {
        currentFrame = trimInSamples;  // Atomic write
        queueEvent(ClipLooped, clipId);
    } else {
        removeFromActive(clipId);
        queueEvent(ClipFinished, clipId);
    }
}
```

**Key points:**
- No allocations - all buffers pre-allocated
- No locks - atomic operations only
- No file I/O - data already in memory
- Deterministic - same input always produces same output

### 4. Event Notification (Audio → UI Thread)

**Sequence:**
1. Audio thread queues event (`ClipFinished`, `ClipLooped`)
2. UI thread polls event queue on timer (e.g., 10Hz)
3. UI thread invokes registered callbacks
4. UI updates visual state (button colors, waveform position)

**Lock-free queue:**
```
Audio Thread                 UI Thread
┌──────────┐                ┌──────────┐
│Clip      │                │          │
│finished  │                │          │
│          │                │          │
│Queue     │───────────────>│Poll      │
│event     │  (lock-free)   │queue     │
│          │                │          │
│Continue  │                │Invoke    │
│processing│                │callback  │
│          │                │          │
└──────────┘                └──────────┘
```

**Why lock-free?**
If the UI thread held a mutex while processing events, and the audio thread tried to acquire the same mutex, the audio thread could be blocked (priority inversion) causing audio glitches.

**Key points:**
- Events posted from audio thread (fast, non-blocking)
- Events consumed on UI thread (may take variable time)
- No feedback path (UI can't respond immediately to audio thread)
- Callbacks invoked on UI thread (safe to allocate, do I/O, etc.)

### 5. Stop Clip Flow (UI Thread)

**Sequence:**
1. User clicks stop button
2. UI thread calls `TransportController::stopClip(clipId)`
3. Transport controller:
   - Removes clip from `activeClips` map (mutex protection)
   - Audio thread sees clip is no longer active on next callback
4. UI updates button state

**Graceful stop:**
```cpp
// Option 1: Immediate stop (current behavior)
activeClips.erase(clipId);  // Mutex protected

// Option 2: Fade out (future enhancement)
activeClip.fadeOutRemaining = fadeOutSamples;  // Atomic write
// Audio thread fades out, then removes
```

**Key points:**
- Removal happens on UI thread (mutex OK)
- Audio thread sees removal on next callback (atomic map access)
- No audio thread notification needed (audio thread polls state)

### 6. Update Gain Flow (UI → Audio Thread, Glitch-Free)

**Sequence:**
1. User adjusts gain slider
2. UI thread calls `TransportController::updateClipGain(clipId, gainDb)`
3. Transport controller:
   - Converts dB to linear gain: `gainLinear = pow(10, gainDb / 20)`
   - Writes to `ActiveClip::gainLinear` (atomic)
4. Next audio callback:
   - Audio thread reads new gain value (atomic)
   - Applies new gain to samples

**Why no glitch?**
The gain change is sample-accurate but not smoothed. For click-free gain changes, use `GainSmoother`:

```cpp
// Without smoothing (may click on large changes)
buffer[i] *= gainLinear;

// With smoothing (click-free)
gainSmoother.setTarget(gainDb, rampMs);
gainSmoother.process(buffer, frameCount);
```

**Key points:**
- Gain changes are atomic (no tearing)
- Audio thread sees change within one buffer period (~5ms)
- Large gain changes may click without smoothing
- Future enhancement: automatic gain smoothing for all parameters

### 7. Session Save Flow (UI Thread Only)

**Sequence:**
1. User saves session
2. UI thread calls `SessionJSON::saveSession(path, session)`
3. Session graph serialized to JSON
4. Clip metadata written (trim, fade, gain, loop)
5. File written to disk

**JSON format:**
```json
{
  "version": "1.0",
  "sessionName": "My Session",
  "tempo": { "bpm": 120.0, "timeSignature": "4/4" },
  "tracks": [
    {
      "id": "track-1",
      "clips": [
        {
          "id": "clip-1",
          "audioFilePath": "/path/to/audio.wav",
          "trimInSamples": 0,
          "trimOutSamples": 96000,
          "fadeInSeconds": 0.01,
          "fadeOutSeconds": 0.01,
          "fadeInCurve": "EqualPower",
          "fadeOutCurve": "EqualPower",
          "gainDb": -6.0,
          "loopEnabled": true
        }
      ]
    }
  ]
}
```

**Key points:**
- Session save never blocks audio thread
- All clip metadata persisted (settings survive reload)
- Human-readable format for version control
- Schema validation before save

## Communication Patterns

### Pattern 1: Atomic State (UI → Audio)

**Use case:** Update clip parameters in real-time

**Implementation:**
```cpp
// UI thread
std::atomic<float> gainLinear;
gainLinear.store(newGain, std::memory_order_release);

// Audio thread
float currentGain = gainLinear.load(std::memory_order_acquire);
```

**Guarantees:**
- No tearing (reads/writes are atomic)
- Memory ordering (writes visible to audio thread)
- Lock-free (no blocking)

**Examples:**
- Clip gain adjustment
- Loop enable/disable
- Transport play/pause state (future)

### Pattern 2: Lock-Free Queue (Audio → UI)

**Use case:** Notify UI of audio thread events

**Implementation:**
```cpp
// Audio thread
eventQueue.enqueue(Event{ClipFinished, clipId});

// UI thread (timer callback)
while (eventQueue.dequeue(event)) {
    handleEvent(event);
}
```

**Guarantees:**
- Non-blocking enqueue (audio thread never waits)
- Multiple events can be queued
- FIFO ordering preserved

**Examples:**
- Clip finished notification
- Clip looped notification
- Buffer underrun warning (future)
- CPU overload warning (future)

### Pattern 3: Immutable Shared Data

**Use case:** Share audio file data between threads

**Implementation:**
```cpp
// UI thread (load file)
auto reader = std::make_shared<AudioFileReader>(path);
// File loaded into memory

// Transfer to audio thread
activeClip.reader = reader;  // shared_ptr (atomic refcount)

// Audio thread (read-only access)
reader->read(buffer, frameCount);  // No modification
```

**Guarantees:**
- No synchronization needed (read-only)
- Automatic lifetime management (shared_ptr)
- Memory safe (reader never deleted while audio thread using it)

**Examples:**
- Audio file data (WAV/AIFF/FLAC samples)
- Session graph (during playback)
- Routing matrix configuration (future)

### Pattern 4: Command/Response (Driver Layer)

**Use case:** JavaScript driver sends command to SDK

**Implementation:**
```typescript
// Client (TypeScript)
const response = await client.sendCommand({
    command: "LoadSession",
    params: { sessionPath: "/path/to/session.json" }
});

// SDK (C++)
json handleLoadSession(json params) {
    std::string path = params["sessionPath"];
    session = SessionJSON::loadSession(path);
    return {{"status", "success"}};
}
```

**Guarantees:**
- Schema validation before execution
- Error handling with typed error codes
- Version negotiation (ABI compatibility)

**Examples:**
- LoadSession, SaveSession, RenderClick (current)
- SetTransport, TriggerClipGridScene (planned)
- SetRouting, SetChannelGain (future)

## State Management

### UI Thread State

**Mutable state (UI thread owns):**
- Session graph (`SessionGraph`)
- Active clips map (`std::map<string, ActiveClip>`)
- Audio driver configuration
- UI component state

**Thread safety:**
- Mutex protection for `activeClips` map (UI thread only accesses)
- Atomic state for parameters read by audio thread
- Lock-free queue for events from audio thread

### Audio Thread State

**Immutable during playback:**
- Audio file data (`IAudioFileReader`)
- Session structure (track count, clip IDs)
- Fade curve types

**Mutable via atomics:**
- Playback position (`currentFrame`)
- Clip gain (`gainLinear`)
- Loop enable (`loopEnabled`)

**Never accessed:**
- UI component state
- File paths (uses pre-loaded data)
- Session JSON (uses in-memory graph)

## Error Handling

### UI Thread Errors

**Sources:**
- File not found (audio file, session JSON)
- Invalid session JSON (schema validation failure)
- Audio driver initialization failure
- Clip ID not found in session

**Handling:**
```cpp
ErrorCode result = transport.startClip(clipId);
if (result != ErrorCode::OK) {
    showErrorDialog("Failed to start clip: " + errorMessage(result));
}
```

**Recovery:**
- Prompt user to locate missing file
- Offer to fix session JSON
- Try fallback audio driver (DummyDriver)

### Audio Thread Errors

**Sources:**
- Buffer underrun (audio thread deadline missed)
- File read failure (corrupted audio file)
- Numerical issues (NaN, inf)

**Handling:**
```cpp
// Audio thread CANNOT block for error handling
// Instead: queue event for UI thread
if (underrun_detected) {
    eventQueue.enqueue(Event{BufferUnderrun, timestamp});
    // Fill buffer with silence (graceful degradation)
}
```

**Recovery:**
- UI thread receives error event
- Display warning to user
- Log to diagnostics
- Continue playback (don't crash)

## Performance Characteristics

### Typical Latencies

**Clip start latency:**
- UI thread → Audio thread: 1 buffer period (~5ms)
- File load (cold): 10-100ms (depends on file size)
- File load (cached): <1ms

**Event notification latency:**
- Audio thread → UI thread: 10-100ms (timer polling interval)
- UI callback invocation: <1ms

**Parameter updates:**
- Gain change: 1 buffer period (~5ms)
- Trim change: Immediate (if not playing), 1 buffer (~5ms if playing)
- Loop toggle: 1 buffer period (~5ms)

### CPU Usage

**Idle (no clips playing):**
- ~0% CPU (audio callback returns immediately)

**Active playback (8 clips):**
- ~5-15% CPU (depends on buffer size, sample rate)
- Target: <20% CPU for 16 clips

**Breakdown:**
- File reading: ~30% of processing time
- Fade calculation: ~20%
- Gain application: ~15%
- Loop checking: ~10%
- Mixing: ~25%

### Memory Usage

**Per session:**
- Session graph: ~10KB (100 clips)
- Active clip state: ~1KB per playing clip

**Per audio file:**
- WAV file (stereo, 48kHz, 60 seconds): ~23MB
- Total for 48 clips: ~1.1GB (if all loaded)

**Optimization:**
- Stream large files from disk (planned v1.0)
- Unload unused files after timeout
- Compress silent regions (future)

## Debugging Data Flow

### Common Issues

**Symptom: Audio glitches (clicks, pops)**

Causes:
1. Audio thread deadline missed (buffer underrun)
2. Memory allocation in audio thread
3. Lock acquisition in audio thread

Debugging:
```bash
# Run with AddressSanitizer (detects allocations)
cmake -B build -DCMAKE_BUILD_TYPE=Debug
ctest --test-dir build --output-on-failure

# Use rt.safety.auditor skill
# Checks for allocations, locks, I/O in audio callback
```

**Symptom: Parameter changes not applied**

Causes:
1. Incorrect memory ordering (atomic loads/stores)
2. Clip not active (already stopped)
3. UI thread not calling correct API

Debugging:
```cpp
// Add logging (UI thread only!)
std::cout << "updateClipGain(" << clipId << ", " << gainDb << ")\n";

// Check return code
ErrorCode result = transport.updateClipGain(clipId, gainDb);
assert(result == ErrorCode::OK);
```

**Symptom: Events not received**

Causes:
1. Event queue not polled (missing timer)
2. Callback not registered
3. Event dropped (queue full)

Debugging:
```cpp
// Verify timer is running
startTimer(100);  // Poll every 100ms

// Verify callback registered
transport.setOnClipFinished([](std::string clipId) {
    std::cout << "Clip finished: " << clipId << "\n";
});
```

### Verification Tools

**rt.safety.auditor skill:**
- Static analysis for audio thread violations
- Detects allocations, locks, I/O in audio callback

**AddressSanitizer:**
- Runtime detection of memory issues
- Enabled automatically in Debug builds

**ThreadSanitizer:**
- Runtime detection of race conditions
- Manual runs (not CI) due to performance overhead

**Determinism tests:**
- Verify bit-identical output across platforms
- `tests/determinism/offline_render_test.cpp`

## Related Documentation

**Core references:**
- `ARCHITECTURE.md` - System architecture (threading model section)
- `docs/integration/TRANSPORT_INTEGRATION.md` - Using TransportController
- `include/orpheus/transport_controller.h` - API documentation

**Implementation details:**
- `src/core/transport/transport_controller.cpp` - Transport implementation
- `src/core/session/session_graph.cpp` - Session management
- `src/core/audio_io/audio_file_reader.cpp` - File reading

**Testing:**
- `tests/transport/` - Transport controller tests
- `tests/determinism/` - Cross-platform determinism tests

## Related Diagrams

- See `component-map.mermaid.md` for component relationships
- See `architecture-overview.mermaid.md` for system layers
- See `entry-points.mermaid.md` for SDK access methods
