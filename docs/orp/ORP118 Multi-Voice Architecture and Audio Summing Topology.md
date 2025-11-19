# ORP118 Multi-Voice Architecture and Audio Summing Topology

**Status:** Authoritative
**Date:** 2025-11-19
**Priority:** Documentation
**Related Docs:** ORP068, ORP085, ORP109, docs/ARCHITECTURE.md, docs/early_spec/core/audio_engine.md

---

## Executive Summary

This document provides a comprehensive technical reference for the Orpheus SDK's Multi-Voice architecture and Audio Summing Topology. The Multi-Voice system manages concurrent clip playback using a fixed-capacity, lock-free design with FIFO voice stealing. The Audio Summing Topology implements a three-tier hierarchical bus structure (Channels → Groups → Master) with deterministic, broadcast-safe signal processing. Both systems are designed for zero audio thread allocations and sample-accurate operation.

---

## I. Multi-Voice Architecture

### 1.1 Design Philosophy

The Multi-Voice architecture prioritizes broadcast reliability over flexible polyphony:

- **Fixed Capacity**: Pre-allocated arrays prevent audio thread allocations
- **Deterministic Behavior**: Same input always produces same output
- **Lock-Free Operation**: UI and audio threads communicate via atomic operations
- **Sample-Accurate**: All timing uses 64-bit sample counts, never floating-point seconds

### 1.2 Core Components

#### TransportController

Primary voice management controller located at `src/core/transport/transport_controller.h`.

**Responsibilities:**
- Voice allocation, rendering, and deallocation
- Lock-free command queue (UI → Audio thread)
- Callback queue (Audio → UI thread)
- Position tracking and state management

#### ActiveClip Structure

Per-voice state structure containing all data needed for playback:

```cpp
struct ActiveClip {
    ClipHandle handle;           // Clip identifier
    uint32_t voiceId;            // Unique voice instance ID
    int64_t startSample;         // Transport time when started (for age tracking)
    int64_t currentSample;       // Current playback position

    // Atomic metadata (cross-thread safe)
    std::atomic<int64_t> trimInSamples;
    std::atomic<int64_t> trimOutSamples;
    std::atomic<float> fadeInSamples;
    std::atomic<float> fadeOutSamples;
    std::atomic<float> gainDb;
    std::atomic<bool> loopEnabled;

    // Fade state
    float fadeOutGain;
    bool isStopping;
    int64_t fadeOutStartPos;
    bool isRestarting;

    // Audio data
    std::shared_ptr<IAudioFileReader> reader;
};
```

#### Capacity Constants

```cpp
static constexpr size_t MAX_ACTIVE_CLIPS = 32;      // Total concurrent clips
static constexpr size_t MAX_VOICES_PER_CLIP = 4;    // Layering same clip
static constexpr size_t MAX_COMMANDS = 256;         // Command queue depth
```

### 1.3 Voice Lifecycle

#### State Machine

```
           +------------+
           |   FREE     |  (slot available in array)
           +-----+------+
                 |
        startClip(handle)
                 |
                 v
           +-----+------+
           |  ACTIVE    |  (rendering audio)
           +-----+------+
                 |
        +--------+--------+
        |                 |
   stopClip()         end of clip
        |                 |
        v                 v
   +----+----+      +-----+-----+
   | STOPPING |      | LOOP/STOP |
   | (fade)   |      +-----+-----+
   +----+----+            |
        |            loop enabled?
   fade complete          |
        |           +-----+-----+
        v           YES        NO
   +----+----+      |          |
   |  FREE   |   seek to    trigger
   +---------+   trimIn     fade-out
                    |          |
                    v          v
                 ACTIVE    STOPPING
```

#### Allocation Phase

