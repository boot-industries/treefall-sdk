# Component Map - Explanatory Notes

## Overview

This diagram provides a detailed class-level view of all major components in Orpheus Clip Composer v0.2.0, showing their public APIs, dependencies, and relationships. Unlike the architecture overview (which shows layers and threads), this map focuses on **how components collaborate** at the code level.

**Total Components:** 18 major classes across UI, application logic, and audio engine integration

## Key Architectural Decisions

### Component Ownership Model

**MainComponent as Root Container:**
- MainComponent owns ClipGrid, TransportControls, TabSwitcher, PerformanceMonitor
- Responsible for layout, keyboard event routing, theme application
- Delegates domain logic to SessionManager and AudioEngine (not owned directly)

**Why This Design?**
- Clear ownership hierarchy (no circular dependencies)
- Easy to swap UI components (e.g., different theme, mobile layout)
- Testable (can mock SessionManager/AudioEngine for UI tests)

### SessionManager as Source of Truth

**Single Responsibility:**
- SessionManager owns ALL clip metadata (name, trim, fade, gain, color, group)
- SessionManager handles JSON persistence (load/save sessions)
- Other components query SessionManager, never maintain their own metadata copy

**Why This Design?**
- Avoids data synchronization bugs (single source of truth)
- Session save/load is trivial (SessionManager.toJSON())
- Easier to implement undo/redo (future feature)

### AudioEngine as Facade

**SDK Abstraction Layer:**
- AudioEngine wraps Orpheus SDK interfaces (ITransportController, IAudioFileReader, IRoutingMatrix)
- Provides OCC-specific API (startClip, stopClip, seekClip, updateClipMetadata)
- Hides SDK implementation details from UI components

**Why This Design?**
- UI components don't depend on SDK headers (cleaner separation)
- Easier to mock AudioEngine for UI testing
- SDK upgrades don't break OCC UI code (stable API boundary)

## Important Patterns

### Component Lifecycle

**Initialization Order (Main.cpp → MainComponent):**

1. **SessionManager** created first (no dependencies)
2. **AudioEngine** created (depends on SessionManager for metadata)
3. **ClipGrid** created (depends on SessionManager, AudioEngine)
4. **TransportControls** created (depends on AudioEngine)
5. **TabSwitcher** created (standalone)
6. **MainComponent** assembles all components

**Why This Order?**
- SessionManager has no dependencies (safest to initialize first)
- AudioEngine needs SessionManager to query clip metadata
- UI components need both SessionManager and AudioEngine

### Callback Pattern (Audio → UI)

**Problem:** Audio thread cannot call UI methods directly (JUCE not thread-safe)

**Solution:** AudioEngine posts callbacks to Message Thread

```cpp
// In AudioEngine (audio thread callback):
callbackQueue.enqueue([clipHandle, position] {
    MessageManager::callAsync([=] {
        clipGrid->highlightPlayingClip(clipHandle);  // Safe on Message Thread
        transportControls->updatePosition(position);
    });
});
```

**Components Using Callbacks:**
- ClipGrid (highlight playing clips, clear highlights)
- TransportControls (update position, show play/stop state)
- WaveformDisplay (update playhead position)

### Query Pattern (UI → SessionManager)

**Problem:** Multiple UI components need clip metadata

**Solution:** SessionManager provides query methods

```cpp
// In ClipGrid::refreshButtons():
for (int i = 0; i < 120; ++i) {
    auto clip = sessionManager->getClipAtButton(currentTab, i);
    buttons[i]->setClip(clip);
}
```

**Query Methods:**
- `getClipAtButton(tab, button)` - Retrieve single clip
- `getAllClips()` - Retrieve all 960 clips (for session save)
- `getRoutingConfig()` - Retrieve routing configuration
- `getPreferences()` - Retrieve user preferences

### Command Pattern (UI → AudioEngine)

**Problem:** UI needs to control audio playback without blocking

**Solution:** AudioEngine provides non-blocking commands

```cpp
// In ClipButton::mouseDown():
auto result = audioEngine->startClip(clipHandle);  // Non-blocking, lock-free enqueue
if (result != SessionGraphError::OK) {
    showError("Failed to start clip");
}
```

**Command Methods:**
- `startClip(handle)` - Trigger clip playback
- `stopClip(handle)` - Stop specific clip
- `stopAllClips()` - Panic button (stop all)
- `seekClip(handle, position)` - Click-to-jog
- `updateClipMetadata(handle, metadata)` - Apply trim/fade/gain changes

