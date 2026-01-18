# Data Flow - Notes

**Last Updated:** 2026-01-18
**Related Diagram:** [data-flow.mermaid.md](./data-flow.mermaid.md)

## Overview

This document describes how data moves through the Orpheus SDK, with emphasis on the ORP121 enhancements to the callback queue, metering, and routing pipeline.

## Threading Model

### Three-Thread Architecture

| Thread | Purpose | Rules |
|--------|---------|-------|
| **Message Thread** | UI updates, user input, callback processing | May allocate, may block |
| **Audio Thread** | Real-time audio processing | NO allocations, NO locks |
| **File I/O Thread** | Background file operations | No audio thread interaction |

### Thread Communication

**Before ORP121 (Mutex-Based):**
```cpp
// DANGEROUS: Priority inversion risk
std::lock_guard<std::mutex> lock(m_callbackMutex);
m_callbackQueue.push(callback);
```

**After ORP121 (Lock-Free):**
```cpp
// SAFE: No contention, no priority inversion
size_t writeIdx = m_callbackWriteIndex.load(std::memory_order_relaxed);
m_callbackRing[writeIdx] = std::move(callback);
m_callbackWriteIndex.store(nextIdx, std::memory_order_release);
```

## Data Flow Phases

### 1. Initialization Phase

```
User → UI Thread → TransportController → RoutingMatrix → Audio Driver
```

**Key Configuration:**
- `config.sampleRate` - Sample rate in Hz (default: 48000)
- `config.headroomMode` - Headroom management mode
- `config.enableTruePeak` - Enable ITU-R BS.1770-4 metering

### 2. Clip Start Flow

```
UI Thread: Click → startClip() → Create ActiveClip → Atomic writes
```

**ActiveClip State:**
- `currentFrame` - Current playback position (atomic)
- `gainLinear` - Current gain value (atomic)
- `loopEnabled` - Loop mode flag (atomic)

**No lock required:** UI thread writes atomics, audio thread reads atomics.

### 3. Audio Processing Loop

The audio callback runs every ~5ms (512 samples @ 48kHz):

#### Step 1: Clip Processing
```cpp
for (auto& clip : activeClips) {
    // Read samples from file (pre-loaded)
    reader->read(buffer, frameCount);

    // Apply fade envelope (IN at start, OUT at end)
    applyFades(buffer, clip);

    // Apply clip gain (atomic read, no lock)
    float gain = clip.gainLinear.load(std::memory_order_relaxed);
    applyGain(buffer, gain);
}
```

#### Step 2: Routing Pipeline (ORP121 Enhanced)

**Channel Processing:**
1. Apply GainSmoother (interpolated gain changes)
2. Apply constant-power pan law (cos/sin coefficients)
3. Sum to stereo group buffers

**Metering (ORP121 Q-04):**
```cpp
// 4x oversampling for inter-sample peak detection
for (int phase = 0; phase < 4; phase++) {
    float interpolated = 0.0f;
    for (int tap = 0; tap < 12; tap++) {
        interpolated += history[tap] * coefficients[phase][tap];
    }
    peak = std::max(peak, std::abs(interpolated));
}
```

**Headroom Compensation (ORP121 Q-05):**
```cpp
float getHeadroomCompensation(uint8_t group_index) {
    switch (config.headroom_mode) {
        case HeadroomMode::None:
            return 1.0f;
        case HeadroomMode::Logarithmic:
            // Broadcast standard: 1/sqrt(n)
            return 1.0f / std::sqrt(static_cast<float>(count));
    }
}
```

**Soft-Knee Limiter (ORP121 C-02):**
```cpp
if (abs_sample > 0.794f) {  // -2 dBFS
    float excess = abs_sample - 0.794f;
    float compressed = 0.794f + std::tanh(excess / 0.3f) * 0.3f;
    sample = std::copysign(std::min(compressed, 0.9999f), sample);
}
```

### 4. Callback Flow (Lock-Free)

