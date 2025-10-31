# Agent: Waveform Optimization

## Purpose

Profile and optimize waveform rendering performance in WaveformDisplay component. Identifies bottlenecks and suggests improvements for generation time and paint latency.

## Triggers

- Waveform generation takes >200ms
- UI feels sluggish when opening clip edit dialog
- User reports slow waveform rendering
- Need to benchmark downsampling algorithms
- User explicitly requests: "Optimize waveform performance" or "Profile waveform rendering"

## Process

### 1. Baseline Performance Measurement

Read current implementation and identify metrics:

- Generation time (background thread)
- Paint latency (UI thread)
- Memory usage per waveform
- File size impact on performance

**Files to analyze:**

- `Source/UI/WaveformDisplay.h`
- `Source/UI/WaveformDisplay.cpp`
- `Source/UI/ClipEditDialog.cpp` (dialog integration)

### 2. Identify Bottlenecks

**Common issues:**

- Reading entire audio file into memory (should downsample)
- Processing on UI thread instead of background thread
- Inefficient min/max tracking algorithm
- Too many samples per pixel (should be 1-2 samples per pixel column)
- Blocking file I/O without async loading
- Excessive repainting without dirty regions

### 3. Profile Code Paths

Use instrumentation or manual timing:

```cpp
auto start = std::chrono::high_resolution_clock::now();
// ... code to profile ...
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
DBG("Operation took: " << duration << "ms");
```

**Target areas:**

- `generateWaveformData()` - Should be <200ms for typical audio files
- `paint()` - Should be <1ms for smooth 60fps UI
- `setAudioFile()` - Should launch background thread immediately
- `setTrimPoints()` - Should only trigger repaint, no recomputation

### 4. Optimization Strategies

#### A. Reduce File I/O

- Stream audio data in chunks (e.g., 64KB blocks)
- Use memory mapping for large files
- Cache waveform data in session

#### B. Optimize Downsampling

- Target resolution: Component width in pixels (e.g., 800px)
- Samples per pixel: `totalSamples / targetWidth`
- Min/max tracking: One pair per pixel column
- Skip samples intelligently (don't read every sample for long files)

#### C. Background Threading

- Always generate waveform on background thread
- Use `juce::Thread::launch()` or `juce::ThreadPool`
- Post completion via `juce::MessageManager::callAsync()`
- Show loading indicator during generation

#### D. Caching

- Store waveform data in `SessionManager::ClipData`
- Serialize to session JSON for instant reload
- Use dirty flag to avoid regeneration

#### E. Paint Optimization

- Pre-render waveform to `juce::Image` once
- Cache `juce::Image` and only repaint on trim point changes
- Use dirty regions: `repaint(bounds)` instead of `repaint()`
- Draw trim markers separately (cheaper than full waveform redraw)

### 5. Implement Improvements

Based on profiling results, implement top 2-3 optimizations:

1. If generation >200ms: Improve downsampling algorithm
2. If paint >5ms: Cache waveform as Image
3. If memory excessive: Reduce data structure size

### 6. Benchmark Results

After optimizations, measure again:

- Generation time: Target <100ms (was 50-200ms in v0.1.0)
- Paint latency: Target <1ms (was <1ms in v0.1.0, maintain)
- Memory per waveform: Target <5KB (was ~3.2KB in v0.1.0, maintain)

### 7. Report Findings

**Format:**

```
Waveform Optimization Report
============================
File: [audio_file.wav, 5 minutes, 48kHz stereo]

BEFORE:
- Generation: 180ms
- Paint: 0.8ms
- Memory: 3.2KB

OPTIMIZATIONS APPLIED:
1. [Optimization 1 description]
2. [Optimization 2 description]

AFTER:
- Generation: 90ms (-50%)
- Paint: 0.6ms (-25%)
- Memory: 3.2KB (unchanged)

RECOMMENDATION: [Further improvements or "Performance acceptable"]
```

## Success Criteria

- [x] Baseline performance measured
- [x] Bottlenecks identified
- [x] At least 2 optimizations implemented
- [x] Performance improved by ≥25% OR verified already optimal
- [x] No regressions in visual quality or accuracy
- [x] Code remains maintainable and readable

## Tools Required

- Read (for analyzing source files)
- Edit (for implementing optimizations)
- Bash (for building and testing)
- Grep (for finding usage patterns)

## Error Handling

### Can't Measure Performance

**Symptom:** No timing logs or insufficient data
**Action:**

1. Add instrumentation: `DBG()` statements with timestamps
2. Test with variety of file sizes (1min, 5min, 30min audio)
3. Use consistent test files for comparison

### Optimization Breaks Functionality

**Symptom:** Waveform renders incorrectly after changes
**Action:**

1. Revert optimization
2. Add unit test for correctness
3. Re-implement with validation checks

### Performance Still Poor

**Symptom:** After optimizations, still >200ms generation
**Action:**

1. Check for I/O bottlenecks (slow disk, network drive)
2. Profile with Instruments (macOS) or Visual Studio Profiler
3. Consider pre-computing waveforms on file load (not in dialog)

---

## Example Usage

**User:** "The waveform display feels slow when I open the edit dialog. Can you optimize it?"

**Agent Response:**

1. Reads WaveformDisplay.cpp and measures current performance
2. Identifies: "Generation taking 180ms for 5min audio file"
3. Implements: Improved downsampling (skip samples intelligently)
4. Tests: "Generation now 90ms (-50%), paint latency <1ms (unchanged)"
5. Reports: "Optimization complete. Waveform generation 50% faster. No visual quality loss."

---

**Last Updated:** October 22, 2025
**Version:** 1.0
**Compatible with:** v0.1.0-alpha and later
**Baseline:** 50-200ms generation, <1ms paint, ~3.2KB memory (v0.1.0-alpha)
