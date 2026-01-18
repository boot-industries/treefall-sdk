# Deployment Infrastructure - Notes

**Last Updated:** 2026-01-18
**Related Diagram:** [deployment-infrastructure.mermaid.md](./deployment-infrastructure.mermaid.md)

## Overview

This document describes the CI/CD pipeline, build system, and testing infrastructure for the Orpheus SDK.

## Development Environment

### Required Tools

| Tool | Version | Purpose |
|------|---------|---------|
| C++ Compiler | C++20 (clang 13+, gcc 11+, msvc 2019+) | Core SDK |
| CMake | 3.20+ | Build system |
| Node.js | 18+ | TypeScript packages |
| pnpm | 8+ | Package manager |
| Git | 2.x | Version control |

### Local Build

```bash
# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build

# Test
ctest --test-dir build --output-on-failure
```

### Sanitizers (Debug Build)

- **AddressSanitizer:** Memory errors, buffer overflows
- **UndefinedBehaviorSanitizer:** Undefined behavior detection

## CI Pipeline

### Unified Pipeline (ci-pipeline.yml)

**Trigger:** Push, Pull Request
**Target Duration:** <25 minutes

### Jobs

| Job | Purpose | Duration |
|-----|---------|----------|
| cpp-build-test | Matrix build (6 combinations) | ~15 min |
| lint | Code style (clang-format, eslint) | ~2 min |
| native-driver | N-API bindings build | ~5 min |
| typescript | TypeScript packages build | ~3 min |
| integration | End-to-end tests | ~5 min |
| deps | Dependency audit | ~2 min |
| perf | Performance budget check | ~3 min |

### Matrix Build

| OS | Build Types |
|----|-------------|
| ubuntu-latest | Debug, Release |
| windows-latest | Debug, Release |
| macos-latest | Debug, Release |

**Total:** 6 parallel jobs

## ORP121 Test Suite

### routing_matrix_test (27 tests)

Tests added for ORP121 features:
- `TruePeakMeteringDetectsInterSamplePeaks`
- `HeadroomModeNoneNoAttenuation`
- `HeadroomModePerGroupAttenuates`
- `HeadroomModeLogarithmicAttenuates`
- `SoftKneeLimiterContinuous`
- `GainSmootherExtendedRange`
- `ConstantPowerPanLaw`

### callback_queue_stress_test (6 tests)

Tests for lock-free SPSC queue:
- `SingleStartStopCallback`
- `ConcurrentStartStopClips`
- `HighFrequencyCommands` (1000 rapid commands)
- `SustainedOperationTwoSeconds`
- `RapidFireWithoutProcessing`
- `CallbackLatency`

### Performance Tests

- `PerformanceTest10MinuteWav` - Waveform rendering benchmark

## Specialized Workflows

### Chaos Tests (Nightly)

**Schedule:** 3 AM UTC
**Duration:** ~2 hours

**Scenarios:**
- Rapid start/stop (100 clips/second)
- 24-hour stability
- Resource exhaustion
- Recovery from failures

### Security Audit (Weekly)

**Schedule:** Monday 8 AM UTC

**Tools:**
- npm audit
- OSV Scanner
- SBOM generation (CycloneDX)
- Dependency review

### Documentation Publish

**Trigger:** Release tags (v*.*.*)

**Steps:**
1. Generate Doxygen API docs
2. Build user guides
3. Publish to GitHub Pages

## Build Artifacts

### C++ Libraries

```
build/src/core/
├── liborpheus_core.a
├── liborpheus_transport.a
├── liborpheus_routing.a
└── liborpheus_audio_io.a
```

### Executables

```
build/adapters/minhost/
└── orpheus_minhost

build/apps/clip-composer/
└── OrpheusClipComposer.app  (macOS)
```

### npm Packages

```
@orpheus/client
@orpheus/contract
@orpheus/engine-native
@orpheus/engine-service
@orpheus/engine-wasm
@orpheus/react
```

## Distribution Channels

### GitHub Releases

- Source code archives (zip, tar.gz)
- Compiled binaries (multi-platform)
- CHANGELOG.md
- Release notes

### npm Registry

```bash
npm install @orpheus/client
```

### GitHub Pages

- API documentation (Doxygen)
- User guides
- Architecture diagrams

## When to Update Infrastructure

### Adding New Test Suites
1. Add test executable to `tests/CMakeLists.txt`
2. Add to CI pipeline if needed
3. Update this document

### Adding New Packages
1. Create package in `packages/`
2. Add to `pnpm-workspace.yaml`
3. Add build job to CI pipeline

### Changing Build Configuration
1. Update `CMakeLists.txt`
2. Test locally on all platforms
3. Verify CI passes

## Related Diagrams

- [architecture-overview](./architecture-overview.notes.md) - System design
- [repo-structure](./repo-structure.notes.md) - Directory layout
