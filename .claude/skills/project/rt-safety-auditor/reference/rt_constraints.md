# Orpheus SDK Real-Time Constraints

**Version:** 1.0
**Last Updated:** 2025-10-18
**Authority:** Orpheus SDK Core Principles (CLAUDE.md)

---

## Executive Summary

Orpheus SDK enforces strict real-time safety requirements to guarantee broadcast-safe performance:

- **24/7 reliability** (>100 hour mean time between failures)
- **<5ms latency** (ASIO/CoreAudio round-trip)
- **Zero audio dropouts** (no buffer underruns)
- **Sample-accurate timing** (bit-identical output across platforms)
- **Deterministic behavior** (same input → same output, always)

These constraints apply to **all code executed in audio threads**.

---

## Audio Thread Definition

**Audio thread** = Any function called from:

- `processBlock()` - Main DSP processing callback
- `audioCallback()` - Platform audio driver callback (CoreAudio, ASIO, WASAPI)
- `render()` - Offline rendering path
- Transport control methods (`play()`, `stop()`, `seek()`)
- Real-time mixer/router processing

**Non-audio threads** = Everything else:

- UI thread (GUI updates, user interaction)
- File I/O thread (loading audio files, saving sessions)
- Network thread (future: OSC control, remote sync)

**Rule:** Audio thread code must NEVER call into non-audio thread code that violates real-time constraints.

---

## Core Constraints

### 1. No Heap Allocations

**Forbidden:**

- `new`, `delete` operators
- `malloc()`, `calloc()`, `realloc()`, `free()`
- `std::make_shared()`, `std::make_unique()`
- `std::vector::push_back()` (may allocate)
- `std::string` operations that allocate
- Throwing exceptions (allocates on throw)

**Rationale:**

- Heap allocation triggers system calls (unbounded latency)
- May cause page faults (milliseconds delay)
- Memory allocator locks (contention with other threads)
- Non-deterministic execution time

**Fix:**

- Pre-allocate in constructor: `buffer_.reserve(kMaxSize);`
- Use stack allocation: `float samples[kMaxFrames];`
- Use fixed-size containers: `std::array<T, N>`
- Use placement new on pre-allocated memory (advanced)

---

### 2. No Locks or Blocking Primitives

**Forbidden:**

- `std::mutex`, `std::recursive_mutex`
- `std::lock_guard`, `std::unique_lock`, `std::scoped_lock`
- `pthread_mutex_lock()`, `pthread_mutex_unlock()`
- `std::condition_variable` (blocks)
- `std::barrier`, `std::latch` (blocks)
- Spinlocks (waste CPU, no priority inheritance)

**Rationale:**

- Locks cause priority inversion (audio thread waits for low-priority thread)
- Blocking operations have unbounded wait time
- Deadlock risk (catastrophic failure)

**Fix:**

- Use `std::atomic<T>` for simple values
- Use lock-free single-producer-single-consumer (SPSC) queues
- Use triple-buffering for complex data structures
- Communicate via wait-free atomic flags

**Example (correct):**

```cpp
// UI thread writes, audio thread reads
std::atomic<float> targetGain_{1.0f};

// Audio thread
void processBlock(float** outputs, int numFrames) {
    float gain = targetGain_.load(std::memory_order_acquire);
    // ... use gain
}

// UI thread
void setGain(float newGain) {
    targetGain_.store(newGain, std::memory_order_release);
}
```

---

### 3. No Blocking I/O

**Forbidden:**

- `std::cout`, `std::cerr`, `printf()`, `fprintf()`
- `fopen()`, `fread()`, `fwrite()`, `fclose()`
- `std::ifstream`, `std::ofstream`
- File system operations (open, read, write, stat)
- Network I/O (sockets, HTTP)
- Database queries

**Rationale:**

- I/O operations block waiting for kernel
- Disk I/O may take milliseconds (or seconds if disk sleeps)
- Network I/O has unbounded latency
- Console output locks (stderr/stdout mutex)

**Fix:**

- Log to lock-free queue, drain from background thread
- Pre-load audio files before entering audio thread
- Stream file data from dedicated I/O thread via lock-free queue
- Never write logs in audio thread (use post-processing diagnostics)

---

### 4. No Unbounded Operations

**Forbidden:**

- Loops with runtime-determined iteration count
- Recursive algorithms without depth limit
- Searching unsorted data structures
- Dynamic dispatch with unbounded call chains
- String operations (strlen, strcmp on unbounded strings)

**Rationale:**

- Execution time must be predictable
- O(n) algorithms with large n cause latency spikes
- Real-time deadlines require worst-case bounds

**Fix:**

- Use fixed iteration counts: `for (int i = 0; i < kMaxFrames; ++i)`
- Pre-sort data structures in initialization
- Use lookup tables instead of computation
- Profile and verify worst-case execution time

---

### 5. Sample-Accurate Timing Only

**Forbidden:**

- `std::chrono` for time measurement
- Floating-point time calculations (seconds, milliseconds)
- `std::this_thread::sleep_for()` (blocking)
- Wall-clock time (`time()`, `gettimeofday()`)

**Rationale:**

- Floating-point has rounding errors (non-deterministic)
- Clock drift causes timing errors
- Sample counts are the authority (bit-exact)

**Fix:**

- Use 64-bit sample counters: `std::atomic<uint64_t> sampleCount_;`
- Convert time to samples: `samples = seconds * sampleRate`
- Always use integer arithmetic for sample positions
- Use `std::atomic<uint64_t>` for cross-thread communication

**Example (correct):**

```cpp
class TransportController {
    std::atomic<uint64_t> playheadSamples_{0};
    int sampleRate_{48000};

    void processBlock(float** outputs, int numFrames) {
        uint64_t startSample = playheadSamples_.load(std::memory_order_relaxed);
        // ... process numFrames samples
        playheadSamples_.fetch_add(numFrames, std::memory_order_relaxed);
    }
};
```

