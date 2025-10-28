# Allowed Patterns for Real-Time Audio Code

**Purpose:** Approved design patterns and constructs safe for Orpheus SDK audio threads
**Version:** 1.0
**Last Updated:** 2025-10-18

---

## Executive Summary

This document catalogs proven safe patterns for real-time audio programming. All patterns listed here:

- **Do not allocate** memory on the heap
- **Do not block** on locks or I/O
- **Have bounded** execution time
- **Are deterministic** (same input → same output)

---

## Category 1: Memory Management

### 1.1 Stack Allocation

**Pattern:** Fixed-size buffers on the stack

```cpp
void processBlock(float** outputs, int numFrames) {
    // SAFE: Stack allocation, automatically cleaned up
    float tempBuffer[kMaxFrames * kMaxChannels];

    // ... use tempBuffer
}
```

**Constraints:**

- Total stack allocation must fit in audio thread stack (typically 512KB - 8MB)
- Avoid very large stack allocations (>100KB) - use pre-allocated members instead

---

### 1.2 Pre-Allocated Members

**Pattern:** Allocate in constructor, use in audio thread

```cpp
class AudioProcessor {
public:
    AudioProcessor() {
        // SAFE: Allocation in constructor (UI thread)
        scratchBuffer_.resize(kMaxFrames * kMaxChannels);
        activeClips_.reserve(kMaxClips);
    }

    void processBlock(float** outputs, int numFrames) {
        // SAFE: No allocation, just using pre-allocated buffer
        scratchBuffer_[0] = outputs[0][0];
    }

private:
    std::vector<float> scratchBuffer_;  // Pre-allocated
    std::vector<int> activeClips_;      // Pre-allocated with reserve()
};
```

**Key Rules:**

- Always `reserve()` enough space in constructor
- Never `push_back()` beyond capacity in audio thread
- Use `resize()` only if new size ≤ capacity

---

### 1.3 std::array (Compile-Time Size)

**Pattern:** Fixed-size containers

```cpp
class AudioMixer {
    // SAFE: Compile-time size, no heap allocation
    std::array<float, kMaxChannels> gainCoefficients_;
    std::array<Clip*, kMaxClips> activeClips_;

    void processBlock(float** outputs, int numFrames) {
        // SAFE: Bounds known at compile time
        for (int i = 0; i < kMaxChannels; ++i) {
            outputs[i][0] *= gainCoefficients_[i];
        }
    }
};
```

**Advantages:**

- No runtime allocation
- Bounds checking in debug builds
- Size known at compile time

---

### 1.4 Placement New (Advanced)

**Pattern:** Construct object in pre-allocated memory

```cpp
class AudioClip {
public:
    AudioClip() {
        // Pre-allocate memory pool in constructor
        clipStorage_ = malloc(kMaxClips * sizeof(Clip));
    }

    ~AudioClip() {
        // Clean up in destructor
        free(clipStorage_);
    }

    void createClip(int id) {
        // SAFE: Placement new on pre-allocated memory
        Clip* clip = new (clipStorage_ + id * sizeof(Clip)) Clip(id);
    }

private:
    void* clipStorage_;
};
```

**Warning:** Requires manual lifetime management. Use only if necessary.

---

## Category 2: Inter-Thread Communication

### 2.1 Atomic Variables

**Pattern:** Lock-free communication for simple types

```cpp
class TransportController {
public:
    // UI thread
    void setPlaying(bool playing) {
        isPlaying_.store(playing, std::memory_order_release);
    }

    void setGain(float gain) {
        targetGain_.store(gain, std::memory_order_release);
    }

    // Audio thread
    void processBlock(float** outputs, int numFrames) {
        // SAFE: Lock-free atomic load
        bool playing = isPlaying_.load(std::memory_order_acquire);
        float gain = targetGain_.load(std::memory_order_acquire);

        if (playing) {
            applyGain(outputs, numFrames, gain);
        }
    }

private:
    std::atomic<bool> isPlaying_{false};
    std::atomic<float> targetGain_{1.0f};
};
```

**Safe Types for std::atomic:**

- `bool`, `int`, `float`, `double`
- Pointers (`T*`)
- Small POD structs (≤16 bytes on most platforms)

**Memory Ordering:**

