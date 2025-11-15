# ORP068 Implementation Plan v2.0: Orpheus SDK × Shmui Integration

**Document Type**: Implementation Specification (Consolidated)

**Status**: Normative

**Version**: 2.0

**Supersedes**: ORP065 v1.1

**Incorporates**: All amendments from ORP066

---

## Executive Summary

This document provides the definitive, consolidated implementation task breakdown for integrating the Shmui React UI into the Orpheus SDK monorepo. All amendments from ORP066 have been fully integrated, including package naming corrections, manifest systems, security hardening, and documentation improvements.

**Total Estimated Tasks**: 104 (consolidated from 95 + 9 new tasks) **Critical Path Duration**: Spans all 4 phases (Phase 0 → Phase 4) **Parallel Workstreams**: Up to 6 concurrent domains per phase **Finish-Line Criteria**: 28 gated requirements

**Consolidation Notes**: This revision incorporates all ORP066 amendments including:

- Corrected package naming (@orpheus/shmui, @orpheus/engine-native)
- Contract and golden audio manifest systems
- Driver selection registry with explicit ordering
- Service driver authentication
- WASM build discipline and SRI verification
- Binary signing UX validation
- Performance budgets configuration
- Enhanced documentation navigation

---

## I. TASK TAXONOMY AND CONVENTIONS

### A. Task Identifier Format

Tasks follow the pattern: `P{Phase}.{Domain}.{Sequence}`

**Phase Codes**:

- `P0` — Preparatory Repository Setup
- `P1` — Tooling Normalization and Basic Integration
- `P2` — Feature Integration & Bridging
- `P3` — Unified Build, Testing, and Release Preparation
- `P4` — Gradual Rollout and Post-Migration Support

**Domain Codes**:

- `REPO` — Repository structure and configuration
- `DRIV` — Driver development (Service, WASM, Native)
- `CONT` — Contract enforcement and schema management
- `CI` — Continuous integration and automation
- `DOC` — Documentation and developer guides
- `GOV` — Governance, security, and compliance
- `UI` — User interface integration
- `TEST` — Testing and validation infrastructure
- `TOOL` — Developer tooling and CLI utilities

### B. Dependency Notation

- `→` Blocking dependency (must complete before dependent task)
- `⇢` Soft dependency (should complete before, but not blocking)
- `‖` Can execute in parallel

### C. Acceptance Criteria Standards

Each task includes:

1. **Functional completion condition** (what is delivered)
2. **Validation method** (how success is verified)
3. **ORP compliance reference** (which specification section is satisfied)

---

## II. PHASE 0: PREPARATORY REPOSITORY SETUP

**Objective**: Establish monorepo structure and import Shmui without altering functionality **Duration Estimate**: 2 weeks **Gate Criteria**: All validation checks from ORP061 §Phase 0 pass

### A. Repository Structure Tasks

#### P0.REPO.001: Initialize Monorepo Workspace (TASK-001)

**Description**: Create top-level `packages/` directory and configure PNPM workspace.

**Acceptance Criteria**:

- [ ] `packages/` directory created at repository root
- [ ] `pnpm-workspace.yaml` configured with `packages/*` glob
- [ ] Root `package.json` contains workspace configuration
- [ ] `pnpm install` executes without errors

**ORP References**: ORP061 §Phase 0 (Monorepo Initialization), ORP063 §II.B

**Tooling Requirements**:

- PNPM ≥8.0
- Node.js ≥18.0

**Dependencies**: None (foundational task)

**Validation Method**:

```javascript
pnpm install
pnpm list --depth 0  # Should show workspace packages

```

---

#### P0.REPO.002: Import Shmui Codebase with History Preservation (TASK-002)

**Description**: Migrate complete Shmui repository into `packages/shmui/` using subtree merge to preserve commit history.

**Acceptance Criteria**:

- [ ] All Shmui code resides in `packages/shmui/`
- [ ] Git history from `chrislyons/shmui` preserved
- [ ] No functional changes to Shmui code
- [ ] Original file structure maintained

**ORP References**: ORP061 §Phase 0 (Import Shmui Codebase)

**Tooling Requirements**:

- Git ≥2.30 (for subtree merge)

**Dependencies**: P0.REPO.001 →

**Validation Method**:

```javascript
git log packages/shmui/  # Verify history preserved
diff -r packages/shmui/ ../shmui-backup/  # Verify content match

```

---

#### P0.REPO.003: Configure Package Namespacing (TASK-003) **[AMENDED]**

**Description**: Establish canonical package names with @orpheus scope, preserving Shmui identity.

**Acceptance Criteria**:

- [ ] `packages/shmui/package.json` name set to `@orpheus/shmui` (preserves original identity)
- [ ] Native driver package named `@orpheus/engine-native` (usable by Electron and Node.js)
- [ ] Scoped package naming documented in `docs/PACKAGE_NAMING.md`
- [ ] Reserved names documented: `@orpheus/core`, `@orpheus/client`, `@orpheus/contract`, `@orpheus/engine-*`
- [ ] NPM scope `@orpheus` registered (if publishing publicly)
- [ ] Internal codename "Shmui" preserved in documentation with explanation

**ORP References**: ORP061 §Phase 0 (Namespacing and Package Names), ORP063 §III.D, ORP066 §II

**Tooling Requirements**:

- NPM account with `@orpheus` scope access

**Dependencies**: P0.REPO.002 →

**Validation Method**:

```javascript
jq '.name' packages/shmui/package.json  # Should output "@orpheus/shmui"
grep "@orpheus/engine-native" packages/engine-native/package.json

```

---

#### P0.REPO.004: Preserve Orpheus C++ Build Structure (TASK-004)

**Description**: Ensure Orpheus C++ code remains buildable at current location without path breakage.

**Acceptance Criteria**:

- [ ] All CMakeLists.txt files function unchanged
- [ ] Include paths resolve correctly
- [ ] GoogleTest suite runs successfully
- [ ] Adapters (minhost, optional REAPER) build successfully

**ORP References**: ORP061 §Phase 0 (Monorepo Initialization)

**Tooling Requirements**:

- CMake ≥3.20
- C++20-capable compiler

**Dependencies**: P0.REPO.001 →

**Validation Method**:

```javascript
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure

```

---

### B. Tooling Baseline Tasks

#### P0.REPO.005: Unify Linting and Formatting Configuration (TASK-005)

**Description**: Configure linters for both C++ and TypeScript to coexist without conflicts.

**Acceptance Criteria**:

- [ ] `.clang-format` and `.clang-tidy` apply only to C++ files
- [ ] `.eslintrc.js` and `.prettierrc` apply only to JS/TS files
- [ ] Ignored directories configured (e.g., ESLint ignores `src/`, clang-format ignores `packages/`)
- [ ] `pnpm run lint` executes both lint passes successfully

**ORP References**: ORP061 §Phase 0 (Tooling Baseline Alignment)

**Tooling Requirements**:

- ESLint ≥8.0, Prettier ≥3.0
- clang-format ≥14.0, clang-tidy ≥14.0

**Dependencies**: P0.REPO.002 →

**Validation Method**:

```javascript
pnpm run lint:js  # Lints TypeScript only
pnpm run lint:cpp  # Lints C++ only
pnpm run lint  # Runs both without conflicts

```

---

#### P0.REPO.006: Initialize Changesets Configuration (TASK-006)

**Description**: Set up changesets for monorepo versioning with all packages registered.

**Acceptance Criteria**:

- [ ] `.changeset/config.json` created with package list
- [ ] Includes `packages/shmui` (as `@orpheus/shmui`)
- [ ] Configured for independent or unified versioning strategy
- [ ] Changesets CLI functional

**ORP References**: ORP061 §Phase 0 (Tooling Baseline Alignment), ORP063 §III.D

**Tooling Requirements**:

- `@changesets/cli` ≥2.26

**Dependencies**: P0.REPO.003 →

**Validation Method**:

```javascript
pnpm changeset  # Should prompt for changeset creation
pnpm changeset version  # Dry-run should succeed

```

---

#### P0.REPO.007: Create Developer Environment Bootstrap Script (TASK-007) **[AMENDED]**

**Description**: Implement one-step setup script for new contributors with comprehensive validation.

**Acceptance Criteria**:

- [ ] Script created: `./scripts/bootstrap-dev.sh`
- [ ] Installs all dependencies (PNPM, CMake tools)
- [ ] **Includes dependency version checks (Node, PNPM, CMake, compiler)**
- [ ] Builds all packages (C++ core, UI, drivers)
- [ ] Runs validation suite (lint, type-check, smoke tests)
- [ ] **Installs Git hooks (Husky) automatically**
- [ ] **Creates `.env.example` with documented configuration options**
- [ ] Outputs setup summary and next steps
- [ ] **Prints troubleshooting guide URL on failure**
- [ ] Works on fresh Ubuntu, macOS, Windows (WSL) installations

**ORP References**: ORP063 §V (Developer Experience), ORP066 §III

**Tooling Requirements**:

- Bash ≥4.0 or equivalent cross-platform shell

**Dependencies**: P0.REPO.001 →, P0.REPO.004 →

**Validation Method**:

```javascript
# On fresh VM/container
./scripts/bootstrap-dev.sh
# Should complete without errors and print "Ready to develop"

```

**Implementation Example**:

```javascript
#!/usr/bin/env bash
set -e

echo "=== Orpheus SDK Bootstrap ==="

# Version checks
command -v node >/dev/null 2>&1 || { echo "Node.js ≥18 required"; exit 1; }
command -v pnpm >/dev/null 2>&1 || { echo "PNPM ≥8 required"; exit 1; }
command -v cmake >/dev/null 2>&1 || { echo "CMake ≥3.20 required"; exit 1; }

echo "→ Installing dependencies..."
pnpm install

echo "→ Building all packages..."
pnpm run build

echo "→ Installing Git hooks..."
pnpm run prepare

echo "→ Running validation..."
./scripts/validate-phase0.sh

echo "✓ Bootstrap complete. Run 'pnpm dev' to start."

```

---

### C. CI Infrastructure Tasks

#### P0.CI.001: Create Interim Parallel CI Workflow (TASK-008)

**Description**: Establish GitHub Actions workflow that builds both Orpheus C++ and Shmui UI independently.

**Acceptance Criteria**:

- [ ] Workflow file `.github/workflows/interim-ci.yml` created
- [ ] Separate jobs for C++ build (matrix: Ubuntu, Windows, macOS)
- [ ] Separate job for Node/UI build and test
- [ ] Both job sets pass on current codebase
- [ ] Matrix caching configured (PNPM, CMake artifacts)

**ORP References**: ORP061 §Phase 0 (CI Parallelization), ORP063 §II.C

**Tooling Requirements**:

- GitHub Actions (hosted runners)

**Dependencies**: P0.REPO.004 →, P0.REPO.005 →

**Validation Method**:

```javascript
# Trigger workflow and verify:
# - All matrix builds pass
# - Cache hit rate >50% on repeat runs
# - Total duration <20 minutes

```

---

#### P0.CI.002: Configure CI Branch Protection (TASK-009)

**Description**: Set branch protection rules requiring interim CI passage for merges.

**Acceptance Criteria**:

- [ ] Main branch requires status checks: `build-cpp`, `build-ui`
- [ ] Required reviewers configured (minimum 1)
- [ ] Force push disabled
- [ ] Require linear history enabled

**ORP References**: ORP063 §III.D (version control governance)

**Tooling Requirements**:

- GitHub repository admin access

**Dependencies**: P0.CI.001 →

**Validation Method**:

- Attempt merge without passing CI (should be blocked)
- Verify protection rules in repository settings

---

### D. Documentation Tasks

#### P0.DOC.001: Create Documentation Index (TASK-010)

**Description**: Establish `docs/INDEX.md` linking all ORP documents and implementation guides.

**Acceptance Criteria**:

- [ ] Index includes ORP061, ORP062, ORP063, ORP064, ORP066, ORP068
- [ ] Cross-references documented (which ORP references which)
- [ ] Migration phases mapped to technical specifications
- [ ] Navigation breadcrumbs for documentation hierarchy

**ORP References**: ORP063 §V.N

**Tooling Requirements**: None (Markdown editing)

**Dependencies**: None (can execute early)

**Validation Method**:

- Manual review of all links (no 404s)
- Verify all ORP documents referenced

---

#### P0.DOC.002: Document Package Naming Conventions (TASK-011)

**Description**: Create `docs/PACKAGE_NAMING.md` with namespace reservation policy.

**Acceptance Criteria**:

- [ ] All `@orpheus/*` package names documented
- [ ] Internal codenames explained (e.g., "Shmui" = `@orpheus/shmui`, preserving identity)
- [ ] Naming guidelines for future packages
- [ ] Scope management policy (who can publish to `@orpheus`)
- [ ] Rationale for preserving Shmui name documented

**ORP References**: ORP061 §Phase 0 (Namespacing), ORP063 §III.D, ORP066 §II

**Tooling Requirements**: None

**Dependencies**: P0.REPO.003 ⇢

**Validation Method**:

- Technical steering review and approval

---

#### P0.DOC.003: Implement Documentation Quick Start Block (TASK-104) **[NEW]**

**Description**: Add clear entry point to documentation index for new users.

**Acceptance Criteria**:

- [ ] Block added to top of `docs/INDEX.md`:

```javascript
## 🚀 Start Here

**New to Orpheus SDK?** Begin with these three guides:

1. **[Getting Started](GETTING_STARTED.md)** – Install, build, and run your first session
2. **[Driver Architecture](DRIVER_ARCHITECTURE.md)** – Understand Service, WASM, and Native drivers
3. **[Contract Guide](CONTRACT_DEVELOPMENT.md)** – Learn the command/event schema system

**For specific tasks:**
- Adding features → [Contributor Guide](../CONTRIBUTING.md)
- Migrating projects → [Migration Guide](MIGRATION_GUIDE.md)
- API reference → [API Surface Index](API_SURFACE_INDEX.md)

```

- [ ] Links verified functional (no 404s)
- [ ] Block positioned above existing ORP document listings
- [ ] Emoji/icons used for visual hierarchy

**ORP References**: ORP063 §V.N, ORP066 §XI

**Tooling Requirements**: None

**Dependencies**: P0.DOC.001 →

**Validation Method**:

- Manual review: first-time user can find entry point within 30 seconds

---

### E. Governance and Configuration Tasks

#### P0.GOV.001: Create Performance Budget Configuration (TASK-103) **[NEW]**

**Description**: Establish version-controlled performance budget file.

**Acceptance Criteria**:

- [ ] File created: `budgets.json` at repository root
- [ ] Structure matches ORP062 §5.3 specifications:

```javascript
{
  "version": "1.0",
  "budgets": {
    "bundleSize": {
      "@orpheus/shmui": {
        "max": 1572864,
        "warn": 1468006,
        "description": "UI bundle (1.5 MB max, warn at 1.4 MB)"
      },
      "@orpheus/engine-wasm": {
        "max": 5242880,
        "warn": 4718592,
        "description": "WASM module (5 MB max, warn at 4.5 MB)"
      }
    },
    "commandLatency": {
      "LoadSession": {
        "p95": 200,
        "p99": 500,
        "unit": "ms"
      },
      "RenderClick": {
        "p95": 1000,
        "p99": 2000,
        "unit": "ms"
      }
    },
    "eventFrequency": {
      "TransportTick": {
        "max": 30,
        "unit": "Hz"
      },
      "RenderProgress": {
        "max": 10,
        "unit": "Hz"
      }
    },
    "degradationTolerance": {
      "latency": "10%",
      "size": "15%"
    }
  }
}

```

- [ ] Changes to budgets require PR with justification
- [ ] Documentation in `docs/PERFORMANCE.md` references this file

**ORP References**: ORP062 §5.3, ORP063 §IV.I, ORP066 §X

**Tooling Requirements**: JSON schema

**Dependencies**: None (can be created early)

**Validation Method**:

```javascript
cat budgets.json | jq '.budgets.bundleSize."@orpheus/shmui".max'
# Should output: 1572864

```

---

### F. Testing Infrastructure Tasks

#### P0.TEST.001: Implement Validation Script Suite (TASK-096) **[NEW]**

**Description**: Create phase validation scripts that aggregate test, lint, and build commands.

**Acceptance Criteria**:

- [ ] Scripts created: `scripts/validate-phase0.sh` through `scripts/validate-phase4.sh`
- [ ] Each script calls appropriate PNPM workspace commands
- [ ] Exit code 0 only if all checks pass
- [ ] Output formatted with clear pass/fail indicators
- [ ] Scripts executable on Linux, macOS, Windows (via Git Bash/WSL)

**ORP References**: ORP065 §II-VI (all phase validations), ORP066 §III

**Tooling Requirements**: Bash ≥4.0

**Dependencies**: After corresponding phase tasks complete

**Validation Method**:

```javascript
./scripts/validate-phase0.sh
# Should execute without 404 errors

```

**Implementation Example** (`scripts/validate-phase0.sh`):

```javascript
#!/usr/bin/env bash
set -e

echo "=== Phase 0 Validation ==="
echo "→ Checking monorepo structure..."
pnpm list --depth 0 || exit 1

echo "→ Building Orpheus C++..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

echo "→ Running C++ tests..."
ctest --test-dir build --output-on-failure

echo "→ Building Shmui..."
pnpm --filter @orpheus/shmui build

echo "→ Running linters..."
pnpm run lint

echo "✓ Phase 0 validation complete"

```

---

### G. Validation Checkpoint

#### P0.TEST.002: Execute Phase 0 Validation Checklist (TASK-012)

**Description**: Run complete validation suite from ORP061 §Phase 0 checklist.

**Acceptance Criteria**:

- [ ] Orpheus C++ build and tests pass
- [ ] Shmui app/Storybook runs without errors
- [ ] CI passes on all platforms
- [ ] No lint/test regressions introduced
- [ ] Bootstrap script succeeds on clean environment
- [ ] Performance budgets file created
- [ ] Documentation quick start block added
- [ ] All P0 tasks marked complete

**ORP References**: ORP061 §Phase 0 Validation Checklist

**Tooling Requirements**: All Phase 0 tooling

**Dependencies**: ALL P0.\* tasks →

**Validation Method**:

```javascript
./scripts/validate-phase0.sh
# Should exit 0 with all checks passed

```

---

## III. PHASE 1: TOOLING NORMALIZATION AND BASIC INTEGRATION

**Objective**: Enable Orpheus core to be consumed by Shmui UI via basic driver integration **Duration Estimate**: 4 weeks **Gate Criteria**: All validation checks from ORP061 §Phase 1 and ORP063 §II.C pass

### A. Contract Schema Tasks

#### P1.CONT.001: Create Contract Schema Package (TASK-013)

**Description**: Establish `@orpheus/contract` package with Zod schemas for contract v0.9.0 (alpha).

**Acceptance Criteria**:

- [ ] Package created at `packages/contract/`
- [ ] Directory structure: `schemas/v0.9.0/{commands,events,errors}.ts`
- [ ] `registry.ts` exports schemas by version
- [ ] Handshake and version negotiation schemas defined
- [ ] Package published internally for consumption

**ORP References**: ORP062 §1.2-1.5, ORP063 §III.E

**Tooling Requirements**:

- Zod ≥3.22

**Dependencies**: P0.REPO.003 →

**Validation Method**:

```javascript
import { HandshakeSchema } from '@orpheus/contract/v0.9.0';
const result = HandshakeSchema.parse({ contractVersion: "0.9.0", ... });
// Should succeed without throwing

```

---

#### P1.CONT.002: Implement Contract Version Roadmap (TASK-014)

**Description**: Create `docs/CONTRACT_ROADMAP.md` mapping contract versions to migration phases.

**Acceptance Criteria**:

- [ ] Phase 0-1: v0.9.0 (alpha) defined
- [ ] Phase 2: v1.0.0-beta defined
- [ ] Phase 3: v1.0.0 (stable) defined
- [ ] Phase 4: v1.1.0+ (post-release) defined
- [ ] Version bump criteria documented

**ORP References**: ORP063 §II.A

**Tooling Requirements**: None

**Dependencies**: P1.CONT.001 →

**Validation Method**:

- Technical review confirms alignment with ORP timeline

---

#### P1.CONT.003: Implement Minimal Command/Event Schemas (TASK-015)

**Description**: Define core schemas needed for Phase 1 proof-of-concept.

**Acceptance Criteria**:

- [ ] `LoadSession` command schema defined
- [ ] `SessionChanged` event schema defined
- [ ] `Error` event schema with taxonomy from ORP062 §1.5
- [ ] `Handshake` schemas with version negotiation
- [ ] TypeScript types auto-generated from Zod schemas

**ORP References**: ORP062 §1.3-1.5

**Tooling Requirements**:

- Zod ≥3.22
- zod-to-ts (for type generation)

**Dependencies**: P1.CONT.001 →

**Validation Method**:

```javascript
// All schemas validate example payloads
LoadSessionSchema.parse({ path: "/tmp/session.json" });
SessionChangedSchema.parse({ trackCount: 5, ... });

```

---

#### P1.CONT.004: Implement Contract Schema Registry Automation (TASK-016) **[AMENDED]**

**Description**: Create tooling for automated contract schema versioning and validation with manifest integration.

**Acceptance Criteria**:

- [ ] Script: `scripts/contract-diff.ts` compares schema versions
- [ ] **Diff tool reads `MANIFEST.json` for version information**
- [ ] **Tool validates checksum before performing diff**
- [ ] Detects breaking changes (MAJOR), additions (MINOR), fixes (PATCH)
- [ ] Generates migration report showing schema differences
- [ ] Validates that contract version bump aligns with schema changes
- [ ] **Manifest automatically updated when new schema version added**
- [ ] **CI fails if manifest checksum doesn't match computed value**
- [ ] Integrated into CI to block incompatible version bumps
- [ ] Output format compatible with codemod generation

**ORP References**: ORP063 §III.E, §V.K, ORP066 §IV

**Tooling Requirements**:

- Zod schema diffing library
- Custom TypeScript tooling

**Dependencies**: P1.CONT.003 →

**Validation Method**:

```javascript
# Modify a schema, run diff tool
pnpm contract:diff v0.9.0 v1.0.0-beta
# Should output structured diff report

```

---

#### P1.CONT.005: Implement Contract Manifest System (TASK-097) **[NEW]**

**Description**: Create manifest-driven contract versioning with checksum validation.

**Acceptance Criteria**:

- [ ] File created: `packages/contract/MANIFEST.json`
- [ ] Manifest structure:

```javascript
{
  "currentVersion": "1.0.0",
  "currentPath": "schemas/v1.0.0",
  "checksum": "sha256:abc123...",
  "availableVersions": [
    {
      "version": "0.9.0",
      "path": "schemas/v0.9.0",
      "checksum": "sha256:def456...",
      "status": "deprecated"
    },
    {
      "version": "1.0.0-beta",
      "path": "schemas/v1.0.0-beta",
      "checksum": "sha256:789ghi...",
      "status": "superseded"
    },
    {
      "version": "1.0.0",
      "path": "schemas/v1.0.0",
      "checksum": "sha256:abc123...",
      "status": "stable"
    }
  ]
}

```

- [ ] Contract diff tool (`scripts/contract-diff.ts`) reads from manifest
- [ ] CI validates manifest checksum matches actual schema files
- [ ] `@orpheus/contract` package exports manifest at runtime

**ORP References**: ORP065 TASK-016 (Contract Schema Automation), ORP063 §III.E, ORP066 §IV

**Tooling Requirements**: JSON schema validation

**Dependencies**: P1.CONT.004 →

**Validation Method**:

```javascript
import { MANIFEST } from '@orpheus/contract';
console.log(MANIFEST.currentVersion); // "1.0.0"
```

---

### B. Service Driver Tasks

#### P1.DRIV.001: Implement Service Driver Foundation (TASK-017) **[AMENDED]**

**Description**: Create `orpheusd` service with HTTP and WebSocket endpoints with secure defaults.

**Acceptance Criteria**:

- [ ] Package created at `packages/engine-service/`
- [ ] HTTP endpoints: `/health`, `/version`, `/contract`
- [ ] WebSocket endpoint: `/ws` for event streaming
- [ ] **Bind address defaults to `127.0.0.1` (configurable via `--host`)**
- [ ] `**--host 0.0.0.0**` **requires explicit flag and logs security warning**
- [ ] Graceful shutdown on `SIGTERM`
- [ ] **Authentication system integration prepared for TASK-099**

**ORP References**: ORP062 §2.1, ORP063 §II.B, ORP066 §VI

**Tooling Requirements**:

- Express.js or Fastify
- ws (WebSocket library)

**Dependencies**: P1.CONT.003 →

**Validation Method**:

```javascript
# Start service
orpheusd &
# Test endpoints
curl http://127.0.0.1:8080/health  # Should return 200
wscat -c ws://127.0.0.1:8080/ws  # Should connect

```

---

#### P1.DRIV.002: Implement Service Driver Command Handler (TASK-018)

**Description**: Wire Orpheus C++ core library to service driver for command execution.

**Acceptance Criteria**:

- [ ] Service links against Orpheus core library
- [ ] `LoadSession` command calls Orpheus session API
- [ ] JSON serialization/deserialization functional
- [ ] Error handling returns structured errors per ORP062 §1.5
- [ ] Command validation via `@orpheus/contract` schemas

**ORP References**: ORP062 §2.1, ORP063 §II.B

**Tooling Requirements**:

- CMake integration for C++ linking
- Orpheus SDK built libraries

**Dependencies**: P1.DRIV.001 →, P0.REPO.004 →

**Validation Method**:

```javascript
# Send LoadSession command via HTTP POST
curl -X POST http://127.0.0.1:8080/command \
  -H "Content-Type: application/json" \
  -d '{"type":"LoadSession","path":"fixtures/test.json"}'
# Should return success response

```

---

#### P1.DRIV.003: Implement Service Driver Event Emission (TASK-019)

**Description**: Stream Orpheus events over WebSocket connection.

**Acceptance Criteria**:

- [ ] Events emitted as JSON over WebSocket
- [ ] Each event includes timestamp and sequence ID
- [ ] `SessionChanged` event fires on successful `LoadSession`
- [ ] Heartbeat emitted every 10 seconds
- [ ] Multiple simultaneous WebSocket clients supported

**ORP References**: ORP062 §2.1 (heartbeats), §1.4 (event schemas)

**Tooling Requirements**:

- WebSocket library with broadcast support

**Dependencies**: P1.DRIV.002 →

**Validation Method**:

```javascript
// Connect WebSocket client and listen for events
const ws = new WebSocket('ws://127.0.0.1:8080/ws');
ws.onmessage = (msg) => {
  const event = JSON.parse(msg.data);
  console.log(event.type, event.timestamp);
};
// Should receive heartbeats every 10s
```

---

#### P1.DRIV.004: Implement Service Driver Authentication (TASK-099) **[NEW]**

**Description**: Add optional token-based authentication to service driver.

**Acceptance Criteria**:

- [ ] Command-line flag: `--auth none|token` (default: `token` in production builds)
- [ ] Token generated on startup if `--auth token` (printed to console)
- [ ] Client passes token via `Authorization: Bearer <token>` header
- [ ] Requests without valid token rejected with 401
- [ ] Token stored in `~/.orpheus/service.token` for persistence
- [ ] `--auth none` only allowed if `NODE_ENV=development`
- [ ] Documentation in `docs/DRIVER_ARCHITECTURE.md` updated

**ORP References**: ORP062 §2.1, §6.1 (Security), ORP066 §VI

**Tooling Requirements**: None

**Dependencies**: P1.DRIV.001 →

**Validation Method**:

```javascript
# Production mode (requires token)
orpheusd --auth token
# Prints: "Service started. Token: abc123..."

# Development mode (no auth)
NODE_ENV=development orpheusd --auth none

```

---

### C. Native Driver Tasks

#### P1.DRIV.005: Create Native Driver Package (TASK-020) **[AMENDED]**

**Description**: Initialize `@orpheus/engine-native` with N-API binding structure (usable by Electron and Node.js).

**Acceptance Criteria**:

- [ ] Package created at `packages/engine-native/` (renamed from `engine-electron`)
- [ ] CMakeLists.txt configured for N-API addon
- [ ] Minimal binding exports `getVersion()` function
- [ ] Builds on Ubuntu (proof of concept)
- [ ] `package.json` configured with install script
- [ ] **Documentation clarifies Node.js and Electron compatibility**

**ORP References**: ORP062 §2.3, ORP063 §II.B, ORP066 §II

**Tooling Requirements**:

- node-addon-api ≥6.0
- CMake ≥3.20
- node-gyp or cmake-js

**Dependencies**: P0.REPO.004 →

**Validation Method**:

```javascript
const orpheus = require('@orpheus/engine-native');
console.log(orpheus.getVersion());
// Should print Orpheus version string
```

---

#### P1.DRIV.006: Implement Native Driver Command Interface (TASK-021)

**Description**: Expose basic command execution via N-API.

**Acceptance Criteria**:

- [ ] `loadSession(path)` function exposed to Node
- [ ] Function calls Orpheus C++ session API
- [ ] Returns promise resolving to success/error
- [ ] Error taxonomy matches ORP062 §1.5
- [ ] Memory management verified (no leaks on load/unload)

**ORP References**: ORP062 §2.3

**Tooling Requirements**:

- Valgrind or AddressSanitizer for leak detection

**Dependencies**: P1.DRIV.005 →

**Validation Method**:

```javascript
const orpheus = require('@orpheus/engine-native');
await orpheus.loadSession('fixtures/test.json');
// Should resolve without throwing
```

---

#### P1.DRIV.007: Implement Native Driver Event Emitter (TASK-022)

**Description**: Use Node EventEmitter pattern for Orpheus events.

**Acceptance Criteria**:

- [ ] Addon inherits from EventEmitter
- [ ] C++ code can emit events to JS
- [ ] `sessionChanged` event emitted on successful load
- [ ] Event payloads match `@orpheus/contract` schemas
- [ ] Listeners can be attached/removed from JS

**ORP References**: ORP062 §2.3, §1.4

**Tooling Requirements**:

- Node.js EventEmitter API

**Dependencies**: P1.DRIV.006 →

**Validation Method**:

```javascript
const orpheus = require('@orpheus/engine-native');
orpheus.on('sessionChanged', (data) => {
  console.log('Session loaded:', data.trackCount);
});
await orpheus.loadSession('fixtures/test.json');
// Should trigger event
```

---

### D. Repository Maintenance Tasks

#### P1.REPO.001: Quarantine Legacy REAPER Adapter Code (TASK-023)

**Description**: Remove or isolate legacy WDL/SWELL/WALTER code from REAPER adapter.

**Acceptance Criteria**:

- [ ] Legacy code moved to `backup/legacy-reaper/` or deleted
- [ ] `adapters/reaper/` contains only maintained code
- [ ] Build system excludes legacy components
- [ ] Documentation updated to reflect adapter status
- [ ] CMake options adjusted (REAPER adapter marked experimental)

**ORP References**: ORP061 §Breaking Changes (adapter deprecation)

**Tooling Requirements**: None

**Dependencies**: P0.REPO.004 →

**Validation Method**:

```javascript
# Verify legacy code not in active build
cmake -S . -B build -DORPHEUS_ENABLE_REAPER=OFF
# Should build without touching legacy files

```

---

### E. Client Broker Tasks

#### P1.DRIV.008: Create Client Broker Package (TASK-024) **[AMENDED]**

**Description**: Implement `@orpheus/client` with driver selection and abstraction.

**Acceptance Criteria**:

- [ ] Package created at `packages/client/`
- [ ] Broker detects available drivers (Service, Native)
- [ ] Selection priority: Native → Service (as per ORP063 §II.B)
- [ ] **Broker uses `driver-registry.ts` for selection logic**
- [ ] **Selection order documented in code comments**
- [ ] Single unified API: `client.send(command)`, `client.on(event)`
- [ ] Async command execution with Promise-based interface
- [ ] **Test suite validates all selection paths (auto, override, fallback)**

**ORP References**: ORP062 §3.1, ORP063 §III.E, ORP066 §V

**Tooling Requirements**: None (pure TypeScript)

**Dependencies**: P1.DRIV.003 →, P1.DRIV.007 →

**Validation Method**:

```javascript
import { OrpheusClient } from '@orpheus/client';
const client = await OrpheusClient.connect();
console.log('Connected via:', client.driver); // 'native' or 'service'
```

---

#### P1.DRIV.009: Implement Driver Selection Registry (TASK-098) **[NEW]**

**Description**: Create deterministic driver selection with environment variable override.

**Acceptance Criteria**:

