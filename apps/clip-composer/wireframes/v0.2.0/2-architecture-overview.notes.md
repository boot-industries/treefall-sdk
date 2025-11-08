# Architecture Overview - Explanatory Notes

## Overview

This diagram visualizes the complete 5-layer architecture of Orpheus Clip Composer v0.2.0, including the critical threading model that enables real-time audio performance. The design follows a strict separation of concerns: UI components (Layer 1), application logic (Layer 2), SDK integration (Layer 3), real-time processing (Layer 4), and platform I/O (Layer 5).

**Key Insight:** Layers 1-2 are OCC-specific application code, while Layers 3-5 are Orpheus SDK shared infrastructure.

## Key Architectural Decisions

### 5-Layer Separation

1. **Layer 1: JUCE UI Components** - All visual elements, user interaction, event handling
   - *Rationale:* JUCE provides cross-platform UI toolkit with mature component library
   - *Constraint:* Runs only on Message Thread (JUCE threading model)

2. **Layer 2: Application Logic** - Session management, clip metadata, routing configuration
   - *Rationale:* Domain logic isolated from UI presentation and audio processing
   - *Benefit:* Testable without JUCE UI, reusable across different frontends

3. **Layer 3: Orpheus SDK Integration** - API boundary between OCC and SDK
   - *Rationale:* Host-neutral SDK enables reuse across REAPER, standalone apps, plugins
   - *Benefit:* OCC doesn't implement audio engine - delegates to proven SDK

4. **Layer 4: Real-Time Audio Processing** - Deterministic audio rendering, driver abstraction
   - *Rationale:* Sample-accurate timing, bit-identical output, 24/7 reliability
   - *Constraint:* NO allocations, NO locks, NO I/O (real-time safety)

5. **Layer 5: Platform Audio I/O** - Hardware interfaces (CoreAudio, ASIO, WASAPI)
   - *Rationale:* Abstract platform differences, support professional low-latency drivers
   - *Benefit:* OCC works on macOS (CoreAudio), Windows (ASIO/WASAPI), Linux (ALSA planned)

### Threading Model

**Why 3 Threads?**

- **Message Thread** - JUCE requirement for UI events, also handles file I/O (session load/save)
- **Audio Thread** - Real-time constraint (10ms @ 512 samples), zero allocations, lock-free
- **File I/O Thread** - Offload expensive operations (waveform rendering, file scanning) from UI and audio threads

**Thread Safety Strategy:**

- **Lock-Free Command Queue** - UI → Audio (startClip, stopClip, seekClip, updateMetadata)
- **Lock-Free Callback Queue** - Audio → UI (onClipStarted, onClipStopped, onPositionUpdate)
- **Atomic State** - Transport position, button states (75fps visual sync in v0.2.0)
- **NO Mutexes** - Mutexes cause priority inversion, unbounded wait times (forbidden on audio thread)

### SDK Integration Philosophy

**Host-Neutral Core:**
- SDK provides ITransportController, IAudioFileReader, IRoutingMatrix interfaces
- Same SDK powers REAPER adapter, standalone Clip Composer, future VST3 plugins
- OCC is just one "host" application using Orpheus SDK

**Why Not Embed SDK Code Directly?**
- **Maintainability** - Bug fixes in SDK benefit all hosts (REAPER, OCC, future plugins)
- **Determinism** - SDK guarantees bit-identical output across platforms
- **Testing** - SDK has comprehensive test suite, OCC inherits reliability

## Important Patterns

### Lock-Free Command Queue Pattern

```cpp
// UI thread (Message Thread):
transportController->startClip(clipHandle);  // Non-blocking, lock-free enqueue

// Audio thread (later, in processBlock):
transportController->processAudio(outputs, numSamples);  // Dequeues commands, starts clip
```

**Key Properties:**
- Single-producer (UI), single-consumer (Audio) - simplifies synchronization
- Fixed-size ring buffer - pre-allocated at startup, no runtime allocation
- Fail-fast on overflow - better than blocking or crashing

### Callback Queue Pattern

```cpp
// Audio thread (during processBlock):
callbackQueue->post(ClipStartedEvent{clipHandle, position});  // Lock-free enqueue

// Message thread (timer callback):
while (auto event = callbackQueue->pop()) {
    clipGrid->highlightPlayingClip(event.clipHandle);  // Safe UI update
}
```

