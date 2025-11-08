# Repository Structure - Explanatory Notes

## Overview

This diagram visualizes the complete directory structure of Orpheus Clip Composer v0.2.0. The application follows a modular JUCE architecture with clear separation between UI components, audio engine integration, session management, and documentation.

**Total Files:** ~28 C++ source files, 35 active documentation files, 4 root-level guides

## Key Architectural Decisions

### Source Directory Organization

- **Modular UI Components** - All UI elements isolated in `Source/UI/` (12 files) for maintainability and testability
- **ClipGrid Isolation** - 960-button grid system separated into dedicated `ClipGrid/` directory
- **Duplicate AudioEngine Directories** - Both `Source/Audio/` and `Source/AudioEngine/` exist (legacy refactoring in progress - use `AudioEngine/` for new code)
- **Session as First-Class Module** - `Source/Session/` handles all JSON persistence, making session format a core abstraction

### Documentation Strategy

- **OCC### Prefix** - All app-specific docs use "OCC" prefix (Orpheus Clip Composer) to distinguish from SDK docs (ORP prefix)
- **Active vs Archive** - Documents older than 90 days move to `docs/occ/archive/` to reduce context clutter
- **35 Active Docs** - Current documentation covering product vision, technical specs, implementation patterns, sprint reports
- **Root-level Guides** - `CLAUDE.md` (dev guide), `PROGRESS.md` (status), `README.md` (index), `CHANGELOG.md` (releases)

### Configuration Files

- **CMakeLists.txt** - JUCE 8.0.4 integration with Orpheus SDK linking
- **.claude/** - Claude Code instance configuration with lazy-loaded skills
- **.claudeignore** - Excludes build artifacts, binaries, and large resource files from indexing

## Important Patterns

### Directory Purpose Matrix

| Directory         | Purpose                                    | Key Files                                 |
|-------------------|--------------------------------------------|-------------------------------------------|
| `Source/UI/`      | All visual components                      | ClipEditDialog, WaveformDisplay, TabSwitcher |
| `Source/ClipGrid/`| 960-button grid system                     | ClipGrid, ClipButton                      |
| `Source/AudioEngine/` | SDK integration layer                  | AudioEngine (transport, file reader)      |
| `Source/Session/` | JSON save/load                             | SessionManager                            |
| `Source/Transport/` | Transport controls UI                    | TransportControls                         |
| `docs/occ/`       | OCC-specific documentation                 | OCC021-OCC098                             |
| `Resources/`      | Fonts, icons, assets                       | HKGrotesk typeface                        |

### File Naming Conventions

- **JUCE Components** - `ComponentName.h` + `ComponentName.cpp` (UpperCamelCase)
- **Documentation** - `OCC### Title.md` (zero-padded 3-digit ID, OCC040, OCC096, etc.)
- **Root Guides** - ALL_CAPS.md for high-visibility (CLAUDE.md, PROGRESS.md, README.md)

### What NOT to Commit

- `build/` - JUCE build artifacts (CMake output, object files, binaries)
- `orpheus_clip_composer_app_artefacts/` - Tauri build outputs
- `*.o`, `*.a`, `*.dylib`, `*.so` - Compiled objects
- `.DS_Store`, `Thumbs.db` - OS-specific metadata
- `Third-party/*/build/` - Dependency builds

## Technical Debt / Complexity

### Known Issues

1. **Duplicate Audio Directories** - `Source/Audio/` is legacy, `Source/AudioEngine/` is current. Need to consolidate (deferred to v0.3.0)
2. **No Tests Directory** - Unit tests planned but not yet implemented (see OCC099 for strategy)
3. **Resource Organization** - Font files in `Resources/HKGrotesk_3003/` could be flattened (minor optimization)

### Complexity Hotspots

- **Source/UI/ClipEditDialog.cpp** - Large file (~1,800 lines) handling waveform editor, trim controls, fade settings, time counter, keyboard shortcuts
- **Source/ClipGrid/ClipGrid.cpp** - Manages 960 button lifecycle, drag-drop, color updates, keyboard navigation
- **Source/Session/SessionManager.cpp** - JSON parsing, metadata validation, file I/O, error recovery

## Common Workflows

### Adding a New UI Component

1. Create `ComponentName.h` and `ComponentName.cpp` in `Source/UI/`
2. Follow JUCE component patterns (inherit from `juce::Component`)
3. Use `InterLookAndFeel` for consistent styling
4. Add to `MainComponent` layout if top-level
5. Update `CMakeLists.txt` to include new source files
6. Document in `docs/occ/OCC098` (UI Components Reference)

### Adding New Documentation

1. Determine next available OCC### ID (check `docs/occ/INDEX.md`)
2. Create `docs/occ/OCC### Title.md`
3. Add entry to `docs/occ/INDEX.md`
4. Cross-reference from relevant root docs (CLAUDE.md, PROGRESS.md)
5. Use markdown with code fences for C++ examples

### Session Loading Flow

