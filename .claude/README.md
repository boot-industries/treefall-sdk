# Orpheus SDK - Claude Development Documentation

This directory contains development notes, progress tracking, and session reports for AI-assisted development of the Orpheus SDK.

---

## Quick Navigation

### Active Documents (ORP068)
- **[progress.md](progress.md)** - ✅ ORP068 implementation tracking (Phase 3 complete, 55/104 tasks)
- **[orp070-progress.md](orp070-progress.md)** - ORP070 OCC MVP Sprint progress tracking
- **[m2_implementation_progress.md](m2_implementation_progress.md)** - M2 Real-Time Infrastructure progress

### Session Reports
- **[SESSION_2025-10-12.md](SESSION_2025-10-12.md)** - Complete extended session report (~3,200 lines implemented)
- **[session-notes.md](session-notes.md)** - Historical session notes archive

### Validation Reports
- **[VALIDATION_COMPLETE.md](VALIDATION_COMPLETE.md)** - ✅ Latest validation status (Oct 12, 2025)
- **[DEBUG_REPORT.md](DEBUG_REPORT.md)** - Comprehensive validation report

### Project Documentation
- **[../CLAUDE.md](../CLAUDE.md)** - Claude Code development guide (for AI assistants)
- **[../AGENTS.md](../AGENTS.md)** - General AI coding assistant guidelines
- **[../ROADMAP.md](../ROADMAP.md)** - Project milestones and timeline
- **[../docs/ORP/](../docs/ORP/)** - Orpheus Reference Plans (ORP) index
- **[../apps/clip-composer/docs/OCC/](../apps/clip-composer/docs/OCC/)** - Orpheus Clip Composer design docs

---

## Latest Status (October 13, 2025)

### ORP068 SDK Integration Plan v2.0
**Status:** ✅ Phase 3 Complete (52.9% overall - 55/104 tasks)

**Phase Progress:**
- ✅ **Phase 0:** Repository consolidation (15/15 tasks, 100%)
- ✅ **Phase 1:** Driver development (23/23 tasks, 100%)
- ✅ **Phase 2:** Expanded contract + UI (11/11 tasks, 100%)
- ✅ **Phase 3:** CI/CD infrastructure (6/6 tasks, 100%) **JUST COMPLETED** 🎉
- ⏳ **Phase 4:** Documentation & productionization (0/14 tasks)

**Phase 3 Achievements:**
- Unified CI pipeline (7 parallel jobs, matrix builds across 3 OS × 2 build types)
- Performance budget enforcement (bundle size validation with 15% tolerance)
- Chaos testing (nightly failure scenario testing)
- Dependency graph integrity (783 files scanned, no circular dependencies)
- Security auditing (NPM audit, OSV scanner, SBOM generation)
- Pre-merge validation (Husky hooks, lint-staged, commitlint, PR template)

**Key Deliverables:**
- @orpheus/contract (JSON schema system)
- @orpheus/engine-service (Node.js service driver)
- @orpheus/engine-native (N-API native driver)
- @orpheus/engine-wasm (WASM driver with SRI security)
- @orpheus/client (unified client broker)
- @orpheus/react (React integration hooks)

**Next Phase:**
- Phase 4: Documentation cleanup, release preparation, changelog generation

**Progress Tracking:** See [progress.md](progress.md) for detailed task breakdown

---

### ORP070 OCC MVP Sprint
**Status:** Active (Parallel with ORP068)
**Focus:** Real-time audio infrastructure for Orpheus Clip Composer

**Scope:**
- Phase 1: Platform audio drivers (CoreAudio, WASAPI) + real audio mixing (Months 1-2)
- Phase 2: Multi-channel routing matrix (4 Clip Groups → Master) (Months 3-4)
- Phase 3: Performance monitor, optimization, stability testing (Months 5-6)

**Progress Tracking:** See [orp070-progress.md](orp070-progress.md)

---

### M2 Real-Time Infrastructure (Legacy)
**Status:** ✅ Foundation Complete (Phase 1, Modules 1-2-4)

**Implemented:**
- Module 1: Transport Controller (15 tests ✅)
- Module 2: Audio File Reader (5 tests ✅)
- Module 4: Dummy Audio Driver (11 tests ✅)
- Integration: Full pipeline verified (6 tests ✅)

**Test Coverage:** 46/47 passing (98%)

---

## Document Guide

### For Resuming Work on ORP068
1. Read **[progress.md](progress.md)** for current phase and tasks
2. Check **[../docs/ORP/ORP068 Implementation Plan (v2.0).md](../docs/ORP/ORP068%20Implementation%20Plan%20(v2.0).md)** for full plan
3. Review recent commits: `git log --oneline -10`
4. Check CI status on active PRs

