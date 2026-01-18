# Orpheus SDK Wireframes - v2026-01-18

Comprehensive Mermaid diagram documentation with accompanying explanatory text for the Orpheus SDK architecture.

## Overview

This wireframe collection provides detailed architectural documentation to help human developers understand the evolved codebase and participate in development decisions. Each topic includes two files:

- **`{topic}.mermaid.md`** - Pure Mermaid diagram (compatible with mermaid.live)
- **`{topic}.notes.md`** - Extended documentation and insights

## What's New in v2026-01-18

This version reflects the **ORP121 Audio Backend Refactoring** work:

### Phase 1-3: Critical Fixes & Architecture
- **C-01:** GainSmoother range extended to +12 dB
- **C-02:** Continuous soft-knee limiter (no discontinuity)
- **C-03:** Lock-free SPSC callback queue (priority inversion fix)
- **C-04:** Constant-power pan law implementation
- **A-01:** Stereo preservation (ST2110-aligned)
- **A-04/A-05:** Multi-channel format abstraction

### Phase 4: Quality Improvements
- **Q-03:** Sample rate parameterization in RoutingConfig
- **Q-04:** ITU-R BS.1770-4 true-peak metering
- **Q-05:** Headroom management modes (None, PerGroup, Global, Logarithmic)
- **Q-07:** Threading stress tests for callback queue

## Documentation Files

### 1. Repository Structure
- **Diagram:** [repo-structure.mermaid.md](./repo-structure.mermaid.md)
- **Notes:** [repo-structure.notes.md](./repo-structure.notes.md)
- **Content:** Complete directory tree visualization

### 2. Architecture Overview
- **Diagram:** [architecture-overview.mermaid.md](./architecture-overview.mermaid.md)
- **Notes:** [architecture-overview.notes.md](./architecture-overview.notes.md)
- **Content:** High-level system design with ORP121 routing enhancements

### 3. Component Map
- **Diagram:** [component-map.mermaid.md](./component-map.mermaid.md)
- **Notes:** [component-map.notes.md](./component-map.notes.md)
- **Content:** Detailed component breakdown including TruePeakMeter, HeadroomMode

### 4. Data Flow
- **Diagram:** [data-flow.mermaid.md](./data-flow.mermaid.md)
- **Notes:** [data-flow.notes.md](./data-flow.notes.md)
- **Content:** Lock-free callback queue, metering flow, headroom compensation

### 5. Entry Points
- **Diagram:** [entry-points.mermaid.md](./entry-points.mermaid.md)
- **Notes:** [entry-points.notes.md](./entry-points.notes.md)
- **Content:** All ways to interact with the codebase

### 6. Deployment Infrastructure
- **Diagram:** [deployment-infrastructure.mermaid.md](./deployment-infrastructure.mermaid.md)
- **Notes:** [deployment-infrastructure.notes.md](./deployment-infrastructure.notes.md)
- **Content:** CI/CD pipeline, build system, testing infrastructure

## How to Use These Diagrams

### Viewing Mermaid Diagrams

**Option 1: GitHub (Inline Rendering)**
GitHub automatically renders Mermaid diagrams in markdown files.

**Option 2: mermaid.live (Interactive Editor)**
1. Go to https://mermaid.live
2. Copy the entire contents of a `.mermaid.md` file
3. Paste into the editor

**Option 3: VS Code (Preview)**
Install the "Markdown Preview Mermaid Support" extension.

**Option 4: Local Mermaid CLI**
```bash
npm install -g @mermaid-js/mermaid-cli
mmdc -i repo-structure.mermaid.md -o repo-structure.png
```

## Key Architectural Principles

The Orpheus SDK is built on four non-negotiable principles:

1. **Offline-first** - No runtime network calls for core features
2. **Deterministic** - Same input → same output, always (bit-identical)
3. **Host-neutral** - Core SDK works across all environments
4. **Broadcast-safe** - 24/7 reliability, no audio thread allocations

## Version History

### v2026-01-18 (Current)
- **ORP121 Audio Backend Refactoring** updates
- Added TruePeakMeter class (ITU-R BS.1770-4)
- Added HeadroomMode enum (4 modes)
- Lock-free SPSC callback queue
- Sample rate parameterization
- Updated component map and data flow diagrams

### v2025-11-08
- Initial comprehensive wireframe documentation
- 6 diagram sets (12 files total)

## Related Documentation

**Core references:**
- [ARCHITECTURE.md](../../ARCHITECTURE.md)
- [ROADMAP.md](../../ROADMAP.md)
- [CLAUDE.md](../../CLAUDE.md)

**ORP121 Documentation:**
- [ORP121 Audio Backend Refactoring Master Plan](../../docs/orp/ORP121%20Audio%20Backend%20Refactoring%20Master%20Plan.md)
- [ORP122 Phase 4 Quality Improvements](../../docs/orp/ORP122%20Phase%204%20Quality%20Improvements%20Implementation%20Report.md)
- [GAIN_STAGING.md](../../docs/orp/GAIN_STAGING.md)

---

**Last Updated:** 2026-01-18
**Maintained By:** SDK Core Team
**Branch:** `feature/orp121-audio-backend-refactoring`
