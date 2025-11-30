# ORP118: Audio Architecture Audit Findings

**Date:** November 26, 2025
**Author:** Gemini 3 Pro
**Status:** Draft
**Related:** ORP114, ORP115

## Executive Summary

A comprehensive audit of the Orpheus SDK codebase (specifically `TransportController` and `RoutingMatrix`) revealed critical architectural violations regarding thread safety and memory management. The most severe issue is a mutex lock (`m_audioFilesMutex`) being held within the real-time audio thread during clip initialization, posing a significant priority inversion risk. Additionally, the signal path is currently limited to mono summing before routing, and `RoutingMatrix` uses unsafe manual memory management.

## Critical Findings

### 1. Thread Safety Violation (Priority: Critical)

**Component:** `TransportController`
**Location:** `addActiveClip()` (called from `processCommands()` in audio thread)

**Issue:**
The `addActiveClip` method locks `m_audioFilesMutex` to look up clip metadata and the file reader. This mutex is shared with UI-thread methods like `registerClipAudio` and `updateClipMetadata`.

**Violation:**
"ZERO locks on audio thread."

**Risk:**
If the UI thread holds this mutex (e.g., while registering a new clip or loading a session), the audio thread will block when attempting to start a clip. This leads to:

- Audio dropouts (glitches)
- Priority inversion (audio thread waiting on lower-priority UI thread)

### 2. Unsafe Memory Management

**Component:** `RoutingMatrix`
**Location:** `initialize()`, `cleanupChannels()`

**Issue:**
`GainSmoother` objects are managed via raw `new` and `delete`.

**Violation:**
Modern C++ safety standards (RAII).

**Risk:**

- Memory leaks if exceptions are thrown or cleanup logic is flawed.
- Dangling pointers if state is not rigorously tracked.

### 3. Suboptimal Signal Flow

**Component:** `TransportController` -> `RoutingMatrix`

**Issue:**
`TransportController` mixes multi-channel audio files down to mono (`m_clipChannelBuffers`) _before_ sending them to the `RoutingMatrix`.

**Impact:**

- Stereo panning in `RoutingMatrix` is effectively applying gain to a mono signal, losing spatial information from stereo source files.
- Limits the SDK to mono-summed processing chains.

## Recommendations

### 1. Refactor Clip Start Logic (Mutex Removal)

**Goal:** Remove `m_audioFilesMutex` lock from the audio thread.

**Proposed Pattern:**
Use the **Command Queue** payload to transfer necessary state.

1.  **UI Thread (`startClip`):** Lookup `AudioFileEntry` while holding the lock.
2.  **UI Thread:** Create a lightweight, thread-safe `ClipPlaybackContext` (containing `shared_ptr<Reader>`, trim points, gain, etc.).
3.  **Command Queue:** Post `TransportCommand::Start` containing this `ClipPlaybackContext` (via shared_ptr or value copy).
4.  **Audio Thread (`processCommands`):** Read context directly. No lookup, no lock.

### 2. Modernize RoutingMatrix

**Goal:** Enforce RAII and correct stereo processing.

**Proposed Changes:**

- Replace `GainSmoother*` with `std::unique_ptr<GainSmoother>`.
- Update `TransportController` to output interleaved stereo (or N-channel) buffers to `RoutingMatrix`.
- Update `RoutingMatrix` to process N-channel inputs.

## Sprint Plan

**Phase 1: Thread Safety (High Priority)**

1.  Define `ClipPlaybackContext` struct.
2.  Refactor `TransportCommand` to carry `shared_ptr<ClipPlaybackContext>`.
3.  Update `startClip` to resolve context on UI thread.
4.  Update `addActiveClip` to use context without locking.

**Phase 2: Modernization (Medium Priority)**

1.  Refactor `RoutingMatrix` to use `std::unique_ptr`.
2.  Verify no performance regression.

**Phase 3: Signal Path (Future)**

1.  Design stereo/N-channel interface between Transport and Routing.
