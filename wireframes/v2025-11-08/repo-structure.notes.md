# Repository Structure Notes

## Overview

The Orpheus SDK repository follows a clear hierarchical organization that separates concerns between the core C++ SDK, language bindings, host integrations, and applications. This structure enables multiple development contexts while maintaining strict architectural boundaries.

## Directory Organization

### Core SDK (`src/` + `include/`)

**Purpose:** Minimal, deterministic audio primitives in C++20

The core SDK is the heart of the repository, implementing all audio processing logic without any host-specific dependencies. It's designed to be:
- **Offline-first** - No runtime network calls
- **Deterministic** - Bit-identical output across platforms
- **Broadcast-safe** - No allocations in audio threads

**Key subdirectories:**
- `src/core/` - Session graph, transport controller, audio I/O primitives
- `src/platform/` - Platform-specific drivers (CoreAudio, WASAPI, ALSA)
- `src/dsp/` - DSP processing (oscillators, gain smoothing)
- `include/orpheus/` - Public API headers exported by the SDK

**When to modify:**
- Adding new audio processing features
- Implementing platform drivers
- Extending the session model
- Core transport/routing logic changes

### Driver Layer (`packages/`)

**Purpose:** JavaScript/TypeScript bindings for web and Node.js environments

This layer provides three different driver types for accessing the C++ SDK from JavaScript:

1. **Native Driver** (`engine-native/`) - N-API bindings for direct in-process access
2. **Service Driver** (`engine-service/`) - HTTP + WebSocket server for remote access
3. **WASM Driver** (`engine-wasm/`) - Emscripten-compiled for browser environments

The `client/` package provides a unified interface that automatically selects the best available driver.

**When to modify:**
- Adding new SDK commands to the contract
- Implementing new driver types
- Updating client handshake protocol
- Adding React components for UI integration

**Key files:**
- `contract/schemas/` - JSON schemas for all commands/events
- `client/src/client.ts` - Unified client interface
- `engine-service/src/server.ts` - HTTP server implementation

### Adapters (`adapters/`)

**Purpose:** Thin integration layers for specific host environments

Adapters are typically ≤300 LOC and focus solely on bridging the core SDK to a specific host. They should not contain business logic.

**Current adapters:**
- **minhost** - Command-line interface for testing and offline rendering
- **reaper** - REAPER DAW extension (currently quarantined)

**When to modify:**
- Adding new host integrations
- Updating command-line argument parsing (minhost)
- Implementing host-specific UI panels

**Architecture guideline:**
Keep adapters minimal. If you find yourself adding complex logic, consider whether it belongs in the core SDK instead.

### Applications (`apps/`)

**Purpose:** Complete applications that compose SDK functionality

Applications are full-featured programs with their own UI, session management, and user workflows. They can use adapters, drivers, or integrate the SDK directly.

**Current applications:**
- **clip-composer** - Professional JUCE-based soundboard (flagship product)
- **juce-demo-host** - JUCE integration demo

**Future applications (planned):**
- **wave-finder** - Harmonic calculator and frequency scope
- **fx-engine** - LLM-powered effects processing

**When to modify:**
- Implementing application-specific features
- Adding UI components
- Integrating new SDK capabilities into apps

**Documentation:**
Each application has its own `docs/` subdirectory. For Clip Composer, see `apps/clip-composer/docs/OCC/` for comprehensive design docs.

### Testing Infrastructure (`tests/` + `tools/`)

**Purpose:** Comprehensive test coverage and validation