```cpp
void TransportController::addActiveClip(ClipHandle handle) {
    // 1. Count active voices for this clip
    size_t currentVoiceCount = countActiveVoices(handle);

    // 2. Voice stealing if at capacity
    if (currentVoiceCount >= MAX_VOICES_PER_CLIP) {
        ActiveClip* oldest = findOldestVoice(handle);
        if (oldest) {
            // Notify UI of stolen voice
            postCallback([this, handle, pos = getCurrentPosition()]() {
                if (m_callback) {
                    m_callback->onClipStopped(handle, pos);
                }
            });
            removeActiveVoice(oldest->voiceId);
        }
    }

    // 3. Allocate slot
    ActiveClip& clip = m_activeClips[m_activeClipCount++];
    clip.handle = handle;
    clip.voiceId = m_nextVoiceId++;
    clip.startSample = m_currentPosition;

    // 4. Load metadata from storage (brief mutex lock)
    {
        std::lock_guard<std::mutex> lock(m_audioFilesMutex);
        auto it = m_audioFiles.find(handle);
        clip.trimInSamples.store(it->second.trimIn);
        clip.trimOutSamples.store(it->second.trimOut);
        // ... copy remaining metadata
    }

    // 5. Initialize playback position
    clip.currentSample = clip.trimInSamples.load();
    clip.reader->seek(clip.currentSample);

    // 6. Notify UI
    postCallback([this, handle]() {
        if (m_callback) {
            m_callback->onClipStarted(handle, /* position */);
        }
    });
}
```

#### Active Phase (Per-Buffer Processing)

```cpp
void TransportController::processAudio(float** outputs, size_t numFrames) {
    // 1. Process pending commands
    processCommands();

    // 2. Pre-render fade-outs
    for (size_t i = 0; i < m_activeClipCount; ++i) {
        if (m_activeClips[i].isStopping) {
            calculateFadeOutGain(m_activeClips[i], numFrames);
        }
    }

    // 3. Render each voice
    for (size_t i = 0; i < m_activeClipCount; ++i) {
        ActiveClip& clip = m_activeClips[i];

        // Read audio data
        clip.reader->read(m_clipReadBuffers[i], numFrames);

        // Apply gains: clip gain, crossfade, stop fade, clip fades
        applyGainEnvelopes(clip, m_clipReadBuffers[i], numFrames);

        // Mix to channel buffer
        for (size_t frame = 0; frame < numFrames; ++frame) {
            m_channelBuffers[clip.outputChannel][frame] +=
                m_clipReadBuffers[i][frame];
        }
    }

    // 4. Route through matrix
    m_routingMatrix->processRouting(m_channelBuffers, outputs, numFrames);

    // 5. Post-render updates
    for (size_t i = 0; i < m_activeClipCount; ) {
        if (shouldRemoveVoice(m_activeClips[i])) {
            removeVoiceAtIndex(i);
        } else {
            ++i;
        }
    }
}
```

#### Release Phase

```cpp
void TransportController::stopClip(ClipHandle handle) {
    // Mark all voices for this clip as stopping
    for (size_t i = 0; i < m_activeClipCount; ++i) {
        if (m_activeClips[i].handle == handle) {
            m_activeClips[i].isStopping = true;
            m_activeClips[i].fadeOutGain = 1.0f;
            m_activeClips[i].fadeOutStartPos = m_currentPosition;
        }
    }
}
```

### 1.4 Voice Stealing Policy

**Algorithm: FIFO (Oldest First)**

When a clip exceeds `MAX_VOICES_PER_CLIP`, the oldest voice is immediately removed:

```cpp
ActiveClip* TransportController::findOldestVoice(ClipHandle handle) {
    ActiveClip* oldest = nullptr;
    int64_t oldestStartSample = INT64_MAX;

    for (size_t i = 0; i < m_activeClipCount; ++i) {
        if (m_activeClips[i].handle == handle) {
            if (m_activeClips[i].startSample < oldestStartSample) {
                oldestStartSample = m_activeClips[i].startSample;
                oldest = &m_activeClips[i];
            }
        }
    }
    return oldest;
}
```

