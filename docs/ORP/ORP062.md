# ORP062 Technical Addendum: Engine Contracts, Drivers, and Integration Guardrails

This addendum defines the **contractual, architectural, and validation guarantees** underpinning the Orpheus SDK and Shmui integration. It extends `ORP061 Migration Plan` and is normative for all implementations of the Orpheus Engine Boundary, including Service, WASM, and Native drivers.

***

## §1.0 Engine Boundary Contract

### 1.1 Overview

All interaction between the Shmui UI and the Orpheus Engine MUST occur through a **versioned contract boundary** implemented by the `@orpheus/client` package. This boundary defines command and event schemas, lifecycle expectations, and error handling.

### 1.2 Contract Versioning

- Version format: `vMAJOR.MINOR.PATCH`.
- MAJOR changes indicate incompatible message structure changes.
- MINOR changes add new fields or optional commands.
- PATCH covers documentation or bugfixes.

Each driver MUST declare its supported contract version at handshake. The client MAY refuse negotiation if the contractVersion differs by a MAJOR number.

```javascript
{
  "contractVersion": "1.0.0",
  "engineId": "orpheus-core-1.0.0",
  "capabilities": ["session", "render", "transport"]
}

```

### 1.3 Command Schemas

Commands are JSON-serializable objects validated via Zod schemas at runtime.

| Command | Description | Payload |
| --- | --- | --- |
| `LoadSession` | Loads a session JSON file or object | `{ path?: string, data?: object }` |
| `SaveSession` | Serializes session to file or string | `{ path?: string }` |
| `TriggerClipGridScene` | Executes a scene | `{ sceneId: string }` |
| `RenderClick` | Renders click track | `{ bars: number, bpm: number }` |
| `SetTransport` | Updates transport state | `{ position: number, playing: boolean }` |

### 1.4 Event Schemas

Engines MUST emit structured events with timestamps and IDs.

| Event | Description | Frequency |
| --- | --- | --- |
| `TransportTick` | Emits position and beat info | ≤ 30 Hz |
| `RenderProgress` | Percentage updates for renders | ≤ 10 Hz |
| `RenderDone` | Fired when render completes | once per render |
| `SessionChanged` | Session structure update | as needed |
| `Error` | Structured engine error | on demand |

### 1.5 Error Taxonomy

Errors follow a hierarchical naming scheme.

| Kind | Code | Example Message |
| --- | --- | --- |
| Validation | `InvalidSession` | "Session JSON malformed at track index 4" |
| Render | `OutOfRange` | "Render duration exceeds buffer limit" |
| Transport | `InvalidPosition` | "Transport set before session loaded" |
| System | `CrashRecovered` | "Engine restarted after fault" |

Errors MUST include `kind`, `code`, `message`, and optional `details`.

### 1.6 Transport & Handshake

Each driver MUST support a handshake protocol establishing connection and version agreement. The handshake MAY be JSON over WebSocket, HTTP POST, or IPC message.

***

## §2.0 Driver Architecture

### 2.1 Service Driver (A)

Implements the boundary via a **local HTTP + WebSocket** service (`orpheusd`).

**Lifecycle:**

1. Spawned automatically by `@orpheus/client` or manually.
2. Responds to `/health`, `/version`, and `/contract` endpoints.
3. WebSocket channel (`/ws`) streams events.

**Requirements:**

- Bind only to `127.0.0.1` by default.
- Support graceful shutdown (`SIGTERM` → flush → exit).
- Return `503` if the engine is busy.
- MUST emit heartbeats every 10s.

### 2.2 WASM Driver (B)

Compiled from `orpheus-core` via **Emscripten**, running inside a Web Worker.

**Constraints:**

- Single-threaded WASM module; heavy tasks MUST yield via async.
- Worker interface proxies all messages through `postMessage`.
- Memory reuse: allocate audio buffers once and reuse.

**Initialization:**

```javascript
const worker = new Worker('orpheus.wasm.js');
worker.postMessage({ type: 'init', contractVersion: '1.0.0' });

```

**Security:**

