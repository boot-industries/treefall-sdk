---
source: standard-notes
sn_filename: "ORP064 Implementation Plan v1_0 Orpheus SDK × Shmui Integration-8b43ef84.txt"
prefix: orp
original_format: lexical
imported: 2026-05-01
status: archive
related:
  - data_pipeline_contracts_prevent_integration_failures
  - component_libraries_reduce_cross_project_ui_debt
  - async_architecture_enables_modularity
---

# ORP064 Implementation Plan v1.0 Orpheus SDK × Shmui Integration



**Document Type**: Implementation Specification

**Status**: Normative

**Dependencies**: ORP061 (Migration Plan), ORP062 (Technical Addendum), ORP063 (Governance Framework)

**Effective Date**: Upon technical steering approval




---




## Executive Summary



This document provides a comprehensive, actionable task breakdown for integrating the Shmui React UI into the Orpheus SDK monorepo. Tasks are organized by the four-phase timeline established in ORP061, with domain-specific groupings enabling parallel workstream execution. Each task includes explicit acceptance criteria, ORP section cross-references, tooling requirements, and dependency mappings.



**Total Estimated Tasks**: 127

**Critical Path Duration**: Spans all 4 phases (Phase 0 → Phase 4)

**Parallel Workstreams**: Up to 6 concurrent domains per phase

**Finish-Line Criteria**: 23 gated requirements




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



**Objective**: Establish monorepo structure and import Shmui without altering functionality

**Duration Estimate**: 1-2 weeks

**Gate Criteria**: All validation checks from ORP061 §Phase 0 pass




### A. Repository Structure Tasks




#### P0.REPO.001: Initialize Monorepo Workspace



**Description**: Create top-level `packages/` directory and configure PNPM workspace.



**Acceptance Criteria**:



- `packages/` directory created at repository root
- `pnpm-workspace.yaml` configured with `packages/*` glob
- Root `package.json` contains workspace configuration
- `pnpm install` executes without errors



**ORP References**: ORP061 §Phase 0 (Monorepo Initialization), ORP063 §II.B



**Tooling Requirements**:



- PNPM ≥8.0
- Node.js ≥18.0



**Dependencies**: None (foundational task)



**Validation Method**:




```
pnpm install
pnpm list --depth 0  # Should show workspace packages
```




---




#### P0.REPO.002: Import Shmui Codebase with History Preservation



**Description**: Migrate complete Shmui repository into `packages/shmui/` using subtree merge to preserve commit history.



**Acceptance Criteria**:



- All Shmui code resides in `packages/shmui/`
- Git history from `chrislyons/shmui` preserved
- No functional changes to Shmui code
- Original file structure maintained



**ORP References**: ORP061 §Phase 0 (Import Shmui Codebase)



**Tooling Requirements**:



- Git ≥2.30 (for subtree merge)



**Dependencies**: P0.REPO.001 →



**Validation Method**:




```
git log packages/shmui/  # Verify history preserved
diff -r packages/shmui/ ../shmui-backup/  # Verify content match
```




---




#### P0.REPO.003: Configure Package Namespacing



**Description**: Rename Shmui package to `@orpheus/ui` and establish naming conventions for all packages.



**Acceptance Criteria**:



- `packages/shmui/package.json` name set to `@orpheus/ui`
- Scoped package naming documented in `docs/PACKAGE_NAMING.md`
- Reserved names documented: `@orpheus/core`, `@orpheus/client`, `@orpheus/engine-*`
- NPM scope `@orpheus` registered (if publishing publicly)



**ORP References**: ORP061 §Phase 0 (Namespacing and Package Names), ORP063 §III.D



**Tooling Requirements**:



- NPM account with `@orpheus` scope access



**Dependencies**: P0.REPO.002 →



**Validation Method**:




```
jq '.name' packages/shmui/package.json  # Should output "@orpheus/ui"
```




---




#### P0.REPO.004: Preserve Orpheus C++ Build Structure



**Description**: Ensure Orpheus C++ code remains buildable at current location without path breakage.



**Acceptance Criteria**:



- All CMakeLists.txt files function unchanged
- Include paths resolve correctly
- GoogleTest suite runs successfully
- Adapters (minhost, optional REAPER) build successfully



**ORP References**: ORP061 §Phase 0 (Monorepo Initialization)



**Tooling Requirements**:



- CMake ≥3.20
- C++20-capable compiler



**Dependencies**: P0.REPO.001 →



**Validation Method**:




```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```




---




### B. Tooling Baseline Tasks




#### P0.REPO.005: Unify Linting and Formatting Configuration



**Description**: Configure linters for both C++ and TypeScript to coexist without conflicts.



**Acceptance Criteria**:



- `.clang-format` and `.clang-tidy` apply only to C++ files
- `.eslintrc.js` and `.prettierrc` apply only to JS/TS files
- Ignored directories configured (e.g., ESLint ignores `src/`, clang-format ignores `packages/`)
- `pnpm run lint` executes both lint passes successfully



**ORP References**: ORP061 §Phase 0 (Tooling Baseline Alignment)



**Tooling Requirements**:



- ESLint ≥8.0, Prettier ≥3.0
- clang-format ≥14.0, clang-tidy ≥14.0



**Dependencies**: P0.REPO.002 →



**Validation Method**:




```
pnpm run lint:js  # Lints TypeScript only
pnpm run lint:cpp  # Lints C++ only
pnpm run lint  # Runs both without conflicts
```




---




#### P0.REPO.006: Initialize Changesets Configuration



**Description**: Set up changesets for monorepo versioning with all packages registered.



**Acceptance Criteria**:



- `.changeset/config.json` created with package list
- Includes `packages/shmui` (as `@orpheus/ui`)
- Configured for independent or unified versioning strategy
- Changesets CLI functional



**ORP References**: ORP061 §Phase 0 (Tooling Baseline Alignment), ORP063 §III.D



**Tooling Requirements**:



- `@changesets/cli` ≥2.26



**Dependencies**: P0.REPO.003 →



**Validation Method**:




```
pnpm changeset  # Should prompt for changeset creation
pnpm changeset version  # Dry-run should succeed
```




---




### C. CI Infrastructure Tasks




#### P0.CI.001: Create Interim Parallel CI Workflow



**Description**: Establish GitHub Actions workflow that builds both Orpheus C++ and Shmui UI independently.



**Acceptance Criteria**:



- Workflow file `.github/workflows/interim-ci.yml` created
- Separate jobs for C++ build (matrix: Ubuntu, Windows, macOS)
- Separate job for Node/UI build and test
- Both job sets pass on current codebase
- Matrix caching configured (PNPM, CMake artifacts)



**ORP References**: ORP061 §Phase 0 (CI Parallelization), ORP063 §II.C



**Tooling Requirements**:



- GitHub Actions (hosted runners)



**Dependencies**: P0.REPO.004 →, P0.REPO.005 →



**Validation Method**:




```
# Trigger workflow and verify:
# - All matrix builds pass
# - Cache hit rate >50% on repeat runs
# - Total duration <20 minutes
```




---




#### P0.CI.002: Configure CI Branch Protection



**Description**: Set branch protection rules requiring interim CI passage for merges.



**Acceptance Criteria**:



- Main branch requires status checks: `build-cpp`, `build-ui`
- Required reviewers configured (minimum 1)
- Force push disabled
- Require linear history enabled



**ORP References**: ORP063 §III.D (version control governance)



**Tooling Requirements**:



- GitHub repository admin access



**Dependencies**: P0.CI.001 →



**Validation Method**:



- Attempt merge without passing CI (should be blocked)
- Verify protection rules in repository settings




---




### D. Documentation Tasks




#### P0.DOC.001: Create Documentation Index



**Description**: Establish `docs/INDEX.md` linking all ORP documents and implementation guides.



**Acceptance Criteria**:



- Index includes ORP061, ORP062, ORP063, ORP064
- Cross-references documented (which ORP references which)
- Migration phases mapped to technical specifications
- Navigation breadcrumbs for documentation hierarchy



**ORP References**: ORP063 §V.N



**Tooling Requirements**: None (Markdown editing)