**Policy Characteristics:**
- Per-clip limit only (no global stealing across different clips)
- Immediate removal without fade-out when stolen
- Deterministic behavior (same playback sequence = same stealing)
- UI callback posted for state tracking

### 1.5 Threading Model

#### Thread Responsibilities

| Thread | Priority | Responsibilities | Constraints |
|--------|----------|------------------|-------------|
| Audio | Highest | `processAudio()`, voice rendering | Zero allocations, no locks |
| UI/Message | Normal | Commands, callbacks, state queries | Can allocate, brief locks OK |
| Background | Low | File loading, format conversion | Can block |

#### Lock-Free Command Queue

```cpp
// Command queue structure
std::array<TransportCommand, MAX_COMMANDS> m_commands;
std::atomic<size_t> m_commandWriteIndex{0};
std::atomic<size_t> m_commandReadIndex{0};

// UI thread: post command
void postCommand(TransportCommand cmd) {
    size_t writeIdx = m_commandWriteIndex.load(std::memory_order_relaxed);
    m_commands[writeIdx % MAX_COMMANDS] = cmd;
    m_commandWriteIndex.store(writeIdx + 1, std::memory_order_release);
}

// Audio thread: process commands
void processCommands() {
    size_t readIdx = m_commandReadIndex.load(std::memory_order_relaxed);
    size_t writeIdx = m_commandWriteIndex.load(std::memory_order_acquire);

    while (readIdx < writeIdx) {
        TransportCommand& cmd = m_commands[readIdx % MAX_COMMANDS];
        executeCommand(cmd);
        ++readIdx;
    }
    m_commandReadIndex.store(readIdx, std::memory_order_relaxed);
}
```

#### Callback Queue (Audio → UI)

```cpp
std::mutex m_callbackMutex;
std::queue<std::function<void()>> m_callbackQueue;

// Audio thread: post callback
void postCallback(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_callbackQueue.push(std::move(callback));
}

// UI thread: process callbacks (called from message loop)
void processCallbacks() {
    std::queue<std::function<void()>> callbacks;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        std::swap(callbacks, m_callbackQueue);
    }
    while (!callbacks.empty()) {
        callbacks.front()();
        callbacks.pop();
    }
}
```

---

## II. Audio Summing Topology

### 2.1 Design Philosophy

The Audio Summing Topology implements a hierarchical mixing architecture optimized for:

- **Broadcast Safety**: Clipping protection, deterministic output
- **Flexibility**: Multiple routing configurations for different workflows
- **Performance**: Pre-allocated buffers, potential for SIMD optimization
- **Monitoring**: Real-time metering at all stages

### 2.2 Buffer Architecture

#### Format

- **Sample Format**: Planar float32 (non-interleaved)
- **Maximum Buffer Size**: 2048 samples (configurable)
- **Channel Capacity**: Up to 64 input channels

```cpp
// Pre-allocated buffer structure
std::vector<std::vector<float>> m_group_buffers;    // [num_groups][max_buffer_size]
std::vector<float> m_temp_buffer;                   // Temporary processing buffer
float** output_buffers_;                            // Planar output pointers
```

#### Channel Layout

Planar format stores all samples for one channel contiguously:

```
Channel 0: [s0, s1, s2, ..., sN-1]
Channel 1: [s0, s1, s2, ..., sN-1]
...
Channel M-1: [s0, s1, s2, ..., sN-1]
```

This layout optimizes for:
- Sequential memory access during per-channel processing
- Future SIMD vectorization (process 4-8 samples per instruction)
- Cache efficiency when processing one channel at a time

### 2.3 Three-Tier Bus Hierarchy