### For Resuming Work on ORP070
1. Read **[orp070-progress.md](orp070-progress.md)** for current sprint status
2. Check **[../docs/ORP/ORP070 OCC MVP Sprint.md](../docs/ORP/ORP070%20OCC%20MVP%20Sprint.md)** for detailed plan
3. Review **[../apps/clip-composer/docs/OCC/OCC027 API Contracts.md](../apps/clip-composer/docs/OCC/OCC027%20API%20Contracts.md)** for interface specs

### For Understanding Architecture
1. See **[../ARCHITECTURE.md](../ARCHITECTURE.md)** for system overview
2. Read **[../docs/DRIVER_ARCHITECTURE.md](../docs/DRIVER_ARCHITECTURE.md)** for driver details
3. Check **[../docs/CONTRACT_DEVELOPMENT.md](../docs/CONTRACT_DEVELOPMENT.md)** for contract guide
4. Review public headers in `../include/orpheus/`

### For Code Context
1. Session reports contain full implementation narrative
2. Progress docs track phase-by-phase status
3. Validation reports verify correctness
4. ORP documents provide authoritative requirements

---

## File Descriptions

### progress.md
**Purpose:** ORP068 implementation tracking (primary progress document)
**Contents:**
- Phase-by-phase status (0-4)
- Task completion tracking (55/104)
- Phase summaries with validation results
- Key files, commands, and recent commits
- Critical path overview
- Notes for AI assistants
**Update Frequency:** After each major milestone or phase completion

### orp070-progress.md
**Purpose:** ORP070 OCC MVP Sprint tracking
**Contents:**
- Sprint phase breakdown
- Module implementation status
- Interface specifications
- Integration checkpoints
- Performance targets
**Update Frequency:** Weekly during active sprint work

### m2_implementation_progress.md
**Purpose:** Detailed progress tracking for Milestone M2 (legacy, superseded by ORP070)
**Contents:**
- Phase-by-phase task lists
- Module status (completed/pending)
- File inventory
- Session accomplishments
**Update Frequency:** Archived (see orp070-progress.md for current work)

### SESSION_2025-10-12.md
**Purpose:** Complete narrative of extended session work (M2 foundation)
**Contents:**
- Executive summary
- Module implementation details
- Integration architecture
- Test results
- File inventory
- Technical highlights
**Length:** ~3,000 lines
**Use Case:** Historical reference for M2 foundation work

### VALIDATION_COMPLETE.md
**Purpose:** Sign-off document confirming validation checks passed
**Contents:**
- Test results summary
- Code quality verification
- Documentation completeness
- Known limitations
- Final assessment
**Update Frequency:** After major validation checkpoints

### DEBUG_REPORT.md
**Purpose:** Comprehensive validation and debugging report
**Contents:**
- Test results breakdown
- Code quality checks
- Architecture validation
- Performance characteristics
- Security checks
**Use Case:** Verifying correctness before moving forward

---

## Development Workflow

### Starting a New Session (ORP068)
1. Check `progress.md` for current phase and next tasks
2. Review PR status on GitHub (phase-3-complete branch)
3. Check CI pipeline status
4. Reference ORP068 implementation plan for task details
5. Update progress.md as you work

### Starting a New Session (ORP070)
1. Check `orp070-progress.md` for current sprint phase
2. Review ORP070 OCC MVP Sprint document for requirements
3. Check OCC API contracts for interface specs
4. Update orp070-progress.md as you work

### Completing a Session
1. Run all tests: `ctest --test-dir build`
2. Check linting: `pnpm run lint:js && pnpm run lint:cpp`
3. Update progress document with accomplishments
4. Create session report (if significant work done)
5. Commit and push changes

### Before Moving to Next Phase (ORP068)
1. Ensure all phase tasks complete
2. Verify CI pipeline passing
3. Update documentation (CLAUDE.md, docs/ORP/README.md)
4. Create PR with comprehensive summary
5. Update progress.md with phase completion

---

## Metrics (As of Oct 13, 2025)

### ORP068 Progress
```
Phase 0:              15/15 tasks (100%) ✅
Phase 1:              23/23 tasks (100%) ✅
Phase 2:              11/11 tasks (100%) ✅
Phase 3:              6/6 tasks (100%) ✅
Phase 4:              0/14 tasks (0%)
Overall:              55/104 tasks (52.9%)
```

### Code Quality
```
Tests:                All passing (C++ + TypeScript)
Linting:              Zero errors (C++ clang-format + TypeScript ESLint)
Security:             0 high severity vulnerabilities
Performance:          Bundle sizes within budget
Dependencies:         No circular dependencies (783 files scanned)
CI Status:            All checks passing (21+ jobs)
```