## Technical Debt / Complexity

### Known Issues

1. **ClipEditDialog Size** - ~1,800 lines, handles too many responsibilities
   - *Impact:* Hard to maintain, test, extend
   - *Plan:* Refactor into smaller components (TrimEditor, FadeEditor, GainEditor) in v0.3.0

2. **Duplicate AudioEngine Classes** - Both `Source/Audio/AudioEngine` and `Source/AudioEngine/AudioEngine` exist
   - *Impact:* Confusion, potential link errors
   - *Plan:* Remove `Source/Audio/` directory in v0.3.0

3. **No Unit Tests for Components** - UI components not tested in isolation
   - *Impact:* Regressions during refactoring
   - *Plan:* Add GoogleTest suite for SessionManager, ClipMetadata in v0.3.0

### Complexity Hotspots

- **ClipGrid** - Manages 120 buttons, keyboard shortcuts, drag-drop, color updates (500+ lines)
- **SessionManager** - JSON parsing, validation, file I/O, error recovery (400+ lines)
- **AudioEngine** - SDK integration, callback queue, device management (600+ lines)

## Common Workflows

### Adding a Clip to the Grid

**User Action:** Drag audio file onto ClipButton

**Component Interaction:**
1. `ClipButton::filesDropped()` receives file path
2. `ClipButton` creates ClipMetadata (name from filename, default trim/fade/gain)
3. `ClipButton` calls `sessionManager->setClipAtButton(tab, button, metadata)`
4. `SessionManager` stores metadata
5. `ClipButton` calls `setClip(metadata)` to update visual state
6. `AudioEngine` loads file via SDK (background thread)

**Files Modified:** ClipButton.cpp, SessionManager.cpp

### Editing Clip Trim Points

**User Action:** Open ClipEditDialog, adjust trim IN/OUT sliders

**Component Interaction:**
1. `ClipEditDialog::loadClip(metadata)` loads clip into editor
2. `WaveformDisplay::setWaveform(data)` renders waveform
3. User drags `trimInSlider`
4. `ClipEditDialog::onTrimInChanged()` updates `currentClip.trimIn`
5. `WaveformDisplay::setTrimPoints(trimIn, trimOut)` updates visual markers
6. User clicks "Save"
7. `ClipEditDialog::saveClip()` returns updated metadata
8. Caller updates `sessionManager->setClipAtButton(tab, button, updatedMetadata)`
9. `audioEngine->updateClipMetadata(handle, updatedMetadata)` applies to SDK

**Files Modified:** ClipEditDialog.cpp, WaveformDisplay.cpp, SessionManager.cpp, AudioEngine.cpp

### Saving a Session

**User Action:** Click "Save Session" button

**Component Interaction:**
1. `MainComponent::onSaveClicked()` shows file picker
2. User selects save location (e.g., `~/Documents/my_session.occSession`)
3. `sessionManager->saveSession(file)` called
4. `SessionManager` calls `toJSON()` on all clips, routing, preferences
5. Write JSON to file (blocking I/O, OK on Message Thread)
6. Show confirmation dialog

**Files Modified:** MainComponent.cpp, SessionManager.cpp

### Triggering a Clip

**User Action:** Click ClipButton or press keyboard shortcut

**Component Interaction:**
1. `ClipButton::mouseDown()` or `ClipGrid::keyPressed()`
2. `ClipGrid::onClipTriggered(row, col)` calculates button index
3. `SessionManager::getClipAtButton(tab, button)` retrieves metadata
4. `audioEngine->startClip(metadata.handle)` sends command
5. AudioEngine enqueues command to audio thread (lock-free)
6. SDK processes command, starts playback
7. SDK posts callback: `onClipStarted(handle, position)`
8. `ClipGrid::highlightPlayingClip(handle)` updates button color

**Files Modified:** ClipButton.cpp, ClipGrid.cpp, AudioEngine.cpp

## Where to Make Changes

### Feature: Add Clip Fade Curve Selection

**Impact on Components:**
- **ClipMetadata** - Add `fadeInCurve` and `fadeOutCurve` fields (enum: Linear, Exponential, Logarithmic)
- **ClipEditDialog** - Add combo boxes for fade curve selection
- **SessionManager** - Serialize fade curve in JSON
- **AudioEngine** - Pass fade curve to SDK (SDK must implement rendering)