- `memory_order_relaxed` - No ordering (counters, flags)
- `memory_order_acquire/release` - Producer-consumer (most common)
- `memory_order_seq_cst` - Sequentially consistent (safest, default)

---

### 2.2 Atomic Flags for State Machines

**Pattern:** Simple state synchronization

```cpp
class AudioEngine {
public:
    // UI thread
    void triggerLoad() {
        loadRequested_.store(true, std::memory_order_release);
    }

    // Audio thread
    void processBlock(float** outputs, int numFrames) {
        // SAFE: Atomic exchange (get and clear in one operation)
        if (loadRequested_.exchange(false, std::memory_order_acq_rel)) {
            // Handle load request (safe part)
            prepareNextBuffer();
        }
    }

private:
    std::atomic<bool> loadRequested_{false};
};
```

**Pattern:** `exchange()` for test-and-set operations

---

### 2.3 Lock-Free SPSC Queue

**Pattern:** Single-producer-single-consumer queue (bounded size)

```cpp
#include <boost/lockfree/spsc_queue.hpp>

class CommandQueue {
public:
    struct Command {
        enum Type { Play, Stop, SetGain } type;
        float value;
    };

    // UI thread (producer)
    void sendCommand(Command cmd) {
        // SAFE: Lock-free push (bounded queue)
        if (!commandQueue_.push(cmd)) {
            // Queue full, handle error
        }
    }

    // Audio thread (consumer)
    void processCommands() {
        Command cmd;
        // SAFE: Lock-free pop
        while (commandQueue_.pop(cmd)) {
            executeCommand(cmd);
        }
    }

private:
    // Fixed capacity, no allocation
    boost::lockfree::spsc_queue<Command, boost::lockfree::capacity<128>> commandQueue_;
};
```

**Alternatives:**

- `juce::AbstractFifo` (JUCE framework)
- Custom ring buffer implementation

---

### 2.4 Triple Buffering

**Pattern:** Lock-free data updates with eventual consistency

```cpp
class TripleBuffer {
public:
    struct Data {
        float gain[kMaxChannels];
        int tempo;
    };

    // UI thread
    void updateData(const Data& newData) {
        int write = writeBuffer_.load(std::memory_order_relaxed);
        buffers_[write] = newData;

        // Swap write and ready buffers atomically
        int expected = writeBuffer_.load(std::memory_order_relaxed);
        while (!writeBuffer_.compare_exchange_weak(expected, readyBuffer_.load(),
                                                     std::memory_order_release,
                                                     std::memory_order_relaxed)) {
            // Retry if concurrent update
        }
        readyBuffer_.store(expected, std::memory_order_release);
    }

    // Audio thread
    const Data& readData() {
        // Check if new data is ready
        int ready = readyBuffer_.load(std::memory_order_acquire);
        int current = readBuffer_.load(std::memory_order_relaxed);

        if (ready != current) {
            readBuffer_.store(ready, std::memory_order_release);
        }

        return buffers_[readBuffer_.load(std::memory_order_relaxed)];
    }

private:
    Data buffers_[3];
    std::atomic<int> writeBuffer_{0};
    std::atomic<int> readyBuffer_{1};
    std::atomic<int> readBuffer_{2};
};
```

**Use Case:** Updating complex data structures without locks

---

## Category 3: Timing and Synchronization

### 3.1 Sample-Accurate Counters

**Pattern:** 64-bit atomic sample counters

```cpp
class SampleClock {
public:
    // Audio thread
    void advance(int numFrames) {
        // SAFE: Atomic increment
        sampleCount_.fetch_add(numFrames, std::memory_order_relaxed);
    }

    uint64_t getCurrentSample() const {
        return sampleCount_.load(std::memory_order_relaxed);
    }

    // UI thread
    double getSampleTime() const {
        uint64_t samples = sampleCount_.load(std::memory_order_relaxed);
        return static_cast<double>(samples) / sampleRate_;
    }

private:
    std::atomic<uint64_t> sampleCount_{0};
    int sampleRate_{48000};
};
```

**Rule:** Always use integer sample counts in audio thread, convert to time only for UI

---

### 3.2 Fixed-Iteration Loops

**Pattern:** Bounded loops with compile-time or runtime-known bounds

