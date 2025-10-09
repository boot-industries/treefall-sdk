# Package Naming Conventions

The Orpheus monorepo publishes packages under the `@orpheus` npm scope to provide a consistent namespace
across our JavaScript and TypeScript surfaces. This scope is registered with npm to prevent naming
collisions when publishing publicly.

## Canonical Packages

- `@orpheus/shmui` &mdash; The Shmui user interface package. The historic codename "Shmui" is retained to
  help developers map legacy documentation and modules to the new monorepo layout.
- `@orpheus/engine-native` &mdash; Native bindings that expose the Orpheus engine to Electron shells and other
  Node.js hosts.

## Reserved Names

The following package names are reserved for future work and must not be used without approval:

- `@orpheus/core`
- `@orpheus/client`
- `@orpheus/contract`
- `@orpheus/engine-*`

These reservations ensure we can grow the Orpheus platform without renaming published artifacts.
