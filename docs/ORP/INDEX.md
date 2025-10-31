# ORP Documentation Index

**Prefix:** ORP (Orpheus SDK)  
**Next Available Number:** ORP098

## Active Documentation

Current, frequently-referenced documents. See individual files for full titles.

Recent documents (ORP090-ORP097):

- [ORP068](./ORP068.md) — Implementation Plan (v2.0) - Current roadmap
- [ORP090](./ORP090.md)
- [ORP091](./ORP091.md)
- [ORP092](./ORP092.md)
- [ORP093](./ORP093.md)
- [ORP094](./ORP094.md)
- [ORP095](./ORP095.md)
- [ORP096](./ORP096.md)
- [ORP097](./ORP097.md)

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

1. Use next available number: **ORP098**
2. Follow naming pattern: `ORP098 Title Here.md` or `ORP098.md`
3. Use IEEE citation style for references
4. Update this INDEX.md with the new document
5. Increment "Next Available Number" above