```cpp
void processBlock(float** outputs, int numFrames) {
    // SAFE: numFrames is bounded by kMaxFrames (checked on input)
    for (int frame = 0; frame < numFrames; ++frame) {
        for (int channel = 0; channel < kMaxChannels; ++channel) {
            outputs[channel][frame] = processSample(channel, frame);
        }
    }
}
```

**Key:** Loop bounds must be known and finite (no `while (condition)` with unbounded condition)

---

## Category 4: Data Structures

### 4.1 Fixed-Size Arrays

**Pattern:** Compile-time or constructor-time sized containers

```cpp
class ClipManager {
public:
    ClipManager() {
        // Pre-allocate exact size needed
        clips_.resize(kMaxClips);
    }

    void processClip(int clipIndex) {
        // SAFE: Access pre-allocated element (bounds check in debug)
        clips_[clipIndex].process();
    }

private:
    std::vector<Clip> clips_;  // Resized once in constructor
    static constexpr int kMaxClips = 960;
};
```

---

### 4.2 Lookup Tables

**Pattern:** Pre-computed values for fast lookup

```cpp
class SineOscillator {
public:
    SineOscillator() {
        // Pre-compute sine table
        for (int i = 0; i < kTableSize; ++i) {
            sineTable_[i] = std::sin(2.0 * M_PI * i / kTableSize);
        }
    }

    float getSample(float phase) {
        // SAFE: Table lookup instead of computation
        int index = static_cast<int>(phase * kTableSize) % kTableSize;
        return sineTable_[index];
    }

private:
    static constexpr int kTableSize = 4096;
    std::array<float, kTableSize> sineTable_;
};
```

**Advantage:** O(1) lookup instead of expensive computation

---

## Category 5: Control Flow

### 5.1 Early Returns (Avoid Deep Nesting)

**Pattern:** Fast-path optimization

```cpp
void processBlock(float** outputs, int numFrames) {
    // SAFE: Early return for common case
    if (!isPlaying_.load(std::memory_order_acquire)) {
        // Fast path: silence
        std::memset(outputs[0], 0, numFrames * sizeof(float));
        return;
    }

    // Complex processing only if playing
    // ...
}
```

---

### 5.2 Branch Prediction Hints

**Pattern:** Help compiler optimize hot paths

```cpp
void processBlock(float** outputs, int numFrames) {
    // SAFE: Likely branch hint (C++20)
    if (isPlaying_.load(std::memory_order_acquire)) [[likely]] {
        // Hot path
        processAudio(outputs, numFrames);
    } else {
        // Cold path
        outputSilence(outputs, numFrames);
    }
}
```

**Alternatives:** `__builtin_expect()` (GCC/Clang), `[[unlikely]]` (C++20)

---

## Category 6: Mathematical Operations

### 6.1 SIMD Intrinsics (Platform-Specific)

**Pattern:** Vectorized processing

```cpp
#include <immintrin.h>  // AVX2 intrinsics

void applyGain(float* buffer, int numSamples, float gain) {
    // SAFE: SIMD processing (no allocation, bounded loop)
    __m256 gainVec = _mm256_set1_ps(gain);

    for (int i = 0; i < numSamples; i += 8) {
        __m256 samples = _mm256_loadu_ps(buffer + i);
        samples = _mm256_mul_ps(samples, gainVec);
        _mm256_storeu_ps(buffer + i, samples);
    }
}
```

**Note:** Requires alignment and bounds checking. Use JUCE or other libraries for cross-platform SIMD.

---

### 6.2 Fast Math Functions

**Pattern:** Approximations for expensive operations

```cpp
// Fast approximate reciprocal (1/x)
inline float fastReciprocal(float x) {
    // SAFE: Single instruction on most CPUs
    return _mm_cvtss_f32(_mm_rcp_ss(_mm_set_ss(x)));
}

// Fast approximate sqrt
inline float fastSqrt(float x) {
    return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(x)));
}
```

**Warning:** Validate accuracy requirements before using approximations

---

## Category 7: Error Handling

### 7.1 Error Flags (No Exceptions)

**Pattern:** Communicate errors without throwing

