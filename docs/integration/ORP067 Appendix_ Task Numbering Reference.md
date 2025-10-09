# XIV. APPENDIX: TASK NUMBERING REFERENCE (COMPLETE)

For integration with project management tools (Jira, Linear, etc.), sequential numbering:

| Task ID | Description | Phase | Domain | Dependencies |
| --- | --- | --- | --- | --- |
| TASK-001 | Initialize Monorepo Workspace | P0 | REPO | None |
| TASK-002 | Import Shmui Codebase | P0 | REPO | 001 |
| TASK-003 | Configure Package Namespacing | P0 | REPO | 002 |
| TASK-004 | Preserve Orpheus C++ Build | P0 | REPO | 001 |
| TASK-005 | Unify Linting Configuration | P0 | REPO | 002 |
| TASK-006 | Initialize Changesets | P0 | REPO | 003 |
| TASK-007 | Create Bootstrap Script | P0 | REPO | 001, 004 |
| TASK-008 | Create Interim CI | P0 | CI | 004, 005 |
| TASK-009 | Configure Branch Protection | P0 | CI | 008 |
| TASK-010 | Create Documentation Index | P0 | DOC | None |
| TASK-011 | Document Package Naming | P0 | DOC | 003 |
| TASK-096 | Implement Validation Script Suite | P0 | TEST | Phase-dependent |
| TASK-103 | Create Performance Budget Config | P0 | GOV | None |
| TASK-104 | Implement Docs Quick Start Block | P0 | DOC | 010 |
| TASK-012 | Phase 0 Validation | P0 | TEST | ALL P0 |
| TASK-013 | Create Contract Package | P1 | CONT | 003 |
| TASK-014 | Contract Version Roadmap | P1 | CONT | 013 |
| TASK-015 | Implement Minimal Schemas | P1 | CONT | 013 |
| TASK-016 | Contract Schema Automation | P1 | CONT | 015 |
| TASK-097 | Implement Contract Manifest System | P1 | CONT | 016 |
| TASK-017 | Service Driver Foundation | P1 | DRIV | 015 |
| TASK-018 | Service Command Handler | P1 | DRIV | 017, 004 |
| TASK-019 | Service Event Emission | P1 | DRIV | 018 |
| TASK-099 | Implement Service Driver Authentication | P1 | GOV | 017 |
| TASK-020 | Native Driver Package | P1 | DRIV | 004 |
| TASK-021 | Native Command Interface | P1 | DRIV | 020 |
| TASK-022 | Native Event Emitter | P1 | DRIV | 021 |
| TASK-023 | Quarantine Legacy REAPER | P1 | REPO | 004 |
| TASK-024 | Create Client Broker | P1 | DRIV | 019, 022 |
| TASK-098 | Implement Driver Selection Registry | P1 | DRIV | 024 |
| TASK-025 | Client Handshake Protocol | P1 | DRIV | 024 |
| TASK-026 | UI Integration Test Hook | P1 | UI | 024 |
| TASK-027 | React OrpheusProvider | P1 | UI | 024 |
| TASK-028 | Extend CI for Drivers | P1 | CI | 018, 021 |
| TASK-029 | Add Integration Smoke Tests | P1 | CI | 024, 028 |
| TASK-030 | Document Driver Architecture | P1 | DOC | 019, 022 |
| TASK-031 | Contract Development Guide | P1 | DOC | 015 |
| TASK-032 | Phase 1 Validation | P1 | TEST | ALL P1 |
| TASK-033 | Initialize WASM Build | P2 | DRIV | 004 |
| TASK-100 | Implement WASM Build Discipline | P2 | DRIV | 033 |
| TASK-034 | WASM Web Worker Wrapper | P2 | DRIV | 033 |
| TASK-035 | WASM Command Interface | P2 | DRIV | 034 |
| TASK-036 | Integrate WASM into Broker | P2 | DRIV | 035, 024 |
| TASK-037 | Upgrade Contract to v1.0.0-beta | P2 | CONT | 015 |
| TASK-038 | Event Frequency Validation | P2 | CONT | 037 |
| TASK-039 | Session Manager Panel | P2 | UI | 037, 027 |
| TASK-040 | Click Track Generator | P2 | UI | 037, 039 |
| TASK-041 | Integrate Orb with Orpheus | P2 | UI | 038, 027 |
| TASK-042 | Track Add/Remove Operations | P2 | UI | 039 |
| TASK-043 | Feature Toggle System | P2 | UI | 039 |
| TASK-044 | Golden Audio Test Suite | P2 | TEST | 037, 036 |
| TASK-102 | Create Golden Audio Manifest | P2 | TEST | 044 |
| TASK-045 | Contract Compliance Matrix | P2 | TEST | 037, 036 |
| TASK-046 | Performance Instrumentation | P2 | TEST | 036 |
| TASK-047 | IPC Stress Testing | P2 | TEST | 022 |
| TASK-048 | Contract Fuzz Testing | P2 | TEST | 037 |
| TASK-049 | Thread Safety Audit | P2 | TEST | 022 |
| TASK-050 | Golden Audio Tests in CI | P2 | CI | 044 |
| TASK-051 | Compliance Tests in CI | P2 | CI | 045 |
| TASK-052 | Document UI Integration | P2 | DOC | 040 |
| TASK-053 | Update Storybook Stories | P2 | DOC | 040, 041 |
| TASK-054 | Phase 2 Validation | P2 | TEST | ALL P2 |
| TASK-055 | Consolidate CI Pipelines | P3 | CI | 051 |
| TASK-056 | Performance Budget Enforcement | P3 | CI | 046 |
| TASK-057 | Chaos Testing in CI | P3 | CI | 045 |
| TASK-058 | Dependency Graph Check | P3 | CI | 055 |
| TASK-059 | Security and SBOM Audit | P3 | CI | 055 |
| TASK-060 | Pre-Merge Contributor Validation | P3 | CI | 055 |
| TASK-061 | Optimize Bundle Size | P3 | REPO | 036 |
| TASK-062 | WASM in Web Worker | P3 | REPO | 035 |
| TASK-063 | Optimize Native Binary Size | P3 | REPO | 021 |
| TASK-064 | Implement @orpheus/cli | P3 | TOOL | 016 |
| TASK-065 | Configure Changesets | P3 | GOV | 006 |
| TASK-066 | NPM Publish Workflow | P3 | GOV | 065 |
| TASK-067 | Generate Prebuilt Binaries | P3 | GOV | 063 |
| TASK-101 | Validate Binary Signing UX | P3 | GOV | 067 |
| TASK-068 | Binary Verification | P3 | GOV | 067 |
| TASK-069 | Contract Diff Automation | P3 | GOV | 016 |
| TASK-070 | Generate SBOM and Provenance | P3 | GOV | 067 |
| TASK-071 | Achieve Coverage Targets | P3 | TEST | 045 |
| TASK-072 | End-to-End Testing | P3 | TEST | 042 |
| TASK-073 | Memory Leak Detection | P3 | TEST | 062 |
| TASK-074 | Binary Compatibility Tests | P3 | TEST | 067 |
| TASK-075 | Complete Migration Guide | P3 | DOC | 053 |
| TASK-076 | Finalize API Documentation | P3 | DOC | 037 |
| TASK-077 | Create Contributor Guide | P3 | DOC | 055 |
| TASK-078 | Update Main README | P3 | DOC | 071, 065 |
| TASK-079 | Create API Surface Index | P3 | DOC | 076 |
| TASK-080 | Automate Docs Index Graph | P3 | DOC | 010 |
| TASK-081 | Phase 3 Validation | P3 | TEST | ALL P3 |
| TASK-082 | Publish Beta Release | P4 | GOV | 081 |
| TASK-083 | Archive Old Repositories | P4 | GOV | 082 |
| TASK-084 | Decommission Legacy CI | P4 | REPO | 083 |
| TASK-085 | Establish Support Channels | P4 | GOV | 082 |
| TASK-086 | Telemetry with Consent Guard | P4 | GOV | 082 |
| TASK-087 | Incident Response Plan | P4 | GOV | 082 |
| TASK-088 | Conduct Beta Testing | P4 | GOV | 085 |
| TASK-089 | Publish Stable v1.0.0 | P4 | GOV | 088 |
| TASK-090 | Upgrade Path Documentation | P4 | DOC | 089 |
| TASK-091 | Release Blog Post | P4 | DOC | 089 |
| TASK-092 | Adapter Plugin Interface | P4 | REPO | 081 |
| TASK-093 | Remove Feature Toggles | P4 | GOV | 089 |
| TASK-094 | Conduct Retrospective | P4 | GOV | 093 |
| TASK-095 | Phase 4 Validation | P4 | TEST | ALL P4 |

