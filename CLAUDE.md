# Orpheus SDK Development Guide

**Workspace:** Inherits conventions from `~/chrislyons/dev/CLAUDE.md`
**Best Practices:** See `~/dev/docs/CLAUDE_CODE_BEST_PRACTICES.md` for comprehensive Claude Code workflow guidance

Professional audio SDK with host-neutral C++20 core for deterministic session, transport, and render management.

## Configuration Hierarchy

This repository follows a three-tier configuration hierarchy:

1. **This file (CLAUDE.md)** — Repository-specific rules and conventions
2. **Workspace config** (`~/chrislyons/dev/CLAUDE.md`) — Cross-repo patterns
3. **Global config** (`~/.claude/CLAUDE.md`) — Universal rules

**Conflict Resolution:** Repo > Workspace > Global > Code behavior

## Core Principles

1. **Offline-first** — No runtime network calls for core features
2. **Deterministic** — Same input → same output, always (sample-accurate, bit-identical)
3. **Host-neutral** — Core SDK works across REAPER, standalone apps, plugins, embedded
4. **Broadcast-safe** — 24/7 reliability, no audio thread allocations

## Repository Structure

```
├── src/, include/       # Core SDK (C++20) - deterministic, portable
├── adapters/            # Host integrations (REAPER, minhost, realtime_engine, clip_grid)
├── apps/                # Applications (Clip Composer, Wave Finder, FX Engine)
├── tests/               # GoogleTest suite
└── docs/                # Architecture, roadmap, adapter guides
```

## Quick Command Reference

**Full command list:** `docs/repo-commands.html` (click-to-copy commands for build, test, git, diagnostics)

### Common Commands

**Build:**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

**Test:**

```bash
ctest --test-dir build --output-on-failure
```

**Run Clip Composer:**

```bash
./scripts/relaunch-occ.sh
```

For all commands, see `docs/repo-commands.html` (open in browser for full reference with click-to-copy)

## Audio Code Rules

**Determinism:**

- 64-bit sample counts (never float seconds)
- Bit-identical output across platforms
- `std::bit_cast` for float determinism

**Broadcast-Safe:**

- No audio thread allocations
- No render path network calls
- Lock-free structures, pre-allocate
- Graceful degradation

**Quality:**

- C++20, passes `clang-format` (CI enforced)
- AddressSanitizer + UBSan on Debug
- Adapters ≤300 LOC when possible

## File Placement

| Type     | Location           | Example                  |
| -------- | ------------------ | ------------------------ |
| Core     | `src/`, `include/` | `SessionGraph.cpp`       |
| Adapters | `adapters/`        | `reaper_adapter.cpp`     |
| Apps     | `apps/`            | `orpheus_clip_composer/` |
| Docs     | `docs/`            | `ARCHITECTURE.md`        |

## Decision Framework

1. **Will this work offline?** → If no, wrong for core
2. **Is this deterministic?** → If no, not in render path
3. **Is this host-neutral?** → If no, belongs in adapter
4. **For all applications?** → If no, it's app-specific

## Do / Don't

### ✅ Do

- Maintain host-neutral determinism
- Sample-accurate rendering, clock isolation
- Keep UI self-contained/mockable
- Document flags/ports/env vars
- Write unit tests, profile first
- Follow `.clang-format`/`.clang-tidy`
- Add Doxygen for public APIs

### ❌ Don't

- Break core/adapter abstraction
- Depend on SaaS for functionality
- Allocate in audio threads
- Use floating-point time
- Commit secrets/binaries
- Add network to core features
- Modify CMake/CI for network access

## Example Workflows

### Debugging Audio Thread Issues

1. Check threading model (Message/Audio/Background threads in `docs/ARCHITECTURE.md`)
2. Verify no allocations in audio callback (`rt.safety.auditor` skill or manual review)
3. Use ORP### docs for architecture decisions (`docs/orp/`)
4. Test with multiple buffer sizes (128, 256, 512, 1024 samples)
5. Run with sanitizers: `cmake -B build -DCMAKE_BUILD_TYPE=Debug && ctest --test-dir build`

## CI/CD (Phase 3 Complete ✅)

**Pipeline:** Matrix builds (ubuntu/windows/macos × Debug/Release)

- 7 parallel jobs: C++ build/test, lint, native driver, TypeScript, integration, deps, perf
- Sanitizers (ASan/UBSan) on Debug
- Performance budgets enforced
- Chaos tests (nightly)
- Pre-commit hooks (Husky)

## ORP068 Status

**Progress:** 55/104 tasks (52.9%)

- ✅ Phase 0-3 (Repo, Driver, UI, CI)
- ⏳ Phase 4: Docs/productionization (0/14)

**Resume:** Check `.claude/implementation_progress.md` + `docs/ORP/ORP068 Implementation Plan (v2.0).md`

## OCC - Clip Composer

**Status:** v0.2.0 Sprint Complete (OCC093) ✅
**Docs:** `apps/clip-composer/docs/OCC/` (12 docs, ~6,000 lines)

**Recent Release:**

- v0.1.0-alpha (October 22, 2025)
- v0.2.0-alpha (pending QA - October 28, 2025)

**Key Docs:**

- `OCC021` - Product vision (broadcast/theater, €500-1,500)
- `OCC026` - 6-month MVP plan
- `OCC027` - API contracts
- `OCC093` - v0.2.0 Sprint (6 UX fixes complete)

