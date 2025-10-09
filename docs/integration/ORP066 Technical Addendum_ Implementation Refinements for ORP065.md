# ORP066 Technical Addendum: Implementation Refinements for ORP065

**Document Type**: Technical Addendum
**Status**: Normative Amendment to ORP065
**Version**: 1.0
**Dependencies**: ORP065 (Implementation Task Breakdown)

***

## I. EXECUTIVE SUMMARY

This addendum addresses ten critical refinements to ORP065 identified during technical review. Changes include package naming hygiene, script implementations, manifest-driven contract management, explicit driver selection order, security hardening, and documentation improvements. All modifications integrate seamlessly with existing task numbering (TASK-001 through TASK-095) and introduce supplementary tasks (TASK-096 through TASK-105) and artifact specifications.

***

## II. PACKAGE NAMING CORRECTIONS

### A. Naming Hygiene Directive

**Issue**: Inconsistent package naming for Shmui and Native driver creates confusion.

**Resolution**: Establish canonical package names:

- `@orpheus/shmui` — Preserve original Shmui identity (not renamed to `@orpheus/ui`)
- `@orpheus/engine-native` — Native addon usable by Electron or plain Node (not `engine-electron`)

### B. Affected Tasks

**TASK-003: Configure Package Namespacing** (AMENDED)

Replace acceptance criteria with:

**Acceptance Criteria**:

- [ ] `packages/shmui/package.json` name set to `@orpheus/shmui`
- [ ] Native driver package named `@orpheus/engine-native`
- [ ] Scoped package naming documented in `docs/PACKAGE_NAMING.md`
- [ ] Reserved names documented: `@orpheus/core`, `@orpheus/client`, `@orpheus/contract`, `@orpheus/engine-*`
- [ ] NPM scope `@orpheus` registered (if publishing publicly)
- [ ] Internal codename "Shmui" preserved in documentation with explanation

**TASK-020: Native Driver Package** (AMENDED)

Replace description and acceptance criteria:

**Description**: Initialize `@orpheus/engine-native` with N-API binding structure (usable by Electron and Node.js).

**Acceptance Criteria**:

- [ ] Package created at `packages/engine-native/`
- [ ] CMakeLists.txt configured for N-API addon
- [ ] Minimal binding exports `getVersion()` function
- [ ] Builds on Ubuntu (proof of concept)
- [ ] `package.json` configured with install script
- [ ] Documentation clarifies Node.js and Electron compatibility

***

## III. SCRIPT IMPLEMENTATIONS

### C. Bootstrap and Validation Scripts

**Issue**: Referenced scripts (`bootstrap-dev.sh`, `validate-phaseX.sh`) do not exist, causing 404s in CI.

**Resolution**: Implement minimal working versions that orchestrate PNPM scripts.

### TASK-096: Implement Validation Script Suite (NEW)

**Description**: Create phase validation scripts that aggregate test, lint, and build commands.

**Acceptance Criteria**:

- [ ] Scripts created: `scripts/validate-phase0.sh` through `scripts/validate-phase4.sh`
- [ ] Each script calls appropriate PNPM workspace commands
- [ ] Exit code 0 only if all checks pass
- [ ] Output formatted with clear pass/fail indicators
- [ ] Scripts executable on Linux, macOS, Windows (via Git Bash/WSL)

**ORP References**: ORP065 §II-VI (all phase validations)

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

***

**TASK-007: Create Bootstrap Script** (AMENDED)

Add to acceptance criteria:

**Additional Acceptance Criteria**:

- [ ] Script includes dependency version checks (Node, PNPM, CMake, compiler)
- [ ] Installs Git hooks (Husky) automatically
- [ ] Creates `.env.example` with documented configuration options
- [ ] Prints troubleshooting guide URL on failure

**Implementation Enhancement** (`scripts/bootstrap-dev.sh`):

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

***

## IV. CONTRACT MANIFEST SYSTEM

### D. Single Source of Truth for Contract Versions