***

## Task Summary by Phase

| Phase | Task Range | Count | Percentage |
| --- | --- | --- | --- |
| Phase 0 | TASK-001 to TASK-012, 096, 103, 104 | 15 | 14.4% |
| Phase 1 | TASK-013 to TASK-032, 097, 098, 099 | 23 | 22.1% |
| Phase 2 | TASK-033 to TASK-054, 100, 102 | 24 | 23.1% |
| Phase 3 | TASK-055 to TASK-081, 101 | 28 | 26.9% |
| Phase 4 | TASK-082 to TASK-095 | 14 | 13.5% |
| **Total** |  | **104** | **100%** |

***

## Task Summary by Domain

| Domain | Count | Primary Phases | Key Tasks |
| --- | --- | --- | --- |
| REPO | 12 | P0, P1, P3, P4 | Monorepo setup, legacy cleanup, optimization |
| DRIV | 19 | P1, P2 | Service, Native, WASM driver implementations |
| CONT | 9 | P1, P2 | Contract schemas, versioning, manifest |
| CI | 13 | P0, P1, P2, P3 | Pipeline setup, testing, validation |
| DOC | 15 | P0, P1, P2, P3, P4 | Guides, API docs, quick start |
| GOV | 19 | P0, P1, P3, P4 | Security, signing, releases, compliance |
| UI | 7 | P1, P2 | React integration, session manager, features |
| TEST | 11 | P0, P1, P2, P3, P4 | Validation, golden audio, stress testing |
| TOOL | 1 | P3 | CLI utility |
| **Total** | **104** |  |  |

