# Contract Development Guide

The contract boundary ensures Shmui UI and Orpheus engine components communicate reliably. This guide summarizes the normative
requirements in ORP062 §1 and ORP063 §IV.

## Versioning

- Contracts follow `vMAJOR.MINOR.PATCH`.
- Breaking schema changes increment `MAJOR` and require coordinated releases across all drivers.
- Minor extensions should remain backward compatible and include capability negotiation metadata.

## Command Schemas

Key commands include `LoadSession`, `RenderClick`, `SetTransport`, and `TriggerClipGridScene`. Implementations MUST validate input
using shared schema definitions (Zod or equivalent) before invoking native code.

## Event Streams

- `TransportTick` – Emits timeline position updates (≤ 30 Hz).
- `RenderProgress` – Reports render completion percentages (≤ 10 Hz).
- `RenderDone` / `SessionChanged` – Emit on completion or structural change.

Consumers should treat events as immutable records and avoid mutating payloads.

## Error Handling

- Standardize error objects with `kind`, `code`, and `message` fields.
- Provide remediation hints (e.g., `resolution: "retry"`).
- Surface contract negotiation failures prominently in UI logs.

## Tooling Checklist

1. Update shared TypeScript types in `@orpheus/client` when schemas evolve.
2. Regenerate test fixtures and add contract regression tests in `tests/`.
3. Coordinate documentation updates in [Performance Budgets](PERFORMANCE.md) if thresholds change.
