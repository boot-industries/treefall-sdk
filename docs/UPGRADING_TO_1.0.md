# Upgrading to Orpheus SDK 1.0

**Document Type:** Migration Guide
**Status:** Normative
**Last Updated:** 2025-10-26
**Target Audience:** Beta users, early adopters, integrators

---

## Executive Summary

This document provides a comprehensive guide for upgrading from Orpheus SDK alpha/beta versions to the stable v1.0.0 release. It covers breaking changes, deprecations, migration strategies, and recommended timelines.

**Key Points:**

- Contract v1.0.0-beta → v1.0.0 stable (minimal breaking changes)
- Driver architecture stabilized (Service, Native, WASM)
- Performance budgets enforced (bundle size, latency SLAs)
- Security hardening (authentication, SBOM, vulnerability scanning)
- Upgrade window: 4-8 weeks recommended

**Before You Begin:**

- Review the [CHANGELOG](../CHANGELOG.md) for detailed release notes
- Back up your existing integration code
- Test in a development environment first
- Plan for ~2-4 hours of migration work for typical integrations

---

## Table of Contents

1. [Version Timeline](#version-timeline)
2. [Breaking Changes](#breaking-changes)
3. [Contract Migration](#contract-migration)
4. [Driver Changes](#driver-changes)
5. [API Changes](#api-changes)
6. [Build & Tooling Changes](#build--tooling-changes)
7. [Migration Steps](#migration-steps)
8. [Testing Your Migration](#testing-your-migration)
9. [Rollback Procedures](#rollback-procedures)
10. [Support & Resources](#support--resources)

---

## Version Timeline

### Development Phases

| Version      | Status              | Release Date | End of Support |
| ------------ | ------------------- | ------------ | -------------- |
| v0.1.0-alpha | **Superseded**      | 2025-10-11   | 2025-11-30     |
| v1.0.0-beta  | **Current Beta**    | 2025-10-13   | 2026-01-31     |
| **v1.0.0**   | **Stable (Target)** | 2025-11-15   | TBD (LTS)      |
| v1.1.0       | Planned             | 2026-Q1      | TBD            |

### Upgrade Paths

```
v0.1.0-alpha ──────┐
                   │
v1.0.0-beta ───────┴──> v1.0.0 (stable)
                         │
                         ├──> v1.1.0 (feature updates)
                         └──> v1.x.x (maintenance)
```

**Recommended Upgrade Timeline:**

- **Week 1-2:** Review breaking changes, test in dev environment
- **Week 3-4:** Update code, run migration tests
- **Week 5-6:** Staging deployment, load testing
- **Week 7-8:** Production rollout (phased)

---

## Breaking Changes

### 1. Contract Package Naming

**Changed:** Package name updated with proper `@orpheus` scope

**Before (v0.1.0-alpha):**

```json
{
  "dependencies": {
    "orpheus-contract": "^0.1.0"
  }
}
```

**After (v1.0.0):**

```json
{
  "dependencies": {
    "@orpheus/contract": "^1.0.0"
  }
}
```

**Impact:** HIGH - All imports must be updated

**Migration:**

```bash
# Update package.json
pnpm remove orpheus-contract
pnpm add @orpheus/contract@^1.0.0

# Update imports in code
# Before:
import { LoadSessionCommand } from 'orpheus-contract';

# After:
import { LoadSessionCommand } from '@orpheus/contract';
```

---

### 2. Driver Selection Registry

**Changed:** Explicit driver priority order enforced

**Before (v0.1.0-alpha):**

```typescript
// Driver selection was implicit
const client = new OrpheusClient();
```

**After (v1.0.0):**

```typescript
// Explicit driver preference with fallback chain
import { OrpheusClient, DriverType } from '@orpheus/client';

const client = new OrpheusClient({
  preferredDrivers: [
    DriverType.Native, // Fastest (Node.js/Electron)
    DriverType.WASM, // Browser fallback
    DriverType.Service, // Network fallback
  ],
});
```

**Impact:** MEDIUM - Default behavior unchanged, but explicit configuration recommended

**Migration:**

- Add explicit `preferredDrivers` array to client initialization
- Remove any custom driver selection logic
- Rely on Client Broker's automatic selection with health checks

---

### 3. Event Frequency Validation

**Changed:** Client-side event frequency limits enforced to prevent UI overload

**Before (v0.1.0-alpha):**

```typescript
// No frequency limits
client.on('TransportTick', (event) => {
  // Called on every audio frame (potentially 1000+ Hz)
  updateUI(event.position);
});
```

**After (v1.0.0):**

```typescript
// Frequency limits enforced (see contract schema)
client.on('TransportTick', (event) => {
  // Maximum 30 Hz (contract validation)
  updateUI(event.position);
});
```

**Impact:** LOW - Event frequency limits already documented in v1.0.0-beta contract

**Contract Limits:**

- `TransportTick`: ≤30 Hz
- `RenderProgress`: ≤10 Hz
- `Heartbeat`: 1 Hz

**Migration:**

- Update UI code to handle lower event frequencies
- Use client-side interpolation for smooth animations
- Remove custom throttling logic (now handled by SDK)

**Example:**

```typescript
// Before: Custom throttling
let lastUpdate = 0;
client.on('TransportTick', (event) => {
  const now = Date.now();
  if (now - lastUpdate > 33) {
    // 30 Hz manual throttle
    updateUI(event.position);
    lastUpdate = now;
  }
});

// After: SDK handles throttling
client.on('TransportTick', (event) => {
  updateUI(event.position); // Already ≤30 Hz
});
```

---

### 4. Service Driver Authentication

**Changed:** Token-based authentication now required for Service Driver HTTP/WebSocket endpoints

**Before (v0.1.0-alpha):**

```typescript
// No authentication
const response = await fetch('http://localhost:8080/command', {
  method: 'POST',
  body: JSON.stringify(command),
});
```

**After (v1.0.0):**

```typescript
// Bearer token authentication
const response = await fetch('http://localhost:8080/command', {
  method: 'POST',
  headers: {
    Authorization: `Bearer ${process.env.ORPHEUS_AUTH_TOKEN}`,
  },
  body: JSON.stringify(command),
});
```

**Impact:** HIGH - Security enhancement, breaks direct HTTP access

**Migration:**

- Set `ORPHEUS_AUTH_TOKEN` environment variable when launching Service Driver
- Update client code to include `Authorization` header
- Use `@orpheus/client` package (handles auth automatically)

**Security Note:**

- Default token: `test-secret-token-123` (development only)
- Production: Generate strong random token, store securely
- Bind to `127.0.0.1` only (localhost-only access)

**Example:**

```bash
# Development
export ORPHEUS_AUTH_TOKEN="test-secret-token-123"
pnpm --filter @orpheus/engine-service start

# Production (generate random token)
export ORPHEUS_AUTH_TOKEN=$(openssl rand -base64 32)
```

---

### 5. WASM Build Discipline with SRI

**Changed:** Subresource Integrity (SRI) verification enforced for WASM artifacts

**Before (v0.1.0-alpha):**

```typescript
// Direct WASM loading
const module = await WebAssembly.instantiateStreaming(fetch('/orpheus.wasm'));
```

**After (v1.0.0):**

```typescript
// SRI verification required
import { loadWASMDriver } from '@orpheus/engine-wasm';

const driver = await loadWASMDriver({
  wasmPath: '/orpheus.wasm',
  integrityFile: '/integrity.json', // SHA-384 hashes
});
```

**Impact:** MEDIUM - Security enhancement, requires integrity.json artifact

**Migration:**

- Use `@orpheus/engine-wasm` loader (handles SRI automatically)
- Ensure `integrity.json` is deployed alongside WASM artifacts
- Update build pipeline to generate integrity hashes

**Build Script:**

```bash
# Build WASM with integrity generation
./scripts/build-wasm.sh

# Output:
# - orpheus.wasm
# - orpheus.wasm.js
# - integrity.json (SHA-384 hashes)
```

---

### 6. Performance Budgets Enforced

**Changed:** Bundle size and latency budgets enforced in CI

**Before (v0.1.0-alpha):**

- No bundle size limits

**After (v1.0.0):**

- UI bundle: ≤1.5 MB (with 15% tolerance = 1.725 MB)
- WASM artifact: ≤5 MB (with 15% tolerance = 5.75 MB)
- Heartbeat latency: ≤100ms (99th percentile)
- Command latency: ≤50ms (95th percentile)

**Impact:** LOW - Enforces existing best practices

**Migration:**

- Run `pnpm run perf:validate` to check current sizes
- Optimize bundle if over budget (code splitting, tree shaking)
- Use `pnpm run dep:graph` to visualize dependencies

**Validation:**

```bash
pnpm run perf:validate

# Output:
# ✓ @orpheus/shmui UI bundle: 1.2 MB (within 1.5 MB budget)
# ✓ @orpheus/engine-wasm: 4.8 MB (within 5 MB budget)
```

---

## Contract Migration

### Contract Version Matrix

| Feature              | v0.1.0-alpha | v1.0.0-beta | v1.0.0 (stable) |
| -------------------- | ------------ | ----------- | --------------- |
| LoadSession          | ✅           | ✅          | ✅              |
| RenderClick          | ✅           | ✅          | ✅              |
| SaveSession          | ❌           | ✅          | ✅              |
| TriggerClipGridScene | ❌           | ✅          | ✅              |
| SetTransport         | ❌           | ✅          | ✅              |
| TransportTick        | ❌           | ✅ (≤30 Hz) | ✅ (≤30 Hz)     |
| RenderProgress       | ❌           | ✅ (≤10 Hz) | ✅ (≤10 Hz)     |
| RenderDone           | ❌           | ✅          | ✅              |
| Error                | ❌           | ✅          | ✅              |

### Importing Contract Versions

**v1.0.0-beta (Current Beta):**

```typescript
import { LoadSessionCommand, SaveSessionCommand } from '@orpheus/contract/v1.0.0-beta';
```

**v1.0.0 (Stable - Recommended):**

```typescript
// Default export is stable version
import { LoadSessionCommand, SaveSessionCommand } from '@orpheus/contract';
```

### Schema Changes

#### SaveSession Command (NEW in v1.0.0-beta)

**Purpose:** Save current session state to disk

**Schema:**

```typescript
interface SaveSessionCommand {
  method: 'SaveSession';
  params: {
    outputPath: string; // Absolute path to save .json file
  };
}
```

**Response:**

```typescript
interface SaveSessionResponse {
  success: boolean;
  bytesWritten: number;
}
```

**Example:**

```typescript
const response = await client.execute({
  method: 'SaveSession',
  params: {
    outputPath: '/path/to/my_session.json',
  },
});

console.log(`Session saved: ${response.bytesWritten} bytes`);
```

#### TriggerClipGridScene Command (NEW in v1.0.0-beta)

**Purpose:** Trigger all clips in a named scene

**Schema:**

```typescript
interface TriggerClipGridSceneCommand {
  method: 'TriggerClipGridScene';
  params: {
    sceneName: string; // Scene identifier
    stopOthers?: boolean; // Stop clips not in scene (default: false)
  };
}
```

**Example:**

```typescript
await client.execute({
  method: 'TriggerClipGridScene',
  params: {
    sceneName: 'Intro Music',
    stopOthers: true,
  },
});
```

#### SetTransport Command (NEW in v1.0.0-beta)

**Purpose:** Control transport state (play, stop, seek)

**Schema:**

```typescript
interface SetTransportCommand {
  method: 'SetTransport';
  params: {
    action: 'play' | 'stop' | 'seek';
    position?: number; // Sample offset (for 'seek' action)
  };
}
```

**Example:**

```typescript
// Start playback
await client.execute({
  method: 'SetTransport',
  params: { action: 'play' },
});

// Seek to 10 seconds at 48kHz
await client.execute({
  method: 'SetTransport',
  params: {
    action: 'seek',
    position: 480000, // 10 * 48000
  },
});
```

#### TransportTick Event (NEW in v1.0.0-beta)

**Purpose:** Real-time transport position updates

**Schema:**

```typescript
interface TransportTickEvent {
  event: 'TransportTick';
  data: {
    sampleOffset: number; // Current playhead position (samples)
    beat: number; // Musical beat (0-based, fractional)
    tempo: number; // Current BPM
  };
}
```

**Frequency Limit:** ≤30 Hz (enforced by SDK)

**Example:**

```typescript
client.on('TransportTick', (event) => {
  console.log(`Position: ${event.data.sampleOffset} samples`);
  console.log(`Beat: ${event.data.beat.toFixed(2)}`);
  console.log(`Tempo: ${event.data.tempo} BPM`);
});
```

#### RenderProgress Event (NEW in v1.0.0-beta)

**Purpose:** Track offline render progress

**Schema:**

```typescript
interface RenderProgressEvent {
  event: 'RenderProgress';
  data: {
    samplesRendered: number;
    totalSamples: number;
    percentComplete: number; // 0.0 to 1.0
  };
}
```

**Frequency Limit:** ≤10 Hz (enforced by SDK)

**Example:**

```typescript
client.on('RenderProgress', (event) => {
  const percent = (event.data.percentComplete * 100).toFixed(1);
  console.log(`Render progress: ${percent}%`);
});
```

#### Error Event (NEW in v1.0.0-beta)

**Purpose:** Communicate SDK errors to client

**Schema:**

```typescript
interface ErrorEvent {
  event: 'Error';
  data: {
    code: number; // SessionGraphError code
    message: string; // Human-readable error
    context?: string; // Additional context (command, file path, etc.)
  };
}
```

**Example:**

```typescript
client.on('Error', (event) => {
  console.error(`SDK Error ${event.data.code}: ${event.data.message}`);
  if (event.data.context) {
    console.error(`Context: ${event.data.context}`);
  }
});
```

### Contract Manifest

**v1.0.0-beta Manifest Entry:**

```json
{
  "version": "1.0.0-beta",
  "status": "beta",
  "checksum": "024eb47a19c6ca74...",
  "supersedes": "0.1.0-alpha"
}
```

**Verification:**

```bash
pnpm --filter @orpheus/contract run validate:manifest

# Output:
# ✓ v1.0.0-beta: Checksum matches (024eb47a...)
# ✓ v0.1.0-alpha: Marked as superseded
```

---

## Driver Changes

### Driver Priority Order

**v1.0.0 Default Order:**

```typescript
[
  DriverType.Native, // Fastest (Node.js/Electron)
  DriverType.WASM, // Browser fallback
  DriverType.Service, // Network fallback
];
```

**Why This Order:**

- **Native:** Zero-copy memory access, direct C++ SDK integration, <1ms overhead
- **WASM:** Browser-compatible, sandboxed, 2-5ms overhead
- **Service:** Network latency (5-50ms), suitable for remote orchestration

**Customizing Driver Order:**

```typescript
const client = new OrpheusClient({
  preferredDrivers: [
    DriverType.Service, // Force Service Driver (e.g., for testing)
  ],
  serviceUrl: 'http://localhost:8080',
  authToken: process.env.ORPHEUS_AUTH_TOKEN,
});
```

### Native Driver Changes

**C++ SDK Integration:**

- Uses N-API (stable Node.js addon API)
- Direct `SessionGraph` access (no serialization overhead)
- Event callbacks via `SessionWrapper::subscribe()`

**Build Requirements:**

- CMake 3.22+
- C++20 compiler
- node-addon-api ^7.0.0

**Build Command:**

```bash
pnpm --filter @orpheus/engine-native rebuild
```

**Binary Output:**

- macOS: `orpheus_native.node` (~107 KB)
- Windows: `orpheus_native.node` (~150 KB)
- Linux: `orpheus_native.node` (~120 KB)

### WASM Driver Changes

**Security Enhancements:**

- Subresource Integrity (SRI) with SHA-384
- MIME type verification (`application/wasm`)
- Same-origin policy enforcement

**Build Discipline:**

- Locked Emscripten version: 3.1.45
- Version enforcement script (`scripts/build-wasm.sh`)
- Integrity manifest generation (`integrity.json`)

**Build Command:**

```bash
./scripts/build-wasm.sh

# Requires:
# - Emscripten SDK 3.1.45
# - CMake 3.22+
# - Python 3.8+ (for Emscripten)
```

**Output Artifacts:**

- `orpheus.wasm` (~4.8 MB, compressed)
- `orpheus.wasm.js` (glue code)
- `integrity.json` (SHA-384 hashes)

### Service Driver Changes

**Authentication:**

- Token-based authentication (Bearer tokens)
- Environment variable: `ORPHEUS_AUTH_TOKEN`
- Default bind: `127.0.0.1:8080` (localhost-only)

**Endpoints:**

- `GET /health` (public, no auth required)
- `GET /version` (public, no auth required)
- `GET /contract` (requires auth)
- `POST /command` (requires auth)
- `WebSocket /ws` (requires auth via query param: `?token=...`)

**Security Warnings:**

- Service Driver logs authentication events (success + failures)
- Bind to `127.0.0.1` only (never `0.0.0.0` in production)
- Rotate tokens regularly
- Use HTTPS/WSS for remote access (not supported by default)

**Starting Service Driver:**

```bash
# Development
export ORPHEUS_AUTH_TOKEN="test-secret-token-123"
pnpm --filter @orpheus/engine-service start

# Production (systemd service)
[Service]
Environment="ORPHEUS_AUTH_TOKEN=<generated-token>"
ExecStart=/usr/local/bin/orpheus-service
Restart=always
```

---

## API Changes

### C++ SDK API

No breaking changes to public C++ API in v1.0.0. All additions are backward-compatible.

**New APIs (v1.0.0):**

- `TransportController::updateClipGain(handle, gainDb)` - Per-clip volume control
- `TransportController::setClipLoopMode(handle, enabled)` - Loop toggle
- `TransportController::updateClipFades(...)` - Fade IN/OUT configuration

**Existing APIs (unchanged):**

- `TransportController::startClip(handle)` - Start playback
- `TransportController::stopClip(handle)` - Stop playback
- `TransportController::stopAllClips()` - Stop all active clips
- `TransportController::updateClipTrimPoints(handle, trimIn, trimOut)` - Trim points
- `TransportController::registerClipAudio(handle, filePath)` - Load audio file

**Example:**

```cpp
// v0.1.0-alpha code still works in v1.0.0
auto transport = std::make_unique<TransportController>(nullptr, 48000);
transport->registerClipAudio(1, "/path/to/clip.wav");
transport->startClip(1);

// v1.0.0 new features (optional)
transport->updateClipGain(1, -3.0);  // -3 dB
transport->setClipLoopMode(1, true); // Enable loop
```

### TypeScript Client API

**Breaking Change:** Driver initialization signature changed

**Before (v0.1.0-alpha):**

```typescript
import { OrpheusClient } from '@orpheus/client';

const client = new OrpheusClient(); // Implicit driver selection
await client.connect();
```

**After (v1.0.0):**

```typescript
import { OrpheusClient, DriverType } from '@orpheus/client';

const client = new OrpheusClient({
  preferredDrivers: [DriverType.Native, DriverType.WASM],
  healthCheckInterval: 5000, // Optional, default: 5000ms
});
await client.connect();
```

**Migration:**

- Add explicit `preferredDrivers` array
- Remove any custom driver selection logic
- Use `DriverType` enum (type-safe)

---

## Build & Tooling Changes

### CMake Changes

No breaking changes. All existing CMake configurations remain valid.

**New Options (v1.0.0):**

- `-DORP_ENABLE_UBSAN=ON/OFF` - Toggle UndefinedBehaviorSanitizer (default: ON for Debug, OFF for Release)
- `-DORPHEUS_ENABLE_APP_CLIP_COMPOSER=ON/OFF` - Build Orpheus Clip Composer application (default: OFF)

**Example:**

```bash
# Build with UBSan disabled (for Release builds with sanitizer linker issues)
cmake -S . -B build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DORP_ENABLE_UBSAN=OFF

cmake --build build-release
```

### PNPM Workspace Changes

**New Scripts:**

- `pnpm run perf:validate` - Check bundle sizes against budgets
- `pnpm run dep:check` - Detect circular dependencies
- `pnpm run dep:graph` - Generate dependency graph SVG

**Updated Scripts:**

- `pnpm run lint` - Now includes C++ formatting check (clang-format)
- `pnpm test` - Now includes frequency validator tests

**Usage:**

```bash
# Before committing
pnpm run lint        # Lint JS/TS + C++
pnpm test            # Run all tests
pnpm run perf:validate  # Check bundle sizes

# Debugging dependencies
pnpm run dep:check   # Find circular deps
pnpm run dep:graph   # Visualize deps (deps-graph.svg)
```

### Pre-Commit Hooks

**v1.0.0 enforces commit quality via Husky:**

**Installed Hooks:**

- `pre-commit` - Runs `lint-staged` (lints only changed files)
- `commit-msg` - Validates conventional commit format

**Linting Rules:**

- TypeScript/JavaScript: ESLint + Prettier
- C++: clang-format (LLVM style)
- JSON: Format check
- Markdown: Format check

**Conventional Commits Enforced:**

```
<type>(<scope>): <subject>

Examples:
feat(transport): add clip gain control API
fix(wasm): resolve SRI verification timeout
docs(upgrade): add migration guide for v1.0.0
test(coverage): add unit tests for loop mode
```

**Types:** `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`, `perf`, `ci`

**Bypassing Hooks (Not Recommended):**

```bash
# Skip pre-commit hook (for emergency commits only)
git commit --no-verify -m "emergency fix"
```

---

## Migration Steps

### Step 1: Update Dependencies

**Update package.json:**

```json
{
  "dependencies": {
    "@orpheus/client": "^1.0.0",
    "@orpheus/contract": "^1.0.0",
    "@orpheus/react": "^1.0.0"
  },
  "devDependencies": {
    "@orpheus/engine-native": "^1.0.0",
    "@orpheus/engine-wasm": "^1.0.0",
    "@orpheus/engine-service": "^1.0.0"
  }
}
```

**Install:**

```bash
pnpm install
```

### Step 2: Update Imports

**Find and replace:**

```bash
# Before:
import { LoadSessionCommand } from 'orpheus-contract';

# After:
import { LoadSessionCommand } from '@orpheus/contract';
```

**Automated fix:**

```bash
# Using sed (macOS)
find src -name "*.ts" -exec sed -i '' 's/orpheus-contract/@orpheus\/contract/g' {} +

# Using sed (Linux)
find src -name "*.ts" -exec sed -i 's/orpheus-contract/@orpheus\/contract/g' {} +
```

### Step 3: Update Client Initialization

**Before (v0.1.0-alpha):**

```typescript
import { OrpheusClient } from '@orpheus/client';

const client = new OrpheusClient();
await client.connect();
```

**After (v1.0.0):**

```typescript
import { OrpheusClient, DriverType } from '@orpheus/client';

const client = new OrpheusClient({
  preferredDrivers: [DriverType.Native, DriverType.WASM, DriverType.Service],
  serviceUrl: 'http://localhost:8080',
  authToken: process.env.ORPHEUS_AUTH_TOKEN,
});

await client.connect();
```

### Step 4: Update Event Handlers

**Remove custom throttling:**

```typescript
// Before (v0.1.0-alpha): Custom throttling
let lastUpdate = 0;
client.on('TransportTick', (event) => {
  const now = Date.now();
  if (now - lastUpdate > 33) {
    updateUI(event.position);
    lastUpdate = now;
  }
});

// After (v1.0.0): SDK throttles automatically
client.on('TransportTick', (event) => {
  updateUI(event.position); // Already ≤30 Hz
});
```

### Step 5: Add Service Driver Authentication

**Set environment variable:**

```bash
# Development
export ORPHEUS_AUTH_TOKEN="test-secret-token-123"

# Production (generate random token)
export ORPHEUS_AUTH_TOKEN=$(openssl rand -base64 32)
echo "ORPHEUS_AUTH_TOKEN=$ORPHEUS_AUTH_TOKEN" >> .env
```

**Update client code:**

```typescript
const client = new OrpheusClient({
  preferredDrivers: [DriverType.Service],
  serviceUrl: 'http://localhost:8080',
  authToken: process.env.ORPHEUS_AUTH_TOKEN, // Required for Service Driver
});
```

### Step 6: Update Build Scripts

**Add performance validation:**

```json
{
  "scripts": {
    "build": "vite build && pnpm run perf:validate",
    "perf:validate": "node scripts/validate-performance.js"
  }
}
```

**Add dependency checks:**

```json
{
  "scripts": {
    "lint": "eslint src && pnpm run dep:check",
    "dep:check": "madge --circular --extensions ts,tsx src"
  }
}
```

### Step 7: Test Migration

**Run validation suite:**

```bash
# Full SDK validation
./scripts/validate-sdk.sh

# Expected output:
# ✓ CMake build succeeded
# ✓ All tests passed (45/45)
# ✓ C++ formatting check passed
# ✓ JavaScript linting passed
# ✓ JavaScript tests passed
```

**Test your integration:**

```bash
# Start Service Driver (if using)
export ORPHEUS_AUTH_TOKEN="test-secret-token-123"
pnpm --filter @orpheus/engine-service start

# Run your application
pnpm dev

# Verify:
# - Client connects to driver successfully
# - Commands execute without errors
# - Events are received (check console)
# - UI updates correctly
```

### Step 8: Deploy to Staging

**Staging deployment checklist:**

- [ ] Update environment variables (`ORPHEUS_AUTH_TOKEN`)
- [ ] Deploy new artifacts (WASM + integrity.json)
- [ ] Verify bundle sizes (≤1.5 MB UI, ≤5 MB WASM)
- [ ] Run smoke tests (load session, trigger clips, verify audio)
- [ ] Monitor for errors (check SDK event logs)
- [ ] Measure latency (Heartbeat ≤100ms, commands ≤50ms)

### Step 9: Production Rollout

**Phased rollout recommended:**

1. **Week 1:** 10% traffic (canary deployment)
2. **Week 2:** 50% traffic (gradual rollout)
3. **Week 3:** 100% traffic (full rollout)

**Rollout checklist:**

- [ ] Backup current production deployment
- [ ] Update environment variables
- [ ] Deploy artifacts (with integrity verification)
- [ ] Monitor error rates (target: <0.1%)
- [ ] Monitor performance (Heartbeat latency, CPU usage)
- [ ] Verify no bundle size regressions
- [ ] Test rollback procedure (dry run)

---

## Testing Your Migration

### Unit Tests

**Update test imports:**

```typescript
// Before:
import { LoadSessionCommand } from 'orpheus-contract';

// After:
import { LoadSessionCommand } from '@orpheus/contract';
```

**Run tests:**

```bash
pnpm test

# Expected output:
# ✓ Contract validation tests (13 tests)
# ✓ Frequency validator tests (13 tests)
# ✓ Driver selection tests (8 tests)
# ✓ Client health check tests (5 tests)
```

### Integration Tests

**Test driver selection:**

```typescript
import { describe, it, expect } from 'vitest';
import { OrpheusClient, DriverType } from '@orpheus/client';

describe('Driver Selection', () => {
  it('should select Native driver first', async () => {
    const client = new OrpheusClient({
      preferredDrivers: [DriverType.Native, DriverType.WASM],
    });

    await client.connect();

    expect(client.getCurrentDriver()).toBe(DriverType.Native);
  });

  it('should fallback to WASM if Native unavailable', async () => {
    const client = new OrpheusClient({
      preferredDrivers: [DriverType.WASM],
    });

    await client.connect();

    expect(client.getCurrentDriver()).toBe(DriverType.WASM);
  });
});
```

**Test authentication:**

```typescript
describe('Service Driver Authentication', () => {
  it('should require auth token for /command endpoint', async () => {
    const response = await fetch('http://localhost:8080/command', {
      method: 'POST',
      body: JSON.stringify({ method: 'LoadSession', params: {} }),
    });

    expect(response.status).toBe(401); // Unauthorized
  });

  it('should accept valid auth token', async () => {
    const response = await fetch('http://localhost:8080/command', {
      method: 'POST',
      headers: {
        Authorization: `Bearer ${process.env.ORPHEUS_AUTH_TOKEN}`,
      },
      body: JSON.stringify({ method: 'LoadSession', params: {} }),
    });

    expect(response.ok).toBe(true);
  });
});
```

### Manual Testing

**Test plan:**

1. **Client Initialization**
   - [ ] Client connects to preferred driver
   - [ ] Fallback works if preferred driver unavailable
   - [ ] Health checks succeed
   - [ ] Reconnection works after disconnect

2. **Command Execution**
   - [ ] LoadSession command succeeds
   - [ ] SaveSession command succeeds
   - [ ] RenderClick command succeeds
   - [ ] SetTransport command succeeds
   - [ ] TriggerClipGridScene command succeeds

3. **Event Handling**
   - [ ] TransportTick events received (≤30 Hz)
   - [ ] RenderProgress events received (≤10 Hz)
   - [ ] RenderDone event received
   - [ ] Error events received (test with invalid command)
   - [ ] SessionChanged events received

4. **Performance**
   - [ ] Bundle size ≤1.5 MB (UI)
   - [ ] WASM artifact ≤5 MB
   - [ ] Heartbeat latency ≤100ms (99th percentile)
   - [ ] Command latency ≤50ms (95th percentile)

5. **Security**
   - [ ] Service Driver rejects requests without auth token
   - [ ] Service Driver accepts requests with valid auth token
   - [ ] WASM SRI verification succeeds
   - [ ] WASM SRI verification fails with tampered file

---

## Rollback Procedures

### Scenario 1: Migration Issues in Development

**Rollback steps:**

```bash
# 1. Revert package.json changes
git checkout HEAD -- package.json pnpm-lock.yaml

# 2. Reinstall old dependencies
pnpm install

# 3. Revert code changes
git checkout HEAD -- src/

# 4. Verify rollback
pnpm dev
```

### Scenario 2: Production Issues

**Immediate rollback (< 5 minutes):**

```bash
# 1. Switch to previous deployment
kubectl rollout undo deployment/orpheus-app

# 2. Verify service health
curl http://app.example.com/health

# 3. Monitor error rates
kubectl logs -f deployment/orpheus-app
```

**Database rollback (if schema changes):**

```sql
-- v1.0.0 introduced no database schema changes
-- No database rollback required
```

### Scenario 3: Performance Degradation

**Investigate:**

```bash
# Check bundle sizes
pnpm run perf:validate

# Check dependency graph
pnpm run dep:graph

# Profile application
pnpm run build --profile
```

**Temporary mitigation:**

```typescript
// Disable expensive features temporarily
const client = new OrpheusClient({
  preferredDrivers: [DriverType.Native], // Skip WASM
  healthCheckInterval: 10000, // Reduce health check frequency
});
```

---

## Support & Resources

### Documentation

- [CHANGELOG](../CHANGELOG.md) - Detailed release notes
- [ARCHITECTURE.md](../ARCHITECTURE.md) - System architecture overview
- [CONTRIBUTING.md](../docs/CONTRIBUTING.md) - Contribution guidelines
- [Contract Development Guide](../docs/CONTRACT_DEVELOPMENT.md) - Contract schema details
- [Driver Architecture](../docs/DRIVER_ARCHITECTURE.md) - Driver implementation details

### Migration Assistance

- **GitHub Discussions:** https://github.com/chrislyons/orpheus-sdk/discussions
- **Issue Tracker:** https://github.com/chrislyons/orpheus-sdk/issues
- **Email:** support@orpheus-sdk.dev (for urgent migration issues)

### Common Migration Issues

**Issue 1: "Cannot find module '@orpheus/contract'"**

**Solution:**

```bash
# Ensure correct package name
pnpm remove orpheus-contract
pnpm add @orpheus/contract@^1.0.0

# Clear cache
rm -rf node_modules pnpm-lock.yaml
pnpm install
```

**Issue 2: "Service Driver returns 401 Unauthorized"**

**Solution:**

```bash
# Set auth token
export ORPHEUS_AUTH_TOKEN="test-secret-token-123"

# Verify token in client
console.log(process.env.ORPHEUS_AUTH_TOKEN); // Should not be undefined

# Restart Service Driver
pnpm --filter @orpheus/engine-service start
```

**Issue 3: "WASM SRI verification failed"**

**Solution:**

```bash
# Regenerate integrity.json
./scripts/build-wasm.sh

# Ensure integrity.json is deployed alongside WASM
ls -lh public/orpheus.wasm public/integrity.json

# Clear browser cache (Cmd+Shift+R)
```

**Issue 4: "Bundle size exceeds budget"**

**Solution:**

```bash
# Analyze bundle
pnpm run build -- --profile

# Check dependency graph
pnpm run dep:graph

# Optimize (example: tree shaking)
# - Remove unused imports
# - Use dynamic imports for large dependencies
# - Enable code splitting in vite.config.ts
```

**Issue 5: "Circular dependency detected"**

**Solution:**

```bash
# Find circular dependencies
pnpm run dep:check

# Output example:
# src/client.ts > src/drivers/native.ts > src/client.ts

# Fix: Break circular dependency with dependency injection or separate interface
```

---

## Deprecation Notices

### Deprecated in v1.0.0 (Removal in v2.0.0)

**None.** v1.0.0 is the first stable release. Deprecations will be announced in v1.x releases with at least 6 months notice before removal in v2.0.0.

### Future Deprecations (Planned)

**v1.1.0 (2026-Q1):**

- Contract v0.1.0-alpha will be marked as "deprecated" (still functional, but no longer recommended)
- Implicit driver selection will issue warnings (explicit `preferredDrivers` array required)

**v2.0.0 (2027+):**

- Contract v0.1.0-alpha support removed (v1.0.0+ required)
- Implicit driver selection removed (explicit configuration mandatory)

---

## Upgrade Timeline Recommendations

### Recommended Timelines by User Type

**Hobbyists / Personal Projects:**

- **Upgrade Window:** 3-6 months
- **Urgency:** LOW (upgrade when convenient)
- **Support End:** v0.1.0-alpha support ends 2025-11-30

**Startups / Small Teams:**

- **Upgrade Window:** 4-8 weeks
- **Urgency:** MEDIUM (plan migration sprint)
- **Support End:** v1.0.0-beta support ends 2026-01-31

**Enterprise / Production Systems:**

- **Upgrade Window:** 8-12 weeks (with staging)
- **Urgency:** HIGH (security hardening in v1.0.0)
- **Support End:** v1.0.0-beta support ends 2026-01-31

### Support End Dates

| Version         | Support End    | Extended Support  | Actions Required                           |
| --------------- | -------------- | ----------------- | ------------------------------------------ |
| v0.1.0-alpha    | **2025-11-30** | None              | Upgrade to v1.0.0+ immediately             |
| v1.0.0-beta     | 2026-01-31     | 2026-04-30 (paid) | Upgrade to v1.0.0 stable by end of January |
| v1.0.0 (stable) | TBD (LTS)      | TBD               | Maintained long-term                       |

---

## Conclusion

Upgrading to Orpheus SDK v1.0.0 brings:

- ✅ Stabilized contract (v1.0.0 stable)
- ✅ Security hardening (authentication, SRI, SBOM)
- ✅ Performance budgets (bundle size, latency SLAs)
- ✅ Improved developer experience (pre-commit hooks, validation scripts)
- ✅ Long-term support (LTS for v1.0.0)

**Most migrations take 2-4 hours for typical integrations.**

If you encounter issues not covered in this guide, please:

1. Check [GitHub Discussions](https://github.com/chrislyons/orpheus-sdk/discussions)
2. Search [Issue Tracker](https://github.com/chrislyons/orpheus-sdk/issues)
3. Contact support@orpheus-sdk.dev (for urgent issues)

**Happy upgrading! 🎉**

---

**Document Version:** 1.0
**Last Updated:** 2025-10-26
**Next Review:** After v1.0.0 stable release