- [ ] File created: `packages/client/src/driver-registry.ts`
- [ ] Default selection order: `native → wasm → service`
- [ ] Environment variable `ORPHEUS_DRIVER` accepts: `native`, `wasm`, `service`
- [ ] Invalid value logs warning and falls back to auto-selection
- [ ] Selection logic exported and testable
- [ ] Documentation in `docs/DRIVER_ARCHITECTURE.md` updated

**ORP References**: ORP062 §3.1, ORP063 §II.B, ORP066 §V

**Tooling Requirements**: None

**Dependencies**: P1.DRIV.008 →

**Validation Method**:

```javascript
// Default selection
const client = await OrpheusClient.connect();
console.log(client.driver); // auto: native → wasm → service

// Forced selection
process.env.ORPHEUS_DRIVER = 'wasm';
const wasmClient = await OrpheusClient.connect();
console.log(wasmClient.driver); // 'wasm'
```

**Implementation** (`packages/client/src/driver-registry.ts`):

```javascript
export const DRIVER_PRIORITY = ['native', 'wasm', 'service'] as const;
export type DriverType = typeof DRIVER_PRIORITY[number];

export function getDriverOverride(): DriverType | null {
  const override = process.env.ORPHEUS_DRIVER;
  if (!override) return null;

  if (!DRIVER_PRIORITY.includes(override as DriverType)) {
    console.warn(`Invalid ORPHEUS_DRIVER="${override}". Using auto-selection.`);
    return null;
  }

  return override as DriverType;
}

export async function selectDriver(): Promise<DriverType> {
  const override = getDriverOverride();
  if (override) return override;

  for (const driver of DRIVER_PRIORITY) {
    if (await isDriverAvailable(driver)) {
      return driver;
    }
  }

  throw new Error('No Orpheus driver available');
}

```

---

#### P1.DRIV.010: Implement Client Handshake Protocol (TASK-025)

**Description**: Establish version negotiation during client-engine connection.

**Acceptance Criteria**:

- [ ] Client sends handshake with contract version on connect
- [ ] Engine responds with supported version and capabilities
- [ ] Connection rejected if MAJOR version mismatch
- [ ] Handshake timeout configured (5 seconds default)
- [ ] Retry logic for transient failures

**ORP References**: ORP062 §1.2, §1.6

**Tooling Requirements**: None

**Dependencies**: P1.DRIV.008 →

**Validation Method**:

```javascript
// Test version mismatch rejection
const client = await OrpheusClient.connect({ contractVersion: '2.0.0' });
// Should throw or reject if engine only supports 0.9.0
```

---

### F. UI Integration Tasks

#### P1.UI.001: Create Orpheus Integration Test Hook (TASK-026)

**Description**: Add debug panel to Shmui UI for calling Orpheus functions.

**Acceptance Criteria**:

- [ ] Debug panel visible in development mode only
- [ ] "Get Core Version" button calls `client.send({ type: 'GetVersion' })`
- [ ] "Load Test Session" button calls `LoadSession` with fixture
- [ ] Results displayed in UI (version string, success/error)
- [ ] Panel disabled/hidden in production builds

**ORP References**: ORP061 §Phase 1 (Basic UI-Core Bridging)

**Tooling Requirements**:

- React ≥18.0

**Dependencies**: P1.DRIV.008 →

**Validation Method**:

```javascript
NODE_ENV=development pnpm dev
# Open app, click "Get Core Version"
# Should display Orpheus version in UI

```

---

#### P1.UI.002: Implement React OrpheusProvider Context (TASK-027)

**Description**: Create React context for sharing Orpheus client throughout component tree.

**Acceptance Criteria**:

- [ ] `OrpheusProvider` component wraps app root
- [ ] `useOrpheus()` hook returns client instance
- [ ] Connection state exposed: `{ status, driver, error }`
- [ ] Auto-connect on mount, cleanup on unmount
- [ ] Error boundary handles connection failures

**ORP References**: ORP062 §3.2, ORP063 §II.B

**Tooling Requirements**:

- React ≥18.0

**Dependencies**: P1.DRIV.008 →

**Validation Method**:

```javascript
function TestComponent() {
  const { client, status } = useOrpheus();
  return <div>Status: {status}</div>;
}
// Should show "connected" when engine available
```

---

### G. CI Integration Tasks

#### P1.CI.001: Extend CI for Driver Builds (TASK-028)

**Description**: Add driver compilation to CI matrix.

**Acceptance Criteria**:

- [ ] Service driver builds on Ubuntu
- [ ] Native driver builds on Ubuntu, Windows, macOS
- [ ] Driver build artifacts cached
- [ ] Build failures fail CI

**ORP References**: ORP063 §II.C

**Tooling Requirements**:

- GitHub Actions matrix configuration

**Dependencies**: P1.DRIV.002 →, P1.DRIV.006 →

**Validation Method**:

```javascript
# CI workflow includes:
- name: Build Service Driver
  run: pnpm build:service
- name: Build Native Driver
  run: pnpm build:native
# All should succeed

```

---

#### P1.CI.002: Add Integration Smoke Tests to CI (TASK-029)

**Description**: Execute basic integration tests in CI.

**Acceptance Criteria**:

- [ ] Test: Start service driver, connect client, send command
- [ ] Test: Load native driver, call getVersion()
- [ ] Test: Client broker selects correct driver
- [ ] Tests run on Ubuntu (representative platform)
- [ ] Failures block PR merge

**ORP References**: ORP061 §Phase 1 Validation, ORP063 §II.C

**Tooling Requirements**:

- Jest or Vitest for test execution

**Dependencies**: P1.DRIV.008 →, P1.CI.001 →

**Validation Method**:

```javascript
pnpm test:integration
# Should pass all smoke tests

```

---

### H. Documentation Tasks

#### P1.DOC.001: Document Driver Architecture (TASK-030)

**Description**: Create developer guide for driver implementations.

**Acceptance Criteria**:

- [ ] Document created: `docs/DRIVER_ARCHITECTURE.md`
- [ ] Explains Service, Native, WASM driver roles
- [ ] Diagrams showing client-driver communication
- [ ] Code examples for each driver type
- [ ] Cross-references ORP062 §2.0
- [ ] Includes driver selection order and override mechanism

**ORP References**: ORP062 §2.0, ORP063 §V.N, ORP066 §V, §VI

**Tooling Requirements**: None (Markdown + diagrams)

**Dependencies**: P1.DRIV.003 →, P1.DRIV.007 →, P1.DRIV.009 →

**Validation Method**:

- Technical review confirms accuracy

---

#### P1.DOC.002: Create Contract Development Guide (TASK-031)

**Description**: Document how to add new commands/events to contract.

**Acceptance Criteria**:

- [ ] Document created: `docs/CONTRACT_DEVELOPMENT.md`
- [ ] Step-by-step guide for adding commands
- [ ] Schema versioning workflow explained
- [ ] Examples of command handler implementation
- [ ] Testing requirements specified
- [ ] Manifest system usage documented

**ORP References**: ORP062 §1.0, ORP063 §III.E, ORP066 §IV

**Tooling Requirements**: None

**Dependencies**: P1.CONT.003 →, P1.CONT.005 →

**Validation Method**:

- Follow guide to add dummy command; should succeed

---

### I. Validation Checkpoint

#### P1.TEST.001: Execute Phase 1 Validation Checklist (TASK-032)

**Description**: Run complete validation suite from ORP061 §Phase 1 and ORP063 §II.C.

**Acceptance Criteria**:

- [ ] Orpheus binding compiles on all OS
- [ ] Smoke test Orpheus call succeeds
- [ ] No memory leaks detected
- [ ] UI integration manual test passes
- [ ] Storybook builds without errors
- [ ] Backwards compatibility toggle verified
- [ ] Contract handshake succeeds
- [ ] All command/event schemas validated
- [ ] Contract diff tool functional
- [ ] Contract manifest system operational
- [ ] Driver selection registry tested
- [ ] Service driver authentication functional
- [ ] All P1 tasks marked complete

**ORP References**: ORP061 §Phase 1 Validation, ORP063 §II.C

**Tooling Requirements**: All Phase 1 tooling

**Dependencies**: ALL P1.\* tasks →

**Validation Method**:

```javascript
./scripts/validate-phase1.sh
# Should exit 0 with comprehensive report

```

---

## IV. PHASE 2: FEATURE INTEGRATION & BRIDGING

**Objective**: Leverage Orpheus capabilities in UI and implement user-facing features **Duration Estimate**: 6 weeks **Gate Criteria**: All validation checks from ORP061 §Phase 2 pass

### A. WASM Driver Tasks

#### P2.DRIV.001: Initialize WASM Build Infrastructure (TASK-033) **[AMENDED]**

**Description**: Set up Emscripten toolchain for compiling Orpheus to WebAssembly with version pinning.

**Acceptance Criteria**:

- [ ] Package created at `packages/engine-wasm/`
- [ ] CMakeLists.txt configured for Emscripten
- [ ] `**.emscripten-version**` **file created and committed**
- [ ] **Build script enforces version check**
- [ ] Minimal WASM module exports `getVersion()`
- [ ] `.wasm` and `.js` glue files generated
- [ ] **SRI integrity file generated automatically**
- [ ] Module instantiates in Node environment

**ORP References**: ORP062 §2.2, ORP063 §II.B, ORP066 §VII

**Tooling Requirements**:

- Emscripten SDK ≥3.1.45

**Dependencies**: P0.REPO.004 →

**Validation Method**:

```javascript
emcc --version  # Verify Emscripten installed
pnpm build:wasm
node -e "require('./packages/engine-wasm/build/orpheus.js')"
# Should load without errors

```

---

#### P2.DRIV.002: Implement WASM Build Discipline (TASK-100) **[NEW]**

**Description**: Establish reproducible WASM builds with security guarantees.

**Acceptance Criteria**:

- [ ] File created: `.emscripten-version` containing exact version (e.g., `3.1.45`)
- [ ] Build script asserts Emscripten version matches before compilation
- [ ] WASM loaded via dynamic import with SRI verification
- [ ] MIME type check: server must serve as `application/wasm`
- [ ] Worker and WASM files hosted side-by-side (no cross-origin issues)
- [ ] Build script generates `integrity.json` with SRI hashes
- [ ] Documentation in `docs/DRIVER_ARCHITECTURE.md` updated

**ORP References**: ORP062 §2.2 (WASM Security), ORP066 §VII

**Tooling Requirements**: Emscripten SDK

**Dependencies**: P2.DRIV.001 →

**Validation Method**:

```javascript
# Verify Emscripten version
cat .emscripten-version # 3.1.45
emcc --version | grep "3.1.45" # Should match

# Build WASM
pnpm build:wasm
cat packages/engine-wasm/build/integrity.json
# Should contain SRI hashes

```

**Implementation** (`packages/engine-wasm/scripts/build.sh`):

```javascript
#!/usr/bin/env bash
set -e

REQUIRED_VERSION=$(cat .emscripten-version)
CURRENT_VERSION=$(emcc --version | head -n1 | grep -oP '\d+\.\d+\.\d+')

if [ "$CURRENT_VERSION" != "$REQUIRED_VERSION" ]; then
  echo "Error: Emscripten $REQUIRED_VERSION required, found $CURRENT_VERSION"
  exit 1
fi

emcc orpheus.cpp -o build/orpheus.js -s WASM=1 ...

# Generate SRI
WASM_HASH=$(openssl dgst -sha384 -binary build/orpheus.wasm | openssl base64 -A)
echo "{\"orpheus.wasm\": \"sha384-$WASM_HASH\"}" > build/integrity.json

```

**Implementation** (`packages/engine-wasm/src/loader.ts`):

```javascript
import integrityData from '../build/integrity.json';

export async function loadWASM(): Promise<WebAssembly.Module> {
  const response = await fetch('/orpheus.wasm', {
    integrity: integrityData['orpheus.wasm'],
    mode: 'same-origin'
  });

  if (response.headers.get('Content-Type') !== 'application/wasm') {
    throw new Error('Invalid WASM MIME type');
  }

  return WebAssembly.compileStreaming(response);
}

```

---

#### P2.DRIV.003: Implement WASM Web Worker Wrapper (TASK-034)

**Description**: Create Web Worker that hosts WASM module to avoid blocking main thread.

**Acceptance Criteria**:

- [ ] Worker script loads WASM module
- [ ] `postMessage` interface for commands
- [ ] Worker emits events via `postMessage`
- [ ] Async command execution in worker thread
- [ ] Worker lifecycle managed (init, terminate)

**ORP References**: ORP062 §2.2

**Tooling Requirements**:

- Web Worker API

**Dependencies**: P2.DRIV.001 →

**Validation Method**:

```javascript
const worker = new Worker('orpheus.wasm.js');
worker.postMessage({ type: 'GetVersion' });
worker.onmessage = (e) => console.log(e.data);
// Should receive version response
```

---

#### P2.DRIV.004: Implement WASM Command Interface (TASK-035)

**Description**: Wire contract commands to WASM module functions.

**Acceptance Criteria**:

- [ ] `LoadSession` command functional in WASM
- [ ] JSON serialization via Emscripten embind
- [ ] Memory management optimized (reuse buffers)
- [ ] Command validation via `@orpheus/contract`
- [ ] Error handling returns structured errors

**ORP References**: ORP062 §2.2, §1.3

**Tooling Requirements**:

- Emscripten embind

**Dependencies**: P2.DRIV.003 →

**Validation Method**:

```javascript
worker.postMessage({
  type: 'LoadSession',
  data: { path: 'test.json' },
});
// Should receive SessionChanged event
```

---

#### P2.DRIV.005: Integrate WASM into Client Broker (TASK-036)

**Description**: Add WASM driver to broker selection logic.

**Acceptance Criteria**:

- [ ] Broker priority updated: Native → WASM → Service
- [ ] WASM driver detected in browser contexts
- [ ] Worker instantiation handled by broker
- [ ] Fallback to Service if WASM unavailable
- [ ] SRI verification for WASM module load

**ORP References**: ORP062 §3.1, §2.2 (security)

**Tooling Requirements**: None

**Dependencies**: P2.DRIV.004 →, P1.DRIV.008 →

**Validation Method**:

```javascript
// In browser context
const client = await OrpheusClient.connect();
console.log(client.driver); // Should be 'wasm' in browser
```

---

### B. Contract Schema Expansion Tasks

#### P2.CONT.001: Upgrade Contract to v1.0.0-beta (TASK-037)

**Description**: Expand contract with full command/event set for Phase 2 features.

**Acceptance Criteria**:

- [ ] New directory: `packages/contract/schemas/v1.0.0-beta/`
- [ ] All commands from ORP062 §1.3 defined
- [ ] All events from ORP062 §1.4 defined
- [ ] Event frequency constraints documented
- [ ] Migration path from v0.9.0 documented
- [ ] Manifest updated with v1.0.0-beta entry

**ORP References**: ORP062 §1.3-1.4, ORP063 §II.A

**Tooling Requirements**:

- Zod ≥3.22

**Dependencies**: P1.CONT.003 →

**Validation Method**:

```javascript
import * as v1beta from '@orpheus/contract/v1.0.0-beta';
// All schemas should be importable
v1beta.RenderClickSchema.parse({ bars: 2, bpm: 120 });
```

---

#### P2.CONT.002: Implement Event Frequency Validation (TASK-038)

**Description**: Add runtime checks for event frequency constraints with cadence validation.

**Acceptance Criteria**:

- [ ] `TransportTick` limited to ≤30 Hz
- [ ] `RenderProgress` limited to ≤10 Hz
- [ ] Violations logged as warnings
- [ ] Rate limiter implementation tested
- [ ] Configurable thresholds
- [ ] **Runtime cadence validator detects missed ticks and clock drift**
- [ ] **Drift warnings emitted when tick timing exceeds ±5ms tolerance**

**ORP References**: ORP062 §1.4, ORP063 §IV (Runtime Validation)

**Tooling Requirements**: None (pure TypeScript)

**Dependencies**: P2.CONT.001 →

**Validation Method**:

```javascript
// Emit 100 TransportTick events in 1 second
// Should throttle to ~30 events/sec
// Missed ticks should generate warnings
```

---

### C. UI Feature Integration Tasks

#### P2.UI.001: Implement Session Manager Panel (TASK-039)

**Description**: Create UI for loading and viewing Orpheus sessions.

**Acceptance Criteria**:

- [ ] Panel added to Shmui UI (behind feature flag initially)
- [ ] "Load Session" button triggers file picker
- [ ] Session loaded via `LoadSession` command
- [ ] Track list displayed (track names, count)
- [ ] Session metadata shown (BPM, length)
- [ ] Loading/error states handled

**ORP References**: ORP061 §Phase 2 (Local Session Management)

**Tooling Requirements**:

- React ≥18.0
- FileSystem Access API (for browser) or Electron dialog

**Dependencies**: P2.CONT.001 →, P1.UI.002 →

**Validation Method**:

```javascript
# Open app, load fixtures/test.json
# Verify UI shows track list matching JSON

```

---

#### P2.UI.002: Implement Click Track Generator (TASK-040)

**Description**: Create UI controls for rendering click tracks via Orpheus.

**Acceptance Criteria**:

- [ ] Form inputs: BPM (numeric), Bars (numeric)
- [ ] "Generate Click Track" button
- [ ] Calls `RenderClick` command with parameters
- [ ] Audio data returned and loaded into AudioPlayer
- [ ] Progress bar shows render progress (via `RenderProgress` events)
- [ ] Completed audio playable in UI

**ORP References**: ORP061 §Phase 2 (Audio Generation Integration)

**Tooling Requirements**:

- Existing Shmui AudioPlayer component

**Dependencies**: P2.CONT.001 →, P2.UI.001 →

**Validation Method**:

```javascript
# Input: 120 BPM, 2 bars
# Click "Generate"
# Should hear metronome click when played

```

---

#### P2.UI.003: Integrate Orb with Orpheus State (TASK-041)

**Description**: Connect Orb visualization to Orpheus transport/render state.

**Acceptance Criteria**:

- [ ] Orb pulses to beat during playback (via `TransportTick`)
- [ ] Orb enters "thinking" state during render
- [ ] State transitions smooth (no jarring changes)
- [ ] Orb disabled if Orpheus unavailable
- [ ] Animation performance remains >30 FPS

**ORP References**: ORP061 §Phase 2 (Enrich Existing Components)

**Tooling Requirements**:

- Existing Shmui Orb component

**Dependencies**: P2.CONT.002 →, P1.UI.002 →

**Validation Method**:

```javascript
# Play session with transport
# Observe Orb pulsing in sync with beats

```

---

#### P2.UI.004: Implement Track Add/Remove Operations (TASK-042)

**Description**: Enable UI-driven track management with Orpheus backend.

**Acceptance Criteria**:

- [ ] "Add Track" button appends track to session
- [ ] Track list updates after addition
- [ ] "Remove Track" button (per track) deletes track
- [ ] Operations reflected in Orpheus internal state
- [ ] Undo/redo support (optional stretch goal)

**ORP References**: ORP061 §Phase 2 (Local Session Management)

**Tooling Requirements**: None

**Dependencies**: P2.UI.001 →

**Validation Method**:

```javascript
# Add track, verify appears in list
# Remove track, verify disappears
# Query Orpheus state, confirm changes persisted

```

---

#### P2.UI.005: Implement Feature Toggle System (TASK-043)

**Description**: Create runtime toggle for Orpheus-powered features.

**Acceptance Criteria**:

- [ ] Environment variable `ENABLE_ORPHEUS_FEATURES`
- [ ] UI settings panel with "Use Local Engine" checkbox
- [ ] Toggle persists across sessions (localStorage in dev)
- [ ] Features gracefully degrade when disabled
- [ ] Default: enabled in dev, configurable in production

**ORP References**: ORP061 §Phase 2 (Backward Compatibility & Toggles)

**Tooling Requirements**: None

**Dependencies**: P2.UI.001 → (need feature to toggle)

**Validation Method**:

```javascript
ENABLE_ORPHEUS_FEATURES=false pnpm dev
# Orpheus features should be hidden/disabled

```

---

### D. Testing Infrastructure Tasks

#### P2.TEST.001: Implement Golden Audio Test Suite (TASK-044) **[AMENDED]**

**Description**: Create deterministic audio rendering validation tests.

**Acceptance Criteria**:

- [ ] Test cases from ORP062 §4.1 implemented
- [ ] Reference WAV files stored in `packages/contract/fixtures/golden-audio/`
- [ ] Tests compute SHA-256 hash of rendered audio
- [ ] ±1 LSB RMS tolerance allowed
- [ ] Tests run across all three drivers
- [ ] **Cross-driver parity tests compare driver-to-driver output (RMS correlation)**
- [ ] **Test suite reads expected hashes from `MANIFEST.json`**
- [ ] **WAV container format validated against manifest specification**
- [ ] **Test fails with clear error if container format doesn't match (e.g., wrong endianness)**
- [ ] **Regeneration script provided: `scripts/regenerate-golden-audio.sh`**

**ORP References**: ORP062 §4.1, ORP063 §IV.G, ORP066 §IX

**Tooling Requirements**:

- Audio hash computation library
- Reference audio files

**Dependencies**: P2.CONT.001 →, P2.DRIV.005 →

**Validation Method**:

```javascript
pnpm test:golden-audio
# All tests should pass with hash matches
# Cross-driver parity tests should show RMS ≤ 1 LSB difference

```

---

#### P2.TEST.002: Create Golden Audio Manifest (TASK-102) **[NEW]**

**Description**: Canonicalize reference audio format and precompute validation hashes.

**Acceptance Criteria**:

- [ ] File created: `packages/contract/fixtures/golden-audio/MANIFEST.json`
- [ ] Manifest structure:

```javascript
{
  "format": {
    "container": "WAV",
    "encoding": "PCM",
    "bitDepth": 16,
    "sampleRate": 44100,
    "channels": 1,
    "endianness": "little",
    "dither": "none",
    "metadata": "stripped"
  },
  "references": [
    {
      "name": "click_100bpm_2bar",
      "file": "click_100bpm_2bar.wav",
      "sha256": "f23c...",
      "parameters": { "bpm": 100, "bars": 2 }
    },
    {
      "name": "click_120bpm_4bar",
      "file": "click_120bpm_4bar.wav",
      "sha256": "9a4e...",
      "parameters": { "bpm": 120, "bars": 4 }
    }
  ]
}

```

- [ ] Test suite reads hashes from manifest (not hardcoded)
- [ ] CI validates manifest hashes match actual files on every build
- [ ] Documentation in `docs/CONTRACT_DEVELOPMENT.md` explains regeneration process

**ORP References**: ORP062 §4.1, ORP063 §IV.G, ORP066 §IX

**Tooling Requirements**: Audio processing library (sox, ffmpeg)

**Dependencies**: P2.TEST.001 →

**Validation Method**:

```javascript
# Validate manifest
cd packages/contract/fixtures/golden-audio
for ref in $(jq -r '.references[].file' MANIFEST.json); do
  computed=$(sha256sum "$ref" | awk '{print $1}')
  expected=$(jq -r ".references[] | select(.file==\"$ref\") | .sha256" MANIFEST.json)
  [ "$computed" = "$expected" ] || { echo "Mismatch: $ref"; exit 1; }
done

```

---

#### P2.TEST.003: Implement Contract Compliance Test Matrix (TASK-045)

**Description**: Comprehensive test suite for contract adherence.

**Acceptance Criteria**:

- [ ] 100% command coverage (success + failure cases)
- [ ] All event schemas validated
- [ ] All error codes have reproduction tests
- [ ] Tests execute against each driver
- [ ] CI generates coverage report with ≥95% threshold

**ORP References**: ORP062 §4.2, ORP063 §IV.H

**Tooling Requirements**:

- Jest/Vitest with coverage plugin

**Dependencies**: P2.CONT.001 →, P2.DRIV.005 →

**Validation Method**:

```javascript
pnpm test:compliance --coverage
# Coverage report: commands 100%, events 100%, errors 100%

```

---

#### P2.TEST.004: Implement Performance Instrumentation (TASK-046)

**Description**: Add telemetry collection for performance metrics.

**Acceptance Criteria**:

- [ ] Command latency tracked per command type (p95, p99)
- [ ] Event frequency measured per event type
- [ ] Bundle size measured for UI and WASM
- [ ] WASM load time measured
- [ ] Memory usage tracked over time

**ORP References**: ORP063 §IV.I

**Tooling Requirements**:

- Custom telemetry module
- Performance API

**Dependencies**: P2.DRIV.005 →

**Validation Method**:

```javascript
pnpm perf:measure
# Should output metrics JSON with all categories

```

---

#### P2.TEST.005: Implement IPC Stress Testing (Native Driver) (TASK-047)

**Description**: Validate Native driver stability under sustained IPC throughput.

**Acceptance Criteria**:

- [ ] Test sends 10,000 commands over 60 seconds
- [ ] Test verifies all responses received correctly
- [ ] Test monitors for memory leaks during sustained load
- [ ] Test validates event ordering remains correct
- [ ] Test runs in CI on all platforms (Ubuntu, Windows, macOS)
- [ ] No crashes or deadlocks observed

**ORP References**: ORP062 §2.3.1 (IPC load validation)

**Tooling Requirements**:

- Jest/Vitest with extended timeout
- System monitoring tools

**Dependencies**: P1.DRIV.007 →

**Validation Method**:

```javascript
pnpm test:ipc-stress
# Should complete without errors or timeouts
# Memory usage should remain stable

```

---

#### P2.TEST.006: Implement Contract Fuzz Testing (TASK-048)

**Description**: Test contract schema validation with malformed and edge-case inputs.

**Acceptance Criteria**:

- [ ] Fuzz test generates random malformed JSON payloads
- [ ] Tests extremely large session files (>100MB)
- [ ] Tests Unicode edge cases, null bytes, special characters
- [ ] Tests boundary values (negative numbers, MAX_INT)
- [ ] All malformed inputs rejected with appropriate errors
- [ ] No crashes or panics on invalid input

**ORP References**: ORP062 §1.3-1.5 (schema validation robustness)

**Tooling Requirements**:

- fast-check or similar fuzzing library

**Dependencies**: P2.CONT.001 →

**Validation Method**:

```javascript
pnpm test:fuzz --iterations=10000
# All iterations should complete without crashes
# Invalid inputs properly rejected

```

---

#### P2.TEST.007: Implement Thread Safety Audit (Native Driver) (TASK-049)

**Description**: Validate thread safety of N-API bindings.

**Acceptance Criteria**:

- [ ] Test concurrent command execution from multiple Node threads
- [ ] Test validates no race conditions or data corruption
- [ ] Thread Sanitizer (TSan) run shows no violations
- [ ] Test validates proper mutex/lock usage
- [ ] Test runs in CI on Linux (TSan supported)

**ORP References**: ORP062 §2.3 (native driver safety)

**Tooling Requirements**:

- Thread Sanitizer (Clang/GCC)
- Node.js worker_threads

**Dependencies**: P1.DRIV.007 →

**Validation Method**:

```javascript
# Build with TSan
cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread" ...
pnpm test:thread-safety
# Should report zero data races

```

---

### E. CI Enhancement Tasks

#### P2.CI.001: Add Golden Audio Tests to CI (TASK-050)

**Description**: Integrate deterministic audio validation into CI pipeline.

**Acceptance Criteria**:

- [ ] Golden audio tests run on Ubuntu
- [ ] Tests execute for all three drivers
- [ ] Failures block PR merge
- [ ] Artifacts (audio files) uploadable for inspection
- [ ] Cross-driver parity tests included

**ORP References**: ORP063 §IV.G

**Tooling Requirements**:

- CI with audio processing capability

**Dependencies**: P2.TEST.001 →

**Validation Method**:

```javascript
- name: Golden Audio Validation
  run: pnpm test:golden-audio
  # Should pass in CI

```

---

#### P2.CI.002: Add Contract Compliance Tests to CI (TASK-051)

**Description**: Execute full compliance matrix in CI.

**Acceptance Criteria**:

- [ ] Compliance tests run on Ubuntu
- [ ] All drivers tested
- [ ] Coverage report generated
- [ ] Minimum 95% coverage enforced

**ORP References**: ORP063 §IV.H

**Tooling Requirements**:

- Jest/Vitest coverage reporting

**Dependencies**: P2.TEST.003 →

**Validation Method**:

```javascript
- name: Contract Compliance
  run: pnpm test:compliance --coverage
  # Coverage must meet threshold

```

---

### F. Documentation Tasks

#### P2.DOC.001: Document UI Integration Patterns (TASK-052)

**Description**: Create guide for integrating Orpheus into React components.

**Acceptance Criteria**:

- [ ] Document created: `docs/UI_INTEGRATION.md`
- [ ] Examples of using `useOrpheus()` hook
- [ ] Patterns for command execution
- [ ] Event subscription examples
- [ ] Error handling best practices

**ORP References**: ORP061 §Phase 2 (Docs & Storybook Updates)

**Tooling Requirements**: None

**Dependencies**: P2.UI.002 →

**Validation Method**:

- Developer follows guide to integrate new component

---

#### P2.DOC.002: Update Storybook with Orpheus Stories (TASK-053)

**Description**: Add Storybook stories demonstrating Orpheus-powered components.

**Acceptance Criteria**:

- [ ] Story: Session Manager component
- [ ] Story: Click Track Generator
- [ ] Story: Orb with Orpheus state
- [ ] Stories use mock Orpheus client in Storybook
- [ ] Storybook builds and deploys successfully

**ORP References**: ORP061 §Phase 2 (Docs & Storybook Updates)

**Tooling Requirements**:

- Storybook ≥7.0

**Dependencies**: P2.UI.002 →, P2.UI.003 →

**Validation Method**:

```javascript
pnpm storybook
# Navigate to Orpheus stories, verify functionality

```

---

### G. Validation Checkpoint

#### P2.TEST.008: Execute Phase 2 Validation Checklist (TASK-054)

**Description**: Run complete validation suite from ORP061 §Phase 2.

**Acceptance Criteria**:

- [ ] Session import/export functional
- [ ] Click track generation works end-to-end
- [ ] Track operations (add/remove) validated
- [ ] Orb reactivity confirmed
- [ ] No regressions in original features
- [ ] Performance acceptable (click track <1s)
- [ ] Cross-platform testing complete
- [ ] Feature flags tested
- [ ] IPC stress tests passed
- [ ] Fuzz testing completed
- [ ] Thread safety audit passed
- [ ] WASM build discipline enforced
- [ ] Golden audio manifest validated
- [ ] All Phase 2 tasks complete

**ORP References**: ORP061 §Phase 2 Validation

**Tooling Requirements**: All Phase 2 tooling

**Dependencies**: ALL P2.\* tasks →

**Validation Method**:

```javascript
./scripts/validate-phase2.sh
# Comprehensive validation across all criteria

```

---

## V. PHASE 3: UNIFIED BUILD, TESTING, AND RELEASE PREPARATION

**Objective**: Unify tooling, CI/CD, quality checks, and prepare for release **Duration Estimate**: 4 weeks **Gate Criteria**: All validation checks from ORP061 §Phase 3 pass

### A. CI Unification Tasks

#### P3.CI.001: Consolidate CI Pipelines (TASK-055)

**Description**: Merge interim workflows into single comprehensive pipeline.

**Acceptance Criteria**:

- [ ] Single workflow file: `.github/workflows/ci-pipeline.yml`
- [ ] Matrix builds: C++ on all OS, UI on Ubuntu
- [ ] Unified dependency installation (PNPM)
- [ ] Parallel job execution (C++, UI, tests)
- [ ] Caching optimized (PNPM, CMake, native builds)
- [ ] Total duration <25 minutes

**ORP References**: ORP061 §Phase 3 (Merge CI Pipelines), ORP063 §IV.G

**Tooling Requirements**:

- GitHub Actions workflow optimization

**Dependencies**: P2.CI.002 →

**Validation Method**:

```javascript
# Trigger pipeline on PR
# Verify:
# - All jobs pass
# - Parallel execution
# - Cache hit rate >70%

```

---

#### P3.CI.002: Implement Performance Budget Enforcement (TASK-056) **[AMENDED]**

**Description**: Add CI gates for performance regressions using budgets.json.

**Acceptance Criteria**:

- [ ] CI step: `pnpm perf:validate --budgets=budgets.json`
- [ ] **Script reads thresholds from `budgets.json`**
- [ ] Bundle size check validates against configured limits
- [ ] Latency check validates against configured limits
- [ ] Violations fail CI with detailed report showing actual vs. budget
- [ ] **Degradation tolerance applied per `budgets.json` configuration**

