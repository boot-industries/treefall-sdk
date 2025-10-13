# Orpheus Clip Composer (OCC) - Design Documentation Governance

**Workspace:** This repo inherits general conventions from `~/chrislyons/dev/CLAUDE.md`

## Purpose

This directory contains product design, planning, and specification documents for **Orpheus Clip Composer**, the flagship application of the Orpheus audio ecosystem.

**IMPORTANT**: This directory is for design artifacts only—no code implementation should be placed here.

## Document Hierarchy

### Authoritative Documents

- **OCC021** - Orpheus Clip Composer - Product Vision
  - Status: AUTHORITATIVE
  - The definitive product vision, market positioning, and strategic direction
  - All design decisions must align with this document

### Current Design Specifications

- **OCC011** - Wireframes v2 (Latest UI wireframes)
- **OCC019** - Cursor Agent Prompt v10 (Latest comprehensive technical specification)
- **OCC009** - Session Metadata Manifest
- **OCC013** - Advanced Audio Driver Integration

### Supporting Documentation

- **OCC001-OCC018** - Historical versions and supporting technical documents
- Security best practices (OCC017)
- Platform-specific deployment guides (OCC015, OCC016)

## Design Iteration Guidelines

When iterating on OCC design documentation, follow these principles:

### 1. Design-First Approach

- **No code implementation** in this phase
- Focus on:
  - Product requirements
  - User experience flows
  - Data models and schemas
  - Component architecture
  - Technical decisions and trade-offs
  - Platform-specific considerations

### 2. Documentation Standards

Each design document should:

- Have a clear version number and date
- Identify what it supersedes (if applicable)
- Use descriptive numbering (OCC0XX format)
- Include status (Draft, Under Review, Authoritative, Superseded)
- Reference related documents

### 3. Alignment with Orpheus SDK

All OCC design work must respect the Orpheus SDK architectural principles:

- **OCC is an APPLICATION**, not a thin adapter
- Uses Orpheus SDK as audio engine foundation
- Extends SDK with real-time playback, UI, recording, remote control
- Should identify SDK capabilities needed (drives SDK evolution)
- Must maintain separation: Core SDK remains host-neutral and minimal

### 4. Document Types

This directory may contain:

- **Product Vision** - Strategic direction, market positioning, user personas
- **Technical Specifications** - Feature requirements, API contracts, data schemas
- **Architecture Diagrams** - Component relationships, data flow, system design
- **Wireframes & UI Mockups** - Interface layouts, interaction patterns
- **Decision Logs** - Technical decisions, trade-offs, rationale
- **User Flows** - Journey maps, interaction sequences
- **Data Models** - Database schemas, metadata structures
- **Platform Guides** - OS-specific considerations, deployment strategies

## Current Design Status (Oct 12, 2025)

**Completed in recent iteration:**

1. ✅ **Detailed Clip Metadata Schema** - OCC022 (complete JSON schema with 8 sections)
2. ✅ **Component Architecture** - OCC023 (5-layer architecture, threading model, data flows)
3. ✅ **User Interaction Flows** - OCC024 (8 complete workflows for key personas)
4. ✅ **UI Framework Decision** - OCC025 (JUCE vs Electron analysis, JUCE recommended)
5. ✅ **Milestone 1 MVP Definition** - OCC026 (6-month plan, acceptance criteria)
6. ✅ **API Contracts** - OCC027 (C++ interfaces between OCC and SDK)
7. ✅ **DSP Library Evaluation** - OCC028 (Rubber Band vs SoundTouch, Rubber Band recommended)
8. ✅ **SDK Enhancement Recommendations** - OCC029 (Gap analysis, 5 critical modules, timeline alignment)

**Next priorities for implementation phase:**

1. **SDK Module Development** - Build 5 critical modules identified in OCC029 (Months 1-6)
2. **High-fidelity mockups** - Figma/Sketch designs based on OCC011 wireframes
3. **Milestone 1 sprint planning** - Break down 6-month MVP into 2-week sprints
4. **JUCE proof-of-concept** - 1-week prototype to validate framework decision
5. **Technical Decision Log** - Capture ongoing architectural decisions (living document)

## Working with AI Assistants

### What to Ask For

- "Analyze gaps in current OCC documentation"
- "Create a detailed clip metadata schema"
- "Design user flow for recording directly into buttons"
- "Document the component architecture"
- "Evaluate UI framework options (JUCE vs Electron)"
- "Create milestone definition for MVP"