**Why Not Direct UI Updates from Audio Thread?**
- JUCE UI is NOT thread-safe - calling from audio thread crashes
- Callback queue + timer provides safe async notification

### File I/O Thread Pattern

**Problem:** Loading 960 clips blocks UI for seconds
**Solution:** Background thread pre-renders waveforms

```cpp
// Message thread (session load):
for (auto& clip : session.clips) {
    fileIOThread->enqueue([clip] {
        auto waveformData = renderWaveform(clip.filePath);  // Slow I/O, OK on background thread
        MessageManager::callAsync([waveformData] {
            waveformDisplay->setWaveform(waveformData);  // Update UI safely
        });
    });
}
```

## Technical Debt / Complexity

### Known Limitations

1. **No ASIO Support Yet (v0.2.0)** - Windows uses WASAPI (~16ms latency)
   - *Impact:* Professional Windows users need <5ms latency
   - *Plan:* ASIO support in v1.0 (requires manual SDK installation, licensing)

2. **Basic Routing Matrix** - 4 groups → master only (no aux sends, no complex routing)
   - *Impact:* Limited flexibility for advanced users
   - *Plan:* Aux sends in v2.0

3. **Fade Curves Not Applied** - Stored in metadata but not rendered during playback
   - *Impact:* All fades use linear curve
   - *Plan:* Implement fade curve rendering in v0.3.0

### Complexity Hotspots

- **Layer 3 ↔ Layer 4 Boundary** - SDK API contracts must remain stable across OCC versions
- **Threading Transitions** - Command queue, callback queue, atomic state require careful design
- **Session Loading** - JSON parsing + file validation + waveform rendering (sequential dependencies)

## Common Workflows

### Clip Triggering Flow (End-to-End)

1. **User clicks ClipButton** (Layer 1, Message Thread)
2. **ClipGrid → ClipManager** (Layer 2, get clip metadata)
3. **ClipManager → CommandQueue** (enqueue startClip command)
4. **CommandQueue → ITransportController** (Audio Thread dequeues)
5. **ITransportController → SessionGraph** (Layer 4, start clip state machine)
6. **SessionGraph → MixEngine** (Layer 4, begin mixing clip audio)
7. **MixEngine → AudioDriver** (Layer 5, output to hardware)
8. **ITransportController → CallbackQueue** (enqueue onClipStarted event)
9. **CallbackQueue → ClipGrid** (Message Thread, highlight button)

**Latency Breakdown:**
- UI → Audio: <1ms (lock-free enqueue)
- Audio processing: <10ms (512 samples @ 48kHz)
- Audio → UI feedback: 13ms (75fps timer callback)
- **Total perceived latency:** ~25ms (acceptable for broadcast)

### Session Save Flow

1. **User clicks Save** (TransportControls, Message Thread)
2. **SessionManager collects metadata** (clips, routing, preferences)
3. **Serialize to JSON** (nlohmann::json or juce::JSON)
4. **Write to file** (blocking I/O, OK on Message Thread during save)
5. **Show confirmation dialog** (UI feedback)

**Performance:** <2 seconds for 960-clip session

### Waveform Display Flow

1. **Clip loaded** (SessionManager, Message Thread)
2. **IAudioFileReader opens file** (SDK, Message Thread)
3. **Enqueue waveform render task** (File I/O Thread)
4. **Render waveform** (downsample audio samples to pixel data)
5. **Post UI update** (MessageManager::callAsync)
6. **WaveformDisplay repaints** (Layer 1, Message Thread)

**Why Background Thread?**
- Waveform rendering for 5-minute clip takes ~100ms
- Blocking UI for 100ms × 960 clips = frozen app
- Background rendering keeps UI responsive

## Where to Make Changes

### Feature: Add New Audio Effect (e.g., EQ)

**Impact on Layers:**
- **Layer 4:** Implement effect in SDK (IEffectProcessor interface)
- **Layer 3:** Add SDK API to OCC (effectController->setEQ(clipHandle, params))
- **Layer 2:** Add effect metadata to ClipManager
- **Layer 1:** Add EQ UI controls to ClipEditDialog

