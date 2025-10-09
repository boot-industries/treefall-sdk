# API Surface Index

This index catalogs public entry points exposed by the Orpheus SDK workspace. Update this document whenever new packages or
notable APIs are added.

## JavaScript Packages

| Package | Entry Point | Notes |
| --- | --- | --- |
| `@orpheus/shmui` | `packages/shmui/src/index.js` | React components and UI helpers; see ORP068 §III for planned feature rollouts. |
| `@orpheus/engine-native` | `packages/engine-native/` | Node/Electron bindings wrapping the C++ engine. Build orchestrated via CMake. |
| `@orpheus/engine-wasm` | _Planned_ | WebAssembly bundle; refer to [Performance Budgets](PERFORMANCE.md) for size constraints. |
| `@orpheus/client` | _Planned_ | Contract negotiation and command helpers based on ORP062. |

## C++ Components

| Component | Location | Description |
| --- | --- | --- |
| Core Engine | `src/` | Primary C++ source compiled into native bindings and executables. |
| Adapters | `adapters/` | Integration points for external hosts and audio backends. |
| Tests | `tests/` | CTest-driven validation of engine behavior. |

## Documentation Cross-Reference

- [Driver Architecture](DRIVER_ARCHITECTURE.md) – Runtime-specific integration notes.
- [Contract Guide](CONTRACT_DEVELOPMENT.md) – Command/event schemas shared across drivers.
- [Migration Guide](MIGRATION_GUIDE.md) – When to expose new APIs per phase.
