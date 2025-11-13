# ORP Documentation Index

**Prefix:** ORP (Orpheus SDK)
**Next Available Number:** ORP111

## Active Documentation

Current, frequently-referenced documents. See individual files for full titles.

Recent documents (ORP090-ORP110):

- [ORP068](./ORP068.md) — Implementation Plan (v2.0) - Current roadmap
- [ORP099](./ORP099.md) — SDK Track Phase 4 Completion and Testing
- [ORP100](./ORP100.md) — Unit Tests Implementation Report
- [ORP101](./ORP101.md) — Phase 4 Completion Report
- [ORP109](./ORP109 SDK Feature Roadmap for Clip Composer Integration.md) — SDK Feature Roadmap for Clip Composer Integration
- [ORP110](./ORP110 ORP109 Implementation Report.md) — ORP109 Implementation Report (7 features complete)

For complete list, see: `ls -1 docs/orp/ORP*.md`

## Archived Documentation

Documents older than 180 days (moved to `archive/` subdirectory):

**Total Archived:** 18 documents (ORP061-ORP081)

Archive highlights:

- ORP061-066: Early planning and architecture
- ORP067-076: Layer development and latency audits
- ORP077-081: Sprint reports and implementation updates

View archived docs: `ls -1 docs/orp/archive/`

## Archive Policy

- Documents older than 180 days are automatically archived
- Run: `~/dev/scripts/archive-old-docs.sh orpheus-sdk ORP`
- Archived docs remain accessible in `docs/orp/archive/`
- Claude Code excludes archived docs from context to optimize token usage

## Creating New Documents

1. Use next available number: **ORP111**
2. Follow naming pattern: `ORP098 Title Here.md` or `ORP098.md`
3. Use IEEE citation style for references
4. Update this INDEX.md with the new document
5. Increment "Next Available Number" above