- Load `.wasm` via SRI-verified URL.
- No direct filesystem access; use `FileSystemAccess` API or Blobs.

### 2.3 Native Driver (C)

Provides N-API bindings for Electron or Node.

**Structure:**

- Main process only; renderer uses IPC via `@orpheus/client`.
- `.node` binary loaded per platform.
- Prebuilt binaries signed and checksum-verified.

**Crash Isolation:**

- Engine runs in isolated process (child or worker thread).
- Auto-restart on fault; notify client via `Error:CrashRecovered`.

***

## §3.0 Client Broker & UI Integration

### 3.1 Broker Selection

The client automatically selects drivers in priority:

```javascript
Native → WASM → Service

```

Developers MAY override via environment or query string (`ENGINE_MODE=wasm`).

### 3.2 React Integration (Shmui)

- Provide `OrpheusProvider` wrapping the app, exposing `useOrpheus()` hook.
- The provider handles connection state and exposes:
    - `engine` (driver type)
    - `status` (connected, error, syncing)
    - `send(command)` / `on(event)`

### 3.3 UI Feedback

- Display engine status indicator in footer or toolbar.
- Toast notifications for disconnects and fallbacks.
- Offline mode if all drivers unavailable.

***

## §4.0 Determinism & Validation

### 4.1 Golden Audio Tests

A fixed suite of deterministic renders ensures reproducibility.

| Test | Input | Expected Output |
| --- | --- | --- |
| `click_100bpm_2bar` | 100 BPM, 2 bars | WAV hash `f23c…` |
| `click_120bpm_4bar` | 120 BPM, 4 bars | WAV hash `9a4e…` |

Tolerance: ±1 LSB RMS difference allowed. All drivers MUST match reference hashes.

### 4.2 Contract Validation Suite

Each driver MUST pass automated tests verifying:

- Round-trip of every command.
- Proper event ordering (RenderDone after RenderProgress).
- Version negotiation compliance.

### 4.3 Chaos & Recovery Tests

Simulate dropped connections, killed processes, or worker termination.
UI MUST recover or degrade gracefully (status: `degraded`).

***

## §5.0 Build, CI, and Artifact Policy

### 5.1 Prebuilt Artifacts

- `orpheusd` and `.node` binaries prebuilt for macOS (x64+arm64), Windows, Linux.
- Each artifact MUST include SHA-256 checksum and optional signature.

### 5.2 CI Pipelines

- **Fast lane:** JS build, lint, Storybook, and tests on Ubuntu.
- **Native matrix:** C++ builds/tests across all OS.
- **Contract tests:** ensure consistency across drivers.

### 5.3 Size & Performance Budgets

- `@orpheus/ui` bundle ≤ 1.5 MB compressed.
- WASM module ≤ 5 MB compressed.
- Any regression >10% in size or 15% in latency MUST trigger CI warning.

### 5.4 Release Artifacts

- Packages versioned with Changesets.
- Prebuilt binaries attached to GitHub Releases.
- SBOMs generated per release.

***

## §6.0 Security, Licensing, and Governance

### 6.1 Security

- All network traffic (if any) uses localhost or HTTPS.
- No external telemetry by default.
- Optional analytics must be opt-in.

### 6.2 Licensing

- ElevenLabs UI components (Shmui) — MIT license.
- Orpheus core — MIT license.
- Dependencies: Radix, shadcn/ui, etc. Retain attributions in NOTICE.

### 6.3 Documentation & ADRs

Maintain Architecture Decision Records:

| ADR | Title | Purpose |
| --- | --- | --- |
| 001 | Home Repo & UI Codename | Establish `orpheus-sdk` canonical repo |
| 002 | Unified Engine Boundary | Define cross-driver contract |
| 003 | Driver Priority & Fallback | Specify broker selection logic |
| 004 | Determinism Guarantees | Formalize golden-audio testing |

### 6.4 Contributor Guide

- Engine devs install CMake ≥3.20, compiler toolchain.
- UI devs install Node ≥18, PNPM.
- `pnpm dev` runs hot reload UI using mock or service driver.
- CI pre-commit hooks enforce format and lint.