```
┌─────────────────────────────────────────────────────────────┐
│                    INPUT LAYER (Channels)                    │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐       ┌──────┐         │
│  │ Ch 0 │ │ Ch 1 │ │ Ch 2 │ │ Ch 3 │  ...  │Ch 63 │         │
│  │(Clip)│ │(Clip)│ │(Clip)│ │(Clip)│       │(Clip)│         │
│  └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘       └──┬───┘         │
│     │        │        │        │              │              │
│     │    ┌───┴────┐   │    ┌───┴────┐         │              │
│     │    │ Gain   │   │    │ Gain   │         │              │
│     │    │ Pan    │   │    │ Pan    │         │              │
│     │    └───┬────┘   │    └───┬────┘         │              │
└─────┼────────┼────────┼────────┼──────────────┼──────────────┘
      │        │        │        │              │
      v        v        v        v              v
┌─────────────────────────────────────────────────────────────┐
│                    GROUP LAYER (Buses)                       │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐              │
│  │  Group A   │  │  Group B   │  │  Group C   │  ...         │
│  │ (Sum + Gain)│ │ (Sum + Gain)│ │ (Sum + Gain)│              │
│  │ Mute/Solo  │  │ Mute/Solo  │  │ Mute/Solo  │              │
│  │  Metering  │  │  Metering  │  │  Metering  │              │
│  └─────┬──────┘  └─────┬──────┘  └─────┬──────┘              │
└────────┼───────────────┼───────────────┼────────────────────┘
         │               │               │
         v               v               v
┌─────────────────────────────────────────────────────────────┐
│                    OUTPUT LAYER (Master)                     │
│  ┌──────────────────────────────────────────────────┐       │
│  │                   Master Bus                      │       │
│  │  Sum → Gain → Clip Protection → Metering → Out   │       │
│  └──────────────────────────────────────────────────┘       │
│                          │                                   │
│                          v                                   │
│              ┌───────────────────────┐                       │
│              │   Hardware Outputs    │                       │
│              │    (1-32 channels)    │                       │
│              └───────────────────────┘                       │
└─────────────────────────────────────────────────────────────┘
```

#### Group Configuration

From `docs/early_spec/core/bus_architecture.md`:

- **Clip Groups A-D**: Primary content buses (broadcast carts, playback)
- **Audition Bus**: Dedicated preview bus (zero impact on main output)
- **Input Buses**: Record A/B (stereo), LTC (mono)
- **Output Assignment**: Each group routes to specific output bus (0-15)

### 2.4 Summing Algorithm

**Method: Simple Additive Summation**

The SDK uses direct sample addition without compensation:

```cpp
// Located in src/core/routing/routing_matrix.cpp

// Channel → Group summing
for (uint32_t frame = 0; frame < num_frames; ++frame) {
    float channel_gain = channel.gain_smoother->process();
    float sample = input[frame];
    sample *= channel_gain;
    group_buffer[frame] += sample;  // Direct additive sum
}

// Group → Master summing
for (uint32_t frame = 0; frame < num_frames; ++frame) {
    float group_gain = group.gain_smoother->process();
    float sample = group_buffer[frame];
    sample *= group_gain;
    master_output[out][frame] += sample;  // Direct additive sum
}
```

**Offline Rendering Enhancement:**

For offline/bounce operations, double precision accumulation improves accuracy:

```cpp
// src/orpheus/render_tracks.cpp
buffer[dest_frame * output_channels + target] +=
    static_cast<double>(channel_samples[frame]);
```

### 2.5 Gain Staging

#### Multi-Stage Architecture

| Stage | Range | Smoothing | Purpose |
|-------|-------|-----------|---------|
| Channel | -100 to +12 dB | 10ms linear | Per-clip fader |
| Group | -100 to +12 dB | 10ms linear | Bus fader |
| Master | -100 to +12 dB | 10ms linear | Output fader |

#### Gain Smoother Implementation

Linear ramping prevents clicks/pops during gain changes:

```cpp
// src/core/routing/gain_smoother.cpp

float GainSmoother::process() {
    if (m_current < m_target) {
        m_current += m_increment;
        if (m_current >= m_target) {
            m_current = m_target;  // Prevent overshoot
        }
    } else if (m_current > m_target) {
        m_current -= m_increment;
        if (m_current <= m_target) {
            m_current = m_target;  // Prevent overshoot
        }
    }
    return m_current;
}
```

**Configuration:**
- Default smoothing time: 10ms
- Configurable range: 1-100ms
- Lock-free operation (UI sets target, audio thread processes)

### 2.6 Pan Law

**Constant-Power Pan Law (-3 dB at Center)**

```cpp
// src/core/routing/routing_matrix.cpp

float pan_radians = (pan + 1.0f) * 0.25f * 3.14159265359f;
float gain_left = std::cos(pan_radians);
float gain_right = std::sin(pan_radians);
```

This ensures:
- Equal power distribution: L² + R² = 1
- -3 dB at center (pan = 0)
- Full left (pan = -1) = left only
- Full right (pan = +1) = right only

### 2.7 Clipping Protection

Two-stage protection ensures broadcast-safe output:

```cpp
// src/core/routing/routing_matrix.cpp

// Stage 1: Soft-knee limiter using tanh
if (std::abs(sample) > 0.9f) {
    sample = std::tanh(sample * 0.9f) / 0.9f;
}

// Stage 2: Hard clip for absolute safety
sample = std::max(-1.0f, std::min(1.0f, sample));
```

**Characteristics:**
- Soft limiting begins at -0.9 dBFS
- Smooth compression curve (no harsh clipping artifacts)
- Hard limit at 0 dBFS (absolute broadcast safety)

### 2.8 Metering System

Real-time metering at all stages:

```cpp
void RoutingMatrix::processMetering(
    float* buffer,
    size_t num_frames,
    std::atomic<float>& peak,
    std::atomic<float>& rms
) {
    // Peak detection
    float peak_value = 0.0f;
    for (size_t i = 0; i < num_frames; ++i) {
        float abs_sample = std::abs(buffer[i]);
        if (abs_sample > peak_value) {
            peak_value = abs_sample;
        }
    }

    // RMS calculation
    float sum_squares = 0.0f;
    for (size_t i = 0; i < num_frames; ++i) {
        sum_squares += buffer[i] * buffer[i];
    }
    float rms_value = std::sqrt(sum_squares / num_frames);

    // Atomic updates for UI thread
    peak.store(peak_value, std::memory_order_release);
    rms.store(rms_value, std::memory_order_release);
}
```

**Metering Points:**
- Per-channel (post-fader)
- Per-group (post-fader)
- Master output (post-limiter)

### 2.9 Thread Safety

#### Double-Buffer Configuration

```cpp
// Safe configuration updates
RoutingConfig m_config_buffers[2];
std::atomic<int> m_active_config_idx{0};

// UI thread: update inactive buffer, then swap
void updateConfig(const RoutingConfig& new_config) {
    int inactive = 1 - m_active_config_idx.load();
    m_config_buffers[inactive] = new_config;
    m_active_config_idx.store(inactive, std::memory_order_release);
}

// Audio thread: read active buffer
const RoutingConfig& getActiveConfig() {
    return m_config_buffers[m_active_config_idx.load(std::memory_order_acquire)];
}
```

#### Atomic State for UI Reads

```cpp
struct ChannelState {
    std::atomic<bool> mute{false};
    std::atomic<bool> solo{false};
    std::atomic<float> peak_level{0.0f};
    std::atomic<float> rms_level{0.0f};
};
```

---

## III. Performance Considerations

### 3.1 Current Optimizations

- **Pre-allocated Buffers**: All audio buffers allocated at initialization
- **Lock-Free Communication**: Commands and state via atomic operations
- **Linear Gain Smoothing**: Predictable, branch-free processing
- **Planar Buffer Format**: Cache-friendly sequential access

