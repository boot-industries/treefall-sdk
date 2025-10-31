# OCC101 - Troubleshooting Guide

**Version:** 1.0
**Date:** 2025-10-30
**Status:** Reference Documentation

Complete troubleshooting guide for common build, runtime, and performance issues in Orpheus Clip Composer.

---

## Overview

This guide helps diagnose and fix common issues during OCC development and deployment.

---

## Build Issues

### Issue: SDK Headers Not Found

**Problem:**

```
fatal error: 'orpheus/transport_controller.h' file not found
```

**Cause:** CMake cannot find Orpheus SDK headers

**Solution:**

1. **Ensure SDK is built:**

   ```bash
   cd /Users/chrislyons/dev/orpheus-sdk
   cmake --build build
   # Verify install includes are correct
   ls build/include/orpheus/
   ```

2. **Update CMake cache:**

   ```bash
   cd /Users/chrislyons/dev/orpheus-sdk/apps/clip-composer
   rm -rf build
   cmake -S . -B build -DORPHEUS_SDK_PATH=/path/to/orpheus-sdk/build
   cmake --build build
   ```

3. **Check CMakeLists.txt:**
   ```cmake
   # Verify this line exists:
   find_package(OrpheusSDK REQUIRED)
   ```

---

### Issue: Linker Errors with libsndfile

**Problem:**

```
Undefined symbols for architecture x86_64:
  "_sf_open", referenced from:
      orpheus::AudioFileReader::open() in liborpheus.a
```

**Cause:** libsndfile not installed or not linked

**Solution:**

1. **Install libsndfile:**

   ```bash
   # macOS
   brew install libsndfile

   # Windows (vcpkg)
   vcpkg install libsndfile:x64-windows

   # Linux (Ubuntu)
   sudo apt-get install libsndfile1-dev
   ```

2. **Rebuild SDK:**

   ```bash
   cd /Users/chrislyons/dev/orpheus-sdk
   rm -rf build
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   cmake --build build
   ```

3. **Verify linkage:**

   ```bash
   # macOS
   otool -L build/liborpheus.dylib | grep sndfile

   # Linux
   ldd build/liborpheus.so | grep sndfile
   ```

---

### Issue: JUCE Modules Not Found

**Problem:**

```
CMake Error: Could not find JuceConfig.cmake
```

**Cause:** JUCE framework not installed or not in CMake search path

**Solution:**

1. **Set JUCE_PATH environment variable:**

   ```bash
   export JUCE_PATH=/path/to/JUCE
   ```

2. **OR update CMakeLists.txt:**

   ```cmake
   set(JUCE_PATH "/Applications/JUCE" CACHE PATH "JUCE framework location")
   list(APPEND CMAKE_PREFIX_PATH "${JUCE_PATH}")
   ```

3. **Download JUCE:**
   - Visit https://juce.com/
   - Download JUCE 7.x (latest stable)
   - Extract to `/Applications/JUCE` (macOS) or `C:\JUCE` (Windows)

---

### Issue: C++20 Compiler Not Supported

**Problem:**

```
CMake Error: C++ compiler does not support C++20
```

**Cause:** Compiler too old

**Solution:**

Update compiler to minimum version:

- **macOS:** Xcode 14+ (Apple Clang 14+)

  ```bash
  xcode-select --install
  ```

- **Windows:** Visual Studio 2019 16.11+ or Visual Studio 2022
  - Download from https://visualstudio.microsoft.com/

- **Linux:** GCC 11+ or Clang 13+
  ```bash
  # Ubuntu 22.04+
  sudo apt-get install g++-11
  export CXX=g++-11
  ```

---

## Runtime Issues

### Issue: Audio Dropouts (Buffer Underruns)

**Problem:** Clicks, pops, or silence during playback

**Symptoms:**

- Audible clicks/pops
- Console warnings: "Audio buffer underrun detected"
- CPU usage spikes to 100%

**Diagnosis:**

```cpp
// Check audio callback time
void AudioEngine::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    auto start = std::chrono::high_resolution_clock::now();

    // Process audio...

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    if (duration.count() > 10000) {  // 10ms budget @ 512 samples, 48kHz
        std::cerr << "Audio callback too slow: " << duration.count() << "μs" << std::endl;
    }
}
```

