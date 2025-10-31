# OCC Documentation Index

**Prefix:** OCC (Orpheus Clip Composer)
**Next Available Number:** OCC102

## Active Documentation

Current, frequently-referenced documents:

### Product & Vision

- [OCC021](./OCC021%20Product%20Vision.md) — Product Vision (broadcast/theater, €500-1,500)
- [OCC026](./OCC026%20MVP%20Plan.md) — 6-month MVP Plan
- [OCC027](./OCC027%20API%20Contracts.md) — API Contracts

### Implementation Reference (NEW)

- [OCC096](./OCC096.md) — SDK Integration Patterns (code examples for OCC + SDK)
- [OCC097](./OCC097.md) — Session Format (JSON schema, loading/saving)
- [OCC098](./OCC098.md) — UI Components (JUCE implementations)
- [OCC099](./OCC099.md) — Testing Strategy (unit/integration tests)
- [OCC100](./OCC100.md) — Performance Requirements (targets, optimization)
- [OCC101](./OCC101.md) — Troubleshooting Guide (common issues, solutions)

### Recent Sprints

- [OCC093](./OCC093%20v020%20Sprint.md) — v0.2.0 Sprint (6 UX fixes complete)
- [OCC094](./OCC094.md) — Clip Edit Dialog Layout Redesign
- [OCC095](./OCC095.md) — ColorSwatchPicker Crash Fix

For complete list, see: `ls -1 apps/clip-composer/docs/occ/OCC*.md`

See also:

- [PROGRESS.md](../../PROGRESS.md) — Current implementation status (app root)
- [README.md](../../README.md) — Public-facing overview (app root)
- [CLAUDE.md](../../CLAUDE.md) — Development guide (app root)

## Archived Documentation

Documents older than 180 days (moved to `archive/` subdirectory):

**Total Archived:** 33 documents (OCC001-OCC039)

Archive highlights:

- OCC001-010: Early Cursor Agent prompts and wireframes
- OCC011-020: UI design iterations and technical specifications
- OCC021-039: Product vision, session schemas, component architecture

View archived docs: `ls -1 apps/clip-composer/docs/occ/archive/`

## Archive Policy

- Documents older than 180 days are automatically archived
- Run: `~/dev/scripts/archive-old-docs.sh orpheus-sdk OCC` (Note: uses repo root, not apps/ subdirectory)
- Archived docs remain accessible in `apps/clip-composer/docs/occ/archive/`
- Claude Code excludes archived docs from context to optimize token usage

## Creating New Documents

1. Use next available number: **OCC102**
2. Follow naming pattern: `OCC102 Title Here.md` or `OCC102.md`
3. Use IEEE citation style for references
4. Update this INDEX.md with the new document
5. Increment "Next Available Number" above