### 3.2 Planned Optimizations

From `docs/early_spec/core/architecture.md`:

```markdown
### SIMD Acceleration (Planned)
* AVX/SSE optimization for:
  - Fade curve processing
  - Format conversion
  - Voice processing
  - Mix operations
* Format-aware vectorization
* Parallel voice processing
* Cache-coherent operations
```

**Note:** Current implementation uses scalar loops. SIMD optimizations are planned but not yet implemented.

### 3.3 Buffer Size Handling

The system supports dynamic buffer size changes:

- Minimum: 64 samples
- Maximum: 2048 samples
- Common sizes: 128, 256, 512, 1024 samples

All internal buffers are allocated for maximum size at initialization.

---

## IV. Integration Points

### 4.1 Host Adapter Integration

Audio drivers call into the SDK via standard callback:

```cpp
// CoreAudio example (src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp)
OSStatus renderCallback(
    void* inRefCon,
    AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData
) {
    CoreAudioDriver* driver = static_cast<CoreAudioDriver*>(inRefCon);

    // Call SDK
    driver->callback_->processAudio(
        input_ptrs,
        output_ptrs,
        num_channels,
        inNumberFrames
    );

    // Copy to host buffers
    // ...
}
```

### 4.2 Query APIs

```cpp
// Voice management queries
size_t countActiveVoices(ClipHandle handle) const;
int64_t getClipPosition(ClipHandle handle) const;
PlaybackState getClipState(ClipHandle handle) const;

// Routing queries
float getChannelPeak(uint32_t channel) const;
float getGroupRMS(uint32_t group) const;
bool isGroupSoloed(uint32_t group) const;
```

---

## V. Related Documentation

### Implementation Files

| Component | Header | Implementation |
|-----------|--------|----------------|
| TransportController | `src/core/transport/transport_controller.h` | `src/core/transport/transport_controller.cpp` |
| RoutingMatrix | `include/orpheus/routing_matrix.h` | `src/core/routing/routing_matrix.cpp` |
| GainSmoother | `src/core/routing/gain_smoother.h` | `src/core/routing/gain_smoother.cpp` |
| DSP Oscillator | `include/orpheus/dsp/oscillator.hpp` | - |

### Architecture Documents

- **ORP068** - Implementation Plan (v2.0): Master SDK development plan
- **ORP085** - OCC SDK Enhancement Sprint: Multi-voice improvements
- **ORP109** - SDK Feature Roadmap: Future voice/routing enhancements
- `docs/ARCHITECTURE.md` - System design overview
- `docs/early_spec/core/audio_engine.md` - Voice pool specifications
- `docs/early_spec/core/bus_architecture.md` - Bus topology design
- `docs/sync-architecture/01-system-overview.md` - Threading model

---

## VI. Summary

### Multi-Voice Architecture

| Aspect | Value |
|--------|-------|
| Max Concurrent Clips | 32 |
| Max Voices Per Clip | 4 |
| Stealing Policy | FIFO (oldest first) |
| Voice ID | Monotonic uint32_t counter |
| Position Tracking | 64-bit sample count |
| Thread Safety | Lock-free commands, atomic metadata |

### Audio Summing Topology

| Aspect | Value |
|--------|-------|
| Buffer Format | Planar float32 |
| Max Buffer Size | 2048 samples |
| Bus Hierarchy | Channels (64) → Groups (16) → Master (32) |
| Summing Method | Simple additive |
| Pan Law | Constant-power (-3 dB center) |
| Gain Range | -100 to +12 dB |
| Smoothing | Linear, 10ms default |
| Clipping Protection | tanh soft-knee + hard clip |
| Metering | Peak + RMS per stage |

Both systems are designed to meet broadcast requirements: deterministic behavior, zero audio thread allocations, and sample-accurate operation.

---

**Document Version:** 1.0
**Last Updated:** 2025-11-19