**Solutions:**

1. **Increase buffer size:**
   - Open audio settings
   - Change buffer size: 512 → 1024 samples
   - Trade-off: Higher latency, fewer dropouts

2. **Reduce clip count:**
   - Limit simultaneous clips to 16 (MVP target)
   - Clips beyond 16 may cause dropouts on slower hardware

3. **Profile audio callback:**

   ```bash
   # macOS
   instruments -t "Time Profiler" /path/to/OCC.app
   # Focus on audio thread

   # Windows
   vsperf /launch:OCC.exe
   # Filter by audio thread
   ```

4. **Check for audio thread violations:**
   - File I/O on audio thread (use Read tool, search for `open()`, `read()` in audio callback)
   - Allocations on audio thread (run with AddressSanitizer)
   - Mutex locks on audio thread (run with ThreadSanitizer)

---

### Issue: High CPU Usage

**Problem:** CPU usage >50% with only 8 clips playing

**Diagnosis:**

```bash
# macOS: Sample process for 10 seconds
sample OCC 10 -file cpu_profile.txt

# Windows: Performance Monitor
perfmon.msc
# Add counter: Process → % Processor Time → OCC.exe
```

**Common Causes:**

1. **Waveform rendering on audio thread**
   - **Fix:** Move to background thread (see OCC098 UI Components)

2. **UI updates blocking audio thread**
   - **Fix:** Use `juce::MessageManager::callAsync()`

3. **Inefficient file I/O**
   - **Fix:** Pre-load audio files, cache file handles

4. **Too many simultaneous clips**
   - **Fix:** Limit to 16 clips for MVP

**Profiling Steps:**

```bash
# macOS: Instruments Time Profiler
instruments -t "Time Profiler" /path/to/OCC.app

# Look for hotspots:
# - processBlock() should be <30% total CPU
# - UI rendering should be <10% total CPU
```

---

### Issue: Clips Not Starting (startClip() Fails)

**Problem:** User clicks button, no sound, no error displayed

**Diagnosis:**

```cpp
auto result = transportController->startClip(handle);
if (result != orpheus::SessionGraphError::OK) {
    std::cerr << "Error: " << static_cast<int>(result) << std::endl;
}
```

**Common Error Codes:**

| Error Code             | Cause                            | Solution                   |
| ---------------------- | -------------------------------- | -------------------------- |
| `INVALID_HANDLE`       | Clip handle out of range (1-960) | Verify session metadata    |
| `FILE_NOT_FOUND`       | Audio file moved/deleted         | Prompt user to locate file |
| `UNSUPPORTED_FORMAT`   | File format not supported        | Convert to WAV/AIFF        |
| `SAMPLE_RATE_MISMATCH` | File sample rate ≠ session rate  | Resample (future feature)  |
| `OUT_OF_MEMORY`        | Too many clips loaded            | Reduce clip count          |

**Solutions:**

1. **Check file paths:**

   ```bash
   # Verify all audio files exist
   jq -r '.clips[].filePath' session.json | while read path; do
       [ -f "$path" ] || echo "Missing: $path"
   done
   ```

2. **Validate clip handles:**

   ```cpp
   // Ensure handles are unique and in range
   std::set<int> handles;
   for (const auto& clip : clips) {
       if (clip.handle < 1 || clip.handle > 960) {
           std::cerr << "Invalid handle: " << clip.handle << std::endl;
       }
       if (!handles.insert(clip.handle).second) {
           std::cerr << "Duplicate handle: " << clip.handle << std::endl;
       }
   }
   ```

3. **Check sample rate compatibility:**
   ```cpp
   if (clip.sampleRate != sessionSampleRate) {
       std::cerr << "Sample rate mismatch: clip=" << clip.sampleRate
                 << " session=" << sessionSampleRate << std::endl;
   }
   ```

---

### Issue: Session Load Fails

**Problem:** "Invalid session file" error when loading

**Diagnosis:**

```bash
# Validate JSON syntax
jq . session.json
# If invalid, jq will report error

# Check schema version
jq -r '.sessionMetadata.version' session.json
# Should be "1.0.0"
```

**Common Causes:**

