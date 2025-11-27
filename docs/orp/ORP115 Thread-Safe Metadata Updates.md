# ORP115: Thread-Safe Metadata Updates via Command Queue

**Status:** In Progress
**Priority:** Critical (Fixes ORP114 Gain Staging Bug)
**Date:** 2025-11-26
**Related:** ORP114, ORP092

## Problem Statement

The investigation in **ORP114** revealed a critical race condition in `TransportController`. Public API methods like `updateClipTrimPoints`, `updateClipGain`, and `updateClipFades` modify active clip data structures directly from the caller's thread (typically the UI thread in Clip Composer).

Simultaneously, the audio thread reads these structures in `processAudio`. This leads to:

1.  **Data Races:** Tearing of 64-bit values or partial updates.
2.  **Logic Errors:** A clip's `currentSample` can suddenly be outside the valid `[trimIn, trimOut]` range mid-buffer, causing fade calculations to overflow and produce massive gain spikes ("Gain went through the roof").
3.  **Violation of Core Principles:** The audio thread is not isolated from external state changes.

## Solution: Command Queue Pattern

We will extend the existing `TransportCommand` queue mechanism—currently used for `Start`, `Stop`, `StopAll`—to handle metadata updates. This ensures all state changes happen **synchronously** within the audio thread's execution flow, _before_ audio processing occurs.

### 1. Extended Command Structure

The `TransportCommand` struct in `transport_controller.h` will be expanded to carry metadata payloads using a union or flexible data structure within the fixed-size command slot.

### 2. API Refactoring

Methods like `updateClipTrimPoints` will be refactored to post commands to the queue instead of modifying memory directly.

### 3. Audio Thread Processing

`processCommands()` will be updated to handle the new command types (`UpdateTrim`, `UpdateGain`, `UpdateFade`, `UpdateLoop`, `UpdateStopOthers`), applying the changes to active clips safely before any audio rendering takes place.

### 4. ORP092 Regression Fix

We will also restore the `clip.reader = nullptr` assignment in the fade-out completion logic. This prevents `libsndfile` from auto-wrapping to the beginning of the file if read past EOF, ensuring clean stops for non-looping clips.

## Verification Plan

- **Unit Tests:** Verify that metadata updates correctly propagate via the queue.
- **Regression:** Ensure existing transport behavior (Start/Stop) remains correct.