**Audio Thread (Producer):**
```cpp
void postCallback(std::function<void()> callback) {
    size_t writeIdx = m_callbackWriteIndex.load(std::memory_order_relaxed);
    size_t nextIdx = (writeIdx + 1) & (QUEUE_SIZE - 1);

    if (nextIdx == m_callbackReadIndex.load(std::memory_order_acquire)) {
        return;  // Queue full, drop callback (better than blocking)
    }

    m_callbackRing[writeIdx] = std::move(callback);
    m_callbackWriteIndex.store(nextIdx, std::memory_order_release);
}
```

**UI Thread (Consumer):**
```cpp
void processCallbacks() {
    size_t readIdx = m_callbackReadIndex.load(std::memory_order_relaxed);
    size_t writeIdx = m_callbackWriteIndex.load(std::memory_order_acquire);

    while (readIdx != writeIdx) {
        auto& callback = m_callbackRing[readIdx];
        if (callback) {
            callback();
            callback = nullptr;
        }
        readIdx = (readIdx + 1) & (QUEUE_SIZE - 1);
    }

    m_callbackReadIndex.store(readIdx, std::memory_order_release);
}
```

### 5. Gain Update Flow (Glitch-Free)

```
UI Thread: Slider change → updateClipGain(-6.0dB)
         → Convert to linear: pow(10, -6.0/20) = 0.501
         → Atomic write to ActiveClip.gainLinear

Audio Thread: Next callback reads new gain (no lock)
            → GainSmoother interpolates over ~10ms
            → No clicks or pops
```

### 6. Metering Query Flow

```
UI Thread (Timer, 30Hz): getTruePeak(channel)
                       → Read from TruePeakMeter
                       → Return MeterData with dBTP value
```

**Meter Update Rate:**
- Audio thread updates meters every callback (~200Hz at 48kHz/256)
- UI thread queries at ~30Hz (sufficient for visual display)
- No synchronization needed (atomic reads)

## Memory Ordering Explained

| Operation | Memory Order | Rationale |
|-----------|--------------|-----------|
| Write index read (same thread) | `relaxed` | No cross-thread dependency |
| Read index read (cross thread) | `acquire` | Synchronize with producer's release |
| Index writes | `release` | Make data visible to consumer |
| ActiveClip reads (audio) | `relaxed` | Approximate values acceptable |
| ActiveClip writes (UI) | `relaxed` | Audio thread will see eventually |

## Performance Characteristics

### Latency Budget

| Operation | Target | Actual |
|-----------|--------|--------|
| Audio callback | <10ms | ~5ms @ 512 samples |
| Callback delivery | <100ms | ~100ms (10Hz timer) |
| Gain change | <20ms | ~10ms (smoother ramp) |
| Meter update | <50ms | ~33ms (30Hz UI) |

### Memory Usage

| Component | Size |
|-----------|------|
| SPSC Queue | 256 × 32 bytes = 8 KB |
| TruePeakMeter (per channel) | 48 floats + state = ~256 bytes |
| ActiveClip | ~128 bytes (7 atomics + metadata) |

## When to Modify Data Flow

### Adding New Callback Types
1. Define callback signature
2. Call `postCallback()` from audio thread
3. Handle in UI thread's timer callback

### Adding New Metering
1. Create meter class with `process()` and `getPeak()` methods
2. Add to RoutingMatrix processing pipeline
3. Add getter to IRoutingMatrix interface

### Changing Headroom Mode
1. Update `HeadroomMode` enum
2. Update `getHeadroomCompensation()` switch
3. Update configuration UI

## Related Diagrams

- [architecture-overview](./architecture-overview.notes.md) - System design
- [component-map](./component-map.notes.md) - Class relationships

## References

1. ORP121 C-03: Lock-free callback queue implementation
2. ORP121 Q-04: ITU-R BS.1770-4 true-peak metering
3. ORP121 Q-05: Headroom management modes
4. C++ Memory Model: https://en.cppreference.com/w/cpp/atomic/memory_order
