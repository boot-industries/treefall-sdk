# OCC096 - SDK Integration Patterns

**Version:** 1.0
**Date:** 2025-10-30
**Status:** Reference Documentation

Comprehensive code patterns for integrating Orpheus Clip Composer with the Orpheus SDK.

---

## Overview

This document provides complete code examples for common integration scenarios between OCC application code and the Orpheus SDK. All patterns follow the 3-thread model (Message Thread, Audio Thread, File I/O Thread) and maintain real-time safety.

---

## Pattern 1: Starting a Clip

**Scenario:** User clicks a clip button in the UI

**Thread Flow:** Message Thread → Audio Thread → Message Thread (callback)

```cpp
// In UI button callback (Message Thread):
void ClipButton::mouseDown(const juce::MouseEvent& e)
{
    auto clipHandle = getClipHandle();  // From session metadata

    // Send command to audio thread (lock-free, non-blocking)
    auto result = transportController->startClip(clipHandle);

    if (result != orpheus::SessionGraphError::OK) {
        // Show error in UI
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Playback Error",
            "Failed to start clip: " + std::to_string(static_cast<int>(result))
        );
    }
}

// In audio callback (Audio Thread):
void AudioEngine::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // SDK processes commands and updates audio
    float* outputs[2] = { buffer.getWritePointer(0), buffer.getWritePointer(1) };
    transportController->processAudio(outputs, 2, buffer.getNumSamples());

    // NO UI updates here! Post to message thread via callbacks.
}

// In transport callback (Posted to Message Thread):
void AudioEngine::onClipStarted(orpheus::ClipHandle handle, orpheus::TransportPosition pos)
{
    // Safe to update UI now (we're on message thread)
    clipGrid->highlightPlayingClip(handle);
    transportDisplay->updatePosition(pos);
}
```

**Key Points:**

- UI button click is non-blocking
- SDK command is lock-free
- UI feedback happens via callback on message thread
- Audio thread NEVER waits or blocks

---

## Pattern 2: Loading an Audio File

**Scenario:** Loading audio file during session load

**Thread Flow:** Message Thread (parse JSON) → Background Thread (waveform) → Message Thread (UI update)

```cpp
// In session loading (Message Thread):
void SessionManager::loadClip(const juce::File& audioFile)
{
    auto reader = orpheus::createAudioFileReader();

    // This is OK on message thread (not audio thread)
    auto result = reader->open(audioFile.getFullPathName().toStdString());

    if (!result.isOk()) {
        showError("Failed to load: " + result.errorMessage);
        return;
    }

    // Store metadata for UI
    ClipMetadata metadata;
    metadata.sampleRate = result.value.sample_rate;
    metadata.numChannels = result.value.num_channels;
    metadata.durationSamples = result.value.duration_samples;
    metadata.format = result.value.format;

    // Pre-render waveform on background thread
    renderWaveformAsync(reader, metadata);
}
```

**Key Points:**

- File I/O only on Message Thread or Background Thread (NEVER Audio Thread)
- Metadata extracted synchronously during session load
- Waveform rendering deferred to background thread
- Audio thread gets pre-loaded buffers (not shown here)

---

## Pattern 3: Routing Configuration

**Scenario:** User assigns clip to routing group or adjusts group gain

**Thread Flow:** Message Thread → Audio Thread (via lock-free command)

```cpp
// In routing panel (Message Thread):
void RoutingPanel::assignClipToGroup(orpheus::ClipHandle clip, uint8_t groupIndex)
{
    // This will be lock-free once IRoutingMatrix is implemented
    auto result = routingMatrix->setClipGroup(clip, groupIndex);

    if (result == orpheus::SessionGraphError::OK) {
        updateRoutingDisplay();
    }
}

void RoutingPanel::setGroupGain(uint8_t groupIndex, float gainDb)
{
    // Gain changes are smoothed over 10ms in audio thread
    routingMatrix->setGroupGain(groupIndex, gainDb);
}
```

**Key Points:**

- Routing changes use lock-free commands
- Gain smoothing happens in audio thread (no clicks/pops)
- UI updates immediately (optimistic rendering)
- SDK validates parameters (e.g., group index bounds)

---

## Pattern 4: Stop All Clips (Panic Button)

**Scenario:** User hits panic button to stop all playback immediately

**Thread Flow:** Message Thread → Audio Thread (immediate stop)

```cpp
// In transport controls (Message Thread):
void TransportControls::onPanicButtonPressed()
{
    // Stop all clips immediately (lock-free broadcast to audio thread)
    transportController->stopAllClips();

    // Update UI to reflect stopped state
    clipGrid->clearAllHighlights();
    transportDisplay->reset();
}
```

**Key Points:**