***

## Domain Distribution Analysis

**Engineering-Heavy Domains** (68 tasks, 65.4%):

- DRIV (19): Driver implementations across three platforms
- GOV (19): Governance, security, release infrastructure
- CI (13): Continuous integration and validation
- TEST (11): Quality assurance and validation
- REPO (12): Repository structure and optimization

**Documentation & Tools** (16 tasks, 15.4%):

- DOC (15): Comprehensive documentation suite
- TOOL (1): CLI utility

**Integration Domains** (20 tasks, 19.2%):

- CONT (9): Contract management and schemas
- UI (7): User interface features
- Plus cross-cutting concerns (validation, testing)

***

## Critical Path Highlights

**Phase 0 Foundation** (15 tasks):

- Core: TASK-001 (Monorepo) → TASK-007 (Bootstrap) → TASK-012 (Validation)
- New: TASK-096 (Scripts), TASK-103 (Budgets), TASK-104 (Docs entry)

**Phase 1 Integration** (23 tasks):

- Core: TASK-013 (Contract) → TASK-024 (Broker) → TASK-032 (Validation)
- New: TASK-097 (Manifest), TASK-098 (Driver registry), TASK-099 (Auth)

**Phase 2 Features** (24 tasks):

- Core: TASK-033 (WASM) → TASK-044 (Golden audio) → TASK-054 (Validation)
- New: TASK-100 (WASM discipline), TASK-102 (Audio manifest)

**Phase 3 Production** (28 tasks):

- Core: TASK-055 (CI unify) → TASK-067 (Binaries) → TASK-081 (Validation)
- New: TASK-101 (Signing UX)

**Phase 4 Release** (14 tasks):

- Core: TASK-082 (Beta) → TASK-089 (Stable) → TASK-095 (Validation)

***

## Amendments from ORP066

**New Tasks Added**: 9 (TASK-096 through TASK-104)

- P0: 3 tasks (validation scripts, budgets, docs quick start)
- P1: 3 tasks (contract manifest, driver registry, auth)
- P2: 2 tasks (WASM discipline, golden audio manifest)
- P3: 1 task (binary signing UX validation)

**Tasks Amended**: 10

- TASK-003, 007, 016, 017, 020, 024, 033, 044, 056, 067
- Amendments include: package naming corrections, enhanced acceptance criteria, manifest integration, security hardening

**Total Task Increase**: +9 tasks (95 → 104) **Documentation Artifacts Added**: 8 new repository files **Package Renames**: 2 (`@orpheus/ui` → `@orpheus/shmui`, `engine-electron` → `engine-native`)

***

## Usage Notes

**For Project Managers**:

- Use Phase-based filtering to stage work across sprints
- Domain grouping enables team specialization (C++ team on DRIV/REPO, Web team on UI/DOC)
- Critical path identification helps resource allocation

**For Engineers**:

- Dependencies column shows blocking tasks (must complete prerequisites first)
- Domain tags help locate related work
- Phase validation tasks (012, 032, 054, 081, 095) are integration checkpoints

**For QA**:

- TEST domain tasks (11 total) are primary quality gates
- Phase validations aggregate multiple test categories
- Golden audio (TASK-044, 102) requires special attention to determinism

**For Release Management**:

- GOV domain tasks (19 total) control release pipeline
- TASK-082 (Beta) and TASK-089 (Stable) are release milestones
- TASK-066 (NPM workflow) gates all public releases

***

**Document Version**: 1.1 (incorporating ORP065 amendments)
**Last Updated**: 2025-01-XX
**Status**: Normative reference for ORP064 implementation