**ORP References**: ORP062 §5.3, ORP063 §IV.I, ORP066 §X

**Tooling Requirements**:

- Bundle analyzer
- Performance validation script

**Dependencies**: P2.TEST.004 →, P0.GOV.001 →

**Validation Method**:

```javascript
# Introduce oversized bundle
pnpm build
pnpm perf:validate
# Should fail CI with budget exceeded message

```

---

#### P3.CI.003: Implement Chaos Testing in CI (TASK-057)

**Description**: Add nightly chaos testing workflow for failure scenario validation with worker recovery.

**Acceptance Criteria**:

- [ ] Separate workflow: `.github/workflows/chaos-tests.yml`
- [ ] Scheduled: runs nightly
- [ ] All scenarios from ORP063 §IV.J implemented
- [ ] **Worker crash recovery simulation included**
- [ ] **Persistent recovery validation (worker restarts correctly)**
- [ ] Tests validate recovery within SLA (e.g., <10s reconnect)
- [ ] Results reported to Slack/email on failure

**ORP References**: ORP062 §4.3, ORP063 §IV.J

**Tooling Requirements**:

- Extended timeout configuration

**Dependencies**: P2.TEST.003 →

**Validation Method**:

```javascript
# Manual trigger chaos tests
pnpm test:chaos
# All recovery scenarios should pass
# Worker termination should trigger proper restart

```

---

#### P3.CI.004: Implement Dependency Graph Integrity Check (TASK-058)

**Description**: Add CI step to detect circular dependencies and enforce clean architecture.

**Acceptance Criteria**:

- [ ] Madge configured to scan all TypeScript packages
- [ ] CI fails on circular dependency detection
- [ ] Dependency graph visualization generated
- [ ] Violations reported with affected modules
- [ ] Whitelist for acceptable cycles (if any)

**ORP References**: ORP063 §IV (Dependency Management)

**Tooling Requirements**:

- Madge ≥6.0

**Dependencies**: P3.CI.001 →

**Validation Method**:

```javascript
pnpm dep:check
# Should fail if circular dependencies introduced
# Output dependency graph for review

```

---

#### P3.CI.005: Implement Security and SBOM Audit Step (TASK-059)

**Description**: Add automated security scanning and software bill of materials generation.

**Acceptance Criteria**:

- [ ] `npm audit` runs in CI with severity threshold (high/critical)
- [ ] OSV scanner checks for known vulnerabilities
- [ ] Snyk or Dependabot integration configured
- [ ] SBOM generated in SPDX or CycloneDX format
- [ ] Audit failures block PR merge
- [ ] False positives whitelistable

**ORP References**: ORP062 §6.1, §5.4 (Security & SBOM)

**Tooling Requirements**:

- npm audit, OSV scanner
- SBOM generation tool (syft, cdxgen)

**Dependencies**: P3.CI.001 →

**Validation Method**:

```javascript
pnpm audit
pnpm security:scan
# Should detect known vulnerabilities
# SBOM file generated in artifacts/

```

---

#### P3.CI.006: Implement Pre-Merge Contributor Validation (TASK-060)

**Description**: Add pre-commit and PR checks for code quality.

**Acceptance Criteria**:

- [ ] Husky pre-commit hooks configured
- [ ] Runs lint, format check, type check on staged files
- [ ] PR template includes checklist (tests added, docs updated)
- [ ] CI validates PR title format (conventional commits)
- [ ] Commit message linting (commitlint)

**ORP References**: ORP062 §6.4 (Contributor Standards)

**Tooling Requirements**:

- Husky, lint-staged
- commitlint

**Dependencies**: P3.CI.001 →

**Validation Method**:

```javascript
# Attempt commit with failing lint
git commit -m "bad code"
# Should be blocked by pre-commit hook

```

---

### B. Build Optimization Tasks

#### P3.REPO.001: Optimize Bundle Size (TASK-061)

**Description**: Reduce production bundle sizes through code splitting and lazy loading.

**Acceptance Criteria**:

- [ ] WASM module loaded on-demand (not in initial bundle)
- [ ] Tree-shaking configured for UI components
- [ ] Unused code eliminated
- [ ] Bundle size meets ORP062 §5.3 limits
- [ ] Source maps generated for debugging

**ORP References**: ORP062 §5.3, ORP061 §Phase 3 (Performance Optimization)

**Tooling Requirements**:

- Webpack or Vite bundle analyzer

**Dependencies**: P2.DRIV.005 →

**Validation Method**:

```javascript
pnpm build --analyze
# Bundle report shows UI <1.5MB, WASM lazy-loaded

```

---

#### P3.REPO.002: Implement WASM in Web Worker (TASK-062)

**Description**: Move heavy WASM computation to Web Worker to prevent UI blocking.

**Acceptance Criteria**:

- [ ] WASM instantiated in dedicated Web Worker
- [ ] `postMessage` API for command/event communication
- [ ] Main thread remains responsive during WASM operations
- [ ] Worker lifecycle managed (start/stop/restart)
- [ ] Error propagation from worker to main thread

**ORP References**: ORP061 §Phase 3 (Performance Optimization), ORP062 §2.2

**Tooling Requirements**:

- Web Worker API

**Dependencies**: P2.DRIV.004 →

**Validation Method**:

```javascript
// Trigger heavy WASM operation
// UI animations should maintain 60 FPS
```

---

#### P3.REPO.003: Optimize Native Binary Size (TASK-063)

**Description**: Strip debug symbols and optimize native addon binaries.

**Acceptance Criteria**:

- [ ] Release builds use `-O3` optimization
- [ ] Debug symbols stripped in production binaries
- [ ] Link-time optimization (LTO) enabled
- [ ] Binary size reduced by ≥30% vs debug builds
- [ ] Functionality unchanged (validated by tests)

**ORP References**: ORP062 §5.1

**Tooling Requirements**:

- CMake release configuration
- `strip` utility

**Dependencies**: P1.DRIV.006 →

**Validation Method**:

```javascript
ls -lh packages/engine-native/build/Release/orpheus.node
# Should be significantly smaller than Debug build

```

---

### C. Developer Tooling Tasks

#### P3.TOOL.001: Implement @orpheus/cli Utility (TASK-064)

**Description**: Create command-line interface for schema validation and smoke tests.

**Acceptance Criteria**:

- [ ] Package created: `@orpheus/cli`
- [ ] Commands: `validate`, `test`, `diff`, `version`
- [ ] `validate` checks session JSON against schema
- [ ] `test` runs smoke tests against local engine
- [ ] `diff` compares two contract versions
- [ ] Installable globally via npm
- [ ] Help text and examples provided

**ORP References**: ORP062 §6.4 (CLI tooling suggestion)

**Tooling Requirements**:

- Commander.js or Yargs

**Dependencies**: P1.CONT.004 →

**Validation Method**:

```javascript
npx @orpheus/cli validate fixtures/test.json
# Should report validation success/failure
npx @orpheus/cli diff v0.9.0 v1.0.0-beta
# Should output contract differences

```

---

### D. Release Infrastructure Tasks

#### P3.GOV.001: Configure Changesets for Release (TASK-065)

**Description**: Finalize changesets configuration for versioned releases.

**Acceptance Criteria**:

- [ ] All packages included in changesets config
- [ ] Version strategy defined (unified vs independent)
- [ ] Changelog generation configured
- [ ] Pre-release tags supported (`beta`, `rc`)
- [ ] Package dependencies automatically updated

**ORP References**: ORP061 §Phase 3 (Release Versioning), ORP063 §III.D

**Tooling Requirements**:

- `@changesets/cli` ≥2.26

**Dependencies**: P0.REPO.006 →

**Validation Method**:

```javascript
# Create changeset
pnpm changeset
# Bump versions
pnpm changeset version
# Verify package.json versions updated correctly

```

---

#### P3.GOV.002: Implement NPM Publish Workflow with Approval Gate (TASK-066)

**Description**: Automate package publishing via GitHub Actions with technical steering approval.

**Acceptance Criteria**:

- [ ] Workflow: `.github/workflows/publish.yml`
- [ ] Triggers on release tag or manual dispatch
- [ ] **Requires TSC approval file in PR before triggering**
- [ ] Builds all packages
- [ ] Publishes to NPM with `@orpheus` scope
- [ ] Updates GitHub Release with changelog
- [ ] Only runs from protected branch (main)

**ORP References**: ORP061 §Phase 3 (Release Versioning), ORP063 §IV (Release Governance)

**Tooling Requirements**:

- NPM_TOKEN secret in GitHub
- GitHub Personal Access Token for releases

**Dependencies**: P3.GOV.001 →

**Validation Method**:

```javascript
# Create test release tag
git tag v1.0.0-rc.1
git push origin v1.0.0-rc.1
# Verify workflow publishes packages after approval

```

---

#### P3.GOV.003: Generate Prebuilt Native Binaries (TASK-067) **[AMENDED]**

**Description**: Build and publish prebuilt native addon binaries for major platforms with signing.

**Acceptance Criteria**:

- [ ] Binaries built for: macOS (x64, arm64), Windows (x64), Linux (x64)
- [ ] SHA-256 checksums generated per binary
- [ ] **macOS binaries notarized with hardened runtime enabled**
- [ ] **Windows binaries signed with Authenticode certificate**
- [ ] **Signing validated per TASK-101 requirements**
- [ ] **Signature verification passes on fresh OS installs**
- [ ] Attached to GitHub Releases
- [ ] `@orpheus/engine-native` postinstall downloads prebuilt

**ORP References**: ORP062 §5.1, ORP063 §V.M, ORP066 §VIII

**Tooling Requirements**:

- GitHub Actions matrix builds
- Signing certificates (macOS, Windows)

**Dependencies**: P3.REPO.003 →

**Validation Method**:

```javascript
# Install package on fresh system
npm install @orpheus/engine-native
# Should download prebuilt binary, not compile from source

```

---

#### P3.GOV.004: Validate Binary Signing User Experience (TASK-101) **[NEW]**

**Description**: Ensure signed binaries produce no security warnings on fresh systems.

**Acceptance Criteria**:

- [ ] **macOS**: Binary downloaded and run on fresh macOS (no prior Orpheus install)
- [ ] **macOS**: Gatekeeper shows no warning dialog (notarization verified)
- [ ] **macOS**: Binary has hardened runtime enabled (`codesign --display --verbose`)
- [ ] **Windows**: Binary downloaded and run on fresh Windows (no prior Orpheus install)
- [ ] **Windows**: SmartScreen shows no warning (Authenticode signature verified)
- [ ] **Windows**: Signature validates via `signtool verify /pa`
- [ ] Test performed on clean VMs for both platforms
- [ ] Results documented in `docs/BINARY_VERIFICATION.md`

**ORP References**: ORP062 §5.1, ORP063 §V.M, ORP066 §VIII

**Tooling Requirements**:

- macOS: Xcode command-line tools
- Windows: Windows SDK (signtool)
- Clean test VMs

**Dependencies**: P3.GOV.003 →

**Validation Method**:

```javascript
# macOS
codesign --display --verbose orpheus.node
# Should show: flags=0x10000(runtime)

spctl --assess --type execute orpheus.node
# Should show: accepted

# Windows
signtool verify /pa orpheus.node
# Should show: Successfully verified

```

---

#### P3.GOV.005: Implement Binary Verification (TASK-068)

**Description**: Add checksum verification to binary installation process.

**Acceptance Criteria**:

- [ ] Postinstall script verifies SHA-256 checksum
- [ ] Checksum file downloaded from GitHub Releases
- [ ] Mismatch throws error and aborts installation
- [ ] Optional signature verification (GPG)
- [ ] Documented in `BINARY_VERIFICATION.md`

**ORP References**: ORP063 §V.M

**Tooling Requirements**:

- Node.js crypto module

**Dependencies**: P3.GOV.003 →

**Validation Method**:

```javascript
# Tamper with binary
# Reinstall package
# Should fail with checksum mismatch error

```

---

#### P3.GOV.006: Implement Contract Diff Automation and Codemod Suggestion (TASK-069)

**Description**: Create automation for contract version migrations with codemod generation.

**Acceptance Criteria**:

- [ ] Script: `scripts/contract-migrate.ts`
- [ ] Compares two contract versions and generates migration guide
- [ ] Suggests codemods for breaking changes
- [ ] Outputs migration report in Markdown
- [ ] Integrated with `@orpheus/cli diff` command
- [ ] Codemod templates auto-generated for simple transformations

**ORP References**: ORP063 §III.E, §V.K

**Tooling Requirements**:

- jscodeshift
- Custom schema diff engine

**Dependencies**: P1.CONT.004 →

**Validation Method**:

```javascript
pnpm contract:migrate v0.9.0 v1.0.0-beta --generate-codemods
# Should output migration guide and codemod files

```

---

#### P3.GOV.007: Generate SBOM and Supply-Chain Provenance (TASK-070)

**Description**: Create software bill of materials and provenance attestation for releases.

**Acceptance Criteria**:

- [ ] SBOM generated in SPDX 2.3 or CycloneDX format
- [ ] Includes all direct and transitive dependencies
- [ ] Provenance attestation using SLSA framework
- [ ] Artifacts signed and attached to GitHub Releases
- [ ] SBOM updated automatically on dependency changes

**ORP References**: ORP062 §5.4, ORP063 §IV (Supply Chain Security)

**Tooling Requirements**:

- syft or cdxgen (SBOM generation)
- cosign or sigstore (attestation signing)

**Dependencies**: P3.GOV.003 →

**Validation Method**:

```javascript
# Verify SBOM contains expected packages
pnpm sbom:generate
cat sbom.spdx.json | jq '.packages | length'
# Should show total dependency count

```

---

### E. Testing and Quality Tasks

#### P3.TEST.001: Achieve Test Coverage Targets (TASK-071)

**Description**: Increase test coverage to meet quality thresholds.

**Acceptance Criteria**:

- [ ] Unit test coverage: ≥80% overall
- [ ] Integration test coverage: ≥90% for contract APIs
- [ ] Critical path coverage: 100% (session, render, transport)
- [ ] Coverage report generated in CI
- [ ] Coverage badge in README

**ORP References**: ORP061 §Phase 3 (Comprehensive Testing)

**Tooling Requirements**:

- Jest/Vitest coverage tooling

**Dependencies**: P2.TEST.003 →

**Validation Method**:

```javascript
pnpm test:coverage
# Coverage report meets thresholds

```

---

#### P3.TEST.002: Implement End-to-End Testing (TASK-072)

**Description**: Add automated E2E tests for complete user workflows.

**Acceptance Criteria**:

- [ ] E2E framework configured (Playwright or Cypress)
- [ ] Test: Load session → Add track → Render click → Play audio
- [ ] Test: Connect to each driver type
- [ ] Test: Graceful degradation on driver unavailability
- [ ] Tests run in CI (headless)

**ORP References**: ORP061 §Phase 3 (Comprehensive Testing)

**Tooling Requirements**:

- Playwright ≥1.40 or Cypress ≥13.0

**Dependencies**: P2.UI.004 →

**Validation Method**:

```javascript
pnpm test:e2e
# All E2E scenarios pass

```

---

#### P3.TEST.003: Run Memory Leak Detection (TASK-073)

**Description**: Profile applications for memory leaks and performance issues.

**Acceptance Criteria**:

- [ ] Native addon tested with Valgrind/AddressSanitizer
- [ ] WASM tested with browser DevTools memory profiler
- [ ] 1000-iteration stress test shows stable memory
- [ ] No leaks detected in critical paths
- [ ] Results documented

**ORP References**: ORP061 §Phase 3 (Performance Optimization)

**Tooling Requirements**:

- Valgrind, AddressSanitizer
- Chrome DevTools

**Dependencies**: P3.REPO.002 →

**Validation Method**:

```javascript
# Run stress test
pnpm test:stress
# Memory usage should be stable after warmup

```

---

#### P3.TEST.004: Implement Binary Compatibility Tests (TASK-074)

**Description**: Validate ABI consistency and output determinism across driver builds.

**Acceptance Criteria**:

- [ ] Test loads same session across all driver builds (Service, WASM, Native)
- [ ] Test validates identical ABI signatures across platforms
- [ ] Test compares output byte-for-byte across builds
- [ ] Test validates version negotiation works cross-platform
- [ ] Runs on all target platforms in CI

**ORP References**: ORP062 §5.1 (Binary Compatibility)

**Tooling Requirements**:

- Custom compatibility test suite

**Dependencies**: P3.GOV.003 →