### What NOT to Ask For

- "Implement the audio engine" (that's code, not design)
- "Write the JUCE application" (code work, not planning)
- "Create Electron boilerplate" (implementation, not design)

Design first, code later. Get the architecture right before writing a line of code.

## Design Review Process

Before finalizing any new design document:

1. **Alignment Check** - Does it align with OCC021 Product Vision?
2. **SDK Boundary Check** - Is the core/adapter/application separation clear?
3. **Completeness Check** - Are all relevant aspects addressed?
4. **Consistency Check** - Does it conflict with existing authoritative docs?
5. **Clarity Check** - Can a developer implement from this spec?

## Version Control

- Use semantic versioning for major design revisions (v1.0, v2.0, etc.)
- Date all documents clearly
- Mark superseded documents explicitly
- Maintain historical versions for reference

## Key Principles from OCC021

Always keep in mind:

1. **Reliability Above All** - 24/7 operational capability, crash-proof
2. **Performance-First** - Ultra-low latency, sample-accurate timing
3. **User Experience Excellence** - Intuitive, configurable, accessible
4. **Sovereign Ecosystem** - No cloud dependencies, local-first, open architecture
5. **Professional-Grade** - Broadcast-safe, deterministic, production-tested

## Success Criteria for Design Documents

A good OCC design document:

- [ ] Clearly states its purpose and scope
- [ ] Includes version number and date
- [ ] References related documents
- [ ] Aligns with OCC021 Product Vision
- [ ] Identifies Orpheus SDK requirements (if applicable)
- [ ] Provides sufficient detail for implementation
- [ ] Considers cross-platform implications (Windows/macOS/Linux/iOS)
- [ ] Addresses performance and reliability requirements
- [ ] Includes diagrams, wireframes, or schemas as appropriate
- [ ] Documents trade-offs and decision rationale

## Strategic Context

OCC is not just an application—it's the flagship that proves the Orpheus ecosystem concept. Every design decision should consider:

- **Immediate user value** - Does this solve a real problem for broadcast/theater/performance users?
- **SDK evolution** - Does this reveal requirements for Orpheus SDK capabilities?
- **Ecosystem growth** - Does this establish patterns for future applications?
- **Market viability** - Does this help us compete with SpotOn, QLab, Ovation?

## Out of Scope

The following should NOT be in OCC design docs:

- Orpheus SDK core implementation details (belongs in /docs at repo root)
- Code samples or implementation (wait until design is approved)
- Third-party library source code (reference only)
- Binary files or compiled artifacts
- Credentials, API keys, or secrets

## Quick Reference: Key Design Decisions Pending

From OCC021, Section 11 (Open Questions):

**Technical:**
- UI Framework (JUCE vs Electron)
- DSP Library (Rubber Band vs SoundTouch vs Sonic)
- Session Format (JSON vs Binary - rec: JSON for MVP)
- Plugin Architecture (VST3 only vs AU/LV2 - v2.0 decision)

**Product:**
- Pricing Model (One-time vs Subscription)
- Open-Source Strategy (Fully open vs Dual-license)
- Enterprise Features (Separate SKU vs Modular)

**Strategic:**
- Launch Platform (macOS, Windows, or Both - rec: Windows first)
- Partnership Strategy (Integration with lighting/video)
- Certification/Standards (EBU R128, Dolby Atmos - v2.0+)

## Document Templates

When creating new design documents, consider these structures:

### For Technical Specifications:
```
# OCC0XX [Title]
Version: X.X
Date: YYYY-MM-DD
Status: [Draft|Under Review|Authoritative]
Supersedes: [Previous doc if applicable]

## Overview
[Purpose and scope]

## Requirements
[Detailed requirements]

## Technical Design
[Architecture, data models, APIs]

## Trade-offs & Decisions
[Rationale for key choices]

## Open Questions
[Unresolved items]

## Related Documents
[References]
```

### For User Flows:
```
# OCC0XX [User Flow Name]
Version: X.X
Date: YYYY-MM-DD

## User Persona
[Who is this for?]

## Entry Point
[How does this flow start?]

## Steps
[Detailed interaction sequence]

## Success Criteria
[How do we know it worked?]

## Error Paths
[What can go wrong and how do we handle it?]

## UI Components Required
[What interface elements are needed?]
```

---

**Remember**: OCC is infrastructure for professionals who depend on reliability, performance, and quality. Design with 10+ years of stability in mind. Favor simplicity, determinism, and user autonomy over short-term convenience.