**Dependencies**: None (can execute early)



**Validation Method**:



- Manual review of all links (no 404s)
- Verify all ORP documents referenced




---




#### P0.DOC.002: Document Package Naming Conventions



**Description**: Create `docs/PACKAGE_NAMING.md` with namespace reservation policy.



**Acceptance Criteria**:



- All `@orpheus/*` package names documented
- Internal codenames explained (e.g., "Shmui" = `@orpheus/ui`)
- Naming guidelines for future packages
- Scope management policy (who can publish to `@orpheus`)



**ORP References**: ORP061 §Phase 0 (Namespacing), ORP063 §III.D



**Tooling Requirements**: None



**Dependencies**: P0.REPO.003 ⇢



**Validation Method**:



- Technical steering review and approval




---




### E. Validation Checkpoint




#### P0.TEST.001: Execute Phase 0 Validation Checklist



**Description**: Run complete validation suite from ORP061 §Phase 0 checklist.



**Acceptance Criteria**:



- Orpheus C++ build and tests pass
- Shmui app/Storybook runs without errors
- CI passes on all platforms
- No lint/test regressions introduced
- All P0 tasks marked complete



**ORP References**: ORP061 §Phase 0 Validation Checklist



**Tooling Requirements**: All Phase 0 tooling



**Dependencies**: ALL P0.* tasks →



**Validation Method**:




```
# Run comprehensive validation
./scripts/validate-phase0.sh
# Should exit 0 with all checks passed
```




---




## III. PHASE 1: TOOLING NORMALIZATION AND BASIC INTEGRATION



**Objective**: Enable Orpheus core to be consumed by Shmui UI via basic driver integration

**Duration Estimate**: 3-4 weeks

**Gate Criteria**: All validation checks from ORP061 §Phase 1 and ORP063 §II.C pass




### A. Contract Schema Tasks




#### P1.CONT.001: Create Contract Schema Package



**Description**: Establish `@orpheus/contract` package with Zod schemas for contract v0.9.0 (alpha).



**Acceptance Criteria**:



- Package created at `packages/contract/`
- Directory structure: `schemas/v0.9.0/{commands,events,errors}.ts`
- `registry.ts` exports schemas by version
- Handshake and version negotiation schemas defined
- Package published internally for consumption



**ORP References**: ORP062 §1.2-1.5, ORP063 §III.E



**Tooling Requirements**:



- Zod ≥3.22



**Dependencies**: P0.REPO.003 →



**Validation Method**:




```
import { HandshakeSchema } from '@orpheus/contract/v0.9.0';
const result = HandshakeSchema.parse({ contractVersion: "0.9.0", ... });
// Should succeed without throwing
```




---




#### P1.CONT.002: Implement Contract Version Roadmap



**Description**: Create `docs/CONTRACT_ROADMAP.md` mapping contract versions to migration phases.



**Acceptance Criteria**:



- Phase 0-1: v0.9.0 (alpha) defined
- Phase 2: v1.0.0-beta defined
- Phase 3: v1.0.0 (stable) defined
- Phase 4: v1.1.0+ (post-release) defined
- Version bump criteria documented



**ORP References**: ORP063 §II.A



**Tooling Requirements**: None



**Dependencies**: P1.CONT.001 →



**Validation Method**:



- Technical review confirms alignment with ORP timeline




---




#### P1.CONT.003: Implement Minimal Command/Event Schemas



**Description**: Define core schemas needed for Phase 1 proof-of-concept.



**Acceptance Criteria**:



- `LoadSession` command schema defined
- `SessionChanged` event schema defined
- `Error` event schema with taxonomy from ORP062 §1.5
- `Handshake` schemas with version negotiation
- TypeScript types auto-generated from Zod schemas



**ORP References**: ORP062 §1.3-1.5



**Tooling Requirements**:



- Zod ≥3.22
- zod-to-ts (for type generation)



**Dependencies**: P1.CONT.001 →



**Validation Method**:




```
// All schemas validate example payloads
LoadSessionSchema.parse({ path: "/tmp/session.json" });
SessionChangedSchema.parse({ trackCount: 5, ... });
```




---




### B. Service Driver Tasks




#### P1.DRIV.001: Implement Service Driver Foundation



**Description**: Create `orpheusd` service with HTTP and WebSocket endpoints.



**Acceptance Criteria**:



- Package created at `packages/engine-service/`
- HTTP endpoints: `/health`, `/version`, `/contract`
- WebSocket endpoint: `/ws` for event streaming
- Binds to `127.0.0.1` only by default
- Graceful shutdown on `SIGTERM`



**ORP References**: ORP062 §2.1, ORP063 §II.B



**Tooling Requirements**:



- Express.js or Fastify
- ws (WebSocket library)



**Dependencies**: P1.CONT.003 →



**Validation Method**:




```
# Start service
orpheusd &
# Test endpoints
curl http://127.0.0.1:8080/health  # Should return 200
wscat -c ws://127.0.0.1:8080/ws  # Should connect
```




---




#### P1.DRIV.002: Implement Service Driver Command Handler



**Description**: Wire Orpheus C++ core library to service driver for command execution.



**Acceptance Criteria**:



- Service links against Orpheus core library
- `LoadSession` command calls Orpheus session API
- JSON serialization/deserialization functional
- Error handling returns structured errors per ORP062 §1.5
- Command validation via `@orpheus/contract` schemas



**ORP References**: ORP062 §2.1, ORP063 §II.B



**Tooling Requirements**:



- CMake integration for C++ linking
- Orpheus SDK built libraries



**Dependencies**: P1.DRIV.001 →, P0.REPO.004 →



**Validation Method**:




```
# Send LoadSession command via HTTP POST
curl -X POST http://127.0.0.1:8080/command \
  -H "Content-Type: application/json" \
  -d '{"type":"LoadSession","path":"fixtures/test.json"}'
# Should return success response
```




---




#### P1.DRIV.003: Implement Service Driver Event Emission



**Description**: Stream Orpheus events over WebSocket connection.



**Acceptance Criteria**:



- Events emitted as JSON over WebSocket
- Each event includes timestamp and sequence ID
- `SessionChanged` event fires on successful `LoadSession`
- Heartbeat emitted every 10 seconds
- Multiple simultaneous WebSocket clients supported



**ORP References**: ORP062 §2.1 (heartbeats), §1.4 (event schemas)



**Tooling Requirements**:



- WebSocket library with broadcast support



**Dependencies**: P1.DRIV.002 →



**Validation Method**:




```
// Connect WebSocket client and listen for events
const ws = new WebSocket('ws://127.0.0.1:8080/ws');
ws.onmessage = (msg) => {
  const event = JSON.parse(msg.data);
  console.log(event.type, event.timestamp);
};
// Should receive heartbeats every 10s
```




---




### C. Native Driver Tasks




#### P1.DRIV.004: Create Native Driver Package



**Description**: Initialize `@orpheus/engine-electron` with N-API binding structure.



**Acceptance Criteria**:



- Package created at `packages/engine-electron/`
- CMakeLists.txt configured for N-API addon
- Minimal binding exports `getVersion()` function
- Builds on Ubuntu (proof of concept)
- `package.json` configured with install script



**ORP References**: ORP062 §2.3, ORP063 §II.B



**Tooling Requirements**:



- node-addon-api ≥6.0
- CMake ≥3.20
- node-gyp or cmake-js



**Dependencies**: P0.REPO.004 →



**Validation Method**:




```
const orpheus = require('@orpheus/engine-electron');
console.log(orpheus.getVersion());
// Should print Orpheus version string
```




---




#### P1.DRIV.005: Implement Native Driver Command Interface



**Description**: Expose basic command execution via N-API.



**Acceptance Criteria**:



- `loadSession(path)` function exposed to Node
- Function calls Orpheus C++ session API
- Returns promise resolving to success/error
- Error taxonomy matches ORP062 §1.5
- Memory management verified (no leaks on load/unload)



**ORP References**: ORP062 §2.3



**Tooling Requirements**:



- Valgrind or AddressSanitizer for leak detection



**Dependencies**: P1.DRIV.004 →



**Validation Method**:




```
const orpheus = require('@orpheus/engine-electron');
await orpheus.loadSession('fixtures/test.json');
// Should resolve without throwing
```




---




#### P1.DRIV.006: Implement Native Driver Event Emitter



**Description**: Use Node EventEmitter pattern for Orpheus events.



**Acceptance Criteria**:



- Addon inherits from EventEmitter
- C++ code can emit events to JS
- `sessionChanged` event emitted on successful load
- Event payloads match `@orpheus/contract` schemas
- Listeners can be attached/removed from JS



**ORP References**: ORP062 §2.3, §1.4



**Tooling Requirements**:



- Node.js EventEmitter API



**Dependencies**: P1.DRIV.005 →



**Validation Method**:




```
const orpheus = require('@orpheus/engine-electron');
orpheus.on('sessionChanged', (data) => {
  console.log('Session loaded:', data.trackCount);
});
await orpheus.loadSession('fixtures/test.json');
// Should trigger event
```




---




### D. Client Broker Tasks




#### P1.DRIV.007: Create Client Broker Package



**Description**: Implement `@orpheus/client` with driver selection and abstraction.



**Acceptance Criteria**:



- Package created at `packages/client/`
- Broker detects available drivers (Service, Native)
- Selection priority: Native → Service (as per ORP063 §II.B)
- Single unified API: `client.send(command)`, `client.on(event)`
- Async command execution with Promise-based interface



**ORP References**: ORP062 §3.1, ORP063 §III.E



**Tooling Requirements**: None (pure TypeScript)



**Dependencies**: P1.DRIV.003 →, P1.DRIV.006 →



**Validation Method**:




```
import { OrpheusClient } from '@orpheus/client';
const client = await OrpheusClient.connect();
console.log('Connected via:', client.driver);  // 'native' or 'service'
```




---




#### P1.DRIV.008: Implement Client Handshake Protocol



**Description**: Establish version negotiation during client-engine connection.



**Acceptance Criteria**:



- Client sends handshake with contract version on connect
- Engine responds with supported version and capabilities
- Connection rejected if MAJOR version mismatch
- Handshake timeout configured (5 seconds default)
- Retry logic for transient failures



**ORP References**: ORP062 §1.2, §1.6



**Tooling Requirements**: None



**Dependencies**: P1.DRIV.007 →



**Validation Method**:




```
// Test version mismatch rejection
const client = await OrpheusClient.connect({ contractVersion: '2.0.0' });
// Should throw or reject if engine only supports 0.9.0
```




---




### E. UI Integration Tasks




#### P1.UI.001: Create Orpheus Integration Test Hook



**Description**: Add debug panel to Shmui UI for calling Orpheus functions.



**Acceptance Criteria**:



- Debug panel visible in development mode only
- "Get Core Version" button calls `client.send({ type: 'GetVersion' })`
- "Load Test Session" button calls `LoadSession` with fixture
- Results displayed in UI (version string, success/error)
- Panel disabled/hidden in production builds



**ORP References**: ORP061 §Phase 1 (Basic UI-Core Bridging)



**Tooling Requirements**:



- React ≥18.0



**Dependencies**: P1.DRIV.007 →



**Validation Method**:




```
NODE_ENV=development pnpm dev
# Open app, click "Get Core Version"
# Should display Orpheus version in UI
```




---




#### P1.UI.002: Implement React OrpheusProvider Context



**Description**: Create React context for sharing Orpheus client throughout component tree.



**Acceptance Criteria**:



- `OrpheusProvider` component wraps app root
- `useOrpheus()` hook returns client instance
- Connection state exposed: `{ status, driver, error }`
- Auto-connect on mount, cleanup on unmount
- Error boundary handles connection failures



**ORP References**: ORP062 §3.2, ORP063 §II.B



**Tooling Requirements**:



- React ≥18.0



**Dependencies**: P1.DRIV.007 →



**Validation Method**:




```
function TestComponent() {
  const { client, status } = useOrpheus();
  return <div>Status: {status}</div>;
}
// Should show "connected" when engine available
```




---




### F. CI Integration Tasks




#### P1.CI.001: Extend CI for Driver Builds



**Description**: Add driver compilation to CI matrix.



**Acceptance Criteria**:



- Service driver builds on Ubuntu
- Native driver builds on Ubuntu, Windows, macOS
- Driver build artifacts cached
- Build failures fail CI



**ORP References**: ORP063 §II.C



**Tooling Requirements**:



- GitHub Actions matrix configuration



**Dependencies**: P1.DRIV.002 →, P1.DRIV.005 →



**Validation Method**:




```
# CI workflow includes:
- name: Build Service Driver
  run: pnpm build:service
- name: Build Native Driver
  run: pnpm build:native
# All should succeed
```




---




#### P1.CI.002: Add Integration Smoke Tests to CI



**Description**: Execute basic integration tests in CI.



**Acceptance Criteria**:



- Test: Start service driver, connect client, send command
- Test: Load native driver, call getVersion()
- Test: Client broker selects correct driver
- Tests run on Ubuntu (representative platform)
- Failures block PR merge



**ORP References**: ORP061 §Phase 1 Validation, ORP063 §II.C



**Tooling Requirements**:



- Jest or Vitest for test execution



**Dependencies**: P1.DRIV.007 →, P1.CI.001 →



**Validation Method**:




```
pnpm test:integration
# Should pass all smoke tests
```




---




### G. Documentation Tasks




#### P1.DOC.001: Document Driver Architecture



**Description**: Create developer guide for driver implementations.



**Acceptance Criteria**:



- Document created: `docs/DRIVER_ARCHITECTURE.md`
- Explains Service, Native, WASM driver roles
- Diagrams showing client-driver communication
- Code examples for each driver type
- Cross-references ORP062 §2.0



**ORP References**: ORP062 §2.0, ORP063 §V.N



**Tooling Requirements**: None (Markdown + diagrams)



**Dependencies**: P1.DRIV.003 →, P1.DRIV.006 →



**Validation Method**:



- Technical review confirms accuracy




---




#### P1.DOC.002: Create Contract Development Guide



**Description**: Document how to add new commands/events to contract.



**Acceptance Criteria**:



- Document created: `docs/CONTRACT_DEVELOPMENT.md`
- Step-by-step guide for adding commands
- Schema versioning workflow explained
- Examples of command handler implementation
- Testing requirements specified



**ORP References**: ORP062 §1.0, ORP063 §III.E



**Tooling Requirements**: None



**Dependencies**: P1.CONT.003 →



**Validation Method**:



- Follow guide to add dummy command; should succeed




---




### H. Validation Checkpoint




#### P1.TEST.001: Execute Phase 1 Validation Checklist



**Description**: Run complete validation suite from ORP061 §Phase 1 and ORP063 §II.C.



**Acceptance Criteria**:



- Orpheus binding compiles on all OS
- Smoke test Orpheus call succeeds
- No memory leaks detected
- UI integration manual test passes
- Storybook builds without errors
- Backwards compatibility toggle verified
- Contract handshake succeeds
- All command/event schemas validated



**ORP References**: ORP061 §Phase 1 Validation, ORP063 §II.C



**Tooling Requirements**: All Phase 1 tooling



**Dependencies**: ALL P1.* tasks →



**Validation Method**:




```
./scripts/validate-phase1.sh
# Should exit 0 with comprehensive report
```




---




## IV. PHASE 2: FEATURE INTEGRATION & BRIDGING



**Objective**: Leverage Orpheus capabilities in UI and implement user-facing features

**Duration Estimate**: 4-6 weeks

**Gate Criteria**: All validation checks from ORP061 §Phase 2 pass




### A. WASM Driver Tasks




#### P2.DRIV.001: Initialize WASM Build Infrastructure



**Description**: Set up Emscripten toolchain for compiling Orpheus to WebAssembly.



**Acceptance Criteria**:



- Package created at `packages/engine-wasm/`
- CMakeLists.txt configured for Emscripten
- Minimal WASM module exports `getVersion()`
- `.wasm` and `.js` glue files generated
- Module instantiates in Node environment



**ORP References**: ORP062 §2.2, ORP063 §II.B



