# Migration Guide

This guide maps practical migration steps to the phases described in ORP061 and ORP068.

## Phase 0 – Prepare the Monorepo

- Import Shmui into `packages/shmui/` without functional changes.
- Ensure parallel CI jobs exist for C++ and UI workflows (see `.github/workflows/interim-ci.yml`).
- Align package names with `@orpheus/*` scope per [Package Naming](PACKAGE_NAMING.md).

## Phase 1 – Normalize Tooling

- Introduce engine bindings (`@orpheus/engine-native` / `@orpheus/engine-wasm`).
- Wire UI scripts to consume contract clients defined in ORP062.
- Expand CI matrices cautiously, validating cache performance and driver compatibility.

## Phase 2 – Integrate Features

- Replace legacy Shmui endpoints with Orpheus engine capabilities.
- Add UI toggles or feature flags for incremental rollouts.
- Update documentation and storybook entries to reflect new flows.

## Phase 3 – Governance and Expansion

- Enforce performance budgets defined in [Performance Budgets](PERFORMANCE.md).
- Apply branch protection and release sign-off requirements outlined in ORP063 §III.D.

## Migration Checklist

1. Validate bundle and latency budgets after each milestone.
2. Audit package versions and update changesets prior to release.
3. Keep the [Documentation Index](INDEX.md) current as new guides ship.
