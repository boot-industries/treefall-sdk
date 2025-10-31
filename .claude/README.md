# Orpheus SDK - Claude Development Documentation

This directory contains development notes and session reports for AI-assisted development of the Orpheus SDK.

---

## Quick Navigation

### Archived Documents

- **[archive/progress.md](archive/progress.md)** - ⚠️ ARCHIVED: ORP068 implementation tracking (historical)

### Project Documentation

- **[../CLAUDE.md](../CLAUDE.md)** - Claude Code development guide (for AI assistants)
- **[../docs/ORP/](../docs/ORP/)** - Orpheus Reference Plans (ORP) index
- **[../apps/clip-composer/docs/OCC/](../apps/clip-composer/docs/OCC/)** - Orpheus Clip Composer design docs
- **[../docs/repo-commands.html](../docs/repo-commands.html)** - Command reference (build, test, git)

---

## Current Development Model (October 31, 2025)

### Sprint-Based Development

The project now uses **sprint-level ORP documents** for focused feature work, rather than large phase-based tracking.

**Recent Sprints:**

- **ORP093** (✅ Complete) - SDK Sprint: Trim Point Boundary Enforcement
- **ORP094** (✅ Complete) - Implementation Report for ORP093
- **ORP095-ORP097** - Recent feature work

**Next Available:** ORP098

**Documentation:** See `docs/ORP/INDEX.md` for complete ORP catalog

### OCC (Clip Composer) Development

**Latest Release:** v0.2.0-alpha (October 28, 2025)

**Recent Work:**

- Clip Edit Dialog improvements
- Button UI enhancements
- Documentation consolidation
- Removal of deprecated shmui package

**OCC Docs:** `apps/clip-composer/docs/OCC/` (12 documents)

### Repository Cleanup

**Recent Maintenance:**

- Removed shmui references (deprecated ElevenLabs UI fork)
- Consolidated documentation (removed duplicates)
- Added command reference pages
- Updated validation scripts

---

## Document Guide

### For Starting New Work

1. Check **[../docs/ORP/INDEX.md](../docs/ORP/INDEX.md)** for next available ORP number (currently ORP098)
2. Review recent commits: `git log --oneline -10`
3. Check current branch: `git branch --show-current`
4. Review **[../CLAUDE.md](../CLAUDE.md)** for repository conventions

### For Understanding Architecture

1. **[../CLAUDE.md](../CLAUDE.md)** - Core principles, build commands, development rules
2. **[../docs/repo-commands.html](../docs/repo-commands.html)** - Complete command reference
3. **[../include/orpheus/](../include/orpheus/)** - Public API headers
4. **[../docs/ORP/](../docs/ORP/)** - Feature specifications and design docs

### For OCC (Clip Composer) Work

1. **[../apps/clip-composer/docs/OCC/](../apps/clip-composer/docs/OCC/)** - Application documentation
2. **[../apps/clip-composer/CLAUDE.md](../apps/clip-composer/CLAUDE.md)** - App-specific conventions (if exists)
3. Use separate Claude instance from `apps/clip-composer/` directory for context isolation

---

## Key Files

### .claude/scratch/

**Purpose:** Temporary notes and drafts (git-ignored)
**Contents:** Ephemeral session notes, experimental work, debugging artifacts
**Cleanup:** Delete freely - not tracked in git

### .claude/archive/

**Purpose:** Historical documentation (preserved but not actively referenced)
**Contents:**

- `progress.md` - ORP068 phase-based tracking (archived)
- Other deprecated progress tracking files

### docs/ORP/

**Purpose:** Authoritative feature specifications
**Contents:** Sprint-level design documents (ORP093+)
**Index:** See `docs/ORP/INDEX.md`

### apps/clip-composer/docs/OCC/

**Purpose:** Clip Composer application documentation
**Contents:** UI design, architecture, API contracts
**Count:** 12 active documents

---

## Development Workflow

### Starting New Work

1. Review recent commits: `git log --oneline -10`
2. Check current branch: `git branch --show-current`
3. Identify next ORP number from `docs/ORP/INDEX.md`
4. Create feature branch if needed
5. Review `CLAUDE.md` for build/test commands

### During Development

1. Follow conventions in `CLAUDE.md`
2. Run tests frequently: `ctest --test-dir build --output-on-failure`
3. Check linting: See `docs/repo-commands.html` for commands
4. Document decisions in ORP documents
5. Update `docs/ORP/INDEX.md` when creating new ORPs