**Tooling Requirements**:



- Emscripten SDK ≥3.1.45



**Dependencies**: P0.REPO.004 →



**Validation Method**:




```
emcc --version  # Verify Emscripten installed
pnpm build:wasm
node -e "require('./packages/engine-wasm/build/orpheus.js')"
# Should load without errors
```




---




#### P2.DRIV.002: Implement WASM Web Worker Wrapper



**Description**: Create Web Worker that hosts WASM module to avoid blocking main thread.



**Acceptance Criteria**:



- Worker script loads WASM module
- `postMessage` interface for commands
- Worker emits events via `postMessage`
- Async command execution in worker thread
- Worker lifecycle managed (init, terminate)



**ORP References**: ORP062 §2.2



**Tooling Requirements**:



- Web Worker API



**Dependencies**: P2.DRIV.001 →



**Validation Method**:




```
const worker = new Worker('orpheus.wasm.js');
worker.postMessage({ type: 'GetVersion' });
worker.onmessage = (e) => console.log(e.data);
// Should receive version response
```




---




#### P2.DRIV.003: Implement WASM Command Interface



**Description**: Wire contract commands to WASM module functions.



**Acceptance Criteria**:



- `LoadSession` command functional in WASM
- JSON serialization via Emscripten embind
- Memory management optimized (reuse buffers)
- Command validation via `@orpheus/contract`
- Error handling returns structured errors



**ORP References**: ORP062 §2.2, §1.3



**Tooling Requirements**:



- Emscripten embind



**Dependencies**: P2.DRIV.002 →



**Validation Method**:




```
worker.postMessage({
  type: 'LoadSession',
  data: { path: 'test.json' }
});
// Should receive SessionChanged event
```




---




#### P2.DRIV.004: Integrate WASM into Client Broker



**Description**: Add WASM driver to broker selection logic.



**Acceptance Criteria**:



- Broker priority updated: Native → WASM → Service
- WASM driver detected in browser contexts
- Worker instantiation handled by broker
- Fallback to Service if WASM unavailable
- SRI verification for WASM module load



**ORP References**: ORP062 §3.1, §2.2 (security)



**Tooling Requirements**: None



**Dependencies**: P2.DRIV.003 →, P1.DRIV.007 →



**Validation Method**:




```
// In browser context
const client = await OrpheusClient.connect();
console.log(client.driver);  // Should be 'wasm' in browser
```




---




### B. Contract Schema Expansion Tasks




#### P2.CONT.001: Upgrade Contract to v1.0.0-beta



**Description**: Expand contract with full command/event set for Phase 2 features.



**Acceptance Criteria**:



- New directory: `packages/contract/schemas/v1.0.0-beta/`
- All commands from ORP062 §1.3 defined
- All events from ORP062 §1.4 defined
- Event frequency constraints documented
- Migration path from v0.9.0 documented



**ORP References**: ORP062 §1.3-1.4, ORP063 §II.A



**Tooling Requirements**:



- Zod ≥3.22



**Dependencies**: P1.CONT.003 →



**Validation Method**:




```
import * as v1beta from '@orpheus/contract/v1.0.0-beta';
// All schemas should be importable
v1beta.RenderClickSchema.parse({ bars: 2, bpm: 120 });
```




---




#### P2.CONT.002: Implement Event Frequency Validation



**Description**: Add runtime checks for event frequency constraints.



**Acceptance Criteria**:



- `TransportTick` limited to ≤30 Hz
- `RenderProgress` limited to ≤10 Hz
- Violations logged as warnings
- Rate limiter implementation tested
- Configurable thresholds



**ORP References**: ORP062 §1.4



**Tooling Requirements**: None (pure TypeScript)



**Dependencies**: P2.CONT.001 →



**Validation Method**:




```
// Emit 100 TransportTick events in 1 second
// Should throttle to ~30 events/sec
```




---




### C. UI Feature Integration Tasks




#### P2.UI.001: Implement Session Manager Panel



**Description**: Create UI for loading and viewing Orpheus sessions.



**Acceptance Criteria**:



- Panel added to Shmui UI (behind feature flag initially)
- "Load Session" button triggers file picker
- Session loaded via `LoadSession` command
- Track list displayed (track names, count)
- Session metadata shown (BPM, length)
- Loading/error states handled



**ORP References**: ORP061 §Phase 2 (Local Session Management)



**Tooling Requirements**:



- React ≥18.0
- FileSystem Access API (for browser) or Electron dialog



**Dependencies**: P2.CONT.001 →, P1.UI.002 →



**Validation Method**:




```
# Open app, load fixtures/test.json
# Verify UI shows track list matching JSON
```




---




#### P2.UI.002: Implement Click Track Generator



**Description**: Create UI controls for rendering click tracks via Orpheus.



**Acceptance Criteria**:



- Form inputs: BPM (numeric), Bars (numeric)
- "Generate Click Track" button
- Calls `RenderClick` command with parameters
- Audio data returned and loaded into AudioPlayer
- Progress bar shows render progress (via `RenderProgress` events)
- Completed audio playable in UI



**ORP References**: ORP061 §Phase 2 (Audio Generation Integration)



**Tooling Requirements**:



- Existing Shmui AudioPlayer component



**Dependencies**: P2.CONT.001 →, P2.UI.001 →



**Validation Method**:




```
# Input: 120 BPM, 2 bars
# Click "Generate"
# Should hear metronome click when played
```




---




#### P2.UI.003: Integrate Orb with Orpheus State



**Description**: Connect Orb visualization to Orpheus transport/render state.



**Acceptance Criteria**:



- Orb pulses to beat during playback (via `TransportTick`)
- Orb enters "thinking" state during render
- State transitions smooth (no jarring changes)
- Orb disabled if Orpheus unavailable
- Animation performance remains >30 FPS



**ORP References**: ORP061 §Phase 2 (Enrich Existing Components)



**Tooling Requirements**:



- Existing Shmui Orb component



**Dependencies**: P2.CONT.002 →, P1.UI.002 →



**Validation Method**:




```
# Play session with transport
# Observe Orb pulsing in sync with beats
```




---




#### P2.UI.004: Implement Track Add/Remove Operations



**Description**: Enable UI-driven track management with Orpheus backend.



**Acceptance Criteria**:



- "Add Track" button appends track to session
- Track list updates after addition
- "Remove Track" button (per track) deletes track
- Operations reflected in Orpheus internal state
- Undo/redo support (optional stretch goal)



**ORP References**: ORP061 §Phase 2 (Local Session Management)



**Tooling Requirements**: None



**Dependencies**: P2.UI.001 →



**Validation Method**:




```
# Add track, verify appears in list
# Remove track, verify disappears
# Query Orpheus state, confirm changes persisted
```




---




#### P2.UI.005: Implement Feature Toggle System



**Description**: Create runtime toggle for Orpheus-powered features.



**Acceptance Criteria**:



- Environment variable `ENABLE_ORPHEUS_FEATURES`
- UI settings panel with "Use Local Engine" checkbox
- Toggle persists across sessions (localStorage in dev)
- Features gracefully degrade when disabled
- Default: enabled in dev, configurable in production



**ORP References**: ORP061 §Phase 2 (Backward Compatibility & Toggles)



**Tooling Requirements**: None



**Dependencies**: P2.UI.001 → (need feature to toggle)



**Validation Method**:




```
ENABLE_ORPHEUS_FEATURES=false pnpm dev
# Orpheus features should be hidden/disabled
```




---




### D. Testing Infrastructure Tasks




#### P2.TEST.001: Implement Golden Audio Test Suite



**Description**: Create deterministic audio rendering validation tests.



**Acceptance Criteria**:



- Test cases from ORP062 §4.1 implemented
- Reference WAV files stored in `packages/contract/fixtures/golden-audio/`
- Tests compute SHA-256 hash of rendered audio
- ±1 LSB RMS tolerance allowed
- Tests run across all three drivers



**ORP References**: ORP062 §4.1, ORP063 §IV.G



**Tooling Requirements**:



- Audio hash computation library
- Reference audio files



**Dependencies**: P2.CONT.001 →, P2.DRIV.004 →



