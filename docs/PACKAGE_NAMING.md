# Package Naming Conventions

The Orpheus monorepo publishes JavaScript packages under the `@orpheus/*` npm scope to maintain a predictable namespace across
UI, engine bindings, and integration utilities. These conventions align with ORP061 Phase 0 namespacing requirements and the
Governance policies in ORP063 §III.D.

## Canonical Packages

| Package | Codename | Description |
| --- | --- | --- |
| `@orpheus/shmui` | **Shmui** | React-based UI library imported from the legacy Shmui project. Retains codename to preserve brand recognition and existing documentation references. |
| `@orpheus/engine-native` | **Orpheus Core (Native)** | Node/Electron bindings that expose compiled C++ artifacts built via the repository CMake toolchain. |
| `@orpheus/engine-wasm` | **Orpheus Core (WASM)** | Planned WebAssembly distribution that surfaces the engine to browser environments. |
| `@orpheus/client` | **Contract Client** | Future abstraction that mediates contract negotiation and command/event flows described in ORP062. |
| `@orpheus/core` | **Core Facade** | Reserved namespace for shared TypeScript utilities that wrap engine primitives. |

## Naming Rules for Future Packages

1. **Scope Prefix** – All publishable packages MUST live under the `@orpheus/` scope. Internal-only tooling may remain private but should still respect the scope to avoid collisions.
2. **Feature-Derived Suffixes** – Use concise suffixes that describe the artifact (`-wasm`, `-native`, `-docs`). Avoid ambiguous terms or raw codenames without context.
3. **Stability Tags** – Pre-release packages SHOULD include semver pre-release identifiers (`-alpha`, `-beta`) until ORP066 stabilization milestones are met.
4. **Codename Preservation** – Legacy initiatives such as Shmui retain their codename within the scoped package name (e.g., `@orpheus/shmui`) to support continuity across migration phases.

## Scope Governance

- **Publishing Rights** – Only members of the Orpheus release engineering group may publish under `@orpheus/*`. Access is managed through npm organization roles and mirrored in GitHub teams.
- **Change Control** – Package renames or new namespace allocations require approval from the Technical Steering Committee, referencing ORP066 §II for validation criteria.
- **Deprecation Policy** – Deprecated packages remain reserved for one major release cycle; updates to `README` and CHANGELOG must warn consumers before removal.

## Rationale for Retaining “Shmui”

Shmui carries significant institutional knowledge and user familiarity. Preserving the codename inside `@orpheus/shmui`:

- Maintains traceability to historical documentation and UI patterns referenced throughout ORP061 and ORP068.
- Reduces onboarding friction for teams migrating from the standalone Shmui repository.
- Signals continuity while the UI transitions into broader Orpheus governance.

Future documentation should refer to the package as **Orpheus Shmui UI (`@orpheus/shmui`)** to align branding with technical accuracy.