- Single command stops all clips
- Audio thread processes on next callback (~10ms latency)
- UI updates optimistically (doesn't wait for confirmation)
- Safe to call from any thread

---

## Pattern 5: Waveform Rendering (Background Thread)

**Scenario:** Pre-render waveform data for UI display

**Thread Flow:** Background Thread (compute) → Message Thread (UI update)

```cpp
// In waveform display (Message Thread):
void WaveformDisplay::setAudioFile(orpheus::IAudioFileReader* reader, const ClipMetadata& metadata)
{
    // Pre-render waveform on background thread
    ThreadPool::getInstance()->addJob([this, reader, metadata]() {
        renderWaveform(reader, metadata);

        // Update UI on message thread
        juce::MessageManager::callAsync([this]() {
            repaint();
        });
    });
}

// On background thread:
void WaveformDisplay::renderWaveform(orpheus::IAudioFileReader* reader, const ClipMetadata& metadata)
{
    // Read entire file and downsample for display
    const size_t displayWidth = 800;  // pixels
    const size_t samplesPerPixel = metadata.durationSamples / displayWidth;

    waveformData.resize(displayWidth);

    for (size_t i = 0; i < displayWidth; ++i) {
        float maxSample = 0.0f;
        // Read and find peak in this pixel's range
        // (Simplified - actual implementation would batch reads)
        waveformData[i] = maxSample;
    }
}
```

**Key Points:**

- Waveform rendering is CPU-intensive (background thread only)
- UI update via `MessageManager::callAsync()`
- No blocking on message thread
- File I/O allowed on background thread

---

## Pattern 6: Performance Monitoring

**Scenario:** Display CPU usage and latency in UI

**Thread Flow:** Audio Thread (measure) → Message Thread (display) via atomic

```cpp
// In audio engine (Audio Thread):
void AudioEngine::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    // Process audio
    float* outputs[2] = { buffer.getWritePointer(0), buffer.getWritePointer(1) };
    transportController->processAudio(outputs, 2, buffer.getNumSamples());

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    // Store performance metrics atomically (lock-free)
    cpuUsageMicroseconds.store(duration.count(), std::memory_order_relaxed);
}

// In performance monitor (Message Thread, timer-based):
void PerformanceMonitor::timerCallback()
{
    // Read atomic value (lock-free)
    auto cpuMicros = audioEngine->getCpuUsageMicroseconds();

    // Calculate percentage (at 512 samples, 48kHz = 10.67ms budget)
    float cpuPercent = (cpuMicros / 10670.0f) * 100.0f;

    // Update UI
    cpuMeter->setValue(cpuPercent);
}
```

**Key Points:**

- Performance measurement happens in audio thread
- Atomic write (lock-free, no contention)
- UI polls periodically (30 Hz timer)
- No audio thread blocking

---

## Pattern 7: Session Save

**Scenario:** User saves current session to JSON

**Thread Flow:** Message Thread only (I/O allowed)

```cpp
// In session manager (Message Thread):
void SessionManager::saveSession(const juce::File& sessionFile)
{
    // Build JSON from current state (on message thread, I/O is OK)
    juce::DynamicObject::Ptr root = new juce::DynamicObject();

    // Session metadata
    auto metadataObj = new juce::DynamicObject();
    metadataObj->setProperty("name", sessionName);
    metadataObj->setProperty("version", "1.0.0");
    metadataObj->setProperty("sampleRate", sampleRate);
    root->setProperty("sessionMetadata", metadataObj);

    // Clips array
    juce::Array<juce::var> clipsArray;
    for (const auto& clip : clips) {
        auto clipObj = new juce::DynamicObject();
        clipObj->setProperty("handle", clip.handle);
        clipObj->setProperty("name", clip.name);
        clipObj->setProperty("filePath", clip.filePath);
        clipObj->setProperty("buttonIndex", clip.buttonIndex);
        clipObj->setProperty("tabIndex", clip.tabIndex);
        clipObj->setProperty("clipGroup", clip.clipGroup);
        clipObj->setProperty("trimIn", clip.trimIn);
        clipObj->setProperty("trimOut", clip.trimOut);
        clipObj->setProperty("gain", clip.gain);
        clipObj->setProperty("color", clip.color);
        clipsArray.add(clipObj);
    }
    root->setProperty("clips", clipsArray);

    // Routing configuration
    auto routingObj = new juce::DynamicObject();
    // ... (build routing object)
    root->setProperty("routing", routingObj);

    // Write to file (blocking I/O is OK on message thread)
    juce::var jsonVar(root.get());
    auto jsonString = juce::JSON::toString(jsonVar, true);  // pretty-print

    if (!sessionFile.replaceWithText(jsonString)) {
        showError("Failed to save session file");
    }
}
```

**Key Points:**

- Session save is blocking operation (message thread only)
- No interaction with audio thread (playback continues)
- JSON serialization is synchronous
- User sees save confirmation/error immediately

---

## Pattern 8: Clip Metadata Query

**Scenario:** UI needs to display clip information (name, duration, etc.)

**Thread Flow:** Message Thread only (no audio thread interaction)

```cpp
// In clip grid (Message Thread):
void ClipGrid::updateButtonLabel(int tabIndex, int buttonIndex)
{
    // Query session manager for clip metadata (no SDK call needed)
    auto clip = sessionManager->getClipAtButton(tabIndex, buttonIndex);

    if (!clip.isValid()) {
        buttons[buttonIndex]->setText("");
        return;
    }

    // Format display text
    auto durationSeconds = clip.durationSamples / static_cast<double>(clip.sampleRate);
    auto labelText = juce::String::formatted("%s (%.1fs)", clip.name.c_str(), durationSeconds);

    buttons[buttonIndex]->setText(labelText);
    buttons[buttonIndex]->setColour(juce::Colours::fromString(clip.color));
}
```

**Key Points:**

- Metadata queries are cheap (in-memory, message thread)
- No SDK interaction required (session manager cache)
- UI updates are synchronous and fast
- No blocking or waiting

---

## Anti-Patterns (What NOT to Do)

### ❌ Anti-Pattern 1: Blocking Audio Thread

```cpp
// WRONG: File I/O on audio thread
void AudioEngine::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // ❌ NEVER DO THIS - blocking file read on audio thread!
    auto file = juce::File("/path/to/clip.wav");
    juce::AudioFormatReader* reader = formatManager.createReaderFor(file);

    // This will cause audio dropouts!
}
```

**Fix:** Pre-load all audio files on message thread or background thread.

---

### ❌ Anti-Pattern 2: Allocating on Audio Thread

```cpp
// WRONG: Memory allocation on audio thread
void AudioEngine::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // ❌ NEVER DO THIS - vector resize allocates memory!
    std::vector<float> tempBuffer;
    tempBuffer.resize(buffer.getNumSamples());

    // This will cause non-deterministic latency!
}
```

**Fix:** Pre-allocate all buffers before audio thread starts.

---

### ❌ Anti-Pattern 3: Using Locks in Audio Thread

```cpp
// WRONG: Mutex lock on audio thread
std::mutex clipStateMutex;

void AudioEngine::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // ❌ NEVER DO THIS - mutex can block indefinitely!
    std::lock_guard<std::mutex> lock(clipStateMutex);

    // Audio thread is now at mercy of other threads!
}
```

**Fix:** Use lock-free atomics or lock-free queues.

---

### ❌ Anti-Pattern 4: Calling UI from Audio Thread

```cpp
// WRONG: Direct UI update from audio thread
void AudioEngine::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // ❌ NEVER DO THIS - JUCE components are NOT thread-safe!
    clipGrid->highlightPlayingClip(currentClipHandle);

    // This will crash or cause data races!
}
```

**Fix:** Use `MessageManager::callAsync()` or SDK callbacks.

---

## Thread Safety Checklist

Before merging any OCC + SDK integration code, verify:

- [ ] Audio thread: No allocations (`new`, `std::vector::push_back()`, etc.)
- [ ] Audio thread: No locks (`std::mutex`, `std::shared_mutex`, etc.)
- [ ] Audio thread: No I/O (file reads, network calls, logging to file)
- [ ] Audio thread: No UI calls (JUCE components, `repaint()`, etc.)
- [ ] Message thread: Uses `callAsync()` for background → UI updates
- [ ] Background thread: No direct UI manipulation (only via `callAsync()`)
- [ ] All threads: SDK error codes checked and handled
- [ ] All threads: Null pointer checks before SDK calls

---

## Quick Reference: Common SDK Calls

**Transport Control:**

```cpp
transportController->startClip(handle);           // Start playback
transportController->stopClip(handle);            // Stop specific clip
transportController->stopAllClips();              // Panic button
```

**Audio File I/O:**

```cpp
auto reader = orpheus::createAudioFileReader();
reader->open(filePath);                           // Load file (message thread only)
```

**Routing:**

```cpp
routingMatrix->setClipGroup(clip, groupIndex);    // Assign to group
routingMatrix->setGroupGain(groupIndex, gainDb);  // Adjust group gain
```

**Performance:**

```cpp
auto cpuUsage = performanceMonitor->getCpuUsage(); // Read atomic value
```

---

## Related Documentation

- **OCC027** - API Contracts (complete SDK interface definitions)
- **OCC023** - Component Architecture (5-layer architecture, threading model)
- **OCC097** - Session Format (JSON schema and loading code)
- **OCC098** - UI Components (JUCE component implementations)

---

**Last Updated:** 2025-10-30
**Maintainer:** OCC Development Team
**Status:** Reference Documentation