### Completing Work

1. Ensure all tests pass
2. Run linting/formatting checks
3. Create ORP document if significant feature
4. Commit with conventional commit format
5. Create PR with clear description

---

## Quick Stats (October 31, 2025)

### Repository Health

- **Build Status:** Passing (C++ Debug/Release)
- **Test Coverage:** 98%+ (GoogleTest suite)
- **Linting:** Zero errors (clang-format + ESLint)
- **Platform Support:** macOS, Linux (Ubuntu), Windows (planned)

### Recent Releases

- **SDK:** v0.2.2 (Trim boundary enforcement - Oct 28, 2025)
- **OCC:** v0.2.0-alpha (Clip Edit improvements - Oct 28, 2025)

### Documentation

- **ORP Documents:** 19 active (ORP082-ORP097), 18 archived (ORP061-ORP081)
- **OCC Documents:** 12 active
- **Next ORP:** ORP098

---

## Quality Standards

All code in this project meets:

- ✅ Zero compiler warnings
- ✅ AddressSanitizer clean
- ✅ UndefinedBehaviorSanitizer clean
- ✅ All tests passing
- ✅ Public APIs documented (Doxygen)
- ✅ Thread safety verified
- ✅ SPDX license headers
- ✅ Conventional commit format (enforced via commitlint)
- ✅ Performance budgets enforced
- ✅ Security audits passing

---

## Claude Skills for Orpheus SDK

The Orpheus SDK includes specialized Claude skills that enforce real-time safety and quality standards automatically.

### Overview

Skills are structured competencies that guide Claude Code in performing specific tasks with deep domain knowledge. Orpheus skills enforce:

- **Real-time safety** (no allocations, no locks, bounded execution)
- **Test quality** (98%+ coverage, sanitizer-clean)
- **Documentation standards** (Doxygen, progress tracking, 8000+ lines)

**Framework:** All skills follow [SKIL003 framework](/Users/chrislyons/dev/SKIL003.md)
**Location:** `.claude/skills/`
**Validation:** Run `.claude/skills/validate.sh`

### Core Skills

#### 1. rt.safety.auditor (Critical)

**Purpose:** Analyze C++ code for real-time safety violations

**Detects:**

- Heap allocations (new, malloc, etc.)
- Locking primitives (mutex, etc.)
- Blocking I/O calls (std::cout, file I/O)
- Unbounded operations
- Improper atomic usage

**Usage:**

```
"Use rt.safety.auditor to check src/modules/m1/transport_controller.cpp"
```

**Files:**

- `project/rt-safety-auditor/SKILL.md` - Main skill definition
- `reference/rt_constraints.md` - Real-time constraints doc
- `reference/banned_functions.md` - Comprehensive banned list
- `reference/allowed_patterns.md` - Safe patterns catalog
- `scripts/check_rt_safety.sh` - Executable audit script
- `examples/` - Safe and violation code examples

**Quality Gate:** Must pass before merging audio thread code

---

#### 2. test.result.analyzer

**Purpose:** Parse test and sanitizer output, identify root causes

**Capabilities:**

- Parse ctest and Google Test output
- Extract sanitizer errors (ASan, TSan, UBSan)
- Identify failing tests by module
- Suggest fixes and root causes
- Track coverage trends

**Usage:**

```
"Analyze the latest test run output"
"Check for AddressSanitizer errors in test output"
```

**Files:**

- `project/test-result-analyzer/SKILL.md` - Main skill
- `reference/test_formats.md` - Test output formats
- `reference/sanitizer_formats.md` - Sanitizer error patterns
- `examples/` - Sample test output and reports

---

#### 3. orpheus.doc.gen

**Purpose:** Generate and maintain comprehensive documentation

**Generates:**

- Doxygen comments for C++ APIs
- Progress reports and session notes
- Implementation tracking documents
- API reference documentation

**Features:**

- Real-time safety annotations
- Thread-safety guarantees
- Usage examples
- Pre/post conditions

**Usage:**

```
"Generate Doxygen comments for the AudioMixer class"
"Create a progress report for Module 3 implementation"
```

**Files:**

- `project/orpheus-doc-gen/SKILL.md` - Main skill
- `templates/doxygen_function.template` - Function comment template
- `templates/doxygen_class.template` - Class comment template
- `templates/progress_report.template.md` - Progress report template
- `templates/session_note.template.md` - Session note template

