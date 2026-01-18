# Repository Structure - Notes

**Last Updated:** 2026-01-18
**Related Diagram:** [repo-structure.mermaid.md](./repo-structure.mermaid.md)

## Overview

The Orpheus SDK repository is organized as a monorepo containing the C++ core SDK, TypeScript packages (archived), adapters, and applications.

## Directory Structure

### Core SDK (`src/`, `include/`)

The C++20 core providing deterministic audio processing.

#### `src/core/`

| Directory | Purpose | Key Files |
|-----------|---------|-----------|
| `session/` | Session management | `session_graph.cpp`, `session_json.cpp` |
| `transport/` | Real-time transport | `transport_controller.cpp` |
| `routing/` | Audio routing & DSP | `routing_matrix.cpp`, `true_peak_meter.h` |
| `audio_io/` | File I/O | `audio_file_reader.cpp` |

#### `src/platform/`

Platform-specific audio drivers:
- `audio_drivers/coreaudio_driver.cpp` - macOS
- `audio_drivers/dummy_driver.cpp` - Testing

#### `include/orpheus/`

Public API headers:

| Header | Purpose | ORP121 Changes |
|--------|---------|----------------|
| `routing_matrix.h` | Routing configuration | Added `HeadroomMode`, `sample_rate` |
| `transport_controller.h` | Transport API | Lock-free callback queue |
| `performance_monitor.h` | Profiling API | Pre-existing (Q-12) |
| `abi_version.h` | Version negotiation | - |

### ORP121 New Files

```
src/core/routing/
├── true_peak_meter.h      # ITU-R BS.1770-4 meter (Q-04)
├── gain_smoother.cpp      # Extended +12 dB range (C-01)
└── routing_matrix.cpp     # Soft-knee limiter (C-02)

docs/orp/
├── ORP121 Audio Backend Refactoring Master Plan.md
├── ORP122 Phase 4 Quality Improvements Implementation Report.md
└── GAIN_STAGING.md        # Gain staging documentation (Q-06)

tests/
├── routing/routing_matrix_test.cpp  # 27 tests
└── transport/callback_queue_stress_test.cpp  # 6 tests (Q-07)
```

### Packages (`packages/`)

#### `shmui-juce/`

Audio visualization components for JUCE applications:

| Component | Purpose |
|-----------|---------|
| `AudioAnalyzer` | FFT, RMS, frequency band analysis |
| `WaveformVisualizer` | Waveform display variants |
| `BarVisualizer` | Frequency band display |
| `MatrixDisplay` | LED-style matrix display |

**Note:** TypeScript packages archived (2025-11-05). See `docs/DECISION_PACKAGES.md`.

### Adapters (`adapters/`)

Thin integration layers for host applications:

| Adapter | Status | Purpose |
|---------|--------|---------|
| `minhost/` | Active | CLI for offline rendering |
| `reaper/` | Quarantined | REAPER extension |

### Applications (`apps/`)

| Application | Technology | Status |
|-------------|------------|--------|
| `clip-composer/` | JUCE C++ | v0.2.x |
| `juce-demo-host/` | JUCE C++ | Active |

### Documentation (`docs/`)

```
docs/
├── orp/                   # ORP implementation documents (120+)
│   ├── INDEX.md          # Document catalog
│   ├── ORP121 ...        # Audio backend refactoring
│   └── ORP122 ...        # Phase 4 report
├── ARCHITECTURE.md        # System design
├── ROADMAP.md            # Development timeline
└── GAIN_STAGING.md       # Gain staging reference
```

### Wireframes (`wireframes/`)

```
wireframes/
├── v2025-11-08/          # Previous version
│   ├── README.md
│   ├── architecture-overview.mermaid.md
│   └── ...
└── v2026-01-18/          # Current (ORP121 updates)
    ├── README.md
    ├── architecture-overview.mermaid.md
    ├── component-map.mermaid.md
    ├── data-flow.mermaid.md
    └── ...
```

### Configuration Files

| File | Purpose |
|------|---------|
| `CMakeLists.txt` | C++ build configuration |
| `CLAUDE.md` | Development guide |
| `.clang-format` | C++ code style |
| `.github/workflows/` | CI/CD pipelines |
| `budgets.json` | Performance budgets |

## Build System

### CMake Structure

```
CMakeLists.txt (root)
├── src/CMakeLists.txt
│   ├── src/core/CMakeLists.txt
│   └── src/platform/CMakeLists.txt
├── tests/CMakeLists.txt
├── adapters/CMakeLists.txt
└── apps/CMakeLists.txt
```

### Build Commands

```bash
# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build

# Test
ctest --test-dir build --output-on-failure
```

## When to Add Files

### Adding Core SDK Features
1. Header in `include/orpheus/`
2. Implementation in `src/core/{module}/`
3. Tests in `tests/{module}/`
4. Documentation in `docs/orp/`

### Adding OCC-Specific Features
1. Files in `apps/clip-composer/Source/`
2. Tests in `apps/clip-composer/tests/`
3. Documentation in `apps/clip-composer/docs/occ/`

### Adding shmui Components
1. Files in `packages/shmui-juce/`
2. Update `packages/shmui-juce/CMakeLists.txt`

## Related Diagrams

- [architecture-overview](./architecture-overview.notes.md) - System layers
- [component-map](./component-map.notes.md) - Class relationships
