# Driver Architecture Overview

The Orpheus engine exposes multiple drivers to accommodate different runtime environments. This summary distills the guidance in
ORP062 §§2–4 and ORP063 §III.

## Service Driver

- **Purpose:** Runs Orpheus as a long-lived service instance for orchestration or remote control.
- **Transport:** gRPC/WebSocket bridge emitting contract events defined in ORP062 §1.4.
- **Usage:** Ideal for automation pipelines or multi-session control surfaces.

## WebAssembly Driver

- **Purpose:** Delivers Orpheus capabilities directly to the browser or WebView contexts.
- **Artifacts:** Published as `@orpheus/engine-wasm` with size budgets enforced via [`budgets.json`](../budgets.json).
- **Integration:** Loaded asynchronously; coordinate with the `@orpheus/client` package for contract negotiation.

## Native Driver

- **Purpose:** Provides a Node/Electron binding around the compiled C++ core.
- **Artifacts:** Distributed as `@orpheus/engine-native`; uses the repository CMake pipeline for builds.
- **Integration:** Requires native modules to be rebuilt per-platform; PNPM scripts should call into CMake as described in ORP061 §Phase 1.

## Selecting a Driver

| Scenario | Recommended Driver | Notes |
| --- | --- | --- |
| Desktop app with rich local features | Native | Offers lowest latency; ensure CI covers target OS combinations. |
| Browser-based session management | WebAssembly | Align with bundle budgets and lazy-load strategies. |
| Centralized orchestration service | Service | Pair with authentication and rate limits per ORP066 §X. |

All drivers must respect the contract schema definitions documented in [Contract Guide](CONTRACT_DEVELOPMENT.md).
