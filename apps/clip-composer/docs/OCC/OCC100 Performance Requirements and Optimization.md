# OCC100 - Performance Requirements and Optimization

**Version:** 1.0
**Date:** 2025-10-30
**Status:** Reference Documentation

Complete performance requirements, benchmarks, and optimization guidelines for Orpheus Clip Composer.

---

## Overview

OCC is a professional broadcast tool. Performance is not optional—it's a core requirement. This document defines measurable targets and optimization strategies.

**Source:** OCC026 (Milestone 1 MVP Definition)

---

## Performance Requirements

### Latency (Critical)

| Metric                    | Target | Maximum | Test Conditions                  |
| ------------------------- | ------ | ------- | -------------------------------- |
| Round-trip latency        | <5ms   | 10ms    | ASIO driver, 512 samples @ 48kHz |
| Button click to audio     | <10ms  | 20ms    | Includes UI processing time      |
| Transport command latency | <1ms   | 2ms     | Lock-free command processing     |

**Measurement:**

```cpp
auto start = std::chrono::high_resolution_clock::now();
transportController->startClip(handle);
auto end = std::chrono::high_resolution_clock::now();
auto latency = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
```

---

### CPU Usage

| Scenario              | Target | Maximum | Hardware Baseline          |
| --------------------- | ------ | ------- | -------------------------- |
| Idle (no clips)       | <5%    | 10%     | Intel i5 8th gen (4 cores) |
| 4 simultaneous clips  | <15%   | 20%     | Intel i5 8th gen (4 cores) |
| 16 simultaneous clips | <30%   | 40%     | Intel i5 8th gen (4 cores) |
| 32 simultaneous clips | <60%   | 80%     | Intel i5 8th gen (4 cores) |

**Measurement:**

- Use OS-level tools: Activity Monitor (macOS), Task Manager (Windows)
- Profiling: Instruments (macOS), Visual Studio Profiler (Windows)
- Audio callback time budget: 10.67ms @ 512 samples, 48kHz

---

### Clip Capacity

| Metric                          | Target | Maximum              |
| ------------------------------- | ------ | -------------------- |
| Total clips loaded              | 960    | 960                  |
| Session load time               | <2s    | 5s                   |
| Waveform render time (per clip) | <100ms | 500ms                |
| UI responsiveness (tab switch)  | <16ms  | 33ms (60fps / 30fps) |

---

### Memory Usage

| State                            | Target  | Maximum             |
| -------------------------------- | ------- | ------------------- |
| Baseline (empty session)         | <50 MB  | 100 MB              |
| 960 clips loaded (metadata only) | <200 MB | 500 MB              |
| After 1 hour operation           | Stable  | +10% growth allowed |

**Measurement:**

```bash
# macOS
/usr/bin/leaks <pid>
/usr/bin/heap <pid>

# Windows
Performance Monitor (perfmon.msc) → Private Bytes
```

---

### File Format Support

| Format | Sample Rates          | Bit Depths               | Channels |
| ------ | --------------------- | ------------------------ | -------- |
| WAV    | 44.1, 48, 96, 192 kHz | 16, 24, 32-bit int/float | 1-32     |
| AIFF   | 44.1, 48, 96, 192 kHz | 16, 24, 32-bit int/float | 1-32     |
| FLAC   | 44.1, 48, 96, 192 kHz | 16, 24-bit               | 1-8      |

**Unsupported (MVP):** MP3, AAC, Ogg Vorbis (lossy formats deferred to v1.0+)

---

### Stability

| Metric                            | Target     | Test Method                               |
| --------------------------------- | ---------- | ----------------------------------------- |
| MTBF (Mean Time Between Failures) | >100 hours | 24-hour stress test                       |
| Crash-free sessions               | >99%       | Beta testing (10 users, 1+ hour sessions) |
| Audio dropouts (buffer underruns) | 0 per hour | Continuous monitoring                     |

---

## Optimization Guidelines

### General Principles

1. **Pre-allocate everything** - No allocations in audio thread
2. **Lock-free communication** - Use atomics, not mutexes
3. **Background I/O** - Never block audio or message thread
4. **Profile first, optimize second** - Measure before changing

---

