# Entry Points - Explanatory Notes

## Overview

This diagram visualizes all entry points into the Orpheus Clip Composer application - every way a user or the system can interact with the codebase. Understanding entry points is critical for new developers to know **where to start debugging**, **where to add new features**, and **how the application responds to events**.

**Key Categories:** Application lifecycle, mouse interactions, keyboard shortcuts, audio callbacks, file operations, UI timers, session loading, environment differences.

## Key Architectural Decisions

### JUCE Application Lifecycle

**Why JUCE?**
- Cross-platform UI framework (macOS, Windows, Linux)
- Mature component library (buttons, sliders, waveform displays)
- Integrated audio I/O (CoreAudio, ASIO, WASAPI)
- Active community and commercial support

**Initialization Order:**
1. **Main.cpp** - Entry point, creates JUCEApplication
2. **JUCEApplication::initialise()** - Set up logging, load preferences
3. **Create MainWindow** - Document window container
4. **Initialize SDK** - AudioEngine connects to Orpheus SDK
5. **Initialize Audio Driver** - Platform-specific (CoreAudio/WASAPI)
6. **Create UI Components** - MainComponent, ClipGrid, TransportControls
7. **Load Last Session** - Auto-restore previous state (optional)
8. **Show Window** - Make UI visible, start event loop

**Why This Order?**
- SDK must initialize before UI (UI depends on AudioEngine)
- Audio driver must initialize before UI callbacks (avoids race conditions)
- Session load happens last (UI is ready to display clips)

### Event-Driven Architecture

**All User Interactions Are Events:**
- Mouse events → JUCE Component::mouseDown(), mouseUp(), mouseDrag()
- Keyboard events → JUCE Component::keyPressed(), keyReleased()
- Audio callbacks → JUCE AudioIODevice::audioDeviceIOCallback()
- Timer callbacks → JUCE Timer::timerCallback()

