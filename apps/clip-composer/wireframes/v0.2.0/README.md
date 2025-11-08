# Orpheus Clip Composer - Wireframes Documentation v0.2.0

**Version:** v0.2.0-alpha
**Release Date:** October 31, 2025
**Purpose:** Comprehensive architectural diagrams for developer onboarding and reference

---

## Overview

This directory contains **6 complete wireframe diagram pairs** (12 files + this README) documenting the architecture of Orpheus Clip Composer v0.2.0. Each pair consists of:

1. **`.mermaid.md`** - Pure Mermaid diagram (paste into [mermaid.live](https://mermaid.live/) for visualization)
2. **`.notes.md`** - Detailed explanatory notes with context, rationale, and usage examples

**Target Audience:** New developers joining the project, senior engineers reviewing architecture, technical writers documenting the system.

**Recommended Reading Order:** 1 → 2 → 3 → 5 → 4 → 6 (repo structure, architecture, components, entry points, data flow, session schema)

---

## Table of Contents

### 1. Repository Structure

**Files:**
- [`1-repo-structure.mermaid.md`](./1-repo-structure.mermaid.md)
- [`1-repo-structure.notes.md`](./1-repo-structure.notes.md)

**What It Shows:**
- Complete directory tree (Source/, docs/occ/, Resources/, build/)
- Key files (Main.cpp, CMakeLists.txt, CLAUDE.md, PROGRESS.md)
- Documentation organization (OCC### naming, archives)
- Configuration files (.claude/, .claudeignore, skills.json)

**When to Use:**
- Finding where to create new files (UI component, documentation, test)
- Understanding directory purpose (what goes in Source/UI/ vs. Source/Session/)
- Locating existing code (ClipGrid, AudioEngine, SessionManager)
- Setting up new development environment

**Key Insight:** Modular organization with clear separation between UI, logic, audio engine, and docs.

---

### 2. Architecture Overview

**Files:**
- [`2-architecture-overview.mermaid.md`](./2-architecture-overview.mermaid.md)
- [`2-architecture-overview.notes.md`](./2-architecture-overview.notes.md)

**What It Shows:**
- 5-layer architecture (JUCE UI, App Logic, SDK Integration, Real-Time Audio, Platform I/O)
- 3-thread model (Message Thread, Audio Thread, Background Thread)
- Lock-free command/callback queues (UI ↔ Audio communication)
- External dependencies (JUCE, Orpheus SDK, libsndfile)

**When to Use:**
- Understanding threading constraints (what's safe on audio thread)
- Designing new features (which layer to modify)
- Debugging performance issues (which thread is bottleneck)
- Explaining system to stakeholders (high-level overview)

**Key Insight:** Strict layer separation enables testability, portability, and real-time safety.

---

### 3. Component Map

**Files:**
- [`3-component-map.mermaid.md`](./3-component-map.mermaid.md)
- [`3-component-map.notes.md`](./3-component-map.notes.md)

**What It Shows:**
- 18 major classes with public APIs (ClipGrid, SessionManager, AudioEngine, etc.)
- Component relationships (who depends on whom)
- Data structures (ClipMetadata, RoutingConfiguration, TransportPosition)
- Shared utilities (InterLookAndFeel for UI styling)

**When to Use:**
- Finding which component owns specific functionality (session save? SessionManager)
- Understanding component lifecycle (initialization order, dependencies)
- Adding new features (which components to modify, new dependencies)
- Refactoring (identifying coupling, opportunities for decoupling)

**Key Insight:** SessionManager is source of truth, AudioEngine is facade to SDK.

---

### 4. Data Flow

**Files:**
- [`4-data-flow.mermaid.md`](./4-data-flow.mermaid.md)
- [`4-data-flow.notes.md`](./4-data-flow.notes.md)

**What It Shows:**
- 4 sequence diagrams (clip trigger, session save, waveform update, transport position)
- Complete request/response cycles (user click → audio playback → UI feedback)
- Thread transitions (Message Thread → Audio Thread → Message Thread)
- Timing breakdowns (26ms latency for clip trigger, etc.)

**When to Use:**
- Understanding end-to-end workflows (how does clip triggering actually work?)
- Debugging latency issues (where is time spent?)
- Designing new features (similar to existing flows?)
- Performance optimization (identify bottlenecks)

**Key Insight:** Lock-free queues enable non-blocking UI and deterministic audio processing.

---

### 5. Entry Points

**Files:**
- [`5-entry-points.mermaid.md`](./5-entry-points.mermaid.md)
- [`5-entry-points.notes.md`](./5-entry-points.notes.md)

**What It Shows:**
- Application lifecycle (startup → running → shutdown)
- User interactions (mouse, keyboard shortcuts, drag-drop)
- System events (audio callbacks, UI timers, file operations)
- Environment differences (macOS vs. Windows, Debug vs. Release)

**When to Use:**
- Finding where to start debugging (where does user interaction enter the system?)
- Adding new keyboard shortcuts (how are they handled?)
- Understanding initialization order (what happens when app starts?)
- Troubleshooting crashes (proper shutdown sequence)

**Key Insight:** Event-driven architecture with keyboard-first workflow for professional users.

---

### 6. Session Schema

**Files:**
- [`6-session-schema.mermaid.md`](./6-session-schema.mermaid.md)
- [`6-session-schema.notes.md`](./6-session-schema.notes.md)

**What It Shows:**
- Complete JSON session file structure (.occSession format)
- ClipMetadata fields (20+ fields: trim, fade, gain, color, group, etc.)
- RoutingConfiguration (4 Clip Groups, master gain)
- SessionPreferences (audio settings, UI state)
- Version migration strategy (v1.0.0 → v1.1.0 → v2.0.0)

**When to Use:**
- Adding new clip metadata fields (how to extend schema)
- Implementing session save/load (serialization format)
- Debugging user-reported issues (inspect session file)
- Version migration (backward/forward compatibility)

**Key Insight:** JSON format is human-readable, version-control friendly, and extensible.

---

## How to Use These Wireframes

### For New Developers

**Day 1:**
1. Read `1-repo-structure` to understand file organization
2. Read `2-architecture-overview` to grasp 5-layer design and threading model
3. Read root-level docs: `CLAUDE.md`, `PROGRESS.md`, `README.md`

**Week 1:**
1. Study `3-component-map` to understand class relationships
2. Review `5-entry-points` to see how user interactions work
3. Pick a small bug or feature, trace through diagrams

**Month 1:**
1. Study `4-data-flow` for complex workflows (clip trigger, session save)
2. Study `6-session-schema` for session format details
3. Contribute first feature or bug fix with confidence

### For Code Reviews

**Before Review:**
- Check `3-component-map` - Does PR add new dependencies? Are they justified?
- Check `2-architecture-overview` - Does PR respect layer boundaries? Threading constraints?
- Check `6-session-schema` - Does PR modify session format? Is migration handled?

**During Review:**
- Reference diagrams in review comments (e.g., "This breaks the pattern shown in 4-data-flow.mermaid.md, Flow 1")
- Suggest diagram updates if PR changes architecture significantly

### For Documentation Writers

**When Writing User Docs:**
- Use `5-entry-points` to document keyboard shortcuts, menu commands
- Use `6-session-schema` to explain session file format for advanced users

**When Writing Developer Docs:**
- Link to wireframes from OCC### docs (e.g., OCC096 links to 3-component-map)
- Keep wireframes in sync with code (update when architecture changes)

### For Architects

**Design Reviews:**
- Present `2-architecture-overview` to stakeholders (high-level design)
- Use `4-data-flow` to explain performance characteristics (latency, throughput)
- Use `3-component-map` to discuss modularity, testability

**Refactoring:**
- Use diagrams to identify technical debt (e.g., duplicate AudioEngine directories)
- Plan layer consolidation (e.g., merge Source/Audio into Source/AudioEngine)

---

## When to Update These Wireframes

### Major Architecture Changes

**Scenarios:**
- New layer added (e.g., Layer 6: Network Sync)
- Threading model changes (e.g., add dedicated DSP thread)
- Component ownership changes (e.g., SessionManager split into SessionManager + ClipLibrary)

**Process:**
1. Update affected `.mermaid.md` files
2. Update corresponding `.notes.md` files
3. Increment version directory (create `wireframes/v0.3.0/`)
4. Link from root `README.md`

### New Features

**Scenarios:**
- New UI component (e.g., RoutingPanel)
- New session metadata field (e.g., DSP effects array)
- New keyboard shortcut category

**Process:**
1. Update `3-component-map` (if new component)
2. Update `5-entry-points` (if new user interaction)
3. Update `6-session-schema` (if new session field)
4. Update `.notes.md` with usage examples

### Bug Fixes

**No Updates Needed** unless:
- Bug reveals incorrect diagram (update to match reality)
- Bug fix changes architecture (e.g., add new callback queue)

---

## Cross-References to OCC Documentation

### Product & Vision
- **OCC021** - Product Vision (Authoritative) - Market positioning, feature roadmap
- **OCC026** - MVP Definition - 6-month plan, acceptance criteria

### Technical Specifications
- **OCC040-OCC045** - Architecture specs (detailed layer design)
- **OCC096** - SDK Integration Patterns (code examples for AudioEngine)
- **OCC097** - Session Format (JSON schema reference)
- **OCC098** - UI Components (JUCE implementation details)

### Implementation Reference
- **OCC099** - Testing Strategy (unit/integration tests)
- **OCC100** - Performance Requirements (latency targets, CPU budgets)
- **OCC101** - Troubleshooting Guide (debugging workflows)

### Sprint Reports
- **OCC093** - v0.2.0 Sprint Completion Report (6 UX fixes)

### Root-Level Guides
- **CLAUDE.md** - Development guide (440 lines, threading model, file organization)
- **PROGRESS.md** - Implementation status (480 lines, release history, roadmap)
- **README.md** - Getting started (build instructions, success criteria)
- **CHANGELOG.md** - Release notes (v0.1.0, v0.2.0)

---

## Viewing Mermaid Diagrams

### Online (Recommended for Quick Preview)

1. Open [mermaid.live](https://mermaid.live/)
2. Copy contents of `.mermaid.md` file
3. Paste into left pane
4. View rendered diagram in right pane
5. Export as PNG/SVG if needed

### VS Code (Recommended for Development)

1. Install "Mermaid Preview" extension
2. Open `.mermaid.md` file
3. Press `Ctrl+Shift+P` (Windows/Linux) or `Cmd+Shift+P` (macOS)
4. Type "Mermaid: Preview" and press Enter
5. Diagram renders in side panel

### GitHub (Automatic Rendering)

- GitHub automatically renders Mermaid diagrams in `.md` files
- View diagrams directly in pull requests and file browser
- Note: GitHub may have size limits (very large diagrams may not render)

### IntelliJ/WebStorm (Plugin Required)

1. Install "Mermaid" plugin from JetBrains Marketplace
2. Open `.mermaid.md` file
3. Click "Preview" tab or icon
4. Diagram renders inline

---

## File Statistics

| File                               | Type    | Size  | Purpose                              |
|------------------------------------|---------|-------|--------------------------------------|
| 1-repo-structure.mermaid.md        | Diagram | ~2KB  | Directory tree visualization         |
| 1-repo-structure.notes.md          | Notes   | ~10KB | Directory purpose, file placement    |
| 2-architecture-overview.mermaid.md | Diagram | ~2KB  | 5-layer architecture, threading      |
| 2-architecture-overview.notes.md   | Notes   | ~12KB | Design philosophy, threading model   |
| 3-component-map.mermaid.md         | Diagram | ~3KB  | Component relationships, APIs        |
| 3-component-map.notes.md           | Notes   | ~15KB | Module responsibilities, dependencies|
| 4-data-flow.mermaid.md             | Diagram | ~2KB  | Sequence diagrams (4 workflows)      |
| 4-data-flow.notes.md               | Notes   | ~18KB | Data transformations, latency        |
| 5-entry-points.mermaid.md          | Diagram | ~3KB  | Application lifecycle, interactions  |
| 5-entry-points.notes.md            | Notes   | ~16KB | Keyboard shortcuts, event handling   |
| 6-session-schema.mermaid.md        | Diagram | ~2KB  | JSON schema structure                |
| 6-session-schema.notes.md          | Notes   | ~20KB | Session format, version migration    |
| README.md                          | Index   | ~8KB  | This file (navigation, usage guide)  |

**Total:** 13 files, ~113KB (highly compressed knowledge transfer)

---

## Version History

### v0.2.0 (October 31, 2025)

**Initial Release:**
- 6 complete diagram pairs (12 files)
- Covers: Repo structure, architecture, components, data flow, entry points, session schema
- ~100KB of documentation
- Cross-referenced with OCC### docs

**Future Versions:**
- v0.3.0: Update for routing panel, audio device selection UI
- v1.0.0: Update for ASIO support, iOS companion app, recording features
- v2.0.0: Update for DSP effects, automation, advanced routing

---

## Feedback & Contributions

### Reporting Issues

**If diagrams are incorrect:**
1. Check if code changed since v0.2.0 (diagrams may be outdated)
2. File GitHub issue: Tag with `wireframes`, `documentation`
3. Specify: Which diagram, what's wrong, link to code proving error

**If diagrams are unclear:**
1. Suggest specific improvements (more examples, clearer labels)
2. File GitHub issue or submit PR with improvements

### Contributing Updates

**Process:**
1. Make changes to `.mermaid.md` and `.notes.md` files
2. Test Mermaid rendering (paste into mermaid.live)
3. Update version history in this README
4. Submit PR with clear description of changes

**Style Guide:**
- Mermaid diagrams: Start with `%%` comments, blank line, then diagram type
- Notes files: Use same structure as existing (Overview, Key Decisions, etc.)
- Cross-references: Link to OCC### docs and other wireframes

---

## Additional Resources

### Mermaid Documentation
- [Mermaid Official Docs](https://mermaid-js.github.io/mermaid/)
- [Flowchart Syntax](https://mermaid-js.github.io/mermaid/#/flowchart)
- [Sequence Diagram Syntax](https://mermaid-js.github.io/mermaid/#/sequenceDiagram)
- [Class Diagram Syntax](https://mermaid-js.github.io/mermaid/#/classDiagram)

### Orpheus Clip Composer Docs
- [OCC Documentation Index](../docs/occ/INDEX.md)
- [CLAUDE.md (Development Guide)](../../CLAUDE.md)
- [PROGRESS.md (Implementation Status)](../../PROGRESS.md)

### Orpheus SDK Docs
- [SDK CLAUDE.md (Core Principles)](/CLAUDE.md)
- [SDK ARCHITECTURE.md (Design Rationale)](/docs/ARCHITECTURE.md)
- [SDK ROADMAP.md (Timeline)](/docs/ROADMAP.md)

---

## License

These wireframes are part of the Orpheus Clip Composer project and inherit the same license as the codebase (MIT License).

**Dependencies:**
- Mermaid.js (MIT License) - Diagram rendering
- JUCE Framework (GPL or commercial license)
- Orpheus SDK (MIT License)

---

**Maintained By:** Claude Code + Human Developers
**Last Updated:** October 31, 2025
**Version:** v0.2.0
**Status:** Active Documentation (update on major architecture changes)

---

## Quick Navigation

- [1. Repo Structure](./1-repo-structure.mermaid.md)
- [2. Architecture Overview](./2-architecture-overview.mermaid.md)
- [3. Component Map](./3-component-map.mermaid.md)
- [4. Data Flow](./4-data-flow.mermaid.md)
- [5. Entry Points](./5-entry-points.mermaid.md)
- [6. Session Schema](./6-session-schema.mermaid.md)

**Happy coding!** 🎵🎹