---

### Shared Skills

#### 4. test.analyzer (Cross-Project)

Shared test analysis skill adapted from SKIL003. Configured for Orpheus via `config.json`:

- Test frameworks: googletest, ctest
- Sanitizers: ASan, TSan, UBSan
- Coverage threshold: 98%
- Module mapping (m1-m4)

#### 5. ci.troubleshooter (Cross-Project)

Shared CI/CD troubleshooting skill with Orpheus-specific error patterns:

- heap-use-after-free → suggests rt.safety.auditor
- data race → suggests atomics/lock-free patterns
- CMake errors → checks dependencies
- Coverage warnings → suggests adding tests

---

### Skill Invocation

Claude Code automatically uses skills based on trigger patterns:

**Automatic Invocation:**

- Real-time code review → rt.safety.auditor
- Test failures → test.result.analyzer
- Undocumented APIs → orpheus.doc.gen
- CI failures → ci.troubleshooter

**Manual Invocation:**

```
"Use rt.safety.auditor to check <file>"
"Use test.result.analyzer on the latest test output"
"Use orpheus.doc.gen to document <class/function>"
```

---

### Quality Standards Enforced

**Real-Time Safety (rt.safety.auditor):**

- ✅ No heap allocations in audio threads
- ✅ No locks or blocking primitives
- ✅ No unbounded operations
- ✅ Sample-accurate timing only
- ✅ Proper atomic memory ordering

**Test Quality (test.result.analyzer):**

- ✅ 98%+ test coverage maintained
- ✅ Sanitizer-clean builds (ASan/TSan/UBSan)
- ✅ All tests passing
- ✅ Failures analyzed with root causes

**Documentation (orpheus.doc.gen):**

- ✅ All public APIs documented (Doxygen)
- ✅ @param, @return, @brief tags present
- ✅ Thread-safety annotations
- ✅ Real-time safety notes
- ✅ Usage examples provided
- ✅ 8000+ lines documentation standard

---

### Validation

Run validation script to verify skills installation:

```bash
.claude/skills/validate.sh
```

**Checks:**

- All SKILL.md files present with valid frontmatter
- Reference documentation exists
- Scripts are executable
- manifest.json is valid JSON
- Orpheus-specific constraints present
- Real-time safety auditor tests pass

**Expected Output:**

```
=== Orpheus SDK Skills Validation ===

--- Checking Directory Structure ---
✓ Manifest file: ./manifest.json
✓ Validation script: ./validate.sh

--- Checking Project Skills ---
✓ rt.safety.auditor SKILL.md
✓ RT constraints reference
✓ Banned functions reference
[...]

--- Testing RT Safety Auditor ---
✓ RT safety auditor correctly detected violations
✓ RT safety auditor correctly passed safe code

=== Validation Summary ===
Total Checks: 25
Errors: 0
Warnings: 0

✓ All Orpheus skills validated successfully!
```

---

### Skill Metrics

**Total Skills:** 5 (3 project-specific, 2 shared)
**Documentation:** ~8,500 lines
**Reference Docs:** 12 files
**Scripts:** 2 executable scripts
**Templates:** 4 documentation templates
**Examples:** 9 code examples

**Critical Skills:** 1 (rt.safety.auditor - blocks merges if violations)

---

### Skill Development

All skills follow the [SKIL003 framework](/Users/chrislyons/dev/SKIL003.md):

**Required Sections:**

- YAML frontmatter (name, description)
- Purpose and domain knowledge
- When to use (trigger patterns)
- Allowed tools and access level
- Expected I/O formats
- Dependencies and examples
- Limitations and validation criteria

**Security Model:**

- **Level 0:** Read-only
- **Level 1:** Local execution (rt.safety.auditor, test.result.analyzer)
- **Level 2:** File modification (orpheus.doc.gen)
- **Level 3+:** Network access (not used in Orpheus)

---

### References

- [SKIL003: Comprehensive SKILL.md Framework](/Users/chrislyons/dev/SKIL003.md)
- `skills/manifest.json` - Complete skill registry
- `skills/validate.sh` - Validation script
- Orpheus real-time constraints: `skills/project/rt-safety-auditor/reference/rt_constraints.md`

---

## Key Architectural Decisions

### Multi-Driver Architecture (ORP068)