```cpp
class AudioProcessor {
public:
    void processBlock(float** outputs, int numFrames) {
        if (numFrames > kMaxFrames) {
            // SAFE: Set error flag, continue gracefully
            errorFlag_.store(ErrorCode::BufferOverflow, std::memory_order_release);
            numFrames = kMaxFrames;  // Clamp to safe value
        }

        // Continue processing with safe value
        processAudio(outputs, numFrames);
    }

    // UI thread checks for errors
    ErrorCode getLastError() {
        return errorFlag_.exchange(ErrorCode::None, std::memory_order_acq_rel);
    }

private:
    enum class ErrorCode { None, BufferOverflow, Underrun };
    std::atomic<ErrorCode> errorFlag_{ErrorCode::None};
};
```

---

### 7.2 Graceful Degradation

**Pattern:** Continue operating under errors

```cpp
void processBlock(float** outputs, int numFrames) {
    if (!audioDataReady_.load(std::memory_order_acquire)) {
        // SAFE: Output silence instead of crashing
        std::memset(outputs[0], 0, numFrames * sizeof(float));

        // Set flag for UI thread to handle
        underrunFlag_.store(true, std::memory_order_release);
        return;
    }

    // Normal processing
    // ...
}
```

---

## Category 8: Debugging and Profiling

### 8.1 Non-Blocking Diagnostics

**Pattern:** Count events without I/O

```cpp
class PerformanceMonitor {
public:
    void recordDropout() {
        // SAFE: Atomic increment (no I/O)
        dropoutCount_.fetch_add(1, std::memory_order_relaxed);
    }

    void recordProcessTime(uint64_t samples) {
        totalSamples_.fetch_add(samples, std::memory_order_relaxed);
    }

    // UI thread retrieves stats
    uint64_t getDropoutCount() {
        return dropoutCount_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<uint64_t> dropoutCount_{0};
    std::atomic<uint64_t> totalSamples_{0};
};
```

---

### 8.2 Conditional Logging (Debug Only)

**Pattern:** Log to lock-free queue, drain from background thread

```cpp
#ifdef DEBUG_AUDIO_THREAD
    // SAFE: Lock-free queue push (bounded)
    if (debugQueue_.push({LogLevel::Warning, "Buffer underrun"})) {
        // Logged successfully
    }
#endif
```

**Production:** Disable all logging in release builds

---

## Anti-Patterns (Avoid These)

### ❌ Dynamic Allocation in Hot Path

```cpp
// WRONG
void processBlock(...) {
    std::vector<float> temp(numFrames);  // Allocates!
}
```

### ❌ Locks in Audio Thread

```cpp
// WRONG
void processBlock(...) {
    std::lock_guard<std::mutex> lock(mutex_);  // Blocks!
}
```

### ❌ Console I/O

```cpp
// WRONG
void processBlock(...) {
    std::cout << "Processing..." << std::endl;  // Blocks on stderr mutex!
}
```

### ❌ Floating-Point Time

```cpp
// WRONG
void processBlock(...) {
    auto now = std::chrono::steady_clock::now();  // Use sample counts!
}
```

---

## Best Practices Summary

1. **Pre-allocate everything** in constructor
2. **Use atomics** for inter-thread communication
3. **Use sample counts** for timing, not chrono
4. **Bound all loops** at compile time or input validation
5. **Profile worst-case** execution time
6. **Test with sanitizers** (ASan, TSan, UBSan)
7. **Document real-time safety** in comments
8. **Gracefully degrade** on errors, never crash

---

## Verification Checklist

For every audio thread function:

- [ ] No `new`, `delete`, `malloc`, `free`
- [ ] No `std::mutex`, `pthread_mutex_lock`
- [ ] No `std::cout`, `printf`, file I/O
- [ ] No `std::chrono` (use sample counts)
- [ ] All loops have bounded iteration counts
- [ ] All allocations happen in constructor
- [ ] All inter-thread communication uses atomics or lock-free queues
- [ ] Tested with ASan/TSan/UBSan
- [ ] Profiled worst-case execution time

---

## References

- Orpheus SDK Real-Time Constraints (rt_constraints.md)
- Orpheus SDK Banned Functions (banned_functions.md)
- JUCE Framework Real-Time Safe Code Examples
- Boost.Lockfree Documentation
- C++ Concurrency in Action (Anthony Williams)