### Audio Thread Optimization

**Rules (enforced by code review + sanitizers):**

- ⛔ NO allocations (verified with AddressSanitizer)
- ⛔ NO locks (verified with ThreadSanitizer)
- ⛔ NO I/O (file reads, network calls, logging to file)
- ⛔ NO UI calls (JUCE components, `repaint()`, etc.)

**Allowed:**

- ✅ Lock-free atomics (`std::atomic`)
- ✅ Lock-free queues (JUCE `LockFreeQueue` or similar)
- ✅ Pre-allocated buffers
- ✅ Sample-accurate arithmetic (64-bit integers, no floats for time)

**Example (Good):**

```cpp
void AudioEngine::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    // Process audio (lock-free, pre-allocated)
    float* outputs[2] = { buffer.getWritePointer(0), buffer.getWritePointer(1) };
    transportController->processAudio(outputs, 2, buffer.getNumSamples());

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    // Store atomically (lock-free)
    cpuUsageMicroseconds.store(duration.count(), std::memory_order_relaxed);
}
```

**Example (Bad):**

```cpp
void AudioEngine::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // ❌ BAD: File I/O on audio thread!
    auto file = juce::File("/path/to/clip.wav");
    juce::AudioFormatReader* reader = formatManager.createReaderFor(file);

    // ❌ BAD: Allocation on audio thread!
    std::vector<float> tempBuffer;
    tempBuffer.resize(buffer.getNumSamples());

    // ❌ BAD: Mutex lock on audio thread!
    std::lock_guard<std::mutex> lock(clipStateMutex);

    // This will cause audio dropouts and non-deterministic latency!
}
```

---

### UI Thread Optimization

**Goal:** <16ms frame time (60 fps)

**Strategies:**

1. **Pre-render waveforms on background thread**

   ```cpp
   ThreadPool::getInstance()->addJob([this, reader, metadata]() {
       renderWaveform(reader, metadata);
       juce::MessageManager::callAsync([this]() { repaint(); });
   });
   ```

2. **Use juce::Grid for responsive layout**
   - Avoids manual layout calculations
   - JUCE optimizes grid rendering

3. **Throttle UI updates**
   - Update position display at 30 Hz, not 60 Hz
   - Update CPU meter at 10 Hz

4. **Batch repaints**
   - Don't repaint individual buttons, repaint entire grid once

---

### Session Loading Optimization

**Goal:** <2 seconds to load 960 clips

**Strategies:**

1. **Parse JSON once, load clips in parallel**

   ```cpp
   void SessionManager::loadSession(const juce::File& sessionFile)
   {
       auto json = juce::JSON::parse(sessionFile);  // ~100ms for 960 clips

       // Load clips in parallel (background threads)
       ThreadPool pool(8);  // 8 worker threads
       for (int i = 0; i < clipsArray.size(); ++i) {
           pool.addJob([this, clipJson = clipsArray[i]]() {
               loadClipFromJson(clipJson);
           });
       }
       pool.waitForAll();  // ~1 second total

       // Update UI on message thread
       clipGrid->refresh();
   }
   ```

2. **Defer waveform rendering**
   - Load metadata first (fast)
   - Render waveforms lazily (when tab is opened)

3. **Cache file handles**
   - Keep `IAudioFileReader` instances in memory
   - Avoids re-opening files during playback

---

### Memory Optimization

**Strategies:**

1. **Use fixed-size buffers**
   - Pre-allocate worst-case (960 clips × 32 channels × 4 bytes/sample × 1024 samples)
   - Avoid dynamic growth

2. **Shared waveform data**
   - Multiple buttons can reference same waveform
   - Copy-on-write semantics

3. **Release unused resources**
   - When switching tabs, unload inactive waveforms
   - Keep metadata, release pixel buffers

---

## Profiling Tools

### macOS: Instruments

```bash
# Launch OCC under Instruments
instruments -t "Time Profiler" /path/to/OCC.app

# Common templates:
# - Time Profiler (CPU hotspots)
# - Allocations (memory usage)
# - Leaks (memory leaks)
# - System Trace (I/O, threading)
```

**Key Metrics:**