**Validation Method**:

```javascript
pnpm test:binary-compat
# Should confirm identical behavior across all builds

```

---

### F. Documentation Finalization Tasks

#### P3.DOC.001: Complete Migration Guide (TASK-075)

**Description**: Comprehensive guide for users migrating from old packages.

**Acceptance Criteria**:

- [ ] Document created: `docs/MIGRATION_GUIDE.md`
- [ ] Step-by-step migration instructions
- [ ] Breaking changes listed (from ORP061 §Breaking Changes)
- [ ] Code examples (before/after)
- [ ] Links to codemods
- [ ] Notes on package name preservation (@orpheus/shmui)

**ORP References**: ORP061 §Breaking Changes, ORP063 §III.F, ORP066 §II

**Tooling Requirements**: None

**Dependencies**: P2.DOC.002 →

**Validation Method**:

- External user successfully migrates using guide

---

#### P3.DOC.002: Finalize API Documentation (TASK-076)

**Description**: Generate complete API reference documentation.

**Acceptance Criteria**:

- [ ] TypeDoc or similar generates API docs
- [ ] All public interfaces documented
- [ ] Examples for each major API
- [ ] Hosted on GitHub Pages or similar
- [ ] Versioned documentation (per release)

**ORP References**: ORP061 §Phase 3 (Documentation)

**Tooling Requirements**:

- TypeDoc ≥0.25

**Dependencies**: P2.CONT.001 →

**Validation Method**:

```javascript
pnpm docs:generate
# API docs published to docs site

```

---

#### P3.DOC.003: Create Contributor Guide (TASK-077)

**Description**: Comprehensive guide for new contributors.

**Acceptance Criteria**:

- [ ] Document updated: `CONTRIBUTING.md`
- [ ] Setup instructions (C++ and UI dev)
- [ ] Code style guidelines
- [ ] PR submission process
- [ ] Testing requirements
- [ ] Links to Architecture Decision Records

**ORP References**: ORP062 §6.4

**Tooling Requirements**: None

**Dependencies**: P3.CI.001 →

**Validation Method**:

- New contributor follows guide and submits PR

---

#### P3.DOC.004: Update Main README (TASK-078)

**Description**: Revise README to reflect unified monorepo structure.

**Acceptance Criteria**:

- [ ] Overview mentions Shmui UI integration
- [ ] Quick start covers both UI and core
- [ ] Installation instructions updated
- [ ] Links to all key documentation
- [ ] Badges (build status, coverage, version)
- [ ] Reflects correct package names (@orpheus/shmui, @orpheus/engine-native)

**ORP References**: ORP061 §Phase 3 (Documentation), ORP066 §II

**Tooling Requirements**: None

**Dependencies**: P3.TEST.001 →, P3.GOV.001 →

**Validation Method**:

- Technical review confirms accuracy

---

#### P3.DOC.005: Create API Surface Index (TASK-079)

**Description**: Generate comprehensive index linking TypeDoc pages.

**Acceptance Criteria**:

- [ ] Document created: `docs/API_SURFACE_INDEX.md`
- [ ] Lists all public packages and their exports
- [ ] Links to TypeDoc-generated pages
- [ ] Categorized by domain (contract, drivers, UI)
- [ ] Includes quick reference table

**ORP References**: ORP063 §V (Documentation Experience)

**Tooling Requirements**:

- Custom script to parse TypeDoc output

**Dependencies**: P3.DOC.002 →

**Validation Method**:

- All links functional, index complete

---

#### P3.DOC.006: Automate Docs Index Graph Generation (TASK-080)

**Description**: Generate visual documentation dependency graph.

**Acceptance Criteria**:

- [ ] Script generates Mermaid diagram of doc relationships
- [ ] Graph shows ORP → implementation doc relationships
- [ ] Auto-updates on doc changes
- [ ] Embedded in `docs/INDEX.md`
- [ ] Clickable nodes in supported viewers

**ORP References**: ORP063 §V.N (Documentation Cross-Referencing)

**Tooling Requirements**:

- Mermaid.js
- Custom graph generation script

**Dependencies**: P0.DOC.001 →

**Validation Method**:

```javascript
pnpm docs:graph
# Should generate docs/DEPGRAPH.mmd
# Verify all ORP docs included

```

---

### G. Validation Checkpoint

#### P3.TEST.005: Execute Phase 3 Validation Checklist (TASK-081)

**Description**: Run complete validation suite from ORP061 §Phase 3.

**Acceptance Criteria**:

- [ ] All tests passing in unified CI
- [ ] Coverage targets met
- [ ] Manual full regression completed
- [ ] Performance profile acceptable
- [ ] Package audit passed
- [ ] Documentation complete
- [ ] Stakeholder sign-off obtained
- [ ] Binary compatibility validated
- [ ] Security audit passed
- [ ] SBOM generated
- [ ] Contract diff tool operational
- [ ] Binary signing UX validated on fresh systems
- [ ] Performance budgets enforced from budgets.json
- [ ] All Phase 3 tasks complete

**ORP References**: ORP061 §Phase 3 Validation

**Tooling Requirements**: All Phase 3 tooling

**Dependencies**: ALL P3.\* tasks →

**Validation Method**:

```javascript
./scripts/validate-phase3.sh
# Comprehensive validation report

```

---

## VI. PHASE 4: GRADUAL ROLLOUT AND POST-MIGRATION SUPPORT

**Objective**: Deploy unified system, monitor, and provide support **Duration Estimate**: 3 weeks **Gate Criteria**: All validation checks from ORP061 §Phase 4 pass

### A. Deployment Tasks

#### P4.GOV.001: Publish Beta Release (TASK-082)

**Description**: Release v1.0.0-beta packages to NPM.

**Acceptance Criteria**:

- [ ] Version bumped to 1.0.0-beta.1
- [ ] Changelog generated
- [ ] Packages published to NPM with `beta` tag
- [ ] GitHub Release created with notes
- [ ] Announcement posted (blog, Discord, etc.)

**ORP References**: ORP061 §Phase 4 (Beta Release & Testing)

**Tooling Requirements**:

- NPM access
- GitHub Releases

**Dependencies**: P3.TEST.005 →

**Validation Method**:

```javascript
npm info @orpheus/shmui
# Should show beta version available

```

---

#### P4.GOV.002: Archive Old Repositories (TASK-083)

**Description**: Mark separate Shmui and legacy repositories as read-only.

**Acceptance Criteria**:

- [ ] `chrislyons/shmui` repository archived
- [ ] README updated with link to Orpheus SDK monorepo
- [ ] Issues/PRs closed with migration notice
- [ ] Topics/tags updated to indicate archived status

**ORP References**: ORP061 §Phase 4 (Deprecate Old Repos)

**Tooling Requirements**:

- GitHub repository admin access

**Dependencies**: P4.GOV.001 →

**Validation Method**:

- Verify repositories show "Archived" badge

---

#### P4.REPO.001: Decommission Legacy CI Workflows (TASK-084)

**Description**: Remove old CI pipelines from archived repositories.

**Acceptance Criteria**:

- [ ] `.github/workflows/` disabled in old Shmui repo
- [ ] CI badge in old README points to new monorepo
- [ ] Build artifacts no longer generated
- [ ] GitHub Actions usage reduced

**ORP References**: ORP063 §IV (CI Cleanup)

**Tooling Requirements**: None

**Dependencies**: P4.GOV.002 →

**Validation Method**:

- Verify no workflows run in archived repos

---

#### P4.GOV.003: Establish Support Channels (TASK-085)

**Description**: Create support infrastructure for beta testers.

**Acceptance Criteria**:

- [ ] GitHub Discussions enabled
- [ ] Issue templates created (bug, feature, question)
- [ ] Support email or Discord channel announced
- [ ] Known issues documented
- [ ] Feedback collection process defined

**ORP References**: ORP061 §Phase 4 (Beta Release & Testing)

**Tooling Requirements**: None

**Dependencies**: P4.GOV.001 →

**Validation Method**:

- Test submitting issue via template

---

### B. Monitoring Tasks

#### P4.GOV.004: Implement Telemetry Collection with Consent Guard (TASK-086)

**Description**: Optional analytics for tracking adoption and issues with privacy enforcement.

**Acceptance Criteria**:

- [ ] Telemetry library integrated (opt-in only)
- [ ] **Runtime guard prevents telemetry if user hasn't consented**
- [ ] **Consent prompt shown on first use**
- [ ] Privacy policy documented
- [ ] Collects: version, driver type, error rates
- [ ] Dashboard for viewing metrics
- [ ] Respects user privacy (no PII)

**ORP References**: ORP062 §6.1 (Security), ORP063 §IV (Telemetry Governance)

**Tooling Requirements**:

- Analytics service (Plausible, Posthog, etc.)

**Dependencies**: P4.GOV.001 →

**Validation Method**:

```javascript
# Attempt to use without consent
ORPHEUS_TELEMETRY=true pnpm dev
# Should prompt for consent, block telemetry until granted

```

---

#### P4.GOV.005: Create Incident Response Plan (TASK-087)

**Description**: Document procedures for handling production issues.

**Acceptance Criteria**:

- [ ] Document created: `docs/INCIDENT_RESPONSE.md`
- [ ] Escalation path defined
- [ ] Hotfix release process documented
- [ ] Rollback procedures defined
- [ ] Communication templates (user notification)

**ORP References**: ORP061 §Phase 4 (Monitor and Rollback Plan)

**Tooling Requirements**: None

**Dependencies**: P4.GOV.001 →

**Validation Method**:

- Simulate incident and follow procedures

---

### C. Feedback Integration Tasks

#### P4.GOV.006: Conduct Beta Testing Period (TASK-088)

**Description**: Gather feedback from early adopters over 2-4 weeks.

**Acceptance Criteria**:

- [ ] Minimum 10 external beta testers recruited
- [ ] Feedback survey distributed
- [ ] Issues triaged and prioritized
- [ ] Critical bugs fixed in patch releases
- [ ] Feature requests documented for future

**ORP References**: ORP061 §Phase 4 (Beta Release & Testing)

**Tooling Requirements**:

- Survey tool (Google Forms, Typeform)

**Dependencies**: P4.GOV.003 →

**Validation Method**:

- Feedback collected and analyzed

---

#### P4.GOV.007: Publish Stable v1.0.0 Release (TASK-089)

**Description**: Promote beta to stable release after successful testing period.

**Acceptance Criteria**:

- [ ] No critical bugs remaining
- [ ] Feedback incorporated or roadmapped
- [ ] Version bumped to 1.0.0 (remove beta tag)
- [ ] NPM `latest` tag updated
- [ ] Release announcement published

**ORP References**: ORP061 §Phase 4 (Full Transition)

**Tooling Requirements**:

- NPM access

**Dependencies**: P4.GOV.006 →

**Validation Method**:

```javascript
npm info @orpheus/shmui
# Should show 1.0.0 as latest

```

---

### D. Documentation Updates

#### P4.DOC.001: Create Upgrade Path Documentation (TASK-090)

**Description**: Guide for beta users upgrading to stable.

**Acceptance Criteria**:

- [ ] Document: `docs/UPGRADING_TO_1.0.md`
- [ ] Beta to stable migration steps
- [ ] Breaking changes between beta and stable (if any)
- [ ] Deprecation notices
- [ ] Recommended upgrade timeline

**ORP References**: ORP063 §III.F

**Tooling Requirements**: None

**Dependencies**: P4.GOV.007 →

**Validation Method**:

- Beta user successfully upgrades using guide

---

#### P4.DOC.002: Publish Release Blog Post (TASK-091)

**Description**: Announcement article summarizing integration and benefits.

**Acceptance Criteria**:

- [ ] Blog post published on website/Medium
- [ ] Highlights unified architecture
- [ ] Showcases new features (Orpheus + UI)
- [ ] Links to documentation
- [ ] Includes screenshots/demo video

**ORP References**: ORP061 §Phase 4 (Full Transition)

**Tooling Requirements**: None

**Dependencies**: P4.GOV.007 →

**Validation Method**:

- Post shared on social media, community channels

---

### E. Finalization Tasks

#### P4.REPO.002: Prepare Adapter Plugin Interface Skeleton (TASK-092)

**Description**: Create future-proofing placeholder for modular adapter system.

**Acceptance Criteria**:

- [ ] Interface defined in `include/orpheus/adapter_plugin.h`
- [ ] Documentation of adapter contract
- [ ] Example stub adapter implementation
- [ ] Design doc for future plugin system
- [ ] No functional changes to current adapters

**ORP References**: ORP063 §IV (Future-Proofing)

**Tooling Requirements**: None

**Dependencies**: P3.TEST.005 →

**Validation Method**:

- Review design doc with technical steering

---

#### P4.GOV.008: Remove Feature Toggles (TASK-093)

**Description**: Clean up temporary feature flags after stable release.

**Acceptance Criteria**:

- [ ] `ENABLE_ORPHEUS_FEATURES` flag removed
- [ ] All Orpheus features enabled by default
- [ ] Fallback paths removed (or made minimal)
- [ ] Code cleanup (remove toggle conditionals)
- [ ] Released as v1.1.0 (minor bump)

**ORP References**: ORP061 §Phase 4 (Full Transition)

**Tooling Requirements**: None

**Dependencies**: P4.GOV.007 → (wait for stable adoption)

**Validation Method**:

- Code review confirms no toggle remnants

---

#### P4.GOV.009: Conduct Retrospective (TASK-094)

**Description**: Team retrospective on migration process.

**Acceptance Criteria**:

- [ ] Retrospective meeting held
- [ ] Lessons learned documented
- [ ] Process improvements identified
- [ ] Success metrics reviewed
- [ ] Recommendations for future migrations

**ORP References**: ORP063 (continuous improvement)

**Tooling Requirements**: None

**Dependencies**: P4.GOV.008 →

**Validation Method**:

- Document: `docs/RETROSPECTIVE_ORP061.md` created

---

### F. Validation Checkpoint

#### P4.TEST.001: Execute Phase 4 Validation Checklist (TASK-095)

**Description**: Run complete validation suite from ORP061 §Phase 4.

**Acceptance Criteria**:

- [ ] Beta testing feedback collected
- [ ] Old repos archived
- [ ] Release process tested
- [ ] Monitoring in place
- [ ] Telemetry consent guard verified
- [ ] Legacy CI decommissioned
- [ ] All Phase 4 tasks complete

**ORP References**: ORP061 §Phase 4 Validation

**Tooling Requirements**: All Phase 4 tooling

**Dependencies**: ALL P4.\* tasks →

**Validation Method**:

```javascript
./scripts/validate-phase4.sh
# Final validation report

```

---

## VII. FINISH-LINE READINESS CHECKLIST

This section provides a concise, gated checklist summarizing the conditions for successful integration. Each criterion must be met before declaring the migration complete.

### A. Technical Completeness

- [ ] **TC-01**: All 104 tasks from Phases 0-4 marked complete
- [ ] **TC-02**: All three drivers (Service, WASM, Native) functional on target platforms
- [ ] **TC-03**: Contract v1.0.0 implemented and validated across all drivers
- [ ] **TC-04**: Golden audio tests pass with deterministic output
- [ ] **TC-05**: Contract compliance test matrix achieves ≥95% coverage
- [ ] **TC-06**: Unified CI pipeline passes on all platforms (Windows, macOS, Linux)
- [ ] **TC-07**: Contract diff tool operational and verified
- [ ] **TC-08**: Contract and golden audio manifest systems functional

### B. Quality Assurance

- [ ] **QA-01**: Unit test coverage ≥80% overall, ≥90% for contract APIs
- [ ] **QA-02**: E2E tests cover critical user workflows
- [ ] **QA-03**: No memory leaks detected in 1000-iteration stress tests
- [ ] **QA-04**: Performance budgets met: UI ≤1.5MB, WASM ≤5MB, latency within SLA
- [ ] **QA-05**: Chaos tests validate recovery within defined timeframes
- [ ] **QA-06**: Cross-browser compatibility verified (Chrome, Firefox, Safari)
- [ ] **QA-07**: Cross-driver parity test passes (RMS ≤ 1 LSB difference)
- [ ] **QA-08**: WASM build discipline enforced with SRI verification

### C. Release Infrastructure