**Validation Method**:




```
pnpm test:golden-audio
# All tests should pass with hash matches
```




---




#### P2.TEST.002: Implement Contract Compliance Test Matrix



**Description**: Comprehensive test suite for contract adherence.



**Acceptance Criteria**:



- 100% command coverage (success + failure cases)
- All event schemas validated
- All error codes have reproduction tests
- Tests execute against each driver
- CI generates coverage report with ≥95% threshold



**ORP References**: ORP062 §4.2, ORP063 §IV.H



**Tooling Requirements**:



- Jest/Vitest with coverage plugin



**Dependencies**: P2.CONT.001 →, P2.DRIV.004 →



**Validation Method**:




```
pnpm test:compliance --coverage
# Coverage report: commands 100%, events 100%, errors 100%
```




---




#### P2.TEST.003: Implement Performance Instrumentation



**Description**: Add telemetry collection for performance metrics.



**Acceptance Criteria**:



- Command latency tracked per command type (p95, p99)
- Event frequency measured per event type
- Bundle size measured for UI and WASM
- WASM load time measured
- Memory usage tracked over time



**ORP References**: ORP063 §IV.I



**Tooling Requirements**:



- Custom telemetry module
- Performance API



**Dependencies**: P2.DRIV.004 →



**Validation Method**:




```
pnpm perf:measure
# Should output metrics JSON with all categories
```




---




### E. CI Enhancement Tasks




#### P2.CI.001: Add Golden Audio Tests to CI



**Description**: Integrate deterministic audio validation into CI pipeline.



**Acceptance Criteria**:



- Golden audio tests run on Ubuntu
- Tests execute for all three drivers
- Failures block PR merge
- Artifacts (audio files) uploadable for inspection



**ORP References**: ORP063 §IV.G



**Tooling Requirements**:



- CI with audio processing capability



**Dependencies**: P2.TEST.001 →



**Validation Method**:




```
- name: Golden Audio Validation
  run: pnpm test:golden-audio
  # Should pass in CI
```




---




#### P2.CI.002: Add Contract Compliance Tests to CI



**Description**: Execute full compliance matrix in CI.



**Acceptance Criteria**:



- Compliance tests run on Ubuntu
- All drivers tested
- Coverage report generated
- Minimum 95% coverage enforced



**ORP References**: ORP063 §IV.H



**Tooling Requirements**:



- Jest/Vitest coverage reporting



**Dependencies**: P2.TEST.002 →



**Validation Method**:




```
- name: Contract Compliance
  run: pnpm test:compliance --coverage
  # Coverage must meet threshold
```




---




### F. Documentation Tasks




#### P2.DOC.001: Document UI Integration Patterns



**Description**: Create guide for integrating Orpheus into React components.



**Acceptance Criteria**:



- Document created: `docs/UI_INTEGRATION.md`
- Examples of using `useOrpheus()` hook
- Patterns for command execution
- Event subscription examples
- Error handling best practices



**ORP References**: ORP061 §Phase 2 (Docs & Storybook Updates)



**Tooling Requirements**: None



**Dependencies**: P2.UI.002 →



**Validation Method**:



- Developer follows guide to integrate new component




---




#### P2.DOC.002: Update Storybook with Orpheus Stories



**Description**: Add Storybook stories demonstrating Orpheus-powered components.



**Acceptance Criteria**:



- Story: Session Manager component
- Story: Click Track Generator
- Story: Orb with Orpheus state
- Stories use mock Orpheus client in Storybook
- Storybook builds and deploys successfully



**ORP References**: ORP061 §Phase 2 (Docs & Storybook Updates)



**Tooling Requirements**:



- Storybook ≥7.0



**Dependencies**: P2.UI.002 →, P2.UI.003 →



**Validation Method**:




```
pnpm storybook
# Navigate to Orpheus stories, verify functionality
```




---




### G. Validation Checkpoint




#### P2.TEST.004: Execute Phase 2 Validation Checklist



**Description**: Run complete validation suite from ORP061 §Phase 2.



**Acceptance Criteria**:



- Session import/export functional
- Click track generation works end-to-end
- Track operations (add/remove) validated
- Orb reactivity confirmed
- No regressions in original features
- Performance acceptable (click track <1s)
- Cross-platform testing complete
- Feature flags tested
- All Phase 2 tasks complete



**ORP References**: ORP061 §Phase 2 Validation



**Tooling Requirements**: All Phase 2 tooling



**Dependencies**: ALL P2.* tasks →



**Validation Method**:




```
./scripts/validate-phase2.sh
# Comprehensive validation across all criteria
```




---




## V. PHASE 3: UNIFIED BUILD, TESTING, AND RELEASE PREPARATION



**Objective**: Unify tooling, CI/CD, quality checks, and prepare for release

**Duration Estimate**: 3-4 weeks

**Gate Criteria**: All validation checks from ORP061 §Phase 3 pass




### A. CI Unification Tasks




#### P3.CI.001: Consolidate CI Pipelines



**Description**: Merge interim workflows into single comprehensive pipeline.



**Acceptance Criteria**:



- Single workflow file: `.github/workflows/ci_pipeline.yml`
- Matrix builds: C++ on all OS, UI on Ubuntu
- Unified dependency installation (PNPM)
- Parallel job execution (C++, UI, tests)
- Caching optimized (PNPM, CMake, native builds)
- Total duration <25 minutes



**ORP References**: ORP061 §Phase 3 (Merge CI Pipelines), ORP063 §IV.G



**Tooling Requirements**:



- GitHub Actions workflow optimization



**Dependencies**: P2.CI.002 →



**Validation Method**:




```
# Trigger pipeline on PR
# Verify:
# - All jobs pass
# - Parallel execution
# - Cache hit rate >70%
```




---




#### P3.CI.002: Implement Performance Budget Enforcement



**Description**: Add CI gates for performance regressions.



**Acceptance Criteria**:



- `budgets.json` with thresholds from ORP062 §5.3
- CI step: `pnpm perf:validate --budgets=budgets.json`
- Bundle size check (UI ≤1.5MB, WASM ≤5MB)
- Latency check (p95 LoadSession ≤200ms)
- Violations fail CI with detailed report
- 10% degradation tolerance for latency, 15% for size



**ORP References**: ORP062 §5.3, ORP063 §IV.I



**Tooling Requirements**:



- Bundle analyzer
- Performance validation script



**Dependencies**: P2.TEST.003 →



**Validation Method**:




```
# Introduce oversized bundle
pnpm build
pnpm perf:validate
# Should fail CI with budget exceeded message
```




---




#### P3.CI.003: Implement Chaos Testing in CI



**Description**: Add nightly chaos testing workflow for failure scenario validation.



**Acceptance Criteria**:



- Separate workflow: `.github/workflows/chaos-tests.yml`
- Scheduled: runs nightly
- All scenarios from ORP063 §IV.J implemented
- Tests validate recovery within SLA (e.g., <10s reconnect)
- Results reported to Slack/email on failure



**ORP References**: ORP062 §4.3, ORP063 §IV.J



**Tooling Requirements**:



- Extended timeout configuration



**Dependencies**: P2.TEST.002 →



**Validation Method**:




```
# Manual trigger chaos tests
pnpm test:chaos
# All recovery scenarios should pass
```




---




### B. Build Optimization Tasks




#### P3.REPO.001: Optimize Bundle Size



**Description**: Reduce production bundle sizes through code splitting and lazy loading.



**Acceptance Criteria**:



- WASM module loaded on-demand (not in initial bundle)
- Tree-shaking configured for UI components
- Unused code eliminated
- Bundle size meets ORP062 §5.3 limits
- Source maps generated for debugging



**ORP References**: ORP062 §5.3, ORP061 §Phase 3 (Performance Optimization)



**Tooling Requirements**:



- Webpack or Vite bundle analyzer



**Dependencies**: P2.DRIV.004 →



**Validation Method**:




```
pnpm build --analyze
# Bundle report shows UI <1.5MB, WASM lazy-loaded
```




---




#### P3.REPO.002: Implement WASM in Web Worker



**Description**: Move heavy WASM computation to Web Worker to prevent UI blocking.