1. **Malformed JSON:**
   - Missing comma, unmatched brace, etc.
   - **Fix:** Use JSON validator (https://jsonlint.com/)

2. **Incompatible version:**
   - Session created with future OCC version
   - **Fix:** Prompt user to update OCC

3. **Missing required fields:**
   - `sessionMetadata.version` missing
   - **Fix:** Add migration code (see OCC097 Session Format)

**Solutions:**

```cpp
// Validate schema before loading
bool SessionManager::validateSchema(const juce::var& json)
{
    // Check required fields
    if (!json.hasProperty("sessionMetadata")) {
        showError("Missing sessionMetadata");
        return false;
    }

    auto metadata = json["sessionMetadata"];
    if (!metadata.hasProperty("version")) {
        showError("Missing version");
        return false;
    }

    auto version = metadata["version"].toString();
    if (!isCompatibleVersion(version)) {
        showError("Incompatible version: " + version);
        return false;
    }

    return true;
}
```

---

### Issue: Waveforms Not Displaying

**Problem:** Clip loaded, but waveform shows "Loading..." forever

**Diagnosis:**

```cpp
// Check background thread pool status
std::cerr << "Thread pool size: " << ThreadPool::getInstance()->getNumThreads() << std::endl;
std::cerr << "Pending jobs: " << ThreadPool::getInstance()->getNumPendingJobs() << std::endl;
```

**Common Causes:**

1. **Thread pool exhausted:**
   - All threads busy, new jobs queued
   - **Fix:** Increase thread pool size (8 → 16)

2. **File I/O blocked:**
   - Network-mounted file, slow disk
   - **Fix:** Copy files locally

3. **UI callback not firing:**
   - `MessageManager::callAsync()` not called
   - **Fix:** Verify callback code (see OCC098 UI Components)

**Solutions:**

```cpp
// Add timeout to waveform rendering
void WaveformDisplay::setAudioFile(orpheus::IAudioFileReader* reader, const ClipMetadata& metadata)
{
    auto timeoutMs = 5000;  // 5 seconds
    auto startTime = std::chrono::steady_clock::now();

    ThreadPool::getInstance()->addJob([this, reader, metadata, startTime, timeoutMs]() {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeoutMs) {
            juce::MessageManager::callAsync([this]() {
                showError("Waveform rendering timed out");
            });
            return;
        }

        renderWaveform(reader, metadata);

        juce::MessageManager::callAsync([this]() {
            repaint();
        });
    });
}
```

---

## Performance Issues

### Issue: Session Load Time >5 Seconds

**Problem:** Loading 960 clips takes >5 seconds (target: <2s)

**Diagnosis:**

```cpp
auto start = std::chrono::high_resolution_clock::now();
sessionManager->loadSession("session.json");
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
std::cout << "Load time: " << duration.count() << "ms" << std::endl;
```

**Common Causes:**

1. **Sequential loading (not parallel):**
   - Loading 960 clips one-by-one
   - **Fix:** Use thread pool (see OCC100 Performance)

2. **Rendering waveforms during load:**
   - Blocks until all waveforms rendered
   - **Fix:** Defer waveform rendering (lazy load)

3. **Network-mounted files:**
   - Audio files on NAS/cloud storage
   - **Fix:** Copy files locally

**Solutions:**

```cpp
// Parallel loading with thread pool
void SessionManager::loadSession(const juce::File& sessionFile)
{
    auto json = juce::JSON::parse(sessionFile);
    auto clipsArray = json["clips"];

    // Create thread pool (8 workers)
    ThreadPool pool(8);

    // Load clips in parallel
    for (int i = 0; i < clipsArray.size(); ++i) {
        pool.addJob([this, clipJson = clipsArray[i]]() {
            loadClipFromJson(clipJson);
        });
    }

    // Wait for all clips to load
    pool.waitForAll();

    // Update UI (message thread)
    clipGrid->refresh();
}
```

---

### Issue: Memory Leaks

**Problem:** Memory usage grows over time, eventually crashes

**Diagnosis:**

```bash
# macOS: Leaks tool
leaks --atExit -- /path/to/OCC.app

# Windows: Performance Monitor
perfmon.msc
# Add counter: Memory → Private Bytes → OCC.exe
# Monitor over 1 hour
```

**Common Causes:**

1. **Waveform data not released:**
   - Switching tabs doesn't free pixel buffers
   - **Fix:** Clear waveform data when tab deactivated

2. **Audio file readers not closed:**
   - `IAudioFileReader` instances accumulate
   - **Fix:** Use RAII, close readers when done

3. **JUCE component leaks:**
   - Components not removed from parent
   - **Fix:** Call `removeAllChildren()` in destructor

**Solutions:**

```cpp
// Proper cleanup in destructor
ClipGrid::~ClipGrid()
{
    // Release waveform data
    for (auto& button : buttons) {
        button->releaseWaveform();
    }

    // Remove all child components
    removeAllChildren();
}
```

---

## Cross-Platform Issues

### macOS: Code Signing Errors

**Problem:**

```
Code signature invalid for /path/to/OCC.app
```

**Solution:**

```bash
# Sign app bundle
codesign --force --deep --sign - /path/to/OCC.app

# Verify signature
codesign --verify --verbose /path/to/OCC.app
```

---

### Windows: ASIO Driver Not Found

**Problem:** "No audio devices found" when using ASIO

**Solution:**

1. **Install ASIO driver:**
   - Download from audio interface manufacturer
   - Install ASIO4ALL as fallback (http://www.asio4all.org/)

2. **Run as Administrator:**
   - ASIO drivers often require elevated privileges
   - Right-click OCC.exe → Run as Administrator

3. **Check ASIO buffer size:**
   - Open ASIO control panel
   - Set buffer size to 512 samples (10ms @ 48kHz)

---

### Linux: ALSA/JACK Configuration (Future)

**Problem:** No audio output on Linux

**Solution:**

```bash
# Install JACK
sudo apt-get install jackd2

# Start JACK server
jackd -d alsa -r 48000 -p 512

# Connect OCC to JACK
jack_connect OCC:output_1 system:playback_1
jack_connect OCC:output_2 system:playback_2
```

---

## Diagnostic Tools

### Enable Debug Logging

```cpp
// In main.cpp
#define OCC_DEBUG_LOGGING 1

// In code
#if OCC_DEBUG_LOGGING
std::cerr << "Debug: Clip started, handle=" << handle << std::endl;
#endif
```

---

### Sanitizers (Debug Builds Only)

**AddressSanitizer (ASan):**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
cmake --build build
./build/OCC
# Will detect memory issues (allocations, leaks, use-after-free)
```

**ThreadSanitizer (TSan):**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
cmake --build build
./build/OCC
# Will detect data races, deadlocks
```

---

### Crash Dumps

**macOS:**

```bash
# Crash logs are in:
~/Library/Logs/DiagnosticReports/OCC_*.crash

# View with:
cat ~/Library/Logs/DiagnosticReports/OCC_*.crash
```

**Windows:**

```bash
# Enable crash dumps:
# Control Panel → System → Advanced → Startup and Recovery → Settings
# Set "Write debugging information" to "Small memory dump"

# Dumps are in:
C:\Windows\Minidump\
```

---

## Quick Fixes

### Reset to Factory Defaults

```bash
# Delete preferences
rm -rf ~/Library/Application Support/Orpheus Clip Composer/

# macOS: Delete cache
rm -rf ~/Library/Caches/com.orpheus.clipcomposer/

# Windows: Delete registry keys
reg delete "HKEY_CURRENT_USER\Software\Orpheus\Clip Composer" /f
```

---

### Clear Auto-Save Files

```bash
# Delete auto-save files
rm ~/Documents/Orpheus Clip Composer/Sessions/*.autosave
```

---

## Related Documentation

- **OCC096** - SDK Integration Patterns (code examples)
- **OCC100** - Performance Requirements (performance targets)
- **OCC099** - Testing Strategy (test procedures)
- **OCC026** - MVP Definition (acceptance criteria)

---

## Getting Help

**GitHub Issues:** https://github.com/orpheus/clip-composer/issues

**Slack Channel:** `#orpheus-occ-support`

**When reporting issues, include:**

1. OCC version (Help → About)
2. OS version (macOS 14.2, Windows 11, etc.)
3. Hardware specs (CPU, RAM)
4. Audio interface (make/model)
5. Steps to reproduce
6. Crash logs (if applicable)
7. Session file (if applicable)

---

**Last Updated:** 2025-10-30
**Maintainer:** OCC Development Team
**Status:** Reference Documentation