**Files to Modify:**
- SDK: `src/core/effects/eq_processor.cpp`
- OCC Layer 3: `Source/AudioEngine/AudioEngine.cpp`
- OCC Layer 2: `Source/Session/SessionManager.cpp` (persist EQ settings)
- OCC Layer 1: `Source/UI/ClipEditDialog.cpp` (EQ controls)

### Feature: Multi-Instance Sync (Network)

**Impact on Layers:**
- **Layer 2:** Add NetworkSyncManager (new component)
- **Layer 1:** Add sync status indicator UI
- **Layer 3:** Extend SDK to support network triggers

**New Files Needed:**
- `Source/Network/NetworkSyncManager.h/cpp`
- `Source/UI/SyncStatusIndicator.h/cpp`

**Threading Considerations:**
- Network I/O on background thread (NOT Message or Audio thread)
- Use lock-free queue to notify Audio thread of remote triggers

### Bug Fix: Audio Dropouts

**Investigation Path:**
1. Check **Layer 5** - Audio driver buffer size (increase from 512 to 1024 samples)
2. Check **Layer 4** - SDK audio callback (profile for allocations with sanitizers)
3. Check **Layer 3** - OCC AudioEngine (verify lock-free command queue usage)
4. Check **Layer 2** - SessionManager (ensure no file I/O blocking audio thread)

**Common Causes:**
- Buffer underruns (small buffer size on slow hardware)
- Allocations on audio thread (use AddressSanitizer to detect)
- Lock contention (use ThreadSanitizer to detect)
- Disk I/O latency (pre-load audio files into memory)

### Performance Optimization: Reduce CPU Usage

**Layer-by-Layer Strategy:**
1. **Layer 5:** Use ASIO on Windows (lower latency, lower CPU)
2. **Layer 4:** Optimize mix engine (SIMD, vectorization)
3. **Layer 3:** Batch SDK commands (reduce queue overhead)
4. **Layer 2:** Cache session metadata (avoid repeated JSON parsing)
5. **Layer 1:** Reduce UI repaint frequency (e.g., 30fps instead of 75fps for non-critical views)

## Related Diagrams

- **1-repo-structure.mermaid.md** - Shows file organization (Source/UI/, Source/Session/, etc.)
- **3-component-map.mermaid.md** - Detailed component relationships within each layer
- **4-data-flow.mermaid.md** - Sequence diagrams for specific workflows (clip trigger, session save)
- **5-entry-points.mermaid.md** - Application initialization, keyboard shortcuts, audio callbacks

## Cross-References to OCC Docs

- **OCC023** - Component Architecture v1.0 (authoritative 5-layer spec)
- **OCC096** - SDK Integration Patterns (code examples for Layer 3)
- **OCC097** - Session Format (Layer 2 SessionManager implementation)
- **OCC098** - UI Components (Layer 1 JUCE component implementations)
- **OCC100** - Performance Requirements (latency targets, CPU budgets)
- **OCC101** - Troubleshooting Guide (debugging audio thread issues)

## Design Philosophy

### Reliability Above All

**24/7 Operational Capability:**
- No runtime allocations on audio thread (pre-allocate at startup)
- Graceful degradation (if file missing, show error but don't crash)
- Deterministic behavior (same input → same output, always)

### Performance-First

**Ultra-Low Latency:**
- <5ms round-trip latency target (ASIO driver)
- Sample-accurate timing (64-bit sample counts, never float seconds)
- Lock-free communication (no priority inversion)

### Real-Time Safety

**Audio Thread Constraints:**
- ⛔ NO allocations (new, malloc, std::vector::push_back)
- ⛔ NO locks (std::mutex, std::lock_guard)
- ⛔ NO I/O (file reads, network calls)
- ⛔ NO unbounded loops (always fixed iteration count)
- ✅ Lock-free data structures (ring buffer, atomic operations)

### Determinism

**Bit-Identical Output:**
- Same session file → same audio output on different machines
- Use `std::bit_cast` for float operations (avoid platform differences)
- 64-bit sample counts (no floating-point time drift)

### Host-Neutral SDK

**Cross-Platform Core:**
- Orpheus SDK works on macOS, Windows, Linux
- Same SDK code in REAPER adapter, Clip Composer, future plugins
- OCC is one "application" of many using the same engine

---

**Last Updated:** October 31, 2025
**Version:** v0.2.0
**Maintained By:** Claude Code + Human Developers