- [ ] **RI-01**: Changesets configured for all packages
- [ ] **RI-02**: NPM publish workflow functional and tested
- [ ] **RI-03**: Prebuilt native binaries available for macOS (x64/arm64), Windows (x64), Linux (x64)
- [ ] **RI-04**: Binary verification (SHA-256 checksum) implemented and tested
- [ ] **RI-05**: GitHub Releases created with changelogs
- [ ] **RI-06**: Version v1.0.0 published to NPM `latest` tag
- [ ] **RI-07**: Binary signing validated on fresh OS installs (no Gatekeeper/SmartScreen warnings)

### D. Documentation Completeness

- [ ] **DC-01**: All ORP documents (061-064, 066, 068) published and cross-referenced
- [ ] **DC-02**: Migration guide available with code examples and codemods
- [ ] **DC-03**: API documentation generated and hosted
- [ ] **DC-04**: Contributor guide updated for monorepo structure
- [ ] **DC-05**: Main README reflects unified architecture
- [ ] **DC-06**: Release blog post published
- [ ] **DC-07**: Docs index graph auto-generated
- [ ] **DC-08**: Documentation quick start block prominently displayed

### E. Operational Readiness

- [ ] **OR-01**: Support channels established (GitHub Discussions, issue templates)
- [ ] **OR-02**: Monitoring and telemetry configured (opt-in with consent guard)
- [ ] **OR-03**: Incident response plan documented
- [ ] **OR-04**: Beta testing period completed with feedback incorporated
- [ ] **OR-05**: Old repositories archived with migration notices
- [ ] **OR-06**: Feature toggles removed or deprecated
- [ ] **OR-07**: Contract diff bot and nightly validation workflow active
- [ ] **OR-08**: Driver selection registry operational with documented override mechanism

### F. Governance and Compliance

- [ ] **GC-01**: Security audit completed (no critical vulnerabilities)
- [ ] **GC-02**: Licensing verified (MIT for all components, attributions preserved)
- [ ] **GC-03**: Dependency audit passed (no known vulnerabilities)
- [ ] **GC-04**: Binary signing implemented (macOS notarized, Windows signed if applicable)
- [ ] **GC-05**: Privacy policy documented for telemetry with consent enforcement
- [ ] **GC-06**: Stakeholder sign-off obtained from technical steering
- [ ] **GC-07**: SBOM generated and stored for all packages
- [ ] **GC-08**: Performance budgets committed to repository as budgets.json

### G. Success Metrics

- [ ] **SM-01**: Build times acceptable (<25 minutes for full CI)
- [ ] **SM-02**: Developer onboarding time <1 hour (fresh clone to running dev server)
- [ ] **SM-03**: Package download success rate >95% (prebuilt binaries)
- [ ] **SM-04**: User-reported critical bugs in first month <5
- [ ] **SM-05**: Community engagement metrics positive (GitHub stars, issues resolved)
- [ ] **SM-06**: Retrospective completed with lessons learned documented

---

## VIII. DEPENDENCY GRAPH SUMMARY

### Critical Path (Serial Dependencies)

```javascript
P0.REPO.001 → P0.REPO.002 → P0.REPO.003 → P0.REPO.006 →
P1.CONT.001 → P1.CONT.003 → P1.CONT.004 → P1.CONT.005 →
P1.DRIV.001 → P1.DRIV.002 → P1.DRIV.008 → P1.DRIV.009 →
P2.CONT.001 → P2.DRIV.001 → P2.DRIV.002 → P2.DRIV.004 → P2.DRIV.005 →
P2.TEST.001 → P2.TEST.002 → P3.CI.001 → P3.CI.005 → P3.TEST.005 →
P4.GOV.001 → P4.GOV.006 → P4.GOV.007

```

**Critical Path Duration**: ~15-19 weeks (assuming sequential execution)

**Note**: A Mermaid visualization of the complete dependency graph is available at `docs/DEPGRAPH.mmd` and can be generated via:

```javascript
pnpm docs:depgraph

```

### Parallelizable Workstreams

**Phase 0**:

- REPO tasks (001-007) ‖ DOC tasks (001-003) ‖ CI tasks (001-002) ‖ GOV.001 ‖ TEST.001

**Phase 1**:

- CONT tasks (001-005) ‖ DOC tasks (001-002) ‖ REPO.001
- After DRIV.002: DRIV.005-007 (Native) ‖ DRIV.001-004 (Service) ‖ UI.001-002

**Phase 2**:

- DRIV.001-005 (WASM) ‖ UI.001-005 ‖ TEST.001-007 ‖ DOC.001-002

**Phase 3**:

- CI.001-006 ‖ REPO.001-003 ‖ GOV.001-007 ‖ TEST.001-004 ‖ DOC.001-006 ‖ TOOL.001

**Phase 4**:

- GOV.001-005 ‖ DOC.001-002 ‖ REPO.001-002 (after initial release)

---

## IX. TOOLING REQUIREMENTS SUMMARY

### Development Environment

- **Node.js**: ≥18.0 (LTS)
- **PNPM**: ≥8.0
- **CMake**: ≥3.20
- **C++ Compiler**: MSVC 2019+, Clang 13+, or GCC 11+
- **Emscripten**: ≥3.1.45 (for WASM)
- **Git**: ≥2.30

### Libraries and Frameworks

- **React**: ≥18.0
- **Zod**: ≥3.22 (schema validation)
- **Express.js** or **Fastify** (Service driver)
- **ws**: WebSocket library
- **node-addon-api**: ≥6.0 (Native driver)

### Testing Tools

- **Jest** or **Vitest**: Test runner
- **Playwright** or **Cypress**: E2E testing
- **Valgrind**: Memory leak detection (Linux)
- **AddressSanitizer**: Memory sanitizer (macOS/Linux)
- **Thread Sanitizer**: Thread safety validation
- **fast-check**: Fuzz testing library

### CI/CD Tools

- **GitHub Actions**: CI/CD platform
- **Changesets**: Version management
- **TypeDoc**: API documentation generation
- **Storybook**: ≥7.0 (component documentation)
- **Madge**: Dependency cycle detection
- **OSV Scanner**: Vulnerability scanning

### Code Quality Tools

- **clang-format**, **clang-tidy**: C++ formatting/linting
- **ESLint**: ≥8.0
- **Prettier**: ≥3.0
- **Husky**: Pre-commit hooks
- **commitlint**: Commit message linting

### Security and Compliance

- **syft** or **cdxgen**: SBOM generation
- **cosign** or **sigstore**: Attestation signing
- **npm audit**: Dependency vulnerability scanning

### Optional Tools

- **Plausible/Posthog**: Telemetry (opt-in)
- **Bundle analyzer**: Performance profiling
- **jscodeshift**: Codemod transformations
- **Commander.js** or **Yargs**: CLI framework

---

## X. ESTIMATED TIMELINE

| Phase                        | Duration | Cumulative | Notes                                                           |
| ---------------------------- | -------- | ---------- | --------------------------------------------------------------- |
| Phase 0: Repository Setup    | 2 weeks  | 2 weeks    | Includes history preservation, bootstrap script, budgets config |
| Phase 1: Basic Integration   | 4 weeks  | 6 weeks    | Dual driver (Native + Service), contract manifest, auth         |
| Phase 2: Feature Integration | 6 weeks  | 12 weeks   | WASM discipline, UI integration, comprehensive test suite       |
| Phase 3: Unified System      | 4 weeks  | 16 weeks   | CI unification, binary signing UX, release prep                 |
| Phase 4: Rollout & Support   | 3 weeks  | 19 weeks   | Beta testing, stable release, cleanup                           |

**Total Estimated Duration**: 15-19 weeks (~4.5 months)

**Assumptions**:

- 2-3 full-time engineers assigned
- Parallelizable tasks executed concurrently
- Minimal unexpected blockers or scope creep
- Stakeholder approvals obtained within 1 week of request

**Buffer Recommendation**: Add 10-15% contingency (2-3 weeks) for:

- WASM build complexity on Windows
- Binary signing certificate acquisition delays
- Beta feedback requiring significant rework
- Dependency upgrade complications

---

## XI. RISK MITIGATION STRATEGIES

### High-Risk Tasks

1. **P2.DRIV.001-005 (WASM Driver)**: Complex Emscripten build with platform-specific issues
   - **Mitigation**: Allocate experienced WebAssembly engineer, allow 2-week buffer, maintain Service+Native as fallback
   - **Fallback**: Defer WASM to v1.1.0 if critical issues arise; release with Service+Native only
2. **P3.CI.001 (CI Unification)**: Potential merge conflicts and workflow complexity
   - **Mitigation**: Implement incrementally, maintain parallel workflows during transition, extensive testing on feature branch
   - **Rollback**: Revert to interim parallel workflows if unified pipeline unstable
3. **P3.GOV.003 (Prebuilt Binaries)**: Platform-specific build issues, signing certificate delays
   - **Mitigation**: Test on clean VMs, document environment requirements exhaustively, start certificate acquisition early
   - **Contingency**: Release without prebuilt binaries initially, require source compilation, add binaries in patch release
4. **P4.GOV.006 (Beta Testing)**: Low participation or critical bugs discovered late
   - **Mitigation**: Recruit beta testers early (minimum 10), incentivize participation, allocate 2-week buffer for fixes
   - **Response**: Delay stable release if critical bugs found; fast-track hotfix process
5. **P2.TEST.005-007 (Advanced Testing)**: Time-consuming test development may delay Phase 2
   - **Mitigation**: Prioritize critical path tests first, run non-critical tests in parallel with Phase 3 work
   - **Adjustment**: Move fuzz testing to post-v1.0.0 if schedule pressured
6. **P1.CONT.004-005 (Contract Automation)**: Custom tooling may be complex to build correctly
   - **Mitigation**: Start early in Phase 1, use existing schema diff libraries where possible, manual fallback available
   - **Scope Reduction**: If automation incomplete, use manual contract validation for v1.0.0, automate in v1.1.0

### Contingency Plans

#### Scenario: WASM Driver Delayed

- **Action**: Proceed with Service + Native drivers only
- **Impact**: Browser-only users must use Service driver (localhost requirement)
- **Timeline**: Defer WASM to v1.1.0 (post-stable release)
- **Communication**: Document clearly in v1.0.0 release notes

#### Scenario: CI Budget Exceeded

- **Action**: Implement selective job execution based on changed files
- **Implementation**: Use GitHub Actions path filters to skip redundant OS builds for documentation-only changes
- **Optimization**: Move non-critical tests (chaos, fuzz) to nightly schedule

#### Scenario: Critical Bug in Beta

- **Action**: Fast-track hotfix through abbreviated review process
- **Timeline**: Delay stable release by 1-2 weeks for thorough validation
- **Communication**: Transparent incident report to beta testers, updated timeline announcement

#### Scenario: Binary Signing Failures

- **Action**: Release without signatures initially for v1.0.0
- **Mitigation**: Clearly document checksum verification as security measure
- **Remediation**: Add signatures in v1.0.1 patch release once certificates obtained
- **User Impact**: macOS users may see Gatekeeper warnings; provide clear instructions

#### Scenario: Performance Budgets Exceeded

- **Action**: Investigate root cause (bundle bloat vs. algorithm inefficiency)
- **Short-term**: Relax budgets by 20% with documented justification
- **Long-term**: Create performance improvement epic for v1.1.0 milestone
- **Gate**: Must not exceed +50% of original budgets

#### Scenario: Test Coverage Below Targets

- **Action**: Prioritize coverage for critical paths only (session, render, transport)
- **Requirement**: Critical path must achieve 100% coverage before stable release
- **Compromise**: Accept 75% overall coverage for v1.0.0 if critical paths covered
- **Remediation**: Plan coverage improvement sprints post-release

---

## XII. SUCCESS CRITERIA VALIDATION

Upon completion of all phases, the following must be demonstrable:

### A. Functional Demonstrations

1. **End-to-End Workflow**: Load session → Add track → Render click → Play audio in UI
   - **Platform**: Windows, macOS, Linux
   - **Browsers**: Chrome, Firefox, Safari (for web builds)
   - **Success Criteria**: Complete workflow in <60 seconds with no errors
2. **Multi-Driver Validation**: Repeat workflow using each driver type
   - **Service Driver**: Workflow succeeds with localhost service
   - **WASM Driver**: Workflow succeeds in browser-only context
   - **Native Driver**: Workflow succeeds in Electron/Node context
   - **Success Criteria**: Identical audio output across all drivers (verified by hash)
3. **Cross-Platform Validation**: Workflow succeeds on all target platforms
   - **Windows**: Both Native and Service drivers functional
   - **macOS**: Both architectures (x64, arm64) work correctly
   - **Linux**: Build and test on Ubuntu 20.04/22.04 LTS
   - **Success Criteria**: Zero platform-specific bugs in core workflows
4. **Performance Validation**: Operations complete within budget
   - **Click Track Render**: <1 second for 2-bar, 120 BPM track
   - **Session Load**: <500ms for typical 10-track session
   - **UI Responsiveness**: Maintains 60 FPS during WASM operations
   - **Success Criteria**: All performance budgets met in production builds

### B. Quality Gates

5. **Documentation Validation**: New contributor onboards successfully
   - **Test**: External contributor with no prior Orpheus experience
   - **Process**: Follow `CONTRIBUTING.md` from clone to PR submission
   - **Timeline**: Complete onboarding within 2 hours
   - **Success Criteria**: PR submitted with passing CI, proper formatting
6. **Release Validation**: Fresh installation succeeds without issues
   - **Test**: `npm install @orpheus/shmui` on clean system
   - **Platforms**: Ubuntu, macOS, Windows
   - **Success Criteria**: Installs without requiring source compilation (prebuilt binary used)
7. **Community Validation**: External adoption proves viability
   - **Target**: Minimum 3 external projects adopt v1.0.0 within first month
   - **Tracking**: GitHub Dependents, community showcase
   - **Success Criteria**: Positive feedback, no major integration blockers reported

### C. Operational Validation

8. **Monitoring Validation**: Telemetry provides actionable insights
   - **Test**: Enable telemetry in test environment
   - **Verification**: Metrics appear in dashboard within 5 minutes
   - **Privacy**: Consent prompt shown, no data collected without approval
   - **Success Criteria**: Dashboard shows driver usage, error rates, latency percentiles
9. **Support Validation**: Issue triage process functions smoothly
   - **Test**: Submit test issues via templates
   - **Process**: Verify auto-labeling, assignment rules work
   - **Timeline**: All issues triaged within 48 hours
   - **Success Criteria**: Support workflow documented and followed
10. **Rollback Validation**: Emergency procedures tested
    - **Test**: Simulate critical bug discovery scenario
    - **Process**: Follow incident response plan
    - **Timeline**: Hotfix release within 24 hours of decision
    - **Success Criteria**: Process documented and executable

---

## XIII. POST-COMPLETION TRANSITION

### A. Handoff to Maintenance

Once all 104 tasks complete and finish-line criteria met:

1. **Repository Ownership Transfer**:
   - [ ] Assign repository maintainers (minimum 2)
   - [ ] Grant NPM publish access to maintainers
   - [ ] Document escalation paths for emergencies
   - [ ] Transfer GitHub org admin rights if applicable
2. **Knowledge Transfer Sessions**:
   - [ ] Architecture overview session (2 hours)
   - [ ] Driver implementation deep-dive (2 hours)
   - [ ] Release process walkthrough (1 hour)
   - [ ] Incident response simulation (1 hour)
3. **Runbook Creation**:
   - [ ] Document: `docs/RUNBOOK.md` with common maintenance tasks
   - [ ] Include: dependency updates, security patching, version bumps
   - [ ] Define: SLAs for critical bugs (24h), minor bugs (1 week), features (backlog)

### B. Continuous Improvement

4. **Quarterly Review Cadence**:
   - Review performance metrics and adjust budgets
   - Evaluate contract evolution needs
   - Plan deprecations and breaking changes
   - Update roadmap based on community feedback
5. **Technical Debt Management**:
   - Create epic for items deferred during migration
   - Prioritize based on user impact and maintenance burden
   - Allocate 20% of sprint capacity to debt reduction
   - Track via GitHub Projects board

### C. Community Engagement

6. **Release Cadence**:
   - **Major releases**: Annually (breaking changes allowed)
   - **Minor releases**: Quarterly (new features, contract extensions)
   - **Patch releases**: As needed (bug fixes, security updates)
   - **Beta/RC releases**: 2 weeks before stable for testing
7. **Contribution Encouragement**:
   - Tag "good first issue" for newcomers
   - Maintain CONTRIBUTORS.md recognizing contributions
   - Host monthly community calls (optional)
   - Publish roadmap transparently