- Audio callback time (should be <10ms)
- UI frame time (should be <16ms)
- Allocations on audio thread (should be 0)

---

### Windows: Visual Studio Profiler

```bash
# Launch OCC under profiler
vsperf /launch:OCC.exe

# Profiling modes:
# - CPU Sampling (hotspots)
# - Instrumentation (call graph)
# - Memory Usage (allocations)
```

**Key Metrics:**

- Audio thread CPU time (should be <30%)
- UI thread CPU time (should be <10%)
- GC pauses (should be 0 in C++ app)

---

### Linux: perf (future)

```bash
# Record performance data
perf record -g ./occ

# Analyze
perf report
```

---

## Performance Benchmarks

### Baseline Tests (CI/CD)

Run on every commit:

```bash
# Latency benchmark
./build/tests/benchmarks/latency_benchmark
# Target: <1ms for startClip()

# CPU benchmark (16 clips)
./build/tests/benchmarks/cpu_benchmark
# Target: <30% CPU on reference hardware

# Session load benchmark
./build/tests/benchmarks/session_load_benchmark
# Target: <2 seconds for 960 clips
```

---

### Stress Tests (Nightly)

Run nightly on build servers:

1. **24-Hour Stability Test**
   - Randomly trigger clips for 24 hours
   - Monitor memory usage (should be stable)
   - Target: 0 crashes, <10% memory growth

2. **960-Clip Load Test**
   - Load session with 960 clips
   - Trigger all 960 clips sequentially
   - Verify no dropouts, no crashes

3. **Rapid-Fire Test**
   - Click 100 buttons in 10 seconds
   - Verify UI remains responsive
   - Verify audio processing continues

---

## Hardware Recommendations

### Minimum Specifications (MVP)

| Component       | Specification                                  |
| --------------- | ---------------------------------------------- |
| CPU             | Intel i5 8th gen (4 cores) or AMD Ryzen 5 3600 |
| RAM             | 8 GB                                           |
| Storage         | SSD (for fast session loading)                 |
| Audio Interface | ASIO-compatible (Windows), CoreAudio (macOS)   |
| OS              | Windows 10/11, macOS 11+ (Big Sur)             |

### Recommended Specifications

| Component       | Specification                                    |
| --------------- | ------------------------------------------------ |
| CPU             | Intel i7 10th gen (8 cores) or AMD Ryzen 7 5800X |
| RAM             | 16 GB                                            |
| Storage         | NVMe SSD                                         |
| Audio Interface | Professional ASIO interface (<5ms latency)       |
| OS              | Windows 11, macOS 14+ (Sonoma)                   |

---

## Performance Regression Detection

### CI/CD Integration

```yaml
# .github/workflows/performance-test.yml
name: Performance Tests

on:
  pull_request:
    branches: [main]

jobs:
  benchmark:
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v3

      - name: Run Benchmarks
        run: |
          cmake --build build
          ./build/tests/benchmarks/latency_benchmark --benchmark_format=json > results.json

      - name: Compare Results
        run: |
          # Compare against baseline (main branch)
          python scripts/compare_benchmarks.py results.json baseline.json
          # Fail if >10% regression
```

---

## Optimization Checklist

Before releasing any OCC version, verify:

- [ ] Latency: <5ms round-trip (measured with ASIO loopback)
- [ ] CPU: <30% with 16 simultaneous clips (measured on reference hardware)
- [ ] Session load: <2 seconds for 960 clips
- [ ] Memory: Stable after 1 hour operation (no leaks)
- [ ] UI responsiveness: 60 fps (measured with JUCE profiler)
- [ ] Audio dropouts: 0 per hour (measured with 24-hour test)
- [ ] Profiling: No allocations on audio thread (verified with AddressSanitizer)
- [ ] Thread safety: No data races (verified with ThreadSanitizer)

---

## Related Documentation

- **OCC026** - MVP Definition (original performance requirements)
- **OCC096** - SDK Integration Patterns (real-time code examples)
- **OCC099** - Testing Strategy (performance benchmarks)
- **OCC101** - Troubleshooting (performance issues and fixes)

---

**Last Updated:** 2025-10-30
**Maintainer:** OCC Development Team
**Status:** Reference Documentation