**Timeline:** MVP at 6mo, v1.0 at 12mo

## Multi-Instance Usage

This repo supports multiple Claude Code instances for context isolation:

### 1. SDK Instance (Core Library Development)

**Working Directory:** `~/dev/orpheus-sdk` (repo root)

**Focus Areas:**

- C++ core library (`src/`, `include/`)
- Cross-platform packages (`packages/*`)
- Core transport, routing, session management
- SDK-level tests and benchmarks

**Active Skills:**

- `rt.safety.auditor` - Real-time safety validation
- `test.result.analyzer` - Test output analysis
- `dependency.audit` - Build system health

**Documentation:**

- Primary: `docs/orp/` (ORP prefix)
- Progress: `.claude/implementation_progress.md`
- Architecture: `docs/ARCHITECTURE.md`, `docs/ROADMAP.md`

### 2. Clip Composer Instance (Application Development)

**Working Directory:** `~/dev/orpheus-sdk/apps/clip-composer`

**Focus Areas:**

- Tauri desktop application
- JUCE UI components
- Application-specific features
- End-user workflows

**Active Skills:**

- `orpheus.doc.gen` - OCC documentation generation
- `test.analyzer` - Application test analysis
- `ui.component.validator` - UI component validation

**Documentation:**

- Primary: `apps/clip-composer/docs/occ/` (OCC prefix)
- Progress: `apps/clip-composer/.claude/implementation_progress.md`
- App-specific: `apps/clip-composer/ARCHITECTURE.md`

### Instance Isolation

**Benefits:**

- No config file collision (separate `.claude/` directories)
- Context-appropriate skill loading
- Clear documentation boundaries
- Independent progress tracking

**Usage:**

```bash
# SDK development (from repo root)
cd ~/dev/orpheus-sdk
claude-code

# Clip Composer development (from app directory)
cd ~/dev/orpheus-sdk/apps/clip-composer
claude-code

# Or use shortcut script:
~/dev/orpheus-sdk/scripts/start-clip-composer-instance.sh
```

**When to Switch:**

- **Use SDK instance** for core library changes, adapters, transport/routing/session work
- **Use Clip Composer instance** for UI, app features, OCC-specific functionality
- **Coordinate** when changes span both (e.g., new SDK API + UI integration)

## Documentation Indexing

**Active Documentation:**

- `docs/orp/` - Active ORP (Orpheus) documentation
- `apps/clip-composer/docs/occ/` - Active OCC (Clip Composer) documentation
- Root-level docs (`docs/*.md`) - Architecture, guides, API references

**Excluded from Indexing:**

- `docs/orp/archive/**` - Archived ORP documents (63+ days old)
- `apps/clip-composer/docs/occ/archive/**` - Archived OCC documents (63+ days old)
- `docs/deprecated/**` - Deprecated documentation
- `*.draft.md` - Draft documents not yet finalized

**Archive Management:**

- Use `~/dev/scripts/archive-old-docs.sh` to move docs older than 63 days
- Archives preserve history without cluttering active context
- Check `INDEX.md` in each prefix directory for document lists

## File Boundaries

### Never Read

- `build-*/` (multiple build variant directories)
- `apps/*/build/` (application-specific builds)
- `apps/clip-composer/build/` (JUCE build artifacts)
- `orpheus_clip_composer_app_artefacts/` (Tauri build outputs)
- `Third-party/*/build/` (dependency builds)
- `*.o`, `*.a`, `*.dylib`, `*.so` (compiled objects)

### Read First

- `.claude/implementation_progress.md` (current sprint status)
- `docs/orp/ORP068 Implementation Plan (v2.0).md` (master plan)
- `docs/ARCHITECTURE.md` (system design)
- `docs/ROADMAP.md` (timeline)
- This `CLAUDE.md` file (repo conventions)

## Token Efficiency Rules

**Debugging:**

- 3 causes + ONE grep per cause BEFORE reading files
- Incremental: `cmake --build build --target file.cpp.o`
- ASan crash on valid code? `rm -rf build` first

**Git:**

- `git log --oneline -10` (not `--all`)
- Pick ONE commit from message

**Files:**

- `grep -rn 'symbol'` before `Read()`
- Never read same file twice

---

## Skill Loading (Context-Aware)

Skills are lazy-loaded based on file patterns to reduce context overhead:

**Template-Based Skills** (from `~/dev/.claude/skill-templates/`):

- **ci.troubleshooter** → `.github/workflows/**/*.yml` (lazy-loaded)
- **test.analyzer** → `tests/**/*`, `**/*.test.cpp` (lazy-loaded)
- **schema.linter** → `**/*.{json,yaml,yml}` (excludes build, node_modules) (lazy-loaded)
- **dependency.audit** → `CMakeLists.txt`, `package.json`, `pnpm-lock.yaml` (triggers on change)
- **doc.standards** → `docs/orp/**/*.md`, `apps/clip-composer/docs/occ/**/*.md` (lazy-loaded)

**Skip Skills For:**

- Quick edits (<5 min, single file changes)
- Read-only exploration
- Docs-only sessions without code changes

**Config:** See `.claude/skills.json` for file pattern mappings and template references.

---

**DON'T USE OPEN TO RUN CLIP COMPOSER** - Always run launch script after build.
**Application rebuilds should be performed manually by the user always.**