**Files to Modify:**
- `Source/Session/SessionManager.h` - Update ClipMetadata struct
- `Source/UI/ClipEditDialog.cpp` - Add UI controls
- `docs/occ/OCC097.md` - Update session schema

**Testing:**
- Save session with fade curves, verify JSON contains correct values
- Load session, verify fade curves restored
- Verify SDK applies fade curves during playback (integration test)

### Feature: Add Clip Color Customization

**Impact on Components:**
- **ClipEditDialog** - Add ColorSwatchPicker button
- **ColorSwatchPicker** - Already implemented, just hook up callback
- **SessionManager** - Already stores color in ClipMetadata
- **ClipButton** - Already applies color, no changes needed

**Files to Modify:**
- `Source/UI/ClipEditDialog.cpp` - Add button, connect to ColorSwatchPicker

**Testing:**
- Select color, verify button updates immediately
- Save session, verify color persisted
- Load session, verify color restored

### Feature: Add Clip Group Routing UI

**Impact on Components:**
- **RoutingPanel** - New component (not yet implemented in v0.2.0)
- **SessionManager** - Already stores RoutingConfiguration
- **AudioEngine** - Already integrates with IRoutingMatrix

**Files to Create:**
- `Source/UI/RoutingPanel.h/cpp` - New component

**Files to Modify:**
- `Source/MainComponent.cpp` - Add RoutingPanel to layout
- `CMakeLists.txt` - Add new source files

**Testing:**
- Change group gain, verify audio level changes
- Mute/solo groups, verify correct clips play
- Save session, verify routing persisted

### Bug Fix: ClipEditDialog Keyboard Shortcuts Not Working

**Investigation Path:**
1. Check `ClipEditDialog::keyPressed()` - Verify method implemented
2. Check JUCE component hierarchy - Ensure dialog has keyboard focus
3. Check MainComponent - Ensure keyboard events not consumed before reaching dialog
4. Check JUCE docs - Verify `KeyPress` handling best practices

**Common Causes:**
- Dialog not focused (call `grabKeyboardFocus()` in constructor)
- MainComponent consuming events (check `keyPressed()` return value)
- Keyboard shortcuts conflicting with OS shortcuts (e.g., Cmd+Q)

**Files to Check:**
- `Source/UI/ClipEditDialog.cpp`
- `Source/MainComponent.cpp`

## Module Responsibilities

### UI Components (Layer 1)

| Component           | Responsibility                                                |
|---------------------|---------------------------------------------------------------|
| MainComponent       | Root container, layout, keyboard routing, theme              |
| ClipGrid            | 120-button grid, keyboard shortcuts, drag-drop               |
| ClipButton          | Individual button, visual state, mouse/keyboard events       |
| TabSwitcher         | 8-tab navigation, keyboard shortcuts 1-8                     |
| TransportControls   | Play/stop UI, position display, panic button                 |
| WaveformDisplay     | Waveform rendering, playhead, trim markers, click-to-jog     |
| ClipEditDialog      | Trim editor, fade controls, gain slider, time counter        |
| ColorSwatchPicker   | Color palette, color selection callback                      |
| PreviewPlayer       | File preview before loading into grid                        |
| AudioSettingsDialog | Audio driver, device, buffer size, sample rate selection     |
| InterLookAndFeel    | UI styling (fonts, colors, button shapes, slider appearance) |

### Application Logic (Layer 2)

| Component             | Responsibility                                    |
|-----------------------|---------------------------------------------------|
| SessionManager        | JSON save/load, clip metadata storage, validation |
| ClipMetadata          | Clip properties (trim, fade, gain, color, group) |
| RoutingConfiguration  | 4 Clip Groups, master gain, routing matrix config |

### Audio Engine Integration (Layer 3)

| Component       | Responsibility                                        |
|-----------------|-------------------------------------------------------|
| AudioEngine     | SDK facade, command queue, callback queue, device I/O |
| TransportPosition | Playback state (position, playing, active clip)     |

## Public APIs

### SessionManager

**Core Methods:**
```cpp
Result loadSession(juce::File file);
Result saveSession(juce::File file);
ClipMetadata getClipAtButton(int tab, int button);
void setClipAtButton(int tab, int button, ClipMetadata metadata);
std::vector<ClipMetadata> getAllClips();
RoutingConfiguration getRoutingConfig();
void setRoutingConfig(RoutingConfiguration config);
```