**Acceptance Criteria**:



- WASM instantiated in dedicated Web Worker
- `postMessage` API for command/event communication
- Main thread remains responsive during WASM operations
- Worker lifecycle managed (start/stop/restart)
- Error propagation from worker to main thread



**ORP References**: ORP061 §Phase 3 (Performance Optimization), ORP062 §2.2



**Tooling Requirements**:



- Web Worker API



**Dependencies**: P2.DRIV.003 →



**Validation Method**:




```
// Trigger heavy WASM operation
// UI animations should maintain 60 FPS
```




---




#### P3.REPO.003: Optimize Native Binary Size



**Description**: Strip debug symbols and optimize native addon binaries.



**Acceptance Criteria**:



- Release builds use `-O3` optimization
- Debug symbols stripped in production binaries
- Link-time optimization (LTO) enabled
- Binary size reduced by ≥30% vs debug builds
- Functionality unchanged (validated by tests)



**ORP References**: ORP062 §5.1



**Tooling Requirements**:



- CMake release configuration
- `strip` utility



**Dependencies**: P1.DRIV.005 →



**Validation Method**:




```
ls -lh packages/engine-electron/build/Release/orpheus.node
# Should be significantly smaller than Debug build
```




---




### C. Release Infrastructure Tasks




#### P3.GOV.001: Configure Changesets for Release



**Description**: Finalize changesets configuration for versioned releases.



**Acceptance Criteria**:



- All packages included in changesets config
- Version strategy defined (unified vs independent)
- Changelog generation configured
- Pre-release tags supported (`beta`, `rc`)
- Package dependencies automatically updated



**ORP References**: ORP061 §Phase 3 (Release Versioning), ORP063 §III.D



**Tooling Requirements**:



- `@changesets/cli` ≥2.26



**Dependencies**: P0.REPO.006 →



**Validation Method**:




```
# Create changeset
pnpm changeset
# Bump versions
pnpm changeset version
# Verify package.json versions updated correctly
```




---




#### P3.GOV.002: Implement NPM Publish Workflow



**Description**: Automate package publishing via GitHub Actions.



**Acceptance Criteria**:



- Workflow: `.github/workflows/publish.yml`
- Triggers on release tag or manual dispatch
- Builds all packages
- Publishes to NPM with `@orpheus` scope
- Updates GitHub Release with changelog
- Only runs from protected branch (main)



**ORP References**: ORP061 §Phase 3 (Release Versioning)



**Tooling Requirements**:



- NPM_TOKEN secret in GitHub
- GitHub Personal Access Token for releases



**Dependencies**: P3.GOV.001 →



**Validation Method**:




```
# Create test release tag
git tag v1.0.0-rc.1
git push origin v1.0.0-rc.1
# Verify workflow publishes packages
```




---




#### P3.GOV.003: Generate Prebuilt Native Binaries



**Description**: Build and publish prebuilt native addon binaries for major platforms.



**Acceptance Criteria**:



- Binaries built for: macOS (x64, arm64), Windows (x64), Linux (x64)
- SHA-256 checksums generated per binary
- Binaries signed (macOS notarized, Windows signed if possible)
- Attached to GitHub Releases
- `@orpheus/engine-electron` postinstall downloads prebuilt



**ORP References**: ORP062 §5.1, ORP063 §V.M



**Tooling Requirements**:



- GitHub Actions matrix builds
- Signing certificates (macOS, Windows)



**Dependencies**: P3.REPO.003 →



**Validation Method**:




```
# Install package on fresh system
npm install @orpheus/engine-electron
# Should download prebuilt binary, not compile from source
```




---




#### P3.GOV.004: Implement Binary Verification



**Description**: Add checksum verification to binary installation process.



**Acceptance Criteria**:



- Postinstall script verifies SHA-256 checksum
- Checksum file downloaded from GitHub Releases
- Mismatch throws error and aborts installation
- Optional signature verification (GPG)
- Documented in `BINARY_VERIFICATION.md`



**ORP References**: ORP063 §V.M



**Tooling Requirements**:



- Node.js crypto module



**Dependencies**: P3.GOV.003 →



**Validation Method**:




```
# Tamper with binary
# Reinstall package
# Should fail with checksum mismatch error
```




---




### D. Testing and Quality Tasks




#### P3.TEST.001: Achieve Test Coverage Targets



**Description**: Increase test coverage to meet quality thresholds.



**Acceptance Criteria**:



- Unit test coverage: ≥80% overall
- Integration test coverage: ≥90% for contract APIs
- Critical path coverage: 100% (session, render, transport)
- Coverage report generated in CI
- Coverage badge in README



**ORP References**: ORP061 §Phase 3 (Comprehensive Testing)



**Tooling Requirements**:



- Jest/Vitest coverage tooling



**Dependencies**: P2.TEST.002 →



**Validation Method**:




```
pnpm test:coverage
# Coverage report meets thresholds
```




---




#### P3.TEST.002: Implement End-to-End Testing



**Description**: Add automated E2E tests for complete user workflows.



**Acceptance Criteria**:



- E2E framework configured (Playwright or Cypress)
- Test: Load session → Add track → Render click → Play audio
- Test: Connect to each driver type
- Test: Graceful degradation on driver unavailability
- Tests run in CI (headless)



**ORP References**: ORP061 §Phase 3 (Comprehensive Testing)



**Tooling Requirements**:



- Playwright ≥1.40 or Cypress ≥13.0



**Dependencies**: P2.UI.004 →



**Validation Method**:




```
pnpm test:e2e
# All E2E scenarios pass
```




---




#### P3.TEST.003: Run Memory Leak Detection



**Description**: Profile applications for memory leaks and performance issues.



**Acceptance Criteria**:



- Native addon tested with Valgrind/AddressSanitizer
- WASM tested with browser DevTools memory profiler
- 1000-iteration stress test shows stable memory
- No leaks detected in critical paths
- Results documented



**ORP References**: ORP061 §Phase 3 (Performance Optimization)



**Tooling Requirements**:



- Valgrind, AddressSanitizer
- Chrome DevTools



**Dependencies**: P3.REPO.002 →



**Validation Method**:




```
# Run stress test
pnpm test:stress
# Memory usage should be stable after warmup
```




---




### E. Documentation Finalization Tasks




#### P3.DOC.001: Complete Migration Guide



**Description**: Comprehensive guide for users migrating from old packages.



**Acceptance Criteria**:



- Document created: `docs/MIGRATION_GUIDE.md`
- Step-by-step migration instructions
- Breaking changes listed (from ORP061 §Breaking Changes)
- Code examples (before/after)
- Links to codemods



**ORP References**: ORP061 §Breaking Changes, ORP063 §III.F



**Tooling Requirements**: None



**Dependencies**: P2.DOC.002 →



**Validation Method**:



- External user successfully migrates using guide




---




#### P3.DOC.002: Finalize API Documentation



**Description**: Generate complete API reference documentation.



**Acceptance Criteria**:



- TypeDoc or similar generates API docs
- All public interfaces documented
- Examples for each major API
- Hosted on GitHub Pages or similar
- Versioned documentation (per release)



**ORP References**: ORP061 §Phase 3 (Documentation)



**Tooling Requirements**:



- TypeDoc ≥0.25



**Dependencies**: P2.CONT.001 →



**Validation Method**:




```
pnpm docs:generate
# API docs published to docs site
```




---




#### P3.DOC.003: Create Contributor Guide



**Description**: Comprehensive guide for new contributors.



**Acceptance Criteria**:



- Document updated: `CONTRIBUTING.md`
- Setup instructions (C++ and UI dev)
- Code style guidelines
- PR submission process
- Testing requirements
- Links to Architecture Decision Records



**ORP References**: ORP062 §6.4



**Tooling Requirements**: None



**Dependencies**: P3.CI.001 →



**Validation Method**:



- New contributor follows guide and submits PR




---




#### P3.DOC.004: Update Main README



**Description**: Revise README to reflect unified monorepo structure.



**Acceptance Criteria**:



- Overview mentions Shmui UI integration
- Quick start covers both UI and core
- Installation instructions updated
- Links to all key documentation
- Badges (build status, coverage, version)



**ORP References**: ORP061 §Phase 3 (Documentation)



**Tooling Requirements**: None



**Dependencies**: P3.TEST.001 →, P3.GOV.001 →



**Validation Method**:



- Technical review confirms accuracy




---




### F. Validation Checkpoint




#### P3.TEST.004: Execute Phase 3 Validation Checklist



**Description**: Run complete validation suite from ORP061 §Phase 3.



**Acceptance Criteria**:



- All tests passing in unified CI
- Coverage targets met
- Manual full regression completed
- Performance profile acceptable
- Package audit passed
- Documentation complete
- Stakeholder sign-off obtained
- All Phase 3 tasks complete



**ORP References**: ORP061 §Phase 3 Validation



**Tooling Requirements**: All Phase 3 tooling



**Dependencies**: ALL P3.* tasks →



**Validation Method**:




```
./scripts/validate-phase3.sh
# Comprehensive validation report
```




---




## VI. PHASE 4: GRADUAL ROLLOUT AND POST-MIGRATION SUPPORT



**Objective**: Deploy unified system, monitor, and provide support

**Duration Estimate**: 2-4 weeks

**Gate Criteria**: All validation checks from ORP061 §Phase 4 pass




### A. Deployment Tasks




#### P4.GOV.001: Publish Beta Release



**Description**: Release v1.0.0-beta packages to NPM.



**Acceptance Criteria**:



- Version bumped to 1.0.0-beta.1
- Changelog generated
- Packages published to NPM with `beta` tag
- GitHub Release created with notes
- Announcement posted (blog, Discord, etc.)



**ORP References**: ORP061 §Phase 4 (Beta Release & Testing)



**Tooling Requirements**:



- NPM access
- GitHub Releases



**Dependencies**: P3.TEST.004 →



**Validation Method**:




```
npm info @orpheus/ui
# Should show beta version available
```




---




#### P4.GOV.002: Archive Old Repositories



**Description**: Mark separate Shmui and legacy repositories as read-only.



**Acceptance Criteria**:



- `chrislyons/shmui` repository archived
- README updated with link to Orpheus SDK monorepo
- Issues/PRs closed with migration notice
- Topics/tags updated to indicate archived status



**ORP References**: ORP061 §Phase 4 (Deprecate Old Repos)



**Tooling Requirements**:



- GitHub repository admin access



**Dependencies**: P4.GOV.001 →



**Validation Method**:



- Verify repositories show "Archived" badge




---




#### P4.GOV.003: Establish Support Channels



**Description**: Create support infrastructure for beta testers.



**Acceptance Criteria**:



- GitHub Discussions enabled
- Issue templates created (bug, feature, question)
- Support email or Discord channel announced
- Known issues documented
- Feedback collection process defined



**ORP References**: ORP061 §Phase 4 (Beta Release & Testing)



**Tooling Requirements**: None



**Dependencies**: P4.GOV.001 →



**Validation Method**:



- Test submitting issue via template




---




### B. Monitoring Tasks




#### P4.GOV.004: Implement Telemetry Collection



**Description**: Optional analytics for tracking adoption and issues.



**Acceptance Criteria**:



- Telemetry library integrated (opt-in only)
- Privacy policy documented
- Collects: version, driver type, error rates
- Dashboard for viewing metrics
- Respects user privacy (no PII)



**ORP References**: ORP062 §6.1 (Security)



**Tooling Requirements**:



- Analytics service (Plausible, Posthog, etc.)



**Dependencies**: P4.GOV.001 →



**Validation Method**:




```
# Opt into telemetry
ORPHEUS_TELEMETRY=true pnpm dev
# Verify events reaching dashboard
```




---




#### P4.GOV.005: Create Incident Response Plan



**Description**: Document procedures for handling production issues.



**Acceptance Criteria**:



- Document created: `docs/INCIDENT_RESPONSE.md`
- Escalation path defined
- Hotfix release process documented
- Rollback procedures defined
- Communication templates (user notification)



**ORP References**: ORP061 §Phase 4 (Monitor and Rollback Plan)



**Tooling Requirements**: None



**Dependencies**: P4.GOV.001 →



**Validation Method**:



- Simulate incident and follow procedures




---




### C. Feedback Integration Tasks




#### P4.GOV.006: Conduct Beta Testing Period



**Description**: Gather feedback from early adopters over 2-4 weeks.



**Acceptance Criteria**:



- Minimum 10 external beta testers recruited
- Feedback survey distributed
- Issues triaged and prioritized
- Critical bugs fixed in patch releases
- Feature requests documented for future



**ORP References**: ORP061 §Phase 4 (Beta Release & Testing)



**Tooling Requirements**:



- Survey tool (Google Forms, Typeform)



**Dependencies**: P4.GOV.003 →



**Validation Method**:



- Feedback collected and analyzed




---




#### P4.GOV.007: Publish Stable v1.0.0 Release



**Description**: Promote beta to stable release after successful testing period.



**Acceptance Criteria**:



- No critical bugs remaining
- Feedback incorporated or roadmapped
- Version bumped to 1.0.0 (remove beta tag)
- NPM `latest` tag updated
- Release announcement published



**ORP References**: ORP061 §Phase 4 (Full Transition)



**Tooling Requirements**:



- NPM access



**Dependencies**: P4.GOV.006 →



**Validation Method**:




```
npm info @orpheus/ui
# Should show 1.0.0 as latest
```




---




### D. Documentation Updates




#### P4.DOC.001: Create Upgrade Path Documentation



**Description**: Guide for beta users upgrading to stable.



**Acceptance Criteria**:



- Document: `docs/UPGRADING_TO_1.0.md`
- Beta to stable migration steps
- Breaking changes between beta and stable (if any)
- Deprecation notices
- Recommended upgrade timeline



**ORP References**: ORP063 §III.F



**Tooling Requirements**: None



**Dependencies**: P4.GOV.007 →



**Validation Method**:



- Beta user successfully upgrades using guide




---




#### P4.DOC.002: Publish Release Blog Post



**Description**: Announcement article summarizing integration and benefits.



**Acceptance Criteria**:



- Blog post published on website/Medium
- Highlights unified architecture
- Showcases new features (Orpheus + UI)
- Links to documentation
- Includes screenshots/demo video



**ORP References**: ORP061 §Phase 4 (Full Transition)



**Tooling Requirements**: None



**Dependencies**: P4.GOV.007 →



**Validation Method**:



- Post shared on social media, community channels




---




### E. Finalization Tasks




#### P4.GOV.008: Remove Feature Toggles



**Description**: Clean up temporary feature flags after stable release.



**Acceptance Criteria**:



- `ENABLE_ORPHEUS_FEATURES` flag removed
- All Orpheus features enabled by default
- Fallback paths removed (or made minimal)
- Code cleanup (remove toggle conditionals)
- Released as v1.1.0 (minor bump)



**ORP References**: ORP061 §Phase 4 (Full Transition)



**Tooling Requirements**: None



**Dependencies**: P4.GOV.007 → (wait for stable adoption)



**Validation Method**:



- Code review confirms no toggle remnants




---




#### P4.GOV.009: Conduct Retrospective



**Description**: Team retrospective on migration process.



**Acceptance Criteria**:



- Retrospective meeting held
- Lessons learned documented
- Process improvements identified
- Success metrics reviewed
- Recommendations for future migrations



**ORP References**: ORP063 (continuous improvement)



**Tooling Requirements**: None



**Dependencies**: P4.GOV.008 →



**Validation Method**:



- Document: `docs/RETROSPECTIVE_ORP061.md` created




---




### F. Validation Checkpoint




#### P4.TEST.001: Execute Phase 4 Validation Checklist



**Description**: Run complete validation suite from ORP061 §Phase 4.



**Acceptance Criteria**:



- Beta testing feedback collected
- Old repos archived
- Release process tested
- Monitoring in place
- All Phase 4 tasks complete



**ORP References**: ORP061 §Phase 4 Validation



**Tooling Requirements**: All Phase 4 tooling



**Dependencies**: ALL P4.* tasks →