**Issue**: Contract versioning relies on folder naming conventions, risking drift and ambiguity.

**Resolution**: Implement `MANIFEST.json` as authoritative contract version registry.

### TASK-097: Implement Contract Manifest System (NEW)

**Description**: Create manifest-driven contract versioning with checksum validation.

**Acceptance Criteria**:

- [ ] File created: `packages/contract/MANIFEST.json`
- [ ] Manifest structure:```javascript
{  "currentVersion": "1.0.0",  "currentPath": "schemas/v1.0.0",  "checksum": "sha256:abc123...",  "availableVersions": [    {      "version": "0.9.0",      "path": "schemas/v0.9.0",      "checksum": "sha256:def456...",      "status": "deprecated"    },    {      "version": "1.0.0-beta",      "path": "schemas/v1.0.0-beta",      "checksum": "sha256:789ghi...",      "status": "superseded"    },    {      "version": "1.0.0",      "path": "schemas/v1.0.0",      "checksum": "sha256:abc123...",      "status": "stable"    }  ]}

```
- [ ] Contract diff tool (`scripts/contract-diff.ts`) reads from manifest
- [ ] CI validates manifest checksum matches actual schema files
- [ ] `@orpheus/contract` package exports manifest at runtime

**ORP References**: ORP065 TASK-016 (Contract Schema Automation), ORP063 §III.E

**Tooling Requirements**: JSON schema validation

**Dependencies**: TASK-016 →

**Validation Method**:

```javascript
import { MANIFEST } from '@orpheus/contract';
console.log(MANIFEST.currentVersion); // "1.0.0"

```

***

**TASK-016: Contract Schema Automation** (AMENDED)

Add to acceptance criteria:

**Additional Acceptance Criteria**:

- [ ] Diff tool reads `MANIFEST.json` for version information
- [ ] Tool validates checksum before performing diff
- [ ] Manifest automatically updated when new schema version added
- [ ] CI fails if manifest checksum doesn't match computed value

***

## V. EXPLICIT DRIVER SELECTION ORDER

### E. Deterministic Driver Priority

**Issue**: Driver selection order implied but not formally specified in code.

**Resolution**: Declare canonical order in `@orpheus/client` with environment override.

### TASK-098: Implement Driver Selection Registry (NEW)

**Description**: Create deterministic driver selection with environment variable override.

**Acceptance Criteria**:

- [ ] File created: `packages/client/src/driver-registry.ts`
- [ ] Default selection order: `native → wasm → service`
- [ ] Environment variable `ORPHEUS_DRIVER` accepts: `native`, `wasm`, `service`
- [ ] Invalid value logs warning and falls back to auto-selection
- [ ] Selection logic exported and testable
- [ ] Documentation in `docs/DRIVER_ARCHITECTURE.md` updated

**ORP References**: ORP062 §3.1, ORP063 §II.B

**Tooling Requirements**: None

**Dependencies**: TASK-024 →

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

***

**TASK-024: Create Client Broker** (AMENDED)

Add to acceptance criteria:

**Additional Acceptance Criteria**:

- [ ] Broker uses `driver-registry.ts` for selection logic
- [ ] Selection order documented in code comments
- [ ] Test suite validates all selection paths (auto, override, fallback)

***

## VI. SERVICE DRIVER SECURITY

### F. Authentication and Bind Safety

**Issue**: Service driver lacks authentication mechanism for production deployments.

**Resolution**: Implement token-based auth with secure defaults.

### TASK-099: Implement Service Driver Authentication (NEW)

**Description**: Add optional token-based authentication to service driver.

**Acceptance Criteria**:

- [ ] Command-line flag: `--auth none|token` (default: `token` in production builds)
- [ ] Token generated on startup if `--auth token` (printed to console)
- [ ] Client passes token via `Authorization: Bearer <token>` header
- [ ] Requests without valid token rejected with 401
- [ ] Token stored in `~/.orpheus/service.token` for persistence
- [ ] `--auth none` only allowed if `NODE_ENV=development`
- [ ] Documentation in `docs/DRIVER_ARCHITECTURE.md` updated