---

## Atomic Memory Ordering Guidelines

**Use Cases:**

- **`memory_order_relaxed`**: Simple counters, no ordering requirements

  ```cpp
  sampleCount_.fetch_add(1, std::memory_order_relaxed);
  ```

- **`memory_order_acquire` / `memory_order_release`**: Producer-consumer patterns

  ```cpp
  // Producer (UI thread)
  data_ = newValue;
  ready_.store(true, std::memory_order_release);

  // Consumer (audio thread)
  if (ready_.load(std::memory_order_acquire)) {
      use(data_);
  }
  ```

- **`memory_order_seq_cst`**: Default (safest, slightly slower)
  ```cpp
  flag_.store(true); // Equivalent to memory_order_seq_cst
  ```

**Rule:** When in doubt, use `memory_order_seq_cst`. Optimize only after profiling.

---

## Allowed Patterns

### Lock-Free Data Structures

**Safe:**

- `std::atomic<T>` for primitive types (int, float, bool, pointers)
- `std::atomic_flag` for spin-wait-free flags
- Single-producer-single-consumer (SPSC) queues (bounded size)
- Triple-buffering (read buffer, write buffer, ready buffer)
- RCU (Read-Copy-Update) for rarely changing data

**Libraries:**

- `juce::AbstractFifo` (JUCE framework, lock-free FIFO)
- `boost::lockfree::spsc_queue` (Boost, bounded SPSC queue)
- Custom implementations (must be proven wait-free or lock-free)

### Pre-Allocated Buffers

**Safe:**

- `std::array<T, N>` (compile-time size)
- `T buffer[N];` (stack allocation)
- Pre-allocated `std::vector` with `reserve()` (never push_back beyond capacity)
- Placement new on pre-allocated memory (advanced)

### Compile-Time Constants

**Safe:**

- `constexpr` values
- `static constexpr int kMaxFrames = 2048;`
- Template parameters: `std::array<float, kBufferSize>`

---

## Verification Methods

### Static Analysis

- `cppcheck` - Detects some allocations and locks
- `clang-tidy` - Custom checks for real-time violations
- `grep` - Pattern matching for banned functions

### Runtime Verification

- **AddressSanitizer (ASan)** - Detects heap allocations
- **ThreadSanitizer (TSan)** - Detects data races, lock contention
- **UndefinedBehaviorSanitizer (UBSan)** - Detects undefined behavior
- **Real-time profiling** - Measure worst-case execution time

### Testing Strategy

1. Run all tests with ASan/TSan/UBSan enabled
2. Profile audio thread under maximum load (16+ simultaneous clips)
3. Stress test with buffer sizes 32-2048 frames
4. Verify no allocations/locks during 1-hour continuous playback
5. Test with real-time kernel (Linux RT-PREEMPT)

---

## Common Mistakes

### Mistake 1: Hidden Allocations in STL

**Wrong:**

```cpp
std::string clipName = "Clip " + std::to_string(id); // Allocates!
```

**Right:**

```cpp
// Pre-allocate in constructor
char clipName_[32];
snprintf(clipName_, sizeof(clipName_), "Clip %d", id);
```

### Mistake 2: Conditional Allocations

**Wrong:**

```cpp
if (needsResampling) {
    resampleBuffer_ = new float[size]; // Only sometimes allocates (bad!)
}
```

**Right:**

```cpp
// Pre-allocate worst-case in constructor
resampleBuffer_.resize(kMaxResampleSize);
```

### Mistake 3: Logging in Audio Thread

**Wrong:**

```cpp
void processBlock(...) {
    if (error) {
        std::cerr << "Error!" << std::endl; // Blocks on stderr mutex!
    }
}
```

**Right:**

```cpp
// Set atomic flag, log from background thread
std::atomic<bool> errorFlag_{false};

void processBlock(...) {
    if (error) {
        errorFlag_.store(true, std::memory_order_release);
    }
}

void backgroundThread() {
    if (errorFlag_.exchange(false, std::memory_order_acquire)) {
        std::cerr << "Error detected in audio thread!" << std::endl;
    }
}
```

---

## Platform-Specific Notes

### macOS (CoreAudio)

- Audio thread priority: `THREAD_TIME_CONSTRAINT_POLICY`
- Buffer sizes: 32-2048 frames typical
- Latency: <5ms achievable with 128 frame buffer at 48kHz

### Windows (ASIO)

- Audio thread priority: `THREAD_PRIORITY_TIME_CRITICAL`
- Buffer sizes: 64-2048 frames typical
- ASIO driver quality varies (some drivers are not truly real-time)

### Linux (ALSA/JACK)

- Requires RT-PREEMPT kernel for true real-time
- Audio thread priority: `SCHED_FIFO` with high priority
- Buffer sizes: 64-2048 frames typical

---

## Enforcement

**Pre-commit Checks:**

- `rt.safety.auditor` skill runs on all audio thread code
- CI pipeline fails if violations detected

**Code Review:**

- All audio thread code requires peer review
- Real-time safety must be explicitly verified

**Testing:**

- All audio thread code must pass sanitizer tests (ASan, TSan, UBSan)
- Performance tests verify <5ms latency under maximum load

---

## References

- [Orpheus SDK CLAUDE.md](../../../CLAUDE.md) - Core principles
- JUCE Framework Real-Time Safety Guide [1]
- Ross Bencina - "Real-time audio programming 101" [2]
- Timur Doumler - "Want fast C++? Know your hardware!" [3]

[1] https://juce.com/
[2] http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing
[3] https://www.youtube.com/watch?v=BP6NxVxDQIs