1. User selects `.occSession` file
2. `SessionManager::loadSession()` reads JSON from `Source/Session/`
3. Parse clip metadata (trim, fade, gain, color, group)
4. Load audio files via `AudioEngine` (delegates to SDK)
5. Update `ClipGrid` with button assignments
6. Restore routing configuration

### Rapid Audition Workflow (Common User Task)

1. User clicks clip button in `ClipGrid`
2. `ClipButton::mouseDown()` triggers `AudioEngine::startClip()`
3. SDK starts playback on audio thread
4. `TransportControls` shows playing state
5. User presses `Space` to stop (keyboard shortcut)

## Where to Make Changes

### Feature: Add New Clip Metadata Field

**Files to Modify:**
- `Source/Session/SessionManager.cpp` - Add JSON parsing for new field
- `docs/occ/OCC097.md` - Update session schema documentation
- `docs/occ/OCC022.md` - Update clip metadata schema (authoritative)
- `Source/UI/ClipEditDialog.cpp` - Add UI control if user-editable

**Testing:**
- Load existing sessions (backward compatibility)
- Save session and verify JSON includes new field
- Test default value for clips without new field

### Feature: Add New UI Component (e.g., Performance Monitor)

**Files to Create:**
- `Source/UI/PerformanceMonitor.h`
- `Source/UI/PerformanceMonitor.cpp`

**Files to Modify:**
- `Source/MainComponent.cpp` - Add component to layout
- `CMakeLists.txt` - Add source files to build
- `docs/occ/OCC098.md` - Document component API

**Dependencies:**
- `AudioEngine` for CPU/latency metrics
- `InterLookAndFeel` for consistent styling

### Feature: Keyboard Shortcut Customization

**Files to Modify:**
- `Source/UI/ClipEditDialog.cpp` - Make shortcuts configurable
- `Source/Session/SessionManager.cpp` - Persist shortcuts in JSON preferences
- `docs/occ/OCC097.md` - Update session schema

**New Files Needed:**
- `Source/UI/KeyboardShortcutsEditor.h/cpp` - UI for customization

### Bug Fix: Audio Dropouts on High CPU

**Investigation Path:**
1. Check `Source/AudioEngine/AudioEngine.cpp` - Audio callback buffer size, sample rate mismatch
2. Verify SDK integration (no allocations on audio thread)
3. Profile with Instruments (macOS) or Visual Studio Profiler (Windows)
4. Review `docs/occ/OCC100.md` (Performance Requirements)
5. Check `docs/occ/OCC101.md` (Troubleshooting Guide)

**Common Causes:**
- Buffer underruns (increase buffer size in audio settings)
- UI thread blocking audio thread (lock-free command queue)
- File I/O on audio thread (should be on background thread)

## Related Diagrams

- **2-architecture-overview.mermaid.md** - Shows 5-layer architecture and threading model
- **3-component-map.mermaid.md** - Detailed component relationships and APIs
- **5-entry-points.mermaid.md** - Application initialization and user interaction flows

## Cross-References to OCC Docs

- **OCC021** - Product Vision (market positioning, feature roadmap)
- **OCC026** - MVP Definition (6-month plan, deliverables)
- **OCC040-OCC045** - Technical architecture specifications
- **OCC096** - SDK Integration Patterns (code examples for OCC + SDK)
- **OCC097** - Session Format (JSON schema)
- **OCC098** - UI Components (JUCE implementation details)
- **OCC093** - v0.2.0 Sprint Completion Report (latest release)

## Notes for New Developers

### First Steps

1. Read `CLAUDE.md` (development guide with threading model, architecture overview)
2. Read `PROGRESS.md` (current status, v0.2.0 complete, v0.2.1 planning)
3. Read `README.md` (getting started, build instructions, success criteria)
4. Explore `docs/occ/OCC021` (product vision - why Clip Composer exists)
5. Review `docs/occ/OCC096` (SDK integration patterns - how OCC uses SDK)

### Build from Source

```bash
cd /home/user/orpheus-sdk
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DORPHEUS_ENABLE_APP_CLIP_COMPOSER=ON
cmake --build build
./scripts/relaunch-occ.sh
```

### Documentation Hierarchy

**When in Doubt:**
1. Check `CLAUDE.md` for development conventions
2. Check `docs/occ/INDEX.md` for complete doc catalog
3. Search `docs/occ/` for relevant OCC### docs
4. Ask on Slack `#orpheus-occ-integration`

### Multi-Instance Usage

Clip Composer can be developed in **two Claude Code instances**:

- **SDK Instance** (`~/dev/orpheus-sdk`) - For core SDK changes
- **OCC Instance** (`~/dev/orpheus-sdk/apps/clip-composer`) - For app-specific work

Each instance has its own `.claude/` configuration and skills. See root `CLAUDE.md` Section "Multi-Instance Usage" for coordination strategy.

---

**Last Updated:** October 31, 2025
**Version:** v0.2.0
**Maintained By:** Claude Code + Human Developers