**Validation Method**:




```
./scripts/validate-phase4.sh
# Final validation report
```




---




## VII. FINISH-LINE READINESS CHECKLIST



This section provides a concise, gated checklist summarizing the conditions for successful integration. Each criterion must be met before declaring the migration complete.




### A. Technical Completeness



- **TC-01**: All 127 tasks from Phases 0-4 marked complete
- **TC-02**: All three drivers (Service, WASM, Native) functional on target platforms
- **TC-03**: Contract v1.0.0 implemented and validated across all drivers
- **TC-04**: Golden audio tests pass with deterministic output
- **TC-05**: Contract compliance test matrix achieves ≥95% coverage
- **TC-06**: Unified CI pipeline passes on all platforms (Windows, macOS, Linux)




### B. Quality Assurance



- **QA-01**: Unit test coverage ≥80% overall, ≥90% for contract APIs
- **QA-02**: E2E tests cover critical user workflows
- **QA-03**: No memory leaks detected in 1000-iteration stress tests
- **QA-04**: Performance budgets met: UI ≤1.5MB, WASM ≤5MB, latency within SLA
- **QA-05**: Chaos tests validate recovery within defined timeframes
- **QA-06**: Cross-browser compatibility verified (Chrome, Firefox, Safari)




### C. Release Infrastructure



- **RI-01**: Changesets configured for all packages
- **RI-02**: NPM publish workflow functional and tested
- **RI-03**: Prebuilt native binaries available for macOS (x64/arm64), Windows (x64), Linux (x64)
- **RI-04**: Binary verification (SHA-256 checksum) implemented and tested
- **RI-05**: GitHub Releases created with changelogs
- **RI-06**: Version v1.0.0 published to NPM `latest` tag




### D. Documentation Completeness



- **DC-01**: All ORP documents (061-064) published and cross-referenced
- **DC-02**: Migration guide available with code examples and codemods
- **DC-03**: API documentation generated and hosted
- **DC-04**: Contributor guide updated for monorepo structure
- **DC-05**: Main README reflects unified architecture
- **DC-06**: Release blog post published




### E. Operational Readiness



- **OR-01**: Support channels established (GitHub Discussions, issue templates)
- **OR-02**: Monitoring and telemetry configured (opt-in)
- **OR-03**: Incident response plan documented
- **OR-04**: Beta testing period completed with feedback incorporated
- **OR-05**: Old repositories archived with migration notices
- **OR-06**: Feature toggles removed or deprecated




### F. Governance and Compliance



- **GC-01**: Security audit completed (no critical vulnerabilities)
- **GC-02**: Licensing verified (MIT for all components, attributions preserved)
- **GC-03**: Dependency audit passed (no known vulnerabilities)
- **GC-04**: Binary signing implemented (macOS notarized, Windows signed if applicable)
- **GC-05**: Privacy policy documented for telemetry
- **GC-06**: Stakeholder sign-off obtained from technical steering




### G. Success Metrics



- **SM-01**: Build times acceptable (<25 minutes for full CI)
- **SM-02**: Developer onboarding time <1 hour (fresh clone to running dev server)
- **SM-03**: Package download success rate >95% (prebuilt binaries)
- **SM-04**: User-reported critical bugs in first month <5
- **SM-05**: Community engagement metrics positive (GitHub stars, issues resolved)
- **SM-06**: Retrospective completed with lessons learned documented




---




## VIII. DEPENDENCY GRAPH SUMMARY




### Critical Path (Serial Dependencies)




```
P0.REPO.001 → P0.REPO.002 → P0.REPO.003 → P0.REPO.006 →
P1.CONT.001 → P1.CONT.003 → P1.DRIV.001 → P1.DRIV.002 → P1.DRIV.007 →
P2.CONT.001 → P2.DRIV.001 → P2.DRIV.003 → P2.DRIV.004 →
P2.TEST.001 → P3.CI.001 → P3.TEST.004 →
P4.GOV.001 → P4.GOV.006 → P4.GOV.007
```



**Critical Path Duration**: ~12-16 weeks (assuming sequential execution)




### Parallelizable Workstreams



**Phase 0**:



- REPO tasks (001-006) ‖ DOC tasks (001-002) ‖ CI tasks (001-002)



**Phase 1**:



- CONT tasks (001-003) ‖ DOC tasks (001-002)
- After DRIV.002: DRIV.004-006 (Native) ‖ DRIV.001-003 (Service) ‖ UI.001-002



**Phase 2**:



- DRIV.001-004 (WASM) ‖ UI.001-005 ‖ TEST.001-003 ‖ DOC.001-002



**Phase 3**:



- CI.001-003 ‖ REPO.001-003 ‖ GOV.001-004 ‖ TEST.001-003 ‖ DOC.001-004



**Phase 4**:



- GOV.001-003 ‖ DOC.001-002 (after initial release)




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




### CI/CD Tools



- **GitHub Actions**: CI/CD platform
- **Changesets**: Version management
- **TypeDoc**: API documentation generation
- **Storybook**: ≥7.0 (component documentation)




### Optional Tools



- **Plausible/Posthog**: Telemetry (opt-in)
- **Bundle analyzer**: Performance profiling
- **Madge**: Dependency cycle detection
- **clang-format**, **clang-tidy**: C++ formatting/linting




---




## X. ESTIMATED TIMELINE



 Phase 

 Duration 

 Cumulative 

 Phase 0: Repository Setup 

 1-2 weeks 

 2 weeks 

 Phase 1: Basic Integration 

 3-4 weeks 

 6 weeks 

 Phase 2: Feature Integration 

 4-6 weeks 

 12 weeks 

 Phase 3: Unified System 

 3-4 weeks 

 16 weeks 

 Phase 4: Rollout & Support 

 2-4 weeks 

 20 weeks 



**Total Estimated Duration**: 12-20 weeks (3-5 months)



**Assumptions**:



- 2-3 full-time engineers assigned
- Parallelizable tasks executed concurrently
- Minimal unexpected blockers or scope creep
- Stakeholder approvals obtained within 1 week of request




---




## XI. RISK MITIGATION STRATEGIES




### High-Risk Tasks



1. **P2.DRIV.001-003 (WASM Driver)**: Complex Emscripten build

- - **Mitigation**: Allocate experienced WebAssembly engineer, allow 2-week buffer

1. **P3.CI.001 (CI Unification)**: Potential merge conflicts

- - **Mitigation**: Implement incrementally, maintain parallel workflows during transition

1. **P3.GOV.003 (Prebuilt Binaries)**: Platform-specific build issues

- - **Mitigation**: Test on clean VMs, document environment requirements exhaustively

1. **P4.GOV.006 (Beta Testing)**: Low participation or critical bugs discovered

- - **Mitigation**: Recruit beta testers early, incentivize participation, allocate buffer for fixes




### Contingency Plans



- **Delay in WASM Driver**: Proceed with Service + Native only, defer WASM to v1.1.0
- **CI Budget Exceeds**: Implement selective job execution (skip redundant OS builds for docs changes)
- **Critical Bug in Beta**: Fast-track hotfix, delay stable release by 1-2 weeks
- **Binary Signing Failures**: Release without signatures initially, add post-v1.0.0




---




## XII. SUCCESS CRITERIA VALIDATION



Upon completion of all phases, the following must be demonstrable:



1. **Functional Demonstration**: Load session → Add track → Render click → Play audio in UI
2. **Multi-Driver Validation**: Repeat workflow using Service, WASM, and Native drivers
3. **Cross-Platform Validation**: Workflow succeeds on Windows, macOS, Linux
4. **Performance Validation**: Click track renders in <1s, UI remains responsive
5. **Documentation Validation**: New contributor onboards and submits PR within 2 hours
6. **Release Validation**: Fresh npm install of `@orpheus/ui` succeeds without compilation
7. **Community Validation**: Minimum 3 external projects adopt v1.0.0 within first month




---



**Document Status**: Normative implementation specification

**Effective Date**: Upon technical steering approval

**Review Cycle**: Weekly during implementation phases, monthly post-release

**Owner**: Orpheus SDK Technical Steering Committee