**tests/**: GoogleTest-based unit and integration tests
- `transport/` - Transport controller tests (clip playback, gain, loop, fade)
- `routing/` - Routing matrix tests
- `audio_io/` - Driver and file reader tests
- `determinism/` - Cross-platform determinism validation

**tools/**: Developer utilities and conformance tools
- `cli/` - Command-line tools (session inspector)
- `conformance/` - JSON round-trip validation
- `fixtures/` - Test audio files and session JSON
- `perf/` - Performance benchmarks

**When to modify:**
- Adding tests for new features
- Creating new conformance tools
- Adding performance benchmarks

**Running tests:**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
ctest --test-dir build --output-on-failure
```

### Documentation (`docs/`)

**Purpose:** Architecture, implementation plans, and guides

**Key documents:**
- `ARCHITECTURE.md` - Authoritative system design (root level)
- `ROADMAP.md` - Development timeline and milestones (root level)
- `orp/` - Orpheus implementation plans (ORP prefix)
- `platform/` - Platform-specific guides
- `integration/` - Integration guides for developers

**Archive strategy:**
Documents older than 180 days are moved to `docs/orp/archive/` to reduce context clutter while preserving history.

**When to create new docs:**
- New implementation plans (use ORP prefix)
- Platform-specific guides
- Integration tutorials
- Architecture decision records

### Configuration Files (Root Level)

**Build System:**
- `CMakeLists.txt` - CMake superbuild configuration
- `cmake/` - CMake modules and helper scripts

**JavaScript/TypeScript:**
- `package.json` - Node.js dependencies and scripts
- `pnpm-workspace.yaml` - Monorepo workspace configuration
- `pnpm-lock.yaml` - Locked dependency versions

**Code Quality:**
- `.clang-format` - C++ code formatting (CI enforced)
- `.clang-tidy` - Static analysis rules
- `.eslintrc.cjs` - TypeScript linting
- `.prettierrc.json` - TypeScript/JSON formatting

**CI/CD:**
- `.github/workflows/` - GitHub Actions pipelines
  - `ci-pipeline.yml` - Main CI (matrix builds, tests, lint)
  - `chaos-tests.yml` - Nightly failure recovery tests
  - `security-audit.yml` - Weekly vulnerability scans

**Development Guides:**
- `CLAUDE.md` - Claude Code development guide
- `AGENTS.md` - AI assistant guidelines
- `.claudeignore` - Files to exclude from AI context

## File Boundaries (What NOT to Read)

To maintain fast context loading, avoid reading these directories:

**Build Artifacts:**
- `build-*/` - Multiple build variant directories
- `apps/*/build/` - Application-specific builds
- `orpheus_clip_composer_app_artefacts/` - Tauri build outputs
- `Third-party/*/build/` - Dependency builds
- `*.o`, `*.a`, `*.dylib`, `*.so` - Compiled objects

**Dependencies:**
- `node_modules/` - npm packages (excluded by `.gitignore`)
- `Third-party/` - Vendored dependencies (excluded by `.claudeignore`)

## Multi-Instance Usage Pattern

The repository supports two Claude Code instances for context isolation:

1. **SDK Instance** (from repo root) - Core library development
2. **Clip Composer Instance** (from `apps/clip-composer/`) - Application development

Each instance has its own `.claude/` directory with instance-specific configuration, preventing config file collisions and maintaining focused context.

## Common Workflows

### Adding a New Core Feature

1. Design API in `include/orpheus/your_feature.h`
2. Implement in `src/core/your_feature/`
3. Add unit tests in `tests/your_feature/`
4. Update CMakeLists.txt to build new files
5. Document in architecture docs

### Adding a New Driver Command

1. Update contract schema in `packages/contract/schemas/`
2. Implement in all three drivers (native, service, WASM)
3. Update client interface in `packages/client/`
4. Add integration tests
5. Update contract version if breaking change

### Creating a New Application

1. Create directory in `apps/your_app/`
2. Add CMake integration (optional for SDK builds)
3. Document architecture in `apps/your_app/docs/`
4. Integrate with SDK via adapters or direct linking
5. Add to build system with `ORPHEUS_ENABLE_APP_YOUR_APP` option

## Architecture Decision Records

Key architectural decisions are documented in:
- `ARCHITECTURE.md` - System-level design principles
- `docs/ORP/` - Implementation plans with rationale
- `apps/clip-composer/docs/OCC/` - Application-specific decisions

When making significant architectural changes, document the decision, alternatives considered, and rationale in the appropriate location.

## Related Diagrams

- See `architecture-overview.mermaid.md` for system layers
- See `component-map.mermaid.md` for module relationships
- See `entry-points.mermaid.md` for interaction methods