**Decision:** Support three driver types (Service, Native, WASM) with unified client broker
**Rationale:** Flexibility for different deployment scenarios (Node.js service, native addon, browser)
**Documentation:** `../docs/DRIVER_ARCHITECTURE.md`

### Contract-First Development

**Decision:** JSON schemas define all command/event interfaces
**Rationale:** Type safety, versioning, cross-language compatibility
**Documentation:** `../docs/CONTRACT_DEVELOPMENT.md`

### Lock-Free Communication

**Decision:** Use atomic operations for UI↔Audio thread commands
**Rationale:** Avoid audio dropouts, maintain real-time guarantees
**Documentation:** `SESSION_2025-10-12.md` → "Lock-Free Command Queue"

### Sample-Accurate Timing

**Decision:** 64-bit atomic sample counter as authority
**Rationale:** Deterministic, portable, no floating-point drift
**Documentation:** `DEBUG_REPORT.md` → "Performance Characteristics"

### Security-First WASM

**Decision:** SRI verification, MIME type checks, same-origin policy
**Rationale:** Prevent WASM tampering, meet security audit requirements
**Documentation:** `../packages/engine-wasm/README.md`

---

## Common Commands

For complete command reference with click-to-copy, see `docs/repo-commands.html`

### Build & Test

```bash
# Build SDK (Debug)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build

# Run all tests
ctest --test-dir build --output-on-failure

# Run specific test
ctest --test-dir build -R TransportControllerTest --output-on-failure
```

### Code Quality

```bash
# Check C++ formatting
clang-format --dry-run --Werror src/**/*.cpp include/**/*.h

# Run linting (if pnpm project exists)
pnpm run lint:js
pnpm run lint:cpp
```

### Git & GitHub

```bash
# Recent work
git log --oneline -10

# Current branch
git branch --show-current

# View PR in browser
gh pr view --web
```

---

## Documentation Types

### ORP Documents (Orpheus Reference Plans)

**Purpose:** Sprint-level feature specifications
**Location:** `docs/ORP/`
**Naming:** `ORP[number] [optional-title].md` (e.g., ORP093.md, ORP094.md)
**Index:** See `docs/ORP/INDEX.md` for catalog
**Style:** Problem statement, technical requirements, acceptance criteria
**Update:** Create new ORP for each significant feature or architectural change

### OCC Documents (Clip Composer Design Docs)

**Purpose:** Application-specific documentation
**Location:** `apps/clip-composer/docs/OCC/`
**Naming:** `OCC[number] [optional-title].md`
**Style:** UI/UX specs, implementation notes, user workflows

### Scratch Notes

**Purpose:** Temporary session notes (git-ignored)
**Location:** `.claude/scratch/`
**Cleanup:** Delete freely - ephemeral content only

---

## Notes for AI Assistants

### Starting a Session

1. **Review recent work:** `git log --oneline -10`
2. **Check current branch:** `git branch --show-current`
3. **Read CLAUDE.md** for repository conventions and build commands
4. **Check ORP INDEX** (`docs/ORP/INDEX.md`) for next available number

### During Development

1. Follow patterns in CLAUDE.md (offline-first, deterministic, host-neutral)
2. Run tests frequently
3. Document significant work in new ORP documents
4. Update `docs/ORP/INDEX.md` when creating new ORPs

### Before Creating a PR

1. All tests pass: `ctest --test-dir build --output-on-failure`
2. Linting clean: Check `docs/repo-commands.html` for commands
3. Create ORP document if feature warrants it
4. Conventional commit format
5. Comprehensive PR description

---

## Repository Structure

**Project Root:** `/Users/chrislyons/dev/orpheus-sdk`

**Key Directories:**

- `src/`, `include/` - Core SDK (C++20)
- `apps/` - Applications (Clip Composer, etc.)
- `adapters/` - Host integrations (REAPER, minhost)
- `tests/` - GoogleTest suite
- `docs/` - Architecture and feature specs
- `build/` - CMake build artifacts (git-ignored)

**Essential Files:**

- `CLAUDE.md` - Development guide (must-read)
- `docs/repo-commands.html` - Command reference
- `CMakeLists.txt` - Build configuration
- `docs/ORP/INDEX.md` - ORP document catalog

**Main Branch:** `main` (production-ready code)

---

**Last Updated:** October 31, 2025
**Current State:** Sprint-based development (ORP093+)
**SDK Version:** v0.2.2
**OCC Version:** v0.2.0-alpha