**Why Event-Driven?**
- Decoupled components (ClipButton doesn't know about MainComponent)
- Testable (can simulate events without real user input)
- Responsive UI (non-blocking, events queued)

### Keyboard-Driven Workflow

**Design Philosophy:** Professional users (broadcast operators, theater technicians) work fast with keyboards, not mice.

**48 Button Shortcuts (QWERTY Layout):**
```
Row 1: Q W E R T Y U I O P
Row 2: A S D F G H J K L ;
Row 3: Z X C V B N M , . /
```

**Additional Shortcuts:**
- 1-8: Switch tabs
- Space: Stop current clip
- Arrow keys: Navigate grid
- Enter: Edit selected clip
- [ ]: Set trim IN/OUT and restart playback
- Cmd+S: Save session
- Cmd+O: Open session

**Why So Many Shortcuts?**
- Faster than mouse (hands stay on keyboard)
- Muscle memory (consistent layout across all tabs)
- Accessibility (screen reader friendly)

## Entry Point Breakdown

### Application Lifecycle

**Main.cpp Entry Point:**
```cpp
START_JUCE_APPLICATION(OrpheusClipComposerApplication)
```

**Initialization Sequence:**
1. JUCE creates application instance
2. `initialise()` called with command-line arguments
3. Create MainWindow, set size (1200×800), center on screen
4. MainComponent constructor creates ClipGrid, TransportControls, etc.
5. AudioEngine::initialize() connects to SDK, enumerates audio devices
6. Audio driver starts (512 samples @ 48kHz default)
7. SessionManager::loadLastSession() restores previous state (if exists)
8. Window becomes visible, event loop starts

**Shutdown Sequence:**
1. User quits (Cmd+Q, File → Quit, close window)
2. `shutdown()` called
3. Stop audio driver (finish processing current buffer)
4. Cleanup SDK resources (AudioEngine destructor)
5. Save preferences (window size, audio device selection)
6. Exit application (return from main)

**Total Startup Time:** <2 seconds (Debug build), <1 second (Release build)

### User Interactions: Mouse

**Click Clip Button:**
- `ClipButton::mouseDown(MouseEvent)` triggered
- Query SessionManager for clip metadata
- Send `startClip(handle)` to AudioEngine
- Update button visual state (optimistic UI)

**Click Waveform (Click-to-Jog):**
- `WaveformDisplay::mouseDown(MouseEvent)` triggered
- Convert pixel position to sample position
- Send `seekClip(handle, position)` to AudioEngine (v0.2.0: single command, gap-free)
- Update playhead visual position

**Drag Trim Slider:**
- `juce::Slider::valueChanged()` callback triggered
- `ClipEditDialog::onTrimInChanged()` updates `currentClip.trimIn`
- `WaveformDisplay::setTrimPoints(trimIn, trimOut)` updates visual markers
- If playing, enforce trim point edit laws (v0.2.0 fix):
  - If IN set after playhead → restart from IN
  - If OUT set before playhead → jump to IN and restart

**Right-Click Button:**
- `ClipButton::mouseDown(MouseEvent)` with `isRightClick()` check
- Show context menu: "Edit Clip", "Delete Clip", "Copy Clip", "Paste Clip"
- Execute selected action

**Drag-Drop Audio File:**
- `ClipButton::filesDropped(StringArray files)` triggered
- Validate file (WAV/AIFF/FLAC), show error if unsupported
- Create ClipMetadata with defaults
- Load file via AudioEngine (background thread)
- Update button with clip name, color

**Drag Button (Reorder Clips):**
- Hold mouse down for 240ms (latch delay)
- `ClipButton::mouseDrag(MouseEvent)` starts drag
- Show drag indicator
- `ClipGrid::itemDropped(DragTarget)` swaps button assignments
- Update SessionManager with new button order

### User Interactions: Keyboard

**Space Bar (Stop Current Clip):**
- `MainComponent::keyPressed(KeyPress)` receives Space
- Forward to ClipGrid
- ClipGrid queries currently playing clip handle
- Send `stopClip(handle)` to AudioEngine

**1-8 Keys (Switch Tabs):**
- `MainComponent::keyPressed(KeyPress)` receives '1'-'8'
- Forward to TabSwitcher
- `TabSwitcher::setCurrentTab(tabIndex)`
- `ClipGrid::setCurrentTab(tabIndex)` refreshes buttons
- Playback state isolated per-tab (v0.2.0 fix)

**QWERTY Layout (48 Button Shortcuts):**
- `ClipGrid::keyPressed(KeyPress)` receives 'Q', 'W', etc.
- Map key to button index (Q=0, W=1, E=2, ...)
- Trigger `onClipTriggered(row, col)` same as mouse click

**Arrow Keys (Navigate Grid):**
- `ClipGrid::keyPressed(KeyPress)` receives Up/Down/Left/Right
- Move selection highlight (currentRow, currentCol)
- Scroll grid if selection moves off-screen

**[ ] Keys (Set Trim IN/OUT, Restart Playback):**
- `ClipEditDialog::keyPressed(KeyPress)` receives '[' or ']'
- '[' sets trim IN to current playhead position, restarts from IN
- ']' sets trim OUT to current playhead position, restarts from IN
- v0.2.0 fix: Restart playback immediately (consistent with mouse buttons)

**Cmd+S (Save Session):**
- `MainComponent::keyPressed(KeyPress)` receives Cmd+S
- If session file known, save immediately
- If new session, show "Save As" file picker
- Call `SessionManager::saveSession(file)`

### System Events: Audio Callbacks

**Audio Driver Callback (Every ~10ms):**
- Audio driver calls `audioDeviceIOCallback(inputBuffer, outputBuffer, numSamples)`
- AudioEngine forwards to SDK: `transport->processAudio(outputs, numSamples)`
- SDK renders audio (mix clips, apply gain, routing)
- SDK updates transport position (atomic write)
- SDK enqueues callbacks (onClipStarted, onClipStopped)

**Why 10ms?**
- 512 samples @ 48kHz = 10.67ms
- Industry standard buffer size (trade-off: latency vs. stability)
- Smaller buffers (128 samples) = lower latency but higher CPU, risk of dropouts

**Real-Time Constraints:**
- ⛔ NO allocations (new, malloc, std::vector::push_back)
- ⛔ NO locks (std::mutex, std::lock_guard)
- ⛔ NO I/O (file reads, network calls)
- ✅ Lock-free queues, atomic operations, pre-allocated buffers

### System Events: File Operations

**File → Open Session:**
- Show file picker (juce::FileChooser)
- User selects `.occSession` file
- `SessionManager::loadSession(file)` parses JSON
- Validate schema version (v1.0)
- Load clip metadata (960 clips)
- Open audio files (background thread)
- Render waveforms (background thread)
- Refresh ClipGrid (current tab)

**File → Save Session:**
- If current session file known, save immediately
- If new session, show "Save As" file picker
- `SessionManager::saveSession(file)` serializes to JSON
- Write to disk (blocking I/O, ~100ms for 960 clips)
- Show confirmation message

**File Dropped on Grid:**
- JUCE detects file drop event
- Forward to ClipButton under cursor
- Validate file format (WAV/AIFF/FLAC)
- Load file, create ClipMetadata
- Update button visual state

**Auto-Save Timer:**
- JUCE Timer fires every 5 minutes (configurable in preferences)
- If session modified since last save, auto-save to temp file
- Show notification: "Session auto-saved"
- Used for crash recovery (future feature)

### System Events: UI Timers

**75fps UI Update Timer:**
- JUCE Timer fires every 13.3ms (75Hz)
- `AudioEngine::timerCallback()` reads transport position (atomic load)
- Process callback queue (onClipStarted, onClipStopped)
- Update WaveformDisplay playhead position
- Update ClipButton states (highlight playing clips)
- Update TransportControls position label

**Why 75fps?**
- Broadcast standard (smooth playhead motion)
- v0.2.0 improvement from 30fps (was too laggy)
- <1% CPU overhead

### Session Loading Flow

**Detailed Sequence:**
1. User clicks "Open Session"
2. juce::FileChooser shows native file picker
3. User selects `~/Documents/my_session.occSession`
4. SessionManager::loadSession(file) called
5. Read file contents into string (blocking I/O)
6. Parse JSON (nlohmann::json or juce::JSON)
7. Validate schema version (check "version": "1.0.0")
8. Parse session metadata (name, author, sampleRate, etc.)
9. Parse 960 clips (for each clip: handle, name, filePath, trim, fade, gain, color, group)
10. Parse routing config (4 groups: gain, mute, solo)
11. Parse preferences (autoSave, showWaveforms, colorScheme)
12. For each clip with valid filePath:
    - Enqueue background task: Open file via IAudioFileReader
    - Enqueue background task: Render waveform (downsample for display)
13. Background thread processes tasks (parallel for 960 clips)
14. As waveforms complete, post UI updates via MessageManager::callAsync
15. ClipGrid::refreshButtons() updates all 120 buttons (current tab)
16. Show notification: "Session loaded: 960 clips"

**Performance:** <2 seconds for 960-clip session (v0.2.0: <1 second achieved)

**Error Handling:**
- If file not found: Show error dialog, don't crash
- If JSON invalid: Show error dialog, suggest "Report Bug"
- If clip file missing: Continue loading, mark button as "Missing File"
- If schema version unsupported: Offer to migrate or abort

### Environment Differences

**macOS Build:**
- Audio driver: CoreAudio
- File paths: `/Users/username/...`
- Keyboard shortcuts: Cmd+Key

**Windows Build:**
- Audio driver: WASAPI (ASIO in v1.0)
- File paths: `C:\Users\username\...`
- Keyboard shortcuts: Ctrl+Key

**Debug Build:**
- AddressSanitizer enabled (detect allocations, memory leaks)
- Verbose logging (trace every clip trigger, file load)
- Assertions enabled (check preconditions, invariants)
- No optimizations (easier debugging)

**Release Build:**
- Optimizations enabled (-O3, SIMD, inlining)
- Minimal logging (errors only)
- No sanitizers (faster but less safety checking)

## Common Workflows

### Workflow: Launch App → Load Session → Play Clip

1. User double-clicks OrpheusClipComposer.app
2. Main.cpp entry point, JUCE initializes
3. MainWindow created, UI visible
4. AudioEngine initializes SDK, starts CoreAudio driver
5. SessionManager auto-loads last session (960 clips)
6. ClipGrid refreshes, shows 120 buttons (tab 0)
7. User presses 'Q' key (shortcut for button 0)
8. ClipGrid::keyPressed() triggers clip
9. AudioEngine sends startClip command to SDK
10. Audio plays through speakers

**Total Time:** ~3 seconds (launch to first audio)

### Workflow: Edit Trim Points → Save Session → Quit

1. User right-clicks ClipButton #5
2. Context menu: "Edit Clip"
3. ClipEditDialog opens, loads clip
4. WaveformDisplay renders waveform
5. User drags trimInSlider (0 → 48,000 samples)
6. User drags trimOutSlider (240,000 → 192,000 samples)
7. User clicks "Save" button
8. ClipEditDialog closes, SessionManager updated
9. User presses Cmd+S (save session)
10. SessionManager writes JSON to disk
11. User presses Cmd+Q (quit)
12. Application shutdown sequence runs
13. Exit

**Total Time:** ~30 seconds (typical editing session)

## Where to Make Changes

### Feature: Add New Keyboard Shortcut (e.g., Cmd+D for Duplicate Clip)

**Files to Modify:**
- `Source/MainComponent.cpp` - Add `keyPressed()` handler for Cmd+D
- `Source/Session/SessionManager.cpp` - Add `duplicateClip(handle)` method
- `docs/occ/OCC099.md` - Document new keyboard shortcut

**Implementation:**
```cpp
// In MainComponent::keyPressed():
if (key == KeyPress('d', ModifierKeys::commandModifier, 0)) {
    auto selectedClip = clipGrid->getSelectedClip();
    if (selectedClip.isValid()) {
        sessionManager->duplicateClip(selectedClip.handle);
        clipGrid->refreshButtons();
    }
    return true;
}
```

### Feature: Add Menu Bar (File, Edit, View, Help)

**Files to Create:**
- `Source/UI/MenuBarModel.h/cpp` - Implement juce::MenuBarModel

**Files to Modify:**
- `Source/MainComponent.cpp` - Add menu bar to layout
- `CMakeLists.txt` - Add new source files

**Menu Structure:**
```
File
  - New Session (Cmd+N)
  - Open Session... (Cmd+O)
  - Save Session (Cmd+S)
  - Save Session As... (Cmd+Shift+S)
  - Quit (Cmd+Q)

Edit
  - Undo (Cmd+Z)
  - Redo (Cmd+Shift+Z)
  - Cut (Cmd+X)
  - Copy (Cmd+C)
  - Paste (Cmd+V)

View
  - Show Waveforms (Toggle)
  - Show Performance Monitor (Toggle)
  - Full Screen (Cmd+F)

Help
  - User Manual
  - Keyboard Shortcuts Reference
  - About Clip Composer
```

### Feature: Add Drag-and-Drop from Finder/Explorer

**Files to Modify:**
- `Source/ClipGrid/ClipButton.cpp` - Implement `isInterestedInFileDrag()`, `filesDropped()`
- `Source/Session/SessionManager.cpp` - Add `loadFileIntoButton(file, buttonIndex)`

**Implementation:**
```cpp
// In ClipButton::isInterestedInFileDrag():
bool ClipButton::isInterestedInFileDrag(const StringArray& files) {
    return files.size() == 1 && isAudioFile(files[0]);
}

// In ClipButton::filesDropped():
void ClipButton::filesDropped(const StringArray& files, int x, int y) {
    auto file = juce::File(files[0]);
    sessionManager->loadFileIntoButton(file, buttonIndex);
    refreshButton();
}
```

### Bug Fix: Application Crashes on Quit

**Investigation Path:**
1. Check shutdown sequence in Main.cpp
2. Verify AudioEngine destructor stops audio driver before cleaning up SDK
3. Check for dangling pointers (UI components accessing deleted SessionManager)
4. Run with AddressSanitizer to detect use-after-free

**Common Causes:**
- Audio thread still running during shutdown (stop driver first!)
- UI timer accessing deleted component (cancel timer in destructor)
- Callback queue not flushed (process remaining callbacks before cleanup)

**Files to Check:**
- `Source/Main.cpp` - Shutdown sequence
- `Source/AudioEngine/AudioEngine.cpp` - Destructor
- `Source/MainComponent.cpp` - Destructor (cancel timers)

## Performance Considerations

### Entry Point Latency

| Entry Point           | Target Latency | Actual (v0.2.0) | Notes                           |
|-----------------------|----------------|-----------------|----------------------------------|
| Application Startup   | <3s            | ~2s             | Debug build, macOS M1            |
| Session Load (960)    | <2s            | ~1s             | Background waveform rendering    |
| Clip Button Click     | <30ms          | ~26ms           | Click → button highlight         |
| Keyboard Shortcut     | <20ms          | ~15ms           | Keypress → clip trigger          |
| Audio Callback        | <10ms          | ~10ms           | 512 samples @ 48kHz              |
| UI Timer (75fps)      | 13.3ms         | 13.3ms          | Playhead updates                 |

### CPU Usage by Entry Point

**Profiling Results (macOS M1, Release Build):**
- Idle (no clips playing): 2-3%
- Audio callback (16 clips): 25-30%
- UI timer (75fps): <1%
- Session save (960 clips): 5% (spiky, 100ms duration)
- Waveform rendering: 5% (background thread, parallel)

## Related Diagrams

- **2-architecture-overview.mermaid.md** - Shows layers and threads (context for entry points)
- **3-component-map.mermaid.md** - Shows which components handle which entry points
- **4-data-flow.mermaid.md** - Shows data flow triggered by entry points

## Cross-References to OCC Docs

- **OCC096** - SDK Integration Patterns (AudioEngine initialization, audio callbacks)
- **OCC097** - Session Format (session loading entry point)
- **OCC098** - UI Components (mouse/keyboard event handling in JUCE)
- **OCC099** - Testing Strategy (testing entry points with mocks)
- **CLAUDE.md** - Development guide (startup sequence, threading model)

---

**Last Updated:** October 31, 2025
**Version:** v0.2.0
**Maintained By:** Claude Code + Human Developers
