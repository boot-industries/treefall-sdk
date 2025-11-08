# Orpheus SDK Wireframes - v2025-11-08

Comprehensive Mermaid diagram documentation with accompanying explanatory text for the Orpheus SDK architecture.

## Overview

This wireframe collection provides detailed architectural documentation to help human developers understand the evolved codebase and participate in development decisions. Each topic includes two files:

- **`{topic}.mermaid.md`** - Pure Mermaid diagram (compatible with mermaid.live)
- **`{topic}.notes.md`** - Extended documentation and insights

## Documentation Files

### 1. Repository Structure
- **Diagram:** [repo-structure.mermaid.md](./repo-structure.mermaid.md)
- **Notes:** [repo-structure.notes.md](./repo-structure.notes.md)
- **Content:** Complete directory tree visualization showing core components, packages, adapters, and applications

### 2. Architecture Overview
- **Diagram:** [architecture-overview.mermaid.md](./architecture-overview.mermaid.md)
- **Notes:** [architecture-overview.notes.md](./architecture-overview.notes.md)
- **Content:** High-level system design showing layered architecture from platform to applications

### 3. Component Map
- **Diagram:** [component-map.mermaid.md](./component-map.mermaid.md)
- **Notes:** [component-map.notes.md](./component-map.notes.md)
- **Content:** Detailed component breakdown with relationships, dependencies, and public APIs

### 4. Data Flow
- **Diagram:** [data-flow.mermaid.md](./data-flow.mermaid.md)
- **Notes:** [data-flow.notes.md](./data-flow.notes.md)
- **Content:** How data moves through the system including thread interactions and event flows

### 5. Entry Points
- **Diagram:** [entry-points.mermaid.md](./entry-points.mermaid.md)
- **Notes:** [entry-points.notes.md](./entry-points.notes.md)
- **Content:** All ways to interact with the codebase (CLI, GUI, APIs, drivers)

### 6. Deployment Infrastructure
- **Diagram:** [deployment-infrastructure.mermaid.md](./deployment-infrastructure.mermaid.md)
- **Notes:** [deployment-infrastructure.notes.md](./deployment-infrastructure.notes.md)
- **Content:** CI/CD pipeline, build system, testing infrastructure, and deployment architecture

## How to Use These Diagrams

### Viewing Mermaid Diagrams

**Option 1: GitHub (Inline Rendering)**
GitHub automatically renders Mermaid diagrams in markdown files. Simply view the `.mermaid.md` files directly on GitHub.

**Option 2: mermaid.live (Interactive Editor)**
1. Go to https://mermaid.live
2. Copy the entire contents of a `.mermaid.md` file
3. Paste into the editor
4. View, edit, and export

**Option 3: VS Code (Preview)**
1. Install the "Markdown Preview Mermaid Support" extension
2. Open any `.mermaid.md` file
3. Use Ctrl+Shift+V (Cmd+Shift+V on macOS) to preview

**Option 4: Local Mermaid CLI**
```bash
npm install -g @mermaid-js/mermaid-cli
mmdc -i repo-structure.mermaid.md -o repo-structure.png
```

### Reading the Documentation

Each `.notes.md` file provides:
- Detailed explanations of the architecture shown in diagrams
- Key architectural decisions and rationale
- Important patterns and conventions
- Areas of technical debt or complexity
- Common workflows and use cases
- Where to look when making specific types of changes
- Links to related diagrams and documentation

### Navigation Tips

**Start here if you're:**
- **New to the codebase** → Begin with [architecture-overview](./architecture-overview.notes.md)
- **Looking for specific components** → See [component-map](./component-map.notes.md)
- **Debugging data flow issues** → Check [data-flow](./data-flow.notes.md)
- **Integrating with the SDK** → Review [entry-points](./entry-points.notes.md)
- **Setting up CI/CD** → Read [deployment-infrastructure](./deployment-infrastructure.notes.md)
- **Understanding project structure** → Study [repo-structure](./repo-structure.notes.md)

## Key Architectural Principles

The Orpheus SDK is built on four non-negotiable principles:

1. **Offline-first** – No runtime network calls for core features
2. **Deterministic** – Same input → same output, always (bit-identical)
3. **Host-neutral** – Core SDK works across all environments
4. **Broadcast-safe** – 24/7 reliability, no audio thread allocations

These principles guide all architectural decisions documented in these wireframes.

## Version History

### v2025-11-08 (Current)
- Initial comprehensive wireframe documentation
- 6 diagram sets (12 files total)
- Covers: repo structure, architecture, components, data flow, entry points, deployment
- ~40,000 words of detailed explanatory documentation

## Related Documentation

**Core references:**
- [ARCHITECTURE.md](../../ARCHITECTURE.md) - Full architecture document
- [ROADMAP.md](../../ROADMAP.md) - Development timeline
- [CLAUDE.md](../../CLAUDE.md) - Development conventions

**Implementation plans:**
- [ORP068 Implementation Plan](../../docs/ORP/ORP068%20Implementation%20Plan%20(v2.0).md) - Driver architecture
- [ORP069](../../docs/ORP/ORP069.md) - OCC-aligned enhancements

**Application documentation:**
- [Clip Composer Docs](../../apps/clip-composer/docs/OCC/) - OCC documentation

## Contributing to Wireframes

### When to Update Wireframes

Update these diagrams when:
- Major architectural changes are made
- New components are added to the system
- Data flow patterns change significantly
- New entry points or deployment targets are added
- CI/CD pipeline is restructured

### Creating New Wireframe Versions

When making significant updates:
1. Create new version directory: `wireframes/vYYYY-MM-DD/`
2. Copy and modify existing diagrams
3. Update version history in README
4. Keep old versions for historical reference

### Diagram Guidelines

**For `.mermaid.md` files:**
- Pure Mermaid syntax only (no markdown fences)
- Start with `%%` comments for context (2-3 lines max)
- Must work in mermaid.live without modification
- Use detailed node labels with `<br/>` for line breaks
- Organize with subgraphs for clarity

**For `.notes.md` files:**
- Provide detailed context and explanations
- Include "When to modify" sections
- Link to related code files with line numbers
- Reference other diagrams when relevant
- Keep language clear and concise

## Feedback and Questions

If you have questions or suggestions about these wireframes:
- Open an issue on GitHub with the `documentation` label
- Contact the SDK team via Discord/Slack
- Submit a PR with improvements

## License

These wireframes are part of the Orpheus SDK and are licensed under the MIT License.

---

**Last Updated:** 2025-11-08
**Maintained By:** SDK Core Team
**Next Review:** After major architectural changes