---

## XIV. APPENDIX: TASK NUMBERING REFERENCE

For integration with project management tools (Jira, Linear, etc.), sequential numbering:

| Task ID  | Description                             | Phase | Domain | Dependencies    |
| -------- | --------------------------------------- | ----- | ------ | --------------- |
| TASK-001 | Initialize Monorepo Workspace           | P0    | REPO   | None            |
| TASK-002 | Import Shmui Codebase                   | P0    | REPO   | 001             |
| TASK-003 | Configure Package Namespacing           | P0    | REPO   | 002             |
| TASK-004 | Preserve Orpheus C++ Build              | P0    | REPO   | 001             |
| TASK-005 | Unify Linting Configuration             | P0    | REPO   | 002             |
| TASK-006 | Initialize Changesets                   | P0    | REPO   | 003             |
| TASK-007 | Create Bootstrap Script                 | P0    | REPO   | 001, 004        |
| TASK-008 | Create Interim CI                       | P0    | CI     | 004, 005        |
| TASK-009 | Configure Branch Protection             | P0    | CI     | 008             |
| TASK-010 | Create Documentation Index              | P0    | DOC    | None            |
| TASK-011 | Document Package Naming                 | P0    | DOC    | 003             |
| TASK-096 | Implement Validation Script Suite       | P0    | TEST   | Phase-dependent |
| TASK-103 | Create Performance Budget Config        | P0    | GOV    | None            |
| TASK-104 | Implement Docs Quick Start Block        | P0    | DOC    | 010             |
| TASK-012 | Phase 0 Validation                      | P0    | TEST   | ALL P0          |
| TASK-013 | Create Contract Package                 | P1    | CONT   | 003             |
| TASK-014 | Contract Version Roadmap                | P1    | CONT   | 013             |
| TASK-015 | Implement Minimal Schemas               | P1    | CONT   | 013             |
| TASK-016 | Contract Schema Automation              | P1    | CONT   | 015             |
| TASK-097 | Implement Contract Manifest System      | P1    | CONT   | 016             |
| TASK-017 | Service Driver Foundation               | P1    | DRIV   | 015             |
| TASK-018 | Service Command Handler                 | P1    | DRIV   | 017, 004        |
| TASK-019 | Service Event Emission                  | P1    | DRIV   | 018             |
| TASK-099 | Implement Service Driver Authentication | P1    | GOV    | 017             |
| TASK-020 | Native Driver Package                   | P1    | DRIV   | 004             |
| TASK-021 | Native Command Interface                | P1    | DRIV   | 020             |
| TASK-022 | Native Event Emitter                    | P1    | DRIV   | 021             |
| TASK-023 | Quarantine Legacy REAPER                | P1    | REPO   | 004             |
| TASK-024 | Create Client Broker                    | P1    | DRIV   | 019, 022        |
| TASK-098 | Implement Driver Selection Registry     | P1    | DRIV   | 024             |
| TASK-025 | Client Handshake Protocol               | P1    | DRIV   | 024             |
| TASK-026 | UI Integration Test Hook                | P1    | UI     | 024             |
| TASK-027 | React OrpheusProvider                   | P1    | UI     | 024             |
| TASK-028 | Extend CI for Drivers                   | P1    | CI     | 018, 021        |
| TASK-029 | Add Integration Smoke Tests             | P1    | CI     | 024, 028        |
| TASK-030 | Document Driver Architecture            | P1    | DOC    | 019, 022        |
| TASK-031 | Contract Development Guide              | P1    | DOC    | 015             |
| TASK-032 | Phase 1 Validation                      | P1    | TEST   | ALL P1          |
| TASK-033 | Initialize WASM Build                   | P2    | DRIV   | 004             |
| TASK-100 | Implement WASM Build Discipline         | P2    | DRIV   | 033             |
| TASK-034 | WASM Web Worker Wrapper                 | P2    | DRIV   | 033             |
| TASK-035 | WASM Command Interface                  | P2    | DRIV   | 034             |
| TASK-036 | Integrate WASM into Broker              | P2    | DRIV   | 035, 024        |
| TASK-037 | Upgrade Contract to v1.0.0-beta         | P2    | CONT   | 015             |
| TASK-038 | Event Frequency Validation              | P2    | CONT   | 037             |
| TASK-039 | Session Manager Panel                   | P2    | UI     | 037, 027        |
| TASK-040 | Click Track Generator                   | P2    | UI     | 037, 039        |
| TASK-041 | Integrate Orb with Orpheus              | P2    | UI     | 038, 027        |
| TASK-042 | Track Add/Remove Operations             | P2    | UI     | 039             |
| TASK-043 | Feature Toggle System                   | P2    | UI     | 039             |
| TASK-044 | Golden Audio Test Suite                 | P2    | TEST   | 037, 036        |
| TASK-102 | Create Golden Audio Manifest            | P2    | TEST   | 044             |
| TASK-045 | Contract Compliance Matrix              | P2    | TEST   | 037, 036        |
| TASK-046 | Performance Instrumentation             | P2    | TEST   | 036             |
| TASK-047 | IPC Stress Testing                      | P2    | TEST   | 022             |
| TASK-048 | Contract Fuzz Testing                   | P2    | TEST   | 037             |
| TASK-049 | Thread Safety Audit                     | P2    | TEST   | 022             |
| TASK-050 | Golden Audio Tests in CI                | P2    | CI     | 044             |
| TASK-051 | Compliance Tests in CI                  | P2    | CI     | 045             |
| TASK-052 | Document UI Integration                 | P2    | DOC    | 040             |
| TASK-053 | Update Storybook Stories                | P2    | DOC    | 040, 041        |
| TASK-054 | Phase 2 Validation                      | P2    | TEST   | ALL P2          |
| TASK-055 | Consolidate CI Pipelines                | P3    | CI     | 051             |
| TASK-056 | Performance Budget Enforcement          | P3    | CI     | 046             |
| TASK-057 | Chaos Testing in CI                     | P3    | CI     | 045             |
| TASK-058 | Dependency Graph Check                  | P3    | CI     | 055             |
| TASK-059 | Security and SBOM Audit                 | P3    | CI     | 055             |
| TASK-060 | Pre-Merge Contributor Validation        | P3    | CI     | 055             |
| TASK-061 | Optimize Bundle Size                    | P3    | REPO   | 036             |
| TASK-062 | WASM in Web Worker                      | P3    | REPO   | 035             |
| TASK-063 | Optimize Native Binary Size             | P3    | REPO   | 021             |
| TASK-064 | Implement @orpheus/cli                  | P3    | TOOL   | 016             |
| TASK-065 | Configure Changesets                    | P3    | GOV    | 006             |
| TASK-066 | NPM Publish Workflow                    | P3    | GOV    | 065             |
| TASK-067 | Generate Prebuilt Binaries              | P3    | GOV    | 063             |
| TASK-101 | Validate Binary Signing UX              | P3    | GOV    | 067             |
| TASK-068 | Binary Verification                     | P3    | GOV    | 067             |
| TASK-069 | Contract Diff Automation                | P3    | GOV    | 016             |
| TASK-070 | Generate SBOM and Provenance            | P3    | GOV    | 067             |
| TASK-071 | Achieve Coverage Targets                | P3    | TEST   | 045             |
| TASK-072 | End-to-End Testing                      | P3    | TEST   | 042             |
| TASK-073 | Memory Leak Detection                   | P3    | TEST   | 062             |
| TASK-074 | Binary Compatibility Tests              | P3    | TEST   | 067             |
| TASK-075 | Complete Migration Guide                | P3    | DOC    | 053             |
| TASK-076 | Finalize API Documentation              | P3    | DOC    | 037             |
| TASK-077 | Create Contributor Guide                | P3    | DOC    | 055             |
| TASK-078 | Update Main README                      | P3    | DOC    | 071, 065        |
| TASK-079 | Create API Surface Index                | P3    | DOC    | 076             |
| TASK-080 | Automate Docs Index Graph               | P3    | DOC    | 010             |
| TASK-081 | Phase 3 Validation                      | P3    | TEST   | ALL P3          |
| TASK-082 | Publish Beta Release                    | P4    | GOV    | 081             |
| TASK-083 | Archive Old Repositories                | P4    | GOV    | 082             |
| TASK-084 | Decommission Legacy CI                  | P4    | REPO   | 083             |
| TASK-085 | Establish Support Channels              | P4    | GOV    | 082             |
| TASK-086 | Telemetry with Consent Guard            | P4    | GOV    | 082             |
| TASK-087 | Incident Response Plan                  | P4    | GOV    | 082             |
| TASK-088 | Conduct Beta Testing                    | P4    | GOV    | 085             |
| TASK-089 | Publish Stable v1.0.0                   | P4    | GOV    | 088             |
| TASK-090 | Upgrade Path Documentation              | P4    | DOC    | 089             |
| TASK-091 | Release Blog Post                       | P4    | DOC    | 089             |
| TASK-092 | Adapter Plugin Interface                | P4    | REPO   | 081             |
| TASK-093 | Remove Feature Toggles                  | P4    | GOV    | 089             |
| TASK-094 | Conduct Retrospective                   | P4    | GOV    | 093             |
| TASK-095 | Phase 4 Validation                      | P4    | TEST   | ALL P4          |

---

## Task Summary by Phase

| Phase     | Task Range                          | Count   | Percentage |
| --------- | ----------------------------------- | ------- | ---------- |
| Phase 0   | TASK-001 to TASK-012, 096, 103, 104 | 15      | 14.4%      |
| Phase 1   | TASK-013 to TASK-032, 097, 098, 099 | 23      | 22.1%      |
| Phase 2   | TASK-033 to TASK-054, 100, 102      | 24      | 23.1%      |
| Phase 3   | TASK-055 to TASK-081, 101           | 28      | 26.9%      |
| Phase 4   | TASK-082 to TASK-095                | 14      | 13.5%      |
| **Total** |                                     | **104** | **100%**   |

---

## Task Summary by Domain

| Domain    | Count   | Primary Phases     | Key Tasks                                    |
| --------- | ------- | ------------------ | -------------------------------------------- |
| REPO      | 12      | P0, P1, P3, P4     | Monorepo setup, legacy cleanup, optimization |
| DRIV      | 19      | P1, P2             | Service, Native, WASM driver implementations |
| CONT      | 9       | P1, P2             | Contract schemas, versioning, manifest       |
| CI        | 13      | P0, P1, P2, P3     | Pipeline setup, testing, validation          |
| DOC       | 15      | P0, P1, P2, P3, P4 | Guides, API docs, quick start                |
| GOV       | 19      | P0, P1, P3, P4     | Security, signing, releases, compliance      |
| UI        | 7       | P1, P2             | React integration, session manager, features |
| TEST      | 11      | P0, P1, P2, P3, P4 | Validation, golden audio, stress testing     |
| TOOL      | 1       | P3                 | CLI utility                                  |
| **Total** | **104** |                    |                                              |

---

## XV. SUPPLEMENTARY ARTIFACT SPECIFICATIONS

### New Repository Artifacts

The following files must be created as part of this consolidated implementation plan:

1. `**.emscripten-version**`
   - Location: Repository root
   - Content: `3.1.45` (or current stable version)
   - Purpose: Pin WASM build toolchain
2. `**budgets.json**`
   - Location: Repository root
   - Content: Performance budget configuration per TASK-103
   - Purpose: Version-controlled performance thresholds
3. `**packages/contract/MANIFEST.json**`
   - Location: Contract package root
   - Content: Contract version registry per TASK-097
   - Purpose: Single source of truth for contract versioning
4. `**packages/contract/fixtures/golden-audio/MANIFEST.json**`
   - Location: Golden audio fixture directory
   - Content: Audio format specification and precomputed hashes per TASK-102
   - Purpose: Canonicalize reference audio validation
5. `**packages/client/src/driver-registry.ts**`
   - Location: Client package source
   - Content: Driver selection logic per TASK-098
   - Purpose: Deterministic driver priority
6. `**scripts/validate-phase0.sh**` through `**scripts/validate-phase4.sh**`
   - Location: Repository scripts directory
   - Content: Phase validation orchestration per TASK-096
   - Purpose: Automate validation checkpoints
7. `**scripts/bootstrap-dev.sh**`
   - Location: Repository scripts directory
   - Content: Developer environment setup per TASK-007 (amended)
   - Purpose: One-command onboarding
8. `**scripts/regenerate-golden-audio.sh**`
   - Location: Repository scripts directory
   - Content: Regenerate reference audio with correct container format
   - Purpose: Maintain golden audio corpus

---

## XVI. AMENDMENT HISTORY

| Version | Date       | Changes                                                                                                                                                                                                                                                                     | Author             |
| ------- | ---------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------ |
| 1.0     | 2025-01-XX | Initial normative specification (ORP065)                                                                                                                                                                                                                                    | Technical Steering |
| 1.1     | 2025-01-XX | Amendments per technical review (ORP066)                                                                                                                                                                                                                                    | Technical Steering |
| 2.0     | 2025-01-XX | **Consolidated specification (ORP068)**:<br>• All ORP066 amendments fully integrated<br>• Package naming corrections applied<br>• Manifest systems incorporated<br>• Security hardening integrated<br>• Task numbering preserved<br>• 104 total tasks (95 original + 9 new) | Technical Steering |

---

## XVII. APPROVALS

**Technical Steering Committee Sign-Off**:

- [ ] **Lead Architect**: ****\*\*****\_****\*\***** Date: **\_\_\_**
- [ ] **Engineering Manager**: **\*\***\_\_\_\_**\*\*** Date: **\_\_\_**
- [ ] **Product Owner**: ****\*\*****\_\_****\*\***** Date: **\_\_\_**
- [ ] **QA Lead**: ****\*\*\*\*****\_\_\_****\*\*\*\***** Date: **\_\_\_**

**Approval Conditions**:

- All amendments from ORP066 reviewed and fully integrated
- Resource allocation confirmed (2-3 FTE for 19 weeks)
- Budget approved for infrastructure costs (CI, binary signing, hosting)
- Contingency plans reviewed and accepted by stakeholders

---

**Document Status**: Normative Implementation Specification (Consolidated) **Version**: 2.0 **Supersedes**: ORP065 v1.1 **Incorporates**: All amendments from ORP066 **Effective Date**: Upon technical steering approval **Review Cycle**: Weekly during implementation phases, monthly post-release **Owner**: Orpheus SDK Technical Steering Committee **Next Review**: End of Phase 1 (Week 6)

---

## XVIII. QUICK REFERENCE

### Critical Milestones

| Milestone               | Target Week | Exit Criteria                                    |
| ----------------------- | ----------- | ------------------------------------------------ |
| M1: Repo Setup Complete | Week 2      | P0.TEST.002 passes                               |
| M2: Basic Integration   | Week 6      | P1.TEST.001 passes, two drivers functional       |
| M3: WASM Integrated     | Week 9      | All three drivers functional                     |
| M4: Feature Complete    | Week 12     | P2.TEST.008 passes, UI features demonstrated     |
| M5: Release Ready       | Week 16     | P3.TEST.005 passes, all quality gates met        |
| M6: Beta Released       | Week 17     | P4.GOV.001 complete                              |
| M7: Stable v1.0.0       | Week 19     | P4.TEST.001 passes, all finish-line criteria met |

### High-Priority Tasks (Do First)

1. **P0.REPO.001-007, P0.GOV.001, P0.TEST.001**: Foundation setup (Week 1)
2. **P1.CONT.001-005**: Contract schema infrastructure (Weeks 3-4)
3. **P1.DRIV.001-010**: Driver implementations (Weeks 3-6)
4. **P2.DRIV.001-005**: WASM driver (Weeks 7-9)
5. **P3.CI.001-006**: Unified CI and security (Weeks 13-14)
6. **P3.GOV.001-007**: Release infrastructure (Weeks 14-16)

### Contact Information

- **Technical Questions**: `#orpheus-integration` Slack channel
- **Blocker Escalation**: Technical Steering Committee
- **Security Issues**: `security@orpheus.dev`
- **Community Support**: GitHub Discussions

---

**END OF DOCUMENT**

_This consolidated implementation task breakdown provides the definitive execution plan for Orpheus SDK × Shmui integration. All tasks, acceptance criteria, dependencies, and validation requirements are normative and must be followed for successful completion. All amendments from ORP066 have been fully integrated. Deviations require Technical Steering Committee approval via amendment process._