### Package Ecosystem
```
Published Packages:   @orpheus/contract, @orpheus/engine-service,
                      @orpheus/engine-native, @orpheus/engine-wasm,
                      @orpheus/client, @orpheus/react
Architecture:         Multi-driver (Service, Native, WASM)
Contract Version:     v1.0.0-beta (11 commands, 8 events)
Build Targets:        ubuntu/windows/macos × Debug/Release
```

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

## Common Tasks

### Check ORP068 Status
```bash
cat .claude/progress.md | head -20
```

### Check ORP070 Status
```bash
cat .claude/orp070-progress.md | head -50
```

### Run All Tests
```bash
cd build && ctest --output-on-failure
```

### Run Linting
```bash
pnpm run lint:js
pnpm run lint:cpp
```

### Run Performance Validation
```bash
pnpm run perf:validate
```

### Check Dependency Graph
```bash
pnpm run dep:check
```

### Run Security Audit
```bash
pnpm audit --audit-level=high
```

### View CI Status
```bash
gh pr view --web  # Opens current PR in browser
gh pr checks      # Shows CI check status
```

---

## Documentation Philosophy

### Progress Documents (progress.md, orp070-progress.md)
**Purpose:** Living task tracker for active work
**Audience:** Current developer, AI assistants
**Style:** Structured lists, status indicators, phase summaries
**Update:** Continuously during development

### Session Reports
**Purpose:** Historical record of what was built
**Audience:** Future developers, code reviewers
**Style:** Narrative, technical, comprehensive
**Update:** After significant work (not every commit)

### Validation Reports
**Purpose:** Checkpoint verification
**Audience:** Project leads, maintainers
**Style:** Systematic checks, pass/fail
**Update:** Before major phase transitions

### ORP Documents (Orpheus Reference Plans)
**Purpose:** Authoritative implementation plans
**Audience:** All team members
**Style:** Structured requirements, acceptance criteria
**Update:** Major revisions only (v1.0, v2.0, etc.)

---

## Notes for AI Assistants

### To Resume ORP068 Work:
1. **Always read [progress.md](progress.md) first** - current phase and tasks
2. **Check [../docs/ORP/ORP068 Implementation Plan (v2.0).md](../docs/ORP/ORP068%20Implementation%20Plan%20(v2.0).md)** - full plan
3. **Review git log** - `git log --oneline -10` for recent commits
4. **Update docs as you work** - keep progress.md synchronized

### To Resume ORP070 Work:
1. **Always read [orp070-progress.md](orp070-progress.md) first** - current sprint status
2. **Check [../docs/ORP/ORP070 OCC MVP Sprint.md](../docs/ORP/ORP070%20OCC%20MVP%20Sprint.md)** - sprint plan
3. **Review [../apps/clip-composer/docs/OCC/OCC027 API Contracts.md](../apps/clip-composer/docs/OCC/OCC027%20API%20Contracts.md)** - interface specs
4. **Update orp070-progress.md as you work**

### When Implementing New Features:
1. Follow existing patterns in session reports and progress docs
2. Document architectural decisions in progress.md
3. Update CLAUDE.md if new tools/workflows added
4. Add tests (aim for 98%+ coverage)
5. Verify with validation checklist
6. Create session report for significant work

### Before Creating a PR:
1. Ensure all tests pass locally
2. Run linting and fix all errors
3. Update progress.md with completion status
4. Update relevant documentation (CLAUDE.md, docs/ORP/README.md)
5. Write comprehensive PR description with context

---

## Contact & References

**Project Root:** `/Users/chrislyons/dev/orpheus-sdk`
**Main Docs:** `../docs/`
**ORP Docs:** `../docs/ORP/`
**OCC Design Docs:** `../apps/clip-composer/docs/OCC/`
**Build Dir:** `../build/`

**Key Files:**
- `../README.md` - Project overview
- `../ROADMAP.md` - Milestones and timeline
- `../CLAUDE.md` - Claude Code development guide
- `../AGENTS.md` - AI assistant coding guidelines
- `../CMakeLists.txt` - Build configuration
- `../budgets.json` - Performance budgets
- `.github/workflows/ci-pipeline.yml` - Unified CI configuration

**Active Branches:**
- `main` - Production-ready code
- `phase-3-complete` - ORP068 Phase 3 work (active PR)

---

**Last Updated:** October 13, 2025
**ORP068 Status:** Phase 3 Complete (55/104 tasks, 52.9%)
**ORP070 Status:** Active Sprint
**Next Update:** After Phase 4 completion or ORP070 milestone