**ORP References**: ORP062 §2.1, §6.1 (Security)

**Tooling Requirements**: None

**Dependencies**: TASK-017 →

**Validation Method**:

```javascript
# Production mode (requires token)
orpheusd --auth token
# Prints: "Service started. Token: abc123..."

# Development mode (no auth)
NODE_ENV=development orpheusd --auth none

```

***

**TASK-017: Service Driver Foundation** (AMENDED)

Add to acceptance criteria:

**Additional Acceptance Criteria**:

- [ ] Bind address defaults to `127.0.0.1` (configurable via `--host`)
- [ ] `--host 0.0.0.0` requires explicit flag and logs security warning
- [ ] Authentication system integrated per TASK-099

***

## VII. WASM LOADING DISCIPLINE

### G. Deterministic WASM Build and Deployment

**Issue**: WASM builds lack version pinning and secure loading mechanisms.

**Resolution**: Pin Emscripten version, enforce SRI, implement secure loading.

### TASK-100: Implement WASM Build Discipline (NEW)

**Description**: Establish reproducible WASM builds with security guarantees.

**Acceptance Criteria**:

- [ ] File created: `.emscripten-version` containing exact version (e.g., `3.1.45`)
- [ ] Build script asserts Emscripten version matches before compilation
- [ ] WASM loaded via dynamic import with SRI verification
- [ ] MIME type check: server must serve as `application/wasm`
- [ ] Worker and WASM files hosted side-by-side (no cross-origin issues)
- [ ] Build script generates `integrity.json` with SRI hashes
- [ ] Documentation in `docs/DRIVER_ARCHITECTURE.md` updated

**ORP References**: ORP062 §2.2 (WASM Security)

**Tooling Requirements**: Emscripten SDK

**Dependencies**: TASK-033 →

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

***

**TASK-033: Initialize WASM Build Infrastructure** (AMENDED)

Add to acceptance criteria:

**Additional Acceptance Criteria**:

- [ ] `.emscripten-version` file created and committed
- [ ] Build script enforces version check
- [ ] SRI integrity file generated automatically

***

## VIII. BINARY DELIVERY REALITY

### H. Platform-Specific Signing Requirements

**Issue**: Signing requirements lack specific acceptance criteria for user experience.

**Resolution**: Add Gatekeeper/SmartScreen validation to signing tasks.

### TASK-101: Validate Binary Signing User Experience (NEW)

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

**ORP References**: ORP062 §5.1, ORP063 §V.M

**Tooling Requirements**:

- macOS: Xcode command-line tools
- Windows: Windows SDK (signtool)
- Clean test VMs

**Dependencies**: TASK-067 →

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

***

**TASK-067: Generate Prebuilt Native Binaries** (AMENDED)

Add to acceptance criteria:

**Additional Acceptance Criteria**:

- [ ] macOS binaries notarized with hardened runtime enabled
- [ ] Windows binaries signed with Authenticode certificate
- [ ] Signing validated per TASK-101 requirements
- [ ] Signature verification passes on fresh OS installs

***

## IX. GOLDEN AUDIO CANONICALIZATION

### I. Reference Audio Manifest

**Issue**: Golden audio tests lack container metadata specification, risking false mismatches.

**Resolution**: Formalize WAV container requirements and precompute hashes in manifest.

### TASK-102: Create Golden Audio Manifest (NEW)

**Description**: Canonicalize reference audio format and precompute validation hashes.

**Acceptance Criteria**:

- [ ] File created: `packages/contract/fixtures/golden-audio/MANIFEST.json`
- [ ] Manifest structure:```javascript
{  "format": {    "container": "WAV",    "encoding": "PCM",    "bitDepth": 16,    "sampleRate": 44100,    "channels": 1,    "endianness": "little",    "dither": "none",    "metadata": "stripped"  },  "references": [    {      "name": "click_100bpm_2bar",      "file": "click_100bpm_2bar.wav",      "sha256": "f23c...",      "parameters": { "bpm": 100, "bars": 2 }    },    {      "name": "click_120bpm_4bar",      "file": "click_120bpm_4bar.wav",      "sha256": "9a4e...",      "parameters": { "bpm": 120, "bars": 4 }    }  ]}

```
- [ ] Test suite reads hashes from manifest (not hardcoded)
- [ ] CI validates manifest hashes match actual files on every build
- [ ] Documentation in `docs/CONTRACT_DEVELOPMENT.md` explains regeneration process

**ORP References**: ORP062 §4.1, ORP063 §IV.G

**Tooling Requirements**: Audio processing library (sox, ffmpeg)

**Dependencies**: TASK-044 →

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

***

**TASK-044: Golden Audio Test Suite** (AMENDED)

Add to acceptance criteria:

**Additional Acceptance Criteria**:

- [ ] Test suite reads expected hashes from `MANIFEST.json`
- [ ] WAV container format validated against manifest specification
- [ ] Test fails with clear error if container format doesn't match (e.g., wrong endianness)
- [ ] Regeneration script provided: `scripts/regenerate-golden-audio.sh`

***

## X. PERFORMANCE BUDGETS IN REPOSITORY

### J. Centralized Budget Configuration

**Issue**: Performance budgets mentioned in text but not committed as artifact.

**Resolution**: Create `budgets.json` in repository for version-controlled thresholds.

### TASK-103: Create Performance Budget Configuration (NEW)

**Description**: Establish version-controlled performance budget file.

**Acceptance Criteria**:

- [ ] File created: `budgets.json` at repository root
- [ ] Structure matches ORP062 §5.3 specifications:```javascript
{  "version": "1.0",  "budgets": {    "bundleSize": {      "@orpheus/shmui": {        "max": 1572864,        "warn": 1468006,        "description": "UI bundle (1.5 MB max, warn at 1.4 MB)"      },      "@orpheus/engine-wasm": {        "max": 5242880,        "warn": 4718592,        "description": "WASM module (5 MB max, warn at 4.5 MB)"      }    },    "commandLatency": {      "LoadSession": {        "p95": 200,        "p99": 500,        "unit": "ms"      },      "RenderClick": {        "p95": 1000,        "p99": 2000,        "unit": "ms"      }    },    "eventFrequency": {      "TransportTick": {        "max": 30,        "unit": "Hz"      },      "RenderProgress": {        "max": 10,        "unit": "Hz"      }    },    "degradationTolerance": {      "latency": "10%",      "size": "15%"    }  }}

```
- [ ] CI imports and validates against this file (TASK-056)
- [ ] Changes to budgets require PR with justification
- [ ] Documentation in `docs/PERFORMANCE.md` references this file

**ORP References**: ORP062 §5.3, ORP063 §IV.I

**Tooling Requirements**: JSON schema

**Dependencies**: None (can be created early)

**Validation Method**:

```javascript
# Validate structure
cat budgets.json | jq '.budgets.bundleSize."@orpheus/shmui".max'
# Should output: 1572864

```

***

**TASK-056: Performance Budget Enforcement** (AMENDED)

Replace acceptance criteria with:

**Acceptance Criteria**:

- [ ] CI step: `pnpm perf:validate --budgets=budgets.json`
- [ ] Script reads thresholds from `budgets.json`
- [ ] Bundle size check validates against configured limits
- [ ] Latency check validates against configured limits
- [ ] Violations fail CI with detailed report showing actual vs. budget
- [ ] Degradation tolerance applied per `budgets.json` configuration

***

## XI. DOCUMENTATION "START HERE" MAP

### K. Onboarding Optimization

**Issue**: Documentation lacks clear entry point for new users.

**Resolution**: Add prominent "Start Here" navigation block.

### TASK-104: Implement Documentation Quick Start Block (NEW)

**Description**: Add clear entry point to documentation index.

**Acceptance Criteria**:

- [ ] Block added to top of `docs/INDEX.md`:```javascript
## 🚀 Start Here**New to Orpheus SDK?** Begin with these three guides:1. **[Getting Started](GETTING_STARTED.md)** — Install, build, and run your first session2. **[Driver Architecture](DRIVER_ARCHITECTURE.md)** — Understand Service, WASM, and Native drivers3. **[Contract Guide](CONTRACT_DEVELOPMENT.md)** — Learn the command/event schema system**For specific tasks:**- Adding features → [Contributor Guide](../CONTRIBUTING.md)- Migrating projects → [Migration Guide](MIGRATION_GUIDE.md)- API reference → [API Surface Index](API_SURFACE_INDEX.md)

```
- [ ] Links verified functional (no 404s)
- [ ] Block positioned above existing ORP document listings
- [ ] Emoji/icons used for visual hierarchy

**ORP References**: ORP063 §V.N

**Tooling Requirements**: None

**Dependencies**: TASK-010 →

**Validation Method**:

- Manual review: first-time user can find entry point within 30 seconds

***

**TASK-010: Create Documentation Index** (AMENDED)

Add to acceptance criteria:

**Additional Acceptance Criteria**:

- [ ] "Start Here" block positioned prominently at top
- [ ] Three primary entry points highlighted
- [ ] Secondary navigation organized by task type

***

## XII. SUPPLEMENTARY ARTIFACT SPECIFICATIONS

### L. New Repository Artifacts

The following files must be created as part of the amendments above:

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
6. `**scripts/validate-phase0.sh**` **through `scripts/validate-phase4.sh`**
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

***

## XIII. UPDATED TASK SUMMARY

### M. Revised Task Inventory

**Original ORP065 Tasks**: TASK-001 through TASK-095 (95 tasks)
**New Tasks (ORP066)**: TASK-096 through TASK-104 (9 tasks)
**Amended Tasks**: TASK-003, TASK-007, TASK-016, TASK-017, TASK-020, TASK-024, TASK-033, TASK-044, TASK-056, TASK-067 (10 tasks amended)

**Total Implementation Tasks**: **104 tasks**

| Task ID | Description | Phase | Domain | Dependencies |
| --- | --- | --- | --- | --- |
| TASK-096 | Implement Validation Script Suite | P0+ | TEST | Phase-dependent |
| TASK-097 | Implement Contract Manifest System | P1 | CONT | 016 |
| TASK-098 | Implement Driver Selection Registry | P1 | DRIV | 024 |
| TASK-099 | Implement Service Driver Authentication | P1 | GOV | 017 |
| TASK-100 | Implement WASM Build Discipline | P2 | DRIV | 033 |
| TASK-101 | Validate Binary Signing UX | P3 | GOV | 067 |
| TASK-102 | Create Golden Audio Manifest | P2 | TEST | 044 |
| TASK-103 | Create Performance Budget Config | P0 | GOV | None |
| TASK-104 | Implement Docs Quick Start Block | P0 | DOC | 010 |

***

## XIV. INTEGRATION INSTRUCTIONS

### N. Merging ORP066 into ORP065

To integrate these amendments:

1. **Replace Task Descriptions**: For TASK-003, 007, 016, 017, 020, 024, 033, 044, 056, 067 — replace acceptance criteria sections with amended versions above.
2. **Append New Tasks**: Add TASK-096 through TASK-104 to the end of Phase sections:
    - TASK-096, 103, 104 → Phase 0
    - TASK-097, 098, 099 → Phase 1
    - TASK-100, 102 → Phase 2
    - TASK-101 → Phase 3
3. **Update Summary Tables**:
    - Change total task count from 95 to 104
    - Add new tasks to task numbering reference table (§XIV)
    - Update phase distribution counts
4. **Create Artifacts**: Generate the 8 new repository files listed in §XII.L
5. **Update Dependencies**: Adjust dependency graphs to include new task dependencies.

***

**END OF ADDENDUM**

*ORP066 Technical Addendum is normative and supersedes conflicting specifications in ORP065. All amendments must be integrated before Phase 0 completion.*