**Usage Example:**
```cpp
// Save session
auto result = sessionManager->saveSession(juce::File("~/my_session.occSession"));
if (!result.isOk()) {
    showError(result.errorMessage);
}

// Load clip
auto clip = sessionManager->getClipAtButton(0, 5);  // Tab 0, button 5
```

### AudioEngine

**Core Methods:**
```cpp
Result initialize();
Result startClip(ClipHandle handle);
Result stopClip(ClipHandle handle);
Result stopAllClips();
Result seekClip(ClipHandle handle, int64_t position);
Result updateClipMetadata(ClipHandle handle, ClipMetadata metadata);
TransportPosition getTransportPosition();
float getCPUUsage();
double getLatency();
```

**Usage Example:**
```cpp
// Start clip
auto result = audioEngine->startClip(clipHandle);
if (result == SessionGraphError::OK) {
    // Clip started successfully
}

// Query transport position
auto pos = audioEngine->getTransportPosition();
if (pos.isPlaying) {
    updatePlayhead(pos.samplePosition);
}
```

### ClipMetadata

**Serialization Methods:**
```cpp
JSON toJSON() const;
static ClipMetadata fromJSON(const JSON& json);
```

**Usage Example:**
```cpp
// Serialize
ClipMetadata clip;
clip.name = "Intro Music";
clip.trimIn = 0;
clip.trimOut = 240000;
auto json = clip.toJSON();

// Deserialize
auto restoredClip = ClipMetadata::fromJSON(json);
```

## Dependency Graph

**Dependency Flow (UI → Logic → Engine):**
```
MainComponent
  ├── ClipGrid → SessionManager → AudioEngine → Orpheus SDK
  ├── TransportControls → AudioEngine → Orpheus SDK
  ├── TabSwitcher (standalone)
  └── InterLookAndFeel (standalone)

ClipEditDialog
  ├── WaveformDisplay → AudioEngine (preview playback)
  ├── ColorSwatchPicker (standalone)
  └── PreviewPlayer → AudioEngine
```

**Key Insight:** All audio operations flow through AudioEngine (never direct SDK calls from UI)

## Testing Boundaries

### Unit Tests (Isolated Components)

**TestableComponents:**
- SessionManager (JSON parsing, file I/O mocked)
- ClipMetadata (serialization, validation)
- RoutingConfiguration (JSON schema)

**Why These?**
- Pure C++ logic, no JUCE UI dependencies
- Deterministic behavior (same input → same output)
- Fast tests (<1ms per test)

### Integration Tests (Component + SDK)

**Test Scenarios:**
- AudioEngine + SessionManager (load session, start clips, verify playback)
- ClipGrid + AudioEngine (trigger clip, verify callback received)
- WaveformDisplay + AudioEngine (verify playhead updates)

**Why These?**
- Test real SDK integration
- Verify lock-free communication (UI ↔ Audio thread)
- Catch threading bugs early

### Manual Tests (Full Application)

**Test Workflows:**
- Load 960-clip session, verify all buttons display correctly
- Play 16 simultaneous clips, verify <30% CPU
- 24-hour stability test, verify no crashes or memory leaks

**Why These?**
- Real-world user workflows
- Performance validation
- Cross-platform compatibility

## Related Diagrams

- **1-repo-structure.mermaid.md** - Shows file locations for each component (Source/UI/, Source/Session/, etc.)
- **2-architecture-overview.mermaid.md** - Shows layers and threading model (which components on which threads)
- **4-data-flow.mermaid.md** - Shows sequence diagrams for specific workflows (clip trigger, session save)
- **5-entry-points.mermaid.md** - Shows application initialization order, keyboard shortcuts, audio callbacks

## Cross-References to OCC Docs

- **OCC096** - SDK Integration Patterns (AudioEngine implementation details)
- **OCC097** - Session Format (SessionManager, ClipMetadata JSON schema)
- **OCC098** - UI Components (JUCE implementation details for ClipGrid, WaveformDisplay, etc.)
- **OCC099** - Testing Strategy (unit tests, integration tests, manual testing)
- **OCC100** - Performance Requirements (CPU budgets, latency targets)

---

**Last Updated:** October 31, 2025
**Version:** v0.2.0
**Maintained By:** Claude Code + Human Developers
