# ORP123 Wireframes and Documentation Update

**Date:** 2026-01-25
**Status:** Complete
**Branch:** `refactor/occ-code-simplifier-audit`

## Overview

Updated architecture wireframes in `/wireframes/v2026-01-18/` and documentation HTMLs to reflect shmui-juce v2.0.0 integration (active since Jan 2026).

## Background

The wireframes dated 2026-01-18 were approximately 65-70% current with the following key gaps:

1. **shmui-juce** - Listed as "planned" but now active at v2.0.0 with CMake integration
2. **shmui components** - Missing newer components (WaveformEditor, ScrubBar, AudioPlayerControls, full button system)
3. **Clip Composer** - Internal 6-module organization not reflected
4. **architecture-gallery.html** - Existed but needed shmui-juce updates
5. **repo-commands.html** - Missing shmui sync commands

## Changes Made

### Phase 1: Wireframe Diagrams (6 files)

| File | Changes |
|------|---------|
| `repo-structure.mermaid.md` | Added shmui-juce v2.0.0 package structure (Controls/, Icons/, Shaders/, CMakeLists.txt, ShmUI.h) |
| `architecture-overview.mermaid.md` | Added shmui-juce subgraph with Audio, Components, Controls, Icons sections |
| `component-map.mermaid.md` | Added shmui namespace classes (AudioAnalyzer, WaveformVisualizer, WaveformEditor, BarVisualizer, LevelMeter, Button hierarchy) |
| `data-flow.mermaid.md` | Added shmui visualization flow (AudioAnalyzer thread-safe pattern, 60 FPS visualizer polling) |
| `entry-points.mermaid.md` | Added shmui-juce API entry point section |
| `deployment-infrastructure.mermaid.md` | Added shmui-juce sync workflow (rsync from ~/dev/shmui, CMake integration) |

### Phase 2: README Update

- `wireframes/v2026-01-18/README.md` - Updated version to "Updated 2026-01-25", added shmui-juce v2.0.0 integration details to "What's New"

### Phase 3: HTML Galleries

- `wireframes/architecture-gallery.html` - Updated all 6 Mermaid diagrams, added shmui-juce to color legend, updated footer
- `docs/repo-commands.html` - Added "shmui-juce (Audio Visualization)" section with 5 commands

## shmui-juce v2.0.0 Package Structure

```
packages/shmui-juce/
├── CMakeLists.txt          # Build integration
├── ShmUI.h                 # Main include header (v2.0.0)
├── Audio/
│   └── AudioAnalyzer       # Thread-safe FFT, RMS, band analysis
├── Components/
│   ├── WaveformVisualizer  # Multiple waveform display variants
│   ├── WaveformEditor      # Advanced waveform with trim/fade/seek
│   ├── BarVisualizer       # Frequency band display
│   ├── OrbVisualizer       # OpenGL shader-based 3D orb
│   ├── MatrixDisplay       # LED-style matrix display
│   ├── LevelMeter          # Professional VU/PPM meter
│   ├── TransportBar        # Full transport control strip
│   ├── ScrubBar            # Timeline scrubber
│   └── AudioPlayerControls # Complete player UI
├── Controls/
│   ├── Button              # Base button with style/size variants
│   ├── ButtonStyles.h      # Style definitions
│   ├── TextButton          # Text label button
│   ├── IconButton          # Icon-only button
│   ├── ToggleButton        # Stateful toggle
│   ├── TransportButton     # Play/Pause/Stop/Record
│   ├── MuteButton          # Mute/Solo/Bypass toggles
│   └── ClipButton          # Clip trigger with state machine
├── Icons/
│   ├── Icons.h             # Icon library API
│   └── Icons.cpp           # Icon data
├── Shaders/
│   ├── OrbFragment.glsl    # Orb fragment shader
│   └── OrbVertex.glsl      # Orb vertex shader
└── Utils/
    ├── AgentState.h        # Agent state management
    ├── ColorUtils.h        # Color utilities
    └── Interpolation.h     # Animation interpolation
```

## shmui Sync Command

The shmui JUCE components are synced from the upstream `~/dev/shmui` repository:

```bash
rsync -av --delete ~/dev/shmui/juce/Source/ ~/dev/orpheus-sdk/packages/shmui-juce/
```

This command is now documented in `docs/repo-commands.html` for easy access.

## Cross-Repo Consideration

**Source of Truth:** `~/dev/shmui` (standalone repo)
**Integration:** `packages/shmui-juce/` (synced copy)
**Version Tracking:** shmui::Version::string in ShmUI.h
**Modification Policy:** No modifications to shmui-juce should be made directly in orpheus-sdk

## Verification Checklist

- [x] Open `wireframes/architecture-gallery.html` in browser - all 6 diagrams render
- [x] Open `docs/repo-commands.html` in browser - shmui section visible
- [x] Paste updated mermaid files into https://mermaid.live to verify syntax (all valid)

## Files Modified

**Wireframe Diagrams (wireframes/v2026-01-18/):**
- `repo-structure.mermaid.md`
- `architecture-overview.mermaid.md`
- `component-map.mermaid.md`
- `data-flow.mermaid.md`
- `entry-points.mermaid.md`
- `deployment-infrastructure.mermaid.md`
- `README.md`

**HTML Files:**
- `wireframes/architecture-gallery.html`
- `docs/repo-commands.html`

**Documentation:**
- `.claude/implementation_progress.md`
- `docs/orp/ORP123 Wireframes and Documentation Update.md` (this file)

## Related Documents

- ORP119 - Shmui Integration Strategy
- ORP120 - Codebase State Assessment
- ORP121 - Audio Backend Refactoring Master Plan (wireframes v2026-01-18 created here)

---

**Author:** Claude Code
**Session:** Wireframes & Documentation Update
